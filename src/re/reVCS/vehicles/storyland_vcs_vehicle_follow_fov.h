#pragma once

#include <algorithm>
#include <cmath>

// Original VCS CCam mode 18, 0x29FF6C..0x2A00F4. The velocity is
// projected onto the vehicle's forward matrix axis in native units/frame.
inline float storylandVcsFollowVehicleFov(float current, int nativeType,
    float forwardVelocity, float nativeStep) {
    if(!std::isfinite(current))current=70.0f;
    if(!std::isfinite(nativeStep)||nativeStep<=0)return std::clamp(current,70.0f,100.0f);
    if(!std::isfinite(forwardVelocity))forwardVelocity=0;
    float increase=0;
    if (nativeType == 0 || nativeType == 6) {
        if (forwardVelocity > 0.4f)
            increase=forwardVelocity-0.4f;
    } else if (nativeType == 2 && forwardVelocity > 0.1f) {
        increase=forwardVelocity*1.1f-0.1f;
    }
    // At a native step of one this is the original add-then-decay rule.
    // Its closed form preserves the same response at other render rates.
    const float retention=std::pow(0.98f,nativeStep);
    const float equilibrium=49.0f*increase;
    current=70.0f+equilibrium+(current-70.0f-equilibrium)*retention;
    return std::clamp(current, 70.0f, 100.0f);
}
