#pragma once

#include <algorithm>

inline float storylandVcsHelicopterEntryHeight(float proposedHeight, float doorHeight,
    bool vcsHelicopter, bool grounded) {
    if (!vcsHelicopter || !grounded) return proposedHeight;
    return std::max(proposedHeight, doorHeight);
}
