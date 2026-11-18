#pragma once

#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_service_vehicle.h"
#include "src/storyland/storyland_service_population.h"
#include "src/re/reVCS/services/storyland_vcs_firefighter_response.h"
#include <cmath>
#include <vector>

// Bounded host approximation: an existing stationary ambient truck may hand its
// already represented driver to the on-foot responder. This does not dispatch a truck.
template<class PlaceDriver>
size_t storylandHandoffVcsFiretruckDrivers(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& pedestrians,
    const std::vector<StorylandNativeTrafficActor>& cars, PlaceDriver placeDriver) {
    if (!runtime.crime.vcs || !runtime.missionEnvironment || runtime.paused || runtime.pauseMenu)
        return 0;
    size_t handedOff = 0;
    const auto& environment = *runtime.missionEnvironment;
    for (size_t slot = 0; slot < cars.size() && slot < runtime.vehicles.size(); ++slot) {
        auto& vehicle = runtime.vehicles[slot];
        if ((vehicle.serviceAutopilot && !vehicle.serviceAutopilot->arrived) || vehicle.nativeDefinition.modelId != 194 ||
            storylandVehicleServiceRole(runtime, vehicle) != StorylandServiceRole::Firefighter ||
            !vehicle.hasDriver || vehicle.missionDriver || vehicle.scriptHandle >= 0 ||
            vehicle.generator >= 0 || vehicle.savedGarage >= 0 || vehicle.coopControlled ||
            vehicle.coopSeatReserved || vehicle.craneCarried || vehicle.positionFrozen ||
            vehicle.wrecked || !vehicle.visible || !vehicle.clumpVisible || vehicle.health <= 0 ||
            int(slot) == runtime.occupiedVehicle || int(slot) == runtime.transitionVehicle ||
            !std::isfinite(vehicle.speed) || !std::isfinite(vehicle.lateralSpeed) ||
            !std::isfinite(vehicle.verticalSpeed) || std::fabs(vehicle.speed) > 0.2f ||
            std::fabs(vehicle.lateralSpeed) > 0.2f || std::fabs(vehicle.verticalSpeed) > 0.2f)
            continue;
        const size_t model = vehicle.driverModel;
        if (model >= runtime.pedestrianModelStats.size() || model >= runtime.pedestrianModelRoles.size())
            continue;
        const auto& stats = runtime.pedestrianModelStats[model];
        if (!stats.valid || stats.pedType != 17 || stats.modelId != 13 ||
            runtime.pedestrianModelRoles[model] != StorylandServiceRole::Firefighter)
            continue;
        const auto& location = cars[slot].position;
        if (!std::isfinite(location.x) || !std::isfinite(location.y) || !std::isfinite(location.z) ||
            !std::isfinite(cars[slot].yaw) || !std::isfinite(vehicle.halfWidth) || vehicle.halfWidth <= 0)
            continue;
        // Native exit has no explicit distance gate. Until the complete stop
        // task is modelled, let dispatched crews exit from a safe stand-off;
        // retain the narrower admission for ordinary ambient trucks.
        const float incidentRange=vehicle.serviceAutopilot?11.0f:5.0f;
        const int32_t fireHandle = storylandSelectVcsAmbientFire(environment, location);
        const auto fire = fireHandle >= 0 ? environment.scriptFires.find(fireHandle) : environment.scriptFires.end();
        StorylandNativePoint firePosition{};
        bool incident = fire != environment.scriptFires.end() &&
            storylandVcsFireTargetPosition(environment,fire->second,firePosition) &&
            std::hypot(firePosition.x-location.x,firePosition.y-location.y)<incidentRange;
        for (size_t other = 0; !incident && other < cars.size() && other < runtime.vehicles.size(); ++other) {
            const auto& burning = runtime.vehicles[other];
            if (other == slot || !burning.burning || burning.scriptHandle >= 0 || burning.missionDriver ||
                burning.generator >= 0 || burning.savedGarage >= 0 || burning.coopControlled ||
                burning.craneCarried || burning.wrecked || !burning.canBeDamaged || burning.health <= 0 ||
                int(other) == runtime.occupiedVehicle || int(other) == runtime.transitionVehicle) continue;
            const auto protectedEffect = [&](const StorylandRuntimeEffect& effect) {
                return effect.vehicle == int(other) && (effect.permanent || effect.scriptFireHandle >= 0);
            };
            if (std::any_of(runtime.effects.begin(), runtime.effects.end(), protectedEffect) ||
                std::any_of(runtime.worldEffects.begin(), runtime.worldEffects.end(), protectedEffect)) continue;
            incident = std::hypot(cars[other].position.x - location.x,
                cars[other].position.y - location.y) < incidentRange;
        }
        if (!incident) continue;
        // Only append a previously unused ambient slot; never recycle a visible actor.
        if (pedestrians.size() + environment.missionCharacters.size() >= runtime.pedestrians.size()) continue;
        const size_t pedSlot = pedestrians.size();
        const auto& unused = runtime.pedestrians[pedSlot];
        if (unused.scriptHandle >= 0 || unused.turfWarActorId != 0) continue;
        StorylandNativePoint position{};
        if (!placeDriver(slot, pedSlot, position)) continue;
        StorylandRuntimePed driver{};
        driver.model = model;
        driver.stats = stats;
        driver.serviceRole = StorylandServiceRole::Firefighter;
        StorylandNativeTrafficActor actor{};
        actor.node = UINT32_MAX;
        actor.nextNode = UINT32_MAX;
        actor.position = position;
        actor.yaw = cars[slot].yaw;
        pedestrians.push_back(actor);
        runtime.pedestrians[pedSlot] = driver;
        vehicle.hasDriver = false;
        vehicle.parked = true;
        vehicle.speed = 0;
        ++handedOff;
    }
    return handedOff;
}
