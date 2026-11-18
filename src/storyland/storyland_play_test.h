#pragma once
#include "src/leeds/leeds_vehicle.h"
#include "src/leeds/leeds_ped_stats.h"
#include "src/storyland/storyland_weapon_types.h"
#include "src/leeds/leeds_dtz.h"
#include "src/leeds/leeds_anim.h"
#include "src/leeds/leeds_animation_groups.h"
#include "src/leeds/leeds_model.h"
#include "src/storyland/storyland_mobile_animation.h"
#include "src/storyland/storyland_ped_gait.h"
#include "src/leeds/leeds_texture.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <set>
#include <utility>

enum class StorylandPlayTestMotion { Idle, Walk, Run, Sprint, Jump, Fall, Land, Cough, Dead, SwimIdle, Swim, SwimFast, Drown };

bool storylandNativeModelAvailable(const StorylandDtzArchive& archive, int32_t model);
bool storylandValidateSpecialModelAsset(const StorylandDtzArchive& archive, int32_t model,
    const std::string& name, std::string& error);
bool storylandPendingModelNeedsAnimation(const StorylandDtzArchive& archive,
    const std::set<int32_t>& requested, const std::set<int32_t>& resident, const std::string& bank);

struct StorylandSeatedDriveByAnimationState {
    int32_t vehicle=-1,seat=-1,association=-1;
    float startedAt=0;
    bool targetLeft=false,front=true,ready=false;
};

struct StorylandAnimationBlendState {
    StorylandPlayTestMotion from = StorylandPlayTestMotion::Idle;
    StorylandPlayTestMotion to = StorylandPlayTestMotion::Idle;
    float startedAt = 0;
    float duration = 0.24f;
    bool initialized = false;
    float speed = 0;
    float measuredSpeed = -1;
    float nativeMoveRatio=-1,nativeSprintWeight=-1;
    std::array<float,4> gaitWeights{1,0,0,0};
    float sprintAssociationWeight=0,sprintAssociationDelta=0;
    int nativeStopVariant=-1,runStopVariant=-1;
    float nativeStopTime=0,runStopStartedAt=0;
    float walkSpeed = 1.25f;
    float runSpeed = 3.8f;
    float locomotionPhase = 0;
    bool distanceDrivenPhase = false;
    float travelledDistance = 0;
    double scriptWalkedDistance = 0;
    bool scriptDistanceInitialized = false;
    float lastSampleTime = 0;
    float upperStartedAt = 0;
    bool upperActive = false;
    float actionStartedAt = 0;
    bool weaponAiming=false,weaponReloading=false;
    bool allowAimedLocomotion=false;
    float weaponAnimationTime=-1,weaponAnimationWeight=1;
    // Actual motion in the actor's facing frame, in metres per second.
    float movementForward=0,movementRight=0;
    float weaponLocomotionForward=1,weaponLocomotionRight=0;
    float weaponAimBlend=0,weaponAimTime=0;
    bool weaponAimWasActive=false;
    float weaponAimYaw=0,weaponAimPitch=0;
    bool weaponAimDirectionValid=false;
    StorylandPedAimState weaponAimIk;
    uint32_t weaponShotSerial=0;
    size_t weaponIndex=SIZE_MAX;
    std::vector<std::array<float,16>> boneFrames;
    StorylandAnimationTransition poseTransition;
    StorylandSeatedDriveByAnimationState seatedDriveBy;
};

