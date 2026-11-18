#include "src/re/reVCS/services/storyland_vcs_service_crew.h"
#include "src/platform/linux/storyland_linux_session.h"
#include "src/storyland/storyland_time.h"
#include "src/storyland/storyland_ped_water_motion.h"
#include "src/storyland/storyland_bike_controls.h"
#include <SDL.h>
#include "src/storyland/storyland_multiplayer.h"
#include "src/storyland/storyland_coop_character_assets.h"
#include "src/storyland/storyland_weapon_animation_policy.h"
#include "src/storyland/storyland_intro_mission.h"
#include "src/storyland/storyland_cutscene_archive_paths.h"
#include "src/storyland/storyland_freeplay_initialization.h"
#include "src/storyland/storyland_pickups.h"
#include "src/storyland/storyland_car_generators.h"
#include "src/storyland/storyland_vehicle_radio.h"
#include "src/storyland/storyland_radio_stations.h"
#include "src/storyland/storyland_play_test_spawn.h"
#include "src/re/reVCS/gangs/storyland_empire_entities.h"
#include "src/storyland/storyland_game_files.h"
#include "src/storyland/storyland_language.h"
#include "src/re/reVCS/modloader/storyland_vcs_modloader.h"
#include "src/storyland/storyland_scenery_log.h"
#include "src/storyland/storyland_native_scenery_visibility.h"
#include "src/re/reLCS2/world/storyland_lcs_garages.h"
#include "src/storyland/storyland_vehicle_systems.h"
#include "src/re/reVCS/world/storyland_vcs_safehouse_garages.h"
#include "src/storyland/storyland_mission_vehicle_transform.h"
#include "src/re/reVCS/vehicles/storyland_vcs_helicopter_coordinates.h"
#include "src/re/reVCS/camera/storyland_vcs_remote_camera.h"
#include "src/storyland/storyland_script_streaming.h"
#include "src/storyland/storyland_ped_animation_dependency.h"
#include "src/leeds/leeds_ped_types.h"
#include "src/storyland/storyland_service_assets.h"
#include "src/storyland/storyland_police.h"
#include "src/re/reVCS/services/storyland_vcs_police_assailant.h"
#include "src/storyland/storyland_mission_radar.h"
#include "src/storyland/storyland_script_object.h"
#include "src/storyland/storyland_script_character_object_contact.h"
#include "src/storyland/storyland_script_object_contact.h"
#include "src/re/reVCS/world/storyland_vcs_ferris_wheel.h"
#include "src/re/reVCS/world/storyland_vcs_ferris_placement.h"
#include "src/storyland/storyland_script_world_query.h"
#include "src/re/reLCS2/runtime/storyland_lcs_host_queries.h"
#include "src/re/reLCS2/world/storyland_lcs_crusher_runtime.h"
#include "src/re/reLCS2/world/storyland_lcs_crane_render.h"
#include "src/re/reLCS2/runtime/storyland_lcs_messages.h"
#include "src/storyland/storyland_play_test_script.h"
#include "src/re/reLCS2/vehicles/storyland_lcs_vehicle_tasks.h"
#include "src/storyland/storyland_script_vehicle_animation.h"
#include "src/storyland/storyland_vehicle_pose_query.h"
#include "src/storyland/storyland_vehicle_victim_recovery.h"
#include "src/storyland/storyland_vehicle_bailout.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_ped_appearance.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include "src/re/reVCS/vehicles/storyland_vcs_helicopter_exit.h"
#include "src/storyland/storyland_vehicle_passengers.h"
#include "src/leeds/leeds_model_bounds.h"
#include "src/storyland/storyland_play_test_weapons.h"
#include "src/storyland/storyland_weapon_attachment.h"
#include "src/storyland/storyland_swimming_pose.h"
#include "src/storyland/storyland_player_pose_frame.h"
#include "src/re/reLCS2/runtime/storyland_lcs_population.h"
#include "src/re/reLCS2/pedestrians/storyland_lcs_wait_animation.h"
#include "src/storyland/storyland_population.h"
#include "src/storyland/storyland_vehicle_wheels.h"
#include "src/storyland/storyland_play_test_hud.h"
#include "src/storyland/storyland_script_camera.h"
#include "src/re/reVCS/camera/storyland_vcs_camera_fov.h"
#include "src/storyland/storyland_crouch_pose.h"
#include "src/storyland/storyland_sa_crouch_motion.h"
#include "src/re/reLCS2/pedestrians/storyland_lcs_ped_look.h"
#include "src/re/reVCS/runtime/storyland_vcs_attachment.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_ped_hit.h"
#include "src/re/reVCS/runtime/storyland_vcs_pad.h"
#include "src/storyland/storyland_character_sound_events.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <codecvt>
#include <iostream>
#include <locale>
#include <set>
#include <sstream>
#include "src/platform/linux/storyland_linux_checkpoint.inc"
#include "src/platform/linux/storyland_linux_save_slots.inc"

