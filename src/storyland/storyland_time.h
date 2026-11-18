#pragma once

#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_clock.h"
#include <cmath>
#include <cstdint>

inline void storylandSetPlayTestClock(StorylandPlayTestRuntime& runtime,float hour) {
    if(!std::isfinite(hour))return;
    const auto clock=storylandClockParts(hour);
    runtime.clockHour=float(clock.hour*3600u+clock.minute*60u+clock.second)/3600.0f;
    runtime.clockProgress.reset(runtime.clockHour);
}

inline void storylandSetClockTime(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,int hour,int minute) {
    const int64_t total=int64_t(hour)*60+minute;
    const int normalized=int((total%1440+1440)%1440);
    const float value=float(normalized)/60.0f;
    storylandSetPlayTestClock(runtime,value);
    world.hour=value;
    world.clockResetRequested=false;
}

inline void storylandApplyMissionClock(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world) {
    if(world.clockResetRequested||runtime.clockHour!=world.hour)
        storylandSetPlayTestClock(runtime,world.hour);
    world.clockResetRequested=false;
    runtime.scriptTimeFrozen=world.timeFrozen;
}

inline bool storylandClockCanAdvance(const StorylandPlayTestRuntime& runtime) {
    return !runtime.scriptTimeFrozen&&!runtime.debugClockFrozen&&
        std::isfinite(runtime.gameMinutesPerSecond)&&runtime.gameMinutesPerSecond>0;
}

inline void storylandAdvanceClockSeconds(StorylandPlayTestRuntime& runtime,double seconds) {
    if(!storylandClockCanAdvance(runtime)||!std::isfinite(seconds)||seconds<=0)return;
    storylandAdvancePlayTestClockSeconds(runtime,seconds*double(runtime.gameMinutesPerSecond)*60.0);
}

inline void storylandAdvancePreviewClock(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,double seconds) {
    storylandAdvanceClockSeconds(runtime,seconds);
    world.hour=runtime.clockHour;
}
