#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include "Engine/GL/Frame.hpp"
#include "Engine/GL/Program.h"
#include "Engine/GL/RenderItem.h"
#include "VolumeData.h"

namespace VCX::Labs::Final {

class CaseSliceCompositing {
public:
    CaseSliceCompositing();
    ~CaseSliceCompositing();

    void OnFrame(class App & app);

private:
    // 初始化切片四边形
    void SetupSliceQuad();
    
    // 初始化Blit四边形
    void SetupBlitQuad();
    
    // 加载体数据
    bool LoadVolume(std::filesystem::path const & file);
    
    // Shader程序
    Engine::GL::UniqueProgram _sliceProgram;  // 渲染切片的shader
    Engine::GL::UniqueProgram _blitProgram;   // 最终输出shader
    
    // 渲染对象
    Engine::GL::UniqueIndexedRenderItem _sliceQuad;  // 单个切片四边形
    Engine::GL::UniqueIndexedRenderItem _blitQuad;   // Blit用四边形
    
    // 帧缓冲
    Engine::GL::UniqueRenderFrame _cachedFrame;
    
    // 体数据
    std::unique_ptr<VolumeData> _volume;      // 体数据对象
    unsigned _volumeTexGL {0};                // OpenGL纹理ID
    
    // 数据集列表
    std::vector<std::pair<std::string, std::string>> _availableVolumes;
    std::string _currentVolumePath;
    
    // 合成参数
    int _numSlices{256};                      // 切片数量
    float _alphaScale{2.0f};                  // 透明度缩放
    float _densityThreshold{0.0f};            // 密度阈值
    int _colorMode{0};                        // 颜色模式: 0=彩虹, 1=肉色, 2=灰度
    
    // 相机参数
    float _azimuth{180.0f};                   // 方位角
    float _elevation{0.0f};                   // 仰角
    float _distance{2.0f};                    // 距离
    
    // 切片顶点结构
    struct SliceVertex {
        glm::vec3 position; 
    };
    
    bool _needsRedraw{true};  // 是否需要重绘
};

} // namespace VCX::Labs::Final