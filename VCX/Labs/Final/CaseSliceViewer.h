#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include "Engine/GL/Frame.hpp"
#include "Engine/GL/Program.h"
#include "Engine/GL/RenderItem.h"
#include "VolumeData.h"

namespace VCX::Labs::Final {

class CaseSliceViewer {
public:
    CaseSliceViewer();
    ~CaseSliceViewer();

    void OnFrame(class App & app);

private:
    void SetupQuad();
    bool LoadVolume(std::filesystem::path const & file);

    Engine::GL::UniqueProgram _sliceProgram;
    Engine::GL::UniqueProgram _blitProgram;
    Engine::GL::UniqueIndexedRenderItem _quad;
    
    Engine::GL::UniqueRenderFrame _cachedFrame;
    
    std::unique_ptr<VolumeData> _volume;
    unsigned _volumeTexGL {0};
    
    std::vector<std::pair<std::string, std::string>> _availableVolumes;
    std::string _currentVolumePath;
    
    bool _useCustomAngle{false};
    glm::vec3 _sliceNormal{0.0f, 0.0f, 1.0f};  // 切片法向量
    float _sliceDistance{0.0f};  // 切片距离中心的距离

    float _normalTheta{0.0f};  // 法向量的极角
    float _normalPhi{0.0f};    // 法向量的方位角
    
    float _alphaScale{1.0f};
    int _colorMode{0};
    
    bool _needsRedraw{true};
};

} // namespace VCX::Labs::Final