#pragma once

#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_script_object.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include "src/re/reVCS/runtime/storyland_vcs_pad.h"
#include <algorithm>
#include <cmath>

inline int32_t storylandForkliftVehicleReference(StorylandPlayTestRuntime& runtime,
    const StorylandNativeTrafficActor& actor, size_t slot) {
    auto& world = *runtime.missionEnvironment;
    auto& vehicle = runtime.vehicles[slot];
    if (vehicle.scriptHandle >= 0) {
        const auto existing = world.missionVehicles.find(vehicle.scriptHandle);
        return existing != world.missionVehicles.end() && !existing->second.deleted &&
            existing->second.spawnedSlot == int32_t(slot) ? vehicle.scriptHandle : -1;
    }
    if (world.nextMissionVehicleHandle == INT32_MAX) return -1;
    const int32_t handle = world.nextMissionVehicleHandle++;
    StorylandMissionEnvironment::VehicleScriptState scripted;
    scripted.model = int32_t(vehicle.nativeDefinition.modelId);
    scripted.spawnedSlot = int32_t(slot);
    scripted.x = actor.position.x;
    scripted.y = actor.position.y;
    scripted.z = actor.position.z;
    scripted.heading = actor.yaw - 90;
    scripted.health = vehicle.health;
    scripted.alive = vehicle.health > 0;
    scripted.speed = vehicle.speed;
    scripted.ambientOrigin = true;
    scripted.persistent = false;
    scripted.scriptStatus = runtime.playerVehicleSeat < 0 ? 0 : 3;
    scripted.driver = runtime.playerVehicleSeat < 0 ? 0 : -1;
    world.missionVehicles.emplace(handle, scripted);
    vehicle.scriptHandle = handle;
    storylandCaptureMissionVehicleOverrides(vehicle, actor, world, handle);
    world.occupiedMissionVehicle = handle;
    return handle;
}

inline void storylandAdvanceForklift(StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& actors, float seconds) {
    if (!runtime.crime.vcs || !runtime.missionEnvironment || !std::isfinite(seconds) || seconds <= 0) return;
    auto& world = *runtime.missionEnvironment;
    for (auto& vehicle : runtime.vehicles) {
        auto& fork = vehicle.forklift;
        fork.moving = false;
        if (fork.carriedObject < 0) continue;
        const auto object = std::find_if(world.objects.begin(), world.objects.end(),
            [&](const auto& candidate) { return candidate.handle == fork.carriedObject; });
        if (object == world.objects.end() || object->attachedVehicle != vehicle.scriptHandle ||
            object->attachedCharacter >= 0) {
            fork.carriedObject = -1;
            fork.heightOffset = 0;
        }
    }
    const int32_t occupied = runtime.occupiedVehicle;
    if (occupied < 0 || size_t(occupied) >= runtime.vehicles.size() || size_t(occupied) >= actors.size()) return;
    auto& vehicle = runtime.vehicles[size_t(occupied)];
    const auto& definition = vehicle.nativeDefinition;
    if (!definition.vcsModelLayout || definition.modelId != 251 || !definition.hasNativeForklift) return;
    const auto& actor = actors[size_t(occupied)];
    auto& fork = vehicle.forklift;
    // ProcessForklift150078 applies the old height to the frame before reading the pad.
    fork.visualOffset = storylandForkliftHeightOffset(fork.height);
    const auto frame = storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
        {actor.position.x, actor.position.y, actor.position.z}, actor.yaw,
        definition.previewMinimumZ, vehicle.suspension, vehicle.bodyRoll - vehicle.roadRoll,
        vehicle.bodyPitch + vehicle.roadPitch, actor.water), definition.previewRoot);
    auto localFork = definition.forkliftFramePosition;
    localFork[2] += fork.visualOffset;
    const auto forkPosition = storylandVehicleTransformPoint(frame, localFork);
    float nearestDistance = 9999.0f;
    if (fork.carriedObject < 0 && world.modelIdByName) {
        const int32_t crateModel = world.modelIdByName("GD_CRATEPILE");
        StorylandMissionEnvironment::Object* selected = nullptr;
        if (crateModel >= 0) for (auto& object : world.objects) {
            storylandHydrateScriptObject(world, object);
            if (object.model != crateModel) continue;
            const float distance = std::hypot(object.x - forkPosition[0], object.y - forkPosition[1]);
            if (distance < nearestDistance || (selected && distance == nearestDistance &&
                object.nativePoolIndex >= 0 && object.nativePoolIndex < selected->nativePoolIndex)) {
                nearestDistance = distance;
                selected = &object;
            }
        }
        if (selected && nearestDistance < .3f) {
            const int32_t handle = storylandForkliftVehicleReference(runtime, actor, size_t(occupied));
            if (handle >= 0) {
                auto& object = *selected;
                object.attachmentOffset = storylandVehicleInversePoint(frame, {object.x, object.y, object.z});
                constexpr float degrees = 57.29577951308232f;
                const float vehicleHeading = std::atan2(-frame[4], frame[5]) * degrees;
                object.attachmentRotation = {0, 0, object.heading - vehicleHeading};
                object.attachedVehicle = handle;
                object.attachedCharacter = -1;
                object.attachedBone = -1;
                fork.carriedObject = object.handle;
                //1507FC..150814 retains local attachment Z minus the world fork Z.
                fork.heightOffset = object.attachmentOffset[2] - forkPosition[2];
            }
        }
    }
    if (fork.carriedObject >= 0) {
        const auto object = std::find_if(world.objects.begin(), world.objects.end(),
            [&](const auto& candidate) { return candidate.handle == fork.carriedObject; });
        if (object != world.objects.end()) {
            object->attachmentOffset[2] = forkPosition[2] + fork.heightOffset;
            storylandApplyScriptObjectAttachment(*object, frame, object->attachmentOffset, object->attachmentRotation);
        }
    }
    const int16_t input = world.control ? StorylandVcsPad::directionalAxis(world.padState, true) : 0;
    if (input == 0 || (input < 0 && nearestDistance <= .75f)) return;
    const float speed = input < 0 ? -10.0f : -20.0f;
    const float nativeStep = seconds * 50.0f;
    const int16_t delta = StorylandVcsPad::scaledAxis(input, speed * nativeStep / 128.0f);
    const int32_t height = std::max(0, int32_t(fork.height) + delta);
    fork.height = uint16_t(std::min(2500, height));
    fork.moving = height <= 2500;
}
