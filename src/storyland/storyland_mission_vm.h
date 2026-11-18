#pragma once
#include "src/storyland/storyland_script_music.h"
#include "src/re/reVCS/audio/storyland_vcs_bullet_pass.h"
#include "src/storyland/storyland_script_player_walk.h"
#include "src/storyland/storyland_script_model_effect.h"
#include "src/leeds/leeds_ped_stats.h"
#include "src/storyland/storyland_ped_look.h"
#include "src/re/reLCS2/vehicles/storyland_lcs_vehicle_checks.h"
#include "src/re/reLCS2/hud/State.h"
#include "src/re/reLCS2/world/storyland_lcs_cranes.h"
#include "src/re/reLCS2/camera/storyland_lcs_garage_camera.h"
#include "src/re/reLCS2/runtime/storyland_lcs_entity_extension_state.h"
#include "src/re/reLCS2/runtime/storyland_lcs_extended_world.h"
#include "src/re/reLCS2/runtime/storyland_lcs_pickup_types.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_swimming_boundary.h"
#include "src/re/reVCS/world/storyland_vcs_ferris_state.h"
#include "src/re/reVCS/runtime/storyland_vcs_attachment_state.h"
#include "src/re/reVCS/runtime/storyland_vcs_mounted_gun.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_ped_motion.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_character_script_flags.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_footstep_marks.h"
#include "src/re/reVCS/hud/Hud.h"
#include "src/re/reVCS/services/storyland_vcs_vice_dispatch.h"
#include "src/re/reVCS/audio/storyland_vcs_police_radio.h"
#include "src/re/reVCS/hud/Hud.h"

#include <cstddef>
#include "src/re/reVCS/hud/storyland_pager.h"
#include "src/storyland/storyland_road_control.h"
#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <array>
#include <map>
#include <set>
#include <functional>
#include <memory>
#include <algorithm>
#include <cmath>

enum class StorylandMissionState { Idle, Running, Waiting, Finished, Unsupported, Fault };
enum class StorylandMissionOutcome : uint8_t { None, Passed, Failed };
enum class StorylandMissionFailure : uint8_t { None, Script, Death, Arrest };

struct StorylandMissionResult {
    StorylandMissionOutcome outcome=StorylandMissionOutcome::None;
    StorylandMissionFailure failure=StorylandMissionFailure::None;
    int32_t missionSlot=-1;
    std::string scriptName;
    std::string key;
    std::string reason;
    uint32_t gameMilliseconds=0;
    uint64_t sequence=0;

    template<class Archive> void saveState(Archive& archive) {
        archive(outcome,failure,missionSlot,scriptName,key,reason,gameMilliseconds,sequence);
    }
};

struct StorylandNativeZones;
struct StorylandNativePaths;
struct StorylandNativeWorld;

// Animation associations are timed in the mission clock, including its uint32 wrap.
struct StorylandAnimationFade {
    float weight=0,rate=0;
    uint32_t startedAt=0;
    bool present=false;
    float at(uint32_t now) const {
        if(!present)return 0;
        return std::clamp(weight+rate*(float(now-startedAt)*.001f),0.0f,1.0f);
    }
    void blend(uint32_t now,float nextRate) {
        weight=at(now);rate=nextRate;startedAt=now;present=true;
    }
};
struct StorylandCrouchState {
    bool requested=false,active=false;
    uint32_t deadline=0,startedAt=0;
    StorylandAnimationFade poseFade;
    int32_t movementState=1,poseAnimation=155;
};
struct StorylandAttackTimers {
    uint32_t shootOverride=0,attackOverride=0,shootDeadline=0,attackDeadline=0;
    static float truncatePositiveFloat(double value) {
        const float rounded=float(value);
        return double(rounded)>value?std::nextafter(rounded,0.0f):rounded;
    }
    static uint32_t variedDuration(uint32_t base,uint32_t random) {
        const float upper=truncatePositiveFloat(double(truncatePositiveFloat(base))*1.5);
        // EE CVT.W.S saturates positive overflow to INT_MAX (PCSX2 FPU.cpp).
        const int32_t converted=!std::isfinite(upper)||upper>=2147483648.0f?
            INT32_MAX:int32_t(std::trunc(upper));
        const uint32_t difference=uint32_t(converted)-base;
        const int64_t signedDifference=difference<=uint32_t(INT32_MAX)?int64_t(difference):
            int64_t(difference)-4294967296LL;
        const int64_t product=int64_t(random)*signedDifference;
        const int64_t scaled=product>=0?product/2147483648LL:
            -((-product+2147483647LL)/2147483648LL);
        return base+uint32_t(scaled);
    }
    template<class Random> void setShoot(uint32_t now,uint32_t callerDuration,bool crouchRequested,Random random) {
        // The native override draws before testing the old deadline.
        const uint32_t duration=shootOverride?variedDuration(shootOverride,random()):
            crouchRequested?uint32_t(truncatePositiveFloat(double(truncatePositiveFloat(callerDuration))*1.5)):callerDuration;
        if(shootDeadline<now)shootDeadline=now+duration;
    }
    template<class Random> void setAttack(uint32_t now,uint32_t callerDuration,Random random) {
        const uint32_t duration=attackOverride?variedDuration(attackOverride,random()):callerDuration;
        if(attackDeadline<now)attackDeadline=std::max(now,shootDeadline)+duration;
    }
};
inline void storylandClearCrouch(StorylandCrouchState& crouch,uint32_t now,bool clearDeadline=true) {
    crouch.active=false;
    if(clearDeadline)crouch.deadline=0;
    if(crouch.poseFade.present)crouch.poseFade.blend(now,-4.0f);
}
inline void storylandObserveCrouchMovement(StorylandCrouchState& crouch,uint32_t now,
    int32_t movementState,int32_t weaponAnimation=155) {
    if(movementState==crouch.movementState||weaponAnimation==217)return;
    crouch.movementState=movementState;
    if((movementState==2||movementState==4||movementState==5||movementState==6)&&
        crouch.poseFade.present)crouch.poseFade.blend(now,-2.0f);
}
inline void storylandAdvanceCrouch(StorylandCrouchState& crouch,uint32_t now,bool alive,
    int32_t weaponAnimation=155) {
    if(!alive) {
        crouch.requested=false;
        storylandClearCrouch(crouch,now);
    } else if(crouch.active&&crouch.deadline!=UINT32_MAX&&crouch.deadline<now) {
        // UpdateDuck uses an unsigned comparison, not a signed wrap-safe timer.
        storylandClearCrouch(crouch,now,false);
    }
    if(crouch.poseFade.present&&crouch.poseFade.rate<0&&crouch.poseFade.at(now)<=0) {
        crouch.poseFade={};
        if(weaponAnimation!=215&&weaponAnimation!=217)crouch.active=false;
    }
}

inline uint16_t storylandInitialVcsWallet(uint32_t& randomState) {
    // CGeneral::GetRandomNumber (0x408058), CPed constructor (0x1EE5E8).
    const int32_t seed=int32_t(randomState);
    const int64_t next=int64_t(seed%127773)*16807-int64_t(seed/127773)*2836;
    randomState=uint32_t(next<=0?next+2147483647:next);
    const uint16_t value=uint16_t(randomState)%25;
    return value==23?400:value;
}

struct StorylandMissionEnvironment {
    bool invertedMap = false;
    bool coopGuest = false;
    bool vcsCharacterHandleBridge = false;
    std::function<int(int32_t, std::array<float,3>&)> coopTargetPosition;
    struct OnscreenClock {
        int32_t variable=-1;
        bool countDown=true;
        std::string label;
        std::array<uint8_t,4> color{244,225,91,255};
        template<class Archive> void saveState(Archive& archive) {archive(variable,countDown,label,color);}
    } onscreenClock;
    struct LcsOnscreenCounter {
        int32_t variable=-1,type=0,total=-1,totalType=0;
        bool money=false;
        std::string label,totalLabel;
        std::array<uint8_t,4> color{112,132,157,255},background{42,58,81,255};
        template<class Archive> void saveState(Archive& archive) {
            archive(variable,type,total,totalType,money,label,totalLabel,color,background);
        }
    };
    std::array<LcsOnscreenCounter,4> lcsOnscreenCounters{};
    bool onscreenTimersFrozen=false;
    bool counterColorPending=false,counterBackgroundPending=false;
    std::array<int32_t,4> analogueSticks{}; // Live pad state; never restored from a save.
    bool confirmJustPressed=false;
    int32_t fortStauntonDestroyedGlobal=-1;
    bool fortStauntonDestroyed=false;
    std::array<int32_t,2> allowedCollisionLevels{0,0};
    int32_t lcsCollisionLevel=1;
    std::function<bool(const std::array<float,2>&,std::string&)> requestLcsCollision;
    std::function<bool(const std::array<float,3>&,std::string&)> loadSceneCollision;
    std::set<int32_t> invisibleScriptObjects;
    std::set<uint64_t> invisibleNativeObjects;
    bool nativeObjectVisibilityChanged=false;
    std::map<int32_t,int32_t> specialCharacterSlots;
    std::set<int32_t> changedSpecialModels;
    float storedClockHour=12;
    int32_t storedWeather=-1;
    bool clockStored=false,weatherStored=false;
    struct Payphone {
        uint64_t worldKey=0;
        std::array<float,3> position{};
        bool enabled=false;
        template<class Archive> void saveState(Archive& archive) {archive(worldKey,position,enabled);}
    };
    std::map<int32_t,Payphone> payphones;
    std::function<bool(const std::array<float,2>&,const std::set<uint64_t>&,uint64_t&,
        std::array<float,3>&)> findPayphone;
    std::array<int16_t,20> padState{}; // Native CPad button/axis snapshot for player zero.
    std::array<int16_t,20> previousPadState{};
    std::array<int16_t,4> padDirectionSamples{}; // Query-local native menu-axis hysteresis.
    std::array<std::array<int16_t,20>,10> padMovementSamples{};
    std::array<float,2> weaponAimPadParameters{1.05f,0.5f};
    bool padLookInverted=false;
    std::function<bool(const std::array<float,3>&,float)> sphereOnScreen;
    std::function<bool(std::array<float,3>&,std::array<float,3>&)> activeCameraVectors;
    std::function<bool(StorylandVcsBulletCameraFrame&)> activeBulletCameraFrame;
    std::function<bool(const std::array<float,3>&,std::array<float,2>&)> screenCoordinates;
    std::function<int32_t()> activeCameraMode;
    uint16_t controllerMode = 0; // The host keyboard uses the native standard mapping.
    bool frenchGame = false, germanGame = false; // Executable region, not text language.
    bool photographTaken = false;
    bool scriptBinocularOverlay = false;
    uint32_t photographFlashUntil = 0; // Transient shutter presentation.
    std::set<int32_t> photographedCharacters;
    std::set<int32_t> photographedVehicles;
    std::function<bool(int32_t,const std::array<float,3>&,const std::array<float,3>&)> photographLineClear;
    std::function<bool(int32_t,const std::array<float,3>&,const std::array<float,3>&)> photographVehicleLineClear;
    std::function<bool(int32_t,int32_t,bool)> entityCollisionStreaming;
    std::function<bool(const std::array<float,3>&,std::string&)> requestCollision;
    std::function<bool(const std::array<float,3>&,const std::array<float,3>&,
        std::array<float,3>&)> shadowCollision;
    bool generateCarsAroundCamera = false, freeHealthCare = false;
    bool securityCamera = false;
    uint8_t drunkCameraIntensity = 0, drunkVisualIntensity = 0;
    uint32_t drunkVisualStartedAt = 0, drunkVisualPeriod = 0;
    float drunkVisualAmplitude = 0;
    int16_t drunkSteeringDrift = 0;
    std::function<std::string()> activeMissionScriptName;
    // Mask: buildings1, vehicles2, characters4, objects8, dummies16.
    // True denotes a collision, matching native ProcessLineOfSight.
    std::function<bool(const std::array<float,3>&,const std::array<float,3>&,uint32_t)> scriptLineOfSightHit;
    std::function<bool(uint32_t,int32_t&,std::string&)> buildingSwapHandle;
    std::function<bool(uint32_t,std::array<float,3>&,float&)> buildingSwapBounds;
    std::function<bool(int32_t,float&)> nativeModelRadius;
    std::function<bool(int32_t,float&)> nativeModelBaseOffset;
    std::function<bool(int32_t,std::array<float,3>&)> nativeModelOriginOffset;
    bool messageSkipPressed = false; // This frame's edge on native pad Cross.
    uint8_t drunkInputDelay = 0; // Native CPad has ten movement/steering samples.
    std::function<bool(const std::array<float,6>&)> sniperBulletInArea;
    std::function<bool(const std::array<float,6>&)> missionEntityInArea;
    std::function<bool(const std::array<float,3>&,std::string&)> loadScene;
    struct CreditsLine {
        std::string key;
        int32_t offset = 0;
        int32_t height = 0;
        std::u16string text;
        template<class Archive> void saveState(Archive& archive) { archive(key,offset); }
    };
    struct CreditsState {
        bool active = false;
        int32_t group = 0;
        uint32_t scroll = 48;
        std::vector<CreditsLine> lines;
        template<class Archive> void saveState(Archive& archive) { archive(active,group,scroll,lines); }
    } credits;
    std::function<bool(int32_t,std::vector<CreditsLine>&,std::string&)> prepareCredits;
    bool creditsSkipPressed = false, creditsFastForward = false;
    bool pointerArrowActive = false;
    std::array<float,3> pointerArrowPosition{};
    // The EE script clock is published once per frame and shared by every SCM
    // thread, including children launched with a zero-millisecond bootstrap.
    float nativeScriptTimeStep=1.0f;
    float nativeAnimationTimeStep=1.0f; // CTimer4CD300, before the physics maximum clamp.
    float nativeTimeScale=1.0f;
    uint32_t nativeFrameMilliseconds=0; // Published once for MAIN, children and runtime clocks.
    uint32_t nativeFrameCounter=0; // Shared presentation cadence, transient across saves.
    uint32_t nativeRandomState=180371; // Retail408058 global seed at48A5B8.
    uint32_t previewNativeRandom() const {
        const int64_t seed=int32_t(nativeRandomState);
        int64_t next=(seed%127773)*16807-(seed/127773)*2836;
        if(next<=0)next+=INT32_MAX;
        return uint32_t(next);
    }
    uint32_t nextNativeRandom() {
        nativeRandomState=previewNativeRandom();return nativeRandomState;
    }
    std::function<float()> nativeScriptTimeStepSource;
    // Optional host effect overlay; script SET_TIME_SCALE remains independently owned.
    std::function<float()> nativeTimeScaleSource;
    std::vector<std::string> scriptTerminationRequests;
    std::deque<int32_t> collectedPickupHandles; // Native one-shot collection events, capacity 20.
    const StorylandNativeZones* zones = nullptr;
    const StorylandNativePaths* pedestrianPaths=nullptr;
    std::array<std::set<uint32_t>,2> pathSearchMarkedNodes; // Native0x200 scratch flags, car/ped.
    std::function<bool(const std::array<float,3>&,const std::array<float,3>&)> pedestrianLineClear;
    struct WanderTask {
        int32_t direction=0;
        uint32_t node=UINT32_MAX;
        template<class Archive> void saveState(Archive& archive) {archive(direction,node);}
    };
    std::map<int32_t,WanderTask> characterWanderTasks;
    // Retail 11B290/11B3F0: four script collectives share 32 member nodes.
    // Members are newest first; membership in different groups uses separate nodes.
    std::array<std::vector<int32_t>, 4> characterCollectives;
    struct CollectivePolicy {
        int32_t mode = 0; // Native 1 attacks on foot; 2 flees the selected member.
        int32_t targetGroup = 0;
        float range = 0;
        bool manualObjective = false;
        template<class Archive> void saveState(Archive& archive) {
            archive(mode, targetGroup, range, manualObjective);
        }
    };
    std::array<CollectivePolicy,4> collectivePolicies;
    bool forceCollectiveUpdate = false;
    uint16_t killFrenzyStatus=0;
    std::function<bool(const std::string&,int32_t,int32_t,int32_t,
        const std::array<int32_t,4>&,bool,bool,std::string&)> startKillFrenzy;
    struct ScriptSound {
        static constexpr int16_t LcsVehicleRocking = -2;
        uint16_t id=0;
        std::array<float,3> position{};
        bool frontend=false; // Frontend events and SCM one-shot IDs are separate native tables.
        bool vcsVehicle039a=false;
        int16_t vcsVehicleAudioEvent=-1;
        float frontendParameter=0;
    };
    std::vector<ScriptSound> scriptSounds;
    StorylandVcsPoliceRadioState vcsPoliceRadio; // Native audio-manager state; not part of saved game state.
    // Native outer service gate is process state and is not part of a save.
    // The host has no verified mapping for its nonzero transitions yet.
    bool vcsAudioServiceSuppressed=false;
    struct ContinuousSound {
        uint16_t id = 0;
        std::array<float, 3> position{};
        template<class Archive> void saveState(Archive& archive) {
            archive(id, position);
        }
    };
    std::map<int32_t, ContinuousSound> continuousSounds;
    int32_t nextContinuousSoundHandle = 1;
    std::function<bool(uint16_t)> continuousSoundAvailable;

