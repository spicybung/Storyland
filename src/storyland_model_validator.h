#pragma once

#include "storyland_model.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class StorylandModelIssueSeverity {
    Warning,
    Fatal
};

struct StorylandModelIntegrityIssue {
    StorylandModelIssueSeverity severity = StorylandModelIssueSeverity::Warning;
    size_t offset = 0;
    std::string message;
};

struct StorylandModelIntegrityReport {
    std::string label;
    size_t byteSize = 0;
    size_t vertices = 0;
    size_t triangles = 0;
    size_t bones = 0;
    size_t rootBones = 0;
    size_t weightedVertices = 0;
    size_t skinInfluences = 0;
    uint32_t warnings = 0;
    uint32_t fatals = 0;
    std::vector<StorylandModelIntegrityIssue> issues;

    bool safe() const { return fatals == 0; }
    std::string text() const;
};

StorylandModelIntegrityReport storylandValidateModelIntegrity(
    const StorylandModelFile& model,
    const std::vector<uint8_t>& bytes,
    const std::string& label
);
