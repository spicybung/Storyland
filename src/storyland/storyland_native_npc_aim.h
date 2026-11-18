#pragma once

#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// SLUS_214.23 0x2A4E5C..0x2A5284 and SLUS_215.90 0x2C42D8..0x2C4724.
// The retail games add spread at the weapon-range endpoint, in world axes.
inline float storylandNativeNpcAimInaccuracy(uint8_t accuracy, bool playerTarget,
    float nativeTargetSpeed) {
    float inaccuracy = 100.0f - float(accuracy);
    if (inaccuracy <= 0.0f) return 0.0f;
    if (playerTarget) {
        const float speed = std::isfinite(nativeTargetSpeed) ?
            std::clamp(nativeTargetSpeed, 0.0f, 0.33f) : 0.0f;
        inaccuracy *= speed * 0.90909094f + 0.8f;
    }
    return inaccuracy;
}

template<class RandomUnit>
inline StorylandNativePoint storylandNativeNpcInstantHitEnd(const StorylandNativePoint& origin,
    const StorylandNativePoint& target, float range, uint8_t accuracy, bool playerTarget,
    float nativeTargetSpeed, float spreadScale, RandomUnit&& randomUnit) {
    const float x = target.x - origin.x;
    const float y = target.y - origin.y;
    const float z = target.z - origin.z;
    const float distance = std::max(0.01f, std::sqrt(x * x + y * y + z * z));
    const float scale = range / distance;
    StorylandNativePoint end{origin.x + x * scale, origin.y + y * scale, origin.z + z * scale};
    const float inaccuracy = storylandNativeNpcAimInaccuracy(accuracy, playerTarget,
        nativeTargetSpeed) * spreadScale;
    if (inaccuracy > 0.0f) {
        const float horizontalX = randomUnit() * 0.4f - 0.2f;
        const float horizontalY = randomUnit() * 0.4f - 0.2f;
        const float vertical = randomUnit() * 0.2f - 0.1f;
        end.x += inaccuracy * horizontalX;
        end.y += inaccuracy * horizontalY;
        end.z += inaccuracy * vertical;
    }
    return end;
}