    std::set<int32_t> importedGarageHandles; // Consumed while MAIN rebuilds imported garages.
    std::map<std::string,double> statistics; // Version 3 STAT subsystem.
    std::vector<std::string> unlockedCostumeKeys() const {
        std::vector<std::string> keys;
        const auto canonical = statistics.find("UnlockedCostumes");
        const auto found = canonical != statistics.end() ? canonical : statistics.find("CostumeUnlockMask");
        if (found == statistics.end() || !(found->second >= 0 && found->second <= UINT16_MAX)) {
            return keys;
        }
        const auto mask = uint16_t(found->second);
        for (unsigned index = 0; index < 16; ++index) {
            if (mask & (1u << index)) {
                keys.push_back("OUT_F" + std::to_string(index + 1));
            }
        }
        return keys;
    }
    StorylandPager pager; // Transient HUD state, like phone conversations.
    std::function<std::u16string(const std::string&)> pagerText;
    std::function<bool(int32_t)> animationGroupLoaded;
    std::function<bool(int32_t,std::string&)> ensureAnimationGroup;
    std::function<bool(int32_t,std::array<float,16>&)> characterWorldMatrix;
    std::vector<StorylandVcsMountedGunRequest> mountedGunRequests;
    std::function<bool(const std::string&)> animationBankAvailable;
    std::function<bool(const std::string&)> animationBankLoaded;
    std::function<bool(const std::string&)> animationRemovalDeferred;
    std::function<void(const std::string&)> removeAnimationBank;
    std::function<bool(int32_t)> nativeModelAvailable;
    std::function<int32_t()> currentPlayerModel;
    std::function<int32_t()> nativePlayerState;
    std::function<bool(bool&,std::string&)> streamingBacklogged;
    std::function<bool(int32_t,int32_t&,std::array<float,16>&)> scriptEntityModelMatrix;
    std::function<bool(int32_t,std::vector<StorylandModelScriptEffect>&)> modelScriptEffects;
    std::function<int32_t(int32_t)> empireBuildingHandle;
    std::function<bool(int32_t,const std::string&,std::string&)> validateSpecialModel;
    std::function<float(int32_t,int32_t)> animationDuration;
    std::function<float(int32_t,int32_t)> weaponAnimationDuration;
    std::function<bool(int32_t,int32_t)> animationLoops;
    std::function<bool(int32_t,int32_t,uint16_t&)> animationAssociationFlags;
    std::function<float(int32_t,int32_t)> animationMovementSpeed;
    std::function<bool(const std::string&,bool,std::string&)> setLcsAnimationBankLoaded;
    std::function<float(const std::string&,const std::string&)> lcsAnimationDuration;
    std::set<std::string> lcsAnimationBanks;
    struct LcsWaitAnimation {
        int32_t state=0;
        uint32_t startedAt=0;
        int32_t durationMilliseconds=0;
        uint64_t serial=0;
        bool repeat=false; // LC13 extension; keep the legacy record layout below.
        template<class Archive> void saveState(Archive& archive) {archive(state,startedAt,durationMilliseconds,serial);}
    };
    std::map<int32_t,LcsWaitAnimation> lcsWaitAnimations;
    std::function<int32_t(int32_t)> characterCrouchWeaponAnimation;
    std::function<bool(int32_t)> characterCrouchWeaponActive;
    std::function<int32_t(int32_t)> nativePedSpeechProfile;
    std::function<int32_t(const std::string&)> modelIdByName;
    std::function<void(const std::array<float,6>&,bool)> clearAmbientArea;
    struct MobilePhone {bool answering=false;uint32_t startedAt=0;};
    std::map<int32_t,MobilePhone> mobilePhones;
    bool suppressLevelSplash=false;
    std::function<bool(const std::array<float,3>&,float&)> groundHeightAt;
    std::function<bool(const std::array<float,3>&,float&)> waterHeightAt;
    struct Garage {
        int32_t handle=0,type=0;
        int32_t maximumCars=0;
        bool swingDoor=false;
        bool locked=false;
        std::array<float,8> bounds{};
        // CGarage states: closed, open, closing, opening, open-awaiting-car,
        // and mission delivery complete. Kept outside the legacy save layout.
        uint8_t state=0;
        float doorPosition=0;
        int32_t targetVehicle=-1,serviceVehicle=-1;
        uint32_t serviceUntil=0,deliveredModelMask=0;
        bool resprayDone=false,cameraFollowsPlayer=false;
        template<class Archive> void saveState(Archive& ar) {
            ar(handle,type,maximumCars,swingDoor,locked,bounds);
        }
};
    struct LcsGarageDoor {
        uint64_t worldKey=0;
        uint32_t model=0;
        std::array<float,16> originalMatrix{};
        template<class Archive> void saveState(Archive& archive) {archive(worldKey,model,originalMatrix);}
    };
    struct LcsGarageDoorState {
        int32_t state=0; // Native fully closed, opened, closing, opening.
        float position=0,height=4;
        uint32_t actionTime=0;
        std::vector<LcsGarageDoor> doors;
        template<class Archive> void saveState(Archive& archive) {archive(state,position,height,actionTime,doors);}
    };
    std::map<int32_t,LcsGarageDoorState> lcsGarageDoors;
    std::set<int32_t> lcsGarageFollowPlayer;
    StorylandLcsGarageCameraCache lcsGarageCamera;
    std::function<bool(const Garage&,LcsGarageDoorState&,std::string&)> prepareLcsGarageDoors;
    std::function<bool(const LcsGarageDoor&,const std::array<float,16>&,std::string&)> applyLcsGarageDoorTransform;
    struct Object {
        int32_t handle=0,model=0;
        float x=0,y=0,z=0;
        float heading=0;
        float rotationX=0,rotationY=0;
        bool persistent=false;
        int32_t nativePoolIndex = -1;
        uint8_t creationSource = 2;
        // Extension state is deliberately outside the original saveState layout.
        std::array<float,3> velocity{},rotationVelocity{},attachmentOffset{};
        bool dynamic=false,gravity=true,collisionEnabled=true,positionFrozen=false;
        bool recordsCollisions=false,collided=false,inWater=false,drawLast=false;
        bool deferredRender=false;
        int32_t ignoredVehicle=-1;
        std::vector<int32_t> collidedVehicles;
        bool ropeLiftable=false;
        int32_t area=0,attachedVehicle=-1,attachedCharacter=-1,attachedBone=0;
        std::array<float,3> attachmentRotation{};
        struct DamageState {
            bool propertiesLoaded=false;
            float mass=1,turnMass=1,airResistance=.99f,elasticity=.5f,buoyancy=1;
            float uprootLimit=0,multiplier=1;
            uint8_t damageEffect=0,collisionResponse=0,effect1=0,effect2=0,proofs=0;
            bool cameraAvoid=false,disableMoveForce=false,disableTurnForce=false;
            int8_t health=0,lastWeapon=-1;
            bool visible=true,renderDamaged=false;
        } damage;
        template<class Archive> void saveState(Archive& ar) {
            ar(handle,model,x,y,z,heading,persistent);
        }
};
    struct AudioSlot {
        std::string name, subtitleKey;
        int32_t flags=0;
        bool priority=false,playFlag=false,reserved=false;
        uint32_t durationMilliseconds=0;
        uint64_t serial=0;
        int32_t animationGroup=-1; // Version 3 XTRA subsystem, defaulted for older saves.
        bool loaded=false,finished=false,play=false,positioned=false;
        bool playbackStarted=false;
        // 0413 keeps its pointer/null distinction separately from subtitle text.
        // Loading is host-transient; the other flags are preserved by SC71.
        bool vcsExtendedLoad=false,vcsSubtitlePresent=false,vcsReleasePending=false,vcsResourceActive=false;
        bool vcsBackendLoading=false;
        float x=0,y=0,z=0;
        template<class Archive> void saveState(Archive& ar) {
            ar(name,subtitleKey,flags,priority,playFlag,reserved,durationMilliseconds,serial,loaded,finished,
                play,positioned,playbackStarted,x,y,z);
        }
};
    std::array<AudioSlot,8> audioSlots{};
    bool vcsMissionAudioAutoRelease(size_t index) const {
        if(index>=5)return false;
        const auto& slot=audioSlots[index];
        return slot.vcsExtendedLoad&&!slot.priority&&(slot.flags!=-1||slot.vcsSubtitlePresent);
    }
    void finishVcsMissionAudioRelease(size_t index) {
        if(index>=5)return;
        auto& slot=audioSlots[index];
        const bool finished=slot.finished;
        slot={};slot.finished=finished;slot.flags=-1;slot.serial=nextAudioSerial++;
    }
    void requestVcsMissionAudioRelease(size_t index) {
        if(index>=5)return;
        auto& slot=audioSlots[index];
        // The original manager only releases allocated records. A request
        // during the asynchronous loading stage completes when loading ends.
        if((!slot.vcsResourceActive&&slot.name.empty())||slot.vcsReleasePending)return;
        if(slot.vcsBackendLoading)slot.vcsReleasePending=true;
        else finishVcsMissionAudioRelease(index);
    }
    std::vector<StorylandScriptMusicEmitter> musicEmitters;
    std::array<int32_t,112> scriptMusicStartPositions{};
    bool saveMenuRequested=false,saveGameFinished=true;
    std::array<float,3> savePosition{};
    bool savePositionValid=false,savePositionExterior=false;
    int32_t saveLevel=0;
    bool audioSubsystemEnabled=true; // Disable only for script probes without an audio worker, never for mute.

