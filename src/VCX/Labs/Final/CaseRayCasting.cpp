#include "CaseRayCasting.h"
#include "App.h"

#include <array>
#include <span>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include "Engine/GL/Shader.h"
#include "Engine/GL/Frame.hpp"
#include "Engine/loader.h"
#include <glad/glad.h>

using namespace VCX::Engine;
using namespace VCX::Engine::GL;

namespace VCX::Labs::Final {

static constexpr std::array<float, 16> quadVerts = {
    // pos.x, pos.y, uv.x, uv.y
    -1.f, -1.f, 0.f, 0.f,
     1.f, -1.f, 1.f, 0.f,
     1.f,  1.f, 1.f, 1.f,
    -1.f,  1.f, 0.f, 1.f
};
static constexpr std::array<unsigned, 6> quadIdx = { 0,1,2, 0,2,3 };

struct Vertex {
    glm::vec2 Pos;
    glm::vec2 UV;
};

CaseRayCasting::CaseRayCasting():
    _program({
        SharedShader("../../../../src/VCX/Labs/Final/shaders/volume.vert"),
        SharedShader("../../../../src/VCX/Labs/Final/shaders/volume.frag")
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
    
    // 预设可用的体数据文件
    _availableVolumes = {
        {"CT-Abdomen", "assets/pvm/CT-Abdomen.pvm"},
        {"Cadaver", "assets/pvm/Cadaver.pvm"},
        {"Carp", "assets/pvm/Carp.pvm"},
        {"Engine", "assets/pvm/Engine.pvm"}
    };
    
    // 自动加载第一个volume
    if (!_availableVolumes.empty()) {
        LoadVolume(_availableVolumes[0].second);
    }
}

CaseRayCasting::~CaseRayCasting() {
    if (_volumeTexGL) {
        glDeleteTextures(1, &_volumeTexGL);
        _volumeTexGL = 0;
    }
}

void CaseRayCasting::SetupQuad() {
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

bool CaseRayCasting::LoadVolume(std::filesystem::path const & file) {
    _volume = std::make_unique<VolumeData>();
    if (! _volume->LoadFromFile(file)) {
        //spdlog::error("Failed to load volume {}", file.string());
        _volume.reset();
        return false;
    }
    unsigned tex = _volume->UploadToGL();
    if (tex==0) {
        //spdlog::error("Failed to upload volume to GL");
        _volume.reset();
        return false;
    }
    _volumeTexGL = tex;
    _currentVolumePath = file.string();
    
    // 根据通道数设置骨骼默认权重
    auto size = _volume->GetSize();
    if (_volume->HasGradientChannel()) {
        // 双通道：骨骼权重默认0.15
        _boneAlpha = 0.15f;
        // spdlog::info("Loaded dual-channel volume {}x{}x{}, set boneAlpha=0.15", size.x, size.y, size.z);
    } else {
        // 单通道：骨骼权重默认1.0
        _boneAlpha = 1.0f;
        // spdlog::info("Loaded single-channel volume {}x{}x{}, set boneAlpha=1.0", size.x, size.y, size.z);
    }
    
    _needsRedraw = true;  // 加载新数据后需要重绘
    return true;
}

void CaseRayCasting::OnFrame(App & app) {
    // 全局字体缩放
    ImGui::GetIO().FontGlobalScale = 1.8f;
    
    // 左侧边栏
    float sidebarWidth = 420.0f;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, ImGui::GetIO().DisplaySize.y));
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    
    ImGui::Begin("Volume Ray Casting", nullptr, 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    
    // ====== 算法选择器 ======
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.8f, 0.8f));
    if (ImGui::CollapsingHeader("Rendering Algorithm", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* algorithms[] = {"Ray Casting (Current)", "Slice Viewer", "Splatting", "Slice Compositing"};
        int idx = static_cast<int>(app.GetCurrentAlgorithm());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##algo", &idx, algorithms, 4)) {  // 改成4
            app.SetCurrentAlgorithm(static_cast<App::Algorithm>(idx));
        }
    }
    ImGui::PopStyleColor();
    ImGui::Separator();
    
    // ====== 文件选择 ======
    if (ImGui::CollapsingHeader("Volume Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
        // 构建下拉框选项
        static int currentIdx = 0;
        std::vector<const char*> items;
        items.reserve(_availableVolumes.size());
        for (auto const & [name, path] : _availableVolumes) {
            items.push_back(name.c_str());
        }
        
        // 查找当前选中的索引
        for (size_t i = 0; i < _availableVolumes.size(); ++i) {
            if (_currentVolumePath == _availableVolumes[i].second) {
                currentIdx = static_cast<int>(i);
                break;
            }
        }
        
        // 下拉框
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##dataset", &currentIdx, items.data(), static_cast<int>(items.size()))) {
            LoadVolume(_availableVolumes[currentIdx].second);
            _needsRedraw = true;
        }
    }
    
