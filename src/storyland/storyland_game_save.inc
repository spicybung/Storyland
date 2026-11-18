#include "src/storyland/storyland_user_directories.h"
#include "src/re/reVCS/services/storyland_army_tolerance.h"
#include "src/storyland/storyland_game_save_policy.h"
#include "src/storyland/storyland_save_weapon_table.h"

static std::string storylandMissionModelName(int32_t model) {
    const auto special=gPlayTestMissionEnvironment.specialModelNames.find(model);
    if(special!=gPlayTestMissionEnvironment.specialModelNames.end())return special->second;
    if(model<0)return gPlayTestScm.scriptObjectName(model);
    size_t index=0;std::string name;
    gDtzArchive.findModelDirEntryByModelId(uint32_t(model),index,name);return name;
}
static bool gStorylandSaveSlotsOpen=false;
static std::string gStorylandSaveStatus;
static std::vector<storyland_ps2_save::Slot> gStorylandPs2Slots;
static bool storylandCanSaveGame() {
    const auto& e=gPlayTestMissionEnvironment;
    const int flag=e.declaredMissionFlag;
    const bool mission=flag>=0&&size_t(flag)<gPlayTestMissionVm.globals.size()&&gPlayTestMissionVm.globals[size_t(flag)]!=0;
    const bool liveMission=gPlayTestMissionVm.isMissionThread&&(gPlayTestMissionVm.state==StorylandMissionState::Running||gPlayTestMissionVm.state==StorylandMissionState::Waiting);
    return !e.mobilePhones.count(gPlayTestMissionVm.vcs?0:1)&&!liveMission&&gPlayTestActive&&gPlayTestGameMode&&gPlayTestMissionProgram.active()&&(!mission||!e.saveGameFinished)&&
        !e.cutscene.loaded&&!e.cutscene.playing&&e.undressedCharacters.empty()&&gPlayTestRuntime.health>0&&gPlayTestRuntime.wanted==0&&
        gPlayTestRuntime.occupiedVehicle<0&&gPlayTestRuntime.entryTransition<0&&
        !gPlayTestRuntime.playerJetSkiFallActive&&!gPlayTestRuntime.playerReaction.scriptedDive()&&!gPlayTestRuntime.playerReaction.holding()&&
        gPlayTestRuntime.vehicleAction==StorylandPlayTestRuntime::VehicleAction::None;
}
static std::filesystem::path storylandDocumentsDirectory() {
    PWSTR documents=nullptr;
    const HRESULT result=SHGetKnownFolderPath(FOLDERID_Documents,KF_FLAG_CREATE,nullptr,&documents);
    const std::unique_ptr<wchar_t,decltype(&CoTaskMemFree)> owned(documents,&CoTaskMemFree);
    if(FAILED(result)||!documents)return {};
    return std::filesystem::path(documents);
}
static std::filesystem::path storylandOldUserFilesDirectory(bool vcs) {
    return storylandPreviousGameUserDirectory(storylandDocumentsDirectory(),vcs);
}
static std::filesystem::path storylandUserFilesDirectory(bool vcs) {
    const auto directory=storylandGameUserDirectory(storylandDocumentsDirectory(),vcs);
    if(directory.empty())return {};
    std::error_code error;
    std::filesystem::create_directories(directory,error);
    if(error)return {};
    return directory;
}
static bool storylandHasSavedGame(bool vcs) {
    const auto directory=storylandUserFilesDirectory(vcs);
    const auto previous=storylandOldUserFilesDirectory(vcs);
    for(int slot=1;slot<=8;++slot) {
        std::error_code error;
        if(std::filesystem::is_regular_file(storylandGameSaveSlot(directory,vcs,slot),error))return true;
        error.clear();
        if(std::filesystem::is_regular_file(storylandGameSaveSlot(previous,vcs,slot),error))return true;
    }
    return false;
}
static std::filesystem::path storylandSaveSlotPath(int slot) {
    if(slot<1||slot>8)return {};
    const auto directory=storylandUserFilesDirectory(gPlayTestMissionVm.vcs);
    if(directory.empty())return {};
    return storylandGameSaveSlot(directory,gPlayTestMissionVm.vcs,slot);
}
static std::filesystem::path storylandPreviousSaveSlotPath(int slot) {
    return storylandGameSaveSlot(storylandOldUserFilesDirectory(gPlayTestMissionVm.vcs),gPlayTestMissionVm.vcs,slot);
}
static std::filesystem::path storylandLegacySaveSlotPath(int slot) {
    wchar_t directory[MAX_PATH]{};
    const auto length=GetEnvironmentVariableW(L"LOCALAPPDATA",directory,MAX_PATH);
    if(!length||length>=MAX_PATH)return {};
    return std::filesystem::path(directory)/L"Storyland"/L"Saves"/
        (std::wstring(gPlayTestMissionVm.vcs?L"VCS-":L"LCS-")+std::to_wstring(slot)+L".slsav");
}
static bool storylandGameSaveSlot(int slot,bool load,const std::filesystem::path& relcsPath={},
    const std::filesystem::path& pcPath={}, const storyland_ps2_save::Slot* nativeSlot=nullptr) {
    const auto path=load&&!pcPath.empty()?pcPath:storylandSaveSlotPath(slot);gStorylandSaveStatus.clear();
    if(path.empty()){gStorylandSaveStatus="Cannot locate your save folder.";return false;}
    try {
        storyland_save::State state;
        std::shared_ptr<storyland_ps2_save::Slot> nativeCandidate;
        storyland::VcsGangTurfs restoredTurfs;
        bool restoreTurfs=false;
        if(!load) {
            if(gPlayTestRuntime.playerReaction.scriptedDive()||gPlayTestRuntime.playerReaction.holding()) {
                gStorylandSaveStatus="Finish the paired action or scripted evasive dive before saving.";return false;
            }
            if(!storylandCanSaveGame()){gStorylandSaveStatus="Save in free roam, on foot, outside missions and without a wanted level.";return false;}
            if(gPlayTestMissionVm.vcs&&gPlayTestVcsTurfs.gameplay().inCaptureWar()) {
                gStorylandSaveStatus="Finish the gang war before saving.";return false;
            }
            storylandUpdateAdrenaline(gPlayTestRuntime);
            storylandUpdateKillFrenzy(gPlayTestRuntime);
            state.environment=gPlayTestMissionEnvironment;state.mission=gPlayTestMissionVm;state.program=gPlayTestMissionProgram;
            // The transient audio ticket is not saved; capture its post-playback music state.
            if(gPlayTestMissionPassedTune&&!gPlayTestMissionVm.vcs&&state.environment.scriptAudioMode==0) {
                state.environment.scriptAudioMode=1;
                state.environment.radioEnabled=true;
                state.environment.radioChanged=true;
            }
            if(!storylandCaptureMissionCrouches(gPlayTestRuntime,state.environment)) {
                gStorylandSaveStatus="The current crouch animation cannot be captured safely.";return false;
            }
            state.environment.statistics=gPlayTestRuntime.statistics;
            auto& e=state.environment;e.x=gPlayerWorldX;e.y=gPlayerWorldY;e.z=gPlayerWorldZ;
            e.heading=gPlayTestPlayerHeading-90;e.health=gPlayTestRuntime.health;e.armour=gPlayTestRuntime.armour;
            e.hour=gPlayTestRuntime.clockHour;
            storylandPrepareSavedGame(e,gPlayTestMissionVm.vcs);
            e.money=gPlayTestRuntime.money;e.wanted=gPlayTestRuntime.wanted;
            e.saveMenuRequested=false;e.saveGameFinished=true;
            storylandUpdateBetaWeaponGrants(gPlayTestRuntime);
            state.betaWeapons=gPlayTestRuntime.betaWeapons;
            state.adrenaline=gPlayTestRuntime.adrenaline;
            state.killFrenzy=gPlayTestRuntime.killFrenzy;
            state.restartTaxi=gPlayTestRuntime.restartTaxi;
            state.populationRandom=gPlayTestRuntime.populationRandom;
            state.generatorStates=gPlayTestRuntime.generatorStates;
            state.betaVehicleBombsEnabled=gPlayTestRuntime.betaVehicleBombsEnabled;
            state.inventory=gPlayTestRuntime.inventory;state.confiscated=gPlayTestRuntime.confiscatedWeapons;
            for(const auto& w:gPlayTestRuntime.weapons)state.weaponIds.push_back(w.nativeId);
            for(const auto& pickup:gPlayTestRuntime.pickups)if(!pickup.randomlyGenerated)state.dropped.push_back(pickup);state.equipped=gPlayTestRuntime.equippedWeapon;
            state.elapsed=gPlayTestRuntime.elapsed;state.fortBaxterHeat=gPlayTestRuntime.fortBaxterHeat;
            state.armyFatigues=gPlayTestRuntime.armyFatigues;state.buyback=gPlayTestRuntime.weaponBuybackAvailable;
            state.buybackPosition=gPlayTestRuntime.weaponBuybackPosition;
            for(size_t i=0;i<gNativeVehicles.size()&&i<gPlayTestRuntime.vehicles.size();++i) {
                auto car=storyland_save::Vehicle::capture(gNativeVehicles[i],gPlayTestRuntime.vehicles[i]);
                if(gPlayTestMissionVm.vcs)storyland_save::captureServiceCrew(car,gPlayTestRuntime.vehicles[i],gPlayTestRuntime);
                if(!gPlayTestMissionVm.vcs)for(const auto& garage:e.garages)if(storyland_save::garageContains(garage,car.actor.position)) {
                    car.garage=garage.handle;break;
                }
                state.vehicles.push_back(std::move(car));
            }
            std::filesystem::create_directories(path.parent_path());
            const bool hasCoopState = gMultiplayer.active() || state.environment.coopFriendlyFire ||
                state.environment.coopGuestStage != 0 ||
                std::any_of(state.vehicles.begin(), state.vehicles.end(),
                    [](const storyland_save::Vehicle& vehicle) { return vehicle.garageOwner == 1; });
            const bool saveNativePs2 = bool(gStorylandPs2Session) && !hasCoopState;
            std::vector<uint8_t> gameBytes,turfBytes;
            std::shared_ptr<storyland_ps2_save::Slot> updated;
            if(saveNativePs2) {
                gameBytes=storyland_ps2_save::encodeLcsSession(*gStorylandPs2Session,gPlayTestScm,state);
                updated=std::make_shared<storyland_ps2_save::Slot>(storyland_ps2_save::decode(
                    gameBytes,gStorylandPs2Session->directory,gStorylandPs2Session->name));
            } else if(!storyland_save::encode(state,gPlayTestMissionVm,gameBytes,gStorylandSaveStatus))return false;
            std::vector<StorylandOutputFile> outputFiles{{path,&gameBytes}};
            if(gPlayTestMissionVm.vcs&&gPlayTestVcsTurfs.hasDataMod()) {
                if(!gPlayTestVcsTurfs.captureGameSave(turfBytes,gStorylandSaveStatus))return false;
                outputFiles.push_back({StorylandVcsTurfRuntime::gameSlotSidecar(path),&turfBytes});
            }
            if(!storylandWriteFilesTransaction(outputFiles,gStorylandSaveStatus))return false;
            gStorylandPs2Session=std::move(updated);
            gPlayTestRuntime.statistics=state.environment.statistics;
            gPlayTestMissionEnvironment.statistics=state.environment.statistics;
            storylandSetPlayTestClock(gPlayTestRuntime,state.environment.hour);
            gPlayTestMissionEnvironment.hour=state.environment.hour;
            gPlayTestMissionEnvironment.clockResetRequested=false;
            gPlayTestMissionEnvironment.playerSprintEnergy=state.environment.playerSprintEnergy;
            gStorylandSaveStatus=saveNativePs2?"Game saved in LCS PS2 format.":
                hasCoopState?"Game saved with co-op storage.":"Game saved.";
        } else {
            if(!gPlayTestGameMode||gPlayTestMissionEnvironment.cutscene.loaded||gPlayTestCutsceneActive) {
                gStorylandSaveStatus="Load from Game mode outside a cutscene.";return false;
            }
            auto readPath=path;
            if(pcPath.empty()&&!std::filesystem::exists(readPath)) {
                const auto previous=storylandPreviousSaveSlotPath(slot);
                if(!previous.empty()&&std::filesystem::exists(previous))readPath=previous;
            }
            if(pcPath.empty()&&!std::filesystem::exists(readPath)) {
                const auto legacy=storylandLegacySaveSlotPath(slot);
                if(!legacy.empty()&&std::filesystem::exists(legacy))readPath=legacy;
            }
            if (nativeSlot) nativeCandidate = std::make_shared<storyland_ps2_save::Slot>(*nativeSlot);
            else if (relcsPath.empty()) {
                std::ifstream input(readPath, std::ios::binary);
                std::array<char, 4> signature{};
                if (input.read(signature.data(), 4) && std::memcmp(signature.data(), "SIMP", 4) == 0)
                    nativeCandidate = std::make_shared<storyland_ps2_save::Slot>(storyland_ps2_save::readSave(readPath));
            }
            if (nativeCandidate) {
                if (gPlayTestMissionVm.vcs || nativeCandidate->vcs) {
                    gStorylandSaveStatus="VCS native player, garage and statistics restoration is not implemented.";
                    return false;
                }
                std::vector<int32_t> weapons;
                for (const auto& weapon : gPlayTestRuntime.weapons) weapons.push_back(weapon.nativeId);
                storyland_ps2_save::prepareLcsSession(*nativeCandidate, gPlayTestScm, weapons, state);
                state.environment.frenchGame=gPlayTestLcsRegion==1;
                state.environment.germanGame=gPlayTestLcsRegion==2;
            } else if(relcsPath.empty()) {
                if(!storyland_save::read(readPath,gPlayTestMissionVm,state,gStorylandSaveStatus))return false;
            } else {
                if(gPlayTestMissionVm.vcs){gStorylandSaveStatus="reLCS progress requires LCS Game mode.";return false;}
                StorylandRelcsProgress progress;
                if(!storylandReadRelcsProgress(relcsPath,gPlayTestScm,progress,gStorylandSaveStatus))return false;
                std::vector<uint32_t> offsets;
                for(const auto& mission:gPlayTestScm.missions())offsets.push_back(mission.binaryOffset);
                if(!state.program.start(gPlayTestScm.rawBytes(),gPlayTestScm.header().mainScriptOffset,
                    offsets,state.mission,state.environment,false)) {
                    gStorylandSaveStatus=state.program.status();return false;
                }
                state.environment.frenchGame=gPlayTestLcsRegion==1;
                state.environment.germanGame=gPlayTestLcsRegion==2;
                state.mission.startRetail(gPlayTestScm.rawBytes(),gPlayTestScm.header().mainScriptOffset,false);
                state.mission.state=StorylandMissionState::Idle;state.mission.isMissionThread=false;
                if(!state.program.seedPersistentGlobals(progress.globals,state.mission)) {
                    gStorylandSaveStatus="Saved LCS globals do not fit the script environment.";return false;
                }
                state.environment.garages=progress.garages;state.vehicles=progress.vehicles;
                for(const auto& garage:progress.garages) {
                    state.environment.importedGarageHandles.insert(garage.handle);
                    state.environment.nextGarageHandle=std::max(state.environment.nextGarageHandle,garage.handle+1);
                }
                state.environment.statistics=progress.statistics;
                state.environment.lastMissionPassedName=progress.lastMissionPassed;
                if(!progress.lastMissionPassed.empty())state.environment.passedMissions.insert(progress.lastMissionPassed);
                const auto stat=[&](const char* key){const auto i=progress.statistics.find(key);return i==progress.statistics.end()?0:int32_t(std::clamp(i->second,0.0,double(INT32_MAX)));};
                state.environment.progressMade=stat("ProgressMade");state.environment.progressTotal=stat("TotalProgressInGame");
                state.environment.missionsGiven=stat("MissionsGiven");state.environment.totalMissions=stat("TotalNumberMissions");
                state.environment.uniqueJumpsFound=stat("NumberOfUniqueJumpsFound");state.environment.totalUniqueJumps=stat("TotalNumberOfUniqueJumps");
                state.environment.money=progress.money;
                state.environment.hour=float(progress.hour)+float(progress.minute)/60;
                state.environment.x=gPlayerWorldX;state.environment.y=gPlayerWorldY;state.environment.z=gPlayerWorldZ;
                state.inventory.resize(gPlayTestRuntime.weapons.size());
                if(!state.inventory.empty())state.inventory[0].owned=true;
                for(const auto& weapon:gPlayTestRuntime.weapons)state.weaponIds.push_back(weapon.nativeId);
            }
            if(state.mission.vcs) {
                if(!gPlayTestVcsTurfs.prepareGameSlot(readPath,restoredTurfs,gStorylandSaveStatus))return false;
                restoreTurfs=true;
            }
            if(!storylandValidateVcsPedColorSetCursors(state.environment,state.mission.vcs,
                gDtzArchive.unpackedBytes(),gStorylandSaveStatus))return false;
            if(!storyland_save::restoreWeaponTable(state,gPlayTestRuntime.weapons,gStorylandSaveStatus))return false;
            if(state.vehicles.size()>gPlayTestRuntime.vehicles.size()) {
                gStorylandSaveStatus="Saved vehicle pool exceeds this build's capacity.";return false;
            }
            for(const auto& car:state.vehicles)if(!ensureStorylandPlayTestVehicleModel(car.model)) {
                gStorylandSaveStatus="Cannot restore saved vehicle model "+std::to_string(car.model)+".";return false;
            }
            for(const auto& vehicle:state.vehicles) {
                const auto definition=std::find_if(gPlayTestRuntime.modelDefinitions.begin(),
                    gPlayTestRuntime.modelDefinitions.end(),
                    [&](const auto& item){return item.modelId==vehicle.model;});
                if(definition==gPlayTestRuntime.modelDefinitions.end()||
                    !vehicle.validVcsPlaneControlsFor(*definition)||
                    !vehicle.validVcsJetSkiAxesFor(*definition)) {
                    gStorylandSaveStatus="Saved VCS vehicle controls do not match the loaded vehicle model.";
                    return false;
                }
            }
            // Rebuild decoded animation residency before publishing the save.
            // Requests from the state being replaced must not make a restored
            // IS_ANIMATION_LOADED query succeed for an unrelated bank.
            StorylandPlayTestAnimation restoredPlayerAnimation;
            StorylandPlayTestAnimation restoredPedestrianAnimation;
            StorylandPlayTestAsset restoredPlayer;
            if(state.mission.vcs) {
                uint32_t model=0;
                const auto& name=state.environment.playerOutfit;
                if(!gDtzArchive.findModelIdByName(name,model)||model>INT32_MAX) {
                    gStorylandSaveStatus="Saved VCS clothing does not identify an original pedestrian model.";
                    return false;
                }
                if(!storylandLoadPlayTestCharacterSkin(gDtzArchive,int32_t(model),name,
                    restoredPlayer,gStorylandSaveStatus))return false;
            } else {
                const auto skin=state.environment.characterSkins.find(1);
                const std::string name=skin==state.environment.characterSkins.end()?"player":skin->second;
                if(!storylandLoadPlayTestCharacterSkin(gDtzArchive,0,name,restoredPlayer,gStorylandSaveStatus))return false;
            }
            if (!restoredPlayerAnimation.load(gDtzArchive, true, gStorylandSaveStatus) ||
                !restoredPedestrianAnimation.load(gDtzArchive, false, gStorylandSaveStatus)) {
                return false;
            }
            for (const auto& bank : state.environment.requestedAnimationBanks) {
                if (!restoredPlayerAnimation.loadAnimationBank(gDtzArchive, bank, gStorylandSaveStatus) ||
                    !restoredPedestrianAnimation.loadAnimationBank(gDtzArchive, bank, gStorylandSaveStatus)) {
                    return false;
                }
            }
            for (int32_t group : state.environment.requestedAnimationGroups) {
                if (!restoredPlayerAnimation.loadAnimationGroup(gDtzArchive, group, gStorylandSaveStatus) ||
                    !restoredPedestrianAnimation.loadAnimationGroup(gDtzArchive, group, gStorylandSaveStatus)) {
                    return false;
                }
            }
            // Rebind to the destination before publishing, never to the temporary snapshot.
            if (!state.program.restoreBindings(gPlayTestMissionVm,gPlayTestMissionEnvironment)) {
                gStorylandSaveStatus="Saved script execution does not match the loaded game. The current session was not replaced.";
                return false;
            }
            state.mission.environment=&gPlayTestMissionEnvironment;
            state.environment.zones=&gDtzArchive.nativeWorld().zones;
            for(auto& entry:state.environment.missionVehicles)entry.second.spawnedSlot=-1;
            for(auto& generator:state.environment.carGenerators)generator.spawnedSlot=-1;
            stopStorylandMissionPassedTune(false);
            resetStorylandBuildingSwaps();
            gPlayTestMissionEnvironment=std::move(state.environment);
            gMultiplayer.setFriendlyFire(gPlayTestMissionEnvironment.coopFriendlyFire);
            gPlayTestNativeCutscene.discard(gPlayTestMissionEnvironment);
            if(!state.mission.vcs)gPlayTestLcsRegion=gPlayTestMissionEnvironment.germanGame?2:
                gPlayTestMissionEnvironment.frenchGame?1:0;
            gPlayTestMissionEnvironment.frameScriptCheckpoints.clear();
            gPlayTestMissionEnvironment.frameScriptShadows.clear();
            gPlayTestMissionEnvironment.photographFlashUntil=0;
            if (state.mission.vcs && state.loadedVersion >= 22) {
                gPlayTestMissionEnvironment.weatherRestoreRequested = true;
            }
            gPlayTestMissionVm=std::move(state.mission);gPlayTestMissionProgram=std::move(state.program);
            gStorylandPs2Session=std::move(nativeCandidate);
            gPlayTestPlayerAnimation = std::move(restoredPlayerAnimation);
            gPlayTestPedestrianAnimation = std::move(restoredPedestrianAnimation);
            if(gPlayTestMissionVm.vcs)gPlayTestRuntime.playerStats=restoredPlayer.pedStats;
            gPlayTestPlayerModel=std::move(restoredPlayer.model);
            gPlayTestPlayerImages=std::move(restoredPlayer.images);
            gPlayTestPlayerTextureNames=std::move(restoredPlayer.textureNames);
            gPlayTestPlayerName=std::move(restoredPlayer.name);
            gPlayTestPlayerLoaded=true;
            gPlayTestPlayerAnimation.configureCombatDefinitions(gPlayTestRuntime.weapons,&gPlayTestPlayerModel);
            ++gPlayTestGraphicsRevision;
            gPlayTestPlayerBlend = {};
            gPlayTestPedestrianBlends = {};
            gPlayTestMissionCharacterBlends.clear();
            gPlayTestPlayerBlend.walkSpeed = gPlayTestRuntime.walkSpeed;
            gPlayTestPlayerBlend.runSpeed = gPlayTestRuntime.runSpeed;
            for (auto& blend : gPlayTestPedestrianBlends) {
                blend.walkSpeed = gPlayTestRuntime.walkSpeed;
                blend.runSpeed = gPlayTestRuntime.runSpeed;
            }
            auto& e=gPlayTestMissionEnvironment;auto& r=gPlayTestRuntime;
            r.statistics=e.statistics;r.statisticsPositionValid=false;
            r.inventory=std::move(state.inventory);r.confiscatedWeapons=std::move(state.confiscated);
            r.betaWeapons=state.betaWeapons;
            r.adrenaline=state.adrenaline;r.killFrenzy=state.killFrenzy;
            r.restartTaxi=state.restartTaxi;
            storylandDisableEscalators(r.escalators,gDtzArchive.nativeWorld());
            r.populationRandom=state.populationRandom;
            r.generatorStates=std::move(state.generatorStates);
            if(state.loadedVersion<28) {
                const int32_t intro=storylandFindIntroMissionSlot(gPlayTestScm,gPlayTestMissionVm.vcs);
                e.introMissionSlot=intro;
                e.introMissionProtected=intro>=0 && gPlayTestMissionProgram.activeMission()>=0 &&
                    gPlayTestMissionProgram.activeMission()<=intro &&
                    !(gPlayTestMissionVm.missionSlot==intro &&
                      gPlayTestMissionVm.missionOutcome==StorylandMissionOutcome::Passed);
            }
            e.introMissionRestartQueued=false;
            r.playerRestart={};r.playerReaction.reset();r.playerDeath={};r.vehicleExitPose={};
            r.playerBulletReaction={};r.playerVehicleContact={};
            r.pedestrianContactPlayerVelocity={};r.nextVehiclePedContactIdentity=1;
            r.playerLastGunshotTime=-1000;r.playerLastGunshotPosition={};
            r.playerCorpseBlood={};r.bloodSplatSequence=0;r.roadSkids.clear();
            r.tyreBloodContacts.clear();
            for(auto& entry:r.missionCombat) {
                entry.second.corpseBlood={};entry.second.bulletReaction={};entry.second.vehicleContact={};
            }
            r.equalizerScope=false;r.equalizerScopeHeld=false;
            r.pcGrabHeld=false;r.pcCancelAim=false;r.combatControlWeapon=-1;
            r.playerFall={};
            r.playerJetSkiFall={};r.playerJetSkiFallActive=false;
            r.vehiclePhysicsError.clear();
            r.betaVehicleBombsEnabled=state.betaVehicleBombsEnabled;
            r.vehicleBombButtonWasDown=false;
            r.pickups=std::move(state.dropped);r.nextPickupRefresh=0;r.equippedWeapon=state.equipped;r.elapsed=state.elapsed;
            r.fortBaxterHeat=state.fortBaxterHeat;r.fortBaxterLastUpdate=r.elapsed;
            r.fortBaxterPendingDeaths=0;r.fortBaxterWasInScriptZone=false;
            r.fortBaxterScriptStateObserved=false;
            r.fortBaxterAlarm=StorylandFortBaxterRetailResponse::alarmFromMissionHeat(r.fortBaxterHeat);
            r.armyFatigues=state.armyFatigues;
            r.weaponBuybackAvailable=state.buyback;r.weaponBuybackPosition=state.buybackPosition;
    r.health=e.health;r.armour=e.armour;r.money=e.money;r.wanted=e.wanted;
    r.lcsHud={};r.lcsHud.resetMoney(r.money);
            storylandSetPlayTestClock(r,e.hour);e.clockResetRequested=false;
            r.visibleArea=e.area;r.playerControl=e.control;r.modelsKilledByPlayer=e.modelsKilledByPlayer;
            if(gPlayTestMissionVm.vcs) {
                const auto forModel=[&](int32_t model) {
                    const auto count=r.modelsKilledByPlayer.find(model);
                    return count==r.modelsKilledByPlayer.end()?uint32_t(0):count->second;
                };
                const uint64_t pending=uint64_t(forModel(97))+uint64_t(forModel(98));
                r.fortBaxterPendingDeaths=uint32_t(std::min<uint64_t>(pending,UINT32_MAX));
                const auto flags=e.characterFlags.find(0);
                if(flags!=e.characterFlags.end()) {
                    r.fortBaxterScriptStateObserved=true;
                    r.fortBaxterAlarm=StorylandFortBaxterRetailResponse::hostileFromRetailScript(
                        e.gangs[3].attackWithPolice,flags->second);
                }
            }
            r.occupiedVehicle=-1;r.transitionVehicle=-1;r.vehicleAction=StorylandPlayTestRuntime::VehicleAction::None;
            r.entryTransition=-1;r.entryFade=0;r.entryNeedsLeave=false;r.verticalSpeed=0;
            storylandResetPlayerSprint(r);
            r.wastedActive=false;r.busted=false;r.playerDeathTime=0;r.respawnFade=0;r.crime={};r.crime.vcs=gPlayTestMissionVm.vcs;
            r.targetedPed=-1;r.shotTraces.clear();r.projectiles.clear();r.vcsSniperBullets={};r.waterCannons={};r.particles.clear();r.effects.clear();r.soundEvents.clear();
            r.sniperTraceActive=false;r.drunkMovementHistory={};
            r.playerLastShotTime=-1000.0f;r.playerMelee={};
            r.livePaths=gDtzArchive.nativeWorld().paths;r.appliedRoadChanges=0;
            gNativeVehicles.clear();gPlayTestVehicleDriverTransitions.clear();r.nextVehicleRefresh=0;
            gNativePedestrians.clear();r.nextLcsPedestrianRefresh=0;
            for(auto& ped:r.pedestrians)ped={};
            // Ambient slots are reconstructed by the native zone/group
            // generator after saved mission vehicles and actors are restored.
            r.fortBaxterPopulationAt=r.elapsed;r.policeDispatchTime=0;r.arrestProgress=0;
            for(auto& v:r.vehicles){v.scriptHandle=-1;v.generator=-1;v.savedGarage=-1;v.parked=false;v.bloodyTyres={};}
            for(const auto& saved:state.vehicles) {
                const auto definition=std::find_if(r.modelDefinitions.begin(),r.modelDefinitions.end(),
                    [&](const auto& model){return model.modelId==saved.model;});
                if(definition==r.modelDefinitions.end())continue; // Preloaded above.
                const size_t slot=gNativeVehicles.size();
                StorylandRuntimeVehicle vehicle;
                vehicle.model=size_t(definition-r.modelDefinitions.begin());
                storylandApplyVehicleDefinition(vehicle,*definition);
                if(vehicle.model<r.modelExtents.size()) {
                    vehicle.halfWidth=r.modelExtents[vehicle.model][0];vehicle.halfLength=r.modelExtents[vehicle.model][1];
                    vehicle.bodyHeight=r.modelExtents[vehicle.model][2];
                }
                saved.restore(vehicle);gNativeVehicles.push_back(saved.actor);r.vehicles[slot]=vehicle;
                if(saved.scriptHandle>=0)gPlayTestMissionEnvironment.missionVehicles.at(saved.scriptHandle).spawnedSlot=int32_t(slot);
                for(auto& generator:gPlayTestMissionEnvironment.carGenerators)
                    if(generator.handle==saved.generator)generator.spawnedSlot=int32_t(slot);
            }
            gPlayerWorldX=e.x;gPlayerWorldY=e.y;gPlayerWorldZ=e.z;gPlayTestPlayerHeading=e.heading+90;
            r.playerPosition={e.x,e.y,e.z};
            storylandResetStoriesCamera(gPlayTestCamera,r.playerPosition,gPlayTestPlayerHeading);
            gPlayTestCameraRefocusRequested=true;
            gPlayTestMissionCharacterAssets.clear();
            gPlayTestObjectAtomicAssets.clear();
            gPlayTestLoadedSpecialModels.clear();
            ++gPlayTestGraphicsRevision;
            gPlayTestMissionEnvironment.residentModels.clear();
            for (const auto& asset : gPlayTestVehicleAssets) {
                gPlayTestMissionEnvironment.residentModels.insert(int32_t(asset.vehicleDefinition.modelId));
            }
            for (auto& voice : gPlayTestContinuousVoices) {
                if (voice.second.ticket) {
                    voice.second.ticket->cancelled = true;
                }
            }
            gPlayTestContinuousVoices.clear();
            gPlayTestMissionCharacterBlends.clear();
            for(const auto& binding:gPlayTestMissionEnvironment.specialModelNames) {
                gPlayTestMissionEnvironment.residentModels.erase(binding.first);
                gPlayTestMissionEnvironment.changedSpecialModels.insert(binding.first);
                gPlayTestMissionEnvironment.requestedModels.insert(binding.first);
            }
            gPlayTestMissionEnvironment.buildingModelSwapsChanged=
                !gPlayTestMissionEnvironment.buildingModelSwaps.empty();
            for(const auto& swap:gPlayTestMissionEnvironment.buildingSwapStates)
                gPlayTestMissionEnvironment.buildingSwapCommands.push_back({swap.first,uint32_t(swap.second),false});
            for(const auto& swap:gPlayTestMissionEnvironment.lcsBuildingSwapGroups)
                gPlayTestMissionEnvironment.buildingSwapCommands.push_back({swap.first,uint32_t(swap.second),true});
            if(restoreTurfs)gPlayTestVcsTurfs.restoreGameSlot(std::move(restoredTurfs));
            gStorylandFrontend=false;
            gStorylandSaveSlotsOpen=false;
            gPlayTestRuntime.pauseMenu=false;
            gPlayerCameraLastTick=0;
            gNativeTrafficLastTick=storylandFrameTimeSeconds();
            gStorylandSaveStatus=gStorylandPs2Session?
                "Loaded LCS PS2 progress, player rewards, garages and statistics. MAIN is restarting from persistent variables; saves remain in PS2 format.":relcsPath.empty()?"Game loaded.":
                "Imported reLCS garages, supported stored cars, script progress, statistics, last completed mission, money and clock. LCS MAIN restarts; this profile does not restore weapons or running entity pools.";
        }
        gPlayTestMissionEnvironment.saveMenuRequested=false;gPlayTestMissionEnvironment.saveGameFinished=true;
        return true;
    }catch(const std::exception& e){gStorylandSaveStatus=e.what();return false;}
}
static std::string storylandSaveMenuText(const char* key, const char* fallback) {
    std::u16string text;
    if (gPlayTestMissionGxt.lookup(key, "MAIN", text) && !text.empty())
        return narrow(std::wstring(text.begin(), text.end()));
    return fallback;
}
static bool storylandSaveMenuButton(const char* identifier, const char* key, const char* fallback) {
    const std::string text = storylandSaveMenuText(key, fallback);
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const bool pressed = ImGui::Button(identifier, ImVec2(width, 38));
    const auto extent = storylandMeasurePlayTestText(&gPlayTestHudAssets, text.c_str(), 20.0f, width - 24);
    const ImU32 accent = gPlayTestMissionVm.vcs ? IM_COL32(242, 111, 178, 255) : IM_COL32(130, 177, 235, 255);
    storylandDrawPlayTestText(ImGui::GetWindowDrawList(), &gPlayTestHudAssets,
        ImVec2(position.x + 12, position.y + (38 - extent.y) * 0.5f),
        ImGui::IsItemHovered() ? accent : IM_COL32_WHITE, text.c_str(), 20.0f, width - 24);
    return pressed;
}
static void storylandFindSave() {
    const auto selected = openFileDialog(L"Game saves and memory cards\0*.b;*.slsav;*.ps2;*.mc2;*.mcd;*.bin;*.sfo\0All files\0*.*\0");
    if (selected.empty()) return;
    const std::filesystem::path path(selected);
    std::ifstream input(path, std::ios::binary);
    std::array<char, 8> header{};
    if (!input.read(header.data(), std::streamsize(header.size()))) {
        gStorylandSaveStatus = "Cannot read the selected save header.";
        return;
    }
    input.close();
    if (std::memcmp(header.data(), "SLNDSAV1", 8) == 0) {
        storylandGameSaveSlot(1, true, {}, path);
        return;
    }
    if (storyland_psp_save::describeUnsupportedImport(path, gStorylandSaveStatus)) return;
    try {
        std::vector<storyland_ps2_save::Slot> slots;
        if (std::memcmp(header.data(), "Sony PS2", 8) == 0) {
            storyland_ps2_save::MemoryCard card;
            slots = card.open(path);
        } else if (std::memcmp(header.data(), "SIMP", 4) == 0) {
            slots.push_back(storyland_ps2_save::readSave(path));
        } else {
            gStorylandSaveStatus = "This file is not a Storyland PC save, PS2 game save, or formatted PS2 memory card.";
            return;
        }
        gStorylandPs2Slots = std::move(slots);
        gStorylandSaveStatus = gStorylandPs2Slots.empty()
            ? "No supported LCS or VCS saves were found on this card."
            : "PS2 saves verified. Expand a save below to load it. VCS restoration is not available yet.";
    } catch (const std::exception& error) {
        gStorylandSaveStatus = error.what();
    }
}
static void storylandDrawSaveSlots() {
    if(!gPlayTestActive){gStorylandSaveSlotsOpen=false;return;}
    if(gPlayTestMissionEnvironment.saveMenuRequested) {
        gStorylandSaveSlotsOpen=true;gPlayTestRuntime.pauseMenu=true;
        gPlayTestMissionEnvironment.saveMenuRequested=false;
    }
    if(!gStorylandSaveSlotsOpen)return;
    gPlayTestRuntime.pauseMenu=true;
    const ImVec2 display=ImGui::GetIO().DisplaySize;
    const ImVec2 maximum(std::max(1.0f,display.x-16),std::max(1.0f,display.y-16));
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(300.0f,maximum.x),std::min(180.0f,maximum.y)),maximum);
    ImGui::SetNextWindowSize(ImVec2(std::min(430.0f,maximum.x),std::min(420.0f,maximum.y)),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(display.x*.5f,display.y*.5f),ImGuiCond_Appearing,ImVec2(.5f,.5f));
    if(ImGui::Begin("Save / Load Game",&gStorylandSaveSlotsOpen,ImGuiWindowFlags_NoCollapse)) {
        if (storylandSaveMenuButton("##FindSave", "FES_FIND", "Find Save")) storylandFindSave();
        ImGui::Separator();
        ImGui::TextUnformatted("Storyland save slots");
        for(int slot=1;slot<=8;++slot) {
            ImGui::PushID(slot);ImGui::Text("Slot %d",slot);ImGui::SameLine();
            if(!gStorylandFrontend) {
                ImGui::BeginDisabled(!storylandCanSaveGame());
                if(ImGui::Button("Save"))storylandGameSaveSlot(slot,false);
                ImGui::EndDisabled();ImGui::SameLine();
            }
            std::error_code slotError;
            const bool exists=std::filesystem::is_regular_file(storylandSaveSlotPath(slot),slotError);
            const bool previous=std::filesystem::is_regular_file(storylandPreviousSaveSlotPath(slot),slotError);
            const bool legacy=std::filesystem::is_regular_file(storylandLegacySaveSlotPath(slot),slotError);
            ImGui::BeginDisabled(!exists&&!previous&&!legacy);
            if(ImGui::Button("Load"))storylandGameSaveSlot(slot,true);
            ImGui::EndDisabled();
            if(!exists&&!previous&&!legacy){ImGui::SameLine();ImGui::TextDisabled("Empty");}
            ImGui::PopID();
        }
        if(!gPlayTestMissionVm.vcs) {
            ImGui::TextWrapped("reLCS import: garages, supported stored cars, script progress, statistics, money and time. Original files are kept.");
            if(ImGui::Button("Import reLCS progress...")) {
                const auto selected=openFileDialog(L"reLCS PC saves\0*.b\0All files\0*.*\0");
                if(!selected.empty())storylandGameSaveSlot(1,true,std::filesystem::path(selected));
            }
        }
        if (ImGui::Button("Open PS2 memory card...")) {
            const auto selected = openFileDialog(L"PS2 memory cards\0*.ps2;*.mc2;*.mcd\0All files\0*.*\0");
            if (!selected.empty()) {
                gStorylandPs2Slots.clear();
                try {
                    storyland_ps2_save::MemoryCard card;
                    gStorylandPs2Slots = card.open(std::filesystem::path(selected));
                    gStorylandSaveStatus = gStorylandPs2Slots.empty()
                        ? "No supported LCS or VCS saves were found on this card."
                        : "Card read successfully. Game-state restoration from PS2 saves is not available yet.";
                } catch (const std::exception& error) {
                    gStorylandSaveStatus = error.what();
                }
            }
        }
        for (size_t index = 0; index < gStorylandPs2Slots.size(); ++index) {
            const auto& slot = gStorylandPs2Slots[index];
            ImGui::PushID(int(index));
            if (ImGui::TreeNode(slot.name.c_str())) {
                ImGui::Text("%s PS2", slot.vcs ? "VCS" : "LCS");
                ImGui::TextUnformatted(slot.directory.c_str());
                ImGui::TextUnformatted("Save checksum passed");
                if (!slot.vcs && ImGui::Button("Load LCS PS2 save"))
                    storylandGameSaveSlot(1, true, {}, {}, &slot);
                if (!slot.vcs && ImGui::Button("Inspect converted LCS state")) {
                    try {
                        const auto converted = storyland_ps2_save::convertLcs(slot);
                        std::string frames;
                        const auto& header = gPlayTestScm.header();
                        if (header.valid && header.dialect == StorylandScmDialect::Lcs &&
                            header.mainScriptSize == converted.mainSize &&
                            header.largestMissionScriptSize == converted.largestMissionSize &&
                            header.missionCount == converted.missionCount) {
                            auto scm = std::make_shared<const std::vector<uint8_t>>(gPlayTestScm.rawBytes());
                            size_t count = 0;
                            for (const auto& thread : converted.threads) {
                                const auto restored = storyland_ps2_save::restoreLcsThread(thread, scm, converted.mainSize);
                                count += restored.functionFrames.size();
                            }
                            frames = " Converted " + std::to_string(count) + " saved function frames.";
                        }
                        gStorylandSaveStatus = "Decoded " + std::to_string(converted.threads.size()) +
                            " native threads, " + std::to_string(converted.garages.size()) +
                            " garages and " + std::to_string(converted.statistics.size()) +
                            " statistics. Money: " + std::to_string(converted.player.money) +
                            ". Last mission: " + converted.lastMission +
                            "." + frames + " This inspection does not replace the running game.";
                    } catch (const std::exception& error) {
                        gStorylandSaveStatus = error.what();
                    }
                }

                constexpr const char* blockNames[] = {"Simple variables", "Scripts", "Garages", "Player", "Statistics"};
                for (size_t block = 0; block < slot.blocks.size(); ++block)
                    ImGui::Text("%s: %zu bytes", blockNames[block], slot.blocks[block].size());
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::TextWrapped("%s",gStorylandSaveStatus.c_str());
        if(ImGui::Button("Close"))gStorylandSaveSlotsOpen=false;
    }
    ImGui::End();
    if(!gStorylandSaveSlotsOpen)gPlayTestMissionEnvironment.saveGameFinished=true;
}