    int32_t playerSpeechMood=0;
    uint32_t playerSpeechMoodMilliseconds=0;
    uint64_t nextAudioSerial=1;
    std::vector<Garage> garages;
    bool freeBombs=false,freeResprays=false,taxiBoost=false;
    std::array<uint32_t,4> importGarageMasks{};
    std::function<bool(int32_t,std::array<std::array<float,3>,8>&)> vehicleWorldCorners;
    std::function<int32_t(int32_t,int32_t)> garageImportModel;
    std::function<void(const Garage&)> updateGarageDoor;
    std::vector<Object> objects;
    std::set<int32_t> missionCleanupCharacters,missionCleanupVehicles,missionCleanupObjects;
    int32_t storedMissionVehicleHandle=-1;
    bool storedMissionVehicleWasAmbient=false;
    // Native CPlayerPed keeps four raw object handles. Deleted handles remain
    // here until MAKE_OBJECT_TARGETTABLE reuses the first vacant pool slot.
    std::array<int32_t,4> targettableObjects{-1,-1,-1,-1};
    enum class TargetKind : uint8_t { None, Character, Object };
    struct Target {
        TargetKind kind=TargetKind::None;
        int32_t handle=-1;
    } playerTarget;
    std::map<int32_t,Target> characterTargets;
    enum class DamageSourceKind : uint8_t {None,Character,Vehicle,Object};
    struct DamageSource {
        DamageSourceKind kind=DamageSourceKind::None;
        int32_t handle=-1;
        template<class A> void saveState(A& a) {a(kind,handle);}
    };
    struct ScriptProjectile {
        int32_t weaponType=-1;
        bool active=false,targeted=false;
        std::array<float,3> position{},velocity{};
        DamageSource source;
        uint32_t deadline=0;
        int32_t flag=0;
        float age=0,trailTime=0,yaw=0;
        bool settled=false;
        uint64_t sequence=0;
        template<class A> void saveState(A& archive) {
            archive(weaponType,active,targeted,position,velocity,source,deadline,flag,
                age,trailTime,yaw,settled,sequence);
        }
    };
    std::array<ScriptProjectile,32> scriptProjectiles{};
    DamageSource playerDamageSource;
    int32_t playerLastWeaponDamage=-1;
    std::function<bool(int32_t,Object::DamageState&)> nativeObjectProperties;
    std::function<int32_t(int32_t)> resolveObjectModel;
    const StorylandNativeWorld* objectWorld=nullptr;
    StorylandVcsFerrisWheelState ferrisWheel;
    std::function<float(const std::array<float,3>&,const std::array<float,3>&)> objectSegmentCollision;
    std::function<bool(int32_t)> nativeObjectBreakableGlass;
    std::function<float(int32_t,uint32_t,bool)> nativeObjectMeleeDamage;
    std::array<int32_t,2> nativeExplosiveObjects{-1,-1};
    std::function<bool(const Object&)> objectOnScreen;
    std::function<bool(const Object&)> objectInWater;
    std::function<bool(const Object&,int32_t)> objectTouchingCharacter;
    std::function<bool(int32_t,int32_t,std::array<float,16>&)> characterBoneMatrix;
    std::function<bool(const std::array<float,3>&,float,int32_t,bool,std::string&)> setClosestObjectVisibility;
    struct ScriptCorona {
        std::array<float,3> position{};
        float size=1;
        int32_t type=0,flareType=0;
        std::array<uint8_t,3> color{255,255,255};
        template<class A> void saveState(A& a) {a(position,size,type,flareType,color);}
    };
    std::map<int32_t,ScriptCorona> scriptCoronas;
    std::map<std::pair<uintptr_t,int32_t>,ScriptCorona> frameScriptCoronas;
    struct ScriptCheckpoint {
        std::array<float,3> position{},direction{};
        std::array<uint8_t,4> color{255,255,255,100};
        float radius = 1;
        uint8_t type = 8;
        int area = 0;
        bool nativeDrawCheckpoint = false;
    };
    std::map<std::pair<uintptr_t,int32_t>,ScriptCheckpoint> frameScriptCheckpoints;
    struct ScriptParticle {
        int32_t type = 0;
        std::array<float,3> position{},velocity{};
        float size = 0;
        std::array<uint8_t,4> color{}; // Zero alpha requests the native descriptor colour.
        uint32_t lifetimeMilliseconds = 0; // Transient native AddParticle override; zero uses the descriptor.
        bool emitterParametersSupported() const {
            if(!std::isfinite(size)||size<0||!std::all_of(position.begin(),position.end(),
                [](float value){return std::isfinite(value);}))return false;
            const float largestSize=type==18?size*1.2f:size;
            if(largestSize>10000)return false;
            for(float value:velocity) {
                if(!std::isfinite(value))return false;
                if(type==12||type==13)continue; // Native constructors erase velocity.
                const float largestSpeed=type==18?std::abs(value)*1.35f:
                    type==17&&value!=0?std::abs(value)+.02f:
                    type==8?std::abs(value)+.25f:std::abs(value);
                if(largestSpeed>100000)return false;
            }
            return true;
        }
        template<class Archive> void saveState(Archive& archive) {
            archive(type,position,velocity,size,color);
        }
    };
    struct ScriptParticleEmitter {
        ScriptParticle particle;
        uint32_t deadline = 0;
        bool finite = false;
        uint16_t frameCounter = 0;
        bool burstStarted = false;
        template<class Archive> void saveState(Archive& archive) {
            archive(particle,deadline,finite,frameCounter,burstStarted);
        }
    };
    std::vector<ScriptParticleEmitter> scriptParticleEmitters;
    std::function<bool(const std::vector<ScriptParticle>&,std::string&)> createScriptParticles;
    std::function<std::array<float,3>()> particleCameraPosition;
    std::function<bool(uint16_t)> scriptEmitterAvailable;
    bool dollarParticleEnabled = false;
    std::function<void(bool)> switchDollarParticles;
    std::function<bool(uint8_t,const ScriptParticle&,std::string&)> createDollarParticles;
    int32_t nextScriptCoronaId=0; // Retail0x4CD408 starts in zero-initialized BSS.
    struct ScriptExplosion {
        std::array<float,3> position{};
        int32_t type=0;
        uint32_t startedAt=0,lifetime=750;
        bool active=false;
        DamageSource source;
        template<class A> void saveState(A& a) {a(position,type,startedAt,lifetime,active,source);}
    };
    std::array<ScriptExplosion,48> scriptExplosions{};
    std::function<bool(ScriptExplosion&,std::string&)> createScriptExplosion;
    struct ScriptFire {
        std::array<float,3> position{};
        int32_t attachedKind=0,attachedHandle=-1;
        bool active=true;
        float strength=1;
        bool suppressionPending=false;
        bool scriptOwned=true;
        uint32_t endsAt=0;
        float scale=.8f;
        DamageSource source;
        template<class A> void saveState(A& a) {a(position,attachedKind,attachedHandle,active,strength,scriptOwned,endsAt,scale,source);}
    };
    std::map<int32_t,ScriptFire> scriptFires;
    // Transient ambient dispatch timing; these are not script-owned vehicles.
    uint32_t vcsLastFireDispatch=0;
    uint32_t vcsLastAmbulanceDispatch=0;
    struct VehicleScriptState {
        float x=0,y=0,z=0,health=1000;
        float maximumHealth=1000;
        int32_t model=0;
        int32_t spawnedSlot=-1;
        bool alive=true,deleted=false,persistent=true;
        bool vcsSeaMineLauncher=false; // 043A: native vehicle+0x268 bit23.
        int32_t scriptColorSet=-1;
        int32_t colorSet=-1;
        bool clumpVisible=true;
        bool ambientOrigin=false;
        bool watertight=false,strong=false,upsideDownDamageDisabled=false;
        bool canBurstTyres=true,visiblyDamaged=false,boatAnchored=false,boatAutopilot=false;
        float tractionMultiplier=1;
        std::array<uint8_t,4> tyreStatus{};
        // Native damage-manager order: bonnet, boot, LF, RF, LR, RR.
        std::array<uint8_t,6> doorDamage{};
        std::array<float,6> doorOpenRatios{};
        float bootAngularVelocity=0;
        bool doorsChangedByScript=false;
        std::array<float,3> rotation{},angularVelocity{};
        bool rotationChangedByScript=false,angularVelocityChangedByScript=false;
        bool aiActive=false;
        int8_t drivingStyle=0,aiMission=0;
        int32_t aiTargetVehicle=-1;
        uint32_t aiMissionStartedAt=0;
        uint16_t routeSeed=0;
        bool routeSeedChanged=false,avoidLevelTransitions=false;
        bool fastLane=true,lanePolicySet=false,convoy=false;
        uint8_t straightLineDistance=0;
        uint16_t alarmMilliseconds=0;
        bool alarmChangedByScript=false;
        float massMultiplier=1,turnMassMultiplier=1;
        std::array<uint8_t,7> panelDamage{};
        std::array<uint8_t,4> lightDamage{};
        float heliOrientation=-1,rotorSpeed=0;
        bool heliStabiliser=false,heliCrashing=false,rotorChangedByScript=false;
        int32_t scriptStatus=-1; // Version 3 XTRA subsystem.
        int32_t primaryColor=-1,secondaryColor=-1; // Version 4 vehicle overrides.
        bool healthChangedByScript=false;
        uint8_t damageProofs=0; // Bullet, fire, explosion, collision, melee.
        bool extinguishFireRequested=false;
        int32_t autoPilotMission=-1;
        bool autoPilotRoutePending=false;
        float boatAnchorHeading=10000.0f;
        bool engineOn=true;
        bool heavy=false;
        bool movedByScript=false;
        bool teleportedByScript=false;
        bool headingChangedByScript=false,velocityChangedByScript=false;
        bool positionFrozen=false,collisionEnabled=true,loadCollision=true;
        bool onlyDamagedByPlayer=false,canBeDamaged=true;
        bool siren=false,taxiLight=false,canRespray=true,fadeOut=false,visible=true;
        bool extinguishRequested=false,wrecked=false;
        int32_t doorLock=0,lastWeaponDamage=-1,bombType=0;
        int32_t bombOwner=-1,previousBombDriver=-1;
        uint32_t bombDeadline=0,lastHunterShot=0;
        bool bombCountdownActive=false,betaBombInstalled=false;
        bool contactExplosion=false,ropeLiftable=false,bikeStandingOverride=false;
        bool bikeStandingChangedByScript=false;
        int32_t ropeType=0;
        std::array<float,3> ropeOffset{};
        DamageSource lastDamageSource;
        uint8_t renderAlpha=255;
        std::array<float,3> velocity{};
        float heading=0,speed=0,cruiseSpeed=0;
        float targetX=0,targetY=0,targetZ=0;
        bool drivingToTarget=false;
        int32_t driver=-1,temporaryAction=0;
        uint32_t temporaryActionUntil=0;
        std::array<int32_t,2> components{-1,-1};
        template<class Archive> void saveState(Archive& ar) {
            ar(x,y,z,health,model,spawnedSlot,alive,deleted,persistent,scriptColorSet,movedByScript,heading,
                speed,cruiseSpeed,targetX,targetY,targetZ,drivingToTarget,driver,temporaryAction,
                temporaryActionUntil,components);
        }
};
    std::map<int32_t,VehicleScriptState> missionVehicles;
    struct VehicleRope {
        int32_t owner=-1,type=0,hookObject=-1,attachedObject=-1,attachedVehicle=-1;
        int32_t vcsReacquireDelay=0;
        float length=0;
        std::array<float,3> anchor{},end{},velocity{};
        template<class A> void saveState(A& a) {
            a(owner,type,hookObject,attachedObject,attachedVehicle,length,anchor,end,velocity);
        }
    };
    std::map<int32_t,VehicleRope> vehicleRopes;
    std::function<bool(int32_t,float&,int32_t&)> nativeRopeDefinition;
    std::function<bool(int32_t,std::string&)> fireHunterGun;
    int32_t remoteVehicle=-1;
    bool remoteDetonationEnabled=true,remoteContactDetonation=true;
    float rcHeliHeightLimit=500;
    bool playerCarBrakes=false;
    int16_t enterCarRangeMultiplier=1;
    std::function<bool(const std::array<float,3>&,float)> createRandomParkedVehicle;
    std::function<void(const std::array<float,4>&)> unlockVehicleDoorsInArea;
    std::function<bool(int32_t,int32_t,std::array<float,3>&)> vehiclePathCoordinate;
    std::function<bool(int32_t,std::array<float,3>&,float&)> vehicleBoundingSphere;
    std::function<int32_t(int32_t)> consumeVehicleCollision;
    std::function<bool(int32_t)> vehicleEntryInProgress;
    std::function<bool(int32_t)> vehicleFlightAvailable;
    std::function<bool(int32_t)> vehicleBoatBuoyant;
    std::function<bool(float,float,float&)> vehicleWaterHeight;
    std::function<int32_t(const std::array<float,4>&,int32_t,bool)> findAmbientVehicleNoSave;
    std::function<int32_t(int32_t,bool)> vehicleColorSetCount;
    std::function<bool(int32_t,bool,int32_t,std::array<int32_t,2>&)> vehicleColorSet;
    std::function<bool(const std::array<float,6>&,uint32_t)> areaOccupied;
    std::function<int(int32_t,const std::array<float,3>&)> objectSlideCollision;
    std::function<int32_t(const std::array<float,4>&,int32_t)> acquireAmbientVehicle;
    enum VehicleAreaFlags : uint32_t { VehicleAreaRoad=1,VehicleAreaAmbient=2,VehicleAreaAlive=4 };
    std::function<std::vector<int32_t>(const std::array<float,4>&,int32_t,uint32_t)> findScriptVehiclesInArea;
    std::function<void(bool)> setAllCarsDamageable;
    std::function<bool(int32_t,int32_t,std::string&)> configureCarMission;
    std::function<bool(int32_t)> vehicleIsBoat;
    std::function<bool(int32_t,uint32_t&)> vehicleHandlingFlags;
    struct RemoteVehicleControl {
        int32_t vehicle=-1;
        uint32_t lostAt=0;
        bool ending=false,restoreCamera=true,detonationDisabled=false,firstPerson=false;
        template<class Archive> void saveState(Archive& archive) {
            archive(vehicle,lostAt,ending,restoreCamera,detonationDisabled,firstPerson);
        }
    } remoteControl;
    std::function<bool(int32_t,std::string&)> prepareRemoteVehicle;
    std::function<void(int32_t)> explodeMissionVehicle;
    std::function<int32_t()> acquireOccupiedVehicle;
    std::function<bool(int32_t)> vehicleInWater;
    std::function<int(int32_t)> vehiclePassengerCapacity;
    // Promote a real ambient vehicle occupant to a script-visible character on demand.
    std::function<int32_t(int32_t,int32_t)> acquireAmbientVehiclePassenger;
    std::function<bool(int32_t,int32_t)> ambientVehiclePassengerPresent;
    std::function<std::array<float,3>(int32_t)> vehicleOrientationAndSpeed;
    struct VehicleContacts {
        bool valid=false,twoWheels=false,bodyContact=false;
        std::array<bool,4> wheels{}; // Cars LF, LR, RF, RR; bikes front pair, rear pair.
        float forwardZ=0,rightZ=0,speed=0;
    };
    std::function<VehicleContacts(int32_t)> vehicleContacts;
    std::function<bool(int32_t,std::array<float,9>&)> vehicleBasis;
    std::function<bool(int32_t,std::array<float,16>&)> vehicleWorldMatrix;
    std::function<bool(int32_t,std::array<float,3>&)> vehicleWorldVelocity;
    std::function<bool(int32_t,int32_t,uint32_t)> vcsCharacterVehicleTurnForce;
    std::function<bool(int32_t,std::array<float,3>&)> nativeVehicleCameraSeat;
    std::function<float(int32_t)> vehicleMaximumAiSpeed;
    std::function<int32_t(int32_t)> vehicleNativeType;
    std::function<int32_t(int32_t)> vehicleHandlingClass;
    std::function<int32_t(int32_t)> vehicleOnScreen,characterOnScreen;
    std::function<bool(int32_t)> vehicleOnFire,characterInWater;
    std::function<void(bool)> setAllVehicleDamageEnabled;
    std::function<int32_t(int32_t)> acquireVehicleDriver;
    std::function<bool(int32_t)> vehicleCanExit;
    std::function<bool(int32_t,int32_t,std::array<float,3>&)> vehicleExitPosition;
    struct VehicleStunt {
        int32_t kind=0;
        uint32_t startedAt=0;
        float distance=0;
        std::array<int32_t,3> milliseconds{};
        std::array<float,3> distances{};
        std::array<uint32_t,3> activeMilliseconds{};
        std::array<float,3> activeDistances{};
        uint32_t graceMilliseconds=0;
        int32_t vehicle=-1;
        std::array<float,3> previousPosition{};
        bool previousPositionValid=false;
    };
    VehicleStunt playerVehicleStunt;
    std::map<int32_t,uint32_t> upsideDownCarChecks;
    std::array<StorylandLcsStuckCarCheck,16> lcsStuckCarChecks{};
    StorylandLcsCraneSystem lcsCranes;
    std::function<std::array<float,3>()> craneCameraPosition;
    struct StuckCarCheck {
        int32_t vehicle = -1;
        std::array<float,3> position{-5000.0f, -5000.0f, -5000.0f};
        uint32_t sampledAt = UINT32_MAX;
        float distance = 0;
        uint32_t interval = 0;
        bool stuck = false;
    };
    std::array<StuckCarCheck,16> stuckCarChecks;
    int32_t nextMissionVehicleHandle=1;
    struct CharacterBehavior {
        bool canDoDriveBy = false;
        bool canAttackPlayerWithCops = false;
        bool seekNodesForMission = false;
        bool onlyFrontPassenger = false;
        bool localAvoidanceSolver = false;
        bool alternateObstructionHandling = false;
        bool allowDuckAgainstArmedTargets = false;
        bool useUncontrolledWeaponDamage = false;
        int32_t targetPriority = 0;
        bool dropsWeapons = true, frightenedWhenJacked = false, leavingVehicleToDie = false;
        uint32_t dyingExitStartedAt = 0;
        int32_t forcedThreat = -1;
        bool forcedThreatIsVehicle = false;
        int32_t conversationPartner = -1;
        uint32_t conversationDeadline = 0, nextConversationSpeech = 0;
        std::array<float,4> area{};
        bool areaEnabled = false, adjustTargetsToArea = false, areaBoundaryBlocked = false;
        int32_t iceCreamVehicle = -1, iceCreamSlot = -1;
        uint32_t iceCreamStartedAt = 0, iceCreamDeadline = 0;
        bool attemptedAttractor = false, boughtIceCream = false;
        uint32_t diveDeadline = 0;
        std::array<float,2> diveVelocity{};
        float statFleeDistance = 16, statHeadingRate = 7.5f;
        float statAttackStrength = 1, statDefendWeakness = 1;
        uint8_t statFear = 50, statTemper = 50, statLawfulness = 50, statView = 50;
        uint8_t statFlags = 0;
        template<class Archive> void saveState(Archive& ar) {
            ar(targetPriority, dropsWeapons, frightenedWhenJacked, leavingVehicleToDie, dyingExitStartedAt,
                forcedThreat, forcedThreatIsVehicle, conversationPartner, conversationDeadline,
                nextConversationSpeech, area, areaEnabled, adjustTargetsToArea, areaBoundaryBlocked,
                iceCreamVehicle, iceCreamSlot, iceCreamStartedAt, iceCreamDeadline, attemptedAttractor, boughtIceCream,
                diveDeadline, diveVelocity, statFleeDistance, statHeadingRate, statAttackStrength, statDefendWeakness,
                statFear, statTemper, statLawfulness, statView, statFlags);
        }
    };
    struct Character {
        bool networkControlled = false;
        StorylandPedStats pedStats; // Native model default or SET_CHAR_PERSONALITY override.
        CharacterBehavior behavior;
        StorylandVcsCharacterScriptFlags vcsScriptFlags;
        StorylandVcsFootstepMarkState vcsFootstepMarks;
        bool dontReenterAfterEjection=false,avoidUnsupportedForwardStep=false;
        uint8_t vcsSeekNodeRadius=5; // Native CPed+0x793; signed at the seek-distance check.
        uint8_t nativePedRandomSeed=0;
        bool nativePedRandomSeedInitialized=false;
        uint8_t accuracy=60,damageProofs=0;
        uint32_t fearFlags=0;
        int32_t passengerSeat=-1;
        int32_t model=0,type=0,vehicle=-1,animationGroup=0,objective=0;
        float x=0,y=0,z=0,heading=0,health=100;
        float maximumHealth=100;
        uint64_t speechIdentity=0;
        StorylandAttackTimers attackTimers;
        bool alive=true,visible=true,persistent=true,attached=false;
        std::array<float,3> attachment{};
        int32_t animationId=-1,lookAtCharacter=-1;
        float animationBlend=1;
        uint32_t animationStartedAt=0;
        bool animationLoop=false;
        bool walkingToTarget=false;
        float targetX=0,targetY=0,walkSpeed=1.25f;
        float targetRadius=0.5f;
        bool positionFrozen=false;
        bool collisionEnabled=true;
        bool disableVerticalMovement=false;
        bool testShotInVehicle=false;
        bool suffersCriticalHits=true;
        bool cantBeDraggedOut=false;
        uint16_t money=0;
        bool moneyGivenByScript=false,deathDropsCreated=false;
        float movementSpeed=0;
        double walkedDistance=0;
        uint8_t movementState=0; // Native none, still, walk, run, sprint.
        uint32_t deathStartedAt=0;
        bool deathRecorded=false;
        float armour=0;
        int32_t pedStat=-1,currentWeapon=0,lastWeaponDamage=-1;
        DamageSource lastDamageSource;
        struct WeaponState {
            int32_t slot=-1,model=-1,ammunition=0;
            bool owned=false;
        };
        std::map<int32_t,WeaponState> weapons;
        bool weaponsChanged=false,weaponSelectionChanged=false;
        bool onlyDamagedByPlayer=false,criticalHits=true,bleeding=true;
        bool heedThreats=true,searchThreats=true,ignoreThreatsBehindObjects=false;
        bool crouchWhenThreatened=false,allowFrontPassenger=true;
        bool keepObjectiveOnRelease=false,loadCollision=true;
        // Native CPed behavior bits; SC22 persists these independently of the
        // original Character save layout.
        bool stayInSamePlace = false;
        bool cannotBeDraggedOut = false;
        bool stayInCarWhenJacked = false;
        bool canBeDraggedFromPassengerSide = false;
        bool passengerSideCarjack = false;
        bool mountedGunInitialized = false;
        bool canBeShotInVehicle = false;
        bool neverTargeted = false;
        bool stopShooting = false;
        bool playerFriend = false;
        bool canFightInGroup = true;
        bool drownsInWater = true;
        uint16_t gangDamageMask = UINT16_MAX;
        uint32_t ceaseAttackDelay = 0;
        uint32_t attackStartedAt = 0;
        int32_t fleeVehicle = -1;
        uint32_t fleeVehicleDeadline = 0;
        bool objectiveCompleted = false;
        float objectiveHeight = 0;
        bool followingScriptPath = false; // Native FOLLOW_PATH state, independent of objective.
        bool threatVisible = false;
        int16_t routeId = -1;
        int16_t routeMode = 1;
        int16_t routeCursor = 0;
        int16_t routeDirection = 1;
        int32_t colorSet = -1, scriptColorSet = -1;
        std::array<uint32_t,6> clothingColors{
            UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
        bool clothingColorsSet = false;
        bool weaponDropped=false,moneyDropped=false,playerKillCounted=false;
        bool walletInitialized=false,walletForced=false,provokedByPlayer=false;
        bool isChrisCriminal=false;
        int32_t creationSource=2;
        uint16_t walletMoney=0;
        uint8_t renderAlpha=255;
        int32_t speechProfile=-1;
        bool speechEnabled = true;
        bool fadeOut=false;
        bool removeFromWorld = false;
        int32_t objectiveTarget=-1,leader=-1,formation=0;
        bool leaderOutOfRange=false;
        uint32_t leaderUpdateAfter=0;
        uint32_t leaderVehicleAfter=0;
        float objectiveRadius=1.0f;
        uint32_t shotAt=0;
        bool hasShot=false;
        bool seekNodes=false;
        uint32_t seekNode=UINT32_MAX,seekWaitUntil=0;
        std::vector<uint32_t> seekRoute;
        uint32_t seekRouteCursor=0;
        std::array<float,2> seekDestination{};
        int32_t waitState=0;
        uint32_t waitDeadline=0;
        bool waitVersionB=false;
        StorylandCrouchState crouch;
        template<class Archive> void saveState(Archive& ar) {
            ar(model,type,vehicle,animationGroup,objective,x,y,z,heading,health,alive,visible,persistent,
                attached,attachment,animationId,lookAtCharacter,animationBlend,animationStartedAt,
                animationLoop,walkingToTarget,targetX,targetY,walkSpeed);
        }
};
    std::map<int32_t,Character> missionCharacters;
    int32_t vcsFollowerLocomotionGroup(const Character& character, int32_t authoredGroup) const {
        if (character.vcsScriptFlags.disablePlayerLeaderAnimationGroup || character.leader < 0)
            return authoredGroup;
        if (character.leader == 0) return 30;
        const auto leader = missionCharacters.find(character.leader);
        return leader != missionCharacters.end() && leader->second.type == 0 ? 30 : authoredGroup;
    }
    int32_t lcsFollowerLocomotionGroup(int32_t handle,const Character& character,int32_t authoredGroup) const {
        const auto flags=characterFlags.find(handle);
        if((flags!=characterFlags.end()&&(flags->second&0x2000u))||character.leader<0)
            return authoredGroup;
        if(character.leader==0)return 31;
        const auto leader=missionCharacters.find(character.leader);
        return leader!=missionCharacters.end()&&leader->second.type==0?31:authoredGroup;
    }
    std::map<int32_t,StorylandVcsPedFallState> vcsPedFalls;
    std::function<bool(int32_t,int32_t,StorylandPedStats&)> resolvePedStats;
    struct CharacterVehicleTask {
        int32_t vehicle=-1,seat=-1,destinationVehicle=-1,destinationSeat=-1;
        bool exiting=false,hijacking=false,animating=false,low=false,rightHandSide=false,boat=false,driverRemoved=false;
        uint32_t startedAt=0,routeIndex=0;
        std::array<float,8> phaseSeconds{};
        std::array<float,3> door{},seatPosition{},start{},vehicleOrigin{};
        float vehicleHeading=0;
        std::vector<std::array<float,3>> route;
        template<class Archive> void saveState(Archive& archive) {
            archive(vehicle,seat,destinationVehicle,destinationSeat,exiting,hijacking,animating,low,
                rightHandSide,boat,driverRemoved,startedAt,routeIndex,phaseSeconds,door,seatPosition,
                start,vehicleOrigin,vehicleHeading,route);
        }
    };
    std::map<int32_t,CharacterVehicleTask> characterVehicleTasks;
    bool vcsCharacterExitingVehicle(int32_t handle) const {
        const auto found = missionCharacters.find(handle);
        if (found == missionCharacters.end()) return false;
        const auto& character = found->second;
        if (character.objective == 0x00E8 || character.objective == 0x00F1) return true;
        const auto task = characterVehicleTasks.find(handle);
        return (character.objective == 0x00EC || character.objective == 0x00F2) &&
            task != characterVehicleTasks.end() && task->second.exiting &&
            (task->second.vehicle == character.vehicle ||
                (task->second.animating && character.vehicle < 0));
    }
    struct CharacterAttachment {
        uint16_t type=0;
        float rotationLimit=0;
        int32_t savedWeapon=-1,savedAmmo=0;
        template<class Archive> void saveState(Archive& archive) {
            archive(type,rotationLimit,savedWeapon,savedAmmo);
        }
    };
    std::map<int32_t,CharacterAttachment> characterAttachments;
    std::map<int32_t,StorylandVcsCharacterAttachment> vcsCharacterAttachments;
    std::map<int32_t,int32_t> vcsCharacterObjectParents;
    bool activeVcsCharacterAttachment(int32_t handle) const;
    void releaseVcsObjectAttachments(int32_t handle);
    std::function<bool(int32_t,std::array<float,16>&)> vehicleAttachmentFrame;
    struct CharacterHitSphere {
        std::array<float,3> center{};
        float radius=0;
        uint8_t piece=0;
    };
    std::function<bool(int32_t,std::vector<CharacterHitSphere>&)> characterHitSpheres;
    uint16_t lcsEnterCarRangeMultiplier=1;
    std::function<bool(int32_t,CharacterVehicleTask&,std::string&)> prepareCharacterVehicleTask;
    std::function<bool(int32_t,const std::array<float,3>&,std::array<float,3>&)> moveMissionCharacter;
    std::function<bool(int32_t)> characterReactionOwned;
    // True when the live character is in a paired action with another ped.
    std::function<bool(int32_t)> characterPairedAction;
    std::function<bool(int32_t)> characterReceivingPairedAction;
    std::function<void()> clearRestartTaxi;
    std::function<void(int32_t)> clearVehicleSpawnSpace;
    std::function<void(int32_t,const std::array<float,3>&)> removeVehicleDriver;
    std::function<void(int32_t,int32_t,unsigned,float)> updateVehicleTaskDoor;
    void releaseVcsVehicleAttachments(int32_t handle);
    void releaseCharacterVehicleReferences(int32_t handle) {
        releaseVcsVehicleAttachments(handle);
        for(auto& entry:vcsPedFalls)
            if(entry.second.sourceVehicle==handle)entry.second.sourceVehicle=-1;
        for (auto& entry : missionCharacters) {
            auto& character = entry.second;
            auto& behavior = character.behavior;
            if (character.fleeVehicle == handle) {
                character.fleeVehicle = -1;
                character.fleeVehicleDeadline = 0;
            }
            if (character.objective == 0x00F2 && character.objectiveTarget == handle) {
                character.objective = 0;
                character.objectiveTarget = -1;
                character.walkingToTarget = false;
                character.objectiveCompleted = true;
            }
            if (behavior.forcedThreatIsVehicle && behavior.forcedThreat == handle) behavior.forcedThreat = -1;
            if (behavior.iceCreamVehicle == handle) {
                behavior.iceCreamVehicle = -1; behavior.iceCreamSlot = -1;
                behavior.iceCreamStartedAt = 0; behavior.attemptedAttractor = true;
                if (character.objective == 0x0359) {
                    character.objective = 0; character.walkingToTarget = false;
                }
            }
            if (character.vehicle == handle && behavior.leavingVehicleToDie) {
                behavior.leavingVehicleToDie = false;
                character.vehicle = -1; character.passengerSeat = -1;
                character.health = 0; character.alive = false; character.walkingToTarget = false;
                character.objective = 0; character.objectiveTarget = -1;
            }
        }
        if (playerAttachedVehicle == handle) {
            playerAttachedVehicle = -1; playerAttachedPitchLimits = {}; manualAim.initialized = false;
        }
    }
    struct AmbientCharacter {
        int32_t slot = -1, model = -1, type = -1;
        std::array<float,3> position{};
        float health = 0;
        bool onFoot = true, visible = true;
    };
    std::function<std::vector<AmbientCharacter>(const std::array<float,6>&)> ambientCharactersInArea;
    std::function<std::vector<AmbientCharacter>(const std::array<float,6>&)> allAmbientCharactersInArea;
    std::function<bool(const std::vector<int32_t>&,bool,std::vector<int32_t>&)> acquireAmbientCharacters;
    std::function<int32_t(const std::array<float,3>&,float)> findVehicleToSteal;
    std::function<bool(int32_t,int32_t&,int32_t&)> randomCharacterModel;
    std::map<int32_t,uint8_t> vcsPedColorSetCursors;
    std::function<bool(Character&,std::string&)> initializeCharacterAppearance;
    std::function<int32_t(int32_t,bool)> characterColorSetCount;
    std::function<bool(int32_t,bool,int32_t,std::array<uint32_t,6>&)> characterColorSet;
    std::function<bool(int32_t)> characterDrowning, characterFighting, characterShooting;
    std::function<bool(int32_t)> characterInControl;
    std::function<bool(int32_t,bool&)> nativeCharacterStopped,nativeVehicleStopped;
    std::function<bool(int32_t,std::string&)> playerEvasiveDive;
    std::function<bool(int32_t,int32_t)> characterTouchingVehicle;
    std::function<bool(int32_t)> explodeCharacterHead;
    std::function<bool(int32_t,int32_t,std::array<float,4>&,std::array<uint8_t,5>&)> nativePedStat;
    std::function<bool(int32_t,std::array<float,3>&,float&)> characterBoundingSphere;
    std::function<bool(int32_t,float&)> characterOriginOffset;
    std::function<void(int32_t,uint16_t)> characterSpeech;
    std::function<bool(int32_t)> setPlayerClipAmmunition;
    std::function<int32_t()> countReturnableWeapons;
    std::function<bool()> returnPlayerWeapons;
    uint32_t playerHavoc = 0;
    int32_t forcedRandomPedType = -1;
    int32_t lastRandomCharacter = -1;
    struct CharacterRoutePoint {
        int16_t route = -1;
        std::array<float,3> position{};
    };
    std::vector<CharacterRoutePoint> characterRoutePoints;
    int32_t nextMissionCharacterHandle=1;
    std::map<int32_t,std::array<int32_t,2>> vehicleModelComponents;
    int32_t playerAttachedVehicle=-1;
    std::array<float,2> playerAttachedPitchLimits{}; // Native CB0/CB4, radians; both zero select defaults.
    bool playerCutsceneSafe = false;
    bool playerCutscenePreviousControl = true, playerCutscenePreviousPoliceIgnore = false;
    uint8_t playerCutscenePreviousDamageProofs = 0;
    void restorePlayerAfterCutscene() {
        if (!playerCutsceneSafe) return;
        control = playerCutscenePreviousControl;
        policeIgnorePlayer = playerCutscenePreviousPoliceIgnore;
        playerDamageProofs = playerCutscenePreviousDamageProofs;
        playerCutsceneSafe = false;
    }
    bool playerAllowFrontPassenger=true;
    int32_t playerPassengerSeat=-1;
    bool playerVehicleTransition=false,playerExitVehicleRequested=false;
    int32_t playerVehicleEntryTarget=-1;
    bool playerVehicleEntryAsPassenger=false;
    uint8_t playerRenderAlpha=255;
    int32_t playerSpeechProfile=0;
    bool playerSpeechEnabled = true;
    bool playerNeverTired = false;
    bool playerDrownsInWater = true;
    bool playerInfiniteSwim = false;
    bool playerFastReload = false;
    float playerSprintEnergy = 150.0f;
    float playerSprintMaximum = 150.0f;
    float playerSprintTraining = 0.0f;
    int32_t playerSavedWeapon = 48;
    std::function<bool(int32_t,std::array<float,3>&)> deadCharacterPickupPosition;
    uint8_t playerHealthLimit = 100;
    uint8_t playerArmourLimit = 100;
    bool playerBleeding=true;
    uint16_t playerGangDamageMask=UINT16_MAX;
    float playerMaximumHealth=100;
    StorylandAttackTimers playerAttackTimers;
    int32_t playerWaitState=0;
    StorylandScriptPlayerWalkTask playerWalkTask;
    bool playerScriptRunning = false;
    StorylandScriptPlayerNavigation playerNavigation;
    StorylandVcsCharacterScriptFlags playerVcsScriptFlags;
    uint8_t playerVcsSeekNodeRadius=5;
    StorylandVcsFootstepMarkState playerVcsFootstepMarks;
    uint32_t playerWaitDeadline=0;
    bool playerWaitVersionB=false;
    StorylandCrouchState playerCrouch;
    bool playerWarped=false;
    std::array<float,3> playerAttachment{};
    bool timeFrozen=false;
    bool clockResetRequested=false;
    uint8_t playerDamageProofs=0;
    bool playerOnlyDamagedByPlayer=false,applyPlayerCarBrakes=false;
    bool playerSuffersCriticalHits=true,playerCanBeShotInVehicle=true;
    bool playerCollisionEnabled=true;
    bool playerDisableVerticalMovement=false;
    bool playerTestShotInVehicle=false;
    bool playerCantBeDraggedOut=false;
    bool playerStayInCarWhenJacked=false;
    bool playerCanBeDraggedFromPassengerSide=false;
    bool playerDontReenterAfterEjection=false,playerAvoidUnsupportedForwardStep=false;
    uint16_t playerPedMoney=0;
    bool playerPedMoneyGivenByScript=false;
    // PlayerInfo+0x170 is a separate reward from CPed's ordinary proof flags.
    // Persisted in SC21 so existing SC20 saves retain their original layout.
    bool playerFireproofReward=false;
    int32_t extraColor=-1;
    bool extraColorFade=false;
    std::array<float,3> extraLightDirection{};
    bool extraLightDirectionSet=false;
    int32_t radioChannel=-1,radioPosition=0;
    bool radioChannelChanged=false;
    std::function<int32_t()> currentRadioChannel; // Live station, independent of pending script requests.
    struct RoadRestriction {std::array<float,6> bounds{};
        template<class Archive> void saveState(Archive& ar) {
            ar(bounds);
        }
};
    std::vector<RoadRestriction> disabledPedRoads;
    std::vector<StorylandRoadChange> roadChanges;
    struct ClearAreaEvent {float x=0,y=0,z=0,radius=0;bool particles=false;
        template<class Archive> void saveState(Archive& ar) {
            ar(x,y,z,radius,particles);
        }
};
    std::vector<ClearAreaEvent> clearAreaEvents;
    std::map<int32_t,int32_t> pedWaitStateMilliseconds;
    struct PedAnimationRequest {
        int32_t animationId=0;
        int32_t durationMilliseconds=0;
        uint64_t serial=0;
        int32_t animationGroup=-1; // Version 3 XTRA subsystem, defaulted for older saves.
        template<class Archive> void saveState(Archive& ar) {
            ar(animationId,durationMilliseconds,serial);
        }
};
    std::map<int32_t,PedAnimationRequest> pedAnimationRequests;
    struct ScriptAnimation {
        int32_t group = 0, animation = -1;
        float time = 0, weight = 1, blendRate = 0;
        uint32_t clock = 0;
        bool loop = false, scriptManaged = false;
        bool paused = false;
        template<class Archive> void saveState(Archive& ar) {
            ar(group, animation, time, weight, blendRate, clock, loop, scriptManaged);
        }
    };
    std::map<int32_t,ScriptAnimation> scriptedCharacterAnimations;
    float characterAnimationTime(int32_t handle, uint32_t startedAt) const {
        const auto found = scriptedCharacterAnimations.find(handle);
        return found == scriptedCharacterAnimations.end() ? float(gameMilliseconds - startedAt) * .001f :
            found->second.time + (found->second.paused ? 0.0f :
                float(gameMilliseconds - found->second.clock) * .001f);
    }
    uint64_t nextPedAnimationSerial=1;
    bool empireHudRequested=false;
    bool empireStatsEnabled=false;
    std::array<uint8_t,4> empireSitesOwned{}; // Vance, Cholo, Sharks, Bikers; native 0x4CD280.
    struct EmpireSiteState {
        // Native VCS constructor 0x2E4B88; mutable fields at offsets 0, 8 and 12.
        int32_t owner=-1,scale=1,condition=-1;
        int32_t type=0,state=-1;
        bool damaged=false;
        template<class Archive> void saveState(Archive& ar) {ar(owner,scale,condition);}
    };
    std::array<EmpireSiteState,30> empireSites{};
    struct AnimatedEmpireEntity {
        int32_t site=-1,model=-1;
        bool construction=false,detailed=false;
        template<class Archive> void saveState(Archive& archive) {
            archive(site,model,construction,detailed);
        }
    };
    std::array<AnimatedEmpireEntity,20> animatedEmpireEntities{};
    int32_t animatedEmpireFreeCursor=-1;
    std::set<int32_t> empireRequestedModels;
    bool emergencyServicesEnabled=true;
    int32_t totalRampages=0;
    int32_t nextGarageHandle=1,nextObjectHandle=1;
    int32_t bridgeGeometry=0,missionsGiven=0,totalMissions=0,progressTotal=0;
    int32_t clockEventHours=0,clockEventMinutes=0,clockWarningMinutes=0;
    bool clockEventFlashing=false,clockEventHidden=false; // Transient CHud presentation state.
    int32_t deathArrestState=0;
    int32_t declaredMissionFlag=-1;
    std::function<bool()> lcsMissionFlagSet;
    std::map<int32_t,std::string> characterSkins;
    std::set<int32_t> undressedCharacters; // Transient; checkpoints reject a pending skin replacement.
    std::function<bool(int32_t,const std::string&,bool,std::string&)> replaceCharacterSkin;
    std::map<int32_t,uint32_t> modelsKilledByPlayer;
    bool playerArrested=false;
    bool rubbishEnabled=true;
    bool worldProcessingEnabled=true;
    bool streamingEnabled=true;
    bool everyoneIgnoresPlayer=false;
    bool textCommandsEnabled=true;
    // Native USE_TEXT_COMMANDS: retain, clear once, or clear before every frame.
    uint8_t lcsTextCommandMode=0;
    struct LcsScriptText {
        float x=0,y=0,scaleX=0.48f,scaleY=1.12f,wrapX=182.0f;
        std::array<uint8_t,4> color{225,225,225,255};
        bool justify=false,rightJustify=false,centered=false,background=false,proportional=true;
        std::string key,missionText;
        std::vector<int32_t> numbers;
        template<class Archive> void saveState(Archive& ar) {
            ar(x,y,scaleX,scaleY,wrapX,color,justify,rightJustify,centered,background,proportional,
                key,missionText,numbers);
        }
    };
    std::vector<LcsScriptText> lcsScriptTexts;
    LcsScriptText lcsNextScriptText;
    int32_t cameraSpecialMode=2;
    bool cameraScriptModeEnabled=false;
    std::set<int32_t> clearedPedThreats;
    std::map<int32_t,uint32_t> pedTypeThreats;
    std::map<int32_t,uint16_t> characterFlags;
    // Native scripted look transitions are transient. Active transitions block SC55 saves.
    std::map<int32_t,StorylandScriptedPedLook> scriptedPedLooks;
    bool resolvingScriptedLookTarget=false;
    bool hasScriptedLookTransition() const {
        return std::any_of(scriptedPedLooks.begin(),scriptedPedLooks.end(),
            [](const auto& look){return look.second.active||look.second.restoring;});
    }
    struct Gang {
        std::array<int32_t,2> pedModels{},weapons{};
        int32_t carModel=0,accuracy=0,health=100,armour=0;
        float attackMultiplier=1;
        bool attackWithPolice=false;
        std::array<uint8_t,4> empireColor{255,255,255,255};
        uint8_t empireColorIndex=0;
        template<class Archive> void saveState(Archive& ar) {
            ar(pedModels,weapons,carModel,accuracy,health,armour,attackMultiplier,attackWithPolice,
                empireColor,empireColorIndex);
        }
};
    std::array<Gang,9> gangs{};
    std::vector<std::array<float,6>> interlevelRoads;
    struct SetPiece {int32_t type=0;std::array<float,12> coordinates{};
        template<class Archive> void saveState(Archive& ar) {
            ar(type,coordinates);
        }
};
    std::vector<SetPiece> setPieces;
    struct BuildingSwapCommand {uint32_t key=0,state=0;bool group=false;
        template<class Archive> void saveState(Archive& ar) {
            ar(key,state,group);
        }
};
    std::vector<BuildingSwapCommand> buildingSwapCommands;
    struct BuildingModelSwap {
        uint32_t originalModel=0,replacementModel=0;
        std::array<float,3> position{};
        template<class Archive> void saveState(Archive& ar) {ar(originalModel,replacementModel,position);}
    };
    std::map<uint64_t,BuildingModelSwap> buildingModelSwaps;
    bool buildingModelSwapsChanged=false;
    std::function<bool(const std::array<float,3>&,float,int32_t,int32_t,std::string&)> swapNearestBuilding;
    std::map<uint32_t,int32_t> buildingSwapStates;
    std::map<uint32_t,int32_t> lcsBuildingSwapGroups;
    std::function<bool()> worldStreamingComplete;
    uint64_t buildingSwapRevision=0;
    std::array<uint8_t,4> counterColor{237,130,180,255};
    std::array<uint8_t,4> counterBackground{110,16,60,255};
    // COnscreenTimer contains one clock and three counter rows in retail VCS.
    // Slots refer to the shared SCM globals, not a copied value or a VM local.
    struct OnscreenTimer {
        int32_t globalSlot=-1;
        std::string label;
        bool countDown=true;
        uint8_t warningFrames=0;
        std::array<uint8_t,4> color{244,225,91,255};
        template<class Archive> void saveState(Archive& ar) {
            ar(globalSlot,label,countDown,warningFrames,color);
        }
    } onscreenTimer;
    struct OnscreenCounter {
        int32_t globalSlot=-1,maximum=-1;
        uint16_t type=0,secondaryType=0;
        std::string label,secondaryLabel;
        bool money=false;
        std::array<uint8_t,4> color{0,0,0,157},background{0,0,0,81};
        template<class Archive> void saveState(Archive& ar) {
            ar(globalSlot,maximum,type,secondaryType,label,secondaryLabel,money,color,background);
        }
    };
    std::array<OnscreenCounter,3> onscreenCounters{};
    std::function<int32_t(int32_t)> scriptGlobalValue; // Live HUD lookup; never serialized.
    bool onscreenTimerFrozen=false;
    struct PoliceRestart {float x=0,y=0,z=0,heading=0;
        template<class Archive> void saveState(Archive& ar) {
            ar(x,y,z,heading);
        }
};
    std::vector<PoliceRestart> policeRestarts;
    std::vector<PoliceRestart> hospitalRestarts;
    std::vector<PoliceRestart> taxiRestarts;
    bool restartOverrideEnabled=false; // LC18 extension; preserve the original WRLD record.
    PoliceRestart restartOverride;
    PoliceRestart nextRestartOverride;
    bool nextRestartOverrideActive = false;
    bool criticalMissionRestartRequested = false;
    int32_t introMissionSlot = -1;
    bool introMissionProtected = false, introMissionRestartQueued = false;
    std::array<float,3> introRestartPosition{};
    float introRestartHeading = 0;
    bool introRestartPositionValid = false, introRestartMartinezCaptured = false;
    bool getOutOfJailFree = false;
    bool policeIgnorePlayer=false;
    bool playerPositionFrozen=false;
    bool cameraBehindPlayerRequested=false;
    float cameraNearClip=0.05f;
    std::string storedPlayerOutfit;
    std::string playerOutfit="PLR";
    bool unverifiedMissionCondition=false;
    std::map<int32_t,int32_t> empireScriptValues;
    int32_t lastEmpireQuery=0;
    std::string helpMessage;
    bool helpMessagePersistent=false;
    uint32_t helpMessageMilliseconds=0;
    int32_t progressMade=0,oddjobsPassed=0;
    std::set<std::string> passedMissions;
    bool coopFriendlyFire = false;
    uint32_t coopGuestStage = 0;
    uint8_t failCurrentMission=0;
    std::function<bool(int32_t,std::string&)> playLcsMissionPassedTune;
    bool radioEnabled=true,radioChanged=false;
    int32_t forcedWeather=-1;
    bool weatherChanged=false;
    struct WeatherState {
        int16_t previous = 0;
        int16_t current = 0;
        float interpolation = 0;
        float rain = 0;
        template<class Archive> void saveState(Archive& archive) {
            archive(previous, current, interpolation, rain);
        }
    };
    WeatherState weatherState;
    WeatherState storedWeatherState;
    bool weatherRestoreRequested = false;
    std::string areaNameKey;
    bool areaNameForced = false;
    bool playerVisible=true;
    bool widescreen=false;
    bool allowPauseInWidescreen=true;
    bool loadAllModelsRequested=false;
    float threatReactionRangeMultiplier=1.0f;
    std::array<uint8_t,3> fadingColor{0,0,0};
    int32_t currentWeapon=0;
    bool clearPlayerWeaponsRequested=false;
    uint32_t gameMilliseconds=0;
    struct Weapon {
        int32_t slot=-1,model=-1,ammunition=0;
        bool owned=false;
        template<class Archive> void saveState(Archive& ar) {
            ar(slot,model,ammunition,owned);
        }
};
    std::map<int32_t,Weapon> playerWeapons;
    std::map<int32_t,std::map<int32_t,Weapon>> characterWeapons;
    std::map<int32_t,int32_t> characterCurrentWeapons;
    bool weaponInventoryChanged=false;
    bool weaponSelectionRequested=false;
    int32_t occupiedVehicleModel=-1,occupiedMissionVehicle=-1;
    int32_t currentPedStat=0;
    float wantedMultiplier=1.0f;
    int32_t totalBalloons=0;
    int32_t totalExportedCars=0;
    uint32_t exportedCars=0;
    int32_t totalUniqueJumps=0;
    uint32_t uniqueJumpsFound=0;
    uint32_t taxiPassengersDroppedOff=0;
    std::array<bool,5> vehicleCheats{};
    int32_t unknownCommand8850=0;
    std::set<int32_t> requestedModels;
    std::set<int32_t> residentModels;
    std::set<int32_t> requestedAnimationGroups;
    // Named animation blocks and special IDE aliases are native streaming
    // requests. Resident bytes remain owned by the live asset decoders.
    std::set<std::string> requestedAnimationBanks;
    std::map<int32_t,std::string> specialModelNames;
    std::map<int32_t,uint8_t> specialModelOwners; // Mission bit2, MAIN bit128.
    struct ZonePedInformation {
        int32_t dayNight=0;
        std::array<int32_t,11> values{};
        template<class Archive> void saveState(Archive& ar) {
            ar(dayNight,values);
        }
};
    std::map<std::pair<std::string,int32_t>,ZonePedInformation> zonePedInformation;
    std::map<std::pair<std::string,int32_t>,ZonePedInformation> zoneCarInformation;
    std::map<std::pair<std::string,int32_t>,uint16_t> zoneCarDensities,zonePedDensities;
    std::map<std::pair<std::string,int32_t>,std::array<int32_t,13>> zoneCivilianCarInformation;
    std::map<std::pair<std::string,int32_t>,int32_t> zonePedGroups;
    std::string missionText;
    std::string splashScreen;
    bool fading=false;
    float fadeDuration=0,fadeTotal=0,fadeAlpha=0,fadeStartAlpha=0,fadeTargetAlpha=0;
    int32_t fadeDirection=0;
    struct Cutscene {
        struct Actor {
            int32_t handle=0;
            int32_t model=0;
            std::string animation;
        