namespace {
std::string lower(std::string value) {
    for (char& c : value) c = char(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
constexpr float radians = .01745329251994329577f;

void setLocomotionMovement(StorylandAnimationBlendState& blend, float deltaX, float deltaY,
    float heading, float seconds) {
    blend.movementForward = blend.movementRight = 0;
    if (!std::isfinite(seconds) || seconds <= 0 || !std::isfinite(heading)) return;
    const float velocityX = deltaX / seconds;
    const float velocityY = deltaY / seconds;
    if (!std::isfinite(velocityX) || !std::isfinite(velocityY)) return;
    const float facing = heading * radians;
    blend.movementForward = velocityX * std::cos(facing) + velocityY * std::sin(facing);
    blend.movementRight = velocityX * std::sin(facing) - velocityY * std::cos(facing);
}

std::string textUtf8(const std::u16string& text) {
    std::string result;
    for (size_t index = 0; index < text.size(); ++index) {
        uint32_t code = text[index];
        if (code >= 0xD800 && code <= 0xDBFF && index + 1 < text.size() &&
            text[index + 1] >= 0xDC00 && text[index + 1] <= 0xDFFF) {
            code = 0x10000 + ((code - 0xD800) << 10) + (text[++index] - 0xDC00);
        } else if (code >= 0xD800 && code <= 0xDFFF) {
            code = 0xFFFD;
        }
        if (code < 0x80) result.push_back(char(code));
        else if (code < 0x800) {
            result.push_back(char(0xC0 | (code >> 6)));
            result.push_back(char(0x80 | (code & 0x3F)));
        } else if (code < 0x10000) {
            result.push_back(char(0xE0 | (code >> 12)));
            result.push_back(char(0x80 | ((code >> 6) & 0x3F)));
            result.push_back(char(0x80 | (code & 0x3F)));
        } else {
            result.push_back(char(0xF0 | (code >> 18)));
            result.push_back(char(0x80 | ((code >> 12) & 0x3F)));
            result.push_back(char(0x80 | ((code >> 6) & 0x3F)));
            result.push_back(char(0x80 | (code & 0x3F)));
        }
    }
    return result;
}

void prepareWeaponPose(StorylandPlayTestAnimation& animation, StorylandAnimationBlendState& blend,
    const StorylandPlayTestRuntime& runtime, size_t weapon, bool aiming, bool reloading,
    bool moving, uint32_t shotSerial, float seconds, int pedGroup = -1, bool police = false,
    uint32_t meleeSequence = 0) {
    if (weapon >= runtime.weapons.size()) {
        blend.weaponAiming = false;
        blend.weaponReloading = false;
        blend.upperActive = false;
        blend.weaponIndex = SIZE_MAX;
        blend.weaponAimBlend = blend.weaponAimTime = 0;
        blend.weaponAimWasActive = false;
        blend.weaponAimIk = {};
        blend.weaponAimDirectionValid = false;
        return;
    }
    const bool newShot = shotSerial != blend.weaponShotSerial;
    if (blend.weaponIndex != weapon) {
        blend.weaponIndex = weapon;
        blend.upperActive = false;
        blend.weaponReloading = false;
        blend.weaponShotSerial = shotSerial;
        blend.upperStartedAt = seconds;
        blend.weaponAimBlend = blend.weaponAimTime = 0;
        blend.weaponAimWasActive = false;
        blend.weaponAimIk = {};
    }
    if (newShot || reloading != blend.weaponReloading) {
        blend.upperStartedAt = seconds;
        blend.upperActive = newShot || reloading;
    }
    blend.weaponShotSerial = shotSerial;
    const auto& definition = runtime.weapons[weapon];
    blend.weaponAiming = aiming && definition.fireType != 0;
    blend.weaponReloading = reloading;
    if(!animation.selectWeaponAction(definition,
        reloading ? "reload" : blend.upperActive ? "fire" : "aim",
        moving, definition.fireType == 0 ? meleeSequence : shotSerial, pedGroup, aiming, police)) {
        blend.weaponAiming=false;
        blend.weaponReloading=false;
        blend.upperActive=false;
    }
}
}

std::string StorylandLinuxSession::translateMissionText(const std::string& key, const std::string& table) const {
    if (key.empty()) return {};
    if (dyomSession) {
        const auto found = dyomMission.text.find(key);
        if (found != dyomMission.text.end()) return found->second;
    }
    std::u16string text;
    if (!missionGxt.lookup(key, table, text)) return key;
    return textUtf8(text);
}

bool StorylandLinuxSession::loadGame(const std::filesystem::path& path, std::string& error) {
    stopGame();
    worldLoaded = false;
    areas.clear();
    sceneryReport={};
    nativeMarkerModels.clear();
    missionAssets.clear();
    compiledWorldAssets.clear();
    missionBlends.clear();
    weaponAssets.clear();
    vehicleAssets.clear();
    pedestrianAssets.clear();
    pedestrianBlends.clear();
    vehicles.clear();
    pedestrians.clear();
    playerLoaded = false;
    playerPoints.clear();
    std::error_code ec;
    gameDirectory = std::filesystem::is_directory(path, ec) ? path : path.parent_path();
    auto dtz = std::filesystem::is_directory(path, ec) ? storylandFindNativeGameFile(gameDirectory, "GAME.DTZ") : path;
    if (dtz.empty() || lower(dtz.extension().string()) != ".dtz") {
        error = "Select GAME.DTZ or the game directory containing it.";
        statusMessage = error; return false;
    }
    if (!archive.loadFromFile(dtz.wstring(), error)) { statusMessage = error; return false; }
    const std::array<uint8_t,2> markerTypes=vcs()?
        std::array<uint8_t,2>{6,7}:std::array<uint8_t,2>{5,0};
    for(uint8_t type:markerTypes) {
        if(!type)continue;
        StorylandModelFile marker;
        if(!marker.loadNativeMarker(archive.unpackedBytes(),vcs(),type,error)) {
            statusMessage="Native marker "+std::to_string(type)+": "+error;
            error=statusMessage;
            return false;
        }
        nativeMarkerModels.emplace(type,std::move(marker));
    }
    for(uint8_t type:std::array<uint8_t,3>{1,3,8}) {
        const uint8_t nativeType=!vcs()&&type==3u?2u:type;
        StorylandModelFile marker;
        std::string markerError;
        if(marker.loadNativeMarker(archive.unpackedBytes(),vcs(),nativeType,markerError))
            nativeMarkerModels.emplace(type,std::move(marker));
    }
    if (!archive.hasCompanionImg()) {
        error = "The matching GTA3PSP.IMG or GTA3PS2.IMG companion is missing or does not match GAME.DTZ.";
        if (!archive.companionStatus().empty()) error += " " + archive.companionStatus();
        statusMessage = error; return false;
    }
    const auto areaFiles = storylandFindNativeWorldAreas(dtz);
    for (const auto& lvz : areaFiles) {
        auto area = std::make_unique<StorylandArchiveBrowser>();
        if (!area->loadLvzWithCompanionImg(lvz.wstring(), error,archive.modloader())) {
            error = "World area " + lvz.filename().string() + ": " + error;
            statusMessage = error; return false;
        }
        areas.push_back(std::move(area));
    }
    if (areas.empty()) {
        error = "No native world LVZ/IMG area pairs were found in the game folders.";
        statusMessage = error; return false;
    }
    auto scenery=std::make_unique<StorylandArchiveBrowser>();
    if(!scenery->loadNativeScenery(archive,sceneryReport,error)) {
        statusMessage="Vegetation and objects: "+error;
        return false;
    }
    nativeSceneryNeedsRefresh=false;
    storylandWriteSceneryReport(std::clog,sceneryReport,archive.companionImgPath());
    storylandSaveSceneryReport(sceneryReport,archive.companionImgPath(),
        gameDirectory/"Storyland-Scenery.log");
    if(!scenery->placements().empty())areas.push_back(std::move(scenery));
    const auto* mods=archive.modloader();
    const auto retailScm=storylandFindNativeGameFile(dtz,"MAIN.SCM");
    const auto scm=mods?(retailScm.empty()?mods->resolveGameFile("MAIN.SCM"):
        mods->resolveRetailPath(retailScm)):retailScm;
    if (scm.empty() || !script.loadFromFile(scm.wstring(), error, false)) {
        if (error.empty()) error = "MAIN.SCM is missing.";
        statusMessage = error; return false;
    }
    missionGxt = {};
    const std::wstring selectedText = storylandGameTextFilename(gStorylandUiLanguage);
    const std::wstring textCandidates[] = {selectedText, L"ENGLISH.GXT"};
    for (const auto& candidate : textCandidates) {
        const std::string filename(candidate.begin(), candidate.end());
        const auto retailText = storylandFindNativeGameFile(dtz, filename);
        const auto textFile = mods ? (retailText.empty() ? mods->resolveGameFile(filename) :
            mods->resolveRetailPath(retailText)) : retailText;
        if (textFile.empty()) continue;
        std::string textError;
        if (!missionGxt.loadFromFile(textFile.wstring(), textError)) continue;
        if (candidate == L"RUSSIAN.GXT") {
            missionGxt.setEncoding(StorylandGxtEncoding::Windows1251);
        }
        script.applyRetailMissionTitles(missionGxt);
        break;
    }
    std::vector<std::filesystem::path> cutsceneFiles;
    for (const auto& folder : storylandNativeGameFolders(dtz)) {
        std::error_code scanError;
        std::filesystem::directory_iterator cursor(folder,
            std::filesystem::directory_options::skip_permission_denied,scanError),end;
        for (;!scanError&&cursor!=end;cursor.increment(scanError)) {
            std::error_code fileError;
            if (cursor->is_regular_file(fileError)&&!fileError) cutsceneFiles.push_back(cursor->path());
        }
    }
    if(mods) {
        cutsceneFiles.clear();
        const auto assets=storylandDiscoverRetailAssets(dtz.wstring(),{},scm.wstring(),mods);
        for(const auto& asset:assets.files)cutsceneFiles.emplace_back(asset.path);
    }
    const auto cutsceneArchive=storylandFindCutsceneArchive(cutsceneFiles,vcs());
    mocap = StorylandMocapArchive{};
    if (cutsceneArchive.valid() &&
        !mocap.load(cutsceneArchive.directory,cutsceneArchive.image,error)) {
        statusMessage = "Native cutscene archive: " + error;
        return false;
    }
    if (!vcs() && !cutsceneArchive.image.empty() && !cutsceneArchive.valid() &&
        !mocap.loadLcsEmbeddedDirectory(archive.unpackedBytes(),cutsceneArchive.image,error)) {
        statusMessage = "Embedded CUTS directory: " + error;
        return false;
    }
    if (!preparePlayer(error)) { statusMessage = error; return false; }
    worldLoaded = true;
    statusMessage = std::string(vcs() ? "VCS" : "LCS") + " native game loaded: " +
        std::to_string(areaFiles.size()) + " world area(s), " + std::to_string(script.missions().size()) +
        " missions. " + sceneryReport.summary();
    error.clear();return true;
}

bool StorylandLinuxSession::preparePlayer(std::string& error) {
    std::vector<StorylandPlayTestAsset> assets;
    if (!storylandLoadPlayTestModels(archive, {vcs() ? "plr" : "player"}, assets, error) || assets.empty()) return false;
    player = std::move(assets.front());
    if (!playerAnimation.load(archive, true, error)) return false;
    if (!pedestrianAnimation.load(archive, false, error)) return false;
    extendedCrouchAvailable = false;
    {
        std::vector<std::filesystem::path> folders{std::filesystem::current_path(),gameDirectory};
        if(char* executable=SDL_GetBasePath()) {folders.emplace_back(executable);SDL_free(executable);}
        std::string diagnostic;
        for(auto folder:folders) {
            for(unsigned level=0;level<5&&!extendedCrouchAvailable;++level) {
                const auto path=folder/"assets"/"animations"/"vcs_sa"/"crouch_movement.anim";
                if(std::filesystem::is_regular_file(path))
                    extendedCrouchAvailable=extendedCrouchAnimations.loadFromFile(path.wstring(),diagnostic);
                const auto parent=folder.parent_path();if(parent==folder)break;folder=parent;
            }
            if(extendedCrouchAvailable)break;
        }
    }
    divingAnimations.available=false;
    if(vcs()) {
        std::vector<std::filesystem::path> folders{std::filesystem::current_path(),gameDirectory};
        if(char* executable=SDL_GetBasePath()) {
            folders.emplace_back(executable);SDL_free(executable);
        }
        std::string diagnostic;
        for(auto folder:folders) {
            for(unsigned level=0;level<5&&!divingAnimations.available;++level) {
                const auto path=folder/"assets"/"animations"/"vcs_sa"/"diving.anim";
                if(std::filesystem::is_regular_file(path))divingAnimations.load(path.wstring(),diagnostic);
                const auto parent=folder.parent_path();if(parent==folder)break;folder=parent;
            }
            if(divingAnimations.available)break;
        }
        if(!divingAnimations.available)statusMessage=diagnostic.empty()?
            "Optional SA diving requires assets/animations/vcs_sa/diving.anim.":diagnostic;
    }
    playerBlend = {};
    playerPoints.clear();
    if (!playerAnimation.sampleTree(player.model, StorylandPlayTestMotion::Idle, 0,
        playerBlend, playerPoints, error)) return false;
    playerLoaded = true;
    return true;
}

bool StorylandLinuxSession::startGame(std::string& error, bool requireCutscenes) {
    if (!worldLoaded) { error = "Open the native game files before starting Play Game."; return false; }
    if (requireCutscenes && !mocap.available()) {
        error = vcs() ? "VCS Play Game needs the native MOCAPPS2.DIR/.IMG or MOCAPPSP.DIR/.IMG pair." :
            "LCS Play Game needs the original ANIM/CUTS.DIR and CUTS.IMG pair.";
        statusMessage = error; return false;
    }
    if (requireCutscenes && !vcs() && !mocap.hasScene("INTRO")) {
        error = "LCS INTRO needs its complete CUT/CAM/ANIM or CUT/DAT/IFP resources in CUTS.IMG.";
        statusMessage = error; return false;
    }
    if(std::filesystem::path(player.name).stem().u8string()!=(vcs()?"plr":"player")&&
        !preparePlayer(error))return false;
    const bool invertedMap=environment.invertedMap;
    const bool aggressivePedestrians=runtime.aggressivePedestrians;
    const bool zombiePedestrians=runtime.zombiePedestrians&&vcs();
    const bool collisionOccupantDamage=runtime.collisionOccupantDamage;
    stopGame();
    if(mission.state==StorylandMissionState::Unsupported) {
        error=mission.diagnostic;
        statusMessage=error;
        return false;
    }
    environment = StorylandMissionEnvironment{};
    environment.invertedMap=invertedMap;
    for (uint32_t type = 0; type < 23; ++type) {
        StorylandPedTypeRecord record;
        if (storylandPedTypeRecord(archive.unpackedBytes(), vcs(), type, record))
            environment.pedTypeThreats[int32_t(type)] = record.threats;
    }
    mission = StorylandMissionVm{};
    runtime.reset(0, 0, 12);
    runtime.aggressivePedestrians=aggressivePedestrians;
    runtime.collisionOccupantDamage=collisionOccupantDamage;
    escalatorStepAsset.reset();escalatorAssetStatus.clear();
    if(!storylandDecodeEscalators(archive.unpackedBytes(),vcs(),runtime.escalators,error)) {
        statusMessage=error;return false;
    }
    if(!runtime.escalators.escalators.empty()) {
        auto asset=std::make_unique<StorylandPlayTestAsset>();
        if(storylandLoadPlayTestEscalatorStep(archive,runtime.escalators.stepModel,*asset,escalatorAssetStatus))
            escalatorStepAsset=std::move(asset);
        else if(escalatorAssetStatus.find("PS2 DMA/VIF/VU/FIFO [")!=std::string::npos) {
            error=statusMessage=escalatorAssetStatus;
            return false;
        }
    }
    runtime.vehicles.resize(StorylandPlayTestRuntime::vehicleCapacity(vcs()));
    runtime.missionEnvironment = &environment;
    runtime.livePaths = archive.nativeWorld().paths;
    storylandLoadArchivedEntryExits(archive,runtime.entryExits);
    runtime.crime.vcs = vcs();
    runtime.playerCanSwim = vcs();
    runtime.playerStats = player.pedStats;
    if (!storylandLoadPlayTestWeapons(archive, runtime.weapons, weaponAssets, error)) {
        statusMessage = error;
        return false;
    }
    playerAnimation.configureCombatDefinitions(runtime.weapons, &player.model);
    runtime.inventory.resize(runtime.weapons.size());
    if (!runtime.inventory.empty()) runtime.inventory.front().owned = true;
    vehicles.clear(); pedestrians.clear(); vehicleAssets.clear(); pedestrianAssets.clear();
    pedestrianBlends.clear(); missionAssets.clear(); compiledWorldAssets.clear();
    missionBlends.clear();
    runtime.modelDefinitions.clear(); runtime.modelExtents.clear(); runtime.modelWheelRadii.clear();
    if (!storylandLoadPlayTestPopulation(archive,player.model,player.name,
        pedestrianAssets,vehicleAssets,error)) {statusMessage=error;return false;}
    if(!storylandLoadServicePedestrians(archive,pedestrianAssets,&error)) {
        statusMessage=error;
        return false;
    }
    std::set<int32_t> populationModels;
    for(const auto& asset:pedestrianAssets)if(asset.pedStats.valid)
        populationModels.insert(int32_t(asset.pedStats.modelId));
    if(!storylandPreparePedModelAnimations(archive,populationModels,{},
        playerAnimation,pedestrianAnimation,error)) {statusMessage=error;return false;}
    for(const auto& asset:pedestrianAssets) {
        runtime.pedestrianModelStats.push_back(asset.pedStats);
        runtime.pedestrianModelRoles.push_back(storylandServicePedestrianRole(asset.name,vcs(),asset.pedStats));
        if(asset.pedStats.valid)environment.residentModels.insert(int32_t(asset.pedStats.modelId));
    }
    if(!storylandDecodePedestrianGroups(archive.unpackedBytes(),vcs(),
        runtime.lcsPedestrianGroups,error)) {statusMessage=error;return false;}
    auto ambientVehicles=std::move(vehicleAssets);vehicleAssets.clear();
    for(auto& asset:ambientVehicles)registerVehicleAsset(std::move(asset));
    runtime.pedestrianWalkSpeed=pedestrianAnimation.locomotionSpeed(StorylandPlayTestMotion::Walk);
    runtime.pedestrianRunSpeed=pedestrianAnimation.locomotionSpeed(StorylandPlayTestMotion::Run);
    playerBlend = {};
    playerPosition = {}; playerHeading = 0;
    animationTime = 0; millisecondRemainder = 0;
    bindEnvironment();
    if(zombiePedestrians&&!storylandSetZombiePedestrians(runtime,archive,pedestrianAssets,
        pedestrianAnimation,true,error)) {statusMessage=error;return false;}
    if(!StorylandVcsFerrisWheel::configure(environment,archive.unpackedBytes(),vcs(),error)) {
        statusMessage=error;return false;
    }
    std::vector<uint32_t> offsets;
    for (const auto& entry : script.missions()) offsets.push_back(entry.binaryOffset);
    if (!program.start(script.rawBytes(), script.header().mainScriptOffset, offsets, mission, environment, vcs())) {
        error = program.status(); statusMessage = error; return false;
    }
    storylandConfigureIntroMission(environment,script,vcs(),true);
    mission.environment = &environment;
    running = playingGame = true;
    paused = false;
    storylandResetStoriesCamera(camera, playerPosition, playerHeading);
    statusMessage = "Play Game: running native MAIN.SCM.";
    if(!escalatorAssetStatus.empty())statusMessage+=" "+escalatorAssetStatus;
    error.clear();return true;
}

bool StorylandLinuxSession::setZombiePedestrians(bool enabled,std::string& error) {
    if(enabled&&(!worldLoaded||!vcs())) {
        error="Zombie Pedestrians requires Vice City Stories and its Brawn of the Dead assets.";
        statusMessage=error;return false;
    }
    if(!running) {
        runtime.zombiePedestrians=enabled;
        error.clear();return true;
    }
    if(!storylandSetZombiePedestrians(runtime,archive,pedestrianAssets,pedestrianAnimation,enabled,error)) {
        statusMessage=error;return false;
    }
    for(auto& blend:pedestrianBlends)blend={};
    return true;
}

bool StorylandLinuxSession::startTest(std::string& error) {
    if (!startGame(error, false)) return false;
    program.stop(); playingGame = false;
    if(vcs() && !storylandInitializeFreeplayScm(script,true,environment,error)) {
        fail(error);return false;
    }
    storylandConfigureIntroMission(environment,script,vcs(),false);
    std::string restartCatalogWarning;
    if(!storylandLoadPlayerRestartDefinitions(environment,script.rawBytes(),script.header().mainScriptOffset,
        script.header().mainScriptSize,vcs(),error)) {
        restartCatalogWarning=error;
        error.clear();
    }
    mission.state = StorylandMissionState::Idle;
    environment.control = true;
    const StorylandNativePoint origin = vcs() ? StorylandNativePoint{-1593.0f,-143.0f,14.0f} :
        StorylandNativePoint{1215.0f,-587.0f,16.0f};
    StorylandPlayTestSpawn spawn;
    if (!archive.ensureNativeCollision(origin,error) ||
        !storylandFindPlayTestSpawn(archive.nativeWorld(),origin,spawn)) {
        if (error.empty()) error = "No walkable native pedestrian path with ground collision was found.";
        fail(error);return false;
    }
    playerPosition = spawn.position;
    playerHeading = spawn.heading;
    environment.x = playerPosition.x;
    environment.y = playerPosition.y;
    environment.z = playerPosition.z;
    environment.heading = playerHeading-90;
    runtime.playerPosition = playerPosition;
    environment.currentWorldLevel=archive.nativeWorld().zones.worldLevelAt(playerPosition,environment.currentWorldLevel);
    storylandResetStoriesCamera(camera, playerPosition, playerHeading);
    statusMessage = "Play Test: native world and player simulation.";
    if(!restartCatalogWarning.empty())
        statusMessage += " Restart points unavailable: " + restartCatalogWarning;
    error.clear();return true;
}

bool StorylandLinuxSession::canStartDyom(std::string* error) const {
    if (error) error->clear();
    const auto reject = [&](const char* reason) {
        if (error) *error = reason;
        return false;
    };
    if (!worldLoaded || !vcs()) return reject("Open Vice City Stories game data to play a DYOM mission.");
    if (running && playingGame) return reject("Stop Play Game before testing an authored mission.");
    if (network && network->connected()) return reject("Disconnect co-op before testing an authored mission.");
    return true;
}

bool StorylandLinuxSession::startDyom(const StorylandDyomProject& project, std::string& error) {
    if (!canStartDyom(&error)) return false;
    StorylandDyomCompiled compiled;
    if (!storylandCompileDyomVcs(project, compiled, error)) return false;
    for (const auto& requirement : compiled.requiredModels) {
        if (!storylandNativeModelAvailable(archive, requirement.model)) {
            error = "Entity " + requirement.entityId + " uses unavailable native model " +
                std::to_string(requirement.model) + ".";
            return false;
        }
        const bool pedestrian = storylandDecodePedStats(archive.unpackedBytes(),
            uint32_t(requirement.model), true).valid;
        const bool vehicle = storylandVehicleDomain(archive.unpackedBytes(),
            uint32_t(requirement.model), true) != StorylandVehicleDomain::Unknown;
        if ((requirement.kind == StorylandDyomEntityKind::Actor && !pedestrian) ||
            (requirement.kind == StorylandDyomEntityKind::Vehicle && !vehicle) ||
            ((requirement.kind == StorylandDyomEntityKind::Object ||
              requirement.kind == StorylandDyomEntityKind::Pickup) && (pedestrian || vehicle))) {
            error = "Entity " + requirement.entityId + " has a model of the wrong type for " +
                storylandDyomEntityKindName(requirement.kind) + ".";
            return false;
        }
    }
    const auto validateWeapons = [&]() {
        for (int32_t weapon : compiled.requiredWeapons) {
            if (std::none_of(runtime.weapons.begin(), runtime.weapons.end(),
                [weapon](const auto& definition) { return definition.nativeId == weapon; })) {
                error = "The loaded game does not provide weapon " + std::to_string(weapon) + ".";
                return false;
            }
        }
        return true;
    };
    if (running && !validateWeapons()) return false;
    const auto& start = project.player.position;
    if (!archive.ensureNativeCollision({start.x, start.y, start.z}, error)) return false;
    for (const auto& entity : project.entities) {
        if (entity.kind == StorylandDyomEntityKind::Object &&
            !archive.ensureNativeCollision({entity.position.x, entity.position.y, entity.position.z}, error))
            return false;
    }
    if (!startTest(error)) return false;
    if (!validateWeapons()) {
        stopGame();
        return false;
    }
    for (const auto& requirement : compiled.requiredModels)
        environment.requestedModels.insert(requirement.model);
    if (!prepareRequestedModels(error)) {
        stopGame();
        return false;
    }
    if (preloadWorldScene && !preloadWorldScene({start.x, start.y, start.z}, error)) {
        stopGame();
        return false;
    }
    for (const auto& requirement : compiled.requiredModels) {
        if (requirement.kind != StorylandDyomEntityKind::Object) continue;
        const auto& models = archive.nativeWorld().collisionModels;
        const auto collision = models.find(uint32_t(requirement.model));
        if (collision == models.end() || !std::isfinite(collision->second.minimum.z)) {
            error = "Object " + requirement.entityId + " requires decoded native collision for model " +
                std::to_string(requirement.model) + ".";
            stopGame();
            return false;
        }
    }
    storylandCaptureMissionWeapons(runtime, environment);
    dyomMission = std::move(compiled);
    dyomSession = true;
    if (!storylandStartDyomVcs(dyomMission, mission, environment, error)) {
        stopGame();
        return false;
    }
    paused = false;
    runtime.pauseMenu = false;
    statusMessage = "DYOM: " + project.title;
    error.clear();
    return true;
}

void StorylandLinuxSession::stopDyom() {
    if (!dyomSession) return;
    stopGame();
}

std::string StorylandLinuxSession::dyomStatus() const {
    if (!dyomSession) return {};
    const std::string prefix = "DYOM: " + dyomMission.title;
    if (mission.state == StorylandMissionState::Fault || mission.state == StorylandMissionState::Unsupported)
        return prefix + " | " + mission.diagnostic;
    if (mission.missionOutcome == StorylandMissionOutcome::Failed) return prefix + " | Mission failed";
    if (mission.missionOutcome == StorylandMissionOutcome::Passed) return prefix + " | Mission passed";
    const int objective = dyomMission.objectiveAt(mission.pc);
    if (objective >= 0 && size_t(objective) < dyomMission.objectives.size()) {
        const auto& step = dyomMission.objectives[size_t(objective)];
        const auto text = dyomMission.text.find(step.textKey);
        return prefix + " | " + std::to_string(objective + 1) + "/" +
            std::to_string(dyomMission.objectives.size()) +
            (text == dyomMission.text.end() ? "" : ": " + text->second);
    }
    return prefix + (mission.state == StorylandMissionState::Finished ? " | Finished" : " | Preparing mission");
}

bool StorylandLinuxSession::updateCoopCharacter() {
    if (!network->connected()) {
        if (coopOriginalPlayer) {
            player = std::move(*coopOriginalPlayer);
            coopOriginalPlayer.reset();
            playerBlend = {};
            playerPoints.clear();
            playerAnimation.configureCombatDefinitions(runtime.weapons, &player.model);
        }
        coopRemoteCharacter.reset();
        coopLocalCharacterName.clear();
        coopRemoteCharacterName.clear();
        return true;
    }
    const std::string guestName = network->guestModelName(vcs());
    if (!network->isHost() && coopLocalCharacterName != guestName) {
        StorylandPlayTestAsset replacement;
        std::string error;
        if (!storylandLoadCoopCharacter(archive, guestName, replacement, error)) {
            statusMessage = error;
            return false;
        }
        if (!coopOriginalPlayer) coopOriginalPlayer = std::make_unique<StorylandPlayTestAsset>(std::move(player));
        player = std::move(replacement);
        coopLocalCharacterName = guestName;
        playerBlend = {};
        playerPoints.clear();
        playerAnimation.configureCombatDefinitions(runtime.weapons, &player.model);
    }
    std::string remoteName = guestName;
    if (!network->isHost()) {
        size_t entry = 0;
        if (network->remote.model >= 0 && archive.findModelDirEntryByModelId(uint32_t(network->remote.model), entry, remoteName))
            remoteName = std::filesystem::path(remoteName).stem().u8string();
        else if (coopOriginalPlayer) remoteName = std::filesystem::path(coopOriginalPlayer->name).stem().u8string();
        else remoteName = std::filesystem::path(player.name).stem().u8string();
    }
    if (coopRemoteCharacterName != remoteName) {
        auto replacement = std::make_unique<StorylandPlayTestAsset>();
        std::string error;
        if (!storylandLoadCoopCharacter(archive, remoteName, *replacement, error)) {
            statusMessage = error;
            return false;
        }
        coopRemoteCharacter = std::move(replacement);
        coopRemoteCharacterName = remoteName;
    }
    return true;
}

const StorylandPlayTestAsset* StorylandLinuxSession::coopCharacterAsset() {
    return coopRemoteCharacter.get();
}

void StorylandLinuxSession::stopGame() {
    std::string dyomCleanupError;
    if (dyomSession) {
        storylandAbortDyomVcs(dyomMission, mission, dyomCleanupError);
        for (const auto& entry : environment.missionVehicles)
            if (entry.second.deleted) storylandRemoveMissionVehicle(runtime, vehicles, environment, entry.first);
    }
    disconnectCoop();
    updateCoopCharacter();
    archive.bindScriptCollisionWorld(nullptr);
    running = playingGame = false;
    dyomSession = false;
    dyomMission = {};
    paused = false;
    program.stop(); cutscene.clear();
    environment.introMissionProtected = environment.introMissionRestartQueued = false;
    const bool modelsChanged=std::any_of(archive.nativeWorld().instances.begin(),
        archive.nativeWorld().instances.end(),[](const auto& instance) {
            return instance.modelId!=instance.originalModelId;
        });
    archive.resetBuildingSwaps();
    storylandDisableEscalators(runtime.escalators,archive.nativeWorld());
    for(const auto& area:areas)area->resetBuildingSwaps();
    environment.buildingModelSwaps.clear();
    environment.buildingModelSwapsChanged=false;
    environment.buildingSwapCommands.clear();
    environment.invisibleNativeObjects.clear();
    environment.nativeObjectVisibilityChanged=false;
    environment.cutscene = {};
    storylandClearPlayTestTarget(runtime);
    mission.state = StorylandMissionState::Idle;
    nativeSceneryNeedsRefresh=nativeSceneryNeedsRefresh||modelsChanged;
    if(nativeSceneryNeedsRefresh) {
        std::string error;
        if(!refreshNativeScenery(error)) {fail(error);return;}
    }
    for(const auto& area:areas)area->synchronizeNativeSceneryTransforms(archive.nativeWorld());
    statusMessage = worldLoaded ? "Stopped." : "Open GAME.DTZ or a game directory.";
    if (!dyomCleanupError.empty()) statusMessage = dyomCleanupError;
}

bool StorylandLinuxSession::restartIntroMission(std::string& error) {
    const int32_t slot = environment.introMissionSlot;
    if(!program.active() || slot < 0 || slot!=storylandFindIntroMissionSlot(script,vcs()) ||
        !storylandPrepareIntroMissionRestart(runtime,environment,playerPosition,vcs(),error)) return false;
    playerPosition={environment.x,environment.y,environment.z};
    playerHeading=environment.heading+90.0f;
    cutscene.discard(environment);
    playerBlend = {};
    playerMotion = StorylandPlayTestMotion::Idle;
    if(!program.restartMission(slot,mission)) {
        error = program.status();
        if(error.empty()) error = "The native intro mission could not restart.";
        return false;
    }
    storylandResetStoriesCamera(camera,playerPosition,playerHeading);
    statusMessage = "Restarted the native intro mission.";
    resetCoopAfterRestart();
    error.clear();
    return true;
}

bool StorylandLinuxSession::updatePlayerRestart() {
    if (dyomSession && (mission.state == StorylandMissionState::Running ||
        mission.state == StorylandMissionState::Waiting)) return true;
    auto& state=runtime.playerRestart;
    const auto cause=environment.criticalMissionRestartRequested?StorylandPlayerRestartCause::Critical:
        runtime.busted?StorylandPlayerRestartCause::Arrest:
        runtime.health<=0?StorylandPlayerRestartCause::Death:
        environment.introMissionRestartQueued?StorylandPlayerRestartCause::Critical:StorylandPlayerRestartCause::None;
    if(cause!=StorylandPlayerRestartCause::None&&state.canBegin()&&
        storylandBeginPlayerRestart(runtime,environment,state,cause)) {
        if(!state.introMissionRestart && program.active())program.failMission(mission,cause==StorylandPlayerRestartCause::Arrest?
            "Mission failed: Busted.":"Mission failed: Wasted.");
        else if(!state.introMissionRestart)mission.handleDeathArrest(cause==StorylandPlayerRestartCause::Arrest?
            StorylandMissionFailure::Arrest:StorylandMissionFailure::Death);
        cutscene.discard(environment);
        environment.camera.active=environment.camera.cutsceneOwned=false;
        environment.camera.restoreRequested=true;environment.widescreen=false;
    }
    if(!state.active())return true;
    const auto step=storylandStepPlayerRestart(state,environment.gameMilliseconds);
    if(step.resurrectionRequested) {
        if(state.introMissionRestart) {
            std::string error;
            if(!restartIntroMission(error)) fail(error);
            return false;
        }
        StorylandPlayerRestartPoint point;std::string error;
        if(!storylandApplyPlayerRestart(runtime,environment,vehicles,state,playerPosition,vcs(),true,point,error)) {
            statusMessage=error;return false;
        }
        playerPosition=point.position;playerHeading=point.heading;playerBlend={};
        playerMotion=StorylandPlayTestMotion::Idle;
        storylandResetStoriesCamera(camera,playerPosition,playerHeading);
    }
    if(step.controlReleased)storylandReleasePlayerRestartControl(runtime,environment);
    storylandApplyPlayerRestartPresentation(runtime,environment,state);
    return true;
}

void StorylandLinuxSession::fail(const std::string& error) {
    statusMessage = error;
    mission.state = StorylandMissionState::Unsupported;
    mission.diagnostic = error;
    running = false;
}

void StorylandLinuxSession::registerVehicleAsset(StorylandPlayTestAsset asset) {
    const size_t index=vehicleAssets.size();
    playerAnimation.configureVehicleAnimations(asset.vehicleDefinition);
    runtime.modelDefinitions.push_back(asset.vehicleDefinition);
    runtime.modelServiceRoles.push_back(storylandServiceVehicleRole(asset.name));
    runtime.modelNames.push_back(storylandServiceModelStem(asset.name));
    float halfWidth=.1f,halfLength=.1f,minZ=1e9f,maxZ=-1e9f;
    for(const auto& point:asset.model.previewPoints()) {
        if(std::isfinite(point.x))halfWidth=std::max(halfWidth,std::fabs(point.x));
        if(std::isfinite(point.y))halfLength=std::max(halfLength,std::fabs(point.y));
        if(std::isfinite(point.z)) {minZ=std::min(minZ,point.z);maxZ=std::max(maxZ,point.z);}
    }
    runtime.modelExtents.push_back({std::clamp(halfWidth,.2f,12.0f),
        std::clamp(halfLength,.5f,18.0f),std::clamp(asset.vehicleDefinition.bodyRoofHeight(maxZ-minZ),.5f,10.0f)});
    runtime.modelWheelRadii.push_back(storylandDecodedWheelRadius(asset.model.previewPoints(),asset.model.previewTriangles()));
    if(asset.vehicleDomain==StorylandVehicleDomain::Road)runtime.roadModels.push_back(index);
    if(asset.vehicleDomain==StorylandVehicleDomain::Water)runtime.waterModels.push_back(index);
    environment.residentModels.insert(int32_t(asset.vehicleDefinition.modelId));
    vehicleAssets.push_back(std::move(asset));
}

bool StorylandLinuxSession::constructMissionVehicle(int32_t handle, std::string& error) {
    const auto found = environment.missionVehicles.find(handle);
    if (found == environment.missionVehicles.end() || found->second.deleted || found->second.model < 0) {
        error = "Physical construction requires a live native script vehicle.";
        return false;
    }
    auto& state = found->second;
    if (state.spawnedSlot >= 0 && size_t(state.spawnedSlot) < vehicles.size() &&
        size_t(state.spawnedSlot) < runtime.vehicles.size() &&
        runtime.vehicles[size_t(state.spawnedSlot)].scriptHandle == handle) {
        return true;
    }
    for (size_t slot = 0; slot < std::min(vehicles.size(), runtime.vehicles.size()); ++slot) {
        if (runtime.vehicles[slot].scriptHandle == handle) {
            state.spawnedSlot = int32_t(slot);
            return true;
        }
    }
    state.spawnedSlot = -1;
    auto definition = std::find_if(runtime.modelDefinitions.begin(), runtime.modelDefinitions.end(),
        [&](const auto& item) { return item.modelId == uint32_t(state.model); });
    if (definition == runtime.modelDefinitions.end()) {
        StorylandPlayTestAsset asset;
        if (!storylandLoadPlayTestVehicle(archive, uint32_t(state.model), asset, error)) return false;
        registerVehicleAsset(std::move(asset));
        definition = std::find_if(runtime.modelDefinitions.begin(), runtime.modelDefinitions.end(),
            [&](const auto& item) { return item.modelId == uint32_t(state.model); });
    }
    if (definition == runtime.modelDefinitions.end()) {
        error = "Script vehicle has no decoded native definition.";
        return false;
    }
    StorylandNativeTrafficActor actor;
    actor.vehicle = true;
    actor.water = definition->domain == StorylandVehicleDomain::Water;
    actor.position = {state.x, state.y, state.z};
    actor.yaw = state.heading + 90;
    StorylandRuntimeVehicle vehicle;
    vehicle.scriptHandle = handle;
    vehicle.model = size_t(definition - runtime.modelDefinitions.begin());
    storylandApplyVehicleDefinition(vehicle, *definition);
    vehicle.health = state.health;
    vehicle.parked = state.scriptStatus < 0 || state.scriptStatus == 4 || state.scriptStatus == 5;
    vehicle.hasDriver = state.driver >= 0 && state.aiActive;
    if (vehicle.model < runtime.modelExtents.size()) {
        const auto& extents = runtime.modelExtents[vehicle.model];
        vehicle.halfWidth = extents[0];
        vehicle.halfLength = extents[1];
        vehicle.bodyHeight = extents[2];
    }
    storylandApplyMissionVehicleOverrides(vehicle, actor, environment, handle);
    storylandApplyMissionVehicleTransform(vehicle, actor, state, environment);
    vehicle.positionFrozen = state.positionFrozen;
    vehicle.collisionEnabled = state.collisionEnabled;
    vehicle.loadCollision = state.loadCollision;
    vehicle.visible = state.visible;
    vehicle.renderAlpha = state.renderAlpha;
    vehicle.canBeDamaged = state.canBeDamaged;
    vehicle.onlyDamagedByPlayer = state.onlyDamagedByPlayer;
    vehicle.damageProofs = state.damageProofs;
    vehicle.siren = state.siren;
    vehicle.doorLock = state.doorLock;
    vehicle.wrecked = state.wrecked;
    if (state.primaryColor >= 0) vehicle.primaryColor = state.primaryColor;
    if (state.secondaryColor >= 0) vehicle.secondaryColor = state.secondaryColor;
    vehicles.reserve(runtime.vehicles.size());
    if(vcs())storylandClearMissionVehicleSpace(runtime, vehicles, environment, archive.nativeWorld(), handle);
    int32_t inheritedGenerator = -1;
    // VCS constructs during CREATE_CAR, before the separate SET_CAR_HEADING opcode.
    size_t slot = storylandFindMissionGeneratorVehicle(runtime, vehicles, environment, state,
        inheritedGenerator, !vcs());
    if (slot >= runtime.vehicles.size())
        slot = storylandMissionVehicleSlot(runtime, vehicles, environment, playerPosition);
    if (slot >= runtime.vehicles.size()) {
        error = "Native vehicle pool is full.";
        return false;
    }
    storylandBindMissionCarGenerator(runtime, vehicles, environment, inheritedGenerator, slot);
    vehicle.generator = inheritedGenerator;
    if (slot == vehicles.size()) vehicles.push_back(actor);
    else vehicles[slot] = actor;
    runtime.vehicles[slot] = std::move(vehicle);
    state.spawnedSlot = int32_t(slot);
    for (auto& contact : runtime.vehicles) {
        if (contact.lastCollidedSlot == int32_t(slot)) contact.lastCollidedSlot = -1;
        if (contact.pendingContactSlot == int32_t(slot)) {
            contact.pendingContactSlot = -1;
            contact.pendingBodyImpact = false;
        }
    }
    error.clear();
    return true;
}

bool StorylandLinuxSession::prepareRequestedModels(std::string& error) {
    std::set<int32_t> requested = environment.requestedModels;
    requested.insert(environment.empireRequestedModels.begin(),environment.empireRequestedModels.end());
    for(const auto& entity:environment.animatedEmpireEntities)if(entity.model>=0)requested.insert(entity.model);
    for (const auto& entry : environment.missionCharacters) if(entry.second.alive) requested.insert(entry.second.model);
    for (const auto& entry : environment.missionVehicles) if(!entry.second.deleted) requested.insert(entry.second.model);
    for (const auto& pickup : environment.pickups)
        if (!pickup.removed && !pickup.unavailable && pickup.type != 0 && pickup.model >= 0 &&
            pickup.modelNameOverride.empty() && std::hypot(pickup.x-playerPosition.x,pickup.y-playerPosition.y)<180)
            requested.insert(pickup.model);
    const auto requiredModels=requested;
    for(const auto& object:environment.objects)requested.insert(object.model);
    for (int32_t changed : environment.changedSpecialModels) {
        missionAssets.erase(changed);compiledWorldAssets.erase(changed);environment.residentModels.erase(changed);
    }
    environment.changedSpecialModels.clear();
    if (!storylandPreparePedModelAnimations(archive, requested, environment.residentModels,
        playerAnimation, pedestrianAnimation, error)) return false;
    for (int32_t model : requested) {
        if(model<0)continue;
        if(environment.residentModels.count(model)&&(missionAssets.count(model)||compiledWorldAssets.count(model)||
            std::any_of(runtime.modelDefinitions.begin(),runtime.modelDefinitions.end(),
                [model](const auto& definition){return definition.modelId==uint32_t(model); })))continue;
        if (!environment.specialModelNames.count(model) &&
            storylandVehicleDomain(archive.unpackedBytes(), uint32_t(model), vcs()) != StorylandVehicleDomain::Unknown) {
            StorylandPlayTestAsset asset;
            if (storylandLoadPlayTestVehicle(archive, uint32_t(model), asset, error)) {
                registerVehicleAsset(std::move(asset));
                environment.residentModels.insert(model); continue;
            }
        }
        std::string name;
        const auto special = environment.specialModelNames.find(model);
        if (special != environment.specialModelNames.end()) name = special->second;
        else { size_t index=0; archive.findModelDirEntryByModelId(uint32_t(model),index,name); }
        std::vector<StorylandPlayTestAsset> assets;
        if (name.empty() || !storylandLoadPlayTestModels(archive,{name},assets,error) || assets.empty()) {
            // Native world props can live solely in master WRLD resources;
            // successful residency requires their decoded geometry, not a DIR name.
            bool loaded=false;std::string compiledError;
            if(special==environment.specialModelNames.end()&&storylandNativeModelAvailable(archive,model))
                for(size_t source=0;source<areas.size();++source) {
                    CompiledWorldAsset asset;asset.sourceArea=source;
                    if(!areas[source]->decodeLcsDynamicModel(uint32_t(model),asset.geometry,compiledError))continue;
                    compiledWorldAssets[model]=std::move(asset);
                    environment.residentModels.insert(model);loaded=true;break;
                }
            if(loaded) {error.clear();continue;}
            // Native0092 constructs with SetModelIndexNoCreate. An unrequested
            // object can retain collision and blips without a resident RW object.
            if(vcs()&&!requiredModels.count(model)&&special==environment.specialModelNames.end()&&
                storylandNativeModelAvailable(archive,model)) {error.clear();continue;}
            if(error.empty())error=compiledError;
            error = "Requested native model " + std::to_string(model) + " (" + name + ") could not be decoded: " + error;
            return false;
        }
        missionAssets.emplace(model,std::move(assets.front()));
        environment.residentModels.insert(model);
    }
    for(const auto& pickup:environment.pickups) {
        if(pickup.removed || pickup.unavailable || pickup.type==0 ||
            (pickup.modelNameOverride.empty() && pickup.model>=0) ||
            std::hypot(pickup.x-playerPosition.x,pickup.y-playerPosition.y)>=180)continue;
        const int32_t key=pickup.modelNameOverride.empty()?pickup.model:INT32_MIN+std::max(0,pickup.handle);
        if(missionAssets.count(key))continue;
        std::vector<StorylandPlayTestAsset> assets;
        std::string pickupError;
        const auto name=pickup.modelNameOverride.empty()?script.scriptObjectName(pickup.model):pickup.modelNameOverride;
        if(!name.empty() && storylandLoadPlayTestModels(archive,{name},assets,pickupError) && !assets.empty())
            missionAssets.emplace(key,std::move(assets.front()));
    }
    for (const auto& bank : environment.requestedAnimationBanks)
        if (!playerAnimation.loadAnimationBank(archive,bank,error) ||
            !pedestrianAnimation.loadAnimationBank(archive,bank,error)) return false;
    for (int32_t group : environment.requestedAnimationGroups)
        if (!playerAnimation.loadAnimationGroup(archive,group,error) ||
            !pedestrianAnimation.loadAnimationGroup(archive,group,error)) return false;
    for(const auto& bank:environment.lcsAnimationBanks)
        if(!environment.setLcsAnimationBankLoaded||!environment.setLcsAnimationBankLoaded(bank,true,error))return false;
    return true;
}

void StorylandLinuxSession::bindEnvironment() {
    environment.constructMissionVehicle = [this](int32_t handle, std::string& error) {
        return constructMissionVehicle(handle, error);
    };
    environment.lcsExtendedWorld.objectAttachmentMatrix=[this](int32_t handle,int32_t bone,
        const std::string& component,std::array<float,16>& matrix) {
        return cutscene.actorAttachmentMatrix(archive,environment,handle,bone,component,matrix);
    };
    environment.lcsExtendedWorld.clearNativeObjects=[this](const std::array<float,6>& bounds,std::string& error) {
        return storylandLcsClearNativeObjects(environment,archive.nativeWorld(),archive.unpackedBytes(),bounds,error);
    };
    environment.lcsExtendedWorld.nearestGlassShattered=[this](const std::array<float,3>& point,bool& shattered) {
        return storylandLcsNearestGlassShattered(environment,archive.nativeWorld(),archive.unpackedBytes(),point,shattered);
    };
    environment.lcsExtendedWorld.playerTouchesObject=[this](int32_t object) {
        return storylandLcsPlayerTouchesObject(runtime,archive.nativeWorld(),vehicles,object);
    };
    environment.nativeObjectBreakableGlass=[this](int32_t model) {
        return storylandObjectIsBreakableGlass(archive.unpackedBytes(),model);
    };
    environment.lcsExtendedWorld.collisionWaiting=[this](int32_t kind,int32_t handle,bool& waiting) {
        return storylandLcsCollisionWaiting(environment,archive.nativeWorld(),kind,handle,waiting);
    };
    environment.lcsExtendedWorld.wantedStarsFlashing=[this] {return storylandLcsWantedStarsFlashing(runtime);};
    environment.lcsExtendedWorld.suspendWanted=[this] {storylandSuspendLcsWanted(runtime);};
    environment.lcsExtendedWorld.crusherVehicleGeometry=[this](int32_t handle,std::array<float,3>& origin,
        std::vector<std::array<float,4>>& spheres,uint32_t& monetaryValue,int32_t& nativeType,std::string& error) {
        return storylandLcsCrusherVehicleGeometry(environment,runtime,archive.unpackedBytes(),handle,
            origin,spheres,monetaryValue,nativeType,error);
    };
    environment.lcsExtendedWorld.crusherVehiclePoolCapacity=[this] {return runtime.vehicles.size();};
    environment.lcsExtendedWorld.crusherPlayerDamage=[this] {runtime.inflictDamage(300,8,39,true);};
    environment.vehicleFlightAvailable=[this](int32_t handle) {
        const auto found=environment.missionVehicles.find(handle);
        return found!=environment.missionVehicles.end()&&!found->second.deleted&&
            storylandDecodeVehicleDefinition(archive.unpackedBytes(),uint32_t(found->second.model),vcs()).hasNativeFlyingHandling;
    };
    environment.lcsExtendedWorld.playMusic=[this](uint8_t track,bool playing,bool looping,std::string& error) {
        if(playing&&environment.audioSubsystemEnabled&&
            (!playMissionTune||!playMissionTune(track,looping,error)))return false;
        if(!playing&&stopMissionTune)stopMissionTune();
        environment.missionPassedTune=playing?track:-1;
        environment.missionTuneLoop=playing&&looping;
        ++environment.missionPassedTuneSerial;
        environment.scriptAudioMode=playing?0:1;
        error.clear();return true;
    };
    environment.startKillFrenzy=[this](const std::string& text,int32_t weapon,int32_t time,
        int32_t kills,const std::array<int32_t,4>& models,bool completed,bool headshots,std::string& error) {
        return storylandStartKillFrenzy(runtime,text,weapon,time,kills,models,completed,headshots,false,error);
    };
    environment.lcsMissionFlagSet=[this] {
        const auto& globals=program.active()?program.persistentSaveGlobals():mission.globals;
        const int32_t flag=environment.declaredMissionFlag;
        return flag>=0&&size_t(flag)<globals.size()&&globals[size_t(flag)]==1;
    };
    environment.nativeTimeScaleSource=[this] {
        return storylandAdrenalineTimeScale(runtime,environment.nativeTimeScale);
    };
    environment.pagerText = [this](const std::string& key) {
        std::u16string text;
        if (key.empty()) return text;
        if (dyomSession) {
            const auto found = dyomMission.text.find(key);
            if (found != dyomMission.text.end()) {
                try {
                    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
                    return converter.from_bytes(found->second);
                } catch (const std::range_error&) {
                    return std::u16string(u"Invalid mission text");
                }
            }
        }
        if (!missionGxt.lookup(key, environment.missionText, text))
            text.assign(key.begin(), key.end());
        return text;
    };
    environment.findScriptVehiclesInArea=[this](const std::array<float,4>& bounds,int32_t model,uint32_t flags) {
        using World=StorylandMissionEnvironment;
        std::vector<int32_t> result;
        const auto inside=[&](float x,float y) {
            return x>=bounds[0]&&x<=bounds[2]&&y>=bounds[1]&&y<=bounds[3];
        };
        for(auto entry=environment.missionVehicles.rbegin();entry!=environment.missionVehicles.rend();++entry) {
            const auto& vehicle=entry->second;
            if(vehicle.spawnedSlot>=0||vehicle.deleted||!inside(vehicle.x,vehicle.y)||
                (model>=0&&vehicle.model!=model)||
                ((flags&World::VehicleAreaAmbient)&&vehicle.persistent)||
                ((flags&World::VehicleAreaAlive)&&vehicle.health<=0))continue;
            if((flags&World::VehicleAreaRoad)&&
                (vehicle.model<0||storylandDecodeVehicleDefinition(archive.unpackedBytes(),
                    uint32_t(vehicle.model),vcs()).domain!=StorylandVehicleDomain::Road))continue;
            result.push_back(entry->first);
        }
        for(size_t count=std::min(vehicles.size(),runtime.vehicles.size());count>0;--count) {
            const size_t slot=count-1;
            auto& vehicle=runtime.vehicles[slot];
            const auto& actor=vehicles[slot];
            const auto scripted=environment.missionVehicles.find(vehicle.scriptHandle);
            if(scripted!=environment.missionVehicles.end()&&scripted->second.deleted)continue;
            if((flags&World::VehicleAreaAmbient)&&(vehicle.generator>=0||vehicle.savedGarage>=0||
                (vehicle.scriptHandle>=0&&(scripted==environment.missionVehicles.end()||scripted->second.persistent))))continue;
            if((flags&World::VehicleAreaRoad)&&vehicle.nativeDefinition.domain!=StorylandVehicleDomain::Road)continue;
            if((flags&World::VehicleAreaAlive)&&vehicle.health<=0)continue;
            const int32_t id=int32_t(vehicle.nativeDefinition.modelId);
            if(model>=0&&model!=id)continue;
            const bool moved=scripted!=environment.missionVehicles.end()&&scripted->second.movedByScript;
            const float x=moved?scripted->second.x:actor.position.x;
            const float y=moved?scripted->second.y:actor.position.y;
            if(!inside(x,y))continue;
            if(scripted!=environment.missionVehicles.end()) {result.push_back(scripted->first);continue;}
            if(environment.nextMissionVehicleHandle==INT32_MAX)break;
            const int32_t handle=environment.nextMissionVehicleHandle++;
            World::VehicleScriptState state;
            state.model=id;state.spawnedSlot=int32_t(slot);state.x=actor.position.x;
            state.y=actor.position.y;state.z=actor.position.z;state.heading=actor.yaw-90;
            state.health=vehicle.health;state.alive=vehicle.health>0;state.speed=vehicle.speed;
            state.persistent=false;
            state.canBeDamaged=vehicle.canBeDamaged;state.onlyDamagedByPlayer=vehicle.onlyDamagedByPlayer;
            state.damageProofs=vehicle.damageProofs;state.heavy=vehicle.heavy;
            environment.missionVehicles.emplace(handle,state);vehicle.scriptHandle=handle;
            result.push_back(handle);
        }
        return result;
    };
    environment.acquireOccupiedVehicle=[this]() -> int32_t {
        const int32_t slot=runtime.occupiedVehicle;
        if(slot<0||size_t(slot)>=runtime.vehicles.size()||size_t(slot)>=vehicles.size())return -1;
        auto& vehicle=runtime.vehicles[size_t(slot)];
        if(vehicle.scriptHandle>=0)return vehicle.scriptHandle;
        if(environment.nextMissionVehicleHandle==INT32_MAX)return -1;
        const int32_t handle=environment.nextMissionVehicleHandle++;
        const auto& actor=vehicles[size_t(slot)];
        StorylandMissionEnvironment::VehicleScriptState state;
        state.model=int32_t(vehicle.nativeDefinition.modelId);state.spawnedSlot=slot;
        state.x=actor.position.x;state.y=actor.position.y;state.z=actor.position.z;
        state.heading=actor.yaw-90;state.health=vehicle.health;state.speed=vehicle.speed;
        state.alive=vehicle.health>0;state.persistent=false;state.ambientOrigin=true;
        state.canBeDamaged=vehicle.canBeDamaged;state.onlyDamagedByPlayer=vehicle.onlyDamagedByPlayer;
        state.damageProofs=vehicle.damageProofs;state.heavy=vehicle.heavy;
        if(!vehicle.nativeDefinition.vcsModelLayout&&vehicle.nativeDefinition.nativeType==1) {
            state.engineOn=vehicle.engineOn;state.autoPilotMission=vehicle.parked?0:1;
            state.cruiseSpeed=vehicle.parked?20.0f*50.0f/60.0f:5.0f;
            state.scriptStatus=0;state.boatAnchored=vehicle.boatAnchored;
            state.boatAnchorHeading=vehicle.boatAnchorHeading;
        }
        if(!environment.missionVehicles.emplace(handle,state).second)return -1;
        vehicle.scriptHandle=handle;
        storylandCaptureMissionVehicleOverrides(vehicle,actor,environment,handle);
        return handle;
    };
    environment.clearRestartTaxi=[this] {storylandClearRestartTaxi(runtime,environment,runtime.restartTaxi);};
    environment.clearVehicleSpawnSpace=[this](int32_t handle) {
        storylandClearMissionVehicleSpace(runtime,vehicles,environment,archive.nativeWorld(),handle);
    };
    environment.characterReactionOwned=[this](int32_t handle) {
        if (vcs() && environment.vcsPedFalls.count(handle)) return true;
        if (storylandMissionBulletReactionActive(runtime,handle)) return true;
        const auto found=environment.missionCharacters.find(handle);
        return found!=environment.missionCharacters.end()&&storylandPlayerReactionHoldsTarget(
            runtime.playerReaction,StorylandReactionTargetKind::Mission,handle,uint32_t(found->second.model));
    };
    environment.characterPairedAction=[this](int32_t handle) {
        const auto& reaction=runtime.playerReaction;
        return handle==0&&reaction.holding()&&reaction.target.kind!=StorylandReactionTargetKind::None;
    };
    environment.characterReceivingPairedAction=[this](int32_t handle) {
        const auto& reaction=runtime.playerReaction;
        const auto found=environment.missionCharacters.find(handle);
        if(found==environment.missionCharacters.end())return false;
        if(storylandPlayerReactionHoldsTarget(reaction,StorylandReactionTargetKind::Mission,
                handle,uint32_t(found->second.model)))return true;
        return reaction.holding()&&reaction.target.kind==StorylandReactionTargetKind::Ambient&&
            reaction.target.model==uint32_t(found->second.model)&&reaction.target.handle>=0&&
            size_t(reaction.target.handle)<runtime.pedestrians.size()&&
            runtime.pedestrians[size_t(reaction.target.handle)].scriptHandle==handle;
    };
    runtime.playerCrouchRootDistance = {};
    if (extendedCrouchAvailable) {
        runtime.playerCrouchRootDistance=[this](bool backward,float begin,float end) {
            return storylandSaCrouchRootDistance(extendedCrouchAnimations,
                backward?"GunCrouchBwd":"GunCrouchFwd",begin,end)*storylandSaCrouchModelScale(player.model);
        };
    }
    runtime.playerDivingAvailable=vcs()&&divingAnimations.available;
    runtime.playerDiveRootDistance=[this](float begin,float end) {return divingAnimations.rootDistance(begin,end);};
    runtime.reactionBindings.duration=[this](int32_t id,int32_t group) {return playerAnimation.retailAnimationDuration(id,group);};
    runtime.reactionBindings.associationFlags=[this](int32_t id,int32_t group,uint16_t& flags) {
        return playerAnimation.retailAnimationAssociationFlags(id,group,flags);
    };
    runtime.reactionBindings.rootDelta=[this](int32_t id,int32_t group,float from,float to) {
        return playerAnimation.retailAnimationRootDelta(player.model,id,group,from,to);
    };
    runtime.reactionBindings.contactTime=[this](int32_t id,int32_t group,uint32_t bone) {
        return playerAnimation.retailAnimationContactTime(player.model,id,group,bone);
    };
    runtime.vehicleExitRootDelta=[this](int32_t id,int32_t group,float from,float to) {
        return playerAnimation.retailAnimationExtractedRootDelta(id,group,from,to);
    };
    runtime.playerRunStopDistance=[this](unsigned variant,float from,float to) {
        return playerAnimation.runStopDistance(variant,from,to);
    };
    environment.applyWantedLevel=[this](int32_t level) {
        storylandSetPlayTestWantedLevel(runtime,std::clamp(level,0,std::clamp(environment.maxWanted,0,6)));
        environment.wanted=runtime.wanted;
    };
    environment.vehicleIsTaxi=[this](int32_t handle) {
        const auto found=environment.missionVehicles.find(handle);
        if(found==environment.missionVehicles.end()||found->second.deleted)return false;
        const int slot=found->second.spawnedSlot;
        if(slot<0||size_t(slot)>=runtime.vehicles.size())return false;
        const auto model=runtime.vehicles[size_t(slot)].model;
        if(model>=runtime.modelNames.size())return false;
        const auto& name=runtime.modelNames[model];
        return name=="taxi"||name=="cabbie"||name=="borgnine";
    };
    environment.vehiclePassengerCapacity=[this](int32_t handle) {
        const auto found=environment.missionVehicles.find(handle);
        if(found==environment.missionVehicles.end()||found->second.deleted)return 0;
        const auto definition=storylandDecodeVehicleDefinition(archive.unpackedBytes(),uint32_t(found->second.model),vcs());
        return storylandNativePassengerCapacity(definition);
    };
    environment.objectWorld = &archive.nativeWorld();
    archive.bindScriptCollisionWorld(&environment);
    environment.buildingSwapHandle=[this](uint32_t hash,int32_t& handle,std::string& error) {
        if(!applyBuildingSwaps(error))return false;
        handle=-1;
        for(const auto& area:areas) {
            handle=storylandBuildingSwapHandle(archive.nativeWorld(),area->buildingSwaps(),hash);
            if(handle>=0)break;
        }
        return true;
    };
    environment.buildingSwapBounds=[this](uint32_t hash,std::array<float,3>& center,float& radius) {
        for(const auto& area:areas)
            if(storylandBuildingSwapBounds(archive.nativeWorld(),area->buildingSwaps(),hash,center,radius))
                return true;
        center={};radius=0;return false;
    };
    environment.particleCameraPosition = [this] {
        return std::array<float,3>{camera.eye.x,camera.eye.y,camera.eye.z};
    };
    environment.createScriptParticles = {};
    environment.createScriptParticles = [this](
        const std::vector<StorylandMissionEnvironment::ScriptParticle>& particles,std::string& error) {
        return storylandCreateScriptParticles(runtime,archive.unpackedBytes(),particles,error);
    };
    runtime.readNativeParticleDescriptor=[this](int32_t type,StorylandNativeParticleDescriptor& descriptor) {
        return storylandReadNativeParticleDescriptor(archive.unpackedBytes(),type,descriptor);
    };
    environment.scriptEmitterAvailable = [this](uint16_t subtype) {
        return storylandScriptEmitterAvailable(archive.unpackedBytes(),subtype,vcs());
    };
    environment.switchDollarParticles = [this](bool) {
        storylandResetDollarParticles(runtime);
    };
    environment.createDollarParticles = [this](uint8_t count,
        const StorylandMissionEnvironment::ScriptParticle& particle,std::string& error) {
        return storylandCreateDollarParticles(runtime,count,particle,error);
    };
    environment.createScriptExplosion = {};
    if(vcs())environment.createScriptExplosion = [this](StorylandMissionEnvironment::ScriptExplosion& explosion,
        std::string& error) {
        return storylandCreatePlayTestExplosion(runtime,archive.nativeWorld(),vehicles,pedestrians,
            playerPosition,explosion,error);
    };
    environment.zones = &archive.nativeWorld().zones;
    environment.pedestrianPaths = &runtime.livePaths;
    environment.pedestrianLineClear = [this](const std::array<float,3>& from,const std::array<float,3>& to) {
        return storylandTraceNativeSegment(archive.nativeWorld(),
            {from[0],from[1],from[2]},{to[0],to[1],to[2]})>=1.0f;
    };
    environment.moveMissionCharacter = [this](int32_t handle,
        const std::array<float,3>& desired,std::array<float,3>& result) {
        return storylandMoveMissionPedestrian(runtime,archive.nativeWorld(),handle,desired,result);
    };
    environment.modelIdByName = [this](const std::string& name) { uint32_t id=0; return archive.findModelIdByName(name,id)?int32_t(id):-1; };
    environment.findPayphone = [this](const std::array<float,2>& target,
        const std::set<uint64_t>& claimed,uint64_t& worldKey,std::array<float,3>& position) {
        uint32_t model=0;
        if(!archive.findModelIdByName("phonebooth1",model))return false;
        float nearest=100.0f;
        bool found=false;
        const auto& instances=archive.nativeWorld().instances;
        for(auto item=instances.rbegin();item!=instances.rend();++item) {
            const auto& instance=*item;
            if(instance.poolHeader!=0x24u||instance.modelId!=model)continue;
            const uint64_t key=(uint64_t(instance.poolHeader)<<32)|instance.poolIndex;
            if(claimed.count(key))continue;
            const float distance=std::hypot(instance.matrix[12]-target[0],instance.matrix[13]-target[1]);
            if(distance>=nearest)continue;
            nearest=distance;
            worldKey=key;
            position={instance.matrix[12],instance.matrix[13],instance.matrix[14]};
            found=true;
        }
        return found;
    };
    environment.resolveObjectModel = [this](int32_t id) { if(id>=0)return id; uint32_t result=0; return archive.findModelIdByName(script.scriptObjectName(id),result)?int32_t(result):-1; };
    environment.setClosestObjectVisibility=[this](const std::array<float,3>& point,float radius,
        int32_t model,bool visible,std::string& error) {
        std::vector<StorylandArchiveBrowser*> browsers;
        for(const auto& area:areas)browsers.push_back(area.get());
        return storylandSetClosestNativeObjectVisibility(environment,archive,browsers,
            point,radius,model,visible,error);
    };
    environment.swapNearestBuilding=[this](const std::array<float,3>& point,float radius,
        int32_t oldModel,int32_t newModel,std::string& error) {
        error.clear();
        const int32_t source=environment.resolveObjectModel(oldModel);
        const int32_t replacement=environment.resolveObjectModel(newModel);
        if(source<0||replacement<0) {
            error="Building swap could not resolve an SCM object model name.";
            return false;
        }
        if(!std::isfinite(radius)||radius<0||
            !std::all_of(point.begin(),point.end(),[](float value) {return std::isfinite(value);})) {
            error="Building swap received an invalid search range.";
            return false;
        }
        const auto& instances=archive.nativeWorld().instances;
        if(instances.empty()) {
            error="Building swap requires loaded native placements.";
            return false;
        }
        const StorylandNativeCollisionInstance* nearest=nullptr;
        const double radiusSquared=double(radius)*radius;
        double distance=4*radiusSquared;
        for(const auto& instance:instances) {
            if((instance.poolHeader!=0x24&&instance.poolHeader!=0x28)||instance.modelId!=uint32_t(source))continue;
            const double x=double(instance.matrix[12])-point[0];
            const double y=double(instance.matrix[13])-point[1];
            const double z=double(instance.matrix[14])-point[2];
            const double candidate=x*x+y*y+z*z;
            if(x*x+y*y>=radiusSquared||candidate>=distance)continue;
            distance=candidate;
            nearest=&instance;
        }
        if(!nearest)return true;
        const uint64_t owner=(uint64_t(nearest->poolHeader)<<32)|nearest->poolIndex;
        if(!environment.buildingModelSwaps.count(owner)&&environment.buildingModelSwaps.size()>=80) {
            error="Native building swap table is full.";
            return false;
        }
        StorylandMissionEnvironment::BuildingModelSwap swap;
        swap.originalModel=nearest->originalModelId;
        swap.replacementModel=uint32_t(replacement);
        swap.position={nearest->matrix[12],nearest->matrix[13],nearest->matrix[14]};
        environment.buildingModelSwaps[owner]=swap;
        environment.buildingModelSwapsChanged=true;
        if(!applyBuildingSwaps(error))return false;
        if(swap.originalModel==swap.replacementModel)environment.buildingModelSwaps.erase(owner);
        return true;
    };
    if(!vcs()) {
        environment.craneCameraPosition=[this] {
            return std::array<float,3>{camera.eye.x,camera.eye.y,camera.eye.z};
        };
        auto& crane=environment.lcsCranes.bindings;
        crane.discover=[this](std::vector<StorylandLcsCrane>& result,std::string& error) {
            return storylandDiscoverLcsCranes(archive.unpackedBytes(),archive.nativeWorld(),result,error);
        };
        crane.readVehicle=[this](int32_t handle,StorylandLcsCraneVehicle& result,std::string& error) {
            error.clear();
            const auto found=environment.missionVehicles.find(handle);
            if(found==environment.missionVehicles.end()||found->second.deleted)return false;
            const auto& scripted=found->second;
            if(scripted.spawnedSlot<0)return false;
            const size_t slot=size_t(scripted.spawnedSlot);
            if(slot>=vehicles.size()||slot>=runtime.vehicles.size()||runtime.vehicles[slot].scriptHandle!=handle)
                return false;
            const auto& vehicle=runtime.vehicles[slot];
            const auto& actor=vehicles[slot];
            const auto& definition=vehicle.nativeDefinition;
            result={};result.handle=handle;result.model=int32_t(definition.modelId);
            result.automobile=definition.nativeType==0&&definition.domain==StorylandVehicleDomain::Road&&
                !definition.twoWheels();
            if(!environment.vehicleAttachmentFrame||!environment.vehicleAttachmentFrame(handle,result.matrix)||
                (result.automobile&&!definition.collision)) {
                error="Crane vehicle has no native body transform or collision bounds.";return false;
            }
            result.health=scripted.healthChangedByScript?scripted.health:vehicle.health;
            result.wrecked=result.health<=0||scripted.scriptStatus==5;
            result.roofHeight=definition.collision?definition.collision->maximum.z:0;
            result.hasDriver=vehicle.hasDriver||vehicle.missionDriver||int(slot)==runtime.occupiedVehicle;
            if(scripted.driver>=0) {
                const auto driver=environment.missionCharacters.find(scripted.driver);
                result.hasDriver=result.hasDriver||(driver!=environment.missionCharacters.end()&&driver->second.alive);
            }
            const auto action=runtime.vehicleAction;
            result.beingEntered=int(slot)==runtime.transitionVehicle&&
                (action==StorylandVehicleAction::Enter||action==StorylandVehicleAction::Hijack);
            const float yaw=actor.yaw*radians;
            result.velocity={(std::cos(yaw)*vehicle.speed-std::sin(yaw)*vehicle.lateralSpeed)/50,
                (std::sin(yaw)*vehicle.speed+std::cos(yaw)*vehicle.lateralSpeed)/50,vehicle.verticalSpeed/50};
            return true;
        };
        crane.transform=[this](const StorylandLcsCrane& definition,
            const StorylandLcsCraneMatrix& matrix,std::string& error) {
            error.clear();
            const auto& instances=archive.nativeWorld().instances;
            const auto found=std::find_if(instances.begin(),instances.end(),[&](const auto& instance) {
                return ((uint64_t(instance.poolHeader)<<32)|instance.poolIndex)==definition.worldKey;
            });
            if(found==instances.end()||int32_t(found->modelId)!=definition.model||
                !storylandLcsCraneFiniteMatrix(matrix)) {
                error="Crane native building identity or transform is invalid.";return false;
            }
            const auto previous=found->matrix;
            struct RenderBinding {
                StorylandArchiveBrowser* browser=nullptr;
                std::vector<StorylandLcsDynamicBinding> placements;
                std::vector<StorylandLcsCraneMatrix> previous;
            };
            std::vector<RenderBinding> render;
            size_t count=0;
            for(const auto& area:areas) {
                RenderBinding binding;binding.browser=area.get();
                if(!storylandFindLcsCraneRenderBindings(*area,archive.unpackedBytes(),archive.nativeWorld(),
                    *found,definition.originalMatrix,binding.placements,error))return false;
                for(const auto& placement:binding.placements) {
                    if(placement.placement>=area->placements().size()) {
                        error="Crane render placement is no longer present.";return false;
                    }
                    StorylandLcsCraneMatrix old{};
                    std::copy_n(area->placements()[placement.placement].matrix,16,old.begin());
                    binding.previous.push_back(old);
                }
                count+=binding.placements.size();
                render.push_back(std::move(binding));
            }
            if(count==0&&definition.status!=StorylandLcsCrane::None) {
                error="The active crane's native render geometry is not loaded.";return false;
            }
            if(!archive.setNativeInstanceTransform(definition.worldKey,matrix)) {
                error="Crane native collision transform was rejected.";return false;
            }
            for(auto& binding:render)
                if(!storylandApplyLcsDynamicTransform(*binding.browser,binding.placements,matrix)) {
                    for(auto& rollback:render)for(size_t index=0;index<rollback.placements.size();++index)
                        rollback.browser->setPlacementTransform(rollback.placements[index].placement,rollback.previous[index]);
                    archive.setNativeInstanceTransform(definition.worldKey,previous);
                    error="Crane render transform was rejected.";return false;
                }
            return true;
        };
        crane.updateVehicle=[this](int32_t handle,const StorylandLcsCraneMatrix& matrix,
            bool carried,bool crusher,std::string& error) {
            StorylandLcsCraneVehicle current;
            if(!environment.lcsCranes.bindings.readVehicle(handle,current,error)) {
                if(error.empty())error="Crane vehicle disappeared before its transform was applied.";
                return false;
            }
            if(!storylandLcsCraneFiniteMatrix(matrix)) {error="Crane vehicle transform is invalid.";return false;}
            auto& scripted=environment.missionVehicles.at(handle);
            const size_t slot=size_t(scripted.spawnedSlot);
            auto& vehicle=runtime.vehicles[slot];
            auto& actor=vehicles[slot];
            StorylandVehiclePose pose;
            if(!storylandVehiclePoseFromNativeFrame(vehicle.nativeDefinition,matrix,vehicle.suspension,pose)) {
                error="Crane vehicle frame cannot be restored to native physics.";return false;
            }
            actor.yaw=pose.heading;vehicle.bodyRoll=pose.roll;vehicle.bodyPitch=pose.pitch;
            vehicle.roadRoll=vehicle.roadPitch=0;
            actor.position={pose.position[0],pose.position[1],pose.position[2]};
            vehicle.craneFrame=matrix;vehicle.craneCarried=carried;
            if(slot<runtime.movementCars.size()) {
                auto& obstacle=runtime.movementCars[slot];
                obstacle.position=actor.position;obstacle.yaw=actor.yaw;obstacle.collisionEnabled=!carried;
            }
            if(slot<runtime.vehiclePositions.size())runtime.vehiclePositions[slot]=actor.position;
            vehicle.speed=vehicle.lateralSpeed=vehicle.verticalSpeed=vehicle.yawVelocity=0;
            if(carried) {vehicle.headlightsOn=false;vehicle.grounded=false;}
            if(carried&&crusher) {vehicle.damageProofs|=8;scripted.damageProofs|=8;}
            scripted.x=actor.position.x;scripted.y=actor.position.y;scripted.z=actor.position.z;
            scripted.heading=actor.yaw-90;scripted.speed=0;scripted.movedByScript=true;
            return true;
        };
        crane.findVehicles=[this](const std::array<float,4>& bounds,
            std::vector<StorylandLcsCraneVehicle>& result,std::string& error) {
            error.clear();result.clear();
            if(!environment.findScriptVehiclesInArea) {error="Crane vehicle pool scan is unavailable.";return false;}
            const auto handles=environment.findScriptVehiclesInArea(bounds,-1,StorylandMissionEnvironment::VehicleAreaRoad);
            for(int32_t handle:handles) {
                StorylandLcsCraneVehicle vehicle;
                std::string vehicleError;
                if(environment.lcsCranes.bindings.readVehicle(handle,vehicle,vehicleError))result.push_back(vehicle);
                else if(!vehicleError.empty()) {error=vehicleError;return false;}
            }
            return true;
        };
        crane.removeVehicle=[this](int32_t handle,std::string& error) {
            StorylandLcsCraneVehicle vehicle;
            if(!environment.lcsCranes.bindings.readVehicle(handle,vehicle,error)) {
                if(error.empty())error="Military crane vehicle is no longer present.";
                return false;
            }
            auto& scripted=environment.missionVehicles.at(handle);
            runtime.vehicles[size_t(scripted.spawnedSlot)].craneCarried=false;
            scripted.deleted=true;scripted.alive=false;
            return true;
        };
        crane.groundHeight=[this](const std::array<float,3>& point,float& height) {
            return environment.groundHeightAt&&environment.groundHeightAt({point[0],point[1],1000},height);
        };
        crane.message=[this](const std::string& key,int32_t number) {
            StorylandMissionEnvironment::LcsMessage message;
            message.key=key;message.durationMilliseconds=4000;
            if(number>=0)message.numbers.push_back(number);
            storylandQueueLcsMessage(environment,std::move(message),0,StorylandLcsMessagePriority::Now);
        };
        crane.reward=[this](int32_t amount) {
            environment.money=int32_t(uint32_t(environment.money)+uint32_t(amount));
        };
    }
    environment.prepareLcsGarageDoors=[this](const StorylandMissionEnvironment::Garage& garage,
        StorylandMissionEnvironment::LcsGarageDoorState& state,std::string& error) {
        if(vcs())return storylandPrepareVcsGarageDoors(archive.unpackedBytes(),archive.nativeWorld(),garage,state,error);
        return storylandPrepareLcsGarageDoors(archive.unpackedBytes(),archive.nativeWorld(),garage,state,error);
    };
    environment.applyLcsGarageDoorTransform=[this](const StorylandMissionEnvironment::LcsGarageDoor& door,
        const std::array<float,16>& matrix,std::string& error) {
        const auto& instances=archive.nativeWorld().instances;
        const auto instance=std::find_if(instances.begin(),instances.end(),[&](const auto& item) {
            return ((uint64_t(item.poolHeader)<<32)|item.poolIndex)==door.worldKey;
        });
        if(instance==instances.end()) {
            error="Garage door native instance is no longer present.";
            return false;
        }
        const auto current=instance->matrix;
        const bool ferris=environment.ferrisWheel.wheelFound&&
            environment.ferrisWheel.worldKey==door.worldKey;
        std::vector<std::vector<StorylandLcsDynamicBinding>> bindings(areas.size());
        for(size_t index=0;index<areas.size();++index) {
            if(ferris||areas[index]->hasNativeScenery())continue;
            if(!storylandFindLcsDynamicPlacements(*areas[index],door.model,door.originalMatrix,
                bindings[index],error,&current))return false;
        }
        if(!archive.setNativeInstanceTransform(door.worldKey,matrix)) {
            error="Garage door native collision transform was rejected.";
            return false;
        }
        for(size_t index=0;index<areas.size();++index) {
            if(ferris&&!storylandApplyVcsFerrisPlacements(*areas[index],matrix,error))return false;
            if(!storylandApplyLcsDynamicTransform(*areas[index],bindings[index],matrix)) {
                error="Garage door render transform was rejected.";
                return false;
            }
            areas[index]->synchronizeNativeSceneryTransforms(archive.nativeWorld());
        }
        error.clear();
        return true;
    };
    environment.updateGarageDoor=[this](const StorylandMissionEnvironment::Garage& garage) {
        if(!vcs())return;
        StorylandMissionEnvironment::LcsGarageDoorState state;
        std::string error;
        if(!storylandPrepareVcsGarageDoors(archive.unpackedBytes(),archive.nativeWorld(),garage,state,error)) {
            fail(error);return;
        }
        if(!storylandApplyLcsGarageDoors(environment,garage,state,error))fail(error);
    };
    if(vcs())environment.fireHunterGun=[this](int32_t handle,std::string& error) {
        StorylandWeaponDefinition cannon;
        cannon.nativeId=37;cannon.fireType=1;cannon.range=cannon.damage=0;
        const auto& bytes=archive.unpackedBytes();
        for(const auto& block:archive.dataBlocks()) {
            if(block.parser!="vcs-weapon-0x70-records"||block.rowSize!=0x70||block.rowCount<=37)continue;
            const uint64_t row=uint64_t(block.offset)+37*0x70;
            if(row+0x70>bytes.size())continue;
            std::memcpy(&cannon.range,bytes.data()+size_t(row)+8,4);
            int32_t damage=0;std::memcpy(&damage,bytes.data()+size_t(row)+0x18,4);
            cannon.damage=float(damage);break;
        }
        return storylandFireHunterGun(runtime,archive.nativeWorld(),vehicles,pedestrians,
            handle,error,&cannon);
    };
    environment.nativeObjectProperties = [this](int32_t id,StorylandMissionEnvironment::Object::DamageState& state) { return storylandReadObjectProperties(archive.unpackedBytes(),id,state,vcs()); };
    environment.currentPlayerModel = [this]() -> int32_t {
        if(!vcs())return 0;
        uint32_t model=0;
        return archive.findModelIdByName(std::filesystem::path(player.name).stem().u8string(),model)?int32_t(model):-1;
    };
    environment.replaceCharacterSkin = [this](int32_t handle,const std::string& requested,
        bool dressed,std::string& error) {
        const bool isPlayer=handle==(vcs()?0:1);
        const auto character=environment.missionCharacters.find(handle);
        if(!isPlayer&&character==environment.missionCharacters.end()) {
            error="Native clothing replacement requires an existing pedestrian.";return false;
        }
        int32_t model=isPlayer?0:character->second.model;
        std::string name=requested;
        if(vcs()) {
            uint32_t namedModel=0;
            if(!dressed||!archive.findModelIdByName(name,namedModel)||namedModel>INT32_MAX) {
                error="VCS clothing requires an existing named pedestrian model.";return false;
            }
            model=int32_t(namedModel);
        } else if(name.empty()) {
            size_t index=0;
            if(!archive.findModelDirEntryByModelId(uint32_t(model),index,name)) {
                error="Character clothing has no native model name.";return false;
            }
            name=std::filesystem::path(name).stem().u8string();
        }
        if(!dressed) {
            if(!storylandValidateSpecialModelAsset(archive,model,name,error))return false;
            if(isPlayer) {playerLoaded=false;playerPoints.clear();}
            else {character->second.removeFromWorld=true;missionBlends.erase(handle);}
            error.clear();return true;
        }
        StorylandPlayTestAsset replacement;
        if(!storylandLoadPlayTestCharacterSkin(archive,model,name,replacement,error))return false;
        StorylandAnimationBlendState blend;
        blend.walkSpeed=runtime.walkSpeed;blend.runSpeed=runtime.runSpeed;
        std::vector<StorylandModelPoint> points;
        auto& animation=isPlayer?playerAnimation:pedestrianAnimation;
        if(!animation.sampleTree(replacement.model,StorylandPlayTestMotion::Idle,
            animationTime,blend,points,error))return false;
        if(isPlayer) {
            player=std::move(replacement);playerBlend=std::move(blend);
            playerPoints=std::move(points);playerMotion=StorylandPlayTestMotion::Idle;
            playerAnimation.configureCombatDefinitions(runtime.weapons,&player.model);
            if(vcs()) {
                runtime.playerStats=player.pedStats;
                runtime.armyFatigues=model==0;
            }
            playerLoaded=true;
        } else {
            if(vcs()) {
                character->second.model=model;
                character->second.pedStats=replacement.pedStats;
                character->second.pedStat=replacement.pedStats.statIndex;
                character->second.animationGroup=replacement.pedStats.animationGroup;
            }
            missionAssets[model]=std::move(replacement);missionBlends[handle]=std::move(blend);
            character->second.removeFromWorld=false;
        }
        environment.residentModels.insert(model);
        error.clear();return true;
    };
    environment.nativeObjectMeleeDamage = [this](int32_t weapon,uint32_t sequence,bool moving) {
        return storylandNativeObjectMeleeDamage(archive.unpackedBytes(),weapon,sequence,moving);
    };
    environment.nativeModelAvailable = [this](int32_t model) { return storylandNativeModelAvailable(archive,model); };
    environment.nativeVehicleNameKey = [this](int32_t model) {
        return vcs() ? storylandVcsVehicleNameKey(archive.unpackedBytes(),model) : std::string{};
    };
    environment.scriptEntityModelMatrix = [this](int32_t handle,int32_t& model,std::array<float,16>& matrix) {
        return storylandResolveScriptEntityModelMatrix(environment,archive.nativeWorld(),handle,model,matrix);
    };
    environment.modelScriptEffects = [this](int32_t model,std::vector<StorylandModelScriptEffect>& effects) {
        std::string error;
        return vcs() && storylandDecodeVcsModelScriptEffects(archive.unpackedBytes(),model,effects,error);
    };
    environment.empireBuildingHandle = [this](int32_t site) {
        const auto point=camera.initialized?camera.eye:playerPosition;
        return storylandGetEmpireBuildingHandle(environment,site,point.x,point.y);
    };
    environment.streamingBacklogged = [this](bool& value,std::string& error) {
        return storylandVcsScriptStreamingBacklogged(archive.unpackedBytes(),environment,false,value,error);
    };
    environment.validateSpecialModel = [this](int32_t model,const std::string& name,std::string& error) { return storylandValidateSpecialModelAsset(archive,model,name,error); };
    environment.animationGroupLoaded = [this](int32_t group) {
        return playerAnimation.hasAnimationGroup(group) && pedestrianAnimation.hasAnimationGroup(group);
    };
    environment.ensureAnimationGroup = [this](int32_t group,std::string& error) {
        if (!playerAnimation.loadAnimationGroup(archive,group,error) ||
            !pedestrianAnimation.loadAnimationGroup(archive,group,error)) return false;
        environment.requestedAnimationGroups.insert(group);
        return true;
    };
    environment.animationAssociationFlags = [this](int32_t group,int32_t animation,uint16_t& flags) {
        return playerAnimation.retailAnimationAssociationFlags(animation,group,flags);
    };
    environment.animationBankAvailable = [this](const std::string& bank) {
        return playerAnimation.knowsAnimationBank(bank)&&pedestrianAnimation.knowsAnimationBank(bank);
    };
    environment.animationBankLoaded = [this](const std::string& bank) {
        return playerAnimation.hasAnimationBank(bank)&&pedestrianAnimation.hasAnimationBank(bank);
    };
    environment.resolvePedStats = [this](int32_t model,int32_t stat,StorylandPedStats& result) {
        if(vcs()||model<0)return false;
        result=storylandDecodePedStats(archive.unpackedBytes(),uint32_t(model),false,stat);
        return result.valid;
    };
    environment.characterSpeech = [this](int32_t handle,uint16_t event) {
        StorylandPlayTestSoundEvent sound(StorylandPlayTestSound::Damage);
        sound.nativeSpeech=event;
        sound.speaker=handle==0?-1:int(runtime.pedestrians.size())+handle;
        storylandStampCharacterSoundEvent(runtime,sound);
        runtime.soundEvents.push_back(sound);
    };
    environment.animationDuration = [this](int32_t group,int32_t animation) { return playerAnimation.retailAnimationDuration(animation,group); };
    environment.animationLoops = [this](int32_t group,int32_t animation) { return playerAnimation.retailAnimationLoops(animation,group); };
    environment.animationMovementSpeed = [this](int32_t group,int32_t movement) {
        std::string diagnostic;
        if(movement<0||movement>2||!pedestrianAnimation.selectLocomotionGroup(group,diagnostic))return 0.0f;
        const auto motion=movement==2?StorylandPlayTestMotion::Sprint:
            movement==1?StorylandPlayTestMotion::Run:StorylandPlayTestMotion::Walk;
        return pedestrianAnimation.locomotionSpeed(motion);
    };
    environment.setLcsAnimationBankLoaded = [this](const std::string& bank,bool loaded,std::string& error) {
        if(bank=="ped")return playerAnimation.hasNamedClip("idle_stance")&&pedestrianAnimation.hasNamedClip("idle_stance");
        const auto clips=storylandLcsAnimationBankClips(bank);
        if(clips.empty()) {error="Unknown LCS wait-state animation bank: "+bank;return false;}
        if(!loaded) {
            playerAnimation.unloadNamedAnimationBank(bank);
            pedestrianAnimation.unloadNamedAnimationBank(bank);
            return true;
        }
        return playerAnimation.loadNamedAnimationBank(archive,bank,clips,error)&&
            pedestrianAnimation.loadNamedAnimationBank(archive,bank,clips,error);
    };
    environment.lcsAnimationDuration = [this](const std::string& bank,const std::string& clip) {
        const float player=playerAnimation.bankClipDuration(bank,clip);
        const float pedestrian=pedestrianAnimation.bankClipDuration(bank,clip);
        return std::min(player,pedestrian);
    };
    environment.animationRemovalDeferred = [this](const std::string& bank) {
        bool pending = environment.requestedAnimationBanks.count(bank) != 0;
        for (int32_t group : environment.requestedAnimationGroups)
            pending |= playerAnimation.animationGroupBank(group) == bank;
        const bool loaded = playerAnimation.hasAnimationBank(bank) || pedestrianAnimation.hasAnimationBank(bank);
        return (pending || loaded) && storylandPendingModelNeedsAnimation(archive,
            environment.requestedModels, environment.residentModels, bank);
    };
    environment.removeAnimationBank = [this](const std::string& bank) {
        for (auto iterator = environment.requestedAnimationGroups.begin();
             iterator != environment.requestedAnimationGroups.end();) {
            if (playerAnimation.animationGroupBank(*iterator) == bank)
                iterator = environment.requestedAnimationGroups.erase(iterator);
            else
                ++iterator;
        }
        for (auto iterator = environment.pedAnimationRequests.begin();
             iterator != environment.pedAnimationRequests.end();) {
            if (playerAnimation.animationGroupBank(iterator->second.animationGroup) == bank)
                iterator = environment.pedAnimationRequests.erase(iterator);
            else
                ++iterator;
        }
        for (auto& character : environment.missionCharacters) {
            if (pedestrianAnimation.animationGroupBank(character.second.animationGroup) == bank) {
                character.second.animationId = -1;
                missionBlends.erase(character.first);
            }
        }
        playerAnimation.removeAnimationBank(bank);
        if(!runtime.zombiePedestrians||
            pedestrianAnimation.animationGroupBank(StorylandBrawnZombie::animationGroup)!=bank)
            pedestrianAnimation.removeAnimationBank(bank);
        playerBlend.boneFrames.clear();
        for (auto& blend : pedestrianBlends)
            blend.boneFrames.clear();
    };
    environment.requestLcsCollision = [this](const std::array<float,2>& point,std::string& error) {
        return archive.requestCollision({point[0],point[1],0},environment.lcsCollisionLevel,error);
    };
    environment.requestCollision = [this](const std::array<float,3>& point,std::string& error) { return archive.ensureNativeCollision({point[0],point[1],point[2]},error); };
    environment.entityCollisionStreaming = [this](int32_t kind,int32_t handle,bool enabled) {
        std::array<float,3> position{};
        if(kind==1&&handle==0)position={environment.x,environment.y,environment.z};
        else if(kind==1) {
            const auto found=environment.missionCharacters.find(handle);
            if(found==environment.missionCharacters.end())return false;
            position={found->second.x,found->second.y,found->second.z};
        } else if(kind==2) {
            const auto found=environment.missionVehicles.find(handle);
            if(found==environment.missionVehicles.end()||found->second.deleted)return false;
            position={found->second.x,found->second.y,found->second.z};
        } else return false;
        std::string error;
        return !enabled||(environment.requestCollision&&environment.requestCollision(position,error));
    };
    environment.shadowCollision = [this](const std::array<float,3>& from,const std::array<float,3>& to,
        std::array<float,3>& hit) {
        const float fraction=storylandTraceNativeSegmentCategories(archive.nativeWorld(),
            {from[0],from[1],from[2]},{to[0],to[1],to[2]},true,false);
        if(fraction>=1)return false;
        for(size_t axis=0;axis<3;++axis)hit[axis]=from[axis]+(to[axis]-from[axis])*fraction;
        return true;
    };
    environment.loadSceneCollision = environment.requestCollision;
    environment.loadScene = [this](const std::array<float,3>& point,std::string& error) {
        if(!applyBuildingSwaps(error))return false;
        if(!environment.requestCollision(point,error))return false;
        if(!preloadWorldScene) {error="LOAD_SCENE requires the native renderer's residency binding.";return false;}
        return preloadWorldScene(point,error);
    };
    environment.worldStreamingComplete = [this] {
        std::string error;
        if(!applyBuildingSwaps(error)) {fail(error);return false;}
        return !areas.empty()&&std::all_of(areas.begin(),areas.end(),[this](const auto& area) {return area->worldStreamingComplete(runtime.clockHour);});
    };
    environment.groundHeightAt = [this](const std::array<float,3>& point,float& height) {
        std::string error;
        if(!applyBuildingSwaps(error)) {fail(error);return false;}
        const float end = std::min(-1000.0f,point[2]-1000.0f);
        const float fraction=storylandTraceNativeMapGround(archive.nativeWorld(),{point[0],point[1],point[2]},{point[0],point[1],end});
        if(fraction>=1)return false; height=point[2]+(end-point[2])*fraction;return true;
    };
    environment.lcsExtendedWorld.selectVehicleComponents = [this](int32_t model,
        const std::array<int8_t,2>& requested,std::array<int32_t,2>& selected) {
        uint32_t rules=0;
        if(model<0||!storylandLcsVehicleComponentRules(archive.unpackedBytes(),uint32_t(model),rules))return false;
        int count=-1;
        for(const auto& asset:vehicleAssets)if(asset.vehicleDefinition.modelId==uint32_t(model)) {
            count=storylandNativeVehicleExtraCount(asset.model.rawBytes());break;
        }
        if(count<0) {
            StorylandPlayTestAsset asset;std::string error;
            if(!storylandLoadPlayTestVehicle(archive,uint32_t(model),asset,error))return false;
            count=storylandNativeVehicleExtraCount(asset.model.rawBytes());
            registerVehicleAsset(std::move(asset));
        }
        if(count<=0)return false;
        selected=storylandChooseLcsVehicleComponents(count,rules,requested,
            environment.weatherState.previous,environment.weatherState.current,
            [this]{return environment.nextNativeRandom();});
        return true;
    };
    environment.nativeModelRadius = [this](int32_t model,float& radius) {
        StorylandNativePoint center;
        return storylandReadNativeModelSphere(archive.unpackedBytes(),archive.nativeWorld(),model,center,radius);
    };
    environment.nativeModelBaseOffset = [this](int32_t model,float& offset) {
        return storylandReadNativeModelBaseOffset(archive.unpackedBytes(),archive.nativeWorld(),model,offset);
    };
    environment.nativeModelOriginOffset = [this](int32_t model,std::array<float,3>& offset) {
        auto definition=std::find_if(runtime.modelDefinitions.begin(),runtime.modelDefinitions.end(),
            [model](const auto& value){return value.modelId==uint32_t(model);});
        if(definition==runtime.modelDefinitions.end()) {
            StorylandPlayTestAsset asset;
            std::string error;
            if(model<0||!storylandLoadPlayTestVehicle(archive,uint32_t(model),asset,error))return false;
            registerVehicleAsset(std::move(asset));
            definition=std::find_if(runtime.modelDefinitions.begin(),runtime.modelDefinitions.end(),
                [model](const auto& value){return value.modelId==uint32_t(model);});
        }
        return definition!=runtime.modelDefinitions.end()&&
            storylandVcsHelicopterOriginOffset(*definition,offset);
    };
    environment.waterHeightAt = [this](const std::array<float,3>& point,float& height) {return storylandNativeWaterHeight(archive.nativeWorld(),point[0],point[1],height);};
    environment.deadCharacterPickupPosition = [this](int32_t handle,std::array<float,3>& position) {
        return storylandFindDeadCharacterPickupPosition(runtime,archive.nativeWorld(),vehicles,handle,position);
    };
    environment.objectInWater = [this](const StorylandMissionEnvironment::Object& object) {
        float height=0;
        return storylandNativeWaterHeight(archive.nativeWorld(),object.x,object.y,height)&&object.z<height;
    };
    environment.objectTouchingCharacter = [this](
        const StorylandMissionEnvironment::Object& object, int32_t handle) {
        return storylandScriptCharacterTouchesObject(
            environment, object, handle, archive.nativeWorld().collisionModels);
    };
    environment.vehicleMaximumAiSpeed = [this](int32_t handle) {
        const auto found=environment.missionVehicles.find(handle);if(found==environment.missionVehicles.end())return -1.0f;
        const auto definition=storylandDecodeVehicleDefinition(archive.unpackedBytes(),uint32_t(found->second.model),vcs());
        return definition.hasNativeHandling?definition.maximumAiVelocity:-1.0f;
    };
    environment.vehicleColorSetCount = [this](int32_t handle,bool scripted) -> int32_t {
        const auto found=environment.missionVehicles.find(handle);
        if(found==environment.missionVehicles.end())return -1;
        const auto definition=storylandDecodeVehicleDefinition(archive.unpackedBytes(),uint32_t(found->second.model),vcs());
        if(definition.domain==StorylandVehicleDomain::Unknown)return -1;
        return int32_t(scripted?definition.scriptColors.size():definition.colors.size());
    };
    environment.vehicleColorSet = [this](int32_t handle,bool scripted,int32_t selected,std::array<int32_t,2>& colors) {
        const auto found=environment.missionVehicles.find(handle);
        if(found==environment.missionVehicles.end())return false;
        const auto definition=storylandDecodeVehicleDefinition(archive.unpackedBytes(),uint32_t(found->second.model),vcs());
        const auto& sets=scripted?definition.scriptColors:definition.colors;
        if(selected<0||size_t(selected)>=sets.size())return false;
        colors={sets[size_t(selected)][0],sets[size_t(selected)][1]};return true;
    };
    environment.nativePedStat = [this](int32_t model,int32_t index,
        std::array<float,4>& parameters,std::array<uint8_t,5>& personality) {
        if(model<0||index<-1)return false;
        const auto stats=storylandDecodePedStats(archive.unpackedBytes(),uint32_t(model),vcs(),index);
        if(!stats.valid)return false;
        parameters={stats.fleeDistance,stats.headingChangeRate,stats.attackStrength,stats.defendWeakness};
        personality={stats.fear,stats.temper,stats.lawfulness,stats.view,uint8_t(stats.flags)};
        return true;
    };
    environment.characterOriginOffset = [this](int32_t handle,float& offset) {
        int32_t model=-1;
        if(handle==(vcs()?0:1)) {
            if(!vcs())model=0;
            else if(environment.modelIdByName)model=environment.modelIdByName(environment.playerOutfit);
        } else {
            const auto found=environment.missionCharacters.find(handle);
            if(found==environment.missionCharacters.end())return false;
            model=found->second.model;
        }
        return storylandReadNativeModelBaseOffset(archive.unpackedBytes(),archive.nativeWorld(),model,offset);
    };
    environment.characterHitSpheres = [this](int32_t handle,
        std::vector<StorylandMissionEnvironment::CharacterHitSphere>& spheres) {
        spheres.clear();
        if(!vcs())return false;
        const auto character=environment.missionCharacters.find(handle);
        if(character==environment.missionCharacters.end())return false;
        const auto asset=missionAssets.find(character->second.model);
        const auto blend=missionBlends.find(handle);
        return asset!=missionAssets.end()&&storylandVcsBoatDriverHitSpheres(environment,runtime,
            handle,asset->second.model,pedestrianAnimation,
            blend==missionBlends.end()?nullptr:&blend->second,spheres);
    };
    environment.characterWorldMatrix = [this](int32_t handle,std::array<float,16>& frame) {
        if(!vcs())return false;
        if(handle==0&&environment.inVehicle&&!environment.playerWarped&&runtime.occupiedVehicle>=0&&
            size_t(runtime.occupiedVehicle)<runtime.vehicles.size()&&
            size_t(runtime.occupiedVehicle)<vehicles.size()) {
            const auto& state=runtime.vehicles[size_t(runtime.occupiedVehicle)];
            if(state.nativeDefinition.vcsJetSki()) {
                if(runtime.vehicleAction!=StorylandVehicleAction::None)return false;
                const auto& actor=vehicles[size_t(runtime.occupiedVehicle)];
                frame=storylandVehicleOccupantFrame(state.nativeDefinition,
                    {actor.position.x,actor.position.y,actor.position.z},actor.yaw,
                    state.nativeDefinition.previewMinimumZ,state.suspension,state.bodyRoll-state.roadRoll,
                    state.bodyPitch+state.roadPitch,unsigned(runtime.playerVehicleSeat+1),actor.water,
                    state.craneCarried?&state.craneFrame:nullptr,storylandVcsJetSkiNativeAngularZ(state),state.bikeVisualLean);
                return std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);});
            }
        }
        const auto character=environment.missionCharacters.find(handle);
        if(handle==0||character==environment.missionCharacters.end()||
            character->second.attached||character->second.vehicle<0)
            return storylandVcsCharacterEntityFrame(environment,handle,frame);
        const auto& ped=character->second;
        const auto car=environment.missionVehicles.find(ped.vehicle);
        const auto task=environment.characterVehicleTasks.find(handle);
        float originOffset=0;
        if(car==environment.missionVehicles.end()||car->second.deleted||
            (task!=environment.characterVehicleTasks.end()&&task->second.animating)||
            !environment.characterOriginOffset||!environment.characterOriginOffset(handle,originOffset)||
            !std::isfinite(originOffset))return false;
        const int slot=car->second.spawnedSlot;
        if(slot<0||size_t(slot)>=runtime.vehicles.size()||size_t(slot)>=vehicles.size())return false;
        if(runtime.vehicles[size_t(slot)].nativeDefinition.vcsJetSki()) {
            const auto& state=runtime.vehicles[size_t(slot)];const auto& actor=vehicles[size_t(slot)];
            frame=storylandVehicleOccupantFrame(state.nativeDefinition,
                {actor.position.x,actor.position.y,actor.position.z},actor.yaw,
                state.nativeDefinition.previewMinimumZ,state.suspension,state.bodyRoll-state.roadRoll,
                state.bodyPitch+state.roadPitch,unsigned(ped.passengerSeat+1),actor.water,
                state.craneCarried?&state.craneFrame:nullptr,storylandVcsJetSkiNativeAngularZ(state),state.bikeVisualLean);
            return std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);});
        }
        auto asset=missionAssets.find(ped.model);
        std::string error;
        if(asset==missionAssets.end()) {
            size_t index=0;std::string name;
            const auto special=environment.specialModelNames.find(ped.model);
            if(special!=environment.specialModelNames.end())name=special->second;
            else if(!archive.findModelDirEntryByModelId(uint32_t(ped.model),index,name))return false;
            std::vector<StorylandPlayTestAsset> loaded;
            if(!storylandLoadPlayTestModels(archive,{name},loaded,error)||loaded.empty())return false;
            asset=missionAssets.emplace(ped.model,std::move(loaded.front())).first;
        }
        const auto& state=runtime.vehicles[size_t(slot)];
        const auto& traffic=vehicles[size_t(slot)];
        const auto& model=asset->second.model;
        if(std::none_of(model.armatureBones().begin(),model.armatureBones().end(),
            [](const auto& bone){return bone.boneId==1;}))return false;
        auto& blend=missionBlends[handle];
        auto options=pedestrianAnimation.vehicleAnimationOptions(state.nativeDefinition,&state.vcsJetSkiPose.sample,&runtime.vehiclePhysicsError);
        options.seatRelative=true;options.passenger=ped.passengerSeat>=0;
        options.rightHandSide=ped.passengerSeat>=0&&ped.passengerSeat%2==0;options.boat=traffic.water;
        options.steering=state.steeringAngle/std::max(1.0f,state.handlingSteerLock);
        options.reversing=state.speed<-.2f;options.elapsedSeconds=animationTime;
        options.transition=&blend.poseTransition;
        StorylandVehicleRootMotion root;
        std::vector<StorylandModelPoint> points;
        if(!pedestrianAnimation.sampleVehicleAction(model,"sit",0,options,points,error,&root,
            &blend.boneFrames)||blend.boneFrames.size()!=model.armatureBones().size()||
            model.armatureBones().empty())return false;
        frame=storylandVehicleOccupantFrame(state.nativeDefinition,
            {traffic.position.x,traffic.position.y,traffic.position.z},traffic.yaw,
            state.nativeDefinition.previewMinimumZ,state.suspension,state.bodyRoll-state.roadRoll,
            state.bodyPitch+state.roadPitch,unsigned(ped.passengerSeat+1),traffic.water,
            state.craneCarried?&state.craneFrame:nullptr,storylandVcsJetSkiNativeAngularZ(state),state.bikeVisualLean);
        const auto orientation=storylandScriptObjectMatrix(0,0,0,0,0,90-storylandPedModelForwardAngle(model));
        const auto pelvis=storylandVehicleTransformPoint(orientation,{root.current.x,root.current.y,root.current.z});
        for(size_t row=0;row<3;++row)
            frame[12+row]-=frame[row]*pelvis[0]+frame[4+row]*pelvis[1]+frame[8+row]*pelvis[2];
        return std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);});
    };
    environment.initializeCharacterAppearance = [this](
        StorylandMissionEnvironment::Character& character, std::string& error) {
        return !vcs() || storylandInitializeVcsPedAppearance(environment, character,
            archive.unpackedBytes(), error);
    };
    environment.characterColorSetCount = [this](int32_t handle,bool scripted) -> int32_t {
        const auto found=environment.missionCharacters.find(handle);
        if(found==environment.missionCharacters.end())return -1;
        const auto colors=storylandDecodePedClothing(archive.unpackedBytes(),uint32_t(found->second.model),vcs());
        return colors.valid?int32_t(scripted?colors.scriptCount:colors.regularCount):-1;
    };
    environment.characterColorSet = [this](int32_t handle,bool scripted,int32_t selected,std::array<uint32_t,6>& result) {
        const auto found=environment.missionCharacters.find(handle);
        if(found==environment.missionCharacters.end())return false;
        const auto colors=storylandDecodePedClothing(archive.unpackedBytes(),uint32_t(found->second.model),vcs());
        if(!colors.valid||selected<0)return false;
        if(!colors.regularCount&&(selected==0||(scripted&&selected<colors.scriptCount))) {result.fill(UINT32_MAX);return true;}
        const auto& sets=scripted?colors.scripted:colors.regular;
        if(size_t(selected)>=sets.size())return false;
        result=sets[size_t(selected)];return true;
    };
    environment.characterBoneMatrix = [this](int32_t handle,int32_t bone,std::array<float,16>& result) {
        if(bone<0)return false;
        const bool isPlayer=handle==(vcs()?0:1);
        const auto character=environment.missionCharacters.find(handle);
        if(!isPlayer&&character==environment.missionCharacters.end())return false;
        const StorylandModelFile* model=&player.model;
        std::string error;
        if(!isPlayer) {
            const int32_t id=character->second.model;
            auto asset=missionAssets.find(id);
            if(asset==missionAssets.end()) {
                size_t index=0;std::string name;
                const auto special=environment.specialModelNames.find(id);
                if(special!=environment.specialModelNames.end())name=special->second;
                else if(!archive.findModelDirEntryByModelId(uint32_t(id),index,name))return false;
                std::vector<StorylandPlayTestAsset> assets;
                if(!storylandLoadPlayTestModels(archive,{name},assets,error)||assets.empty())return false;
                asset=missionAssets.emplace(id,std::move(assets.front())).first;
            }
            model=&asset->second.model;
        }
        auto& animation=isPlayer?playerAnimation:pedestrianAnimation;
        auto blend=isPlayer?playerBlend:missionBlends[handle];
        std::vector<StorylandModelPoint> points;
        StorylandVehicleMatrix queryParent{};
        const bool vehicleQuery=isPlayer?
            storylandSamplePlayerVehicleQueryPose(animation,*model,runtime,vehicles,animationTime,blend,queryParent,error):
            storylandSampleMissionVehicleQueryPose(animation,*model,runtime,vehicles,handle,
                environment.gameMilliseconds,blend,queryParent,error);
        if(vehicleQuery) {
            const auto& bones=model->armatureBones();
            const auto node=std::find_if(bones.begin(),bones.end(),
                [bone](const auto& value){return value.boneId==uint32_t(bone);});
            if(node==bones.end()||blend.boneFrames.size()!=bones.size())return false;
            const auto orientation=storylandScriptObjectMatrix(0,0,0,0,0,90-storylandPedModelForwardAngle(*model));
            result=storylandVehicleMatrixProduct(queryParent,storylandVehicleMatrixProduct(
                orientation,blend.boneFrames[size_t(node-bones.begin())]));
            return std::all_of(result.begin(),result.end(),[](float value){return std::isfinite(value);});
        }
        int slot=isPlayer?runtime.occupiedVehicle:-1;
        const int seat=isPlayer?runtime.playerVehicleSeat:character->second.passengerSeat;
        if(!isPlayer&&character->second.vehicle>=0&&!character->second.attached) {
            const auto car=environment.missionVehicles.find(character->second.vehicle);
            if(car!=environment.missionVehicles.end())slot=car->second.spawnedSlot;
        }
        if(isPlayer&&runtime.vehicleAction==StorylandVehicleAction::Exit&&runtime.vehicleTransitionPrepared&&
            runtime.transitionVehicle>=0&&size_t(runtime.transitionVehicle)<runtime.vehicles.size()&&
            size_t(runtime.transitionVehicle)<vehicles.size()&&storylandVcsHelicopterBailout(
                runtime.vehicles[size_t(runtime.transitionVehicle)].nativeDefinition,runtime.vehicleEntryVariant)) {
            const auto& state=runtime.vehicles[size_t(runtime.transitionVehicle)];
            const auto& traffic=vehicles[size_t(runtime.transitionVehicle)];
            auto options=animation.vehicleAnimationOptions(state.nativeDefinition,&state.vcsJetSkiPose.sample,&runtime.vehiclePhysicsError);
            options.variant=runtime.vehicleEntryVariant;options.rootMotion=true;
            if(!animation.sampleVehicleAction(*model,"exit",runtime.vehiclePhase.fraction,options,
                points,error,nullptr,&blend.boneFrames))return false;
            StorylandVehicleMatrix parent;
            if(!storylandVcsHelicopterBailoutFrame(state.nativeDefinition,
                {traffic.position.x,traffic.position.y,traffic.position.z},traffic.yaw,state.suspension,
                state.bodyRoll-state.roadRoll,state.bodyPitch+state.roadPitch,parent))return false;
            const auto& bones=model->armatureBones();
            const auto node=std::find_if(bones.begin(),bones.end(),
                [bone](const auto& value){return value.boneId==uint32_t(bone);});
            if(node==bones.end()||blend.boneFrames.size()!=bones.size())return false;
            const auto orientation=storylandScriptObjectMatrix(0,0,0,0,0,90-storylandPedModelForwardAngle(*model));
            result=storylandVehicleMatrixProduct(parent,storylandVehicleMatrixProduct(
                orientation,blend.boneFrames[size_t(node-bones.begin())]));
            return std::all_of(result.begin(),result.end(),[](float value){return std::isfinite(value);});
        }
        if(slot>=0&&size_t(slot)<runtime.vehicles.size()&&size_t(slot)<vehicles.size()) {
            const auto& state=runtime.vehicles[size_t(slot)];
            const auto& traffic=vehicles[size_t(slot)];
            auto options=animation.vehicleAnimationOptions(state.nativeDefinition,&state.vcsJetSkiPose.sample,&runtime.vehiclePhysicsError);
            options.seatRelative=true;options.passenger=seat>=0;
            options.rightHandSide=seat>=0&&seat%2==0;options.boat=traffic.water;
            options.steering=state.steeringAngle/std::max(1.0f,state.handlingSteerLock);
            options.reversing=state.speed<-.2f;options.elapsedSeconds=animationTime;
            options.transition=&blend.poseTransition;
            std::array<float,16> driveByFrame{};
            const bool driveBy=isPlayer&&vcs()&&runtime.equippedWeapon<runtime.weapons.size()&&
                storylandPlayerSeatedDriveByVehicle(environment,runtime.weapons[runtime.equippedWeapon].nativeId,&driveByFrame);
            if(driveBy) {
                if(!animation.sampleSeatedDriveBy(*model,environment.occupiedMissionVehicle,seat,
                    storylandSeatedDriveByLocalTarget(environment,driveByFrame,runtime.manualAimActive),
                    animationTime,blend,points,error,false))return false;
            } else if(!animation.sampleVehicleAction(*model,"sit",0,options,points,error,nullptr,&blend.boneFrames))return false;
            const auto& bones=model->armatureBones();
            const auto node=std::find_if(bones.begin(),bones.end(),
                [bone](const auto& value){return value.boneId==uint32_t(bone);});
            if(node==bones.end()||blend.boneFrames.size()!=bones.size())return false;
            const auto parent=storylandVehicleOccupantFrame(state.nativeDefinition,
                {traffic.position.x,traffic.position.y,traffic.position.z},traffic.yaw,
                state.nativeDefinition.previewMinimumZ,state.suspension,state.bodyRoll-state.roadRoll,
                state.bodyPitch+state.roadPitch,unsigned(seat+1),traffic.water,
                state.craneCarried?&state.craneFrame:nullptr,storylandVcsJetSkiNativeAngularZ(state),state.bikeVisualLean);
            const auto orientation=storylandScriptObjectMatrix(0,0,0,0,0,90-storylandPedModelForwardAngle(*model));
            if(!vcs()&&!storylandApplyLcsScriptedLook(environment,handle,*model,
                storylandLcsLookSeatFrame(*model,parent),{parent[12],parent[13],parent[14]},
                blend,points,error,false,true))return false;
            result=storylandVehicleMatrixProduct(parent,storylandVehicleMatrixProduct(
                orientation,blend.boneFrames[size_t(node-bones.begin())]));
            return std::all_of(result.begin(),result.end(),[](float value){return std::isfinite(value);});
        }
        if(isPlayer) {
            const auto association=environment.scriptedCharacterAnimations.find(handle);
            if(runtime.playerWater.diving&&divingAnimations.available) {
                if(!storylandSampleSaDivingPose(animation,*model,divingAnimations,runtime.playerWater,
                    points,error,&blend.boneFrames))return false;
            } else if(runtime.playerJetSkiFallActive) {
                const auto& fall=runtime.playerJetSkiFall;
                const bool ready=storylandVcsCarjackedContinuation(fall)?
                    storylandSampleVcsCarjackedContinuation(animation,*model,runtime,vehicles,fall,
                        playerPosition,playerHeading,animationTime,blend,points,error,false):
                    animation.sampleRetailAnimationId(*model,fall.animation(),fall.time,points,error,0,
                        &blend.boneFrames,StorylandPlayTestMotion::Idle,-1,1,false,&blend.poseTransition,
                        storylandVcsPedFallPoseBlend(fall,runtime.health>0));
                if(!ready)return false;
            } else if(association!=environment.scriptedCharacterAnimations.end()) {
                if(!animation.sampleScriptAnimation(*model,association->second.animation,association->second.group,
                    environment.characterAnimationTime(handle,0),association->second.weight,blend,points,error,false))return false;
            } else if(!animation.sampleTree(*model,playerMotion,animationTime,blend,points,error,false))return false;
        } else {
            const auto fall=environment.vcsPedFalls.find(handle);
            if(vcs()&&character->second.alive&&character->second.health>0&&fall!=environment.vcsPedFalls.end()) {
                const bool ready=storylandVcsCarjackedContinuation(fall->second)?
                    storylandSampleVcsCarjackedContinuation(animation,*model,runtime,vehicles,fall->second,
                        {character->second.x,character->second.y,character->second.z},character->second.heading+90,
                        animationTime,blend,points,error,false):
                    animation.sampleRetailAnimationId(*model,fall->second.animation(),fall->second.time,points,error,0,
                        &blend.boneFrames,StorylandPlayTestMotion::Idle,-1,1,false,&blend.poseTransition,
                        storylandVcsPedFallPoseBlend(fall->second,character->second.alive&&character->second.health>0));
                if(!ready)return false;
            } else if(!sampleMissionCharacter(handle,points,error,false))return false;
        }
        if(vcs()&&(isPlayer?environment.activeVcsCharacterAttachment(0):character->second.attached)) {
            std::array<float,16> root{};
            if(!storylandVcsCharacterEntityFrame(environment,handle,root))return false;
            const auto& bones=model->armatureBones();
            const auto node=std::find_if(bones.begin(),bones.end(),
                [bone](const auto& value){return value.boneId==uint32_t(bone);});
            if(node==bones.end()||blend.boneFrames.size()!=bones.size())return false;
            const auto orientation=storylandScriptObjectMatrix(0,0,0,0,0,90-storylandPedModelForwardAngle(*model));
            result=storylandVehicleMatrixProduct(root,storylandVehicleMatrixProduct(
                orientation,blend.boneFrames[size_t(node-bones.begin())]));
            return std::all_of(result.begin(),result.end(),[](float value){return std::isfinite(value);});
        }
        const StorylandNativePoint position=isPlayer?playerPosition:
            StorylandNativePoint{character->second.x,character->second.y,character->second.z};
        if(!vcs()&&isPlayer&&!storylandApplyLcsStandingLook(environment,handle,*model,
            position,playerHeading,blend,points,error,false))return false;
        return storylandPoseBoneWorldFrame(*model,blend.boneFrames,uint32_t(bone),position,
            isPlayer?playerHeading:character->second.heading+90,1.8f,result);
    };
    environment.characterTouchingVehicle = [this](int32_t character,int32_t vehicle) {
        return storylandCharacterTouchesVehicle(runtime,archive.nativeWorld(),vehicles,
            character,vehicle);
    };
    environment.setPlayerClipAmmunition = [this](int32_t ammunition) {
        if(ammunition<0)return false;
        std::string error;
        if(!storylandApplyMissionWeapons(runtime,environment,error))return false;
        const size_t weapon=runtime.equippedWeapon;
        if(weapon>=runtime.inventory.size())return false;
        runtime.inventory[weapon].magazine=ammunition;
        return true;
    };
    environment.vehicleBasis = [this](int32_t handle,std::array<float,9>& basis) {
        const auto found=environment.missionVehicles.find(handle);if(found==environment.missionVehicles.end()||found->second.deleted)return false;
        const auto& car=found->second;
        if(car.rotationChangedByScript) {
            const auto frame=storylandVehicleMeshFrame(StorylandVehicleDefinition{},
                {car.x,car.y,car.z},car.heading+90,0,0,car.rotation[0],car.rotation[1]);
            basis={frame[0],frame[1],frame[2],frame[4],frame[5],frame[6],frame[8],frame[9],frame[10]};
        } else if(car.spawnedSlot>=0&&size_t(car.spawnedSlot)<vehicles.size()) {
            const auto& actor=vehicles[size_t(car.spawnedSlot)];const auto& vehicle=runtime.vehicles[size_t(car.spawnedSlot)];
            auto frame=storylandVehicleMeshFrame(vehicle.nativeDefinition,{actor.position.x,actor.position.y,actor.position.z},
                car.headingChangedByScript?car.heading+90:actor.yaw,
                vehicle.nativeDefinition.previewMinimumZ,vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,actor.water);
            basis={frame[0],frame[1],frame[2],frame[4],frame[5],frame[6],frame[8],frame[9],frame[10]};
        } else {const float a=car.heading*radians,c=std::cos(a),s=std::sin(a);basis={c,s,0,-s,c,0,0,0,1};}
        return true;
    };
    environment.nativeRopeDefinition = [this](int32_t type,float& length,int32_t& model) {
        return storylandNativeRopeDefinition(archive.unpackedBytes(),type,length,model);
    };
    environment.vehicleWorldMatrix = [this](int32_t handle,std::array<float,16>& matrix) {
        const auto found=environment.missionVehicles.find(handle);
        if(found==environment.missionVehicles.end()||found->second.deleted)return false;
        const auto& car=found->second;const int slot=car.spawnedSlot;
        if(slot>=0&&size_t(slot)<vehicles.size()&&runtime.vehicles[size_t(slot)].scriptHandle==handle) {
            const auto& actor=vehicles[size_t(slot)];const auto& vehicle=runtime.vehicles[size_t(slot)];
            matrix=storylandMissionVehicleWorldMatrix(vehicle,actor,car);
        } else matrix=storylandVehicleMeshFrame(StorylandVehicleDefinition{},
            {car.x,car.y,car.z},car.heading+90,0,0,car.rotation[0],car.rotation[1]);
        return true;
    };
    environment.vehicleAttachmentFrame = [this](int32_t handle,std::array<float,16>& frame) {
        const auto found=environment.missionVehicles.find(handle);
        if(found==environment.missionVehicles.end()||found->second.deleted||found->second.model<0)return false;
        const auto& car=found->second;
        auto definition=std::find_if(runtime.modelDefinitions.begin(),runtime.modelDefinitions.end(),
            [&](const auto& value){return value.modelId==uint32_t(car.model);});
        if(definition==runtime.modelDefinitions.end()) {
            StorylandPlayTestAsset asset;
            std::string error;
            if(!storylandLoadPlayTestVehicle(archive,uint32_t(car.model),asset,error))return false;
            registerVehicleAsset(std::move(asset));
            definition=std::find_if(runtime.modelDefinitions.begin(),runtime.modelDefinitions.end(),
                [&](const auto& value){return value.modelId==uint32_t(car.model);});
        }
        if(definition==runtime.modelDefinitions.end())return false;
        std::array<float,3> position{car.x,car.y,car.z};
        float yaw=car.heading+90,suspension=0,roll=car.rotation[0],pitch=car.rotation[1];
        const int32_t slot=car.spawnedSlot;
        if(slot>=0&&size_t(slot)<vehicles.size()&&size_t(slot)<runtime.vehicles.size()&&
            runtime.vehicles[size_t(slot)].scriptHandle==handle) {
            const auto& actor=vehicles[size_t(slot)];
            const auto& vehicle=runtime.vehicles[size_t(slot)];
            if(vehicle.craneCarried) {frame=vehicle.craneFrame;return true;}
            if(!car.movedByScript)position={actor.position.x,actor.position.y,actor.position.z};
            if(!car.headingChangedByScript)yaw=actor.yaw;
            suspension=vehicle.suspension;
            if(!car.rotationChangedByScript) {
                roll=vehicle.bodyRoll-vehicle.roadRoll;
                pitch=vehicle.bodyPitch+vehicle.roadPitch;
            }
        }
        frame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(*definition,position,yaw,
            definition->previewMinimumZ,suspension,roll,pitch,definition->domain==StorylandVehicleDomain::Water),
            definition->previewRoot);
        return std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);});
    };
    environment.vehicleNativeType = [this](int32_t handle) {const auto it=environment.missionVehicles.find(handle);return it==environment.missionVehicles.end()?-1:storylandDecodeVehicleDefinition(archive.unpackedBytes(),uint32_t(it->second.model),vcs()).nativeType;};
    environment.nativeVehicleCameraSeat = [this](int32_t model,std::array<float,3>& seat) {
        return vcs() && storylandReadVcsRemoteCameraSeat(archive.unpackedBytes(),model,seat);
    };
    environment.vehicleOnScreen = [this](int32_t handle) -> int32_t {
        const auto found=environment.missionVehicles.find(handle);
        if(found==environment.missionVehicles.end()||found->second.deleted||!environment.sphereOnScreen)return -1;
        StorylandNativePoint local{};
        float radius=0;
        std::array<float,16> matrix{};
        if(!storylandReadNativeModelSphere(archive.unpackedBytes(),archive.nativeWorld(),found->second.model,local,radius)||
            !environment.vehicleWorldMatrix||!environment.vehicleWorldMatrix(handle,matrix))return -1;
        return environment.sphereOnScreen({matrix[12]+matrix[0]*local.x+matrix[4]*local.y+matrix[8]*local.z,
            matrix[13]+matrix[1]*local.x+matrix[5]*local.y+matrix[9]*local.z,
            matrix[14]+matrix[2]*local.x+matrix[6]*local.y+matrix[10]*local.z},radius)?1:0;
    };
    environment.characterOnScreen = [this](int32_t handle) -> int32_t {
        if(!environment.sphereOnScreen)return -1;
        int32_t model=-1;
        float x=environment.x,y=environment.y,z=environment.z,heading=environment.heading;
        bool attached=false;
        if(handle==0) {
            if(environment.modelIdByName)model=environment.modelIdByName(environment.playerOutfit);
        } else {
            const auto found=environment.missionCharacters.find(handle);
            if(found==environment.missionCharacters.end())return -1;
            const auto& character=found->second;
            model=character.model;x=character.x;y=character.y;z=character.z;heading=character.heading;
            attached=character.attached;
        }
        StorylandNativePoint local{};
        float radius=0;
        if(!storylandReadNativeModelSphere(archive.unpackedBytes(),archive.nativeWorld(),model,local,radius))return -1;
        if(vcs()&&attached) {
            std::array<float,16> frame{};
            if(!storylandVcsCharacterEntityFrame(environment,handle,frame))return -1;
            return environment.sphereOnScreen({frame[12]+frame[0]*local.x+frame[4]*local.y+frame[8]*local.z,
                frame[13]+frame[1]*local.x+frame[5]*local.y+frame[9]*local.z,
                frame[14]+frame[2]*local.x+frame[6]*local.y+frame[10]*local.z},radius)?1:0;
        }
        const float angle=heading*.0174532925199433f,c=std::cos(angle),s=std::sin(angle);
        return environment.sphereOnScreen({x+c*local.x-s*local.y,y+s*local.x+c*local.y,z+local.z},radius)?1:0;
    };
    environment.objectOnScreen = [this](const StorylandMissionEnvironment::Object& object) {
        if(!environment.sphereOnScreen)return false;
        StorylandNativePoint local{};
        float radius=0;
        const int32_t model=object.model<0&&environment.resolveObjectModel?
            environment.resolveObjectModel(object.model):object.model;
        if(!storylandReadNativeModelSphere(archive.unpackedBytes(),archive.nativeWorld(),model,local,radius))return false;
        const auto matrix=storylandScriptObjectMatrix(object.x,object.y,object.z,
            object.rotationX,object.rotationY,object.heading);
        return environment.sphereOnScreen({matrix[12]+matrix[0]*local.x+matrix[4]*local.y+matrix[8]*local.z,
            matrix[13]+matrix[1]*local.x+matrix[5]*local.y+matrix[9]*local.z,
            matrix[14]+matrix[2]*local.x+matrix[6]*local.y+matrix[10]*local.z},radius);
    };
    environment.vehicleHandlingClass = [this](int32_t handle) -> int32_t {
        const auto found=environment.missionVehicles.find(handle);
        if(found==environment.missionVehicles.end()||found->second.deleted)return -1;
        if(!vcs()) {
            uint32_t flags=0;
            return found->second.model>=0&&storylandLcsVehicleHandlingFlags(archive.unpackedBytes(),
                uint32_t(found->second.model),flags)?storylandLcsVehicleHandlingClass(flags):-1;
        }
        const auto definition=storylandDecodeVehicleDefinition(archive.unpackedBytes(),uint32_t(found->second.model),vcs());
        if(!definition.hasNativeHandling)return -1;
        const uint32_t flags=definition.modelFlags;
        return flags&0x01000000u?2:flags&0x02000000u?3:flags&0x04000000u?5:flags&0x08000000u?4:1;
    };
    environment.activeVehicleCameraClass = [this]() -> int32_t {
        const int slot=runtime.occupiedVehicle;
        if(slot<0||size_t(slot)>=vehicles.size()||size_t(slot)>=runtime.vehicles.size())return -1;
        if(!vcs()) {
            uint32_t flags=0;
            return storylandLcsVehicleHandlingFlags(archive.unpackedBytes(),
                runtime.vehicles[size_t(slot)].nativeDefinition.modelId,flags)?storylandLcsVehicleHandlingClass(flags):-1;
        }
        const uint32_t flags=runtime.vehicles[size_t(slot)].nativeDefinition.modelFlags;
        return flags&0x01000000u?2:flags&0x02000000u?3:flags&0x04000000u?5:flags&0x08000000u?4:1;
    };
    environment.vehicleContacts = [this](int32_t handle) {
        StorylandMissionEnvironment::VehicleContacts result;
        const auto found=environment.missionVehicles.find(handle);
        if(found==environment.missionVehicles.end()||found->second.deleted)return result;
        const int slot=found->second.spawnedSlot;
        if(slot<0||size_t(slot)>=vehicles.size()||size_t(slot)>=runtime.vehicles.size())return result;
        const auto& vehicle=runtime.vehicles[size_t(slot)];
        if(vehicle.scriptHandle!=handle)return result;
        // CPhysical initializes an empty contact list before suspension runs.
        result.valid=true;result.twoWheels=vehicle.nativeDefinition.twoWheels();
        result.wheels=vehicle.wheelContacts;result.bodyContact=vehicle.bodyContact;
        result.speed=std::hypot(vehicle.speed,vehicle.lateralSpeed,vehicle.verticalSpeed);
        std::array<float,9> basis{};
        if(environment.vehicleBasis(handle,basis)) {result.forwardZ=basis[5];result.rightZ=basis[2];}
        return result;
    };
    environment.vehicleWorldVelocity = [this](int32_t handle,std::array<float,3>& velocity) {
        const auto found=environment.missionVehicles.find(handle);
        if(found==environment.missionVehicles.end()||found->second.deleted)return false;
        const auto& scripted=found->second;const int slot=scripted.spawnedSlot;
        velocity=scripted.velocity;
        if(!scripted.velocityChangedByScript&&slot>=0&&size_t(slot)<vehicles.size()&&
            size_t(slot)<runtime.vehicles.size()&&runtime.vehicles[size_t(slot)].scriptHandle==handle) {
            const auto& vehicle=runtime.vehicles[size_t(slot)];const float yaw=vehicles[size_t(slot)].yaw*radians;
            velocity={std::cos(yaw)*vehicle.speed-std::sin(yaw)*vehicle.lateralSpeed,
                std::sin(yaw)*vehicle.speed+std::cos(yaw)*vehicle.lateralSpeed,vehicle.verticalSpeed};
        }
        return true;
    };
    environment.vehicleOrientationAndSpeed = [this](int32_t handle) {
        const auto found=environment.missionVehicles.find(handle);
        if(found==environment.missionVehicles.end()||found->second.deleted)return std::array<float,3>{1,0,0};
        const int slot=found->second.spawnedSlot;
        if(slot<0||size_t(slot)>=vehicles.size()||size_t(slot)>=runtime.vehicles.size())return std::array<float,3>{1,0,0};
        const auto& vehicle=runtime.vehicles[size_t(slot)];
        if(vehicle.scriptHandle!=handle)return std::array<float,3>{1,0,0};
        const float up=std::cos((vehicle.bodyRoll-vehicle.roadRoll)*radians)*std::cos((vehicle.bodyPitch+vehicle.roadPitch)*radians);
        return std::array<float,3>{up,std::hypot(vehicle.speed,vehicle.lateralSpeed,vehicle.verticalSpeed)/50.0f,
            std::hypot(vehicle.yawVelocity,vehicle.springRollVelocity,vehicle.springPitchVelocity)/50.0f};
    };
    environment.vehicleIsBoat = [this](int32_t handle) {return environment.vehicleNativeType(handle)==1;};
    environment.vehicleHandlingFlags = [this](int32_t model,uint32_t& flags) {
        return !vcs()&&model>=0&&
            storylandLcsVehicleHandlingFlags(archive.unpackedBytes(),uint32_t(model),flags);
    };
    environment.configureCarMission = [this](int32_t handle,int32_t mission,std::string& error) {
        const auto car=environment.missionVehicles.find(handle);
        if(car==environment.missionVehicles.end()||car->second.deleted||car->second.model<0) {
            error="Car mission requires a live native vehicle.";return false;
        }
        const StorylandVehicleDefinition* definition=nullptr;
        for(const auto& candidate:runtime.modelDefinitions)
            if(candidate.modelId==uint32_t(car->second.model)) {definition=&candidate;break;}
        if(!definition) {
            StorylandPlayTestAsset asset;
            if(!storylandLoadPlayTestVehicle(archive,uint32_t(car->second.model),asset,error))return false;
            registerVehicleAsset(std::move(asset));
            definition=&runtime.modelDefinitions.back();
        }
        if(mission==14) {
            if(definition->nativeType!=1||!definition->hasNativeHandling||
                !definition->hasNativeBoatHandling||!definition->collision) {
                error="Boat control requires decoded native boat propulsion, handling and collision hull.";
                return false;
            }
            error.clear();return true;
        }
        if((definition->handlingFlags&0x60000u)||definition->domain==StorylandVehicleDomain::Other) {
            error="Car mission requires implemented ground or boat physics.";return false;
        }
        if(mission==1) {
            StorylandNativeTrafficActor route;
            route.vehicle=true;route.water=definition->domain!=StorylandVehicleDomain::Road;
            route.position={car->second.x,car->second.y,car->second.z};route.yaw=car->second.heading+90;
            if(!storylandJoinMissionCarWithRoad(archive.nativeWorld().paths,route)) {
                error="Cruise mission has no connected native traffic route.";return false;
            }
        }
        error.clear();return true;
    };
    environment.vehicleExitPosition = [this](int32_t handle,int32_t seat,std::array<float,3>& result) {
        const auto found=environment.missionVehicles.find(handle);if(found==environment.missionVehicles.end())return false;
        const auto& car=found->second;
        const auto definition=storylandDecodeVehicleDefinition(archive.unpackedBytes(),uint32_t(car.model),vcs());
        if(!definition.hasNativeDummies)return false;
        const auto nativeSeat=definition.seatPosition(seat<0?0u:unsigned(seat+1));
        const float width=definition.collision?std::max(std::fabs(definition.collision->minimum.x),std::fabs(definition.collision->maximum.x)):.8f;
        const float a=car.heading*radians,c=std::cos(a),s=std::sin(a),preferred=seat>=0&&seat%2==0?1.0f:-1.0f;
        for(float side:{preferred,-preferred}) {
            StorylandNativePoint exit{car.x+c*side*(width+.5f)-s*nativeSeat[1],car.y+s*side*(width+.5f)+c*nativeSeat[1],car.z};
            float ground=0;
            if(environment.groundHeightAt({exit.x,exit.y,exit.z+1.2f},ground)&&std::fabs(ground-exit.z)<2)exit.z=ground;
            if(!storylandNativeBodyClear(archive.nativeWorld(),exit,storylandNativeCharacterBody()))continue;
            result={exit.x,exit.y,exit.z};return true;
        }
        return false;
    };
    environment.prepareCharacterVehicleTask = [this](int32_t handle,StorylandMissionEnvironment::CharacterVehicleTask& task,std::string& error) {
        if(!storylandPrepareLcsVehicleTask(environment,runtime,archive.nativeWorld(),vehicles,handle,task,
            [this](const std::string& action,const StorylandVehicleDefinition& definition,bool right,int32_t seat) {
                auto options=pedestrianAnimation.vehicleAnimationOptions(definition);
                options.rightHandSide=right;options.passenger=seat>=0;options.rearDoor=seat>=1;
                return pedestrianAnimation.vehicleActionDuration(action,options,true);
            },error,
            [this](const StorylandVehicleDefinition& definition,std::array<float,3>& root) {
                const auto options=pedestrianAnimation.vehicleAnimationOptions(definition);
                return storylandReadVcsVehicleEntryRoot(archive.unpackedBytes(),options.animationGroup,root);
            }))return false;
        const auto car=environment.missionVehicles.find(task.vehicle);
        const auto ped=environment.missionCharacters.find(handle);
        if(car==environment.missionVehicles.end()||ped==environment.missionCharacters.end()) {
            error="Vehicle transition actor or car is unavailable.";return false;
        }
        auto asset=missionAssets.find(ped->second.model);
        if(asset==missionAssets.end()) {
            size_t index=0;std::string name;
            const auto special=environment.specialModelNames.find(ped->second.model);
            if(special!=environment.specialModelNames.end())name=special->second;
            else if(!archive.findModelDirEntryByModelId(uint32_t(ped->second.model),index,name)) {
                error="Vehicle transition pedestrian model directory entry is unavailable.";return false;
            }
            std::vector<StorylandPlayTestAsset> loaded;
            if(!storylandLoadPlayTestModels(archive,{name},loaded,error)||loaded.empty()) {
                if(error.empty())error="Vehicle transition pedestrian model is unavailable.";
                return false;
            }
            asset=missionAssets.emplace(ped->second.model,std::move(loaded.front())).first;
        }
        const auto definition=std::find_if(runtime.modelDefinitions.begin(),runtime.modelDefinitions.end(),
            [&](const auto& vehicle) {return vehicle.modelId==uint32_t(car->second.model);});
        if(definition==runtime.modelDefinitions.end()) {
            error="Vehicle transition definition is unavailable.";return false;
        }
        return storylandVerifyScriptVehicleTaskAnimations(pedestrianAnimation,
            asset->second.model,task,*definition,error);
    };
    environment.updateVehicleTaskDoor = [this](int32_t handle,int32_t seat,unsigned phase,float seconds) {
        const auto found=environment.missionVehicles.find(handle);if(found==environment.missionVehicles.end())return;
        const int slot=found->second.spawnedSlot;if(slot<0||size_t(slot)>=vehicles.size())return;
        bool right=seat>=0&&seat%2==0;
        for(const auto& entry:environment.characterVehicleTasks)if(entry.second.vehicle==handle&&entry.second.seat==seat&&entry.second.animating) {right=entry.second.rightHandSide;break;}
        storylandUpdatePlayTestVehicleDoor(runtime,size_t(slot),vehicles[size_t(slot)].position,
            seat<0?0u:unsigned(seat+1),right,StorylandVehiclePhase(phase),seconds);
    };
    if(vcs())environment.acquireVehicleDriver = [this](int32_t handle)->int32_t {
        const auto found=environment.missionVehicles.find(handle);
        if(found==environment.missionVehicles.end()||found->second.deleted)return -2;
        auto& car=found->second;
        if(car.driver>=0)return car.driver;
        if(environment.inVehicle&&environment.occupiedMissionVehicle==handle&&environment.playerPassengerSeat<0)return 0;
        if(car.spawnedSlot<0||size_t(car.spawnedSlot)>=runtime.vehicles.size()||
            size_t(car.spawnedSlot)>=vehicles.size())return -1;
        const size_t slot=size_t(car.spawnedSlot);auto& vehicle=runtime.vehicles[slot];
        if(vehicle.scriptHandle!=handle)return -2;
        if(!vehicle.hasDriver)return -1;
        const int32_t driver=storylandPromoteCarjackDriver(runtime,slot,vehicles[slot],{car.x,car.y,car.z});
        if(driver<0)return -2;
        car.driver=driver;vehicle.hasDriver=false;vehicle.missionDriver=true;
        return driver;
    };
    environment.removeVehicleDriver = [this](int32_t handle,const std::array<float,3>& door) {
        const auto car=environment.missionVehicles.find(handle);
        if(car==environment.missionVehicles.end()||car->second.spawnedSlot<0)return;
        const size_t slot=size_t(car->second.spawnedSlot);
        if(slot>=runtime.vehicles.size()||runtime.vehicles[slot].scriptHandle!=handle)return;
        auto& vehicle=runtime.vehicles[slot];
        bool right=false;
        for(const auto& entry:environment.characterVehicleTasks)
            if(entry.second.vehicle==handle&&entry.second.hijacking&&entry.second.animating) {
                right=entry.second.rightHandSide;break;
            }
        storylandBeginVcsCarjackedPlayerGetUp(runtime,slot,right,door,playerPosition);
        if(vehicle.hasDriver&&!vehicle.missionDriver&&car->second.driver<0) {
            vehicle.displacedDriverTime=4;vehicle.displacedDriverPosition={door[0],door[1],door[2]};
        }
        vehicle.hasDriver=false;vehicle.missionDriver=false;vehicle.parked=true;vehicle.speed=0;
    };
    environment.anyPickupInSphere = [this](const std::array<float,3>& point,float radius) {
        return storylandAnyPickupInSphere(environment,runtime,point,radius);
    };
    environment.pickupWeaponInSphere = [this](const std::array<float,3>& point,float radius,int32_t& weapon) {
        const auto resident=[this](int32_t model) {
            if(missionAssets.count(model))return true;
            return std::any_of(runtime.weapons.begin(),runtime.weapons.end(),
                [model](const auto& definition){return definition.modelId==model;});
        };
        return storylandFindPickupWeapon(environment,runtime,archive.unpackedBytes(),point,radius,resident,weapon);
    };
    environment.sniperBulletInArea = [this](const std::array<float,6>& bounds) {
        return storylandSniperBulletInArea(runtime,bounds);
    };
    environment.deadCharacterPickupPosition = [this](int32_t handle,std::array<float,3>& point) {
        return storylandFindDeadCharacterPickupPosition(runtime,archive.nativeWorld(),vehicles,handle,point);
    };
    environment.characterTouchingVehicle = [this](int32_t character,int32_t vehicle) {
        return storylandCharacterTouchesVehicle(runtime,archive.nativeWorld(),vehicles,character,vehicle);
    };
    environment.characterInWater = [this](int32_t handle) {
        if(handle==0)return runtime.playerWater.active;
        const auto found=environment.missionCharacters.find(handle);
        if(found==environment.missionCharacters.end())return false;
        const auto& ped=found->second;
        if(ped.vehicle>=0&&!ped.attached&&environment.vehicleInWater)
            return environment.vehicleInWater(ped.vehicle);
        float height=0;
        return storylandNativeWaterHeight(archive.nativeWorld(),ped.x,ped.y,height)&&ped.z<height;
    };
    environment.createScriptExplosion = [this](StorylandMissionEnvironment::ScriptExplosion& explosion,
        std::string& error) {
        return storylandCreatePlayTestExplosion(runtime,archive.nativeWorld(),vehicles,pedestrians,
            playerPosition,explosion,error);
    };
    environment.prepareCredits = [this](int32_t group,
        std::vector<StorylandMissionEnvironment::CreditsLine>& lines,std::string& error) {
        if(!prepareCredits) {error="Credits require the native HUD font.";return false;}
        return prepareCredits(group,lines,error);
    };
    environment.activeMissionScriptName = [this] {return program.activeMissionName(mission);};
    environment.currentRadioChannel = [this] {
        if (storylandOccupiedVehicleUsesPoliceRadio(runtime)) return storylandPoliceRadioChannel(vcs());
        return currentRadioChannel ? currentRadioChannel() : (vcs() ? 9 : 10);
    };
    environment.musicStreamPlaying = [this](int32_t stream) {return musicStreamPlaying&&musicStreamPlaying(stream);};
    environment.musicStreamPositionMilliseconds = [this]() {
        return musicStreamPositionMilliseconds?musicStreamPositionMilliseconds():uint64_t(0);
    };
    environment.playMissionPassedTune = [this](int32_t stream,bool looping,std::string& error) {
        if (!playMissionTune) { error="Mission tune requires a live retail stream consumer."; return false; }
        return playMissionTune(stream,looping,error);
    };
    environment.playLcsMissionPassedTune = [this](int32_t selector,std::string& error) {
        if(vcs()||selector!=1) {
            error="Mission completion music requires the verified LCS tune selector 1.";return false;
        }
        error.clear();
        if(!environment.audioSubsystemEnabled)return true;
        const int32_t stream=selector+63;
        if(environment.missionPassedTune==stream)return true;
        if(!playMissionTune) {
            error="Mission completion music requires a live native MISCOM2 stream consumer.";return false;
        }
        if(!playMissionTune(stream,false,error))return false;
        environment.missionPassedTune=stream;
        environment.missionTuneLoop=false;
        ++environment.missionPassedTuneSerial;
        environment.scriptAudioMode=0;
        environment.radioEnabled=false;
        environment.radioChanged=true;
        return true;
    };
    environment.stopMissionPassedTune = [this] {if (stopMissionTune) stopMissionTune();};
    environment.activeCameraVectors = [this](std::array<float,3>& eye,std::array<float,3>& target) {
        if(environment.camera.active&&!camera.initialized) {
            eye={environment.camera.x,environment.camera.y,environment.camera.z};
            target={environment.camera.targetX,environment.camera.targetY,environment.camera.targetZ};
        } else {
            eye={camera.eye.x,camera.eye.y,camera.eye.z};
            target={camera.target.x,camera.target.y,camera.target.z};
        }
        return true;
    };
    environment.activeBulletCameraFrame = [this](StorylandVcsBulletCameraFrame& frame) {
        std::array<float,3> eye{},target{};
        if(!environment.activeCameraVectors||!environment.activeCameraVectors(eye,target))return false;
        std::array<float,3> up{0,0,1};
        if(environment.camera.active)
            up={environment.camera.upX,environment.camera.upY,1.0f+environment.camera.upZ};
        else if(camera.vehicleCameraActive)
            up={camera.vehicleUp.x,camera.vehicleUp.y,camera.vehicleUp.z};
        return storylandVcsBulletCameraFrame(eye,target,up,
            environment.camera.active?environment.camera.roll:0.0f,frame);
    };
    environment.activeVehicleCameraClass = [this] {
        const int slot=runtime.occupiedVehicle;
        if(slot<0||size_t(slot)>=runtime.vehicles.size())return -1;
        const auto& definition=runtime.vehicles[size_t(slot)].nativeDefinition;
        if(!vcs()) {
            uint32_t flags=0;
            return storylandLcsVehicleHandlingFlags(archive.unpackedBytes(),definition.modelId,flags)?
                storylandLcsVehicleHandlingClass(flags):-1;
        }
        const uint32_t flags=definition.modelFlags;
        if(flags&0x01000000u)return 2;
        if(flags&0x02000000u)return 3;
        if(flags&0x04000000u)return 5;
        return flags&0x08000000u?4:1;
    };
    environment.activeCameraMode = [this] {
        if(environment.camera.active)return environment.cameraFocus.kind?environment.cameraFocus.mode:int32_t(15);
        if(camera.manualAimActive)return camera.manualAimMode;
        if(runtime.occupiedVehicle<0)return int32_t(4);
        if(camera.vehicleCameraActive&&camera.vehicleMode==0)return int32_t(16);
        if(camera.vehicleCameraActive&&camera.vehicleMode==5) {
            const int mode=camera.vehicleController.cinematicMode;
            if(mode==0)return int32_t(14);
            if(mode==1||mode==2||mode==3||mode==5)return int32_t(15);
        }
        return int32_t(18);
    };
    environment.photographLineClear = [this](int32_t character,const std::array<float,3>& start,
        const std::array<float,3>& end) {
        return storylandPhotographLineClear(runtime,archive.nativeWorld(),vehicles,pedestrians,character,
            {start[0],start[1],start[2]},{end[0],end[1],end[2]});
    };
    environment.photographVehicleLineClear = [this](int32_t vehicle,const std::array<float,3>& start,
        const std::array<float,3>& end) {
        return storylandPhotographVehicleLineClear(runtime,archive.nativeWorld(),vehicles,pedestrians,vehicle,
            {start[0],start[1],start[2]},{end[0],end[1],end[2]});
    };
    environment.scriptLineOfSightHit = [this](const std::array<float,3>& a,const std::array<float,3>& b,uint32_t mask) {
        return storylandScriptLineOfSightHit(runtime,archive.nativeWorld(),vehicles,pedestrians,{a[0],a[1],a[2]},{b[0],b[1],b[2]},mask);
    };
    environment.objectSegmentCollision = [this](const std::array<float,3>& a,const std::array<float,3>& b) {return storylandTraceNativeSegment(archive.nativeWorld(),{a[0],a[1],a[2]},{b[0],b[1],b[2]});};
    environment.objectSlideCollision = [this](int32_t handle,const std::array<float,3>& position) {
        const auto found=std::find_if(environment.objects.begin(),environment.objects.end(),[&](const auto& item){return item.handle==handle;});
        if(found==environment.objects.end())return -1;
        const auto& models=archive.nativeWorld().collisionModels;const auto bounds=models.find(uint32_t(found->model));if(bounds==models.end())return -1;
        const StorylandScriptObjectBox moving{bounds->second.minimum,bounds->second.maximum,{position[0],position[1],position[2]},found->heading,found->rotationX,found->rotationY};
        const auto overlap=[&](int32_t model,StorylandNativePoint p,float heading,bool ped) {
            const auto box=models.find(uint32_t(model));if(box!=models.end())return storylandScriptBoxesOverlap(moving,{box->second.minimum,box->second.maximum,p,heading});
            return ped&&storylandScriptBoxesOverlap(moving,{{-.35f,-.35f,0},{.35f,.35f,1.8f},p,heading});
        };
        const auto recordVehicle=[&](int32_t handle) {
            if(found->recordsCollisions) {
                found->collided=true;
                if(handle>=0&&found->collidedVehicles.size()<6&&
                    std::find(found->collidedVehicles.begin(),found->collidedVehicles.end(),handle)==found->collidedVehicles.end())
                    found->collidedVehicles.push_back(handle);
            }
            return 1;
        };
        for(size_t slot=0;slot<vehicles.size()&&slot<runtime.vehicles.size();++slot) {
            const auto& vehicle=runtime.vehicles[slot];
            const auto& actor=vehicles[slot];
            if(!vehicle.collisionEnabled||(found->ignoredVehicle>=0&&vehicle.scriptHandle==found->ignoredVehicle))continue;
            if(models.count(vehicle.nativeDefinition.modelId)) {
                if(overlap(int32_t(vehicle.nativeDefinition.modelId),actor.position,actor.yaw-90,false))
                    return recordVehicle(vehicle.scriptHandle);
            } else if(storylandScriptBoxesOverlap(moving,
                {{-vehicle.halfWidth,-vehicle.halfLength,0},
                 {vehicle.halfWidth,vehicle.halfLength,vehicle.bodyHeight},actor.position,actor.yaw-90}))
                return recordVehicle(vehicle.scriptHandle);
        }
        for(const auto& entry:environment.missionVehicles) {
            const auto& vehicle=entry.second;
            if(!vehicle.deleted&&vehicle.collisionEnabled&&vehicle.spawnedSlot<0&&
                entry.first!=found->ignoredVehicle&&overlap(vehicle.model,
                    {vehicle.x,vehicle.y,vehicle.z},vehicle.heading,false))return recordVehicle(entry.first);
        }
        if(!environment.inVehicle&&overlap(-1,playerPosition,playerHeading-90,true))return 1;
        for(size_t slot=0;slot<pedestrians.size()&&slot<runtime.pedestrians.size();++slot) {
            const auto& pedestrian=runtime.pedestrians[slot];
            if(pedestrian.renderAlpha==0||pedestrian.scriptHandle>=0)continue;
            const auto& actor=pedestrians[slot];
            const int32_t model=pedestrian.stats.valid?int32_t(pedestrian.stats.modelId):-1;
            if(overlap(model,actor.position,actor.yaw-90,true))return 1;
        }
        for(const auto& entry:environment.missionCharacters)if(entry.second.vehicle<0&&!entry.second.attached&&overlap(entry.second.model,{entry.second.x,entry.second.y,entry.second.z},entry.second.heading,true))return 1;
        return vcs()&&storylandScriptObjectContact(environment,models,*found,position)?1:0;
    };
    environment.clearAmbientArea = [this](const std::array<float,6>& bounds,bool cars) {storylandClearMissionArea(runtime,vehicles,pedestrians,environment,bounds,cars);};
    environment.randomCharacterModel = [this](int32_t vehicle,int32_t& model,int32_t& type) {
        int32_t vehicleModel = -1;
        int32_t vehicleClass = -1;
        if (vehicle >= 0) {
            const auto found = environment.missionVehicles.find(vehicle);
            if (found == environment.missionVehicles.end() || found->second.deleted) return false;
            vehicleModel = int16_t(uint16_t(found->second.model));
            switch (vehicleModel) {
            case 183: case 191: case 193: case 194: case 204: case 207:
            case 218: case 219: case 226: case 227: case 234: case 246:
            case 261: case -959: case -937:
                return false;
            default:
                break;
            }
            for (const auto& gang : environment.gangs)
                if (gang.carModel > 0 && gang.carModel == vehicleModel) return false;
            const auto definition = storylandDecodeVehicleDefinition(
                archive.unpackedBytes(), uint32_t(found->second.model), vcs());
            if (definition.domain != StorylandVehicleDomain::Road ||
                definition.vehicleClass < 0 || definition.vehicleClass >= 32) return false;
            vehicleClass = definition.vehicleClass;
        }
        std::vector<const StorylandPlayTestAsset*> candidates;
        for (const auto& asset : pedestrianAssets) {
            const auto& stats = asset.pedStats;
            if (!stats.valid || (stats.pedType != 4 && stats.pedType != 5) ||
                storylandServicePedestrianRole(asset.name,vcs(),asset.pedStats) != StorylandServiceRole::Civilian ||
                !storylandCivilianPedestrianModelAllowed(runtime.lcsPedestrianGroups,stats.modelId)) continue;
            if (vehicleClass >= 0 &&
                (!(stats.vehicleClassMask & (1u << vehicleClass)) ||
                 ((vehicleModel == 231 || vehicleModel == 232) && stats.pedType == 5))) continue;
            candidates.push_back(&asset);
        }
        if (candidates.empty()) return false;
        const size_t chosen = size_t(environment.nextNativeRandom()) % candidates.size();
        model = int32_t(candidates[chosen]->pedStats.modelId);
        type = candidates[chosen]->pedStats.pedType;
        return true;
    };
    storylandBindAmbientCharacterCallbacks(runtime,pedestrians);
    if(vcs())storylandBindVcsScriptPopulationQuery(runtime,archive.nativeWorld(),vehicles,pedestrians);
    else storylandBindScriptWorldQueries(runtime,archive.nativeWorld(),vehicles,pedestrians);
    storylandBindVehicleAutopilotCallbacks(runtime,vehicles,archive.nativeWorld());
}

