#include "src/leeds/leeds_ctw_city.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace storyland_ctw {
namespace {

constexpr unsigned cityGridWidth = 59;
constexpr unsigned cityGridHeight = 42;
// Each map entry covers four 60-unit sectors in the native PSP world grid.
constexpr int32_t citySectorSize = 60;
constexpr int32_t cityOriginX = -3510;
constexpr int32_t cityOriginY = -2490;
constexpr size_t cityMapHeaderSize = 22;
constexpr size_t cityMapReferenceOffset = 46;

struct PlacementKey {
    uint16_t modelId = 0;
    uint16_t angle = 0;
    uint16_t flags = 0;
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;

    bool operator==(const PlacementKey& other) const {
        return modelId == other.modelId && angle == other.angle && flags == other.flags &&
               x == other.x && y == other.y && z == other.z;
    }
};

struct PlacementKeyHash {
    size_t operator()(const PlacementKey& key) const {
        size_t result = key.modelId;
        const auto append = [&result](uint32_t value) {
            result ^= std::hash<uint32_t>{}(value) + size_t(0x9E3779B9u) +
                      (result << 6) + (result >> 2);
        };
        append(key.angle);
        append(key.flags);
        append(uint32_t(key.x));
        append(uint32_t(key.y));
        append(uint32_t(key.z));
        return result;
    }
};

void includePoint(CityBounds& bounds, float x, float y, float z) {
    const std::array<float, 3> point{x, y, z};
    if (!bounds.valid) {
        bounds.minimum = bounds.maximum = point;
        bounds.valid = true;
        return;
    }
    for (size_t axis = 0; axis < point.size(); ++axis) {
        bounds.minimum[axis] = std::min(bounds.minimum[axis], point[axis]);
        bounds.maximum[axis] = std::max(bounds.maximum[axis], point[axis]);
    }
}

void includeBounds(CityBounds& destination, const CityBounds& source) {
    if (!source.valid) return;
    includePoint(destination, source.minimum[0], source.minimum[1], source.minimum[2]);
    includePoint(destination, source.maximum[0], source.maximum[1], source.maximum[2]);
}

CityBounds modelBounds(const PakModel& model) {
    CityBounds bounds;
    for (const PakVertex& vertex : model.vertices)
        includePoint(bounds, vertex.x, vertex.y, vertex.z);
    return bounds;
}

CityBounds instanceBounds(const CityBounds& model, const std::array<float, 16>& transform) {
    CityBounds bounds;
    if (!model.valid) return bounds;
    for (unsigned corner = 0; corner < 8; ++corner) {
        const float x = (corner & 1) ? model.maximum[0] : model.minimum[0];
        const float y = (corner & 2) ? model.maximum[1] : model.minimum[1];
        const float z = (corner & 4) ? model.maximum[2] : model.minimum[2];
        includePoint(bounds,
            transform[0] * x + transform[4] * y + transform[8] * z + transform[12],
            transform[1] * x + transform[5] * y + transform[9] * z + transform[13],
            transform[2] * x + transform[6] * y + transform[10] * z + transform[14]);
    }
    return bounds;
}

float nativeSine(int32_t angle) {
    // The executable truncates its sine result to fixed point with 12 fractional bits.
    constexpr float nativeAngularScale = 0x1.921fb0p-14f;
    const float value = std::sin(float(angle) * nativeAngularScale) * 4096.0f;
    return float(int32_t(value)) / 4096.0f;
}

std::array<float, 16> placementTransform(const PlacementKey& placement) {
    const int32_t angle = int16_t(placement.angle);
    const float sine = nativeSine(angle);
    const float cosine = nativeSine(angle + 16384);
    return {
        cosine, -sine, 0, 0,
        sine, cosine, 0, 0,
        0, 0, 1, 0,
        float(placement.x) / 128.0f, float(placement.y) / 128.0f, float(placement.z) / 128.0f, 1
    };
}

bool reportProgress(const CityLoadOptions& options, const std::string& stage,
                    size_t completed, size_t total, std::string& error) {
    if (options.progress && !options.progress({completed, total, stage})) {
        error = "CTW city loading was cancelled.";
        return false;
    }
    return true;
}

bool loadTexture(const PakArchive& archive, uint16_t resourceId,
                 CityTexture& texture, std::string& error) {
    uint16_t selected = resourceId;
    std::unordered_set<uint16_t> visited;
    std::vector<uint8_t> bytes;
    for (;;) {
        if (!visited.insert(selected).second) {
            error = "Animated PSP texture references form a cycle.";
            return false;
        }
        if (!archive.extractResource(selected, bytes, error)) return false;
        if (bytes.size() < 4 || read32(bytes.data()) != 255) break;
        if (bytes.size() < 18) {
            error = "Animated PSP texture header is truncated.";
            return false;
        }
        const uint16_t frames = read16(bytes.data() + 16);
        if (frames == 0 || 18u + uint64_t(frames) * 4u > bytes.size()) {
            error = "Animated PSP texture frame table is truncated.";
            return false;
        }
        selected = read16(bytes.data() + 18);
    }
    texture.resourceId = resourceId;
    return decodePspTexture(bytes, texture.width, texture.height, texture.rgba, error);
}

bool assemblePspCity(const PakArchive& archive, CityScene& scene, std::string& error,
                     const CityLoadOptions& options) {
    if (archive.sourcePath().empty() || archive.resources().empty()) {
        error = "Open a Chinatown Wars PSP GAME.PAK before assembling its city.";
        return false;
    }
    if (archive.header().mobile || archive.isOverlayArchive()) {
        error = "Whole-city assembly requires the original PSP GAME.PAK world layout.";
        return false;
    }
    if (options.section > 4) {
        error = "CTW city quadrant must be 0, 1, 2, 3, or 4 for all quadrants.";
        return false;
    }
    if (!reportProgress(options, "Reading world map", 0, 1, error)) return false;

    std::vector<uint8_t> catalog;
    if (!archive.extractResource(0, catalog, error)) {
        error = "Cannot read the CTW resource catalog: " + error;
        return false;
    }
    if (catalog.size() < cityMapReferenceOffset + 2) {
        error = "CTW resource catalog has no world-map reference.";
        return false;
    }
    const uint16_t mapResource = read16(catalog.data() + cityMapReferenceOffset);
    if (mapResource == 0 || mapResource == UINT16_MAX || mapResource >= archive.resources().size()) {
        error = "CTW resource catalog references an invalid world map.";
        return false;
    }
    std::vector<uint8_t> map;
    if (!archive.extractResource(mapResource, map, error)) {
        error = "Cannot read CTW world-map resource " + std::to_string(mapResource) + ": " + error;
        return false;
    }
    constexpr size_t cellCount = size_t(cityGridWidth) * cityGridHeight;
    if (map.size() < cityMapHeaderSize + cellCount * 2) {
        error = "CTW world map is missing part of its 59 by 42 block grid.";
        return false;
    }

    CityScene assembled;
    assembled.section = options.section;
    std::unordered_map<uint16_t, uint32_t> modelIndices;
    std::unordered_set<uint16_t> worldResources;
    std::unordered_set<PlacementKey, PlacementKeyHash> uniquePlacements;
    std::set<uint16_t> textureResources;
    std::vector<uint8_t> worldBytes;
    std::vector<uint8_t> modelBytes;

    for (size_t cell = 0; cell < cellCount; ++cell) {
        if (!reportProgress(options, "Assembling world blocks", cell, cellCount, error)) return false;
        const uint16_t resourceId = read16(map.data() + cityMapHeaderSize + cell * 2);
        if (resourceId == UINT16_MAX) continue;
        if (resourceId >= archive.resources().size()) {
            error = "CTW world-map cell " + std::to_string(cell) +
                    " references missing resource " + std::to_string(resourceId) + ".";
            return false;
        }
        if (!archive.extractResource(resourceId, worldBytes, error)) {
            error = "Cannot read CTW world block " + std::to_string(resourceId) + ": " + error;
            return false;
        }
        if (worldBytes.size() < 60) {
            error = "CTW world block " + std::to_string(resourceId) +
                    " has a truncated header.";
            return false;
        }
        const uint32_t embeddedCount = read32(worldBytes.data() + 16);
        if (embeddedCount != 0) {
            // These streaming copies retain the same resource IDs as the global PAK directory.
            const uint32_t directory = read32(worldBytes.data() + 20);
            const uint64_t directoryBytes = (uint64_t(embeddedCount) + 1) * 8;
            const uint64_t worldEnd = 60u + uint64_t(read32(worldBytes.data() + 12));
            if (directory < worldEnd || uint64_t(directory) + directoryBytes > worldBytes.size()) {
                error = "CTW world block " + std::to_string(resourceId) +
                        " has a truncated embedded-resource directory.";
                return false;
            }
            const uint8_t* entries = worldBytes.data() + directory;
            uint32_t previousOffset = read32(entries + 4);
            const uint32_t firstOffset = previousOffset;
            if (previousOffset < directoryBytes || uint64_t(directory) + previousOffset > worldBytes.size()) {
                error = "CTW world block " + std::to_string(resourceId) +
                        " has an invalid embedded-resource data offset.";
                return false;
            }
            for (uint32_t entry = 0; entry < embeddedCount; ++entry) {
                const uint32_t nextOffset = read32(entries + size_t(entry + 1) * 8 + 4);
                const uint16_t embeddedId = read16(entries + size_t(entry) * 8);
                if (nextOffset < previousOffset || uint64_t(directory) + nextOffset > worldBytes.size() ||
                    embeddedId >= archive.resources().size()) {
                    error = "CTW world block " + std::to_string(resourceId) +
                            " has an invalid embedded-resource reference.";
                    return false;
                }
                previousOffset = nextOffset;
            }
            if (previousOffset - firstOffset != read32(worldBytes.data() + 24)) {
                error = "CTW world block " + std::to_string(resourceId) +
                        " has an inconsistent embedded-resource payload length.";
                return false;
            }
            assembled.diagnostics.embeddedResources += embeddedCount;
        }
        CityBlock block;
        block.resourceId = resourceId;
        block.gridX = uint16_t(cell % cityGridWidth);
        block.gridY = uint16_t(cell / cityGridWidth);
        const int32_t blockOriginX = cityOriginX + int32_t(block.gridX) * 2 * citySectorSize;
        const int32_t blockOriginY = cityOriginY + int32_t(block.gridY) * 2 * citySectorSize;
        block.origin = {float(blockOriginX), float(blockOriginY), 0};
        block.firstInstance = uint32_t(assembled.instances.size());
        const uint32_t blockIndex = uint32_t(assembled.blocks.size());
        const unsigned firstSection = options.section < 4 ? options.section : 0;
        const unsigned endSection = options.section < 4 ? options.section + 1 : 4;

        for (unsigned section = firstSection; section < endSection; ++section) {
            PakWorldSection decoded;
            if (!decodePspWorldSection(worldBytes, section, decoded, error)) {
                error = "CTW world block " + std::to_string(resourceId) + ", quadrant " +
                        std::to_string(section) + ": " + error;
                return false;
            }
            ++assembled.diagnostics.worldSections;
            if (decoded.boundaryOnly) ++assembled.diagnostics.boundarySections;
            assembled.diagnostics.sourcePlacements += decoded.placements.size();
            const int32_t originX = blockOriginX + int32_t(section & 1) * citySectorSize;
            const int32_t originY = blockOriginY + int32_t(section >> 1) * citySectorSize;

            for (const PakPlacement& placement : decoded.placements) {
                const PlacementKey key{placement.modelId, placement.angle, placement.flags,
                    originX * 128 + placement.rawX, originY * 128 + placement.rawY, placement.rawZ};
                if (!uniquePlacements.insert(key).second) {
                    ++assembled.diagnostics.duplicatePlacements;
                    continue;
                }
                auto model = modelIndices.find(placement.modelId);
                if (model == modelIndices.end()) {
                    CityModel decodedModel;
                    decodedModel.resourceId = placement.modelId;
                    if (!archive.extractResource(placement.modelId, modelBytes, error) ||
                        !decodePspModel(modelBytes, decodedModel.geometry, error)) {
                        error = "CTW world block " + std::to_string(resourceId) +
                                " references an unreadable model " + std::to_string(placement.modelId) +
                                ": " + error;
                        return false;
                    }
                    decodedModel.bounds = modelBounds(decodedModel.geometry);
                    for (const PakModelPart& part : decodedModel.geometry.parts) {
                        if (part.textureId != UINT16_MAX) textureResources.insert(part.textureId);
                    }
                    assembled.diagnostics.uniqueVertices += decodedModel.geometry.vertices.size();
                    const uint32_t modelIndex = uint32_t(assembled.models.size());
                    assembled.models.push_back(std::move(decodedModel));
                    model = modelIndices.emplace(placement.modelId, modelIndex).first;
                }
                CityInstance instance;
                instance.modelIndex = model->second;
                instance.blockIndex = blockIndex;
                instance.instanceId = placement.instanceId;
                instance.section = section;
                instance.flags = placement.flags;
                instance.transform = placementTransform(key);
                const CityModel& geometry = assembled.models[instance.modelIndex];
                instance.bounds = instanceBounds(geometry.bounds, instance.transform);
                assembled.diagnostics.instancedVertices += geometry.geometry.vertices.size();
                includeBounds(block.bounds, instance.bounds);
                includeBounds(assembled.bounds, instance.bounds);
                assembled.instances.push_back(std::move(instance));
            }
        }
        block.instanceCount = uint32_t(assembled.instances.size()) - block.firstInstance;
        if (block.instanceCount == 0) ++assembled.diagnostics.emptyBlocks;
        assembled.blocks.push_back(std::move(block));
        worldResources.insert(resourceId);
    }
    assembled.diagnostics.worldResources = worldResources.size();
    if (assembled.instances.empty()) {
        error = "The CTW world map contains no renderable placements in the selected quadrants.";
        return false;
    }

    if (options.loadTextures) {
        assembled.textures.reserve(textureResources.size());
        size_t completed = 0;
        for (uint16_t resourceId : textureResources) {
            if (!reportProgress(options, "Decoding world textures", completed, textureResources.size(), error))
                return false;
            CityTexture texture;
            std::string textureError;
            if (loadTexture(archive, resourceId, texture, textureError)) {
                assembled.textures.push_back(std::move(texture));
            } else {
                ++assembled.diagnostics.missingTextures;
                assembled.diagnostics.warnings.push_back("Texture " + std::to_string(resourceId) +
                                                        ": " + textureError);
            }
            ++completed;
        }
    }
    if (!reportProgress(options, "City ready", 1, 1, error)) return false;
    scene = std::move(assembled);
    error.clear();
    return true;
}

}

bool loadPspCity(const PakArchive& archive, CityScene& scene, std::string& error,
                 const CityLoadOptions& options) {
    try {
        return assemblePspCity(archive, scene, error, options);
    } catch (const std::bad_alloc&) {
        error = "There is not enough memory to assemble this CTW city.";
        return false;
    }
}

}
