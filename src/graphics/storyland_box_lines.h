#pragma once
#include "src/leeds/leeds_native_world.h"
#include <cmath>

// Each corner only draws edges toward the high side (12 edges, no doubles).
template<class Vertex>
void storylandBoxLines(StorylandNativePoint low,StorylandNativePoint high,Vertex vertex) {
    if(!std::isfinite(low.x)||!std::isfinite(low.y)||!std::isfinite(low.z)||
       !std::isfinite(high.x)||!std::isfinite(high.y)||!std::isfinite(high.z)||
       low.x>high.x||low.y>high.y||low.z>high.z)return;
    const auto corner=[&](unsigned index) {
        return StorylandNativePoint{index&1?high.x:low.x,index&2?high.y:low.y,index&4?high.z:low.z};
    };
    for(unsigned index=0;index<8;++index) {
        for(unsigned axis=1;axis<=4;axis*=2) {
            if(index&axis)continue;
            vertex(corner(index));vertex(corner(index|axis));
        }
    }
}
