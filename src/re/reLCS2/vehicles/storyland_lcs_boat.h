#pragma once
#include "src/leeds/leeds_vehicle.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

struct StorylandLcsBoatControls {
    float steering=0; // Radians, native CCarCtrl output.
    float throttle=0;
};

inline bool storylandLcsBoatCruiseByte(float value,uint8_t& result) {
    // The native setter reads a float into CAutoPilot's uint8 speed. Keep the
    // defined conversion range explicit instead of invoking host C++ UB for
    // out-of-range or non-finite float-to-byte conversions.
    if(!std::isfinite(value)||value<=-1||value>=256)return false;
    result=uint8_t(value);return true;
}

inline StorylandLcsBoatControls storylandLcsBoatControls(float headingRadians,
    float targetX,float targetY,float speedMetresPerSecond,float cruiseMetresPerSecond) {
    StorylandLcsBoatControls result;
    float angle=std::remainder(std::atan2(targetY,targetX)-headingRadians,6.283185307179586f);
    if(angle>=3.141592653589793f)angle-=6.283185307179586f;
    result.steering=std::clamp(angle,-.5f,.5f);
    const float speed=std::max(0.0f,speedMetresPerSecond)*60.0f/50.0f;
    const float target=std::max(0.0f,cruiseMetresPerSecond)*60.0f/50.0f;
    const float difference=target-speed;
    // SteerAIBoatWithPhysicsHeadingForTarget. Zero is a speed target, not
    // BOAT_STOP: the native controller uses reverse thrust when at/above it.
    if(difference<=0) {
        result.throttle=difference < -5.0f?-.2f:-.1f;
        result.steering=-result.steering;
    } else if(speed<=0||difference/speed>.25f)result.throttle=1;
    else result.throttle=4*difference/speed;
    return result;
}

inline float storylandLcsBoatSubmergedVolume(const StorylandVehicleDefinition& definition,
    float bottom,float top,float waterLocalZ) {
    if(!(top>bottom)||waterLocalZ<=bottom)return 0;
    // cBuoyancy's nine hull slices, with the native speedboat distribution.
    static constexpr float ordinary[]={.75f,.9f,.75f,.95f,1,.95f,.4f,.7f,.4f};
    static constexpr float speedboat[]={.7f,.9f,.7f,.95f,1,.95f,.6f,.7f,.6f};
    const auto* distribution=definition.handlingId==73?speedboat:ordinary;
    const float submerged=std::min(waterLocalZ,top)-bottom;
    float volume=0;
    for(unsigned index=0;index<9;++index) {
        const float slice=std::max(0.0f,submerged-(1-distribution[index]))*distribution[index];
        volume+=slice*slice;
    }
    return volume/((top-bottom)*9);
}
