#pragma once
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_mission_vm.h"

bool storylandGiveWeapon(StorylandPlayTestRuntime& runtime,size_t weapon,int ammunition,bool equip=true);
bool storylandCyclePlayerWeapon(StorylandPlayTestRuntime& runtime);
size_t storylandBetaWeaponIndex(const StorylandPlayTestRuntime& runtime,StorylandBetaWeapon feature,bool vcs);
void storylandUpdateBetaWeaponGrants(StorylandPlayTestRuntime& runtime);
bool storylandSetBetaWeaponEnabled(StorylandPlayTestRuntime& runtime,StorylandBetaWeapon feature,
    bool enabled,bool vcs,std::string& error);
bool storylandGiveBetaWeapon(StorylandPlayTestRuntime& runtime,StorylandBetaWeapon feature,
    int ammunition,bool vcs,std::string& error);
bool storylandSetHeadshotRampageEnabled(StorylandPlayTestRuntime& runtime,bool enabled,std::string& error);
bool storylandStartKillFrenzy(StorylandPlayTestRuntime& runtime,const std::string& text,
    int32_t nativeWeapon,int32_t timeLimitMilliseconds,int32_t targetKills,
    const std::array<int32_t,4>& models,bool completedText,bool onlyHeadshots,
    bool custom,std::string& error);
void storylandCancelKillFrenzy(StorylandPlayTestRuntime& runtime);
void storylandUpdateKillFrenzy(StorylandPlayTestRuntime& runtime);
int32_t storylandKillFrenzyRemainingMilliseconds(const StorylandPlayTestRuntime& runtime);
bool storylandCreateScriptParticles(StorylandPlayTestRuntime& runtime,const std::vector<uint8_t>& game,
    const std::vector<StorylandMissionEnvironment::ScriptParticle>& requests,std::string& error);
bool storylandScriptEmitterAvailable(const std::vector<uint8_t>& game,uint16_t subtype,bool vcs=false);
void storylandUpdateScriptParticleEmitters(StorylandPlayTestRuntime& runtime);
void storylandResetDollarParticles(StorylandPlayTestRuntime& runtime);
bool storylandCreateDollarParticles(StorylandPlayTestRuntime& runtime,uint8_t count,
    const StorylandMissionEnvironment::ScriptParticle& request,std::string& error);
int32_t storylandCountReturnableWeapons(const StorylandPlayTestRuntime& runtime);
bool storylandReturnPlayerWeapons(StorylandPlayTestRuntime& runtime);
// The caller must first validate the session, entity generation and action ownership.
bool storylandApplyMissionCharacterDamage(StorylandPlayTestRuntime& runtime,
    const StorylandMissionCharacterDamage& request, bool creditLocalPlayer = true);
bool storylandExplodeMissionCharacterHead(StorylandPlayTestRuntime& runtime, int32_t handle);
void storylandReloadWeapon(StorylandPlayTestRuntime& runtime);
void storylandSetPlayTestWantedLevel(StorylandPlayTestRuntime& runtime,int wanted);
bool storylandTakePhotograph(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& nativeWorld);
bool storylandMissionVehicleAimPoint(const StorylandPlayTestRuntime& runtime,int32_t handle,
    StorylandNativePoint& point);
int32_t storylandCrouchWeaponAnimation(const StorylandPlayTestRuntime& runtime,int32_t handle);
float storylandCrouchWeaponPoseWeight(const StorylandCrouchWeaponPose& pose,uint32_t now);
void storylandAdvanceCrouchWeaponPose(StorylandPlayTestRuntime& runtime,int32_t handle,
    StorylandCrouchWeaponPose& pose,const StorylandWeaponDefinition& weapon,bool firing);
bool storylandBeginCrouchReload(StorylandPlayTestRuntime& runtime,int32_t handle,
    StorylandCrouchWeaponPose& pose,const StorylandWeaponDefinition& weapon);
int storylandPrepareCrouchShot(StorylandPlayTestRuntime& runtime,int32_t handle,
    StorylandCrouchWeaponPose& pose,const StorylandWeaponDefinition& weapon);
bool storylandCanAimPlayTestWeapon(const StorylandPlayTestRuntime& runtime);
bool storylandCanFreeAimPlayTestWeapon(const StorylandPlayTestRuntime& runtime);
void storylandPreparePlayTestFreeAim(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const std::vector<StorylandNativeTrafficActor>& pedestrians,
    const StorylandNativePoint& player,float& heading,StorylandStoriesCamera& camera,
    const StorylandPlayTestCameraInput& input,float seconds);
void storylandRefreshPlayTestFreeAim(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const std::vector<StorylandNativeTrafficActor>& pedestrians,
    const StorylandNativePoint& player,float& heading,StorylandStoriesCamera& camera);
bool storylandHasPlayTestTarget(const StorylandPlayTestRuntime& runtime);
bool storylandPlayTestTargetNativeRoot(const StorylandPlayTestRuntime& runtime,
    StorylandNativePoint& result);
bool storylandPlayTestAimKeepsHeading(const StorylandPlayTestRuntime& runtime);
bool storylandPlayTestAimBlocksMovement(const StorylandPlayTestRuntime& runtime);
void storylandClearPlayTestTarget(StorylandPlayTestRuntime& runtime);
void storylandPreparePlayTestTarget(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, const std::vector<StorylandNativeTrafficActor>& pedestrians,
    const StorylandNativePoint& player, float& heading, bool targetHeld);
void storylandUpdatePlayTestWeapons(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    std::vector<StorylandNativeTrafficActor>& pedestrians,const StorylandNativePoint& player,
    float& heading,float cameraYaw,bool targetHeld,bool fire,bool reloadPressed,float seconds,
    const std::vector<StorylandNativeTrafficActor>* vehicles=nullptr);
void storylandRestoreScriptProjectiles(StorylandPlayTestRuntime& runtime,float seconds);
void storylandStoreScriptProjectiles(StorylandPlayTestRuntime& runtime);

void storylandDamagePlayTestPed(StorylandPlayTestRuntime& runtime,size_t index,float damage,
    const StorylandNativePoint& position,bool playerAttack,bool vehicleImpact=false,uint8_t hitDirection=0,
    const StorylandNativePoint* source=nullptr);
void storylandDamagePlayTestPedWithVehicle(StorylandPlayTestRuntime& runtime,size_t index,float damage,
    const StorylandNativePoint& position,bool playerAttack,int32_t nativeWeapon,uint8_t hitDirection,
    const StorylandNativePoint& source,uint8_t hitPiece=0);

void storylandDropMissionCharacterPossessions(StorylandPlayTestRuntime& runtime,int32_t handle);
bool storylandCreatePlayTestExplosion(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& vehicles,const std::vector<StorylandNativeTrafficActor>& pedestrians,
    const StorylandNativePoint& player,
    StorylandMissionEnvironment::ScriptExplosion& explosion,std::string& error,
    size_t sourceVehicleSlot=SIZE_MAX);
bool storylandFireHunterGun(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& vehicles,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,int32_t handle,std::string& error,
    const StorylandWeaponDefinition* nativeWeapon=nullptr);

bool storylandWreckPlayTestVehicle(StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& vehicles,size_t slot);
void storylandDetonatePlayTestVehicle(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld* native,const std::vector<StorylandNativeTrafficActor>& vehicles,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,const StorylandNativePoint& player,size_t slot);
bool storylandBlowUpMissionVehicle(StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& vehicles,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,int32_t handle);
