#include "leeds_texture.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <sstream>

static uint32_t readU32(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 4 > data.size()) return 0;
    return uint32_t(data[offset]) | (uint32_t(data[offset + 1]) << 8) | (uint32_t(data[offset + 2]) << 16) | (uint32_t(data[offset + 3]) << 24);
}

static uint16_t readU16(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 2 > data.size()) return 0;
    return uint16_t(data[offset]) | (uint16_t(data[offset + 1]) << 8);
}

static void writeU32(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
    if (offset + 4 > data.size()) return;
    data[offset + 0] = uint8_t(value & 0xFF);
    data[offset + 1] = uint8_t((value >> 8) & 0xFF);
    data[offset + 2] = uint8_t((value >> 16) & 0xFF);
    data[offset + 3] = uint8_t((value >> 24) & 0xFF);
}

static std::string readCString(const std::vector<uint8_t>& data, size_t offset, size_t length) {
    std::string text;
    for (size_t i = 0; i < length && offset + i < data.size(); ++i) {
        uint8_t c = data[offset + i];
        if (c == 0) break;
        text.push_back(char(c));
    }
    return text;
}

static bool isPrintableName(const std::string& value) {
    if (value.empty()) return false;
    for (char c : value) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 32 || uc >= 127) return false;
    }
    return true;
}

static uint32_t slotBaseFromSlotPtr(uint32_t slotPtr) {
    if (slotPtr <= 8) return 0;
    return slotPtr - 8;
}

static int safePow2(uint32_t exponent) {
    if (exponent >= 31) return 0;
    return 1 << exponent;
}

static bool isPowerOfTwoInt(int value) {
    return value >= 4 && value <= 4096 && (value & (value - 1)) == 0;
}

static uint8_t log2ExactInt(int value) {
    uint8_t exponent = 0;
    while (value > 1) {
        value >>= 1;
        ++exponent;
    }
    return exponent;
}

static bool dimensionsReasonable(int width, int height) {
    return width >= 4 && height >= 4 && width <= 4096 && height <= 4096;
}

static bool bppReasonablePsp(uint32_t bpp) {
    return bpp == 4 || bpp == 8 || bpp == 32;
}

static bool bppReasonablePs2(uint32_t bpp) {
    return bpp == 4 || bpp == 8 || bpp == 16 || bpp == 32;
}

static bool parsePspHeader(const std::vector<uint8_t>& data, uint32_t offset, LeedsTextureEntry& entry) {
    if (offset == 0 || size_t(offset) + 16 > data.size()) return false;
    uint32_t rasterOffset = readU32(data, offset + 4);
    uint16_t swizzleWidth = readU16(data, offset + 8);
    uint8_t widthPow2 = data[offset + 10];
    uint8_t heightPow2 = data[offset + 11];
    uint8_t bpp = data[offset + 12];
    uint8_t mipCount = data[offset + 13];
    int width = safePow2(widthPow2);
    int height = safePow2(heightPow2);
    if (!dimensionsReasonable(width, height) || !bppReasonablePsp(bpp)) return false;
    if (rasterOffset == 0 || rasterOffset >= data.size()) return false;
    entry.kind = TextureKind::Psp;
    entry.textureHeaderOffset = offset;
    entry.rasterOffset = rasterOffset;
    entry.swizzleWidth = swizzleWidth;
    entry.widthPow2 = widthPow2;
    entry.heightPow2 = heightPow2;
    entry.bpp = bpp;
    entry.mipCount = mipCount;
    entry.width = width;
    entry.height = height;
    return true;
}

static bool parsePs2Header(const std::vector<uint8_t>& data, uint32_t offset, LeedsTextureEntry& entry) {
    if (offset == 0 || size_t(offset) + 16 > data.size()) return false;
    uint32_t reserved0 = readU32(data, offset + 0);
    uint32_t reserved1 = readU32(data, offset + 4);
    uint32_t rasterOffset = readU32(data, offset + 8);
    uint32_t flags = readU32(data, offset + 12);

    uint32_t swizzle = flags & 0xFF;
    uint32_t mipCount = (flags >> 8) & 0xF;
    uint32_t bpp = (flags >> 14) & 0x3F;
    uint32_t widthPow2 = (flags >> 20) & 0x3F;
    uint32_t heightPow2 = (flags >> 26) & 0x3F;
    int width = safePow2(widthPow2);
    int height = safePow2(heightPow2);

    uint32_t heightPow2Alt = flags & 0x3F;
    uint32_t widthPow2Alt = (flags >> 6) & 0x3F;
    uint32_t bppAlt = (flags >> 12) & 0x3F;
    uint32_t unknownAlt = (flags >> 18) & 0x3;
    uint32_t mipAlt = (flags >> 20) & 0xF;
    uint32_t swizzleAlt = (flags >> 24) & 0xFF;
    int widthAlt = safePow2(widthPow2Alt);
    int heightAlt = safePow2(heightPow2Alt);

    bool standardBad = !dimensionsReasonable(width, height) || !bppReasonablePs2(bpp);
    bool alternateGood = dimensionsReasonable(widthAlt, heightAlt) && bppReasonablePs2(bppAlt);
    if (standardBad && alternateGood) {
        swizzle = swizzleAlt;
        mipCount = mipAlt;
        bpp = bppAlt;
        widthPow2 = widthPow2Alt;
        heightPow2 = heightPow2Alt;
        width = widthAlt;
        height = heightAlt;
        flags = (swizzle & 0xFF) | ((mipCount & 0xF) << 8) | ((unknownAlt & 0x3) << 12) | ((bpp & 0x3F) << 14) | ((widthPow2 & 0x3F) << 20) | ((heightPow2 & 0x3F) << 26);
    }

    if (!dimensionsReasonable(width, height) || !bppReasonablePs2(bpp)) return false;
    if (rasterOffset == 0 || rasterOffset >= data.size()) return false;

    entry.kind = TextureKind::Ps2;
    entry.textureHeaderOffset = offset;
    entry.rasterOffset = rasterOffset;
    entry.reserved0 = reserved0;
    entry.reserved1 = reserved1;
    entry.flags = flags;
    entry.swizzleMask = uint8_t(swizzle);
    entry.mipCount = uint8_t(mipCount);
    entry.bpp = uint8_t(bpp);
    entry.widthPow2 = uint8_t(widthPow2);
    entry.heightPow2 = uint8_t(heightPow2);
    entry.width = width;
    entry.height = height;
    return true;
}

static bool finishDecodedTexture(RgbaImage& image);

static bool parseCtwStandaloneTex(const std::vector<uint8_t>& data, LeedsTextureEntry& entry) {
    if (data.size() < 12) return false;

    uint16_t width = readU16(data, 0x00);
    uint16_t height = readU16(data, 0x02);
    uint16_t marker = readU16(data, 0x04);
    uint16_t format = readU16(data, 0x06);
    uint32_t storedBytes = readU32(data, 0x08);

    if (!dimensionsReasonable(width, height)) return false;
    if ((marker & 0xFF00u) != 0x8C00u) return false;
    if (storedBytes == 0 || uint64_t(12u) + uint64_t(storedBytes) > uint64_t(data.size())) return false;

    entry = LeedsTextureEntry{};
    entry.name = "texture";
    entry.kind = TextureKind::CtwTex;
    entry.textureHeaderOffset = 0;
    entry.rasterOffset = 12;
    entry.blockSize = storedBytes;
    entry.flags = uint32_t(format) | (uint32_t(marker) << 16);
    entry.reserved0 = marker;
    entry.reserved1 = format;
    entry.width = width;
    entry.height = height;
    entry.widthPow2 = isPowerOfTwoInt(width) ? log2ExactInt(width) : 0;
    entry.heightPow2 = isPowerOfTwoInt(height) ? log2ExactInt(height) : 0;

    if (format == 0x0004u || format == 0x0104u) {
        uint32_t need = uint32_t(((uint32_t(width) + 3u) / 4u) * ((uint32_t(height) + 3u) / 4u) * 8u);
        if (storedBytes < need) return false;
        entry.bpp = 4;
        return true;
    }

    if (format == 0x0002u || format == 0x0102u) {
        uint32_t need = uint32_t(((uint32_t(width) + 7u) / 8u) * ((uint32_t(height) + 3u) / 4u) * 8u);
        if (storedBytes < need) return false;
        entry.bpp = 2;
        return true;
    }

    return false;
}

static uint32_t ctwPart1By1(uint32_t value) {
    value &= 0x0000FFFFu;
    value = (value | (value << 8)) & 0x00FF00FFu;
    value = (value | (value << 4)) & 0x0F0F0F0Fu;
    value = (value | (value << 2)) & 0x33333333u;
    value = (value | (value << 1)) & 0x55555555u;
    return value;
}

