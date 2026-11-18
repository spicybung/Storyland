#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/leeds/leeds_native_world.h"
#include "src/storyland/storyland_script_object.h"

inline bool storylandLcsClearNativeObjects(StorylandMissionEnvironment& world,
    const StorylandNativeWorld& native,const std::vector<uint8_t>& bytes,
    const std::array<float,6>& bounds,std::string& error) {
    error.clear();
    for(float value:bounds)if(!std::isfinite(value)) {
        error="Object clearing bounds must be finite.";return false;
    }
    const auto word=[&](size_t offset) {
        uint32_t value=0;
        if(offset<=bytes.size()&&bytes.size()-offset>=4)std::memcpy(&value,bytes.data()+offset,4);
        return value;
    };
    const uint32_t pool=word(0x2C),items=word(pool),count=word(size_t(pool)+8);
    for(const auto& instance:native.instances) {
        if(instance.poolHeader!=0x2C||instance.poolIndex>=count)continue;
        const uint64_t row=uint64_t(items)+uint64_t(instance.poolIndex)*0x60;
        if(!pool||!items||row+0x60>bytes.size()) {
            error="Native dummy pool has an invalid object record.";return false;
        }
        if(((bytes[size_t(row)+0x44]>>1)&7)!=4)continue;
        const auto& m=instance.matrix;
        if(m[12]<bounds[0]||m[12]>bounds[3]||m[13]<bounds[1]||m[13]>bounds[4]||
            m[14]<bounds[2]||m[14]>bounds[5])continue;
        const uint64_t key=(uint64_t(instance.poolHeader)<<32)|instance.poolIndex;
        world.lcsExtendedWorld.deletedNativeObjects.insert(key);
        world.invisibleNativeObjects.insert(key);
        world.nativeObjectVisibilityChanged=true;
    }
    return true;
}

inline bool storylandLcsNearestGlassShattered(const StorylandMissionEnvironment& world,
    const StorylandNativeWorld& native,const std::vector<uint8_t>& bytes,
    const std::array<float,3>& point,bool& shattered) {
    shattered=false;
    float nearest=20;
    for(const auto& object:world.objects) {
        if(!storylandObjectIsBreakableGlass(bytes,object.model))continue;
        const float distance=std::hypot(object.x-point[0],object.y-point[1],object.z-point[2]);
        if(distance<nearest) {
            nearest=distance;
            shattered=world.lcsExtendedWorld.brokenGlassObjects.count(object.handle)!=0;
        }
    }
    for(const auto& instance:native.instances) {
        if(instance.poolHeader!=0x2C||!storylandObjectIsBreakableGlass(bytes,int32_t(instance.modelId)))continue;
        const uint64_t key=(uint64_t(instance.poolHeader)<<32)|instance.poolIndex;
        if(world.lcsExtendedWorld.deletedNativeObjects.count(key))continue;
        const float distance=std::hypot(instance.matrix[12]-point[0],instance.matrix[13]-point[1],instance.matrix[14]-point[2]);
        if(distance<nearest) {nearest=distance;shattered=false;}
    }
    return true;
}

