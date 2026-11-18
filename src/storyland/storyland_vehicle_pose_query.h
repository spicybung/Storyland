#pragma once

#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_script_vehicle_animation.h"
#include "src/storyland/storyland_vehicle_carjack_animation.h"
#include "src/storyland/storyland_vehicle_entry.h"
#include "src/re/reVCS/vehicles/storyland_vcs_helicopter_exit.h"

inline void storylandOffsetVehiclePoseFrames(const StorylandModelFile& model,
    const std::array<float,3>& anchor,std::vector<std::array<float,16>>& frames) {
    const float yaw=(90.0f-storylandPedModelForwardAngle(model))*.01745329251994329577f;
    const float cosine=std::cos(yaw),sine=std::sin(yaw);
    const float x=cosine*anchor[0]+sine*anchor[1];
    const float y=-sine*anchor[0]+cosine*anchor[1];
    for(auto& frame:frames) {
        frame[12]+=x;frame[13]+=y;frame[14]+=anchor[2];
    }
}

inline bool storylandSamplePlayerVehicleQueryPose(StorylandPlayTestAnimation& animation,
    const StorylandModelFile& model,const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& cars,float seconds,
    StorylandAnimationBlendState& query,StorylandVehicleMatrix& parent,std::string& error) {
    using Phase=StorylandVehiclePhase;
    if(runtime.vehicleAction==StorylandVehicleAction::None&&runtime.occupiedVehicle>=0&&
        runtime.playerVehicleSeat<0&&size_t(runtime.occupiedVehicle)<cars.size()&&
        size_t(runtime.occupiedVehicle)<runtime.vehicles.size()) {
        const auto slot=size_t(runtime.occupiedVehicle);
        const auto jacked=storylandVehicleCarjackSample(runtime,slot);
        if(jacked.active) {
            const auto& vehicle=runtime.vehicles[slot];const auto& car=cars[slot];
            const auto& definition=vehicle.nativeDefinition;
            parent=storylandVehicleOccupantFrame(definition,{car.position.x,car.position.y,car.position.z},
                car.yaw,definition.previewMinimumZ,vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,
                vehicle.bodyPitch+vehicle.roadPitch,0,car.water,
                vehicle.craneCarried?&vehicle.craneFrame:nullptr,storylandVcsJetSkiNativeAngularZ(vehicle),
                vehicle.bikeVisualLean);
            std::vector<StorylandModelPoint> ignored;std::array<float,3> anchor{};
            return storylandSampleVehicleCarjackVictim(animation,model,definition,jacked,parent,
                seconds,ignored,anchor,error,&query.boneFrames,&query.poseTransition,false);
        }
    }
    if(runtime.vehicleAction==StorylandVehicleAction::None||!runtime.vehicleTransitionPrepared||
        runtime.transitionVehicle<0||size_t(runtime.transitionVehicle)>=cars.size()||
        size_t(runtime.transitionVehicle)>=runtime.vehicles.size()||
        runtime.vehiclePhase.phase==Phase::Approach||runtime.vehiclePhase.phase==Phase::Done)return false;
    const auto slot=size_t(runtime.transitionVehicle);
    const auto& vehicle=runtime.vehicles[slot];const auto& car=cars[slot];
    const auto& definition=vehicle.nativeDefinition;
    const auto phase=runtime.vehiclePhase.phase;
    auto options=animation.vehicleAnimationOptions(definition,&vehicle.vcsJetSkiPose.sample);
    options.boat=car.water;options.rightHandSide=runtime.vehicleExitRight;
    options.rearExit=runtime.vehicleExitRear;options.variant=runtime.vehicleEntryVariant;
    options.strongWaterContact=runtime.vehicleJetSkiWaterContact;options.highAlign=runtime.vehicleJetSkiHighAlign;
    options.rearDoor=runtime.playerVehicleSeat>=1;options.passenger=runtime.playerVehicleSeat>=0;
    const auto nativeSeat=runtime.playerVehicleSeat<0?0u:unsigned(runtime.playerVehicleSeat+1);
    const unsigned door=storylandVehicleSeatDoor(nativeSeat,runtime.vehicleExitRight);
    options.doorMissing=door+2<vehicle.doorDamage.size()&&vehicle.doorDamage[door+2]==3;
    options.elapsedSeconds=seconds;options.transition=&query.poseTransition;
    options.rootMotion=true;options.seatRelative=true;options.skinGeometry=false;
    const bool bailout=storylandVcsHelicopterBailout(definition,options.variant)&&
        runtime.vehicleAction==StorylandVehicleAction::Exit;
    const bool lifting=definition.twoWheels()&&
        (options.variant==StorylandVehicleEntryVariant::BikePickup||
         options.variant==StorylandVehicleEntryVariant::BikePullup)&&
        (phase==Phase::Align||phase==Phase::Open||phase==Phase::PullOut);
    parent=storylandVehicleOccupantFrame(definition,{car.position.x,car.position.y,car.position.z},
        car.yaw,definition.previewMinimumZ,vehicle.suspension,lifting?0:vehicle.bodyRoll-vehicle.roadRoll,
        lifting?0:vehicle.bodyPitch+vehicle.roadPitch,nativeSeat,definition.vcsJetSki()&&car.water,
        vehicle.craneCarried?&vehicle.craneFrame:nullptr,storylandVcsJetSkiNativeAngularZ(vehicle),
        lifting?0:vehicle.bikeVisualLean);
    if(bailout&&!storylandVcsHelicopterBailoutFrame(definition,
        {car.position.x,car.position.y,car.position.z},car.yaw,vehicle.suspension,
        vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,parent))return false;
    StorylandVehicleRootMotion root;std::vector<StorylandModelPoint> ignored;
    if(!animation.sampleVehicleAction(model,storylandVehiclePhaseAction(phase),runtime.vehiclePhase.fraction,
        options,ignored,error,&root,&query.boneFrames))return false;
    if(!bailout) {
        float minimumZ=1e9f,maximumZ=-1e9f;
        for(const auto& point:model.previewPoints())if(std::isfinite(point.z)) {
            minimumZ=std::min(minimumZ,point.z);maximumZ=std::max(maximumZ,point.z);
        }
        if(maximumZ<minimumZ)return false;
        float hip=(maximumZ-minimumZ)*.56f;
        for(const auto& bone:model.armatureBones())if(bone.boneId==1&&bone.hasPreviewPosition) {
            hip=bone.previewPosition.z-minimumZ;break;
        }
        const auto& doorPosition=runtime.vehicleActionDoor;
        const auto doorLocal=storylandVehicleInversePoint(parent,
            {doorPosition.x,doorPosition.y,doorPosition.z+hip});
        const auto target=storylandVehicleEntrySeatTarget(definition,options.variant,phase,
            runtime.vehiclePhase.elapsed,options.doorMissing);
        const float yaw=90.0f-storylandPedModelForwardAngle(model);
        std::array<float,3> anchor;
        if(phase==Phase::GetIn)
            anchor=storylandVehicleWarpMotion(root.start,root.current,root.end,doorLocal,target,
                runtime.vehiclePhase.fraction,yaw);
        else if(phase==Phase::GetOut)
            anchor=storylandVehicleWarpMotion(root.start,root.current,root.end,{0,0,0},doorLocal,
                runtime.vehiclePhase.fraction,yaw);
        else if(phase==Phase::CloseInside)anchor=target;
        else anchor=storylandVehicleWarpMotion(root.start,root.current,root.end,doorLocal,doorLocal,
            runtime.vehiclePhase.fraction,yaw);
        storylandOffsetVehiclePoseFrames(model,anchor,query.boneFrames);
    }
    return true;
}

