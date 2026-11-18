#pragma once

#include "src/storyland/storyland_mission_vm.h"

#include <cmath>

inline bool storylandVcsVehiclePosition(const StorylandMissionEnvironment& world,int32_t handle,
    std::array<float,3>& position,std::string& error) {
    const auto vehicle=world.missionVehicles.find(handle);
    if(vehicle==world.missionVehicles.end()||vehicle->second.deleted) {
        error="Vehicle position query references an unknown live car.";
        return false;
    }
    std::array<float,16> matrix{};
    if(!world.vehicleWorldMatrix||!world.vehicleWorldMatrix(handle,matrix)) {
        error="Vehicle position query requires its live native world matrix.";
        return false;
    }
    const std::array<float,3> nativePosition{matrix[12],matrix[13],matrix[14]};
    for(float coordinate:nativePosition)if(!std::isfinite(coordinate)) {
        error="Vehicle position query returned a non-finite native coordinate.";
        return false;
    }
    position=nativePosition;
    return true;
}