inline bool storylandLcsPlayerTouchesObject(const StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& native,const std::vector<StorylandNativeTrafficActor>& cars,
    int32_t handle) {
    if(!runtime.missionEnvironment)return false;
    const auto& world=*runtime.missionEnvironment;
    const auto object=std::find_if(world.objects.begin(),world.objects.end(),
        [&](const auto& item){return item.handle==handle;});
    if(object==world.objects.end()||!object->collisionEnabled)return false;
    const auto collision=native.collisionModels.find(uint32_t(object->model));
    if(collision==native.collisionModels.end())return false;
    const StorylandScriptObjectBox objectBox{collision->second.minimum,collision->second.maximum,
        {object->x,object->y,object->z},object->heading,object->rotationX,object->rotationY};
    if(runtime.occupiedVehicle>=0) {
        const size_t slot=size_t(runtime.occupiedVehicle);
        if(slot>=cars.size()||slot>=runtime.vehicles.size())return false;
        const auto& vehicle=runtime.vehicles[slot];
        if(!vehicle.collisionEnabled||object->ignoredVehicle==vehicle.scriptHandle)return false;
        const auto found=native.collisionModels.find(vehicle.nativeDefinition.modelId);
        const auto* bounds=vehicle.nativeDefinition.collision.get();
        if(found!=native.collisionModels.end())bounds=&found->second;
        if(!bounds)return false;
        return storylandScriptBoxesOverlap(objectBox,{bounds->minimum,bounds->maximum,
            cars[slot].position,cars[slot].yaw-90,vehicle.bodyRoll-vehicle.roadRoll,
            vehicle.bodyPitch+vehicle.roadPitch});
    }
    StorylandNativePoint minimum{-.35f,-.35f,0},maximum{.35f,.35f,1.8f};
    if(world.currentPlayerModel) {
        const auto found=native.collisionModels.find(uint32_t(world.currentPlayerModel()));
        if(found!=native.collisionModels.end()) {minimum=found->second.minimum;maximum=found->second.maximum;}
    }
    return storylandScriptBoxesOverlap(objectBox,{minimum,maximum,{world.x,world.y,world.z},world.heading});
}

inline bool storylandLcsCollisionWaiting(const StorylandMissionEnvironment& world,
    const StorylandNativeWorld& native,int32_t kind,int32_t handle,bool& waiting) {
    waiting=false;
    StorylandNativePoint point;
    bool enabled=true;
    if(kind==1&&(handle==0||handle==1))point={world.x,world.y,world.z};
    else if(kind==1) {
        const auto found=world.missionCharacters.find(handle);
        if(found==world.missionCharacters.end())return false;
        point={found->second.x,found->second.y,found->second.z};
        enabled=found->second.loadCollision;
    } else if(kind==2) {
        const auto found=world.missionVehicles.find(handle);
        if(found==world.missionVehicles.end()||found->second.deleted)return false;
        point={found->second.x,found->second.y,found->second.z};
        enabled=found->second.loadCollision;
    } else return false;
    if(!enabled)return true;
    for(const auto& bank:native.collisionBanks) {
        if(bank.contains(point.x,point.y)&&!bank.resident&&
            native.requestedCollisionSlots.count(bank.slot)) {
            waiting=true;
            break;
        }
    }
    return true;
}

inline void storylandSuspendLcsWanted(StorylandPlayTestRuntime& runtime) {
    if(runtime.crime.vcs||!runtime.missionEnvironment)return;
    auto& world=*runtime.missionEnvironment;
    auto& state=world.lcsExtendedWorld;
    state.minSuspendedWanted=std::clamp(world.wanted,0,6);
    state.minSuspendedChaos=runtime.crime.heat;
    state.wantedSuspendedAt=world.gameMilliseconds;
    world.statistics["WantedStarsEvaded"]+=state.minSuspendedWanted;
    runtime.wanted=world.wanted=0;
    runtime.crime.setWantedLevel(0);
}

inline void storylandUpdateLcsWantedSuspension(StorylandPlayTestRuntime& runtime) {
    if(runtime.crime.vcs||!runtime.missionEnvironment)return;
    auto& world=*runtime.missionEnvironment;
    auto& state=world.lcsExtendedWorld;
    if(uint32_t(world.gameMilliseconds-state.wantedSuspendedAt)>20000) {
        state.minSuspendedWanted=0;
        state.minSuspendedChaos=0;
    } else if(runtime.wanted>0&&state.minSuspendedWanted>runtime.wanted) {
        runtime.wanted=state.minSuspendedWanted;
        runtime.crime.heat=std::max(runtime.crime.heat,state.minSuspendedChaos);
        runtime.crime.publishedWanted=runtime.wanted;
    }
}

inline bool storylandLcsWantedStarsFlashing(const StorylandPlayTestRuntime& runtime) {
    if(runtime.crime.vcs||!runtime.missionEnvironment)return false;
    const auto& world=*runtime.missionEnvironment;
    return uint32_t(world.gameMilliseconds-world.lcsExtendedWorld.wantedSuspendedAt)<=20000&&
        world.lcsExtendedWorld.minSuspendedWanted>runtime.wanted;
}
