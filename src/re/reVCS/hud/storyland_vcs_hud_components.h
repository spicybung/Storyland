#pragma once

#include "src/storyland/storyland_mission_vm.h"

inline bool storylandSetVcsHudComponentVisible(StorylandMissionEnvironment& world,
    int32_t selector,bool visible) {
    const uint8_t component = uint8_t(selector);
    if (component < 8 && component != 6) {
        const uint8_t bit = uint8_t(1u << component);
        if (visible) world.vcsHudHiddenComponents &= uint8_t(~bit);
        else world.vcsHudHiddenComponents |= bit;
        return true;
    }
    if (component == 6) {
        world.vcsAreaCaption.visible = visible;
        return true;
    }
    if (component == 12) {
        world.vcsAmmoHudVisible = visible;
        return true;
    }
    if (component == 10 || component == 11) {
        (component == 10 ? world.vcsHudMeters.oxygenEnabled :
            world.vcsHudMeters.sprintEnabled) = visible;
        return true;
    }
    return false;
}

inline bool storylandVcsHudComponentVisible(const StorylandMissionEnvironment& world,uint8_t component) {
    if (component < 8 && component != 6)
        return (world.vcsHudHiddenComponents & uint8_t(1u << component)) == 0;
    if (component == 6) return world.vcsAreaCaption.visible;
    if (component == 12) return world.vcsAmmoHudVisible;
    if (component == 10) return world.vcsHudMeters.oxygenEnabled;
    if (component == 11) return world.vcsHudMeters.sprintEnabled;
    return true;
}
