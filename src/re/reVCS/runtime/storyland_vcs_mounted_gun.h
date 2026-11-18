#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

constexpr int32_t storylandVcsMountedGunGroup = 89;
constexpr int32_t storylandVcsMountedGunAimAnimation = 413;
constexpr int32_t storylandVcsMountedGunIdleAnimation = 414;

struct StorylandVcsMountedGunRequest {
    int32_t character = -1;
    int32_t target = -1;
    bool vehicle = false;
    bool fire = false;
};

struct StorylandVcsMountedGunAim {
    std::array<float, 3> direction{};
    float rightDot = 0;
    float forwardDot = 0;
    float animationTime = 0;
    bool inFront = false;
};

// SLUS_215.90: CPed::... at 217AF8, aim calculation 217BE4..217D6C.
inline bool storylandVcsEvaluateMountedGunAim(const std::array<float, 16>& frame,
    const std::array<float, 3>& target, float previousTime, float duration,
    bool repeating, StorylandVcsMountedGunAim& result) {
    result = {};
    if (!std::isfinite(previousTime) || previousTime < 0 ||
        !std::isfinite(duration) || duration <= 0) return false;
    for (size_t axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(frame[axis]) || !std::isfinite(frame[4 + axis]) ||
            !std::isfinite(frame[12 + axis]) || !std::isfinite(target[axis])) return false;
        result.direction[axis] = target[axis] - frame[12 + axis];
    }
    const float squaredLength = result.direction[0] * result.direction[0] +
        result.direction[1] * result.direction[1] + result.direction[2] * result.direction[2];
    if (!std::isfinite(squaredLength)) return false;
    if (squaredLength > 0) {
        const float inverseLength = 1.0f / std::sqrt(squaredLength);
        for (float& component : result.direction) component *= inverseLength;
    } else {
        result.direction[0] = 1;
    }
    // The original normalizes in 3D and then clears Z without normalizing again.
    result.direction[2] = 0;
    for (size_t axis = 0; axis < 3; ++axis) {
        result.rightDot += frame[axis] * result.direction[axis];
        result.forwardDot += frame[4 + axis] * result.direction[axis];
    }
    const float targetTime = (result.rightDot + 1.0f) * 0.5f * duration;
    const float nextTime = previousTime + (targetTime - previousTime) * 0.1f;
    if (!std::isfinite(nextTime) || !std::isfinite(result.forwardDot)) return false;
    // SetCurrentTime at 18EB70 leaves a negative input unchanged.
    result.animationTime = nextTime < 0 ? previousTime : nextTime;
    if (nextTime >= duration) {
        result.animationTime = repeating ? std::fmod(nextTime, duration) : duration;
    }
    result.inFront = result.forwardDot > 0;
    return true;
}

// SLUS_215.90: 216690. Timers are the existing persisted CPed attack timers.
template<class AttackTimers, class Random>
inline bool storylandVcsMountedGunBurst(AttackTimers& timers, uint32_t now,
    bool crouchRequested, Random random) {
    if (timers.shootDeadline != 0) {
        if (timers.shootDeadline < now) {
            timers.setAttack(now, 500, random);
            timers.shootDeadline = 0;
        }
        return true;
    }
    if (timers.attackDeadline != 0) {
        if (timers.attackDeadline < now) {
            const uint32_t duration = 500 +
                uint32_t((uint64_t(random()) * 500u) >> 31);
            timers.setShoot(now, duration, crouchRequested, random);
            timers.attackDeadline = 0;
        }
        return false;
    }
    if (((uint64_t(random()) * 2u) >> 31) != 0) timers.attackDeadline = 1;
    else timers.shootDeadline = 1;
    return false;
}
