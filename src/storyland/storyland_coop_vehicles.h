#pragma once
#include "src/storyland/storyland_multiplayer.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_play_test_script.h"
#include "src/storyland/storyland_vehicle_occupants.h"

inline bool storylandCoopAtDriverDoor(const StorylandNativePoint& player,
    const StorylandNativeTrafficActor& actor, const StorylandRuntimeVehicle& vehicle) {
    constexpr float radians = .01745329251994329577f;
    const float cosine = std::cos(actor.yaw * radians), sine = std::sin(actor.yaw * radians);
    const float dx = player.x - actor.position.x, dy = player.y - actor.position.y;
    const float along = dx * cosine + dy * sine;
    const float left = -dx * sine + dy * cosine;
    const float doorAlong = vehicle.nativeDefinition.seatPosition(0)[1] - .45f;
    const float doorLeft = vehicle.halfWidth + .5f;
    return std::isfinite(along) && std::isfinite(left) && left > vehicle.halfWidth * .5f &&
        std::fabs(along - doorAlong) <= .85f && std::fabs(left - doorLeft) <= .8f &&
        std::fabs(player.z - actor.position.z) <= 1.25f;
}

inline int32_t storylandCoopVehicleSlot(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& actors, uint32_t identity, int32_t model) {
    if (!identity || model < 0) return -1;
    for (size_t slot = 0; slot < actors.size() && slot < runtime.vehicles.size(); ++slot) {
        const auto& vehicle = runtime.vehicles[slot];
        if (vehicle.coopIdentity == identity && int32_t(vehicle.nativeDefinition.modelId) == model)
            return int32_t(slot);
    }
    return -1;
}

inline int32_t storylandBindCoopVehicle(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& actors, const StorylandNetworkPlayer& remote,
    const std::function<bool(int32_t)>& ensureModel) {
    if (!runtime.missionEnvironment || !remote.vehicleIdentity || remote.vehicleModel < 0 ||
        remote.area != runtime.missionEnvironment->area) return -1;
    int32_t slot = storylandCoopVehicleSlot(runtime, actors, remote.vehicleIdentity, remote.vehicleModel);
    if (slot < 0 && remote.vehicleHandle >= 0) {
        const auto found = runtime.missionEnvironment->missionVehicles.find(remote.vehicleHandle);
        if (found != runtime.missionEnvironment->missionVehicles.end() && !found->second.deleted &&
            found->second.model == remote.vehicleModel && found->second.spawnedSlot >= 0 &&
            size_t(found->second.spawnedSlot) < actors.size() &&
            size_t(found->second.spawnedSlot) < runtime.vehicles.size()) {
            auto& vehicle = runtime.vehicles[size_t(found->second.spawnedSlot)];
            if (!vehicle.coopIdentity || vehicle.coopIdentity == remote.vehicleIdentity) {
                slot = found->second.spawnedSlot;
                vehicle.coopIdentity = remote.vehicleIdentity;
            }
        }
    }
    if (slot < 0) {
        if (ensureModel && !ensureModel(remote.vehicleModel)) return -1;
        const auto definition = std::find_if(runtime.modelDefinitions.begin(), runtime.modelDefinitions.end(),
            [&](const auto& value) { return value.modelId == uint32_t(remote.vehicleModel); });
        if (definition == runtime.modelDefinitions.end()) return -1;
        const size_t selected = storylandMissionVehicleSlot(runtime, actors,
            *runtime.missionEnvironment, runtime.playerPosition);
        if (selected >= runtime.vehicles.size()) return -1;
        slot = int32_t(selected);
        StorylandNativeTrafficActor actor;
        actor.vehicle = true; actor.water = (remote.carFlags & 1u) != 0;
        actor.position = {remote.carX, remote.carY, remote.carZ}; actor.yaw = remote.carHeading;
        if (selected == actors.size()) actors.push_back(actor); else actors[selected] = actor;
        auto& vehicle = runtime.vehicles[selected];
        vehicle = {};
        vehicle.model = size_t(definition - runtime.modelDefinitions.begin());
        storylandApplyVehicleDefinition(vehicle, *definition);
        if (vehicle.model < runtime.modelExtents.size()) {
            const auto& size = runtime.modelExtents[vehicle.model];
            vehicle.halfWidth = size[0]; vehicle.halfLength = size[1]; vehicle.bodyHeight = size[2];
        }
        vehicle.coopIdentity = remote.vehicleIdentity;
        vehicle.hasDriver = false; vehicle.parked = true;
        vehicle.garageOwner = uint8_t(1 - runtime.coopPlayerIndex);
    }
    return slot;
}

