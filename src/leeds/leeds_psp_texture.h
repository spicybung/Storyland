#pragma once

#include "src/leeds/leeds_texture.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

struct StorylandPspTextureLayout {
    size_t dataOffset = 0;
    size_t paletteOffset = 0;
    size_t pixelBytes = 0;
    size_t storageBytes = 0;
    size_t rowPitch = 0;
    int width = 0;
    int height = 0;
    unsigned bpp = 0;
    unsigned mipCount = 0;
    bool swizzled = false;
};

inline bool storylandResolvePspTextureLayout(
    size_t dataOffset, unsigned logWidth, unsigned logHeight, unsigned bpp,
    unsigned mipCount, size_t rowPitch, bool swizzled, size_t dataEnd,
    StorylandPspTextureLayout& layout) {
    layout = {};
    if (logWidth > 11 || logHeight > 11 || (bpp != 4 && bpp != 8 && bpp != 32) ||
        !mipCount || mipCount > std::max(logWidth, logHeight) + 1u || dataOffset >= dataEnd)
        return false;
    const size_t width = size_t(1) << logWidth;
    const size_t height = size_t(1) << logHeight;
    const size_t minimumPitch = std::max<size_t>(16u, (width * bpp + 7u) / 8u);
    if (!rowPitch) rowPitch = minimumPitch;
    if (rowPitch < minimumPitch || (rowPitch & 15u) || rowPitch > 8192u) return false;

    size_t mipBytes = 0;
    for (unsigned level = 0; level < mipCount; ++level) {
        const size_t pitch = std::max<size_t>(16u, rowPitch >> level);
        const size_t rows = std::max<size_t>(1u, height >> level);
        mipBytes += pitch * rows;
    }
    const size_t paletteBytes = bpp == 4 ? 64u : bpp == 8 ? 1024u : 0u;
    if (mipBytes > dataEnd - dataOffset || paletteBytes > dataEnd - dataOffset - mipBytes)
        return false;

    layout.dataOffset = dataOffset;
    layout.paletteOffset = dataOffset + mipBytes;
    layout.pixelBytes = rowPitch * height;
    layout.storageBytes = mipBytes + paletteBytes;
    layout.rowPitch = rowPitch;
    layout.width = int(width);
    layout.height = int(height);
    layout.bpp = bpp;
    layout.mipCount = mipCount;
    layout.swizzled = swizzled;
    return true;
}

inline size_t storylandPspTextureDecodedBytes(const StorylandPspTextureLayout& layout) {
    if (layout.width <= 0 || layout.height <= 0 || !layout.mipCount || layout.mipCount > 12u)
        return 0;
    size_t total = 0;
    for (unsigned level = 0; level < layout.mipCount; ++level) {
        const uint64_t pixels = uint64_t(std::max(1, layout.width >> level)) *
            uint64_t(std::max(1, layout.height >> level));
        if (pixels > (std::numeric_limits<size_t>::max() - total) / 4u) return 0;
        total += size_t(pixels * 4u);
    }
    return total;
}

inline RgbaImage storylandDecodePspTexture(
    const std::vector<uint8_t>& bytes, const StorylandPspTextureLayout& layout,
    size_t maximumDecodedBytes = std::numeric_limits<size_t>::max()) {
    const size_t decodedBytes = storylandPspTextureDecodedBytes(layout);
    if (!decodedBytes || decodedBytes > maximumDecodedBytes ||
        (layout.bpp != 4u && layout.bpp != 8u && layout.bpp != 32u) ||
        layout.dataOffset > bytes.size() || layout.storageBytes > bytes.size() - layout.dataOffset ||
        layout.paletteOffset < layout.dataOffset ||
        layout.paletteOffset > layout.dataOffset + layout.storageBytes) return {};
    RgbaImage image;
    image.width = layout.width;
    image.height = layout.height;
    image.mipLevels.reserve(layout.mipCount - 1u);
    const size_t dataEnd = layout.dataOffset + layout.storageBytes;
    size_t levelOffset = layout.dataOffset;
    for (unsigned level = 0; level < layout.mipCount; ++level) {
        RgbaMipLevel mip;
        mip.width = std::max(1, image.width >> level);
        mip.height = std::max(1, image.height >> level);
        const size_t rowPitch = std::max<size_t>(16u, layout.rowPitch >> level);
        const size_t pixelBytes = rowPitch * size_t(mip.height);
        if (levelOffset > layout.paletteOffset || pixelBytes > layout.paletteOffset - levelOffset)
            return {};
        mip.rgba.resize(size_t(mip.width) * size_t(mip.height) * 4u);

        for (int y = 0; y < mip.height; ++y) {
            for (int x = 0; x < mip.width; ++x) {
                const size_t byteX = size_t(x) * layout.bpp / 8u;
                const size_t source = layout.swizzled ?
                    size_t(y / 8) * rowPitch * 8u + (byteX / 16u) * 128u +
                        size_t(y % 8) * 16u + byteX % 16u :
                    size_t(y) * rowPitch + byteX;
                const size_t sourceBytes = layout.bpp == 32u ? 4u : 1u;
                // Short rectangular mips retain GE's 16-byte by 8-row addressing.
                // Retail allocations can place the next mip inside that address range.
                if (source > dataEnd - levelOffset ||
                    sourceBytes > dataEnd - levelOffset - source) return {};
                const size_t destination = (size_t(y) * size_t(mip.width) + size_t(x)) * 4u;
                size_t color = levelOffset + source;
                if (layout.bpp != 32u) {
                    unsigned index = bytes[color];
                    if (layout.bpp == 4u) index = (index >> ((unsigned(x) & 1u) * 4u)) & 15u;
                    color = layout.paletteOffset + size_t(index) * 4u;
                }
                if (color > dataEnd || dataEnd - color < 4u) return {};
                std::copy_n(bytes.data() + color, 4u, mip.rgba.data() + destination);
            }
        }
        levelOffset += pixelBytes;
        if (level == 0u) image.rgba = std::move(mip.rgba);
        else image.mipLevels.push_back(std::move(mip));
    }
    return image;
}
