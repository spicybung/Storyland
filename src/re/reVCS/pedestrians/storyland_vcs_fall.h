#pragma once

#include "src/storyland/storyland_environment.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

struct StorylandVcsFallState {
    bool active=false;
    float distanceWithoutGround=0;
    float collapseTime=0;
    int32_t airAnimationId=140;
    float airAnimationTime=0;
    int32_t landingAnimationId=-1;
    float landingAnimationTime=0;
    float landingAnimationDuration=0;
    float landingHeading=0;
    bool relcsRollCandidate=false;
};

inline int32_t storylandVcsFallAssociationFlags(int32_t animation,bool landing) {
    if(landing&&animation==139)return 0x54;
    if(landing&&animation==147)return 0x1044;
    if(!landing&&animation==140)return 0x16;
    if(!landing&&animation==142)return 0x56;
    return -1;
}

struct StorylandVcsAirPose {
    int32_t animation=-1;
    float seconds=0;
    bool landing=false;
};

inline StorylandVcsAirPose storylandVcsAirbornePose(const StorylandVcsFallState& fall,
    bool launching,float launchTime,bool grounded,float landingTime) {
    if(launching)return {136,std::max(0.0f,launchTime),false};
    if(!fall.active)return {};
    if(fall.landingAnimationId>=0&&landingTime>0)
        return {fall.landingAnimationId,fall.landingAnimationTime,true};
    if(!grounded)return {fall.airAnimationId,fall.airAnimationTime,false};
    return {};
}

// The supplied reLCS SetLanding custom roll uses native 50 Hz velocities.
// Only Extended+SA admits it; ordinary VCS landing selection stays below.
inline bool storylandRelcsLandingRollAllowed(const StorylandVcsFallState& fall,
    bool extended,float horizontalSpeed,float verticalSpeed,bool standing,
    uint8_t surface,float health) {
    if(!extended||!fall.relcsRollCandidate||surface==26||!(health>0)||
        !std::isfinite(horizontalSpeed)||!std::isfinite(verticalSpeed))return false;
    const bool resolved=standing||std::fabs(verticalSpeed)<4.0f;
    const bool moderate=verticalSpeed < -5.0f&&verticalSpeed > -42.5f;
    return horizontalSpeed>5.0f&&(resolved||moderate);
}

// SLUS_215.90 0x1F9D00..0x1F9E9C; native velocity is distance per 50 Hz step.
inline float storylandVcsFallImpactDamage(float horizontalSpeed,float verticalSpeed,
    uint8_t surface) {
    if(!std::isfinite(horizontalSpeed)||!std::isfinite(verticalSpeed))return 0;
    const float horizontal=std::max(0.0f,horizontalSpeed)/StorylandNativePhysicsRate;
    const float vertical=verticalSpeed/StorylandNativePhysicsRate;
    if(horizontal<=.66f&&vertical>=-.25f)return 0;
    float damage=std::max(0.0f,horizontal-.25f)*100.0f+
        std::max(0.0f,-.25f-vertical)*300.0f;
    // 0x36C650 identifies the three cushioning surface classes.
    if(surface==2||surface==18||surface==33)damage*=.5f;
    return damage;
}

inline float storylandVcsFallContactSeconds(float height,float velocity,float gravity,
    float seconds) {
    if(!std::isfinite(height)||!std::isfinite(velocity)||!std::isfinite(gravity)||
        !std::isfinite(seconds)||seconds<=0)return 0;
    height=std::max(0.0f,height);
    if(gravity<=0)return velocity<0?std::clamp(-height/velocity,0.0f,seconds):seconds;
    const float discriminant=velocity*velocity+2.0f*gravity*height;
    const float contact=(velocity+std::sqrt(std::max(0.0f,discriminant)))/gravity;
    return std::clamp(contact,0.0f,seconds);
}

inline void storylandBeginVcsFall(StorylandVcsFallState& state,bool launched) {
    state={};
    state.active=true;
    state.airAnimationId=launched?137:140;
}

// CPed 0x202D78..0x202F90. +1C4 accumulates CPhysical's travelled distance
// only while descending without support in its native 4.04 metre ray.
inline void storylandAdvanceVcsFall(StorylandVcsFallState& state,float distance,
    bool descending,bool groundInRay,float seconds) {
    if(!state.active||!std::isfinite(seconds)||seconds<=0)return;
    state.airAnimationTime+=seconds;
    if(state.landingAnimationId>=0)return;
    if(!descending||groundInRay)return;
    if(std::isfinite(distance)&&distance>0)state.distanceWithoutGround+=distance;
    if(state.airAnimationId==142) {
        state.collapseTime+=seconds;
        // 0x202F04 tests association+10 (blend amount), not clip time.
        // 0x202EE8 blends this association at4.0 units per second.
        if(state.collapseTime>=.25f)state.distanceWithoutGround=std::max(11.0f,state.distanceWithoutGround);
    } else if(state.distanceWithoutGround>10.0f) {
        state.airAnimationId=142;
        state.airAnimationTime=0;
        state.collapseTime=0;
    }
}

// CPed::SetLanding 0x202F98 and CPlayerPed 0x237AB8. The player's moving
// replacement copies the fall_land time/blend; it does not start a roll.
inline int32_t storylandVcsFallLandingAnimation(const StorylandVcsFallState& state,
    float moveBlend,bool movementInput) {
    if(state.distanceWithoutGround>=11.0f)return 139;
    return moveBlend>1.5f&&movementInput?138:141;
}
