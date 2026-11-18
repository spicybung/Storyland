#pragma once
#include "src/storyland/storyland_play_test_camera.h"
#include "src/storyland/storyland_clock.h"
#include "src/re/reLCS2/hud/State.h"
#include <array>
#include <functional>
#include <memory>
#include <set>
#include "src/storyland/storyland_weapon_types.h"
#include "src/storyland/storyland_blood_state.h"
#include "src/storyland/storyland_vehicle_drive.h"
#include "src/storyland/storyland_vehicle_contact.h"
#include "src/storyland/storyland_vehicle_ped_impact.h"
#include "src/re/reVCS/vehicles/storyland_vcs_traffic_support.h"
#include "src/storyland/storyland_bike_fall.h"
#include "src/storyland/storyland_vehicle_effects.h"
#include "src/storyland/storyland_traffic_lights.h"
#include "src/storyland/storyland_entry_exit.h"
#include "src/storyland/storyland_environment.h"
#include "src/storyland/storyland_ped_movement.h"
#include "src/storyland/storyland_ped_contact.h"
#include "src/leeds/leeds_ped_stats.h"
#include "src/storyland/storyland_water.h"
#include "src/storyland/storyland_water_cannon.h"
#include "src/re/reVCS/runtime/storyland_vcs_sniper_bullet.h"
#include "src/storyland/storyland_debug_movement.h"
#include "src/storyland/storyland_crime.h"
#include "src/storyland/storyland_service_roles.h"
#include "src/storyland/storyland_player_restart.h"
#include "src/storyland/storyland_player_reactions.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_formation.h"
#include "src/storyland/storyland_ped_damage.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_bullet_reaction.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_ped_motion.h"
#include "src/storyland/storyland_escalators.h"
#include "src/re/reVCS/pedestrians/storyland_brawn_zombie.h"
#include "src/storyland/storyland_extended_movement.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_fall.h"
#include "src/storyland/storyland_screen_blood_events.h"
#include "src/storyland/storyland_sprint.h"
#include "src/storyland/storyland_play_rubbish.h"
#include "src/storyland/storyland_vehicle_exit.h"
#include "src/re/reLCS2/runtime/storyland_lcs_stopped.h"

enum class StorylandPedState { Walk, Idle, Flee, Combat, Dead };
struct StorylandPlayTestRuntime;
struct StorylandServiceVehicleAutopilot;
bool storylandVcsJetSkiRoomClear(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t target,
    const StorylandVcsJetSkiRoomQuery& query,bool strongWaterContact);
bool storylandVcsHelicopterRoomClear(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t target,
    const StorylandVcsVehicleRoomQuery& query);
bool storylandVcsAutomobileRoomClear(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t target,
    const StorylandVcsVehicleRoomQuery& query);
bool storylandAutomobileRoomClear(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t target,
    const StorylandVcsVehicleRoomQuery& query);
bool storylandVehicleExitDoorReserved(const StorylandPlayTestRuntime& runtime,size_t target,
    unsigned nativeSeat,bool rightHandSide,int32_t character=0);
bool storylandVehicleExitSeatOccupied(const StorylandPlayTestRuntime& runtime,size_t target,
    unsigned nativeSeat,int32_t character=0);
bool storylandVehicleExitPositionClear(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t target,const StorylandNativePoint& position,
    bool allowMotorcycleOverlap=false);
bool storylandVehicleExitDestinationClear(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t target,const StorylandNativePoint& position);
bool storylandFindVehicleExitPosition(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t target,bool rightHandSide,
    const StorylandNativePoint& originalDoor,StorylandNativePoint& result);
void storylandBindAmbientCharacterCallbacks(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& pedestrians);
bool storylandCharacterTouchesVehicle(const StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, const std::vector<StorylandNativeTrafficActor>& cars,
    int32_t character, int32_t vehicle);
bool storylandScriptLineOfSightHit(const StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, const std::vector<StorylandNativeTrafficActor>& cars,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,
    const StorylandNativePoint& start, const StorylandNativePoint& end, uint32_t mask,
    int32_t ignoredCharacter=-1);
bool storylandPhotographLineClear(const StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const std::vector<StorylandNativeTrafficActor>& cars,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,int32_t character,
    const StorylandNativePoint& start,const StorylandNativePoint& end);
bool storylandPhotographVehicleLineClear(const StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const std::vector<StorylandNativeTrafficActor>& cars,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,int32_t vehicle,
    const StorylandNativePoint& start,const StorylandNativePoint& end);
enum class StorylandPlayTestSound { Footstep, Jump, Land, VehicleEnter, VehicleExit, Damage, WaterSplash, WeaponShot, WeaponReload, WeaponPickup, Melee, Explosion, VehicleImpact, VehicleStarter, RadioSwitch, VehicleAlarm, VehicleDoorOpen, VehicleDoorClose, MeleeSwing, Count };
void storylandUpdatePlayTestVehicleDoor(StorylandPlayTestRuntime& runtime,size_t slot,
    const StorylandNativePoint& position,unsigned nativeSeat,bool rightHandSide,
    StorylandVehiclePhase phase,float seconds);
