#pragma once

#include "src/re/reVCS/services/storyland_vcs_service_crew.h"
#include "src/re/reVCS/services/storyland_vcs_vice_dispatch.h"
#include "src/storyland/storyland_service_population.h"

// Called only once a traffic candidate and its real driver have been placed.
// A failed passenger association leaves the dispatch clock and pair sequence
// untouched so the caller can roll the candidate vehicle back safely.
inline bool storylandCompleteVcsViceCrew(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& pedestrians,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t slot) {
    if(!runtime.crime.vcs||!runtime.missionEnvironment||slot>=cars.size()||slot>=runtime.vehicles.size())return false;
    auto& vehicle=runtime.vehicles[slot];
    const auto& dispatch=runtime.missionEnvironment->vcsViceDispatch;
    if(!vehicle.vcsViceUnit||vehicle.nativeDefinition.modelId!=204||!vehicle.hasDriver||
        vehicle.vcsViceCrewSequence>=4||vehicle.vcsViceCrewSequence!=dispatch.crewSequence||
        vehicle.driverModel>=runtime.pedestrianModelStats.size()||
        vehicle.driverModel>=runtime.pedestrianModelRoles.size())return false;
    const auto& stats=runtime.pedestrianModelStats[vehicle.driverModel];
    if(!stats.valid||stats.modelId!=storylandVcsViceCrewModel(dispatch.crewSequence)||stats.pedType!=6||
        runtime.pedestrianModelRoles[vehicle.driverModel]!=StorylandServiceRole::Vice)return false;
    const uint64_t previousIdentity=vehicle.serviceCrewIdentity;
    const uint64_t previousNextIdentity=runtime.nextEmergencyIdentity;
    if(storylandCreateVcsAmbientPassenger(runtime,pedestrians,cars,slot,vehicle.driverModel,0)<0) {
        vehicle.serviceCrewIdentity=previousIdentity;
        runtime.nextEmergencyIdentity=previousNextIdentity;
        return false;
    }
    storylandCommitVcsViceDispatch(runtime.missionEnvironment->vcsViceDispatch,
        runtime.missionEnvironment->gameMilliseconds);
    return true;
}

inline void storylandUpdateVcsViceExitRequest(StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t slot,
    const StorylandNativePoint& player,float seconds) {
    if(slot>=cars.size()||slot>=runtime.vehicles.size())return;
    auto& vehicle=runtime.vehicles[slot];
    if(!runtime.crime.vcs||!vehicle.vcsViceUnit||!vehicle.policePursuing||!vehicle.hasDriver||
        runtime.wanted<=0||runtime.policeIgnorePlayer||runtime.health<=0||
        !std::isfinite(seconds)||seconds<=0) {
        vehicle.vcsVicePlayerStopMilliseconds=0;vehicle.vcsViceExitRequested=false;return;
    }
    const bool playerInVehicle=runtime.occupiedVehicle>=0;
    float speed=0;
    bool targetUpsideDown=false;
    if(playerInVehicle) {
        const size_t playerSlot=size_t(runtime.occupiedVehicle);
        if(playerSlot>=cars.size()||playerSlot>=runtime.vehicles.size()) {
            vehicle.vcsVicePlayerStopMilliseconds=0;return;
        }
        const auto& target=runtime.vehicles[playerSlot];
        speed=std::hypot(target.speed,target.lateralSpeed,target.verticalSpeed)/StorylandVehicleStepsPerSecond;
        constexpr float radians=.01745329251994329577f;
        const float up=std::cos((target.bodyRoll-target.roadRoll)*radians)*
            std::cos((target.bodyPitch+target.roadPitch)*radians);
        targetUpsideDown=std::isfinite(up)&&up<=-.9f;
        if(std::isfinite(speed)&&speed<.05f)
            vehicle.vcsVicePlayerStopMilliseconds=std::fmod(
                std::floor(vehicle.vcsVicePlayerStopMilliseconds+seconds*1000.0f),65536.0f);
        else vehicle.vcsVicePlayerStopMilliseconds=0;
    } else vehicle.vcsVicePlayerStopMilliseconds=0;
    const float distance=std::hypot(cars[slot].position.x-player.x,cars[slot].position.y-player.y);
    const uint16_t counter=uint16_t(std::clamp(vehicle.vcsVicePlayerStopMilliseconds,0.0f,65535.0f));
    if(storylandVcsViceExitGate(true,distance,playerInVehicle,targetUpsideDown,speed,counter))
        vehicle.vcsViceExitRequested=true;
}

