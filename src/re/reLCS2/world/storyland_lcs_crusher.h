#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/re/reLCS2/runtime/storyland_lcs_messages.h"
#include <limits>

inline float storylandLcsCrusherFloat(double value) {
    const double maximum = std::numeric_limits<float>::max();
    value = std::clamp(value, -maximum, maximum);
    float result = float(value);
    if ((value > 0 && double(result) > value) || (value < 0 && double(result) < value))
        result = std::nextafter(result, 0.0f);
    if (std::fpclassify(result) == FP_SUBNORMAL) return std::copysign(0.0f, result);
    return result;
}

inline int32_t storylandLcsCrusherReward(uint32_t monetaryValue, float health, uint8_t multiplier) {
    const float value = storylandLcsCrusherFloat(double(monetaryValue));
    const float condition = storylandLcsCrusherFloat(double(value) * health);
    const float scaled = storylandLcsCrusherFloat(double(condition) * 0.000002f);
    const float reward = std::min(125.0f, storylandLcsCrusherFloat(double(scaled) + 25.0));
    return int32_t(reward) * int32_t(multiplier);
}

inline std::array<float,16> storylandLcsCrusherLidMatrix(
    const StorylandMissionEnvironment::LcsGarageDoor& door, float position) {
    auto matrix = door.originalMatrix;
    const float angle = 6.2831853071795864769f - position;
    const float sine = std::sin(angle), cosine = std::cos(angle);
    matrix[0] = 1; matrix[1] = 0; matrix[2] = 0;
    matrix[4] = 0; matrix[5] = cosine; matrix[6] = sine;
    matrix[8] = 0; matrix[9] = -sine; matrix[10] = cosine;
    return matrix;
}