void StorylandLinuxSession::tick(float seconds,const StorylandPlayTestInput& input,
    const StorylandPlayTestCameraInput& rawCameraInput,bool jump,bool interact,
    bool aimHeld,bool fireHeld,bool reloadPressed,const std::array<int16_t,20>* padInput) {
    auto cameraInput=rawCameraInput;
    if(!running)return;
    if(paused && !coop) {
        runtime.playerReaction.clearAction();
        runtime.pcGrabHeld=false;runtime.pcCancelAim=true;
        runtime.equalizerScope=false;runtime.equalizerScopeHeld=false;
        storylandClearPlayTestTarget(runtime);
        camera.aimLocked = false;
        runtime.manualAimActive = camera.manualAimActive = false;
        runtime.freeAimActive = camera.freeAimActive = false;
        runtime.manualAimMode = camera.manualAimMode = 0;
        return;
    }
    seconds=std::clamp(seconds,0.0f,.1f);
    if(seconds<=0)return;
    std::array<int16_t,20> derivedPad{};
    if(!padInput) {
        const bool motorcycle=storylandControlledVehicleIsMotorcycle(runtime);
        const auto pedals=input.vehiclePedalPressures();
        derivedPad[0]=int16_t(128*(int(input.right)-int(input.left)));
        derivedPad[1]=motorcycle?int16_t(-128*std::clamp(input.bikeLean,-1.0f,1.0f)):
            int16_t(128*(int(input.backward)-int(input.forward)));
        derivedPad[2]=int16_t(128*(int(cameraInput.right)-int(cameraInput.left)));
        derivedPad[3]=int16_t(128*(int(cameraInput.down)-int(cameraInput.up)));
        derivedPad[6]=aimHeld||cameraInput.freeAim?255:0;
        derivedPad[14]=motorcycle?int16_t(pedals[1]):(runtime.occupiedVehicle>=0?input.backward:jump)?255:0;
        derivedPad[15]=interact?255:0;
        derivedPad[16]=motorcycle?int16_t(pedals[0]):(runtime.occupiedVehicle>=0?input.forward:input.sprinting)?255:0;
        derivedPad[17]=fireHeld?255:0;
        padInput=&derivedPad;
    }
    const bool previousStart=environment.padState[12]!=0;
    storylandPublishMissionPad(environment,*padInput);
    if(!vcs()) {
        environment.creditsSkipPressed=!previousStart&&environment.padState[12]!=0;
        environment.creditsFastForward=environment.padState[16]!=0;
    }
    storylandUpdateAdrenaline(runtime);
    const double milliseconds=double(seconds)*1000+millisecondRemainder;
    const uint32_t elapsed=uint32_t(milliseconds);millisecondRemainder=milliseconds-elapsed;
    environment.nativeScriptTimeStepSource=[elapsed] {return float(elapsed)*.05f;};
    // Capture completed physical movement before the next script instruction.
    for(auto& entry:environment.missionVehicles) {
        auto& state=entry.second;const int slot=state.spawnedSlot;
        if(slot<0||size_t(slot)>=vehicles.size()||runtime.vehicles[size_t(slot)].scriptHandle!=entry.first)continue;
        auto& vehicle=runtime.vehicles[size_t(slot)];const auto& actor=vehicles[size_t(slot)];
        storylandCaptureMissionVehicleOverrides(vehicle,actor,environment,entry.first);
        if(!state.movedByScript) {state.x=actor.position.x;state.y=actor.position.y;state.z=actor.position.z;}
        if(!state.headingChangedByScript)state.heading=actor.yaw-90;
        if(!state.velocityChangedByScript) {const float a=actor.yaw*radians,c=std::cos(a),s=std::sin(a);state.velocity={c*vehicle.speed-s*vehicle.lateralSpeed,s*vehicle.speed+c*vehicle.lateralSpeed,vehicle.verticalSpeed};state.speed=vehicle.speed;}
        if(!state.healthChangedByScript) {state.health=vehicle.health;state.alive=vehicle.health>0;}
        if(slot==runtime.occupiedVehicle&&runtime.playerVehicleSeat<0)state.driver=0;
        else if(state.driver==0)state.driver=-1;
    }
    environment.x=playerPosition.x;environment.y=playerPosition.y;environment.z=playerPosition.z;environment.heading=playerHeading-90;
    environment.currentWorldLevel=archive.nativeWorld().zones.worldLevelAt(playerPosition,environment.currentWorldLevel);
    environment.health=runtime.health;environment.armour=runtime.armour;
    environment.hour=runtime.clockHour;
    environment.wanted=runtime.wanted;environment.playerArrested=runtime.busted;
    storylandCaptureMissionWeapons(runtime, environment);
    if(!environment.playerWarped) {
        environment.inVehicle=runtime.occupiedVehicle>=0;
        environment.playerPassengerSeat=runtime.playerVehicleSeat;
        environment.playerVehicleTransition=runtime.vehicleAction!=StorylandVehicleAction::None;
        environment.occupiedMissionVehicle=-1;environment.occupiedVehicleModel=-1;
        if(runtime.occupiedVehicle>=0&&size_t(runtime.occupiedVehicle)<vehicles.size()) {
            const auto& occupied=runtime.vehicles[size_t(runtime.occupiedVehicle)];
            environment.occupiedMissionVehicle=occupied.scriptHandle;
            environment.occupiedVehicleModel=int32_t(occupied.nativeDefinition.modelId);
        }
    }
    if(vcs()) {
        const auto point=camera.initialized?camera.eye:playerPosition;
        storylandUpdateAnimatedEmpireEntities(environment,point.x,point.y);
    }
    environment.statistics=runtime.statistics;
    environment.modelsKilledByPlayer=runtime.modelsKilledByPlayer;
    std::map<int32_t, StorylandNativePoint> beforeMissionPositions;
    for (const auto& entry : environment.missionCharacters)
        beforeMissionPositions.emplace(entry.first,
            StorylandNativePoint{entry.second.x, entry.second.y, entry.second.z});
    if(playingGame && storylandHoldIntroMissionForRestart(environment,program,mission))
        storylandAdvanceIntroRestartClock(environment,vcs(),elapsed);
    else if(playingGame)program.tick(elapsed,mission);
    else mission.tick(elapsed);
    if(playingGame)storylandHoldIntroMissionForRestart(environment,program,mission);
    runtime.statistics=environment.statistics;
    runtime.modelsKilledByPlayer=environment.modelsKilledByPlayer;
    seconds=environment.nativeScriptTimeStep*.02f;
    if(mission.state==StorylandMissionState::Fault||mission.state==StorylandMissionState::Unsupported) {fail(mission.diagnostic.empty()?program.status():mission.diagnostic);return;}
    const auto updateMissionLocomotion = [&] {
        for (const auto& entry : environment.missionCharacters) {
            const auto& character = entry.second;
            if (character.networkControlled) continue;
            auto& blend = missionBlends[entry.first];
            blend.movementForward = blend.movementRight = 0;
            const auto previous = beforeMissionPositions.find(entry.first);
            if (previous == beforeMissionPositions.end() || !character.alive || character.health <= 0 ||
                character.positionFrozen || character.stayInSamePlace || character.vehicle >= 0 ||
                character.attached || character.movementSpeed <= 0) continue;
            setLocomotionMovement(blend, character.x - previous->second.x,
                character.y - previous->second.y, character.heading + 90, seconds);
        }
    };
    updateMissionLocomotion();
    std::string error;
    for(const auto& clear:environment.clearAreaEvents) {
        storylandClearMissionRadius(runtime,vehicles,pedestrians,environment,
            {clear.x,clear.y,clear.z},clear.radius);
        if(clear.particles) {
            const auto inside=[&](const StorylandRuntimeParticle& particle) {
                const float dx=particle.position.x-clear.x,dy=particle.position.y-clear.y,dz=particle.position.z-clear.z;
                return dx*dx+dy*dy+dz*dz<=clear.radius*clear.radius;
            };
            runtime.particles.erase(std::remove_if(runtime.particles.begin(),runtime.particles.end(),inside),runtime.particles.end());
        }
    }
    environment.clearAreaEvents.clear();
    if(!applyBuildingSwaps(error)) {fail(error);return;}
    if(!prepareRequestedModels(error)) {fail(error);return;}
    storylandUpdateCarGenerators(runtime,environment,archive.nativeWorld(),vehicles,playerPosition,vcs(),
        storylandVcsCameraFieldOfView(environment.camera.active?environment.camera.fieldOfView:camera.fieldOfView,
            vcs()?environment.camera.vcsFieldOfViewOverride:0.0f),
        [this](uint32_t model) {
            if(std::any_of(runtime.modelDefinitions.begin(),runtime.modelDefinitions.end(),
                [model](const auto& item){return item.modelId==model;}))return true;
            StorylandPlayTestAsset asset;
            std::string diagnostic;
            if(!storylandLoadPlayTestVehicle(archive,model,asset,diagnostic))return false;
            registerVehicleAsset(std::move(asset));
            return true;
        },
        [this](size_t removed) {
            if(removed<vehicleDriverBlends.size())
                vehicleDriverBlends.erase(vehicleDriverBlends.begin()+removed);
        });
    if(vcs()) {
        const auto point=camera.initialized?camera.eye:playerPosition;
        storylandUpdateAnimatedEmpireEntities(environment,point.x,point.y);
    }
    if (!storylandApplyMissionWeapons(runtime, environment, error, vcs())) {
        fail(error);
        return;
    }
    for(auto& entry:environment.missionVehicles) {
        const int handle=entry.first;auto& state=entry.second;
        if(state.deleted) {storylandRemoveMissionVehicle(runtime,vehicles,environment,handle);continue;}
        int slot=state.spawnedSlot;
        if(slot<0||size_t(slot)>=vehicles.size()||size_t(slot)>=runtime.vehicles.size()||
            runtime.vehicles[size_t(slot)].scriptHandle!=handle) {
            if(!constructMissionVehicle(handle,error)) {fail(error);return;}
            slot=state.spawnedSlot;
        }
        auto& vehicle=runtime.vehicles[size_t(slot)];auto& actor=vehicles[size_t(slot)];
        storylandApplyMissionVehicleOverrides(vehicle,actor,environment,handle);
        storylandApplyMissionVehicleTransform(vehicle,actor,state,environment);
        vehicle.positionFrozen=state.positionFrozen;vehicle.collisionEnabled=state.collisionEnabled;vehicle.loadCollision=state.loadCollision;vehicle.visible=state.visible;vehicle.renderAlpha=state.renderAlpha;
        vehicle.canBeDamaged=state.canBeDamaged;vehicle.onlyDamagedByPlayer=state.onlyDamagedByPlayer;vehicle.damageProofs=state.damageProofs;
        vehicle.siren=state.siren;vehicle.doorLock=state.doorLock;vehicle.wrecked=state.wrecked;
        if(state.extinguishRequested) {vehicle.burning=false;vehicle.engineFireSpent=true;state.extinguishRequested=false;}
        if(state.healthChangedByScript)vehicle.health=state.health;
        if(state.primaryColor>=0)vehicle.primaryColor=state.primaryColor;if(state.secondaryColor>=0)vehicle.secondaryColor=state.secondaryColor;
        if(state.scriptStatus>=0)vehicle.parked=state.scriptStatus==4||state.scriptStatus==5;
        if(state.driver>=0)vehicle.hasDriver=state.aiActive;
        state.headingChangedByScript=state.velocityChangedByScript=state.movedByScript=state.healthChangedByScript=false;
    }
    if(environment.playerWarped) {
        int slot=-1;
        if(environment.inVehicle) {const auto found=environment.missionVehicles.find(environment.occupiedMissionVehicle);if(found!=environment.missionVehicles.end())slot=found->second.spawnedSlot;}
        if(!environment.inVehicle||slot>=0) {
            runtime.occupiedVehicle=slot;runtime.playerVehicleSeat=slot>=0?environment.playerPassengerSeat:-1;
            runtime.vehicleAction=StorylandVehicleAction::None;runtime.transitionVehicle=-1;environment.playerWarped=false;
            if(!vcs()) {
                environment.playerWalkTask.clear();
                environment.playerVehicleEntryTarget=-1;environment.playerVehicleEntryAsPassenger=false;
            }
            runtime.jumpLaunching=runtime.jumpFromLaunch=runtime.jumpHorizontalValid=false;
            runtime.jumpHorizontalVelocity={};runtime.verticalSpeed=0;
            runtime.airborneTime=runtime.landingTime=0;
            runtime.playerFall={};
            runtime.playerReaction.reset();runtime.vehicleExitPose={};
            runtime.playerVehicleContact={};runtime.playerBulletReaction={};
            runtime.statisticsPositionValid=false;runtime.pedestrianContactPlayerVelocity={};
        }
    }
    playerPosition={environment.x,environment.y,environment.z};playerHeading=environment.heading+90;
    runtime.playerControl=environment.control&&(!environment.playerPositionFrozen||
        (vcs()&&storylandVcsRemoteVehicleSlot(runtime)>=0));
    runtime.maxWanted=std::clamp(environment.maxWanted,0,6);
    runtime.policeIgnorePlayer=environment.policeIgnorePlayer;
    runtime.wantedMultiplier=environment.wantedMultiplier;
    runtime.visibleArea=environment.area;
    const auto wind=storylandWeatherWindVector(unsigned(std::max<int16_t>(0,environment.weatherState.previous)),
        unsigned(std::max<int16_t>(0,environment.weatherState.current)),environment.weatherState.interpolation,
        windDirectionDegrees);
    runtime.windVelocity={wind[0],wind[1],wind[2]};
    if(environment.wantedResetRequested) {
        storylandSetPlayTestWantedLevel(runtime,std::clamp(environment.wanted,0,runtime.maxWanted));
        environment.wantedResetRequested=false;
    }
    storylandApplyMissionClock(runtime,environment);
    runtime.health=environment.health;runtime.armour=environment.armour;
    runtime.walkSpeed=playerAnimation.locomotionSpeed(StorylandPlayTestMotion::Walk);
    runtime.runSpeed=playerAnimation.locomotionSpeed(StorylandPlayTestMotion::Run);
    runtime.sprintSpeed=playerAnimation.locomotionSpeed(StorylandPlayTestMotion::Sprint);
    runtime.playerRunStopDuration={playerAnimation.runStopDuration(0),playerAnimation.runStopDuration(1)};
    runtime.playerLocomotionPhase=playerBlend.locomotionPhase;
    runtime.jumpLaunchDuration=playerAnimation.movementActionDuration(StorylandPlayTestMotion::Jump);
    runtime.jumpLandingDuration=playerAnimation.movementActionDuration(StorylandPlayTestMotion::Land);
    if(runtime.jumpLandingDuration<=0)runtime.jumpLandingDuration=.35f;

    if(!vcs())storylandApplyMissionVehicleAutopilot(runtime,vehicles,environment,runtime.livePaths);
    storylandUpdateEscalators(runtime.escalators,archive.nativeWorld(),camera.eye,environment.gameMilliseconds,
        storylandEscalatorFreeObjectSlots(runtime.escalators,environment.objects.size()),bool(escalatorStepAsset),
        [this](const StorylandNativePoint& point,float radius) {
            return !environment.sphereOnScreen||environment.sphereOnScreen({point.x,point.y,point.z},radius);
        });
    if(!environment.cutscene.playing) {
        // Zone pedestrian groups can change as the player crosses the city.
        // Decode at most one missing native group model per frame.
        if(environment.streamingEnabled&&environment.pedDensityMultiplier>0) {
            const auto population=storylandNativePopulationAt(environment,playerPosition,runtime.clockHour);
            for(uint32_t id:storylandPopulationPedestrianModels(environment,population,runtime.lcsPedestrianGroups)) {
                    if(std::any_of(pedestrianAssets.begin(),pedestrianAssets.end(),
                        [id](const auto& asset){return asset.pedStats.valid&&asset.pedStats.modelId==id;})||
                        !runtime.lcsPopulationModelsAttempted.insert(id).second)continue;
                    size_t entry=0;std::string name,diagnostic;
                    std::vector<StorylandPlayTestAsset> decoded;
                    if(archive.findModelDirEntryByModelId(id,entry,name)&&
                        storylandLoadPlayTestModels(archive,{name},decoded,diagnostic))
                        for(auto& asset:decoded)if(asset.pedStats.valid&&asset.pedStats.modelId==id) {
                            if(!storylandPreparePedModelAnimations(archive,{int32_t(id)},{},
                                playerAnimation,pedestrianAnimation,diagnostic)) {fail(diagnostic);return;}
                            runtime.pedestrianModelStats.push_back(asset.pedStats);
                            runtime.pedestrianModelRoles.push_back(storylandServicePedestrianRole(asset.name,vcs(),asset.pedStats));
                            pedestrianAssets.push_back(std::move(asset));
                            environment.residentModels.insert(int32_t(id));
                        }
                    break;
                }
        }
        const auto previous=pedestrians;
        std::vector<size_t> previousModels;
        for(size_t i=0;i<previous.size()&&i<runtime.pedestrians.size();++i)
            previousModels.push_back(runtime.pedestrians[i].model);
        storylandRefreshLcsPedestrians(runtime,runtime.livePaths,archive.nativeWorld(),
            pedestrians,playerPosition,camera.yaw);
        storylandCarryEscalatorActors(runtime,archive.nativeWorld(),pedestrians,playerPosition);
        pedestrianBlends.resize(pedestrians.size());
        std::vector<StorylandNativePoint> beforeTraffic;
        for(size_t i=0;i<pedestrians.size();++i) {
            const auto& position=pedestrians[i].position;
            if(i>=previous.size()||i>=previousModels.size()||
                previousModels[i]!=runtime.pedestrians[i].model||
                position.x!=previous[i].position.x||position.y!=previous[i].position.y||position.z!=previous[i].position.z)
                pedestrianBlends[i]={};
            beforeTraffic.push_back(position);
        }
        std::string garageError;
        if(!storylandAdvanceVcsSafehouseGarages(runtime,vehicles,seconds,garageError)) {
            fail(garageError);return;
        }
        storylandUpdatePlayTestTraffic(runtime,runtime.livePaths,pedestrians,vehicles,playerPosition,seconds,&archive.nativeWorld());
        if(!runtime.vehiclePhysicsError.empty()) {fail(runtime.vehiclePhysicsError);return;}
        storylandDispatchPolice(runtime,archive.nativeWorld(),pedestrians,playerPosition,camera.yaw,seconds);
        for(size_t i=0;i<pedestrians.size()&&i<beforeTraffic.size();++i) {
            const auto& ped=runtime.pedestrians[i];auto& blend=pedestrianBlends[i];
            if(ped.state==StorylandPedState::Combat) {
                blend.measuredSpeed=0;
                blend.movementForward=blend.movementRight=0;
                continue;
            }
            const float deltaX=pedestrians[i].position.x-beforeTraffic[i].x;
            const float deltaY=pedestrians[i].position.y-beforeTraffic[i].y;
            const float travelled=std::hypot(deltaX,deltaY);
            setLocomotionMovement(blend,deltaX,deltaY,pedestrians[i].yaw,seconds);
            const auto motion=(ped.patrol.running||(ped.state==StorylandPedState::Flee&&(vcs()||!ped.fleeWalking)))?
                StorylandPlayTestMotion::Run:StorylandPlayTestMotion::Walk;
            std::string diagnostic;
            const int32_t nativeWeapon=ped.weapon<runtime.weapons.size()?
                runtime.weapons[ped.weapon].nativeId:-1;
            const int32_t group=storylandNativePedWeaponLocomotionGroup(vcs(),
                nativeWeapon,ped.stats.animationGroup);
            if(!pedestrianAnimation.advanceLocomotionDistance(group,motion,
                travelled,seconds,blend,diagnostic))
                pedestrianAnimation.advanceLocomotionDistance(0,motion,travelled,seconds,blend,diagnostic);
        }
    }
    if(environment.cutscene.playing) {
        runtime.playerReaction.reset();runtime.vehicleExitPose={};environment.playerWalkTask.clear();
    }
    const StorylandNativePoint beforeMovement = playerPosition;
    if (input.pcControls && cameraInput.freeAim && storylandPlayerUsesBareHands(runtime)) {
        aimHeld = true;
        cameraInput.freeAim = false;
    }
    const bool aimingInput=(aimHeld||cameraInput.freeAim)&&!input.cancelAim;
    storylandUpdatePlayerCombatControls(runtime,input,aimingInput);
    if(input.cancelAim) {aimHeld=false;cameraInput.freeAim=false;}
    if(aimingInput&&storylandEqualizerUsesNormalAim(runtime))cameraInput.freeAim=true;
    runtime.playerAiming = (aimingInput||runtime.playerReaction.holding()) && storylandCanAimPlayTestWeapon(runtime);
    if (!aimHeld&&!runtime.playerReaction.holding()) storylandClearPlayTestTarget(runtime);
    if(!environment.cutscene.playing) {
        const bool rawJump=jump;
        storylandPreparePlayTestTarget(runtime,archive.nativeWorld(),pedestrians,
            playerPosition,playerHeading,aimHeld);
        storylandPreparePlayTestFreeAim(runtime,archive.nativeWorld(),pedestrians,
            playerPosition,playerHeading,camera,cameraInput,seconds);
        const bool reactionMovement=!runtime.vehicleExitPose.active&&storylandUpdatePlayerCombatReactions(runtime,
            runtime.playerReaction,runtime.reactionBindings,archive.nativeWorld(),pedestrians,vehicles,
            playerPosition,playerHeading,jump,interact,aimingInput,fireHeld,reloadPressed,input.forward,seconds);
        const StorylandPlayTestInput movementInput=reactionMovement?StorylandPlayTestInput{}:input;
        storylandUpdatePlayTestPlayer(runtime,archive.nativeWorld(),playerPosition,playerHeading,camera.yaw,
            movementInput,reactionMovement?false:jump,reactionMovement?false:interact,vehicles,seconds);
        storylandUpdateVcsJetSkiOccupants(runtime,vehicles,playerPosition,playerHeading,seconds);
        if(!runtime.vehiclePhysicsError.empty()) {fail(runtime.vehiclePhysicsError);return;}
        if(reactionMovement)runtime.jumpButtonHeld=rawJump;
        for(size_t i=0;i<vehicles.size()&&i<runtime.vehicles.size();++i)
            storylandUpdateVehicleWheelTravel(runtime.vehicles[i],vehicles[i],seconds);
        environment.x=playerPosition.x;environment.y=playerPosition.y;environment.z=playerPosition.z;environment.heading=playerHeading-90;
        const auto pickupModelName=[this](int32_t model) {
            if(model<0)return script.scriptObjectName(model);
            size_t index=0;std::string name;
            archive.findModelDirEntryByModelId(uint32_t(model),index,name);
            return name;
        };
        storylandUpdateMissionPickups(environment,runtime,pickupModelName,vcs());
        updateCombatPoseAnchors();
        StorylandManualAimInput manualInput;
        manualInput.look = cameraInput;
        if(vcs()) {
            manualInput.zoomIn = storylandVcsPadState(environment,32)!=0;
            manualInput.zoomOut = storylandVcsPadState(environment,33)!=0;
        }
        manualInput.head = runtime.playerHead;
        manualInput.headValid = runtime.playerHeadValid;
        manualInput.torso = runtime.playerTorso;
        manualInput.torsoValid = runtime.playerTorsoValid;
        manualInput.shoulder = runtime.playerShoulder;
        manualInput.shoulderValid = runtime.playerShoulderValid;
        manualInput.controlEnabled = runtime.playerControl && environment.cameraControlEnabled;
        manualInput.suspended = environment.playerWalkTask.active||runtime.playerWater.active ||
            runtime.vehicleAction != StorylandVehicleAction::None;
        if(runtime.equippedWeapon<runtime.weapons.size()&&runtime.equippedWeapon<runtime.inventory.size()&&
            runtime.inventory[runtime.equippedWeapon].owned) {
            manualInput.nativeWeapon=runtime.weapons[runtime.equippedWeapon].nativeId;
            manualInput.weaponRange=runtime.weapons[runtime.equippedWeapon].range;
        }
        bool lcsCamera = false;
        if (aimingInput && runtime.playerControl &&
            runtime.equippedWeapon < runtime.weapons.size() &&
            runtime.equippedWeapon < runtime.inventory.size()) {
            const auto& inventory = runtime.inventory[runtime.equippedWeapon];
            const auto& weapon = runtime.weapons[runtime.equippedWeapon];
            lcsCamera = !vcs() && weapon.nativeId==36 && weapon.fireType==4;
            if (inventory.owned && inventory.ammunition > 0) {
                if(lcsCamera)manualInput.weaponMode=46;
                else if(vcs())manualInput.weaponMode=storylandPlayerWeaponManualAimMode(runtime,weapon.nativeId);
            }
        }
        runtime.manualAimActive = (vcs() || lcsCamera) && storylandUpdateManualAimCamera(camera,
            environment, playerPosition, playerHeading, manualInput, archive.nativeWorld());
        runtime.manualAimMode = runtime.manualAimActive ? camera.manualAimMode : 0;
        if (runtime.manualAimActive) {
            runtime.freeAimActive=camera.freeAimActive=false;
            runtime.manualAimOrigin = camera.eye;
            runtime.manualAimTarget = camera.target;
            runtime.aimPitch = camera.pitch;
            if(!environment.inVehicle)playerHeading = camera.yaw;
        }
        storylandRefreshPlayTestFreeAim(runtime,archive.nativeWorld(),pedestrians,
            playerPosition,playerHeading,camera);
        std::vector<StorylandNativePoint> beforeCombatPositions;
        beforeCombatPositions.reserve(pedestrians.size());
        for(const auto& pedestrian:pedestrians)beforeCombatPositions.push_back(pedestrian.position);
        storylandUpdatePlayTestWeapons(runtime, archive.nativeWorld(), pedestrians,
            playerPosition, playerHeading, camera.yaw, aimHeld && !runtime.playerWater.active,
            fireHeld && !runtime.playerWater.active, reloadPressed && !runtime.playerWater.active,
            seconds,&vehicles);
        updateMissionLocomotion();
        pedestrianBlends.resize(pedestrians.size());
        for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size()&&
            index<beforeCombatPositions.size();++index) {
            const auto& ped=runtime.pedestrians[index];
            if(ped.state!=StorylandPedState::Combat)continue;
            const auto& position=pedestrians[index].position;
            const auto& previous=beforeCombatPositions[index];
            const float deltaX=position.x-previous.x;
            const float deltaY=position.y-previous.y;
            const float travelled=std::hypot(deltaX,deltaY);
            setLocomotionMovement(pedestrianBlends[index],deltaX,deltaY,pedestrians[index].yaw,seconds);
            const int32_t nativeWeapon=ped.weapon<runtime.weapons.size()?
                runtime.weapons[ped.weapon].nativeId:-1;
            const int32_t group=storylandNativePedWeaponLocomotionGroup(vcs(),
                nativeWeapon,ped.stats.animationGroup);
            std::string diagnostic;
            if(!pedestrianAnimation.advanceLocomotionDistance(group,StorylandPlayTestMotion::Walk,
                travelled,seconds,pedestrianBlends[index],diagnostic))
                pedestrianAnimation.advanceLocomotionDistance(0,StorylandPlayTestMotion::Walk,
                    travelled,seconds,pedestrianBlends[index],diagnostic);
        }
        environment.heading = playerHeading - 90;
    } else {
        storylandClearPlayTestTarget(runtime);
        runtime.playerAiming = false;
        runtime.freeAimActive=camera.freeAimActive=false;
    }
    environment.hour=runtime.clockHour;
    environment.wanted=runtime.wanted;
    storylandPruneMissionRadarBlips(environment,vcs());
    if(!updatePlayerRestart())return;
    std::string taxiError;
    if(!storylandUpdateRestartTaxi(runtime,environment,vehicles,runtime.restartTaxi,
        playerPosition,playerHeading,taxiError)) {fail(taxiError);return;}
    const bool wasPlaying=environment.cutscene.playing;
    const uint32_t cutsceneElapsed=wasPlaying&&cutsceneElapsedMilliseconds?cutsceneElapsedMilliseconds(elapsed):elapsed;
    if(!cutscene.update(cutsceneElapsed,mocap,archive,environment,error)) {fail("Native cutscene: "+error);return;}
    const auto translate = [this](const std::string& key, const std::string& table) {
        return translateMissionText(key, table);
    };
    if (!vcs() && environment.lcsMessageQueuesUsed) {
        runtime.missionText.clear();
        if (!environment.lcsBriefMessages.empty() && !environment.lcsBriefMessages.front().overrideFade &&
            environment.lcsBigMessages[2].empty())
            runtime.missionText = storylandFormatLcsMessage(environment.lcsBriefMessages.front(), translate);
    } else {
        runtime.missionText = environment.messageMilliseconds ? textUtf8(storylandMissionFormattedText(
            environment, environment.messageKey, environment.messageNumbers, environment.messageStringKey)) : std::string{};
    }
    uint64_t newestSubtitle = 0;
    for (const auto& slot : environment.audioSlots) {
        if (!environment.missionAudioSubtitleEnabled || !slot.playbackStarted || slot.finished ||
            slot.subtitleKey.empty() || slot.serial <= newestSubtitle) continue;
        std::u16string text;
        if (missionGxt.lookup(slot.subtitleKey, environment.missionText, text)) {
            newestSubtitle = slot.serial;
            runtime.missionText = textUtf8(text);
        }
    }
    if (environment.cutscene.playing) runtime.missionText.clear();
    runtime.missionHelp = textUtf8(storylandMissionFormattedText(
        environment, environment.helpMessage, environment.helpMessageNumbers));
    const bool pagerDisplaySuppressed = environment.widescreen || environment.cutscene.playing ||
        !runtime.hud || !environment.wantsHud;
    runtime.pagerText = textUtf8(storylandUpdateMissionPager(environment,pagerDisplaySuppressed));
    if(wasPlaying&&!environment.cutscene.playing&&environment.cutscene.finished) {
        environment.restorePlayerAfterCutscene();
        runtime.jumpLaunching=runtime.jumpFromLaunch=runtime.jumpHorizontalValid=false;
        runtime.jumpHorizontalVelocity={};runtime.verticalSpeed=0;
        runtime.airborneTime=runtime.landingTime=0;
        runtime.playerFall={};
        runtime.playerReaction.reset();runtime.vehicleExitPose={};
    }
    storylandUpdateScriptCameraFocus(environment);
    if(environment.camera.restoreRequested||runtime.entryCameraResetPending) {
        const bool aiming=(runtime.manualAimActive&&camera.manualAimActive)||
            (runtime.freeAimActive&&camera.freeAimActive);
        storylandResetStoriesCameraForTransition(camera,playerPosition,playerHeading,aiming);
        environment.camera.restoreRequested=false;
        runtime.entryCameraResetPending=false;
    }
    camera.aimPlayerRootValid = storylandPedNativeRootWorldPoint(player.model, playerPosition,
        playerHeading, 1.8f, camera.aimPlayerRoot);
    camera.aimNativeWeapon = runtime.equippedWeapon < runtime.weapons.size() ?
        runtime.weapons[runtime.equippedWeapon].nativeId : -1;
    camera.aimFireType = runtime.equippedWeapon < runtime.weapons.size() ?
        runtime.weapons[runtime.equippedWeapon].fireType : -1;
    camera.aimTargetRootValid = storylandPlayTestTargetNativeRoot(runtime, camera.aimTargetRoot);
    camera.aimLocked = storylandHasPlayTestTarget(runtime) && !environment.camera.active;
    camera.aimTarget = runtime.targetPosition;
    const bool vehicleCamera=runtime.occupiedVehicle>=0&&
        size_t(runtime.occupiedVehicle)<runtime.vehicles.size()&&size_t(runtime.occupiedVehicle)<vehicles.size();
    runtime.vehicleEffectsLookingForward=true;
    if(vehicleCamera) {
        const auto& vehicle=runtime.vehicles[size_t(runtime.occupiedVehicle)];
        const auto& actor=vehicles[size_t(runtime.occupiedVehicle)];
        if(environment.cameraControlEnabled&&!environment.camera.active&&!environment.cutscene.playing&&
            !environment.widescreen&&!environment.cameraZoom.vehicle) {
            const size_t inward=environment.controllerMode==1?8:13;
            if(environment.padState[inward]&&!environment.previousPadState[inward])
                environment.cameraVehicleZoom=storylandCycleVehicleCameraMode(environment.cameraVehicleZoom,-1,
                    int32_t(vehicle.nativeDefinition.modelId));
            if(environment.controllerMode==1&&environment.padState[9]&&!environment.previousPadState[9])
                environment.cameraVehicleZoom=storylandCycleVehicleCameraMode(environment.cameraVehicleZoom,1,
                    int32_t(vehicle.nativeDefinition.modelId));
        }
        camera.vehicleMode=environment.cameraVehicleZoom;
        if(!environment.camera.active&&!runtime.manualAimActive&&!runtime.freeAimActive) {
            StorylandVehicleCameraInput vehicleInput;
            vehicleInput.vehicleSlot=runtime.occupiedVehicle;
            if(environment.cameraControlEnabled) {
                vehicleInput.look=cameraInput;
                vehicleInput.lookLeft=storylandVcsPadState(environment,8)!=0;
                vehicleInput.lookRight=storylandVcsPadState(environment,9)!=0;
                vehicleInput.lookBehind=storylandVcsPadState(environment,10)!=0;
                if(!cameraInput.left&&!cameraInput.right&&cameraInput.yawDelta==0)
                    vehicleInput.look.yawDelta=-float(environment.padState[2])/128*100*seconds;
                if(!cameraInput.up&&!cameraInput.down&&cameraInput.pitchDelta==0)
                    vehicleInput.look.pitchDelta=float(environment.padState[3])/128*70*seconds;
            }
            vehicleInput.scriptedZoom=environment.cameraZoom.vehicle;
            vehicleInput.zoom=environment.cameraZoom.vehicleCurrent;
            runtime.vehicleEffectsLookingForward=!(vehicleInput.lookBehind||vehicleInput.lookLeft||vehicleInput.lookRight);
            storylandUpdateVehicleCamera(camera,vehicle,actor,vehicleInput,seconds,archive.nativeWorld());
        }
    } else {
        camera.requestedDistance=storylandScriptCameraDistance(environment,3.5f,false);
        if(!environment.camera.active&&!runtime.manualAimActive&&!runtime.freeAimActive)
            storylandUpdateStoriesCamera(camera,playerPosition,playerHeading,
                environment.cameraControlEnabled?cameraInput:StorylandPlayTestCameraInput{},
                seconds,archive.nativeWorld(),true,vcs()&&!archive.isPsp()&&
                !environment.activeVcsCharacterAttachment(0));
    }
    if(environment.camera.active||runtime.manualAimActive||runtime.freeAimActive) {
        camera.vehicleCameraActive=false;
        camera.nativeFollowPedActive=false;
    }
    if(environment.camera.active&&environment.camera.positionSet&&environment.camera.targetSet) {
        auto& scriptCamera=environment.camera;
        const StorylandNativePoint previousEye=camera.initialized?camera.eye:
            StorylandNativePoint{scriptCamera.x,scriptCamera.y,scriptCamera.z};
        const StorylandNativePoint previousTarget=camera.initialized?camera.target:
            StorylandNativePoint{scriptCamera.targetX,scriptCamera.targetY,scriptCamera.targetZ};
        storylandApplyScriptCamera(scriptCamera,environment.gameMilliseconds,previousEye,previousTarget,
            camera.eye,camera.target);
        camera.initialized=true;
        camera.aimLocked=false;
        const float dx=camera.target.x-camera.eye.x,dy=camera.target.y-camera.eye.y;
        const float horizontal=std::hypot(dx,dy);
        if(horizontal>.0001f)camera.yaw=std::atan2(dy,dx)/radians;
        camera.pitch=std::atan2(camera.target.z-camera.eye.z,horizontal)/radians;
    }
    runtime.vehicleEffectsFirstPerson=camera.vehicleCameraActive&&camera.vehicleMode==0;
    if(vcs())StorylandVcsIntoxication::advance(environment);
    storylandApplyScriptViewEffects(camera,environment);
    const auto& shake=environment.cameraShake;
    const float shakeElapsed=float(uint32_t(environment.gameMilliseconds-shake.startedAt));
    if(shake.strength>shakeElapsed*.00028f)
        storylandSampleCameraShake(environment);
    const int cameraVehicle=runtime.occupiedVehicle;
    if(cameraVehicle>=0&&size_t(cameraVehicle)<runtime.vehicles.size()&&
        size_t(cameraVehicle)<vehicles.size()) {
        const auto& vehicle=runtime.vehicles[size_t(cameraVehicle)];
        storylandUpdateStoriesSpeedCamera(camera,environment,vehicle,vehicles[size_t(cameraVehicle)],seconds,
            !environment.camera.active&&!environment.cutscene.playing&&!environment.widescreen&&
            camera.vehicleCameraActive&&camera.vehicleMode>=0&&camera.vehicleMode<=3);
    } else storylandResetVehicleSpeedCamera(camera);
    animationTime+=seconds;
    const float velocityX = (playerPosition.x - beforeMovement.x) / seconds;
    const float velocityY = (playerPosition.y - beforeMovement.y) / seconds;
    const float facing = playerHeading * radians;
    playerBlend.measuredSpeed = std::hypot(velocityX, velocityY);
    playerBlend.movementForward = velocityX * std::cos(facing) + velocityY * std::sin(facing);
    playerBlend.movementRight = velocityX * std::sin(facing) - velocityY * std::cos(facing);
    playerBlend.walkSpeed = runtime.walkSpeed;
    playerBlend.runSpeed = runtime.runSpeed;
    playerBlend.nativeMoveRatio=runtime.playerMoveBlend;
    playerBlend.nativeSprintWeight=runtime.playerSprintBlend;
    playerBlend.nativeStopVariant=runtime.playerStopVariant;
    playerBlend.nativeStopTime=runtime.playerStopTime;
    runtime.jumpRunBlend=playerBlend.gaitWeights[2];
    runtime.jumpSprintBlend=playerBlend.gaitWeights[3];
    if (runtime.health <= 0) playerMotion = StorylandPlayTestMotion::Dead;
    else if (runtime.playerWater.active) {
        playerMotion = runtime.playerWater.drowning ? StorylandPlayTestMotion::Drown :
            runtime.movementSpeed > .1f ? (input.sprinting ? StorylandPlayTestMotion::SwimFast :
                StorylandPlayTestMotion::Swim) : StorylandPlayTestMotion::SwimIdle;
    } else if ((runtime.jumpLaunching || !runtime.grounded) && runtime.occupiedVehicle < 0) {
        playerMotion = runtime.jumpLaunching || runtime.jumpFromLaunch ?
            StorylandPlayTestMotion::Jump : StorylandPlayTestMotion::Fall;
    } else if (runtime.landingTime > 0) playerMotion = StorylandPlayTestMotion::Land;
    else if (storylandPlayTestAimBlocksMovement(runtime)) playerMotion = StorylandPlayTestMotion::Idle;
    else if (playerBlend.measuredSpeed > .1f && runtime.occupiedVehicle < 0) {
        playerMotion = runtime.playerSprintActive ? StorylandPlayTestMotion::Sprint :
            environment.playerWalkTask.active?
                (vcs()&&environment.playerWalkTask.moveMode!=StorylandScriptPlayerMoveMode::Walk?
                    StorylandPlayTestMotion::Run:StorylandPlayTestMotion::Walk):
                input.running ? StorylandPlayTestMotion::Run : StorylandPlayTestMotion::Walk;
    } else playerMotion = StorylandPlayTestMotion::Idle;
    if(storylandPlayerLocomotionSoundAllowed(runtime)) {
        const float distance=std::hypot(playerPosition.x-beforeMovement.x,playerPosition.y-beforeMovement.y);
        const float stride=playerAnimation.locomotionStrideAtSpeed(playerBlend.measuredSpeed);
        const float phaseAdvance=distance/std::max(.05f,stride);
        playerBlend.locomotionPhase=std::fmod(playerBlend.locomotionPhase+phaseAdvance,1.0f);
        playerBlend.distanceDrivenPhase=true;
        if(vcs()&&playerMotion>=StorylandPlayTestMotion::Walk&&playerMotion<=StorylandPlayTestMotion::Sprint) {
            const float duration=playerAnimation.locomotionDuration(playerMotion);
            storylandQueuePlayerLocomotionSound(runtime,playerPosition,
                playerBlend.locomotionPhase,phaseAdvance,duration);
            storylandObserveVcsFootstepMarks(environment.playerVcsFootstepMarks,runtime.playerStats.statIndex,
                playerBlend.locomotionPhase*duration,phaseAdvance*duration,duration);
        }
    }
    const bool reloading = runtime.equippedWeapon < runtime.inventory.size() &&
        runtime.inventory[runtime.equippedWeapon].reloading > 0;
    const bool firing = runtime.playerShotSerial != playerBlend.weaponShotSerial ||
        (playerBlend.weaponIndex == runtime.equippedWeapon && playerBlend.upperActive &&
            !playerBlend.weaponReloading);
    const bool targetAiming = storylandHasPlayTestTarget(runtime) || runtime.manualAimActive || runtime.freeAimActive;
    playerBlend.allowAimedLocomotion=runtime.extendedAndSaFeatures||runtime.extendedLcsCrouch;
    prepareWeaponPose(playerAnimation, playerBlend, runtime, runtime.equippedWeapon,
        targetAiming || firing, reloading,
        playerBlend.measuredSpeed > .1f, runtime.playerShotSerial, animationTime,
        -1, false, runtime.playerMelee.sequence);
    playerBlend.weaponAimYaw = playerBlend.weaponAimPitch = 0;
    playerBlend.weaponAimDirectionValid = false;
    if (runtime.playerShoulderValid && targetAiming) {
        const auto& target = runtime.freeAimActive ? runtime.freeAimTarget :
            runtime.manualAimActive ? runtime.manualAimTarget : runtime.targetPosition;
        const float dx = target.x - runtime.playerShoulder.x;
        const float dy = target.y - runtime.playerShoulder.y;
        const float dz = target.z - runtime.playerShoulder.z;
        const float distance = std::hypot(dx, dy);
        if (distance > .0001f && std::isfinite(distance) && std::isfinite(dz)) {
            playerBlend.weaponAimYaw = std::remainder(std::atan2(dy, dx) / radians - playerHeading, 360.0f);
            playerBlend.weaponAimPitch = std::atan2(dz, distance) / radians;
            playerBlend.weaponAimDirectionValid = true;
        }
    }
    // Native one-arm free fire points along the actor's heading. Holding aim
    // without a target does not create a pose or preserve the last target's IK.
    if (!targetAiming && firing && runtime.equippedWeapon < runtime.weapons.size() &&
        (runtime.weapons[runtime.equippedWeapon].nativeFlags & 0x80u) != 0) {
        playerBlend.weaponAimDirectionValid = true;
    }
    const auto association=environment.scriptedCharacterAnimations.find(vcs() ? 0 : 1);
    const auto waitAnimation=environment.pedAnimationRequests.find(0);
    float waitAnimationTime=0;
    bool waitAnimationActive=false;
    if(vcs()&&(environment.playerWaitState==23||environment.playerWaitState==24)&&
        waitAnimation!=environment.pedAnimationRequests.end()&&waitAnimation->second.durationMilliseconds>=0) {
        const uint32_t started=environment.playerWaitDeadline-
            uint32_t(waitAnimation->second.durationMilliseconds);
        waitAnimationTime=float(environment.gameMilliseconds-started)*.001f;
        const float duration=playerAnimation.retailAnimationDuration(
            waitAnimation->second.animationId,waitAnimation->second.animationGroup);
        waitAnimationActive=environment.playerWaitVersionB||
            (std::isfinite(duration)&&duration>0&&waitAnimationTime<duration);
    }
    bool sampled=true;
    if(runtime.health<=0&&runtime.occupiedVehicle<0)
        sampled=playerAnimation.sampleDeathAnimation(player.model,runtime.playerDeath.animation,
            runtime.playerDeathTime,playerPoints,error,&playerBlend.boneFrames,true,
            &playerBlend.poseTransition,runtime.playerDeath.blendRate,runtime.playerDeath.speed);
    else if(vcs()&&runtime.playerWater.diving&&divingAnimations.available)
        sampled=storylandSampleSaDivingPose(playerAnimation,player.model,divingAnimations,
            runtime.playerWater,playerPoints,error,&playerBlend.boneFrames);
    else if(vcs()&&!environment.inVehicle&&!storylandPlayerBulletReactionActive(runtime)&&
        (environment.playerCrouch.active||environment.playerCrouch.poseFade.present))
        sampled=storylandSampleCrouchPose(environment,runtime,0,playerAnimation,player.model,
            playerBlend,playerPoints,error);
    else if(association!=environment.scriptedCharacterAnimations.end())
        sampled=playerAnimation.sampleScriptAnimation(player.model,association->second.animation,association->second.group,
            environment.characterAnimationTime(vcs() ? 0 : 1,0),association->second.weight,playerBlend,playerPoints,error);
    else if(waitAnimationActive)
        sampled=playerAnimation.sampleScriptAnimation(player.model,waitAnimation->second.animationId,
            waitAnimation->second.animationGroup,waitAnimationTime,1,playerBlend,playerPoints,error);
    else if(runtime.playerJetSkiFallActive) {
        const auto& fall=runtime.playerJetSkiFall;
        sampled=storylandVcsCarjackedContinuation(fall)?
            storylandSampleVcsCarjackedContinuation(playerAnimation,player.model,runtime,vehicles,
                fall,playerPosition,playerHeading,animationTime,playerBlend,playerPoints,error):
            playerAnimation.sampleRetailAnimationId(player.model,fall.animation(),fall.time,
            playerPoints,error,0,&playerBlend.boneFrames,StorylandPlayTestMotion::Idle,
            -1,1,true,&playerBlend.poseTransition,storylandVcsPedFallPoseBlend(fall,true));
    }
    else if(runtime.vehicleExitPose.active) {
        const auto& pose=runtime.vehicleExitPose;
        sampled=playerAnimation.sampleRetailAnimationId(player.model,pose.animation,pose.elapsed,
            playerPoints,error,pose.group,&playerBlend.boneFrames,StorylandPlayTestMotion::Idle,
            -1,1,true,&playerBlend.poseTransition,storylandVehicleExitPoseBlend(pose),-1,-1,
            !player.model.isMobileLcsDff()&&!player.model.isManhuntModel());
    }
    else if(storylandPlayerBulletReactionActive(runtime)) {
        const auto& hit=runtime.playerBulletReaction;
        playerBlend.upperActive=false;playerBlend.weaponAiming=false;playerBlend.weaponAimBlend=0;
        sampled=playerAnimation.sampleRetailAnimationId(player.model,hit.animation,hit.time(runtime.elapsed),
            playerPoints,error,0,&playerBlend.boneFrames,StorylandPlayTestMotion::Idle,-1,1,true,
            &playerBlend.poseTransition,hit.blendRate());
    }
    else if(runtime.playerReaction.active()) {
        const auto pose=storylandPlayerReactionPose(runtime.playerReaction);
        sampled=playerAnimation.sampleScriptAnimation(player.model,pose.animation,pose.group,pose.seconds,
            1,playerBlend,playerPoints,error);
    } else if(runtime.occupiedVehicle<0 && !runtime.playerWater.active && !runtime.extendedMovement.busy() &&
        runtime.grounded && !runtime.jumpLaunching && runtime.landingTime<=0 &&
        (runtime.extendedMovement.crouched||runtime.extendedMovement.crouchStanceBlend>0) &&
        extendedCrouchAvailable) {
        const auto& crouch=runtime.extendedMovement;
        sampled=playerAnimation.sampleExternalCrouchPose(player.model,extendedCrouchAnimations,
            crouch.crouchMotionDirection<0?"GunCrouchBwd":"GunCrouchFwd",
            crouch.crouchAnimationTime,crouch.crouchStanceBlend,crouch.crouchMoveBlend,
            playerPoints,error,&playerBlend.boneFrames,&playerBlend,animationTime);
    } else if(vcs()&&runtime.occupiedVehicle<0&&!runtime.extendedMovement.busy()&&
        !runtime.playerWater.active&&(runtime.jumpLaunching||runtime.playerFall.active)&&
        runtime.debugMovement==StorylandDebugMovement::Walking&&
        (runtime.jumpLaunching||!runtime.grounded||
            (runtime.landingTime>0&&runtime.playerFall.landingAnimationId>=0))) {
        const auto pose=storylandVcsAirbornePose(runtime.playerFall,runtime.jumpLaunching,
            runtime.jumpLaunchTime,runtime.grounded,runtime.landingTime);
        sampled=playerAnimation.sampleRetailAnimationId(player.model,pose.animation,
            pose.seconds,playerPoints,error,0,
            &playerBlend.boneFrames,pose.landing?StorylandPlayTestMotion::Land:StorylandPlayTestMotion::Fall,
            -1,1,true,&playerBlend.poseTransition,8.0f,
            storylandVcsFallAssociationFlags(pose.animation,pose.landing));
    } else if(runtime.occupiedVehicle<0&&runtime.grounded&&runtime.playerSprint.pose(vcs())>=0&&
        runtime.vehicleAction==StorylandVehicleAction::None) {
        sampled=playerAnimation.sampleRetailAnimationId(player.model,runtime.playerSprint.pose(vcs()),
            runtime.playerSprint.poseTime(),playerPoints,error,0,&playerBlend.boneFrames,
            StorylandPlayTestMotion::Idle,-1,runtime.playerSprint.poseWeight(),true,
            &playerBlend.poseTransition,4.0f);
    } else if(runtime.occupiedVehicle<0&&(runtime.vehicleAction==StorylandVehicleAction::None||
        storylandVehicleApproachLocomotion(runtime.vehicleAction,
            runtime.vehicleTransitionPrepared,runtime.vehiclePhase.phase))) {
        const auto motion=runtime.vehicleAction!=StorylandVehicleAction::None?
            (runtime.vehicleApproachSpeed>runtime.walkSpeed?StorylandPlayTestMotion::Run:StorylandPlayTestMotion::Walk):playerMotion;
        sampled=playerAnimation.sampleTree(player.model,motion,animationTime,playerBlend,playerPoints,error);
    }
    // The renderer owns the seated/entry/exit association. Sampling an on-foot
    // idle here would replace its local-bone transition source every frame.
    if(sampled&&!vcs()&&runtime.occupiedVehicle<0)
        sampled=storylandApplyLcsStandingLook(environment,1,player.model,playerPosition,
            playerHeading,playerBlend,playerPoints,error);
    if(!sampled) {fail("Player native animation: "+error);return;}
    statusMessage=dyomSession?dyomStatus():playingGame?program.status():"Play Test: native world and player simulation.";
}

