#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

struct StorylandVcsMedicCasualty {
    bool occupied = false;
    float health = 0.0f;
    uint32_t nativeStatus = 0;
    int32_t assignedResponders = 0;
    std::array<float,3> nativePosition{};
};

struct StorylandVcsMedicSelection {
    int index = -1;
    float horizontalDistance = 999999.0f;
};

inline StorylandVcsMedicSelection storylandSelectVcsMedicCasualty(
    const std::array<StorylandVcsMedicCasualty,20>& casualties,
    const std::array<float,3>& responderPosition) {
    for (int assigned = 0; assigned < 2; ++assigned) {
        StorylandVcsMedicSelection selected;
        for (size_t index = 0; index < casualties.size(); ++index) {
            const auto& casualty = casualties[index];
            if (!casualty.occupied || casualty.nativeStatus == 2 || casualty.health != 0.0f ||
                casualty.assignedResponders != assigned) continue;
            const float dx = casualty.nativePosition[0] - responderPosition[0];
            const float dy = casualty.nativePosition[1] - responderPosition[1];
            const float dz = casualty.nativePosition[2] - responderPosition[2];
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (dz < distance * 0.5f && distance < selected.horizontalDistance) {
                selected.index = int(index);
                selected.horizontalDistance = distance;
            }
        }
        if (selected.index >= 0) return selected;
    }
    return {};
}
