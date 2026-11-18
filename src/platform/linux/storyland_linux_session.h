#pragma once
#include "src/leeds/leeds_archive.h"
#include "src/storyland/storyland_cutscene.h"
#include "src/storyland/storyland_mission_program.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_sa_diving.h"
#include "src/storyland/storyland_scm.h"
#include "src/storyland/storyland_gxt.h"
#include "src/storyland/storyland_dyom.h"
#include <filesystem>
#include <functional>
#include <map>
#include <memory>

class StorylandCoopGame;
class StorylandMultiplayer;
struct StorylandNetworkPlayer;
namespace storyland_save { struct State; }

class StorylandLinuxSession {
public:
    StorylandLinuxSession();
    ~StorylandLinuxSession();
    bool loadGame(const std::filesystem::path& path, std::string& error);
    bool startGame(std::string& error, bool requireCutscenes = true);
    bool startTest(std::string& error);
    bool canStartDyom(std::string* error = nullptr) const;
    bool startDyom(const StorylandDyomProject& project, std::string& error);
    void stopDyom();
    bool dyomActive() const { return dyomSession; }
    const StorylandDyomCompiled& dyomCompiled() const { return dyomMission; }
    std::string dyomStatus() const;
    bool setZombiePedestrians(bool enabled,std::string& error);
    bool restartIntroMission(std::string& error);
    bool captureCheckpoint(storyland_save::State& state, std::string& error);
    bool restoreCheckpoint(storyland_save::State state, std::string& error);
    bool saveCheckpoint(const std::filesystem::path& path, std::string& error);
    bool loadCheckpoint(const std::filesystem::path& path, std::string& error);
    bool canSaveGame(std::string* error = nullptr) const;
    std::filesystem::path saveGameSlotPath(const std::filesystem::path& documents,
        int slot, bool previous = false) const;
    bool hasGameSaveSlot(const std::filesystem::path& documents, int slot) const;
    bool saveGameSlot(const std::filesystem::path& documents, int slot, std::string& error);
    bool loadGameSlot(const std::filesystem::path& documents, int slot, std::string& error);
    bool deleteGameSlot(const std::filesystem::path& documents, int slot, std::string& error);
    void stopGame();
    void tick(float seconds, const StorylandPlayTestInput& input = {},
        const StorylandPlayTestCameraInput& cameraInput = {}, bool jump = false,
        bool interact = false, bool aimHeld = false, bool fireHeld = false,
        bool reloadPressed = false, const std::array<int16_t,20>* padInput = nullptr);
    bool sampleMissionCharacter(int32_t handle, std::vector<StorylandModelPoint>& points,
        std::string& error, bool skinGeometry = true);
    bool sampleMissionCharacterBasePose(int32_t handle,std::vector<StorylandModelPoint>& points,
        std::string& error,bool skinGeometry);
    bool samplePedestrian(size_t index, std::vector<StorylandModelPoint>& points,
        std::string& error, bool skinGeometry = true);
    std::string translateMissionText(const std::string& key, const std::string& table) const;
    float animationSeconds() const { return animationTime; }
    bool active() const { return running; }
    bool loaded() const { return worldLoaded; }
    bool isPlayingGame() const { return playingGame; }
    bool vcs() const { return archive.gameVariant() == StorylandDtzGameVariant::Vcs; }
    bool canPlayCoop() const { return running && playingGame && program.active(); }
    StorylandMultiplayer& coopNetwork();
    StorylandNetworkPlayer coopDisplay() const;
    double coopTime() const { return coopClock; }
    const std::string& coopStatus() const;
    void updateCoop(double now);
    void publishCoop(double now);
    bool requestCutsceneSkip();
    const StorylandPlayTestAsset* coopVehicleAsset(int32_t model);
    bool updateCoopCharacter();
    const StorylandPlayTestAsset* coopCharacterAsset();
    const std::string& status() const { return statusMessage; }
    const std::filesystem::path& gameRoot() const { return gameDirectory; }

