static uint32_t manhuntBoneIdFromName(std::string name) {
    name = modelLowerAscii(name);
    for (char &character : name)
        if (character == ' ' || character == '-')
            character = '_';
    if (name.compare(0, 6, "bip01_") == 0)
        name.erase(0, 6);
    static const std::map<std::string, uint32_t> identifiers = {
        {"bip01", 1000},      {"root", 1000},       {"head", 1001},       {"l_calf", 1002},
        {"l_clavicle", 1003}, {"l_finger0", 1004},  {"l_finger1", 1005},  {"l_finger01", 1006},
        {"l_finger2", 1008},  {"l_finger11", 1011}, {"l_finger21", 1013}, {"l_foot", 1019},
        {"l_forearm", 1020},  {"l_hand", 1021},     {"l_thigh", 1023},    {"l_toe", 1024},
        {"l_toe0", 1024},     {"l_upperarm", 1039}, {"neck", 1040},       {"pelvis", 1045},
        {"r_calf", 1056},     {"r_clavicle", 1057}, {"r_finger0", 1058},  {"r_finger1", 1059},
        {"r_finger01", 1060}, {"r_finger2", 1062},  {"r_finger11", 1065}, {"r_finger21", 1067},
        {"r_foot", 1073},     {"r_forearm", 1074},  {"r_hand", 1075},     {"r_thigh", 1077},
        {"r_toe", 1078},      {"r_toe0", 1078},     {"r_upperarm", 1093}, {"spine", 1094},
        {"spine1", 1095},     {"spine2", 1096}};
    const auto found = identifiers.find(name);
    return found == identifiers.end() ? 0xFFFFFFFFu : found->second;
}

