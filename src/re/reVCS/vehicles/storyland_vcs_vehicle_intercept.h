#pragma once

#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <cmath>

namespace StorylandVcsVehicleIntercept {

inline StorylandNativePoint predictedTarget(const StorylandNativePoint& target,
    const StorylandNativePoint& nativeVelocity) {
    const float speed = std::hypot(nativeVelocity.x, nativeVelocity.y);
    const float scale = speed > 0.13f ? 0.13f / speed : 1.0f;
    return {target.x + nativeVelocity.x * scale * 60.0f,
        target.y + nativeVelocity.y * scale * 60.0f, target.z};
}

inline bool reachedIntercept(const StorylandNativePoint& vehicle,
    const StorylandNativePoint& predicted) {
    const float dx = vehicle.x - predicted.x;
    const float dy = vehicle.y - predicted.y;
    return dx * dx + dy * dy < 25.0f;
}

inline bool targetEscaped(const StorylandNativePoint& vehicle,
    const StorylandNativePoint& target) {
    const float dx = vehicle.x - target.x;
    const float dy = vehicle.y - target.y;
    return dx * dx + dy * dy > 100.0f;
}

}
