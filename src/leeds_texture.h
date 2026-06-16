#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct RgbaImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;
};

enum class LeedsPlatform {
    Auto,
    Psp,
    Ps2
};

enum class TextureKind {
    Unknown,
    Psp,
    Ps2
};

struct LeedsTextureEntry {
    std::string name;
    TextureKind kind = TextureKind::Unknown;
    uint32_t containerBase = 0;
    uint32_t textureHeaderOffset = 0;
    uint32_t rasterOffset = 0;
    uint32_t blockSize = 0;
    uint32_t flags = 0;
    uint32_t reserved0 = 0;
    uint32_t reserved1 = 0;
    uint16_t swizzleWidth = 0;
    uint8_t widthPow2 = 0;
    uint8_t heightPow2 = 0;
    uint8_t bpp = 0;
    uint8_t mipCount = 0;
    uint8_t swizzleMask = 0;
    int width = 0;
    int height = 0;
};

class LeedsTextureArchive {
public:
    bool loadFromFile(const std::wstring& path, LeedsPlatform platform, std::string& errorMessage);
    bool saveToFile(const std::wstring& path, std::string& errorMessage) const;
    const std::vector<LeedsTextureEntry>& textures() const;
    bool decodeTexture(size_t textureIndex, RgbaImage& image, std::string& errorMessage) const;
    bool replaceTexture(size_t textureIndex, const RgbaImage& image, std::string& errorMessage);
    bool replaceTextureAsBpp(size_t textureIndex, const RgbaImage& image, uint8_t targetBpp, std::string& errorMessage);
    bool renameTexture(size_t textureIndex, const std::string& newName, std::string& errorMessage);
    const std::wstring& sourcePath() const;

private:
    std::vector<uint8_t> dataBytes;
    std::vector<LeedsTextureEntry> entries;
    std::wstring path;
    LeedsPlatform loadedPlatform = LeedsPlatform::Auto;

    bool parse(LeedsPlatform platform, std::string& errorMessage);
};
