#pragma once

#include "src/storyland/storyland_screen_droplets.h"
#include "src/storyland/storyland_water_cannon.h"

#include <array>
#include <cmath>

inline float storylandScreenWaterSpray(const StorylandScreenDropletCamera& camera,
    const std::array<StorylandWaterCannon,3>& cannons) {
    for(const auto& cannon:cannons) {
        if(cannon.ownerSlot<0)continue;
        for(const auto& sample:cannon.samples) {
            if(!sample.active)continue;
            float distanceSquared=0,depth=0;
            for(std::size_t axis=0;axis<3;++axis) {
                const float delta=sample.position[axis]-camera.position[axis];
                distanceSquared+=delta*delta;
                depth+=delta*camera.forward[axis];
            }
            if(std::isfinite(distanceSquared)&&std::isfinite(depth)&&
                distanceSquared<40.0f&&depth>=0)return 1.0f;
        }
    }
    return 0.0f;
}