// The original queues an animated exit objective. Until that full native
// ambient-occupant transition is available, wait for the physical car to stop
// and a clear door before transferring its existing virtual driver to a real
// pursuing pedestrian. This prevents duplicate drivers and unsafe placement.
template<class PlaceDriver>
size_t storylandHandoffVcsViceDrivers(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& pedestrians,
    const std::vector<StorylandNativeTrafficActor>& cars,PlaceDriver placeDriver) {
    if(!runtime.crime.vcs||!runtime.missionEnvironment||runtime.paused||runtime.pauseMenu)return 0;
    size_t handedOff=0;
    for(size_t slot=0;slot<cars.size()&&slot<runtime.vehicles.size();++slot) {
        auto& vehicle=runtime.vehicles[slot];
        if(!vehicle.vcsViceUnit||!vehicle.vcsViceExitRequested||!vehicle.hasDriver||
            vehicle.nativeDefinition.modelId!=204||vehicle.vcsViceCrewSequence>=4||
            vehicle.scriptHandle>=0||vehicle.missionDriver||vehicle.coopControlled||vehicle.coopSeatReserved||
            vehicle.health<=0||vehicle.wrecked||!vehicle.visible||!vehicle.clumpVisible||
            int(slot)==runtime.occupiedVehicle||int(slot)==runtime.transitionVehicle||
            !std::isfinite(vehicle.speed)||!std::isfinite(vehicle.lateralSpeed)||
            std::hypot(vehicle.speed,vehicle.lateralSpeed)>=.5f)continue;
        // Runtime saves store native model IDs, not resident array indices.
        // Resolve again after loading so the driver can still disembark.
        const size_t model=storylandVehicleDriverModel(runtime,vehicle);
        if(model>=runtime.pedestrianModelStats.size()||model>=runtime.pedestrianModelRoles.size())continue;
        const auto& stats=runtime.pedestrianModelStats[model];
        if(!stats.valid||stats.pedType!=6||stats.modelId!=storylandVcsViceCrewModel(vehicle.vcsViceCrewSequence)||
            runtime.pedestrianModelRoles[model]!=StorylandServiceRole::Vice)continue;
        if(pedestrians.size()+runtime.missionEnvironment->missionCharacters.size()>=runtime.pedestrians.size())continue;
        const size_t index=pedestrians.size();
        if(runtime.pedestrians[index].scriptHandle>=0||runtime.pedestrians[index].turfWarActorId!=0)continue;
        StorylandNativePoint position{};
        if(!placeDriver(slot,index,position))continue;
        StorylandRuntimePed driver{};
        driver.model=model;driver.stats=stats;driver.serviceRole=StorylandServiceRole::Vice;
        driver.armour=100;driver.policePursuing=true;driver.hostile=true;
        driver.policeLastSeenAt=runtime.elapsed;driver.state=StorylandPedState::Combat;
        driver.policeLastKnownPosition=runtime.playerPosition;driver.policeLastKnownPositionValid=true;
        for(size_t weapon=0;weapon<runtime.weapons.size();++weapon) {
            if(runtime.weapons[weapon].nativeId!=25)continue;
            driver.weapon=weapon;driver.magazine=runtime.weapons[weapon].clipSize;
            driver.ammunition=1000;break;
        }
        StorylandNativeTrafficActor actor{};
        actor.node=actor.nextNode=UINT32_MAX;actor.position=position;actor.yaw=cars[slot].yaw;
        pedestrians.push_back(actor);runtime.pedestrians[index]=driver;
        vehicle.hasDriver=false;vehicle.parked=true;vehicle.speed=0;
        vehicle.policePursuing=false;vehicle.siren=false;vehicle.vcsViceExitRequested=false;
        ++handedOff;
    }
    return handedOff;
}
