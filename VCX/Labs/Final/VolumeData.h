#pragma once

#include <string>
#include <optional>
#include <vector>
#include <filesystem>

#include "Engine/TextureND.hpp"
#include "Engine/Formats.hpp"
#include <glm/vec3.hpp>

namespace VCX::Labs::Final {

// 原始体数据元数据
struct RawMetadata {
    int width = 0;
    int height = 0;
    int depth = 0;
    int components = 1;
};

// 体数据类,负责加载和管理3D体数据
class VolumeData {
public:
    using VolumeTex = VCX::Engine::Texture3D<VCX::Engine::Formats::R8>;

    VolumeData() = default;

    // 从文件加载体数据
    bool LoadFromFile(std::filesystem::path const & file);
    
    // 获取纹理对象
    VolumeTex const & GetTexture() const { return _tex; }
    
    // 获取体数据尺寸
    glm::ivec3 GetSize() const { return _size; }
    
    // 上传到OpenGL并返回纹理ID
    unsigned UploadToGL();
    
    // 获取梯度纹理(如果有)
    unsigned GetGradientTextureGL() const { return _gradientTexGL; }
    
    // 检查是否有梯度通道
    bool HasGradientChannel() const { return _hasGradientChannel; }

    // 获取原始数据指针
    std::byte const * GetData() const { return _bytes.data(); }
    std::byte const * GetGradientData() const { return _gradientBytes.data(); }

private:
    bool TryLoadRawWithMetadata(std::filesystem::path const & file, RawMetadata const & meta);
    bool TryLoadPVM(std::filesystem::path const & file);
    bool TryLoadMHD(std::filesystem::path const & file);
    bool TryInferRaw(std::filesystem::path const & file);  // 添加这个声明
    
    VolumeTex _tex;
    glm::ivec3 _size{0,0,0};
    std::vector<std::byte> _bytes;
    std::vector<std::byte> _gradientBytes;
    unsigned _glTex{0};
    unsigned _gradientTexGL{0};
    bool _hasGradientChannel{false};
};
} // namespace VCX::Labs::Final