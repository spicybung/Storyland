#include "src/storyland/storyland_save.h"
#include "src/storyland/storyland_save_weapon_table.h"
#include "src/re/reVCS/services/storyland_army_tolerance.h"

bool StorylandLinuxSession::captureCheckpoint(storyland_save::State& state, std::string& error) {
    if (!running || !playingGame || !program.active() || mission.scriptBytes().empty()) {
        error = "Start a game before saving its state.";
        return false;
    }
    if (runtime.playerReaction.scriptedDive() || runtime.playerReaction.holding()) {
        error = "Finish the paired action or scripted evasive dive before saving its transient animation state.";
        return false;
    }
    if (environment.cutscene.loaded || environment.cutscene.playing ||
        !environment.undressedCharacters.empty() || environment.hasScriptedLookTransition() ||
        runtime.occupiedVehicle >= 0 || environment.inVehicle || runtime.health <= 0 ||
        runtime.entryTransition >= 0 || runtime.playerJetSkiFallActive ||
        runtime.vehicleAction != StorylandPlayTestRuntime::VehicleAction::None) {
        error = "A checkpoint requires a living player on foot, outside a cutscene or transition.";
        return false;
    }
    storylandUpdateAdrenaline(runtime);
    storylandUpdateKillFrenzy(runtime);
    storylandUpdateBetaWeaponGrants(runtime);
    state = {};
    state.loadedVersion = storyland_save::Version;
    state.environment = environment;
    state.mission = mission;
    state.program = program;
    if (!storylandCaptureMissionCrouches(runtime, state.environment)) {
        error = "The current crouch animation cannot be captured safely.";
        return false;
    }
    auto& world = state.environment;
    world.statistics = runtime.statistics;
    world.x = playerPosition.x;
    world.y = playerPosition.y;
    world.z = playerPosition.z;
    world.heading = playerHeading - 90;
    world.health = runtime.health;
    world.armour = runtime.armour;
    world.hour = runtime.clockHour;
    world.money = runtime.money;
    world.wanted = runtime.wanted;
    state.betaWeapons = runtime.betaWeapons;
    state.adrenaline = runtime.adrenaline;
    state.killFrenzy = runtime.killFrenzy;
    state.restartTaxi = runtime.restartTaxi;
    state.populationRandom = runtime.populationRandom;
    state.generatorStates = runtime.generatorStates;
    state.betaVehicleBombsEnabled = runtime.betaVehicleBombsEnabled;
    state.inventory = runtime.inventory;
    state.confiscated = runtime.confiscatedWeapons;
    for (const auto& weapon : runtime.weapons) state.weaponIds.push_back(weapon.nativeId);
    for (const auto& pickup : runtime.pickups)
        if (!pickup.randomlyGenerated) state.dropped.push_back(pickup);
    state.equipped = runtime.equippedWeapon;
    state.elapsed = runtime.elapsed;
    state.fortBaxterHeat = runtime.fortBaxterHeat;
    state.armyFatigues = runtime.armyFatigues;
    state.buyback = runtime.weaponBuybackAvailable;
    state.buybackPosition = runtime.weaponBuybackPosition;
    for (size_t index = 0; index < vehicles.size() && index < runtime.vehicles.size(); ++index) {
        auto vehicle = storyland_save::Vehicle::capture(vehicles[index], runtime.vehicles[index]);
        if(vcs())storyland_save::captureServiceCrew(vehicle,runtime.vehicles[index],runtime);
        if (!vcs()) for (const auto& garage : world.garages)
            if (storyland_save::garageContains(garage, vehicle.actor.position)) {
                vehicle.garage = garage.handle;
                break;
            }
        state.vehicles.push_back(std::move(vehicle));
    }
    error.clear();
    return true;
}