bool StorylandLinuxSession::samplePedestrian(size_t index,
    std::vector<StorylandModelPoint>& points,std::string& error,bool skinGeometry) {
    points.clear();
    if(index>=pedestrians.size()||index>=runtime.pedestrians.size()) {
        error="Pedestrian pose references an unavailable native actor.";
        return false;
    }
    const auto& actor=pedestrians[index];
    const auto& ped=runtime.pedestrians[index];
    if(ped.model>=pedestrianAssets.size()||ped.scriptHandle>=0) {
        error="Pedestrian pose requires its original resident model and ambient owner.";
        return false;
    }
    pedestrianBlends.resize(pedestrians.size());
    auto& blend=pedestrianBlends[index];
    const auto& asset=pedestrianAssets[ped.model];
    if(ped.serviceVehicleIdentity) {
        const size_t slot=storylandServiceCrewVehicleSlot(runtime,vehicles,ped.serviceVehicleIdentity);
        if(slot>=vehicles.size()||slot>=runtime.vehicles.size())return false;
        const auto& vehicle=runtime.vehicles[slot];
        auto options=pedestrianAnimation.vehicleAnimationOptions(vehicle.nativeDefinition);
        options.seatRelative=true;options.passenger=true;
        options.rightHandSide=(ped.servicePassengerSeat==0);
        options.elapsedSeconds=animationTime;options.transition=&blend.poseTransition;
        return pedestrianAnimation.sampleVehicleAction(asset.model,"sit",0,options,points,error,
            nullptr,&blend.boneFrames);
    }
    const int32_t group=ped.stats.valid?ped.stats.animationGroup:asset.pedStats.animationGroup;
    blend.allowAimedLocomotion=runtime.extendedAndSaFeatures;
    StorylandNativePoint assailantAim{};
    const bool hasAssailantAim=storylandVcsPoliceAssailantAimTarget(runtime,pedestrians,index,assailantAim);
    const bool combatTarget=ped.hostile||hasAssailantAim;
    if(!pedestrianAnimation.selectLocomotionGroup(group,error))return false;
    const float seconds=animationTime+float(index)*.17f;
    prepareWeaponPose(pedestrianAnimation,blend,runtime,ped.weapon,
        combatTarget&&ped.health>0,ped.reloading>0,
        ped.melee.active?ped.melee.moving:ped.combatSpeed>.15f,
        ped.shotSerial,seconds,group,ped.isPolice(vcs()),ped.melee.sequence);
    const bool nativeRanged=!vcs()&&ped.ranged.active&&ped.ranged.weapon==ped.weapon&&
        ped.health>0&&!ped.water.active&&ped.weapon<runtime.weapons.size();
    blend.weaponAnimationTime=nativeRanged?ped.ranged.time:-1.0f;
    blend.weaponAnimationWeight=nativeRanged?ped.ranged.blend:1.0f;
    if(nativeRanged) {
        blend.upperActive=true;
        blend.weaponReloading=ped.ranged.reloading;
        blend.weaponAiming=ped.hostile;
        if(!pedestrianAnimation.selectWeaponAction(runtime.weapons[ped.weapon],
            ped.ranged.reloading?"reload":"fire",ped.combatSpeed>.15f,
            ped.melee.sequence,group,ped.hostile,ped.isPolice(false))) {
            error="The pedestrian's native weapon association is unavailable.";
            return false;
        }
    }
    blend.walkSpeed=pedestrianAnimation.locomotionSpeed(StorylandPlayTestMotion::Walk);
    blend.runSpeed=pedestrianAnimation.locomotionSpeed(StorylandPlayTestMotion::Run);
    if(ped.state==StorylandPedState::Combat)blend.measuredSpeed=std::max(0.0f,ped.combatSpeed);
    else if(ped.state==StorylandPedState::Idle)blend.measuredSpeed=0;
    blend.weaponAimDirectionValid=false;
    StorylandNativePoint shoulder;
    const bool aimAtAssailant=hasAssailantAim&&ped.state==StorylandPedState::Combat;
    if(ped.health>0&&(aimAtAssailant||(ped.hostile&&runtime.health>0&&runtime.playerTorsoValid))&&
        storylandPoseBoneWorldPoint(asset.model,blend.boneFrames,22,
            actor.position,actor.yaw,1.8f,shoulder)) {
        const auto& target=aimAtAssailant?assailantAim:runtime.playerTorso;
        const float dx=target.x-shoulder.x;
        const float dy=target.y-shoulder.y;
        const float dz=target.z-shoulder.z;
        const float horizontal=std::hypot(dx,dy);
        if(horizontal>.0001f&&std::isfinite(horizontal)&&std::isfinite(dz)) {
            blend.weaponAimYaw=std::remainder(std::atan2(dy,dx)/radians-actor.yaw,360.0f);
            blend.weaponAimPitch=std::atan2(dz,horizontal)/radians;
            blend.weaponAimDirectionValid=true;
        }
    }
    const auto motion=ped.state==StorylandPedState::Dead?StorylandPlayTestMotion::Dead:
        ped.water.active?storylandPedWaterMotion(vcs(),ped.water,ped.combatSpeed):
        ped.coughTime>0?StorylandPlayTestMotion::Cough:
        (ped.state==StorylandPedState::Idle||
            (ped.state==StorylandPedState::Combat&&ped.combatSpeed<.1f))?StorylandPlayTestMotion::Idle:
        (ped.patrol.running||(ped.state==StorylandPedState::Flee&&(vcs()||!ped.fleeWalking)))?
            StorylandPlayTestMotion::Run:StorylandPlayTestMotion::Walk;
    const auto pairedPose = storylandHeldVictimPose(runtime, StorylandReactionTargetKind::Ambient,
        int32_t(index), ped.stats.modelId, ped.health <= 0 ? ped.pairedDeathPose : StorylandPlayerReactionPose{});
    if (pairedPose.animation >= 0) {
        blend.upperActive = false; blend.weaponAimBlend = 0; blend.weaponAiming = false;
        return pedestrianAnimation.sampleScriptAnimation(asset.model, pairedPose.animation, pairedPose.group,
            pairedPose.seconds, 1, blend, points, error, skinGeometry);
    }
    if(ped.state==StorylandPedState::Dead)
        return pedestrianAnimation.sampleDeathAnimation(asset.model,ped.death.animation,ped.deathTime,
            points,error,&blend.boneFrames,skinGeometry,&blend.poseTransition,
            ped.death.blendRate,ped.death.speed);
    if(storylandPlayerReactionHoldsTarget(runtime.playerReaction,
        StorylandReactionTargetKind::Ambient,int32_t(index),ped.stats.modelId)) {
        const auto pose=storylandPlayerReactionPose(runtime.playerReaction,true);
        return pedestrianAnimation.sampleScriptAnimation(asset.model,pose.animation,pose.group,
            pose.seconds,1,blend,points,error,skinGeometry);
    }
    if(ped.health>0&&!ped.water.active&&ped.coughTime<=0&&ped.bulletReaction.active(runtime.elapsed)) {
        blend.upperActive=false;blend.weaponAiming=false;blend.weaponAimBlend=0;
        if(pedestrianAnimation.sampleRetailAnimationId(asset.model,ped.bulletReaction.animation,
            ped.bulletReaction.time(runtime.elapsed),points,error,0,&blend.boneFrames,
            StorylandPlayTestMotion::Idle,-1,1,skinGeometry,&blend.poseTransition,
            ped.bulletReaction.blendRate()))return true;
    }
    if(vcs()&&ped.emergencyTreating&&ped.health>0&&!ped.water.active&&
        pedestrianAnimation.sampleScriptAnimation(asset.model,223,26,
            ped.emergencyTreatmentTime,1,blend,points,error,skinGeometry))return true;
    if(ped.threatAnimation>=0&&ped.health>0&&!ped.water.active)
        return pedestrianAnimation.sampleRetailAnimationId(asset.model,ped.threatAnimation,
            ped.threatAnimationTime,points,error,0,&blend.boneFrames,motion,
            blend.locomotionPhase,ped.threatBlend,skinGeometry,&blend.poseTransition);
    if(ped.contact.active()&&!ped.water.active&&ped.coughTime<=0&&!ped.melee.active&&!ped.ranged.active&&
        pedestrianAnimation.sampleRetailAnimationId(asset.model,ped.contact.animation,
            ped.contact.animationTime,points,error,0,&blend.boneFrames,motion,
            blend.locomotionPhase,ped.contact.weight(),skinGeometry,&blend.poseTransition,8))return true;
    if(ped.extendedCrouchBlend>0&&ped.health>0&&!ped.water.active&&ped.coughTime<=0&&
        ped.weapon<runtime.weapons.size()) {
        const auto& weapon=runtime.weapons[ped.weapon];
        const int animationId=ped.extendedCrouch?215:155;
        const int animationGroup=ped.extendedCrouch?weapon.animationGroup:0;
        const float duration=pedestrianAnimation.retailAnimationDuration(animationId,animationGroup);
        const float elapsed=duration>0?std::clamp(ped.extendedCrouch?
                    (ped.fireFlash>0?weapon.crouchFireTime:weapon.crouchLoopStart):0.0f,
                    0.0f,std::max(0.0f,duration-.001f)):0.0f;
        if(pedestrianAnimation.sampleCrouchPose(asset.model,animationId,animationGroup,elapsed,
            ped.extendedCrouchBlend,blend,points,error,skinGeometry))return true;
    }
    return pedestrianAnimation.sampleTree(asset.model,motion,seconds,blend,points,error,skinGeometry);
}

