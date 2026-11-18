#pragma once

#include "src/storyland/storyland_play_test_runtime.h"

inline bool storylandControlledVehicleIsMotorcycle(const StorylandPlayTestRuntime& runtime) {
    int32_t slot = runtime.occupiedVehicle;
    if (runtime.missionEnvironment) {
        const auto& world = *runtime.missionEnvironment;
        const int32_t remote = runtime.crime.vcs ? world.remoteVehicle : world.remoteControl.vehicle;
        if (remote >= 0) {
            const auto vehicle = world.missionVehicles.find(remote);
            slot = vehicle == world.missionVehicles.end() ? -1 : vehicle->second.spawnedSlot;
        }
    }
    return slot >= 0 && size_t(slot) < runtime.vehicles.size() &&
        runtime.vehicles[size_t(slot)].nativeDefinition.motorcycle();
}