        template<class Archive> void saveState(Archive& ar) {
            ar(handle,model,animation);
        }
};
        std::string name,subtitleKey,subtitleText;
        bool loaded=false,playing=false,finished=false,loop=false,offsetSet=false;
        uint32_t elapsedMilliseconds=0;
        float offsetX=0,offsetY=0,offsetZ=0;
        uint32_t loadRequest=0,startRequest=0,clearRequest=0;
        int32_t nextActorHandle=1;
        std::vector<Actor> actors;
        template<class Archive> void saveState(Archive& ar) {
            ar(name,subtitleKey,subtitleText,loaded,playing,finished,loop,offsetSet,elapsedMilliseconds,
                offsetX,offsetY,offsetZ,loadRequest,startRequest,clearRequest,nextActorHandle,actors);
        }
} cutscene;
    bool cutsceneSkipped=false;
    struct Pickup {
        int32_t handle=0,model=0,type=0;
        float x=0,y=0,z=0;
        bool collected=false;
        int32_t ammunition=0;
        float revenue=0;
        int32_t regenerationModel=-1;
        bool removed=false;
        bool unavailable=false;
        uint32_t respawnAt=0,expiresAt=0;
        std::string modelNameOverride;
        int area=0;
        template<class Archive> void saveState(Archive& ar) {
            ar(handle,model,type,x,y,z,collected,ammunition,removed,unavailable,respawnAt,expiresAt,area,modelNameOverride);
        }
};
    std::function<bool(const std::array<float,3>&,float,int32_t&)> pickupWeaponInSphere;
    std::function<bool(const std::array<float,3>&,float)> anyPickupInSphere;
    struct CarGenerator {
        int32_t handle=0,model=0,color1=-1,color2=-1;
        float x=0,y=0,z=0,heading=0;
        int32_t forceSpawn=0,alarm=0,doorLock=0,minimumDelay=0,maximumDelay=0;
        int32_t generations=0;
        int32_t spawnedSlot=-1;
        template<class Archive> void saveState(Archive& ar) {
            ar(handle,model,color1,color2,x,y,z,heading,forceSpawn,alarm,doorLock,minimumDelay,maximumDelay,
                generations,spawnedSlot);
        }
};
    std::vector<CarGenerator> carGenerators;
    int32_t nextCarGeneratorHandle=1;
    float x=0,y=0,z=0,heading=0,health=100,armour=0,hour=12;
    struct Marker {
        int32_t handle=0;float x=0,y=0,z=0,radius=1;
        uint8_t red=255,green=210,blue=30;
        uint32_t milliseconds=0;int area=0;bool persistent=false;
        uint8_t type=6;
        uint32_t createdAt=0;
        template<class Archive> void saveState(Archive& ar) {
            ar(handle,x,y,z,radius,red,green,blue,milliseconds,area,persistent);
        }
};
    std::vector<Marker> markers;
    std::array<uint16_t,16> markerGenerations{};
    struct LocateRegion {
        std::array<float,3> center{},extent{};
        uint32_t expiresAt=0;
        bool threeDimensional=false;
    };
    std::vector<LocateRegion> locateRegions;
    int32_t nextMarkerHandle=1;
    int area=0;
    uint32_t currentWorldLevel=0;
    enum class BlipEntity { Coordinate, Vehicle, Character, Object, Pickup, ContactPoint };
    struct RadarBlip {
        int32_t handle=0,entityHandle=0,sprite=0,display=3,scale=3;
        uint32_t color=5;
        float x=0,y=0,z=0;
        bool shortRange=false,bright=true;
        BlipEntity entity=BlipEntity::Coordinate;
        bool exterior=false; // Native bit0x10; archived separately from the delivered blip layout.
        template<class Archive> void saveState(Archive& ar) {
            ar(handle,entityHandle,sprite,display,scale,color,x,y,z,shortRange,bright,entity);
        }
};
    std::vector<RadarBlip> radarBlips;
    std::array<uint16_t,75> radarBlipGenerations{};
    int32_t nextBlipHandle=0x10000;
    bool radarVisible=true,radarPositionFrozen=false;
    std::array<float,3> radarPosition{};
    int32_t radarDisplayMode=0;
    float radarRange=0;
    int16_t lcsHudFlashItem=-1;
    StorylandLcsHudFlashState lcsHudFlash;
    StorylandLcsRadarBlinkState lcsRadarBlink;
    bool lcsNavigationArrowVisible=false;
    std::array<float,3> lcsNavigationArrowTarget{};
    std::array<float, 3> radarMapAnchor{};
    std::array<float, 3> radarLocalOrigin{};
    std::array<std::vector<std::array<float,2>>,3> toggleableRadarBlips;
    struct ScriptCamera {
        bool active=false,positionSet=false,targetSet=false,restoreRequested=false;
        float x=0,y=0,z=0,upX=0,upY=0,upZ=0;
        float targetX=0,targetY=0,targetZ=0;
        int32_t mode=0;
        int32_t pedZoomIndicator=2;
        bool pedZoomSwitchFrozen=false;
        int32_t lcsTrackedKind=0,lcsTrackedHandle=-1; // 1: player, 2: character, 3: vehicle.
        bool lcsFrontRequested=false;
        int32_t interpolationMilliseconds=0;
        float interpolationWeight=0.5f;
        bool interpolationScripted=false,transitionPending=false;
        float previousX=0,previousY=0,previousZ=0;
        float previousTargetX=0,previousTargetY=0,previousTargetZ=0;
        uint32_t transitionStartedAt=0;
        bool transitioning=false,cutsceneOwned=false;
        float fieldOfView=70,roll=0;
        float vcsFieldOfViewOverride=0; // Transient active CCam +0x80; zero releases the override.
        float vehicleBetaSpeed=0;
        uint32_t vehicleUpdatedAt=0;
        bool vehicleAligning=false,vehicleUpdated=false;
        template<class Archive> void saveState(Archive& ar) {
            ar(active,positionSet,targetSet,restoreRequested,x,y,z,upX,upY,upZ,targetX,targetY,targetZ,mode,
                interpolationMilliseconds,interpolationWeight,previousX,previousY,previousZ,previousTargetX,
                previousTargetY,previousTargetZ,transitionStartedAt,transitioning,cutsceneOwned,fieldOfView,
                roll);
        }
} camera;
    bool developerMode=false;
    bool pauseMenuDisabled=false;
    bool planesAllowedToMove=true;
    bool carGeneratorsDisabled=false; // Retained legacy save field; not the VCS04C6 policy.
    bool vcsRandomBoatsEnabled=true; // Native48A9C0; serialized only in its appended extension.
    StorylandVcsViceDispatchState vcsViceDispatch;
    int32_t scriptAudioMode=2;
    float carDensityMultiplier=1,pedDensityMultiplier=1;
    float speed=0;
    int32_t money=0,wanted=0,maxWanted=6;
    bool wantedResetRequested=false; // Transient explicit script command, including a same-level reset.
    std::function<void(int32_t)> applyWantedLevel;
    void requestWantedReset() {
        wantedResetRequested=true;
        if(applyWantedLevel) {applyWantedLevel(wanted);wantedResetRequested=false;}
    }
    bool control=true,inVehicle=false;
    bool playerCanDoDriveBy=true,playerInStadium=false;
    bool playerSeatedDriveByEnabled=false;
    uint8_t vcsStopSniperBulletsOnBuildings=0;
    std::function<bool(int32_t)> vehicleIsTaxi;
    PoliceRestart restartMissionTaxiDestination;
    bool restartMissionTaxiDestinationSet=false;
    std::vector<int32_t> messageNumbers;
    std::string messageKey;
    int32_t messageStyle=0; // Zero is a subtitle; positive values are native big-message styles.
    struct LcsMessage {
        std::string key,missionText,stringKey;
        std::vector<int32_t> numbers;
        uint32_t durationMilliseconds=0,startedAt=0;
        uint16_t flags=0;
        bool overrideFade=false,addToBrief=true;
        template<class Archive> void saveState(Archive& ar) {
            ar(key,missionText,stringKey,numbers,durationMilliseconds,startedAt,flags,overrideFade,addToBrief);
        }
    };
    bool lcsMessageQueuesUsed=false;
    std::vector<LcsMessage> lcsBriefMessages;
    std::array<std::vector<LcsMessage>,8> lcsBigMessages;
    std::vector<LcsMessage> lcsPreviousBriefs;
    uint32_t messageMilliseconds=0;
    std::vector<Pickup> pickups;
    StorylandLcsEntityExtensionState lcsEntityExtension;
    StorylandLcsExtendedWorld lcsExtendedWorld;
    std::map<int32_t,StorylandLcsPickupDetails> lcsPickupDetails;
    int32_t nextPickupHandle=1;

