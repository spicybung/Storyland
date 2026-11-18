#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/leeds/leeds_native_world.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_mission_vehicle_transform.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

inline bool storylandScriptQueryBoundsValid(const std::array<float, 6>& bounds) {
    for (size_t axis = 0; axis < 3; ++axis)
        if (!std::isfinite(bounds[axis]) || !std::isfinite(bounds[axis + 3]) ||
            bounds[axis] > bounds[axis + 3]) return false;
    return true;
}

inline bool storylandScriptQueryRadiusOverlap(const std::array<float, 6>& bounds,
    const StorylandNativePoint& origin, float radius) {
    if (!std::isfinite(radius) || radius < 0 || !std::isfinite(origin.x) ||
        !std::isfinite(origin.y) || !std::isfinite(origin.z)) return false;
    // LCS 0x178AA8/0x1793A8 and VCS 0x195C08 use origin +/- COL radius on all axes.
    // COL center, orientation, individual shapes and collision flags are unused.
    return origin.x + radius >= bounds[0] && origin.x - radius <= bounds[3] &&
        origin.y + radius >= bounds[1] && origin.y - radius <= bounds[4] &&
        origin.z + radius >= bounds[2] && origin.z - radius <= bounds[5];
}

inline bool storylandScriptQueryModelRadius(const StorylandMissionEnvironment& environment,
    const StorylandNativeWorld& native, int32_t model, float& radius,
    const StorylandNativeCollisionModel* liveCollision = nullptr) {
    if (model < 0) return false;
    if (environment.nativeModelRadius && environment.nativeModelRadius(model, radius) &&
        std::isfinite(radius) && radius >= 0) return true;
    const auto collision = native.collisionModels.find(uint32_t(model));
    if (collision != native.collisionModels.end()) liveCollision = &collision->second;
    if (!liveCollision || !std::isfinite(liveCollision->boundingRadius) ||
        liveCollision->boundingRadius < 0) return false;
    radius = liveCollision->boundingRadius;
    return true;
}

inline StorylandNativePoint storylandScriptQueryVehiclePosition(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& cars, int32_t handle,
    const StorylandMissionEnvironment::VehicleScriptState& vehicle) {
    const int32_t slot = vehicle.spawnedSlot;
    if (!vehicle.movedByScript && slot >= 0 && size_t(slot) < cars.size() &&
        size_t(slot) < runtime.vehicles.size() && runtime.vehicles[size_t(slot)].scriptHandle == handle)
        return cars[size_t(slot)].position;
    return {vehicle.x, vehicle.y, vehicle.z};
}

inline StorylandNativePoint storylandScriptQueryNativeVehicleOrigin(const StorylandRuntimeVehicle& vehicle,
    const StorylandNativeTrafficActor& actor,
    const StorylandMissionEnvironment::VehicleScriptState* scripted=nullptr) {
    const auto& definition=vehicle.nativeDefinition;
    const auto matrix=scripted?storylandMissionVehicleWorldMatrix(vehicle,actor,*scripted):
        storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
            {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
            vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
            actor.water),definition.previewRoot);
    return {matrix[12],matrix[13],matrix[14]};
}

inline bool storylandScriptMissionEntityInArea(const StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& native, const std::vector<StorylandNativeTrafficActor>& cars,
    const std::array<float, 6>& bounds) {
    if (!runtime.missionEnvironment || !storylandScriptQueryBoundsValid(bounds)) return false;
    const auto& environment = *runtime.missionEnvironment;
    const auto overlaps = [&](int32_t model, const StorylandNativePoint& origin,
        const StorylandNativeCollisionModel* collision = nullptr) {
        float radius = 0;
        return storylandScriptQueryModelRadius(environment, native, model, radius, collision) &&
            storylandScriptQueryRadiusOverlap(bounds, origin, radius);
    };
    for (const auto& entry : environment.missionVehicles) {
        const auto& vehicle = entry.second;
        if (vehicle.deleted || !vehicle.persistent) continue;
        const StorylandNativeCollisionModel* collision = nullptr;
        if (vehicle.spawnedSlot >= 0 && size_t(vehicle.spawnedSlot) < runtime.vehicles.size()) {
            const auto& live = runtime.vehicles[size_t(vehicle.spawnedSlot)];
            if (live.scriptHandle == entry.first) collision = live.nativeDefinition.collision.get();
        }
        // Acquired ambient vehicles become mission-owned without losing their origin metadata.
        if (overlaps(vehicle.model, storylandScriptQueryVehiclePosition(runtime, cars, entry.first, vehicle), collision))
            return true;
    }
    for (const auto& entry : environment.missionCharacters) {
        const auto& character = entry.second;
        if (character.creationSource == 2 && !character.removeFromWorld &&
            overlaps(character.model, {character.x, character.y, character.z})) return true;
    }
    for (const auto& object : environment.objects) {
        if (object.creationSource != 2) continue;
        const int32_t model = object.model < 0 && environment.resolveObjectModel ?
            environment.resolveObjectModel(object.model) : object.model;
        if (overlaps(model, {object.x, object.y, object.z})) return true;
    }
    return false;
}

