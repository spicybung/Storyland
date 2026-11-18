#pragma once

#include <cstddef>
#include "src/leeds/leeds_native_world.h"

// Intro soldiers are owned by MAIN.SCM and its mission character pool.
// A second ambient Fort Baxter spawner must not duplicate them.
inline bool storylandAllowAdditionalFortBaxterPatrol(bool vcs,
    bool playGame,bool introProtected,bool insideBase,
    bool populationEnabled,size_t existingPatrols,size_t cap=3u) {
    return vcs && !playGame && !introProtected && insideBase &&
        populationEnabled && existingPatrols < cap;
}

// The authored ARMYBAS rectangle applies to the location of the generated
// soldier, not merely to Victor's current zone. A pedestrian route can cross
// the wall and otherwise place an Army member on the public pavement.
inline bool storylandArmyPopulationPositionAllowed(bool vcs,int gangSlot,
    const StorylandNativeZone* base,
    const StorylandNativePoint& spawnPosition) {
    if(!vcs||gangSlot!=3)return true;
    return base && base->contains(spawnPosition);
}
