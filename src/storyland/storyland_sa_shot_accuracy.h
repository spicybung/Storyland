#pragma once
#include "src/leeds/leeds_native_world.h"
#include <cmath>
#include <cstdint>

inline float storylandSaCrouchSpreadScale(bool extended, bool crouching) {
    return extended && crouching ? 0.90f : 1.0f;
}

inline float storylandSaShotNoise(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return float(value & 65535u) / 32767.5f - 1.0f;
}

// Extended+SA uses a very small cone about the chosen aim point. Shot serials
// give repeatable offsets without consuming the game's AI or audio RNG.
inline StorylandNativePoint storylandSaFirearmAimPoint(const StorylandNativePoint& start,
    const StorylandNativePoint& end, bool extended, bool crouching, bool lockOn, uint32_t serial) {
    if (!extended) return end;
    const float dx=end.x-start.x, dy=end.y-start.y, dz=end.z-start.z;
    const float horizontal=std::hypot(dx,dy), distance=std::hypot(horizontal,dz);
    if (!std::isfinite(distance) || distance<0.001f || horizontal<0.00001f) return end;
    const float cone=0.003f * storylandSaCrouchSpreadScale(true,crouching) * (lockOn ? 0.5f : 1.0f);
    const float side=storylandSaShotNoise(serial ^ 0x91e10da5u)*cone*distance;
    const float up=storylandSaShotNoise(serial ^ 0xd1b54a35u)*cone*distance;
    const float hx=dx/horizontal, hy=dy/horizontal;
    const float nz=horizontal/distance, tilt=dz/distance;
    return {end.x-hy*side-hx*tilt*up,end.y+hx*side-hy*tilt*up,end.z+nz*up};
}
