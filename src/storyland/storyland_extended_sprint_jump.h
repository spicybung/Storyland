#pragma once
#include <cmath>

// Extended+SA only: an established sprint contributes a modest upward boost
// to the physical jump. The character must still reach and clear the ledge.
inline bool storylandExtendedSprintJumpEligible(bool vcs, bool extended, bool sprinting,
    float sprintBlend, float movementSpeed, float runSpeed) {
    return vcs && extended && sprinting && std::isfinite(sprintBlend) && sprintBlend >= .25f &&
        std::isfinite(movementSpeed) && std::isfinite(runSpeed) && runSpeed > 0 &&
        movementSpeed >= runSpeed * .8f;
}

inline float storylandExtendedSprintJumpImpulse(float nativeImpulse, bool eligible) {
    return eligible && std::isfinite(nativeImpulse) && nativeImpulse > 0 ? nativeImpulse * 1.2f : nativeImpulse;
}
