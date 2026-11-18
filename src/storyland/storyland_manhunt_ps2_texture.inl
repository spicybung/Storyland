static std::vector<uint8_t> unswizzlePs2Indices(const std::vector<uint8_t> &indices, int width, int height);
static void convertClutPs2(std::vector<uint8_t> &indices);

static bool manhuntTcdtIsPs2(const std::vector<uint8_t> &data) {
    if (data.size() < 48 || readU32(data, 0) != 0x54444354u)
        return false;
    const uint32_t count = readU32(data, 32);
    uint32_t offset = readU32(data, 36);
    std::set<uint32_t> visited;
    for (uint32_t i = 0; i < count && i < 65536; ++i) {
        if (!checkedTextureRange(offset, 0x68, data.size()) || !visited.insert(offset).second)
            return false;
        if (data[offset + 0x5D] || data[offset + 0x5C] > 1)
            return true;
        offset = readU32(data, offset);
    }
    return false;
}

static bool decodeManhuntPs2Texture(const std::vector<uint8_t> &bytes, const LeedsTextureEntry &entry,
                                    RgbaImage &image, std::string &error) {
    const size_t count = size_t(entry.width) * entry.height;
    const size_t storage = (count * entry.bpp + 7) / 8;
    if (!checkedTextureRange(entry.rasterOffset, storage, bytes.size())) {
        error = "PS2 texture raster is truncated.";
        return false;
    }
    image.width = entry.width;
    image.height = entry.height;
    image.rgba.resize(count * 4);
    if (entry.bpp == 32) {
        std::copy_n(bytes.data() + entry.rasterOffset, count * 4, image.rgba.data());
        for (size_t i = 0; i < count; ++i)
            image.rgba[i * 4 + 3] = uint8_t(std::min(255u, unsigned(image.rgba[i * 4 + 3]) * 255 / 128));
        return true;
    }
    if (entry.bpp == 16 && (entry.swizzleMask & 1) == 0) {
        for (size_t i = 0; i < count; ++i) {
            const uint16_t packed = uint16_t(bytes[entry.rasterOffset + i * 2]) |
                                    uint16_t(bytes[entry.rasterOffset + i * 2 + 1]) << 8;
            image.rgba[i * 4] = uint8_t((packed & 31) * 255 / 31);
            image.rgba[i * 4 + 1] = uint8_t(((packed >> 5) & 31) * 255 / 31);
            image.rgba[i * 4 + 2] = uint8_t(((packed >> 10) & 31) * 255 / 31);
            image.rgba[i * 4 + 3] = (packed & 0x8000) ? 255 : 0;
        }
        error.clear();
        return true;
    }
    if (entry.bpp != 4 && entry.bpp != 8) {
        error = "Unsupported PS2 Manhunt texture depth.";
        return false;
    }
    std::vector<uint8_t> indices(count);
    for (size_t i = 0; i < count; ++i) {
        const uint8_t value = bytes[entry.rasterOffset + (entry.bpp == 4 ? i / 2 : i)];
        indices[i] = entry.bpp == 4 ? uint8_t((value >> ((i & 1) * 4)) & 15) : value;
    }
    if (entry.swizzleMask & 1)
        indices = unswizzlePs2Indices(indices, entry.width, entry.height);
    if (entry.bpp == 8)
        convertClutPs2(indices);
    const size_t paletteBytes = entry.bpp == 4 ? 64 : 1024;
    if (!checkedTextureRange(entry.mobilePaletteOffset, paletteBytes, bytes.size())) {
        error = "PS2 texture palette is truncated.";
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        const size_t palette = entry.mobilePaletteOffset + size_t(indices[i]) * 4;
        std::copy_n(bytes.data() + palette, 4, image.rgba.data() + i * 4);
        image.rgba[i * 4 + 3] = uint8_t(std::min(255u, unsigned(image.rgba[i * 4 + 3]) * 255 / 128));
    }
    error.clear();
    return true;
}