static uint32_t ctwMortonIndex(uint32_t x, uint32_t y) {
    return ctwPart1By1(x) | (ctwPart1By1(y) << 1);
}

static uint8_t ctwExpand3(uint32_t value) {
    value &= 0x7u;
    return uint8_t((value << 5) | (value << 2) | (value >> 1));
}

static uint8_t ctwExpand4(uint32_t value) {
    value &= 0xFu;
    return uint8_t((value << 4) | value);
}

static uint8_t ctwExpand5(uint32_t value) {
    value &= 0x1Fu;
    return uint8_t((value << 3) | (value >> 2));
}

static void ctwPvrtcColorA(uint32_t colorData, uint8_t out[4]) {
    if (colorData & 0x8000u) {
        out[0] = ctwExpand5((colorData >> 10) & 0x1Fu);
        out[1] = ctwExpand5((colorData >> 5) & 0x1Fu);
        out[2] = ctwExpand4((colorData >> 1) & 0x0Fu);
        out[3] = 255;
    } else {
        out[0] = ctwExpand4((colorData >> 8) & 0x0Fu);
        out[1] = ctwExpand4((colorData >> 4) & 0x0Fu);
        out[2] = ctwExpand3((colorData >> 1) & 0x07u);
        out[3] = ctwExpand3((colorData >> 12) & 0x07u);
    }
}

static void ctwPvrtcColorB(uint32_t colorData, uint8_t out[4]) {
    if (colorData & 0x80000000u) {
        out[0] = ctwExpand5((colorData >> 26) & 0x1Fu);
        out[1] = ctwExpand5((colorData >> 21) & 0x1Fu);
        out[2] = ctwExpand5((colorData >> 16) & 0x1Fu);
        out[3] = 255;
    } else {
        out[0] = ctwExpand4((colorData >> 24) & 0x0Fu);
        out[1] = ctwExpand4((colorData >> 20) & 0x0Fu);
        out[2] = ctwExpand4((colorData >> 16) & 0x0Fu);
        out[3] = ctwExpand3((colorData >> 28) & 0x07u);
    }
}

static bool decodeCtwPvrtcTexture(const std::vector<uint8_t>& data, const LeedsTextureEntry& entry, RgbaImage& image, std::string& errorMessage) {
    int width = entry.width;
    int height = entry.height;
    if (width <= 0 || height <= 0) {
        errorMessage = "Invalid CTW TEX dimensions.";
        return false;
    }

    bool twoBpp = (uint16_t(entry.flags & 0xFFFFu) == 0x0002u || uint16_t(entry.flags & 0xFFFFu) == 0x0102u);
    uint32_t blockWidth = twoBpp ? 8u : 4u;
    uint32_t blockHeight = 4u;
    uint32_t blocksX = uint32_t((uint32_t(width) + blockWidth - 1u) / blockWidth);
    uint32_t blocksY = uint32_t((uint32_t(height) + blockHeight - 1u) / blockHeight);
    uint64_t need = uint64_t(blocksX) * uint64_t(blocksY) * 8ull;
    if (uint64_t(entry.rasterOffset) + need > uint64_t(data.size())) {
        errorMessage = "CTW TEX PVRTC payload is truncated.";
        return false;
    }

    image.width = width;
    image.height = height;
    image.rgba.assign(size_t(width) * size_t(height) * 4u, 255);

    const uint8_t* src = data.data() + entry.rasterOffset;
    bool mortonBlocks = isPowerOfTwoInt(int(blocksX)) && isPowerOfTwoInt(int(blocksY));
    uint32_t blockCount = blocksX * blocksY;
    if (mortonBlocks && ctwMortonIndex(blocksX - 1u, blocksY - 1u) >= blockCount) mortonBlocks = false;

    for (uint32_t by = 0; by < blocksY; ++by) {
        for (uint32_t bx = 0; bx < blocksX; ++bx) {
            uint32_t blockIndex = mortonBlocks ? ctwMortonIndex(bx, by) : (by * blocksX + bx);
            size_t pos = size_t(blockIndex) * 8u;
            if (pos + 8u > size_t(need)) continue;

            uint32_t modulation = uint32_t(src[pos + 0]) | (uint32_t(src[pos + 1]) << 8) | (uint32_t(src[pos + 2]) << 16) | (uint32_t(src[pos + 3]) << 24);
            uint32_t colorData = uint32_t(src[pos + 4]) | (uint32_t(src[pos + 5]) << 8) | (uint32_t(src[pos + 6]) << 16) | (uint32_t(src[pos + 7]) << 24);

            uint8_t colorA[4] = {};
            uint8_t colorB[4] = {};
            ctwPvrtcColorA(colorData, colorA);
            ctwPvrtcColorB(colorData, colorB);

            for (uint32_t py = 0; py < blockHeight; ++py) {
                for (uint32_t px = 0; px < blockWidth; ++px) {
                    uint32_t x = bx * blockWidth + px;
                    uint32_t y = by * blockHeight + py;
                    if (x >= uint32_t(width) || y >= uint32_t(height)) continue;

                    uint32_t weight = 0;
                    if (twoBpp) {
                        uint32_t bit = (modulation >> (py * blockWidth + px)) & 0x1u;
                        weight = bit ? 8u : 0u;
                    } else {
                        uint32_t code = (modulation >> (2u * (py * blockWidth + px))) & 0x3u;
                        static const uint32_t weights[4] = {0u, 3u, 5u, 8u};
                        weight = weights[code];
                    }

                    size_t dst = (size_t(y) * size_t(width) + size_t(x)) * 4u;
                    for (int ch = 0; ch < 4; ++ch) {
                        image.rgba[dst + size_t(ch)] = uint8_t((uint32_t(colorA[ch]) * (8u - weight) + uint32_t(colorB[ch]) * weight) / 8u);
                    }
                }
            }
        }
    }

    return finishDecodedTexture(image);
}

static bool decodeCtwUnknownTexture(const std::vector<uint8_t>& data, const LeedsTextureEntry& entry, RgbaImage& image, std::string& errorMessage) {
    int width = entry.width;
    int height = entry.height;
    if (width <= 0 || height <= 0) {
        errorMessage = "Invalid CTW TEX dimensions.";
        return false;
    }

    image.width = width;
    image.height = height;
    image.rgba.assign(size_t(width) * size_t(height) * 4u, 255);
    const uint8_t* src = data.data() + entry.rasterOffset;
    uint32_t available = entry.blockSize;
    for (size_t i = 0; i < size_t(width) * size_t(height); ++i) {
        uint8_t v = available ? src[i % available] : 0;
        image.rgba[i * 4 + 0] = v;
        image.rgba[i * 4 + 1] = v;
        image.rgba[i * 4 + 2] = v;
        image.rgba[i * 4 + 3] = 255;
    }
    return finishDecodedTexture(image);
}

static uint32_t swizzlePs2Index(int x, int y, int logw) {
    uint32_t nx = uint32_t(x & 7) ^ (uint32_t(((y >> 1) ^ (y >> 2)) << 2));
    nx = (nx & 7) | (uint32_t((x >> 1) & ~7));
    uint32_t ny = uint32_t(y & 1) | uint32_t((y >> 1) & ~1);
    uint32_t n = uint32_t((y >> 1) & 1) | (uint32_t((x >> 3) & 1) << 1);
    return n | (nx << 2) | (ny << uint32_t(logw - 1 + 2));
}

static int log2Width(int width) {
    int logw = 0;
    int value = 1;
    while (value < width) {
        value <<= 1;
        ++logw;
    }
    return logw;
}

static std::vector<uint8_t> unswizzlePs2Indices(const std::vector<uint8_t>& indices, int width, int height) {
    std::vector<uint8_t> out(size_t(width) * size_t(height));
    int logw = log2Width(width);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t source = swizzlePs2Index(x, y, logw);
            out[size_t(y) * size_t(width) + size_t(x)] = indices[source % indices.size()];
        }
    }
    return out;
}

static std::vector<uint8_t> swizzlePs2Indices(const std::vector<uint8_t>& linear, int width, int height) {
    std::vector<uint8_t> out(linear.size());
    int logw = log2Width(width);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t target = swizzlePs2Index(x, y, logw);
            out[target % out.size()] = linear[size_t(y) * size_t(width) + size_t(x)];
        }
    }
    return out;
}

