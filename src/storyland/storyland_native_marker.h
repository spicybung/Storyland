#pragma once

#include "src/storyland/storyland_mission_vm.h"
#include <array>
#include <cmath>
#include <cstdint>

inline uint32_t storylandNativeMarkerPeriod(uint8_t type) {
    return type==7u?1024u:2048u;
}

inline float storylandNativeMarkerPulse(uint8_t type,uint32_t createdAt,uint32_t now) {
    const uint32_t period=storylandNativeMarkerPeriod(type);
    const uint32_t elapsed=(now-createdAt)&(period-1u);
    return std::sin(float(elapsed)*6.2831853071795864769f/float(period));
}

inline std::array<float,3> storylandNativeMarkerPosition(
    const StorylandMissionEnvironment::Marker& marker,uint32_t now) {
    std::array<float,3> position{marker.x,marker.y,marker.z};
    if(marker.type==7u)
        position[2]+=0.7f+marker.radius*0.6f*
            storylandNativeMarkerPulse(marker.type,marker.createdAt,now);
    return position;
}
