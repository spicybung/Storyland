#pragma once
#include "src/leeds/leeds_native_world.h"

struct StorylandNativeBody {
    float halfLength=.3f,halfWidth=.3f,lower=.05f,height=1.7f,yaw=0,pitch=0,roll=0;
    bool character=false;
    const StorylandNativeCollisionModel* collision=nullptr;
    std::array<float,16> collisionTransform{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    uint16_t sectorLists=0xFFFF;
};
struct StorylandNativeContact {
    StorylandNativePoint point;
    StorylandNativePoint normal;
    bool valid = false;
    float penetration = 0.0f;
};
struct StorylandNativeMoveResult {
    StorylandNativePoint position;
    bool clear=false,blocked=false,recovered=false;
    StorylandNativeContact contact;
};
struct StorylandNativeBodySample {
    StorylandNativePoint position;
    StorylandNativeBody body;
};
bool storylandNativeBuildingSphereContact(const StorylandNativeWorld& world,
    const StorylandNativePoint& position,float radius,bool& hit);
bool storylandNativeWorldSphereContact(const StorylandNativeWorld& world,
    const StorylandNativePoint& position,float radius,uint16_t sectorLists,bool scriptObjects,bool& hit,
    int32_t ignoredVehicle=-1);
bool storylandNativeSphereModelContact(const StorylandNativePoint& position,float radius,
    const StorylandNativeCollisionModel& model,const std::array<float,16>& matrix,bool& hit,
    StorylandNativeContact* contact=nullptr);
bool storylandNativeBodyClear(const StorylandNativeWorld& world,const StorylandNativePoint& position,
    const StorylandNativeBody& body,StorylandNativeContact* contact=nullptr);
bool storylandNativeBodySweepClear(const StorylandNativeWorld& world,
    const std::vector<StorylandNativeBodySample>& samples,
    StorylandNativeContact* contact=nullptr,size_t* blockedSample=nullptr);
StorylandNativeMoveResult storylandMoveNativeBody(const StorylandNativeWorld& world,
    const StorylandNativePoint& from,const StorylandNativePoint& desired,const StorylandNativeBody& body);
StorylandNativeBody storylandNativeCharacterBody();
bool storylandNativeCharacterModelClear(const StorylandNativePoint& position,
    const StorylandNativeCollisionModel& model,const std::array<float,16>& matrix);
StorylandNativeMoveResult storylandMoveNativeCharacter(const StorylandNativeWorld& world,
    const StorylandNativePoint& from,const StorylandNativePoint& desired);
bool storylandNativeVehicleBodiesOverlap(const StorylandNativePoint& first,const StorylandNativeBody& firstBody,
    const StorylandNativePoint& second,const StorylandNativeBody& secondBody,
    StorylandNativeContact* contact=nullptr);
