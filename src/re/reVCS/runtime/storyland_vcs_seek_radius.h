#pragma once

#include <cstdint>

// Original 204AC4 loads CPed+0x793 as a signed byte and 204AFC
// compares squared planar seek distance against its square.
inline bool storylandVcsWithinSeekRadius(uint8_t rawRadius,float dx,float dy) {
    const int radius=rawRadius<128?int(rawRadius):int(rawRadius)-256;
    return dx*dx+dy*dy<float(radius*radius);
}
