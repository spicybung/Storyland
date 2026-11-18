#include "src/platform/linux/storyland_linux_session.h"
#include "src/storyland/storyland_coop_game.h"
#include "src/storyland/storyland_intro_mission.h"
#include "src/storyland/storyland_play_test_weapons.h"
#include "src/storyland/storyland_vehicle_occupants.h"

StorylandLinuxSession::StorylandLinuxSession()
    : network(std::make_unique<StorylandMultiplayer>()) {}

StorylandLinuxSession::~StorylandLinuxSession() = default;

StorylandMultiplayer& StorylandLinuxSession::coopNetwork() { return *network; }

StorylandNetworkPlayer StorylandLinuxSession::coopDisplay() const { return network->display(coopClock); }

const std::string& StorylandLinuxSession::coopStatus() const {
    return coop && !coop->status.empty() ? coop->status : network->status;
}

void StorylandLinuxSession::disconnectCoop() {
    coop.reset();
    network->disconnect();
    remoteVehicleAsset.reset();
    remoteVehicleModel = -1;
    coopRestartPending = false;
}

void StorylandLinuxSession::resetCoopAfterRestart() {
    if (!coop) return;
    lastCoopAttack = -1;
    coop->restartWorld();
}

bool StorylandLinuxSession::requestCutsceneSkip() {
    if (!environment.cutscene.playing) return false;
    if (coop && network->connected()) return coop->requestCutsceneSkip();
    cutscene.skip(environment);
    return true;
}

const StorylandPlayTestAsset* StorylandLinuxSession::coopVehicleAsset(int32_t model) {
    if (model < 0) return nullptr;
    const auto local = std::find_if(vehicleAssets.begin(), vehicleAssets.end(), [&](const auto& asset) {
        return asset.vehicleDefinition.modelId == uint32_t(model);
    });
    if (local != vehicleAssets.end()) return &*local;
    if (remoteVehicleModel != model) {
        remoteVehicleModel = model;
        remoteVehicleAsset.reset();
        auto asset = std::make_unique<StorylandPlayTestAsset>();
        std::string error;
        if (storylandLoadPlayTestVehicle(archive, uint32_t(model), *asset, error)) {
            playerAnimation.configureVehicleAnimations(asset->vehicleDefinition);
            remoteVehicleAsset = std::move(asset);
        }
    }
    return remoteVehicleAsset.get();
}