static void convertClutPs2(std::vector<uint8_t>& indices) {
    static const uint8_t mapping[4] = {0x00, 0x10, 0x08, 0x18};
    for (uint8_t& value : indices) {
        value = uint8_t((value & uint8_t(~0x18)) | mapping[(value & 0x18) >> 3]);
    }
}

static std::vector<uint8_t> inverseConvertClutPs2(std::vector<uint8_t> indices) {
    for (uint8_t& value : indices) {
        uint8_t group = value & 0x18;
        uint8_t original = 0;
        if (group == 0x00) original = 0x00;
        else if (group == 0x10) original = 0x08;
        else if (group == 0x08) original = 0x10;
        else original = 0x18;
        value = uint8_t((value & uint8_t(~0x18)) | original);
    }
    return indices;
}

static std::vector<uint8_t> expandNibblesLoFirst(const uint8_t* bytes, size_t byteCount, size_t pixelCount) {
    std::vector<uint8_t> indices(pixelCount);
    for (size_t i = 0; i < pixelCount; ++i) {
        uint8_t b = bytes[i / 2];
        indices[i] = (i & 1) ? uint8_t(b >> 4) : uint8_t(b & 0x0F);
    }
    return indices;
}

static std::vector<uint8_t> packNibblesLoFirst(const std::vector<uint8_t>& indices) {
    std::vector<uint8_t> bytes((indices.size() + 1) / 2, 0);
    for (size_t i = 0; i < indices.size(); ++i) {
        if (i & 1) bytes[i / 2] |= uint8_t((indices[i] & 0x0F) << 4);
        else bytes[i / 2] |= uint8_t(indices[i] & 0x0F);
    }
    return bytes;
}

static void flipRgbaRowsInPlace(RgbaImage& image) {
    if (image.width <= 0 || image.height <= 1 || image.rgba.empty()) return;
    size_t rowBytes = size_t(image.width) * 4u;
    std::vector<uint8_t> temp(rowBytes);
    for (int y = 0; y < image.height / 2; ++y) {
        size_t top = size_t(y) * rowBytes;
        size_t bottom = size_t(image.height - 1 - y) * rowBytes;
        std::copy(image.rgba.begin() + top, image.rgba.begin() + top + rowBytes, temp.begin());
        std::copy(image.rgba.begin() + bottom, image.rgba.begin() + bottom + rowBytes, image.rgba.begin() + top);
        std::copy(temp.begin(), temp.end(), image.rgba.begin() + bottom);
    }
}

static RgbaImage flippedRgbaRowsCopy(const RgbaImage& image) {
    RgbaImage copy = image;
    flipRgbaRowsInPlace(copy);
    return copy;
}

static bool finishDecodedTexture(RgbaImage&) {
    // Default viewer/export orientation is the archive's decoded row order.
    // The old forced flip made many CHK/XTX textures appear upside down because
    // Storyland already displays DIBs as top-down images. The UI now has an
    // explicit texture-preview flip toggle instead of mutating decoded pixels.
    return true;
}

static std::vector<uint8_t> unswizzlePspBytes(const uint8_t* source, size_t sourceSize, int width, int height, int bytesPerPixelNumerator, int bytesPerPixelDenominator) {
    int minWidth = 16;
    if (bytesPerPixelNumerator == 4 && bytesPerPixelDenominator == 1) minWidth = 4;
    if (bytesPerPixelNumerator == 1 && bytesPerPixelDenominator == 2) minWidth = 32;
    int bufw = std::max(width, minWidth);
    int stride = (bufw * bytesPerPixelNumerator) / bytesPerPixelDenominator;
    int rowBytes = (width * bytesPerPixelNumerator) / bytesPerPixelDenominator;
    std::vector<uint8_t> temp(size_t(stride) * size_t(height), 0);
    int nbx = stride / 16;
    int nby = (height + 7) / 8;
    size_t sourcePos = 0;
    for (int yb = 0; yb < nby; ++yb) {
        int rowBase = yb * 8 * stride;
        for (int xb = 0; xb < nbx; ++xb) {
            int blockBase = rowBase + xb * 16;
            int destOffset = blockBase;
            for (int n = 0; n < 8; ++n) {
                if (sourcePos + 16 <= sourceSize && destOffset + 16 <= int(temp.size())) {
                    std::copy(source + sourcePos, source + sourcePos + 16, temp.begin() + destOffset);
                }
                sourcePos += 16;
                destOffset += stride;
            }
        }
    }
    std::vector<uint8_t> out(size_t(rowBytes) * size_t(height));
    for (int y = 0; y < height; ++y) {
        std::copy(temp.begin() + size_t(y) * size_t(stride), temp.begin() + size_t(y) * size_t(stride) + rowBytes, out.begin() + size_t(y) * size_t(rowBytes));
    }
    return out;
}

static std::vector<uint8_t> swizzlePspBytes(const uint8_t* linear, size_t linearSize, int width, int height, int bytesPerPixelNumerator, int bytesPerPixelDenominator) {
    int minWidth = 16;
    if (bytesPerPixelNumerator == 4 && bytesPerPixelDenominator == 1) minWidth = 4;
    if (bytesPerPixelNumerator == 1 && bytesPerPixelDenominator == 2) minWidth = 32;
    int bufw = std::max(width, minWidth);
    int stride = (bufw * bytesPerPixelNumerator) / bytesPerPixelDenominator;
    int rowBytes = (width * bytesPerPixelNumerator) / bytesPerPixelDenominator;
    std::vector<uint8_t> temp(size_t(stride) * size_t(height), 0);
    for (int y = 0; y < height; ++y) {
        std::copy(linear + size_t(y) * size_t(rowBytes), linear + size_t(y) * size_t(rowBytes) + rowBytes, temp.begin() + size_t(y) * size_t(stride));
    }
    std::vector<uint8_t> out(linearSize, 0);
    int nbx = stride / 16;
    int nby = (height + 7) / 8;
    size_t outPos = 0;
    for (int yb = 0; yb < nby; ++yb) {
        int rowBase = yb * 8 * stride;
        for (int xb = 0; xb < nbx; ++xb) {
            int blockBase = rowBase + xb * 16;
            int sourceOffset = blockBase;
            for (int n = 0; n < 8; ++n) {
                if (outPos + 16 <= out.size() && sourceOffset + 16 <= int(temp.size())) {
                    std::copy(temp.begin() + sourceOffset, temp.begin() + sourceOffset + 16, out.begin() + outPos);
                }
                outPos += 16;
                sourceOffset += stride;
            }
        }
    }
    return out;
}

static uint32_t bytesForTextureLevel(int width, int height, uint8_t bpp) {
    if (width <= 0 || height <= 0) return 0;

    uint64_t pixelCount = uint64_t(width) * uint64_t(height);
    uint64_t byteCount = 0;

    if (bpp == 4) byteCount = (pixelCount + 1u) / 2u;
    else if (bpp == 8) byteCount = pixelCount;
    else if (bpp == 16) byteCount = pixelCount * 2u;
    else if (bpp == 32) byteCount = pixelCount * 4u;

    if (byteCount > 0xFFFFFFFFu) return 0;
    return uint32_t(byteCount);
}

static uint32_t rasterPayloadBytesBeforePalette(const LeedsTextureEntry& entry) {
    int mipCount = int(entry.mipCount);
    if (mipCount <= 0) mipCount = 1;
    if (mipCount > 16) mipCount = 16;

    uint64_t total = 0;
    int width = entry.width;
    int height = entry.height;

    for (int level = 0; level < mipCount; ++level) {
        total += bytesForTextureLevel(width, height, entry.bpp);
        if (total > 0xFFFFFFFFu) return 0;
        width = std::max(1, width >> 1);
        height = std::max(1, height >> 1);
    }

    return uint32_t(total);
}

static bool paletteLooksUseful(const std::vector<uint8_t>& data, uint32_t paletteStart, uint32_t paletteBytes) {
    if (size_t(paletteStart) + paletteBytes > data.size()) return false;

    int nonZero = 0;
    int nonPad = 0;
    int varied = 0;
    uint32_t first = readU32(data, paletteStart);

    for (uint32_t offset = 0; offset + 4 <= paletteBytes; offset += 4) {
        uint32_t value = readU32(data, paletteStart + offset);
        if (value != 0) ++nonZero;
        if (value != 0xAAAAAAAAu && value != 0xCCCCCCCCu && value != 0x00000000u) ++nonPad;
        if (value != first) ++varied;
    }

    return nonZero > 0 && nonPad > 0 && varied > 0;
}

