#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

inline bool storylandMotorcycleDoorPosition(const std::array<float,16>& frame,
    const std::array<float,3>& root,const std::array<float,3>& seat,
    const std::array<float,3>& collisionMaximum,bool rightHandSide,
    std::array<float,3>& result) {
    const auto finite=[](const auto& values) {
        return std::all_of(values.begin(),values.end(),[](float value){return std::isfinite(value);});
    };
    if(!finite(frame)||!finite(root)||!finite(seat)||!finite(collisionMaximum))return false;
    const std::array<float,3> forward{frame[4],frame[5],frame[6]};
    const std::array<float,3> right{forward[1]*1.0f-forward[2]*0.0f,
        -forward[0]*1.0f+forward[2]*0.0f,forward[0]*0.0f-forward[1]*0.0f};
    const std::array<float,3> vertical{right[1]*forward[2]-right[2]*forward[1],
        -right[0]*forward[2]+right[2]*forward[0],right[0]*forward[1]-right[1]*forward[0]};
    const float projectedUp=(frame[8]*right[0]+frame[9]*right[1])+frame[10]*right[2];
    const float correction=projectedUp*(collisionMaximum[0]<collisionMaximum[2]?
        collisionMaximum[2]-collisionMaximum[0]:0.0f);
    const float x=(seat[0]+(rightHandSide?root[0]:-root[0]))+correction;
    const float y=seat[1]-root[1],z=seat[2]-root[2];
    std::array<float,3> point{};
    for(size_t axis=0;axis<3;++axis)
        point[axis]=((y*forward[axis]+x*right[axis])+z*vertical[axis])+frame[12+axis];
    if(!finite(point))return false;
    result=point;
    return true;
}
