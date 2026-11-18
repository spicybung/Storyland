#pragma once
#include "src/storyland/storyland_play_test.h"
#include "src/storyland/storyland_play_test_weapons.h"

inline bool storylandSampleCrouchPose(StorylandMissionEnvironment& world,
    StorylandPlayTestRuntime& runtime,int32_t handle,StorylandPlayTestAnimation& animation,
    const StorylandModelFile& model,StorylandAnimationBlendState& blend,
    std::vector<StorylandModelPoint>& points,std::string& error,bool skinGeometry=true) {
    const bool player=handle==0;
    const auto character=world.missionCharacters.find(handle);
    if(!player&&character==world.missionCharacters.end())return false;
    const auto& crouch=player?world.playerCrouch:character->second.crouch;
    const bool seated=player?world.inVehicle:character->second.vehicle>=0&&
        !(runtime.crime.vcs&&character->second.attached);
    if(seated||(!crouch.active&&!crouch.poseFade.present))return false;
    const int32_t phase=storylandCrouchWeaponAnimation(runtime,handle);
    const StorylandCrouchWeaponPose* weaponPose=nullptr;
    if(phase!=155) {
        if(player)weaponPose=&runtime.playerCrouchPose;
        else {
            const auto combat=runtime.missionCombat.find(handle);
            if(combat!=runtime.missionCombat.end())weaponPose=&combat->second.crouchPose;
        }
    }
    int group=0;
    float seconds=float(world.gameMilliseconds-crouch.startedAt)*.001f;
    float weight=crouch.poseFade.at(world.gameMilliseconds);
    if(weaponPose) {
        const auto definition=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
            [&](const auto& weapon){return weapon.nativeId==weaponPose->nativeWeapon;});
        if(definition==runtime.weapons.end())return false;
        group=definition->animationGroup;
        seconds=float(world.gameMilliseconds-weaponPose->startedAt)*.001f+weaponPose->offset;
        weight=storylandCrouchWeaponPoseWeight(*weaponPose,world.gameMilliseconds);
    }
    return animation.sampleCrouchPose(model,weaponPose?phase:crouch.poseAnimation,group,
        seconds,weight,blend,points,error,skinGeometry);
}
