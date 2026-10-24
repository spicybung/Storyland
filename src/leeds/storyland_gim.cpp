#include "storyland_gim.h"
#include "leeds_texture.h"
#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <set>

// GIM format investigation: SPP_EXP, SP-P_Engine/Parsers/Texture.cpp,
// revision e02082a835b364e431ddf9f23fe40e0a28c25a51.
// Header/index fields cross-checked with gimconv_v's published format notes.
namespace {
uint16_t u16(const std::vector<uint8_t>& data, size_t offset) {
    return uint16_t(data[offset]) | uint16_t(data[offset + 1]) << 8;
}
uint32_t u32(const std::vector<uint8_t>& data, size_t offset) {
    return uint32_t(u16(data, offset)) | uint32_t(u16(data, offset + 2)) << 16;
}
bool range(size_t start, size_t length, size_t end) { return start <= end && length <= end - start; }
uint32_t align(uint32_t value, uint32_t alignment) { return ((value + alignment - 1) / alignment) * alignment; }
uint32_t bpp(uint16_t format) {
    const uint32_t values[] = {16, 16, 16, 32, 4, 8, 16, 32};
    return format < 8 ? values[format] : 0;
}
size_t address(const StorylandGimImage& image, const StorylandGimLevel& level, uint32_t xByte, uint32_t y) {
    if (!image.swizzled) return size_t(level.offset) + size_t(y) * level.pitch + xByte;
    return size_t(level.offset) + size_t(y / 8) * level.pitch * 8 + size_t(xByte / 16) * 128 + (y % 8) * 16 + xByte % 16;
}
std::array<uint8_t, 4> unpack(uint32_t value, uint16_t format) {
    if (format == 3) return {uint8_t(value), uint8_t(value >> 8), uint8_t(value >> 16), uint8_t(value >> 24)};
    if (format == 0) return {uint8_t((value & 31) * 255 / 31), uint8_t(((value >> 5) & 63) * 255 / 63), uint8_t(((value >> 11) & 31) * 255 / 31), 255};
    if (format == 1) return {uint8_t((value & 31) * 255 / 31), uint8_t(((value >> 5) & 31) * 255 / 31), uint8_t(((value >> 10) & 31) * 255 / 31), uint8_t(value & 32768 ? 255 : 0)};
    return {uint8_t((value & 15) * 17), uint8_t(((value >> 4) & 15) * 17), uint8_t(((value >> 8) & 15) * 17), uint8_t(((value >> 12) & 15) * 17)};
}
uint32_t pack(const uint8_t* rgba, uint16_t format) {
    if (format == 3) return uint32_t(rgba[0]) | uint32_t(rgba[1]) << 8 | uint32_t(rgba[2]) << 16 | uint32_t(rgba[3]) << 24;
    if (format == 0) return uint32_t((rgba[0] * 31 + 127) / 255) | uint32_t((rgba[1] * 63 + 127) / 255) << 5 | uint32_t((rgba[2] * 31 + 127) / 255) << 11;
    if (format == 1) return uint32_t((rgba[0] * 31 + 127) / 255) | uint32_t((rgba[1] * 31 + 127) / 255) << 5 | uint32_t((rgba[2] * 31 + 127) / 255) << 10 | uint32_t(rgba[3] >= 128) << 15;
    return uint32_t((rgba[0] + 8) / 17) | uint32_t((rgba[1] + 8) / 17) << 4 | uint32_t((rgba[2] + 8) / 17) << 8 | uint32_t((rgba[3] + 8) / 17) << 12;
}
bool raster(const std::vector<uint8_t>& data, size_t chunk, size_t end, StorylandGimImage& image, std::string& error) {
    if (!range(chunk, 16, end)) return false;
    const uint32_t relative = u32(data, chunk + 12);
    if (relative < 16 || !range(chunk, relative, end)) return false;
    const size_t header = chunk + relative;
    if (!range(header, 48, end)) return false;
    const uint16_t headerSize = u16(data, header);
    const uint16_t order = u16(data, header + 6);
    const uint32_t width = u16(data, header + 8), height = u16(data, header + 10);
    image.format = u16(data, header + 4);
    image.headerOffset = uint32_t(chunk);
    image.swizzled = order == 1;
    const uint32_t depth = bpp(image.format);
    const uint32_t count = u16(data, header + 42), frames = u16(data, header + 46);
    if (headerSize < 48 || !range(header, headerSize, end) || !depth || order > 1 || !width || !height || width > 8192 || height > 8192 || count < 1 || count > 14 || frames != 1) {
        error = "Unsupported GIM raster dimensions, format, or animation frames."; return false;
    }
    uint32_t pitchAlign = u16(data, header + 14), heightAlign = u16(data, header + 16);
    if (!pitchAlign) pitchAlign = image.swizzled ? 16 : 1;
    if (!heightAlign) heightAlign = image.swizzled ? 8 : 1;
    if (pitchAlign > 4096 || heightAlign > 4096) return false;
    const uint32_t index = u32(data, header + 24);
    const uint32_t pixelsStart = u32(data, header + 28), pixelsEnd = u32(data, header + 32);
    if (index < headerSize || !range(header, index, end) || !range(header + index, size_t(count) * 4, end) || pixelsStart < index + count * 4 || pixelsEnd < pixelsStart || !range(header, pixelsEnd, end)) return false;
    std::vector<std::pair<size_t, size_t>> spans;
    for (uint32_t level = 0; level < count; ++level) {
        StorylandGimLevel mip;
        mip.width = std::max(1u, width >> level); mip.height = std::max(1u, height >> level);
        mip.pitch = align((mip.width * depth + 7) / 8, pitchAlign);
        mip.storedHeight = align(mip.height, heightAlign);
        if (image.swizzled && (mip.pitch % 16 || mip.storedHeight % 8)) return false;
        const uint64_t size = uint64_t(mip.pitch) * mip.storedHeight;
        const uint32_t offset = u32(data, header + index + level * 4);
        if (offset < pixelsStart || offset > pixelsEnd || size > pixelsEnd - offset || size > 256ull * 1024 * 1024) return false;
        mip.offset = uint32_t(header + offset); mip.size = uint32_t(size);
        for (const auto& previous : spans) if (mip.offset < previous.second && size_t(mip.offset) + mip.size > previous.first) return false;
        spans.emplace_back(mip.offset, size_t(mip.offset) + mip.size);
        image.levels.push_back(mip);
    }
    return true;
}
}

