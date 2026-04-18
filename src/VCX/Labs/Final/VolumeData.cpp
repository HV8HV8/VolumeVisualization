#include "VolumeData.h"

#include <fstream>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <vector>
#include <spdlog/spdlog.h>

#include "Engine/loader.h"
#include <glad/glad.h>

using namespace VCX::Engine;

namespace VCX::Labs::Final {

static bool ParseRawMeta(std::string const & jsonStr, RawMetadata & meta) {
    auto findValue = [&jsonStr](const char* key) -> int {
        std::string searchStr = std::string("\"") + key + "\"";
        size_t pos = jsonStr.find(searchStr);
        if (pos == std::string::npos) return -1;
        
        pos = jsonStr.find(':', pos);
        if (pos == std::string::npos) return -1;
        
        int value = 0;
        if (std::sscanf(jsonStr.c_str() + pos, ": %d", &value) == 1) {
            return value;
        }
        return -1;
    };
    
    meta.width = findValue("width");
    meta.height = findValue("height");
    meta.depth = findValue("depth");
    meta.components = findValue("components");
    
    return meta.width > 0 && meta.height > 0 && meta.depth > 0;
}

static bool LoadRawMetadata(std::filesystem::path const & rawPath, RawMetadata & meta) {
    auto metaPath = rawPath;
    metaPath.replace_extension(".raw.meta");
    
    if (!std::filesystem::exists(metaPath)) {
        // spdlog::warn("VolumeData: metadata file not found: {}", metaPath.string());
        return false;
    }
    
    std::ifstream fin(metaPath);
    if (!fin) {
        // spdlog::error("VolumeData: failed to open metadata file: {}", metaPath.string());
        return false;
    }
    
    std::stringstream buffer;
    buffer << fin.rdbuf();
    std::string jsonStr = buffer.str();
    fin.close();
    
    if (!ParseRawMeta(jsonStr, meta)) {
        // spdlog::error("VolumeData: failed to parse metadata from {}", metaPath.string());
        return false;
    }
    
    // spdlog::info("VolumeData: loaded metadata {}x{}x{} components={}", 
    //              meta.width, meta.height, meta.depth, meta.components);
    return true;
}

bool VolumeData::LoadFromFile(std::filesystem::path const & file) {
    _glTex = 0;
    _gradientTexGL = 0;
    _bytes.clear();
    _gradientBytes.clear();
    _hasGradientChannel = false;
    _size = {0,0,0};
    auto ext = file.extension().string();
    
    if (ext == ".mhd" || ext == ".mha") {
        return TryLoadMHD(file);
    }
    
    if (ext == ".pvm") {
        // 优先尝试加载对应的 .raw 文件
        auto rawPath = file;
        rawPath.replace_extension(".raw");
        if (std::filesystem::exists(rawPath)) {
            RawMetadata meta;
            if (LoadRawMetadata(rawPath, meta)) {
                // spdlog::info("VolumeData::LoadFromFile: found converted raw file for {}", file.filename().string());
                return TryLoadRawWithMetadata(rawPath, meta);
            }
        }
        // 如果没有 .raw 文件或元数据，尝试加载 PVM
        if (TryLoadPVM(file)) return true;
    }
    
    if (TryInferRaw(file)) return true;

    // spdlog::error("VolumeData::LoadFromFile: unsupported or failed to load '{}'", file.string());
    return false;
}

bool VolumeData::TryLoadRawWithMetadata(std::filesystem::path const & file, RawMetadata const & meta) {
    auto blob = LoadBytes(file);
    if (blob.empty()) {
        // spdlog::error("VolumeData::TryLoadRawWithMetadata: failed to load file '{}'", file.string());
        return false;
    }

    std::size_t expectedBytes = std::size_t(meta.width) * meta.height * meta.depth * meta.components;
    if (blob.size() < expectedBytes) {
        // spdlog::error("VolumeData::TryLoadRawWithMetadata: size mismatch {} vs expected {}", 
        //              blob.size(), expectedBytes);
        return false;
    }

    _size = {meta.width, meta.height, meta.depth};
    _gradientBytes.clear();
    
    if (meta.components == 1) {
        _bytes.assign(blob.begin(), blob.begin() + expectedBytes);
        _hasGradientChannel = false;
    } else if (meta.components >= 2) {
        // 多通道格式
        // spdlog::info("VolumeData::TryLoadRawWithMetadata: extracting first 2 channels from {} components (SWAPPED)", meta.components);
        std::size_t singleBytes = expectedBytes / meta.components;
        _bytes.resize(singleBytes);
        _gradientBytes.resize(singleBytes);
        
        for (std::size_t i = 0; i < singleBytes; ++i) {
            _bytes[i] = blob[i * meta.components + 1];      // 第二通道：密度
            _gradientBytes[i] = blob[i * meta.components];  // 第一通道：梯度
        }
        _hasGradientChannel = true;
    }

    _tex = VolumeTex(_size.x, _size.y, _size.z);
    std::memcpy(const_cast<std::byte*>(_tex.GetBytes().data()), _bytes.data(), _bytes.size());
    
    // spdlog::info("VolumeData::TryLoadRawWithMetadata: successfully loaded {}x{}x{}, hasGradient={}", 
    //              _size.x, _size.y, _size.z, _hasGradientChannel);
    return true;
}

bool VolumeData::TryLoadPVM(std::filesystem::path const & file) {
    auto blob = LoadBytes(file);
    if (blob.size() < 20) {
        // spdlog::error("VolumeData::TryLoadPVM: file too small");
        return false;
    }

    const unsigned char * data = reinterpret_cast<const unsigned char*>(blob.data());
    std::size_t bytes = blob.size();

    if (std::memcmp(data, "DDS v3", 6) == 0) {
        // spdlog::info("VolumeData::TryLoadPVM: detected DDS-encoded PVM format");
        // spdlog::warn("VolumeData::TryLoadPVM: DDS-encoded PVM requires conversion to RAW first");
        return false;
    }

    std::vector<unsigned char> dataCopy(data, data + bytes);
    dataCopy.push_back('\0');
    unsigned char * dataPtr = dataCopy.data();

    int version = 0;
    unsigned char * ptr = NULL;
    
    if (std::strncmp((char*)dataPtr, "PVM\n", 4) == 0) {
        version = 1;
        ptr = dataPtr + 4;
    } else if (std::strncmp((char*)dataPtr, "PVM2\n", 5) == 0) {
        version = 2;
        ptr = dataPtr + 5;
    } else if (std::strncmp((char*)dataPtr, "PVM3\n", 5) == 0) {
        version = 3;
        ptr = dataPtr + 5;
    } else {
        // spdlog::warn("VolumeData::TryLoadPVM: unrecognized header");
        return false;
    }

    // spdlog::info("VolumeData::TryLoadPVM: detected PVM version {}", version);

    unsigned int width = 0, height = 0, depth = 0;
    float scalex = 1.0f, scaley = 1.0f, scalez = 1.0f;
    unsigned int components = 0;

    while (*ptr == '#') {
        while (*ptr != '\n' && *ptr != '\0') ptr++;
        if (*ptr == '\n') ptr++;
    }

    if (version == 1) {
        if (std::sscanf((char*)ptr, "%u %u %u\n", &width, &height, &depth) != 3) {
            // spdlog::error("VolumeData::TryLoadPVM: failed to parse dimensions (v1)");
            return false;
        }
    } else {
        if (std::sscanf((char*)ptr, "%u %u %u\n%g %g %g\n", 
                        &width, &height, &depth, &scalex, &scaley, &scalez) != 6) {
            // spdlog::error("VolumeData::TryLoadPVM: failed to parse dimensions and scale (v2/v3)");
            return false;
        }
    }

    // spdlog::info("VolumeData::TryLoadPVM: dimensions {}x{}x{}", width, height, depth);

    if (width == 0 || height == 0 || depth == 0) {
        // spdlog::error("VolumeData::TryLoadPVM: invalid dimensions");
        return false;
    }

    ptr = (unsigned char*)std::strchr((char*)ptr, '\n') + 1;
    ptr = (unsigned char*)std::strchr((char*)ptr, '\n') + 1;

    if (std::sscanf((char*)ptr, "%u\n", &components) != 1) {
        // spdlog::error("VolumeData::TryLoadPVM: failed to parse components");
        return false;
    }

    if (components == 0) {
        // spdlog::error("VolumeData::TryLoadPVM: invalid components");
        return false;
    }

    ptr = (unsigned char*)std::strchr((char*)ptr, '\n') + 1;

    std::size_t volumeBytes = std::size_t(width) * height * depth * components;
    std::size_t dataStart = ptr - dataPtr;
    std::size_t remaining = bytes - dataStart;

    // spdlog::info("VolumeData::TryLoadPVM: dataStart={}, expected={}, remaining={}", 
    //              dataStart, volumeBytes, remaining);

    if (remaining < volumeBytes) {
        // spdlog::error("VolumeData::TryLoadPVM: insufficient data");
        return false;
    }

    _size = {int(width), int(height), int(depth)};
    _gradientBytes.clear();

    if (components == 1) {
        _bytes.resize(volumeBytes);
        std::memcpy(_bytes.data(), ptr, volumeBytes);
        _hasGradientChannel = false;
    } else if (components >= 2) {
        std::size_t singleBytes = volumeBytes / components;
        _bytes.resize(singleBytes);
        _gradientBytes.resize(singleBytes);
        
        for (std::size_t i = 0; i < singleBytes; ++i) {
            _bytes[i] = std::byte(ptr[i * components + 1]);      // 第二通道：密度
            _gradientBytes[i] = std::byte(ptr[i * components]);  // 第一通道：梯度
        }
        _hasGradientChannel = true;
        
        // spdlog::info("VolumeData::TryLoadPVM: extracted 2 channels from {} components (SWAPPED)", components);
    }

    _tex = VolumeTex(_size.x, _size.y, _size.z);
    std::memcpy(const_cast<std::byte*>(_tex.GetBytes().data()), _bytes.data(), _bytes.size());

    // spdlog::info("VolumeData::TryLoadPVM: successfully loaded {}x{}x{}, hasGradient={}", 
    //              _size.x, _size.y, _size.z, _hasGradientChannel);
    return true;
}

bool VolumeData::TryLoadMHD(std::filesystem::path const & file) {
    std::ifstream fin(file);
    if (!fin) return false;
    std::string line;
    std::string rawFile;
    int dimX=0, dimY=0, dimZ=0;
    while (std::getline(fin, line)) {
        if (line.find("DimSize") == 0) {
            std::sscanf(line.c_str(), "DimSize = %d %d %d", &dimX, &dimY, &dimZ);
        } else if (line.find("ElementDataFile") == 0) {
            auto pos = line.find('=');
            if (pos != std::string::npos) {
                rawFile = line.substr(pos+1);
                while (!rawFile.empty() && rawFile.front()==' ') rawFile.erase(rawFile.begin());
            }
        }
    }
    fin.close();

    if (dimX<=0 || dimY<=0 || dimZ<=0) return false;
    auto dir = file.parent_path();
    std::filesystem::path rawPath;
    if (rawFile.empty()) {
        rawPath = file;
        rawPath.replace_extension(".raw");
    } else if (rawFile == "LOCAL") {
        // spdlog::error("VolumeData::TryLoadMHD: LOCAL inline data not supported");
        return false;
    } else {
        rawPath = dir / rawFile;
    }

    auto blob = LoadBytes(rawPath);
    if (blob.empty()) return false;

    std::size_t expected = std::size_t(dimX) * dimY * dimZ;
    if (blob.size() < expected) {
        if (blob.size() == expected * 2) {
            _bytes.resize(expected);
            const unsigned short * src = reinterpret_cast<const unsigned short*>(blob.data());
            for (std::size_t i=0;i<expected;i++) {
                unsigned short v = src[i];
                unsigned char out = static_cast<unsigned char>(std::round((float)v / 65535.0f * 255.0f));
                _bytes[i] = std::byte(out);
            }
        } else {
            // spdlog::error("VolumeData::TryLoadMHD: raw size mismatch {} vs {}", blob.size(), expected);
            return false;
        }
    } else {
        _bytes.assign(blob.begin(), blob.begin() + expected);
    }

    _tex = VolumeTex(dimX, dimY, dimZ);
    std::memcpy(const_cast<std::byte*>(_tex.GetBytes().data()), _bytes.data(), _bytes.size());
    _size = {dimX, dimY, dimZ};
    _hasGradientChannel = false;
    _gradientBytes.clear();
    return true;
}

bool VolumeData::TryInferRaw(std::filesystem::path const & file) {
    auto blob = LoadBytes(file);
    if (blob.empty()) return false;
    std::size_t size = blob.size();
    
    std::size_t dim = std::size_t(std::round(std::cbrt((double)size)));
    if (dim*dim*dim == size && dim > 0) {
        _size = {int(dim), int(dim), int(dim)};
        _bytes.assign(blob.begin(), blob.end());
        _tex = VolumeTex(dim, dim, dim);
        std::memcpy(const_cast<std::byte*>(_tex.GetBytes().data()), _bytes.data(), _bytes.size());
        _hasGradientChannel = false;
        _gradientBytes.clear();
        return true;
    }
    
    for (std::size_t z = 1; z<=4096 && z*z*z <= size*64; ++z) {
        if (size % z != 0) continue;
        std::size_t rem = size / z;
        std::size_t y = std::sqrt((double)rem);
        for (std::size_t yy = std::max((std::size_t)1, y-2); yy<=y+2; ++yy) {
            if (rem % yy != 0) continue;
            std::size_t x = rem / yy;
            if (x>0 && yy>0) {
                _size = {int(x), int(yy), int(z)};
                _bytes.assign(blob.begin(), blob.end());
                _tex = VolumeTex(x, yy, z);
                std::memcpy(const_cast<std::byte*>(_tex.GetBytes().data()), _bytes.data(), _bytes.size());
                _hasGradientChannel = false;
                _gradientBytes.clear();
                return true;
            }
        }
    }
    return false;
}

unsigned VolumeData::UploadToGL() {
    if (_bytes.empty() || _size.x==0) return 0;
    if (_glTex==0) glGenTextures(1, &_glTex);
    glBindTexture(GL_TEXTURE_3D, _glTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, _size.x, _size.y, _size.z, 0, GL_RED, GL_UNSIGNED_BYTE, _bytes.data());
    glBindTexture(GL_TEXTURE_3D, 0);

    // 上传梯度通道
    if (_hasGradientChannel && !_gradientBytes.empty()) {
        if (_gradientTexGL==0) glGenTextures(1, &_gradientTexGL);
        glBindTexture(GL_TEXTURE_3D, _gradientTexGL);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, _size.x, _size.y, _size.z, 0, GL_RED, GL_UNSIGNED_BYTE, _gradientBytes.data());
        glBindTexture(GL_TEXTURE_3D, 0);
        // spdlog::info("VolumeData::UploadToGL: gradient texture uploaded");
    }

    // spdlog::info("VolumeData::UploadToGL: main texture {} uploaded", _glTex);
    return _glTex;
}

} // namespace VCX::Labs::Final