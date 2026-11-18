#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_clock.h"
#include <cmath>

inline void storylandPrepareSavedGame(StorylandMissionEnvironment& world, bool vcs) {
    if(vcs)world.playerSprintEnergy=world.playerSprintMaximum;
    if (!vcs) {
        world.statistics["TotalLegitimateKills"] += world.statistics["KillsSinceLastCheckpoint"];
        world.statistics["KillsSinceLastCheckpoint"] = 0;
    }
    const float previous = vcs ? float(storylandClockMinute(world.hour)) / 60.0f : world.hour;
    const float advanced = previous + 6.0f;
    if (advanced >= 24.0f) {
        auto& days = world.statistics["DaysPassed"];
        const double passed = std::floor(double(advanced) / 24.0);
        if (std::isfinite(days) && days >= 0 && days <= UINT32_MAX && std::floor(days) == days)
            days = std::fmod(days + passed, 4294967296.0);
        else days += passed;
    }
    world.hour = std::fmod(advanced, 24.0f);
    world.clockResetRequested=true;
    world.saveMenuRequested = false;
    world.saveGameFinished = true;
}