StorylandNetworkPlayer StorylandLinuxSession::coopPlayerSnapshot() const {
    StorylandNetworkPlayer local;
    local.x = playerPosition.x;
    local.y = playerPosition.y;
    local.z = playerPosition.z;
    local.heading = playerHeading;
    local.animationTime = animationTime;
    local.motion = uint32_t(playerMotion);
    local.motionTime = std::max(0.f, animationTime - playerBlend.actionStartedAt);
    local.life = runtime.coopLife;
    local.maximumHealth = runtime.maximumHealth();
    local.maximumArmour = runtime.maximumArmour();
    uint32_t playerModel = 0;
    if (archive.findModelIdByName(storylandServiceModelStem(player.name), playerModel)) local.model = int32_t(playerModel);
    local.area = environment.area;
    local.gameHour = runtime.clockHour;
    local.gameMilliseconds = environment.gameMilliseconds;
    local.weather = environment.forcedWeather;
    local.elapsed = runtime.elapsed;
    local.shotSerial = runtime.playerShotSerial;
    if (runtime.equippedWeapon < runtime.weapons.size())
        local.weapon = runtime.weapons[runtime.equippedWeapon].nativeId;
    local.phase = playerBlend.locomotionPhase;
    local.moveForward = playerBlend.movementForward;
    local.moveRight = playerBlend.movementRight;
    local.actionTime = std::max(0.f, animationTime - playerBlend.upperStartedAt);
    if (runtime.freeAimActive || runtime.manualAimActive || storylandHasPlayTestTarget(runtime)) local.actionFlags |= 1;
    if (playerBlend.weaponReloading) local.actionFlags |= 2;
    if (playerBlend.upperActive) local.actionFlags |= 4;
    if (runtime.playerTorsoValid) {
        local.actionFlags |= 32;
        local.torsoX = runtime.playerTorso.x;
        local.torsoY = runtime.playerTorso.y;
        local.torsoZ = runtime.playerTorso.z;
    }
    const auto aim = runtime.freeAimActive ? runtime.freeAimTarget :
        runtime.manualAimActive ? runtime.manualAimTarget : runtime.targetPosition;
    local.aimX = aim.x;
    local.aimY = aim.y;
    local.aimZ = aim.z;
    const auto association = environment.scriptedCharacterAnimations.find(vcs() ? 0 : 1);
    if (association != environment.scriptedCharacterAnimations.end() && association->second.weight > 0) {
        local.animation = association->second.animation;
        local.animationGroup = association->second.group;
        local.scriptTime = std::max(0.f, association->second.time);
    }
    if (runtime.vehicleExitPose.active) {
        local.animation = runtime.vehicleExitPose.animation;
        local.animationGroup = runtime.vehicleExitPose.group;
        local.scriptTime = runtime.vehicleExitPose.elapsed;
    }

    const bool transitioning = runtime.vehicleAction != StorylandVehicleAction::None;
    const int slot = transitioning ? runtime.transitionVehicle : runtime.occupiedVehicle;
    if (slot >= 0 && size_t(slot) < vehicles.size() && size_t(slot) < runtime.vehicles.size()) {
        const auto& car = runtime.vehicles[size_t(slot)];
        const auto& pose = vehicles[size_t(slot)];
        local.vehicleModel = int32_t(car.nativeDefinition.modelId);
        local.vehicleIdentity = car.coopIdentity;
        local.vehicleHandle = car.scriptHandle;
        local.vehiclePhase = uint32_t(runtime.vehiclePhase.phase);
        local.seat = runtime.playerVehicleSeat;
        local.vehicleAction = uint32_t(runtime.vehicleAction);
        local.vehicleFraction = std::clamp(runtime.vehicleActionTime /
            std::max(.01f, runtime.vehicleActionDuration), 0.f, 1.f);
        local.carX = pose.position.x;
        local.carY = pose.position.y;
        local.carZ = pose.position.z;
        local.carHeading = pose.yaw;
        local.wheel = car.wheelRotation;
        local.rearWheel = car.rearWheelRotation;
        local.steering = car.steeringAngle;
        local.suspension = car.suspension;
        local.roll = car.bodyRoll - car.roadRoll;
        local.pitch = car.bodyPitch + car.roadPitch;
        StorylandVehiclePose renderedBike;
        if (storylandVehicleBikeRenderPose(car.nativeDefinition,
            {pose.position.x, pose.position.y, pose.position.z}, pose.yaw, car.suspension,
            local.roll, local.pitch, car.bikeVisualLean, renderedBike,
            car.craneCarried ? &car.craneFrame : nullptr)) {
            local.carX = renderedBike.position[0];
            local.carY = renderedBike.position[1];
            local.carZ = renderedBike.position[2];
            local.carHeading = renderedBike.heading;
            local.roll = renderedBike.roll;
            local.pitch = renderedBike.pitch;
        }
        local.speed = car.speed;
        local.carHealth = car.health;
        local.primaryColor = car.primaryColor;
        local.secondaryColor = car.secondaryColor;
        local.carFlags = (pose.water ? 1u : 0u) | (car.headlightsOn ? 2u : 0u) |
            (car.braking ? 4u : 0u) | (car.siren ? 8u : 0u) | (car.burning ? 16u : 0u) |
            (car.wrecked ? 32u : 0u) | (car.visible ? 64u : 0u);
        local.wheelPosition0 = car.wheelPositions[0];
        local.wheelPosition1 = car.wheelPositions[1];
        local.wheelPosition2 = car.wheelPositions[2];
        local.wheelPosition3 = car.wheelPositions[3];
        local.door0 = car.doorOpenRatios[0];
        local.door1 = car.doorOpenRatios[1];
        local.door2 = car.doorOpenRatios[2];
        local.door3 = car.doorOpenRatios[3];
        if (!transitioning) {
            local.actionFlags |= 8 | 32;
            local.radioStation = runtime.radioStationStem.empty() ? "off" : runtime.radioStationStem;
            const auto& definition = car.nativeDefinition;
            const auto frame = storylandVehicleOccupantFrame(definition,
                {pose.position.x, pose.position.y, pose.position.z}, pose.yaw, definition.previewMinimumZ,
                car.suspension, car.bodyRoll - car.roadRoll, car.bodyPitch + car.roadPitch,
                unsigned(local.seat + 1), pose.water, car.craneCarried ? &car.craneFrame : nullptr,
                storylandVcsJetSkiNativeAngularZ(car), car.bikeVisualLean);
            local.torsoX = frame[12];
            local.torsoY = frame[13];
            local.torsoZ = frame[14] + .35f;
        }
    }

    local.health = std::clamp(runtime.health, 0.f, 100000.f);
    local.armour = std::clamp(runtime.armour, 0.f, 100000.f);
    local.recoveryTime = std::clamp(runtime.playerDeathTime, 0.f, 3600.f);
    local.flags = runtime.busted ? 2u : runtime.health <= 0 ? 1u : 0u;
    if (environment.playerVisible) local.flags |= 4;
    if (runtime.policeIgnorePlayer) local.flags |= 8;
    local.wanted = uint32_t(std::clamp(runtime.wanted, 0, 6));
    if (local.dead()) {
        local.animation = runtime.playerDeath.animation;
        local.animationGroup = 0;
        local.scriptTime = runtime.playerDeathTime;
        local.deathBlendRate = runtime.playerDeath.blendRate;
        local.deathSpeed = runtime.playerDeath.speed;
        local.actionFlags &= 40u;
    }
    return local;
}