constexpr size_t StorylandPlayTestSoundCount = size_t(StorylandPlayTestSound::Count);
struct StorylandRangedAttack {
    bool active=false,reloading=false,reloadPending=false,cancelling=false;
    size_t weapon=SIZE_MAX;
    int32_t animationId=-1;
    float time=0,blend=0;
};
struct StorylandZombiePedestrianState {
    bool active=false;
    size_t originalModel=0,originalWeapon=0;
    StorylandPedStats originalStats;
    int originalAmmunition=0,originalMagazine=0;
    float originalHealth=0,initialHealth=0,originalArmour=0;
    bool originalHostile=false;
};
struct StorylandRuntimePed {
    int32_t scriptHandle = -1;
    uint64_t speechIdentity=0;
    uint64_t turfWarActorId=0;
    int32_t extendedGangIdentity=-1;
    int32_t naturalPedType = -1;
    StorylandCharacterWaterState water;
    StorylandPedState state = StorylandPedState::Walk;
    float health = 100, armour = 0, timer = 0;
    float deathTime = 0;
    StorylandCorpseBloodState corpseBlood;
    uint8_t renderAlpha=255;
    bool populationFadeOut=false;
    StorylandDeathAssociation death;
    StorylandPlayerReactionPose pairedDeathPose;
    float nextVoiceTime=0,nextVehicleHitTime=0;
    size_t model = 0;
    size_t weapon=0;
    int ammunition=60,magazine=0;
    float weaponCooldown=0,reloading=0,fireFlash=0,combatSpeed=0;
    bool hostile=false,policePursuing=false;
    bool extendedCrouch=false;
    float extendedCrouchBlend=0.0f,extendedCrouchTime=0.0f;
    float policeLastSeenAt=-100000.0f;
    float policeLastHeardAt=-100000.0f;
    StorylandNativePoint policeLastKnownPosition{};
    bool policeLastKnownPositionValid=false,policeCanSeePlayer=false;
    uint64_t policeAssailantIdentity=0;
    int32_t policeAssailantMissionHandle=-1;
    float policeAssailantUntil=0;
    bool playOptionHostile=false;
    StorylandZombiePedestrianState zombie;
    StorylandServiceRole serviceRole=StorylandServiceRole::Civilian;
    uint64_t serviceVehicleIdentity=0;
    uint8_t servicePassengerSeat=0;
    uint64_t emergencyIdentity=0, emergencyTarget=0;
    float emergencyTreatmentTime=0;
    bool emergencyTreating=false, emergencyScriptOwned=false;
    int32_t emergencyFireHandle=-1;
    uint32_t emergencyFireDeadline=0;
    uint64_t emergencyFireVehicle=0;
    bool emergencyExtinguishing=false;
    bool fortBaxterSoldier=false,personallyProvoked=false;
    bool playerDamaged=false,playerKillCounted=false;
    StorylandNativePoint militaryPatrolAnchor;
    StorylandPatrolMember patrol;
    float coughTime=0;
    unsigned burstShots=0;
    StorylandNativePoint weaponMuzzle{};
    bool weaponMuzzleValid=false;
    StorylandNativePoint head{};
    bool headValid=false;
    StorylandNativePoint torso{};
    bool torsoValid=false;
    StorylandNativePoint nativeRoot{};
    bool nativeRootValid=false;
    uint32_t shotSerial=0;
    StorylandMeleeAttack melee;
    StorylandRangedAttack ranged;
    StorylandPedStats stats;
    StorylandPedContactState contact;
    StorylandVcsBulletReaction bulletReaction;
    StorylandPedVehicleContactState vehicleContact;
    bool fleeActive=false,fleeFromPlayer=false,fleeWalking=false;
    StorylandNativePoint fleePosition{};
    uint32_t fleeLastNode=UINT32_MAX,fleeNextNode=UINT32_MAX;
    float fleeRouteTime=0;
    int32_t threatAnimation=-1;
    float threatAnimationTime=0,threatTimeRemaining=0,threatClipDuration=0,threatBlend=0;
    bool threatWaiting=false;
    bool isPolice(bool vcs) const {
        return storylandLawEnforcementRole(serviceRole)||
            (stats.valid&&(stats.pedType==6||stats.name=="STAT_COP"));
    }
    void startFlee(const StorylandNativePoint& source,float seconds,bool followPlayer=false) {
        if(!fleeActive||fleeFromPlayer!=followPlayer) {
            fleeLastNode=UINT32_MAX;fleeNextNode=UINT32_MAX;fleeRouteTime=0;
        }
        fleeActive=true;fleeFromPlayer=followPlayer;fleeWalking=false;fleePosition=source;
        threatWaiting=false;threatTimeRemaining=0;
        state=StorylandPedState::Flee;timer=seconds;
    }
    void clearFlee() {
        fleeActive=false;fleeFromPlayer=false;fleeWalking=false;
        fleeLastNode=UINT32_MAX;fleeNextNode=UINT32_MAX;fleeRouteTime=0;
    }
    void clearGunThreat() {
        threatAnimation=-1;threatAnimationTime=0;threatTimeRemaining=0;
        threatClipDuration=0;threatBlend=0;threatWaiting=false;
    }
};
void storylandRestoreZombiePedestrian(StorylandRuntimePed& pedestrian);
void storylandUpdateZombiePedestrian(StorylandPlayTestRuntime& runtime,
    StorylandRuntimePed& pedestrian);

