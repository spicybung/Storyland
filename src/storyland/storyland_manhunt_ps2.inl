static bool manhuntPs2Packet(const std::vector<uint8_t> &bytes, size_t start, size_t limit,
                             const StorylandPreviewTransform &transform, float scaleU, float scaleV,
                             uint32_t material, const std::vector<uint32_t> &palette,
                             const std::array<float, 16> *rigid, std::vector<StorylandModelPoint> &points,
                             std::vector<StorylandModelTriangle> &triangles,
                             std::vector<StorylandModelTexcoord> &texcoords,
                             std::vector<StorylandModelSkinWeights> &skin,
                             std::vector<StorylandModelPrelight> &colors) {
    if (!modelRangeFits(start, 32, limit))
        return false;
    const uint32_t tag = modelReadU32(bytes, start);
    const size_t size = (size_t(tag & 65535) + 1) * 16;
    if (((tag >> 28) & 7) != 6 || !modelRangeFits(start, size, limit))
        return false;
    const size_t end = start + size;
    size_t cursor = start + 16;
    bool decoded = false;
    while (cursor + 20 <= end) {
        while (cursor + 4 <= end && modelReadU32(bytes, cursor) == 0)
            cursor += 4;
        if (cursor == end)
            break;
        if (!modelRangeFits(cursor, 20, end) || modelReadU32(bytes, cursor) != 0x6C018000u)
            return false;
        const uint32_t count = modelReadU32(bytes, cursor + 12) & 32767;
        if (count < 3 || count > 256)
            return false;
        cursor += 20;
        size_t positions = 0, uv = 0, color = 0, weights = 0;
        uint32_t colorType = 0;
        bool emitted = false;
        while (cursor + 4 <= end) {
            const uint32_t word = modelReadU32(bytes, cursor);
            cursor += 4;
            const uint32_t command = (word >> 24) & 127;
            if (command == 0x14) {
                emitted = true;
                break;
            }
            if (command == 0x20) {
                if (!modelRangeFits(cursor, 4, end))
                    return false;
                cursor += 4;
                continue;
            }
            if (command == 0x30 || command == 0x31) {
                if (!modelRangeFits(cursor, 16, end))
                    return false;
                cursor += 16;
                continue;
            }
            if (command == 0 || command == 1 || command == 5)
                continue;
            if (command < 0x60)
                return false;
            const uint32_t n = ((word >> 16) & 255) ? ((word >> 16) & 255) : 256;
            const uint32_t format = command & 15, components = ((format >> 2) & 3) + 1, bits = format & 3;
            const size_t element = bits == 3 ? 2 : components * (bits == 0 ? 4 : bits == 1 ? 2 : 1);
            const size_t payload = size_t(n) * element;
            if (!modelRangeFits(cursor, (payload + 3) & ~size_t(3), end))
                return false;
            if (format == 9 && n == count)
                positions = cursor;
            else if (format == 6 && n == count)
                uv = cursor;
            else if ((format == 14 || format == 15) && n == count) {
                color = cursor;
                colorType = format;
            } else if (format == 12 && n == count)
                weights = cursor;
            cursor += (payload + 3) & ~size_t(3);
        }
        if (!emitted || !positions)
            return false;
        const uint32_t base = uint32_t(points.size());
        std::vector<StorylandModelSkinWeights> decodedWeights;
        if (weights &&
            !decodeSkinWeightsFromPedPayload(bytes, weights, count, count, decodedWeights))
            return false;
        for (uint32_t i = 0; i < count; ++i) {
            auto point = decodeLeedsPackedPosition(modelReadI16(bytes, positions + i * 6),
                                                   modelReadI16(bytes, positions + i * 6 + 2),
                                                   modelReadI16(bytes, positions + i * 6 + 4), transform);
            if (!weights && rigid)
                point = transformMh2Point(point, rigid->data());
            points.push_back(point);
            texcoords.push_back(uv ? StorylandModelTexcoord{bytes[uv + i * 2] / 128.f * scaleU,
                                                            bytes[uv + i * 2 + 1] / 128.f * scaleV}
                                   : StorylandModelTexcoord{});
            StorylandModelSkinWeights row = weights ? decodedWeights[i] : StorylandModelSkinWeights{};
            for (uint32_t j = 0; j < row.influenceCount; ++j) {
                auto &item = row.influences[j];
                const uint32_t index = item.rawMatrixIndex / 4;
                item.boneIndex = index < palette.size() ? palette[index] : 0xFFFFFFFFu;
                if (item.boneIndex == 0xFFFFFFFFu)
                    return false;
            }
            skin.push_back(row);
            StorylandModelPrelight value;
            if (color) {
                if (colorType == 14) {
                    const size_t offset = color + i * 4;
                    value.red = bytes[offset];
                    value.green = bytes[offset + 1];
                    value.blue = bytes[offset + 2];
                    value.alpha = uint8_t(std::min(255u, unsigned(bytes[offset + 3]) * 255 / 128));
                    value.fileOffset = uint32_t(offset);
                    value.encoding = StorylandPrelightEncoding::Ps2VifRgba8888;
                } else {
                    const uint16_t packed = modelReadU16(bytes, color + i * 2);
                    value.red = uint8_t((packed & 31) * 255 / 31);
                    value.green = uint8_t(((packed >> 5) & 31) * 255 / 31);
                    value.blue = uint8_t(((packed >> 10) & 31) * 255 / 31);
                    value.alpha = (packed & 0x8000) ? 255 : 0;
                    value.fileOffset = uint32_t(color + i * 2);
                    value.encoding = StorylandPrelightEncoding::Ps2Vif5551;
                }
                value.valid = true;
            }
            colors.push_back(value);
        }
        appendTriangleStripPreview(base, count, material, points, triangles);
        decoded = true;
    }
    return decoded;
}

