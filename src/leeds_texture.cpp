#include "leeds_texture.h"
#include "storyland_atomic_io.h"

#include <cctype>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <limits>
#include <map>
#include <set>
#include <sstream>

static uint32_t readU32(const std::vector<uint8_t>& data, size_t offset) {
    if (offset > data.size() || data.size() - offset < 4u) return 0;
    return uint32_t(data[offset]) | (uint32_t(data[offset + 1]) << 8) | (uint32_t(data[offset + 2]) << 16) | (uint32_t(data[offset + 3]) << 24);
}

static uint16_t readU16(const std::vector<uint8_t>& data, size_t offset) {
    if (offset > data.size() || data.size() - offset < 2u) return 0;
    return uint16_t(data[offset]) | (uint16_t(data[offset + 1]) << 8);
}

static uint32_t packPs2RuntimeFlags(
    uint8_t widthPow2,
    uint8_t heightPow2,
    uint8_t bpp,
    uint8_t mipCount,
    uint8_t swizzleMask,
    uint8_t unknownBits);

static uint32_t defaultPs2Reserved1(int width, uint8_t bpp);

static bool isVcsPlayerTexturePath(const std::wstring& sourcePath) {
    if (sourcePath.empty()) return false;
    std::wstring name = std::filesystem::path(sourcePath).filename().wstring();
    std::transform(name.begin(), name.end(), name.begin(), [](wchar_t ch) {
        return wchar_t(std::towlower(ch));
    });
    return name == L"plr.xtx" || name == L"plr.chk" || name == L"plr.tex";
}

static std::string lowerAsciiTextureName(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
        return char(std::tolower(ch));
    });
    return name;
}

static bool looksLikeVcsPlayerTextureEntries(const std::vector<LeedsTextureEntry>& textureEntries) {
    if (textureEntries.empty()) return false;
    std::set<std::string> names;
    for (const LeedsTextureEntry& entry : textureEntries) names.insert(lowerAsciiTextureName(entry.name));
    const bool hasHead = names.count("head") != 0u;
    const bool hasShoes = names.count("shoes") != 0u || names.count("shoe") != 0u || names.count("ig_vance_boots") != 0u;
    const bool hasUpper = names.count("torso") != 0u || names.count("upper body") != 0u || names.count("ig_vance_t_shirt") != 0u;
    const bool hasLegs = names.count("jeans") != 0u || names.count("legs") != 0u || names.count("ig_vance_trousers_camo") != 0u;
    return hasHead && hasShoes && hasUpper && hasLegs;
}

