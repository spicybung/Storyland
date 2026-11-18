#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/leeds/leeds_native_world.h"
#include <cmath>
#include <limits>

struct StorylandLcsObjectSelection {
    int32_t scriptHandle=-1;
    uint64_t nativeKey=UINT64_MAX;
    bool found() const {return scriptHandle>=0||nativeKey!=UINT64_MAX;}
};
inline StorylandLcsObjectSelection storylandFindLcsClosestObject(const StorylandMissionEnvironment& environment,
    const StorylandNativeWorld& native,const std::array<float,3>& point,float radius,int32_t model,int32_t scriptAlias=INT32_MAX) {
    StorylandLcsObjectSelection selected;
    float nearest=2*radius;
    const auto candidate=[&](float x,float y,float z,int32_t handle,uint64_t key) {
        const float dx=x-point[0],dy=y-point[1],dz=z-point[2];
        // Native range collection ignores Z; the final closest choice uses 3D distance.
        if(std::hypot(dx,dy)>=radius)return;
        const float distance=std::hypot(dx,dy,dz);
        if(distance<nearest) {nearest=distance;selected={handle,key};}
    };
    for(const auto& instance:native.instances)if(instance.modelId==uint32_t(model))
        candidate(instance.matrix[12],instance.matrix[13],instance.matrix[14],-1,
            (uint64_t(instance.poolHeader)<<32)|instance.poolIndex);
    for(const auto& object:environment.objects)if(object.model==model||object.model==scriptAlias)
        candidate(object.x,object.y,object.z,object.handle,UINT64_MAX);
    return selected;
}
