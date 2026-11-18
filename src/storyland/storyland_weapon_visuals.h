#pragma once
#include "src/storyland/storyland_weapon_types.h"
#include "src/re/reVCS/runtime/storyland_vcs_bullet_traces.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// Retains the collision-clamped hitscan endpoints while drawing a brief,
// moving bullet streak rather than a bright line across the whole scene.
struct StorylandWeaponTraceSegment {
    StorylandNativePoint tail{},head{};
    float alpha=0;
    float width=1.0f;
    bool visible=false;
};

inline StorylandWeaponTraceSegment storylandWeaponTraceSegment(const StorylandWeaponTrace& trace) {
    StorylandWeaponTraceSegment result;
    const auto finite=[](const StorylandNativePoint& p) {
        return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);
    };
    if(!finite(trace.from)||!finite(trace.to)||!std::isfinite(trace.remaining)||
       !std::isfinite(trace.lifetime)||trace.remaining<=0||trace.lifetime<=0 ||
       trace.remaining>trace.lifetime+0.001f)return result;
    const float dx=trace.to.x-trace.from.x,dy=trace.to.y-trace.from.y,dz=trace.to.z-trace.from.z;
    const float distance=std::hypot(dx,dy,dz);
    if(!std::isfinite(distance)||distance<0.12f)return result;
    const float elapsed=std::max(0.0f,trace.lifetime-trace.remaining);
    const float speed=std::max(220.0f,distance/std::max(0.015f,trace.lifetime*0.8f));
    const float head=std::min(distance,0.3f+elapsed*speed);
    const float tail=std::max(0.0f,head-std::min(2.25f,distance*.3f));
    if(head-tail<0.05f)return result;
    const auto along=[&](float position) {
        const float amount=position/distance;
        return StorylandNativePoint{trace.from.x+dx*amount,trace.from.y+dy*amount,
            trace.from.z+dz*amount};
    };
    result.tail=along(tail);result.head=along(head);
    const float fade=std::clamp(trace.remaining/trace.lifetime,0.0f,1.0f);
    result.alpha=0.45f*fade*fade;
    result.width=trace.nativeWeapon==30||trace.nativeWeapon==31?1.45f:1.0f;
    result.visible=result.alpha>0.004f;
    return result;
}

inline bool storylandWeaponUsesBulletTrace(const StorylandWeaponDefinition& weapon) {
    // FireType 0=melee, 1=hitscan, 2=projectile, 3=flame,
    // 4=camera: only actual bullets receive a muzzle flash/tracer.
    return weapon.fireType == 1;
}

inline bool storylandAddWeaponTrace(std::vector<StorylandWeaponTrace>& traces,
    StorylandNativePoint from,StorylandNativePoint to,int32_t weapon,bool vcs=false) {
    StorylandWeaponTrace trace;
    trace.from=from;trace.to=to;trace.nativeWeapon=weapon;
    trace.lifetime=trace.remaining=0.075f;
    if(vcs) {
        const auto finite=[](const StorylandNativePoint& point) {
            return std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z);
        };
        if(!finite(from)||!finite(to)||std::hypot(to.x-from.x,to.y-from.y,to.z-from.z)<=0.0001f||
            !storylandVcsPrepareBulletTrace(trace,traces.size()))return false;
    } else if(!storylandWeaponTraceSegment(trace).visible)return false;
    constexpr size_t maximumTraces=256;
    if(traces.size()>=maximumTraces)traces.erase(traces.begin());
    traces.push_back(trace);
    return true;
}
