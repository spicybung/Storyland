#pragma once
#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <cmath>

// Separate optional vehicle collision damage from the retail car/ped physics.
// The velocity uses metres per second, and the contact dimensions use metres.
inline float storylandMafia2VehicleImpactDamage(const StorylandNativePoint& car,
    const StorylandNativePoint& person,float yawDegrees,float forwardSpeed,
    float lateralSpeed,float halfLength,float halfWidth) {
    const float speed=std::hypot(forwardSpeed,lateralSpeed);
    if(!std::isfinite(speed)||speed<=4.5f||!std::isfinite(yawDegrees)||
       !std::isfinite(car.x)||!std::isfinite(car.y)||!std::isfinite(car.z)||
       !std::isfinite(person.x)||!std::isfinite(person.y)||!std::isfinite(person.z)||
       !std::isfinite(halfLength)||!std::isfinite(halfWidth)||
       halfLength<=0||halfWidth<=0||std::fabs(car.z-person.z)>1.65f)return 0.0f;
    const float angle=yawDegrees*.01745329251994329577f;
    const float cs=std::cos(angle),sn=std::sin(angle);
    const float dx=person.x-car.x,dy=person.y-car.y;
    const float ahead=dx*cs+dy*sn;
    const float side=std::fabs(-dx*sn+dy*cs);
    if(std::fabs(ahead)>halfLength+.36f||side>halfWidth+.35f)return 0.0f;
    // A parked vehicle or a glancing movement without real forward contact
    // must never damage the player. Backing cars use the same relative test.
    const float drivingSign=forwardSpeed>=0?1.0f:-1.0f;
    if(ahead*drivingSign < -halfLength*.9f)return 0.0f;
    return std::clamp((speed-4.5f)*2.0f,0.0f,120.0f);
}
