#pragma once

#include "src/leeds/leeds_ctw_pak.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace storyland_ctw {

struct CityBounds {
    std::array<float, 3> minimum{};
    std::array<float, 3> maximum{};
    bool valid = false;
};

struct CityModel {
    uint32_t resourceId = 0;
    PakModel geometry;
    CityBounds bounds;
};

struct CityInstance {
    uint32_t modelIndex = 0;
    uint32_t blockIndex = 0;
    uint32_t instanceId = 0;
    unsigned section = 0;
    uint16_t flags = 0;
    std::array<float, 16> transform{};
    CityBounds bounds;
};

struct CityBlock {
    uint32_t resourceId = 0;
    uint16_t gridX = 0;
    uint16_t gridY = 0;
    std::array<float, 3> origin{};
    uint32_t firstInstance = 0;
    uint32_t instanceCount = 0;
    CityBounds bounds;
};

struct CityTexture {
    uint32_t resourceId = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba;
};

struct CityDiagnostics {
    size_t worldResources = 0;
    size_t worldSections = 0;
    size_t boundarySections = 0;
    size_t embeddedResources = 0;
    size_t emptyBlocks = 0;
    size_t sourcePlacements = 0;
    size_t duplicatePlacements = 0;
    size_t skippedPlacements = 0;
    size_t missingModels = 0;
    size_t missingTextures = 0;
    size_t uniqueVertices = 0;
    size_t instancedVertices = 0;
    std::vector<std::string> warnings;
};

struct CityScene {
    std::vector<CityModel> models;
    std::vector<CityInstance> instances;
    std::vector<CityBlock> blocks;
    std::vector<CityTexture> textures;
    CityBounds bounds;
    CityDiagnostics diagnostics;
    unsigned section = 4;
};

struct CityLoadProgress {
    size_t completed = 0;
    size_t total = 0;
    std::string stage;
};

struct CityLoadOptions {
    // 0..3 select one spatial quadrant; 4 assembles every quadrant.
    unsigned section = 4;
    bool loadTextures = true;
    std::function<bool(const CityLoadProgress&)> progress;
};

bool loadPspCity(const PakArchive& archive, CityScene& scene, std::string& error,
                 const CityLoadOptions& options = {});

}
