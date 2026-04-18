#include "CaseSliceViewer.h"
#include "App.h"

#include <array>
#include <span>
#include <imgui.h>

#include "Engine/GL/Shader.h"
#include "Engine/GL/Frame.hpp"
#include "Engine/loader.h"
#include <glad/glad.h>

using namespace VCX::Engine;
using namespace VCX::Engine::GL;

namespace VCX::Labs::Final {

static constexpr std::array<float, 16> quadVerts = {
    -1.f, -1.f, 0.f, 0.f,
     1.f, -1.f, 1.f, 0.f,
     1.f,  1.f, 1.f, 1.f,
    -1.f,  1.f, 0.f, 1.f
};

struct Vertex {
    glm::vec2 Pos;
    glm::vec2 UV;
};

CaseSliceViewer::CaseSliceViewer():
    _sliceProgram({
        SharedShader("../../../../src/VCX/Labs/Final/shaders/slice.vert"),
        SharedShader("../../../../src/VCX/Labs/Final/shaders/slice.frag")
    }),
    _blitProgram({
        SharedShader("../../../../src/VCX/Labs/Final/shaders/volume.vert"),
        SharedShader("../../../../src/VCX/Labs/Final/shaders/blit.frag")
    }),
    _quad(
        VertexLayout()
            .Add<Vertex>("vertex", DrawFrequency::Static)
            .At(0, &Vertex::Pos)
            .At(1, &Vertex::UV),
        PrimitiveType::Triangles
    )
{
    SetupQuad();
    
    _availableVolumes = {
        {"CT-Abdomen", "assets/pvm/CT-Abdomen.pvm"},
        {"Cadaver", "assets/pvm/Cadaver.pvm"},
        {"Carp", "assets/pvm/Carp.pvm"},
        {"Engine", "assets/pvm/Engine.pvm"}
    };
    
    if (!_availableVolumes.empty()) {
        LoadVolume(_availableVolumes[0].second);
    }
}

CaseSliceViewer::~CaseSliceViewer() {
    if (_volumeTexGL) {
        glDeleteTextures(1, &_volumeTexGL);
        _volumeTexGL = 0;
    }
}

void CaseSliceViewer::SetupQuad() {
    std::array<Vertex, 4> verts;
    for (size_t i = 0; i < 4; ++i) {
        verts[i].Pos = glm::vec2(quadVerts[i*4+0], quadVerts[i*4+1]);
        verts[i].UV  = glm::vec2(quadVerts[i*4+2], quadVerts[i*4+3]);
    }
    auto spanBytes = make_span_bytes(std::span<Vertex const>(verts.data(), verts.size()));
    _quad.UpdateVertexBuffer("vertex", spanBytes);
    
    std::array<std::uint32_t, 6> indices = { 0, 1, 2, 0, 2, 3 };
    _quad.UpdateElementBuffer(std::span<std::uint32_t const>(indices.data(), indices.size()));
}

bool CaseSliceViewer::LoadVolume(std::filesystem::path const & file) {
    _volume = std::make_unique<VolumeData>();
    if (!_volume->LoadFromFile(file)) {
        _volume.reset();
        return false;
    }
    unsigned tex = _volume->UploadToGL();
    if (tex == 0) {
        _volume.reset();
        return false;
    }
    _volumeTexGL = tex;
    _currentVolumePath = file.string();
    _needsRedraw = true;
    return true;
}

void CaseSliceViewer::OnFrame(App & app) {
    ImGui::GetIO().FontGlobalScale = 1.8f;
    
    float sidebarWidth = 420.0f;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, ImGui::GetIO().DisplaySize.y));
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    
    ImGui::Begin("Volume Slice Viewer", nullptr, 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    
    // ====== 算法选择器 ======
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.8f, 0.8f));
    if (ImGui::CollapsingHeader("Rendering Algorithm", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* algorithms[] = {"Ray Casting", "Slice Viewer (Current)", "Splatting", "Slice Compositing"};
            int idx = static_cast<int>(app.GetCurrentAlgorithm());
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##algo", &idx, algorithms, 4)) {  // 改成4
            app.SetCurrentAlgorithm(static_cast<App::Algorithm>(idx));
        }
    }
    ImGui::PopStyleColor();
    ImGui::Separator();
    
    // ====== 数据选择 ======
    if (ImGui::CollapsingHeader("Volume Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
        static int currentIdx = 0;
        std::vector<const char*> items;
        items.reserve(_availableVolumes.size());
        for (auto const & [name, path] : _availableVolumes) {
            items.push_back(name.c_str());
        }
        
        for (size_t i = 0; i < _availableVolumes.size(); ++i) {
            if (_currentVolumePath == _availableVolumes[i].second) {
                currentIdx = static_cast<int>(i);
                break;
            }
        }
        
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##dataset", &currentIdx, items.data(), static_cast<int>(items.size()))) {
            LoadVolume(_availableVolumes[currentIdx].second);
        }
    }
    
    ImGui::Separator();
    
    // ====== 切片控制 ======
    if (ImGui::CollapsingHeader("Slice Control", ImGuiTreeNodeFlags_DefaultOpen)) {
        // 预设方向
        ImGui::Text("Quick Presets:");
        if (ImGui::Button("X-Axis (YZ)", ImVec2(-1, 30))) {
            _sliceNormal = glm::vec3(1.0f, 0.0f, 0.0f);
            _useCustomAngle = false;
            _needsRedraw = true;
        }
        if (ImGui::Button("Y-Axis (XZ)", ImVec2(-1, 30))) {
            _sliceNormal = glm::vec3(0.0f, 1.0f, 0.0f);
            _useCustomAngle = false;
            _needsRedraw = true;
        }
        if (ImGui::Button("Z-Axis (XY)", ImVec2(-1, 30))) {
            _sliceNormal = glm::vec3(0.0f, 0.0f, 1.0f);
            _useCustomAngle = false;
            _needsRedraw = true;
        }
        
        ImGui::Separator();
        
        if (ImGui::Checkbox("Custom Angle", &_useCustomAngle)) {
            _needsRedraw = true;
        }
        
        if (_useCustomAngle) {
            ImGui::Text("Theta (Polar)");  
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##theta", &_normalTheta, 0.0f, 180.0f, "%.1f°")) {
                float thetaRad = glm::radians(_normalTheta);
                float phiRad = glm::radians(_normalPhi);
                _sliceNormal = glm::vec3(
                    sin(thetaRad) * cos(phiRad),
                    sin(thetaRad) * sin(phiRad),
                    cos(thetaRad)
                );
                _needsRedraw = true;
            }
            
            ImGui::Text("Phi (Azimuth)");  
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##phi", &_normalPhi, 0.0f, 360.0f, "%.1f°")) {
                float thetaRad = glm::radians(_normalTheta);
                float phiRad = glm::radians(_normalPhi);
                _sliceNormal = glm::vec3(
                    sin(thetaRad) * cos(phiRad),
                    sin(thetaRad) * sin(phiRad),
                    cos(thetaRad)
                );
                _needsRedraw = true;
            }
        }
        
        ImGui::Separator();
        
        ImGui::Text("Slice Distance");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##sliceDist", &_sliceDistance, -0.5f, 0.5f, "%.3f")) {
            _needsRedraw = true;
        }
        
        if (ImGui::Button("Reset Position", ImVec2(-1, 0))) {
            _sliceDistance = 0.0f;
            _needsRedraw = true;
        }
    }
    
    ImGui::Separator();
    
    // ====== 转换函数 ======
    if (ImGui::CollapsingHeader("Transfer Function", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Alpha Scale");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##alpha", &_alphaScale, 0.01f, 3.0f, "%.2f")) {
            _needsRedraw = true;
        }
        
        ImGui::Text("Color Mode");
        ImGui::SetNextItemWidth(-1);
        const char* modes[] = { "Rainbow", "Flesh", "Grayscale" };  // 添加Flesh
        if (ImGui::Combo("##colorMode", &_colorMode, modes, 3)) {  // 改为3
            _needsRedraw = true;
        }
        
        if (ImGui::Button("Reset Transfer", ImVec2(-1, 0))) {
            _alphaScale = 1.0f;
            _colorMode = 0;
            _needsRedraw = true;
        }
    }
    
    ImGui::End();
    ImGui::PopStyleColor(2);

    // ====== 鼠标滚轮控制切片深度 ======
    ImGuiIO& io = ImGui::GetIO();
    if (io.MousePos.x > sidebarWidth && io.MouseWheel != 0.0f) {
        _sliceDistance += io.MouseWheel * 0.01f;
        _sliceDistance = glm::clamp(_sliceDistance, -0.5f, 0.5f);
        _needsRedraw = true;
    }
    
    // ====== OpenGL ======
    int displayW = (int)ImGui::GetIO().DisplaySize.x;
    int displayH = (int)ImGui::GetIO().DisplaySize.y;
    int viewportW = displayW - (int)sidebarWidth;
    int viewportH = displayH;
    
    if (_cachedFrame.GetSize() != std::make_pair((std::uint32_t)viewportW, (std::uint32_t)viewportH)) {
        _cachedFrame.Resize(std::make_pair((std::uint32_t)viewportW, (std::uint32_t)viewportH));
        _needsRedraw = true;
    }
    
    if (_needsRedraw && _volumeTexGL && _volume) {
        auto scope = _cachedFrame.Use();
        
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, _volumeTexGL);
        
        _sliceProgram.GetUniforms().SetByName("volumeTex", 0);
        _sliceProgram.GetUniforms().SetByName("sliceNormal", _sliceNormal);
        _sliceProgram.GetUniforms().SetByName("sliceDistance", _sliceDistance);
        _sliceProgram.GetUniforms().SetByName("alphaScale", _alphaScale);
        _sliceProgram.GetUniforms().SetByName("colorMode", _colorMode);
        
        _quad.Draw({ _sliceProgram.Use() });
        
        glBindTexture(GL_TEXTURE_3D, 0);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        
        _needsRedraw = false;
    }
    
    glViewport((int)sidebarWidth, 0, viewportW, viewportH);
    glDisable(GL_DEPTH_TEST);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _cachedFrame.GetColorAttachment().Get());
    _blitProgram.GetUniforms().SetByName("cachedTex", 0);
    
    _quad.Draw({ _blitProgram.Use() });
    
    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, displayW, displayH);
}

} // namespace VCX::Labs::Final