struct StorylandRuntimeVehicle {
    StorylandScriptVehicleTravel scriptTravel;
    StorylandVehiclePedSweepState pedestrianSweep;
    StorylandVcsTrafficSupport::RailState vcsTrafficRail;
    uint64_t serviceCrewIdentity=0;
    int32_t pendingCrewModel=-1;
    uint8_t pendingCrewSeat=0;
    float pendingCrewHealth=100,pendingCrewArmour=0;
    // Ambient service routes follow the vehicle when traffic slots move.
    std::shared_ptr<StorylandServiceVehicleAutopilot> serviceAutopilot;
    uint64_t emergencyFireIdentity=0;
    float emergencyFireStrength=1.0f;
    bool explicitComponents = false;
    std::array<int32_t,2> components{-1,-1};
    StorylandForkliftState forklift;
    bool craneCarried=false;
    std::array<float,16> craneFrame{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    bool aiActive=false;
    int8_t drivingStyle=0,aiMission=0;
    int32_t aiTargetVehicle=-1;
    uint32_t aiMissionStartedAt=0;
    uint32_t lastNativeVehicleContactAt=0,lastNativeTrafficMovementAt=0;
    uint16_t routeSeed=0,alarmMilliseconds=0;
    bool avoidLevelTransitions=false,fastLane=true,lanePolicySet=false,convoy=false;
    uint8_t straightLineDistance=0;
    float massMultiplier=1,turnMassMultiplier=1;
    std::array<uint8_t,7> panelDamage{};
    std::array<uint8_t,4> lightDamage{};
    float heliOrientation=-1,rotorSpeed=0,rotorAngle=0;
    // Transient firing state for the ambient police helicopter. Restart the
    // acquisition after loading because the original prediction history is
    // not yet represented in the host save format.
    uint32_t policeHeliAimSince=0,policeHeliNextShot=0,policeHeliLastClock=0;
    bool policeHeliAiming=false;
    std::array<float,3> vcsPlaneControlAxes{};
    std::array<float,4> vcsJetSkiAxes{};
    storyland_vcs_jetski_pose::State vcsJetSkiPose;
    float vcsJetSkiBrake=0;
    float vcsJetSkiContactImpulse=0;
    std::array<float,3> vcsJetSkiContactNormal{};
    bool heliStabiliser=false,heliCrashing=false;
    int32_t lastCollidedSlot=-1;
    int32_t pendingContactSlot=-1;
    bool pendingBodyImpact=false;
    bool watertight=false,strong=false,upsideDownDamageDisabled=false;
    bool canBurstTyres=true,visiblyDamaged=false,boatAnchored=false;
    float tractionMultiplier=1;
    std::array<uint8_t,4> tyreStatus{};
    std::array<uint8_t,6> doorDamage{};
    float bonnetOpenRatio=0,bootOpenRatio=0,bootAngularVelocity=0;
    std::array<float,3> angularVelocity{};
    int32_t colorSet=-1,scriptColorSet=-1;
    bool clumpVisible=true;
    bool positionFrozen=false,collisionEnabled=true,loadCollision=true;
    bool onlyDamagedByPlayer=false,canBeDamaged=true,visible=true,siren=false,wrecked=false;
    bool taxiLight=false,bikeStandingOverride=false;
    int32_t doorLock=0;
    uint8_t renderAlpha=255;
    bool heavy=false;
    uint8_t damageProofs=0;
    int32_t autoPilotMission=-1;
    bool missionDriver=false,engineOn=true;
    float boatAcceleratorInput=0,boatBrakeInput=0,boatSteeringInput=0;
    float boatAnchorHeading=10000.0f;
    float waterCannonYaw=0,waterCannonPitch=.05f;
    StorylandVehicleWaterState water;
    float speed = 0, health = 1000;
    StorylandVehicleDefinition nativeDefinition;
    StorylandVehicleTransmissionState transmission;
    bool nativeHandling=false;
    float yawVelocity=0;
    std::array<float,4> doorOpenRatios{};
    std::array<float,4> wheelPositions{};
    std::array<float,4> wheelSpringRatios{1,1,1,1};
    std::array<bool,4> wheelContacts{};
    std::array<bool,4> wheelContactHistoryValid{};
    std::array<float,4> wheelContactTimers{};
    std::array<StorylandNativePoint,4> wheelContactPoints{},wheelContactNormals{};
    std::array<uint8_t,4> wheelSurfaces{};
    std::array<StorylandVehicleContact::TyreState,4> wheelTyreStates{};
    std::array<float,4> wheelTyreSlip{};
    bool tyreForcesApplied=false;
    float bikeTyreAcceleration=0;
    std::array<float,3> bikeTyreAccelerationWorld{};
    float bikeLeanInput=0;
    float bikeVisualLean=0,bikeRawSteeringAngle=0;
    bool bikeBeingPickedUp=false;
    std::array<float,4> wheelLineFractions{1,1,1,1};
    bool contactsValid=false,bodyContact=false;
    float stoppedSeconds = 0;
    size_t model = 0;
    bool waitingForSignal=false;
    bool parked = false;
    bool hasDriver = true;
    size_t driverModel = SIZE_MAX;
    StorylandServiceRole serviceRole=StorylandServiceRole::Civilian;
    bool vcsViceUnit=false;
    uint8_t vcsViceCrewSequence=0;
    float vcsVicePlayerStopMilliseconds=0;
    bool vcsViceExitRequested=false;
    bool policePursuing=false,playerBodyContact=false;
    float policeLastSeenAt=-100000.0f;
    int radioStation = -1;
    float displacedDriverTime = 0;
    StorylandNativePoint displacedDriverPosition{};
    float roadPitch=0,roadRoll=0,bodyHeight=1.35f;
    float halfWidth=0.8f,halfLength=1.7f,mass=1500,traction=1,nextGroundCheck=0;
    float handlingTopSpeed=32,handlingReverse=8,handlingEngine=7,handlingBrake=10;
    float handlingSteerLock=35,handlingDrag=1,handlingTractionLoss=1;
    float handlingSuspensionForce=1.4f,handlingSuspensionDamping=0.4f;
    int primaryColor=-1,secondaryColor=-1,generator=-1,scriptHandle=-1,savedGarage=-1;
    uint32_t coopIdentity = 0;
    uint8_t garageOwner = 255;
    bool vcsGarageStorable = false;
    bool coopControlled = false, coopSeatReserved = false;
    float suspension=0,suspensionVelocity=0,bodyRoll=0,bodyPitch=0;
    float springRollVelocity=0,springPitchVelocity=0,suspensionAnchorZ=0;
    bool suspensionAnchorValid=false;
    std::array<float,4> wheelSpringImpulses{},wheelDampingImpulses{};
    float wheelRotation=0,rearWheelRotation=0,steeringAngle=0,wheelRadius=0.34f;
    float steeringInput=0;
    bool steeringInputValid=false;
    StorylandNativePoint wheelTravelPosition{};
    bool wheelTravelInitialized=false;
    bool braking=false,headlightsOn=false;
    bool burning=false;
    bool charred=false;
    float engineFireTime=0;
    bool engineFireSpent=false; // Finite engine fuel stays spent until the damage is repaired.
    bool handbrake=false;
    float longitudinalAcceleration=0,particleTime=0,impactCooldown=0;
    uint32_t particleFrame=0;
    float throttle=0,lateralSpeed=0;
    float tyreSlip=0,skidSmokeTime=0;
    bool skidRoadContact=false;
    std::array<StorylandNativePoint,4> skidContacts{};
    std::array<bool,4> skidContactValid{};
    std::array<StorylandBloodyTyreState,4> bloodyTyres{};
    float verticalSpeed=0;
    bool grounded=true;
};
inline float storylandVcsJetSkiNativeAngularZ(const StorylandRuntimeVehicle& vehicle) {
    return (vehicle.yawVelocity*.01745329251994329577f+vehicle.angularVelocity[2])*.02f;
}
inline void storylandApplyVehicleDefinition(StorylandRuntimeVehicle& vehicle,
    const StorylandVehicleDefinition& definition) {
    vehicle.suspensionAnchorValid=false;vehicle.contactsValid=false;
    vehicle.springPitchVelocity=vehicle.springRollVelocity=0;
    vehicle.wheelTyreStates={};vehicle.wheelTyreSlip={};vehicle.tyreForcesApplied=false;
    vehicle.bikeTyreAcceleration=0;vehicle.bikeTyreAccelerationWorld={};
    vehicle.bikeVisualLean=0;vehicle.bikeRawSteeringAngle=0;vehicle.bikeBeingPickedUp=false;
    const bool newDefinition=vehicle.nativeDefinition.modelId!=definition.modelId||
        vehicle.nativeDefinition.vcsModelLayout!=definition.vcsModelLayout;
    if(newDefinition) {
        vehicle.wheelContactHistoryValid={};
        vehicle.wheelContactTimers={};
        vehicle.bikeLeanInput=0;
        vehicle.forklift={};
        vehicle.steeringInput=0;
        vehicle.steeringInputValid=false;
        vehicle.waterCannonYaw=0;vehicle.waterCannonPitch=.05f;
        vehicle.vcsPlaneControlAxes={};
        vehicle.vcsJetSkiAxes={};vehicle.vcsJetSkiPose={};
        vehicle.vcsJetSkiBrake=0;
        vehicle.vcsJetSkiContactImpulse=0;vehicle.vcsJetSkiContactNormal={};
        if(definition.vcsHovercraft()&&(definition.modelFlags&0x100u)!=0)
            for(size_t door=2;door<vehicle.doorDamage.size();++door)vehicle.doorDamage[door]=3;
    }
    vehicle.nativeDefinition=definition;vehicle.nativeHandling=definition.hasNativeHandling;
    if(newDefinition&&!definition.vcsModelLayout) {
        vehicle.boatAnchored=definition.domain==StorylandVehicleDomain::Water;
        vehicle.doorLock=1;
    }
    for(size_t i=0;i<definition.wheels.size()&&i<vehicle.wheelPositions.size();++i)
        vehicle.wheelPositions[i]=definition.wheels[i].restZ;
    vehicle.wheelSpringRatios={1,1,1,1};vehicle.wheelContacts={};
    vehicle.wheelRadius=definition.wheelRadius(false,vehicle.wheelRadius);
    vehicle.bodyHeight=definition.bodyRoofHeight(vehicle.bodyHeight);
    vehicle.mass=definition.mass;
    vehicle.traction=definition.traction;
    if (vehicle.primaryColor < 0 && vehicle.secondaryColor < 0 && !definition.colors.empty()) {
        vehicle.colorSet = 0;
        vehicle.scriptColorSet = -1;
        vehicle.primaryColor = definition.colors.front()[0];
        vehicle.secondaryColor = definition.colors.front()[1];
    }
    if(!definition.hasNativeHandling)return;
    const float accelerationScale=definition.driveType=='4'?25000.0f:12500.0f;
    const float nativeMaximum=definition.gears[std::clamp<unsigned>(definition.numberOfGears,1,5)].maximum;
    vehicle.handlingTopSpeed=std::clamp(nativeMaximum>.001f?nativeMaximum*StorylandVehicleStepsPerSecond:
        definition.maximumVelocity/3.6f,4.0f,65.0f);
    vehicle.handlingReverse=std::clamp(std::fabs(definition.reverseVelocity)*StorylandVehicleStepsPerSecond,2.0f,15.0f);
    vehicle.handlingEngine=std::clamp(definition.engineAcceleration*accelerationScale,1.0f,28.0f);
    vehicle.handlingBrake=std::clamp(definition.brakeDeceleration*2500.0f,2.0f,35.0f);
    vehicle.handlingSteerLock=std::clamp(definition.steeringLock,5.0f,65.0f);
    vehicle.handlingDrag=std::clamp(definition.dragMultiplier/2.0f,0.2f,5.0f);
    vehicle.handlingTractionLoss=std::clamp(definition.tractionLoss,0.2f,2.0f);
    vehicle.handlingSuspensionForce=std::clamp(definition.suspensionForce,0.25f,5.0f);
    vehicle.handlingSuspensionDamping=std::clamp(definition.suspensionDamping,0.05f,2.0f);
}
struct StorylandPlayTestSoundEvent {
    StorylandPlayTestSound type;
    StorylandNativePoint position;
    int weapon=-1;
    int speaker=-1;
    uint64_t speakerIdentity=0;
    int nativeSpeech=-1;
    int nativePedAudioEvent=-1;
    uint8_t nativePedMoveState=1;
    int vehicle=-1;
    bool fatal=false;
    uint8_t surface=0;
    float volume=1;
    bool positioned=false;
    StorylandPlayTestSoundEvent(StorylandPlayTestSound value) : type(value) {}
};
struct StorylandVehicleObstacle {
    StorylandNativePoint position;
    float yaw=0,halfLength=1.7f,halfWidth=0.8f,height=1.35f;
    bool collisionEnabled=true;
};
struct StorylandCrouchWeaponPose {
    int32_t nativeWeapon=-1,animation=155;
    uint32_t startedAt=0,blendStartedAt=0;
    float offset=0,duration=0;
    bool firstShotPending=false,hasFired=false,baseRestored=false;
    bool active() const {return animation==215||animation==217;}
};
struct StorylandMissionCombatState {
    StorylandCorpseBloodState corpseBlood;
    StorylandPlayerReactionPose pairedDeathPose;
    size_t weapon=0;
    int32_t nativeWeapon=0,magazine=0;
    float cooldown=0,reloading=0;
    uint32_t shotSerial=0;
    StorylandCrouchWeaponPose crouchPose;
    bool extendedAutoCrouch=false;
    uint32_t extendedAutoCrouchDeadline=0;
    StorylandMeleeAttack melee;
    StorylandPedContactState contact;
    StorylandVcsBulletReaction bulletReaction;
    StorylandPedVehicleContactState vehicleContact;
    StorylandCharacterWaterState water;
    StorylandNativePoint muzzle{},head{},torso{};
    bool anchorsValid=false;
    bool muzzleValid=false;
    StorylandNativePoint nativeRoot{};
    bool nativeRootValid=false;
    float coughTime=0;
    // Transient physical state, shared with the character's water/body state.
    float verticalSpeed=0;
    float airborneTime=0,landingTime=0;
    StorylandNativePoint gravityPosition{};
    bool gravityPositionValid=false,grounded=false;
    StorylandVcsFallState fall;
};
struct StorylandAdrenalineState {
    static constexpr int32_t pickupModel=467;
    static constexpr uint32_t durationMilliseconds=20000;
    bool enabled=false,active=false;
    uint32_t expiresAt=0;
    int32_t pickupHandle=-1;
    template<class Archive> void saveState(Archive& archive) {
        archive(enabled,active,expiresAt,pickupHandle);
    }
};
struct StorylandTargetIndicatorState {
    int32_t ambient=-1,handle=-1;
    uint8_t kind=0;
    float healthRatio=0;
    uint32_t frame=0;
    bool active=false;

