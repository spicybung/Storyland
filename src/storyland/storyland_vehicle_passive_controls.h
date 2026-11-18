#pragma once
#include "src/re/reVCS/runtime/storyland_vcs_retail_quirks.h"
#include <cstdint>

enum class StorylandPassiveVehicleStatus : uint8_t {
    Abandoned = 4,
    Wrecked = 5,
    PlayerDisabled = 13
};

struct StorylandPassiveVehicleControls {
    float steering = 0;
    float throttle = 0;
    float brake = 0;
    bool handbrake = false;
    bool clearBikeLean = false;
};

inline StorylandPassiveVehicleControls storylandPassiveVehicleControls(bool vcs, bool bike,
    StorylandPassiveVehicleStatus status, float nativeSpeedSquared, float priorSteering,
    bool standing, bool carjacked, bool driverMustBrake, uint32_t handlingFlags) {
    // LCS car 0x129C24 / bike 0x158280; VCS car 0x13BCB4 / bike 0x337B60.
    StorylandPassiveVehicleControls result;
    const bool slow = nativeSpeedSquared < 0.01f;
    if (status == StorylandPassiveVehicleStatus::Abandoned) {
        if (bike) {
            result.steering = priorSteering;
            result.handbrake = slow || standing;
            result.clearBikeLean = true;
        } else {
            result.brake = slow ? 0.2f : 0.0f;
        }
        if (carjacked) {
            result.brake = 1.0f;
            result.handbrake = true;
        }
    } else if (status == StorylandPassiveVehicleStatus::Wrecked) {
        result.brake = 0.05f;
        result.handbrake = true;
        result.clearBikeLean = bike;
    } else {
        result.handbrake = slow || (!bike && driverMustBrake);
        result.brake = result.handbrake ? 1.0f : 0.0f;
        result.clearBikeLean = bike;
    }
    result.handbrake = StorylandVcsRetailQuirks::handbrakeEnabled(vcs, handlingFlags, result.handbrake);
    return result;
}
