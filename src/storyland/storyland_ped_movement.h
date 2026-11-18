#pragma once
#include "src/storyland/storyland_environment.h"
#include <algorithm>
#include <array>
#include <cmath>

inline constexpr float StorylandNativePedMass=70.0f;

// VCS CPed constructor 0x1EE380..0x1EE3C8; LCS 0x1D2784..0x1D27D8.
// Launch callbacks 0x212878/0x1EFA14 supply force; ApplyMoveForce divides by mass.
inline float storylandPedJumpImpulse(bool player,float mass=StorylandNativePedMass) {
    if(!std::isfinite(mass)||mass<=0)return 0;
    return (player?8.5f:4.5f)/mass*StorylandNativePhysicsRate;
}

// VCS 0x2127AC..0x212824; LCS 0x1EF978..0x1EF9E4. These are association weights,
// not walking/running speed multipliers. Native velocities use one 50 Hz step.
inline float storylandPedJumpForwardSpeed(float runBlend,float sprintBlend) {
    runBlend=std::isfinite(runBlend)?std::clamp(runBlend,0.0f,1.0f):0;
    sprintBlend=std::isfinite(sprintBlend)?std::clamp(sprintBlend,0.0f,1.0f):0;
    return (sprintBlend>0?.17f+sprintBlend*.05f:.1f+runBlend*.07f)*StorylandNativePhysicsRate;
}

inline std::array<float,2> storylandPedJumpHorizontal(float x,float y,float headingDegrees,
    float runBlend,float sprintBlend) {
    const float minimum=storylandPedJumpForwardSpeed(runBlend,sprintBlend);
    if(!std::isfinite(x)||!std::isfinite(y))x=y=0;
    if(std::hypot(x,y)>=minimum)return {x,y};
    const float angle=headingDegrees*.0174532925199433f;
    return {std::cos(angle)*minimum,std::sin(angle)*minimum};
}

// Finish the decoded JUMP_launch association before applying its launch force.
// Return only the part of this frame remaining after its finish callback.
inline float storylandAdvanceJumpLaunch(float& elapsed,float duration,float seconds,bool& launching) {
    if(!launching||!std::isfinite(seconds)||seconds<=0)return 0;
    duration=std::isfinite(duration)?std::max(0.0f,duration):0;
    const float remaining=std::max(0.0f,duration-elapsed);
    elapsed+=seconds;
    if(seconds+1.0e-6f<remaining)return 0;
    launching=false;
    return std::max(0.0f,seconds-remaining);
}

// VCS CPlayerPed::ProcessControl 0x232EAC and 0x232F88..0x232FB8;
// LCS 0x3492E8..0x34931C uses the same 0.07 * native timestep limiter.
inline float storylandPedMoveTarget(float amount,bool running,bool analog) {
    if(!std::isfinite(amount))return 0;
    amount=std::clamp(amount,0.0f,1.0f);
    if(amount<=0)return 0;
    return analog?std::min(running?2.0f:1.0f,amount*(128.0f/60.0f)):
        running?2.0f:1.0f;
}
inline float storylandStepPedMoveBlend(float previous,float target,float seconds) {
    previous=std::isfinite(previous)?std::clamp(previous,0.0f,2.0f):0;
    target=std::isfinite(target)?std::clamp(target,0.0f,2.0f):0;
    if(target<=previous)return target;
    return std::min(target,previous+std::max(0.0f,seconds)*3.5f);
}
inline float storylandStepPedSprintBlend(float previous,bool sprinting,float seconds,
    float moveRatio=2,bool alreadyFading=false) {
    previous=std::isfinite(previous)?std::clamp(previous,0.0f,1.0f):0;
    // VCS 0x23279C..0x2327DC: only an association already fading out
    // accelerates its fade, with both threshold comparisons strict.
    const bool fast=(!sprinting||moveRatio<.4f)&&alreadyFading&&previous<.8f&&moveRatio<1;
    const float delta=std::max(0.0f,seconds)*(fast?-8.0f:sprinting?2.0f:-1.0f);
    return std::clamp(previous+delta,0.0f,1.0f);
}
inline float storylandPedGaitSpeed(float ratio,float sprintBlend,float walk,float run,float sprint) {
    ratio=std::clamp(ratio,0.0f,2.0f);sprintBlend=std::clamp(sprintBlend,0.0f,1.0f);
    const float base=ratio<=1?walk*ratio:walk*(2-ratio)+run*(ratio-1);
    return base*(1-sprintBlend)+sprint*sprintBlend;
}

struct StorylandPedVerticalStep {
    float displacement=0,velocity=0;
};
inline StorylandPedVerticalStep storylandPedVerticalStep(float velocity,float gravity,float seconds) {
    if(!std::isfinite(velocity)||!std::isfinite(gravity)||!std::isfinite(seconds)||seconds<=0)
        return {0,std::isfinite(velocity)?velocity:0};
    gravity=std::max(0.0f,gravity);
    return {velocity*seconds-.5f*gravity*seconds*seconds,velocity-gravity*seconds};
}
