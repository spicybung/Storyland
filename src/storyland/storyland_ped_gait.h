#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// VCS SetMoveAnim: walk/run weights at 0x232854, sprint deltas at
// 0x232774/0x23282C, and CPed idle/moving deltas at 0x1EFC84..0x1EFD30.
// LCS equivalents: player 0x348B90..0x348CD4, CPed 0x1D3A44..0x1D3AB8.
inline std::array<float,4> storylandStepNativeGaitWeights(std::array<float,4> current,
    unsigned motion,bool player,float ratio,float sprint,float seconds) {
    const float dt=std::clamp(seconds,0.0f,.1f);
    if(!player) {
        motion=std::min(3u,motion);
        const float incoming=std::min(1.0f,current[motion]+dt*(motion?1.0f:4.0f));
        const float outgoing=1-current[motion];
        for(unsigned i=0;i<4;++i)if(i!=motion)
            current[i]=outgoing>1e-6f?current[i]*(1-incoming)/outgoing:0;
        current[motion]=incoming;
        return current;
    }
    const bool moving=motion!=0;
    const float idle=std::clamp(current[0]+(moving?-4.0f:4.0f)*dt,0.0f,1.0f);
    const float run=std::clamp(ratio-1.0f,0.0f,1.0f);
    const float sprintWeight=std::clamp(sprint,0.0f,1.0f);
    return {idle,(1-idle)*(1-sprintWeight)*(1-run),
        (1-idle)*(1-sprintWeight)*run,(1-idle)*sprintWeight};
}

// VCS 0x23257C..0x232758 / LCS 0x348A10..0x348B74. An already
// fading sprint below 80% takes the fast-fade path before the stop test.
inline int storylandNativeSprintStopVariant(float sprintWeight,bool alreadyFading,
    float moveRatio,float phase,bool sprinting) {
    if(sprintWeight<=0||(sprinting&&moveRatio>=.4f)||
        (alreadyFading&&sprintWeight<.8f)||moveRatio>=.4f)return -1;
    phase=std::fmod(std::max(0.0f,phase),1.0f);
    return phase<.5f?0:1;
}
