#pragma once

#include "src/storyland/storyland_mission_radar.h"
#include "src/storyland/storyland_mission_vehicle_transform.h"
#include <algorithm>
#include <array>
#include <cmath>

struct StorylandMissionWorldMarker {
    int32_t handle=0;
    uint8_t type=1;
    std::array<float,3> position{};
    float size=1;
    std::array<uint8_t,4> color{255,255,255,255};
    uint32_t createdAt=0;
};

inline size_t storylandMissionMarkerVehicleSlot(const StorylandMissionEnvironment& world,
    const StorylandPlayTestRuntime& runtime,const std::vector<StorylandNativeTrafficActor>& cars,
    int32_t handle) {
    const auto scripted=world.missionVehicles.find(handle);
    if(scripted==world.missionVehicles.end()||scripted->second.deleted)return SIZE_MAX;
    const size_t count=std::min(cars.size(),runtime.vehicles.size());
    const int32_t slot=scripted->second.spawnedSlot;
    if(slot>=0&&size_t(slot)<count&&runtime.vehicles[size_t(slot)].scriptHandle==handle)
        return size_t(slot);
    for(size_t index=0;index<count;++index)
        if(runtime.vehicles[index].scriptHandle==handle)return index;
    return SIZE_MAX;
}

inline bool storylandMissionMarkerVehicleOrigin(const StorylandMissionEnvironment& world,
    const StorylandPlayTestRuntime& runtime,const std::vector<StorylandNativeTrafficActor>& cars,
    size_t slot,StorylandNativePoint& position,float& collisionTop) {
    if(slot>=cars.size()||slot>=runtime.vehicles.size())return false;
    const auto& vehicle=runtime.vehicles[slot];
    const auto& actor=cars[slot];
    const auto& definition=vehicle.nativeDefinition;
    if(!definition.collision||!std::isfinite(definition.collision->maximum.z))return false;
    const auto scripted=world.missionVehicles.find(vehicle.scriptHandle);
    const auto frame=vehicle.craneCarried?vehicle.craneFrame:
        scripted!=world.missionVehicles.end()?
            storylandMissionVehicleWorldMatrix(vehicle,actor,scripted->second):
            storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
                {actor.position.x,actor.position.y,actor.position.z},actor.yaw,
                definition.previewMinimumZ,vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,
                vehicle.bodyPitch+vehicle.roadPitch,actor.water),definition.previewRoot);
    position={frame[12],frame[13],frame[14]};
    collisionTop=definition.collision->maximum.z;
    return storylandRadarFinitePoint(position);
}

inline std::array<uint8_t,4> storylandMissionWorldArrowColor(
    const StorylandMissionEnvironment::RadarBlip& blip) {
    // VCS 0x113F60 and LCS 0x118538 use the same native 3D palette,
    // independently of their 2D radar colors.
    if(blip.color==0)return {255,72,77,255};
    if(blip.entity==StorylandMissionEnvironment::BlipEntity::Object&&blip.color!=2)
        return {128,255,128,255};
    return {72,77,255,255};
}