static bool findPaletteStartForEntry(const std::vector<uint8_t>& data, const LeedsTextureEntry& entry, int paletteEntries, uint32_t& paletteStartOut) {
    uint32_t paletteBytes = uint32_t(paletteEntries * 4);
    std::vector<uint32_t> paletteStartCandidates;

    // PSP xet stores the palette at the end of the raster block.  The bytes
    // immediately after the base level are usually mip data / padding, and they
    // look "varied" enough to fool the old usefulness test into treating them as
    // a palette.  PS2 archives more often use the immediate-palette layout, so
    // keep that order for PS2.
    if (entry.kind == TextureKind::Psp && entry.blockSize >= paletteBytes) {
        paletteStartCandidates.push_back(entry.rasterOffset + entry.blockSize - paletteBytes);
    }

    uint32_t payloadBytes = rasterPayloadBytesBeforePalette(entry);
    if (payloadBytes != 0) {
        paletteStartCandidates.push_back(entry.rasterOffset + payloadBytes);
    }

    if (entry.kind != TextureKind::Psp && entry.blockSize >= paletteBytes) {
        paletteStartCandidates.push_back(entry.rasterOffset + entry.blockSize - paletteBytes);
    }

    std::vector<uint32_t> uniqueCandidates;
    for (uint32_t candidate : paletteStartCandidates) {
        if (std::find(uniqueCandidates.begin(), uniqueCandidates.end(), candidate) == uniqueCandidates.end()) {
            uniqueCandidates.push_back(candidate);
        }
    }

    for (uint32_t candidate : uniqueCandidates) {
        if (size_t(candidate) + paletteBytes <= data.size() && paletteLooksUseful(data, candidate, paletteBytes)) {
            paletteStartOut = candidate;
            return true;
        }
    }

    for (uint32_t candidate : uniqueCandidates) {
        if (size_t(candidate) + paletteBytes <= data.size()) {
            paletteStartOut = candidate;
            return true;
        }
    }

    return false;
}

static uint32_t paletteBytesForEntry(const LeedsTextureEntry& entry) {
    if (entry.bpp == 4) return 16u * 4u;
    if (entry.bpp == 8) return 256u * 4u;
    return 0;
}

static uint32_t rasterBaseBytesForDimensions(int width, int height, uint8_t bpp) {
    return bytesForTextureLevel(width, height, bpp);
}

static uint32_t rasterStoredBytesForEntry(const LeedsTextureEntry& entry) {
    uint32_t withMips = rasterPayloadBytesBeforePalette(entry);
    if (withMips != 0) return withMips;
    return rasterBaseBytesForDimensions(entry.width, entry.height, entry.bpp);
}

static void appendOffsetField(std::vector<uint32_t>& fields, uint32_t offset) {
    if (offset == 0) return;
    if (std::find(fields.begin(), fields.end(), offset) == fields.end()) fields.push_back(offset);
}

static uint32_t shiftedOffsetValue(uint32_t value, uint32_t oldEnd, int64_t delta) {
    if (value >= oldEnd) return uint32_t(int64_t(value) + delta);
    return value;
}

static uint32_t shiftedFieldOffset(uint32_t fieldOffset, uint32_t oldEnd, int64_t delta) {
    if (fieldOffset >= oldEnd) return uint32_t(int64_t(fieldOffset) + delta);
    return fieldOffset;
}

static std::vector<uint32_t> collectKnownOffsetFields(const std::vector<uint8_t>& data, const std::vector<LeedsTextureEntry>& entries) {
    std::vector<uint32_t> fields;

    appendOffsetField(fields, 0x0C);
    appendOffsetField(fields, 0x10);
    appendOffsetField(fields, 0x24);
    appendOffsetField(fields, 0x28);
    appendOffsetField(fields, 0x2C);

    for (const LeedsTextureEntry& entry : entries) {
        if (entry.containerBase + 0x10 <= data.size()) {
            appendOffsetField(fields, entry.containerBase + 0x00);
            appendOffsetField(fields, entry.containerBase + 0x04);
            appendOffsetField(fields, entry.containerBase + 0x08);
            appendOffsetField(fields, entry.containerBase + 0x0C);
        }
        if (entry.kind == TextureKind::Psp) {
            appendOffsetField(fields, entry.textureHeaderOffset + 0x04);
        } else if (entry.kind == TextureKind::Ps2) {
            appendOffsetField(fields, entry.textureHeaderOffset + 0x08);
        }
    }

    uint32_t globalA = readU32(data, 0x0C);
    uint32_t globalB = readU32(data, 0x10);
    uint32_t globals = globalA != 0 ? globalA : globalB;
    uint32_t globalCount = readU32(data, 0x14);
    if (globals >= 0x30 && globals + globalCount * 4u <= data.size() && globalCount < 0x200000) {
        for (uint32_t i = 0; i < globalCount; ++i) {
            appendOffsetField(fields, globals + i * 4u);
            uint32_t pointerField = readU32(data, globals + i * 4u);
            if (pointerField + 4 <= data.size()) appendOffsetField(fields, pointerField);
        }
    }

    std::sort(fields.begin(), fields.end());
    fields.erase(std::unique(fields.begin(), fields.end()), fields.end());
    return fields;
}

static void applyKnownOffsetShift(std::vector<uint8_t>& data, const std::vector<uint32_t>& oldFields, uint32_t oldEnd, int64_t delta, uint32_t oldFileSize) {
    if (delta == 0) return;

    if (data.size() >= 0x0C) {
        writeU32(data, 0x08, uint32_t(data.size()));
    }

    for (uint32_t oldFieldOffset : oldFields) {
        if (oldFieldOffset + 4 > oldFileSize) continue;
        uint32_t newFieldOffset = shiftedFieldOffset(oldFieldOffset, oldEnd, delta);
        if (newFieldOffset + 4 > data.size()) continue;

        uint32_t value = readU32(data, newFieldOffset);
        if (value >= oldEnd && value <= oldFileSize) {
            writeU32(data, newFieldOffset, shiftedOffsetValue(value, oldEnd, delta));
        }
    }
}

static bool writeTextureHeaderDimensions(std::vector<uint8_t>& data, const LeedsTextureEntry& entry, int width, int height, uint32_t rasterOffset, std::string& errorMessage) {
    if (!isPowerOfTwoInt(width) || !isPowerOfTwoInt(height)) {
        errorMessage = "Replacement image dimensions must be powers of two between 4 and 4096.";
        return false;
    }

    uint8_t widthPow2 = log2ExactInt(width);
    uint8_t heightPow2 = log2ExactInt(height);

    if (entry.kind == TextureKind::Psp) {
        if (entry.textureHeaderOffset + 16 > data.size()) {
            errorMessage = "PSP texture header points outside the file.";
            return false;
        }
        writeU32(data, entry.textureHeaderOffset + 4, rasterOffset);
        uint16_t swizzleWidth = entry.swizzleWidth ? uint16_t(entry.bpp == 4 ? std::max(1, width / 2) : width) : 0;
        data[entry.textureHeaderOffset + 8] = uint8_t(swizzleWidth & 0xFF);
        data[entry.textureHeaderOffset + 9] = uint8_t((swizzleWidth >> 8) & 0xFF);
        data[entry.textureHeaderOffset + 10] = widthPow2;
        data[entry.textureHeaderOffset + 11] = heightPow2;
        data[entry.textureHeaderOffset + 12] = entry.bpp;
        data[entry.textureHeaderOffset + 13] = 1;
        return true;
    }

    if (entry.kind == TextureKind::Ps2) {
        if (entry.textureHeaderOffset + 16 > data.size()) {
            errorMessage = "PS2 texture header points outside the file.";
            return false;
        }
        writeU32(data, entry.textureHeaderOffset + 8, rasterOffset);
        uint32_t unknown = (entry.flags >> 12) & 0x3u;
        uint32_t flags =
            (uint32_t(entry.swizzleMask) & 0xFFu) |
            (1u << 8) |
            ((unknown & 0x3u) << 12) |
            ((uint32_t(entry.bpp) & 0x3Fu) << 14) |
            ((uint32_t(widthPow2) & 0x3Fu) << 20) |
            ((uint32_t(heightPow2) & 0x3Fu) << 26);
        writeU32(data, entry.textureHeaderOffset + 12, flags);
        return true;
    }

    errorMessage = "Unknown texture kind.";
    return false;
}

