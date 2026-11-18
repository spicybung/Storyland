#pragma once

#include "src/leeds/leeds_archive.h"
#include "src/leeds/leeds_world_texture.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

struct StorylandWorldFrameMesh {
    uint64_t key = 0;
    const StorylandWorldMesh* draw = nullptr;
    const StorylandWorldMesh* detail = nullptr;
};

struct StorylandWorldFrameTextureGroup {
    int32_t material = -1;
    size_t begin = 0;
    size_t end = 0;
};

struct StorylandWorldFrameResources {
    std::vector<StorylandWorldFrameMesh> meshes;
    std::vector<const StorylandDirectTextureResource*> textures;
    std::vector<StorylandWorldFrameTextureGroup> textureGroups;
    std::vector<size_t> meshSlots;
    std::vector<size_t> textureSlots;

    static uint64_t meshKey(uint32_t sector, uint32_t resource) {
        return (uint64_t(sector) << 32) | resource;
    }

    static size_t slotCount(size_t count) {
        size_t capacity = 1;
        while (capacity / 2 < count) {
            if (capacity > std::numeric_limits<size_t>::max() / 2)
                throw std::length_error("World frame resource count exceeds addressable memory.");
            capacity *= 2;
        }
        return capacity;
    }

    static size_t hashKey(uint64_t key) {
        key ^= key >> 33;
        key *= 0xFF51AFD7ED558CCDull;
        key ^= key >> 33;
        key *= 0xC4CEB9FE1A85EC53ull;
        key ^= key >> 33;
        return size_t(key);
    }

    static bool textureAvailable(const StorylandDirectTextureResource& texture, bool decodedTexturesOnly) {
        return texture.materialId >= 0 && (!decodedTexturesOnly ||
            (texture.width > 0 && texture.height > 0 && !texture.rgba.empty()));
    }

    void build(const std::vector<StorylandWorldMesh>& worldMeshes,
        const std::vector<StorylandDirectTextureResource>& directTextures, bool decodedTexturesOnly = false) {
        meshes.clear();
        meshes.reserve(worldMeshes.size());
        meshSlots.assign(worldMeshes.empty() ? 0 : slotCount(worldMeshes.size()), SIZE_MAX);
        for (const auto& mesh : worldMeshes) {
            const uint64_t key = meshKey(mesh.sectorIndex, mesh.resourceIndex);
            size_t slot = hashKey(key) & (meshSlots.size() - 1);
            while (meshSlots[slot] != SIZE_MAX && meshes[meshSlots[slot]].key != key)
                slot = (slot + 1) & (meshSlots.size() - 1);
            if (meshSlots[slot] == SIZE_MAX) {
                meshSlots[slot] = meshes.size();
                meshes.push_back({key, &mesh, &mesh});
            } else {
                meshes[meshSlots[slot]].detail = &mesh;
            }
        }

        textureGroups.clear();
        textureGroups.reserve(directTextures.size());
        textureSlots.assign(directTextures.empty() ? 0 : slotCount(directTextures.size()), SIZE_MAX);
        for (const auto& texture : directTextures) {
            if (!textureAvailable(texture, decodedTexturesOnly)) continue;
            size_t slot = hashKey(uint32_t(texture.materialId)) & (textureSlots.size() - 1);
            while (textureSlots[slot] != SIZE_MAX &&
                textureGroups[textureSlots[slot]].material != texture.materialId)
                slot = (slot + 1) & (textureSlots.size() - 1);
            if (textureSlots[slot] == SIZE_MAX) {
                textureSlots[slot] = textureGroups.size();
                textureGroups.push_back({texture.materialId, 0, 1});
            } else {
                ++textureGroups[textureSlots[slot]].end;
            }
        }
        size_t offset = 0;
        for (auto& group : textureGroups) {
            const size_t count = group.end;
            group.begin = group.end = offset;
            offset += count;
        }
        textures.resize(offset);
        for (const auto& texture : directTextures) {
            if (!textureAvailable(texture, decodedTexturesOnly)) continue;
            auto& group = textureGroups[textureGroupIndex(texture.materialId)];
            textures[group.end++] = &texture;
        }
    }

    const StorylandWorldFrameMesh* findMesh(const StorylandWorldPlacement& placement) const {
        if (meshSlots.empty()) return nullptr;
        const uint64_t key = meshKey(placement.sectorIndex, placement.resourceIndex);
        size_t slot = hashKey(key) & (meshSlots.size() - 1);
        while (meshSlots[slot] != SIZE_MAX) {
            const auto& mesh = meshes[meshSlots[slot]];
            if (mesh.key == key) return &mesh;
            slot = (slot + 1) & (meshSlots.size() - 1);
        }
        return nullptr;
    }

    size_t textureGroupIndex(int32_t material) const {
        if (textureSlots.empty()) return SIZE_MAX;
        size_t slot = hashKey(uint32_t(material)) & (textureSlots.size() - 1);
        while (textureSlots[slot] != SIZE_MAX) {
            const size_t index = textureSlots[slot];
            if (textureGroups[index].material == material) return index;
            slot = (slot + 1) & (textureSlots.size() - 1);
        }
        return SIZE_MAX;
    }

    const StorylandDirectTextureResource* resolveTexture(int32_t material,
        const StorylandWorldMesh& mesh, const StorylandWorldPlacement& placement) const {
        const size_t index = textureGroupIndex(material);
        if (index == SIZE_MAX) return nullptr;
        const auto& group = textureGroups[index];
        return storylandResolveWorldTexture(textures.begin() + group.begin,
            textures.begin() + group.end, mesh, placement);
    }

};

struct StorylandWorldFramePlacement {
    size_t area = 0;
    size_t index = 0;
};

template<class Sources>
std::array<std::vector<StorylandWorldFramePlacement>, 4> storylandWorldRenderPhases(
    const Sources& sources, const std::vector<size_t>& offsets,
    const std::vector<uint8_t>& selected) {
    std::array<std::vector<StorylandWorldFramePlacement>, 4> phases;
    for (size_t area = 0; area < sources.size(); ++area) {
        if (!sources[area].placements) continue;
        const auto& placements = *sources[area].placements;
        for (size_t index = 0; index < placements.size(); ++index) {
            if (offsets[area] + index >= selected.size() || !selected[offsets[area] + index]) continue;
            const auto& pass = placements[index].passName;
            const unsigned phase = pass == "LIGHTS" ? 3u : pass == "TRANSPARENT" ? 2u :
                pass == "NOZWRITE" ? 1u : 0u;
            phases[phase].push_back({area, index});
        }
    }
    return phases;
}
