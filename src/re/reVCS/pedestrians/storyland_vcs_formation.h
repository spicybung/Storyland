#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// SLUS_215.90: 0x40F810 computes a new point each time the objective
// requests one. `randomValue` is the next 31-bit native random result.
inline std::array<float,3> storylandVcsFormationTarget(int32_t formation,
    const std::array<float,3>& position, float headingDegrees, uint32_t randomValue) {
    static constexpr std::array<std::array<int,2>,9> offsets{{
        {0,0}, {0,-1}, {-1,-1}, {1,-1}, {-1,1},
        {1,1}, {-1,0}, {1,0}, {0,1}
    }};
    const auto offset=formation>=0&&formation<int32_t(offsets.size()) ?
        offsets[std::size_t(formation)] : offsets[0];
    const float randomFraction=float(randomValue)*(1.0f/2147483648.0f);
    const float distance=(1.0f+randomFraction*0.25f)*1.75f;
    const float localX=float(offset[0])*distance;
    const float localY=float(offset[1])*distance;
    const float angle=headingDegrees*0.01745329251994329577f;
    const float cosine=std::cos(angle),sine=std::sin(angle);
    return {position[0]+cosine*localX-sine*localY,
        position[1]+sine*localX+cosine*localY,position[2]};
}


struct StorylandPatrolMember {
    uint64_t group = 0;
    uint8_t role = 0;
    uint32_t spacingRandom = 0;
    float blockedSeconds = 0;
    bool running = false;
    void clear() { *this = {}; }
};

struct StorylandPatrolStep {
    std::array<float,3> position{};
    float heading = 0;
    bool running = false;
};

inline StorylandPatrolStep storylandStepPatrolFollower(const std::array<float,3>& position,
    float heading, const std::array<float,3>& target, float walkSpeed, float runSpeed,
    float seconds) {
    StorylandPatrolStep result{position, heading, false};
    if (!std::isfinite(seconds) || seconds <= 0 || !std::isfinite(heading) ||
        !std::isfinite(walkSpeed) || !std::isfinite(runSpeed)) return result;
    for (size_t axis = 0; axis < 3; ++axis)
        if (!std::isfinite(position[axis]) || !std::isfinite(target[axis])) return result;
    const float dx = target[0] - position[0], dy = target[1] - position[1];
    const float distance = std::hypot(dx, dy);
    if (distance <= 1.0f || std::fabs(target[2] - position[2]) > 1.0f) return result;
    constexpr float radians = 0.01745329251994329577f;
    const float turn = std::remainder(std::atan2(dy, dx) / radians - heading, 360.0f);
    const float step = std::min(seconds, 0.1f);
    result.heading = std::remainder(heading + std::clamp(turn, -180.0f * step, 180.0f * step), 360.0f);
    result.running = distance > 3.0f;
    const float speed = std::max(0.0f, result.running ? runSpeed : walkSpeed);
    const float alignment = std::max(0.0f, std::cos(turn * radians));
    const float advance = std::min(distance - 1.0f, speed * step * alignment);
    result.position[0] += std::cos(result.heading * radians) * advance;
    result.position[1] += std::sin(result.heading * radians) * advance;
    return result;
}
