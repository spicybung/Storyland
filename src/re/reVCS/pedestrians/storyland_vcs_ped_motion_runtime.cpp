#include "src/re/reVCS/pedestrians/storyland_vcs_ped_motion_runtime.h"
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_ped_movement.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_play_test_weapons.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_ped_motion.h"
#include "src/re/reVCS/vehicles/storyland_vcs_jetski_knockoff.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include <algorithm>
#include <cmath>

unsigned storylandEjectVcsMissionBikeRiders(StorylandPlayTestRuntime& runtime,
    const StorylandNativeTrafficActor& actor, StorylandRuntimeVehicle& vehicle,
    const StorylandBikeFallDecision& decision, bool forced) {
    if (!runtime.crime.vcs || !runtime.missionEnvironment || !decision.eject ||
        !vehicle.nativeDefinition.twoWheels() || vehicle.scriptHandle < 0 ||
        !storylandVcsBikeFallAnimation(decision.animation)) return 0;
    auto& environment = *runtime.missionEnvironment;
    const auto scripted = environment.missionVehicles.find(vehicle.scriptHandle);
    if (scripted == environment.missionVehicles.end() || scripted->second.deleted ||
        !environment.animationDuration || !environment.animationAssociationFlags ||
        !environment.characterWorldMatrix || !environment.characterOriginOffset) return 0;
    uint16_t fallFlags = 0, getUpFlags = 0;
    if (!environment.animationAssociationFlags(0, decision.animation, fallFlags) ||
        (fallFlags & 2u) != 0) return 0;
    const int32_t getUpAnimation = storylandVcsPedGetUpAnimation(fallFlags);
    if (!environment.animationAssociationFlags(0, getUpAnimation, getUpFlags) ||
        (getUpFlags & 2u) != 0) return 0;
    const float fallDuration = environment.animationDuration(0, decision.animation);
    const float getUpDuration = environment.animationDuration(0, getUpAnimation);
    if (!std::isfinite(fallDuration) || fallDuration <= 0 ||
        !std::isfinite(getUpDuration) || getUpDuration <= 0) return 0;
    const auto& definition = vehicle.nativeDefinition;
    if (!definition.hasNativeDummies) return 0;
    const auto frame = storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
        {actor.position.x, actor.position.y, actor.position.z}, actor.yaw,
        definition.previewMinimumZ, vehicle.suspension, vehicle.bodyRoll - vehicle.roadRoll,
        vehicle.bodyPitch + vehicle.roadPitch), definition.previewRoot);
    constexpr float radians = 0.01745329251994329577f;
    const float angle = actor.yaw * radians, cosine = std::cos(angle), sine = std::sin(angle);
    const std::array<float,3> inherited{
        cosine * vehicle.speed - sine * vehicle.lateralSpeed,
        sine * vehicle.speed + cosine * vehicle.lateralSpeed, vehicle.verticalSpeed};
    const float forceScale = StorylandNativePhysicsRate / StorylandNativePedMass;
    unsigned count = 0;
    for (auto& entry : environment.missionCharacters) {
        const int32_t handle = entry.first;
        auto& character = entry.second;
        if(decision.nativeWaterFall&&vehicle.watertight&&character.type!=0)continue;
        if (character.vehicle != vehicle.scriptHandle || character.attached ||
            character.removeFromWorld || character.networkControlled ||
            environment.vcsPedFalls.count(handle)) continue;
        const int32_t oldSeat = character.passengerSeat;
        const bool driver = scripted->second.driver == handle;
        if ((oldSeat < 0) != driver) continue;
        const auto transition = environment.characterVehicleTasks.find(handle);
        if (transition != environment.characterVehicleTasks.end() && transition->second.animating) continue;
        std::array<float,16> entityFrame{};
        float originOffset = 0;
        if (!environment.characterWorldMatrix(handle, entityFrame) ||
            !environment.characterOriginOffset(handle, originOffset) ||
            !std::isfinite(originOffset) ||
            !std::all_of(entityFrame.begin(), entityFrame.end(), [](float value) { return std::isfinite(value); })) continue;
        const float feetZ = entityFrame[14] - originOffset;
        if (!std::isfinite(feetZ)) continue;
        StorylandVcsPedFallState fall;
        fall.sourceVehicle = vehicle.scriptHandle;
        fall.fallAnimation = decision.animation;
        fall.getUpAnimation = getUpAnimation;
        fall.fallDuration = fallDuration;
        fall.getUpDuration = getUpDuration;
        fall.time = decision.animation == 121 ? std::min(.4f, fallDuration) : 0;
        if (!character.nativePedRandomSeedInitialized) {
            character.nativePedRandomSeed = uint8_t(environment.nextNativeRandom());
            character.nativePedRandomSeedInitialized = true;
        }
        fall.getUpAfter = storylandVcsPedGetUpDeadline(environment.gameMilliseconds,
            fallDuration, character.nativePedRandomSeed, environment.nativeFrameCounter);
        if(decision.nativeWaterFall) {
            fall.velocity={inherited[0]*.2f,inherited[1]*.2f,0};
        } else if (decision.nativeAngularFall) {
            storyland_vcs_jetski_knockoff::Input input;
            input.pedestrianMass = StorylandNativePedMass;
            input.vehicleMass = vehicle.mass;
            input.nativeNormalImpulse = vehicle.vcsJetSkiContactImpulse;
            input.contactNormal = vehicle.vcsJetSkiContactNormal;
            for (size_t axis = 0; axis < input.nativeVehicleVelocity.size(); ++axis)
                input.nativeVehicleVelocity[axis] = inherited[axis] / StorylandNativePhysicsRate;
            if (vehicle.nativeDefinition.hasNativeSuspension && vehicle.grounded)
                input.nativeVehicleVelocity[2] = vehicle.suspensionVelocity / StorylandNativePhysicsRate;
            input.nativeRandom = float(environment.nextNativeRandom()) / 2147483648.0f;
            storyland_vcs_jetski_knockoff::Output impulse;
            if (!storyland_vcs_jetski_knockoff::force(input, impulse)) {
                runtime.vehiclePhysicsError = "Native VCS bike angular fall: cause47 contact force is invalid.";
                return count;
            }
            for (size_t axis = 0; axis < fall.velocity.size(); ++axis)
                fall.velocity[axis] = impulse.nativeVelocity[axis] * StorylandNativePhysicsRate;
        } else {
            for (size_t axis = 0; axis < fall.velocity.size(); ++axis)
                fall.velocity[axis] = inherited[axis] * decision.inheritVelocity + forceScale *
                    (frame[axis] * decision.sideForce + frame[4 + axis] * decision.forwardForce +
                        frame[8 + axis] * decision.upForce);
            if (decision.inheritVelocity == 0) fall.velocity[2] += .1f * StorylandNativePhysicsRate;
        }
        if (!storylandValidVcsPedFall(fall)) continue;
        character.x = entityFrame[12]; character.y = entityFrame[13]; character.z = feetZ;
        character.heading = actor.yaw - 90.0f;
        character.vehicle = -1; character.passengerSeat = -1;
        character.animationId = -1;
        character.walkingToTarget = false;
        character.movementSpeed = 0;
        character.seekRoute.clear(); character.seekRouteCursor = 0;
        environment.characterVehicleTasks.erase(handle);
        environment.scriptedCharacterAnimations.erase(handle);
        environment.characterWanderTasks.erase(handle);
        environment.characterTargets.erase(handle);
        storylandClearCrouch(character.crouch, environment.gameMilliseconds);
        if (driver) {
            scripted->second.driver = -1;
            vehicle.hasDriver = false; vehicle.missionDriver = false; vehicle.parked = false;
        }
        const auto objective = storylandVcsEjectionObjective(false, forced,
            character.leader >= 0, character.dontReenterAfterEjection, driver);
        if (objective != StorylandVcsEjectionObjective::Preserve) {
            character.objective = objective == StorylandVcsEjectionObjective::EnterDriver ? 0x00EA :
                objective == StorylandVcsEjectionObjective::EnterPassenger ? 0x00E9 : 0;
            character.objectiveTarget = objective == StorylandVcsEjectionObjective::None ? -1 : vehicle.scriptHandle;
            character.objectiveCompleted = false;
            if (objective == StorylandVcsEjectionObjective::EnterPassenger) character.passengerSeat = oldSeat;
        }
        auto& combat = runtime.missionCombat[handle];
        combat.contact.clear(); combat.melee = {}; combat.crouchPose = {};
        combat.verticalSpeed = fall.velocity[2]; combat.grounded = false;
        combat.gravityPositionValid = false; combat.airborneTime = combat.landingTime = 0;
        environment.vcsPedFalls.emplace(handle, fall);
        if (decision.damage > 0) {
            StorylandMissionCharacterDamage damage;
            damage.handle = handle; damage.model = character.model; damage.amount = decision.damage;
            damage.kind = 8; damage.weapon = 42;
            damage.sourceKind = uint8_t(StorylandMissionEnvironment::DamageSourceKind::Vehicle);
            damage.sourceHandle = vehicle.scriptHandle;
            storylandApplyMissionCharacterDamage(runtime, damage, false);
        }
        ++count;
    }
    return count;
}