bool StorylandLinuxSession::restoreCheckpoint(storyland_save::State state, std::string& error) {
    if (!worldLoaded || runtime.weapons.empty() || script.rawBytes().empty()) {
        error = "Initialize the matching game before loading its state.";
        return false;
    }
    StorylandMissionVm source = mission;
    if (source.scriptBytes().empty())
        source.startRetail(script.rawBytes(), script.header().mainScriptOffset, vcs());
    source.environment = &environment;
    // Use the same validation for an in-memory checkpoint and a file-backed save.
    const uint32_t loadedVersion = state.loadedVersion;
    std::vector<uint8_t> encoded;
    if (!storyland_save::encode(state, source, encoded, error) ||
        !storyland_save::decode(std::move(encoded), source, state, error)) return false;
    if (loadedVersion) state.loadedVersion = loadedVersion;
    if (!storylandValidateVcsPedColorSetCursors(state.environment, state.mission.vcs,
        archive.unpackedBytes(), error)) return false;
    if (!storyland_save::restoreWeaponTable(state, runtime.weapons, error)) return false;
    if (state.vehicles.size() > runtime.vehicles.size()) {
        error = "Saved vehicle pool exceeds this build's capacity.";
        return false;
    }
    std::vector<StorylandPlayTestAsset> restoredVehicles;
    std::set<int32_t> missingModels;
    for (const auto& vehicle : state.vehicles) {
        const bool present = std::any_of(runtime.modelDefinitions.begin(), runtime.modelDefinitions.end(),
            [&](const auto& definition) { return definition.modelId == uint32_t(vehicle.model); });
        if (present || !missingModels.insert(vehicle.model).second) continue;
        StorylandPlayTestAsset asset;
        if (!storylandLoadPlayTestVehicle(archive, uint32_t(vehicle.model), asset, error)) return false;
        restoredVehicles.push_back(std::move(asset));
    }
    for(const auto& vehicle:state.vehicles) {
        const auto resident=std::find_if(runtime.modelDefinitions.begin(),runtime.modelDefinitions.end(),
            [&](const auto& definition){return definition.modelId==vehicle.model;});
        const StorylandVehicleDefinition* definition=resident==runtime.modelDefinitions.end()?nullptr:&*resident;
        if(!definition) {
            const auto restored=std::find_if(restoredVehicles.begin(),restoredVehicles.end(),
                [&](const auto& asset){return asset.vehicleDefinition.modelId==vehicle.model;});
            if(restored!=restoredVehicles.end())definition=&restored->vehicleDefinition;
        }
        if(!definition||!vehicle.validVcsPlaneControlsFor(*definition)||
            !vehicle.validVcsJetSkiAxesFor(*definition)) {
            error="Saved VCS vehicle controls do not match the loaded vehicle model.";
            return false;
        }
    }
    StorylandPlayTestAnimation restoredPlayerAnimation;
    StorylandPlayTestAnimation restoredPedestrianAnimation;
    StorylandPlayTestAsset restoredPlayer;
    if(vcs()) {
        uint32_t model=0;
        const auto& name=state.environment.playerOutfit;
        if(!archive.findModelIdByName(name,model)||model>INT32_MAX) {
            error="Saved VCS clothing does not identify an original pedestrian model.";
            return false;
        }
        if(!storylandLoadPlayTestCharacterSkin(archive,int32_t(model),name,restoredPlayer,error))return false;
    } else {
        const auto skin=state.environment.characterSkins.find(1);
        const std::string name=skin==state.environment.characterSkins.end()?"player":skin->second;
        if(!storylandLoadPlayTestCharacterSkin(archive,0,name,restoredPlayer,error))return false;
    }
    if (!restoredPlayerAnimation.load(archive, true, error) ||
        !restoredPedestrianAnimation.load(archive, false, error)) return false;
    for (const auto& bank : state.environment.requestedAnimationBanks)
        if (!restoredPlayerAnimation.loadAnimationBank(archive, bank, error) ||
            !restoredPedestrianAnimation.loadAnimationBank(archive, bank, error)) return false;
    for (int32_t group : state.environment.requestedAnimationGroups)
        if (!restoredPlayerAnimation.loadAnimationGroup(archive, group, error) ||
            !restoredPedestrianAnimation.loadAnimationGroup(archive, group, error)) return false;
    if (!state.program.restoreBindings(source, environment)) {
        error = "Saved script execution does not match the loaded game.";
        return false;
    }
    state.mission.environment = &environment;
    for (auto& vehicle : state.environment.missionVehicles) vehicle.second.spawnedSlot = -1;
    for (auto& generator : state.environment.carGenerators) generator.spawnedSlot = -1;
    if (stopMissionTune) stopMissionTune();
    disconnectCoop();
    archive.resetBuildingSwaps();
    for (const auto& area : areas) area->resetBuildingSwaps();
    storylandDisableEscalators(runtime.escalators, archive.nativeWorld());
    environment = std::move(state.environment);
    network->setFriendlyFire(environment.coopFriendlyFire);
    cutscene.discard(environment);
    mission = std::move(state.mission);
    program = std::move(state.program);
    playerAnimation = std::move(restoredPlayerAnimation);
    pedestrianAnimation = std::move(restoredPedestrianAnimation);
    player=std::move(restoredPlayer);playerLoaded=true;
    if(vcs())runtime.playerStats=player.pedStats;
    playerAnimation.configureCombatDefinitions(runtime.weapons, &player.model);
    for (auto& asset : vehicleAssets) playerAnimation.configureVehicleAnimations(asset.vehicleDefinition);
    for (auto& asset : restoredVehicles) registerVehicleAsset(std::move(asset));
    playerBlend = {};
    playerBlend.walkSpeed = runtime.walkSpeed;
    playerBlend.runSpeed = runtime.runSpeed;
    pedestrianBlends.clear();
    missionBlends.clear();
    vehicleDriverBlends.clear();
    playerPoints.clear();
    auto& world = environment;
    world.frameScriptCheckpoints.clear();
    world.frameScriptShadows.clear();
    world.photographFlashUntil = 0;
    world.weatherRestoreRequested = vcs() && state.loadedVersion >= 22;
    runtime.statistics = world.statistics;
    runtime.statisticsPositionValid = false;
    runtime.inventory = std::move(state.inventory);
    runtime.confiscatedWeapons = std::move(state.confiscated);
    runtime.betaWeapons = state.betaWeapons;
    runtime.adrenaline = state.adrenaline;
    runtime.killFrenzy = state.killFrenzy;
    runtime.restartTaxi = state.restartTaxi;
    runtime.populationRandom = state.populationRandom;
    runtime.generatorStates = std::move(state.generatorStates);
    if (state.loadedVersion < 28) {
        const int32_t intro = storylandFindIntroMissionSlot(script, vcs());
        world.introMissionSlot = intro;
        world.introMissionProtected = intro >= 0 && program.activeMission() >= 0 &&
            program.activeMission() <= intro && !(mission.missionSlot == intro &&
                mission.missionOutcome == StorylandMissionOutcome::Passed);
    }
    world.introMissionRestartQueued = false;
    runtime.playerRestart = {};
    runtime.vehiclePhysicsError.clear();
    runtime.playerReaction.reset();
    runtime.playerBulletReaction={};runtime.playerVehicleContact={};
    runtime.pedestrianContactPlayerVelocity={};runtime.nextVehiclePedContactIdentity=1;
    runtime.playerLastGunshotTime=-1000;runtime.playerLastGunshotPosition={};
    runtime.equalizerScope=false;runtime.equalizerScopeHeld=false;
    runtime.pcGrabHeld=false;runtime.pcCancelAim=false;runtime.combatControlWeapon=-1;
    runtime.playerDeath = {};
    runtime.playerCorpseBlood = {};
    runtime.bloodSplatSequence = 0;
    runtime.tyreBloodContacts.clear();
    runtime.roadSkids.clear();
    for(auto& entry:runtime.missionCombat) {
        entry.second.corpseBlood={};entry.second.bulletReaction={};entry.second.vehicleContact={};
    }
    runtime.vehicleExitPose = {};runtime.playerJetSkiFall={};runtime.playerJetSkiFallActive=false;
    runtime.betaVehicleBombsEnabled = state.betaVehicleBombsEnabled;
    runtime.vehicleBombButtonWasDown = false;
    runtime.pickups = std::move(state.dropped);
    runtime.nextPickupRefresh = 0;
    runtime.equippedWeapon = state.equipped;
    runtime.elapsed = state.elapsed;
    runtime.fortBaxterHeat = state.fortBaxterHeat;
    runtime.fortBaxterLastUpdate = runtime.elapsed;
    runtime.fortBaxterPendingDeaths = 0;
    runtime.fortBaxterWasInScriptZone = false;
    runtime.fortBaxterScriptStateObserved = false;
    runtime.fortBaxterAlarm = StorylandFortBaxterRetailResponse::alarmFromMissionHeat(runtime.fortBaxterHeat);
    runtime.armyFatigues = state.armyFatigues;
    runtime.weaponBuybackAvailable = state.buyback;
    runtime.weaponBuybackPosition = state.buybackPosition;
    runtime.health = world.health;
    runtime.armour = world.armour;
    runtime.money = world.money;
    runtime.lcsHud = {};
    runtime.lcsHud.resetMoney(runtime.money);
    runtime.wanted = world.wanted;
    storylandSetPlayTestClock(runtime,world.hour);
    world.clockResetRequested=false;
    runtime.visibleArea = world.area;
    runtime.playerControl = world.control;
    runtime.modelsKilledByPlayer = world.modelsKilledByPlayer;
    if (vcs()) {
        const auto count = [&](int32_t model) {
            const auto found = runtime.modelsKilledByPlayer.find(model);
            return found == runtime.modelsKilledByPlayer.end() ? uint32_t(0) : found->second;
        };
        runtime.fortBaxterPendingDeaths = uint32_t(std::min<uint64_t>(uint64_t(count(97)) + count(98), UINT32_MAX));
        const auto flags = world.characterFlags.find(0);
        if (flags != world.characterFlags.end()) {
            runtime.fortBaxterScriptStateObserved = true;
            runtime.fortBaxterAlarm = StorylandFortBaxterRetailResponse::hostileFromRetailScript(
                world.gangs[3].attackWithPolice, flags->second);
        }
    }
    runtime.occupiedVehicle = -1;
    runtime.playerVehicleSeat = -1;
    runtime.transitionVehicle = -1;
    runtime.vehicleAction = StorylandPlayTestRuntime::VehicleAction::None;
    runtime.entryTransition = -1;
    runtime.entryFade = 0;
    runtime.entryNeedsLeave = false;
    runtime.verticalSpeed = 0;
    storylandResetPlayerSprint(runtime);
    runtime.wastedActive = false;
    runtime.busted = false;
    runtime.playerDeathTime = 0;
    runtime.respawnFade = 0;
    runtime.crime = {};
    runtime.crime.vcs = vcs();
    runtime.targetedPed = -1;
    runtime.shotTraces.clear();
    runtime.projectiles.clear();
    runtime.vcsSniperBullets={};
    runtime.waterCannons={};
    runtime.particles.clear();
    runtime.effects.clear();
    runtime.soundEvents.clear();
    runtime.sniperTraceActive = false;
    runtime.drunkMovementHistory = {};
    runtime.playerLastShotTime = -1000;
    runtime.playerMelee = {};
    runtime.livePaths = archive.nativeWorld().paths;
    runtime.appliedRoadChanges = 0;
    vehicles.clear();
    pedestrians.clear();
    runtime.nextVehicleRefresh = 0;
    runtime.nextLcsPedestrianRefresh = 0;
    for (auto& pedestrian : runtime.pedestrians) pedestrian = {};
    runtime.fortBaxterPopulationAt = runtime.elapsed;
    runtime.policeDispatchTime = 0;
    runtime.arrestProgress = 0;
    for (auto& vehicle : runtime.vehicles) {
        vehicle = {};
        vehicle.scriptHandle = vehicle.generator = vehicle.savedGarage = -1;
    }
    for (const auto& saved : state.vehicles) {
        const auto definition = std::find_if(runtime.modelDefinitions.begin(), runtime.modelDefinitions.end(),
            [&](const auto& model) { return model.modelId == uint32_t(saved.model); });
        const size_t slot = vehicles.size();
        StorylandRuntimeVehicle vehicle;
        vehicle.model = size_t(definition - runtime.modelDefinitions.begin());
        storylandApplyVehicleDefinition(vehicle, *definition);
        if (vehicle.model < runtime.modelExtents.size()) {
            vehicle.halfWidth = runtime.modelExtents[vehicle.model][0];
            vehicle.halfLength = runtime.modelExtents[vehicle.model][1];
            vehicle.bodyHeight = runtime.modelExtents[vehicle.model][2];
        }
        saved.restore(vehicle);
        vehicles.push_back(saved.actor);
        runtime.vehicles[slot] = vehicle;
        const auto scriptVehicle = world.missionVehicles.find(saved.scriptHandle);
        if (scriptVehicle != world.missionVehicles.end()) scriptVehicle->second.spawnedSlot = int32_t(slot);
        for (auto& generator : world.carGenerators)
            if (generator.handle == saved.generator) generator.spawnedSlot = int32_t(slot);
    }
    playerPosition = {world.x, world.y, world.z};
    playerHeading = world.heading + 90;
    playerMotion = StorylandPlayTestMotion::Idle;
    runtime.playerPosition = playerPosition;
    runtime.missionEnvironment = &environment;
    runtime.pauseMenu = false;
    storylandResetStoriesCamera(camera, playerPosition, playerHeading);
    missionAssets.clear();
    compiledWorldAssets.clear();
    world.residentModels.clear();
    for (const auto& asset : vehicleAssets)
        world.residentModels.insert(int32_t(asset.vehicleDefinition.modelId));
    for (const auto& binding : world.specialModelNames) {
        world.changedSpecialModels.insert(binding.first);
        world.requestedModels.insert(binding.first);
    }
    world.buildingModelSwapsChanged = !world.buildingModelSwaps.empty();
    for (const auto& swap : world.buildingSwapStates)
        world.buildingSwapCommands.push_back({swap.first, uint32_t(swap.second), false});
    for (const auto& swap : world.lcsBuildingSwapGroups)
        world.buildingSwapCommands.push_back({swap.first, uint32_t(swap.second), true});
    nativeSceneryNeedsRefresh = true;
    ++sceneryRevision;
    animationTime = runtime.elapsed;
    millisecondRemainder = 0;
    bindEnvironment();
    running = playingGame = true;
    paused = false;
    statusMessage = "Game loaded.";
    error.clear();
    return true;
}

bool StorylandLinuxSession::saveCheckpoint(const std::filesystem::path& path, std::string& error) {
    storyland_save::State state;
    if (!captureCheckpoint(state, error)) return false;
    return storyland_save::write(path, state, mission, error);
}

bool StorylandLinuxSession::loadCheckpoint(const std::filesystem::path& path, std::string& error) {
    storyland_save::State state;
    StorylandMissionVm source = mission;
    if (source.scriptBytes().empty())
        source.startRetail(script.rawBytes(), script.header().mainScriptOffset, vcs());
    source.environment = &environment;
    if (!storyland_save::read(path, source, state, error)) return false;
    return restoreCheckpoint(std::move(state), error);
}
