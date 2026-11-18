#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace storyland_vcs_jetski_knockoff {

struct Input {
    float pedestrianMass = 0;
    float vehicleMass = 0;
    float nativeNormalImpulse = 0;
    std::array<float, 3> nativeVehicleVelocity{};
    std::array<float, 3> contactNormal{};
    float nativeRandom = 0;
};

struct Output {
    std::array<float, 3> nativeForce{};
    std::array<float, 3> nativeVelocity{};
    int animationGroup = 0;
    int animation = 121;
    uint32_t requestedFallMilliseconds = 1000;
    float initialAnimationTime = .4f;
    float blendRate = 8;
};

inline bool force(const Input& input, Output& output) {
    if (!std::isfinite(input.pedestrianMass) || input.pedestrianMass <= 0 ||
        !std::isfinite(input.vehicleMass) || input.vehicleMass <= 0 ||
        !std::isfinite(input.nativeNormalImpulse) || input.nativeNormalImpulse < 0 ||
        !std::isfinite(input.nativeRandom) || input.nativeRandom < 0 || input.nativeRandom > 1)
        return false;
    for (size_t axis = 0; axis < 3; ++axis)
        if (!std::isfinite(input.nativeVehicleVelocity[axis]) ||
            !std::isfinite(input.contactNormal[axis])) return false;
    Output result;
    const float massRatio = input.pedestrianMass / input.vehicleMass;
    const float scaledMass = massRatio * -.60000002384185791015625f;
    const float contactScale = scaledMass * input.nativeNormalImpulse;
    const float randomLift = input.nativeRandom * 4.0f;
    result.nativeForce = {contactScale * input.contactNormal[0],
        contactScale * input.contactNormal[1], randomLift + 3.0f};
    const float inverseMass = 1.0f / input.pedestrianMass;
    if (!std::isfinite(massRatio) || !std::isfinite(scaledMass) ||
        !std::isfinite(contactScale) || !std::isfinite(inverseMass)) return false;
    for (size_t axis = 0; axis < 3; ++axis) {
        const float delta = result.nativeForce[axis] * inverseMass;
        const float velocity = input.nativeVehicleVelocity[axis] + delta;
        if (!std::isfinite(result.nativeForce[axis]) || !std::isfinite(delta) ||
            !std::isfinite(velocity)) return false;
        result.nativeVelocity[axis] = std::clamp(velocity, -4.0f, 4.0f);
    }
    output = result;
    return true;
}

inline float truncatedFloat(double value) {
    float rounded = float(value);
    if (std::isfinite(rounded) && std::fabs(double(rounded)) > std::fabs(value))
        rounded = std::nextafter(rounded, 0.0f);
    return rounded;
}

inline bool playerGetUpDeadline(uint32_t now, float duration, uint32_t& output) {
    if (!std::isfinite(duration) || duration <= 0) return false;
    const float nativeNow = truncatedFloat(double(now));
    const float milliseconds = truncatedFloat(double(duration) * 1000.0);
    float deadline = truncatedFloat(double(nativeNow) + milliseconds);
    deadline = truncatedFloat(double(deadline) + 500.0);
    if (!std::isfinite(milliseconds) || !std::isfinite(deadline)) return false;
    if (deadline >= 2147483648.0f) output = uint32_t(std::numeric_limits<int32_t>::max());
    else if (deadline <= -2147483648.0f) output = uint32_t(std::numeric_limits<int32_t>::min());
    else output = uint32_t(int32_t(deadline));
    return true;
}

inline bool canBeginGetUp(uint32_t now, uint32_t deadline, bool standing, bool physicalWaterContact) {
    return deadline != std::numeric_limits<uint32_t>::max() &&
        (standing || physicalWaterContact) && deadline < now;
}

}
