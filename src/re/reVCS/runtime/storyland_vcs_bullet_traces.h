#pragma once

#include "src/storyland/storyland_weapon_types.h"
#include <array>
#include <algorithm>
#include <cmath>

struct StorylandVcsBulletTraceProfile {
    float radius=0.4f;
    unsigned milliseconds=750;
    uint8_t opacity=150;
};

// SLUS_215.90: 0x167ED0, weapon branch table at 0x4A0C20.
inline StorylandVcsBulletTraceProfile storylandVcsBulletTraceProfile(int32_t weapon) {
    if(weapon>=20&&weapon<=23)return {0.7f,1000,200};
    if((weapon>=28&&weapon<=31)||weapon==34||weapon==35||weapon==37)
        return {1.0f,2000,220};
    return {};
}

inline bool storylandVcsBulletTraceVisibleForShooter(bool player,int cameraMode,
    float nativeMovementSpeed) {
    if(!player)return true;
    switch(cameraMode) {
    case 34:case 7:case 46:case 8:case 42:case 39:case 40:case 45:
        return std::isfinite(nativeMovementSpeed)&&nativeMovementSpeed>=0.05f;
    default:return true;
    }
}

inline bool storylandVcsPrepareBulletTrace(StorylandWeaponTrace& trace,size_t active) {
    if(active>=16)return false;
    const auto profile=storylandVcsBulletTraceProfile(trace.nativeWeapon);
    unsigned milliseconds=profile.milliseconds;
    if(active>=10)milliseconds/=4;
    else if(active>=5)milliseconds/=2;
    // 0x168210 divides the admission time by ten before storing it.
    trace.lifetime=trace.remaining=float(milliseconds/10)*0.001f;
    trace.radiusScale=profile.radius;
    trace.opacity=profile.opacity;
    trace.vcs=true;
    return true;
}

struct StorylandBulletTraceVertex {
    StorylandNativePoint position{};
    uint8_t alpha=0;
};

template<class Emit>
bool storylandVcsBulletTraceTriangles(const StorylandWeaponTrace& trace,
    const float* view,Emit emit) {
    const auto finite=[](const StorylandNativePoint& point) {
        return std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z);
    };
    if(!trace.vcs||!view||!finite(trace.from)||!finite(trace.to)||
        !std::isfinite(trace.remaining)||!std::isfinite(trace.lifetime)||
        !std::isfinite(trace.radiusScale)||trace.remaining<=0||trace.lifetime<=0||
        trace.remaining>trace.lifetime+0.001f||trace.radiusScale<=0)return false;
    const StorylandNativePoint direction{trace.to.x-trace.from.x,
        trace.to.y-trace.from.y,trace.to.z-trace.from.z};
    const float length=std::hypot(direction.x,direction.y,direction.z);
    if(!std::isfinite(length)||length<=0.0001f)return false;
    auto start=trace.from,end=trace.to;
    const auto depth=[&](const StorylandNativePoint& point) {
        return -(view[2]*point.x+view[6]*point.y+view[10]*point.z+view[14]);
    };
    const float startDepth=depth(start),endDepth=depth(end);
    if(!std::isfinite(startDepth)||!std::isfinite(endDepth))return false;
    constexpr float nearDepth=0.7f;
    if(startDepth<nearDepth&&endDepth<nearDepth)return false;
    if(startDepth<nearDepth||endDepth<nearDepth) {
        const float fraction=(nearDepth-startDepth)/(endDepth-startDepth);
        const StorylandNativePoint crossing{start.x+(end.x-start.x)*fraction,
            start.y+(end.y-start.y)*fraction,start.z+(end.z-start.z)*fraction};
        if(startDepth<nearDepth)start=crossing;else end=crossing;
    }
    const float progress=std::clamp(1.0f-trace.remaining/trace.lifetime,0.0f,1.0f);
    const float radius=trace.radiusScale*(1.0f/60.0f)*progress;
    const auto opacity=uint8_t(std::clamp(float(trace.opacity)*(1.0f-progress),0.0f,255.0f));
    if(radius<=0||opacity==0)return false;
    const StorylandNativePoint side{direction.y/length*radius,-direction.x/length*radius,0};
    constexpr std::array<float,4> stations{0.0f,0.125f,0.875f,1.0f};
    std::array<std::array<StorylandBulletTraceVertex,5>,4> rings{};
    for(size_t ring=0;ring<rings.size();++ring) {
        const float fraction=stations[ring];
        const StorylandNativePoint center{start.x+(end.x-start.x)*fraction,
            start.y+(end.y-start.y)*fraction,start.z+(end.z-start.z)*fraction};
        const uint8_t alpha=ring==0||ring==3?uint8_t(0):opacity;
        rings[ring]={{{center,alpha},
            {{center.x,center.y,center.z+radius},alpha},
            {{center.x+side.x,center.y+side.y,center.z},alpha},
            {{center.x,center.y,center.z-radius},alpha},
            // 0x168B70/0x168B88 retain the native asymmetric negative edge.
            {{center.x-side.x,center.y-side.x,center.z},alpha}}};
    }
    // Native two-sided crossed strips, index table 0x481D00.
    constexpr std::array<unsigned,48> indices{0,5,7,0,7,2,0,7,5,0,2,7,
        0,4,9,0,9,5,0,9,4,0,5,9,0,1,6,0,6,5,0,6,1,0,5,6,
        0,3,8,0,8,5,0,8,3,0,5,8};
    for(size_t section=0;section<3;++section)
        for(unsigned index:indices)emit(rings[section+index/5][index%5]);
    return true;
}
