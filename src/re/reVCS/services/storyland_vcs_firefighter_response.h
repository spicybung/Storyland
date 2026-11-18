#pragma once

#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/re/reVCS/services/storyland_vcs_fire_selection.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

inline void storylandClearVcsFirefighterTask(StorylandRuntimePed& firefighter) {
    firefighter.emergencyFireHandle=-1;
    firefighter.emergencyFireDeadline=0;
    firefighter.emergencyFireVehicle=0;
    firefighter.emergencyExtinguishing=false;
}

// Unreserved native fires may follow an entity. Resolve the live attachment
// rather than a cached fire position; script-reserved fires remain ineligible.
inline bool storylandVcsFireTargetPosition(const StorylandMissionEnvironment& environment,
    const StorylandMissionEnvironment::ScriptFire& fire,StorylandNativePoint& position) {
    if(fire.attachedKind==0)position={fire.position[0],fire.position[1],fire.position[2]};
    else if(fire.attachedKind==1) {
        if(fire.attachedHandle==0) {
            if(environment.health<=0)return false;
            position={environment.x,environment.y,environment.z};
        } else {
            const auto actor=environment.missionCharacters.find(fire.attachedHandle);
            if(actor==environment.missionCharacters.end()||!actor->second.alive||
                actor->second.health<=0||actor->second.removeFromWorld)return false;
            position={actor->second.x,actor->second.y,actor->second.z};
        }
    } else if(fire.attachedKind==2) {
        const auto vehicle=environment.missionVehicles.find(fire.attachedHandle);
        if(vehicle==environment.missionVehicles.end()||vehicle->second.deleted)return false;
        position={vehicle->second.x,vehicle->second.y,vehicle->second.z};
    } else return false;
    return std::isfinite(position.x)&&std::isfinite(position.y)&&std::isfinite(position.z);
}

// 2BD520 selects active, unreserved fire slots by strict planar distance.
// Script-created fires reserve their slot in retail, so the host excludes them.
inline int32_t storylandSelectVcsAmbientFire(const StorylandMissionEnvironment& environment,
    const StorylandNativePoint& position) {
    std::array<StorylandVcsFireSelectionEntry,40> fires{};
    for(const auto& entry:environment.scriptFires) {
        const auto& fire=entry.second;
        if(entry.first<0||entry.first>=40||int32_t(environment.gameMilliseconds-fire.endsAt)>=0)continue;
        StorylandNativePoint position{};
        if(!storylandVcsFireTargetPosition(environment,fire,position))continue;
        fires[size_t(entry.first)]={fire.active,fire.scriptOwned,fire.attachedKind!=0,position.x,position.y};
    }
    return storylandSelectVcsFire(fires,position.x,position.y).index;
}

