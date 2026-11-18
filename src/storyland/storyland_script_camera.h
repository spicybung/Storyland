#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <cmath>

inline bool storylandResolveLcsCameraTarget(const StorylandMissionEnvironment& world,int32_t kind,
    int32_t handle,StorylandNativePoint& target) {
    if(kind==1||(kind==2&&handle==1)) {
        target={world.x,world.y,world.z};
        // POINT_CAMERA_AT_PLAYER resolves FindPlayerVehicle before FindPlayerPed.
        if(kind==1&&world.inVehicle) {
            const auto vehicle=world.missionVehicles.find(world.occupiedMissionVehicle);
            if(vehicle!=world.missionVehicles.end()&&!vehicle->second.deleted)
                target={vehicle->second.x,vehicle->second.y,vehicle->second.z};
        }
    } else if(kind==2) {
        const auto character=world.missionCharacters.find(handle);
        if(character==world.missionCharacters.end())return false;
        target={character->second.x,character->second.y,character->second.z};
    } else if(kind==3) {
        const auto vehicle=world.missionVehicles.find(handle);
        if(vehicle==world.missionVehicles.end()||vehicle->second.deleted)return false;
        target={vehicle->second.x,vehicle->second.y,vehicle->second.z};
    } else return false;
    return std::isfinite(target.x)&&std::isfinite(target.y)&&std::isfinite(target.z);
}

inline bool storylandRefreshLcsCameraTarget(StorylandMissionEnvironment& world) {
    auto& camera=world.camera;
    if(!camera.active||!camera.lcsTrackedKind)return false;
    StorylandNativePoint target;
    if(!storylandResolveLcsCameraTarget(world,camera.lcsTrackedKind,camera.lcsTrackedHandle,target)) {
        camera.active=false;camera.restoreRequested=true;camera.lcsTrackedKind=0;camera.lcsTrackedHandle=-1;
        camera.transitioning=false;camera.transitionPending=false;return false;
    }
    camera.targetX=target.x;camera.targetY=target.y;camera.targetZ=target.z;camera.targetSet=true;
    return true;
}

inline bool storylandCycleLcsPedZoom(StorylandMissionEnvironment& world) {
    auto& camera=world.camera;
    if(camera.pedZoomSwitchFrozen||camera.active||world.inVehicle||world.cutscene.playing||world.widescreen)return false;
    // Native standard controls toggle near/far; script-set middle distance remains until the next key press.
    camera.pedZoomIndicator=camera.pedZoomIndicator==3?1:3;
    return true;
}

// Fixed cameras have no outgoing velocity. Native script interpolation therefore
// holds their starting position for the stop-moving phase, then catches up with
// a cosine curve. The renderer supplies the last displayed view at the switch.
inline void storylandApplyScriptCamera(StorylandMissionEnvironment::ScriptCamera& camera,
    uint32_t now,const StorylandNativePoint& previousEye,const StorylandNativePoint& previousTarget,
    StorylandNativePoint& eye,StorylandNativePoint& target) {
    const StorylandNativePoint destination{camera.x,camera.y,camera.z};
    const StorylandNativePoint destinationTarget{camera.targetX,camera.targetY,camera.targetZ};
    if(camera.cutsceneOwned||!camera.transitioning||camera.interpolationMilliseconds<=0) {
        camera.transitioning=false;camera.transitionPending=false;
        eye=destination;target=destinationTarget;return;
    }
    if(camera.transitionPending) {
        camera.previousX=previousEye.x;camera.previousY=previousEye.y;camera.previousZ=previousEye.z;
        camera.previousTargetX=previousTarget.x;camera.previousTargetY=previousTarget.y;
        camera.previousTargetZ=previousTarget.z;
        camera.transitionStartedAt=now;camera.transitionPending=false;
    }
    const auto elapsed=uint32_t(now-camera.transitionStartedAt);
    float weight=1;
    if(elapsed>=uint32_t(camera.interpolationMilliseconds))camera.transitioning=false;
    else {
        const float progress=float(elapsed)/camera.interpolationMilliseconds;
        const float stop=std::clamp(camera.interpolationWeight,0.0f,1.0f);
        const float catchUp=stop>=1?0:std::clamp((progress-stop)/(1-stop),0.0f,1.0f);
        weight=0.5f-0.5f*std::cos(catchUp*3.14159265358979323846f);
    }
    const auto blend=[&](float from,float to){return from+(to-from)*weight;};
    eye={blend(camera.previousX,destination.x),blend(camera.previousY,destination.y),
        blend(camera.previousZ,destination.z)};
    target={blend(camera.previousTargetX,destinationTarget.x),blend(camera.previousTargetY,destinationTarget.y),
        blend(camera.previousTargetZ,destinationTarget.z)};
}