inline void storylandApplyCoopVehicle(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& actors, int32_t slot, const StorylandNetworkPlayer& remote) {
    if (slot < 0 || size_t(slot) >= actors.size() || size_t(slot) >= runtime.vehicles.size()) return;
    auto& vehicle = runtime.vehicles[size_t(slot)];
    if (!remote.seated() || remote.seat >= 0) return;
    if (runtime.occupiedVehicle == slot && runtime.playerVehicleSeat < 0) return;
    auto& actor = actors[size_t(slot)];
    actor.position = {remote.carX, remote.carY, remote.carZ}; actor.yaw = remote.carHeading;
    actor.water = (remote.carFlags & 1u) != 0;
    vehicle.coopControlled = true;
    vehicle.garageOwner = uint8_t(1 - runtime.coopPlayerIndex);
    vehicle.parked = true; vehicle.hasDriver = false; vehicle.missionDriver = false; vehicle.aiActive = false;
    vehicle.visible = (remote.carFlags & 64u) != 0; vehicle.collisionEnabled = vehicle.visible;
    vehicle.speed = remote.speed; vehicle.health = std::max(0.0f, remote.carHealth);
    vehicle.primaryColor = remote.primaryColor; vehicle.secondaryColor = remote.secondaryColor;
    vehicle.wheelRotation = remote.wheel; vehicle.rearWheelRotation = remote.rearWheel;
    vehicle.steeringAngle = remote.steering; vehicle.suspension = remote.suspension;
    vehicle.bodyRoll = remote.roll + vehicle.roadRoll; vehicle.bodyPitch = remote.pitch - vehicle.roadPitch;
    // The existing packet carries the already leaned render pose.
    vehicle.bikeVisualLean = 0;
    vehicle.bikeRawSteeringAngle = remote.steering;
    vehicle.bikeTyreAcceleration = 0; vehicle.bikeTyreAccelerationWorld = {};
    vehicle.wheelPositions = {remote.wheelPosition0,remote.wheelPosition1,remote.wheelPosition2,remote.wheelPosition3};
    vehicle.doorOpenRatios = {remote.door0,remote.door1,remote.door2,remote.door3};
    vehicle.headlightsOn = (remote.carFlags & 2u) != 0; vehicle.braking = (remote.carFlags & 4u) != 0;
    vehicle.siren = (remote.carFlags & 8u) != 0; vehicle.burning = (remote.carFlags & 16u) != 0;
    vehicle.wrecked = (remote.carFlags & 32u) != 0;
    if (runtime.missionEnvironment && vehicle.scriptHandle >= 0) {
        auto found = runtime.missionEnvironment->missionVehicles.find(vehicle.scriptHandle);
        if (found != runtime.missionEnvironment->missionVehicles.end()) {
            auto& scripted = found->second;
            scripted.x = remote.carX; scripted.y = remote.carY; scripted.z = remote.carZ;
            scripted.heading = remote.carHeading - 90; scripted.health = vehicle.health;
            scripted.speed = remote.speed; scripted.movedByScript = false;
        }
    }
    if (runtime.occupiedVehicle == slot && runtime.playerVehicleSeat >= 0) {
        runtime.playerPosition = actor.position;
        if (runtime.missionEnvironment) {
            auto& world = *runtime.missionEnvironment;
            world.x = remote.carX; world.y = remote.carY; world.z = remote.carZ;
        }
    }
}