struct StorylandVehicleAnimationOptions {
    bool low=false,boat=false,passenger=false,rightHandSide=false,seatRelative=false,reversing=false;
    bool rootMotion=false,twoWheels=false,quad=false,rearExit=false,vcsHelicopter=false;
    bool vcsHovercraft=false,jumpIn=false;
    bool skinGeometry=true;
    bool vcsJetSki=false,vcsOrdinaryBoat=false,strongWaterContact=false,highAlign=false;
    const storyland_vcs_jetski_pose::Sample* jetSkiPose=nullptr;
    std::string* liveVehicleError=nullptr;
    bool van=false,bus=false,rearDoor=false,doorMissing=false;
    StorylandVehicleEntryVariant variant=StorylandVehicleEntryVariant::Normal;
    int32_t animationGroup=0;
    float steering=0,elapsedSeconds=0;
    StorylandAnimationTransition* transition=nullptr;
};
struct StorylandVehicleRootMotion {
    StorylandModelPoint start,current,end;
};
class StorylandPlayTestAnimation {
public:
    bool load(const StorylandDtzArchive& archive, bool player, std::string& error);
    bool loadFromDtzBytes(const std::vector<uint8_t>& bytes,bool vcs,bool player,std::string& error);
    bool hasAnimationGroup(int32_t group) const;
    bool retailAnimationLoops(int32_t animationId,int32_t group) const;
    bool retailAnimationAssociationFlags(int32_t animationId,int32_t group,uint16_t& flags) const {
        if(animationId<0||group<0)return false;
        const auto* association=groups.find(uint32_t(group),uint32_t(animationId));
        if(!association)return false;
        flags=association->flags;return true;
    }
    bool loadAnimationGroup(const StorylandDtzArchive& archive,int32_t group,std::string& error);
    bool loadNamedAnimationBank(const StorylandDtzArchive& archive,const std::string& bank,
        const std::vector<std::string>& clips,std::string& error);
    bool importNamedAnimationBank(const std::vector<uint8_t>& bytes,const std::string& bank,
        const std::vector<std::string>& clips,std::string& error);
    bool hasNamedAnimationBank(const std::string& bank,const std::vector<std::string>& clips) const;
    void unloadNamedAnimationBank(const std::string& bank);
    float bankClipDuration(const std::string& bank,const std::string& clip) const;
    bool sampleBankClip(const StorylandModelFile& model,const std::string& bank,const std::string& clip,
        float seconds,bool repeat,std::vector<StorylandModelPoint>& points,std::string& error,
        std::vector<std::array<float,16>>* frames=nullptr,bool partial=false);
    bool hasAnimationBank(const std::string& bank) const;
    bool knowsAnimationBank(const std::string& bank) const;
    bool loadAnimationBank(const StorylandDtzArchive& archive,const std::string& bank,std::string& error);
    void removeAnimationBank(const std::string& bank);
    std::string animationGroupBank(int32_t group) const;
    bool importWeaponBank(const std::vector<uint8_t>& bytes,const std::string& bank,std::string& error);
    const std::string& bankReport() const {return bankDiagnostic;}
    bool sample(const StorylandModelFile& model, StorylandPlayTestMotion motion, float seconds,
        std::vector<StorylandModelPoint>& points, std::string& error);
    bool sampleBlended(const StorylandModelFile& model, StorylandPlayTestMotion motion, float seconds,
        StorylandAnimationBlendState& state, std::vector<StorylandModelPoint>& points, std::string& error);
    // Bone queries use the same pose and blend state with skinGeometry=false,
    // leaving points empty instead of deforming a mesh that will not be drawn.
    bool sampleTree(const StorylandModelFile& model, StorylandPlayTestMotion motion, float seconds,
        StorylandAnimationBlendState& state, std::vector<StorylandModelPoint>& points, std::string& error,
        bool skinGeometry=true);
    // Select this actor's native movement associations before sampling a shared
    // animation bank. A script group must provide all four movement clips.
    bool selectLocomotionGroup(int32_t group,std::string& error);
    bool advanceLocomotionDistance(int32_t group,StorylandPlayTestMotion motion,float travelled,
        float seconds,StorylandAnimationBlendState& state,std::string& error);
    bool sampleVehicleAction(const StorylandModelFile& model, const std::string& action,
        float fraction, std::vector<StorylandModelPoint>& points, std::string& error);
    bool sampleVehicleAction(const StorylandModelFile& model,const std::string& action,float fraction,
        const StorylandVehicleAnimationOptions& options,std::vector<StorylandModelPoint>& points,std::string& error,
        StorylandVehicleRootMotion* root=nullptr,std::vector<std::array<float,16>>* frames=nullptr);
    bool sampleVcsBoatDriverPose(const StorylandModelFile& model,const StorylandVehicleDefinition& definition,
        std::vector<StorylandNativePedBoneFrame>& frames,std::string& error);
    bool sampleSeatedDriveBy(const StorylandModelFile& model,int32_t vehicle,int32_t seat,
        const StorylandNativePoint& localTarget,float seconds,StorylandAnimationBlendState& blend,
        std::vector<StorylandModelPoint>& points,std::string& error,bool skinGeometry=true);
    float vehicleActionDuration(const std::string& action,bool low=false,bool rightHandSide=false,bool requireNative=false) const;
    float vehicleActionDuration(const std::string& action,const StorylandVehicleAnimationOptions& options,
        bool requireNative=false) const;
    StorylandVehicleAnimationOptions vehicleAnimationOptions(const StorylandVehicleDefinition& definition,
        const storyland_vcs_jetski_pose::Sample* jetSkiPose=nullptr,std::string* liveVehicleError=nullptr) const;
    void configureVehicleAnimations(StorylandVehicleDefinition& definition);
    bool sampleMovementAction(const StorylandModelFile& model, StorylandPlayTestMotion motion,
        float seconds, std::vector<StorylandModelPoint>& points, std::string& error,
        float locomotionPhase=-1.0f,std::vector<std::array<float,16>>* frames=nullptr,bool skinGeometry=true,
        StorylandAnimationTransition* transition=nullptr,float presentationSeconds=-1.0f);
    float movementActionDuration(StorylandPlayTestMotion motion) const;
    float runStopDuration(unsigned variant) const;
    float runStopDistance(unsigned variant,float fromSeconds,float toSeconds) const;
    std::string runStopClipName(unsigned variant) const;
    bool hasNamedClip(const std::string& name) const;
    float namedClipDuration(const std::string& name) const;
    bool sampleNamedClip(const StorylandModelFile& model,const std::string& name,float seconds,
        std::vector<StorylandModelPoint>& points,std::string& error);
    float retailAnimationDuration(int32_t animationId,int32_t group=-1) const;
    StorylandNativePoint retailAnimationRootDelta(const StorylandModelFile& model,
        int32_t animationId,int32_t group,float fromSeconds,float toSeconds);
    StorylandNativePoint retailAnimationExtractedRootDelta(int32_t animationId,int32_t group,
        float fromSeconds,float toSeconds);
    float retailAnimationContactTime(const StorylandModelFile& model,
        int32_t animationId,int32_t group,uint32_t boneId);
    float weaponAnimationDuration(int32_t group,int32_t animationId) const;
    bool sampleRetailAnimationId(const StorylandModelFile& model,int32_t animationId,float seconds,
        std::vector<StorylandModelPoint>& points,std::string& error,int32_t group=-1,
        std::vector<std::array<float,16>>* frames=nullptr,
        StorylandPlayTestMotion baseMotion=StorylandPlayTestMotion::Idle,float basePhase=-1,
        float associationWeight=1,bool skinGeometry=true,StorylandAnimationTransition* transition=nullptr,
        float blendRate=8,int32_t associationFlags=-1,float transitionSeconds=-1,
        bool retainAuthoredRoot=false);
    bool sampleDeathAnimation(const StorylandModelFile& model,int32_t animationId,float seconds,
        std::vector<StorylandModelPoint>& points,std::string& error,
        std::vector<std::array<float,16>>* frames=nullptr,bool skinGeometry=true,
        StorylandAnimationTransition* transition=nullptr,float blendRate=4,float speed=1);
    bool sampleScriptAnimation(const StorylandModelFile& model,int32_t animationId,int32_t group,
        float seconds,float weight,StorylandAnimationBlendState& blend,
        std::vector<StorylandModelPoint>& points,std::string& error,bool skinGeometry=true);
    bool sampleScriptAnimation(const StorylandModelFile& model,int32_t animationId,int32_t group,
        float seconds,float weight,StorylandAnimationBlendState& blend,
        std::vector<StorylandModelPoint>& points,std::string& error,bool skinGeometry,
        StorylandPlayTestMotion baseMotion,float basePhase);
    bool sampleCrouchPose(const StorylandModelFile& model,int32_t animationId,int32_t group,
        float seconds,float weight,StorylandAnimationBlendState& blend,
        std::vector<StorylandModelPoint>& points,std::string& error,bool skinGeometry=true);
    bool sampleExternalClip(const StorylandModelFile& model,StorylandAnimFile& external,float seconds,
        std::vector<StorylandModelPoint>& points,std::string& error);
    bool sampleExternalNamedClip(const StorylandModelFile& model,StorylandAnimFile& external,
        const std::string& clipName,float seconds,bool loop,
        std::vector<StorylandModelPoint>& points,std::string& error,
        std::vector<std::array<float,16>>* frames=nullptr,
        bool preserveRootVertical=false);
    bool sampleExternalClimbPose(const StorylandModelFile& model,StorylandAnimFile& external,
        const std::string& clipName,float seconds,bool loop,float sampleTime,
        StorylandAnimationBlendState& blend,std::vector<StorylandModelPoint>& points,std::string& error);
    bool sampleExternalCrouchPose(const StorylandModelFile& model,StorylandAnimFile& external,
        const std::string& clipName,float seconds,float stanceWeight,float moveWeight,
        std::vector<StorylandModelPoint>& points,std::string& error,
        std::vector<std::array<float,16>>* frames=nullptr);
    bool sampleExternalCrouchPose(const StorylandModelFile& model,StorylandAnimFile& external,
        const std::string& clipName,float seconds,float stanceWeight,float moveWeight,
        std::vector<StorylandModelPoint>& points,std::string& error,
        std::vector<std::array<float,16>>* frames,
        StorylandAnimationBlendState* weaponState,float sampleTime);
    float selectedWeaponClipDuration() const;
    bool hasUpperBodyAction() const;
    bool hasDeathAnimation() const;
    std::string selectedMovementName() const {
        return animation.activeClipIndex()<animation.clips().size()?animation.clips()[animation.activeClipIndex()].name:std::string{};
    }
    bool selectWeaponAction(const StorylandWeaponDefinition& weapon,const std::string& phase,
        bool moving=false,uint32_t serial=0,int pedGroup=-1,bool fighting=false,bool police=false);
    void configureCombatDefinitions(std::vector<StorylandWeaponDefinition>& definitions,
        const StorylandModelFile* model=nullptr);
    std::string selectedActionBank() const;
    std::string selectedActionName() const;
    std::string clipName(StorylandPlayTestMotion motion) const;
    float locomotionStride(StorylandPlayTestMotion motion) const;
    float locomotionStrideAtSpeed(float speed) const;
    float locomotionSpeed(StorylandPlayTestMotion motion) const;
    float locomotionDuration(StorylandPlayTestMotion motion) const;
private:
    void updateLocomotionMetrics();
    std::string bankDiagnostic;
    StorylandAnimationGroups groups;
    std::map<std::string,std::vector<std::string>> bankClips;
    std::map<uint32_t,uint32_t> vehicleAnimationGroups;
    bool playerAnimation=false,fullBodyAction=false,weaponUsesArmAim=false;
    bool weaponUsesMovingAim=false,weaponUsesRocketLocomotion=false,weaponIsMelee=false;
    uint32_t sprint=UINT32_MAX;
    int locomotionGroup=-1;
    float aimPoseTime=0,firePoseStart=0,firePoseEnd=0,reloadDuration=1;
    uint32_t aimAction=UINT32_MAX;
    StorylandAnimFile animation;
    StorylandAnimFile secondaryAnimation;
    StorylandAnimFile upperAnimation;
    std::map<uint32_t,std::vector<StorylandAnimTrack>> locomotionTracks;
    std::array<std::vector<StorylandAnimKey>,2> runStopRootKeys;
    uint32_t idle = UINT32_MAX, walk = UINT32_MAX, run = UINT32_MAX;
    uint32_t upperAction = UINT32_MAX;
    bool deathAnimationAvailable = false;
    bool lastDeathClipSampled = false;
    float walkStride = 1.25f;
    float runStride = 2.4f;
    float sprintStride = 3.2f;
    float walkDuration = 1.0f;
    float runDuration = 0.7f;
    float sprintDuration = 0.6f;
};

