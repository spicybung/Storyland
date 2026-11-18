#pragma once

#include "src/storyland/storyland_mission_vm.h"
#include <algorithm>
#include <cmath>

inline bool storylandNormalizeVcsVehicleGroundHeight(const StorylandMissionEnvironment& world,
    float x,float y,float& z,std::string& error) {
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)) {
        error="Vehicle placement coordinates must be finite.";
        return false;
    }
    // CREATE_CAR, SET_CAR_COORDINATES and SET_CAR_COORDINATES_NO_OFFSET share these sentinels.
    if(z!=-100.0f&&z>-250.0f)return true;
    if(!world.groundHeightAt) {
        error="Vehicle ground sentinel requires the native collision world.";
        return false;
    }
    float ground=20.0f;
    if(!world.groundHeightAt({x,y,1000.0f},ground))ground=20.0f;
    if(!std::isfinite(ground)) {
        error="Vehicle ground collision returned a non-finite height.";
        return false;
    }
    z=ground;
    return true;
}

inline bool storylandSetVcsVehicleCoordinates(const StorylandMissionEnvironment& world,
    int32_t handle,StorylandMissionEnvironment::VehicleScriptState& vehicle,
    float x,float y,float z,bool noOffset,std::string& error) {
    if(!storylandNormalizeVcsVehicleGroundHeight(world,x,y,z,error))return false;
    const int32_t nativeType=world.vehicleNativeType?world.vehicleNativeType(handle):-1;
    const bool resetRoadVehicle=nativeType==0||nativeType==6;
    const bool resetBoat=nativeType==1||nativeType==2;
    const bool resetHelicopter=nativeType==4;
    if((noOffset&&resetRoadVehicle)||(!noOffset&&resetBoat)) {
        float baseOffset=0;
        if(!world.nativeModelBaseOffset||!world.nativeModelBaseOffset(vehicle.model,baseOffset)||
            !std::isfinite(baseOffset)) {
            error="Native vehicle placement requires its collision base offset.";
            return false;
        }
        // 3D0C80 adds -COL.minimum.z for004E. Water actors already use the entity origin.
        z+=resetBoat?baseOffset:-baseOffset;
        if(!std::isfinite(z)) {
            error="Native vehicle placement exceeds the finite coordinate range.";
            return false;
        }
    }
    if(resetHelicopter) {
        std::array<float,3> originOffset{};
        if(!world.nativeModelOriginOffset||!world.nativeModelOriginOffset(vehicle.model,originOffset)||
            !std::all_of(originOffset.begin(),originOffset.end(),
                [](float value){return std::isfinite(value);})) {
            error="Native helicopter placement requires its decoded model-origin offset.";
            return false;
        }
        if(!noOffset) {
            float baseOffset=0;
            if(!world.nativeModelBaseOffset||!world.nativeModelBaseOffset(vehicle.model,baseOffset)||
                !std::isfinite(baseOffset)) {
                error="Native helicopter placement requires its collision base offset.";
                return false;
            }
            z+=baseOffset;
        }
        // Type4 inherits1402D8. Preserve the host mesh anchor while publishing
        // the original native entity origin and identity basis immediately.
        x-=originOffset[0];y-=originOffset[1];z-=originOffset[2];
        if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)) {
            error="Native helicopter placement exceeds the finite coordinate range.";
            return false;
        }
    }
    vehicle.x=x;vehicle.y=y;vehicle.z=z;
    vehicle.movedByScript=true;
    vehicle.teleportedByScript=true;
    if(resetRoadVehicle||resetBoat||resetHelicopter) {
        // Native automobile1402D8, bike33D388 and boat1A3498 teleport reset rigid motion.
        vehicle.heading=0;vehicle.rotation={};vehicle.angularVelocity={};
        vehicle.velocity={};vehicle.speed=0;
        vehicle.headingChangedByScript=vehicle.rotationChangedByScript=true;
        vehicle.velocityChangedByScript=vehicle.angularVelocityChangedByScript=true;
    }
    return true;
}