    // These restored fields are serialized by the versioned save extension;
    // the original environment archive below remains byte compatible.
    StorylandMissionResult missionResult;
    std::string lastMissionPassedName;
    struct Message {
        std::string key,stringKey;
        std::vector<int32_t> numbers;
        uint32_t duration=0,remaining=0;
        int32_t style=0,flags=0;
        bool addToBrief=true;
        template<class Archive> void saveState(Archive& archive) {
            archive(key,stringKey,numbers,duration,remaining,style,flags,addToBrief);
        }
    };
    std::deque<Message> subtitleQueue;
    std::array<std::deque<Message>,8> bigMessageQueues;
    std::deque<Message> briefMessages;
    std::string messageStringKey;
    std::vector<int32_t> helpMessageNumbers;
    int32_t missionPassedTune=-1;
    uint32_t missionPassedTuneSerial=0;
    bool missionTuneLoop=false,musicDoesFade=true;
    std::function<bool(int32_t,bool,std::string&)> playMissionPassedTune;
    std::function<void()> stopMissionPassedTune;
    std::function<bool(int32_t)> musicStreamPlaying;
    std::function<uint64_t()> musicStreamPositionMilliseconds;
    bool missionAudioSubtitleEnabled=true;
    bool frontendAudioEnabled=true;

