#pragma once

#include "src/leeds/leeds_archive.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

struct StorylandWorldDetailSource {
    const std::vector<StorylandWorldPlacement>* placements = nullptr;
    const std::vector<StorylandWorldMesh>* meshes = nullptr;
    bool authoredSectorLod = false;
};

struct StorylandNativeLodLink {
    uint32_t modelId = 0;
    uint32_t relatedModelId = 0;
    float maximumDistance = 0.0f;
    float detailDistance = 0.0f;
};

inline std::vector<StorylandNativeLodLink> storylandReadNativeLodLinks(const std::vector<uint8_t>& data) {
    std::vector<StorylandNativeLodLink> links;
    if (data.size() < 0x40u ||
        (std::memcmp(data.data(), "GATG", 4) != 0 &&
         std::memcmp(data.data(), "GTAG", 4) != 0)) return links;

    auto word = [&](size_t offset) -> uint32_t {
        uint32_t value = 0;
        std::memcpy(&value, data.data() + offset, sizeof(value));
        return value;
    };
    auto scalar = [&](size_t offset) -> float {
        float value = 0.0f;
        std::memcpy(&value, data.data() + offset, sizeof(value));
        return value;
    };

    const uint32_t count = word(0x38u);
    const uint32_t table = word(0x3Cu);
    if (!count || count > 200000 || !table ||
        uint64_t(table) + uint64_t(count) * 4u > data.size()) return links;

    std::unordered_map<uint32_t, uint32_t> ids;
    ids.reserve(count);
    std::vector<uint32_t> pointers(count, 0);
    for (uint32_t index = 0; index < count; ++index) {
        const uint32_t pointer = word(size_t(table) + size_t(index) * 4u);
        if (!pointer || uint64_t(pointer) + 0x40u > data.size()) continue;
        pointers[index] = pointer;
        auto inserted = ids.emplace(pointer, index);
        if (!inserted.second) inserted.first->second = UINT32_MAX;
    }
    struct NativeLayout {
        size_t distanceOffset;
        size_t flagsOffset;
        size_t relatedOffset;
    };
    const NativeLayout layouts[] = {
        {0x28u, 0x36u, 0x38u}, // LCS PS2
        {0x2Cu, 0x3Au, 0x3Cu}  // VCS PS2
    };
    NativeLayout layout = layouts[0];
    size_t strongestEvidence = 0;
    for (const auto& candidate : layouts) {
        size_t verified = 0;
        for (uint32_t index = 0; index < count; ++index) {
            const uint32_t pointer = pointers[index];
            if (!pointer || ids[pointer] != index ||
                (data[pointer + 0x10u] != 1 && data[pointer + 0x10u] != 3)) continue;
            uint16_t flags = 0;
            std::memcpy(&flags, data.data() + pointer + candidate.flagsOffset, sizeof(flags));
            if ((flags & 0x10u) == 0u) continue;
            const auto related = ids.find(word(pointer + candidate.relatedOffset));
            if (related != ids.end() && related->second != UINT32_MAX &&
                related->second != index &&
                (data[related->first + 0x10u] == 1 || data[related->first + 0x10u] == 3))
                ++verified;
        }
        if (verified > strongestEvidence) {
            strongestEvidence = verified;
            layout = candidate;
        }
    }
    if (strongestEvidence == 0) return links;

    for (uint32_t index = 0; index < count; ++index) {
        const uint32_t pointer = pointers[index];
        if (!pointer || ids[pointer] != index ||
            (data[pointer + 0x10u] != 1 && data[pointer + 0x10u] != 3)) continue;
        uint16_t flags = 0;
        std::memcpy(&flags, data.data() + pointer + layout.flagsOffset, sizeof(flags));
        if ((flags & 0x10u) == 0u) continue;
        const uint32_t related = word(pointer + layout.relatedOffset);
        const auto found = ids.find(related);
        if (!related || found == ids.end() || found->second == UINT32_MAX ||
            found->second == index ||
            (data[related + 0x10u] != 1 && data[related + 0x10u] != 3)) continue;
        const float maximum = scalar(pointer + layout.distanceOffset);
        const float detail = scalar(pointer + layout.distanceOffset + 8u);
        if (!std::isfinite(maximum) || !std::isfinite(detail) ||
            maximum < 0.0f || maximum > 10000.0f ||
            detail < 0.0f || detail > 10000.0f) continue;
        links.push_back({index, found->second, maximum, detail});
    }
    return links;
}

