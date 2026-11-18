#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test_runtime.h"

inline bool storylandLcsVehicleDamageSourceAllowed(const StorylandPlayTestRuntime& runtime,
    size_t slot,bool playerAttack) {
    if(slot>=runtime.vehicles.size())return false;
    const auto& vehicle=runtime.vehicles[slot];
    bool restricted=vehicle.onlyDamagedByPlayer;
    bool playerStatus=int(slot)==runtime.occupiedVehicle;
    if(runtime.missionEnvironment) {
        const auto found=runtime.missionEnvironment->missionVehicles.find(vehicle.scriptHandle);
        if(found!=runtime.missionEnvironment->missionVehicles.end()) {
            restricted=found->second.onlyDamagedByPlayer;
            if(found->second.scriptStatus>=0)playerStatus=found->second.scriptStatus==0;
        }
    }
    // CVehicle::InflictDamage and CAutomobile::VehicleDamage exempt
    // STATUS_PLAYER. A scripted physics boat can carry the player without
    // having that status. Unlike the ped flag, explosions are not exempt.
    return !restricted||playerStatus||playerAttack;
}
