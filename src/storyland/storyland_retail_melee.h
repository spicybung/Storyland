#pragma once

#include "src/leeds/leeds_animation_groups.h"
#include <array>
#include <cstdint>

// Select attack associations from each native weapon group. The PART and IDLE
// associations are not swing animations and must not be used as combo steps.
inline std::array<uint32_t, 3> storylandRetailMeleeSwingAssociations(
    const StorylandAnimationGroups& groups, int32_t groupId) {
    const uint32_t first = groups.vcsLayout ? 214u : 201u;
    std::array<uint32_t, 3> result{first, first, first};
    if (groupId < 0) return result;
    const auto group = groups.groups.find(uint32_t(groupId));
    if (group == groups.groups.end()) return result;

    const auto& name = group->second.name;
    if (name == "knife") {
        result = {first, first + 1u, first + 4u};
    } else if (name == "baseball") {
        result = {first, first + 1u, first};
    } else if (name == "golfclub") {
        result = {first + 1u, first + 4u, first + 1u};
    } else if (name == "chainsaw") {
        result = {first, first + 1u, first};
    }

    // A missing native association is never a reason to replace a swing with
    // a collision-reaction PART clip (knife_part, bat_part, csaw_part).
    const auto validSwing = [&](uint32_t associationId) {
        const auto* association = groups.find(uint32_t(groupId), associationId);
        if (!association) return false;
        const auto clip = StorylandAnimationGroups::key(association->name);
        return clip.find("part") == std::string::npos &&
            clip.find("idle") == std::string::npos &&
            clip.find("hit_") == std::string::npos &&
            clip.find("floor") == std::string::npos;
    };
    for (uint32_t& id : result) {
        if (!validSwing(id)) id = validSwing(first) ? first : UINT32_MAX;
    }
    return result;
}

inline uint32_t storylandRetailMeleeSwingAssociation(const StorylandAnimationGroups& groups,
    int32_t groupId, uint32_t serial, bool moving) {
    const auto swings = storylandRetailMeleeSwingAssociations(groups, groupId);
    const size_t index = moving ? 0u : size_t((serial ? serial - 1u : 0u) % swings.size());
    return swings[index];
}