static std::vector<uint8_t> readPalette(const std::vector<uint8_t>& data, const LeedsTextureEntry& entry, int paletteEntries, bool ps2Alpha) {
    std::vector<uint8_t> palette(size_t(paletteEntries) * 4, 0);
    uint32_t paletteBytes = uint32_t(paletteEntries * 4);

    std::vector<uint32_t> paletteStartCandidates;

    // Platform-specific palette order:
    //
    // PS2 xet/chk usually stores the palette immediately after the raster/base
    // payload.  That fixed the older PS2 EMPHUD/ty_loan-style failures.
    //
    // PSP xet is different in the regression corpus: the palette sits at the
    // end of the raster block, after mip bytes / padding.  The immediate
    // post-base bytes are valid-looking mip data, so the old "first useful
    // palette" heuristic picked mip pixels as colours and made PSP textures look
    // scrambled/garbage.
    if (entry.kind == TextureKind::Psp && entry.blockSize >= paletteBytes) {
        paletteStartCandidates.push_back(entry.rasterOffset + entry.blockSize - paletteBytes);
    }

    uint32_t payloadBytes = rasterPayloadBytesBeforePalette(entry);
    if (payloadBytes != 0) {
        paletteStartCandidates.push_back(entry.rasterOffset + payloadBytes);
    }

    if (entry.kind != TextureKind::Psp && entry.blockSize >= paletteBytes) {
        paletteStartCandidates.push_back(entry.rasterOffset + entry.blockSize - paletteBytes);
    }

    std::vector<uint32_t> uniqueCandidates;
    for (uint32_t candidate : paletteStartCandidates) {
        if (std::find(uniqueCandidates.begin(), uniqueCandidates.end(), candidate) == uniqueCandidates.end()) {
            uniqueCandidates.push_back(candidate);
        }
    }

    uint32_t paletteStart = 0;
    bool havePalette = false;
    for (uint32_t candidate : uniqueCandidates) {
        if (size_t(candidate) + paletteBytes <= data.size() && paletteLooksUseful(data, candidate, paletteBytes)) {
            paletteStart = candidate;
            havePalette = true;
            break;
        }
    }

    if (!havePalette) {
        for (uint32_t candidate : uniqueCandidates) {
            if (size_t(candidate) + paletteBytes <= data.size()) {
                paletteStart = candidate;
                havePalette = true;
                break;
            }
        }
    }

    if (!havePalette) return palette;

    for (int i = 0; i < paletteEntries; ++i) {
        uint8_t r = data[paletteStart + i * 4 + 0];
        uint8_t g = data[paletteStart + i * 4 + 1];
        uint8_t b = data[paletteStart + i * 4 + 2];
        uint8_t a = data[paletteStart + i * 4 + 3];
        if (ps2Alpha) a = uint8_t(std::min(255, int(a) * 255 / 128));
        palette[size_t(i) * 4 + 0] = r;
        palette[size_t(i) * 4 + 1] = g;
        palette[size_t(i) * 4 + 2] = b;
        palette[size_t(i) * 4 + 3] = a;
    }
    return palette;
}

static int nearestPaletteIndex(const std::vector<uint8_t>& palette, int paletteEntries, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int bestIndex = 0;
    int bestScore = std::numeric_limits<int>::max();
    for (int i = 0; i < paletteEntries; ++i) {
        int dr = int(r) - int(palette[size_t(i) * 4 + 0]);
        int dg = int(g) - int(palette[size_t(i) * 4 + 1]);
        int db = int(b) - int(palette[size_t(i) * 4 + 2]);
        int da = int(a) - int(palette[size_t(i) * 4 + 3]);
        int score = dr * dr + dg * dg + db * db + (da * da) / 2;
        if (score < bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }
    return bestIndex;
}

struct PaletteSourceColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
    uint32_t count = 0;
};

struct PaletteBucket {
    std::vector<size_t> indices;
};

static int paletteSourceChannel(const PaletteSourceColor& color, int channel) {
    if (channel == 0) return color.r;
    if (channel == 1) return color.g;
    if (channel == 2) return color.b;
    return color.a;
}

static uint64_t paletteBucketWeight(const std::vector<PaletteSourceColor>& colors, const PaletteBucket& bucket) {
    uint64_t weight = 0;
    for (size_t index : bucket.indices) weight += colors[index].count;
    return weight;
}

static int paletteBucketRange(const std::vector<PaletteSourceColor>& colors, const PaletteBucket& bucket, int channel) {
    if (bucket.indices.empty()) return 0;
    int lo = 255;
    int hi = 0;
    for (size_t index : bucket.indices) {
        int value = paletteSourceChannel(colors[index], channel);
        lo = std::min(lo, value);
        hi = std::max(hi, value);
    }
    return hi - lo;
}

static std::vector<uint8_t> buildPaletteFromReplacementImage(const RgbaImage& image, int paletteEntries, bool ps2Alpha) {
    std::map<uint32_t, uint32_t> histogram;

    size_t pixelCount = size_t(image.width) * size_t(image.height);
    for (size_t i = 0; i < pixelCount; ++i) {
        uint8_t r = image.rgba[i * 4 + 0];
        uint8_t g = image.rgba[i * 4 + 1];
        uint8_t b = image.rgba[i * 4 + 2];
        uint8_t a = image.rgba[i * 4 + 3];

        if (a < 8) {
            r = 0;
            g = 0;
            b = 0;
            a = 0;
        }

        uint32_t key =
            uint32_t(r) |
            (uint32_t(g) << 8) |
            (uint32_t(b) << 16) |
            (uint32_t(a) << 24);
        histogram[key]++;
    }

    std::vector<PaletteSourceColor> colors;
    colors.reserve(histogram.size());
    for (const auto& item : histogram) {
        PaletteSourceColor color;
        color.r = uint8_t(item.first & 0xFF);
        color.g = uint8_t((item.first >> 8) & 0xFF);
        color.b = uint8_t((item.first >> 16) & 0xFF);
        color.a = uint8_t((item.first >> 24) & 0xFF);
        color.count = item.second;
        colors.push_back(color);
    }

    std::vector<PaletteSourceColor> paletteColors;

    if (colors.size() <= size_t(paletteEntries)) {
        paletteColors = colors;
        std::sort(paletteColors.begin(), paletteColors.end(), [](const PaletteSourceColor& lhs, const PaletteSourceColor& rhs) {
            if ((lhs.a < 8) != (rhs.a < 8)) return lhs.a < 8;
            if (lhs.count != rhs.count) return lhs.count > rhs.count;
            if (lhs.r != rhs.r) return lhs.r < rhs.r;
            if (lhs.g != rhs.g) return lhs.g < rhs.g;
            return lhs.b < rhs.b;
        });
    } else {
        std::vector<PaletteBucket> buckets;
        PaletteBucket initial;
        initial.indices.reserve(colors.size());
        for (size_t i = 0; i < colors.size(); ++i) initial.indices.push_back(i);
        buckets.push_back(std::move(initial));

        while (buckets.size() < size_t(paletteEntries)) {
            size_t bestBucket = SIZE_MAX;
            int bestChannel = 0;
            uint64_t bestScore = 0;

            for (size_t i = 0; i < buckets.size(); ++i) {
                if (buckets[i].indices.size() <= 1) continue;

                int ranges[4] = {
                    paletteBucketRange(colors, buckets[i], 0),
                    paletteBucketRange(colors, buckets[i], 1),
                    paletteBucketRange(colors, buckets[i], 2),
                    paletteBucketRange(colors, buckets[i], 3)
                };

                int channel = 0;
                if (ranges[1] > ranges[channel]) channel = 1;
                if (ranges[2] > ranges[channel]) channel = 2;
                if (ranges[3] > ranges[channel]) channel = 3;

                uint64_t score = uint64_t(ranges[channel]) * std::max<uint64_t>(1, paletteBucketWeight(colors, buckets[i]));
                if (score > bestScore) {
                    bestScore = score;
                    bestBucket = i;
                    bestChannel = channel;
                }
            }

            if (bestBucket == SIZE_MAX) break;

            PaletteBucket bucket = std::move(buckets[bestBucket]);
            std::sort(bucket.indices.begin(), bucket.indices.end(), [&](size_t lhs, size_t rhs) {
                return paletteSourceChannel(colors[lhs], bestChannel) < paletteSourceChannel(colors[rhs], bestChannel);
            });

            uint64_t totalWeight = paletteBucketWeight(colors, bucket);
            uint64_t halfWeight = totalWeight / 2;
            uint64_t running = 0;
            size_t split = 1;

            for (size_t i = 0; i < bucket.indices.size(); ++i) {
                running += colors[bucket.indices[i]].count;
                if (running >= halfWeight) {
                    split = std::max<size_t>(1, i + 1);
                    break;
                }
            }

            if (split >= bucket.indices.size()) split = bucket.indices.size() / 2;
            if (split == 0 || split >= bucket.indices.size()) break;

            PaletteBucket left;
            PaletteBucket right;
            left.indices.assign(bucket.indices.begin(), bucket.indices.begin() + split);
            right.indices.assign(bucket.indices.begin() + split, bucket.indices.end());
            buckets[bestBucket] = std::move(left);
            buckets.push_back(std::move(right));
        }

        for (const PaletteBucket& bucket : buckets) {
            if (bucket.indices.empty()) continue;

            uint64_t total = 0;
            uint64_t r = 0;
            uint64_t g = 0;
            uint64_t b = 0;
            uint64_t a = 0;

            for (size_t index : bucket.indices) {
                const PaletteSourceColor& color = colors[index];
                total += color.count;
                r += uint64_t(color.r) * color.count;
                g += uint64_t(color.g) * color.count;
                b += uint64_t(color.b) * color.count;
                a += uint64_t(color.a) * color.count;
            }

            if (total == 0) total = 1;

            PaletteSourceColor averaged;
            averaged.r = uint8_t(r / total);
            averaged.g = uint8_t(g / total);
            averaged.b = uint8_t(b / total);
            averaged.a = uint8_t(a / total);
            averaged.count = uint32_t(std::min<uint64_t>(total, 0xFFFFFFFFu));
            paletteColors.push_back(averaged);
        }

        std::sort(paletteColors.begin(), paletteColors.end(), [](const PaletteSourceColor& lhs, const PaletteSourceColor& rhs) {
            if ((lhs.a < 8) != (rhs.a < 8)) return lhs.a < 8;
            return lhs.count > rhs.count;
        });
    }

    std::vector<uint8_t> palette(size_t(paletteEntries) * 4, 0);
    for (int i = 0; i < paletteEntries && i < int(paletteColors.size()); ++i) {
        const PaletteSourceColor& color = paletteColors[size_t(i)];
        palette[size_t(i) * 4 + 0] = color.r;
        palette[size_t(i) * 4 + 1] = color.g;
        palette[size_t(i) * 4 + 2] = color.b;
        palette[size_t(i) * 4 + 3] = color.a;
    }

    if (ps2Alpha) {
        for (int i = 0; i < paletteEntries; ++i) {
            palette[size_t(i) * 4 + 3] = uint8_t(std::min(128, int(palette[size_t(i) * 4 + 3]) * 128 / 255));
        }
    }

    return palette;
}

