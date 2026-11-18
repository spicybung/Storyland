#pragma once
#include <cstdint>

inline bool storylandNativeGangProtectsPlayer(int32_t pedestrianType, uint16_t playerFlags) {
    const int32_t gang = pedestrianType - 7;
    return gang >= 0 && gang < 9 && (playerFlags & (uint16_t(1) << unsigned(gang))) != 0;
}
