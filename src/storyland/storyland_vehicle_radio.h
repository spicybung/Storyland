#pragma once
#include "src/storyland/storyland_service_population.h"

inline bool storylandNativeVehicleUsesPoliceRadio(uint32_t model, bool vcs) {
    if (vcs) {
        // SLUS_215.90, cMusicManager::UsesPoliceRadio at 0x18BA10.
        switch (model) {
        case 191: case 193: case 194: case 207: case 219:
        case 227: case 234: case 261: case 275: case 279:
            return true;
        default:
            return false;
        }
    }
    // SLUS_214.23, cMusicManager::UsesPoliceRadio at 0x16A298.
    switch (model) {
    case 138: case 147: case 148: case 157: case 158: case 196:
        return true;
    default:
        return false;
    }
}

inline bool storylandVehicleUsesPoliceRadio(const StorylandPlayTestRuntime& runtime,
    const StorylandRuntimeVehicle& vehicle) {
    if (storylandNativeVehicleUsesPoliceRadio(vehicle.nativeDefinition.modelId, runtime.crime.vcs))
        return true;
    const auto role = storylandVehicleServiceRole(runtime, vehicle);
    return storylandEmergencySirenRole(role);
}

inline bool storylandOccupiedVehicleUsesPoliceRadio(const StorylandPlayTestRuntime& runtime) {
    return runtime.occupiedVehicle >= 0 && size_t(runtime.occupiedVehicle) < runtime.vehicles.size() &&
        storylandVehicleUsesPoliceRadio(runtime, runtime.vehicles[size_t(runtime.occupiedVehicle)]);
}