static std::vector<uint8_t> paletteForReplacementMatching(const std::vector<uint8_t>& rawPalette, bool ps2Alpha) {
    std::vector<uint8_t> palette = rawPalette;
    if (ps2Alpha) {
        for (size_t i = 0; i + 3 < palette.size(); i += 4) {
            palette[i + 3] = uint8_t(std::min(255, int(palette[i + 3]) * 255 / 128));
        }
    }
    return palette;
}

const std::vector<LeedsTextureEntry>& LeedsTextureArchive::textures() const {
    return entries;
}

const std::wstring& LeedsTextureArchive::sourcePath() const {
    return path;
}

bool LeedsTextureArchive::loadFromFile(const std::wstring& filePath, LeedsPlatform platform, std::string& errorMessage) {
    dataBytes.clear();
    entries.clear();
    path.clear();

#ifdef _WIN32
    FILE* file = nullptr;
    if (_wfopen_s(&file, filePath.c_str(), L"rb") != 0 || file == nullptr) {
        errorMessage = "Could not open input file.";
        return false;
    }

    if (_fseeki64(file, 0, SEEK_END) != 0) {
        fclose(file);
        errorMessage = "Could not seek input file.";
        return false;
    }

    __int64 fileSize = _ftelli64(file);
    if (fileSize < 0) {
        fclose(file);
        errorMessage = "Could not determine input file size.";
        return false;
    }

    if (_fseeki64(file, 0, SEEK_SET) != 0) {
        fclose(file);
        errorMessage = "Could not rewind input file.";
        return false;
    }

    dataBytes.resize(size_t(fileSize));
    if (!dataBytes.empty()) {
        size_t readCount = fread(dataBytes.data(), 1, dataBytes.size(), file);
        if (readCount != dataBytes.size()) {
            fclose(file);
            errorMessage = "Could not read complete input file.";
            dataBytes.clear();
            return false;
        }
    }
    fclose(file);
#else
    std::ifstream file(std::string(filePath.begin(), filePath.end()), std::ios::binary);
    if (!file) {
        errorMessage = "Could not open input file.";
        return false;
    }
    dataBytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
#endif

    path = filePath;
    loadedPlatform = platform;
    return parse(platform, errorMessage);
}

bool LeedsTextureArchive::saveToFile(const std::wstring& filePath, std::string& errorMessage) const {
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        errorMessage = "Could not open output file.";
        return false;
    }
    file.write(reinterpret_cast<const char*>(dataBytes.data()), std::streamsize(dataBytes.size()));
    if (!file) {
        errorMessage = "Could not write output file.";
        return false;
    }
    return true;
}

bool LeedsTextureArchive::parse(LeedsPlatform platform, std::string& errorMessage) {
    entries.clear();

    LeedsTextureEntry ctwTex;
    if (parseCtwStandaloneTex(dataBytes, ctwTex)) {
        entries.push_back(ctwTex);
        return true;
    }

    if (dataBytes.size() < 0x30) {
        errorMessage = "File is too small for a CHK/XTX/TEX texture file.";
        return false;
    }
    std::string sig(reinterpret_cast<const char*>(dataBytes.data()), reinterpret_cast<const char*>(dataBytes.data() + 4));
    if (sig.rfind("xet", 0) != 0 && sig != "TCDT") {
        errorMessage = "Not a recognized Leeds/CTW texture file. Expected xet/TCDT collection or standalone CTW TEX.";
        return false;
    }
    if (sig == "TCDT") {
        errorMessage = "This build focuses on LCS/VCS xet CHK/XTX. Manhunt 2 TCDT listing is not enabled yet.";
        return false;
    }

    uint32_t collectionSize = readU32(dataBytes, 0x08);
    uint32_t global1 = readU32(dataBytes, 0x0C);
    uint32_t global2 = readU32(dataBytes, 0x10);
    uint32_t firstSlot = readU32(dataBytes, 0x28);
    uint32_t lastSlot = readU32(dataBytes, 0x2C);
    uint32_t base = slotBaseFromSlotPtr(firstSlot);
    std::set<uint32_t> visited;
    uint32_t lastBase = slotBaseFromSlotPtr(lastSlot);

    for (int guard = 0; guard < 10000; ++guard) {
        if (base < 0x30 || size_t(base) + 0x50 > dataBytes.size()) break;
        if (visited.count(base)) break;
        visited.insert(base);

        uint32_t texOff = readU32(dataBytes, base + 0x00);
        uint32_t nextSlot = readU32(dataBytes, base + 0x08);
        std::string name = readCString(dataBytes, base + 0x10, 64);
        if (texOff >= 0x30 && texOff + 16 <= dataBytes.size() && isPrintableName(name)) {
            LeedsTextureEntry psp;
            LeedsTextureEntry ps2;
            bool havePsp = platform != LeedsPlatform::Ps2 && parsePspHeader(dataBytes, texOff, psp);
            bool havePs2 = platform != LeedsPlatform::Psp && parsePs2Header(dataBytes, texOff, ps2);
            LeedsTextureEntry chosen;
            bool haveChosen = false;
            if (platform == LeedsPlatform::Psp && havePsp) { chosen = psp; haveChosen = true; }
            else if (platform == LeedsPlatform::Ps2 && havePs2) { chosen = ps2; haveChosen = true; }
            else if (havePsp && !havePs2) { chosen = psp; haveChosen = true; }
            else if (havePs2 && !havePsp) { chosen = ps2; haveChosen = true; }
            else if (havePsp && havePs2) {
                int areaPsp = psp.width * psp.height;
                int areaPs2 = ps2.width * ps2.height;
                chosen = areaPs2 > areaPsp ? ps2 : psp;
                haveChosen = true;
            }
            if (haveChosen) {
                chosen.name = name;
                chosen.containerBase = base;
                entries.push_back(chosen);
            }
        }

        if (nextSlot == 0 || nextSlot < 0x30) break;
        uint32_t nextBase = slotBaseFromSlotPtr(nextSlot);
        if (nextBase < 0x30 || nextBase == base) break;
        base = nextBase;
        (void)lastBase;
    }

    if (entries.empty()) {
        errorMessage = "No decodable texture containers were found.";
        return false;
    }

    std::vector<uint32_t> rasterBoundaries;
    rasterBoundaries.reserve(entries.size() * 3 + 8);
    auto addBoundary = [&](uint32_t value) {
        if (value > 0 && value <= dataBytes.size()) rasterBoundaries.push_back(value);
    };

    addBoundary(collectionSize);
    addBoundary(global1);
    addBoundary(global2);
    addBoundary(firstSlot);
    addBoundary(lastSlot);
    addBoundary(uint32_t(dataBytes.size()));

    for (const LeedsTextureEntry& entry : entries) {
        addBoundary(entry.rasterOffset);
        addBoundary(entry.textureHeaderOffset);
        addBoundary(entry.containerBase);
    }

    std::sort(rasterBoundaries.begin(), rasterBoundaries.end());
    rasterBoundaries.erase(std::unique(rasterBoundaries.begin(), rasterBoundaries.end()), rasterBoundaries.end());

    for (LeedsTextureEntry& entry : entries) {
        uint32_t start = entry.rasterOffset;
        auto it = std::upper_bound(rasterBoundaries.begin(), rasterBoundaries.end(), start);
        uint32_t end = it == rasterBoundaries.end() ? uint32_t(dataBytes.size()) : *it;
        entry.blockSize = end > start ? end - start : 0;
    }

    return true;
}

