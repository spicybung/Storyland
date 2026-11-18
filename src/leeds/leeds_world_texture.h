#pragma once
#include "src/leeds/leeds_archive.h"

template<class Iterator>
const StorylandDirectTextureResource* storylandResolveWorldTexture(
    Iterator first, Iterator last,
    const StorylandWorldMesh& mesh,
    const StorylandWorldPlacement& placement
) {
    if (first == last) return nullptr;

    // A resource ID is meaningful within its owner table, not across all IMG chunks.
    // Prefer the exact allocation owner of the mesh when available.
    if (mesh.textureScopeOffset != uint64_t(-1)) {
        for (auto candidate = first; candidate != last; ++candidate) {
            const auto* texture = *candidate;
            if (texture != nullptr &&
                texture->storedInImg == mesh.textureScopeInImg &&
                uint64_t(texture->baseOffset) == mesh.textureScopeOffset) {
                return texture;
            }
        }
    }

    // Sector-local resources take priority over resources with the same ID
    // from other streamed sectors.
    if (mesh.sectorIndex != 0xFFFFFFFFu) {
        for (auto candidate = first; candidate != last; ++candidate) {
            const auto* texture = *candidate;
            if (texture != nullptr && texture->storedInImg &&
                texture->scopeSectorIndex == mesh.sectorIndex) {
                return texture;
            }
        }
    }

    // AREA resources are only shared within their actual streaming cell.
    const int32_t cellX = mesh.hasTextureScopeCell ?
        mesh.textureScopeCellX : int32_t(placement.sectorX / 3u);
    const int32_t cellY = mesh.hasTextureScopeCell ?
        mesh.textureScopeCellY : int32_t(placement.sectorY / 3u);
    if (mesh.hasTextureScopeCell || mesh.sectorIndex != 0xFFFFFFFFu) {
        for (auto candidate = first; candidate != last; ++candidate) {
            const auto* texture = *candidate;
            if (texture != nullptr && texture->hasScopeCell &&
                texture->scopeCellX == cellX && texture->scopeCellY == cellY) {
                return texture;
            }
        }
    }

    // Master LVZ resources are valid fallbacks for streamed meshes, but only
    // when a unique master binding exists. Do not use unrelated sector IDs.
    const StorylandDirectTextureResource* master = nullptr;
    for (auto candidate = first; candidate != last; ++candidate) {
        const auto* texture = *candidate;
        if (texture == nullptr || texture->storedInImg ||
            texture->hasScopeCell || texture->scopeSectorIndex != 0xFFFFFFFFu ||
            texture->baseOffset != 0u) continue;
        if (master != nullptr && (master->rgba != texture->rgba ||
            master->width != texture->width || master->height != texture->height)) {
            master = nullptr;
            break;
        }
        master = texture;
    }
    if (master != nullptr) return master;

    // Copies are interchangeable only when their decoded pixels are identical.
    const auto* firstCandidate = *first;
    if (firstCandidate != nullptr && firstCandidate->unambiguousMaterialBinding) {
        return firstCandidate;
    }

    // Never bind a local ID from an unrelated container simply because it is
    // the sole candidate carrying that number.
    auto second = first;
    ++second;
    if (second == last && firstCandidate != nullptr &&
        !firstCandidate->storedInImg && !firstCandidate->hasScopeCell &&
        firstCandidate->scopeSectorIndex == 0xFFFFFFFFu) {
        return firstCandidate;
    }
    return nullptr;
}

inline const StorylandDirectTextureResource* storylandResolveWorldTexture(
    const std::vector<const StorylandDirectTextureResource*>& candidates,
    const StorylandWorldMesh& mesh,
    const StorylandWorldPlacement& placement
) {
    return storylandResolveWorldTexture(candidates.begin(), candidates.end(), mesh, placement);
}
