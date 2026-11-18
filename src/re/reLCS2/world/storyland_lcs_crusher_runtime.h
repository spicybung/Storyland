#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include <cstring>

inline bool storylandLcsCrusherVehicleGeometry(const StorylandMissionEnvironment& world,
    const StorylandPlayTestRuntime& runtime, const std::vector<uint8_t>& game, int32_t handle,
    std::array<float,3>& origin, std::vector<std::array<float,4>>& spheres,
    uint32_t& monetaryValue, int32_t& nativeType, std::string& error) {
    spheres.clear();
    const auto found = world.missionVehicles.find(handle);
    if (found == world.missionVehicles.end() || found->second.deleted || found->second.spawnedSlot < 0 ||
        size_t(found->second.spawnedSlot) >= runtime.vehicles.size()) {
        error = "Crusher target has no resident native vehicle.";
        return false;
    }
    const auto& vehicle = runtime.vehicles[size_t(found->second.spawnedSlot)];
    const auto& definition = vehicle.nativeDefinition;
    if (vehicle.scriptHandle != handle || !definition.collision || !definition.hasNativeHandling ||
        !definition.handlingPointer || uint64_t(definition.handlingPointer) + 0xDC > game.size()) {
        error = "Crusher target requires its original collision spheres and handling value.";
        return false;
    }
    std::array<float,16> matrix{};
    if (!world.vehicleWorldMatrix || !world.vehicleWorldMatrix(handle, matrix)) {
        error = "Crusher target has no native world transform.";
        return false;
    }
    origin = {matrix[12], matrix[13], matrix[14]};
    nativeType = definition.nativeType;
    const size_t valueOffset = size_t(definition.handlingPointer) + 0xD8;
    monetaryValue = uint32_t(game[valueOffset]) | uint32_t(game[valueOffset + 1]) << 8 |
        uint32_t(game[valueOffset + 2]) << 16 | uint32_t(game[valueOffset + 3]) << 24;
    for (const auto& sphere : definition.collision->spheres) {
        const auto point = storylandVehicleTransformPoint(matrix,
            {sphere.center.x, sphere.center.y, sphere.center.z});
        spheres.push_back({point[0], point[1], point[2], sphere.radius});
    }
    error.clear();
    return true;
}