static bool manhuntPs2Geometry(const std::vector<uint8_t> &bytes, size_t marker, size_t limit,
                               uint32_t materialBase, uint32_t materialCount,
                               const std::vector<uint32_t> &palette, const std::array<float, 16> *rigid,
                               std::vector<StorylandModelPoint> &points,
                               std::vector<StorylandModelTriangle> &triangles,
                               std::vector<StorylandModelTexcoord> &texcoords,
                               std::vector<StorylandModelSkinWeights> &skin,
                               std::vector<StorylandModelPrelight> &colors, size_t &geometryEnd) {
    if (marker < 64 || !modelRangeFits(marker, 64, limit) || modelReadU32(bytes, marker) != 0x50533244u)
        return false;
    const size_t header = marker - 64, descriptors = marker + 16;
    uint32_t count = 0;
    size_t packets = 0;
    for (uint32_t i = 1; i <= 4096; ++i) {
        const size_t probe = descriptors + size_t(i) * 48;
        if (!modelRangeFits(probe, 36, limit))
            break;
        if (((modelReadU32(bytes, probe) >> 28) & 7) == 6 && modelReadU32(bytes, probe + 16) == 0x6C018000u) {
            count = i;
            packets = probe;
            break;
        }
    }
    if (!count)
        return false;
    StorylandPreviewTransform transform;
    transform.valid = true;
    transform.sx = modelReadF32(bytes, header + 40);
    transform.sy = modelReadF32(bytes, header + 44);
    transform.sz = modelReadF32(bytes, header + 48);
    transform.tx = modelReadF32(bytes, header + 52);
    transform.ty = modelReadF32(bytes, header + 56);
    transform.tz = modelReadF32(bytes, header + 60);
    if (!std::isfinite(transform.sx + transform.sy + transform.sz + transform.tx + transform.ty +
                       transform.tz))
        return false;
    geometryEnd = packets;
    for (uint32_t i = 0; i < count; ++i) {
        const size_t descriptor = descriptors + size_t(i) * 48;
        const uint32_t material = modelReadU16(bytes, descriptor + 34);
        if (material >= materialCount)
            return false;
        const size_t packet = packets + modelReadU32(bytes, descriptor + 28);
        float u = modelReadF32(bytes, descriptor + 16), v = modelReadF32(bytes, descriptor + 20);
        if (!std::isfinite(u + v))
            return false;
        if (!manhuntPs2Packet(bytes, packet, limit, transform, u, v, materialBase + material, palette, rigid,
                              points, triangles, texcoords, skin, colors))
            return false;
        geometryEnd = std::max(geometryEnd, packet + (size_t(modelReadU32(bytes, packet) & 65535) + 1) * 16);
    }
    return true;
}
