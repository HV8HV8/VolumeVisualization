#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <glm/glm.hpp>

#include "Engine/GL/Frame.hpp"
#include "Engine/GL/Program.h"
#include "Engine/GL/RenderItem.h"
#include "VolumeData.h"

namespace VCX::Labs::Final {

class App;

// Volume Splatting算法实现
// 基于点基元渲染体数据，使用高斯核函数和透明度混合
class CaseSplatting {
public:
    CaseSplatting();
    ~CaseSplatting();

    void OnFrame(App & app);

private:
    struct SplatVertex {
        glm::vec3 position;  // 世界空间位置
        float density;       // 密度值[0,1]
    };

    // ========== 渲染资源 ==========
    Engine::GL::UniqueProgram       _splatProgram;  // Splat渲染程序
    Engine::GL::UniqueProgram       _blitProgram;   // 屏幕Blit程序
    Engine::GL::UniqueIndexedRenderItem _quad;      // 全屏四边形
    Engine::GL::UniqueRenderItem        _splats;    // Splat点集
    Engine::GL::UniqueRenderFrame       _cachedFrame; // 离屏渲染缓存

    // ========== 体数据 ==========
    std::unique_ptr<VolumeData> _volume;
    unsigned int                _volumeTexGL { 0 };
    std::string                 _currentVolumePath;
    std::vector<std::pair<std::string, std::string>> _availableVolumes;

    // ========== Splat数据 ==========
    std::vector<SplatVertex> _splatVertices;  // CPU端顶点缓存
    bool                     _needsResort { false };  // 是否需要深度排序
    bool                     _needsRedraw { true };   // 是否需要重绘

    // ========== 相机参数 ==========
    float _azimuth   { 45.0f };   // 方位角[0,360]
    float _elevation { 20.0f };   // 仰角[-89,89]
    float _distance  { 2.0f };    // 相机距离

    // ========== Splatting参数 ==========
    float _splatSize        { 0.012f };
    float _densityThreshold { 0.05f };
    float _alphaScale       { 1.0f };
    int   _colorMode        { 0 };
    int   _samplingStep     { 2 };       

    // ========== 内部方法 ==========
    void SetupQuad();                              // 初始化全屏四边形
    bool LoadVolume(std::filesystem::path const & file);  // 加载体数据
    void GenerateSplats();                         // 从体数据生成Splat点集
    void SortSplats(glm::vec3 const & viewDir);    // 按深度排序（back-to-front）
};

} // namespace VCX::Labs::Final