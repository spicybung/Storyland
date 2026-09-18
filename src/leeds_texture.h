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
    RwPsp,
    RwPc,
    Ps2,
    CtwTex,
    Dds
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
    uint32_t redMask = 0;
    uint32_t greenMask = 0;
    uint32_t blueMask = 0;
    uint32_t alphaMask = 0;
    int width = 0;
    int height = 0;
};

class LeedsTextureArchive {
public:
    bool loadFromFile(const std::wstring& path, LeedsPlatform platform, std::string& errorMessage);
    bool loadFromMemory(const std::vector<uint8_t>& bytes, LeedsPlatform platform, std::string& errorMessage, const std::wstring& virtualPath = L"");
    bool saveToFile(const std::wstring& path, std::string& errorMessage) const;
    const std::vector<LeedsTextureEntry>& textures() const;
    bool decodeTexture(size_t textureIndex, RgbaImage& image, std::string& errorMessage) const;
    bool replaceTexture(size_t textureIndex, const RgbaImage& image, std::string& errorMessage);
    bool replaceTextureAsBpp(size_t textureIndex, const RgbaImage& image, uint8_t targetBpp, std::string& errorMessage);
    bool renameTexture(size_t textureIndex, const std::string& newName, std::string& errorMessage);
    bool createEmptyPs2(const std::wstring& virtualPath, std::string& errorMessage);
    bool createLcsBetaTxd(const std::wstring& virtualPath, const std::string& firstName, const RgbaImage& image, uint8_t bpp, std::string& errorMessage);
    bool addTexture(const std::string& name, const RgbaImage& image, uint8_t bpp, std::string& errorMessage);
    bool swapTextureData(size_t firstTextureIndex, size_t secondTextureIndex, std::string& errorMessage);
    bool duplicateTexture(size_t textureIndex, const std::string& newName, std::string& errorMessage);
    bool removeTexture(size_t textureIndex, std::string& errorMessage);
    bool validateStructure(std::string& report, std::string& errorMessage) const;
    bool normalizePs2RuntimeLayout(std::string& report, std::string& errorMessage);
    const std::vector<uint8_t>& rawBytes() const;
    const std::wstring& sourcePath() const;

private:
    std::vector<uint8_t> dataBytes;
    std::vector<LeedsTextureEntry> entries;
    std::wstring path;
    LeedsPlatform loadedPlatform = LeedsPlatform::Auto;

    bool parse(LeedsPlatform platform, std::string& errorMessage);
};
