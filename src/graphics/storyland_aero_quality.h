#pragma once
#include <algorithm>
#include <array>
#include <cmath>

struct StorylandAeroQuality {
    const char* name;
    int shadowSize;
    float worldShadowRadius;
    int aoDirections;
    int sunRaySamples;
    bool normalMaps;
    bool subdivision;
    int lightingModel;
    float heightScale;
    int shadowRefreshInterval;
    float bloomStrength;
};

inline const StorylandAeroQuality& storylandAeroQuality(int level) {
    static constexpr StorylandAeroQuality levels[] = {
        {"Low",         512,  62.0f, 0, 0, false, false, 0, 0.000f, 3, 0.00f},
        {"Medium",     1024,  92.0f, 0, 0, true,  false, 1, 0.012f, 2, 0.00f},
        {"High",       1536, 128.0f, 4, 0, true,  false, 2, 0.022f, 2, 0.12f},
        {"Ultra High", 2048, 170.0f, 6, 10, true,  true,  2, 0.030f, 2, 0.22f}
    };
    return levels[std::clamp(level, 0, 3)];
}

inline std::array<float,3> storylandAeroShadowCenter(
    std::array<float,3> center, const std::array<float,3>& right,
    const std::array<float,3>& up, float radius, int resolution) {
    if (!std::isfinite(radius) || radius <= 0 || resolution <= 0) return center;
    const float texel = 2.0f * radius / float(resolution);
    float x = 0.0f, y = 0.0f;
    for (unsigned i = 0; i < 3; ++i) {
        x += center[i] * right[i];
        y += center[i] * up[i];
    }
    const float dx = std::round(x / texel) * texel - x;
    const float dy = std::round(y / texel) * texel - y;
    for (unsigned i = 0; i < 3; ++i) center[i] += right[i] * dx + up[i] * dy;
    return center;
}
