#pragma once

#include "src/storyland/storyland_play_test_runtime.h"
#include <cmath>

inline bool storylandAdvanceVcsBackgroundPlayer(bool active, bool inputEnabled, bool blocked,
    StorylandPlayTestRuntime& runtime, const StorylandNativeWorld& world,
    std::vector<StorylandNativeTrafficActor>& vehicles, StorylandNativePoint& player,
    float& heading, float cameraYaw, float seconds) {
    if (!active || inputEnabled || blocked || !world.paths.vcs || runtime.paused || runtime.pauseMenu ||
        !std::isfinite(seconds) || seconds <= 0) return false;
    runtime.drunkMovementHistory = {};
    runtime.playerAiming = false;
    runtime.playerDiveDown = false;
    runtime.playerDiveUp = false;
    runtime.extendedMovement.crouchPressed = false;
    storylandUpdatePlayTestPlayer(runtime, world, player, heading, cameraYaw,
        StorylandPlayTestInput{}, false, false, vehicles, seconds);
    storylandUpdateVcsJetSkiOccupants(runtime, vehicles, player, heading, seconds);
    return true;
}