inline bool storylandAdvanceLcsCrusher(StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::Garage& garage,
    StorylandMissionEnvironment::LcsGarageDoorState& state, std::string& error) {
    auto& extension = world.lcsExtendedWorld;
    auto target = extension.crusherTargets.find(garage.handle);
    const auto targetVehicle = [&]() -> StorylandMissionEnvironment::VehicleScriptState* {
        if (target == extension.crusherTargets.end()) return nullptr;
        const auto found = world.missionVehicles.find(target->second);
        return found == world.missionVehicles.end() || found->second.deleted ? nullptr : &found->second;
    };
    const float step = std::isfinite(world.nativeScriptTimeStep) ? world.nativeScriptTimeStep : 0;
    const float speed = storylandLcsCrusherFloat(double(step) * 0.005f);
    if (state.state == 1) {
        if (!extension.crusherVehicleGeometry || !extension.crusherVehiclePoolCapacity ||
            !world.findScriptVehiclesInArea) {
            error = "The LCS crusher requires live vehicle collision and pool bindings.";
            return false;
        }
        StorylandLcsGarageCameraGeometry geometry;
        if (!storylandLcsGarageCameraGeometryFor(garage, geometry)) {
            error = "The native crusher garage has invalid bounds.";
            return false;
        }
        auto vehicles = world.findScriptVehiclesInArea(
            {geometry.minimumX, geometry.minimumY, geometry.maximumX, geometry.maximumY}, -1,
            StorylandMissionEnvironment::VehicleAreaRoad);
        std::sort(vehicles.begin(), vehicles.end(), [&](int32_t first, int32_t second) {
            return world.missionVehicles.at(first).spawnedSlot < world.missionVehicles.at(second).spawnedSlot;
        });
        const size_t capacity = extension.crusherVehiclePoolCapacity();
        const size_t frame = world.nativeFrameCounter % 8;
        const size_t begin = capacity * frame / 8;
        const size_t end = capacity * (frame + 1) / 8;
        for (int32_t handle : vehicles) {
            const auto found = world.missionVehicles.find(handle);
            if (found == world.missionVehicles.end() || found->second.deleted || found->second.spawnedSlot < 0)
                continue;
            const size_t slot = size_t(found->second.spawnedSlot);
            if (slot < begin || slot >= end) continue;
            std::array<float,3> origin{};
            std::vector<std::array<float,4>> nativeSpheres;
            uint32_t monetaryValue = 0;
            int32_t nativeType = -1;
            if (!extension.crusherVehicleGeometry(handle, origin, nativeSpheres, monetaryValue, nativeType, error))
                return false;
            if (nativeType != 0 && nativeType != 5) continue;
            std::vector<StorylandLcsGarageCameraSphere> spheres;
            for (const auto& sphere : nativeSpheres)
                spheres.push_back({{sphere[0], sphere[1], sphere[2]}, sphere[3]});
            if (!storylandLcsGarageCameraEntityInside(garage, origin, spheres, 0)) continue;
            extension.crusherTargets[garage.handle] = handle;
            state.state = 7;
            state.actionTime = world.gameMilliseconds;
        }
    } else if (state.state == 7) {
        if (!targetVehicle()) {
            extension.crusherTargets.erase(garage.handle);
            state.state = 3;
        } else if (uint32_t(world.gameMilliseconds - state.actionTime) > 3000) state.state = 2;
    } else if (state.state == 2) {
        auto* vehicle = targetVehicle();
        if (!vehicle) {
            extension.crusherTargets.erase(garage.handle);
            state.state = 3;
        } else {
            state.position = std::max(0.0f, storylandLcsCrusherFloat(double(state.position) - speed));
            if (state.position < 1.2566370614359172954f) {
                vehicle->collisionEnabled = false;
                vehicle->positionFrozen = true;
                vehicle->velocity = {};
                vehicle->speed = 0;
                vehicle->velocityChangedByScript = true;
            } else {
                const float factor = std::pow(0.8f, step);
                for (float& component : vehicle->velocity) component = storylandLcsCrusherFloat(double(component) * factor);
                vehicle->speed = storylandLcsCrusherFloat(double(vehicle->speed) * factor);
                vehicle->velocityChangedByScript = true;
            }
            if (state.position == 0) {
                std::array<float,3> origin{};
                std::vector<std::array<float,4>> spheres;
                uint32_t monetaryValue = 0;
                int32_t nativeType = -1;
                if (!extension.crusherVehicleGeometry || !extension.crusherVehicleGeometry(target->second,
                        origin, spheres, monetaryValue, nativeType, error)) return false;
                const int32_t reward = storylandLcsCrusherReward(monetaryValue, vehicle->health,
                    extension.crusherRewardMultiplier);
                world.money = int32_t(std::clamp<int64_t>(int64_t(world.money) + reward, INT32_MIN, INT32_MAX));
                extension.lastCrushedVehicle = target->second;
                StorylandMissionEnvironment::LcsMessage message;
                message.key = "CRUSHED";
                message.numbers = {reward};
                message.durationMilliseconds = 5000;
                storylandQueueLcsMessage(world, std::move(message));
                const auto statistic = world.statistics.find("CarsCrushed");
                const double previous = statistic == world.statistics.end() ? 0 : statistic->second;
                world.statistics["CarsCrushed"] = std::isfinite(previous) ?
                    std::min(double(INT32_MAX), previous + 1) : 1;
                std::vector<int32_t> occupants;
                for (const auto& character : world.missionCharacters)
                    if (character.second.vehicle == target->second) occupants.push_back(character.first);
                for (int32_t character : occupants) storylandDeleteMissionCharacter(world, character);
                if (world.inVehicle && world.occupiedMissionVehicle == target->second && extension.crusherPlayerDamage)
                    extension.crusherPlayerDamage();
                vehicle->deleted = true;
                vehicle->alive = false;
                vehicle->health = 0;
                vehicle->healthChangedByScript = true;
                extension.crusherTargets.erase(garage.handle);
                state.state = 6;
                state.actionTime = world.gameMilliseconds + 3000;
            }
        }
    } else if (state.state == 6) {
        if (int32_t(world.gameMilliseconds - state.actionTime) > 0) state.state = 3;
    } else if (state.state == 3) {
        state.position = std::min(1.5707963267948966192f,
            storylandLcsCrusherFloat(double(state.position) + speed));
        if (state.position == 1.5707963267948966192f) {
            state.state = 1;
        }
    }
    if ((world.nativeFrameCounter & 31) == 23 && extension.crusherPlayerDamage) {
        StorylandLcsGarageCameraGeometry geometry;
        if (storylandLcsGarageCameraGeometryFor(garage, geometry) &&
            world.x > geometry.minimumX && world.x < geometry.maximumX &&
            world.y > geometry.minimumY && world.y < geometry.maximumY &&
            world.z > geometry.minimumZ - 1 && world.z < geometry.maximumZ) {
            extension.crusherPlayerDamage();
        }
    }
    error.clear();
    return true;
}
