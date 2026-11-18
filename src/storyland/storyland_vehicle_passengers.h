#pragma once
#include "src/leeds/leeds_vehicle.h"

inline int storylandNativePassengerCapacity(const StorylandVehicleDefinition& definition) {
    if (definition.nativeType < 0) return 0;
    const int32_t model = int32_t(definition.modelId);
    int doors = definition.numberOfDoors;
    if (definition.vcsModelLayout) {
        // SLUS_215.90 GetMaximumNumberOfPassengersFromNumberOfDoors, 395868.
        if (definition.nativeType == 8 || (definition.modelFlags & 0x4200u)) return 0;
        if (definition.nativeType == 1) return 1;
        if (model == -939) doors = 3;
        else if (model == 194) doors = 2;
        else if (model == 275) doors = 1;
        if (doors == 0) return model == -943 || model == 174 ? 0 : 1;
        return model == 237 ? 8 : doors - 1;
    }
    // SLUS_214.23 equivalent, 2B95E8; its model IDs and boat rule differ.
    if (definition.nativeType == 1) return 0;
    if (model == 197) doors = 3;
    else if (model == 138) doors = 2;
    else if (model == 213) doors = 1;
    if (doors == 0) return model == 169 || model == -963 ? 0 : 1;
    return model == 165 ? 8 : doors - 1;
}
