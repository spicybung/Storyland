#pragma once
#include "src/storyland/storyland_mission_vm.h"

inline bool storylandLcsVehiclePositionFrozen(const StorylandMissionEnvironment* world,int32_t handle) {
    if(!world)return false;
    const auto car=world->missionVehicles.find(handle);
    return car!=world->missionVehicles.end()&&!car->second.deleted&&car->second.positionFrozen;
}

inline bool storylandLcsVehicleDoorsLocked(int32_t lock) {
    // CVehicle::CanPedOpenLocks. LCS deliberately does not implement the
    // commented-out CARLOCK_LOCKOUT_PLAYER_ONLY (3) restriction.
    return lock==2||lock==4||lock==5||lock==9;
}
inline bool storylandLcsVehicleDriverProtected(const StorylandMissionEnvironment& world,
    int32_t vehicle,int32_t entrant) {
    const auto car=world.missionVehicles.find(vehicle);
    if(car==world.missionVehicles.end())return false;
    if(world.inVehicle&&world.occupiedMissionVehicle==vehicle&&world.playerPassengerSeat<0&&entrant!=1)
        return world.playerCantBeDraggedOut||!world.control;
    const int32_t driver=car->second.driver;
    const auto ped=world.missionCharacters.find(driver);
    return driver!=entrant&&ped!=world.missionCharacters.end()&&ped->second.cantBeDraggedOut;
}

inline bool storylandLcsVehicleEntryBlocked(const StorylandMissionEnvironment& world,
    int32_t vehicle,int32_t seat,int32_t entrant) {
    const auto car=world.missionVehicles.find(vehicle);
    return car!=world.missionVehicles.end()&&(storylandLcsVehicleDoorsLocked(car->second.doorLock)||
        (seat<0&&storylandLcsVehicleDriverProtected(world,vehicle,entrant)));
}
