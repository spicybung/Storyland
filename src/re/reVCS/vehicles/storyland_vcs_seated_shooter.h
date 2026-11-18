#pragma once

#include <cstdint>

inline bool storylandVcsUsesSeatedInstantHitWrapper(int32_t weapon) {
    return weapon == 19 || weapon == 20 || (weapon >= 24 && weapon <= 29) ||
        weapon == 34 || weapon == 35;
}

inline int32_t storylandVcsSeatedShooterIgnoredVehicle(bool vcs, int32_t weapon, int32_t occupiedHandle,
    bool occupiedDeleted, int32_t occupiedSlot, int32_t physicalHandle) {
    if (!vcs || !storylandVcsUsesSeatedInstantHitWrapper(weapon) ||
        occupiedHandle < 0 || occupiedDeleted || occupiedSlot < 0 ||
        physicalHandle != occupiedHandle) return -1;
    return occupiedSlot;
}
