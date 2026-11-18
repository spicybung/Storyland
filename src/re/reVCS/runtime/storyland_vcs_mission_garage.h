#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

inline std::array<float,4> storylandVcsMissionGarageBounds(const std::array<float,8>& bounds) {
    const float fourthX=bounds[3]+bounds[5]-bounds[0];
    const float fourthY=bounds[4]+bounds[6]-bounds[1];
    return {std::min({bounds[0],bounds[3],bounds[5],fourthX}),
        std::max({bounds[0],bounds[3],bounds[5],fourthX}),
        std::min({bounds[1],bounds[4],bounds[6],fourthY}),
        std::max({bounds[1],bounds[4],bounds[6],fourthY})};
}

inline float storylandVcsMissionGarageDistanceSquared(const std::array<float,8>& bounds,
    const std::array<float,3>& point) {
    if(!std::all_of(bounds.begin(),bounds.end(),[](float value){return std::isfinite(value);})||
        !std::isfinite(point[0])||!std::isfinite(point[1]))
        return std::numeric_limits<float>::infinity();
    const auto outer=storylandVcsMissionGarageBounds(bounds);
    const float x=std::max({outer[0]-point[0],point[0]-outer[1],0.0f});
    const float y=std::max({outer[2]-point[1],point[1]-outer[3],0.0f});
    return x*x+y*y;
}

inline bool storylandVcsMissionGarageCanOpen(bool locked,bool inVehicle,int32_t target,
    int32_t occupied,const std::array<float,8>& bounds,const std::array<float,3>& position) {
    // Original420758..4207B8 admits only the occupied target within squared XY distance64.
    return !locked&&inVehicle&&target>=0&&occupied==target&&
        storylandVcsMissionGarageDistanceSquared(bounds,position)<64.0f;
}

inline float storylandVcsMissionGarageDoorPosition(float position,float height,float nativeStep,
    bool swing,bool opening) {
    const float rate=opening?(swing?.015f:.035f):(swing?.02f:.04f);
    return opening?std::min(height,position+nativeStep*rate):
        std::max(0.0f,position-nativeStep*rate);
}
