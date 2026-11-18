#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Manhunt 2 window decoration. Animation is derived from one monotonic time
// sample, so Win32 and OpenGL draw the same drop without persistent timers.
inline constexpr std::array<int, 8> storylandBloodPositions = {
    9, 18, 29, 41, 54, 67, 78, 91
};
inline constexpr std::array<int, 8> storylandBloodBaseLengths = {
    4, 6, 3, 5, 7, 4, 6, 4
};

struct StorylandBloodFallingDrop {
    bool falling = false;
    bool impacting = false;
    float x = 0.0f;
    float y = 0.0f;
    float radius = 0.0f;
    float velocity = 0.0f;
    float impactFraction = 0.0f;
};

struct StorylandBloodBorderFrame {
    int activeIndex = -1;
    float accumulation = 0.0f;
    float extension = 0.0f;
    StorylandBloodFallingDrop drop;
};

inline float storylandBloodSmooth(float fraction) {
    const float amount = std::clamp(fraction, 0.0f, 1.0f);
    return amount * amount * (3.0f - 2.0f * amount);
}

inline StorylandBloodBorderFrame storylandSampleBloodBorder(
    uint64_t elapsedMilliseconds, float width, float height) {
    StorylandBloodBorderFrame frame;
    if (width < 100.0f || height < 60.0f) return frame;

    constexpr uint64_t cycleDuration = 9600;
    constexpr uint64_t accumulationDuration = 700;
    constexpr uint64_t detachmentTime = 1620;
    const uint64_t cycle = elapsedMilliseconds / cycleDuration;
    const uint64_t phase = elapsedMilliseconds % cycleDuration;

    // Two quiet intervals in every six cycles keep the effect occasional.
    if (cycle % 6u == 2u || cycle % 6u == 5u) return frame;
    const uint32_t hash = static_cast<uint32_t>((cycle + 17u) * 0x9E3779B9u);
    const int selected = static_cast<int>((hash ^ (hash >> 16u)) & 7u);
    frame.activeIndex = selected;

    if (phase < accumulationDuration) {
        frame.accumulation = storylandBloodSmooth(
            float(phase) / float(accumulationDuration));
        return frame;
    }
    if (phase < detachmentTime) {
        const float progress = storylandBloodSmooth(
            float(phase - accumulationDuration) /
            float(detachmentTime - accumulationDuration));
        frame.accumulation = 1.0f - progress;
        frame.extension = progress * (13.0f + float(selected % 3) * 3.0f);
        return frame;
    }

    // Detach the tip instead of stretching it down and retracting it upward.
    constexpr float gravity = 720.0f;
    const float seconds = float(phase - detachmentTime) * 0.001f;
    const float originY = float(storylandBloodBaseLengths[size_t(selected)]) +
        13.0f + float(selected % 3) * 3.0f;
    const float floorY = height - 3.0f;
    const float fallDistance = std::max(0.0f, floorY - originY);
    const float duration = std::sqrt(2.0f * fallDistance / gravity);

    StorylandBloodFallingDrop& drop = frame.drop;
    drop.radius = 1.4f + float(selected % 3) * 0.18f;
    drop.x = width * float(storylandBloodPositions[size_t(selected)]) / 100.0f
        + std::sin(float(selected) * 1.9f + seconds * 1.6f) *
        (0.6f + seconds * 1.2f);
    if (seconds < duration) {
        drop.falling = true;
        drop.y = std::min(floorY, originY + 0.5f * gravity * seconds * seconds);
        drop.velocity = gravity * seconds;
    } else if (seconds < duration + 0.46f) {
        drop.impacting = true;
        drop.y = floorY;
        drop.impactFraction = std::clamp(
            (seconds - duration) / 0.46f, 0.0f, 1.0f);
    }
    return frame;
}