void storylandAdvanceVcsMissionPedFalls(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, float seconds) {
    if (!world.paths.vcs || !runtime.missionEnvironment || !std::isfinite(seconds) || seconds <= 0) return;
    auto& environment = *runtime.missionEnvironment;
    if (environment.cutscene.playing) return;
    const float step = std::min(seconds, .1f);
    for (auto entry = environment.vcsPedFalls.begin(); entry != environment.vcsPedFalls.end();) {
        const auto found = environment.missionCharacters.find(entry->first);
        if (found == environment.missionCharacters.end() || found->second.removeFromWorld ||
            found->second.vehicle >= 0 || found->second.attached) {
            entry = environment.vcsPedFalls.erase(entry); continue;
        }
        auto& character = found->second;
        auto& fall = entry->second;
        auto& combat = runtime.missionCombat[entry->first];
        if (character.networkControlled) { ++entry; continue; }
        if (combat.water.active) {
            entry = environment.vcsPedFalls.erase(entry); continue;
        }
        const bool alive = character.alive && character.health > 0;
        // A recovery association may already be active when the pedestrian
        // dies. Its root motion and get-up animation belong only to living
        // pedestrians; hand the body to the normal death pose immediately.
        if (!alive && fall.recovering) {
            character.movementSpeed = 0;
            character.walkingToTarget = false;
            entry = environment.vcsPedFalls.erase(entry);
            continue;
        }
        if (character.positionFrozen) { ++entry; continue; }
        if(fall.fallAnimation>=59&&fall.fallAnimation<=62&&!alive) {
            character.movementSpeed=0;
            entry=environment.vcsPedFalls.erase(entry);continue;
        }
        if(fall.fallAnimation>=59&&fall.fallAnimation<=62&&!fall.recovering) {
            fall.time=std::min(fall.fallDuration,fall.time+step);
            character.movementSpeed=0;
            if(fall.time>=fall.fallDuration) {fall.recovering=true;fall.time=0;fall.velocity={};}
            ++entry;continue;
        }
        if (storylandVcsPedCanGetUp(fall, environment.gameMilliseconds, alive)) {
            fall.recovering = true; fall.time = 0; fall.velocity = {};
        }
        const float previousTime = fall.time;
        fall.time = std::min(fall.duration(), fall.time + step);
        const StorylandNativePoint before{character.x, character.y, character.z};
        StorylandNativePoint desired{before.x + fall.velocity[0] * step,
            before.y + fall.velocity[1] * step, before.z};
        if (fall.recovering && runtime.reactionBindings.rootDelta) {
            const auto root = runtime.reactionBindings.rootDelta(fall.animation(), 0, previousTime, fall.time);
            if (std::isfinite(root.x) && std::isfinite(root.y)) {
                constexpr float radians = 0.01745329251994329577f;
                const float angle = (character.heading + 90.0f) * radians;
                desired.x += std::cos(angle) * root.x + std::sin(angle) * root.y;
                desired.y += std::sin(angle) * root.x - std::cos(angle) * root.y;
            }
        }
        const auto vertical = storylandPedVerticalStep(fall.velocity[2], runtime.gravity, step);
        desired.z += vertical.displacement;
        uint8_t material = 0;
        const StorylandNativePoint top{desired.x, desired.y, before.z + .08f};
        const StorylandNativePoint bottom{desired.x, desired.y,
            std::min(before.z - 3.0f, desired.z - .15f)};
        const float fraction = storylandTraceNativeGround(world, top, bottom, material);
        bool hasFloor = fraction < 1;
        float floor = top.z + (bottom.z - top.z) * fraction;
        int32_t ignoredVehicle = -1;
        if (fall.sourceVehicle >= 0) {
            const auto source = environment.missionVehicles.find(fall.sourceVehicle);
            if (source == environment.missionVehicles.end() || source->second.deleted) fall.sourceVehicle = -1;
            else ignoredVehicle = source->second.spawnedSlot;
        }
        for (size_t index = 0; index < runtime.movementCars.size(); ++index) {
            const auto& car = runtime.movementCars[index];
            if (int32_t(index) == ignoredVehicle || !car.collisionEnabled) continue;
            const float roof = car.position.z + car.height;
            if (before.z < roof - .15f || roof < bottom.z || roof > top.z) continue;
            constexpr float radians = 0.01745329251994329577f;
            const float cosine = std::cos(car.yaw * radians), sine = std::sin(car.yaw * radians);
            const float dx = desired.x - car.position.x, dy = desired.y - car.position.y;
            if (std::fabs(dx * cosine + dy * sine) > car.halfLength ||
                std::fabs(-dx * sine + dy * cosine) > car.halfWidth) continue;
            if (!hasFloor || roof > floor) { floor = roof; hasFloor = true; }
        }
        const bool landing = hasFloor && vertical.velocity <= 0 && desired.z <= floor + .005f;
        if (landing) desired.z = floor;
        const auto moved = storylandMovePlayTestCharacter(world, runtime, before, desired,
            INT32_MAX, false, false, false, ignoredVehicle);
        if (moved.clear) {
            character.x = moved.position.x; character.y = moved.position.y; character.z = moved.position.z;
        }
        fall.grounded = landing && std::fabs(character.z - floor) < .05f;
        fall.velocity[2] = fall.grounded ? 0 : vertical.velocity;
        if (moved.blocked && desired.z > before.z) fall.velocity[2] = std::min(0.0f, fall.velocity[2]);
        const float travelled = std::hypot(character.x - before.x, character.y - before.y);
        if (moved.blocked && travelled < .15f * std::hypot(desired.x - before.x, desired.y - before.y))
            fall.velocity[0] = fall.velocity[1] = 0;
        character.movementSpeed = travelled / step;
        combat.verticalSpeed = fall.velocity[2]; combat.grounded = fall.grounded;
        combat.gravityPosition = {character.x, character.y, character.z};
        combat.gravityPositionValid = true;
        if (fall.grounded) combat.airborneTime = 0;
        else combat.airborneTime += step;
        combat.landingTime = 0;
        if ((fall.recovering && fall.time >= fall.getUpDuration) ||
            (!alive && fall.grounded && fall.time >= fall.fallDuration)) {
            character.movementSpeed = 0;
            entry = environment.vcsPedFalls.erase(entry);
        } else ++entry;
    }
}
