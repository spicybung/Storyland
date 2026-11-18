#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace StorylandVcsPedCameraZoom {
constexpr int32_t defaultIndicator=2;
constexpr float followDistance=2.0f;

inline int32_t normalizeIndicator(int32_t indicator) {
    if(indicator>3)return 1;
    if(indicator<1)return 3;
    return indicator;
}

inline float target(int32_t indicator) {
    constexpr float offsets[]={0.0f,2.5f,3.5f};
    return offsets[normalizeIndicator(indicator)-1];
}

inline int32_t cycle(int32_t indicator,bool nearer,bool farther,bool frozen) {
    if(!frozen) {
        if(nearer)indicator=int32_t(uint32_t(indicator)-1u);
        if(farther)indicator=int32_t(uint32_t(indicator)+1u);
    }
    return normalizeIndicator(indicator);
}

inline float advance(float current,float desired,float nativeTimeStep) {
    const float step=0.12f*std::max(0.0f,nativeTimeStep);
    return current<desired?std::min(desired,current+step):std::max(desired,current-step);
}

inline float distance(float current) {
    return std::max(0.03f,followDistance+current);
}

struct Input {
    bool nearer=false;
    bool farther=false;
};

inline Input pressed(uint16_t controllerMode,
    const std::array<int16_t,20>& current,const std::array<int16_t,20>& previous) {
    const auto edge=[&](size_t index) {return current[index]!=0&&previous[index]==0;};
    if(controllerMode==1)return {edge(8),edge(9)};
    if(controllerMode==0||controllerMode==2||controllerMode==3)return {edge(13),false};
    return {};
}
}
