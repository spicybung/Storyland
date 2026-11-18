#pragma once
#include "src/leeds/leeds_native_world.h"
#include "src/storyland/storyland_vehicle_contact.h"
#include <algorithm>
#include <array>
#include <cmath>

struct StorylandRoadSkid {
    StorylandNativePoint start, end, normal;
    float width = .18f, remaining = 25, lifetime = 25, opacity = 1;
    int area = 0;
    bool blood = false;
    float textureStart=0,textureEnd=1;
    float fadeSeconds=5;
};

inline bool storylandVcsAutomobileRubberSmoke(
    const std::array<StorylandVehicleContact::TyreState,4>& states,
    size_t wheel,float forwardSpeed) {
    using State=StorylandVehicleContact::TyreState;
    if(wheel>=states.size()||!std::isfinite(forwardSpeed))return false;
    const auto state=states[wheel];
    if(state==State::Spinning)return true;
    // 0x142DA0 and 0x142E84 use a strict 5 km/h forward-speed threshold.
    if(std::fabs(forwardSpeed)*3.6f<=5.0f)return false;
    if(state==State::Locked)return true;
    if(state!=State::Sliding)return false;
    // Sliding front wheels emit only if either rear wheel is also sliding.
    return (wheel&1u)!=0||states[1]==State::Sliding||states[3]==State::Sliding;
}

inline float storylandVehicleTyreSlip(float speed, float lateralSpeed, float steeringDegrees,
    float yawDegrees, float wheelbase, bool handbrake, bool braking, bool grounded) {
    if (!grounded || !std::isfinite(speed) || !std::isfinite(lateralSpeed) ||
        !std::isfinite(steeringDegrees) || !std::isfinite(yawDegrees)) return 0;
    const float velocity = std::fabs(speed);
    if (velocity < 1) return 0;
    constexpr float radians = .0174532925199433f;
    const float requestedYaw = speed / std::max(.5f, wheelbase) *
        std::tan(std::clamp(steeringDegrees, -65.0f, 65.0f) * radians);
    const float scrub = std::fabs(requestedYaw - yawDegrees * radians) * velocity;
    const float lateral = std::clamp((std::fabs(lateralSpeed) - .8f) / 4.0f, 0.0f, 1.0f);
    const float understeer = std::clamp((scrub - 3.0f) / 12.0f, 0.0f, 1.0f);
    const float locked = handbrake ? std::clamp((velocity - 2.0f) / 8.0f, 0.0f, 1.0f) : 0;
    const float brakingSlip = braking ? std::clamp((velocity - 16.0f) / 20.0f, 0.0f, .35f) : 0;
    return std::max({lateral, understeer, locked, brakingSlip});
}

inline bool storylandTyreLeavesRoadMark(uint8_t surface) {
    switch(surface) {
    case 2:case 3:case 4:case 8:case 17:case 18:case 19:case 23:case 25:case 33:return false;
    default:return true;
    }
}

inline bool storylandSkidCanJoin(const StorylandNativePoint& previous,
    const StorylandNativePoint& current, float seconds, float speed) {
    const float length = std::hypot(current.x - previous.x, current.y - previous.y);
    return std::isfinite(length) && length >= .06f &&
        length <= std::max(.8f, std::fabs(speed) * std::min(seconds, .1f) * 3 + .4f) &&
        std::fabs(current.z - previous.z) < .65f;
}

inline bool storylandGeneratorMatchesMission(int32_t generatorModel,
    const StorylandNativePoint& generator, float generatorHeading, int32_t missionModel,
    const StorylandNativePoint& mission, float missionHeading, bool headingKnown=true) {
    return generatorModel == missionModel &&
        std::hypot(generator.x - mission.x, generator.y - mission.y) <= .25f &&
        std::fabs(generator.z - mission.z) <= 2 &&
        (!headingKnown || std::fabs(std::remainder(generatorHeading - missionHeading, 360.0f)) <= 5);
}