bool storylandIsGim(const std::vector<uint8_t>& data) {
    static const uint8_t signature[] = {'M','I','G','.','0','0','.','1','P','S','P',0};
    return data.size() >= 16 && std::equal(std::begin(signature), std::end(signature), data.begin());
}
bool storylandParseGim(const std::vector<uint8_t>& data, std::vector<StorylandGimImage>& images, std::string& error) {
    images.clear(); error.clear();
    if (!storylandIsGim(data) || data.size() > std::numeric_limits<uint32_t>::max()) { error = "Not a little-endian PSP GIM file."; return false; }
    std::vector<StorylandGimImage> parsed;
    std::set<size_t> visited;
    std::function<bool(size_t, size_t, unsigned)> visit;
    visit = [&](size_t start, size_t limit, unsigned depth) {
        if (depth > 16 || !range(start, 16, limit) || !visited.insert(start).second || visited.size() > 65536) return false;
        const uint16_t type = u16(data, start);
        const uint32_t size = u32(data, start + 4);
        if (size < 16 || !range(start, size, limit)) return false;
        const size_t end = start + size;
        if (type != 2 && type != 3) return true;
        const uint32_t child = u32(data, start + 8);
        if (child < 16 || child >= size) return false;
        std::vector<StorylandGimImage> pictures;
        StorylandGimImage palette;
        bool havePalette = false;
        for (size_t offset = start + child; offset < end;) {
            if (!range(offset, 16, end)) return false;
            const uint16_t kind = u16(data, offset);
            const uint32_t childSize = u32(data, offset + 4);
            if (childSize < 16 || !range(offset, childSize, end)) return false;
            if (type == 3 && (kind == 4 || kind == 5)) {
                StorylandGimImage image;
                if (!raster(data, offset, offset + childSize, image, error)) return false;
                if (kind == 4) pictures.push_back(std::move(image));
                else {
                    if (havePalette || image.format > 3 || image.levels.size() != 1 || image.swizzled) return false;
                    palette = std::move(image); havePalette = true;
                }
            } else if (!visit(offset, end, depth + 1)) return false;
            offset += childSize;
        }
        for (auto& image : pictures) {
            if (image.format >= 4) {
                if (!havePalette) { error = "GIM indexed image has no palette."; return false; }
                const auto& level = palette.levels[0];
                if (level.height != 1 || level.width > 65536) return false;
                image.paletteOffset = level.offset;
                image.paletteCount = level.width;
                image.paletteFormat = palette.format;
            }
            image.name = "texture_" + std::to_string(parsed.size());
            parsed.push_back(std::move(image));
        }
        return true;
    };
    if (!range(16, 16, data.size()) || u16(data, 16) != 2 || !visit(16, data.size(), 0) || parsed.empty()) {
        if (error.empty()) error = "GIM has an invalid or truncated block, raster, or mipmap table.";
        return false;
    }
    images = std::move(parsed);
    return true;
}
bool storylandDecodeGim(const std::vector<uint8_t>& data, const StorylandGimImage& image, uint32_t levelIndex, RgbaImage& output, std::string& error) {
    output = {}; error.clear();
    if (levelIndex >= image.levels.size()) { error = "GIM mipmap index is out of range."; return false; }
    const auto& level = image.levels[levelIndex];
    const uint32_t depth = bpp(image.format);
    if (!depth || !range(level.offset, level.size, data.size()) || uint64_t(level.width) * level.height > 64ull * 1024 * 1024) { error = "Invalid GIM raster span."; return false; }
    RgbaImage decoded;
    decoded.width = int(level.width); decoded.height = int(level.height);
    decoded.rgba.resize(size_t(level.width) * level.height * 4);
    for (uint32_t y = 0; y < level.height; ++y) for (uint32_t x = 0; x < level.width; ++x) {
        const uint32_t xByte = x * depth / 8;
        const size_t offset = address(image, level, xByte, y);
        const size_t count = (depth + 7) / 8;
        if (offset < level.offset || !range(offset - level.offset, count, level.size) || !range(offset, count, data.size())) { error = "GIM pixels exceed the raster."; return false; }
        uint32_t value = 0;
        for (size_t byte = 0; byte < count; ++byte) value |= uint32_t(data[offset + byte]) << (byte * 8);
        if (depth == 4) value = (value >> ((x & 1) * 4)) & 15;
        uint16_t format = image.format;
        if (format >= 4) {
            if (value >= image.paletteCount) { error = "GIM pixel references a missing palette color."; return false; }
            const uint32_t stride = image.paletteFormat == 3 ? 4 : 2;
            const size_t palette = size_t(image.paletteOffset) + size_t(value) * stride;
            if (!range(palette, stride, data.size())) { error = "GIM palette exceeds the file."; return false; }
            value = stride == 4 ? u32(data, palette) : u16(data, palette);
            format = image.paletteFormat;
        }
        const auto color = unpack(value, format);
        std::copy(color.begin(), color.end(), decoded.rgba.begin() + (size_t(y) * level.width + x) * 4);
    }
    output = std::move(decoded);
    return true;
}
bool storylandReplaceGim(std::vector<uint8_t>& data, const StorylandGimImage& image, const RgbaImage& replacement, const std::vector<uint8_t>& palette, std::string& error) {
    error.clear();
    if (image.levels.empty() || replacement.width != int(image.levels[0].width) || replacement.height != int(image.levels[0].height) || replacement.rgba.size() != size_t(replacement.width) * replacement.height * 4) { error = "GIM replacement must match the original dimensions."; return false; }
    if (!bpp(image.format) || (image.format >= 4 && (!image.paletteCount || image.paletteFormat > 3))) { error = "Invalid GIM pixel format or palette."; return false; }
    std::vector<uint8_t> edited = data;
    std::vector<std::array<uint8_t, 4>> colors;
    if (image.format >= 4) {
        if (palette.size() != size_t(image.paletteCount) * 4) { error = "GIM replacement palette is incomplete."; return false; }
        const size_t stride = image.paletteFormat == 3 ? 4 : 2;
        if (!range(image.paletteOffset, image.paletteCount * stride, edited.size())) return false;
        for (size_t i = 0; i < image.paletteCount; ++i) {
            const uint32_t value = pack(palette.data() + i * 4, image.paletteFormat);
            for (size_t byte = 0; byte < stride; ++byte) edited[image.paletteOffset + i * stride + byte] = uint8_t(value >> (8 * byte));
            if (image.format != 4 || i < 16) colors.push_back(unpack(value, image.paletteFormat));
        }
    }
    RgbaImage mip = replacement;
    const uint32_t depth = bpp(image.format);
    for (const auto& level : image.levels) {
        if (!range(level.offset, level.size, edited.size()) || mip.width != int(level.width) || mip.height != int(level.height)) { error = "GIM mipmap dimensions are inconsistent."; return false; }
        for (uint32_t y = 0; y < level.height; ++y) for (uint32_t x = 0; x < level.width; ++x) {
            const uint8_t* pixel = mip.rgba.data() + (size_t(y) * level.width + x) * 4;
            uint32_t value = 0;
            if (image.format < 4) value = pack(pixel, image.format);
            else {
                uint32_t best = std::numeric_limits<uint32_t>::max();
                for (size_t index = 0; index < colors.size(); ++index) {
                    uint32_t distance = 0;
                    for (size_t channel = 0; channel < 4; ++channel) { const int delta = int(pixel[channel]) - colors[index][channel]; distance += uint32_t(delta * delta); }
                    if (distance < best) { best = distance; value = uint32_t(index); if (!best) break; }
                }
            }
            const size_t offset = address(image, level, x * depth / 8, y);
            if (offset < level.offset || !range(offset - level.offset, (depth + 7) / 8, level.size) || !range(offset, (depth + 7) / 8, edited.size())) { error = "GIM replacement pixels exceed the raster."; return false; }
            if (depth == 4) {
                const unsigned shift = (x & 1) * 4;
                edited[offset] = uint8_t((edited[offset] & ~(15u << shift)) | (value << shift));
            } else for (size_t byte = 0; byte < depth / 8; ++byte) edited[offset + byte] = uint8_t(value >> (byte * 8));
        }
        RgbaImage next;
        next.width = std::max(1, mip.width / 2); next.height = std::max(1, mip.height / 2);
        next.rgba.resize(size_t(next.width) * next.height * 4);
        for (int y = 0; y < next.height; ++y) for (int x = 0; x < next.width; ++x) for (size_t c = 0; c < 4; ++c) {
            unsigned sum = 0, count = 0;
            for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) {
                const int sx = x * 2 + dx, sy = y * 2 + dy;
                if (sx < mip.width && sy < mip.height) { sum += mip.rgba[(size_t(sy) * mip.width + sx) * 4 + c]; ++count; }
            }
            next.rgba[(size_t(y) * next.width + x) * 4 + c] = uint8_t((sum + count / 2) / count);
        }
        mip = std::move(next);
    }
    data = std::move(edited);
    return true;
}
