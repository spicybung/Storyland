#pragma once
#include <cstdint>

inline uint32_t storylandNativeRandom(uint32_t& state) {
    const int32_t seed = static_cast<int32_t>(state);
    int64_t next = int64_t(16807) * (seed % 127773) - int64_t(2836) * (seed / 127773);
    if (next <= 0) next += 2147483647;
    state = static_cast<uint32_t>(next);
    return state;
}

inline float storylandNativeRandomUnit(uint32_t& state) {
    return float(storylandNativeRandom(state)) / 2147483647.0f;
}

inline uint32_t storylandNativeRandomRangeFromValue(uint32_t value, uint32_t limit) {
    return uint32_t((uint64_t(value) * limit) >> 31);
}

inline uint32_t storylandNativeRandomRange(uint32_t& state, uint32_t limit) {
    return storylandNativeRandomRangeFromValue(storylandNativeRandom(state), limit);
}
