#pragma once

#include "src/storyland/storyland_environment.h"

#include <cmath>

// 13C45C compares the native 50 Hz velocity magnitude and wheel-0 fraction.
inline bool storylandVcsTaxiBoostGate(float forward, float lateral, float vertical,
    float wheelFraction, bool hornPressed) {
    const float x=forward/StorylandNativePhysicsRate;
    const float y=lateral/StorylandNativePhysicsRate;
    const float z=vertical/StorylandNativePhysicsRate;
    return hornPressed && wheelFraction<1.0f && std::sqrt(x*x+y*y+z*z)>0.2f;
}