bool LeedsTextureArchive::decodeTexture(size_t textureIndex, RgbaImage& image, std::string& errorMessage) const {
    if (textureIndex >= entries.size()) {
        errorMessage = "Texture index is out of range.";
        return false;
    }
    const LeedsTextureEntry& entry = entries[textureIndex];
    size_t pixelCount = size_t(entry.width) * size_t(entry.height);

    if (entry.kind == TextureKind::CtwTex) {
        uint16_t format = uint16_t(entry.flags & 0xFFFFu);
        if (format == 0x0004u || format == 0x0104u || format == 0x0002u || format == 0x0102u) {
            return decodeCtwPvrtcTexture(dataBytes, entry, image, errorMessage);
        }
        return decodeCtwUnknownTexture(dataBytes, entry, image, errorMessage);
    }

    size_t baseBytes = 0;
    if (entry.bpp == 4) baseBytes = (pixelCount + 1) / 2;
    else if (entry.bpp == 8) baseBytes = pixelCount;
    else if (entry.bpp == 16) baseBytes = pixelCount * 2;
    else if (entry.bpp == 32) baseBytes = pixelCount * 4;
    else {
        errorMessage = "Unsupported BPP.";
        return false;
    }
    if (size_t(entry.rasterOffset) + baseBytes > dataBytes.size()) {
        errorMessage = "Raster data points outside the file.";
        return false;
    }

    image.width = entry.width;
    image.height = entry.height;
    image.rgba.assign(pixelCount * 4, 255);
    const uint8_t* raw = dataBytes.data() + entry.rasterOffset;

    if (entry.kind == TextureKind::Psp) {
        if (entry.bpp == 32) {
            std::vector<uint8_t> rgba = entry.swizzleWidth ? unswizzlePspBytes(raw, baseBytes, entry.width, entry.height, 4, 1) : std::vector<uint8_t>(raw, raw + baseBytes);
            image.rgba = std::move(rgba);
            return finishDecodedTexture(image);
        }
        std::vector<uint8_t> packed = entry.swizzleWidth ? unswizzlePspBytes(raw, baseBytes, entry.width, entry.height, entry.bpp == 4 ? 1 : 1, entry.bpp == 4 ? 2 : 1) : std::vector<uint8_t>(raw, raw + baseBytes);
        std::vector<uint8_t> indices = entry.bpp == 4 ? expandNibblesLoFirst(packed.data(), packed.size(), pixelCount) : packed;
        int paletteEntries = entry.bpp == 4 ? 16 : 256;
        std::vector<uint8_t> palette = readPalette(dataBytes, entry, paletteEntries, false);
        for (size_t i = 0; i < pixelCount; ++i) {
            int p = indices[i] % paletteEntries;
            image.rgba[i * 4 + 0] = palette[size_t(p) * 4 + 0];
            image.rgba[i * 4 + 1] = palette[size_t(p) * 4 + 1];
            image.rgba[i * 4 + 2] = palette[size_t(p) * 4 + 2];
            image.rgba[i * 4 + 3] = palette[size_t(p) * 4 + 3];
        }
        return finishDecodedTexture(image);
    }

    if (entry.kind == TextureKind::Ps2) {
        if (entry.bpp == 32) {
            std::vector<uint8_t> rgba = (entry.swizzleMask & 1) ? unswizzlePspBytes(raw, baseBytes, entry.width, entry.height, 4, 1) : std::vector<uint8_t>(raw, raw + baseBytes);
            image.rgba = std::move(rgba);
            for (size_t i = 0; i < pixelCount; ++i) image.rgba[i * 4 + 3] = uint8_t(std::min(255, int(image.rgba[i * 4 + 3]) * 255 / 128));
            return finishDecodedTexture(image);
        }
        if (entry.bpp == 16) {
            for (size_t i = 0; i < pixelCount; ++i) {
                uint16_t v = uint16_t(raw[i * 2]) | (uint16_t(raw[i * 2 + 1]) << 8);
                image.rgba[i * 4 + 0] = uint8_t(((v >> 0) & 0x1F) * 255 / 31);
                image.rgba[i * 4 + 1] = uint8_t(((v >> 5) & 0x1F) * 255 / 31);
                image.rgba[i * 4 + 2] = uint8_t(((v >> 10) & 0x1F) * 255 / 31);
                image.rgba[i * 4 + 3] = (v & 0x8000) ? 255 : 0;
            }
            return finishDecodedTexture(image);
        }
        std::vector<uint8_t> indices;
        if (entry.bpp == 4) {
            std::vector<uint8_t> packed(raw, raw + baseBytes);
            indices = expandNibblesLoFirst(packed.data(), packed.size(), pixelCount);
            if (entry.swizzleMask & 1) indices = unswizzlePs2Indices(indices, entry.width, entry.height);
        } else {
            indices = std::vector<uint8_t>(raw, raw + baseBytes);
            if (entry.swizzleMask & 1) indices = unswizzlePs2Indices(indices, entry.width, entry.height);
            convertClutPs2(indices);
        }
        int paletteEntries = entry.bpp == 4 ? 16 : 256;
        std::vector<uint8_t> palette = readPalette(dataBytes, entry, paletteEntries, true);
        for (size_t i = 0; i < pixelCount; ++i) {
            int p = indices[i] % paletteEntries;
            image.rgba[i * 4 + 0] = palette[size_t(p) * 4 + 0];
            image.rgba[i * 4 + 1] = palette[size_t(p) * 4 + 1];
            image.rgba[i * 4 + 2] = palette[size_t(p) * 4 + 2];
            image.rgba[i * 4 + 3] = palette[size_t(p) * 4 + 3];
        }
        return finishDecodedTexture(image);
    }

    errorMessage = "Unknown texture kind.";
    return false;
}


bool LeedsTextureArchive::replaceTexture(size_t textureIndex, const RgbaImage& image, std::string& errorMessage) {
    return replaceTextureAsBpp(textureIndex, image, 0, errorMessage);
}