template<class Move,class ClearLine,class Suppress>
bool storylandAdvanceVcsFirefighter(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& actors,const std::vector<StorylandNativeTrafficActor>& cars,
    size_t index,float seconds,Move move,ClearLine clearLine,Suppress suppress) {
    if(index>=actors.size()||index>=runtime.pedestrians.size())return false;
    auto& ped=runtime.pedestrians[index];
    if(!runtime.crime.vcs||!runtime.missionEnvironment||ped.scriptHandle>=0||ped.emergencyScriptOwned||
        ped.turfWarActorId!=0||ped.renderAlpha==0||
        ped.serviceRole!=StorylandServiceRole::Firefighter||!ped.stats.valid||ped.stats.pedType!=17||
        ped.health<=0||ped.water.active||ped.zombie.active||ped.hostile||ped.fleeActive||
        ped.coughTime>0||ped.threatWaiting||ped.contact.active()||ped.bulletReaction.active(runtime.elapsed)||
        !std::isfinite(seconds)||seconds<=0) {
        storylandClearVcsFirefighterTask(ped);
        return false;
    }
    auto& environment=*runtime.missionEnvironment;
    auto& actor=actors[index];
    const int32_t target=storylandSelectVcsAmbientFire(environment,actor.position);
    auto fire=target>=0?environment.scriptFires.find(target):environment.scriptFires.end();
    StorylandNativePoint targetPosition{};
    float closest=999999.0f;
    if(fire!=environment.scriptFires.end()) {
        if(!storylandVcsFireTargetPosition(environment,fire->second,targetPosition)) {
            storylandClearVcsFirefighterTask(ped);return false;
        }
        closest=std::hypot(targetPosition.x-actor.position.x,targetPosition.y-actor.position.y);
    }
    size_t vehicleSlot=cars.size();
    for(size_t slot=0;slot<cars.size()&&slot<runtime.vehicles.size();++slot) {
        auto& vehicle=runtime.vehicles[slot];
        if(!vehicle.burning) {vehicle.emergencyFireIdentity=0;vehicle.emergencyFireStrength=1;continue;}
        // Ambient engine flames are host effects, not native fire-pool records.
        // Never take over a scripted, player-occupied, parked generator or network vehicle.
        if(vehicle.scriptHandle>=0||vehicle.missionDriver||vehicle.generator>=0||vehicle.savedGarage>=0||
            vehicle.coopControlled||vehicle.craneCarried||vehicle.wrecked||!vehicle.canBeDamaged||vehicle.health<=0||
            int(slot)==runtime.occupiedVehicle||int(slot)==runtime.transitionVehicle)continue;
        const auto protectedEffect=[&](const StorylandRuntimeEffect& effect) {
            return effect.vehicle==int(slot)&&(effect.permanent||effect.scriptFireHandle>=0);
        };
        if(std::any_of(runtime.effects.begin(),runtime.effects.end(),protectedEffect)||
            std::any_of(runtime.worldEffects.begin(),runtime.worldEffects.end(),protectedEffect))continue;
        const float distance=std::hypot(cars[slot].position.x-actor.position.x,
            cars[slot].position.y-actor.position.y);
        if(distance<closest) {closest=distance;vehicleSlot=slot;targetPosition=cars[slot].position;}
    }
    const bool vehicleIncident=vehicleSlot<cars.size();
    if(!vehicleIncident&&fire==environment.scriptFires.end()) {
        storylandClearVcsFirefighterTask(ped);return false;
    }
    if(vehicleIncident) {
        auto& vehicle=runtime.vehicles[vehicleSlot];
        if(!vehicle.emergencyFireIdentity) {
            if(!runtime.nextEmergencyIdentity)++runtime.nextEmergencyIdentity;
            vehicle.emergencyFireIdentity=runtime.nextEmergencyIdentity++;
        }
        if(ped.emergencyFireVehicle!=vehicle.emergencyFireIdentity)ped.emergencyExtinguishing=false;
        ped.emergencyFireVehicle=vehicle.emergencyFireIdentity;
        ped.emergencyFireHandle=-1;ped.emergencyFireDeadline=0;
    } else {
        if(ped.emergencyFireVehicle||ped.emergencyFireHandle!=target||
            ped.emergencyFireDeadline!=fire->second.endsAt)ped.emergencyExtinguishing=false;
        ped.emergencyFireHandle=target;
        ped.emergencyFireDeadline=fire->second.endsAt;
        ped.emergencyFireVehicle=0;
    }
    const float dx=targetPosition.x-actor.position.x,dy=targetPosition.y-actor.position.y;
    const float distance=std::hypot(dx,dy);
    if(!std::isfinite(distance)) {storylandClearVcsFirefighterTask(ped);return false;}
    if(distance>0)actor.yaw=std::atan2(dy,dx)*57.2957795131f;
    ped.melee={};ped.ranged={};ped.combatSpeed=0;ped.clearGunThreat();
    const bool visible=clearLine(actor.position,targetPosition);
    if(distance>=5.0f||!visible) {
        ped.emergencyExtinguishing=false;
        ped.state=StorylandPedState::Flee;
        const float stoppingDistance=visible?4.5f:.95f;
        const float advance=std::min(std::max(0.0f,distance-stoppingDistance),runtime.pedestrianRunSpeed*seconds);
        if(distance>0&&advance>0) {
            auto desired=actor.position;
            desired.x+=dx*advance/distance;desired.y+=dy*advance/distance;
            move(actor.position,desired);
        }
        return true;
    }
    ped.state=StorylandPedState::Idle;
    ped.emergencyExtinguishing=true;
    suppress(target,vehicleIncident?int32_t(vehicleSlot):-1);
    if(vehicleIncident?!runtime.vehicles[vehicleSlot].burning:!fire->second.active)
        storylandClearVcsFirefighterTask(ped);
    return true;
}
