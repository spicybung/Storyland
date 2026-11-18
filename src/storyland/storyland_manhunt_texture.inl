static bool parseManhuntPspTextures(const std::vector<uint8_t> &bytes,
                                    std::vector<LeedsTextureEntry> &textures, std::string &error) {
    const uint32_t count = readU32(bytes, 0x20), end = readU32(bytes, 0x10);
    if (count > 65536 || end > bytes.size() || end < 0x30) {
        error = "Invalid PSP TCDT header.";
        return false;
    }
    uint32_t offset = readU32(bytes, 0x24);
    std::set<uint32_t> visited;
    std::vector<LeedsTextureEntry> parsed;
    for (uint32_t i = 0; i < count; ++i) {
        if (offset < 0x30 || !checkedTextureRange(offset, 0x68, end) || !visited.insert(offset).second) {
            error = "Invalid PSP TCDT texture list.";
            return false;
        }
        LeedsTextureEntry entry;
        entry.kind = TextureKind::ManhuntPsp;
        entry.containerBase = offset;
        entry.textureHeaderOffset = offset;
        entry.name = readCString(bytes, offset + 8, 64);
        const uint32_t width = readU32(bytes, offset + 0x48), height = readU32(bytes, offset + 0x4C),
                       bpp = readU32(bytes, offset + 0x50);
        if (!width || !height || width > 8192 || height > 8192 ||
            (bpp != 4 && bpp != 8 && bpp != 16 && bpp != 32)) {
            error = "Invalid PSP TCDT pixel format.";
            return false;
        }
        entry.width = int(width);
        entry.height = int(height);
        entry.bpp = uint8_t(bpp);
        entry.flags = readU32(bytes, offset + 0x58);
        entry.mipCount = bytes[offset + 0x5C];
        entry.swizzleMask = bytes[offset + 0x5D];
        entry.rasterOffset = readU32(bytes, offset + 0x60);
        entry.mobilePaletteOffset = readU32(bytes, offset + 0x64);
        const size_t packedRowBytes = (size_t(width) * bpp + 7) / 8;
        const size_t rowBytes = (packedRowBytes + 15) & ~size_t(15);
        const size_t rasterBytes = rowBytes * ((size_t(height) + 7) / 8 * 8);
        const size_t paletteBytes = bpp == 4 ? 64 : (bpp == 8 ? 1024 : 0);
        if (!checkedTextureRange(entry.rasterOffset, rasterBytes, end) ||
            (paletteBytes && !checkedTextureRange(entry.mobilePaletteOffset, paletteBytes, end))) {
            error = "PSP TCDT raster or palette is truncated.";
            return false;
        }
        entry.blockSize = uint32_t(rasterBytes);
        entry.mobilePaletteBytes = uint32_t(paletteBytes);
        parsed.push_back(entry);
        offset = readU32(bytes, offset);
    }
    textures = std::move(parsed);
    error.clear();
    return true;
}

static bool decodeManhuntPspTexture(const std::vector<uint8_t> &bytes, const LeedsTextureEntry &entry,
                                    RgbaImage &image, std::string &error) {
    const size_t packedRowBytes = (size_t(entry.width) * entry.bpp + 7) / 8;
    const size_t rowBytes = (packedRowBytes + 15) & ~size_t(15);
    const size_t storage = rowBytes * ((size_t(entry.height) + 7) / 8 * 8);
    if (!checkedTextureRange(entry.rasterOffset, storage, bytes.size())) {
        error = "PSP texture raster is truncated.";
        return false;
    }
    image.width = entry.width;
    image.height = entry.height;
    image.rgba.resize(size_t(entry.width) * entry.height * 4);
    auto pixelByte = [&](size_t x, size_t y) {
        const size_t address = (y / 8) * (rowBytes * 8) + (x / 16) * 128 + (y % 8) * 16 + x % 16;
        return bytes[entry.rasterOffset + address];
    };
    for (int y = 0; y < entry.height; ++y)
        for (int x = 0; x < entry.width; ++x) {
            uint8_t *target = image.rgba.data() + (size_t(y) * entry.width + x) * 4;
            if (entry.bpp == 4 || entry.bpp == 8) {
                const uint8_t packed = pixelByte(entry.bpp == 4 ? size_t(x) / 2 : size_t(x), size_t(y));
                const unsigned index = entry.bpp == 4 ? ((packed >> ((x & 1) * 4)) & 15) : packed;
                const size_t palette = entry.mobilePaletteOffset + index * 4;
                if (!checkedTextureRange(palette, 4, bytes.size())) {
                    error = "PSP texture palette is truncated.";
                    return false;
                }
                std::copy_n(bytes.data() + palette, 4, target);
            } else if (entry.bpp == 32) {
                for (size_t component = 0; component < 4; ++component)
                    target[component] = pixelByte(size_t(x) * 4 + component, size_t(y));
            } else {
                const uint16_t value = uint16_t(pixelByte(size_t(x) * 2, size_t(y))) |
                                       uint16_t(pixelByte(size_t(x) * 2 + 1, size_t(y))) << 8;
                target[0] = uint8_t((value & 31) * 255 / 31);
                target[1] = uint8_t(((value >> 5) & 31) * 255 / 31);
                target[2] = uint8_t(((value >> 10) & 31) * 255 / 31);
                target[3] = (value & 0x8000) ? 255 : 0;
            }
        }
    error.clear();
    return true;
}
