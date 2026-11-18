#pragma once

#include "src/storyland/storyland_play_test_runtime.h"
#include <algorithm>
#include <cmath>
#include <vector>

inline std::vector<StorylandRubbishVehicle> storylandCollectRubbishWakes(
    const StorylandPlayTestRuntime& runtime,const std::vector<StorylandNativeTrafficActor>& cars,
    const StorylandNativePoint& focus) {
    struct Candidate {float distanceSquared;StorylandRubbishVehicle wake;};
    std::vector<Candidate> candidates;
    candidates.reserve(std::min(cars.size(),runtime.vehicles.size()));
    for(size_t index=0;index<cars.size()&&index<runtime.vehicles.size();++index) {
        const auto& actor=cars[index];
        const auto& vehicle=runtime.vehicles[index];
        if(!vehicle.visible||!vehicle.collisionEnabled||!vehicle.grounded||actor.water||
           vehicle.nativeDefinition.domain!=StorylandVehicleDomain::Road||
           !std::isfinite(vehicle.speed)||std::fabs(vehicle.speed)<2.0f||
           !std::isfinite(actor.yaw)||!std::isfinite(actor.position.x)||
           !std::isfinite(actor.position.y)||!std::isfinite(actor.position.z))continue;
        const float dx=actor.position.x-focus.x,dy=actor.position.y-focus.y;
        const float distanceSquared=dx*dx+dy*dy;
        if(!std::isfinite(distanceSquared)||distanceSquared>30.0f*30.0f||
           std::fabs(actor.position.z-focus.z)>5.0f)continue;
        float halfWidth=.85f,halfLength=1.7f;
        if(const auto& collision=vehicle.nativeDefinition.collision) {
            const float width=(collision->maximum.x-collision->minimum.x)*.5f;
            const float length=(collision->maximum.y-collision->minimum.y)*.5f;
            if(std::isfinite(width)&&width>.05f&&width<10.0f)halfWidth=width;
            if(std::isfinite(length)&&length>.05f&&length<20.0f)halfLength=length;
        }
        const float direction=actor.yaw*.01745329251994329577f+
            (vehicle.speed<0.0f?3.14159265358979323846f:0.0f);
        candidates.push_back({distanceSquared,{actor.position,direction,std::fabs(vehicle.speed),halfWidth,halfLength}});
    }
    constexpr size_t maxWakes=24;
    if(candidates.size()>maxWakes) {
        std::nth_element(candidates.begin(),candidates.begin()+maxWakes,candidates.end(),
            [](const Candidate& a,const Candidate& b){return a.distanceSquared<b.distanceSquared;});
        candidates.resize(maxWakes);
    }
    std::vector<StorylandRubbishVehicle> wakes;
    wakes.reserve(candidates.size());
    for(const auto& candidate:candidates)wakes.push_back(candidate.wake);
    return wakes;
}

inline void storylandUpdateRubbishRuntime(StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& cars,const StorylandNativePoint& player,
    float seconds,bool allowed,const StorylandPlayRubbish::GroundHeight& ground) {
    if(runtime.paused||runtime.pauseMenu||!std::isfinite(seconds)||seconds<=0.0f)return;
    runtime.rubbish.enabled=allowed;
    if(!allowed) {
        if(!runtime.rubbish.sheets().empty())runtime.rubbish.reset();
        return;
    }
    const auto wakes=storylandCollectRubbishWakes(runtime,cars,player);
    runtime.rubbish.update(player,runtime.windVelocity,wakes,seconds,ground);
}
