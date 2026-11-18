#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include "src/storyland/storyland_vehicle_transition.h"
#include <algorithm>
#include <cmath>

// Keep the authored pelvis arc while matching the actual car's seat and grounded door.
inline bool storylandSampleScriptVehicleAnimation(StorylandPlayTestAnimation& animation,
    const StorylandModelFile& model,const StorylandMissionEnvironment::CharacterVehicleTask& task,
    uint32_t now,const StorylandVehicleMatrix& seatFrame,std::vector<StorylandModelPoint>& points,
    std::array<float,3>& anchor,std::string& error,std::vector<std::array<float,16>>* frames=nullptr,
    StorylandAnimationTransition* transition=nullptr,const StorylandVehicleDefinition* definition=nullptr,
    bool skinGeometry=true) {
    if(!task.animating)return false;
    StorylandVehiclePhaseTiming timing;timing.seconds=task.phaseSeconds;
    const auto action=task.exiting?StorylandVehicleAction::Exit:
        task.hijacking?StorylandVehicleAction::Hijack:StorylandVehicleAction::Enter;
    const auto sample=storylandSampleVehicleTransition(action,timing,float(now-task.startedAt)/1000.0f);
    if(sample.phase==StorylandVehiclePhase::Approach||sample.phase==StorylandVehiclePhase::Done)return false;
    StorylandVehicleAnimationOptions options;
    if(definition)options=animation.vehicleAnimationOptions(*definition);
    options.passenger=task.seat>=0;options.low=task.low;options.rightHandSide=task.rightHandSide;
    options.rearDoor=task.seat>=1;
    options.boat=task.boat;options.rootMotion=true;options.seatRelative=true;
    options.skinGeometry=skinGeometry;
    options.elapsedSeconds=float(now)*.001f;options.transition=transition;
    StorylandVehicleRootMotion root;
    if(!animation.sampleVehicleAction(model,storylandVehiclePhaseAction(sample.phase),sample.fraction,
        options,points,error,&root,frames))return false;
    float minimumZ=1e9f,maximumZ=-1e9f;
    for(const auto& point:model.previewPoints())if(std::isfinite(point.z)) {
        minimumZ=std::min(minimumZ,point.z);maximumZ=std::max(maximumZ,point.z);
    }
    if(maximumZ<minimumZ) {error="Vehicle animation model has no finite body geometry.";return false;}
    float hipHeight=(maximumZ-minimumZ)*0.56f;
    for(const auto& bone:model.armatureBones())if(bone.boneId==1&&bone.hasPreviewPosition) {
        hipHeight=bone.previewPosition.z-minimumZ;break;
    }
    const auto door=storylandVehicleInversePoint(seatFrame,{task.door[0],task.door[1],task.door[2]+hipHeight});
    const float modelYaw=90.0f-storylandPedModelForwardAngle(model);
    if(sample.phase==StorylandVehiclePhase::GetIn)
        anchor=storylandVehicleWarpMotion(root.start,root.current,root.end,door,{0,0,0},sample.fraction,modelYaw);
    else if(sample.phase==StorylandVehiclePhase::GetOut)
        anchor=storylandVehicleWarpMotion(root.start,root.current,root.end,{0,0,0},door,sample.fraction,modelYaw);
    else if(sample.phase==StorylandVehiclePhase::CloseInside)anchor={0,0,0};
    else anchor=storylandVehicleWarpMotion(root.start,root.current,root.end,door,door,sample.fraction,modelYaw);
    // Collision/attachment callers receive the complete seat-relative pose; mesh rendering
    // retains its separate anchor so that the existing GL transform remains unchanged.
    if(frames) {
        const float yaw=(90.0f-storylandPedModelForwardAngle(model))*.01745329251994329577f;
        const float cosine=std::cos(yaw),sine=std::sin(yaw);
        const float modelX=cosine*anchor[0]+sine*anchor[1];
        const float modelY=-sine*anchor[0]+cosine*anchor[1];
        for(auto& frame:*frames) {
            frame[12]+=modelX;frame[13]+=modelY;frame[14]+=anchor[2];
        }
    }
    return true;
}

// Check every transition phase before a mission actor begins approaching a
// vehicle. A duration alone does not prove that the native bank has decoded
// bone data for this pedestrian model. Rejecting a missing clip before the
// task starts prevents the logic from moving an idle-looking pedestrian into
// the car or motorcycle.
inline bool storylandVerifyScriptVehicleTaskAnimations(StorylandPlayTestAnimation& animation,
    const StorylandModelFile& model,const StorylandMissionEnvironment::CharacterVehicleTask& task,
    const StorylandVehicleDefinition& definition,std::string& error) {
    StorylandVehiclePhaseTiming timing;
    timing.seconds=task.phaseSeconds;
    const auto action=task.exiting?StorylandVehicleAction::Exit:
        task.hijacking?StorylandVehicleAction::Hijack:StorylandVehicleAction::Enter;
    auto options=animation.vehicleAnimationOptions(definition);
    options.passenger=task.seat>=0;
    options.rightHandSide=task.rightHandSide;
    options.low=task.low;
    options.rearDoor=task.seat>=1;
    options.boat=task.boat;
    options.rootMotion=true;
    options.seatRelative=true;
    std::vector<StorylandModelPoint> points;
    for(StorylandVehiclePhase phase:{StorylandVehiclePhase::Align,StorylandVehiclePhase::Open,
        StorylandVehiclePhase::PullOut,StorylandVehiclePhase::GetIn,
        StorylandVehiclePhase::CloseInside,StorylandVehiclePhase::GetOut,
        StorylandVehiclePhase::CloseOutside}) {
        if(!(timing.seconds[unsigned(phase)]>0))continue;
        if(action==StorylandVehicleAction::Exit&&phase!=StorylandVehiclePhase::GetOut&&
            phase!=StorylandVehiclePhase::CloseOutside)continue;
        if(action!=StorylandVehicleAction::Exit&&(phase==StorylandVehiclePhase::GetOut||
            phase==StorylandVehiclePhase::CloseOutside))continue;
        StorylandVehicleRootMotion root;
        std::string clipError;
        if(!animation.sampleVehicleAction(model,storylandVehiclePhaseAction(phase),.5f,
            options,points,clipError,&root)||points.empty()||
            !std::isfinite(root.current.x)||!std::isfinite(root.current.y)||!std::isfinite(root.current.z)||
            !std::isfinite(root.start.x)||!std::isfinite(root.start.y)||!std::isfinite(root.start.z)||
            !std::isfinite(root.end.x)||!std::isfinite(root.end.y)||!std::isfinite(root.end.z)) {
            error=std::string("Pedestrian vehicle transition has no decoded pose for ")+
                storylandVehiclePhaseAction(phase)+": "+clipError;
            return false;
        }
    }
    return true;
}
