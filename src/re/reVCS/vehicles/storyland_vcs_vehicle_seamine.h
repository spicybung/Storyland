#pragma once

#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <array>
#include <cmath>

struct StorylandVcsVehicleSeaMineLaunch {
    StorylandNativePoint position{}, lineStart{}, lineEnd{}, velocity{};
    float yaw=0;
};

// Original 2CFDF8 constructs weapon 15 at vehicle position - forward + up.
// 2C9660 then clamps the point ahead of the source and tests its line of fire.
inline bool storylandVcsVehicleSeaMineLaunch(const std::array<float,16>& matrix,
    StorylandVcsVehicleSeaMineLaunch& result) {
    if (!std::all_of(matrix.begin(),matrix.end(),[](float value){return std::isfinite(value);}))
        return false;
    const StorylandNativePoint position{matrix[12],matrix[13],matrix[14]};
    const StorylandNativePoint forward{matrix[4],matrix[5],matrix[6]};
    const StorylandNativePoint up{matrix[8],matrix[9],matrix[10]};
    StorylandNativePoint spawn{position.x-forward.x+up.x,position.y-forward.y+up.y,
        position.z-forward.z+up.z};
    float longitudinal=(spawn.x-position.x)*forward.x+(spawn.y-position.y)*forward.y+
        (spawn.z-position.z)*forward.z;
    if(longitudinal<.3f) {
        const float adjustment=.3f-longitudinal;
        spawn.x+=adjustment*forward.x;spawn.y+=adjustment*forward.y;spawn.z+=adjustment*forward.z;
    }
    longitudinal=(spawn.x-position.x)*forward.x+(spawn.y-position.y)*forward.y+
        (spawn.z-position.z)*forward.z;
    result.position=spawn;
    result.lineStart={spawn.x-longitudinal*forward.x,spawn.y-longitudinal*forward.y,
        spawn.z-longitudinal*forward.z};
    result.lineEnd=spawn;
    if(spawn.z>position.z) {
        result.lineEnd.x+=.6f*forward.x;result.lineEnd.y+=.6f*forward.y;
        result.lineEnd.z+=.6f*forward.z;
    }
    const float horizontal=std::hypot(forward.x,forward.y);
    const float x=horizontal>0?forward.x/horizontal:0;
    const float y=horizontal>0?forward.y/horizontal:1;
    // Native mine creation uses 0.2 per simulation step, converted to units/second.
    result.velocity={-10.0f*x,-10.0f*y,10.0f};
    result.yaw=std::atan2(-x,y)*57.29577951308232f;
    return true;
}
