#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

inline bool storylandVcsKeepStagedEntryRoute(bool nativeAutomobile, bool rightHandSide,
    float heading, const std::array<float,3>& door,
    const std::vector<std::array<float,3>>& route, uint32_t routeIndex,
    float movementSpeed, uint32_t now, uint32_t& lastProgress) {
    if (!nativeAutomobile || route.size() < 3 || routeIndex >= route.size()) return false;
    const float angle = (heading + 90.0f) * 0.0174532925199433f;
    const float side = rightHandSide ? -1.0f : 1.0f;
    const auto& staging = route[route.size() - 2];
    if (std::hypot(staging[0] - (door[0] - std::sin(angle) * side),
        staging[1] - (door[1] + std::cos(angle) * side)) >= .01f) return false;
    if (lastProgress == 0 || movementSpeed > .05f) lastProgress = now;
    if (uint32_t(now - lastProgress) > 1000) {
        lastProgress = now;
        return false;
    }
    return true;
}