    struct CameraShake {
        float strength=0;
        uint32_t startedAt=0;
        int32_t randomState=180371;
        uint32_t sampledAt=0;
        bool sampled=false;
        std::array<float,3> offset{};
        template<class Archive> void saveState(Archive& archive) {
            archive(strength,startedAt,randomState,sampledAt,sampled,offset);
        }
    } cameraShake;
    struct ManualAim {
        bool forced=false,initialized=false;
        bool resetRequested=false;
        bool seatedActive=false;
        int32_t mode=0;
        float yaw=0,pitch=0,fieldOfView=70;
        std::array<float,3> eye{},target{};
        template<class Archive> void saveState(Archive& archive) {
            archive(forced,initialized,mode,yaw,pitch,fieldOfView,eye,target);
        }
    } manualAim;
    struct CameraFocus {
        uint8_t kind=0; // 0: fixed point, 1: player, 2: character, 3: vehicle.
        int32_t handle=0,mode=15,switchStyle=2;
        template<class Archive> void saveState(Archive& archive) {archive(kind,handle,mode,switchStyle);}
    } cameraFocus;
    CameraFocus cameraLookAt; // Independent look-at reference; vehicle takes priority.
    struct ScriptShadow {
        std::array<float,3> position{};
        std::array<uint8_t,3> color{};
        int16_t intensity = 0;
        float radius = 0;
        bool pedestrian = false;
    };
    std::map<std::pair<uintptr_t,int32_t>,ScriptShadow> frameScriptShadows;
    int32_t cameraVehicleZoom=2;
    struct ScriptCameraZoom {
        bool ped = false;
        bool vehicle = false;
        float pedTarget = 0;
        float pedCurrent = 0;
        float vehicleTarget = 0;
        float vehicleCurrent = 0;
        template<class Archive> void saveState(Archive& archive) {
            archive(ped, vehicle, pedTarget, pedCurrent, vehicleTarget, vehicleCurrent);
        }
    } cameraZoom;
    std::function<int32_t()> activeVehicleCameraClass;
    bool cameraControlEnabled=true;
    bool cameraFrontPlayerRequested=false;
    bool wantsHud=true;
    int16_t flashingHudObject=-1;
    std::array<int32_t,2> hudFlashFrames{50,50};
    std::array<uint32_t,2> hudFlashLastFrame{UINT32_MAX,UINT32_MAX}; // Draw deduplication, transient.
    StorylandVcsHudMeters vcsHudMeters;

