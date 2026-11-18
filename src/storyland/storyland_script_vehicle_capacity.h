#pragma once

#include <cstddef>

template<class Vehicles>
bool storylandScriptVehicleCapacityAvailable(const Vehicles& vehicles, size_t activeLimit) {
    // VCS 47ED00/47EDA0 reuse freed slots; saved tombstones retain old identities.
    if (activeLimit == 0 || vehicles.size() >= 4096) return false;
    size_t active = 0;
    for (const auto& entry : vehicles) {
        if (!entry.second.deleted && ++active >= activeLimit) return false;
    }
    return true;
}