bool StorylandLinuxSession::sampleMissionCharacter(int32_t handle,
    std::vector<StorylandModelPoint>& points, std::string& error, bool skinGeometry) {
    if(!sampleMissionCharacterBasePose(handle,points,error,skinGeometry))return false;
    if(vcs()||!environment.scriptedPedLooks.count(handle))return true;
    const auto& character=environment.missionCharacters.at(handle);
    if(character.vehicle>=0&&!character.attached)return true;
    const auto& model=missionAssets.at(character.model).model;
    return storylandApplyLcsStandingLook(environment,handle,model,
        {character.x,character.y,character.z},character.heading+90,
        missionBlends[handle],points,error,skinGeometry);
}

bool StorylandLinuxSession::sampleMissionCharacterBasePose(int32_t handle,
    std::vector<StorylandModelPoint>& points,std::string& error,bool skinGeometry) {
    points.clear();
    const auto characterEntry = environment.missionCharacters.find(handle);
    if (characterEntry == environment.missionCharacters.end()) return false;
    const auto& character = characterEntry->second;
    const auto assetEntry = missionAssets.find(character.model);
    if (assetEntry == missionAssets.end()) return false;
    const auto& asset = assetEntry->second;
    auto& blend = missionBlends[handle];
    blend.allowAimedLocomotion=runtime.extendedAndSaFeatures;
    auto& combat = runtime.missionCombat[handle];
    const auto pairedPose = storylandHeldVictimPose(runtime, StorylandReactionTargetKind::Mission,
        handle, uint32_t(character.model), character.health <= 0 ? combat.pairedDeathPose : StorylandPlayerReactionPose{});
    if (pairedPose.animation >= 0) {
        blend.upperActive = false; blend.weaponAimBlend = 0; blend.weaponAiming = false;
        return pedestrianAnimation.sampleScriptAnimation(asset.model, pairedPose.animation, pairedPose.group,
            pairedPose.seconds, 1, blend, points, error, skinGeometry);
    }
    const auto fall=environment.vcsPedFalls.find(handle);
    if(vcs()&&character.alive&&character.health>0&&fall!=environment.vcsPedFalls.end())
        return storylandVcsCarjackedContinuation(fall->second)?
            storylandSampleVcsCarjackedContinuation(pedestrianAnimation,asset.model,runtime,vehicles,
                fall->second,{character.x,character.y,character.z},character.heading+90,
                animationTime,blend,points,error,skinGeometry):
            pedestrianAnimation.sampleRetailAnimationId(asset.model,fall->second.animation(),
            fall->second.time,points,error,0,&blend.boneFrames,StorylandPlayTestMotion::Idle,
            -1,1,skinGeometry,&blend.poseTransition,
            storylandVcsPedFallPoseBlend(fall->second,character.alive&&character.health>0));
    if(!character.alive||character.health<=0) {
        const float elapsed=character.deathRecorded?
            float(environment.gameMilliseconds-character.deathStartedAt)*.001f:0.0f;
        const int32_t death=storylandNativeDeathAnimation(vcs(),character.animationId)?character.animationId:-1;
        blend.upperActive=false;blend.weaponAiming=false;blend.weaponAimBlend=0;
        return pedestrianAnimation.sampleDeathAnimation(asset.model,death,elapsed,points,error,
            &blend.boneFrames,skinGeometry,&blend.poseTransition,4);
    }
    const auto nativeSelection = environment.characterCurrentWeapons.find(handle);
    const int nativeWeapon = vcs() ? character.currentWeapon :
        nativeSelection == environment.characterCurrentWeapons.end() ? 0 : nativeSelection->second;
    const auto selectedWeapon = std::find_if(runtime.weapons.begin(), runtime.weapons.end(),
        [&](const auto& weapon) { return weapon.nativeId == nativeWeapon; });
    combat.weapon = selectedWeapon == runtime.weapons.end() ? SIZE_MAX :
        size_t(selectedWeapon - runtime.weapons.begin());
    combat.nativeWeapon = nativeWeapon;
    const auto target = environment.characterTargets.find(handle);
    StorylandNativePoint vehicleAim{};
    const bool vehicleAiming = storylandMissionVehicleAimPoint(runtime, handle, vehicleAim);
    const bool aiming = vehicleAiming || (target != environment.characterTargets.end() &&
        target->second.kind != StorylandMissionEnvironment::TargetKind::None);
    const int32_t locomotionGroup=vcs()?
        environment.vcsFollowerLocomotionGroup(character,character.animationGroup):
        environment.lcsFollowerLocomotionGroup(handle,character,character.animationGroup);
    if(!pedestrianAnimation.selectLocomotionGroup(locomotionGroup,error)&&
        !pedestrianAnimation.selectLocomotionGroup(0,error))return false;
    blend.measuredSpeed = character.movementSpeed;
    prepareWeaponPose(pedestrianAnimation, blend, runtime, combat.weapon, aiming,
        combat.reloading > 0, character.walkingToTarget, combat.shotSerial,
        animationTime, locomotionGroup, character.type == 6, combat.melee.sequence);
    blend.walkSpeed = pedestrianAnimation.locomotionSpeed(StorylandPlayTestMotion::Walk);
    blend.runSpeed = pedestrianAnimation.locomotionSpeed(StorylandPlayTestMotion::Run);
    blend.weaponAimDirectionValid = false;
    StorylandNativePoint shoulder, aimPoint;
    bool aimPointValid = false;
    if (target != environment.characterTargets.end() &&
        target->second.kind == StorylandMissionEnvironment::TargetKind::Character) {
        if (target->second.handle == (vcs() ? 0 : 1) && runtime.playerTorsoValid) {
            aimPoint = runtime.playerTorso;
            aimPointValid = true;
        } else {
            const auto aimed = runtime.missionCombat.find(target->second.handle);
            if (aimed != runtime.missionCombat.end() && aimed->second.anchorsValid) {
                aimPoint = aimed->second.torso;
                aimPointValid = true;
            }
        }
    } else if (vehicleAiming) {
        aimPoint = vehicleAim;
        aimPointValid = true;
    }
    if (aimPointValid && storylandPoseBoneWorldPoint(asset.model, blend.boneFrames,
        22, {character.x, character.y, character.z}, character.heading + 90, 1.8f, shoulder)) {
        const float dx = aimPoint.x - shoulder.x, dy = aimPoint.y - shoulder.y;
        const float dz = aimPoint.z - shoulder.z;
        const float horizontal = std::hypot(dx, dy);
        if (horizontal > .0001f && std::isfinite(horizontal) && std::isfinite(dz)) {
            blend.weaponAimYaw = std::remainder(std::atan2(dy, dx) / radians - character.heading - 90, 360.f);
            blend.weaponAimPitch = std::atan2(dz, horizontal) / radians;
            blend.weaponAimDirectionValid = true;
        }
    }
    if(storylandPlayerReactionHoldsTarget(runtime.playerReaction,StorylandReactionTargetKind::Mission,
        handle,uint32_t(character.model))) {
        const auto pose=storylandPlayerReactionPose(runtime.playerReaction,true);
        return pedestrianAnimation.sampleScriptAnimation(asset.model,pose.animation,pose.group,
            pose.seconds,1,blend,points,error,skinGeometry);
    }
    if(vcs()&&(character.vehicle<0||character.attached)&&(character.crouch.active||character.crouch.poseFade.present)&&
        !storylandMissionBulletReactionActive(runtime,handle))
        return storylandSampleCrouchPose(environment,runtime,handle,pedestrianAnimation,
            asset.model,blend,points,error,skinGeometry);
    if (combat.water.active)
        return pedestrianAnimation.sampleTree(asset.model,
            storylandPedWaterMotion(vcs(),combat.water,character.movementSpeed),
            animationTime,blend,points,error,skinGeometry);
    if (combat.coughTime>0)
        return pedestrianAnimation.sampleTree(asset.model,StorylandPlayTestMotion::Cough,
            animationTime,blend,points,error,skinGeometry);
    if (storylandMissionBulletReactionActive(runtime,handle)) {
        blend.upperActive=false;blend.weaponAiming=false;blend.weaponAimBlend=0;
        if (pedestrianAnimation.sampleRetailAnimationId(asset.model,combat.bulletReaction.animation,
            combat.bulletReaction.time(runtime.elapsed),points,error,0,&blend.boneFrames,
            StorylandPlayTestMotion::Idle,-1,1,skinGeometry,&blend.poseTransition,
            combat.bulletReaction.blendRate())) return true;
    }
    const bool moving=!character.positionFrozen&&!character.stayInSamePlace&&character.vehicle<0&&!character.attached&&
        character.movementSpeed>0;
    const auto motion=!moving?StorylandPlayTestMotion::Idle:!vcs()?
        (character.movementState==4?StorylandPlayTestMotion::Sprint:
            character.movementState==3?StorylandPlayTestMotion::Run:StorylandPlayTestMotion::Walk):
        (character.walkSpeed>4?StorylandPlayTestMotion::Sprint:
            character.walkSpeed>2?StorylandPlayTestMotion::Run:StorylandPlayTestMotion::Walk);
    const auto advanceLocomotion = [&]() {
        blend.measuredSpeed=moving?character.movementSpeed:0;
        if(!moving)blend.movementForward=blend.movementRight=0;
        if(!blend.scriptDistanceInitialized||character.walkedDistance<blend.scriptWalkedDistance) {
            blend.scriptDistanceInitialized=true;blend.locomotionPhase=0;
        } else {
            const float stride=pedestrianAnimation.locomotionStrideAtSpeed(character.movementSpeed);
            if(std::isfinite(stride)&&stride>0) {
                const double phaseAdvance=(character.walkedDistance-blend.scriptWalkedDistance)/stride;
                blend.locomotionPhase=float(std::fmod(double(blend.locomotionPhase)+
                    phaseAdvance,1.0));
                if(vcs()&&moving) {
                    const float duration=pedestrianAnimation.locomotionDuration(motion);
                    storylandObserveVcsFootstepMarks(characterEntry->second.vcsFootstepMarks,
                        character.pedStats.statIndex,blend.locomotionPhase*duration,float(phaseAdvance)*duration,duration);
                }
            }
        }
        blend.scriptWalkedDistance=character.walkedDistance;blend.distanceDrivenPhase=true;
    };
    if (character.animationId >= 0) {
        if (vcs()) advanceLocomotion();
        return pedestrianAnimation.sampleScriptAnimation(asset.model, character.animationId,
            character.animationGroup, environment.characterAnimationTime(handle, character.animationStartedAt),
            character.animationBlend, blend, points, error, skinGeometry,
            vcs() ? motion : StorylandPlayTestMotion::Idle, blend.locomotionPhase);
    }
    if (combat.gravityPositionValid && (!combat.grounded || combat.landingTime > 0)) {
        if(vcs()) {
            const auto pose=storylandVcsAirbornePose(combat.fall,false,0,combat.grounded,combat.landingTime);
            return pedestrianAnimation.sampleRetailAnimationId(asset.model,pose.animation,pose.seconds,
                points,error,0,&blend.boneFrames,StorylandPlayTestMotion::Idle,-1,1,
                skinGeometry,&blend.poseTransition,8,storylandVcsFallAssociationFlags(pose.animation,pose.landing));
        }
        const auto motion=combat.grounded?StorylandPlayTestMotion::Land:StorylandPlayTestMotion::Fall;
        const float time=combat.grounded?runtime.jumpLandingDuration-combat.landingTime:combat.airborneTime;
        return pedestrianAnimation.sampleMovementAction(asset.model,motion,time,points,error,
            -1,&blend.boneFrames,skinGeometry,&blend.poseTransition);
    }
    advanceLocomotion();
    if(combat.contact.active()&&character.vehicle<0&&!character.attached&&!character.positionFrozen&&
        !character.stayInSamePlace&&pedestrianAnimation.sampleRetailAnimationId(asset.model,
            combat.contact.animation,combat.contact.animationTime,points,error,0,&blend.boneFrames,
            motion,blend.locomotionPhase,combat.contact.weight(),skinGeometry,&blend.poseTransition,8))return true;
    return pedestrianAnimation.sampleTree(asset.model,motion,animationTime,blend,points,error,skinGeometry);
}

