#pragma once
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_script_vehicle_animation.h"

struct StorylandVehicleCarjackSample {
    bool active=false;
    bool rightHandSide=false;
    bool motorcycleVictim=false;
    float fraction=0;
    float elapsed=0;
    std::array<float,3> door{};
};

inline StorylandVehicleCarjackSample storylandVehicleCarjackSample(
    const StorylandPlayTestRuntime& runtime,size_t vehicleSlot) {
    StorylandVehicleCarjackSample result;
    if(vehicleSlot>=runtime.vehicles.size())return result;
    const auto& vehicle=runtime.vehicles[vehicleSlot];
    const bool motorcycle=vehicle.nativeDefinition.vcsModelLayout&&vehicle.nativeDefinition.motorcycle();
    if(vehicle.nativeDefinition.nativeType!=0&&!motorcycle)return result;
    if(runtime.vehicleAction==StorylandVehicleAction::Hijack&&
        runtime.transitionVehicle==int32_t(vehicleSlot)&&runtime.vehicleTransitionPrepared&&
        runtime.vehiclePhase.phase==StorylandVehiclePhase::PullOut) {
        result.active=true;
        result.rightHandSide=runtime.vehicleExitRight;
        result.motorcycleVictim=motorcycle;
        result.fraction=runtime.vehiclePhase.fraction;
        result.elapsed=runtime.vehiclePhase.elapsed;
        result.door={runtime.vehicleActionDoor.x,runtime.vehicleActionDoor.y,runtime.vehicleActionDoor.z};
        return result;
    }
    if(!runtime.missionEnvironment||vehicle.scriptHandle<0)return result;
    const auto& world=*runtime.missionEnvironment;
    for(const auto& entry:world.characterVehicleTasks) {
        const auto& task=entry.second;
        if(task.vehicle!=vehicle.scriptHandle||task.exiting||!task.hijacking||
            !task.animating||task.driverRemoved)continue;
        StorylandVehiclePhaseTiming timing;timing.seconds=task.phaseSeconds;
        const auto sample=storylandSampleVehicleTransition(StorylandVehicleAction::Hijack,
            timing,float(world.gameMilliseconds-task.startedAt)*.001f);
        if(sample.phase!=StorylandVehiclePhase::PullOut)continue;
        result.active=true;result.rightHandSide=task.rightHandSide;
        result.motorcycleVictim=motorcycle;
        result.fraction=sample.fraction;result.elapsed=sample.elapsed;result.door=task.door;
        return result;
    }
    return result;
}

inline bool storylandSampleVehicleCarjackVictim(StorylandPlayTestAnimation& animation,
    const StorylandModelFile& model,const StorylandVehicleDefinition& definition,
    const StorylandVehicleCarjackSample& sample,const StorylandVehicleMatrix& seatFrame,
    float now,std::vector<StorylandModelPoint>& points,std::array<float,3>& anchor,
    std::string& error,std::vector<std::array<float,16>>* frames=nullptr,
    StorylandAnimationTransition* transition=nullptr,bool skinGeometry=true) {
    if(!sample.active)return false;
    auto options=animation.vehicleAnimationOptions(definition);
    options.rightHandSide=sample.rightHandSide;options.rootMotion=true;options.seatRelative=true;
    options.skinGeometry=skinGeometry;
    options.elapsedSeconds=now;options.transition=transition;
    StorylandVehicleRootMotion root;
    const char* action=sample.motorcycleVictim?"bike_victim_hit":"jacked";
    float fraction=sample.fraction;
    const float duration=animation.vehicleActionDuration(action,options,true);
    if(!std::isfinite(duration)||duration<=0) {
        error="Native carjacking victim animation is unavailable.";return false;
    }
    fraction=std::clamp(sample.elapsed/duration,0.0f,1.0f);
    if(!animation.sampleVehicleAction(model,action,fraction,options,
        points,error,&root,frames))return false;
    float minimumZ=1e9f,maximumZ=-1e9f;
    for(const auto& point:model.previewPoints())if(std::isfinite(point.z)) {
        minimumZ=std::min(minimumZ,point.z);maximumZ=std::max(maximumZ,point.z);
    }
    if(maximumZ<minimumZ) {error="Carjacking victim has no finite model geometry.";return false;}
    float hipHeight=(maximumZ-minimumZ)*.56f;
    for(const auto& bone:model.armatureBones())if(bone.boneId==1&&bone.hasPreviewPosition) {
        hipHeight=bone.previewPosition.z-minimumZ;break;
    }
    const auto door=storylandVehicleInversePoint(seatFrame,
        {sample.door[0],sample.door[1],sample.door[2]+hipHeight});
    const float modelRotation=90.0f-storylandPedModelForwardAngle(model);
    if(sample.motorcycleVictim) {
        const float angle=modelRotation*.01745329251994329577f;
        const float dx=root.current.x-root.start.x,dy=root.current.y-root.start.y;
        anchor={std::cos(angle)*dx-std::sin(angle)*dy,
            std::sin(angle)*dx+std::cos(angle)*dy,root.current.z-root.start.z};
    } else {
        anchor=storylandVehicleWarpMotion(root.start,root.current,root.end,{0,0,0},door,
            fraction,modelRotation);
    }
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
