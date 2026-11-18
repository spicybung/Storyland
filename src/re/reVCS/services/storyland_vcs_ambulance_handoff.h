#pragma once

#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_service_vehicle.h"
#include "src/storyland/storyland_service_population.h"
#include "src/re/reVCS/services/storyland_vcs_emergency_medic.h"
#include "src/re/reVCS/services/storyland_vcs_medic_triage.h"
#include <array>
#include <cmath>
#include <vector>

// 409388..409408: a medic in a stopped vehicle can leave once the original
// casualty selector finds a target at strictly less than 25 planar units and
// the vehicle's native velocity magnitude is strictly below 0.01 per step.
// Host admission uses the narrower, approximate ambient-casualty adapter.
template<class GroundCheck,class PlaceDriver>
size_t storylandHandoffVcsAmbulanceDrivers(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& pedestrians,
    const std::vector<StorylandNativeTrafficActor>& cars,GroundCheck groundCheck,PlaceDriver placeDriver) {
    if(!runtime.crime.vcs||!runtime.missionEnvironment||runtime.paused||runtime.pauseMenu)return 0;
    auto& environment=*runtime.missionEnvironment;
    size_t handedOff=0;
    for(size_t slot=0;slot<cars.size()&&slot<runtime.vehicles.size();++slot) {
        auto& vehicle=runtime.vehicles[slot];
        if((vehicle.serviceAutopilot&&!vehicle.serviceAutopilot->arrived)||vehicle.nativeDefinition.modelId!=227||
            storylandVehicleServiceRole(runtime,vehicle)!=StorylandServiceRole::Paramedic||
            !vehicle.hasDriver||vehicle.missionDriver||vehicle.scriptHandle>=0||vehicle.generator>=0||
            vehicle.savedGarage>=0||vehicle.coopControlled||vehicle.coopSeatReserved||vehicle.craneCarried||
            vehicle.positionFrozen||vehicle.wrecked||!vehicle.visible||!vehicle.clumpVisible||vehicle.health<=0||
            int(slot)==runtime.occupiedVehicle||int(slot)==runtime.transitionVehicle||
            !std::isfinite(vehicle.speed)||!std::isfinite(vehicle.lateralSpeed)||!std::isfinite(vehicle.verticalSpeed)||
            !(std::hypot(vehicle.speed,vehicle.lateralSpeed,vehicle.verticalSpeed)/StorylandVehicleStepsPerSecond<.01f))
            continue;
        const size_t model=vehicle.driverModel;
        if(model>=runtime.pedestrianModelStats.size()||model>=runtime.pedestrianModelRoles.size())continue;
        const auto& stats=runtime.pedestrianModelStats[model];
        if(!stats.valid||stats.pedType!=16||stats.modelId!=12||
            runtime.pedestrianModelRoles[model]!=StorylandServiceRole::Paramedic)continue;
        const auto& position=cars[slot].position;
        if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z)||
            !std::isfinite(cars[slot].yaw)||!std::isfinite(vehicle.halfWidth)||vehicle.halfWidth<=0)continue;
        std::array<StorylandVcsMedicCasualty,20> casualties{};
        size_t used=0;
        const size_t count=std::min(pedestrians.size(),runtime.pedestrians.size());
        for(size_t candidate=0;candidate<count&&used<casualties.size();++candidate) {
            const auto& victim=runtime.pedestrians[candidate];
            if(!storylandVcsAmbientMedicCasualty(victim)||!groundCheck(pedestrians[candidate].position))continue;
            auto& entry=casualties[used++];
            entry.occupied=true;entry.health=victim.health;
            const auto& location=pedestrians[candidate].position;
            entry.nativePosition={location.x,location.y,location.z};
            if(victim.emergencyIdentity)for(size_t worker=0;worker<count;++worker) {
                const auto& medic=runtime.pedestrians[worker];
                if(medic.health>0&&medic.scriptHandle<0&&medic.emergencyTreating&&
                    medic.emergencyTarget==victim.emergencyIdentity)++entry.assignedResponders;
            }
        }
        const auto selected=storylandSelectVcsMedicCasualty(casualties,{position.x,position.y,position.z});
        if(selected.index<0||!(selected.horizontalDistance<25.0f))continue;
        if(pedestrians.size()+environment.missionCharacters.size()>=runtime.pedestrians.size())continue;
        const size_t pedSlot=pedestrians.size();
        const auto& unused=runtime.pedestrians[pedSlot];
        if(unused.scriptHandle>=0||unused.turfWarActorId!=0)continue;
        StorylandNativePoint door{};
        if(!placeDriver(slot,pedSlot,door))continue;
        StorylandRuntimePed medic{};
        medic.model=model;medic.stats=stats;medic.serviceRole=StorylandServiceRole::Paramedic;
        StorylandNativeTrafficActor actor{};
        actor.node=UINT32_MAX;actor.nextNode=UINT32_MAX;actor.position=door;actor.yaw=cars[slot].yaw;
        pedestrians.push_back(actor);
        runtime.pedestrians[pedSlot]=medic;
        vehicle.hasDriver=false;vehicle.parked=true;vehicle.speed=0;
        ++handedOff;
    }
    return handedOff;
}