void StorylandLinuxSession::updateCombatPoseAnchors() {
    storylandAdvanceVcsCarjackVictims(pedestrianAnimation,runtime,vehicles,playerPosition);
    runtime.playerHeadValid = runtime.playerTorsoValid = runtime.playerShoulderValid = false;
    runtime.playerMuzzleValid = false;
    const auto muzzle = [&](const StorylandPlayTestAsset& asset,
        const StorylandAnimationBlendState& blend, const std::array<float,16>& standingFrame,
        size_t weapon, StorylandNativePoint& point) {
        if (weapon >= runtime.weapons.size()) return false;
        const auto& definition = runtime.weapons[weapon];
        if (!definition.fireOffsetValid) return false;
        std::array<float,16> frame;
        if (!storylandWeaponAttachmentFrame(asset.model.armatureBones(), blend.boneFrames,
            standingFrame, frame)) return false;
        return storylandWeaponFirePoint(frame,definition.fireOffset,point);
    };
    std::vector<StorylandModelPoint> points;
    std::string error;
    StorylandVehicleMatrix playerVehicleFrame{};
    auto playerQuery=playerBlend;
    const bool playerVehiclePose=runtime.health>0&&storylandSamplePlayerVehicleQueryPose(
        playerAnimation,player.model,runtime,vehicles,animationTime,playerQuery,playerVehicleFrame,error);
    if(playerVehiclePose) {
        runtime.playerHeadValid=storylandNativeRootPoseBoneWorldPoint(player.model,playerQuery.boneFrames,
            5,playerVehicleFrame,runtime.playerHead);
        runtime.playerTorsoValid=storylandNativeRootPoseBoneWorldPoint(player.model,playerQuery.boneFrames,
            3,playerVehicleFrame,runtime.playerTorso);
        runtime.playerShoulderValid=storylandNativeRootPoseBoneWorldPoint(player.model,playerQuery.boneFrames,
            22,playerVehicleFrame,runtime.playerShoulder);
        const auto oriented=storylandVehicleMatrixProduct(playerVehicleFrame,
            storylandScriptObjectMatrix(0,0,0,0,0,90-storylandPedModelForwardAngle(player.model)));
        runtime.playerMuzzleValid=muzzle(player,playerQuery,oriented,runtime.equippedWeapon,runtime.playerMuzzle);
    }
    std::array<float,16> driveByFrame{};
    const bool seatedDriveBy=vcs()&&runtime.health>0&&runtime.occupiedVehicle>=0&&
        size_t(runtime.occupiedVehicle)<runtime.vehicles.size()&&size_t(runtime.occupiedVehicle)<vehicles.size()&&
        runtime.vehicleAction==StorylandVehicleAction::None&&runtime.equippedWeapon<runtime.weapons.size()&&
        storylandPlayerSeatedDriveByVehicle(environment,runtime.weapons[runtime.equippedWeapon].nativeId,&driveByFrame);
    if(!seatedDriveBy)playerBlend.seatedDriveBy={};
    else {
        const auto& vehicle=runtime.vehicles[size_t(runtime.occupiedVehicle)];
        const auto& actor=vehicles[size_t(runtime.occupiedVehicle)];
        if(playerAnimation.sampleSeatedDriveBy(player.model,environment.occupiedMissionVehicle,
            runtime.playerVehicleSeat,storylandSeatedDriveByLocalTarget(environment,driveByFrame,runtime.manualAimActive),
            animationTime,playerBlend,points,error,false)) {
            const auto seat=storylandVehicleOccupantFrame(vehicle.nativeDefinition,
                {actor.position.x,actor.position.y,actor.position.z},actor.yaw,vehicle.nativeDefinition.previewMinimumZ,
                vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
                unsigned(runtime.playerVehicleSeat+1),actor.water,vehicle.craneCarried?&vehicle.craneFrame:nullptr,
                storylandVcsJetSkiNativeAngularZ(vehicle),vehicle.bikeVisualLean);
            const auto frame=storylandVehicleMatrixProduct(seat,
                storylandScriptObjectMatrix(0,0,0,0,0,90-storylandPedModelForwardAngle(player.model)));
            runtime.playerHeadValid=storylandPoseBoneWorldPoint(player.model,playerBlend.boneFrames,5,frame,runtime.playerHead);
            runtime.playerTorsoValid=storylandPoseBoneWorldPoint(player.model,playerBlend.boneFrames,3,frame,runtime.playerTorso);
            runtime.playerShoulderValid=storylandPoseBoneWorldPoint(player.model,playerBlend.boneFrames,22,frame,runtime.playerShoulder);
            runtime.playerMuzzleValid=playerBlend.seatedDriveBy.ready&&
                muzzle(player,playerBlend,frame,runtime.equippedWeapon,runtime.playerMuzzle);
        }
    }
    if(vcs()&&runtime.health>0&&!runtime.playerHeadValid&&runtime.occupiedVehicle>=0&&
        size_t(runtime.occupiedVehicle)<runtime.vehicles.size()&&size_t(runtime.occupiedVehicle)<vehicles.size()&&
        runtime.vehicleAction==StorylandVehicleAction::None&&environment.characterBoneMatrix) {
        StorylandVehicleMatrix frame;
        if(environment.characterBoneMatrix(0,5,frame)) {
            runtime.playerHead={frame[12],frame[13],frame[14]};runtime.playerHeadValid=true;
        }
    }
    if(runtime.health>0&&runtime.vehicleAction==StorylandVehicleAction::Exit&&runtime.vehicleTransitionPrepared&&
        runtime.transitionVehicle>=0&&size_t(runtime.transitionVehicle)<runtime.vehicles.size()&&
        storylandVcsHelicopterBailout(runtime.vehicles[size_t(runtime.transitionVehicle)].nativeDefinition,
            runtime.vehicleEntryVariant)&&environment.characterBoneMatrix) {
        const auto point=[&](int32_t bone,StorylandNativePoint& target) {
            StorylandVehicleMatrix frame;
            if(!environment.characterBoneMatrix(0,bone,frame))return false;
            target={frame[12],frame[13],frame[14]};return true;
        };
        runtime.playerHeadValid=point(5,runtime.playerHead);
        runtime.playerTorsoValid=point(3,runtime.playerTorso);
        runtime.playerShoulderValid=point(22,runtime.playerShoulder);
    }
    if (runtime.health > 0 && runtime.occupiedVehicle < 0 &&
        runtime.vehicleAction == StorylandVehicleAction::None) {
        const bool continuation=runtime.playerJetSkiFallActive&&
            storylandVcsCarjackedContinuation(runtime.playerJetSkiFall);
        StorylandAnimationBlendState query;
        if(continuation)query=playerBlend;
        auto& anchorBlend=continuation?query:playerBlend;
        if(continuation)
            storylandSampleVcsCarjackedContinuation(playerAnimation,player.model,runtime,vehicles,
                runtime.playerJetSkiFall,playerPosition,playerHeading,animationTime,anchorBlend,points,error,false);
        if (anchorBlend.boneFrames.empty())
            playerAnimation.sampleTree(player.model, playerMotion, animationTime,
                anchorBlend, points, error, false);
        std::array<float,16> standingFrame;
        const bool standingValid=storylandPlayerPoseWorldFrame(vcs(),environment,player.model,playerPosition,
            playerHeading,1.8f,standingFrame);
        if(standingValid)storylandCaptureSwimmingLimbs(runtime.playerWater,player.model,
            anchorBlend.boneFrames,standingFrame);
        runtime.playerHeadValid = standingValid&&storylandPoseBoneWorldPoint(player.model, anchorBlend.boneFrames,
            5, standingFrame, runtime.playerHead);
        runtime.playerTorsoValid = standingValid&&storylandPoseBoneWorldPoint(player.model, anchorBlend.boneFrames,
            3, standingFrame, runtime.playerTorso);
        runtime.playerShoulderValid = standingValid&&storylandPoseBoneWorldPoint(player.model, anchorBlend.boneFrames,
            22, standingFrame, runtime.playerShoulder);
        runtime.playerMuzzleValid = standingValid&&muzzle(player,anchorBlend,standingFrame,
            runtime.equippedWeapon,runtime.playerMuzzle);
    }
    for(size_t index=0;index<runtime.pedestrians.size();++index) {
        auto& ped=runtime.pedestrians[index];
        ped.headValid=ped.torsoValid=ped.nativeRootValid=ped.weaponMuzzleValid=false;
        if(index>=pedestrians.size()||ped.scriptHandle>=0||ped.health<=0||
            ped.model>=pedestrianAssets.size()||!samplePedestrian(index,points,error,false))continue;
        const auto& asset=pedestrianAssets[ped.model];
        const auto& blend=pedestrianBlends[index];
        const auto& actor=pedestrians[index];
        if(ped.serviceVehicleIdentity) {
            StorylandVehicleMatrix seat;
            if(!storylandVcsServicePassengerFrame(runtime,vehicles,ped,seat))continue;
            ped.headValid=storylandNativeRootPoseBoneWorldPoint(asset.model,blend.boneFrames,5,seat,ped.head);
            ped.torsoValid=storylandNativeRootPoseBoneWorldPoint(asset.model,blend.boneFrames,3,seat,ped.torso);
            ped.nativeRoot={seat[12],seat[13],seat[14]};ped.nativeRootValid=true;
            continue;
        }
        std::array<float,16> standingFrame;
        if(!storylandStandingPedWorldFrame(asset.model,actor.position,actor.yaw,1.8f,standingFrame))continue;
        storylandCaptureSwimmingLimbs(ped.water,asset.model,blend.boneFrames,standingFrame);
        ped.headValid=storylandPoseBoneWorldPoint(asset.model,blend.boneFrames,5,standingFrame,ped.head);
        ped.torsoValid=storylandPoseBoneWorldPoint(asset.model,blend.boneFrames,3,standingFrame,ped.torso);
        ped.nativeRoot={standingFrame[12],standingFrame[13],standingFrame[14]};
        ped.nativeRootValid=true;
        ped.weaponMuzzleValid=muzzle(asset,blend,standingFrame,ped.weapon,ped.weaponMuzzle);
    }
    for (const auto& entry : environment.missionCharacters) {
        const auto& character = entry.second;
        auto& combat = runtime.missionCombat[entry.first];
        combat.anchorsValid = combat.muzzleValid = combat.nativeRootValid = false;
        if (!character.alive || character.health <= 0 || (character.attached && !vcs()))
            continue;
        const auto asset = missionAssets.find(character.model);
        if (asset == missionAssets.end()) continue;
        const auto task=environment.characterVehicleTasks.find(entry.first);
        const bool transitioning=task!=environment.characterVehicleTasks.end()&&task->second.animating;
        if(transitioning||(character.vehicle>=0&&!character.attached)) {
            auto query=missionBlends[entry.first];StorylandVehicleMatrix vehicleFrame{};
            if(storylandSampleMissionVehicleQueryPose(pedestrianAnimation,asset->second.model,runtime,
                vehicles,entry.first,environment.gameMilliseconds,query,vehicleFrame,error)) {
                combat.anchorsValid=storylandNativeRootPoseBoneWorldPoint(asset->second.model,
                    query.boneFrames,5,vehicleFrame,combat.head)&&
                    storylandNativeRootPoseBoneWorldPoint(asset->second.model,
                        query.boneFrames,3,vehicleFrame,combat.torso);
                combat.nativeRoot={vehicleFrame[12],vehicleFrame[13],vehicleFrame[14]};
                combat.nativeRootValid=true;
                const auto oriented=storylandVehicleMatrixProduct(vehicleFrame,
                    storylandScriptObjectMatrix(0,0,0,0,0,90-storylandPedModelForwardAngle(asset->second.model)));
                combat.muzzleValid=muzzle(asset->second,query,oriented,combat.weapon,combat.muzzle);
                continue;
            }
            if(transitioning)continue;
        }
        if (character.vehicle >= 0 && !character.attached) {
            const auto transition = environment.characterVehicleTasks.find(entry.first);
            if (transition != environment.characterVehicleTasks.end() && transition->second.animating)
                continue;
            const auto vehicle = environment.missionVehicles.find(character.vehicle);
            if (vehicle == environment.missionVehicles.end()) continue;
            const int slot = vehicle->second.spawnedSlot;
            if (slot < 0 || size_t(slot) >= runtime.vehicles.size() || size_t(slot) >= vehicles.size())
                continue;
            const auto& state = runtime.vehicles[size_t(slot)];
            const auto& traffic = vehicles[size_t(slot)];
            if (state.nativeDefinition.nativeType != (vcs() ? 6 : 5)) continue;
            const int seat = character.passengerSeat;
            auto options = pedestrianAnimation.vehicleAnimationOptions(state.nativeDefinition,&state.vcsJetSkiPose.sample,&runtime.vehiclePhysicsError);
            options.seatRelative = true;
            options.passenger = seat >= 0;
            options.rightHandSide = seat >= 0 && seat % 2 == 0;
            options.steering = state.steeringAngle / std::max(1.f, state.handlingSteerLock);
            options.reversing = state.speed < -.2f;
            auto blend = missionBlends[entry.first];
            options.elapsedSeconds=animationTime;
            options.transition=&blend.poseTransition;
            if (!pedestrianAnimation.sampleVehicleAction(asset->second.model, "sit", 0, options,
                points, error, nullptr, &blend.boneFrames)) continue;
            const auto frame = storylandVehicleOccupantFrame(state.nativeDefinition,
                {traffic.position.x, traffic.position.y, traffic.position.z}, traffic.yaw,
                state.nativeDefinition.previewMinimumZ, state.suspension, state.bodyRoll - state.roadRoll,
                state.bodyPitch + state.roadPitch, unsigned(seat + 1), traffic.water,
                state.craneCarried ? &state.craneFrame : nullptr,storylandVcsJetSkiNativeAngularZ(state),state.bikeVisualLean);
            if(!vcs()&&!storylandApplyLcsScriptedLook(environment,entry.first,asset->second.model,
                storylandLcsLookSeatFrame(asset->second.model,frame),{frame[12],frame[13],frame[14]},
                blend,points,error,false,true))continue;
            combat.anchorsValid = storylandNativeRootPoseBoneWorldPoint(asset->second.model,
                blend.boneFrames, 5, frame, combat.head) &&
                storylandNativeRootPoseBoneWorldPoint(asset->second.model, blend.boneFrames, 3, frame, combat.torso);
            combat.nativeRoot = {frame[12], frame[13], frame[14]};
            combat.nativeRootValid = combat.anchorsValid;
            continue;
        }
        if (!sampleMissionCharacter(entry.first, points, error, false))
            continue;
        const auto& blend = missionBlends[entry.first];
        if(vcs()&&character.attached) {
            std::array<float,16> root{};
            if(!storylandVcsCharacterEntityFrame(environment,entry.first,root))continue;
            const auto orientation=storylandScriptObjectMatrix(0,0,0,0,0,
                90-storylandPedModelForwardAngle(asset->second.model));
            const auto frame=storylandVehicleMatrixProduct(root,orientation);
            combat.nativeRoot={root[12],root[13],root[14]};
            combat.nativeRootValid=true;
            combat.anchorsValid=storylandPoseBoneWorldPoint(asset->second.model,blend.boneFrames,5,frame,combat.head)&&
                storylandPoseBoneWorldPoint(asset->second.model,blend.boneFrames,3,frame,combat.torso);
            combat.muzzleValid=muzzle(asset->second,blend,frame,combat.weapon,combat.muzzle);
            continue;
        }
        const StorylandNativePoint position{character.x, character.y, character.z};
        std::array<float,16> standingFrame;
        const bool standingValid=storylandStandingPedWorldFrame(asset->second.model,position,
            character.heading+90,1.8f,standingFrame);
        if(standingValid)storylandCaptureSwimmingLimbs(combat.water,asset->second.model,
            blend.boneFrames,standingFrame);
        combat.nativeRootValid=standingValid;
        if(standingValid)combat.nativeRoot={standingFrame[12],standingFrame[13],standingFrame[14]};
        combat.anchorsValid = standingValid&&storylandPoseBoneWorldPoint(asset->second.model,blend.boneFrames,
            5,standingFrame,combat.head)&&storylandPoseBoneWorldPoint(asset->second.model,blend.boneFrames,
                3,standingFrame,combat.torso);
        combat.muzzleValid = standingValid&&muzzle(asset->second,blend,standingFrame,combat.weapon,combat.muzzle);
    }
    for (auto entry = missionBlends.begin(); entry != missionBlends.end();) {
        if (!environment.missionCharacters.count(entry->first)) entry = missionBlends.erase(entry);
        else ++entry;
    }
}

