#include "CaseSplatting.h"
#include "App.h"

#include <array>
#include <span>
#include <algorithm>
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

CaseSplatting::CaseSplatting():
    _splatProgram({
        SharedShader("../../../../src/VCX/Labs/Final/shaders/splat.vert"),
        SharedShader("../../../../src/VCX/Labs/Final/shaders/splat.frag")
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
    ),
    _splats(
        VertexLayout()
            .Add<SplatVertex>("splat", DrawFrequency::Dynamic)
            .At(0, &SplatVertex::position)
            .At(1, &SplatVertex::density),
        PrimitiveType::Points
    )
{
    SetupQuad();
    
    // 预设体数据文件列表
    _availableVolumes = {
        {"CT-Abdomen", "assets/pvm/CT-Abdomen.pvm"},
        {"CT-Head", "assets/pvm/Cadaver.pvm"},
        {"Carp", "assets/pvm/Carp.pvm"},
        {"Engine", "assets/pvm/Engine.pvm"}
    };
    
    // 默认加载第一个数据集
    if (!_availableVolumes.empty()) {
        LoadVolume(_availableVolumes[0].second);
    }
}

CaseSplatting::~CaseSplatting() {
    if (_volumeTexGL) {
        glDeleteTextures(1, &_volumeTexGL);
        _volumeTexGL = 0;
    }
}

void CaseSplatting::SetupQuad() {
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

bool CaseSplatting::LoadVolume(std::filesystem::path const & file) {
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
    
    // 生成Splat点集
    GenerateSplats();
    
    _needsRedraw = true;
    _needsResort = true;
    return true;
}

void CaseSplatting::GenerateSplats() {
    if (!_volume) return;
    
    _splatVertices.clear();
    
    auto const * data = _volume->GetData();
    auto dims = _volume->GetSize();
    
    // ========== 使用用户设置的采样步长 ==========
    int step = _samplingStep;
    
    // ========== 生成Splat点集：采样所有数据 ==========
    for (int z = 0; z < dims.z; z += step) {
        for (int y = 0; y < dims.y; y += step) {
            for (int x = 0; x < dims.x; x += step) {
                int idx = x + y * dims.x + z * dims.x * dims.y;
                float density = static_cast<float>(static_cast<unsigned char>(data[idx])) / 255.0f;
                
                SplatVertex s;
                s.position = glm::vec3(
                    (float)x / (float)dims.x,
                    (float)y / (float)dims.y,
                    (float)z / (float)dims.z
                );
                s.density = density;
                _splatVertices.push_back(s);
            }
        }
    }
    
    // 上传到GPU
    if (!_splatVertices.empty()) {
        auto spanBytes = make_span_bytes(std::span<SplatVertex const>(
            _splatVertices.data(), _splatVertices.size()));
        _splats.UpdateVertexBuffer("splat", spanBytes);
    }
    
    _needsResort = true;
}

void CaseSplatting::SortSplats(glm::vec3 const & viewDir) {
    // Back-to-front排序
    // viewDir是从相机指向中心的方向
    std::sort(_splatVertices.begin(), _splatVertices.end(), 
        [&viewDir](SplatVertex const & a, SplatVertex const & b) {
            float distA = glm::dot(a.position, viewDir);
            float distB = glm::dot(b.position, viewDir);
            return distA > distB;  
        });
    
    // 重新上传排序后的数据
    if (!_splatVertices.empty()) {
        auto spanBytes = make_span_bytes(std::span<SplatVertex const>(
            _splatVertices.data(), _splatVertices.size()));
        _splats.UpdateVertexBuffer("splat", spanBytes);
    }
}

void CaseSplatting::OnFrame(App & app) {
    // ========== UI渲染 ==========
    ImGui::GetIO().FontGlobalScale = 1.8f;
    
    float sidebarWidth = 420.0f;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, ImGui::GetIO().DisplaySize.y));
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    
    ImGui::Begin("Volume Splatting", nullptr, 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    
    // 算法选择器
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.8f, 0.8f));
    if (ImGui::CollapsingHeader("Rendering Algorithm", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* algorithms[] = {"Ray Casting", "Slice Viewer", "Splatting (Current)", "Slice Compositing"};
        int idx = static_cast<int>(app.GetCurrentAlgorithm());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##algo", &idx, algorithms, 4)) {
            app.SetCurrentAlgorithm(static_cast<App::Algorithm>(idx));
        }
    }
    ImGui::PopStyleColor();
    ImGui::Separator();
    
    // 数据集选择
    if (ImGui::CollapsingHeader("Volume Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
        static int currentIdx = 0;
        std::vector<const char*> items;
        items.reserve(_availableVolumes.size());
        for (auto const & [name, path] : _availableVolumes) {
            items.push_back(name.c_str());
        }
        
        // 查找当前选中的数据集索引
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
    
    // 相机控制
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Azimuth (Horizontal)");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##azimuth", &_azimuth, 0.0f, 360.0f, "%.0f°")) {
            _needsRedraw = true;
            _needsResort = true;
        }
        
        ImGui::Text("Elevation (Vertical)");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##elevation", &_elevation, -89.0f, 89.0f, "%.1f°")) {
            _needsRedraw = true;
            _needsResort = true;
        }
        
        ImGui::Text("Distance");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##distance", &_distance, 0.5f, 5.0f, "%.2f")) {
            _needsRedraw = true;
            _needsResort = true;
        }
        
        if (ImGui::Button("Reset Camera", ImVec2(-1, 0))) {
            _azimuth = 45.0f;
            _elevation = 20.0f;
            _distance = 2.0f;
            _needsRedraw = true;
            _needsResort = true;
        }
    }
    ImGui::Separator();
    
    // Splatting参数控制
    if (ImGui::CollapsingHeader("Splatting Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Sampling Step (Density)");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderInt("##samplingStep", &_samplingStep, 1, 5)) {
            GenerateSplats();  
            _needsRedraw = true;
            _needsResort = true;
        }
        ImGui::TextWrapped("Tip: 1=Most Dense (Slowest), 5=Least Dense (Fastest)");
        
        ImGui::Separator();
        
        ImGui::Text("Splat Size");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##splatSize", &_splatSize, 0.001f, 0.05f, "%.4f")) {
            _needsRedraw = true;
        }
        
        ImGui::Text("Density Threshold");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##threshold", &_densityThreshold, 0.0f, 0.8f, "%.3f")) {
            _needsRedraw = true;
        }
        
        ImGui::Text("Alpha Scale");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##alpha", &_alphaScale, 0.1f, 5.0f, "%.2f")) {
            _needsRedraw = true;
        }
        
        ImGui::Text("Color Mode");
        ImGui::SetNextItemWidth(-1);
        const char* modes[] = {"Rainbow", "Flesh", "Grayscale"};
        if (ImGui::Combo("##colorMode", &_colorMode, modes, 3)) {
            _needsRedraw = true;
        }
        
        if (ImGui::Button("Reset Splatting", ImVec2(-1, 0))) {
            _splatSize = 0.012f;
            _densityThreshold = 0.05f;
            _alphaScale = 1.0f;
            _colorMode = 0;
            _samplingStep = 2;
            GenerateSplats();
            _needsRedraw = true;
            _needsResort = true;
        }
        
        ImGui::Separator();
        ImGui::Text("Splat Count: %zu", _splatVertices.size());
    }
    
    ImGui::End();
    ImGui::PopStyleColor(2);
    
    // ========== 视口交互 ==========
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
    
    // 鼠标拖拽旋转
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        
        _azimuth += delta.x * 0.5f;
        _elevation -= delta.y * 0.5f;
        _elevation = glm::clamp(_elevation, -89.0f, 89.0f);
        
        // 保持方位角在[0, 360)范围
        while (_azimuth < 0.0f) _azimuth += 360.0f;
        while (_azimuth >= 360.0f) _azimuth -= 360.0f;
        
        _needsRedraw = true;
        _needsResort = true;
    }
    
    // 鼠标滚轮缩放
    float wheel = ImGui::GetIO().MouseWheel;
    if (ImGui::IsItemHovered() && wheel != 0.0f) {
        _distance -= wheel * 0.1f;
        _distance = glm::clamp(_distance, 0.5f, 5.0f);
        _needsRedraw = true;
        _needsResort = true;
    }
    
    ImGui::End();
    ImGui::PopStyleColor();
    
    // ========== Splatting渲染 ==========
    int viewportW = displayW - (int)sidebarWidth;
    int viewportH = displayH;
    
    // 检查FBO尺寸变化
    if (_cachedFrame.GetSize() != std::make_pair((std::uint32_t)viewportW, (std::uint32_t)viewportH)) {
        _cachedFrame.Resize(std::make_pair((std::uint32_t)viewportW, (std::uint32_t)viewportH));
        _needsRedraw = true;
    }
    
    // 渲染到离屏FBO
    if ((_needsRedraw || _needsResort) && !_splatVertices.empty()) {
        // 计算相机位置
        float azimuthRad = glm::radians(_azimuth);
        float elevationRad = glm::radians(_elevation);
        
        glm::vec3 center(0.5f, 0.5f, 0.5f);
        
        // 相机偏移向量
        glm::vec3 cameraOffset(
            std::cos(elevationRad) * std::sin(azimuthRad),
            std::sin(elevationRad),
            std::cos(elevationRad) * std::cos(azimuthRad)
        );
        
        glm::vec3 cameraPos = center + cameraOffset * _distance;
        
        // 视线方向：从相机指向中心
        glm::vec3 viewDir = glm::normalize(center - cameraPos);
        
        // 执行Back-to-Front排序
        if (_needsResort) {
            SortSplats(viewDir);
            _needsResort = false;
        }
        
        // 绑定FBO
        auto scope = _cachedFrame.Use();
        
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // 关键渲染状态设置
        glDisable(GL_DEPTH_TEST);        
        glDepthMask(GL_FALSE);           
        glEnable(GL_BLEND);              // 启用Alpha混合
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // 标准透明度混合
        glEnable(GL_PROGRAM_POINT_SIZE); 
        
        // 构建MVP矩阵：lookAt现在指向center
        glm::mat4 view = glm::lookAt(cameraPos, center, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)viewportW / viewportH, 0.1f, 100.0f);
        glm::mat4 mvp = proj * view;
        
        // 设置Shader Uniforms
        _splatProgram.GetUniforms().SetByName("mvp", mvp);
        _splatProgram.GetUniforms().SetByName("splatSize", _splatSize);
        _splatProgram.GetUniforms().SetByName("alphaScale", _alphaScale);
        _splatProgram.GetUniforms().SetByName("colorMode", _colorMode);
        _splatProgram.GetUniforms().SetByName("viewportSize", glm::vec2(viewportW, viewportH));
        _splatProgram.GetUniforms().SetByName("densityThreshold", _densityThreshold);
        
        // 绘制Splat点集
        _splats.Draw({ _splatProgram.Use() });
        
        // 恢复渲染状态
        glDisable(GL_PROGRAM_POINT_SIZE);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        
        _needsRedraw = false;
    }
    
    // ========== Blit到屏幕 ==========
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