// Compiled WRLD pass membership is not a distance test: a sector can contain
// NORMAL, LOD and SUPERLOD instances simultaneously. Draw exactly the range
// intended for each pass rather than submitting all three to the renderer.
// Native DTZ instances carry their own independent min/max draw distances.
inline uint8_t storylandAuthoredWorldPass(const std::string& pass) {
    if (pass == "SUPERLOD") return 1;
    if (pass == "LOD") return 2;
    if (pass == "ROADS" || pass == "UNDERWATER") return 4;
    if (pass == "NORMAL") return 3;
    return 5;
}

inline bool storylandAuthoredWorldPassVisible(uint8_t pass, double distance,
    double nearDistance, double farDistance) {
    if (!std::isfinite(distance) || !std::isfinite(nearDistance) ||
        !std::isfinite(farDistance) || farDistance <= 0.0 ||
        distance < 0.0 || distance > farDistance) return false;
    const double detailLimit = std::min(farDistance, std::max(100.0, nearDistance));
    const double superStart = std::min(farDistance,
        std::max(detailLimit * 2.0, farDistance * 0.55));
    if (pass == 1) return distance >= superStart;
    if (pass == 2) return distance >= detailLimit && distance < superStart;
    if (pass == 4) return distance < std::min(farDistance, detailLimit * 2.6);
    if (pass == 5) return distance < std::min(farDistance, detailLimit * 1.8);
    return distance < detailLimit;
}

// Both games' selected WRLD and resident Resource[] already choose each pass.
// Native DTZ placements use their model-info draw distances.
inline bool storylandAuthoredWorldRangeRequired(bool compiledSectorVisibility,
    bool nativePlacement, bool nativeSectorSelection = false) {
    return !nativeSectorSelection && !compiledSectorVisibility && !nativePlacement;
}

struct StorylandWorldDetailObject {
    StorylandMeshBounds bounds;
    size_t sourceIndex = 0;
    int32_t nativeModelInfoId = -1;
    unsigned rank = 3;
    bool available = false;
    bool independentPass = false;
    bool nativeBound = false;
    bool authoredSectorLod = false;
    bool compiledSectorVisibility = false;
    bool nativeSectorSelection = false;
    bool nativePlacement = false;
    uint8_t authoredPass = 3;
    double detailDistance = 300.0;
    double maximumDistance = 2000.0;
    std::vector<size_t> replacements;
    std::vector<size_t> finer;
};

// Keep the same choice of detail for the world render and its shadow pass.
struct StorylandWorldDetailPlan {
    std::vector<StorylandWorldDetailObject> objects;
    std::vector<size_t> sourceOffsets;
    size_t nativeBindings = 0;
    size_t footprintFallbacks = 0;

    static double distance(const StorylandMeshBounds& bounds, const std::array<double, 3>& position) {
        double squared = 0.0;
        for (unsigned axis = 0; axis < 3; ++axis) {
            const double gap = std::max({bounds.minimum[axis] - position[axis], 0.0,
                                         position[axis] - bounds.maximum[axis]});
            squared += gap * gap;
        }
        return std::sqrt(squared);
    }

    void build(const std::vector<StorylandWorldPlacement>& placements,
               const std::vector<StorylandWorldMesh>& meshes) {
        build({{&placements, &meshes}}, {});
    }

