#pragma once

#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

struct StorylandPedVehicleContactState {
    std::vector<uint64_t> touchingVehicles;
    StorylandNativePoint previousPosition{}, displacement{};
    bool previousPositionValid = false, pushedAlong = false;
};

struct StorylandVehiclePedSweepState {
    StorylandNativePoint position{};
    float heading = 0;
    uint32_t model = UINT32_MAX;
    uint64_t identity = 0;
    bool initialized = false;
};

struct StorylandVehiclePedImpactPolicy {
    float damage = 0;
    float carriedVelocity = 0;
    float recoilForce = 0;
    bool knockdown = false;
    bool lethal = false;
};

inline StorylandVehiclePedImpactPolicy storylandNativeVehiclePedImpact(
    bool player, bool alreadyDown, float nativeImpulse, float approachSpeed,
    float vehicleSpeed, float normalZ, bool pushedAlong = false) {
    StorylandVehiclePedImpactPolicy result;
    if (alreadyDown || !std::isfinite(nativeImpulse) || !std::isfinite(approachSpeed) ||
        !std::isfinite(vehicleSpeed) || !std::isfinite(normalZ) || nativeImpulse <= 0 ||
        vehicleSpeed <= 2.5f) return result;
    // reLCS CPed::ProcessControl; VCS 1F5E8C..1F5F28. Host speeds are metres/second.
    if (player) {
        if (approachSpeed > 5.0f) {
            result.damage = pushedAlong ? 10.0f : 20.0f;
            result.knockdown = true;
        }
        return result;
    }
    // reLCS CPed::KillPedWithCar; VCS 1DA408..1DB334.
    if (nativeImpulse > 12.0f) {
        result.damage = 1000.0f;
        result.carriedVelocity = .9f;
        result.recoilForce = 100.0f;
        result.knockdown = true;
        result.lethal = true;
    } else if ((normalZ < -.8f && nativeImpulse > 3.0f) || nativeImpulse > 6.0f) {
        result.damage = 30.0f;
        result.carriedVelocity = .75f;
        result.recoilForce = 60.0f;
        result.knockdown = true;
    }
    return result;
}

inline float storylandNativePedVehicleImpulse(float closingSpeed, float pedMass,
    float vehicleMass, float turnMass, float leverSquared, bool fixedVehicle,
    bool playerAgainstAbandonedVehicle = false) {
    if (!std::isfinite(closingSpeed) || !std::isfinite(pedMass) ||
        !std::isfinite(vehicleMass) || !std::isfinite(turnMass) ||
        !std::isfinite(leverSquared) || closingSpeed <= 0 || pedMass <= 0 ||
        vehicleMass <= 0 || turnMass <= 0 || leverSquared < 0) return 0;
    // CPhysical::ApplyCollision/GetMassTweak: the vehicle's contact mass is
    // multiplied by ten against a pedestrian, before the equal-speed solve.
    const float factor = playerAgainstAbandonedVehicle ?
        1.0f / (std::max(vehicleMass - 2000.0f, 0.0f) / 5000.0f + 1.0f) : 10.0f;
    const float inverseVehicleMass = fixedVehicle ? 0.0f :
        (1.0f / vehicleMass + leverSquared / turnMass) / factor;
    return (closingSpeed / 50.0f) * 1.05f /
        (1.0f / pedMass + inverseVehicleMass);
}

struct StorylandPlayTestRuntime;
void storylandUpdateVehiclePedestrianImpacts(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, std::vector<StorylandNativeTrafficActor>& pedestrians,
    std::vector<StorylandNativeTrafficActor>& vehicles,
    const std::vector<StorylandNativeTrafficActor>& vehicleStarts,
    const StorylandNativePoint& player, float seconds);
void storylandApplyPlayerVehicleDisplacement(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, StorylandNativePoint& player);
