#pragma once

#include <cstdint>

// VCS's 90-row CParticleSystemData table is in GAME.DTZ at root +0xBC.
// Row 51 is the fine, tan foot-dust descriptor; row 3 is the wheel-dirt
// descriptor. Row 60 is TREE_LEAVES and must never be used for footfalls.
constexpr int32_t StorylandVcsPedFootDustParticle = 51;
constexpr int32_t StorylandVcsWheelDirtParticle = 3;
constexpr int32_t StorylandVcsTreeLeavesParticle = 60;

inline int32_t storylandRetailFootfallParticle(bool vcs, uint8_t surface,
    bool grounded, float movementSpeed) {
    if (!vcs || !grounded || surface != 2 || movementSpeed <= 0.5f)
        return -1;
    return StorylandVcsPedFootDustParticle;
}

inline int32_t storylandRetailGrassTyreParticle(bool vcs) {
    // LCS has a distinct native particle numbering scheme; until its exact
    // wheel-dirt association is verified, use the existing smoke fallback.
    return vcs ? StorylandVcsWheelDirtParticle : 20;
}