    void build(const std::vector<StorylandWorldDetailSource>& sources,
               const std::vector<uint8_t>& nativeDtz) {
        objects.clear();
        sourceOffsets.clear();
        nativeBindings = footprintFallbacks = 0;
        size_t placementCount=0;
        for(const auto& source:sources)if(source.placements)placementCount+=source.placements->size();
        objects.reserve(placementCount);sourceOffsets.reserve(sources.size()+1);

        for (size_t sourceIndex = 0; sourceIndex < sources.size(); ++sourceIndex) {
            const auto& source = sources[sourceIndex];
            sourceOffsets.push_back(objects.size());
            if (!source.placements || !source.meshes) continue;
            std::unordered_map<uint64_t, const StorylandWorldMesh*> meshes;
            meshes.reserve(source.meshes->size());
            for (const auto& mesh : *source.meshes)
                meshes[(uint64_t(mesh.sectorIndex) << 32) | mesh.resourceIndex] = &mesh;

            for (const auto& placement : *source.placements) {
                StorylandWorldDetailObject object;
                object.sourceIndex = sourceIndex;
                object.authoredSectorLod = source.authoredSectorLod;
                object.compiledSectorVisibility = placement.compiledSectorVisibility;
                object.nativeSectorSelection = placement.nativeSectorSelection;
                object.nativePlacement = placement.nativeInstanceIndex != UINT32_MAX;
                object.authoredPass = storylandAuthoredWorldPass(placement.passName);
                object.rank = placement.passName == "SUPERLOD" ? 1u :
                              placement.passName == "LOD" ? 2u : 3u;
                object.independentPass = object.rank == 3u && placement.passName != "NORMAL";
                const auto found = meshes.find((uint64_t(placement.sectorIndex) << 32) |
                                               placement.resourceIndex);
                if (found != meshes.end()) {
                    const auto& mesh = *found->second;
                    if (mesh.bounds.valid && !mesh.vertices.empty() && !mesh.triangles.empty()) {
                        object.nativeModelInfoId = mesh.nativeModelInfoId;
                        // Affine interval transform: the same box as eight
                        // transformed corners, with each matrix term used once.
                        object.bounds.valid=true;
                        for(unsigned row=0;row<3;++row) {
                            double minimum=placement.matrix[12+row],maximum=minimum;
                            for(unsigned column=0;column<3;++column) {
                                const double coefficient=placement.matrix[column*4+row];
                                const double first=coefficient*mesh.bounds.minimum[column];
                                const double second=coefficient*mesh.bounds.maximum[column];
                                minimum+=std::min(first,second);maximum+=std::max(first,second);
                            }
                            object.bounds.minimum[row]=minimum;object.bounds.maximum[row]=maximum;
                            if(!std::isfinite(minimum)||!std::isfinite(maximum))object.bounds.valid=false;
                        }
                        object.available = object.bounds.valid;
                    }
                }
                objects.push_back(std::move(object));
            }
        }
        sourceOffsets.push_back(objects.size());
        if (std::all_of(objects.begin(), objects.end(),
            [](const StorylandWorldDetailObject& object) { return object.authoredSectorLod; })) return;

        std::unordered_map<int32_t, std::vector<size_t>> byNativeId;
        byNativeId.reserve(objects.size());
        for (size_t index = 0; index < objects.size(); ++index)
            if (objects[index].available && !objects[index].authoredSectorLod &&
                objects[index].nativeModelInfoId >= 0)
                byNativeId[objects[index].nativeModelInfoId].push_back(index);

        auto coverage = [](const StorylandMeshBounds& fine, const StorylandMeshBounds& coarse) {
            double commonArea = 1.0;
            double fineArea = 1.0;
            for (unsigned axis = 0; axis < 2; ++axis) {
                fineArea *= std::max(0.0, fine.maximum[axis] - fine.minimum[axis]);
                commonArea *= std::max(0.0,
                    std::min(fine.maximum[axis], coarse.maximum[axis]) -
                    std::max(fine.minimum[axis], coarse.minimum[axis]));
            }
            return fineArea > 0.0001 ? commonArea / fineArea : 0.0;
        };

        auto bind = [&](size_t fine, size_t coarse, bool native) {
            auto& detailed = objects[fine];
            auto& distant = objects[coarse];
            if (std::find(detailed.replacements.begin(), detailed.replacements.end(), coarse) !=
                detailed.replacements.end()) return;
            detailed.replacements.push_back(coarse);
            distant.finer.push_back(fine);
            if (native) {
                detailed.nativeBound = distant.nativeBound = true;
                ++nativeBindings;
            } else ++footprintFallbacks;
        };

        const auto nativeLinks = byNativeId.empty() ? std::vector<StorylandNativeLodLink>{} :
            storylandReadNativeLodLinks(nativeDtz);
        for (const auto& link : nativeLinks) {
            const auto left = byNativeId.find(int32_t(link.modelId));
            const auto right = byNativeId.find(int32_t(link.relatedModelId));
            if (left == byNativeId.end() || right == byNativeId.end()) continue;

            for (size_t leftIndex : left->second) {
                const auto& first = objects[leftIndex];
                size_t bestIndex = SIZE_MAX;
                double bestScore = std::numeric_limits<double>::infinity();
                double secondScore = bestScore;
                for (size_t rightIndex : right->second) {
                    const auto& second = objects[rightIndex];
                    if (first.rank == second.rank) continue;
                    const size_t fineIndex = first.rank > second.rank ? leftIndex : rightIndex;
                    const size_t coarseIndex = first.rank > second.rank ? rightIndex : leftIndex;
                    if (objects[fineIndex].independentPass) continue;
                    const auto& fine = objects[fineIndex].bounds;
                    const auto& coarse = objects[coarseIndex].bounds;
                    const double covered = coverage(fine, coarse);
                    if (covered < 0.35) continue;
                    const double span = std::max(1.0, std::hypot(
                        fine.maximum[0] - fine.minimum[0], fine.maximum[1] - fine.minimum[1]));
                    const double centerDistance = std::hypot(
                        (fine.minimum[0] + fine.maximum[0] - coarse.minimum[0] - coarse.maximum[0]) * 0.5,
                        (fine.minimum[1] + fine.maximum[1] - coarse.minimum[1] - coarse.maximum[1]) * 0.5);
                    const double score = (1.0 - covered) + centerDistance / span;
                    if (score < bestScore) {
                        secondScore = bestScore;
                        bestScore = score;
                        bestIndex = rightIndex;
                    } else if (score < secondScore) secondScore = score;
                }
                // Reused native model IDs require a unique placement match.
                if (bestIndex == SIZE_MAX || secondScore - bestScore < 0.05) continue;
                const size_t fine = first.rank > objects[bestIndex].rank ? leftIndex : bestIndex;
                const size_t coarse = fine == leftIndex ? bestIndex : leftIndex;
                bind(fine, coarse, true);
                if (link.detailDistance >= 10.0f && link.detailDistance <= 1500.0f)
                    objects[fine].detailDistance = link.detailDistance;
                if (link.maximumDistance >= 100.0f)
                    objects[coarse].maximumDistance = link.maximumDistance;
            }
        }

        // Sector-local and LCS resources may lack model-info identity.  Only
        // these retain footprint fallback, indexed by geographic cells.
        using Cell = std::pair<int32_t, int32_t>;
        std::map<Cell, std::vector<size_t>> coarseCells;
        constexpr double cellSize = 256.0;
        const auto cellCoordinate = [&](double position, int32_t& coordinate) {
            const double value = std::floor(position / cellSize);
            if (!std::isfinite(value) ||
                value < double((std::numeric_limits<int32_t>::min)()) ||
                value > double((std::numeric_limits<int32_t>::max)())) return false;
            coordinate = int32_t(value);
            return true;
        };
        for (size_t index = 0; index < objects.size(); ++index) {
            const auto& object = objects[index];
            if (!object.available || object.authoredSectorLod ||
                object.rank >= 3 || object.nativeModelInfoId >= 0) continue;
            int32_t minX, maxX, minY, maxY;
            if (!cellCoordinate(object.bounds.minimum[0], minX) ||
                !cellCoordinate(object.bounds.maximum[0], maxX) ||
                !cellCoordinate(object.bounds.minimum[1], minY) ||
                !cellCoordinate(object.bounds.maximum[1], maxY)) continue;
            const int64_t width = int64_t(maxX) - minX + 1;
            const int64_t height = int64_t(maxY) - minY + 1;
            if (width <= 0 || height <= 0 || width > 128 || height > 128 ||
                width * height > 128) continue;
            for (int64_t x = minX; x <= maxX; ++x)
                for (int64_t y = minY; y <= maxY; ++y)
                    coarseCells[{int32_t(x), int32_t(y)}].push_back(index);
        }
        for (size_t index = 0; index < objects.size(); ++index) {
            const auto& object = objects[index];
            if (!object.available || object.authoredSectorLod || object.rank <= 1 ||
                object.independentPass || object.nativeModelInfoId >= 0 || object.nativeBound) continue;
            const double x = (object.bounds.minimum[0] + object.bounds.maximum[0]) * 0.5;
            const double y = (object.bounds.minimum[1] + object.bounds.maximum[1]) * 0.5;
            int32_t cellX, cellY;
            if (!cellCoordinate(x, cellX) || !cellCoordinate(y, cellY)) continue;
            const auto cell = coarseCells.find({cellX, cellY});
            if (cell == coarseCells.end()) continue;
            auto bestContaining = [&](unsigned rank) -> size_t {
                size_t best = SIZE_MAX;
                double bestArea = std::numeric_limits<double>::infinity();
                double secondArea = bestArea;
                for (size_t candidate : cell->second) {
                    const auto& distant = objects[candidate];
                    if (distant.rank != rank || distant.sourceIndex != object.sourceIndex ||
                        distant.nativeBound) continue;
                    // Retail coarse meshes are simplified silhouettes, not
                    // enclosing boxes. The airport tower LOD is slightly
                    // narrower than its detailed tower; strict containment
                    // left both unlinked and rendered them together at distance.
                    const double covered=coverage(object.bounds,distant.bounds);
                    if(covered<0.85)continue;
                    const double height=object.bounds.maximum[2]-object.bounds.minimum[2];
                    const double verticalOverlap=std::max(0.0,
                        std::min(object.bounds.maximum[2],distant.bounds.maximum[2])-
                        std::max(object.bounds.minimum[2],distant.bounds.minimum[2]));
                    if(height>1.0&&verticalOverlap<height*.70)continue;
                    const double area = std::max(0.0, distant.bounds.maximum[0] - distant.bounds.minimum[0]) *
                                        std::max(0.0, distant.bounds.maximum[1] - distant.bounds.minimum[1]);
                    if (area < bestArea) {
                        secondArea = bestArea;
                        bestArea = area;
                        best = candidate;
                    } else if (area < secondArea) secondArea = area;
                }
                // Repeated cell entries and overlapping district LODs do not
                // prove which parent a particular detailed instance belongs to.
                if (best == SIZE_MAX || secondArea - bestArea <= bestArea * 0.02) return SIZE_MAX;
                return best;
            };
            size_t replacement = bestContaining(object.rank - 1u);
            if (replacement == SIZE_MAX && object.rank == 3u)
                replacement = bestContaining(1u);
            if (replacement != SIZE_MAX) bind(index, replacement, false);
        }
    }