bool StorylandModelFile::parseManhuntPsp() {
    const bool world = data.size() >= 0x60 && std::memcmp(data.data(), "DLRW", 4) == 0;
    const bool clump = data.size() >= 0x30 && std::memcmp(data.data(), "PMLC", 4) == 0;
    if (!world && !clump)
        return false;
    points.clear();
    triangles.clear();
    texcoords.clear();
    skinWeights.clear();
    bones.clear();
    prelights.clear();
    pointFrameIndices.clear();
    materialTextureNames.clear();
    const size_t payloadEnd = modelReadU32(data, 0x10);
    if (payloadEnd < 0x30 || payloadEnd > data.size())
        return false;
    if (uint64_t(payloadEnd) + uint64_t(modelReadU32(data, 0x14)) * 4 > data.size())
        return false;
    struct Geometry {
        uint32_t offset;
        uint32_t bone;
        uint32_t materials;
        uint32_t count;
        std::vector<uint32_t> palette;
    };
    std::vector<Geometry> geometry;
    std::map<uint32_t, uint32_t> boneIndices;
    std::map<uint32_t, std::array<float, 16>> matrices;
    uint32_t clumpEntries = 0;
    if (world) {
        const uint32_t table = modelReadU32(data, 0x50), count = modelReadU32(data, 0x54);
        if (count > 65536 || !modelRangeFits(table, size_t(count) * 12, payloadEnd))
            return false;
        for (uint32_t i = 0; i < count; ++i)
            materialTextureNames.push_back(
                modelLowerAscii(modelReadBoundedAscii(data, modelReadU32(data, table + i * 12), 128)));
        for (size_t offset = 0x60; offset + 0x50 <= payloadEnd; offset += 4) {
            if (modelReadU32(data, offset) == 0x50533244u && offset >= 64)
                geometry.push_back({uint32_t(offset - 64), 0, 0, 0, {}});
            else if (modelReadU32(data, offset + 4) == 0x115)
                geometry.push_back({uint32_t(offset), 0, 0, 0, {}});
        }
    } else {
        std::set<uint32_t> entries;
        uint32_t entry = modelReadU32(data, 0x20);
        while (entry >= 0x30 && modelRangeFits(entry, 16, payloadEnd) && entries.insert(entry).second) {
            ++clumpEntries;
            const uint32_t body = modelReadU32(data, entry + 8);
            if (!modelRangeFits(body, 28, payloadEnd))
                return false;
            const uint32_t firstBone = uint32_t(bones.size());
            std::vector<uint32_t> pending{modelReadU32(data, body)};
            while (!pending.empty()) {
                const uint32_t offset = pending.back();
                pending.pop_back();
                if (!offset || boneIndices.count(offset))
                    continue;
                if (bones.size() >= 65536 || !modelRangeFits(offset, 192, payloadEnd))
                    return false;
                StorylandModelBone bone;
                bone.index = uint32_t(bones.size());
                bone.offset = offset;
                bone.parentOffset = modelReadU32(data, offset + 8);
                bone.nameOffset = offset + 0x18;
                bone.name = modelReadBoundedAscii(data, offset + 0x18, 40);
                bone.sectionKind = "Skeleton";
                std::array<float, 16> matrix;
                for (size_t i = 0; i < 16; ++i) {
                    matrix[i] = modelReadF32(data, offset + 0x80 + i * 4);
                    if (!std::isfinite(matrix[i]))
                        return false;
                }
                bone.worldPosition = {matrix[12], matrix[13], matrix[14]};
                bone.previewPosition = bone.worldPosition;
                bone.hasWorldPosition = bone.hasPreviewPosition = true;
                bone.previewPositionSource = "world matrix";
                boneIndices[offset] = bone.index;
                matrices[offset] = matrix;
                bones.push_back(bone);
                pending.push_back(modelReadU32(data, offset + 4));
                pending.push_back(modelReadU32(data, offset + 0x10));
            }
            std::vector<uint32_t> palette;
            for (size_t b = firstBone; b < bones.size(); ++b) {
                const uint32_t binding = modelReadU32(data, bones[b].offset + 0x14);
                if (!binding || !modelRangeFits(binding, 24, payloadEnd))
                    continue;
                const uint32_t count = modelReadU32(data, binding);
                const uint32_t root = modelReadU32(data, binding + 8);
                const uint32_t table = modelReadU32(data, binding + 12);
                if (!boneIndices.count(root) || !count || count > 4096 ||
                    !modelRangeFits(table, size_t(count) * 8, payloadEnd))
                    continue;
                const uint32_t rootIndex = boneIndices.at(root);
                bool hierarchyOrderValid = uint64_t(rootIndex) + count <= bones.size();
                for (uint32_t i = 0; hierarchyOrderValid && i < count; ++i) {
                    const uint32_t knownId = manhuntBoneIdFromName(bones[rootIndex + i].name);
                    const uint32_t storedId = uint32_t(int32_t(modelReadI16(data, table + i * 8)));
                    const uint32_t storedOffset = modelReadU32(data, table + i * 8 + 4);
                    if ((knownId != 0xFFFFFFFFu && knownId != storedId) ||
                        (storedOffset != 0 && storedOffset != bones[rootIndex + i].offset))
                        hierarchyOrderValid = false;
                }
                std::vector<uint32_t> candidate;
                for (uint32_t i = 0; i < count; ++i) {
                    const uint32_t sourceOffset = modelReadU32(data, table + i * 8 + 4);
                    const uint32_t identifier = uint32_t(int32_t(modelReadI16(data, table + i * 8)));
                    const auto found = boneIndices.find(sourceOffset);
                    uint32_t index = found == boneIndices.end() ? 0xFFFFFFFFu : found->second;
                    if (sourceOffset == 0 && hierarchyOrderValid)
                        index = rootIndex + i;
                    if (sourceOffset == 0 && index == 0xFFFFFFFFu && identifier != 0xFFFFFFFFu) {
                        if (i == 0 && identifier == 8889)
                            index = boneIndices.at(root);
                        else
                            for (size_t b = firstBone; b < bones.size(); ++b) {
                                if (manhuntBoneIdFromName(bones[b].name) == identifier) {
                                    index = uint32_t(b);
                                    break;
                                }
                            }
                    }
                    candidate.push_back(index);
                    if (index != 0xFFFFFFFFu) {
                        bones[index].boneId = identifier;
                        bones[index].nodeId = i;
                    }
                }
                palette = std::move(candidate);
                break;
            }
            std::set<uint32_t> objects;
            uint32_t object = modelReadU32(data, body + 0x14);
            while (object >= 0x30 && object != body + 0x14 && objects.insert(object).second) {
                if (!modelRangeFits(object, 28, payloadEnd))
                    return false;
                const uint32_t mesh = modelReadU32(data, object + 12);
                if (!modelRangeFits(mesh, 0x60, payloadEnd))
                    return false;
                PspNativeVertexLayout meshLayout;
                if (modelReadU32(data, mesh + 0x50) == 0x50533244u ||
                    buildPspNativeVertexLayout(modelReadU32(data, mesh + 0x14), meshLayout))
                    geometry.push_back({mesh + 0x10, modelReadU32(data, object + 8), modelReadU32(data, mesh),
                                        modelReadU32(data, mesh + 4), palette});
                const uint32_t next = modelReadU32(data, object);
                if (next == object || object == modelReadU32(data, body + 0x18))
                    break;
                object = next;
            }
            entry = modelReadU32(data, entry);
        }
        for (auto &bone : bones) {
            const auto parent = boneIndices.find(bone.parentOffset);
            if (parent != boneIndices.end())
                bone.parentIndex = parent->second;
        }
    }
    uint32_t blocks = 0, stripsTotal = 0;
    size_t acceptedEnd = 0;
    for (const auto &mesh : geometry) {
        const size_t header = mesh.offset;
        if (world && header < acceptedEnd)
            continue;
        if (modelReadU32(data, header + 64) == 0x50533244u) {
            const uint32_t materialBase = world ? 0 : uint32_t(materialTextureNames.size());
            if (!world) {
                if (mesh.count > 4096 || !modelRangeFits(mesh.materials, size_t(mesh.count) * 12, payloadEnd))
                    return false;
                for (uint32_t i = 0; i < mesh.count; ++i)
                    materialTextureNames.push_back(modelLowerAscii(
                        modelReadBoundedAscii(data, modelReadU32(data, mesh.materials + i * 12), 128)));
            }
            const uint32_t baseVertex = uint32_t(points.size());
            const auto found = matrices.find(mesh.bone);
            const std::array<float, 16> *rigid = found != matrices.end() ? &found->second : nullptr;
            size_t end = 0;
            if (!manhuntPs2Geometry(data, header + 64, payloadEnd, materialBase,
                                    world ? uint32_t(materialTextureNames.size()) : mesh.count, mesh.palette,
                                    rigid, points, triangles, texcoords, skinWeights, prelights, end))
                return false;
            while (pointFrameIndices.size() < points.size())
                pointFrameIndices.push_back(boneIndices.count(mesh.bone) ? boneIndices.at(mesh.bone)
                                                                         : 0xFFFFFFFFu);
            addField(fieldRows, "Geometry", "vertices", uint32_t(header),
                     uint32_t(points.size()) - baseVertex, "");
            manhuntPs2 = true;
            acceptedEnd = end;
            ++blocks;
            continue;
        }
        const uint32_t size = modelReadU32(data, header), flags = modelReadU32(data, header + 4);
        const uint32_t strips = modelReadU32(data, header + 8), vertices = modelReadU32(data, header + 0x2C),
                       buffer = modelReadU32(data, header + 0x40);
        PspNativeVertexLayout layout;
        if (!buildPspNativeVertexLayout(flags, layout) || !strips || strips > 4096 || !vertices ||
            vertices > 10000000)
            continue;
        if (buffer != 0x50 + strips * 0x20 || uint64_t(buffer) + uint64_t(vertices) * layout.stride != size ||
            !modelRangeFits(header, size, payloadEnd))
            continue;
        uint32_t sum = 0;
        bool valid = true;
        for (uint32_t i = 0; i < strips; ++i) {
            const uint32_t count = modelReadU16(data, header + 0x50 + i * 0x20 + 0x10);
            sum += count;
            if (!count || sum > vertices)
                valid = false;
        }
        if (!valid || sum != vertices)
            continue;
        const uint32_t materialBase = world ? 0 : uint32_t(materialTextureNames.size());
        if (!world) {
            if (mesh.count > 4096 || !modelRangeFits(mesh.materials, size_t(mesh.count) * 12, payloadEnd))
                return false;
            for (uint32_t i = 0; i < mesh.count; ++i)
                materialTextureNames.push_back(modelLowerAscii(
                    modelReadBoundedAscii(data, modelReadU32(data, mesh.materials + i * 12), 128)));
        }
        const float sx = modelReadF32(data, header + 0x20), sy = modelReadF32(data, header + 0x24),
                    sz = modelReadF32(data, header + 0x28);
        const float tx = modelReadF32(data, header + 0x30), ty = modelReadF32(data, header + 0x34),
                    tz = modelReadF32(data, header + 0x38);
        if (!std::isfinite(sx + sy + sz + tx + ty + tz))
            return false;
        uint32_t cursor = 0;
        for (uint32_t strip = 0; strip < strips; ++strip) {
            const size_t descriptor = header + 0x50 + strip * 0x20;
            const uint32_t count = modelReadU16(data, descriptor + 0x10);
            const uint32_t material = materialBase + modelReadU16(data, descriptor + 0x12);
            if (material >= materialTextureNames.size())
                return false;
            float us = modelReadF32(data, descriptor + 0x14),
                  vs = world ? modelReadF32(data, descriptor + 0x18) : us;
            if (!std::isfinite(us) || !std::isfinite(vs))
                return false;
            if (std::abs(us) < 1e-30f)
                us = 1;
            if (std::abs(vs) < 1e-30f)
                vs = us;
            const uint32_t base = uint32_t(points.size());
            for (uint32_t i = 0; i < count; ++i, ++cursor) {
                const size_t row = header + buffer + size_t(cursor) * layout.stride;
                const size_t position = row + layout.positionOffset;
                StorylandModelPoint point;
                if (layout.positionFormat == 2)
                    point = {modelReadI16(data, position) / 32768.f * sx + tx,
                             modelReadI16(data, position + 2) / 32768.f * sy + ty,
                             modelReadI16(data, position + 4) / 32768.f * sz + tz};
                else
                    point = {int8_t(data[position]) / 128.f * sx + tx,
                             int8_t(data[position + 1]) / 128.f * sy + ty,
                             int8_t(data[position + 2]) / 128.f * sz + tz};
                if (!world && layout.weightOffset == SIZE_MAX && matrices.count(mesh.bone))
                    point = transformMh2Point(point, matrices.at(mesh.bone).data());
                points.push_back(point);
                StorylandModelTexcoord uv;
                if (layout.uvOffset != SIZE_MAX) {
                    uv.u = data[row + layout.uvOffset] / 128.f * us;
                    uv.v = data[row + layout.uvOffset + 1] / 128.f * vs;
                }
                texcoords.push_back(uv);
                StorylandModelSkinWeights weights;
                if (layout.weightOffset != SIZE_MAX) {
                    float total = 0;
                    std::vector<std::pair<uint8_t, uint32_t>> order;
                    for (uint32_t j = 0; j < layout.numberOfWeights; ++j)
                        order.push_back({data[row + layout.weightOffset + j], j});
                    std::stable_sort(order.begin(), order.end(),
                                     [](const auto &a, const auto &b) { return a.first > b.first; });
                    for (const auto &item : order) {
                        if (!item.first || weights.influenceCount == 4)
                            continue;
                        const uint32_t token = data[descriptor + 0x18 + item.second];
                        const uint32_t id = token < mesh.palette.size() ? mesh.palette[token] : 0xFFFFFFFFu;
                        if (id >= bones.size())
                            continue;
                        auto &influence = weights.influences[weights.influenceCount++];
                        influence.boneIndex = id;
                        influence.rawMatrixIndex = id;
                        influence.rawPackedWord = item.first;
                        influence.weight = float(item.first) / 128.f;
                        total += influence.weight;
                    }
                    if (total > 0) {
                        weights.valid = true;
                        for (uint32_t j = 0; j < weights.influenceCount; ++j)
                            weights.influences[j].weight /= total;
                    }
                }
                skinWeights.push_back(weights);
                pointFrameIndices.push_back(boneIndices.count(mesh.bone) ? boneIndices.at(mesh.bone)
                                                                         : 0xFFFFFFFFu);
                StorylandModelPrelight color;
                if (layout.colorOffset != SIZE_MAX) {
                    const uint16_t packed = modelReadU16(data, row + layout.colorOffset);
                    color.red = uint8_t((packed & 31) * 255 / 31);
                    color.green = uint8_t(((packed >> 5) & 31) * 255 / 31);
                    color.blue = uint8_t(((packed >> 10) & 31) * 255 / 31);
                    color.alpha = (packed & 0x8000) ? 255 : 0;
                    color.fileOffset = uint32_t(row + layout.colorOffset);
                    color.encoding = StorylandPrelightEncoding::Psp5551;
                    color.valid = true;
                }
                prelights.push_back(color);
            }
            for (uint32_t i = 2; i < count; ++i) {
                uint32_t a = base + i - 2, b = base + i - 1, c = base + i;
                if (i & 1)
                    std::swap(a, b);
                const auto &p = points[a];
                const auto &q = points[b];
                const auto &r = points[c];
                const float nx = (q.y - p.y) * (r.z - p.z) - (q.z - p.z) * (r.y - p.y);
                const float ny = (q.z - p.z) * (r.x - p.x) - (q.x - p.x) * (r.z - p.z);
                const float nz = (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x);
                if (nx * nx + ny * ny + nz * nz > 1e-30f)
                    triangles.push_back({a, b, c, material});
            }
        }
        addField(fieldRows, "Geometry", "vertices", uint32_t(header + 0x2C), vertices, "");
        addField(fieldRows, "Geometry", "strips", uint32_t(header + 8), strips, "");
        acceptedEnd = header + size;
        ++blocks;
        stripsTotal += strips;
    }
    if (!blocks && !world)
        return false;
    if (world && !blocks) {
        const auto directory = std::filesystem::path(path).parent_path();
        std::error_code fileError;
        manhuntPs2 = std::filesystem::exists(directory / "TEXPS2.TXD", fileError);
        const std::string stem = modelLowerAscii(std::filesystem::path(path).stem().string());
        manhuntPc = !manhuntPs2 && stem.size() >= 3u &&
                    stem.compare(stem.size() - 3u, 3u, "_pc") == 0;
    }
    manhuntPsp = !manhuntPc;
    pmlcMdl = clump;
    previewUsesPspNativeSkinPalette = false;
    kind = (world || clumpEntries > 1)
               ? StorylandModelKind::WorldModel
               : (std::any_of(skinWeights.begin(), skinWeights.end(),
                              [](const StorylandModelSkinWeights &row) { return row.valid; })
                      ? StorylandModelKind::PedModel
                      : StorylandModelKind::SimpleModel);
    textureHints = materialTextureNames;
    outputLines.push_back({manhuntPc ? "Format: Manhunt 2 PC BSP"
                        : manhuntPs2 ? (world ? "Format: Manhunt 2 PS2 BSP" : "Format: Manhunt 2 PS2 DFF")
                                     : (world ? "Format: Manhunt 2 PSP BSP" : "Format: Manhunt 2 PSP DFF")});
    outputLines.push_back(
        {"Geometry blocks: " + std::to_string(blocks) + ", strips: " + std::to_string(stripsTotal)});
    outputLines.push_back(
        {"Vertices: " + std::to_string(points.size()) + ", triangles: " + std::to_string(triangles.size())});
    if (world && !blocks)
        outputLines.push_back({manhuntPc ? "PC BSP geometry streams are not yet decoded."
                                         : "No render geometry in this BSP."});
    return true;
}
