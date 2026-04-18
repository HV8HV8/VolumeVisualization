#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include "Engine/GL/Frame.hpp"
#include "Engine/GL/Program.h"
#include "Engine/GL/RenderItem.h"
#include "VolumeData.h"

namespace VCX::Labs::Final {

class CaseRayCasting {
public:
    CaseRayCasting();
    ~CaseRayCasting();

    void OnFrame(class App & app);

private:
    void SetupQuad();
    bool LoadVolume(std::filesystem::path const & file);

    // Shader程序
    Engine::GL::UniqueProgram _program;
    Engine::GL::UniqueProgram _blitProgram;
    Engine::GL::UniqueIndexedRenderItem _quad;
    
    // FBO缓存
    Engine::GL::UniqueRenderFrame _cachedFrame;
    
    // 体数据
    std::unique_ptr<VolumeData> _volume;
    unsigned _volumeTexGL {0};
    
    // 可用的体数据文件列表
    std::vector<std::pair<std::string, std::string>> _availableVolumes;
    std::string _currentVolumePath;
    
    // 相机参数
    float _azimuth   {180.0f};
    float _elevation {-89.0f};
    float _distance  {1.0f};
    
    // 光线投射参数
    float _stepSize  {0.5f};
    int   _maxSteps  {1024};
    float _alphaScale{1.0f};
    
    // 光照参数
    glm::vec3 _lightDir {0.577f, 0.577f, 0.577f};
    float _ambient {0.4f};
    float _diffuse {0.6f};
    
    // 传输函数控制
    float _softTissueAlpha{0.3f};
    float _boneAlpha{1.0f};
    float _gradientThreshold{0.5f};
    int _colorMode{0};
    
    bool _needsRedraw{true};
};

} // namespace VCX::Labs::Final