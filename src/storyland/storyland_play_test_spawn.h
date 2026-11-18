#pragma once

#include "src/storyland/storyland_native_body.h"
#include "src/storyland/storyland_play_test_camera.h"
#include <algorithm>
#include <cmath>
#include <vector>

struct StorylandPlayTestSpawn {
    StorylandNativePoint position;
    float heading = 0;
};

// Find a walkable, collision-supported native pedestrian path near the desired
// location. Path heights can lie inside a bridge or below a second road deck,
// so trace locally and verify both the standing body and a short walking route.
inline bool storylandFindPlayTestSpawn(const StorylandNativeWorld& world,
    const StorylandNativePoint& origin, StorylandPlayTestSpawn& result) {
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z)) return false;
    const auto& nodes = world.paths.nodes;
    std::vector<size_t> candidates;
    candidates.reserve(nodes.size());
    for (size_t index = 0; index < nodes.size(); ++index) {
        const auto& node = nodes[index];
        if (node.vehicle || node.water || node.disabled || node.links.empty() ||
            !std::isfinite(node.position.x) || !std::isfinite(node.position.y) ||
            !std::isfinite(node.position.z)) continue;
        candidates.push_back(index);
    }
    const auto distance = [&](size_t index) {
        const auto& point = nodes[index].position;
        return std::hypot(point.x-origin.x,point.y-origin.y,point.z-origin.z);
    };
    std::stable_sort(candidates.begin(),candidates.end(),[&](size_t first,size_t second) {
        return distance(first) < distance(second);
    });
    constexpr float radians = .01745329251994329577f;
    const auto body = storylandNativeCharacterBody();
    for (size_t index : candidates) {
        const auto& node = nodes[index];
        const StorylandNativePoint top{node.position.x,node.position.y,node.position.z+2.0f};
        const StorylandNativePoint bottom{node.position.x,node.position.y,node.position.z-4.0f};
        const float fraction = storylandTraceNativeMapGround(world,top,bottom);
        if (!std::isfinite(fraction) || fraction < 0 || fraction >= 1) continue;
        StorylandNativePoint start{node.position.x,node.position.y,top.z+(bottom.z-top.z)*fraction};
        if (!storylandNativeBodyClear(world,start,body)) continue;
        std::vector<float> headings;
        for (const auto& link : node.links) {
            if (link.target >= nodes.size()) continue;
            const auto& target = nodes[link.target];
            const float x=target.position.x-start.x,y=target.position.y-start.y;
            if (std::hypot(x,y) > .1f) headings.push_back(std::atan2(y,x)/radians);
        }
        headings.insert(headings.end(),{0,90,180,270});
        for (float heading : headings) {
            StorylandNativePoint point=start;
            const float x=std::cos(heading*radians),y=std::sin(heading*radians);
            bool clear=true;
            for (unsigned step=0;step<12;++step) {
                const StorylandNativePoint target{point.x+x,point.y+y,point.z};
                const auto movement=storylandMoveNativeCharacter(world,point,target);
                if (!movement.clear || movement.blocked || movement.recovered ||
                    std::hypot(movement.position.x-point.x,movement.position.y-point.y)<.9f) {
                    clear=false;break;
                }
                point=movement.position;
                const StorylandNativePoint above{point.x,point.y,point.z+.4f};
                const StorylandNativePoint below{point.x,point.y,point.z-.5f};
                const float support=storylandTraceNativeMapGround(world,above,below);
                if (!std::isfinite(support) || support < 0 || support >= 1) {
                    clear=false;break;
                }
                point.z=above.z+(below.z-above.z)*support;
            }
            if (clear) {result={start,heading};return true;}
        }
    }
    return false;
}