inline bool storylandSampleMissionVehicleQueryPose(StorylandPlayTestAnimation& animation,
    const StorylandModelFile& model,const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& cars,int32_t handle,uint32_t now,
    StorylandAnimationBlendState& query,StorylandVehicleMatrix& parent,std::string& error) {
    if(!runtime.missionEnvironment)return false;
    const auto& world=*runtime.missionEnvironment;
    const auto character=world.missionCharacters.find(handle);
    if(character==world.missionCharacters.end())return false;
    const auto task=world.characterVehicleTasks.find(handle);
    const bool transitioning=task!=world.characterVehicleTasks.end()&&task->second.animating;
    const int32_t vehicleHandle=transitioning?task->second.vehicle:character->second.vehicle;
    const auto scripted=world.missionVehicles.find(vehicleHandle);
    if(scripted==world.missionVehicles.end()||scripted->second.spawnedSlot<0)return false;
    const auto slot=size_t(scripted->second.spawnedSlot);
    if(slot>=runtime.vehicles.size()||slot>=cars.size())return false;
    const auto& vehicle=runtime.vehicles[slot];const auto& car=cars[slot];
    const auto& definition=vehicle.nativeDefinition;
    const int32_t seat=transitioning?task->second.seat:character->second.passengerSeat;
    parent=storylandVehicleOccupantFrame(definition,{car.position.x,car.position.y,car.position.z},
        car.yaw,definition.previewMinimumZ,vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,
        vehicle.bodyPitch+vehicle.roadPitch,seat<0?0u:unsigned(seat+1),car.water,
        vehicle.craneCarried?&vehicle.craneFrame:nullptr,storylandVcsJetSkiNativeAngularZ(vehicle),
        vehicle.bikeVisualLean);
    std::vector<StorylandModelPoint> ignored;std::array<float,3> anchor{};
    if(transitioning)return storylandSampleScriptVehicleAnimation(animation,model,task->second,now,parent,
        ignored,anchor,error,&query.boneFrames,&query.poseTransition,&definition,false);
    const auto jacked=storylandVehicleCarjackSample(runtime,slot);
    if(!jacked.active||seat>=0)return false;
    return storylandSampleVehicleCarjackVictim(animation,model,definition,jacked,parent,
        float(now)*.001f,ignored,anchor,error,&query.boneFrames,&query.poseTransition,false);
}
