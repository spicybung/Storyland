#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// VCS 0x2BD608 scans the native fire pool in order and selects the farthest
// active, non-script-reserved fire strictly between ten and thirty-five units.
template<class Fires>
inline int32_t storylandSelectVcsFiretruckTarget(const Fires& fires,const std::array<float,3>& position) {
    int32_t selected=-1;
    float farthest=0;
    for(const auto& entry:fires) {
        if(entry.first<0||entry.first>=40||!entry.second.active||entry.second.scriptOwned)continue;
        const float dx=entry.second.position[0]-position[0];
        const float dy=entry.second.position[1]-position[1];
        const float distance=std::sqrt(dx*dx+dy*dy);
        if(distance>10.0f&&distance<35.0f&&distance>farthest) {
            farthest=distance;
            selected=entry.first;
        }
    }
    return selected;
}

inline bool storylandVcsFiretruckSprayWindow(uint32_t milliseconds) {
    return ((milliseconds>>10u)&3u)!=0;
}

inline void storylandAimVcsFiretruckAtFire(float& yaw,float& pitch,float nativeStep,
    const std::array<float,16>& frame,const std::array<float,3>& fire,uint32_t milliseconds) {
    constexpr float pi=3.141592741f;
    constexpr float twoPi=6.283185482f;
    float fireAngle=std::atan2(fire[1]-frame[13],fire[0]-frame[12]);
    if(fireAngle<0)fireAngle+=twoPi;
    float vehicleAngle=std::atan2(frame[5],frame[4]);
    if(vehicleAngle<0)vehicleAngle+=twoPi;
    const float target=vehicleAngle-fireAngle;
    float difference=target-yaw;
    if(difference<-pi)difference+=twoPi;
    else if(difference>=pi)difference-=twoPi;
    const float rate=.01f*nativeStep;
    if(std::fabs(difference)<rate)yaw=target;
    else if(difference>0)yaw+=rate;
    else yaw-=rate;
    pitch=.2f*std::sin(float(milliseconds&4095u)*.001533203176f)+.2f;
}