inline bool storylandResolveMissionWorldMarker(const StorylandMissionEnvironment& world,
    const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& native,
    const std::vector<StorylandNativeTrafficActor>& cars,const StorylandNativePoint& player,
    const StorylandMissionEnvironment::RadarBlip& blip,bool vcs,StorylandMissionWorldMarker& marker) {
    using Entity=StorylandMissionEnvironment::BlipEntity;
    // Both games distinguish MARKER_ONLY(1) and BOTH(3) from RADAR_ONLY(2).
    if((blip.display!=1&&blip.display!=3)||
        !storylandMissionBlipEntityExists(world,blip,vcs))return false;
    marker={};
    marker.handle=blip.handle;
    marker.color=storylandMissionWorldArrowColor(blip);
    StorylandNativePoint position{};
    float collisionTop=0;
    switch(blip.entity) {
    case Entity::Vehicle: {
        const size_t slot=storylandMissionMarkerVehicleSlot(world,runtime,cars,blip.entityHandle);
        if(!storylandMissionMarkerVehicleOrigin(world,runtime,cars,slot,position,collisionTop))return false;
        // VCS 0x113EF0..0x113F64; reLCS CRadar::Draw3dMarkers BLIP_CAR.
        position.z+=1.2f*collisionTop+2.5f;
        marker.size=2.5f;
        break;
    }
    case Entity::Character: {
        size_t vehicleSlot=SIZE_MAX;
        const bool playerBlip=blip.entityHandle==0||(!vcs&&blip.entityHandle==1);
        if(playerBlip) {
            position=player;
            if(runtime.occupiedVehicle>=0)vehicleSlot=size_t(runtime.occupiedVehicle);
        } else {
            const auto& character=world.missionCharacters.at(blip.entityHandle);
            position={character.x,character.y,character.z};
            if(character.vehicle>=0) {
                vehicleSlot=storylandMissionMarkerVehicleSlot(world,runtime,cars,character.vehicle);
                if(vehicleSlot==SIZE_MAX)return false;
            }
        }
        if(vehicleSlot!=SIZE_MAX) {
            if(!storylandMissionMarkerVehicleOrigin(world,runtime,cars,vehicleSlot,position,collisionTop))return false;
        } else {
            std::array<float,16> frame{};
            const int32_t characterHandle=playerBlip?(vcs?0:1):blip.entityHandle;
            if(!playerBlip&&world.characterWorldMatrix&&world.characterWorldMatrix(characterHandle,frame))
                position={frame[12],frame[13],frame[14]};
            else {
                float offset=0;
                if(!world.characterOriginOffset||!world.characterOriginOffset(characterHandle,offset)||
                    !std::isfinite(offset))return false;
                position.z+=offset;
            }
        }
        position.z+=vehicleSlot!=SIZE_MAX?1.2f*collisionTop+1.5f:vcs?2.5f:3.0f;
        marker.size=1.5f;
        break;
    }
    case Entity::Object: {
        const auto object=std::find_if(world.objects.begin(),world.objects.end(),
            [&](const auto& value){return value.handle==blip.entityHandle;});
        const int32_t model=object->model<0&&world.resolveObjectModel?
            world.resolveObjectModel(object->model):object->model;
        const auto collision=native.collisionModels.find(uint32_t(model));
        if(collision==native.collisionModels.end()||
            !std::isfinite(collision->second.maximum.z))return false;
        position={object->x,object->y,object->z+collision->second.maximum.z+(vcs?1.5f:2.0f)};
        marker.size=1.0f;
        break;
    }
    case Entity::ContactPoint:
        if(world.lcsMissionFlagSet&&world.lcsMissionFlagSet())return false;
        position={blip.x,blip.y,blip.z};
        marker.type=vcs?6u:5u;
        marker.size=2.0f;
        marker.color=vcs?std::array<uint8_t,4>{237,130,180,228}:
            std::array<uint8_t,4>{255,255,128,228};
        break;
    case Entity::Coordinate:
    case Entity::Pickup:
        return false;
    }
    if(!storylandRadarFinitePoint(position))return false;
    marker.position={position.x,position.y,position.z};
    return true;
}

template<class DrawMarker>
void storylandVisitMissionWorldMarkers(const StorylandMissionEnvironment& world,
    const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& native,
    const std::vector<StorylandNativeTrafficActor>& cars,const StorylandNativePoint& player,
    bool vcs,const DrawMarker& drawMarker) {
    if(world.cutscene.playing||world.widescreen)return;
    for(const auto& blip:world.radarBlips) {
        StorylandMissionWorldMarker marker;
        if(storylandResolveMissionWorldMarker(world,runtime,native,cars,player,blip,vcs,marker))
            drawMarker(marker);
    }
}
