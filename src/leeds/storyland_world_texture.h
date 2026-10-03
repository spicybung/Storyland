#pragma once
#include "storyland_archive.h"

inline const StorylandDirectTextureResource* storylandResolveWorldTexture(
    const std::vector<const StorylandDirectTextureResource*>& candidates,
    const StorylandWorldMesh& mesh,
    const StorylandWorldPlacement& placement
) {
    if (mesh.textureScopeOffset != uint64_t(-1)) {
        for (const auto* texture : candidates) {
            if (texture && texture->storedInImg == mesh.textureScopeInImg &&
                uint64_t(texture->baseOffset) == mesh.textureScopeOffset) return texture;
        }
    }
    for (const auto* texture : candidates) {
        if (texture && texture->storedInImg &&
            texture->scopeSectorIndex == mesh.sectorIndex &&
            mesh.sectorIndex != 0xFFFFFFFFu) return texture;
    }
    const int32_t cellX = mesh.hasTextureScopeCell ? mesh.textureScopeCellX : int32_t(placement.sectorX / 3u);
    const int32_t cellY = mesh.hasTextureScopeCell ? mesh.textureScopeCellY : int32_t(placement.sectorY / 3u);
    for (const auto* texture : candidates) {
        if (texture && texture->hasScopeCell &&
            texture->scopeCellX == cellX && texture->scopeCellY == cellY) return texture;
    }
    for (const auto* texture : candidates) {
        if (texture && !texture->storedInImg && texture->baseOffset == 0u) return texture;
    }
    // Identical copies in multiple containers are the same material content.
    if (!candidates.empty() && candidates.front() && candidates.front()->unambiguousMaterialBinding) {
        return candidates.front();
    }
    // Only an unscoped unique binding can be reused without container evidence.
    if (candidates.size() == 1u && candidates.front() &&
        !candidates.front()->hasScopeCell && candidates.front()->scopeSectorIndex == 0xFFFFFFFFu) {
        return candidates.front();
    }
    return nullptr;
}
