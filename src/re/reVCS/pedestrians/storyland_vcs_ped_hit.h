#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include <algorithm>
#include <cmath>

inline bool storylandVcsPedHitSpheres(const std::vector<StorylandNativePedBoneFrame>& pose,
    const StorylandVehicleMatrix& entity,
    std::vector<StorylandMissionEnvironment::CharacterHitSphere>& output) {
    struct Node {uint32_t tag;uint8_t piece;float x,z,radius;};
    static constexpr Node nodes[]={{5,6,0,.05f,.15f},{3,0,0,.15f,.2f},{3,0,0,-.05f,.25f},
        {3,1,0,-.25f,.25f},{32,2,.03f,-.05f,.16f},{22,3,-.03f,-.05f,.16f},
        {42,4,0,.15f,.2f},{52,5,0,.15f,.2f},{43,4,0,.15f,.15f},{53,5,0,.15f,.15f}};
    output.clear();
    const auto finite=[](float value){return std::isfinite(value);};
    if(pose.empty()||!std::all_of(entity.begin(),entity.end(),finite)||
        std::any_of(pose.begin(),pose.end(),[&](const auto& bone) {
            return !std::all_of(bone.matrix.begin(),bone.matrix.end(),finite);
        }))return false;
    std::vector<StorylandMissionEnvironment::CharacterHitSphere> spheres;
    spheres.reserve(std::size(nodes));
    for(const auto& node:nodes) {
        const auto bone=std::find_if(pose.begin(),pose.end(),
            [&](const auto& value){return value.tag==node.tag;});
        if(bone==pose.end())return false;
        StorylandMissionEnvironment::CharacterHitSphere sphere;
        sphere.center=storylandVehicleTransformPoint(entity,
            {bone->matrix[12]+node.x,bone->matrix[13],bone->matrix[14]+node.z});
        sphere.radius=node.radius;sphere.piece=node.piece;
        if(!std::all_of(sphere.center.begin(),sphere.center.end(),finite))return false;
        spheres.push_back(sphere);
    }
    output=std::move(spheres);return true;
}

inline bool storylandVcsBoatDriverHitSpheres(const StorylandMissionEnvironment& world,
    const StorylandPlayTestRuntime& runtime,int32_t handle,const StorylandModelFile& model,
    StorylandPlayTestAnimation& animation,const StorylandAnimationBlendState* blend,
    std::vector<StorylandMissionEnvironment::CharacterHitSphere>& output) {
    output.clear();
    const auto found=world.missionCharacters.find(handle);
    if(found==world.missionCharacters.end()||model.modelKind()!=StorylandModelKind::PedModel)return false;
    const auto& character=found->second;
    if(character.attached||character.vehicle<0||character.passengerSeat>=0||
        character.animationId>=0||character.lookAtCharacter>=0||character.currentWeapon!=0||
        character.threatVisible||character.attackTimers.shootDeadline>world.gameMilliseconds||
        character.attackTimers.attackDeadline>world.gameMilliseconds||character.crouch.active||
        character.crouch.poseFade.present||world.scriptedCharacterAnimations.count(handle)||
        world.vcsPedFalls.count(handle)||
        world.vcsCharacterExitingVehicle(handle)||!world.residentModels.count(character.model))return false;
    const auto target=world.characterTargets.find(handle);
    if(target!=world.characterTargets.end()&&target->second.kind!=StorylandMissionEnvironment::TargetKind::None)
        return false;
    if(blend&&(blend->upperActive||blend->weaponAiming||blend->weaponReloading||
        blend->weaponAimBlend>0||blend->weaponAnimationTime>=0||blend->seatedDriveBy.ready))return false;
    const auto task=world.characterVehicleTasks.find(handle);
    if(task!=world.characterVehicleTasks.end()&&
        (task->second.animating||task->second.exiting||task->second.hijacking))return false;
    const auto combat=runtime.missionCombat.find(handle);
    if(combat!=runtime.missionCombat.end()&&
        (combat->second.contact.active()||combat->second.melee.active||combat->second.crouchPose.active()||
            combat->second.cooldown>0||combat->second.reloading>0))return false;
    const auto car=world.missionVehicles.find(character.vehicle);
    if(car==world.missionVehicles.end()||car->second.deleted||car->second.driver!=handle||
        car->second.spawnedSlot<0||size_t(car->second.spawnedSlot)>=runtime.vehicles.size())return false;
    const auto& vehicle=runtime.vehicles[size_t(car->second.spawnedSlot)];
    const auto& definition=vehicle.nativeDefinition;
    if(vehicle.scriptHandle!=character.vehicle||definition.modelId!=uint32_t(car->second.model)||
        !definition.vcsModelLayout||definition.nativeType!=1||!definition.hasNativeDummies||
        !definition.hasNativeHandling||(definition.modelFlags&0x400u)==0||!world.vehicleWorldMatrix)return false;
    StorylandVehicleMatrix entity;
    if(!world.vehicleWorldMatrix(character.vehicle,entity))return false;
    const auto seat=storylandVehicleTransformPoint(entity,definition.dummyPositions[0]);
    std::copy(seat.begin(),seat.end(),entity.begin()+12);
    std::vector<StorylandNativePedBoneFrame> pose;
    std::string error;
    return animation.sampleVcsBoatDriverPose(model,definition,pose,error)&&
        storylandVcsPedHitSpheres(pose,entity,output);
}