    struct ScriptPanelItem {
        int32_t handle=0,x=0,y=0,alignment=0,font=1;
        float scale=1;
        std::string key;
        std::array<uint8_t,4> color{255,255,255,255};
        int8_t highlight=0;
        std::array<uint8_t,4> highlightColor{255,0,0,64};
        std::vector<int32_t> numbers;
        bool sprite=false;
        int16_t width=-1,height=-1;
        template<class Archive> void saveState(Archive& archive) {
            archive(handle,x,y,alignment,font,scale,key,color,highlight,highlightColor,numbers,sprite,width,height);
        }
    };
    struct ScriptPanel {
        bool visible=false;
        int32_t x=0,y=0,width=0,height=0;
        std::array<uint8_t,4> borderColor{127,127,127,127};
        std::array<uint8_t,4> backgroundColor{0,0,0,255};
        std::vector<ScriptPanelItem> items;
        template<class Archive> void saveState(Archive& archive) {
            archive(visible,x,y,width,height,borderColor,backgroundColor,items);
        }
    };
    std::array<ScriptPanel,10> scriptPanels;
    int32_t nextScriptPanelItem=0;

    struct ScriptFrameText {
        float scaleX=.48f,scaleY=1.12f,wrap=478;
        int32_t x=0,y=0;
        std::array<uint8_t,4> color{225,225,225,255};
        uint8_t flags=0x10;
        uint8_t font=1;
        std::u16string text;
    };
    // Native CTheScripts owns twenty rows, reset once before all script threads.
    // Rendering data is deliberately transient and is never archived.
    std::vector<ScriptFrameText> scriptFrameText;
    ScriptFrameText scriptFrameTextStyle;

