#pragma once

#include "src/storyland/storyland_world_detail.h"

inline std::vector<size_t> storylandWorldDetailSourceOffsets(
    const std::vector<StorylandWorldDetailSource>& sources) {
    std::vector<size_t> offsets;
    offsets.reserve(sources.size() + 1);
    size_t count = 0;
    for (const auto& source : sources) {
        offsets.push_back(count);
        if (source.placements && source.meshes) count += source.placements->size();
    }
    offsets.push_back(count);
    return offsets;
}

inline bool storylandWorldDetailPlacementBounds(const StorylandWorldPlacement& placement,
    const StorylandWorldMesh* mesh, StorylandMeshBounds& bounds) {
    bounds = {};
    if (!mesh || !mesh->bounds.valid || mesh->vertices.empty() || mesh->triangles.empty()) return false;
    for (unsigned row = 0; row < 3; ++row) {
        double minimum = placement.matrix[12 + row];
        double maximum = minimum;
        for (unsigned column = 0; column < 3; ++column) {
            const double coefficient = placement.matrix[column * 4 + row];
            const double first = coefficient * mesh->bounds.minimum[column];
            const double second = coefficient * mesh->bounds.maximum[column];
            minimum += std::min(first, second);
            maximum += std::max(first, second);
        }
        if (!std::isfinite(minimum) || !std::isfinite(maximum)) return false;
        bounds.minimum[row] = minimum;
        bounds.maximum[row] = maximum;
    }
    bounds.valid = true;
    return true;
}

inline bool storylandWorldDetailPlacementAvailable(const StorylandWorldPlacement& placement,
    const StorylandWorldMesh* mesh) {
    StorylandMeshBounds bounds;
    return storylandWorldDetailPlacementBounds(placement, mesh, bounds);
}

template<class FindMesh>
bool storylandSelectAuthoredWorldDetails(const std::vector<StorylandWorldDetailSource>& sources,
    const std::array<double, 3>& position, const std::vector<uint8_t>& visible,
    const FindMesh& findMesh, std::vector<uint8_t>& selected,
    double nearDistance = 300.0, double farDistance = 500.0) {
    for (const auto& source : sources)
        if (source.placements && source.meshes && !source.placements->empty() &&
            !source.authoredSectorLod) return false;

    const auto offsets = storylandWorldDetailSourceOffsets(sources);
    selected.assign(offsets.back(), 0);
    if (!std::all_of(position.begin(), position.end(),
        [](double value) { return std::isfinite(value); })) return true;

    for (size_t sourceIndex = 0; sourceIndex < sources.size(); ++sourceIndex) {
        const auto& source = sources[sourceIndex];
        if (!source.placements || !source.meshes) continue;
        for (size_t index = 0; index < source.placements->size(); ++index) {
            const size_t target = offsets[sourceIndex] + index;
            if (!visible.empty() && (target >= visible.size() || !visible[target])) continue;
            const auto& placement = (*source.placements)[index];
            StorylandMeshBounds bounds;
            if (!storylandWorldDetailPlacementBounds(placement,
                findMesh(sourceIndex, placement), bounds)) continue;
            const bool nativePlacement = placement.nativeInstanceIndex != UINT32_MAX;
            const bool rangeRequired = storylandAuthoredWorldRangeRequired(
                placement.compiledSectorVisibility, nativePlacement, placement.nativeSectorSelection);
            selected[target] = !rangeRequired ||
                storylandAuthoredWorldPassVisible(storylandAuthoredWorldPass(placement.passName),
                    StorylandWorldDetailPlan::distance(bounds, position),
                    nearDistance, farDistance);
        }
    }
    return true;
}
