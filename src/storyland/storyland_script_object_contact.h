#pragma once

#include "src/storyland/storyland_native_body.h"
#include "src/storyland/storyland_script_object.h"

inline bool storylandScriptObjectModelsTouch(
    const StorylandNativeCollisionModel& first,
    const std::array<float, 16>& firstMatrix,
    const StorylandNativeCollisionModel& second,
    const std::array<float, 16>& secondMatrix) {
    const auto spheresTouch = [](const StorylandNativeCollisionModel& moving,
        const std::array<float, 16>& movingMatrix,
        const StorylandNativeCollisionModel& target,
        const std::array<float, 16>& targetMatrix) {
        for (const auto& sphere : moving.spheres) {
            if (!std::isfinite(sphere.radius) || sphere.radius <= 0)
                continue;
            const auto& point = sphere.center;
            const StorylandNativePoint center{
                movingMatrix[0] * point.x + movingMatrix[4] * point.y +
                    movingMatrix[8] * point.z + movingMatrix[12],
                movingMatrix[1] * point.x + movingMatrix[5] * point.y +
                    movingMatrix[9] * point.z + movingMatrix[13],
                movingMatrix[2] * point.x + movingMatrix[6] * point.y +
                    movingMatrix[10] * point.z + movingMatrix[14]};
            bool hit = false;
            if (storylandNativeSphereModelContact(center, sphere.radius, target, targetMatrix, hit) && hit)
                return true;
        }
        return false;
    };
    return spheresTouch(first, firstMatrix, second, secondMatrix) ||
        spheresTouch(second, secondMatrix, first, firstMatrix);
}

inline bool storylandScriptObjectContact(StorylandMissionEnvironment& world,
    const std::map<uint32_t, StorylandNativeCollisionModel>& models,
    StorylandMissionEnvironment::Object& moving,
    const std::array<float, 3>& position) {
    if (!moving.collisionEnabled || moving.model < 0 ||
        !std::all_of(position.begin(), position.end(), [](float value) { return std::isfinite(value); }))
        return false;
    const auto movingModel = models.find(uint32_t(moving.model));
    if (movingModel == models.end())
        return false;
    const auto movingMatrix = storylandScriptObjectMatrix(position[0], position[1], position[2],
        moving.rotationX, moving.rotationY, moving.heading);
    const StorylandScriptObjectBox movingBounds{movingModel->second.minimum, movingModel->second.maximum,
        {position[0], position[1], position[2]}, moving.heading, moving.rotationX, moving.rotationY};
    bool collided = false;
    for (auto& target : world.objects) {
        if (target.handle == moving.handle || !target.collisionEnabled ||
            target.area != moving.area || target.model < 0)
            continue;
        const auto targetModel = models.find(uint32_t(target.model));
        if (targetModel == models.end())
            continue;
        const StorylandScriptObjectBox targetBounds{targetModel->second.minimum, targetModel->second.maximum,
            {target.x, target.y, target.z}, target.heading, target.rotationX, target.rotationY};
        if (!storylandScriptBoxesOverlap(movingBounds, targetBounds))
            continue;
        const auto targetMatrix = storylandScriptObjectMatrix(target.x, target.y, target.z,
            target.rotationX, target.rotationY, target.heading);
        if (!storylandScriptObjectModelsTouch(movingModel->second, movingMatrix,
            targetModel->second, targetMatrix))
            continue;
        collided = true;
        if (moving.recordsCollisions)
            moving.collided = true;
        if (target.recordsCollisions)
            target.collided = true;
    }
    return collided;
}