        template<class Archive> void saveState(Archive& ar) {
            ar(audioSlots,saveMenuRequested,saveGameFinished,audioSubsystemEnabled,playerSpeechMood,
                playerSpeechMoodMilliseconds,nextAudioSerial,garages,objects,missionVehicles,
                nextMissionVehicleHandle,missionCharacters,nextMissionCharacterHandle,vehicleModelComponents,
                playerAttachedVehicle,playerWarped,playerAttachment,timeFrozen,extraColor,extraColorFade,
                extraLightDirection,extraLightDirectionSet,radioChannel,radioPosition,radioChannelChanged,
                disabledPedRoads,roadChanges,clearAreaEvents,pedWaitStateMilliseconds,pedAnimationRequests,
                nextPedAnimationSerial,empireHudRequested,emergencyServicesEnabled,totalRampages,
                nextGarageHandle,nextObjectHandle,bridgeGeometry,missionsGiven,totalMissions,progressTotal,
                clockEventHours,clockEventMinutes,clockWarningMinutes,deathArrestState,declaredMissionFlag,
                characterSkins,modelsKilledByPlayer,playerArrested,rubbishEnabled,worldProcessingEnabled,
                streamingEnabled,everyoneIgnoresPlayer,textCommandsEnabled,cameraSpecialMode,
                cameraScriptModeEnabled,clearedPedThreats,pedTypeThreats,characterFlags,gangs,interlevelRoads,
                setPieces,buildingSwapCommands,buildingSwapStates,buildingSwapRevision,counterColor,
                counterBackground,policeRestarts,hospitalRestarts,taxiRestarts,policeIgnorePlayer,
                playerPositionFrozen,cameraBehindPlayerRequested,cameraNearClip,storedPlayerOutfit,
                playerOutfit,unverifiedMissionCondition,empireScriptValues,lastEmpireQuery,helpMessage,
                helpMessagePersistent,helpMessageMilliseconds,progressMade,oddjobsPassed,passedMissions,
                radioEnabled,radioChanged,forcedWeather,weatherChanged,playerVisible,widescreen,
                allowPauseInWidescreen,loadAllModelsRequested,threatReactionRangeMultiplier,fadingColor,
                currentWeapon,clearPlayerWeaponsRequested,gameMilliseconds,playerWeapons,
                weaponInventoryChanged,weaponSelectionRequested,occupiedVehicleModel,occupiedMissionVehicle,
                currentPedStat,wantedMultiplier,totalBalloons,totalExportedCars,exportedCars,totalUniqueJumps,
                uniqueJumpsFound,taxiPassengersDroppedOff,vehicleCheats,unknownCommand8850,requestedModels,
                residentModels,requestedAnimationGroups,zonePedInformation,zoneCarInformation,
                zoneCivilianCarInformation,zonePedGroups,missionText,splashScreen,fading,fadeDuration,
                fadeTotal,fadeAlpha,fadeStartAlpha,fadeTargetAlpha,fadeDirection,cutscene,carGenerators,
                nextCarGeneratorHandle,x,y,z,heading,health,armour,hour,markers,nextMarkerHandle,area,
                radarBlips,nextBlipHandle,radarVisible,radarPositionFrozen,radarPosition,radarDisplayMode,
                radarRange,camera,developerMode,pauseMenuDisabled,planesAllowedToMove,carGeneratorsDisabled,
                scriptAudioMode,carDensityMultiplier,pedDensityMultiplier,speed,money,wanted,maxWanted,
                control,inVehicle,messageKey,messageMilliseconds,pickups,nextPickupHandle);
        }
    int32_t vcsArrayScratch = 0;
    bool vcsCashHudRaised = false;
    bool vcsAmmoHudVisible = true;
    uint8_t vcsHudHiddenComponents = 0;
    float vcsScript0556Value = 1.0f;
    float vcsRemoteCameraUpsideDownOffset = 0.0f;
    StorylandVcsVehicleCaption vcsVehicleCaption;
    std::function<std::string(int32_t)> nativeVehicleNameKey;
    std::set<int32_t> vcsPedFlag28; // Legacy SC30 data.
    std::set<int32_t> vcsVehicleFlag28;
    std::set<int32_t> vcsGarageStorableMissionVehicles;
    std::set<int32_t> vcsPedFlag14;
    std::set<int32_t> vcsPedFlag1D8Bit6;
    StorylandVcsSwimmingBoundary vcsSwimmingBoundary;
    struct VcsAreaCaption {
        enum class Phase : uint8_t { Hidden, Hold, FadeIn, FadeOut, Replace };
        bool visible = true;
        std::string previousKey, displayedKey;
        uint32_t displayMilliseconds = 0;
        int32_t fadeMilliseconds = 0;
        Phase phase = Phase::Hidden;
        uint8_t alpha = 0;
    } vcsAreaCaption;
    std::function<bool(int32_t,std::string&)> constructMissionVehicle;
};
std::string storylandMissionAreaNameKey(const StorylandMissionEnvironment& environment);
void storylandAdvanceVcsAreaCaption(StorylandMissionEnvironment& environment);
bool storylandMissionPagerScrollSuppressed(const StorylandMissionEnvironment& environment,
    bool displaySuppressed);
std::u16string storylandUpdateMissionPager(StorylandMissionEnvironment& environment,
    bool displaySuppressed);
void storylandAdvanceMissionPresentation(StorylandMissionEnvironment& environment,uint32_t elapsedMilliseconds,
    bool vcs=true);
void storylandResetMissionPresentation(StorylandMissionEnvironment& environment);
void storylandDeleteMissionCharacter(StorylandMissionEnvironment& world,int32_t handle);
void storylandCleanupVcsMissionActors(StorylandMissionEnvironment& world);
std::u16string storylandMissionFormattedText(const StorylandMissionEnvironment& environment,
    const std::string& key,const std::vector<int32_t>& numbers={},const std::string& stringKey={});
struct StorylandMissionVm {
    struct ExecutedInstruction {
        size_t offset=0;
        uint16_t opcode=0;
    };
    struct ScriptLaunch {
        bool internalMission=false,missionThread=false;
        int32_t target=0;
        std::vector<int32_t> arguments;
        std::map<size_t,std::string> strings;
        template<class A> void saveState(A& a) {a(internalMission,missionThread,target,arguments,strings);}
    };
    std::deque<ScriptLaunch> pendingLaunches;
    bool missionCleanupRequested=false;
    StorylandMissionEnvironment* environment=nullptr;
    StorylandMissionState state = StorylandMissionState::Idle;
    std::vector<uint8_t> script;
    std::shared_ptr<const std::vector<uint8_t>> sharedRetailScript;
    const std::vector<uint8_t>& scriptBytes() const {return sharedRetailScript?*sharedRetailScript:script;}
    size_t pc = 0;
    uint32_t fileOffset = 0;
    uint32_t delayMilliseconds = 0;
    uint32_t vcsWakeTime = 0;
    bool vcsAbsoluteWait = false;
    bool messageWaitSkippable=false; // Version 12 extension, not the legacy SCR layout.
    size_t instructions = 0;
    std::deque<ExecutedInstruction> executionTrace;
    std::string diagnostic;
    std::string scriptName;
    bool vcs = false;
    bool isMissionThread=true;
    bool isInternalMission=true;
    bool deathArrestEnabled=true,deathArrestExecuted=false;
    StorylandMissionOutcome missionOutcome=StorylandMissionOutcome::None;
    StorylandMissionFailure missionFailure=StorylandMissionFailure::None;
    std::string missionResultKey;
    int32_t missionSlot=-1;
    uint64_t missionResultSequence=0;
    uint32_t randomState=0x51A3D279u;
    bool condition = false;
    bool conditionValid = false;
    bool conditionOr = false;
    uint32_t conditionCount = 0;
    std::vector<int32_t> globals;
    std::map<size_t,std::string> globalStrings,localStrings;
    std::array<int32_t,128> locals{};
    std::vector<size_t> returnStack;
    struct FunctionFrame {
        size_t returnPc=0;
        std::array<int32_t,128> previousLocals{};
        std::map<size_t,std::string> previousStrings;
        std::vector<std::pair<uint16_t,bool>> outputs;
        uint8_t localBase=0;
        uint8_t inputCount=0;
        uint8_t outputCount=0;
        size_t returnDepth=0;
        bool invertReturn=false;
        template<class A> void saveState(A& a) {a(returnPc,previousLocals,previousStrings,outputs,localBase,inputCount,outputCount,returnDepth,invertReturn);}
    };
    std::vector<FunctionFrame> functionFrames;
    size_t missionBase=0;
    bool wholeScm=false;
    std::map<uint16_t,std::pair<int,std::string>> opcodeDefinitions;
    std::set<uint16_t> provisionalOpcodes;

    struct OutcomeState {
        StorylandMissionOutcome outcome=StorylandMissionOutcome::None;
        StorylandMissionFailure failure=StorylandMissionFailure::None;
        std::string key;
        int32_t missionSlot=-1;
        uint64_t sequence=0;
        bool internalMission=true;

        template<class Archive> void saveState(Archive& archive) {
            archive(outcome,failure,key,missionSlot,sequence,internalMission);
        }
    };
    OutcomeState outcomeState() const {
        return {missionOutcome,missionFailure,missionResultKey,missionSlot,
            missionResultSequence,isInternalMission};
    }
    bool restoreOutcomeState(const OutcomeState& saved) {
        if(unsigned(saved.outcome)>unsigned(StorylandMissionOutcome::Failed)||
            unsigned(saved.failure)>unsigned(StorylandMissionFailure::Arrest)||
            saved.missionSlot<-1||saved.key.size()>256||
            (saved.outcome==StorylandMissionOutcome::None&&
                (saved.failure!=StorylandMissionFailure::None||saved.sequence!=0||!saved.key.empty()))||
            (saved.outcome!=StorylandMissionOutcome::None&&saved.sequence==0)||
            ((saved.outcome==StorylandMissionOutcome::Failed)!=(saved.failure!=StorylandMissionFailure::None)))return false;
        missionOutcome=saved.outcome;
        missionFailure=saved.failure;
        missionResultKey=saved.key;
        missionSlot=saved.missionSlot;
        missionResultSequence=saved.sequence;
        isInternalMission=saved.internalMission;
        return true;
    }

    template<class A> void saveState(A& a) {
        a(pendingLaunches,missionCleanupRequested,state,pc,fileOffset,delayMilliseconds,instructions,
          diagnostic,scriptName,vcs,isMissionThread,randomState,condition,conditionValid,conditionOr,
          conditionCount,globals,globalStrings,localStrings,locals,returnStack,functionFrames,missionBase,wholeScm);
    }
    bool validSavedExecution(size_t length) const {
        if(unsigned(state)>unsigned(StorylandMissionState::Fault)||pc>length||missionBase>length||
            returnStack.size()>1024||functionFrames.size()>128||pendingLaunches.size()>96)return false;
        for(auto address:returnStack)if(address>=length)return false;
        for(const auto& entry:localStrings)if(entry.first>=locals.size())return false;
        for(const auto& entry:globalStrings)if(entry.first>=globals.size())return false;
        for(const auto& frame:functionFrames) {
            if(frame.returnPc>=length||frame.returnDepth>returnStack.size()||frame.outputs.size()!=frame.outputCount||
                size_t(frame.localBase)+frame.inputCount>locals.size())return false;
            for(const auto& output:frame.outputs)if(output.first>=(output.second?globals.size():locals.size()))return false;
        }
        for(const auto& launch:pendingLaunches)if(launch.arguments.size()>locals.size())return false;
        return true;
    }
    bool loadOpcodeDefinitions(const std::wstring& path);
    void start(std::vector<uint8_t> mission, uint32_t missionFileOffset, bool vcsGame = false,
        bool resetMissionMarkers = true);
    void startRetail(const std::vector<uint8_t>& fullScm, uint32_t missionOffset, bool vcsGame = false);
    void startRetailShared(std::shared_ptr<const std::vector<uint8_t>> fullScm,uint32_t missionOffset,
        bool vcsGame=false,bool resetMissionMarkers=true);
    void stop();
    void cleanupLcsMission();
    void tick(uint32_t elapsedMilliseconds,bool advanceEnvironment=true);
    void registerMissionPassed(const std::string& key,bool oddjob=false);
    void recordMissionFailure(StorylandMissionFailure failure,const std::string& reason);
    void observeMissionMessage(const std::string& key,int32_t style);
    bool handleDeathArrest(StorylandMissionFailure failure);
    void recordMissionOutcome(StorylandMissionOutcome outcome,StorylandMissionFailure failure,
        const std::string& key,const std::string& reason);
};

bool storylandChooseLcsRestart(StorylandMissionEnvironment& world,
    const std::array<float,3>& position,bool arrested,
    StorylandMissionEnvironment::PoliceRestart& result);

enum class StorylandMissionObjectiveState { Idle, GoToCheckpoint, ReturnToStart, Complete };

struct StorylandMissionObjective {
    StorylandMissionObjectiveState state = StorylandMissionObjectiveState::Idle;
    int missionId = -1;
    float startX = 0, startY = 0, startZ = 0;
    float destinationX = 0, destinationY = 0, destinationZ = 0;
    float radius = 5;
    int reward = 100;

    void start(int id, float playerX, float playerY, float playerZ,
        float checkpointX, float checkpointY, float checkpointZ);
    bool update(float x, float y, float z);
    void stop();
};
