#pragma once
#include "src/storyland/storyland_native_random.h"
#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <cmath>
#include <limits>

struct StorylandTrafficGenerationProfile {
    float radius=0;
    float directionCosine=0;
    bool inFront=true;
    bool velocityDirection=false;
};

inline float storylandVcsCameraGenerationDistance(float fieldOfView) {
    if(!std::isfinite(fieldOfView)||fieldOfView<=0)return 0;
    return std::min(2.0f,70.0f/fieldOfView);
}

inline StorylandTrafficGenerationProfile storylandVcsTrafficGenerationProfile(
    bool playerVehicle,float nativeSpeed,uint32_t frame,float cameraGenerationDistance,bool steepDownwardCamera) {
    StorylandTrafficGenerationProfile result;
    if(!std::isfinite(cameraGenerationDistance)||cameraGenerationDistance<=0||!std::isfinite(nativeSpeed))return result;
    if(steepDownwardCamera)return {55.0f,-1.0f,true,false};
    const uint32_t phase=frame&3u;
    if(playerVehicle&&nativeSpeed>.4f) {
        result.velocityDirection=true;
        result.inFront=phase!=3;
        result.directionCosine=phase<2?.85f:.707f;
    } else if(playerVehicle&&nativeSpeed>.1f) {
        result.velocityDirection=true;
        result.inFront=phase<2;
        result.directionCosine=phase==0?.85f:.707f;
    } else {
        result.inFront=(frame&1u)==0;
        result.directionCosine=.707f;
    }
    result.radius=result.inFront?110.0f*cameraGenerationDistance:40.0f;
    return result;
}

inline bool storylandVcsTrafficCreationLink(const StorylandNativePoint& source,
    const StorylandNativePoint& target,uint16_t flags,const StorylandNativePoint& focus,
    const StorylandNativePoint& direction,const StorylandTrafficGenerationProfile& profile,
    float& fraction,StorylandNativePoint& point) {
    if(profile.radius<=0||!std::isfinite(profile.radius))return false;
    const float sourceDistance=std::hypot(source.x-focus.x,source.y-focus.y);
    const float targetDistance=std::hypot(target.x-focus.x,target.y-focus.y);
    const float searchDistance=std::max(profile.radius*1.7f,profile.radius+70.0f);
    if(!(sourceDistance<searchDistance))return false;
    const float scale=(flags&0x100u)?1.5f:1.0f;
    const float crossingSquared=profile.radius*profile.radius*scale;
    if(!((sourceDistance*sourceDistance-crossingSquared)*
        (targetDistance*targetDistance-crossingSquared)<0))return false;
    const float radius=profile.radius*scale;
    const float from=std::fabs(sourceDistance-radius),to=std::fabs(targetDistance-radius);
    if(from+to<=0)return false;
    fraction=from/(from+to);
    point={source.x+(target.x-source.x)*fraction,source.y+(target.y-source.y)*fraction,
        source.z+(target.z-source.z)*fraction};
    const float x=point.x-focus.x,y=point.y-focus.y,length=std::hypot(x,y);
    const float directionLength=std::hypot(direction.x,direction.y);
    if(length<=0||directionLength<=0)return false;
    const float dot=(x*direction.x+y*direction.y)/(length*directionLength);
    return profile.inFront?dot>profile.directionCosine:dot<=profile.directionCosine;
}

inline std::vector<StorylandNativeTrafficActor> storylandVcsTrafficCreationCandidates(
    const StorylandNativePaths& paths,const StorylandNativePoint& focus,const StorylandNativePoint& direction,
    const StorylandTrafficGenerationProfile& profile,bool water,size_t maximum) {
    std::vector<StorylandNativeTrafficActor> result;
    if(profile.radius<=0||!std::isfinite(profile.radius))return result;
    const float searchDistance=std::max(profile.radius*1.7f,profile.radius+70.0f);
    for(uint32_t index=0;index<paths.nodes.size()&&result.size()<maximum;++index) {
        const auto& source=paths.nodes[index];
        if(!source.vehicle||source.water!=water||source.disabled)continue;
        // Every link from this node has the same source-distance admission test.
        // Reject it once before visiting its links, retaining the link test's
        // original hypot calculation and strict boundary.
        if(!(std::hypot(source.position.x-focus.x,source.position.y-focus.y)<searchDistance))continue;
        for(const auto& link:source.links) {
            if(link.target>=paths.nodes.size()||(link.nativeLanes&&!water&&link.lanes==0))continue;
            const auto& target=paths.nodes[link.target];
            if(!target.vehicle||target.water!=water||target.disabled)continue;
            float fraction=0;StorylandNativePoint point;
            if(!storylandVcsTrafficCreationLink(source.position,target.position,source.flags,focus,direction,
                profile,fraction,point))continue;
            StorylandNativeTrafficActor actor;
            actor.vehicle=true;actor.water=water;actor.node=index;actor.nextNode=link.target;
            actor.routeSeed=(index+1)*0x9E3779B9u^link.target;
            if(!water&&link.nativeLanes) {
                actor.lane=uint8_t(actor.routeSeed%link.lanes);
                actor.laneOffset=link.oppositeLanes?(2.0f*actor.lane+1)*2.5f+link.dividerWidth:
                    (2.0f*actor.lane+1-link.lanes)*2.5f;
            }
            const float x=target.position.x-source.position.x,y=target.position.y-source.position.y;
            const float planarLength=std::hypot(x,y),z=target.position.z-source.position.z;
            if(planarLength<=0)continue;
            actor.distance=std::sqrt(x*x+y*y+z*z)*fraction;
            actor.position={point.x+y/planarLength*actor.laneOffset,
                point.y-x/planarLength*actor.laneOffset,point.z};
            actor.yaw=std::atan2(y,x)*57.295779513f;
            result.push_back(actor);
            if(result.size()>=maximum)break;
        }
    }
    return result;
}

inline uint8_t storylandVcsTrafficSpawnRate(uint16_t sourceFlags,uint16_t targetFlags) {
    constexpr uint8_t rates[4]={0,4,7,15};
    return std::min(rates[(sourceFlags>>12)&3u],rates[(targetFlags>>12)&3u]);
}

inline float storylandVcsTrafficCreationClearance(bool water) {
    return water?40.0f:10.0f;
}

inline float storylandVcsTrafficRemovalDistance(float cameraGenerationDistance,bool temporaryCull,
    bool steepDownwardCamera,bool extendedRange) {
    if(!std::isfinite(cameraGenerationDistance)||cameraGenerationDistance<=0)return 0;
    float distance=temporaryCull?60.0f:160.0f*cameraGenerationDistance;
    if(steepDownwardCamera)distance=70.0f;
    return extendedRange?distance*1.5f:distance;
}

struct StorylandParkedVehicleSecurity {
    uint16_t alarmMilliseconds=0;
    int32_t doorLock=1;
};

inline StorylandParkedVehicleSecurity storylandVcsParkedVehicleSecurity(uint32_t& random,
    int32_t alarm,int32_t doorLock) {
    StorylandParkedVehicleSecurity result;
    if(storylandNativeRandomRange(random,100)<uint8_t(alarm))result.alarmMilliseconds=UINT16_MAX;
    if(storylandNativeRandomRange(random,100)<uint8_t(doorLock))result.doorLock=2;
    return result;
}
