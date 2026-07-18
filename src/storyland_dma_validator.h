#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class StorylandDmaIssueSeverity {
    Info,
    Warning,
    Fatal
};

struct StorylandDmaIssue {
    StorylandDmaIssueSeverity severity = StorylandDmaIssueSeverity::Info;
    size_t offset = 0;
    uint32_t address = 0;
    std::string message;
};

struct StorylandDmaTlbReport {
    std::string label;
    size_t byteSize = 0;
    uint32_t dmaTags = 0;
    uint32_t dmaChains = 0;
    uint32_t vifStreams = 0;
    uint32_t vifCommands = 0;
    uint32_t vifUnpacks = 0;
    uint32_t warnings = 0;
    uint32_t fatals = 0;
    std::vector<StorylandDmaIssue> issues;

    bool safe() const { return fatals == 0; }
    std::string text() const;
};

StorylandDmaTlbReport storylandValidatePs2DmaTlb(
    const std::vector<uint8_t>& bytes,
    const std::string& label
);