    StorylandDtzArchive archive;
    std::vector<std::unique_ptr<StorylandArchiveBrowser>> areas;
    StorylandNativeSceneryReport sceneryReport;
    uint64_t sceneryRevision = 0;
    std::map<uint8_t,StorylandModelFile> nativeMarkerModels;
    StorylandScmFile script;
    StorylandGxtFile missionGxt;
    StorylandMocapArchive mocap;
    StorylandCutscenePlayer cutscene;
    StorylandMissionEnvironment environment;
    StorylandMissionVm mission;
    StorylandMissionProgram program;
    StorylandStoriesCamera camera;
    StorylandPlayTestAsset player;
    StorylandPlayTestAnimation playerAnimation;
    StorylandSaDivingBank divingAnimations;
    StorylandAnimFile extendedCrouchAnimations;
    bool extendedCrouchAvailable = false;
    StorylandPlayTestAnimation pedestrianAnimation;
    StorylandPlayTestRuntime runtime;
    std::unique_ptr<StorylandPlayTestAsset> escalatorStepAsset;
    std::string escalatorAssetStatus;
    StorylandNativePoint playerPosition;
    float playerHeading = 0;
    bool playerLoaded = false;
    StorylandPlayTestMotion playerMotion = StorylandPlayTestMotion::Idle;
    std::vector<StorylandModelPoint> playerPoints;
    StorylandAnimationBlendState playerBlend;
    std::map<int32_t, StorylandPlayTestAsset> missionAssets;
    struct CompiledWorldAsset {
        size_t sourceArea=0;
        StorylandLcsDynamicGeometry geometry;
    };
    std::map<int32_t,CompiledWorldAsset> compiledWorldAssets;
    std::map<int32_t, StorylandAnimationBlendState> missionBlends;
    std::vector<StorylandPlayTestAsset> weaponAssets;
    std::vector<StorylandPlayTestAsset> vehicleAssets;
    std::vector<StorylandAnimationBlendState> vehicleDriverBlends;
    std::vector<StorylandPlayTestAsset> pedestrianAssets;
    std::vector<StorylandAnimationBlendState> pedestrianBlends;
    std::vector<StorylandNativeTrafficActor> vehicles;
    std::vector<StorylandNativeTrafficActor> pedestrians;
    std::function<bool(const std::array<float,3>&,std::string&)> preloadWorldScene;
    std::function<uint32_t(uint32_t)> cutsceneElapsedMilliseconds;
    std::function<bool(int32_t,std::vector<StorylandMissionEnvironment::CreditsLine>&,std::string&)> prepareCredits;
    std::function<int32_t()> currentRadioChannel;
    std::function<bool(int32_t)> musicStreamPlaying;
    std::function<uint64_t()> musicStreamPositionMilliseconds;
    std::function<bool(int32_t,bool,std::string&)> playMissionTune;
    std::function<void()> stopMissionTune;
    float windDirectionDegrees = 45.0f;
    bool paused = false;

private:
    std::unique_ptr<StorylandMultiplayer> network;
    std::unique_ptr<StorylandCoopGame> coop;
    std::unique_ptr<StorylandPlayTestAsset> remoteVehicleAsset;
    std::unique_ptr<StorylandPlayTestAsset> coopOriginalPlayer;
    std::unique_ptr<StorylandPlayTestAsset> coopRemoteCharacter;
    std::string coopLocalCharacterName, coopRemoteCharacterName;
    int32_t remoteVehicleModel = -1;
    bool coopRestartPending = false;
    float lastCoopAttack = -1;
    double coopClock = 0;
    StorylandNetworkPlayer coopPlayerSnapshot() const;
    void disconnectCoop();
    void resetCoopAfterRestart();
    bool worldLoaded = false;
    bool running = false;
    bool playingGame = false;
    bool dyomSession = false;
    StorylandDyomCompiled dyomMission;
    float animationTime = 0;
    double millisecondRemainder = 0;
    std::filesystem::path gameDirectory;
    std::string statusMessage;
    bool preparePlayer(std::string& error);
    bool prepareRequestedModels(std::string& error);
    void registerVehicleAsset(StorylandPlayTestAsset asset);
    bool constructMissionVehicle(int32_t handle, std::string& error);
    bool nativeSceneryNeedsRefresh = false;
    bool refreshNativeScenery(std::string& error);
    bool applyBuildingSwaps(std::string& error);
    void updateCombatPoseAnchors();
    bool updatePlayerRestart();
    void bindEnvironment();
    void fail(const std::string& error);
};
