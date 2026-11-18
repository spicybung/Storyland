#pragma once

#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_vehicle_transition.h"
#include <limits>

inline bool storylandVcsCarjackedContinuation(const StorylandVcsPedFallState& state) {
    return !state.recovering&&state.fallAnimation>=59&&state.fallAnimation<=62;
}

inline bool storylandBeginVcsCarjackedGetUp(StorylandMissionEnvironment& world,
    int32_t handle,int32_t vehicle,bool low,bool right,
    float elapsed=std::numeric_limits<float>::infinity()) {
    const auto found=world.missionCharacters.find(handle);
    if(found==world.missionCharacters.end()||!world.animationDuration||
        !world.animationAssociationFlags||world.vcsPedFalls.count(handle))return false;
    auto& character=found->second;
    if(character.vehicle>=0||character.attached||!character.alive||character.health<=0)return false;
    StorylandVcsPedFallState recovery;
    recovery.sourceVehicle=vehicle;
    recovery.fallAnimation=int32_t(storylandVehicleAssociation("jacked",low,right,true));
    recovery.fallDuration=world.animationDuration(0,recovery.fallAnimation);
    recovery.getUpAnimation=132;recovery.getUpDuration=world.animationDuration(0,132);
    recovery.getUpAfter=world.gameMilliseconds;
    recovery.recovering=elapsed>=recovery.fallDuration;
    recovery.time=recovery.recovering?0:std::clamp(elapsed,0.0f,recovery.fallDuration);
    uint16_t flags=0;
    if(!world.animationAssociationFlags(0,132,flags)||(flags&2u)||
        !storylandValidVcsPedFall(recovery))return false;
    if(!character.nativePedRandomSeedInitialized) {
        character.nativePedRandomSeed=uint8_t(world.nextNativeRandom());
        character.nativePedRandomSeedInitialized=true;
    }
    character.animationId=-1;character.movementSpeed=0;character.walkingToTarget=false;
    world.scriptedCharacterAnimations.erase(handle);
    world.characterWanderTasks.erase(handle);world.characterTargets.erase(handle);
    storylandClearCrouch(character.crouch,world.gameMilliseconds);
    world.vcsPedFalls.emplace(handle,recovery);
    return true;
}
