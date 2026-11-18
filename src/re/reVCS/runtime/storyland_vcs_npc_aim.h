#pragma once

#include "src/storyland/storyland_native_npc_aim.h"

// SLUS_215.90 0x2C42D8..0x2C4724. Speed is displacement per native 50 Hz step.
inline float storylandVcsNpcAimInaccuracy(uint8_t accuracy, bool playerTarget,
    float nativeTargetSpeed) {
    return storylandNativeNpcAimInaccuracy(accuracy, playerTarget, nativeTargetSpeed);
}

template<class RandomUnit>
inline StorylandNativePoint storylandVcsNpcInstantHitEnd(const StorylandNativePoint& origin,
    const StorylandNativePoint& target, float range, uint8_t accuracy, bool playerTarget,
    float nativeTargetSpeed, float spreadScale, RandomUnit&& randomUnit) {
    return storylandNativeNpcInstantHitEnd(origin, target, range, accuracy, playerTarget,
        nativeTargetSpeed, spreadScale, randomUnit);
}
