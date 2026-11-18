#pragma once

#include "src/storyland/storyland_play_test.h"
#include "src/storyland/storyland_water.h"
#include <cmath>

// VCS has a separate surface-swimming bank. LCS keeps its existing
// drowning motion. Death is selected before this helper is called.
inline StorylandPlayTestMotion storylandPedWaterMotion(bool vcs,
    const StorylandCharacterWaterState& water, float movementSpeed) {
    if (!vcs || water.drowning) return StorylandPlayTestMotion::Drown;
    if (std::isfinite(movementSpeed) && movementSpeed > 0.1f)
        return StorylandPlayTestMotion::Swim;
    return StorylandPlayTestMotion::SwimIdle;
}