struct StorylandPlayTestInput {
    bool forward = false, backward = false, left = false, right = false, running = true;
    bool sprinting = false;
    bool handbrake = false;
    bool ascend = false, descend = false;
    bool analogMovement = false;
    float analogHorizontal = 0, analogForward = 0;
    // -1 means that input came from the existing digital/analog movement path.
    int acceleratorPressure = -1, brakePressure = -1;
    float bikeLean=0;
    bool pcControls=false, grab=false, scopeToggle=false, cancelAim=false;
    std::array<uint8_t,2> vehiclePedalPressures() const {
        if(acceleratorPressure>=0 && brakePressure>=0)
            return {uint8_t(std::clamp(acceleratorPressure,0,255)),
                uint8_t(std::clamp(brakePressure,0,255))};
        if(analogMovement && !(forward&&backward)) {
            const float amount=forwardAmount();
            return {uint8_t(std::lround(std::max(0.0f,amount)*255)),
                uint8_t(std::lround(std::max(0.0f,-amount)*255))};
        }
        return {uint8_t(forward?255:0),uint8_t(backward?255:0)};
    }
    float horizontal() const {
        if(!analogMovement)return float(right)-float(left);
        return (right||left)&&std::isfinite(analogHorizontal)?std::clamp(analogHorizontal,-1.0f,1.0f):0;
    }
    float forwardAmount() const {
        if(!analogMovement)return float(forward)-float(backward);
        return (forward||backward)&&std::isfinite(analogForward)?std::clamp(analogForward,-1.0f,1.0f):0;
    }
    float movementAmount() const {
        return analogMovement?std::min(1.0f,std::hypot(horizontal(),forwardAmount())):1.0f;
    }
};
struct StorylandPlayTestMovement {
    float x = 0, y = 0, heading = 0;
    StorylandPlayTestMotion motion = StorylandPlayTestMotion::Idle;
};
StorylandPlayTestMovement storylandMovePlayTestPlayer(float x, float y, float heading,
    float cameraYaw, const StorylandPlayTestInput& input, float seconds);