    ImGui::Separator();
    
    // ====== 视角控制 ======
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Azimuth (Horizontal)");  
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##azimuth", &_azimuth, 0.0f, 360.0f, "%.0f°")) { 
            _needsRedraw = true;
        }
        
        ImGui::Text("Elevation (Vertical)");  
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##elevation", &_elevation, -89.0f, 89.0f, "%.0f°")) {  
            _needsRedraw = true;
        }
        
        ImGui::Text("Distance");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##distance", &_distance, 0.5f, 3.0f, "%.2f")) {
            _needsRedraw = true;
        }
        
        if (ImGui::Button("Reset Camera", ImVec2(-1, 0))) {
            _azimuth = 180.0f;
            _elevation = -89.0f;
            _distance = 1.0f;
            _needsRedraw = true;
        }
    }
    
    ImGui::Separator();
    
    // ====== Ray Casting 参数 ======
    if (ImGui::CollapsingHeader("Ray Casting", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Sampling Step");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##step", &_stepSize, 0.1f, 2.0f, "%.2f")) {
            _needsRedraw = true;
        }
        
        ImGui::Text("Max Steps");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderInt("##maxsteps", &_maxSteps, 128, 2048)) {
            _needsRedraw = true;
        }
        
        if (ImGui::Button("Reset Ray Casting", ImVec2(-1, 0))) {
            _stepSize = 0.5f;
            _maxSteps = 1024;
            _needsRedraw = true;
        }
    }
    
    ImGui::Separator();
    
    // ====== 传输函数 ======
    if (ImGui::CollapsingHeader("Transfer Function", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Alpha Scale");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##alpha", &_alphaScale, 0.01f, 2.0f, "%.01f")) {
            _needsRedraw = true;
        }
        
        if (ImGui::Button("Reset Transfer", ImVec2(-1, 0))) {
            _alphaScale = 1.0f;
            _needsRedraw = true;
        }
    }

    ImGui::Separator();

    // ====== 高级控制 ======
    if (ImGui::CollapsingHeader("Advanced Control", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Color Mode");
        ImGui::SetNextItemWidth(-1);
        const char* modes[] = { "Rainbow", "Flesh", "Grayscale" };
        if (ImGui::Combo("##colorMode", &_colorMode, modes, 3)) {
            _needsRedraw = true;
        }
        
        ImGui::Text("Soft Tissue Opacity");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##softTissue", &_softTissueAlpha, 0.0f, 1.0f, "%.2f")) {
            _needsRedraw = true;
        }
        
        ImGui::Text("Bone Opacity");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##boneAlpha", &_boneAlpha, 0.0f, 1.5f, "%.2f")) {
            _needsRedraw = true;
        }
        
        ImGui::Text("Gradient Threshold");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##gradThreshold", &_gradientThreshold, 0.0f, 1.0f, "%.2f")) {
            _needsRedraw = true;
        }
        
        if (ImGui::Button("Reset Advanced", ImVec2(-1, 0))) {
            _colorMode = 0;
            _softTissueAlpha = 0.3f;
            // 根据当前数据类型重置boneAlpha
            if (_volume && _volume->HasGradientChannel()) {
                _boneAlpha = 0.15f;
            } else {
                _boneAlpha = 1.0f;
            }
            _gradientThreshold = 0.5f;
            _needsRedraw = true;
        }
    }
    
    ImGui::Separator();
    
    // ====== 光照 ======
    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Light Direction");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat3("##light", &_lightDir[0], -1.0f, 1.0f, "%.2f")) {
            _needsRedraw = true;
        }
        
        ImGui::Text("Ambient");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##ambient", &_ambient, 0.0f, 1.0f, "%.2f")) {
            _needsRedraw = true;
        }
        
        ImGui::Text("Diffuse");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##diffuse", &_diffuse, 0.0f, 1.0f, "%.2f")) {
            _needsRedraw = true;
        }
        
        if (ImGui::Button("Reset Lighting", ImVec2(-1, 0))) {
            _lightDir = glm::vec3(0.577f, 0.577f, 0.577f);
            _ambient = 0.4f;
            _diffuse = 0.6f;
            _needsRedraw = true;
        }
    }
    
    ImGui::End();
    ImGui::PopStyleColor(2);

    // ====== 右侧渲染视口 - 鼠标拖拽旋转 ======
    int displayW = (int)ImGui::GetIO().DisplaySize.x;
    int displayH = (int)ImGui::GetIO().DisplaySize.y;
    
    // 创建一个覆盖整个渲染区域的不可见窗口来捕获鼠标输入
    ImGui::SetNextWindowPos(ImVec2(sidebarWidth, 0));
    ImGui::SetNextWindowSize(ImVec2(displayW - sidebarWidth, displayH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0)); // 完全透明
    ImGui::Begin("ViewportInteraction", nullptr, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground);
    
    // 使用不可见按钮覆盖整个区域来捕获鼠标
    ImGui::InvisibleButton("viewport", ImVec2(displayW - sidebarWidth, displayH));
    
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        
        // 左键拖拽: 旋转方位角和仰角
        _azimuth += delta.x * 0.5f;
        _elevation -= delta.y * 0.5f;
        _elevation = glm::clamp(_elevation, -89.0f, 89.0f);
        
        while (_azimuth < 0.0f) _azimuth += 360.0f;
        while (_azimuth >= 360.0f) _azimuth -= 360.0f;
        
        _needsRedraw = true;
    }
    
    // 鼠标滚轮: 调整距离
    float wheel = ImGui::GetIO().MouseWheel;
    if (ImGui::IsItemHovered() && wheel != 0.0f) {
        _distance -= wheel * 0.15f;
        _distance = glm::clamp(_distance, 0.8f, 4.5f);
        _needsRedraw = true;
    }
    
    ImGui::End();
    ImGui::PopStyleColor();

    // ====== OpenGL 渲染 ======
    int viewportW = displayW - (int)sidebarWidth;
    int viewportH = displayH;
    
    // 检查是否需要调整FBO大小
    if (_cachedFrame.GetSize() != std::make_pair((std::uint32_t)viewportW, (std::uint32_t)viewportH)) {
        _cachedFrame.Resize(std::make_pair((std::uint32_t)viewportW, (std::uint32_t)viewportH));
        _needsRedraw = true;  // 窗口大小改变需要重绘
    }
    
    // 只在需要时重新渲染体数据
    if (_needsRedraw && _volumeTexGL) {
        auto scope = _cachedFrame.Use();  // 渲染到FBO
        
        glDisable(GL_DEPTH_TEST);
        
        // 绑定密度texture到unit 0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, _volumeTexGL);
        _program.GetUniforms().SetByName("volumeTex", 0);
        
        // 绑定梯度texture到unit 1
        if (_volume->HasGradientChannel()) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_3D, _volume->GetGradientTextureGL());
            _program.GetUniforms().SetByName("gradientTex", 1);
        }
        
        glm::ivec3 s = _volume->GetSize();
        _program.GetUniforms().SetByName("volumeSize", glm::vec3((float)s.x, (float)s.y, (float)s.z));
        _program.GetUniforms().SetByName("hasGradientChannel", _volume->HasGradientChannel() ? 1 : 0);
        _program.GetUniforms().SetByName("stepSize", _stepSize);
        _program.GetUniforms().SetByName("alphaScale", _alphaScale);
        _program.GetUniforms().SetByName("softTissueAlpha", _softTissueAlpha);
        _program.GetUniforms().SetByName("boneAlpha", _boneAlpha);
        _program.GetUniforms().SetByName("gradientThreshold", _gradientThreshold);
        _program.GetUniforms().SetByName("colorMode", _colorMode);
        _program.GetUniforms().SetByName("lightDir", glm::normalize(_lightDir));
        _program.GetUniforms().SetByName("ambient", _ambient);
        _program.GetUniforms().SetByName("diffuse", _diffuse);
        _program.GetUniforms().SetByName("maxSteps", _maxSteps);
        
        float safeElevation = glm::clamp(_elevation, -89.0f, 89.0f);
        _program.GetUniforms().SetByName("azimuth", glm::radians(_azimuth));
        _program.GetUniforms().SetByName("elevation", glm::radians(safeElevation));
        _program.GetUniforms().SetByName("roll", 0.0f);  // roll固定为0
        _program.GetUniforms().SetByName("distance", _distance);

        // 绘制到FBO
        _quad.Draw({ _program.Use() });
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_3D, 0);
        
        glEnable(GL_DEPTH_TEST);
        
        _needsRedraw = false;  // 渲染完成，清除标志
    }
    
    // 将缓存的FBO内容blit到屏幕
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