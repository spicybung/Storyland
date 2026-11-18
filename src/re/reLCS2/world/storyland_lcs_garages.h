#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/leeds/leeds_native_world.h"
#include "src/re/reLCS2/world/storyland_lcs_crusher.h"
#include <algorithm>
#include <cmath>
#include <set>

inline bool storylandPrepareLcsGarageDoors(const std::vector<uint8_t>& data,const StorylandNativeWorld& native,
    const StorylandMissionEnvironment::Garage& garage,StorylandMissionEnvironment::LcsGarageDoorState& state,
    std::string& error) {
    if(data.size()<0x60||native.instances.empty()) {
        error="Garage doors require the native LCS model-index table and placements.";return false;
    }
    const auto word=[&](size_t offset) {
        return uint32_t(data[offset])|uint32_t(data[offset+1])<<8|
            uint32_t(data[offset+2])<<16|uint32_t(data[offset+3])<<24;
    };
    const uint32_t table=word(0x5C),modelCount=word(0x38);
    if(!table||uint64_t(table)+282u*2u>data.size()) {
        error="Truncated native LCS garage-door model table.";return false;
    }
    std::set<uint32_t> doorModels;
    const auto add=[&](size_t index) {
        const size_t at=size_t(table)+index*2;
        const int32_t model=int16_t(uint16_t(data[at])|uint16_t(data[at+1])<<8);
        if(model>=0&&uint32_t(model)<modelCount)doorModels.insert(uint32_t(model));
    };
    // CModelIndices preserves the retail garage-door members consecutively;
    // GARAGEDOOR8 is absent. Door2_sjl is the separate final garage door.
    for(size_t index=63;index<=100;++index)add(index);
    add(281);
    if(doorModels.empty()) {error="No native garage-door model identities were decoded.";return false;}
    const auto& b=garage.bounds;
    const float centerX=(b[3]+b[5])*.5f,centerY=(b[4]+b[6])*.5f;
    if(garage.type==13) {
        const size_t offset=size_t(table)+105u*2u;
        const int32_t model=int16_t(uint16_t(data[offset])|uint16_t(data[offset+1])<<8);
        if(model<0||uint32_t(model)>=modelCount) {
            error="The original LCS crusher lid model is unavailable.";return false;
        }
        StorylandMissionEnvironment::LcsGarageDoorState decoded;
        decoded.state=1;decoded.height=decoded.position=1.5707963267948966192f;
        for(const auto& instance:native.instances) {
            if((instance.poolHeader!=0x28&&instance.poolHeader!=0x2C)||instance.modelId!=uint32_t(model)||
                std::fabs(instance.matrix[12]-centerX)>=20||std::fabs(instance.matrix[13]-centerY)>=20)continue;
            const uint64_t key=uint64_t(instance.poolHeader)<<32|instance.poolIndex;
            const auto original=native.originalScriptInstanceMatrices.find(key);
            decoded.doors={{key,instance.modelId,
                original==native.originalScriptInstanceMatrices.end()?instance.matrix:original->second}};
        }
        if(decoded.doors.empty()) {
            error="The original LCS crusher lid placement is not loaded.";return false;
        }
        state=std::move(decoded);error.clear();return true;
    }
    const StorylandNativeCollisionInstance* first=nullptr,*second=nullptr;
    for(const auto& instance:native.instances) {
        if(instance.poolHeader!=0x2C||!doorModels.count(instance.modelId)||
            std::fabs(instance.matrix[12]-centerX)>=20||std::fabs(instance.matrix[13]-centerY)>=20)continue;
        if(!first)first=&instance;else second=&instance;
    }
    if(first&&second) {
        const float ax=first->matrix[12]-centerX,ay=first->matrix[13]-centerY;
        const float bx=second->matrix[12]-centerX,by=second->matrix[13]-centerY;
        if(ax*bx+ay*by>0) {
            if(ax*ax+ay*ay>=bx*bx+by*by)first=second;
            second=nullptr;
        }
    }
    StorylandMissionEnvironment::LcsGarageDoorState decoded;
    if(first) {
        const auto collision=native.collisionModels.find(first->modelId);
        if(collision==native.collisionModels.end()) {error="Native garage-door collision bounds are unavailable.";return false;}
        decoded.height=collision->second.maximum.z-collision->second.minimum.z-.1f;
        if(!std::isfinite(decoded.height)||decoded.height<=0) {error="Native garage-door height is invalid.";return false;}
    }
    if(garage.swingDoor)decoded.height=std::max(0.01f,decoded.height*.5f-.1f);
    for(const auto* door:{first,second})if(door) {
        const uint64_t key=uint64_t(door->poolHeader)<<32|door->poolIndex;
        const auto original=native.originalScriptInstanceMatrices.find(key);
        decoded.doors.push_back({key,door->modelId,original==native.originalScriptInstanceMatrices.end()?door->matrix:original->second});
    }
    if(!garage.locked&&((garage.type>=2&&garage.type<=5)||garage.type==12)) {
        decoded.state=1;decoded.position=decoded.height;
    }
    state=std::move(decoded);return true;
}