float storylandPedModelForwardAngle(const StorylandModelFile& model);

bool storylandLoadPlayTestPedestrian(const StorylandDtzArchive& archive, const std::string& playerName,
    StorylandModelFile& model, std::vector<RgbaImage>& images, std::vector<std::string>& textureNames,
    std::string& modelName, std::string& error);

struct StorylandPlayTestCameraInput {
    bool up = false, down = false, left = false, right = false, refocus = false;
    bool freeAim = false;
    float yawDelta = 0, pitchDelta = 0;
};
// A reflected view keeps WASD, camera orbit and the analog stick aligned with the screen.
inline void storylandInvertPlayControls(StorylandPlayTestInput& player,
    StorylandPlayTestCameraInput& camera) {
    std::swap(player.left, player.right);
    if (player.analogMovement) player.analogHorizontal = -player.analogHorizontal;
    std::swap(camera.left, camera.right);
    camera.yawDelta = -camera.yawDelta;
}
struct StorylandPlayTestCamera {
    float yaw = 0, pitch = 15;
};
// Orbit stays anchored to the player. Refocus independently returns behind their heading.
StorylandPlayTestCamera storylandMovePlayTestCamera(float yaw, float pitch, float playerHeading,
    const StorylandPlayTestCameraInput& input, float seconds);