    std::vector<uint8_t> select(const std::array<double, 3>& position,
                                double nearDistance = 300.0, double farDistance = 2000.0,
                                const std::vector<uint8_t>& visible = {}) const {
        std::vector<uint8_t> selected(objects.size(), 0);
        std::vector<double> distances;
        const bool finitePosition = std::all_of(position.begin(), position.end(),
            [](double value) { return std::isfinite(value); });
        for (size_t index = 0; index < objects.size(); ++index) {
            const auto& object = objects[index];
            if (!object.authoredSectorLod && distances.empty())
                distances.assign(objects.size(), std::numeric_limits<double>::infinity());
            if (!object.available || (!visible.empty() &&
                (index >= visible.size() || !visible[index]))) continue;
            if (object.authoredSectorLod) {
                const bool rangeRequired = storylandAuthoredWorldRangeRequired(
                    object.compiledSectorVisibility, object.nativePlacement, object.nativeSectorSelection);
                selected[index] = finitePosition &&
                    (!rangeRequired || storylandAuthoredWorldPassVisible(
                        object.authoredPass, distance(object.bounds, position),
                        nearDistance, farDistance));
            } else distances[index] = distance(object.bounds, position);
        }
        if (distances.empty()) return selected;

        // A hidden/swapped detailed instance must not reappear as its LOD.
        // Propagate visibility (not draw distance) through the replacement chain.
        if (!visible.empty()) for (unsigned rank : {2u, 1u}) {
            for (size_t index = 0; index < objects.size(); ++index) {
                const auto& object = objects[index];
                if (object.authoredSectorLod || object.rank != rank) continue;
                bool resident = false, enabled = false;
                for (size_t finer : object.finer) if (objects[finer].available) {
                    resident = true;
                    enabled |= std::isfinite(distances[finer]);
                }
                if (resident && !enabled) distances[index] = std::numeric_limits<double>::infinity();
            }
        }

        for (unsigned rank : {3u, 2u, 1u}) {
            for (size_t index = 0; index < objects.size(); ++index) {
                const auto& object = objects[index];
                if (!object.available || object.authoredSectorLod || object.rank != rank ||
                    distances[index] > std::min(farDistance, object.maximumDistance)) continue;
                bool finerVisible = false;
                for (size_t finer : object.finer) {
                    if (selected[finer]) finerVisible = true;
                    for (size_t descendant : objects[finer].finer)
                        if (selected[descendant]) finerVisible = true;
                    if (finerVisible) break;
                }
                if (finerVisible) continue;
                if (rank == 3) {
                    selected[index] = object.independentPass || distances[index] <=
                        (object.nativeBound ? object.detailDistance : nearDistance);
                } else if (rank == 2 && !object.nativeBound &&
                           !object.replacements.empty() && distances[index] > farDistance * 0.5) {
                    selected[index] = 0;
                } else if (rank == 2 && object.nativeBound &&
                           !object.replacements.empty()) {
                    selected[index] = distances[index] <= object.detailDistance;
                } else if (!object.finer.empty()) {
                    selected[index] = 1;
                } else {
                    selected[index] = distances[index] >
                        (rank == 2 ? nearDistance : farDistance * 0.5);
                }
            }
        }

        return selected;
    }
};