inline std::array<float,16> storylandLcsGarageDoorMatrix(const StorylandMissionEnvironment::LcsGarageDoor& door,
    const StorylandMissionEnvironment::LcsGarageDoorState& state,bool swing) {
    auto matrix=door.originalMatrix;
    matrix[14]+=state.position;
    if(swing) {
        matrix[12]+=state.position*matrix[5]*(5.0f/6.0f);
        matrix[13]-=state.position*matrix[4]*(5.0f/6.0f);
        const float angle=-(state.position/state.height)*1.5707963267948966f;
        const std::array<float,3> up{-std::sin(angle)*matrix[5],std::sin(angle)*matrix[4],std::cos(angle)};
        matrix[0]=up[1]*matrix[6]-up[2]*matrix[5];
        matrix[1]=up[2]*matrix[4]-up[0]*matrix[6];
        matrix[2]=up[0]*matrix[5]-up[1]*matrix[4];
        matrix[8]=up[0];matrix[9]=up[1];matrix[10]=up[2];
    }
    return matrix;
}

inline bool storylandApplyLcsGarageDoors(StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::Garage& garage,const StorylandMissionEnvironment::LcsGarageDoorState& state,
    std::string& error) {
    if(!world.applyLcsGarageDoorTransform) {error="Garage doors require a native collision and render transform binding.";return false;}
    for(const auto& door:state.doors) {
        const auto matrix=garage.type==13?storylandLcsCrusherLidMatrix(door,state.position):
            storylandLcsGarageDoorMatrix(door,state,garage.swingDoor);
        if(!world.applyLcsGarageDoorTransform(door,matrix,error))return false;
    }
    return true;
}

inline void storylandAdvanceLcsGarageDoors(StorylandMissionVm& vm,uint32_t elapsedMilliseconds) {
    if(vm.vcs||!vm.environment)return;
    auto& world=*vm.environment;
    for(const auto& garage:world.garages) {
        if(garage.type!=13||world.lcsGarageDoors.count(garage.handle))continue;
        StorylandMissionEnvironment::LcsGarageDoorState prepared;
        std::string error;
        if(!world.prepareLcsGarageDoors||!world.prepareLcsGarageDoors(garage,prepared,error)) {
            vm.state=StorylandMissionState::Unsupported;
            vm.diagnostic=error.empty()?"The native LCS crusher requires a world binding.":error;
            return;
        }
        world.lcsGarageDoors.emplace(garage.handle,std::move(prepared));
    }
    for(auto& entry:world.lcsGarageDoors) {
        const auto garage=std::find_if(world.garages.begin(),world.garages.end(),
            [&](const auto& item){return item.handle==entry.first;});
        if(garage==world.garages.end())continue;
        if((garage->type>=16&&garage->type<=18)||(garage->type>=24&&garage->type<=32))continue;
        auto& state=entry.second;
        if(garage->type==13) {
            std::string error;
            if(!storylandAdvanceLcsCrusher(world,*garage,state,error)||
                !storylandApplyLcsGarageDoors(world,*garage,state,error)) {
                vm.state=StorylandMissionState::Unsupported;vm.diagnostic=error;return;
            }
            continue;
        }
        if(!garage->locked&&(state.state==2||state.state==3)) {
            const bool opening=state.state==3;
            const float rate=garage->swingDoor?(opening?.75f:1.0f):(opening?1.75f:2.0f);
            const float distance=rate*(float(elapsedMilliseconds)*.001f);
            state.position=std::clamp(state.position+(opening?distance:-distance),0.0f,state.height);
            if(state.position==0)state.state=0;
            else if(state.position==state.height)state.state=1;
        }
        std::string error;
        if(!storylandApplyLcsGarageDoors(world,*garage,state,error)) {
            vm.state=StorylandMissionState::Unsupported;vm.diagnostic=error;return;
        }
    }
}
