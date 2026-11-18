#pragma once

#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include <algorithm>
#include <vector>

inline size_t storylandServiceCrewVehicleSlot(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& cars,uint64_t identity) {
    if(!identity)return SIZE_MAX;
    for(size_t slot=0;slot<cars.size()&&slot<runtime.vehicles.size();++slot)
        if(runtime.vehicles[slot].serviceCrewIdentity==identity)return slot;
    return SIZE_MAX;
}

inline bool storylandVcsServiceSeatOccupied(const StorylandPlayTestRuntime& runtime,
    const StorylandRuntimeVehicle& vehicle,int32_t seat) {
    if(!vehicle.serviceCrewIdentity||seat<0)return false;
    for(const auto& ped:runtime.pedestrians)
        if(ped.serviceVehicleIdentity==vehicle.serviceCrewIdentity&&ped.servicePassengerSeat==seat&&
            ped.renderAlpha>0)return true;
    return false;
}

inline bool storylandVcsServicePassengerFrame(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& cars,const StorylandRuntimePed& passenger,
    StorylandVehicleMatrix& frame) {
    const size_t slot=storylandServiceCrewVehicleSlot(runtime,cars,passenger.serviceVehicleIdentity);
    if(slot==SIZE_MAX)return false;
    const auto& vehicle=runtime.vehicles[slot];
    const auto& actor=cars[slot];
    frame=storylandVehicleOccupantFrame(vehicle.nativeDefinition,
        {actor.position.x,actor.position.y,actor.position.z},actor.yaw,
        vehicle.nativeDefinition.previewMinimumZ,vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,
        vehicle.bodyPitch+vehicle.roadPitch,unsigned(passenger.servicePassengerSeat+1),actor.water,
        vehicle.craneCarried?&vehicle.craneFrame:nullptr,storylandVcsJetSkiNativeAngularZ(vehicle));
    return std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);});
}

// Called immediately after a native-type ambient service vehicle is created.
// The passenger is a real ambient ped, not a renderer-only duplicate.
inline int storylandCreateVcsAmbientPassenger(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& pedestrians,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t slot,
    size_t model,uint8_t seat,bool restoring=false) {
    if(!runtime.crime.vcs||!runtime.missionEnvironment||slot>=cars.size()||slot>=runtime.vehicles.size())return -1;
    auto& vehicle=runtime.vehicles[slot];
    if((!restoring&&(vehicle.scriptHandle>=0||vehicle.missionDriver||vehicle.coopControlled||!vehicle.hasDriver))||
        vehicle.health<=0||model>=runtime.pedestrianModelStats.size()||
        model>=runtime.pedestrianModelRoles.size())return -1;
    if(restoring&&vehicle.scriptHandle>=0)
        for(const auto& entry:runtime.missionEnvironment->missionCharacters)
            if(entry.second.vehicle==vehicle.scriptHandle&&!entry.second.attached&&entry.second.passengerSeat==seat)return -1;
    const auto& stats=runtime.pedestrianModelStats[model];
    if(!stats.valid||storylandVcsServiceSeatOccupied(runtime,vehicle,seat))return -1;
    if(pedestrians.size()+runtime.missionEnvironment->missionCharacters.size()>=runtime.pedestrians.size())return -1;
    const size_t index=pedestrians.size();
    if(runtime.pedestrians[index].scriptHandle>=0||runtime.pedestrians[index].turfWarActorId!=0)return -1;
    if(!vehicle.serviceCrewIdentity) {
        if(!runtime.nextEmergencyIdentity)++runtime.nextEmergencyIdentity;
        vehicle.serviceCrewIdentity=runtime.nextEmergencyIdentity++;
    }
    StorylandRuntimePed passenger{};
    passenger.model=model;passenger.stats=stats;passenger.serviceRole=runtime.pedestrianModelRoles[model];
    if(storylandLawEnforcementRole(passenger.serviceRole)) {
        int nativeWeapon=stats.modelId==8||stats.modelId==119?19:
            stats.modelId==10||stats.modelId==11?27:25;
        for(size_t index=0;index<runtime.weapons.size();++index)if(runtime.weapons[index].nativeId==nativeWeapon) {
            passenger.weapon=index;passenger.ammunition=1000;break;
        }
        if(stats.modelId==9)passenger.armour=50;
        else if(stats.modelId==10||stats.modelId==11||stats.modelId==117||stats.modelId==118)passenger.armour=100;
    }
    passenger.serviceVehicleIdentity=vehicle.serviceCrewIdentity;passenger.servicePassengerSeat=seat;
    StorylandVehicleMatrix frame;
    if(!storylandVcsServicePassengerFrame(runtime,cars,passenger,frame))return -1;
    StorylandNativeTrafficActor actor{};
    actor.node=UINT32_MAX;actor.nextNode=UINT32_MAX;
    actor.position={frame[12],frame[13],frame[14]};actor.yaw=cars[slot].yaw;
    pedestrians.push_back(actor);
    runtime.pedestrians[index]=passenger;
    return int(index);
}

