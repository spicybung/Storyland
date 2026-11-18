#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

inline std::uint8_t storylandPs2BalancedEnvironmentFix(std::uint8_t coefficient) {
    const float nativeFix=float(coefficient>>1u);
    const float increase=std::min(10.0f,std::floor(nativeFix*(127.0f-nativeFix)*(0.32f/127.0f)+0.5f));
    return std::uint8_t(nativeFix+increase);
}
