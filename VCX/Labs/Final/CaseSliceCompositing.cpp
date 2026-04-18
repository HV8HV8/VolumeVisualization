#include "CaseSliceCompositing.h"
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

CaseSliceCompositing::CaseSliceCompositing():
    _sliceProgram({
        SharedShader("../../../../src/VCX/Labs/Final/shaders/slice_composite.vert"),
        SharedShader("../../../../src/VCX/Labs/Final/shaders/slice_composite.frag")
    }),
    _blitProgram({
        SharedShader("../../../../src/VCX/Labs/Final/shaders/volume.vert"),
        SharedShader("../../../../src/VCX/Labs/Final/shaders/blit.frag")
    }),
    _sliceQuad(
        VertexLayout()
            .Add<SliceVertex>("vertex", DrawFrequency::Static)
            .At(0, &SliceVertex::position),
        PrimitiveType::Triangles
    ),
    _blitQuad(
        VertexLayout()
            .Add<Vertex>("vertex", DrawFrequency::Static)
            .At(0, &Vertex::Pos)
            .At(1, &Vertex::UV),
        PrimitiveType::Triangles
    )
{
    SetupSliceQuad();
    SetupBlitQuad();
    
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

CaseSliceCompositing::~CaseSliceCompositing() {
    if (_volumeTexGL) {
        glDeleteTextures(1, &_volumeTexGL);
        _volumeTexGL = 0;
    }
}

void CaseSliceCompositing::SetupSliceQuad() {
    std::array<SliceVertex, 4> verts = {{
        {{-0.5f, -0.5f, 0.0f}},
        {{ 0.5f, -0.5f, 0.0f}},
        {{ 0.5f,  0.5f, 0.0f}},
        {{-0.5f,  0.5f, 0.0f}}
    }};
    
    auto spanBytes = make_span_bytes(std::span<SliceVertex const>(verts.data(), verts.size()));
    _sliceQuad.UpdateVertexBuffer("vertex", spanBytes);
    
    std::array<std::uint32_t, 6> indices = { 0, 1, 2, 0, 2, 3 };
    _sliceQuad.UpdateElementBuffer(std::span<std::uint32_t const>(indices.data(), indices.size()));
}

void CaseSliceCompositing::SetupBlitQuad() {
    std::array<Vertex, 4> verts;
    for (size_t i = 0; i < 4; ++i) {
        verts[i].Pos = glm::vec2(quadVerts[i*4+0], quadVerts[i*4+1]);
        verts[i].UV  = glm::vec2(quadVerts[i*4+2], quadVerts[i*4+3]);
    }
    auto spanBytes = make_span_bytes(std::span<Vertex const>(verts.data(), verts.size()));
    _blitQuad.UpdateVertexBuffer("vertex", spanBytes);
    
    std::array<std::uint32_t, 6> indices = { 0, 1, 2, 0, 2, 3 };
    _blitQuad.UpdateElementBuffer(std::span<std::uint32_t const>(indices.data(), indices.size()));
}

bool CaseSliceCompositing::LoadVolume(std::filesystem::path const & file) {
    if (_volumeTexGL) {
        glDeleteTextures(1, &_volumeTexGL);
        _volumeTexGL = 0;
    }
    
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

void CaseSliceCompositing::OnFrame(App & app) {
    ImGui::GetIO().FontGlobalScale = 1.8f;
    
    float sidebarWidth = 420.0f;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, ImGui::GetIO().DisplaySize.y));
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    
    ImGui::Begin("Slice Compositing", nullptr, 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.8f, 0.8f));
    if (ImGui::CollapsingHeader("Rendering Algorithm", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* algorithms[] = {"Ray Casting", "Slice Viewer", "Splatting", "Slice Compositing (Current)"};
        int idx = static_cast<int>(app.GetCurrentAlgorithm());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##algo", &idx, algorithms, 4)) {
            app.SetCurrentAlgorithm(static_cast<App::Algorithm>(idx));
        }
    }
    ImGui::PopStyleColor();
    ImGui::Separator();
    
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
    
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Azimuth (Horizontal)");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##azimuth", &_azimuth, 0.0f, 360.0f, "%.1f°")) {
            _needsRedraw = true;
        }
        
        ImGui::Text("Elevation (Vertical)");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##elevation", &_elevation, -89.0f, 89.0f, "%.1f°")) {
            _needsRedraw = true;
        }
        
        ImGui::Text("Distance");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##distance", &_distance, 1.0f, 5.0f, "%.2f")) {
            _needsRedraw = true;
        }
        
        if (ImGui::Button("Reset Camera", ImVec2(-1, 0))) {
            _azimuth = 180.0f;
            _elevation = 0.0f;
            _distance = 2.0f;
            _needsRedraw = true;
        }
    }
    
    ImGui::Separator();
    
    if (ImGui::CollapsingHeader("Compositing Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Number of Slices");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderInt("##slices", &_numSlices, 32, 512)) {
            _needsRedraw = true;
        }
        
        ImGui::Text("Alpha Scale");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##alpha", &_alphaScale, 0.01f, 5.0f, "%.2f")) {
            _needsRedraw = true;
        }
        
        ImGui::Text("Density Threshold");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##threshold", &_densityThreshold, 0.0f, 1.0f, "%.3f")) {
            _needsRedraw = true;
        }
        
        ImGui::Separator();
        
        ImGui::Text("Color Mode");
        const char* modes[] = {"Rainbow", "Flesh", "Grayscale"};
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##colormode", &_colorMode, modes, 3)) {
            _needsRedraw = true;
        }
        
        if (ImGui::Button("Reset Compositing", ImVec2(-1, 0))) {
            _numSlices = 256;
            _alphaScale = 2.0f;
            _densityThreshold = 0.0f;
            _colorMode = 0;
            _needsRedraw = true;
        }
    }
    
    ImGui::End();
    ImGui::PopStyleColor(2);

    // 鼠标交互窗口
    int displayW = (int)ImGui::GetIO().DisplaySize.x;
    int displayH = (int)ImGui::GetIO().DisplaySize.y;
    
    ImGui::SetNextWindowPos(ImVec2(sidebarWidth, 0));
    ImGui::SetNextWindowSize(ImVec2(displayW - sidebarWidth, displayH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin("ViewportInteraction", nullptr, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground);
    
    ImGui::InvisibleButton("viewport", ImVec2(displayW - sidebarWidth, displayH));
    
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        
        _azimuth += delta.x * 0.5f;
        _elevation -= delta.y * 0.5f;
        _elevation = glm::clamp(_elevation, -89.0f, 89.0f);
        
        while (_azimuth < 0.0f) _azimuth += 360.0f;
        while (_azimuth >= 360.0f) _azimuth -= 360.0f;
        
        _needsRedraw = true;
    }
    
    float wheel = ImGui::GetIO().MouseWheel;
    if (ImGui::IsItemHovered() && wheel != 0.0f) {
        _distance -= wheel * 0.15f;
        _distance = glm::clamp(_distance, 0.8f, 5.0f);
        _needsRedraw = true;
    }
    
    ImGui::End();
    ImGui::PopStyleColor();

    int viewportW = displayW - (int)sidebarWidth;
    int viewportH = displayH;
    
    if (_cachedFrame.GetSize() != std::make_pair((std::uint32_t)viewportW, (std::uint32_t)viewportH)) {
        _cachedFrame.Resize(std::make_pair((std::uint32_t)viewportW, (std::uint32_t)viewportH));
        _needsRedraw = true;
    }
    
    if (_needsRedraw && _volume) {
        float azimuthRad = glm::radians(_azimuth);
        float elevationRad = glm::radians(_elevation);
        
        float cosElev = cos(elevationRad);
        glm::vec3 cameraPos(
            _distance * cosElev * sin(azimuthRad),
            _distance * sin(elevationRad),
            _distance * cosElev * cos(azimuthRad)
        );
        
        glm::vec3 viewDir = -glm::normalize(cameraPos);
        
        auto scope = _cachedFrame.Use();
        
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        
        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)viewportW / viewportH, 0.1f, 100.0f);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, _volumeTexGL);
        
        // 从后向前渲染所有切片
        for (int i = 0; i < _numSlices; ++i) {
            float t = (float)i / (_numSlices - 1);
            float depth = t - 0.5f;
            
            glm::vec3 sliceCenter = -viewDir * depth;
            
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, sliceCenter);
            
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            if (abs(glm::dot(viewDir, up)) > 0.99f) {
                up = glm::vec3(0.0f, 0.0f, 1.0f);
            }
            glm::vec3 right = glm::normalize(glm::cross(up, viewDir));
            up = glm::cross(viewDir, right);
            
            glm::mat3 rotation(right, up, viewDir);
            model = model * glm::mat4(rotation);
            
            glm::mat4 mvp = proj * view * model;
            
            _sliceProgram.GetUniforms().SetByName("mvp", mvp);
            _sliceProgram.GetUniforms().SetByName("model", model);
            _sliceProgram.GetUniforms().SetByName("volumeTex", 0);
            _sliceProgram.GetUniforms().SetByName("sliceDepth", t);
            _sliceProgram.GetUniforms().SetByName("alphaScale", _alphaScale / _numSlices);
            _sliceProgram.GetUniforms().SetByName("densityThreshold", _densityThreshold);
            _sliceProgram.GetUniforms().SetByName("colorMode", _colorMode);
            _sliceProgram.GetUniforms().SetByName("hasGradient", _volume->HasGradientChannel() ? 1 : 0);
            
            _sliceQuad.Draw({ _sliceProgram.Use() });
        }
        
        glBindTexture(GL_TEXTURE_3D, 0);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        
        _needsRedraw = false;
    }
    
    // Blit到屏幕
    glViewport((int)sidebarWidth, 0, viewportW, viewportH);
    glDisable(GL_DEPTH_TEST);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _cachedFrame.GetColorAttachment().Get());
    _blitProgram.GetUniforms().SetByName("cachedTex", 0);
    
    _blitQuad.Draw({ _blitProgram.Use() });
    
    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, displayW, displayH);
}

} // namespace VCX::Labs::Final