#pragma once

#include "src/storyland/storyland_play_test_runtime.h"
#include "src/re/reVCS/services/storyland_vcs_medic_triage.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// The host does not expose all retail casualty flags. Restrict the adapter to
// visible, grounded ambient casualties; never take ownership of script actors.
inline bool storylandVcsAmbientMedicCasualty(const StorylandRuntimePed& ped) {
    return ped.scriptHandle < 0 && !ped.emergencyScriptOwned && !ped.serviceVehicleIdentity && ped.stats.valid &&
        ped.turfWarActorId==0 && ped.health == 0.0f && ped.state==StorylandPedState::Dead &&
        ped.renderAlpha > 0 &&
        !ped.populationFadeOut && !ped.water.active && !ped.zombie.active;
}

inline void storylandClearVcsMedicTask(StorylandRuntimePed& medic) {
    medic.emergencyTarget=0;
    medic.emergencyTreatmentTime=0;
    medic.emergencyTreating=false;
}

template<class GroundCheck, class Move>
bool storylandAdvanceVcsEmergencyMedic(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& actors,size_t index,float seconds,
    GroundCheck groundCheck,Move move) {
    if(index>=actors.size()||index>=runtime.pedestrians.size())return false;
    auto& medic=runtime.pedestrians[index];
    if(!runtime.crime.vcs || medic.scriptHandle>=0 || medic.serviceRole!=StorylandServiceRole::Paramedic ||
        medic.emergencyScriptOwned || !medic.stats.valid || medic.stats.pedType!=16 ||
        medic.health<=0 || medic.water.active || medic.zombie.active || medic.hostile || medic.fleeActive ||
        medic.coughTime>0 || medic.threatWaiting ||
        medic.contact.active() || medic.bulletReaction.active(runtime.elapsed) || !std::isfinite(seconds) || seconds<=0) {
        storylandClearVcsMedicTask(medic);
        return false;
    }
    const size_t count=std::min(actors.size(),runtime.pedestrians.size());
    const auto eligible=[&](size_t target) {
        return target!=index && storylandVcsAmbientMedicCasualty(runtime.pedestrians[target]) &&
            groundCheck(actors[target].position);
    };
    size_t target=count;
    if(medic.emergencyTarget)for(size_t candidate=0;candidate<count;++candidate)
        if(runtime.pedestrians[candidate].emergencyIdentity==medic.emergencyTarget && eligible(candidate)) {
            target=candidate;
            break;
        }
    if(target==count) {
        storylandClearVcsMedicTask(medic);
        std::array<StorylandVcsMedicCasualty,20> casualties{};
        std::array<size_t,20> slots{};
        size_t used=0;
        for(size_t candidate=0;candidate<count && used<casualties.size();++candidate) {
            if(!eligible(candidate))continue;
            auto& victim=runtime.pedestrians[candidate];
            if(!victim.emergencyIdentity) {
                if(!runtime.nextEmergencyIdentity)++runtime.nextEmergencyIdentity;
                victim.emergencyIdentity=runtime.nextEmergencyIdentity++;
            }
            auto& casualty=casualties[used];
            casualty.occupied=true;
            casualty.health=victim.health;
            const auto& position=actors[candidate].position;
            casualty.nativePosition={position.x,position.y,position.z};
            for(size_t other=0;other<count;++other) {
                const auto& worker=runtime.pedestrians[other];
                if(worker.health>0 && worker.scriptHandle<0 && worker.emergencyTreating &&
                    worker.emergencyTarget==victim.emergencyIdentity)++casualty.assignedResponders;
            }
            slots[used++]=candidate;
        }
        const auto& position=actors[index].position;
        const auto selected=storylandSelectVcsMedicCasualty(casualties,{position.x,position.y,position.z});
        if(selected.index<0)return false;
        target=slots[size_t(selected.index)];
        medic.emergencyTarget=runtime.pedestrians[target].emergencyIdentity;
    }
    auto& actor=actors[index];
    auto& victim=runtime.pedestrians[target];
    const auto& destination=actors[target].position;
    const float dx=destination.x-actor.position.x,dy=destination.y-actor.position.y;
    const float distance=std::hypot(dx,dy);
    if(!std::isfinite(distance)) {storylandClearVcsMedicTask(medic);return false;}
    if(distance>0)actor.yaw=std::atan2(dy,dx)*57.2957795131f;
    medic.clearGunThreat();
    medic.melee={};medic.ranged={};medic.combatSpeed=0;
    if(distance>=1.0f) {
        // A displaced casualty must be approached again before CPR can resume.
        medic.emergencyTreating=false;
        medic.emergencyTreatmentTime=0;
        medic.state=StorylandPedState::Walk;
        const float advance=std::min(std::max(0.0f,distance-0.95f),runtime.pedestrianWalkSpeed*seconds);
        if(distance>0 && advance>0) {
            auto desired=actor.position;
            desired.x+=dx*advance/distance;
            desired.y+=dy*advance/distance;
            move(actor.position,desired);
        }
        return true;
    }
    medic.state=StorylandPedState::Idle;
    medic.emergencyTreating=true;
    medic.emergencyTreatmentTime+=seconds;
    if(medic.emergencyTreatmentTime>=2.0f) {
        victim.health=100.0f;
        victim.deathTime=0;
        victim.death={};
        victim.populationFadeOut=false;
        victim.state=StorylandPedState::Idle;
        victim.timer=0;
        victim.hostile=false;victim.playOptionHostile=false;
        victim.playerDamaged=false;victim.personallyProvoked=false;
        victim.melee={};victim.ranged={};victim.contact.clear();
        victim.combatSpeed=0;victim.coughTime=0;
        victim.clearFlee();victim.clearGunThreat();
        storylandClearVcsMedicTask(medic);
    }
    return true;
}
