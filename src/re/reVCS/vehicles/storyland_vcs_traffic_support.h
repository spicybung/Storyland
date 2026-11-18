#pragma once

#include "src/leeds/leeds_native_world.h"
#include <array>
#include <cmath>

namespace StorylandVcsTrafficSupport {

struct Hit {
    bool found = false;
    StorylandNativePoint point;
    StorylandNativePoint normal;
    uint8_t surface = 0;
};

struct Placement {
    bool valid = false;
    bool upward = false;
    Hit contact;
    std::array<float,16> matrix{};
};

struct RailState {
    bool active = false;
    float frontHeight = 0;
    float rearHeight = 0;
};

struct RailFootprint {
    bool valid = false;
    StorylandNativePoint front;
    StorylandNativePoint rear;
};

struct RailRay {
    StorylandNativePoint start;
    StorylandNativePoint end;
};

inline bool simpleSpawn(int rating, bool boat, int mission) {
    if(rating == 13 || rating == 24)return mission == 1;
    return rating != 23 && !boat;
}

inline bool madDriver(int appearance, uint32_t random, bool cheat) {
    const uint32_t threshold = appearance == 2 ? 20u : appearance == 4 ? 40u : 3u;
    return (random & 127u) < threshold || cheat;
}

inline bool groundCheckDue(uint16_t seed, uint32_t frame) {
    return (seed & 63u) == (frame & 63u);
}

inline StorylandNativePoint queryStart(StorylandNativePoint pathPosition) {
    pathPosition.z += 1.5f;
    return pathPosition;
}

inline Placement resolve(const StorylandNativePoint& pathPosition,
    const Hit& upward, const Hit& downward, const StorylandNativePoint& right,
    float collisionMinimumZ) {
    Placement result;
    const float queryZ = pathPosition.z + 1.5f;
    float groundZ = 1000000000.0f;
    if(upward.found) {
        result.contact = upward;
        result.upward = true;
        groundZ = upward.point.z;
    }
    if(downward.found && std::fabs(downward.point.z-queryZ) < std::fabs(groundZ-queryZ)) {
        result.contact = downward;
        result.upward = false;
        groundZ = downward.point.z;
    }
    // SLUS_215.90 431A00..431B58: absolute ray ends +1000/-1000;
    // equal distances keep the upward hit, with range checked against path Z.
    if(!result.contact.found || groundZ == 1000000000.0f ||
        std::fabs(groundZ-pathPosition.z) > 7.0f)return result;

    auto up = result.contact.normal;
    if(up.z <= 0.0f)up = {-up.x,-up.y,-up.z};
    const float denominator = up.z == 0.0f ? 1.0f : up.z;
    const StorylandNativePoint forward{
        up.y*right.z-up.z*right.y,
        -up.x*right.z+up.z*right.x,
        up.x*right.y-up.y*right.x
    };
    const auto& point = result.contact.point;
    // 3C61E0 preserves right and uses up cross right without normalization;
    // 3C4B08 returns minus the native COL minimum.z, not suspension ride height.
    result.matrix = {
        right.x,right.y,right.z,0,
        forward.x,forward.y,forward.z,0,
        up.x,up.y,up.z,0,
        point.x,point.y,point.z+(-collisionMinimumZ)/denominator,1
    };
    result.valid = true;
    return result;
}

inline RailFootprint railFootprint(const StorylandNativePoint& oldOrigin,
    const StorylandNativePoint& oldForward, const StorylandNativePoint& curveTarget) {
    RailFootprint result;
    const float behindX = oldOrigin.x-oldForward.x*1.5f;
    const float behindY = oldOrigin.y-oldForward.y*1.5f;
    const float directionX = behindX-curveTarget.x;
    const float directionY = behindY-curveTarget.y;
    const float squared = directionX*directionX+directionY*directionY;
    if(!std::isfinite(squared) || squared <= 0.0f)return result;
    const float scale = 3.0f/std::sqrt(squared);
    result.front = {curveTarget.x,curveTarget.y,oldOrigin.z};
    result.rear = {curveTarget.x+directionX*scale,
        curveTarget.y+directionY*scale,oldOrigin.z};
    result.valid = true;
    return result;
}

inline RailRay railRay(const StorylandNativePoint& point, float originZ, bool retry) {
    return {{point.x,point.y,originZ+(retry ? 3.0f : 1.5f)},
        {point.x,point.y,originZ-(retry ? 3.0f : 2.0f)}};
}

inline float railHeight(const Hit& first, const Hit& retry, float previous) {
    return first.found ? first.point.z : retry.found ? retry.point.z : previous;
}

inline std::array<float,16> railMatrix(const RailFootprint& footprint,
    float frontHeight, float rearHeight, float heightAboveRoad) {
    // SLUS_215.90 437B98..43805C: fixed three-unit probe separation.
    const float inverseLength = 1.0f/3.0f;
    const float pitch = std::atan((frontHeight-rearHeight)*inverseLength);
    const float cosine = std::cos(pitch);
    const float sine = std::sin(pitch);
    const StorylandNativePoint right{
        (footprint.front.y-footprint.rear.y)*inverseLength,
        (footprint.rear.x-footprint.front.x)*inverseLength,0
    };
    const StorylandNativePoint forward{-cosine*right.y,cosine*right.x,sine};
    const StorylandNativePoint up{
        right.y*forward.z,-right.x*forward.z,
        right.x*forward.y-right.y*forward.x
    };
    return {
        right.x,right.y,right.z,0,
        forward.x,forward.y,forward.z,0,
        up.x,up.y,up.z,0,
        (footprint.front.x+footprint.rear.x)*.5f,
        (footprint.front.y+footprint.rear.y)*.5f,
        (frontHeight+rearHeight)*.5f+heightAboveRoad,1
    };
}

}
