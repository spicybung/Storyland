#pragma once

#include <array>
#include <cmath>
#include <cstddef>

struct StorylandVcsFireSelectionEntry {
    bool active = false;
    bool reserved = false;
    bool attached = false;
    float x = 0;
    float y = 0;
};

struct StorylandVcsFireSelectionResult {
    int index = -1;
    float distance = 999999.0f;
};

inline StorylandVcsFireSelectionResult storylandSelectVcsFire(
    const std::array<StorylandVcsFireSelectionEntry, 40>& fires,
    float x, float y, bool requireAttached = false) {
    StorylandVcsFireSelectionResult result;
    for (std::size_t index = 0; index < fires.size(); ++index) {
        const auto& fire = fires[index];
        if (!fire.active || fire.reserved || (requireAttached && !fire.attached)) continue;
        const float dx = fire.x - x;
        const float dy = fire.y - y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance < result.distance) {
            result.distance = distance;
            result.index = int(index);
        }
    }
    return result;
}