bool LeedsTextureArchive::replaceTextureAsBpp(size_t textureIndex, const RgbaImage& image, uint8_t targetBpp, std::string& errorMessage) {
    if (textureIndex >= entries.size()) {
        errorMessage = "Texture index is out of range.";
        return false;
    }
    if (!isPowerOfTwoInt(image.width) || !isPowerOfTwoInt(image.height)) {
        errorMessage = "Replacement image width and height must both be powers of two between 4 and 4096.";
        return false;
    }
    if (image.rgba.size() != size_t(image.width) * size_t(image.height) * 4u) {
        errorMessage = "Replacement image RGBA buffer is invalid.";
        return false;
    }

    LeedsTextureEntry entry = entries[textureIndex];
    LeedsTextureEntry oldEntry = entry;

    if (targetBpp == 0) targetBpp = entry.bpp;
    if (targetBpp != 4 && targetBpp != 8 && targetBpp != 16 && targetBpp != 32) {
        errorMessage = "Target BPP must be 4, 8, 16, or 32. Use 0 to keep the original BPP.";
        return false;
    }
    if (entry.kind == TextureKind::CtwTex) {
        errorMessage = "Standalone CTW .tex replacement is not enabled yet.";
        return false;
    }
    if (entry.kind == TextureKind::Psp && targetBpp == 16) {
        errorMessage = "PSP texture replacement does not support 16bpp in this archive path. Use 4, 8, or 32.";
        return false;
    }

    uint32_t oldRasterBytes = rasterStoredBytesForEntry(oldEntry);
    uint32_t oldPaletteBytes = paletteBytesForEntry(oldEntry);
    uint32_t oldPaletteStart = 0;
    if (oldPaletteBytes != 0) {
        int oldPaletteEntries = oldEntry.bpp == 4 ? 16 : 256;
        if (!findPaletteStartForEntry(dataBytes, oldEntry, oldPaletteEntries, oldPaletteStart)) {
            errorMessage = "Could not locate the original indexed texture palette.";
            return false;
        }
        if (size_t(oldPaletteStart) + oldPaletteBytes > dataBytes.size()) {
            errorMessage = "Original indexed texture palette points outside the file.";
            return false;
        }
        uint32_t immediatePaletteStart = oldEntry.rasterOffset + oldRasterBytes;
        if (oldPaletteStart > immediatePaletteStart) {
            if (oldEntry.kind == TextureKind::Psp) {
                // PSP indexed textures keep mip/padding bytes between the base
                // raster payload and the block-end palette.  When replacing, the
                // whole old raster+mip/pad area must be removed, not just the
                // base level, or the archive keeps stale mip bytes in front of
                // the new palette.
                oldRasterBytes = oldPaletteStart - oldEntry.rasterOffset;
            } else if (oldPaletteStart - immediatePaletteStart <= 0x40) {
                oldRasterBytes = oldPaletteStart - oldEntry.rasterOffset;
            }
        }
    }

    entry.bpp = targetBpp;
    uint32_t paletteBytes = paletteBytesForEntry(entry);
    std::vector<uint8_t> rawPalette;
    if (paletteBytes != 0) {
        rawPalette = buildPaletteFromReplacementImage(image, entry.bpp == 4 ? 16 : 256, entry.kind == TextureKind::Ps2);
    }

    uint32_t oldSegmentBytes = oldRasterBytes + oldPaletteBytes;
    uint32_t oldEnd = entry.rasterOffset + oldSegmentBytes;
    if (oldRasterBytes == 0 || oldEnd > dataBytes.size() || oldEnd < entry.rasterOffset) {
        errorMessage = "Replacement target raster block is invalid.";
        return false;
    }

    uint32_t newBaseBytes = rasterBaseBytesForDimensions(image.width, image.height, entry.bpp);
    if (newBaseBytes == 0) {
        errorMessage = "Unsupported BPP for replacement.";
        return false;
    }

    std::vector<uint8_t> encodedRaster;

    if (entry.bpp == 32) {
        encodedRaster = image.rgba;
        size_t pixelCount = size_t(image.width) * size_t(image.height);
        if (entry.kind == TextureKind::Ps2) {
            for (size_t i = 0; i < pixelCount; ++i) {
                encodedRaster[i * 4 + 3] = uint8_t(std::min(128, int(encodedRaster[i * 4 + 3]) * 128 / 255));
            }
        }
        bool doSwizzle = (entry.kind == TextureKind::Psp && entry.swizzleWidth) || (entry.kind == TextureKind::Ps2 && (entry.swizzleMask & 1));
        if (doSwizzle) {
            encodedRaster = swizzlePspBytes(encodedRaster.data(), encodedRaster.size(), image.width, image.height, 4, 1);
        }
    } else if (entry.bpp == 16) {
        encodedRaster.assign(size_t(newBaseBytes), 0);
        size_t pixelCount = size_t(image.width) * size_t(image.height);
        for (size_t i = 0; i < pixelCount; ++i) {
            uint8_t r = image.rgba[i * 4 + 0];
            uint8_t g = image.rgba[i * 4 + 1];
            uint8_t b = image.rgba[i * 4 + 2];
            uint8_t a = image.rgba[i * 4 + 3];
            uint16_t v = uint16_t((r * 31 / 255) | ((g * 31 / 255) << 5) | ((b * 31 / 255) << 10) | (a >= 128 ? 0x8000 : 0));
            encodedRaster[i * 2 + 0] = uint8_t(v & 0xFF);
            encodedRaster[i * 2 + 1] = uint8_t((v >> 8) & 0xFF);
        }
    } else {
        int paletteEntries = entry.bpp == 4 ? 16 : 256;
        std::vector<uint8_t> palette = paletteForReplacementMatching(rawPalette, entry.kind == TextureKind::Ps2);
        size_t pixelCount = size_t(image.width) * size_t(image.height);
        std::vector<uint8_t> indices(pixelCount);
        for (size_t i = 0; i < pixelCount; ++i) {
            indices[i] = uint8_t(nearestPaletteIndex(
                palette,
                paletteEntries,
                image.rgba[i * 4 + 0],
                image.rgba[i * 4 + 1],
                image.rgba[i * 4 + 2],
                image.rgba[i * 4 + 3]
            ));
        }

        if (entry.kind == TextureKind::Ps2 && entry.bpp == 8) {
            indices = inverseConvertClutPs2(indices);
            if (entry.swizzleMask & 1) indices = swizzlePs2Indices(indices, image.width, image.height);
            encodedRaster = indices;
        } else if (entry.kind == TextureKind::Ps2 && entry.bpp == 4) {
            if (entry.swizzleMask & 1) indices = swizzlePs2Indices(indices, image.width, image.height);
            encodedRaster = packNibblesLoFirst(indices);
        } else {
            encodedRaster = entry.bpp == 4 ? packNibblesLoFirst(indices) : indices;
            if (entry.kind == TextureKind::Psp && entry.swizzleWidth) {
                encodedRaster = swizzlePspBytes(
                    encodedRaster.data(),
                    encodedRaster.size(),
                    image.width,
                    image.height,
                    1,
                    entry.bpp == 4 ? 2 : 1
                );
            }
        }
    }

    if (encodedRaster.size() != newBaseBytes) {
        errorMessage = "Replacement encoder produced an unexpected raster byte count.";
        return false;
    }

    std::vector<uint8_t> newSegment;
    newSegment.reserve(encodedRaster.size() + rawPalette.size());
    newSegment.insert(newSegment.end(), encodedRaster.begin(), encodedRaster.end());
    newSegment.insert(newSegment.end(), rawPalette.begin(), rawPalette.end());

    uint32_t oldFileSize = uint32_t(dataBytes.size());
    int64_t delta = int64_t(newSegment.size()) - int64_t(oldSegmentBytes);
    std::vector<uint32_t> oldOffsetFields = collectKnownOffsetFields(dataBytes, entries);

    dataBytes.erase(dataBytes.begin() + entry.rasterOffset, dataBytes.begin() + oldEnd);
    dataBytes.insert(dataBytes.begin() + entry.rasterOffset, newSegment.begin(), newSegment.end());

    if (dataBytes.size() > 0xFFFFFFFFu) {
        errorMessage = "Replacement made the archive too large.";
        return false;
    }

    applyKnownOffsetShift(dataBytes, oldOffsetFields, oldEnd, delta, oldFileSize);

    uint32_t shiftedHeaderOffset = shiftedFieldOffset(entry.textureHeaderOffset, oldEnd, delta);
    entry.textureHeaderOffset = shiftedHeaderOffset;
    if (!writeTextureHeaderDimensions(dataBytes, entry, image.width, image.height, entry.rasterOffset, errorMessage)) {
        return false;
    }

    if (!parse(loadedPlatform, errorMessage)) {
        return false;
    }

    return true;
}

bool LeedsTextureArchive::renameTexture(size_t textureIndex, const std::string& newName, std::string& errorMessage) {
    if (textureIndex >= entries.size()) {
        errorMessage = "Texture index is out of range.";
        return false;
    }
    if (newName.empty()) {
        errorMessage = "Texture name cannot be empty.";
        return false;
    }
    if (newName.size() >= 64) {
        errorMessage = "Texture name must be 63 characters or less.";
        return false;
    }
    for (unsigned char c : newName) {
        if (c < 32 || c >= 127) {
            errorMessage = "Texture name must use printable ASCII characters.";
            return false;
        }
    }

    LeedsTextureEntry entry = entries[textureIndex];
    if (entry.containerBase + 0x50 > dataBytes.size()) {
        errorMessage = "Texture container name field points outside the file.";
        return false;
    }

    std::fill(dataBytes.begin() + entry.containerBase + 0x10, dataBytes.begin() + entry.containerBase + 0x50, 0);
    std::copy(newName.begin(), newName.end(), dataBytes.begin() + entry.containerBase + 0x10);

    if (!parse(loadedPlatform, errorMessage)) {
        return false;
    }

    return true;
}