struct StorylandPlayTestAsset {
    std::shared_ptr<StorylandPlayTestAsset> sharedWheel;
    StorylandPedStats pedStats;
    StorylandVehicleDomain vehicleDomain = StorylandVehicleDomain::Unknown;
    StorylandVehicleDefinition vehicleDefinition;
    StorylandModelFile model;
    std::string name;
    std::vector<RgbaImage> images;
    std::vector<std::string> textureNames;
    uint64_t geometryIdentity = 0;
    std::vector<uint32_t> materialColors;
};
bool storylandLoadPlayTestVehicle(const StorylandDtzArchive& archive,uint32_t modelId,
    StorylandPlayTestAsset& asset,std::string& error);
bool storylandLoadPlayTestPopulation(const StorylandDtzArchive& archive,
    const StorylandModelFile& player, const std::string& playerName,
    std::vector<StorylandPlayTestAsset>& pedestrians,
    std::vector<StorylandPlayTestAsset>& vehicles, std::string& error);

bool storylandFindRequiredPlayTestScm(const std::wstring& dtzPath,const std::wstring& worldPath,
    bool vcs,std::wstring& result,std::string& error,const StorylandVcsModloader* mods = nullptr);

struct StorylandRetailAsset {
    std::wstring path;
    std::string group;
    uint64_t bytes = 0;
};

struct StorylandRetailAssetIndex {
    std::vector<StorylandRetailAsset> files;
    size_t movieCount = 0;
    size_t audioCount = 0;
    size_t scriptCount = 0;
    size_t modelCount = 0;
    bool truncated = false;
};

StorylandRetailAssetIndex storylandDiscoverRetailAssets(
    const std::wstring& dtzPath, const std::wstring& worldPath,
    const std::wstring& scmPath = {}, const StorylandVcsModloader* mods = nullptr);

bool storylandLoadPlayTestWeapons(const StorylandDtzArchive& archive,
    std::vector<StorylandWeaponDefinition>& definitions,std::vector<StorylandPlayTestAsset>& assets,
    std::string& diagnostic);

bool storylandLoadPlayTestWaterTexture(const StorylandDtzArchive& archive,RgbaImage& image,std::string& error);

bool storylandLoadPlayTestModels(const StorylandDtzArchive& archive,const std::vector<std::string>& names,
    std::vector<StorylandPlayTestAsset>& assets,std::string& error);

struct StorylandPlayTestRuntime;
bool storylandSetZombiePedestrians(StorylandPlayTestRuntime& runtime,
    const StorylandDtzArchive& archive,std::vector<StorylandPlayTestAsset>& assets,
    StorylandPlayTestAnimation& animation,bool enabled,std::string& error);

bool storylandLoadPlayTestCharacterSkin(const StorylandDtzArchive& archive,int32_t model,
    const std::string& name,StorylandPlayTestAsset& asset,std::string& error);

bool storylandLoadPlayTestEscalatorStep(const StorylandDtzArchive& archive,uint32_t model,
    StorylandPlayTestAsset& asset,std::string& error);
