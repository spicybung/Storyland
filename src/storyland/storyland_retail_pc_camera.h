#pragma once

#include <algorithm>
#include <cmath>

inline float storylandRetailCameraStick(int rawAxis) {
    const float normalized = std::clamp(float(rawAxis) / 128.0f, -1.0f, 1.0f);
    constexpr float deadzone = 0.17f;
    const float magnitude = std::abs(normalized);
    if (magnitude <= deadzone) return 0.0f;
    const float response = (magnitude - deadzone) / (1.0f - deadzone);
    return std::copysign(response, normalized);
}

inline bool storylandRetailCameraCentrePressed(bool down, bool& previouslyDown) {
    const bool pressed = down && !previouslyDown;
    previouslyDown = down;
    return pressed;
}