// Rollback is limited to an actor just appended by the transaction.
inline bool storylandRemoveVcsAmbientPassenger(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& pedestrians,size_t index) {
    if(index>=runtime.pedestrians.size()||index+1!=pedestrians.size()||
        !runtime.pedestrians[index].serviceVehicleIdentity)return false;
    runtime.pedestrians[index]=StorylandRuntimePed{};
    pedestrians.pop_back();return true;
}

inline bool storylandRestorePendingVcsCrew(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& pedestrians,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t slot) {
    if(slot>=runtime.vehicles.size())return false;
    auto& vehicle=runtime.vehicles[slot];
    if(vehicle.pendingCrewModel<0)return true;
    for(size_t model=0;model<runtime.pedestrianModelStats.size();++model) {
        if(!runtime.pedestrianModelStats[model].valid||
            int32_t(runtime.pedestrianModelStats[model].modelId)!=vehicle.pendingCrewModel)continue;
        const int passenger=storylandCreateVcsAmbientPassenger(runtime,pedestrians,cars,slot,model,vehicle.pendingCrewSeat,true);
        if(passenger<0)return false;
        auto& ped=runtime.pedestrians[size_t(passenger)];
        ped.health=vehicle.pendingCrewHealth;ped.armour=vehicle.pendingCrewArmour;
        if(ped.health<=0)ped.state=StorylandPedState::Dead;
        vehicle.pendingCrewModel=-1;
        return true;
    }
    return false;
}

inline void storylandSyncVcsServicePassengers(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& pedestrians,const std::vector<StorylandNativeTrafficActor>& cars) {
    for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size();++index) {
        auto& passenger=runtime.pedestrians[index];
        if(!passenger.serviceVehicleIdentity)continue;
        const size_t slot=storylandServiceCrewVehicleSlot(runtime,cars,passenger.serviceVehicleIdentity);
        if(slot==SIZE_MAX) {
            // An ambient vehicle leaving the pool takes its still-seated crew with it.
            passenger.serviceVehicleIdentity=0;
            passenger.populationFadeOut=true;passenger.renderAlpha=0;
            continue;
        }
        StorylandVehicleMatrix frame;
        if(storylandVcsServicePassengerFrame(runtime,cars,passenger,frame)) {
            pedestrians[index].position={frame[12],frame[13],frame[14]};
            pedestrians[index].yaw=cars[slot].yaw;
        }
    }
}

template<class PlacePassenger>
void storylandReleaseVcsServicePassengers(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& pedestrians,const std::vector<StorylandNativeTrafficActor>& cars,
    PlacePassenger placePassenger) {
    for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size();++index) {
        auto& passenger=runtime.pedestrians[index];
        if(!passenger.serviceVehicleIdentity)continue;
        const size_t slot=storylandServiceCrewVehicleSlot(runtime,cars,passenger.serviceVehicleIdentity);
        if(slot==SIZE_MAX)continue;
        const auto& vehicle=runtime.vehicles[slot];
        const bool destroyed=vehicle.wrecked||vehicle.health<=0;
        const bool playerTookDriver=int(slot)==runtime.occupiedVehicle&&runtime.playerVehicleSeat<0&&
            runtime.vehicleAction==StorylandVehicleAction::None&&std::fabs(vehicle.speed)<.5f;
        if((!destroyed&&!playerTookDriver&&(vehicle.hasDriver||vehicle.missionDriver||!vehicle.parked||vehicle.scriptHandle>=0))||
            vehicle.coopControlled||int(slot)==runtime.transitionVehicle)continue;
        StorylandNativePoint door{};
        if(!placePassenger(slot,index,door))continue;
        passenger.serviceVehicleIdentity=0;
        passenger.state=passenger.health>0?StorylandPedState::Idle:StorylandPedState::Dead;
        pedestrians[index].position=door;pedestrians[index].yaw=cars[slot].yaw;
    }
}
