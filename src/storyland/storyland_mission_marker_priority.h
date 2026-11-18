#pragma once

#include "src/storyland/storyland_entry_exit.h"
#include "src/storyland/storyland_mission_vm.h"
#include <cmath>
#include <cstdint>

inline bool storylandMissionMarkerVisibleInArea(
    const StorylandMissionEnvironment::Marker& marker,int area,bool vcs) {
    return marker.area==area || (vcs&&marker.persistent&&(marker.type==6u||marker.type==7u));
}

inline bool storylandHasAuthoredMissionCircle(
    const StorylandMissionEnvironment& world,int area,bool vcs=false) {
    for(const auto& entry:world.frameScriptCheckpoints) {
        const auto& marker=entry.second;
        if(marker.area==area&&marker.type!=3u&&marker.type!=7u&&marker.radius>0.0f &&
            std::isfinite(marker.radius))return true;
    }
    for(const auto& marker:world.markers) {
        if(storylandMissionMarkerVisibleInArea(marker,area,vcs)&&marker.type!=7u&&
            marker.radius>0.0f&&std::isfinite(marker.radius))return true;
    }
    return false;
}

inline bool storylandHasAuthoredMissionArrow(
    const StorylandMissionEnvironment& world,int area) {
    if(world.pointerArrowActive&&area==world.area)return true;
    for(const auto& item:world.frameScriptCheckpoints)
        if(item.second.area==area&&item.second.type==3u)return true;
    return false;
}

inline bool storylandEnexHasWarp(const StorylandEntryExit& entry,bool reverse=false) {
    StorylandEntryExitWarp warp;
    return storylandResolveEntryExitWarp(entry,reverse,warp);
}
