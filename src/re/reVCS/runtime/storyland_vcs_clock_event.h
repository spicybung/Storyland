#pragma once

#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_clock.h"
#include <cmath>

inline void storylandSetStoriesClockEvent(StorylandMissionEnvironment& world,
    int32_t hour,int32_t minute,int32_t warningMinutes) {
    // 405FA8 retains an active event unless the new warning operand is zero.
    if(uint16_t(world.clockWarningMinutes)!=0&&warningMinutes!=0)return;
    world.clockEventHours=uint8_t(hour);
    world.clockEventMinutes=uint8_t(minute);
    world.clockWarningMinutes=uint16_t(warningMinutes);
    world.clockEventFlashing=false;
}

inline bool storylandStoriesClockEventVisible(StorylandMissionEnvironment& world,
    uint8_t hour,uint8_t minute,uint8_t second,uint16_t sound) {
    const uint16_t warning=uint16_t(world.clockWarningMinutes);
    if(warning!=0) {
        if(!world.clockEventFlashing) {
            int warningHour=int(uint8_t(world.clockEventHours))-warning/60;
            int warningMinute=int(uint8_t(world.clockEventMinutes))-warning%60;
            if(warningMinute<0) {warningMinute+=60;--warningHour;}
            if(warningHour<0)warningHour+=24;
            if(hour==warningHour&&minute==warningMinute)world.clockEventFlashing=true;
        }
        if(world.clockEventFlashing&&hour==uint8_t(world.clockEventHours)&&minute==uint8_t(world.clockEventMinutes)) {
            world.clockEventFlashing=false;
            world.clockWarningMinutes=0;
        }
    }
    // CHud31ED10 samples game-clock seconds at4CD2C4, not rendered frames.
    if(world.clockEventFlashing&&second>=31) {
        world.clockEventHidden=true;
        return false;
    }
    if(world.clockEventHidden) {
        world.clockEventHidden=false;
        if(world.audioSubsystemEnabled&&world.scriptSounds.size()<256)
            world.scriptSounds.push_back({sound,{world.x,world.y,world.z},true});
    }
    return true;
}

inline void storylandSetVcsClockEvent(StorylandMissionEnvironment& world,
    int32_t hour,int32_t minute,int32_t warningMinutes) {
    storylandSetStoriesClockEvent(world,hour,minute,warningMinutes);
}

inline bool storylandVcsClockEventVisible(StorylandMissionEnvironment& world,
    uint8_t hour,uint8_t minute,uint8_t second) {
    return storylandStoriesClockEventVisible(world,hour,minute,second,168);
}

inline bool storylandVcsClockEventVisible(StorylandMissionEnvironment& world,float hour) {
    if(!std::isfinite(hour))return true;
    const auto clock=storylandClockParts(hour);
    return storylandVcsClockEventVisible(world,uint8_t(clock.hour),
        uint8_t(clock.minute),uint8_t(clock.second));
}

inline bool storylandLcsClockEventVisible(StorylandMissionEnvironment& world,float hour) {
    if(!std::isfinite(hour))return true;
    const auto clock=storylandClockParts(hour);
    return storylandStoriesClockEventVisible(world,uint8_t(clock.hour),
        uint8_t(clock.minute),uint8_t(clock.second),167);
}
