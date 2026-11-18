#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace StorylandVcsIntoxication {

inline float truncatedFloat(double value) {
    float rounded = float(value);
    if (std::isfinite(rounded) && std::fabs(double(rounded)) > std::fabs(value))
        rounded = std::nextafter(rounded, 0.0f);
    return rounded;
}

template<class World> void choosePeriod(World& world, uint32_t minimum, uint32_t maximum) {
    const uint32_t low = minimum / world.drunkVisualIntensity;
    const uint32_t high = maximum / world.drunkVisualIntensity;
    world.drunkVisualPeriod = low + uint32_t((uint64_t(world.nextNativeRandom()) * (high - low)) >> 31);
    const float random = truncatedFloat(double(truncatedFloat(world.nextNativeRandom())) / 2147483648.0);
    const float range = truncatedFloat(double(world.drunkVisualIntensity) * .5 - double(.2f));
    const float amount = truncatedFloat(double(range) * random);
    world.drunkVisualAmplitude = truncatedFloat(double(amount) + double(.2f));
}

template<class World> void set(World& world, int32_t intensity) {
    // SLUS-21590 167D78..167E9C retains the previous oscillation on disable.
    world.drunkVisualIntensity = uint8_t(intensity);
    if (!world.drunkVisualIntensity) {
        world.nativeTimeScale = 1.0f;
        return;
    }
    world.drunkVisualStartedAt = world.gameMilliseconds;
    choosePeriod(world, 8192, 16384);
}

template<class World> void advance(World& world) {
    if (!world.drunkVisualIntensity) return;
    // 167A18..167C30 advances at most one period after a strict unsigned expiry.
    if (world.drunkVisualPeriod < uint32_t(world.gameMilliseconds - world.drunkVisualStartedAt)) {
        world.drunkVisualStartedAt += world.drunkVisualPeriod;
        choosePeriod(world, 12288, 24576);
    }
    world.nativeTimeScale = .7f;
}

inline int16_t advanceSteeringDrift(int16_t& stored, uint8_t intensity, int16_t rawAxis) {
    // 2852C0 updates global487A78 before the controls/mode admission checks.
    if (!intensity) {
        stored = 0;
        return 0;
    }
    int drift = stored ? int(stored) : 1;
    const int change = (int(intensity) + 1) >> 1;
    if (drift > 0) drift = rawAxis < -32 ? -1 : drift + change;
    else if (drift < 0) drift = rawAxis >= 33 ? 1 : drift - change;
    stored = int16_t(std::clamp(drift, -96, 96));
    return stored;
}

inline int16_t steeringSample(int16_t delayed, int16_t drift) {
    const uint32_t bits = uint32_t(int32_t(delayed) + int32_t(drift)) & 0xFFFFu;
    return int16_t(bits < 0x8000u ? int32_t(bits) : int32_t(bits) - 0x10000);
}

inline float recoveryAmount(uint8_t intensity, float amount) {
    // CPlayerPed::RestoreSprintEnergy232B28 divides the requested recovery.
    return intensity ? truncatedFloat(double(amount) / (float(intensity) * 5.0f)) : amount;
}

}
