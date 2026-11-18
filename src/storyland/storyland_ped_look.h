#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

struct StorylandPedLookState {
    float headYaw=0,headPitch=0,torsoYaw=0,torsoPitch=0;
    uint32_t sampledAt=0,tickRemainder=0;
    bool initialized=false,restoreStarted=false,restored=false;
};

struct StorylandScriptedPedLook {
    int32_t target=-1;
    uint32_t deadline=0;
    bool active=false,restoring=false;
    StorylandPedLookState pose;
};

struct StorylandPedLookPose {
    StorylandPedLookState* state=nullptr;
    std::array<float,16> modelWorld{};
    std::array<float,3> actorPosition{},targetPosition{};
    uint32_t milliseconds=0;
    float modelForwardAngle=90;
    bool looking=false,allowTorso=true;
};

inline int storylandMoveNativeLookLimb(float& yaw,float& pitch,float desiredYaw,float desiredPitch,
    const std::array<float,6>& limits) {
    int result=1;
    if(std::fabs(yaw-desiredYaw)<limits[2]) {yaw=desiredYaw;result=2;}
    else if(yaw<desiredYaw)yaw+=limits[2];
    else if(yaw>desiredYaw)yaw-=limits[2];
    if(yaw>limits[0]) {yaw=limits[0];result=0;}
    else if(yaw<limits[1]) {yaw=limits[1];result=0;}
    if(std::fabs(pitch-desiredPitch)<limits[5])pitch=desiredPitch;
    else {
        if(pitch<desiredPitch)pitch+=limits[5];
        else if(pitch>desiredPitch)pitch-=limits[5];
        result=1;
    }
    if(pitch>limits[3]) {pitch=limits[3];result=0;}
    else if(pitch<limits[4]) {pitch=limits[4];result=0;}
    return result;
}

inline constexpr std::array<float,6> StorylandNativeLookHeadLimits{
    1.5707963705062866f,-1.5707963705062866f,.2617993950843811f,
    .7853981852531433f,-.7853981852531433f,.13962635397911072f};
inline constexpr std::array<float,6> StorylandNativeLookRestoreLimits{
    1.5707963705062866f,-1.5707963705062866f,.1745329350233078f,
    .7853981852531433f,-.7853981852531433f,.0872664675116539f};
inline constexpr std::array<float,6> StorylandNativeLookTorsoLimits{
    .8726646900177002f,-.8726646900177002f,.13962635397911072f,
    .7853981852531433f,-.7853981852531433f,.0872664675116539f};

inline void storylandStopScriptedPedLook(StorylandScriptedPedLook& look,uint32_t now,bool player) {
    if(!look.active)return;
    look.active=false;
    look.restoring=look.pose.initialized;
    look.pose.restoreStarted=false;
    look.deadline=now+(player?2000u:4000u);
}

inline void storylandStartScriptedPedLook(StorylandScriptedPedLook& look,int32_t target,
    uint32_t now,bool force) {
    if(look.deadline<now||force) {
        if(!look.active)look.pose={};
        look.target=target;
        look.active=true;
        look.restoring=false;
        look.deadline=0;
    }
    if(look.deadline<now)look.deadline=now+60000u;
}
