#pragma once

#include "src/storyland/storyland_service_population.h"
#include <algorithm>
#include <cmath>

inline StorylandServiceRole storylandVehicleSirenRole(const StorylandPlayTestRuntime& runtime,
    const StorylandRuntimeVehicle& vehicle) {
    const auto role=storylandVehicleServiceRole(runtime,vehicle);
    if(role!=StorylandServiceRole::Civilian)return role;
    const auto& definition=vehicle.nativeDefinition;
    const auto native=storylandNativeVehicleSirenRole(definition.modelId,definition.vcsModelLayout);
    return native==StorylandServiceRole::Civilian?vehicle.serviceRole:native;
}

inline bool storylandPlayerVehicleHasSiren(const StorylandPlayTestRuntime& runtime) {
    return runtime.occupiedVehicle>=0&&size_t(runtime.occupiedVehicle)<runtime.vehicles.size()&&
        runtime.playerVehicleSeat<0&&storylandEmergencySirenRole(
            storylandVehicleSirenRole(runtime,runtime.vehicles[size_t(runtime.occupiedVehicle)]));
}

class StorylandVehicleSirenControls {
public:
    bool update(StorylandPlayTestRuntime& runtime,bool held,bool enabled,float seconds) {
        const int slot=runtime.occupiedVehicle;
        const bool available=enabled&&runtime.playerControl&&runtime.health>0&&
            !runtime.paused&&!runtime.pauseMenu&&runtime.playerVehicleSeat<0&&
            runtime.vehicleAction==StorylandVehicleAction::None&&slot>=0&&
            size_t(slot)<runtime.vehicles.size();
        if(!available) {
            reset(held);
            return false;
        }
        auto& vehicle=runtime.vehicles[size_t(slot)];
        if(vehicle.health<=0||vehicle.charred||!vehicle.visible) {
            reset(held);
            return false;
        }
        if(slot!=vehicleSlot||vehicle.model!=vehicleModel||vehicle.scriptHandle!=scriptHandle) {
            const bool alreadyHeld=previousHeld;
            reset(alreadyHeld&&held);
            vehicleSlot=slot;
            vehicleModel=vehicle.model;
            scriptHandle=vehicle.scriptHandle;
        }
        if(blockedUntilRelease) {
            blockedUntilRelease=held;
            previousHeld=held;
            return false;
        }
        const bool hasSiren=storylandEmergencySirenRole(storylandVehicleSirenRole(runtime,vehicle));
        if(held) {
            if(!previousHeld)heldSeconds=0;
            if(std::isfinite(seconds))heldSeconds+=std::clamp(seconds,0.0f,.1f);
        } else if(previousHeld) {
            // The original eight-frame horn history distinguishes a tap from
            // a sustained horn. Use its 30 Hz duration independently of FPS.
            if(hasSiren&&heldSeconds<8.0f/30.0f) {
                vehicle.siren=!vehicle.siren;
                vehicle.serviceRole=storylandVehicleSirenRole(runtime,vehicle);
                if(runtime.missionEnvironment&&vehicle.scriptHandle>=0) {
                    const auto found=runtime.missionEnvironment->missionVehicles.find(vehicle.scriptHandle);
                    if(found!=runtime.missionEnvironment->missionVehicles.end())found->second.siren=vehicle.siren;
                }
            }
            heldSeconds=0;
        }
        previousHeld=held;
        return held&&(!hasSiren||heldSeconds>=8.0f/30.0f);
    }

    void reset(bool held=false) {
        vehicleSlot=-1;
        vehicleModel=SIZE_MAX;
        scriptHandle=-1;
        heldSeconds=0;
        previousHeld=held;
        blockedUntilRelease=held;
    }

private:
    int vehicleSlot=-1;
    size_t vehicleModel=SIZE_MAX;
    int32_t scriptHandle=-1;
    float heldSeconds=0;
    bool previousHeld=false,blockedUntilRelease=false;
};