inline bool storylandScriptAreaOccupied(const StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& native, const std::vector<StorylandNativeTrafficActor>& cars,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,
    const std::array<float, 6>& bounds, uint32_t mask,bool nativeVehicleOrigins=false) {
    if (!runtime.missionEnvironment || !storylandScriptQueryBoundsValid(bounds) || !(mask & 31u)) return false;
    const auto& environment = *runtime.missionEnvironment;
    const auto overlaps = [&](int32_t model, const StorylandNativePoint& origin,
        const StorylandNativeCollisionModel* collision = nullptr) {
        float radius = 0;
        return storylandScriptQueryModelRadius(environment, native, model, radius, collision) &&
            storylandScriptQueryRadiusOverlap(bounds, origin, radius);
    };

    if (mask & 17u) {
        for (const auto& instance : native.instances) {
            if(environment.lcsExtendedWorld.deletedNativeObjects.count(
                (uint64_t(instance.poolHeader)<<32)|instance.poolIndex))continue;
            const uint32_t category = instance.poolHeader == 0x2C ? 16u : 1u;
            if (!(mask & category)) continue;
            if (overlaps(int32_t(instance.modelId),
                {instance.matrix[12], instance.matrix[13], instance.matrix[14]})) return true;
        }
    }
    if (mask & 2u) {
        for (size_t slot = 0; slot < cars.size() && slot < runtime.vehicles.size(); ++slot) {
            const auto& vehicle = runtime.vehicles[slot];
            if (vehicle.nativeDefinition.modelId == UINT32_MAX) continue;
            if (environment.missionVehicles.count(vehicle.scriptHandle)) continue;
            const auto origin=nativeVehicleOrigins?storylandScriptQueryNativeVehicleOrigin(vehicle,cars[slot]):
                cars[slot].position;
            if (overlaps(int32_t(vehicle.nativeDefinition.modelId), origin,
                vehicle.nativeDefinition.collision.get())) return true;
        }
        for (const auto& entry : environment.missionVehicles) {
            const auto& vehicle = entry.second;
            if (vehicle.deleted) continue;
            const StorylandNativeCollisionModel* collision = nullptr;
            StorylandNativePoint origin=storylandScriptQueryVehiclePosition(runtime,cars,entry.first,vehicle);
            bool constructed=false;
            if (vehicle.spawnedSlot >= 0 && size_t(vehicle.spawnedSlot) < runtime.vehicles.size()) {
                const auto& live = runtime.vehicles[size_t(vehicle.spawnedSlot)];
                if (live.scriptHandle == entry.first) {
                    collision=live.nativeDefinition.collision.get();
                    constructed=size_t(vehicle.spawnedSlot)<cars.size();
                    if(nativeVehicleOrigins&&constructed)origin=storylandScriptQueryNativeVehicleOrigin(
                        live,cars[size_t(vehicle.spawnedSlot)],&vehicle);
                }
            }
            if(nativeVehicleOrigins&&!constructed) {
                const auto definition=std::find_if(runtime.modelDefinitions.begin(),runtime.modelDefinitions.end(),
                    [&](const auto& item){return item.modelId==uint32_t(vehicle.model);});
                if(definition!=runtime.modelDefinitions.end()) {
                    StorylandRuntimeVehicle pending;
                    pending.nativeDefinition=*definition;
                    StorylandNativeTrafficActor actor;
                    actor.position={vehicle.x,vehicle.y,vehicle.z};actor.yaw=vehicle.heading+90;
                    actor.water=definition->domain==StorylandVehicleDomain::Water;
                    origin=storylandScriptQueryNativeVehicleOrigin(pending,actor,&vehicle);
                    collision=definition->collision.get();
                }
            }
            if (overlaps(vehicle.model,origin,collision)) return true;
        }
    }
    if (mask & 4u) {
        if (environment.currentPlayerModel && overlaps(environment.currentPlayerModel(),
            {environment.x, environment.y, environment.z})) return true;
        for (size_t slot = 0; slot < pedestrians.size() && slot < runtime.pedestrians.size(); ++slot) {
            const auto& character = runtime.pedestrians[slot];
            if (character.scriptHandle >= 0 || !character.stats.valid) continue;
            if (overlaps(int32_t(character.stats.modelId), pedestrians[slot].position)) return true;
        }
        for (const auto& entry : environment.missionCharacters) {
            const auto& character = entry.second;
            if (!character.removeFromWorld &&
                overlaps(character.model, {character.x, character.y, character.z})) return true;
        }
    }
    if (mask & 8u) {
        for (const auto& object : environment.objects) {
            const int32_t model = object.model < 0 && environment.resolveObjectModel ?
                environment.resolveObjectModel(object.model) : object.model;
            if (overlaps(model, {object.x, object.y, object.z})) return true;
        }
    }
    return false;
}

inline void storylandBindScriptWorldQueries(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& native, const std::vector<StorylandNativeTrafficActor>& cars,
    const std::vector<StorylandNativeTrafficActor>& pedestrians) {
    if (!runtime.missionEnvironment) return;
    auto& environment = *runtime.missionEnvironment;
    environment.areaOccupied = [&runtime, &native, &cars, &pedestrians](const std::array<float, 6>& bounds, uint32_t mask) {
        return storylandScriptAreaOccupied(runtime, native, cars, pedestrians, bounds, mask);
    };
    environment.missionEntityInArea = [&runtime, &native, &cars](const std::array<float, 6>& bounds) {
        return storylandScriptMissionEntityInArea(runtime, native, cars, bounds);
    };
}

inline void storylandBindVcsScriptPopulationQuery(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& native, const std::vector<StorylandNativeTrafficActor>& cars,
    const std::vector<StorylandNativeTrafficActor>& pedestrians) {
    if (!runtime.missionEnvironment) return;
    runtime.missionEnvironment->areaOccupied = [&runtime, &native, &cars, &pedestrians](
        const std::array<float, 6>& bounds, uint32_t mask) {
        // VCS 0202 routes these four categories through 0x195880. Its solid
        // geometry flag has a separate 0x1908C0 path, outside this pool query.
        return storylandScriptAreaOccupied(runtime, native, cars, pedestrians, bounds, mask & 30u,true);
    };
}