bool StorylandLinuxSession::refreshNativeScenery(std::string& error) {
    auto scenery=std::make_unique<StorylandArchiveBrowser>();
    StorylandNativeSceneryReport report;
    if(!scenery->loadNativeScenery(archive,report,error)) {
        error="Vegetation and objects: "+error;
        return false;
    }
    if(!storylandApplyNativeObjectVisibility(*scenery,archive,environment.invisibleNativeObjects,error))
        return false;
    const auto existing=std::find_if(areas.begin(),areas.end(),
        [](const auto& area) {return area->hasNativeScenery();});
    if(existing==areas.end())areas.push_back(std::move(scenery));
    else **existing=std::move(*scenery);
    sceneryReport=std::move(report);
    nativeSceneryNeedsRefresh=false;
    ++sceneryRevision;
    storylandWriteSceneryReport(std::clog,sceneryReport,archive.companionImgPath());
    storylandSaveSceneryReport(sceneryReport,archive.companionImgPath(),
        gameDirectory/"Storyland-Scenery.log");
    error.clear();
    return true;
}

bool StorylandLinuxSession::applyBuildingSwaps(std::string& error) {
    error.clear();
    if (environment.buildingModelSwapsChanged) {
        for (const auto& entry : environment.buildingModelSwaps) {
            const auto& instances=archive.nativeWorld().instances;
            const auto instance=std::find_if(instances.begin(),instances.end(),[&](const auto& item) {
                return ((uint64_t(item.poolHeader)<<32)|item.poolIndex)==entry.first;
            });
            if(instance!=instances.end()&&instance->modelId==entry.second.replacementModel)continue;
            if (!archive.replaceBuildingModel(entry.first,entry.second.replacementModel)) {
                error = "Building swap references an unavailable native instance.";
                return false;
            }
            nativeSceneryNeedsRefresh=true;
        }
        environment.buildingModelSwapsChanged = false;
    }
    // Preserve command order: VCS gives a newly selected conditional sector
    // priority over the previous group, including authored cutscene interiors.
    for (const auto& command : environment.buildingSwapCommands) {
        for (const auto& area : areas) {
            if (command.group) area->setBuildingSwapGroup(command.key,command.state);
            else {
                area->setBuildingSwap(command.key,command.state);
                if(archive.applyBuildingSwaps(area->buildingSwaps(),
                    {{command.key,int32_t(command.state)}})!=0) {
                    nativeSceneryNeedsRefresh=true;
                }
            }
        }
    }
    environment.buildingSwapCommands.clear();
    const bool refresh=nativeSceneryNeedsRefresh;
    if(refresh&&!refreshNativeScenery(error))return false;
    if(refresh)environment.nativeObjectVisibilityChanged=true;
    if(refresh||environment.nativeObjectVisibilityChanged) {
        for(const auto& area:areas)
            if(!storylandApplyNativeObjectVisibility(*area,archive,environment.invisibleNativeObjects,error))
                return false;
        environment.nativeObjectVisibilityChanged=false;
    }
    return true;
}
