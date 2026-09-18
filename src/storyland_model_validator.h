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
    size_t relocationFields = 0;
    size_t declaredRelocationFields = 0;
    size_t declaredRelocationTableDwords = 0;
    size_t relocationPrefixDwords = 0;
    size_t recoveredTrailingRelocations = 0;
    size_t missingRequiredRelocations = 0;
    size_t fileLocalPointers = 0;
    size_t runtimePointers = 0;
    size_t suspiciousPointers = 0;
    size_t degenerateTriangles = 0;
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


// Canonicalizes the exact synthetic PS2 PED layout produced by the short-lived
// BLeeds 1.4.8 one-material-slot-per-part regression. Only byte-identical
// duplicate descriptor objects are folded, and the existing runtime part tex_id
// fields are remapped in place. No geometry or file offsets are moved.
bool storylandRepairLeedsPedDuplicateMaterialSlots(
    std::vector<uint8_t>& bytes,
    uint32_t& removedDescriptorSlots,
    std::string& detail
);

// Restores the retail VCS PS2 player PED HAnim identity table and the matching
// per-frame node IDs. This is intentionally target-specific: call it only when
// replacing the base VCS plr.mdl resource. It changes identity metadata only;
// geometry, skin weights, DMA/VIF payloads, material names and file offsets are
// not moved.
bool storylandRepairVcsRetailPlayerHAnimIds(
    std::vector<uint8_t>& bytes,
    uint32_t& repairedHierarchyRows,
    uint32_t& repairedFrameIds,
    std::string& detail
);

// Repairs the specific Leeds MDL case where valid relocation-field offsets are
// physically present immediately after the declared relocation table but the
// header count stops too early. The function only changes the count field; it
// never invents relocation entries.
bool storylandRepairLeedsRelocationCount(
    std::vector<uint8_t>& bytes,
    uint32_t& addedEntries,
    std::string& detail
);
