#pragma once

#include <cstdint>

// VCS SCM 0x0220 and 0x03FB preserve the native wait-state ID and deadline.
// Only states with a confirmed group-0 idle association take over skeletal
// animation. Other IDs are consumed by movement/interaction tasks, not a
// fabricated idle animation.
inline int32_t storylandVcsWaitAssociation(int32_t state) {
    switch (state) {
    case 12: return 12;
    case 13: return 11;
    case 17: return 156;
    case 18: return 157;
    case 19: return 14;
    case 23: return 163;
    case 24: return 164;
    default: return -1;
    }
}

inline bool storylandVcsWaitBlocksMovement(int32_t state) {
    return state==1||state==12||state==13||state==17||state==18||
        state==19||state==23||state==24;
}

inline bool storylandVcsWaitStateValid(int32_t state) {
    return state>=0 && state<=255;
}
