#pragma once

#include "src/storyland/storyland_mission_vm.h"
#include "src/leeds/leeds_ped_stats.h"

inline uint8_t storylandNextVcsPedColorSet(uint8_t previous, uint8_t count,
    bool forceFirst = false) {
    const uint8_t next = uint8_t(unsigned(previous) + 1u);
    return forceFirst || next >= count ? 0 : next;
}

inline bool storylandInitializeVcsPedAppearance(StorylandMissionEnvironment& world,
    StorylandMissionEnvironment::Character& character, const std::vector<uint8_t>& bytes,
    std::string& error) {
    if (character.model < 0) {
        error = "Native character appearance requires a valid model.";
        return false;
    }
    const auto clothing = storylandDecodePedClothing(bytes, uint32_t(character.model), true);
    if (!clothing.valid) {
        error = "Native character appearance requires the original model colour table.";
        return false;
    }
    const auto cursor = world.vcsPedColorSetCursors.find(character.model);
    const uint8_t previous = cursor == world.vcsPedColorSetCursors.end() ?
        clothing.regularCursor : cursor->second;
    const uint8_t selected = storylandNextVcsPedColorSet(previous, clothing.regularCount);
    std::array<uint32_t,6> colors{};
    if (clothing.regularCount == 0) colors.fill(UINT32_MAX);
    else colors = clothing.regular.at(selected);
    world.vcsPedColorSetCursors[character.model] = selected;
    character.colorSet = selected;
    character.scriptColorSet = -1;
    character.clothingColors = colors;
    character.clothingColorsSet = true;
    return true;
}

inline bool storylandValidateVcsPedColorSetCursors(const StorylandMissionEnvironment& world,
    bool vcs, const std::vector<uint8_t>& bytes, std::string& error) {
    if (!vcs && !world.vcsPedColorSetCursors.empty()) {
        error = "VCS character colour cursors cannot be restored in LCS.";
        return false;
    }
    for (const auto& entry : world.vcsPedColorSetCursors) {
        if (entry.first < 0 || entry.first > 65535) {
            error = "Saved character colour cursor has an invalid model.";
            return false;
        }
        const auto clothing = storylandDecodePedClothing(bytes, uint32_t(entry.first), true);
        if (!clothing.valid || (clothing.regularCount == 0 ? entry.second != 0 :
                entry.second >= clothing.regularCount)) {
            error = "Saved character colour cursor does not match the original model table.";
            return false;
        }
    }
    return true;
}
