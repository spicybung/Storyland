#pragma once

#include "src/re/reVCS/runtime/storyland_vcs_projectile.h"

namespace StorylandVcsHunterRockets {

inline bool ready(bool pressed, uint32_t now, uint32_t lastShot) {
    return pressed && uint32_t(lastShot + 350u) < now;
}

inline bool fire(StorylandMissionEnvironment& world, int32_t vehicleHandle,
    float frameSeconds, std::string& error) {
    const auto vehicle = world.missionVehicles.find(vehicleHandle);
    if (vehicle == world.missionVehicles.end() || vehicle->second.deleted ||
        vehicle->second.model != 275 || !world.vehicleWorldMatrix) {
        error = "Hunter rockets require a live Hunter and its world matrix.";
        return false;
    }
    std::array<float,16> matrix{};
    if (!world.vehicleWorldMatrix(vehicleHandle, matrix)) {
        error = "Hunter rockets require the vehicle's world matrix.";
        return false;
    }
    if (!std::all_of(matrix.begin(), matrix.end(), [](float value) { return std::isfinite(value); }) ||
        !std::isfinite(frameSeconds) || frameSeconds < 0.0f) {
        error = "Hunter rocket launch coordinates are invalid.";
        return false;
    }
    size_t freeSlots = 0;
    for (const auto& projectile : world.scriptProjectiles)
        if (!projectile.active) ++freeSlots;
    std::array<float,3> velocity = vehicle->second.velocity;
    if (world.vehicleWorldVelocity) world.vehicleWorldVelocity(vehicleHandle, velocity);
    if (!std::all_of(velocity.begin(), velocity.end(), [](float value) { return std::isfinite(value); })) {
        error = "Hunter rocket launch velocity is invalid.";
        return false;
    }
    std::array<int32_t,2> handles{-1,-1};
    // Native firing still advances its cooldown when the projectile pool is full.
    const size_t shotCount = std::min(freeSlots, handles.size());
    for (size_t side = 0; side < shotCount; ++side) {
        const float lateral = side == 0 ? 2.75f : -2.75f;
        std::array<float,3> position{};
        for (size_t axis = 0; axis < position.size(); ++axis)
            position[axis] = matrix[12 + axis] + matrix[axis] * lateral +
                matrix[4 + axis] * 2.5f - matrix[8 + axis] * 0.5f +
                velocity[axis] * frameSeconds;
        if (!storylandCreateVcsScriptProjectile(world, 18, position, {}, 1.0f,
            {StorylandMissionEnvironment::DamageSourceKind::Vehicle, vehicleHandle},
            0, false, handles[side], error)) {
            for (size_t previous = 0; previous < side; ++previous)
                world.scriptProjectiles[size_t(handles[previous])].active = false;
            return false;
        }
    }
    return true;
}

}
