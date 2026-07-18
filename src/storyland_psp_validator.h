#pragma once

#include "storyland_dma_validator.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class StorylandModelFile;

struct StorylandPspDmaReport {
    std::string label;
    size_t byteSize = 0;
    uint32_t geometryBlocks = 0;
    uint32_t meshStreams = 0;
    uint32_t vertexStreams = 0;
    uint32_t warnings = 0;
    uint32_t fatals = 0;
    std::vector<StorylandDmaIssue> issues;

    bool safe() const { return fatals == 0; }
    std::string text() const;
};

StorylandPspDmaReport storylandValidatePspDmaGe(
    const StorylandModelFile& model,
    const std::vector<uint8_t>& bytes,
    const std::string& label
);