void StorylandLinuxSession::updateCoop(double now) {
    if (!updateCoopCharacter()) return;
    if (!canPlayCoop()) {
        if (network->active() || coop) disconnectCoop();
        return;
    }
    if (!network->active()) {
        if (coop) {
            coop.reset();
            remoteVehicleAsset.reset();
            remoteVehicleModel = -1;
            coopRestartPending = false;
        }
        return;
    }
    const auto local = coopPlayerSnapshot();
    coopClock = now;
    network->update(local, now);
    if (!updateCoopCharacter()) return;
    if (!network->connected()) {
        coop.reset();
        remoteVehicleAsset.reset();
        remoteVehicleModel = -1;
        coopRestartPending = false;
        return;
    }
    if (!coop) {
        coop = std::make_unique<StorylandCoopGame>(runtime, mission, program, *network);
        lastCoopAttack = -1;
        coop->actorRemoved = [this](int32_t handle) { missionBlends.erase(handle); };
        coop->actorMovement = [this](int32_t handle, float forward, float right) {
            auto& blend = missionBlends[handle];
            blend.movementForward = forward;
            blend.movementRight = right;
        };
        coop->collisionWorld = &archive.nativeWorld();
        coop->vehicleActors = &vehicles;
        coop->ensureVehicleModel = [this](int32_t model) {
            if (std::any_of(runtime.modelDefinitions.begin(), runtime.modelDefinitions.end(),
                [&](const auto& definition) { return definition.modelId == uint32_t(model); })) return true;
            StorylandPlayTestAsset asset;
            std::string error;
            if (!storylandLoadPlayTestVehicle(archive, uint32_t(model), asset, error)) {
                coop->status = error.empty() ? "The peer's vehicle model could not be loaded." : error;
                return false;
            }
            registerVehicleAsset(std::move(asset));
            return true;
        };
        coop->skipCutscene = [this] {
            if (!environment.cutscene.playing) return false;
            cutscene.skip(environment);
            return true;
        };
        coop->restartMission = [this](int32_t slot) {
            const int32_t intro = storylandFindIntroMissionSlot(script, vcs());
            if (intro < 0 || slot != intro) return false;
            environment.introMissionSlot = intro;
            coopRestartPending = true;
            return true;
        };
        coop->validateHit = [this](unsigned playerIndex, const StorylandMissionCharacterDamage& hit) {
            if (playerIndex != 1 || !network->isHost() || !network->connected() ||
                (hit.kind != 1 && hit.kind != 16)) return false;
            const auto character = environment.missionCharacters.find(hit.handle);
            if (character == environment.missionCharacters.end()) return false;
            const auto weapon = std::find_if(runtime.weapons.begin(), runtime.weapons.end(),
                [&](const auto& definition) { return definition.nativeId == hit.weapon; });
            if (weapon == runtime.weapons.end() || weapon->damage <= 0 ||
                std::fabs(hit.amount - weapon->damage) > .01f ||
                (hit.kind == 1 && weapon->fireType != 1) || (hit.kind == 16 && weapon->fireType != 0)) return false;
            if (lastCoopAttack >= 0 && hit.attackTime - lastCoopAttack < std::max(.01f, weapon->interval) * .8f)
                return false;
            const auto& remote = network->remote;
            const StorylandNativePoint origin{remote.x, remote.y, remote.z + 1.f};
            const auto& targetActor = character->second;
            const StorylandNativePoint target{targetActor.x, targetActor.y, targetActor.z + (hit.headHit ? 1.6f : 1.f)};
            if (std::hypot(origin.x - target.x, origin.y - target.y, origin.z - target.z) > weapon->range + 1.f ||
                storylandTraceNativeSegment(archive.nativeWorld(), origin, target) < .995f) return false;
            lastCoopAttack = hit.attackTime;
            return true;
        };
    }
    coop->beforeSimulation(local, now);
    if (!updateCoopCharacter()) return;
    if (coopRestartPending) {
        coopRestartPending = false;
        std::string error;
        if (!restartIntroMission(error)) coop->status = error.empty() ? "Could not restart the host's intro." : error;
    }
}

void StorylandLinuxSession::publishCoop(double now) {
    if (!coop || !network->connected() || !canPlayCoop()) return;
    int32_t activeMission = program.activeMission();
    const bool executing = mission.state == StorylandMissionState::Running ||
        mission.state == StorylandMissionState::Waiting;
    if (activeMission < 0 && executing && mission.isMissionThread) activeMission = mission.missionSlot;
    if (activeMission < 0) {
        const auto name = program.activeMissionName(mission);
        if (!name.empty()) {
            uint32_t identifier = 2166136261u;
            for (unsigned char letter : name) identifier = (identifier ^ letter) * 16777619u;
            activeMission = int32_t((identifier & 0x3fffffffu) | 0x40000000u);
        }
    }
    coop->afterSimulation(activeMission, now);
}