static void writeU32(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
    if (offset > data.size() || data.size() - offset < 4u) return;
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

static int nearestSupportedPowerOfTwo(int value) {
    if (value <= 4) return 4;
    if (value >= 4096) return 4096;
    int lo = 4;
    while ((lo << 1) <= value && lo < 4096) lo <<= 1;
    int hi = std::min(4096, lo << 1);
    return (value - lo <= hi - value) ? lo : hi;
}

static bool normalizePs2TextureImage(const RgbaImage& source, RgbaImage& output, std::string& noteOrError) {
    noteOrError.clear();
    if (source.width <= 0 || source.height <= 0) {
        noteOrError = "Image buffer/dimensions are invalid.";
        return false;
    }
    const uint64_t sourcePixels = uint64_t(source.width) * uint64_t(source.height);
    if (sourcePixels > uint64_t((std::numeric_limits<size_t>::max)()) / 4ull ||
        source.rgba.size() != size_t(sourcePixels * 4ull)) {
        noteOrError = "Image buffer size does not match its dimensions safely.";
        return false;
    }

    if (source.width > 4096 || source.height > 4096) {
        noteOrError = "Texture is larger than the Leeds PS2 4096x4096 limit.";
        return false;
    }

    if (isPowerOfTwoInt(source.width) && isPowerOfTwoInt(source.height)) {
        output = source;
        return true;
    }

    const int targetWidth = nearestSupportedPowerOfTwo(source.width);
    const int targetHeight = nearestSupportedPowerOfTwo(source.height);
    output.width = targetWidth;
    output.height = targetHeight;
    output.rgba.assign(size_t(targetWidth) * size_t(targetHeight) * 4u, 0u);

    // Bilinear resize keeps arbitrary source PNGs usable while the archive still
    // receives the power-of-two dimensions required by the Leeds PS2 header.
    for (int y = 0; y < targetHeight; ++y) {
        const float sy = ((float(y) + 0.5f) * float(source.height) / float(targetHeight)) - 0.5f;
        const int y0 = std::clamp(int(std::floor(sy)), 0, source.height - 1);
        const int y1 = std::min(y0 + 1, source.height - 1);
        const float fy = std::clamp(sy - float(y0), 0.0f, 1.0f);
        for (int x = 0; x < targetWidth; ++x) {
            const float sx = ((float(x) + 0.5f) * float(source.width) / float(targetWidth)) - 0.5f;
            const int x0 = std::clamp(int(std::floor(sx)), 0, source.width - 1);
            const int x1 = std::min(x0 + 1, source.width - 1);
            const float fx = std::clamp(sx - float(x0), 0.0f, 1.0f);
            for (int c = 0; c < 4; ++c) {
                const float p00 = source.rgba[(size_t(y0) * size_t(source.width) + size_t(x0)) * 4u + size_t(c)];
                const float p10 = source.rgba[(size_t(y0) * size_t(source.width) + size_t(x1)) * 4u + size_t(c)];
                const float p01 = source.rgba[(size_t(y1) * size_t(source.width) + size_t(x0)) * 4u + size_t(c)];
                const float p11 = source.rgba[(size_t(y1) * size_t(source.width) + size_t(x1)) * 4u + size_t(c)];
                const float top = p00 + (p10 - p00) * fx;
                const float bottom = p01 + (p11 - p01) * fx;
                const float v = top + (bottom - top) * fy;
                output.rgba[(size_t(y) * size_t(targetWidth) + size_t(x)) * 4u + size_t(c)] =
                    uint8_t(std::clamp(int(std::lround(v)), 0, 255));
            }
        }
    }

    noteOrError = "Resized " + std::to_string(source.width) + "x" + std::to_string(source.height) +
                  " to " + std::to_string(targetWidth) + "x" + std::to_string(targetHeight) +
                  " for the Leeds PS2 texture header.";
    return true;
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

static bool isPspTextureKind(TextureKind kind) {
    return kind == TextureKind::Psp || kind == TextureKind::RwPsp;
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
    // PS2 Stories raster flags follow the Leeds/Rsl ordering used by BLeeds:
    // width exponent at bits 26..31, height exponent at bits 20..25.
    uint32_t widthPow2 = (flags >> 26) & 0x3F;
    uint32_t heightPow2 = (flags >> 20) & 0x3F;
    int width = safePow2(widthPow2);
    int height = safePow2(heightPow2);

    // Older/alternate serialized Leeds header packs width first, then height.
    uint32_t widthPow2Alt = flags & 0x3F;
    uint32_t heightPow2Alt = (flags >> 6) & 0x3F;
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
        flags = (swizzle & 0xFF) | ((mipCount & 0xF) << 8) | ((unknownAlt & 0x3) << 12) | ((bpp & 0x3F) << 14) | ((heightPow2 & 0x3F) << 20) | ((widthPow2 & 0x3F) << 26);
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

static constexpr uint32_t LeedsDdsMagic = 0x20534444u;
static constexpr uint32_t LeedsDxt1 = 0x31545844u;
static constexpr uint32_t LeedsDxt3 = 0x33545844u;
static constexpr uint32_t LeedsDxt5 = 0x35545844u;
static constexpr uint64_t LeedsMaxTextureFileBytes = 1024ull * 1024ull * 1024ull;
static constexpr uint64_t LeedsMaxDecodedTextureBytes = 512ull * 1024ull * 1024ull;

static bool checkedTextureRange(size_t offset, size_t size, size_t total) {
    return offset <= total && size <= total - offset;
}

static bool parseDdsHeader(const std::vector<uint8_t>& data, uint32_t offset, LeedsTextureEntry& entry) {
    if (!checkedTextureRange(size_t(offset), 128u, data.size())) return false;
    if (readU32(data, offset + 0u) != LeedsDdsMagic || readU32(data, offset + 4u) != 124u) return false;

    uint32_t height = readU32(data, offset + 12u);
    uint32_t width = readU32(data, offset + 16u);
    uint32_t mipCount = readU32(data, offset + 28u);
    uint32_t pixelFormatSize = readU32(data, offset + 76u);
    uint32_t pixelFormatFlags = readU32(data, offset + 80u);
    uint32_t fourCc = readU32(data, offset + 84u);
    uint32_t rgbBits = readU32(data, offset + 88u);

    if (pixelFormatSize != 32u || width == 0u || height == 0u || width > 8192u || height > 8192u) return false;
    if (uint64_t(width) > UINT64_MAX / uint64_t(height) ||
        uint64_t(width) * uint64_t(height) > LeedsMaxDecodedTextureBytes / 4ull) return false;

    uint64_t payloadBytes = 0u;
    uint8_t displayBpp = 0u;
    if ((pixelFormatFlags & 0x4u) != 0u) {
        uint64_t blockBytes = fourCc == LeedsDxt1 ? 8u : (fourCc == LeedsDxt3 || fourCc == LeedsDxt5 ? 16u : 0u);
        if (blockBytes == 0u) return false;
        uint64_t blocksWide = (uint64_t(width) + 3u) / 4u;
        uint64_t blocksHigh = (uint64_t(height) + 3u) / 4u;
        if (blocksWide > UINT64_MAX / blocksHigh || blocksWide * blocksHigh > UINT64_MAX / blockBytes) return false;
        payloadBytes = blocksWide * blocksHigh * blockBytes;
        displayBpp = fourCc == LeedsDxt1 ? 4u : 8u;
    } else if ((pixelFormatFlags & 0x40u) != 0u && (rgbBits == 24u || rgbBits == 32u)) {
        uint64_t bytesPerPixel = rgbBits / 8u;
        if (uint64_t(width) > UINT64_MAX / uint64_t(height) || uint64_t(width) * uint64_t(height) > UINT64_MAX / bytesPerPixel) return false;
        payloadBytes = uint64_t(width) * uint64_t(height) * bytesPerPixel;
        displayBpp = uint8_t(rgbBits);
    } else {
        return false;
    }

    uint64_t rasterOffset = uint64_t(offset) + 128u;
    if (payloadBytes > UINT32_MAX || rasterOffset > UINT32_MAX || rasterOffset > data.size() || payloadBytes > data.size() - size_t(rasterOffset)) return false;

    entry.kind = TextureKind::Dds;
    entry.textureHeaderOffset = offset;
    entry.rasterOffset = uint32_t(rasterOffset);
    entry.blockSize = uint32_t(payloadBytes);
    entry.flags = fourCc;
    entry.reserved0 = pixelFormatFlags;
    entry.reserved1 = rgbBits;
    entry.redMask = readU32(data, offset + 92u);
    entry.greenMask = readU32(data, offset + 96u);
    entry.blueMask = readU32(data, offset + 100u);
    entry.alphaMask = readU32(data, offset + 104u);
    entry.width = int(width);
    entry.height = int(height);
    entry.widthPow2 = isPowerOfTwoInt(int(width)) ? log2ExactInt(int(width)) : 0u;
    entry.heightPow2 = isPowerOfTwoInt(int(height)) ? log2ExactInt(int(height)) : 0u;
    entry.bpp = displayBpp;
    entry.mipCount = uint8_t(std::min<uint32_t>(mipCount == 0u ? 1u : mipCount, 255u));
    return true;
}

static bool parseTcdtContainerAt(const std::vector<uint8_t>& data, uint32_t base, LeedsTextureEntry& entry) {
    if (!checkedTextureRange(size_t(base), 0x68u, data.size())) return false;
    std::string name = readCString(data, size_t(base) + 8u, 64u);
    if (!isPrintableName(name)) return false;

    uint32_t height = readU32(data, size_t(base) + 0x48u);
    uint32_t width = readU32(data, size_t(base) + 0x4Cu);
    uint32_t bitDepth = readU32(data, size_t(base) + 0x50u);
    uint16_t mipCount = readU16(data, size_t(base) + 0x5Cu);
    uint32_t textureHeader = readU32(data, size_t(base) + 0x60u);

    LeedsTextureEntry parsed;
    bool valid = false;
    std::vector<uint32_t> headerCandidates;
    auto addHeaderCandidate = [&](uint64_t value) {
        if (value <= UINT32_MAX && value < data.size()) headerCandidates.push_back(uint32_t(value));
    };
    addHeaderCandidate(textureHeader);
    addHeaderCandidate(uint64_t(base) + uint64_t(textureHeader));

    const size_t initialCandidateCount = headerCandidates.size();
    for (size_t candidateIndex = 0; candidateIndex < initialCandidateCount; ++candidateIndex) {
        const uint32_t candidate = headerCandidates[candidateIndex];
        if (!checkedTextureRange(candidate, 4u, data.size())) continue;
        const uint32_t indirect = readU32(data, candidate);
        addHeaderCandidate(indirect);
        addHeaderCandidate(uint64_t(base) + uint64_t(indirect));
    }

    std::sort(headerCandidates.begin(), headerCandidates.end());
    headerCandidates.erase(std::unique(headerCandidates.begin(), headerCandidates.end()), headerCandidates.end());
    for (uint32_t candidate : headerCandidates) {
        if (parseDdsHeader(data, candidate, parsed)) {
            valid = true;
            break;
        }
    }
    if (!valid) return false;
    if ((width != 0u && uint32_t(parsed.width) != width) || (height != 0u && uint32_t(parsed.height) != height)) return false;
    if (bitDepth != 0u && bitDepth != parsed.reserved1 && bitDepth != parsed.bpp && bitDepth != 4u && bitDepth != 8u) {
        return false;
    }

    parsed.name = name;
    parsed.containerBase = base;
    if (mipCount != 0u) parsed.mipCount = uint8_t(std::min<uint16_t>(mipCount, 255u));
    entry = parsed;
    return true;
}

static bool parseTcdtCollection(const std::vector<uint8_t>& data, std::vector<LeedsTextureEntry>& entries, std::string& errorMessage) {
    if (data.size() < 0x30u || readU32(data, 0u) != 0x54444354u) return false;
    uint32_t declaredSize = readU32(data, 8u);
    if (declaredSize != 0u && declaredSize > data.size()) {
        errorMessage = "Texture collection declares a size beyond the input file.";
        return false;
    }

    uint32_t expectedCount = data[0x20u];
    if (expectedCount == 0u || expectedCount > 4096u) {
        errorMessage = "Texture collection has an invalid container count.";
        return false;
    }

    std::vector<uint32_t> rawPointers;
    auto addPointer = [&](uint32_t value) {
        if (value != 0u && value < data.size()) rawPointers.push_back(value);
    };
    addPointer(readU32(data, 0x24u));
    addPointer(readU32(data, 0x28u));

    uint32_t globalOffset = readU32(data, 0x0Cu);
    uint32_t globalCount = readU32(data, 0x14u);
    if (globalCount <= 1'000'000u && checkedTextureRange(globalOffset, size_t(globalCount) * 4u, data.size())) {
        for (uint32_t i = 0; i < globalCount; ++i) addPointer(readU32(data, size_t(globalOffset) + size_t(i) * 4u));
    }

    const size_t initialPointerCount = rawPointers.size();
    for (size_t i = 0; i < initialPointerCount; ++i) {
        uint32_t pointer = rawPointers[i];
        if (checkedTextureRange(pointer, 4u, data.size())) addPointer(readU32(data, pointer));
    }

    std::sort(rawPointers.begin(), rawPointers.end());
    rawPointers.erase(std::unique(rawPointers.begin(), rawPointers.end()), rawPointers.end());

    std::set<uint32_t> acceptedBases;
    const uint32_t shifts[] = {0u, 4u, 8u, 12u, 16u};
    for (uint32_t pointer : rawPointers) {
        for (uint32_t shift : shifts) {
            if (pointer < shift) continue;
            uint32_t base = pointer - shift;
            if (acceptedBases.count(base) != 0u) continue;
            LeedsTextureEntry entry;
            if (parseTcdtContainerAt(data, base, entry)) {
                acceptedBases.insert(base);
                entries.push_back(std::move(entry));
            }
        }
    }

    if (entries.size() < expectedCount) {
        size_t scanLimit = std::min<size_t>(data.size(), 64u * 1024u * 1024u);
        for (size_t base = 0x30u; base + 0x68u <= scanLimit && entries.size() < expectedCount; base += 4u) {
            if (acceptedBases.count(uint32_t(base)) != 0u) continue;
            LeedsTextureEntry entry;
            if (parseTcdtContainerAt(data, uint32_t(base), entry)) {
                acceptedBases.insert(uint32_t(base));
                entries.push_back(std::move(entry));
            }
        }
    }

    if (entries.empty()) {
        errorMessage = "Texture collection contained no supported texture records.";
        return false;
    }

    std::sort(entries.begin(), entries.end(), [](const LeedsTextureEntry& a, const LeedsTextureEntry& b) {
        return a.containerBase < b.containerBase;
    });
    if (entries.size() > expectedCount) entries.resize(expectedCount);
    return true;
}

static void decodeRgb565(uint16_t value, uint8_t& red, uint8_t& green, uint8_t& blue) {
    red = uint8_t(((value >> 11u) & 0x1Fu) * 255u / 31u);
    green = uint8_t(((value >> 5u) & 0x3Fu) * 255u / 63u);
    blue = uint8_t((value & 0x1Fu) * 255u / 31u);
}

static uint8_t expandMaskedChannel(uint32_t value, uint32_t mask, uint8_t defaultValue) {
    if (mask == 0u) return defaultValue;
    uint32_t shift = 0u;
    while (shift < 32u && ((mask >> shift) & 1u) == 0u) ++shift;
    uint32_t normalizedMask = mask >> shift;
    uint32_t bits = 0u;
    uint32_t temp = normalizedMask;
    while ((temp & 1u) != 0u && bits < 32u) {
        ++bits;
        temp >>= 1u;
    }
    if (bits == 0u || bits > 16u || temp != 0u) return defaultValue;
    const uint32_t maximum = (1u << bits) - 1u;
    const uint32_t channel = (value & mask) >> shift;
    return uint8_t((uint64_t(channel) * 255ull + maximum / 2u) / maximum);
}

static void writeDdsPixel(RgbaImage& image, int x, int y, const uint8_t color[4]) {
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) return;
    const size_t pixel = (size_t(y) * size_t(image.width) + size_t(x)) * 4u;
    image.rgba[pixel + 0u] = color[0];
    image.rgba[pixel + 1u] = color[1];
    image.rgba[pixel + 2u] = color[2];
    image.rgba[pixel + 3u] = color[3];
}

static void buildDxtColorTable(uint16_t color0, uint16_t color1, bool allowTransparent, uint8_t table[4][4]) {
    decodeRgb565(color0, table[0][0], table[0][1], table[0][2]);
    table[0][3] = 255u;
    decodeRgb565(color1, table[1][0], table[1][1], table[1][2]);
    table[1][3] = 255u;

    if (!allowTransparent || color0 > color1) {
        for (int channel = 0; channel < 3; ++channel) {
            table[2][channel] = uint8_t((2u * uint32_t(table[0][channel]) + uint32_t(table[1][channel])) / 3u);
            table[3][channel] = uint8_t((uint32_t(table[0][channel]) + 2u * uint32_t(table[1][channel])) / 3u);
        }
        table[2][3] = 255u;
        table[3][3] = 255u;
    } else {
        for (int channel = 0; channel < 3; ++channel) {
            table[2][channel] = uint8_t((uint32_t(table[0][channel]) + uint32_t(table[1][channel])) / 2u);
            table[3][channel] = 0u;
        }
        table[2][3] = 255u;
        table[3][3] = 0u;
    }
}

static bool decodeDdsDxt(const std::vector<uint8_t>& data, const LeedsTextureEntry& entry, RgbaImage& image, std::string& errorMessage) {
    const bool dxt1 = entry.flags == LeedsDxt1;
    const bool dxt3 = entry.flags == LeedsDxt3;
    const bool dxt5 = entry.flags == LeedsDxt5;
    if (!dxt1 && !dxt3 && !dxt5) {
        errorMessage = "Unsupported DDS block-compression format.";
        return false;
    }

    const size_t blockBytes = dxt1 ? 8u : 16u;
    const size_t blocksWide = (size_t(entry.width) + 3u) / 4u;
    const size_t blocksHigh = (size_t(entry.height) + 3u) / 4u;
    if (blocksWide != 0u && blocksHigh > (std::numeric_limits<size_t>::max)() / blocksWide) {
        errorMessage = "DDS block count overflows address space.";
        return false;
    }
    const size_t blockCount = blocksWide * blocksHigh;
    if (blockCount > (std::numeric_limits<size_t>::max)() / blockBytes) {
        errorMessage = "DDS payload size overflows address space.";
        return false;
    }
    const size_t payloadBytes = blockCount * blockBytes;
    if (!checkedTextureRange(entry.rasterOffset, payloadBytes, data.size())) {
        errorMessage = "DDS block payload points outside the file.";
        return false;
    }

    const uint8_t* source = data.data() + entry.rasterOffset;
    for (size_t blockY = 0u; blockY < blocksHigh; ++blockY) {
        for (size_t blockX = 0u; blockX < blocksWide; ++blockX) {
            const uint8_t* block = source + (blockY * blocksWide + blockX) * blockBytes;
            uint8_t alphaValues[16];
            std::fill(std::begin(alphaValues), std::end(alphaValues), uint8_t(255));
            const uint8_t* colorBlock = block;

            if (dxt3) {
                uint64_t alphaBits = 0u;
                for (int byteIndex = 0; byteIndex < 8; ++byteIndex) alphaBits |= uint64_t(block[byteIndex]) << (byteIndex * 8u);
                for (int pixel = 0; pixel < 16; ++pixel) {
                    const uint8_t alpha4 = uint8_t((alphaBits >> (pixel * 4u)) & 0xFu);
                    alphaValues[pixel] = uint8_t(alpha4 * 17u);
                }
                colorBlock = block + 8u;
            } else if (dxt5) {
                const uint8_t alpha0 = block[0];
                const uint8_t alpha1 = block[1];
                uint8_t alphaTable[8] = {alpha0, alpha1, 0, 0, 0, 0, 0, 0};
                if (alpha0 > alpha1) {
                    for (uint32_t index = 1u; index <= 6u; ++index) {
                        alphaTable[index + 1u] = uint8_t(((7u - index) * uint32_t(alpha0) + index * uint32_t(alpha1)) / 7u);
                    }
                } else {
                    for (uint32_t index = 1u; index <= 4u; ++index) {
                        alphaTable[index + 1u] = uint8_t(((5u - index) * uint32_t(alpha0) + index * uint32_t(alpha1)) / 5u);
                    }
                    alphaTable[6] = 0u;
                    alphaTable[7] = 255u;
                }
                uint64_t alphaIndexBits = 0u;
                for (int byteIndex = 0; byteIndex < 6; ++byteIndex) alphaIndexBits |= uint64_t(block[2 + byteIndex]) << (byteIndex * 8u);
                for (int pixel = 0; pixel < 16; ++pixel) alphaValues[pixel] = alphaTable[(alphaIndexBits >> (pixel * 3u)) & 0x7u];
                colorBlock = block + 8u;
            }

            const uint16_t color0 = uint16_t(colorBlock[0]) | (uint16_t(colorBlock[1]) << 8u);
            const uint16_t color1 = uint16_t(colorBlock[2]) | (uint16_t(colorBlock[3]) << 8u);
            uint8_t colorTable[4][4] = {};
            buildDxtColorTable(color0, color1, dxt1, colorTable);
            const uint32_t colorBits = uint32_t(colorBlock[4]) |
                                       (uint32_t(colorBlock[5]) << 8u) |
                                       (uint32_t(colorBlock[6]) << 16u) |
                                       (uint32_t(colorBlock[7]) << 24u);

            for (int localY = 0; localY < 4; ++localY) {
                for (int localX = 0; localX < 4; ++localX) {
                    const int pixelIndex = localY * 4 + localX;
                    const uint32_t colorIndex = (colorBits >> (pixelIndex * 2u)) & 0x3u;
                    uint8_t color[4] = {
                        colorTable[colorIndex][0],
                        colorTable[colorIndex][1],
                        colorTable[colorIndex][2],
                        colorTable[colorIndex][3]
                    };
                    if (!dxt1 || color[3] != 0u) color[3] = alphaValues[pixelIndex];
                    writeDdsPixel(image, int(blockX * 4u) + localX, int(blockY * 4u) + localY, color);
                }
            }
        }
    }
    return true;
}

static bool decodeDdsRgb(const std::vector<uint8_t>& data, const LeedsTextureEntry& entry, RgbaImage& image, std::string& errorMessage) {
    const uint32_t rgbBits = entry.reserved1;
    if (rgbBits != 24u && rgbBits != 32u) {
        errorMessage = "Unsupported DDS RGB bit depth.";
        return false;
    }
    const size_t bytesPerPixel = rgbBits / 8u;
    const size_t rowBytes = size_t(entry.width) * bytesPerPixel;
    if (rowBytes > (std::numeric_limits<size_t>::max)() / size_t(entry.height)) {
        errorMessage = "DDS RGB payload size overflows address space.";
        return false;
    }
    const size_t payloadBytes = rowBytes * size_t(entry.height);
    if (!checkedTextureRange(entry.rasterOffset, payloadBytes, data.size())) {
        errorMessage = "DDS RGB payload points outside the file.";
        return false;
    }

    const uint8_t* source = data.data() + entry.rasterOffset;
    for (int y = 0; y < entry.height; ++y) {
        for (int x = 0; x < entry.width; ++x) {
            const uint8_t* pixelBytes = source + size_t(y) * rowBytes + size_t(x) * bytesPerPixel;
            uint32_t packed = uint32_t(pixelBytes[0]) | (uint32_t(pixelBytes[1]) << 8u) | (uint32_t(pixelBytes[2]) << 16u);
            if (bytesPerPixel == 4u) packed |= uint32_t(pixelBytes[3]) << 24u;
            uint8_t color[4] = {
                expandMaskedChannel(packed, entry.redMask, pixelBytes[2]),
                expandMaskedChannel(packed, entry.greenMask, pixelBytes[1]),
                expandMaskedChannel(packed, entry.blueMask, pixelBytes[0]),
                expandMaskedChannel(packed, entry.alphaMask, 255u)
            };
            writeDdsPixel(image, x, y, color);
        }
    }
    return true;
}

static bool decodeDdsTexture(const std::vector<uint8_t>& data, const LeedsTextureEntry& entry, RgbaImage& image, std::string& errorMessage) {
    if (entry.width <= 0 || entry.height <= 0) {
        errorMessage = "DDS texture has invalid dimensions.";
        return false;
    }
    const uint64_t pixelCount = uint64_t(entry.width) * uint64_t(entry.height);
    if (pixelCount > LeedsMaxDecodedTextureBytes / 4ull) {
        errorMessage = "DDS texture is too large to decode safely.";
        return false;
    }
    image.width = entry.width;
    image.height = entry.height;
    image.rgba.assign(size_t(pixelCount) * 4u, uint8_t(0));

    if ((entry.reserved0 & 0x4u) != 0u) {
        if (!decodeDdsDxt(data, entry, image, errorMessage)) return false;
    } else if ((entry.reserved0 & 0x40u) != 0u) {
        if (!decodeDdsRgb(data, entry, image, errorMessage)) return false;
    } else {
        errorMessage = "Unsupported DDS pixel format.";
        return false;
    }
    return finishDecodedTexture(image);
}

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

static bool rwBuildStampSupported(uint32_t version) {
    return version == 0x00000310u ||
           version == 0x1003FFFFu ||
           version == 0x1803FFFFu;
}

static bool rwChunkHeaderValid(const std::vector<uint8_t>& data, size_t offset, size_t limit, uint32_t expectedType) {
    if (limit > data.size() || offset > limit || limit - offset < 12u) return false;
    uint32_t type = readU32(data, offset + 0u);
    uint32_t size = readU32(data, offset + 4u);
    uint32_t version = readU32(data, offset + 8u);
    if (type != expectedType || !rwBuildStampSupported(version)) return false;
    return size <= limit - offset - 12u;
}

static bool parseRwStringChunk(
    const std::vector<uint8_t>& data,
    size_t offset,
    size_t limit,
    std::string& textOut,
    size_t& nextOffsetOut
) {
    textOut.clear();
    nextOffsetOut = offset;
    if (!rwChunkHeaderValid(data, offset, limit, 0x02u)) return false;
    uint32_t size = readU32(data, offset + 4u);
    if (size > 1024u) return false;

    size_t payload = offset + 12u;
    for (uint32_t index = 0u; index < size; ++index) {
        unsigned char ch = data[payload + index];
        if (ch == 0u) break;
        if (ch < 32u || ch >= 127u) {
            textOut.clear();
            return false;
        }
        textOut.push_back(char(ch));
    }
    nextOffsetOut = payload + size_t(size);
    return true;
}


static std::string rwFixedAscii(const std::vector<uint8_t>& data, size_t offset, size_t length) {
    if (offset > data.size() || length > data.size() - offset) return {};
    std::string out;
    out.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        unsigned char ch = data[offset + i];
        if (ch == 0u) break;
        if (ch < 32u || ch >= 127u) return {};
        out.push_back(char(ch));
    }
    return out;
}

static bool parseLcsBetaRwTxd(
    const std::vector<uint8_t>& data,
    std::vector<LeedsTextureEntry>& entriesOut,
    std::string& errorMessage
) {
    entriesOut.clear();
    if (data.size() < 28u ||
        readU32(data, 0u) != 0x16u ||
        readU32(data, 8u) != 0x1003FFFFu) {
        return false;
    }

    const uint32_t rootPayloadSize = readU32(data, 4u);
    if (uint64_t(rootPayloadSize) + 12ull > data.size()) {
        errorMessage = "LCS beta TXD root chunk exceeds the file.";
        return false;
    }
    const size_t rootEnd = 12u + size_t(rootPayloadSize);

    size_t cursor = 12u;
    if (!rwChunkHeaderValid(data, cursor, rootEnd, 0x01u)) {
        errorMessage = "LCS beta TXD dictionary Struct is invalid.";
        return false;
    }
    const uint32_t dictionaryStructSize = readU32(data, cursor + 4u);
    if (dictionaryStructSize < 4u || dictionaryStructSize > 32u) {
        errorMessage = "LCS beta TXD dictionary Struct size is invalid.";
        return false;
    }
    const uint32_t textureCount = readU16(data, cursor + 12u);
    if (textureCount > 4096u) {
        errorMessage = "LCS beta TXD texture count is unreasonable.";
        return false;
    }
    cursor += 12u + size_t(dictionaryStructSize);

    // Empty dictionaries are valid in the May 2005 beta data.
    if (textureCount == 0u) {
        errorMessage.clear();
        return true;
    }

    entriesOut.reserve(textureCount);
    for (uint32_t textureIndex = 0u; textureIndex < textureCount; ++textureIndex) {
        if (!rwChunkHeaderValid(data, cursor, rootEnd, 0x15u)) {
            errorMessage = "LCS beta TXD ended before all native textures were parsed.";
            entriesOut.clear();
            return false;
        }
        const size_t nativeEnd = cursor + 12u + size_t(readU32(data, cursor + 4u));
        const size_t nativeBase = cursor;
        size_t child = cursor + 12u;

        if (!rwChunkHeaderValid(data, child, nativeEnd, 0x01u)) {
            errorMessage = "LCS beta native texture Struct is invalid.";
            entriesOut.clear();
            return false;
        }
        const uint32_t structSize = readU32(data, child + 4u);
        const size_t payload = child + 12u;
        if (structSize < 0x5Cu || payload > nativeEnd || structSize > nativeEnd - payload) {
            errorMessage = "LCS beta native texture Struct is truncated.";
            entriesOut.clear();
            return false;
        }

        // Verified May-2005 PSP native texture payload:
        // +00 platform id (8), +04 filter/addressing,
        // +08 name[32], +28 mask[32], +48 raster format,
        // +50 width/height packed u16/u16,
        // +54 depth/mips/flags, +58 base raster byte count, +5C raster.
        const uint32_t platformId = readU32(data, payload + 0x00u);
        if (platformId != 8u) {
            errorMessage = "LCS beta TXD contains a non-PSP native texture.";
            entriesOut.clear();
            return false;
        }

        const uint32_t dimensions = readU32(data, payload + 0x50u);
        const uint32_t packedFormat = readU32(data, payload + 0x54u);
        const uint32_t rasterBytes = readU32(data, payload + 0x58u);
        const uint32_t width = dimensions & 0xFFFFu;
        const uint32_t height = (dimensions >> 16u) & 0xFFFFu;
        uint32_t bpp = packedFormat & 0xFFu;
        uint32_t mipCount = (packedFormat >> 8u) & 0xFFu;
        if (mipCount == 0u) mipCount = 1u;

        if (!dimensionsReasonable(int(width), int(height)) ||
            (bpp != 4u && bpp != 8u && bpp != 16u && bpp != 32u) ||
            mipCount > 16u) {
            errorMessage = "LCS beta TXD texture dimensions/BPP/mip count are unsupported.";
            entriesOut.clear();
            return false;
        }

        const size_t rasterOffset = payload + 0x5Cu;
        const uint64_t minimumBaseBytes =
            bpp == 4u ? (uint64_t(width) * uint64_t(height) + 1ull) / 2ull :
            bpp == 8u ? uint64_t(width) * uint64_t(height) :
            bpp == 16u ? uint64_t(width) * uint64_t(height) * 2ull :
                         uint64_t(width) * uint64_t(height) * 4ull;
        if (rasterBytes < minimumBaseBytes ||
            uint64_t(rasterOffset) + uint64_t(rasterBytes) > uint64_t(payload) + uint64_t(structSize)) {
            errorMessage = "LCS beta TXD raster payload is truncated.";
            entriesOut.clear();
            return false;
        }

        LeedsTextureEntry entry;
        entry.name = rwFixedAscii(data, payload + 0x08u, 32u);
        if (entry.name.empty()) entry.name = "texture_" + std::to_string(textureIndex);
        entry.kind = TextureKind::RwPsp;
        entry.containerBase = uint32_t(nativeBase);
        entry.textureHeaderOffset = uint32_t(payload);
        entry.rasterOffset = uint32_t(rasterOffset);
        entry.blockSize = rasterBytes;
        entry.flags = readU32(data, payload + 0x48u);
        entry.reserved0 = readU32(data, payload + 0x04u);
        entry.width = int(width);
        entry.height = int(height);
        entry.widthPow2 = isPowerOfTwoInt(int(width)) ? log2ExactInt(int(width)) : 0u;
        entry.heightPow2 = isPowerOfTwoInt(int(height)) ? log2ExactInt(int(height)) : 0u;
        entry.bpp = uint8_t(bpp);
        entry.mipCount = uint8_t(mipCount);

        // 32bpp beta samples are linear RGBA. Paletted payloads continue to use
        // the existing PSP native unswizzle/palette path.
        entry.swizzleWidth =
            bpp == 32u ? 0u :
            uint16_t(bpp == 4u ? std::max<uint32_t>(1u, width / 2u) : width);
        entriesOut.push_back(entry);

        cursor = nativeEnd;
        if (rwChunkHeaderValid(data, cursor, rootEnd, 0x03u)) {
            cursor += 12u + size_t(readU32(data, cursor + 4u));
        }
    }

    errorMessage.clear();
    return true;
}


static bool parseGtaSaRwTxd(
    const std::vector<uint8_t>& data,
    std::vector<LeedsTextureEntry>& entriesOut,
    std::string& errorMessage
) {
    entriesOut.clear();
    if (data.size() < 32u ||
        readU32(data, 0u) != 0x16u ||
        readU32(data, 8u) != 0x1803FFFFu) {
        return false;
    }

    const size_t rootEnd =
        std::min<size_t>(
            data.size(),
            12u + size_t(readU32(data, 4u)));
    size_t cursor = 12u;

    if (!rwChunkHeaderValid(data, cursor, rootEnd, 0x01u)) {
        errorMessage = "GTA SA TXD has an invalid dictionary header.";
        return false;
    }

    const uint32_t dictionaryStructSize = readU32(data, cursor + 4u);
    if (dictionaryStructSize < 4u ||
        cursor + 12u + dictionaryStructSize > rootEnd) {
        errorMessage = "GTA SA TXD dictionary header is truncated.";
        return false;
    }

    const uint32_t textureCount = readU16(data, cursor + 12u);
    if (textureCount > 4096u) {
        errorMessage = "GTA SA TXD texture count is unreasonable.";
        return false;
    }

    cursor += 12u + size_t(dictionaryStructSize);
    entriesOut.reserve(textureCount);

    for (uint32_t textureIndex = 0u;
         textureIndex < textureCount;
         ++textureIndex) {
        if (!rwChunkHeaderValid(data, cursor, rootEnd, 0x15u)) {
            errorMessage = "GTA SA TXD ended before all textures were read.";
            entriesOut.clear();
            return false;
        }

        const size_t nativeEnd =
            cursor + 12u + size_t(readU32(data, cursor + 4u));
        const size_t nativeBase = cursor;
        size_t child = cursor + 12u;

        if (!rwChunkHeaderValid(data, child, nativeEnd, 0x01u)) {
            errorMessage = "GTA SA texture has an invalid data block.";
            entriesOut.clear();
            return false;
        }

        const uint32_t structSize = readU32(data, child + 4u);
        const size_t payload = child + 12u;
        if (structSize < 88u ||
            payload + size_t(structSize) > nativeEnd) {
            errorMessage = "GTA SA texture header is truncated.";
            entriesOut.clear();
            return false;
        }

        const uint32_t platformId = readU32(data, payload + 0u);
        if (platformId != 9u) {
            errorMessage = "GTA SA TXD contains a non-PC texture.";
            entriesOut.clear();
            return false;
        }

        const uint32_t rasterFormat = readU32(data, payload + 72u);
        const uint32_t d3dFormat = readU32(data, payload + 76u);
        const uint16_t width = readU16(data, payload + 80u);
        const uint16_t height = readU16(data, payload + 82u);
        const uint8_t depth = data[payload + 84u];
        const uint8_t mipCount = data[payload + 85u];
        const uint8_t rasterType = data[payload + 86u];
        const uint8_t flags = data[payload + 87u];

        if (!dimensionsReasonable(int(width), int(height)) ||
            mipCount == 0u || mipCount > 16u) {
            errorMessage = "GTA SA texture dimensions or mip count are invalid.";
            entriesOut.clear();
            return false;
        }

        size_t rasterCursor = payload + 88u;

        // PC palettes, when present, precede the mip payloads.
        const bool pal8 = (rasterFormat & 0x2000u) != 0u;
        const bool pal4 = (rasterFormat & 0x4000u) != 0u;
        if (pal8) {
            if (rasterCursor + 256u * 4u > payload + structSize) {
                errorMessage = "GTA SA texture palette is truncated.";
                entriesOut.clear();
                return false;
            }
            rasterCursor += 256u * 4u;
        } else if (pal4) {
            // GTA SA PC PAL4 data is uncommon. Reserve the conventional
            // 32-entry PC palette so unsupported PAL4 files fail safely later.
            if (rasterCursor + 32u * 4u > payload + structSize) {
                errorMessage = "GTA SA texture palette is truncated.";
                entriesOut.clear();
                return false;
            }
            rasterCursor += 32u * 4u;
        }

        if (rasterCursor + 4u > payload + structSize) {
            errorMessage = "GTA SA texture has no raster payload.";
            entriesOut.clear();
            return false;
        }

        const uint32_t rasterBytes = readU32(data, rasterCursor);
        rasterCursor += 4u;
        if (uint64_t(rasterCursor) + uint64_t(rasterBytes) >
            uint64_t(payload) + uint64_t(structSize)) {
            errorMessage = "GTA SA texture raster payload is truncated.";
            entriesOut.clear();
            return false;
        }

        LeedsTextureEntry entry;
        entry.name = rwFixedAscii(data, payload + 8u, 32u);
        if (entry.name.empty()) {
            entry.name = "texture_" + std::to_string(textureIndex);
        }
        entry.kind = TextureKind::RwPc;
        entry.containerBase = uint32_t(nativeBase);
        entry.textureHeaderOffset = uint32_t(payload);
        entry.rasterOffset = uint32_t(rasterCursor);
        entry.blockSize = rasterBytes;
        entry.flags = d3dFormat;
        entry.reserved0 = rasterFormat;
        entry.reserved1 =
            uint32_t(depth) |
            (uint32_t(rasterType) << 8u) |
            (uint32_t(flags) << 16u);
        entry.width = int(width);
        entry.height = int(height);
        entry.bpp = depth;
        entry.mipCount = mipCount;
        entriesOut.push_back(entry);

        cursor = nativeEnd;
        if (rwChunkHeaderValid(data, cursor, rootEnd, 0x03u)) {
            cursor += 12u + size_t(readU32(data, cursor + 4u));
        }
    }

    errorMessage.clear();
    return true;
}

static bool parseMobileLcsRwTxd(
    const std::vector<uint8_t>& data,
    std::vector<LeedsTextureEntry>& entriesOut,
    std::string& errorMessage
) {
    entriesOut.clear();
    if (data.size() < 32u || readU32(data, 0u) != 0x16u || readU32(data, 8u) != 0x00000310u) return false;

    size_t limit = data.size();
    size_t cursor = 12u;
    if (!rwChunkHeaderValid(data, cursor, limit, 0x01u)) {
        errorMessage = "Mobile LCS RenderWare TXD has an invalid dictionary Struct chunk.";
        return false;
    }

    uint32_t dictionaryStructSize = readU32(data, cursor + 4u);
    if (dictionaryStructSize < 4u || dictionaryStructSize > 32u) {
        errorMessage = "Mobile LCS RenderWare TXD has an invalid dictionary Struct size.";
        return false;
    }
    uint32_t textureCount = readU16(data, cursor + 12u);
    if (textureCount > 4096u) {
        errorMessage = "Mobile LCS RenderWare TXD has an invalid texture count.";
        return false;
    }
    cursor += 12u + size_t(dictionaryStructSize);
    if (textureCount == 0u) {
        errorMessage.clear();
        return true;
    }

    entriesOut.reserve(textureCount);
    for (uint32_t textureIndex = 0u; textureIndex < textureCount; ++textureIndex) {
        if (cursor + 12u > limit || readU32(data, cursor + 0u) != 0x15u || readU32(data, cursor + 8u) != 0x00000310u) {
            errorMessage = "Mobile LCS RenderWare TXD ended before all native textures were parsed.";
            entriesOut.clear();
            return false;
        }

        size_t nativeOffset = cursor;
        size_t child = cursor + 12u;
        if (!rwChunkHeaderValid(data, child, limit, 0x01u)) {
            errorMessage = "Mobile LCS RenderWare native texture has an invalid platform Struct chunk.";
            entriesOut.clear();
            return false;
        }
        uint32_t platformStructSize = readU32(data, child + 4u);
        if (platformStructSize < 8u || platformStructSize > 64u || readU32(data, child + 12u) != 0x00505350u) {
            errorMessage = "RenderWare TXD is not the supported Mobile LCS PSP-native texture layout.";
            entriesOut.clear();
            return false;
        }
        child += 12u + size_t(platformStructSize);

        std::string textureName;
        size_t nextChild = child;
        if (!parseRwStringChunk(data, child, limit, textureName, nextChild)) {
            errorMessage = "Mobile LCS RenderWare native texture has an invalid name chunk.";
            entriesOut.clear();
            return false;
        }
        child = nextChild;

        std::string maskName;
        if (!parseRwStringChunk(data, child, limit, maskName, nextChild)) {
            errorMessage = "Mobile LCS RenderWare native texture has an invalid mask-name chunk.";
            entriesOut.clear();
            return false;
        }
        child = nextChild;

        if (!rwChunkHeaderValid(data, child, limit, 0x01u)) {
            errorMessage = "Mobile LCS RenderWare native texture has an invalid raster container.";
            entriesOut.clear();
            return false;
        }
        size_t rasterContainerEnd = child + 12u + size_t(readU32(data, child + 4u));
        size_t rasterChild = child + 12u;
        if (!rwChunkHeaderValid(data, rasterChild, rasterContainerEnd, 0x01u) || readU32(data, rasterChild + 4u) < 20u) {
            errorMessage = "Mobile LCS RenderWare native texture has an invalid raster header.";
            entriesOut.clear();
            return false;
        }

        size_t rasterHeaderPayload = rasterChild + 12u;
        uint32_t width = readU32(data, rasterHeaderPayload + 0u);
        uint32_t height = readU32(data, rasterHeaderPayload + 4u);
        uint32_t bpp = readU32(data, rasterHeaderPayload + 8u);
        uint32_t mipCount = readU32(data, rasterHeaderPayload + 12u);
        uint32_t rasterFlags = readU32(data, rasterHeaderPayload + 16u);
        if (!dimensionsReasonable(int(width), int(height)) || !bppReasonablePsp(bpp) || mipCount == 0u || mipCount > 16u) {
            errorMessage = "Mobile LCS RenderWare native texture has unsupported dimensions, BPP, or mip count.";
            entriesOut.clear();
            return false;
        }

        rasterChild += 12u + size_t(readU32(data, rasterChild + 4u));
        if (!rwChunkHeaderValid(data, rasterChild, rasterContainerEnd, 0x01u)) {
            errorMessage = "Mobile LCS RenderWare native texture has no raster payload chunk.";
            entriesOut.clear();
            return false;
        }

        LeedsTextureEntry entry;
        entry.name = textureName.empty() ? ("texture_" + std::to_string(textureIndex)) : textureName;
        entry.kind = TextureKind::RwPsp;
        entry.containerBase = uint32_t(nativeOffset);
        entry.textureHeaderOffset = uint32_t(rasterHeaderPayload);
        entry.rasterOffset = uint32_t(rasterChild + 12u);
        entry.blockSize = readU32(data, rasterChild + 4u);
        entry.flags = rasterFlags;
        entry.width = int(width);
        entry.height = int(height);
        entry.widthPow2 = isPowerOfTwoInt(int(width)) ? log2ExactInt(int(width)) : 0u;
        entry.heightPow2 = isPowerOfTwoInt(int(height)) ? log2ExactInt(int(height)) : 0u;
        entry.bpp = uint8_t(bpp);
        entry.mipCount = uint8_t(mipCount);
        // Mobile LCS RenderWare PSP dictionaries use the native 16-byte by
        // 8-row PSP raster swizzle even though the compact RW raster flags are
        // zero.  Preserve the derived byte width so the existing PSP decoder
        // unswizzles the base level before palette lookup.
        entry.swizzleWidth = uint16_t(bpp == 4u ? std::max<uint32_t>(1u, width / 2u) : width);
        entriesOut.push_back(entry);

        cursor = rasterContainerEnd;
        if (rwChunkHeaderValid(data, cursor, limit, 0x03u)) {
            cursor += 12u + size_t(readU32(data, cursor + 4u));
        }
    }

    errorMessage.clear();
    return !entriesOut.empty();
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
    // Match librw's PS2 PAL4/PAL8 rasterLock path. The swizzle permutation is
    // local to each four-row transfer slice. Treating the address as a global
    // texture index (and taking modulo texture size) repeats data from the top
    // of the image and is what produced the badly corrupted 4bpp previews.
    if (width <= 0 || height <= 0 || indices.size() < size_t(width) * size_t(height)) return {};
    std::vector<uint8_t> out(size_t(width) * size_t(height), 0u);
    const int logw = log2Width(width);
    const uint32_t mask = (logw + 2 >= 31) ? 0xFFFFFFFFu : ((1u << uint32_t(logw + 2)) - 1u);
    for (int y0 = 0; y0 < height; y0 += 4) {
        const int rows = std::min(4, height - y0);
        const size_t base = size_t(y0) * size_t(width);
        const size_t sliceCount = size_t(rows) * size_t(width);
        std::vector<uint8_t> slice(indices.begin() + base, indices.begin() + base + sliceCount);
        for (int i = 0; i < rows; ++i) {
            for (int x = 0; x < width; ++x) {
                const uint32_t source = swizzlePs2Index(x, y0 + i, logw) & mask;
                if (source < slice.size())
                    out[base + size_t(i) * size_t(width) + size_t(x)] = slice[source];
            }
        }
    }
    return out;
}

static std::vector<uint8_t> swizzlePs2Indices(const std::vector<uint8_t>& linear, int width, int height) {
    if (width <= 0 || height <= 0 || linear.size() < size_t(width) * size_t(height)) return {};
    std::vector<uint8_t> out(size_t(width) * size_t(height), 0u);
    const int logw = log2Width(width);
    const uint32_t mask = (logw + 2 >= 31) ? 0xFFFFFFFFu : ((1u << uint32_t(logw + 2)) - 1u);
    for (int y0 = 0; y0 < height; y0 += 4) {
        const int rows = std::min(4, height - y0);
        const size_t base = size_t(y0) * size_t(width);
        const size_t sliceCount = size_t(rows) * size_t(width);
        std::vector<uint8_t> slice(sliceCount, 0u);
        for (int i = 0; i < rows; ++i) {
            for (int x = 0; x < width; ++x) {
                const uint32_t target = swizzlePs2Index(x, y0 + i, logw) & mask;
                if (target < slice.size())
                    slice[target] = linear[base + size_t(i) * size_t(width) + size_t(x)];
            }
        }
        std::copy(slice.begin(), slice.end(), out.begin() + base);
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
    if (isPspTextureKind(entry.kind) && entry.blockSize >= paletteBytes) {
        paletteStartCandidates.push_back(entry.rasterOffset + entry.blockSize - paletteBytes);
    }

    uint32_t payloadBytes = rasterPayloadBytesBeforePalette(entry);
    if (payloadBytes != 0) {
        paletteStartCandidates.push_back(entry.rasterOffset + payloadBytes);
    }

    if (!isPspTextureKind(entry.kind) && entry.blockSize >= paletteBytes) {
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

    // 0x08 is the logical Leeds collection length, not the padded backing-file
    // length. Retail PS2 XTX files (and known-good modified plr.xtx files) keep
    // this at the end of the relocation table, while the physical file may
    // contain trailing alignment/sector padding. Replacing a raster used to
    // overwrite 0x08 with data.size(), accidentally folding that padding into
    // the logical archive. Preserve the old logical boundary and shift it only
    // when the edited raster lies before it.
    if (data.size() >= 0x0C) {
        const uint32_t oldLogicalSize = readU32(data, 0x08);
        uint32_t newLogicalSize = oldLogicalSize;
        if (oldLogicalSize >= oldEnd && oldLogicalSize <= oldFileSize) {
            const int64_t shifted = int64_t(oldLogicalSize) + delta;
            if (shifted >= 0 && shifted <= int64_t(data.size())) {
                newLogicalSize = uint32_t(shifted);
            }
        }
        writeU32(data, 0x08, newLogicalSize);
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

        uint32_t reserved1 = readU32(data, entry.textureHeaderOffset + 4);
        const bool generic8 = entry.bpp == 8u && (reserved1 & 0xFFFF0000u) == 0x00250000u;
        const bool generic4 = entry.bpp == 4u && (reserved1 & 0xFFFF0000u) == 0x00450000u;
        // Zero is not a VCS player-texture convention. It was a Storyland bug.
        // Repair zero transfer words whenever dimensions are rewritten, and
        // refresh normal retail-style transfer words when width changes.
        if (reserved1 == 0u || generic8 || generic4) {
            reserved1 = defaultPs2Reserved1(width, entry.bpp);
            writeU32(data, entry.textureHeaderOffset + 4, reserved1);
        }

        const uint8_t unknownBits = uint8_t((entry.flags >> 12u) & 0x03u);
        const uint8_t mipCount = entry.mipCount == 0u ? 1u : entry.mipCount;
        const uint32_t flags = packPs2RuntimeFlags(
            widthPow2,
            heightPow2,
            entry.bpp,
            mipCount,
            entry.swizzleMask,
            unknownBits);
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
    // palette" fallback picked mip pixels as colours and made PSP textures look
    // scrambled/garbage.
    if (isPspTextureKind(entry.kind) && entry.blockSize >= paletteBytes) {
        paletteStartCandidates.push_back(entry.rasterOffset + entry.blockSize - paletteBytes);
    }

    uint32_t payloadBytes = rasterPayloadBytesBeforePalette(entry);
    if (payloadBytes != 0) {
        paletteStartCandidates.push_back(entry.rasterOffset + payloadBytes);
    }

    if (!isPspTextureKind(entry.kind) && entry.blockSize >= paletteBytes) {
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
    int bestScore = (std::numeric_limits<int>::max)();
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


static bool isLcsBetaRwTxdBytes(const std::vector<uint8_t>& data) {
    return data.size() >= 28u &&
           readU32(data, 0u) == 0x16u &&
           readU32(data, 8u) == 0x1003FFFFu &&
           uint64_t(readU32(data, 4u)) + 12ull <= data.size();
}

static void appendU32Le(std::vector<uint8_t>& out, uint32_t value) {
    const size_t base = out.size();
    out.resize(base + 4u);
    writeU32(out, base, value);
}

static void writeU16Le(std::vector<uint8_t>& out, size_t offset, uint16_t value) {
    if (offset > out.size() || out.size() - offset < 2u) return;
    out[offset + 0u] = uint8_t(value & 0xFFu);
    out[offset + 1u] = uint8_t((value >> 8u) & 0xFFu);
}

static void appendRwChunk(
    std::vector<uint8_t>& out,
    uint32_t type,
    uint32_t version,
    const std::vector<uint8_t>& payload
) {
    appendU32Le(out, type);
    appendU32Le(out, uint32_t(payload.size()));
    appendU32Le(out, version);
    out.insert(out.end(), payload.begin(), payload.end());
}

static bool buildLcsBetaRwNativeTextureChunk(
    const std::string& name,
    const RgbaImage& sourceImage,
    uint8_t bpp,
    std::vector<uint8_t>& nativeChunk,
    std::string& errorMessage
) {
    nativeChunk.clear();
    if (bpp != 4u && bpp != 8u) {
        errorMessage = "LCS beta TXD authoring currently supports 4bpp or 8bpp indexed PSP textures.";
        return false;
    }
    if (name.empty() || name.size() > 31u || !isPrintableName(name)) {
        errorMessage = "LCS beta TXD texture names must be 1-31 printable ASCII characters.";
        return false;
    }

    RgbaImage image;
    std::string resizeNote;
    if (!normalizePs2TextureImage(sourceImage, image, resizeNote)) {
        errorMessage = resizeNote;
        return false;
    }

    const int paletteEntries = bpp == 4u ? 16 : 256;
    std::vector<uint8_t> palette =
        buildPaletteFromReplacementImage(image, paletteEntries, false);
    if (palette.size() != size_t(paletteEntries) * 4u) {
        errorMessage = "Could not build the PSP TXD palette.";
        return false;
    }

    const size_t pixelCount = size_t(image.width) * size_t(image.height);
    std::vector<uint8_t> indices(pixelCount, 0u);
    for (size_t i = 0u; i < pixelCount; ++i) {
        indices[i] = uint8_t(nearestPaletteIndex(
            palette,
            paletteEntries,
            image.rgba[i * 4u + 0u],
            image.rgba[i * 4u + 1u],
            image.rgba[i * 4u + 2u],
            image.rgba[i * 4u + 3u]));
    }

    std::vector<uint8_t> linearRaster =
        bpp == 4u ? packNibblesLoFirst(indices) : indices;

    std::vector<uint8_t> swizzledRaster = swizzlePspBytes(
        linearRaster.data(),
        linearRaster.size(),
        image.width,
        image.height,
        1,
        bpp == 4u ? 2 : 1);

    if (swizzledRaster.size() != linearRaster.size()) {
        errorMessage = "Could not swizzle the PSP TXD raster safely.";
        return false;
    }

    std::vector<uint8_t> rasterBlock = std::move(swizzledRaster);
    rasterBlock.insert(rasterBlock.end(), palette.begin(), palette.end());
    if (rasterBlock.size() > 0xFFFFFFFFu) {
        errorMessage = "PSP TXD raster block is too large.";
        return false;
    }

    // May-2005 LCS beta native-texture Struct. This matches the supplied
    // 0x1003FFFF beta TXDs: platform=8, filter/addressing=0x1102,
    // raster format=0x600, width/height packed at +0x50 and native raster
    // byte count at +0x58.
    std::vector<uint8_t> nativeStructPayload(0x5Cu, 0u);
    writeU32(nativeStructPayload, 0x00u, 8u);
    writeU32(nativeStructPayload, 0x04u, 0x00001102u);
    std::copy(
        name.begin(),
        name.end(),
        nativeStructPayload.begin() + 0x08u);
    writeU32(nativeStructPayload, 0x48u, 0x00000600u);
    writeU32(
        nativeStructPayload,
        0x50u,
        uint32_t(uint16_t(image.width)) |
        (uint32_t(uint16_t(image.height)) << 16u));
    writeU32(
        nativeStructPayload,
        0x54u,
        0x00040000u | 0x00000100u | uint32_t(bpp));
    writeU32(
        nativeStructPayload,
        0x58u,
        uint32_t(rasterBlock.size()));
    nativeStructPayload.insert(
        nativeStructPayload.end(),
        rasterBlock.begin(),
        rasterBlock.end());

    std::vector<uint8_t> nativePayload;
    appendRwChunk(
        nativePayload,
        0x01u,
        0x1003FFFFu,
        nativeStructPayload);
    appendRwChunk(
        nativePayload,
        0x03u,
        0x1003FFFFu,
        {});

    appendRwChunk(
        nativeChunk,
        0x15u,
        0x1003FFFFu,
        nativePayload);

    errorMessage.clear();
    return true;
}

struct StorylandRwTopLevelChunk {
    uint32_t type = 0u;
    size_t offset = 0u;
    size_t totalSize = 0u;
};

static bool collectLcsBetaTxdTopLevelChunks(
    const std::vector<uint8_t>& data,
    std::vector<StorylandRwTopLevelChunk>& chunks,
    std::string& errorMessage
) {
    chunks.clear();
    if (!isLcsBetaRwTxdBytes(data)) {
        errorMessage = "Texture archive is not a supported LCS beta RenderWare TXD.";
        return false;
    }

    const size_t rootEnd = 12u + size_t(readU32(data, 4u));
    size_t cursor = 12u;
    while (cursor < rootEnd) {
        if (rootEnd - cursor < 12u) {
            errorMessage = "LCS beta TXD has a truncated top-level chunk header.";
            return false;
        }

        const uint32_t type = readU32(data, cursor + 0u);
        const uint32_t payloadSize = readU32(data, cursor + 4u);
        const uint32_t version = readU32(data, cursor + 8u);
        const uint64_t totalSize64 = 12ull + uint64_t(payloadSize);
        if (version != 0x1003FFFFu ||
            totalSize64 > uint64_t(rootEnd - cursor)) {
            errorMessage = "LCS beta TXD contains an invalid top-level chunk.";
            return false;
        }

        chunks.push_back({
            type,
            cursor,
            size_t(totalSize64)
        });
        cursor += size_t(totalSize64);
    }

    if (cursor != rootEnd || chunks.empty() || chunks.front().type != 0x01u) {
        errorMessage = "LCS beta TXD top-level layout is invalid.";
        return false;
    }
    return true;
}

static bool rebuildLcsBetaTxdFromNativeChunks(
    const std::vector<uint8_t>& original,
    const std::vector<std::vector<uint8_t>>& nativeChunks,
    std::vector<uint8_t>& rebuilt,
    std::string& errorMessage
) {
    if (nativeChunks.size() > 0xFFFFu) {
        errorMessage = "LCS beta TXD texture count exceeds 65535.";
        return false;
    }

    std::vector<StorylandRwTopLevelChunk> top;
    if (!collectLcsBetaTxdTopLevelChunks(original, top, errorMessage)) {
        return false;
    }

    const StorylandRwTopLevelChunk& dictionaryStruct = top.front();
    if (dictionaryStruct.totalSize < 16u ||
        readU32(original, dictionaryStruct.offset + 0u) != 0x01u) {
        errorMessage = "LCS beta TXD dictionary Struct is missing.";
        return false;
    }

    std::vector<uint8_t> dictionaryBytes(
        original.begin() + dictionaryStruct.offset,
        original.begin() + dictionaryStruct.offset + dictionaryStruct.totalSize);
    if (dictionaryBytes.size() < 16u) {
        errorMessage = "LCS beta TXD dictionary Struct is truncated.";
        return false;
    }
    writeU16Le(dictionaryBytes, 12u, uint16_t(nativeChunks.size()));

    std::vector<uint8_t> rootPayload;
    rootPayload.insert(
        rootPayload.end(),
        dictionaryBytes.begin(),
        dictionaryBytes.end());

    for (const std::vector<uint8_t>& native : nativeChunks) {
        if (native.size() < 12u ||
            readU32(native, 0u) != 0x15u ||
            readU32(native, 8u) != 0x1003FFFFu ||
            uint64_t(readU32(native, 4u)) + 12ull != native.size()) {
            errorMessage = "Attempted to serialize an invalid LCS beta native-texture chunk.";
            return false;
        }
        rootPayload.insert(rootPayload.end(), native.begin(), native.end());
    }

    // Preserve non-native top-level chunks (normally the root Extension)
    // byte-for-byte and in their original order.
    for (size_t i = 1u; i < top.size(); ++i) {
        if (top[i].type == 0x15u) continue;
        rootPayload.insert(
            rootPayload.end(),
            original.begin() + top[i].offset,
            original.begin() + top[i].offset + top[i].totalSize);
    }

    rebuilt.clear();
    appendRwChunk(
        rebuilt,
        0x16u,
        0x1003FFFFu,
        rootPayload);

    LeedsTextureArchive trial;
    std::string trialError;
    if (!trial.loadFromMemory(
            rebuilt,
            LeedsPlatform::Auto,
            trialError,
            L"storyland-authored.txd")) {
        errorMessage = "Rebuilt LCS beta TXD failed parse verification: " + trialError;
        rebuilt.clear();
        return false;
    }
    if (trial.textures().size() != nativeChunks.size()) {
        errorMessage = "Rebuilt LCS beta TXD texture count did not round-trip.";
        rebuilt.clear();
        return false;
    }

    errorMessage.clear();
    return true;
}

static bool collectLcsBetaTxdNativeChunks(
    const std::vector<uint8_t>& data,
    std::vector<std::vector<uint8_t>>& nativeChunks,
    std::string& errorMessage
) {
    nativeChunks.clear();
    std::vector<StorylandRwTopLevelChunk> top;
    if (!collectLcsBetaTxdTopLevelChunks(data, top, errorMessage)) {
        return false;
    }
    for (const StorylandRwTopLevelChunk& child : top) {
        if (child.type != 0x15u) continue;
        nativeChunks.emplace_back(
            data.begin() + child.offset,
            data.begin() + child.offset + child.totalSize);
    }
    return true;
}

static bool patchLcsBetaNativeChunkName(
    std::vector<uint8_t>& nativeChunk,
    const std::string& name,
    std::string& errorMessage
) {
    if (name.empty() || name.size() > 31u || !isPrintableName(name)) {
        errorMessage = "LCS beta TXD texture names must be 1-31 printable ASCII characters.";
        return false;
    }
    if (nativeChunk.size() < 44u ||
        readU32(nativeChunk, 0u) != 0x15u ||
        readU32(nativeChunk, 12u) != 0x01u) {
        errorMessage = "LCS beta native texture chunk has an invalid Struct layout.";
        return false;
    }

    // Native +0x0C child Struct header, then Struct payload +0x08 name[32].
    const size_t nameOffset = 12u + 12u + 8u;
    if (nameOffset + 32u > nativeChunk.size()) {
        errorMessage = "LCS beta native texture name field is truncated.";
        return false;
    }
    std::fill(
        nativeChunk.begin() + nameOffset,
        nativeChunk.begin() + nameOffset + 32u,
        0u);
    std::copy(
        name.begin(),
        name.end(),
        nativeChunk.begin() + nameOffset);
    return true;
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

const std::vector<uint8_t>& LeedsTextureArchive::rawBytes() const {
    return dataBytes;
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

    if (uint64_t(fileSize) > LeedsMaxTextureFileBytes ||
        uint64_t(fileSize) > uint64_t((std::numeric_limits<size_t>::max)())) {
        fclose(file);
        errorMessage = "Texture file is too large to load safely.";
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
    std::ifstream file(std::filesystem::path(filePath), std::ios::binary);
    if (!file) {
        errorMessage = "Could not open input file.";
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff fileSize = file.tellg();
    if (fileSize < 0 || uint64_t(fileSize) > LeedsMaxTextureFileBytes ||
        uint64_t(fileSize) > uint64_t((std::numeric_limits<size_t>::max)()) ||
        uint64_t(fileSize) > uint64_t((std::numeric_limits<std::streamsize>::max)())) {
        errorMessage = "Texture file is too large or its size could not be determined safely.";
        return false;
    }
    file.seekg(0, std::ios::beg);
    dataBytes.assign(size_t(fileSize), uint8_t(0));
    if (!dataBytes.empty()) {
        file.read(reinterpret_cast<char*>(dataBytes.data()), std::streamsize(fileSize));
        if (!file) {
            dataBytes.clear();
            errorMessage = "Could not read complete input file.";
            return false;
        }
    }
#endif

    path = filePath;
    loadedPlatform = platform;
    return parse(platform, errorMessage);
}

bool LeedsTextureArchive::loadFromMemory(const std::vector<uint8_t>& bytes, LeedsPlatform platform, std::string& errorMessage, const std::wstring& virtualPath) {
    dataBytes.clear();
    entries.clear();
    path.clear();

    if (uint64_t(bytes.size()) > LeedsMaxTextureFileBytes) {
        errorMessage = "Texture data is too large to load safely.";
        return false;
    }

    dataBytes = bytes;
    path = virtualPath;
    loadedPlatform = platform;
    return parse(platform, errorMessage);
}

static bool ps2StoriesXetLogicalEnd(const std::vector<uint8_t>& bytes, uint32_t& logicalEnd) {
    logicalEnd = 0u;
    if (bytes.size() < 0x50u ||
        bytes[0] != 'x' || bytes[1] != 'e' || bytes[2] != 't' ||
        readU32(bytes, 0x20u) != 0x00008606u) {
        return false;
    }

    const uint32_t relocationOffset = readU32(bytes, 0x0Cu);
    const uint32_t relocationCount = readU32(bytes, 0x14u);
    const uint64_t end64 = uint64_t(relocationOffset) + uint64_t(relocationCount) * 4ull;
    if (relocationOffset < 0x50u || end64 > uint64_t(bytes.size()) || end64 > uint64_t(UINT32_MAX)) {
        return false;
    }
    logicalEnd = uint32_t(end64);
    return true;
}

static void padPs2StoriesXetToSectorBoundary(std::vector<uint8_t>& bytes) {
    uint32_t logicalEnd = 0u;
    if (!ps2StoriesXetLogicalEnd(bytes, logicalEnd)) return;

    // Retail PS2 Stories XTX keeps the logical Leeds collection size at the
    // end of the relocation table, but the standalone file itself is padded
    // to a full 2048-byte CD/DVD sector.  Example retail plr.xtx:
    //   logical end = 0xB338, physical EOF = 0xB800.
    writeU32(bytes, 0x08u, logicalEnd);

    if (bytes.size() < logicalEnd) bytes.resize(logicalEnd, 0u);
    if (bytes.size() > logicalEnd) bytes.resize(logicalEnd);

    constexpr size_t kPs2SectorSize = 2048u;
    const size_t paddedSize =
        (size_t(logicalEnd) + (kPs2SectorSize - 1u)) & ~(kPs2SectorSize - 1u);
    bytes.resize(paddedSize, 0u);
}

bool LeedsTextureArchive::saveToFile(const std::wstring& filePath, std::string& errorMessage) const {
    if (filePath.empty()) { errorMessage = "Output path is empty."; return false; }

    // A parseable PS2 Stories XTX is not necessarily a runtime-safe XTX.
    // Old Storyland builds emitted several editor/legacy serializations:
    //   * no 0x50 retail runtime preface
    //   * editor/high-bit texture flags
    //   * stale/zero transfer-layout words
    //   * non-canonical object/relocation layout
    //
    // Export and Export As therefore serialize from the parsed semantic archive,
    // never by copying the original container bytes. Raster/palette payloads are
    // preserved byte-for-byte; only the Leeds runtime container is rebuilt.
    const bool ps2Stories =
        dataBytes.size() >= 0x24u &&
        dataBytes[0] == 'x' && dataBytes[1] == 'e' && dataBytes[2] == 't' &&
        readU32(dataBytes, 0x20u) == 0x00008606u &&
        (entries.empty() || std::all_of(
            entries.begin(), entries.end(),
            [](const LeedsTextureEntry& e) { return e.kind == TextureKind::Ps2; }));

    if (ps2Stories && !entries.empty()) {
        // If the archive already has the retail runtime skeleton, preserve that
        // exact in-memory container. This is why editing a retail XTX and using
        // Export has historically been the most reliable path: the good skeleton
        // survives and only the requested material data changes.
        std::string validationReport;
        std::string validationError;
        if (validateStructure(validationReport, validationError)) {
            return storylandWriteFilesTransaction(
                {{std::filesystem::path(filePath), &dataBytes}}, errorMessage);
        }

        // Legacy/bad XTX: rebuild exactly one canonical retail runtime skeleton
        // while preserving the raw raster/palette blocks.
        LeedsTextureArchive canonical = *this;
        std::string normalizationReport;
        std::string normalizationError;
        if (!canonical.normalizePs2RuntimeLayout(normalizationReport, normalizationError)) {
            errorMessage =
                "Could not rebuild PS2 Stories XTX into the retail runtime skeleton. "
                "Original validation failure: " + validationError +
                "\nRebuild failure: " + normalizationError;
            return false;
        }

        const std::vector<uint8_t>& serialized = canonical.rawBytes();
        std::string canonicalReport;
        std::string canonicalError;
        if (!canonical.validateStructure(canonicalReport, canonicalError)) {
            errorMessage = "Rebuilt PS2 Stories XTX failed runtime validation: " + canonicalError;
            return false;
        }
        return storylandWriteFilesTransaction(
            {{std::filesystem::path(filePath), &serialized}}, errorMessage);
    }

    // Empty/new PS2 dictionaries are already initialized with the runtime header.
    uint32_t logicalEnd = 0u;
    if (ps2StoriesXetLogicalEnd(dataBytes, logicalEnd)) {
        std::vector<uint8_t> serialized(dataBytes.begin(), dataBytes.begin() + logicalEnd);
        writeU32(serialized, 0x08u, logicalEnd);
        constexpr size_t kPs2SectorSize = 2048u;
        const size_t paddedSize =
            (size_t(logicalEnd) + (kPs2SectorSize - 1u)) & ~(kPs2SectorSize - 1u);
        serialized.resize(paddedSize, 0u);
        return storylandWriteFilesTransaction({{std::filesystem::path(filePath), &serialized}}, errorMessage);
    }

    return storylandWriteFilesTransaction({{std::filesystem::path(filePath), &dataBytes}}, errorMessage);
}

bool LeedsTextureArchive::parse(LeedsPlatform platform, std::string& errorMessage) {
    entries.clear();

    if (dataBytes.size() >= 12u && readU32(dataBytes, 0u) == 0x16u) {
        const uint32_t rwVersion = readU32(dataBytes, 8u);
        if (rwVersion == 0x1003FFFFu) {
            return parseLcsBetaRwTxd(dataBytes, entries, errorMessage);
        }
        if (rwVersion == 0x00000310u) {
            return parseMobileLcsRwTxd(dataBytes, entries, errorMessage);
        }
        if (rwVersion == 0x1803FFFFu) {
            return parseGtaSaRwTxd(dataBytes, entries, errorMessage);
        }
    }

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
        return parseTcdtCollection(dataBytes, entries, errorMessage);
    }

    uint32_t collectionSize = readU32(dataBytes, 0x08);
    uint32_t global1 = readU32(dataBytes, 0x0C);
    uint32_t global2 = readU32(dataBytes, 0x10);
    // Retail Leeds intrusive-list semantics:
    //   collection +0x28 = tail link (last node + 8)
    //   collection +0x2C = head link (first node + 8)
    //   node +0x08       = previous link
    //   node +0x0C       = next link
    //
    // Older Storyland accidentally interpreted these backwards. That still
    // allowed retail XTX files to be *read* (in reverse order), but anything
    // rebuilt from that interpretation had a non-retail runtime list skeleton.
    uint32_t tailSlot = readU32(dataBytes, 0x28);
    uint32_t headSlot = readU32(dataBytes, 0x2C);
    uint32_t base = slotBaseFromSlotPtr(headSlot);
    std::set<uint32_t> visited;
    uint32_t lastBase = slotBaseFromSlotPtr(tailSlot);

    // A freshly-created empty Leeds texture dictionary uses the list sentinel
    // at 0x28 as both head and tail.
    if (headSlot == 0x28u && tailSlot == 0x28u) {
        errorMessage.clear();
        return true;
    }

    for (int guard = 0; guard < 10000; ++guard) {
        if (base < 0x30 || size_t(base) + 0x50 > dataBytes.size()) break;
        if (visited.count(base)) break;
        visited.insert(base);

        uint32_t texOff = readU32(dataBytes, base + 0x00);
        uint32_t nextSlot = readU32(dataBytes, base + 0x0C);
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
    addBoundary(headSlot);
    addBoundary(tailSlot);
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


static bool decodeGtaSaPcTexture(
    const std::vector<uint8_t>& data,
    const LeedsTextureEntry& entry,
    RgbaImage& image,
    std::string& errorMessage
) {
    const uint32_t d3dFormat = entry.flags;
    const uint32_t rasterFormat = entry.reserved0;
    const uint8_t depth = uint8_t(entry.reserved1 & 0xFFu);

    if (entry.width <= 0 || entry.height <= 0) {
        errorMessage = "GTA SA texture has invalid dimensions.";
        return false;
    }

    // D3DFORMAT stores FOURCC directly for DXT textures.
    if (d3dFormat == LeedsDxt1 ||
        d3dFormat == LeedsDxt3 ||
        d3dFormat == LeedsDxt5) {
        LeedsTextureEntry dds = entry;
        dds.kind = TextureKind::Dds;
        dds.flags = d3dFormat;
        dds.reserved0 = 0x4u; // DDS-style FOURCC marker for shared decoder.
        dds.reserved1 = 0u;
        return decodeDdsTexture(data, dds, image, errorMessage);
    }

    const size_t pixelCount =
        size_t(entry.width) * size_t(entry.height);
    image.width = entry.width;
    image.height = entry.height;
    image.rgba.assign(pixelCount * 4u, 255u);

    const uint32_t baseFormat = rasterFormat & 0x0F00u;
    const bool pal8 = (rasterFormat & 0x2000u) != 0u;

    if (pal8) {
        const size_t paletteStart =
            size_t(entry.textureHeaderOffset) + 88u;
        if (!checkedTextureRange(
                paletteStart,
                256u * 4u,
                data.size()) ||
            !checkedTextureRange(
                entry.rasterOffset,
                pixelCount,
                data.size())) {
            errorMessage = "GTA SA paletted texture data is truncated.";
            return false;
        }

        for (size_t i = 0u; i < pixelCount; ++i) {
            const uint8_t index = data[entry.rasterOffset + i];
            const size_t p = paletteStart + size_t(index) * 4u;
            const size_t d = i * 4u;
            image.rgba[d + 0u] = data[p + 2u];
            image.rgba[d + 1u] = data[p + 1u];
            image.rgba[d + 2u] = data[p + 0u];
            image.rgba[d + 3u] = data[p + 3u];
        }
        return finishDecodedTexture(image);
    }

    if (depth == 32u &&
        (baseFormat == 0x0500u ||
         baseFormat == 0x0600u ||
         d3dFormat == 21u || // D3DFMT_A8R8G8B8
         d3dFormat == 22u)) { // D3DFMT_X8R8G8B8
        const size_t need = pixelCount * 4u;
        if (!checkedTextureRange(
                entry.rasterOffset,
                need,
                data.size())) {
            errorMessage = "GTA SA 32-bit texture data is truncated.";
            return false;
        }

        const uint8_t* src = data.data() + entry.rasterOffset;
        for (size_t i = 0u; i < pixelCount; ++i) {
            const size_t s = i * 4u;
            const size_t d = i * 4u;
            image.rgba[d + 0u] = src[s + 2u];
            image.rgba[d + 1u] = src[s + 1u];
            image.rgba[d + 2u] = src[s + 0u];
            image.rgba[d + 3u] =
                (d3dFormat == 22u || baseFormat == 0x0600u)
                    ? 255u
                    : src[s + 3u];
        }
        return finishDecodedTexture(image);
    }

    errorMessage =
        "This GTA SA PC texture format is not supported yet.";
    return false;
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

    if (entry.kind == TextureKind::Dds) {
        return decodeDdsTexture(dataBytes, entry, image, errorMessage);
    }

    if (entry.kind == TextureKind::RwPc) {
        return decodeGtaSaPcTexture(dataBytes, entry, image, errorMessage);
    }

    if (entry.width <= 0 || entry.height <= 0 ||
        uint64_t(entry.width) * uint64_t(entry.height) > LeedsMaxDecodedTextureBytes / 4ull) {
        errorMessage = "Texture dimensions are too large to decode safely.";
        return false;
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

    if (isPspTextureKind(entry.kind)) {
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

    if (entry.kind == TextureKind::Dds) {
        errorMessage = "TCDT DDS texture replacement is read-only in this build.";
        return false;
    }

    if (targetBpp == 0) targetBpp = entry.bpp;
    if (targetBpp != 4 && targetBpp != 8 && targetBpp != 16 && targetBpp != 32) {
        errorMessage = "Target BPP must be 4, 8, 16, or 32. Use 0 to keep the original BPP.";
        return false;
    }
    if (entry.kind == TextureKind::CtwTex) {
        errorMessage = "Standalone CTW .tex replacement is not enabled yet.";
        return false;
    }
    if (entry.kind == TextureKind::RwPsp) {
        if (!isLcsBetaRwTxdBytes(dataBytes)) {
            errorMessage = "This RenderWare TXD variant is read-only. Editable TXD authoring currently targets the LCS beta 0x1003FFFF layout.";
            return false;
        }
        if (targetBpp != 4u && targetBpp != 8u) {
            errorMessage = "Editable LCS beta TXD textures currently support 4bpp or 8bpp.";
            return false;
        }

        std::vector<std::vector<uint8_t>> nativeChunks;
        if (!collectLcsBetaTxdNativeChunks(
                dataBytes,
                nativeChunks,
                errorMessage)) {
            return false;
        }
        if (nativeChunks.size() != entries.size() ||
            textureIndex >= nativeChunks.size()) {
            errorMessage = "LCS beta TXD native-texture list does not match the parsed texture table.";
            return false;
        }

        std::vector<uint8_t> replacementNative;
        if (!buildLcsBetaRwNativeTextureChunk(
                entry.name,
                image,
                targetBpp,
                replacementNative,
                errorMessage)) {
            return false;
        }
        nativeChunks[textureIndex] = std::move(replacementNative);

        std::vector<uint8_t> rebuilt;
        if (!rebuildLcsBetaTxdFromNativeChunks(
                dataBytes,
                nativeChunks,
                rebuilt,
                errorMessage)) {
            return false;
        }
        dataBytes = std::move(rebuilt);
        loadedPlatform = LeedsPlatform::Auto;
        return parse(loadedPlatform, errorMessage);
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

    // Keep the logical end at the relocation table, then regenerate exactly the
    // retail PS2 2048-byte sector padding.  This removes stale short alignment
    // tails while preserving the sector-padded standalone XTX form.
    padPs2StoriesXetToSectorBoundary(dataBytes);

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


struct StorylandPs2TextureBuildItem {
    std::string name;
    RgbaImage image;
    uint8_t bpp = 8;
};

static void alignVector16(std::vector<uint8_t>& bytes) {
    while ((bytes.size() & 15u) != 0u) bytes.push_back(0u);
}

static uint32_t packPs2RuntimeFlags(
    uint8_t widthPow2,
    uint8_t heightPow2,
    uint8_t bpp,
    uint8_t mipCount,
    uint8_t swizzleMask,
    uint8_t unknownBits = 0u
) {
    return
        (uint32_t(widthPow2) & 0x3Fu) |
        ((uint32_t(heightPow2) & 0x3Fu) << 6u) |
        ((uint32_t(bpp) & 0x3Fu) << 12u) |
        ((uint32_t(unknownBits) & 0x03u) << 18u) |
        ((uint32_t(mipCount) & 0x0Fu) << 20u) |
        ((uint32_t(swizzleMask) & 0xFFu) << 24u);
}

static bool decodePs2RuntimeFlags(
    uint32_t flags,
    uint8_t& widthPow2,
    uint8_t& heightPow2,
    uint8_t& bpp,
    uint8_t& mipCount,
    uint8_t& swizzleMask,
    uint8_t& unknownBits
) {
    widthPow2 = uint8_t(flags & 0x3Fu);
    heightPow2 = uint8_t((flags >> 6u) & 0x3Fu);
    bpp = uint8_t((flags >> 12u) & 0x3Fu);
    unknownBits = uint8_t((flags >> 18u) & 0x03u);
    mipCount = uint8_t((flags >> 20u) & 0x0Fu);
    swizzleMask = uint8_t((flags >> 24u) & 0xFFu);
    const int width = safePow2(widthPow2);
    const int height = safePow2(heightPow2);
    return dimensionsReasonable(width, height) && bppReasonablePs2(bpp);
}

static uint32_t defaultPs2Reserved1(int width, uint8_t bpp) {
    if (bpp == 8u) return 0x00250000u | uint32_t(width & 0xFFFF);
    if (bpp == 4u) return 0x00450000u | uint32_t((width / 2) & 0xFFFF);
    return 0u;
}

static void initializePs2RuntimeXetHeader(std::vector<uint8_t>& bytes) {
    bytes.assign(0x50u, 0u);
    bytes[0] = 'x';
    bytes[1] = 'e';
    bytes[2] = 't';
    bytes[3] = 0;
    writeU32(bytes, 0x20u, 0x00008606u);
    writeU32(bytes, 0x24u, 0u);
    writeU32(bytes, 0x30u, 0x00000001u);
    writeU32(bytes, 0x34u, 0x0012FD70u);
    writeU32(bytes, 0x38u, 0x000003B5u);
    writeU32(bytes, 0x3Cu, 0x0012FDB8u);
}

static void normalizePs2SerializedHeaderForRuntime(std::vector<uint8_t>& headerBytes) {
    if (headerBytes.size() != 16u) return;

    uint32_t flags = readU32(headerBytes, 0x0Cu);
    uint8_t widthPow2 = 0;
    uint8_t heightPow2 = 0;
    uint8_t bpp = 0;
    uint8_t mipCount = 0;
    uint8_t swizzleMask = 0;
    uint8_t unknownBits = 0;

    if (decodePs2RuntimeFlags(flags, widthPow2, heightPow2, bpp, mipCount, swizzleMask, unknownBits)) {
        return;
    }

    widthPow2 = uint8_t((flags >> 26u) & 0x3Fu);
    heightPow2 = uint8_t((flags >> 20u) & 0x3Fu);
    bpp = uint8_t((flags >> 14u) & 0x3Fu);
    unknownBits = uint8_t((flags >> 12u) & 0x03u);
    mipCount = uint8_t((flags >> 8u) & 0x0Fu);
    swizzleMask = uint8_t(flags & 0xFFu);

    const int width = safePow2(widthPow2);
    const int height = safePow2(heightPow2);
    if (!dimensionsReasonable(width, height) || !bppReasonablePs2(bpp)) return;

    writeU32(
        headerBytes,
        0x0Cu,
        packPs2RuntimeFlags(widthPow2, heightPow2, bpp, mipCount, swizzleMask, unknownBits));
}

static bool encodeCanonicalPs2Texture(
    const StorylandPs2TextureBuildItem& item,
    std::vector<uint8_t>& rasterOut,
    std::vector<uint8_t>& paletteOut,
    std::string& errorMessage
) {
    rasterOut.clear();
    paletteOut.clear();
    if (!isPowerOfTwoInt(item.image.width) || !isPowerOfTwoInt(item.image.height)) {
        errorMessage = "Texture dimensions must be powers of two between 4 and 4096.";
        return false;
    }
    if (item.image.rgba.size() != size_t(item.image.width) * size_t(item.image.height) * 4u) {
        errorMessage = "Texture RGBA buffer size is invalid.";
        return false;
    }
    if (item.bpp != 4u && item.bpp != 8u && item.bpp != 16u && item.bpp != 32u) {
        errorMessage = "PS2 texture BPP must be 4, 8, 16, or 32.";
        return false;
    }

    const size_t pixelCount = size_t(item.image.width) * size_t(item.image.height);
    if (item.bpp == 32u) {
        rasterOut = item.image.rgba;
        for (size_t i = 0; i < pixelCount; ++i) {
            rasterOut[i * 4u + 3u] = uint8_t(std::min(128, int(rasterOut[i * 4u + 3u]) * 128 / 255));
        }
        return true;
    }
    if (item.bpp == 16u) {
        rasterOut.assign(pixelCount * 2u, 0u);
        for (size_t i = 0; i < pixelCount; ++i) {
            const uint8_t r = item.image.rgba[i * 4u + 0u];
            const uint8_t g = item.image.rgba[i * 4u + 1u];
            const uint8_t b = item.image.rgba[i * 4u + 2u];
            const uint8_t a = item.image.rgba[i * 4u + 3u];
            const uint16_t v = uint16_t((r * 31 / 255) | ((g * 31 / 255) << 5) | ((b * 31 / 255) << 10) | (a >= 128 ? 0x8000 : 0));
            rasterOut[i * 2u + 0u] = uint8_t(v & 0xFFu);
            rasterOut[i * 2u + 1u] = uint8_t(v >> 8u);
        }
        return true;
    }

    const int paletteEntries = item.bpp == 4u ? 16 : 256;
    paletteOut = buildPaletteFromReplacementImage(item.image, paletteEntries, true);
    const std::vector<uint8_t> matchingPalette = paletteForReplacementMatching(paletteOut, true);
    std::vector<uint8_t> indices(pixelCount, 0u);
    for (size_t i = 0; i < pixelCount; ++i) {
        indices[i] = uint8_t(nearestPaletteIndex(
            matchingPalette,
            paletteEntries,
            item.image.rgba[i * 4u + 0u], item.image.rgba[i * 4u + 1u],
            item.image.rgba[i * 4u + 2u], item.image.rgba[i * 4u + 3u]));
    }

    // New Storyland dictionaries deliberately use linear PS2 rasters. The Rsl
    // header swizzle mask is zero, so the runtime must not apply the PS2 swizzle
    // path. This avoids pretending the 8-bit page-address function is also a
    // correct 4-bit nibble swizzler. 8-bit CLUT lane order is still converted
    // because that is independent of raster swizzling.
    if (item.bpp == 8u) {
        indices = inverseConvertClutPs2(indices);
        rasterOut = std::move(indices);
    } else {
        rasterOut = packNibblesLoFirst(indices);
    }
    return true;
}

static bool buildCanonicalPs2Xet(
    const std::vector<StorylandPs2TextureBuildItem>& items,
    std::vector<uint8_t>& bytesOut,
    std::string& errorMessage
) {
    if (items.size() > 4096u) {
        errorMessage = "Too many textures for a Leeds texture dictionary.";
        return false;
    }

    initializePs2RuntimeXetHeader(bytesOut);

    if (items.empty()) {
        writeU32(bytesOut, 0x28u, 0x28u);
        writeU32(bytesOut, 0x2Cu, 0x28u);
        writeU32(bytesOut, 0x08u, uint32_t(bytesOut.size()));
        writeU32(bytesOut, 0x0Cu, uint32_t(bytesOut.size()));
        writeU32(bytesOut, 0x10u, uint32_t(bytesOut.size()));
        writeU32(bytesOut, 0x14u, 0u);
        writeU32(bytesOut, 0x18u, 0u);
        return true;
    }

    struct BuiltMeta {
        uint32_t raster = 0;
        uint32_t header = 0;
        uint32_t base = 0;
        uint8_t bpp = 0;
        int width = 0;
        int height = 0;
        std::string name;
    };
    std::vector<BuiltMeta> meta(items.size());

    for (size_t i = 0; i < items.size(); ++i) {
        std::vector<uint8_t> raster;
        std::vector<uint8_t> palette;
        if (!encodeCanonicalPs2Texture(items[i], raster, palette, errorMessage)) return false;
        alignVector16(bytesOut);
        meta[i].raster = uint32_t(bytesOut.size());
        meta[i].bpp = items[i].bpp;
        meta[i].width = items[i].image.width;
        meta[i].height = items[i].image.height;
        meta[i].name = items[i].name;
        bytesOut.insert(bytesOut.end(), raster.begin(), raster.end());
        bytesOut.insert(bytesOut.end(), palette.begin(), palette.end());
    }

    alignVector16(bytesOut);
    for (size_t i = 0; i < items.size(); ++i) {
        meta[i].base = uint32_t(bytesOut.size());
        bytesOut.resize(bytesOut.size() + 0x60u, 0u);
    }

    alignVector16(bytesOut);
    for (size_t i = 0; i < items.size(); ++i) {
        meta[i].header = uint32_t(bytesOut.size());
        bytesOut.resize(bytesOut.size() + 16u, 0u);
        writeU32(bytesOut, meta[i].header + 0x00u, 0u);
        writeU32(bytesOut, meta[i].header + 0x04u, defaultPs2Reserved1(meta[i].width, meta[i].bpp));
        writeU32(bytesOut, meta[i].header + 0x08u, meta[i].raster);
        const uint8_t widthPow2 = log2ExactInt(meta[i].width);
        const uint8_t heightPow2 = log2ExactInt(meta[i].height);
        writeU32(
            bytesOut,
            meta[i].header + 0x0Cu,
            packPs2RuntimeFlags(widthPow2, heightPow2, meta[i].bpp, 1u, 0u, 0u));
    }

    // Retail Leeds intrusive-list skeleton.
    writeU32(bytesOut, 0x28u, meta.back().base + 8u);   // tail
    writeU32(bytesOut, 0x2Cu, meta.front().base + 8u);  // head
    for (size_t i = 0; i < items.size(); ++i) {
        const uint32_t base = meta[i].base;
        const uint32_t previousSlot = i == 0u ? 0x28u : meta[i - 1u].base + 8u;
        const uint32_t nextSlot = i + 1u == items.size() ? 0x28u : meta[i + 1u].base + 8u;
        writeU32(bytesOut, base + 0x00u, meta[i].header);
        writeU32(bytesOut, base + 0x04u, 0x20u);
        writeU32(bytesOut, base + 0x08u, previousSlot);
        writeU32(bytesOut, base + 0x0Cu, nextSlot);
        const std::string safeName = meta[i].name.empty() ? ("texture_" + std::to_string(i)) : meta[i].name;
        const size_t copyCount = std::min<size_t>(63u, safeName.size());
        std::copy(safeName.begin(), safeName.begin() + copyCount, bytesOut.begin() + base + 0x10u);
    }

    alignVector16(bytesOut);
    const uint32_t relocationOffset = uint32_t(bytesOut.size());
    std::vector<uint32_t> relocations = {0x28u, 0x2Cu};
    for (const BuiltMeta& item : meta) {
        relocations.push_back(item.base + 0x00u);
        relocations.push_back(item.base + 0x04u);
        relocations.push_back(item.base + 0x08u);
        relocations.push_back(item.base + 0x0Cu);
    }
    for (const BuiltMeta& item : meta) relocations.push_back(item.header + 0x08u);
    for (uint32_t relocation : relocations) {
        const size_t oldSize = bytesOut.size();
        bytesOut.resize(oldSize + 4u);
        writeU32(bytesOut, oldSize, relocation);
    }

    writeU32(bytesOut, 0x08u, uint32_t(bytesOut.size()));
    writeU32(bytesOut, 0x0Cu, relocationOffset);
    writeU32(bytesOut, 0x10u, relocationOffset);
    writeU32(bytesOut, 0x14u, uint32_t(relocations.size()));
    writeU32(bytesOut, 0x18u, 0u);

    // Header +0x08 remains the logical end of the relocation table. Retail
    // standalone PS2 XTX files are then physically padded to a 2048-byte sector.
    const uint32_t logicalEnd = uint32_t(bytesOut.size());
    writeU32(bytesOut, 0x08u, logicalEnd);
    constexpr size_t kPs2SectorSize = 2048u;
    const size_t paddedSize =
        (size_t(logicalEnd) + (kPs2SectorSize - 1u)) & ~(kPs2SectorSize - 1u);
    bytesOut.resize(paddedSize, 0u);
    return true;
}

struct StorylandPs2RawBuildItem {
    std::string name;
    std::vector<uint8_t> headerBytes;
    std::vector<uint8_t> rasterBlock;
};

static bool collectRawPs2BuildItems(
    const std::vector<uint8_t>& source,
    const std::vector<LeedsTextureEntry>& sourceEntries,
    std::vector<StorylandPs2RawBuildItem>& itemsOut,
    std::string& errorMessage
) {
    itemsOut.clear();
    itemsOut.reserve(sourceEntries.size());
    for (const LeedsTextureEntry& e : sourceEntries) {
        if (e.kind != TextureKind::Ps2) {
            errorMessage = "Raw-preserving texture editing is currently available for PS2 Leeds XTX/CHK entries only.";
            return false;
        }
        if (e.textureHeaderOffset + 16u > source.size() || e.rasterOffset >= source.size() || e.blockSize == 0u ||
            uint64_t(e.rasterOffset) + uint64_t(e.blockSize) > uint64_t(source.size())) {
            errorMessage = "A PS2 texture has an invalid header or raster block range.";
            return false;
        }
        StorylandPs2RawBuildItem item;
        item.name = e.name;
        item.headerBytes.assign(source.begin() + e.textureHeaderOffset, source.begin() + e.textureHeaderOffset + 16u);
        item.rasterBlock.assign(source.begin() + e.rasterOffset, source.begin() + e.rasterOffset + e.blockSize);
        itemsOut.push_back(std::move(item));
    }
    return true;
}

static bool buildPs2XetPreservingRawTextureData(
    const std::vector<StorylandPs2RawBuildItem>& items,
    std::vector<uint8_t>& bytesOut,
    std::string& errorMessage
) {
    if (items.size() > 4096u) {
        errorMessage = "Too many textures for a Leeds texture dictionary.";
        return false;
    }

    initializePs2RuntimeXetHeader(bytesOut);

    if (items.empty()) {
        writeU32(bytesOut, 0x28u, 0x28u);
        writeU32(bytesOut, 0x2Cu, 0x28u);
        writeU32(bytesOut, 0x08u, uint32_t(bytesOut.size()));
        writeU32(bytesOut, 0x0Cu, uint32_t(bytesOut.size()));
        writeU32(bytesOut, 0x10u, uint32_t(bytesOut.size()));
        writeU32(bytesOut, 0x14u, 0u);
        writeU32(bytesOut, 0x18u, 0u);
        return true;
    }

    struct Meta {
        uint32_t raster = 0;
        uint32_t header = 0;
        uint32_t base = 0;
    };
    std::vector<Meta> meta(items.size());

    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].headerBytes.size() != 16u || items[i].rasterBlock.empty()) {
            errorMessage = "Raw PS2 texture item is incomplete.";
            return false;
        }
        alignVector16(bytesOut);
        meta[i].raster = uint32_t(bytesOut.size());
        bytesOut.insert(bytesOut.end(), items[i].rasterBlock.begin(), items[i].rasterBlock.end());
    }

    alignVector16(bytesOut);
    for (size_t i = 0; i < items.size(); ++i) {
        meta[i].base = uint32_t(bytesOut.size());
        bytesOut.resize(bytesOut.size() + 0x60u, 0u);
    }

    alignVector16(bytesOut);
    for (size_t i = 0; i < items.size(); ++i) {
        meta[i].header = uint32_t(bytesOut.size());
        std::vector<uint8_t> runtimeHeader = items[i].headerBytes;
        normalizePs2SerializedHeaderForRuntime(runtimeHeader);
        bytesOut.insert(bytesOut.end(), runtimeHeader.begin(), runtimeHeader.end());
        writeU32(bytesOut, meta[i].header + 0x08u, meta[i].raster);
    }

    // Retail Leeds intrusive-list skeleton.
    writeU32(bytesOut, 0x28u, meta.back().base + 8u);   // tail
    writeU32(bytesOut, 0x2Cu, meta.front().base + 8u);  // head
    for (size_t i = 0; i < items.size(); ++i) {
        const uint32_t base = meta[i].base;
        writeU32(bytesOut, base + 0x00u, meta[i].header);
        writeU32(bytesOut, base + 0x04u, 0x20u);
        writeU32(bytesOut, base + 0x08u, i == 0u ? 0x28u : meta[i - 1u].base + 8u);
        writeU32(bytesOut, base + 0x0Cu, i + 1u == items.size() ? 0x28u : meta[i + 1u].base + 8u);
        const std::string safeName = items[i].name.empty() ? ("texture_" + std::to_string(i)) : items[i].name;
        const size_t copyCount = std::min<size_t>(63u, safeName.size());
        std::copy(safeName.begin(), safeName.begin() + copyCount, bytesOut.begin() + base + 0x10u);
    }

    alignVector16(bytesOut);
    const uint32_t relocationOffset = uint32_t(bytesOut.size());
    std::vector<uint32_t> relocations = {0x28u, 0x2Cu};
    for (const Meta& item : meta) {
        relocations.push_back(item.base + 0x00u);
        relocations.push_back(item.base + 0x04u);
        relocations.push_back(item.base + 0x08u);
        relocations.push_back(item.base + 0x0Cu);
    }
    for (const Meta& item : meta) relocations.push_back(item.header + 0x08u);
    for (uint32_t relocation : relocations) {
        const size_t oldSize = bytesOut.size();
        bytesOut.resize(oldSize + 4u);
        writeU32(bytesOut, oldSize, relocation);
    }

    writeU32(bytesOut, 0x08u, uint32_t(bytesOut.size()));
    writeU32(bytesOut, 0x0Cu, relocationOffset);
    writeU32(bytesOut, 0x10u, relocationOffset);
    writeU32(bytesOut, 0x14u, uint32_t(relocations.size()));
    writeU32(bytesOut, 0x18u, 0u);

    // Header +0x08 remains the logical end of the relocation table. Retail
    // standalone PS2 XTX files are then physically padded to a 2048-byte sector.
    const uint32_t logicalEnd = uint32_t(bytesOut.size());
    writeU32(bytesOut, 0x08u, logicalEnd);
    constexpr size_t kPs2SectorSize = 2048u;
    const size_t paddedSize =
        (size_t(logicalEnd) + (kPs2SectorSize - 1u)) & ~(kPs2SectorSize - 1u);
    bytesOut.resize(paddedSize, 0u);
    return true;
}

bool LeedsTextureArchive::normalizePs2RuntimeLayout(std::string& report, std::string& errorMessage) {
    report.clear();
    if (entries.empty()) {
        errorMessage = "Texture archive has no PS2 textures to normalize.";
        return false;
    }
    for (const LeedsTextureEntry& entry : entries) {
        if (entry.kind != TextureKind::Ps2) {
            errorMessage = "Runtime XTX normalization is only available for PS2 Leeds texture archives.";
            return false;
        }
    }

    std::vector<StorylandPs2RawBuildItem> items;
    if (!collectRawPs2BuildItems(dataBytes, entries, items, errorMessage)) return false;
    if (items.size() != entries.size()) {
        errorMessage = "PS2 XTX normalization lost texture-entry correspondence.";
        return false;
    }

    // IMPORTANT: never decide whether a header is already runtime-safe by trying
    // to decode the raw flag DWORD in isolation. Some old editor/high-bit flag
    // values can accidentally look plausible under the runtime low/middle-bit
    // decoder. The parsed LeedsTextureEntry is the semantic source of truth.
    //
    // Re-emit every serialized header from those semantics. This is the core
    // fix for the class of 'Storyland opens it, PS2 crashes on it' XTX files.
    for (size_t i = 0; i < items.size(); ++i) {
        StorylandPs2RawBuildItem& item = items[i];
        const LeedsTextureEntry& entry = entries[i];
        if (item.headerBytes.size() != 16u) {
            errorMessage = "PS2 XTX contains a truncated texture header.";
            return false;
        }

        writeU32(item.headerBytes, 0x00u, entry.reserved0);

        uint32_t transfer = entry.reserved1;
        const bool generic8 = entry.bpp == 8u && (transfer & 0xFFFF0000u) == 0x00250000u;
        const bool generic4 = entry.bpp == 4u && (transfer & 0xFFFF0000u) == 0x00450000u;
        if (transfer == 0u || generic8 || generic4) {
            transfer = defaultPs2Reserved1(entry.width, entry.bpp);
        }
        writeU32(item.headerBytes, 0x04u, transfer);

        // +0x08 is rebuilt later from the actual raster position.
        writeU32(
            item.headerBytes,
            0x0Cu,
            packPs2RuntimeFlags(
                entry.widthPow2,
                entry.heightPow2,
                entry.bpp,
                entry.mipCount,
                entry.swizzleMask,
                0u));
    }

    std::vector<uint8_t> rebuilt;
    if (!buildPs2XetPreservingRawTextureData(items, rebuilt, errorMessage)) return false;

    LeedsTextureArchive trial;
    if (!trial.loadFromMemory(rebuilt, LeedsPlatform::Ps2, errorMessage, path)) return false;


    std::string validationReport;
    if (!trial.validateStructure(validationReport, errorMessage)) return false;

    dataBytes = std::move(rebuilt);
    loadedPlatform = LeedsPlatform::Ps2;
    if (!parse(loadedPlatform, errorMessage)) return false;

    std::ostringstream stream;
    stream << "Normalized PS2 Stories XTX runtime layout.\n";
    stream << "Runtime preface: 0x50 bytes.\n";
    stream << "Serialized texture flags: Leeds PS2 runtime bit layout.\n";
    stream << "Object ordering: raster data, 0x60-byte linked texture nodes, texture headers, relocation table.\n";
    stream << "Runtime list: +0x2C head, +0x28 tail, node +0x08 previous, node +0x0C next.\n";
    stream << validationReport;
    report = stream.str();
    errorMessage.clear();
    return true;
}


bool LeedsTextureArchive::createEmptyPs2(const std::wstring& virtualPath, std::string& errorMessage) {
    std::vector<uint8_t> fresh;
    if (!buildCanonicalPs2Xet({}, fresh, errorMessage)) return false;
    dataBytes = std::move(fresh);
    path = virtualPath;
    loadedPlatform = LeedsPlatform::Ps2;
    return parse(loadedPlatform, errorMessage);
}

bool LeedsTextureArchive::createLcsBetaTxd(
    const std::wstring& virtualPath,
    const std::string& firstName,
    const RgbaImage& image,
    uint8_t bpp,
    std::string& errorMessage
) {
    std::vector<uint8_t> firstNative;
    if (!buildLcsBetaRwNativeTextureChunk(
            firstName,
            image,
            bpp,
            firstNative,
            errorMessage)) {
        return false;
    }

    // Minimal May-2005 LCS beta TXD root:
    // Texture Dictionary -> Struct(count/device) -> Native Texture -> Extension.
    std::vector<uint8_t> dictionaryStructPayload(4u, 0u);
    writeU16Le(dictionaryStructPayload, 0u, 1u);
    writeU16Le(dictionaryStructPayload, 2u, 0u);

    std::vector<uint8_t> rootPayload;
    appendRwChunk(
        rootPayload,
        0x01u,
        0x1003FFFFu,
        dictionaryStructPayload);
    rootPayload.insert(
        rootPayload.end(),
        firstNative.begin(),
        firstNative.end());
    appendRwChunk(
        rootPayload,
        0x03u,
        0x1003FFFFu,
        {});

    std::vector<uint8_t> fresh;
    appendRwChunk(
        fresh,
        0x16u,
        0x1003FFFFu,
        rootPayload);

    LeedsTextureArchive trial;
    std::string trialError;
    if (!trial.loadFromMemory(
            fresh,
            LeedsPlatform::Auto,
            trialError,
            virtualPath)) {
        errorMessage = "New LCS beta TXD failed parse verification: " + trialError;
        return false;
    }
    if (trial.textures().size() != 1u ||
        trial.textures()[0].kind != TextureKind::RwPsp ||
        trial.textures()[0].bpp != bpp) {
        errorMessage = "New LCS beta TXD did not round-trip the initial texture slot.";
        return false;
    }

    dataBytes = std::move(fresh);
    path = virtualPath;
    loadedPlatform = LeedsPlatform::Auto;
    return parse(loadedPlatform, errorMessage);
}

bool LeedsTextureArchive::addTexture(const std::string& name, const RgbaImage& image, uint8_t bpp, std::string& errorMessage) {
    if (bpp != 4u && bpp != 8u) {
        errorMessage = "New CHK/XTX/TXD materials currently support 4bpp or 8bpp.";
        return false;
    }
    if (name.empty() || name.size() > 63u || !isPrintableName(name)) {
        errorMessage = "Texture name must be 1-63 printable ASCII characters.";
        return false;
    }
    if (isLcsBetaRwTxdBytes(dataBytes)) {
        if (name.size() > 31u) {
            errorMessage = "LCS beta TXD texture names are limited to 31 printable ASCII characters.";
            return false;
        }
        for (const LeedsTextureEntry& entry : entries) {
            std::string a = entry.name;
            std::string b = name;
            std::transform(a.begin(), a.end(), a.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
            std::transform(b.begin(), b.end(), b.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
            if (a == b) {
                errorMessage = "A texture with that name already exists.";
                return false;
            }
        }

        std::vector<uint8_t> addedNative;
        if (!buildLcsBetaRwNativeTextureChunk(
                name,
                image,
                bpp,
                addedNative,
                errorMessage)) {
            return false;
        }

        std::vector<std::vector<uint8_t>> nativeChunks;
        if (!collectLcsBetaTxdNativeChunks(
                dataBytes,
                nativeChunks,
                errorMessage)) {
            return false;
        }
        nativeChunks.push_back(std::move(addedNative));

        std::vector<uint8_t> rebuilt;
        if (!rebuildLcsBetaTxdFromNativeChunks(
                dataBytes,
                nativeChunks,
                rebuilt,
                errorMessage)) {
            return false;
        }
        dataBytes = std::move(rebuilt);
        loadedPlatform = LeedsPlatform::Auto;
        return parse(loadedPlatform, errorMessage);
    }

    for (const LeedsTextureEntry& entry : entries) {
        std::string a = entry.name, b = name;
        std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (a == b) {
            errorMessage = "A texture with that name already exists.";
            return false;
        }
        if (entry.kind != TextureKind::Ps2) {
            errorMessage = "Adding textures is currently enabled for PS2 Leeds CHK/XTX archives only.";
            return false;
        }
    }

    // Preserve every existing PS2 raster/header byte-for-byte. Adding one
    // texture must never silently requantize or reswizzle the rest of the
    // archive.
    std::vector<StorylandPs2RawBuildItem> rawItems;
    if (!collectRawPs2BuildItems(dataBytes, entries, rawItems, errorMessage)) return false;

    StorylandPs2TextureBuildItem added;
    added.name = name;
    std::string resizeNote;
    if (!normalizePs2TextureImage(image, added.image, resizeNote)) {
        errorMessage = resizeNote;
        return false;
    }
    added.bpp = bpp;
    std::vector<uint8_t> raster, palette;
    if (!encodeCanonicalPs2Texture(added, raster, palette, errorMessage)) return false;

    StorylandPs2RawBuildItem rawAdded;
    rawAdded.name = name;
    rawAdded.rasterBlock = std::move(raster);
    rawAdded.rasterBlock.insert(rawAdded.rasterBlock.end(), palette.begin(), palette.end());
    rawAdded.headerBytes.assign(16u, 0u);
    writeU32(rawAdded.headerBytes, 0x04u, defaultPs2Reserved1(added.image.width, bpp));
    const uint8_t widthPow2 = log2ExactInt(added.image.width);
    const uint8_t heightPow2 = log2ExactInt(added.image.height);
    writeU32(
        rawAdded.headerBytes,
        0x0Cu,
        packPs2RuntimeFlags(widthPow2, heightPow2, bpp, 1u, 0u, 0u));
    rawItems.push_back(std::move(rawAdded));

    // Do not special-case plr.xtx by zeroing +0x04. Retail and the known-good
    // CJ archive both use normal PS2 transfer words here. Existing zero values
    // are repaired below before rebuilding.
    for (StorylandPs2RawBuildItem& item : rawItems) {
        if (item.headerBytes.size() != 16u) continue;
        const uint32_t transfer = readU32(item.headerBytes, 0x04u);
        const uint32_t flags = readU32(item.headerBytes, 0x0Cu);
        const uint8_t widthPow2 = uint8_t(flags & 0x3Fu);
        const uint8_t bpp = uint8_t((flags >> 12u) & 0x3Fu);
        if (transfer == 0u && widthPow2 < 31u && (bpp == 4u || bpp == 8u)) {
            writeU32(item.headerBytes, 0x04u, defaultPs2Reserved1(1 << widthPow2, bpp));
        }
    }

    std::vector<uint8_t> rebuilt;
    if (!buildPs2XetPreservingRawTextureData(rawItems, rebuilt, errorMessage)) return false;
    dataBytes = std::move(rebuilt);
    loadedPlatform = LeedsPlatform::Ps2;
    return parse(loadedPlatform, errorMessage);
}

bool LeedsTextureArchive::swapTextureData(size_t firstTextureIndex, size_t secondTextureIndex, std::string& errorMessage) {
    if (firstTextureIndex >= entries.size() || secondTextureIndex >= entries.size()) {
        errorMessage = "Texture swap index is out of range.";
        return false;
    }
    if (firstTextureIndex == secondTextureIndex) return true;

    if (isLcsBetaRwTxdBytes(dataBytes)) {
        std::vector<std::vector<uint8_t>> nativeChunks;
        if (!collectLcsBetaTxdNativeChunks(
                dataBytes,
                nativeChunks,
                errorMessage)) {
            return false;
        }
        if (nativeChunks.size() != entries.size()) {
            errorMessage = "LCS beta TXD native-texture list does not match the parsed texture table.";
            return false;
        }

        const std::string firstName = entries[firstTextureIndex].name;
        const std::string secondName = entries[secondTextureIndex].name;
        std::swap(
            nativeChunks[firstTextureIndex],
            nativeChunks[secondTextureIndex]);

        if (!patchLcsBetaNativeChunkName(
                nativeChunks[firstTextureIndex],
                firstName,
                errorMessage) ||
            !patchLcsBetaNativeChunkName(
                nativeChunks[secondTextureIndex],
                secondName,
                errorMessage)) {
            return false;
        }

        std::vector<uint8_t> rebuilt;
        if (!rebuildLcsBetaTxdFromNativeChunks(
                dataBytes,
                nativeChunks,
                rebuilt,
                errorMessage)) {
            return false;
        }
        dataBytes = std::move(rebuilt);
        loadedPlatform = LeedsPlatform::Auto;
        return parse(loadedPlatform, errorMessage);
    }

    std::vector<StorylandPs2RawBuildItem> rawItems;
    if (!collectRawPs2BuildItems(dataBytes, entries, rawItems, errorMessage)) return false;

    // Names belong to the archive slots. The encoded texture payload and the
    // corresponding 16-byte PS2 header move together, so dimensions/BPP/
    // swizzle/palette semantics stay internally consistent without a lossy
    // decode->re-encode round trip.
    const std::string firstName = rawItems[firstTextureIndex].name;
    const std::string secondName = rawItems[secondTextureIndex].name;
    std::swap(rawItems[firstTextureIndex].headerBytes, rawItems[secondTextureIndex].headerBytes);
    std::swap(rawItems[firstTextureIndex].rasterBlock, rawItems[secondTextureIndex].rasterBlock);
    rawItems[firstTextureIndex].name = firstName;
    rawItems[secondTextureIndex].name = secondName;

    std::vector<uint8_t> rebuilt;
    if (!buildPs2XetPreservingRawTextureData(rawItems, rebuilt, errorMessage)) return false;

    // Commit only after the complete rebuilt archive parses successfully.
    LeedsTextureArchive trial;
    if (!trial.loadFromMemory(rebuilt, LeedsPlatform::Ps2, errorMessage, path)) return false;
    dataBytes = std::move(rebuilt);
    loadedPlatform = LeedsPlatform::Ps2;
    return parse(loadedPlatform, errorMessage);
}

bool LeedsTextureArchive::duplicateTexture(size_t textureIndex, const std::string& newName, std::string& errorMessage) {
    if (textureIndex >= entries.size()) {
        errorMessage = "Texture duplicate index is out of range.";
        return false;
    }
    if (newName.empty() || newName.size() > 63u || !isPrintableName(newName)) {
        errorMessage = "Texture name must be 1-63 printable ASCII characters.";
        return false;
    }
    for (const LeedsTextureEntry& entry : entries) {
        std::string a = entry.name, b = newName;
        std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (a == b) {
            errorMessage = "A texture with that name already exists.";
            return false;
        }
    }

    if (isLcsBetaRwTxdBytes(dataBytes)) {
        if (newName.size() > 31u) {
            errorMessage = "LCS beta TXD texture names are limited to 31 printable ASCII characters.";
            return false;
        }

        std::vector<std::vector<uint8_t>> nativeChunks;
        if (!collectLcsBetaTxdNativeChunks(
                dataBytes,
                nativeChunks,
                errorMessage)) {
            return false;
        }
        if (nativeChunks.size() != entries.size() ||
            textureIndex >= nativeChunks.size()) {
            errorMessage = "LCS beta TXD native-texture list does not match the parsed texture table.";
            return false;
        }

        std::vector<uint8_t> copy = nativeChunks[textureIndex];
        if (!patchLcsBetaNativeChunkName(
                copy,
                newName,
                errorMessage)) {
            return false;
        }
        nativeChunks.insert(
            nativeChunks.begin() + std::ptrdiff_t(textureIndex + 1u),
            std::move(copy));

        std::vector<uint8_t> rebuilt;
        if (!rebuildLcsBetaTxdFromNativeChunks(
                dataBytes,
                nativeChunks,
                rebuilt,
                errorMessage)) {
            return false;
        }
        dataBytes = std::move(rebuilt);
        loadedPlatform = LeedsPlatform::Auto;
        return parse(loadedPlatform, errorMessage);
    }

    std::vector<StorylandPs2RawBuildItem> rawItems;
    if (!collectRawPs2BuildItems(dataBytes, entries, rawItems, errorMessage)) return false;
    StorylandPs2RawBuildItem copy = rawItems[textureIndex];
    copy.name = newName;
    rawItems.insert(rawItems.begin() + std::ptrdiff_t(textureIndex + 1u), std::move(copy));

    std::vector<uint8_t> rebuilt;
    if (!buildPs2XetPreservingRawTextureData(rawItems, rebuilt, errorMessage)) return false;
    LeedsTextureArchive trial;
    if (!trial.loadFromMemory(rebuilt, LeedsPlatform::Ps2, errorMessage, path)) return false;
    dataBytes = std::move(rebuilt);
    loadedPlatform = LeedsPlatform::Ps2;
    return parse(loadedPlatform, errorMessage);
}

bool LeedsTextureArchive::removeTexture(size_t textureIndex, std::string& errorMessage) {
    if (textureIndex >= entries.size()) {
        errorMessage = "Texture remove index is out of range.";
        return false;
    }

    if (isLcsBetaRwTxdBytes(dataBytes)) {
        std::vector<std::vector<uint8_t>> nativeChunks;
        if (!collectLcsBetaTxdNativeChunks(
                dataBytes,
                nativeChunks,
                errorMessage)) {
            return false;
        }
        if (nativeChunks.size() != entries.size() ||
            textureIndex >= nativeChunks.size()) {
            errorMessage = "LCS beta TXD native-texture list does not match the parsed texture table.";
            return false;
        }
        nativeChunks.erase(
            nativeChunks.begin() + std::ptrdiff_t(textureIndex));

        std::vector<uint8_t> rebuilt;
        if (!rebuildLcsBetaTxdFromNativeChunks(
                dataBytes,
                nativeChunks,
                rebuilt,
                errorMessage)) {
            return false;
        }
        dataBytes = std::move(rebuilt);
        loadedPlatform = LeedsPlatform::Auto;
        return parse(loadedPlatform, errorMessage);
    }

    std::vector<StorylandPs2RawBuildItem> rawItems;
    if (!collectRawPs2BuildItems(dataBytes, entries, rawItems, errorMessage)) return false;
    rawItems.erase(rawItems.begin() + std::ptrdiff_t(textureIndex));

    std::vector<uint8_t> rebuilt;
    if (!buildPs2XetPreservingRawTextureData(rawItems, rebuilt, errorMessage)) return false;
    LeedsTextureArchive trial;
    if (!trial.loadFromMemory(rebuilt, LeedsPlatform::Ps2, errorMessage, path)) return false;
    dataBytes = std::move(rebuilt);
    loadedPlatform = LeedsPlatform::Ps2;
    return parse(loadedPlatform, errorMessage);
}

bool LeedsTextureArchive::validateStructure(std::string& report, std::string& errorMessage) const {
    if (dataBytes.size() >= 12u && readU32(dataBytes, 0u) == 0x16u &&
        rwBuildStampSupported(readU32(dataBytes, 8u))) {
        LeedsTextureArchive trial;
        std::string parseError;
        if (!trial.loadFromMemory(dataBytes, LeedsPlatform::Auto, parseError, path)) {
            errorMessage = parseError;
            return false;
        }

        std::ostringstream txdReport;
        txdReport << "RenderWare TXD validation: PASS\n";
        txdReport << "Build stamp: 0x" << std::hex << std::uppercase << readU32(dataBytes, 8u) << std::dec << "\n";
        txdReport << "Bytes: " << dataBytes.size() << "\n";
        txdReport << "Textures: " << trial.textures().size() << "\n";
        for (size_t i = 0u; i < trial.textures().size(); ++i) {
            const LeedsTextureEntry& entry = trial.textures()[i];
            if (entry.rasterOffset > dataBytes.size() ||
                entry.blockSize > dataBytes.size() - entry.rasterOffset) {
                errorMessage = "RenderWare TXD contains an out-of-range raster block.";
                return false;
            }
            txdReport << "[" << i << "] " << entry.name << " "
                      << entry.width << "x" << entry.height << " "
                      << int(entry.bpp) << "bpp mips=" << int(entry.mipCount)
                      << " raster=0x" << std::hex << entry.rasterOffset
                      << " bytes=0x" << entry.blockSize << std::dec << "\n";
        }
        report = txdReport.str();
        errorMessage.clear();
        return true;
    }

    report.clear();
    if (dataBytes.size() < 0x30u) {
        errorMessage = "Texture archive is smaller than the Leeds container header.";
        return false;
    }
    if (!(dataBytes[0] == 'x' && dataBytes[1] == 'e' && dataBytes[2] == 't')) {
        errorMessage = "Texture archive is not an xet/CHK/XTX Leeds container.";
        return false;
    }

    LeedsTextureArchive trial;
    std::string parseError;
    if (!trial.loadFromMemory(
            dataBytes,
            loadedPlatform == LeedsPlatform::Auto ? LeedsPlatform::Auto : loadedPlatform,
            parseError,
            path)) {
        errorMessage = parseError;
        return false;
    }

    const bool ps2StoriesRuntimeXet =
        dataBytes.size() >= 0x24u &&
        readU32(dataBytes, 0x20u) == 0x00008606u &&
        !trial.textures().empty() &&
        std::all_of(
            trial.textures().begin(),
            trial.textures().end(),
            [](const LeedsTextureEntry& entry) { return entry.kind == TextureKind::Ps2; });

    if (ps2StoriesRuntimeXet) {
        if (dataBytes.size() < 0x50u) {
            errorMessage = "PS2 Stories XTX is missing the retail 0x50-byte runtime preface.";
            return false;
        }
        const bool retailRuntimePreface =
            readU32(dataBytes, 0x30u) == 0x00000001u &&
            readU32(dataBytes, 0x34u) == 0x0012FD70u &&
            readU32(dataBytes, 0x38u) == 0x000003B5u &&
            readU32(dataBytes, 0x3Cu) == 0x0012FDB8u;
        if (!retailRuntimePreface) {
            errorMessage =
                "PS2 Stories XTX is parseable but not canonical runtime serialization: "
                "runtime preface 0x30..0x3C must be 1, 0x0012FD70, 0x000003B5, 0x0012FDB8. "
                "Use Export/Export As to rebuild the runtime container.";
            return false;
        }

        const uint32_t relocationOffset = readU32(dataBytes, 0x0Cu);
        const uint32_t relocationCount = readU32(dataBytes, 0x14u);
        const uint64_t relocationEnd64 =
            uint64_t(relocationOffset) + uint64_t(relocationCount) * 4ull;
        if (relocationEnd64 > uint64_t(dataBytes.size())) {
            errorMessage = "PS2 Stories XTX relocation table extends beyond the physical file.";
            return false;
        }
        const uint32_t logicalSize = readU32(dataBytes, 0x08u);
        if (logicalSize != uint32_t(relocationEnd64)) {
            errorMessage =
                "PS2 Stories XTX logical size is inconsistent. "
                "Header +0x08 must end exactly at the relocation table; "
                "trailing alignment/sector padding must remain outside the logical collection.";
            return false;
        }

        uint32_t firstNode = UINT32_MAX;
        uint32_t firstHeader = UINT32_MAX;
        uint32_t lastRasterEnd = 0u;
        for (const LeedsTextureEntry& entry : trial.textures()) {
            if (entry.textureHeaderOffset + 16u > dataBytes.size()) {
                errorMessage = "PS2 Stories XTX texture header points outside the file.";
                return false;
            }
            if (entry.rasterOffset < 0x50u) {
                errorMessage = "PS2 Stories XTX raster overlaps the runtime preface.";
                return false;
            }
            const uint32_t transfer = readU32(dataBytes, entry.textureHeaderOffset + 0x04u);
            if (entry.bpp == 8u) {
                const uint32_t expected = defaultPs2Reserved1(entry.width, entry.bpp);
                if (transfer == 0u || ((transfer & 0xFFFF0000u) == 0x00250000u && transfer != expected)) {
                    errorMessage =
                        "PS2 Stories XTX has an invalid 8bpp transfer-layout word at texture header +0x04.";
                    return false;
                }
            } else if (entry.bpp == 4u) {
                const uint32_t expected = defaultPs2Reserved1(entry.width, entry.bpp);
                if (transfer == 0u || ((transfer & 0xFFFF0000u) == 0x00450000u && transfer != expected)) {
                    errorMessage =
                        "PS2 Stories XTX has an invalid 4bpp transfer-layout word at texture header +0x04.";
                    return false;
                }
            }

            const uint32_t rawFlags = readU32(dataBytes, entry.textureHeaderOffset + 0x0Cu);
            uint8_t widthPow2 = 0;
            uint8_t heightPow2 = 0;
            uint8_t bpp = 0;
            uint8_t mipCount = 0;
            uint8_t swizzleMask = 0;
            uint8_t unknownBits = 0;
            if (!decodePs2RuntimeFlags(
                    rawFlags,
                    widthPow2,
                    heightPow2,
                    bpp,
                    mipCount,
                    swizzleMask,
                    unknownBits)) {
                errorMessage =
                    "PS2 Stories XTX contains editor-canonical texture flags instead of the serialized Leeds runtime bit layout.";
                return false;
            }
            if (safePow2(widthPow2) != entry.width || safePow2(heightPow2) != entry.height || bpp != entry.bpp) {
                errorMessage = "PS2 Stories XTX serialized texture flags disagree with the decoded texture dimensions/BPP.";
                return false;
            }

            firstNode = std::min(firstNode, entry.containerBase);
            firstHeader = std::min(firstHeader, entry.textureHeaderOffset);
            const uint64_t rasterEnd = uint64_t(entry.rasterOffset) + uint64_t(entry.blockSize);
            if (rasterEnd > UINT32_MAX) {
                errorMessage = "PS2 Stories XTX raster range exceeds 32-bit file offsets.";
                return false;
            }
            lastRasterEnd = std::max(lastRasterEnd, uint32_t(rasterEnd));
        }

        // Verify the retail intrusive-list topology, not merely parseability.
        // +0x2C is the head link, +0x28 the tail link; each node's +0x08 is
        // previous and +0x0C is next. This is the distinction that older
        // Storyland writers got backwards.
        const uint32_t runtimeHeadSlot = readU32(dataBytes, 0x2Cu);
        const uint32_t runtimeTailSlot = readU32(dataBytes, 0x28u);
        if (runtimeHeadSlot == 0x28u || runtimeTailSlot == 0x28u) {
            errorMessage = "PS2 Stories XTX has an invalid non-empty head/tail sentinel.";
            return false;
        }
        uint32_t walkBase = slotBaseFromSlotPtr(runtimeHeadSlot);
        uint32_t previousSlot = 0x28u;
        std::set<uint32_t> runtimeVisited;
        size_t runtimeCount = 0u;
        while (walkBase >= 0x30u && walkBase + 0x60u <= dataBytes.size()) {
            if (!runtimeVisited.insert(walkBase).second) {
                errorMessage = "PS2 Stories XTX runtime list contains a cycle before the sentinel.";
                return false;
            }
            if (readU32(dataBytes, walkBase + 0x08u) != previousSlot) {
                errorMessage = "PS2 Stories XTX runtime previous-link chain is invalid.";
                return false;
            }
            const uint32_t nextSlot = readU32(dataBytes, walkBase + 0x0Cu);
            ++runtimeCount;
            if (nextSlot == 0x28u) {
                if (runtimeTailSlot != walkBase + 8u) {
                    errorMessage = "PS2 Stories XTX runtime tail link does not identify the last node.";
                    return false;
                }
                break;
            }
            if (nextSlot < 0x30u) {
                errorMessage = "PS2 Stories XTX runtime next-link chain terminates outside the sentinel.";
                return false;
            }
            previousSlot = walkBase + 8u;
            walkBase = slotBaseFromSlotPtr(nextSlot);
        }
        if (runtimeCount != trial.textures().size()) {
            errorMessage = "PS2 Stories XTX runtime list count does not match the decoded texture count.";
            return false;
        }

        const bool retailObjectOrder =
            firstNode != UINT32_MAX && firstHeader != UINT32_MAX &&
            lastRasterEnd <= firstNode && firstNode <= firstHeader;
        if (!retailObjectOrder) {
            errorMessage =
                "PS2 Stories XTX object order is not the retail runtime layout. "
                "Expected rasters, then 0x60-byte linked texture nodes, then 16-byte texture headers, then relocations.";
            return false;
        }
    }

    std::ostringstream stream;
    stream << "Texture archive validation: PASS\n";
    stream << "Bytes: " << dataBytes.size() << "\n";
    stream << "Textures: " << trial.textures().size() << "\n";
    if (ps2StoriesRuntimeXet) {
        stream << "PS2 runtime preface: recognized working profile\n";
        stream << "PS2 serialized flag layout: PASS\n";
        stream << "PS2 object ordering: recognized working profile\n";
    }
    for (size_t i = 0; i < trial.textures().size(); ++i) {
        const LeedsTextureEntry& entry = trial.textures()[i];
        const uint64_t end = uint64_t(entry.rasterOffset) + uint64_t(entry.blockSize);
        if (entry.textureHeaderOffset + 16u > dataBytes.size() ||
            entry.rasterOffset >= dataBytes.size() ||
            end > dataBytes.size()) {
            errorMessage = "Texture archive contains an out-of-range header or raster block.";
            return false;
        }
        stream << "[" << i << "] " << entry.name << " " << entry.width << "x" << entry.height
               << " " << int(entry.bpp) << "bpp mips=" << int(entry.mipCount)
               << " swizzle=" << int(entry.swizzleMask)
               << " header=0x" << std::hex << entry.textureHeaderOffset
               << " raster=0x" << entry.rasterOffset
               << " bytes=0x" << entry.blockSize << std::dec << "\n";
    }
    report = stream.str();
    errorMessage.clear();
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
    if (entry.kind == TextureKind::RwPsp) {
        if (!isLcsBetaRwTxdBytes(dataBytes)) {
            errorMessage = "This RenderWare TXD variant is read-only. Editable TXD authoring currently targets the LCS beta 0x1003FFFF layout.";
            return false;
        }
        if (newName.empty() || newName.size() > 31u || !isPrintableName(newName)) {
            errorMessage = "LCS beta TXD texture names must be 1-31 printable ASCII characters.";
            return false;
        }
        for (size_t i = 0u; i < entries.size(); ++i) {
            if (i == textureIndex) continue;
            std::string a = entries[i].name;
            std::string b = newName;
            std::transform(a.begin(), a.end(), a.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
            std::transform(b.begin(), b.end(), b.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
            if (a == b) {
                errorMessage = "A texture with that name already exists.";
                return false;
            }
        }

        std::vector<std::vector<uint8_t>> nativeChunks;
        if (!collectLcsBetaTxdNativeChunks(
                dataBytes,
                nativeChunks,
                errorMessage)) {
            return false;
        }
        if (nativeChunks.size() != entries.size() ||
            textureIndex >= nativeChunks.size()) {
            errorMessage = "LCS beta TXD native-texture list does not match the parsed texture table.";
            return false;
        }
        if (!patchLcsBetaNativeChunkName(
                nativeChunks[textureIndex],
                newName,
                errorMessage)) {
            return false;
        }

        std::vector<uint8_t> rebuilt;
        if (!rebuildLcsBetaTxdFromNativeChunks(
                dataBytes,
                nativeChunks,
                rebuilt,
                errorMessage)) {
            return false;
        }
        dataBytes = std::move(rebuilt);
        loadedPlatform = LeedsPlatform::Auto;
        return parse(loadedPlatform, errorMessage);
    }

    const size_t nameOffset = size_t(entry.containerBase) + (entry.kind == TextureKind::Dds ? 0x08u : 0x10u);
    if (!checkedTextureRange(nameOffset, 64u, dataBytes.size())) {
        errorMessage = "Texture container name field points outside the file.";
        return false;
    }

    std::fill(dataBytes.begin() + nameOffset, dataBytes.begin() + nameOffset + 64u, 0);
    std::copy(newName.begin(), newName.end(), dataBytes.begin() + nameOffset);

    if (!parse(loadedPlatform, errorMessage)) {
        return false;
    }

    // Renaming must never mutate PS2 transfer metadata. In particular, player
    // texture names do not imply reserved1 == 0.

    return true;
}