    void reset() { *this={}; }
    void update(int32_t ambientTarget,uint8_t targetKind,int32_t targetHandle,
        float ratio,uint32_t nativeFrame) {
        ratio=std::isfinite(ratio)?std::clamp(ratio,0.0f,1.0f):0.0f;
        if(!active||ambient!=ambientTarget||kind!=targetKind||handle!=targetHandle||nativeFrame<frame) {
            ambient=ambientTarget;kind=targetKind;handle=targetHandle;
            healthRatio=ratio;frame=nativeFrame;active=true;
        } else if(frame!=nativeFrame) {
            if(ratio<healthRatio)healthRatio=std::max(0.0f,healthRatio-.1f);
            frame=nativeFrame;
        }
    }
};
struct StorylandMissionCharacterDamage {
    int32_t handle = -1, model = -1;
    float amount = 0;
    bool playerAttack = false;
    uint8_t kind = 0;
    int32_t weapon = -1;
    uint8_t sourceKind = 0;
    int32_t sourceHandle = -1;
    bool headHit = false, critical = false;
    float attackTime = 0;
    uint8_t hitPiece = 0, hitDirection = 0;
};

struct StorylandCoopPlayerHit {
    int32_t weapon = -1;
    int32_t explosionType = -1;
    uint32_t kind = 0, shot = 0, targetLife = 0;
    float attackTime = 0;
    StorylandNativePoint origin{}, impact{};
    uint8_t hitPiece = 0, hitDirection = 0;
};

struct StorylandExtendedHaitianGang {
    static constexpr int32_t populationKind=10;
    static constexpr int32_t pedestrianType=32;
    bool available=false;
    std::array<int32_t,2> pedModels{-1,-1};
    std::array<int32_t,2> weapons{-1,-1};
    int32_t health=100,armour=0;
};

struct StorylandPlayTestRuntime {
    StorylandPlayRubbish rubbish;
    uint64_t nextEmergencyIdentity=1;
    bool extendedAndSaFeatures=false;
    bool extendedLcsCrouch=false;
    std::function<int32_t(const StorylandNativePoint&,int32_t)> extendedGangSlotAt;
    StorylandExtendedHaitianGang extendedHaitianGang;
    StorylandExtendedMovement extendedMovement;
    float playerReloadClipSeconds=0.0f;
    float playerReloadElapsed=0.0f;
    size_t playerReloadWeapon=SIZE_MAX;
    float playerCameraWaterDroplets=0.0f;
    float playerCameraBloodDroplets=0.0f;
    bool vehicleEffectsFirstPerson=false,vehicleEffectsLookingForward=true;
    uint32_t playerCameraWaterSerial=0;
    uint32_t playerCameraBloodSerial=0;
    StorylandScreenBloodView screenBloodView;
    StorylandEscalatorState escalators;
    uint32_t playerVehicleAnnoyanceSerial=0;
    StorylandRestartTaxiState restartTaxi;
    uint32_t coopLife = 1;
    std::function<void(const StorylandCoopPlayerHit&)> coopHitPlayer;
    bool coopGuest = false;
    bool coopConnected = false, coopFriendlyFire = false;
    uint8_t coopPlayerIndex = 0;
    uint32_t coopDamageSerial = 0;
    int32_t coopRemoteVehicle = -1;
    std::function<bool(int32_t, int32_t, const StorylandNativePoint&)> coopVehicleEntryAllowed;
    std::function<int32_t(int32_t, const StorylandNativePoint&)> coopPreferredVehicleSeat;
    std::function<bool(uint32_t, const StorylandNativePoint&)> coopDisplaceDriver;
    bool coopRemoteAlive = false;
    uint32_t coopRemoteLife = 0;
    StorylandNativePoint coopRemotePosition{};
    StorylandNativePoint coopRemoteTorso{};
    float coopRemoteHeading = 0;
    std::function<int(const StorylandNativePoint&, bool)> coopChooseTarget;
    std::function<void(float, int32_t, uint8_t, int32_t)> coopDamageRemote;
    std::function<bool(const StorylandMissionCharacterDamage&)> routeMissionCharacterDamage;
    struct StorylandMissionEnvironment* missionEnvironment=nullptr;
    float nextPlayerVoiceTime=0;
    bool playerCanSwim=true;
    bool playerSprintActive=false;
    StorylandSprint::State playerSprint;
    bool vehicleBombButtonWasDown=false,vehicleTaxiButtonWasDown=false;
    StorylandCharacterWaterState playerWater;
    bool playerDivingAvailable=false,playerDiveDown=false,playerDiveUp=false;
    std::function<float(float,float)> playerDiveRootDistance;
    bool vehicleExitRight=false,vehicleExitRear=false,vehicleWasParked=false;
    bool vehicleJetSkiWaterContact=false,vehicleJetSkiHighAlign=false;
    std::string vehicleTransitionError;
    std::string vehiclePhysicsError; // Transient; never included in checkpoint state.
    std::set<uint64_t> vehicleContactPairsThisFrame; // Transient impulse bookkeeping; never saved.
    StorylandVehicleEntryVariant vehicleEntryVariant=StorylandVehicleEntryVariant::Normal;
    float vehicleApproachSpeed=0,vehicleTransitionInitialRoll=0,vehicleTransitionInitialPitch=0;
    StorylandVehicleExitPose vehicleExitPose;
    StorylandVcsPedFallState playerJetSkiFall;
    bool playerJetSkiFallActive=false;
    std::function<StorylandNativePoint(int32_t,int32_t,float,float)> vehicleExitRootDelta;
    StorylandNativePaths livePaths;
    size_t appliedRoadChanges=0;
    StorylandPedStats playerStats;
    std::vector<StorylandWeaponDefinition> weapons;
    std::vector<StorylandWeaponInventory> inventory;
    std::vector<StorylandWeaponInventory> confiscatedWeapons;
    std::array<StorylandBetaWeaponGrant,size_t(StorylandBetaWeapon::Count)> betaWeapons{};
    StorylandAdrenalineState adrenaline;
    StorylandKillFrenzy killFrenzy;
    bool betaVehicleBombsEnabled=false;
    uint64_t nextProjectileSequence=1;
    StorylandNativePoint weaponBuybackPosition;
    bool weaponBuybackAvailable=false;
    bool busted=false;
    bool armyFatigues=true,fortBaxterAlarm=false;
    bool militaryScriptControlled=false,militaryScriptHostile=false;
    bool militaryPopulationEnabled=true;
    std::map<std::string,double> statistics;
    StorylandNativePoint statisticsPosition;
    bool statisticsPositionValid=false;
    std::map<int32_t,uint32_t> modelsKilledByPlayer;
    unsigned fortBaxterKills=0;
    uint32_t fortBaxterPendingDeaths=0;
    bool fortBaxterWasInScriptZone=false,fortBaxterScriptStateObserved=false;
    float fortBaxterPopulationAt=0;
    float fortBaxterHeat=0,fortBaxterLastUpdate=0;
    float policeDispatchTime=0;
    float arrestProgress=0;
    int arrestWanted=0,arrestOfficer=-1;
    std::vector<StorylandWeaponPickup> pickups;
    std::vector<StorylandWeaponTrace> shotTraces;
    bool sniperTraceActive = false;
    StorylandNativePoint sniperTraceFrom{},sniperTraceTo{};
    struct MovementInputSample {
        float horizontal=0,forward=0;
        bool analogMovement=false;
    };
    std::array<MovementInputSample,10> drunkMovementHistory{};
    std::vector<StorylandWeaponProjectile> projectiles;
    std::array<StorylandVcsSniperBullet,100> vcsSniperBullets{};
    std::array<StorylandWaterCannon,3> waterCannons{};
    std::vector<StorylandRuntimeParticle> particles;
    std::function<bool(int32_t,StorylandNativeParticleDescriptor&)> readNativeParticleDescriptor;
    uint32_t bloodSplatSequence=0;
    std::vector<StorylandTyreBloodContact> tyreBloodContacts;
    std::vector<StorylandRuntimeEffect> effects,worldEffects;
    std::vector<StorylandNativePoint> vehiclePositions;
    std::vector<std::array<float,16>> vehicleWorldMatrices;
    std::vector<StorylandVehicleObstacle> movementCars;
    std::vector<StorylandNativePoint> movementPedestrians;
    StorylandNativePoint pedestrianContactPlayerVelocity{};
    float playerCoughTime=0,aimPitch=0;
    StorylandNativePoint playerMuzzle{};
    bool playerMuzzleValid=false;
    StorylandNativePoint playerHead{},playerTorso{},playerShoulder{};
    bool playerHeadValid=false,playerTorsoValid=false,playerShoulderValid=false;
    bool manualAimActive=false;
    int32_t manualAimMode=0;
    StorylandNativePoint manualAimOrigin{},manualAimTarget{};
    bool freeAimActive=false;
    StorylandNativePoint freeAimOrigin{},freeAimTarget{};
    StorylandCrouchWeaponPose playerCrouchPose;
    std::map<int32_t,StorylandMissionCombatState> missionCombat;
    StorylandNativePoint playerPosition{};
    size_t equippedWeapon=0;
    int targetedPed=-1;
    bool targetLockRequested=false;
    bool playerAiming=false;
    size_t targetWeapon=SIZE_MAX;
    StorylandNativePoint targetPosition{};
    bool targetVisible=false;
    StorylandTargetIndicatorState targetIndicator;
    float nextPickupRefresh=0;
    bool weaponAi=true,pickupsEnabled=true;
    bool aggressivePedestrians=false;
    bool zombiePedestrians=false;
    size_t zombiePedestrianModel=SIZE_MAX,zombiePedestrianWeapon=SIZE_MAX;
    bool randomStreetWeapons=false;
    uint32_t combatRandom=0x51A3D279u;
    uint32_t playerShotSerial=0;
    uint32_t photographSerial=0;
    bool remoteDetonationHeld=false;
    float playerLastShotTime = -1000.0f;
    float playerLastGunshotTime = -1000.0f;
    StorylandNativePoint playerLastGunshotPosition{};
    StorylandMeleeAttack playerMelee;
    uint8_t groundSurface=0;
    float health = 100, armour = 0, verticalSpeed = 0;
    float playerDeathTime = 0;
    StorylandCorpseBloodState playerCorpseBlood;
    StorylandDeathAssociation playerDeath;
    StorylandBulletReaction playerBulletReaction;
    StorylandPedVehicleContactState playerVehicleContact;
    uint64_t nextVehiclePedContactIdentity = 1;
    float gravity = StorylandGravityPerSecond;
    StorylandNativePoint windVelocity{};
    int money = 0, wanted = 0, occupiedVehicle = -1;
    StorylandLcsHudState lcsHud;
    int32_t pendingBikeImpactVehicle=-1;
    StorylandBikeFallDecision pendingBikeImpact;
    int32_t playerVehicleSeat=-1; // -1 driver; native passenger slots start at zero.
    int maxWanted=6;
    StorylandCrimeState crime;
    StorylandPlayerRestartState playerRestart;
    StorylandPlayerReactionState playerReaction;
    bool pcCombatControls=false, pcGrabHeld=false, pcCancelAim=false;
    bool equalizerScope=false, equalizerScopeHeld=false;
    int32_t combatControlWeapon=-1;
    StorylandPlayerReactionBindings reactionBindings;
    uint64_t nextPatrolGroup=1;
    float patrolRecruitAt=0;
    bool policeIgnorePlayer=false;
    float wantedMultiplier=1.0f;
    bool wastedActive=false;
    float respawnFade=0;
    bool playerControl=true;
    bool pauseMenu=false,pauseOptions=false;
    bool pauseMap=false;
    float pauseMapZoom=1.0f,pauseMapPanX=0,pauseMapPanY=0;
    int32_t mapCollectibleLayer=-1;
    bool radioEnabled=true;
    bool collisionOccupantDamage=false;
    std::vector<StorylandRoadSkid> roadSkids;
    struct GeneratorState {
        bool spawned=false,waitingForDeparture=false;
        uint32_t nextSpawnAt=0;
    };
    std::map<int32_t,GeneratorState> generatorStates;
    uint8_t carGeneratorPhase=0;
    std::string radioStationStem,radioStationLabel;
    std::string splashScreen;
    bool suppressLevelSplash=false;
    float splashStartedAt=-100;
    float radioPopupTime=0;
    std::vector<StorylandEntryExit> entryExits;
    std::string entryError;
    float entryErrorUntil=0;
    int visibleArea=0,entryTransition=-1;
    float entryFade=0,entryTime=0,entryCooldown=0;
    bool entryReverse=false,entryWarped=false,entryNeedsLeave=false,entryCameraResetPending=false;
    bool scriptTimeFrozen=false;
    bool debugClockFrozen=false;
    using VehicleAction=StorylandVehicleAction;
    VehicleAction vehicleAction = VehicleAction::None;
    int transitionVehicle = -1;
    float vehicleActionTime = 0;
    float vehicleActionDuration = 0;
    StorylandNativePoint vehicleActionStart{},vehicleActionDoor{};
    StorylandVehiclePhaseTiming vehicleTiming;
    StorylandVehiclePhaseSample vehiclePhase;
    std::vector<StorylandNativePoint> vehicleApproach;
    bool vehicleTransitionPrepared=false;
    std::string missionText,missionHelp,pagerText;
    float clockHour = 12, gameMinutesPerSecond = 1;
    StorylandClockProgress clockProgress;
    float elapsed = 0, footstepTime = 0, waterSplashCooldown = 0;
    uint32_t trafficClockMilliseconds=0;
    uint32_t fallenWorldFrame=0;
    bool fallenPlayerVehicleRepositioned=false;
    double trafficClockRemainder=0;
    bool trafficLightsForcedGreen=false;
    StorylandDebugMovement debugMovement=StorylandDebugMovement::Walking;
    float debugMoveSpeed=10.0f;
    float movementSpeed = 0;
    std::array<float,2> playerAnimationMoveDelta{};
    bool playerAnimationMoveDeltaValid=false;
    float walkSpeed = 1.25f, runSpeed = 3.8f, sprintSpeed = 5.5f;
    float airborneTime=0,landingTime=0;
    float jumpLaunchDuration=0,jumpLandingDuration=.35f,jumpLaunchTime=0;
    StorylandVcsFallState playerFall;
    float jumpRunBlend=-1,jumpSprintBlend=-1,playerMass=StorylandNativePedMass;
    float playerMoveBlend=0,playerSprintBlend=0;
    bool playerSprintFading=false;
    float playerLocomotionPhase=0,playerStopTime=0;
    int playerStopVariant=-1;
    std::array<float,2> playerRunStopDuration{};
    std::function<float(unsigned,float,float)> playerRunStopDistance;
    std::function<float(bool,float,float)> playerCrouchRootDistance;
    std::function<float(StorylandExtendedAction,float)> extendedClimbRootProgress;
    std::function<StorylandNativePoint(StorylandExtendedAction,float,float)> extendedRollRootDelta;
    std::array<float,2> jumpHorizontalVelocity{};
    bool jumpLaunching=false,jumpButtonHeld=false,jumpHorizontalValid=false,jumpFromLaunch=false;
    float pedestrianWalkSpeed = 1.25f, pedestrianRunSpeed = 3.8f;
    float nextVehicleRefresh = 0;
    float nextLcsPedestrianRefresh=0;
    uint32_t populationRandom=0x4C435350u;
    std::vector<StorylandPedStats> pedestrianModelStats;
    std::vector<StorylandServiceRole> pedestrianModelRoles,modelServiceRoles;
    std::vector<std::string> modelNames;
    std::vector<std::array<uint32_t,16>> lcsPedestrianGroups;
    std::set<uint32_t> lcsPopulationModelsAttempted;
    uint32_t vehicleRespawns = 0;
    std::vector<size_t> roadModels, waterModels;
    std::vector<StorylandVehicleDefinition> modelDefinitions;
    std::vector<std::array<float,3>> modelExtents;
    std::vector<float> modelWheelRadii;
    bool grounded = true, paused = false, hud = true, sounds = true;
    // Allocation limits are distinct from the number of ambient actors wanted
    // by the active native zone. Mission actors share the native pool budget.
    // VCS 0x288134 -> 0x478F28 and LCS 0x22E6FC -> 0x22E9D8:
    // a1=70, with the a2 string naming the pool "Peds" in both executables.
    static constexpr size_t pedestrianCapacity(bool) {return 70;}
    std::vector<StorylandRuntimePed> pedestrians=std::vector<StorylandRuntimePed>(pedestrianCapacity(true));
    // Verified PS2 pool constructors: VCS 0x288158, LCS 0x22E71C.
    static constexpr size_t vehicleCapacity(bool vcs) {return vcs?40:70;}
    std::vector<StorylandRuntimeVehicle> vehicles=std::vector<StorylandRuntimeVehicle>(vehicleCapacity(true));
    std::vector<StorylandPlayTestSoundEvent> soundEvents;
    uint32_t policeHelicopterNextDispatch=0;
    bool buyBackWeapons();
    float maximumHealth() const;
    float maximumArmour() const;
    void reset(size_t pedModels,size_t vehicleModels,float hour);
    void damage(float amount,bool bleed=true,bool bypassArmour=false);
    bool inflictDamage(float amount,uint8_t kind,int32_t weapon,bool bleed=true,
        uint8_t piece=0,uint8_t direction=0,bool playerAttack=false,
        const StorylandNativePoint* source=nullptr);
    void recordCloseExecutionBlood(const StorylandNativePoint& victim,uint8_t kind,
        bool headHit,bool localPlayer);
};
bool storylandBeginPlayerDeath(StorylandPlayTestRuntime& runtime,
    StorylandDeathAssociation association,bool force=false);
bool storylandBeginBulletReaction(StorylandPlayTestRuntime& runtime,
    StorylandBulletReaction& reaction,uint8_t direction,const StorylandBulletImpact& impact={});
bool storylandAdvanceBulletKnockdown(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,StorylandBulletReaction& reaction,
    StorylandNativePoint& position,float heading,float seconds,int32_t pedestrian=-1,
    StorylandCharacterWaterState* water=nullptr);
bool storylandPlayerBulletReactionAllowed(const StorylandPlayTestRuntime& runtime);
bool storylandPlayerBulletReactionActive(const StorylandPlayTestRuntime& runtime);
bool storylandMissionBulletReactionAllowed(const StorylandPlayTestRuntime& runtime,int32_t handle);
bool storylandMissionBulletReactionActive(const StorylandPlayTestRuntime& runtime,int32_t handle);
bool storylandSetAdrenalineEnabled(StorylandPlayTestRuntime& runtime,bool enabled,bool vcs,std::string& error);
bool storylandActivateAdrenaline(StorylandPlayTestRuntime& runtime,bool vcs,std::string& error);
void storylandClearAdrenaline(StorylandPlayTestRuntime& runtime);
void storylandUpdateAdrenaline(StorylandPlayTestRuntime& runtime);
uint32_t storylandAdrenalineRemainingMilliseconds(const StorylandPlayTestRuntime& runtime);
float storylandAdrenalineTimeScale(const StorylandPlayTestRuntime& runtime,float scriptTimeScale);
float storylandAdrenalineAnimationRate(const StorylandPlayTestRuntime& runtime);
float storylandVehicleTyreGrip(const StorylandRuntimeVehicle& vehicle);
float storylandVehicleCollisionDamage(const StorylandRuntimeVehicle& vehicle, float damage, bool occupied);
bool storylandPlaceParkedRoadVehicle(StorylandRuntimeVehicle& vehicle,
    StorylandNativeTrafficActor& actor,const StorylandNativeWorld& world);
bool storylandPlaceVcsTrafficVehicle(StorylandRuntimeVehicle& vehicle,
    StorylandNativeTrafficActor& actor,const StorylandNativeWorld& world);
bool storylandUpdateVcsTrafficSurface(StorylandRuntimeVehicle& vehicle,
    StorylandNativeTrafficActor& actor,const StorylandNativeWorld& world,uint32_t frame);
void storylandUpdateScriptVehicleSystems(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& cars,float seconds);
void storylandApplyMissionVehicleOverrides(StorylandRuntimeVehicle& vehicle,
    StorylandNativeTrafficActor& actor, struct StorylandMissionEnvironment& world, int32_t handle);
void storylandBindVehicleAutopilotCallbacks(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& actors, const StorylandNativeWorld& nativeWorld);
void storylandCaptureMissionVehicleOverrides(const StorylandRuntimeVehicle& vehicle,
    const StorylandNativeTrafficActor& actor, struct StorylandMissionEnvironment& world, int32_t handle);
void storylandStepVehicleScriptPhysics(StorylandRuntimeVehicle& vehicle,
    StorylandNativeTrafficActor& actor, float seconds);
int32_t storylandFindAmbientVehicleNoSave(StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& actors, struct StorylandMissionEnvironment& world,
    const std::array<float,4>& bounds, int32_t model, bool aliveOnly);
void storylandAdvancePlayTestClock(StorylandPlayTestRuntime& runtime, float hours);
void storylandAdvancePlayTestClockSeconds(StorylandPlayTestRuntime& runtime, double seconds);
StorylandPlayTestInput storylandDelayedMovementInput(StorylandPlayTestRuntime& runtime,
    const StorylandPlayTestInput& input,uint8_t delay);
bool storylandSniperBulletInArea(const StorylandPlayTestRuntime& runtime,const std::array<float,6>& bounds);
bool storylandConsumeRestartOverride(StorylandMissionEnvironment& world,StorylandNativePoint& position,float& heading);
void storylandUpdatePlayTestTraffic(StorylandPlayTestRuntime& runtime,const StorylandNativePaths& paths,
    std::vector<StorylandNativeTrafficActor>& pedestrians,std::vector<StorylandNativeTrafficActor>& cars,
    const StorylandNativePoint& player,float seconds,const StorylandNativeWorld* world=nullptr);
bool storylandGangPlayerProtected(const StorylandPlayTestRuntime& runtime,int32_t pedestrianType);
void storylandUpdateMilitaryResponse(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld* world,
    const StorylandNativePoint& player);
void storylandCarryEscalatorActors(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    std::vector<StorylandNativeTrafficActor>& pedestrians,StorylandNativePoint& player);
void storylandUpdateFallenWorld(StorylandPlayTestRuntime& runtime,const StorylandNativePaths& paths,
    const StorylandNativeWorld& world,std::vector<StorylandNativeTrafficActor>& pedestrians,
    std::vector<StorylandNativeTrafficActor>& cars);
bool storylandRecoverFallenPlayer(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    StorylandNativePoint& player,std::vector<StorylandNativeTrafficActor>& cars);
void storylandRefreshPlayTestVehicles(StorylandPlayTestRuntime& runtime,const StorylandNativePaths& paths,
    std::vector<StorylandNativeTrafficActor>& cars,const StorylandNativePoint& player,size_t maximum,
    const StorylandNativeWorld* world=nullptr,
    std::vector<StorylandNativeTrafficActor>* pedestrians=nullptr);
size_t storylandAmbientPedestrianTarget(const StorylandPlayTestRuntime& runtime,
    const StorylandNativePoint& point);
void storylandRefreshLcsPedestrians(StorylandPlayTestRuntime& runtime,const StorylandNativePaths& paths,
    const StorylandNativeWorld& world,std::vector<StorylandNativeTrafficActor>& pedestrians,
    const StorylandNativePoint& player,float cameraYaw);
void storylandExtinguishPlayTestVehicle(StorylandPlayTestRuntime& runtime,size_t slot);
void storylandUpdatePlayTestVehicleEffects(StorylandPlayTestRuntime& runtime,
    const StorylandNativeTrafficActor& actor,StorylandRuntimeVehicle& vehicle,float seconds,
    const StorylandNativeWorld* world=nullptr);
float storylandZonePopulationScale(const StorylandMissionEnvironment& world,
    const StorylandNativePoint& point,bool vehicles);
inline float storylandVcsPedFallPoseBlend(const StorylandVcsPedFallState& fall,bool alive) {
    if(fall.recovering)return 1000;
    return !alive&&fall.fallAnimation==121&&fall.getUpAfter==UINT32_MAX?4.0f:8.0f;
}

void storylandUpdateVcsJetSkiOccupants(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& vehicles,StorylandNativePoint& player,
    float& heading,float seconds);
void storylandResetPlayerSprint(StorylandPlayTestRuntime& runtime);
void storylandUpdatePlayTestPlayer(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    StorylandNativePoint& player,float& heading,float cameraYaw,const StorylandPlayTestInput& input,
    bool jump,bool interact,std::vector<StorylandNativeTrafficActor>& cars,float seconds);

StorylandNativeMoveResult storylandMovePlayTestCharacter(const StorylandNativeWorld& world,
    const StorylandPlayTestRuntime& runtime,const StorylandNativePoint& from,const StorylandNativePoint& desired,
    int pedestrian=-1,bool avoidPlayer=false,bool followGround=true,bool collideWithPedestrians=true,
    int ignoredVehicle=-1);

bool storylandMoveMissionPedestrian(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,int32_t handle,
    const std::array<float,3>& desired,std::array<float,3>& result);

std::vector<StorylandPedContactObstacle> storylandPedContactObstacles(
    const StorylandPlayTestRuntime& runtime);
StorylandNativeMoveResult storylandMoveAmbientPedestrian(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,size_t index,const StorylandNativePoint& from,
    const StorylandNativePoint& desired,float heading,float seconds,
    const std::vector<StorylandPedContactObstacle>& obstacles);

int storylandRemoteVehicleSlot(const StorylandPlayTestRuntime& runtime);
int storylandVcsRemoteVehicleSlot(const StorylandPlayTestRuntime& runtime);
bool storylandDetonateRemoteVehicle(StorylandPlayTestRuntime& runtime,bool contact=false);

int storylandDispatchFootPolice(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const StorylandNativePaths& paths,std::vector<StorylandNativeTrafficActor>& actors,
    const StorylandRuntimePed& officer,const StorylandNativePoint& player,float cameraYaw,float seconds);

int storylandPopulateFortBaxter(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    std::vector<StorylandNativeTrafficActor>& actors,const StorylandRuntimePed& soldier,
    const StorylandNativePoint& player,float cameraYaw=0);

void storylandUpdatePlayerVehicleStunts(StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& vehicles);
bool storylandFindDeadCharacterPickupPosition(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, const std::vector<StorylandNativeTrafficActor>& cars,
    int32_t handle, std::array<float,3>& result);

void storylandUpdateVehicleWheelContactPose(StorylandRuntimeVehicle& vehicle,
    const StorylandNativeTrafficActor& actor);

inline void storylandUpdateVehicleWheelTravel(StorylandRuntimeVehicle& vehicle,
    const StorylandNativeTrafficActor& actor,float seconds) {
    if(std::isfinite(seconds)&&seconds>0)
        vehicle.scriptTravel.finish({actor.position.x,actor.position.y,actor.position.z});
    const auto previous=vehicle.wheelTravelPosition;
    vehicle.wheelTravelPosition=actor.position;
    const bool initialized=vehicle.wheelTravelInitialized;
    vehicle.wheelTravelInitialized=true;
    if(!initialized||seconds<=0||actor.water||vehicle.craneCarried)return;
    const float dx=actor.position.x-previous.x,dy=actor.position.y-previous.y;
    const float travel=std::hypot(dx,dy);
    if(!std::isfinite(travel)||travel>std::max(5.0f,std::fabs(vehicle.speed)*seconds*2.0f+1.0f))return;
    const float heading=actor.yaw*0.0174532925199433f;
    const float rolling=dx*std::cos(heading)+dy*std::sin(heading);
    const auto axleLocked=[&](bool front) {
        if(!vehicle.tyreForcesApplied||!vehicle.contactsValid)return false;
        const auto& wheels=vehicle.nativeDefinition.wheels;
        if(wheels.empty())return false;
        unsigned contacts=0,locked=0;
        for(size_t index=0;index<wheels.size()&&index<vehicle.wheelTyreStates.size();++index) {
            if(wheels[index].front!=front)continue;
            const size_t line=vehicle.nativeDefinition.twoWheels()?index*2:index;
            const bool inContact=vehicle.wheelContacts[line]||
                (vehicle.nativeDefinition.twoWheels()&&vehicle.wheelContacts[line+1]);
            if(!inContact)continue;
            ++contacts;
            if(vehicle.wheelTyreStates[index]==StorylandVehicleContact::TyreState::Locked)++locked;
        }
        return contacts>0&&locked==contacts;
    };
    const auto axleSupported=[&](bool front) {
        if(!vehicle.nativeDefinition.hasNativeSuspension||!vehicle.contactsValid)
            return vehicle.grounded;
        const auto& wheels=vehicle.nativeDefinition.wheels;
        for(size_t index=0;index<wheels.size()&&index<vehicle.wheelTyreStates.size();++index) {
            if(wheels[index].front!=front)continue;
            const size_t line=vehicle.nativeDefinition.twoWheels()?index*2:index;
            if(vehicle.wheelContacts[line] ||
               (vehicle.nativeDefinition.twoWheels()&&vehicle.wheelContacts[line+1]))return true;
        }
        return false;
    };
    // The visible roll angle must follow road distance at the contacting tyre,
    // not the chassis translation of a bike jumping, wheeling or being dragged.
    if(axleSupported(true)&&!axleLocked(true))
        vehicle.wheelRotation=storylandAdvanceWheelRotation(vehicle.wheelRotation,rolling,vehicle.wheelRadius,1.0f);
    if(axleSupported(false)&&!vehicle.handbrake&&!axleLocked(false))
        vehicle.rearWheelRotation=storylandAdvanceWheelRotation(vehicle.rearWheelRotation,
            rolling,vehicle.nativeDefinition.wheelRadius(true,vehicle.wheelRadius),1.0f);
    storylandUpdateVehicleWheelContactPose(vehicle,actor);
}

void storylandUpdateAmbientPatrols(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld* world, const std::vector<StorylandNativeTrafficActor>& actors);
