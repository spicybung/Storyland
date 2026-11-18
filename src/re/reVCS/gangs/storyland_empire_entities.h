#pragma once
#include "src/re/reVCS/gangs/storyland_empire.h"
#include "src/storyland/storyland_mission_vm.h"
#include "src/leeds/leeds_native_world.h"
#include "src/storyland/storyland_script_object.h"
#include <algorithm>

inline bool storylandValidAnimatedEmpireEntities(const StorylandMissionEnvironment& world) {
    if(world.animatedEmpireFreeCursor< -1||world.animatedEmpireFreeCursor>=int32_t(world.animatedEmpireEntities.size()))return false;
    std::set<std::pair<int32_t,bool>> occupied;
    for(const auto& entity:world.animatedEmpireEntities) {
        if(entity.site<0) {
            if(entity.site!=-1||entity.model!=-1||entity.construction||entity.detailed)return false;
            continue;
        }
        if(size_t(entity.site)>=world.empireSites.size()||entity.model<0||
            (entity.construction&&entity.detailed)||!occupied.emplace(entity.site,entity.construction).second)return false;
    }
    return true;
}

inline int32_t storylandAllocateAnimatedEmpireEntity(StorylandMissionEnvironment& world) {
    // Native 288198 creates 20 slots. 473CE0 increments before examining a
    // slot and wraps once; allocation is independent of the empire site ID.
    for(size_t attempt=0;attempt<world.animatedEmpireEntities.size();++attempt) {
        world.animatedEmpireFreeCursor=(world.animatedEmpireFreeCursor+1)%int32_t(world.animatedEmpireEntities.size());
        if(world.animatedEmpireEntities[size_t(world.animatedEmpireFreeCursor)].site<0)
            return world.animatedEmpireFreeCursor;
    }
    return -1;
}
inline void storylandReleaseAnimatedEmpireEntity(StorylandMissionEnvironment& world,size_t slot) {
    if(slot>=world.animatedEmpireEntities.size())return;
    world.animatedEmpireEntities[slot]={};
    if(int32_t(slot)<world.animatedEmpireFreeCursor)world.animatedEmpireFreeCursor=int32_t(slot);
}
inline std::array<float,16> storylandAnimatedEmpireMatrix(int32_t site) {
    if(site<0||size_t(site)>=StorylandVcsEmpireSites.size())return {};
    const auto& pose=StorylandVcsEmpireSites[size_t(site)];
    return storylandScriptObjectMatrix(pose[0],pose[1],pose[2],0,0,pose[3]*57.2957763671875f);
}
inline void storylandUpdateAnimatedEmpireEntities(StorylandMissionEnvironment& world,float cameraX,float cameraY) {
    if(!world.modelIdByName||!std::isfinite(cameraX)||!std::isfinite(cameraY))return;
    world.empireRequestedModels.clear();
    for(size_t site=0;site<world.empireSites.size();++site) {
        const auto& state=world.empireSites[site];
        const auto visual=storylandEmpireVisual(state.type,state.scale,state.state,state.damaged,
            StorylandVcsEmpireSites[site],cameraX,cameraY);
        const std::array<std::string,2> names{visual.building,
            !visual.building.empty()&&visual.construction?"eb_cons_a":""};
        for(size_t part=0;part<names.size();++part) {
            const int32_t wanted=names[part].empty()?-1:world.modelIdByName(names[part]);
            const bool resident=wanted>=0&&world.residentModels.count(wanted)!=0;
            if(wanted>=0)world.empireRequestedModels.insert(wanted);
            auto old=std::find_if(world.animatedEmpireEntities.begin(),world.animatedEmpireEntities.end(),
                [&](const auto& entity){return entity.site==int32_t(site)&&entity.construction==(part==1);});
            if(old!=world.animatedEmpireEntities.end()) {
                if(old->model==wanted)continue;
                // 2E4F38 retains an existing LOD until its replacement is
                // resident; a detailed instance is removed immediately.
                if(part==0&&!old->detailed&&wanted>=0&&!resident) {
                    world.empireRequestedModels.insert(old->model);continue;
                }
                const size_t slot=size_t(old-world.animatedEmpireEntities.begin());
                storylandReleaseAnimatedEmpireEntity(world,slot);
            }
            if(!resident)continue;
            const int32_t slot=storylandAllocateAnimatedEmpireEntity(world);
            if(slot<0)continue;
            auto& entity=world.animatedEmpireEntities[size_t(slot)];
            entity.site=int32_t(site);entity.model=wanted;entity.construction=part==1;
            entity.detailed=part==0&&visual.detailed;
        }
    }
}
inline int32_t storylandGetEmpireBuildingHandle(const StorylandMissionEnvironment& world,int32_t site,
    float cameraX,float cameraY) {
    if(site<0||size_t(site)>=world.empireSites.size()||!world.modelIdByName)return -1;
    const auto& state=world.empireSites[size_t(site)];
    const auto visual=storylandEmpireVisual(state.type,state.scale,state.state,state.damaged,
        StorylandVcsEmpireSites[size_t(site)],cameraX,cameraY);
    const int32_t expected=visual.building.empty()?-1:world.modelIdByName(visual.building);
    if(expected<0)return -1;
    for(size_t slot=0;slot<world.animatedEmpireEntities.size();++slot) {
        const auto& entity=world.animatedEmpireEntities[slot];
        if(entity.site==site&&!entity.construction&&entity.model==expected)
            return int32_t(0x800000u|uint32_t(slot));
    }
    return -1;
}
inline bool storylandResolveScriptEntityModelMatrix(const StorylandMissionEnvironment& world,
    const StorylandNativeWorld& native,int32_t handle,int32_t& model,std::array<float,16>& matrix) {
    const uint32_t bits=uint32_t(handle);
    if(bits&0x1000000u) {
        const uint32_t slot=bits&0x7FFFFFu;
        for(const auto& instance:native.instances)if(instance.poolHeader==0x24&&instance.poolIndex==slot) {
            model=int32_t(instance.modelId);matrix=instance.matrix;return true;
        }
        return false;
    }
    if(bits&0x800000u) {
        const uint32_t slot=bits&0x7FFFFFu;
        if(slot>=world.animatedEmpireEntities.size())return false;
        const auto& entity=world.animatedEmpireEntities[slot];
        if(entity.site<0||entity.model<0)return false;
        model=entity.model;matrix=storylandAnimatedEmpireMatrix(entity.site);return true;
    }
    if(handle<0)return false;
    for(const auto& object:world.objects)if(object.handle==handle) {
        model=object.model;
        matrix=storylandScriptObjectMatrix(object.x,object.y,object.z,object.rotationX,object.rotationY,object.heading);
        return true;
    }
    const int32_t slot=handle>>8;
    for(const auto& object:world.objects)if(object.nativePoolIndex==slot) {
        model=object.model;
        matrix=storylandScriptObjectMatrix(object.x,object.y,object.z,object.rotationX,object.rotationY,object.heading);
        return true;
    }
    return false;
}
