#pragma once
#include "src/storyland/storyland_mission_program.h"
#include "src/re/reLCS2/pedestrians/storyland_lcs_wait_animation.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_player_restart.h"
#include "src/re/reVCS/gangs/storyland_empire_entities.h"
#include "src/storyland/storyland_atomic_io.h"
#include "src/leeds/leeds_psp_save.h"
#include <cstring>
#include <memory>
#include <stdexcept>
#include <type_traits>

// Native PC representation of persistent game subsystems. Console card containers are not used.
namespace storyland_save {
// SC73 appends VCS undercover police generation state.
// LGA1 appends LCS hideout vehicle permissions.
// SC75 appends the independent VCS swimming and sprint HUD meter controllers.
constexpr uint32_t Version=75;
constexpr size_t MaximumBytes=32*1024*1024;
struct Archive {
    std::vector<uint8_t>& bytes;bool reading=false;size_t cursor=0,budget=1000000;
    template<class... T> void operator()(T&... values) {(value(values),...);}
    void raw(void* data,size_t count) {
        if(count>MaximumBytes||cursor>MaximumBytes-count)throw std::runtime_error("Save exceeds size limit.");
        if(reading) {
            if(cursor>bytes.size()||count>bytes.size()-cursor)throw std::runtime_error("Truncated save.");
            std::memcpy(data,bytes.data()+cursor,count);
        } else {const auto* p=static_cast<const uint8_t*>(data);bytes.insert(bytes.end(),p,p+count);}
        cursor+=count;
    }
    template<class T> auto value(T& item)->decltype(item.saveState(*this),void()) {item.saveState(*this);}
    template<class T,std::enable_if_t<std::is_arithmetic_v<T>||std::is_enum_v<T>,int> =0>
    void value(T& item) {
        if constexpr(std::is_same_v<T,bool>) {
            uint8_t v=item?1:0;raw(&v,1);if(v>1)throw std::runtime_error("Invalid boolean in save.");item=v!=0;
        } else {
            std::array<uint8_t,sizeof(T)> encoded{};
            if(!reading)std::memcpy(encoded.data(),&item,sizeof(T));
            const uint16_t endian=1;
            const bool littleEndian=*reinterpret_cast<const uint8_t*>(&endian)==1;
            if(!reading&&!littleEndian)std::reverse(encoded.begin(),encoded.end());
            raw(encoded.data(),encoded.size());
            if(reading) {
                if(!littleEndian)std::reverse(encoded.begin(),encoded.end());
                std::memcpy(&item,encoded.data(),sizeof(T));
            }
            if constexpr(std::is_floating_point_v<T>)
                if(!std::isfinite(item))throw std::runtime_error("Invalid numeric value in save.");
        }
    }
    uint32_t count(size_t size) {
        uint32_t n=uint32_t(size);value(n);
        if(n>budget)throw std::runtime_error("Save collection limit exceeded.");budget-=n;return n;
    }
    void value(std::string& s) {auto n=count(s.size());if(reading)s.resize(n);if(n)raw(&s[0],n);}
    void value(std::u16string& text) {
        const auto length=count(text.size());
        if(reading)text.resize(length);
        for(char16_t& character:text) {
            uint16_t code=uint16_t(character);value(code);character=char16_t(code);
        }
    }
    template<class T,size_t N> void value(std::array<T,N>& a) {for(auto& v:a)value(v);}
    template<class T,class U> void value(std::pair<T,U>& p) {value(p.first);value(p.second);}
    template<class T> void value(std::vector<T>& a) {auto n=count(a.size());if(reading)a.resize(n);for(auto& v:a)value(v);}
    template<class T> void value(std::deque<T>& a) {auto n=count(a.size());if(reading)a.resize(n);for(auto& v:a)value(v);}
    template<class K,class V> void value(std::map<K,V>& a) {
        auto n=count(a.size());if(reading) {a.clear();for(uint32_t i=0;i<n;++i) {K k{};V v{};value(k);value(v);if(!a.emplace(std::move(k),std::move(v)).second)throw std::runtime_error("Duplicate save key.");}}
        else for(auto& entry:a){K k=entry.first;value(k);value(entry.second);}
    }
    template<class K> void value(std::set<K>& a) {
        auto n=count(a.size());if(reading) {a.clear();for(uint32_t i=0;i<n;++i){K k{};value(k);if(!a.insert(std::move(k)).second)throw std::runtime_error("Duplicate save entry.");}}
        else for(auto k:a)value(k);
    }
    void value(StorylandMissionEnvironment::Character::WeaponState& v) {(*this)(v.slot,v.model,v.ammunition,v.owned);}
    void value(StorylandAnimationFade& v) {(*this)(v.weight,v.rate,v.startedAt,v.present);}
    void value(StorylandCrouchState& v) {(*this)(v.requested,v.active,v.deadline,v.startedAt,v.poseFade,v.movementState,v.poseAnimation);}
    void value(StorylandMissionEnvironment::Target& v) {(*this)(v.kind,v.handle);}
    void value(StorylandRoadChange& v){(*this)(v.bounds,v.vehicle,v.enabled);}
    void value(StorylandNativePoint& v){(*this)(v.x,v.y,v.z);}
    void value(StorylandWeaponInventory& v){(*this)(v.ammunition,v.magazine,v.cooldown,v.reloading,v.owned);}
    void value(StorylandWeaponPickup& v){(*this)(v.position,v.weapon,v.ammunition,v.respawn);}
    void value(StorylandPlayTestRuntime::GeneratorState& v){(*this)(v.spawned,v.waitingForDeparture,v.nextSpawnAt);}
};
inline uint64_t hash(const std::vector<uint8_t>& bytes) {
    uint64_t h=14695981039346656037ull;for(auto b:bytes){h^=b;h*=1099511628211ull;}return h;
}
struct VehicleTransmission {
    uint32_t gear=1;
    float engineSpeed=0,inertiaFactor=1;
    template<class A> void saveState(A& archive) {archive(gear,engineSpeed,inertiaFactor);}
};
inline bool validVcsPlaneControlAxes(const std::array<float,3>& axes) {
    return std::all_of(axes.begin(),axes.end(),[](float value){return std::isfinite(value);})&&
        std::fabs(axes[0])<=1.5f&&std::fabs(axes[1])<=1.0f&&std::fabs(axes[2])<=1.0f;
}
struct Vehicle {
    bool vcsViceUnit=false;
    uint8_t vcsViceCrewSequence=0;
    struct Crew {
        int32_t model=-1;
        uint8_t seat=0;
        float health=100,armour=0;
        template<class A> void saveState(A& a) {a(model,seat,health,armour);}
    } crew;
    std::array<float,3> vcsPlaneControlAxes{};
    std::array<float,4> vcsJetSkiAxes{};
    StorylandForkliftState forklift;
    float waterCannonYaw=0,waterCannonPitch=.05f;
    int32_t creationClass=-1; // SC28; keep the original vehicle record unchanged.
    VehicleTransmission transmission;
    bool craneCarried=false; // Version 15 extension; retain the original VHLS layout.
    StorylandLcsCraneMatrix craneFrame{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    uint8_t damageProofs=0; // Version 5 subsystem; retain the original VHLS layout.
    // Rebuilt physical state belongs to SC20, outside the original VHLS bytes.
    bool positionFrozen=false,collisionEnabled=true,loadCollision=true,onlyDamagedByPlayer=false,heavy=false;
    bool canBeDamaged=true,visible=true,siren=false,wrecked=false;
    int32_t doorLock=0;
    uint8_t renderAlpha=255;
    bool contactsValid=false,bodyContact=false,suspensionAnchorValid=false;
    std::array<bool,4> wheelContacts{};
    std::array<float,4> wheelSpringRatios{1,1,1,1},wheelLineFractions{1,1,1,1};
    std::array<float,4> wheelSpringImpulses{},wheelDampingImpulses{};
    float suspensionVelocity=0,springRollVelocity=0,springPitchVelocity=0,suspensionAnchorZ=0;
    float rearWheelRotation=0,roadPitch=0,roadRoll=0;
    // SC22 retains physical state for both script-owned and ambient vehicles.
    bool watertight = false;
    bool strong = false;
    bool upsideDownDamageDisabled = false;
    bool canBurstTyres = true;
    bool visiblyDamaged = false;
    bool boatAnchored = false;
    bool clumpVisible = true;
    float tractionMultiplier = 1;
    std::array<uint8_t, 4> tyreStatus{};
    std::array<uint8_t, 6> doorDamage{};
    float bonnetOpenRatio = 0;
    float bootOpenRatio = 0;
    float bootAngularVelocity = 0;
    std::array<float, 3> angularVelocity{};
    int32_t colorSet = -1;
    int32_t scriptColorSet = -1;
    bool aiActive = false;
    int8_t drivingStyle = 0, aiMission = 0;
    int32_t aiTargetVehicle = -1;
    uint32_t aiMissionStartedAt = 0;
    uint16_t routeSeed = 0, alarmMilliseconds = 0;
    bool avoidLevelTransitions = false, fastLane = true, lanePolicySet = false, convoy = false;
    uint8_t straightLineDistance = 0;
    float massMultiplier = 1, turnMassMultiplier = 1;
    std::array<uint8_t,7> panelDamage{};
    std::array<uint8_t,4> lightDamage{};
    float heliOrientation = -1, rotorSpeed = 0, rotorAngle = 0;
    bool heliStabiliser = false, heliCrashing = false;
    bool taxiLight = false, bikeStandingOverride = false;
    uint32_t model=0;
    int32_t garage=-1;
    uint8_t garageOwner=255;
    bool vcsGarageStorable=false;
    StorylandNativeTrafficActor actor;
    float health=1000,speed=0,lateralSpeed=0,verticalSpeed=0,yawVelocity=0;
    int32_t primaryColor=-1,secondaryColor=-1,radioStation=-1,scriptHandle=-1,generator=-1;
    bool parked=true,hasDriver=false,burning=false,headlightsOn=false,grounded=true;
    std::array<float,4> doors{},wheels{};
    float suspension=0,bodyRoll=0,bodyPitch=0,wheelRotation=0,steeringAngle=0;
    template<class A> void saveState(A& a) {
        a(model,garage,actor.node,actor.nextNode,actor.previousNode,actor.routeSeed,actor.distance,
          actor.groundOffset,actor.laneOffset,actor.lane,actor.blockedSeconds,actor.crossingCommitted,
          actor.vehicle,actor.water,actor.position,actor.yaw,health,speed,lateralSpeed,verticalSpeed,yawVelocity,
          primaryColor,secondaryColor,radioStation,scriptHandle,generator,parked,hasDriver,burning,headlightsOn,
          grounded,doors,wheels,suspension,bodyRoll,bodyPitch,wheelRotation,steeringAngle);
    }
    template<class Source, class Target> static void copyThirdBatch(const Source& from, Target& to) {
        to.aiActive = from.aiActive;
        to.drivingStyle = from.drivingStyle;
        to.aiMission = from.aiMission;
        to.aiTargetVehicle = from.aiTargetVehicle;
        to.aiMissionStartedAt = from.aiMissionStartedAt;
        to.routeSeed = from.routeSeed;
        to.alarmMilliseconds = from.alarmMilliseconds;
        to.avoidLevelTransitions = from.avoidLevelTransitions;
        to.fastLane = from.fastLane;
        to.lanePolicySet = from.lanePolicySet;
        to.convoy = from.convoy;
        to.straightLineDistance = from.straightLineDistance;
        to.massMultiplier = from.massMultiplier;
        to.turnMassMultiplier = from.turnMassMultiplier;
        to.panelDamage = from.panelDamage;
        to.lightDamage = from.lightDamage;
        to.heliOrientation = from.heliOrientation;
        to.rotorSpeed = from.rotorSpeed;
        to.rotorAngle = from.rotorAngle;
        to.heliStabiliser = from.heliStabiliser;
        to.heliCrashing = from.heliCrashing;
    }
    template<class Source, class Target> static void copyFourthBatch(const Source& from, Target& to) {
        to.taxiLight=from.taxiLight;
        to.bikeStandingOverride=from.bikeStandingOverride;
        to.vcsGarageStorable=from.vcsGarageStorable;
    }
    bool validVcsPlaneControlsFor(const StorylandVehicleDefinition& definition) const {
        return validVcsPlaneControlAxes(vcsPlaneControlAxes)&&
            (vcsPlaneControlAxes==std::array<float,3>{}||definition.vcsHovercraft());
    }
    bool validVcsJetSkiAxesFor(const StorylandVehicleDefinition& definition) const {
        return storylandVcsJetSkiAxesValid(vcsJetSkiAxes)&&
            (vcsJetSkiAxes==std::array<float,4>{}||definition.vcsJetSki());
    }
    static Vehicle capture(const StorylandNativeTrafficActor& actor,const StorylandRuntimeVehicle& car) {
        if(car.nativeDefinition.vcsJetSki()&&car.vcsJetSkiPose.initialised&&
            car.vcsJetSkiPose.driver>0)
            throw std::runtime_error("An active NPC JetSki occupant association cannot be captured.");
        if(!storylandVcsJetSkiAxesValid(car.vcsJetSkiAxes)||
            (car.vcsJetSkiAxes!=std::array<float,4>{}&&!car.nativeDefinition.vcsJetSki()))
            throw std::runtime_error("Native JetSki axes do not match the captured vehicle model.");
        Vehicle v;v.model=car.nativeDefinition.modelId;v.actor=actor;v.damageProofs=car.damageProofs;v.canBeDamaged=car.canBeDamaged;
        v.vcsViceUnit=car.vcsViceUnit;v.vcsViceCrewSequence=car.vcsViceCrewSequence;
        v.creationClass=car.nativeDefinition.vehicleClass;
        v.vcsPlaneControlAxes=car.vcsPlaneControlAxes;
        v.vcsJetSkiAxes=car.vcsJetSkiAxes;
        v.forklift=car.forklift;
        v.waterCannonYaw=car.waterCannonYaw;v.waterCannonPitch=car.waterCannonPitch;
        v.transmission={car.transmission.gear,car.transmission.engineSpeed,car.transmission.inertiaFactor};
        v.craneCarried=car.craneCarried;v.craneFrame=car.craneFrame;
        v.onlyDamagedByPlayer=car.onlyDamagedByPlayer;
        v.health=car.health;v.speed=car.speed;v.lateralSpeed=car.lateralSpeed;v.verticalSpeed=car.verticalSpeed;
        v.yawVelocity=car.yawVelocity;v.primaryColor=car.primaryColor;v.secondaryColor=car.secondaryColor;
        v.radioStation=car.radioStation;v.scriptHandle=car.scriptHandle;v.generator=car.generator;
        v.garage=car.savedGarage;v.garageOwner=car.garageOwner;
        v.parked=car.parked;v.hasDriver=car.hasDriver;v.burning=car.burning;v.headlightsOn=car.headlightsOn;
        v.grounded=car.grounded;v.doors=car.doorOpenRatios;v.wheels=car.wheelPositions;
        v.suspension=car.suspension;v.bodyRoll=car.bodyRoll;v.bodyPitch=car.bodyPitch;
        v.wheelRotation=car.wheelRotation;v.steeringAngle=car.steeringAngle;
        v.positionFrozen=car.positionFrozen;v.collisionEnabled=car.collisionEnabled;v.loadCollision=car.loadCollision;
        v.onlyDamagedByPlayer=car.onlyDamagedByPlayer;v.heavy=car.heavy;v.renderAlpha=car.renderAlpha;
        v.canBeDamaged=car.canBeDamaged;v.visible=car.visible;v.siren=car.siren;v.wrecked=car.wrecked;v.doorLock=car.doorLock;
        v.contactsValid=car.contactsValid;v.bodyContact=car.bodyContact;v.suspensionAnchorValid=car.suspensionAnchorValid;
        v.wheelContacts=car.wheelContacts;v.wheelSpringRatios=car.wheelSpringRatios;v.wheelLineFractions=car.wheelLineFractions;
        v.wheelSpringImpulses=car.wheelSpringImpulses;v.wheelDampingImpulses=car.wheelDampingImpulses;
        v.suspensionVelocity=car.suspensionVelocity;v.springRollVelocity=car.springRollVelocity;
        v.springPitchVelocity=car.springPitchVelocity;v.suspensionAnchorZ=car.suspensionAnchorZ;
        v.rearWheelRotation=car.rearWheelRotation;v.roadPitch=car.roadPitch;v.roadRoll=car.roadRoll;
        v.watertight = car.watertight;
        v.strong = car.strong;
        v.upsideDownDamageDisabled = car.upsideDownDamageDisabled;
        v.canBurstTyres = car.canBurstTyres;
        v.visiblyDamaged = car.visiblyDamaged;
        v.boatAnchored = car.boatAnchored;
        v.clumpVisible = car.clumpVisible;
        v.tractionMultiplier = car.tractionMultiplier;
        v.tyreStatus = car.tyreStatus;
        v.doorDamage = car.doorDamage;
        v.bonnetOpenRatio = car.bonnetOpenRatio;
        v.bootOpenRatio = car.bootOpenRatio;
        v.bootAngularVelocity = car.bootAngularVelocity;
        v.angularVelocity = car.angularVelocity;
        v.colorSet = car.colorSet;
        v.scriptColorSet = car.scriptColorSet;
        copyThirdBatch(car, v);
        copyFourthBatch(car, v);
        // Service incidents are transient rather than mission-owned. A save
        // restores the vehicle safely parked without its unsaved route/siren.
        if(car.serviceAutopilot) {
            v.siren=false;v.aiActive=false;v.aiMission=0;v.parked=true;
            v.speed=0;v.lateralSpeed=0;v.verticalSpeed=0;
            v.actor.destinationRoute.clear();v.actor.destinationRouteValid=false;
            v.actor.destinationRouteFinished=false;
        }
        return v;
    }
    void restore(StorylandRuntimeVehicle& car) const {
        car.vcsViceUnit=vcsViceUnit;car.vcsViceCrewSequence=vcsViceCrewSequence;
        car.pendingCrewModel=crew.model;car.pendingCrewSeat=crew.seat;
        car.pendingCrewHealth=crew.health;car.pendingCrewArmour=crew.armour;
        car.vcsPlaneControlAxes=vcsPlaneControlAxes;
        car.vcsJetSkiAxes=vcsJetSkiAxes;car.vcsJetSkiPose={};
        car.vcsJetSkiBrake=car.nativeDefinition.vcsJetSki()?1.0f:0.0f;
        car.vcsJetSkiContactImpulse=0;car.vcsJetSkiContactNormal={};
        car.waterCannonYaw=waterCannonYaw;car.waterCannonPitch=waterCannonPitch;
        car.forklift=forklift;
        car.forklift.moving=false;
        if(creationClass>=0)car.nativeDefinition.vehicleClass=creationClass;
        car.transmission={};
        car.transmission.gear=transmission.gear;
        car.transmission.engineSpeed=transmission.engineSpeed;
        car.transmission.inertiaFactor=transmission.inertiaFactor;
        car.craneCarried=craneCarried;car.craneFrame=craneFrame;
        car.damageProofs=damageProofs;
        car.canBeDamaged=canBeDamaged;
        car.onlyDamagedByPlayer=onlyDamagedByPlayer;
        car.savedGarage=garage;
        car.garageOwner=garageOwner==255&&garage>=0?0:garageOwner;
        car.health=health;car.speed=speed;car.lateralSpeed=lateralSpeed;car.verticalSpeed=verticalSpeed;
        car.yawVelocity=yawVelocity;car.primaryColor=primaryColor;car.secondaryColor=secondaryColor;
        car.radioStation=radioStation;car.scriptHandle=scriptHandle;car.generator=generator;
        car.parked=parked;car.hasDriver=hasDriver;car.burning=burning;car.headlightsOn=headlightsOn;
        car.grounded=grounded;car.doorOpenRatios=doors;car.wheelPositions=wheels;
        car.suspension=suspension;car.bodyRoll=bodyRoll;car.bodyPitch=bodyPitch;
        car.wheelRotation=wheelRotation;car.steeringAngle=steeringAngle;
        car.positionFrozen=positionFrozen;car.collisionEnabled=collisionEnabled;car.loadCollision=loadCollision;
        car.onlyDamagedByPlayer=onlyDamagedByPlayer;car.heavy=heavy;car.renderAlpha=renderAlpha;
        car.canBeDamaged=canBeDamaged;car.visible=visible;car.siren=siren;car.wrecked=wrecked;car.doorLock=doorLock;
        car.contactsValid=contactsValid;car.bodyContact=bodyContact;car.suspensionAnchorValid=suspensionAnchorValid;
        car.wheelContacts=wheelContacts;car.wheelSpringRatios=wheelSpringRatios;car.wheelLineFractions=wheelLineFractions;
        car.wheelSpringImpulses=wheelSpringImpulses;car.wheelDampingImpulses=wheelDampingImpulses;
        car.suspensionVelocity=suspensionVelocity;car.springRollVelocity=springRollVelocity;
        car.springPitchVelocity=springPitchVelocity;car.suspensionAnchorZ=suspensionAnchorZ;
        car.rearWheelRotation=rearWheelRotation;car.roadPitch=roadPitch;car.roadRoll=roadRoll;
        car.watertight = watertight;
        car.strong = strong;
        car.upsideDownDamageDisabled = upsideDownDamageDisabled;
        car.canBurstTyres = canBurstTyres;
        car.visiblyDamaged = visiblyDamaged;
        car.boatAnchored = boatAnchored;
        car.clumpVisible = clumpVisible;
        car.tractionMultiplier = tractionMultiplier;
        car.tyreStatus = tyreStatus;
        car.doorDamage = doorDamage;
        car.bonnetOpenRatio = bonnetOpenRatio;
        car.bootOpenRatio = bootOpenRatio;
        car.bootAngularVelocity = bootAngularVelocity;
        car.angularVelocity = angularVelocity;
        car.colorSet = colorSet;
        car.scriptColorSet = scriptColorSet;
        copyThirdBatch(*this, car);
        copyFourthBatch(*this, car);
        if (garageOwner == 255 && garage >= 0) {
            car.visible = false; car.collisionEnabled = false; car.positionFrozen = true;
        }
        car.mass = car.nativeDefinition.mass * car.massMultiplier * (car.heavy ? 3.0f : 1.0f);
        car.lastCollidedSlot = -1; // Contacts from the previous live frame are transient.
    }
};
inline void captureServiceCrew(Vehicle& saved,const StorylandRuntimeVehicle& car,
    const StorylandPlayTestRuntime& runtime) {
    if(car.pendingCrewModel>=0) {
        saved.crew={car.pendingCrewModel,car.pendingCrewSeat,car.pendingCrewHealth,car.pendingCrewArmour};
        return;
    }
    if(!car.serviceCrewIdentity)return;
    for(const auto& ped:runtime.pedestrians) {
        if(ped.serviceVehicleIdentity!=car.serviceCrewIdentity||ped.scriptHandle>=0)continue;
        if(saved.crew.model>=0)throw std::runtime_error("Cannot save multiple ambient crew in one vehicle yet.");
        saved.crew={int32_t(ped.stats.modelId),ped.servicePassengerSeat,ped.health,ped.armour};
    }
}
inline bool garageContains(const StorylandMissionEnvironment::Garage& garage,const StorylandNativePoint& p) {
    const auto& b=garage.bounds;
    if(p.z<std::min(b[2],b[7])||p.z>std::max(b[2],b[7]))return false;
    const float ax=b[3]-b[0],ay=b[4]-b[1],bx=b[5]-b[0],by=b[6]-b[1];
    const float det=ax*by-ay*bx;if(std::fabs(det)<0.0001f)return false;
    const float x=p.x-b[0],y=p.y-b[1],u=(x*by-y*bx)/det,v=(ax*y-ay*x)/det;
    return u>=0&&u<=1&&v>=0&&v<=1;
}
struct State {
    uint32_t loadedVersion = 0; // Decoder metadata; never part of a saved subsystem.
    StorylandMissionEnvironment environment;
    StorylandMissionVm mission;
    StorylandMissionProgram program;
    std::vector<StorylandWeaponInventory> inventory,confiscated;
    std::vector<int32_t> weaponIds;
    std::vector<StorylandWeaponPickup> dropped;
    std::vector<Vehicle> vehicles;
    std::array<StorylandBetaWeaponGrant,size_t(StorylandBetaWeapon::Count)> betaWeapons{};
    StorylandAdrenalineState adrenaline;
    StorylandKillFrenzy killFrenzy;
    StorylandRestartTaxiState restartTaxi;
    uint32_t populationRandom=0x4C435350u;
    std::map<int32_t,StorylandPlayTestRuntime::GeneratorState> generatorStates;
    bool betaVehicleBombsEnabled=false;
    size_t equipped=0;
    float elapsed=0,fortBaxterHeat=0;
    bool buyback=false,armyFatigues=true;
    StorylandNativePoint buybackPosition;
    template<class A> void saveState(A& a) {
        a(environment,mission,program,inventory,confiscated,weaponIds,dropped,equipped,elapsed,
          fortBaxterHeat,buyback,armyFatigues,buybackPosition);
    }
};
struct LegacyScriptFire {
    std::array<float,3> position{};
    int32_t attachedKind=0,attachedHandle=-1;
    bool active=true;
    template<class A> void saveState(A& archive) {
        archive(position,attachedKind,attachedHandle,active);
    }
};
struct MissionCharacterTask {
    bool completed=false;
    float radius=0.5f;
    int32_t target=-1;
    bool frozen=false;
    bool onlyDamagedByPlayer=false;
    bool suffersCriticalHits=true,canBeShotInVehicle=true,cantBeDraggedOut=false;
    uint16_t money=0;
    bool moneyGivenByScript=false,deathDropsCreated=false;
    template<class A> void saveState(A& archive) {archive(completed,radius,target,frozen,onlyDamagedByPlayer,suffersCriticalHits,canBeShotInVehicle,cantBeDraggedOut,money,moneyGivenByScript,deathDropsCreated);}
};
struct MissionVehicleControl {
    int32_t mission=-1;
    bool routePending=false,anchored=true;
    float anchorHeading=10000.0f;
    int32_t doorLock=1;
    bool positionFrozen=false;
    template<class A> void saveState(A& archive) {archive(mission,routePending,anchored,anchorHeading,doorLock,positionFrozen);}
};
struct MissionCharacterMotion {
    float speed=0;
    double distance=0;
    uint8_t movementState=0;
    uint32_t deathStartedAt=0;
    bool deathRecorded=false;
    template<class A> void saveState(A& archive) {archive(speed,distance,movementState,deathStartedAt,deathRecorded);}
};
template<class... Values>
void subsystem(Archive& archive,uint32_t expectedTag,Values&... values) {
    uint32_t tag=expectedTag;
    std::vector<uint8_t> block;
    if(!archive.reading) {
        Archive writer{block};writer(values...);
    }
    uint32_t length=uint32_t(block.size());
    uint64_t checksum=hash(block);
    archive(tag,length,checksum);
    if(tag!=expectedTag||length>MaximumBytes)throw std::runtime_error("Invalid save subsystem.");
    if(archive.reading)block.resize(length);
    if(length)archive.raw(block.data(),length);
    if(archive.reading) {
        if(hash(block)!=checksum)throw std::runtime_error("Save subsystem checksum failed.");
        Archive reader{block,true};reader(values...);
        if(reader.cursor!=block.size())throw std::runtime_error("Invalid save subsystem length.");
    }
    uint8_t padding=0;
    for(uint32_t i=length;i%4;++i) {
        archive(padding);
        if(padding!=0)throw std::runtime_error("Invalid save alignment.");
    }
}
struct RecoveryExtensions {
    State& state;
    uint32_t version=0;
    template<class A> void saveState(A& a) {
        auto& world=state.environment;
        uint32_t objects=uint32_t(world.objects.size());a(objects);
        if(objects!=world.objects.size()||objects>4096)throw std::runtime_error("Invalid object extension count.");
        std::set<int32_t> objectHandles;
        for(uint32_t index=0;index<objects;++index) {
            int32_t handle=world.objects[index].handle;a(handle);
            auto found=std::find_if(world.objects.begin(),world.objects.end(),[&](const auto& o){return o.handle==handle;});
            if(found==world.objects.end()||!objectHandles.insert(handle).second)throw std::runtime_error("Invalid object extension reference.");
            auto& o=*found;auto& d=o.damage;
            a(o.velocity,o.rotationVelocity,o.attachmentOffset,o.dynamic,o.gravity,o.collisionEnabled,o.positionFrozen,
                o.recordsCollisions,o.collided,o.inWater,o.drawLast,o.area,o.attachedVehicle,
                o.attachedCharacter,o.attachedBone,o.attachmentRotation,
                d.propertiesLoaded,d.mass,d.turnMass,d.airResistance,d.elasticity,d.buoyancy,d.uprootLimit,d.multiplier,
                d.damageEffect,d.collisionResponse,d.effect1,d.effect2,d.proofs,d.cameraAvoid,d.disableMoveForce,
                d.disableTurnForce,d.health,d.lastWeapon,d.visible,d.renderDamaged);
            if(d.mass<=0||d.turnMass<=0||d.airResistance<0||d.airResistance>1||d.proofs>31||o.area<0||o.area>255||
                (o.attachedVehicle>=0&&!world.missionVehicles.count(o.attachedVehicle))||
                (o.attachedCharacter>0&&!world.missionCharacters.count(o.attachedCharacter))||
                (o.attachedVehicle>=0&&o.attachedCharacter>=0)||o.attachedCharacter<-1||
                o.attachedBone<-128||o.attachedBone>127)throw std::runtime_error("Invalid object extension state.");
        }
        uint32_t characters=uint32_t(world.missionCharacters.size());a(characters);
        if(characters!=world.missionCharacters.size()||characters>4096)throw std::runtime_error("Invalid character extension count.");
        std::set<int32_t> characterHandles;auto character=world.missionCharacters.begin();
        for(uint32_t index=0;index<characters;++index,++character) {
            int32_t handle=character->first;a(handle);
            const auto found=world.missionCharacters.find(handle);
            if(found==world.missionCharacters.end()||!characterHandles.insert(handle).second)throw std::runtime_error("Invalid character extension reference.");
            auto& c=found->second;
            a(c.maximumHealth,c.attackTimers.shootOverride,c.attackTimers.attackOverride,
                c.attackTimers.shootDeadline,c.attackTimers.attackDeadline,
                c.armour,c.pedStat,c.currentWeapon,c.lastWeaponDamage,c.lastDamageSource,c.weapons,c.weaponsChanged,c.weaponSelectionChanged,
                c.onlyDamagedByPlayer,c.criticalHits,c.bleeding,c.heedThreats,c.searchThreats,c.ignoreThreatsBehindObjects,
                c.crouchWhenThreatened,c.allowFrontPassenger,c.keepObjectiveOnRelease,c.loadCollision,c.weaponDropped,
                c.moneyDropped,c.playerKillCounted,c.walletInitialized,c.walletForced,c.provokedByPlayer,c.creationSource,
                c.walletMoney,c.renderAlpha,c.speechProfile,c.fadeOut,
                c.objectiveTarget,c.leader,c.formation,c.objectiveRadius,c.shotAt,c.hasShot,c.seekNodes,c.seekNode,c.seekWaitUntil,
                c.seekRoute,c.seekRouteCursor,c.seekDestination,c.waitState,c.waitDeadline,c.waitVersionB,c.crouch);
            if(c.armour<0||c.weapons.size()>64||c.seekRoute.size()>65536||c.seekRouteCursor>c.seekRoute.size()||
                c.objectiveRadius<0||
                (c.waitState!=0&&c.waitState!=12&&c.waitState!=13&&c.waitState!=17&&c.waitState!=18&&c.waitState!=19&&c.waitState!=23&&c.waitState!=24))throw std::runtime_error("Invalid character extension state.");
            for(const auto& weapon:c.weapons)if(weapon.first<0||weapon.first>255||
                (!state.mission.vcs&&weapon.second.ammunition<0))
                throw std::runtime_error("Invalid saved character weapon.");
            validateCrouch(c.crouch);
        }
        uint32_t vehicles=uint32_t(world.missionVehicles.size());a(vehicles);
        if(vehicles!=world.missionVehicles.size()||vehicles>4096)throw std::runtime_error("Invalid vehicle extension count.");
        std::set<int32_t> vehicleHandles;auto vehicle=world.missionVehicles.begin();
        for(uint32_t index=0;index<vehicles;++index,++vehicle) {
            int32_t handle=vehicle->first;a(handle);
            const auto found=world.missionVehicles.find(handle);
            if(found==world.missionVehicles.end()||!vehicleHandles.insert(handle).second)throw std::runtime_error("Invalid vehicle extension reference.");
            auto& c=found->second;
            a(c.maximumHealth,c.headingChangedByScript,c.velocityChangedByScript,c.positionFrozen,c.collisionEnabled,c.loadCollision,
                c.onlyDamagedByPlayer,c.canBeDamaged,c.siren,c.taxiLight,c.canRespray,c.fadeOut,c.doorLock,c.lastWeaponDamage,
                c.bombType,c.renderAlpha,c.velocity,c.visible,c.extinguishRequested,c.wrecked,c.lastDamageSource);
        }
        uint32_t physicalVehicles=uint32_t(state.vehicles.size());a(physicalVehicles);
        if(physicalVehicles!=state.vehicles.size()||physicalVehicles>70)throw std::runtime_error("Invalid physical vehicle extension count.");
        for(auto& c:state.vehicles) {
            a(c.positionFrozen,c.collisionEnabled,c.loadCollision,c.onlyDamagedByPlayer,c.heavy,c.renderAlpha,
                c.canBeDamaged,c.visible,c.siren,c.wrecked,c.doorLock,
                c.contactsValid,c.bodyContact,c.suspensionAnchorValid,c.wheelContacts,c.wheelSpringRatios,
                c.wheelLineFractions,c.wheelSpringImpulses,c.wheelDampingImpulses,c.suspensionVelocity,
                c.springRollVelocity,c.springPitchVelocity,c.suspensionAnchorZ,c.rearWheelRotation,c.roadPitch,c.roadRoll);
            for(float fraction:c.wheelLineFractions)if(fraction<0||fraction>1)throw std::runtime_error("Invalid saved wheel contact fraction.");
        }
        a(world.missionCleanupObjects,world.missionCleanupCharacters,world.missionCleanupVehicles,world.collectedPickupHandles,
            world.pathSearchMarkedNodes,
            world.onscreenTimer,world.onscreenCounters,world.onscreenTimerFrozen,
            world.counterColorPending,world.counterBackgroundPending,world.storedClockHour,world.clockStored,
            world.playerAllowFrontPassenger,world.playerRenderAlpha,world.playerSpeechProfile,world.playerCrouch,
            world.playerMaximumHealth,world.playerAttackTimers.shootOverride,world.playerAttackTimers.attackOverride,
            world.playerAttackTimers.shootDeadline,world.playerAttackTimers.attackDeadline,
            world.playerWaitState,world.playerWaitDeadline,world.playerWaitVersionB,
            world.playerPassengerSeat,world.playerVehicleTransition,world.playerExitVehicleRequested,
            world.targettableObjects,world.playerTarget,world.characterTargets,world.playerDamageSource,world.playerLastWeaponDamage,
            world.scriptCoronas,world.nextScriptCoronaId,world.scriptFires,world.scriptExplosions,world.nativeScriptTimeStep,
            world.nativeRandomState,world.nativeTimeScale,
            world.playerVehicleStunt.kind,world.playerVehicleStunt.startedAt,world.playerVehicleStunt.distance,
            world.playerVehicleStunt.milliseconds,world.playerVehicleStunt.distances,
            world.playerVehicleStunt.activeMilliseconds,world.playerVehicleStunt.activeDistances,
            world.playerVehicleStunt.graceMilliseconds,world.playerVehicleStunt.vehicle,
            world.playerVehicleStunt.previousPosition,world.playerVehicleStunt.previousPositionValid);
        validateCrouch(world.playerCrouch);
        for(int32_t handle:world.missionCleanupObjects)if(!objectHandles.count(handle))throw std::runtime_error("Invalid mission object ownership.");
        for(int32_t handle:world.missionCleanupCharacters)if(!world.missionCharacters.count(handle))throw std::runtime_error("Invalid mission character ownership.");
        for(int32_t handle:world.missionCleanupVehicles)if(!world.missionVehicles.count(handle))throw std::runtime_error("Invalid mission vehicle ownership.");
        if(world.collectedPickupHandles.size()>20)throw std::runtime_error("Invalid collected-pickup queue.");
        for(const auto& kind:world.pathSearchMarkedNodes)for(uint32_t index:kind)
            if(index>=16384)throw std::runtime_error("Invalid native path search mark.");
        if(world.onscreenTimer.globalSlot<-1||world.onscreenTimer.globalSlot>=int32_t(state.mission.globals.size())||
            world.onscreenTimer.label.size()>10||world.storedClockHour<0||world.storedClockHour>=24)
            throw std::runtime_error("Invalid saved onscreen timer.");
        for(const auto& counter:world.onscreenCounters)if(counter.globalSlot<-1||
            counter.globalSlot>=int32_t(state.mission.globals.size())||counter.label.size()>10||counter.secondaryLabel.size()>10)
            throw std::runtime_error("Invalid saved onscreen counter.");
        if(world.playerWaitState!=0&&world.playerWaitState!=12&&world.playerWaitState!=13&&world.playerWaitState!=17&&world.playerWaitState!=18&&world.playerWaitState!=19&&world.playerWaitState!=23&&world.playerWaitState!=24)
            throw std::runtime_error("Invalid saved player wait state.");
        if(world.playerPassengerSeat<-1||world.playerPassengerSeat>=8)
            throw std::runtime_error("Invalid saved player passenger seat.");
        if(world.nativeRandomState>uint32_t(INT32_MAX)||world.nativeTimeScale<0||world.nativeScriptTimeStep<0||world.scriptFires.size()>40||
            world.scriptCoronas.size()>4096||world.nextScriptCoronaId<0||world.characterTargets.size()>world.missionCharacters.size())
            throw std::runtime_error("Invalid restored world state.");
        const auto validateTarget=[&](const StorylandMissionEnvironment::Target& target) {
            using K=StorylandMissionEnvironment::TargetKind;
            if(target.kind==K::None) {if(target.handle!=-1)throw std::runtime_error("Invalid empty target.");}
            else if(target.kind==K::Object) {if(!objectHandles.count(target.handle))throw std::runtime_error("Invalid object target.");}
            else if(target.kind==K::Character) {if(target.handle!=0&&!world.missionCharacters.count(target.handle))throw std::runtime_error("Invalid character target.");}
            else throw std::runtime_error("Invalid target kind.");
        };
        validateTarget(world.playerTarget);
        for(const auto& target:world.characterTargets) {
            if(!world.missionCharacters.count(target.first))throw std::runtime_error("Invalid target owner.");
            validateTarget(target.second);
        }
        for(const auto& entry:world.scriptCoronas)if(entry.first<0||entry.first>=world.nextScriptCoronaId||
            entry.second.type<0||entry.second.type>8||entry.second.flareType<0||entry.second.flareType>2)
            throw std::runtime_error("Invalid saved corona.");
        const auto validateSource=[&](const StorylandMissionEnvironment::DamageSource& source) {
            using Kind=StorylandMissionEnvironment::DamageSourceKind;
            const bool valid=source.kind==Kind::None?source.handle==-1:
                source.kind==Kind::Character?(source.handle==0||world.missionCharacters.count(source.handle)!=0):
                source.kind==Kind::Vehicle?world.missionVehicles.count(source.handle)!=0:
                source.kind==Kind::Object?objectHandles.count(source.handle)!=0:false;
            if(!valid)throw std::runtime_error("Invalid saved damage source.");
        };
        validateSource(world.playerDamageSource);
        for(const auto& entry:world.missionCharacters)validateSource(entry.second.lastDamageSource);
        for(const auto& entry:world.missionVehicles)validateSource(entry.second.lastDamageSource);
        for(const auto& entry:world.scriptFires) {
            const auto& fire=entry.second;
            validateSource(fire.source);
            if(entry.first<0||entry.first>=40||fire.attachedKind<0||fire.attachedKind>2||
                fire.strength>1||(fire.active&&fire.strength<0)||fire.scale<0||
                (fire.active&&fire.attachedKind==1&&fire.attachedHandle!=0&&!world.missionCharacters.count(fire.attachedHandle))||
                (fire.active&&fire.attachedKind==2&&!world.missionVehicles.count(fire.attachedHandle)))
                throw std::runtime_error("Invalid script fire attachment.");
        }
        for(const auto& explosion:world.scriptExplosions) {
            validateSource(explosion.source);
            if(explosion.type<0||explosion.type>11||explosion.lifetime>4250||!explosion.lifetime)
                throw std::runtime_error("Invalid saved explosion pool.");
        }
        auto foreground=state.mission.outcomeState();auto background=state.program.resultStates();
        a(foreground,background,world.missionResult,world.lastMissionPassedName,
            world.subtitleQueue,world.bigMessageQueues,world.briefMessages,world.messageStyle,world.messageStringKey,
            world.helpMessageNumbers,world.missionPassedTune,world.missionPassedTuneSerial,
            world.missionTuneLoop,world.musicDoesFade,
            world.missionAudioSubtitleEnabled,world.frontendAudioEnabled,world.cameraShake,world.manualAim,
            world.cameraFocus,world.cameraVehicleZoom,
            world.wantsHud,world.flashingHudObject,world.cameraControlEnabled,world.cameraFrontPlayerRequested,world.hudFlashFrames,
            world.scriptPanels,world.nextScriptPanelItem);
        if(!state.mission.restoreOutcomeState(foreground)||!state.program.restoreResultStates(background))
            throw std::runtime_error("Invalid saved mission outcome.");
        if(world.missionPassedTune<-1||world.missionPassedTune>111)
            throw std::runtime_error("Invalid saved music stream.");
        const auto& result=world.missionResult;
        if(unsigned(result.outcome)>2||unsigned(result.failure)>3||result.missionSlot<-1||
            (result.outcome==StorylandMissionOutcome::None&&(result.sequence||result.failure!=StorylandMissionFailure::None))||
            (result.outcome!=StorylandMissionOutcome::None&&!result.sequence)||
            ((result.outcome==StorylandMissionOutcome::Failed)!=(result.failure!=StorylandMissionFailure::None)))
            throw std::runtime_error("Invalid world mission result.");
        const auto validateMessage=[](const StorylandMissionEnvironment::Message& message) {
            if(message.numbers.size()>6||message.key.size()>256||message.stringKey.size()>256||message.style<0||message.style>8)
                throw std::runtime_error("Invalid saved message.");
        };
        if(world.subtitleQueue.size()>8||world.briefMessages.size()>20||world.helpMessageNumbers.size()>6||
            world.messageStyle<0||world.messageStyle>8)throw std::runtime_error("Invalid saved message queues.");
        for(const auto& message:world.subtitleQueue)validateMessage(message);
        for(const auto& message:world.briefMessages)validateMessage(message);
        for(const auto& queue:world.bigMessageQueues) {
            if(queue.size()>4)throw std::runtime_error("Invalid big-message queue.");
            for(const auto& message:queue)validateMessage(message);
        }
        const auto& aim=world.manualAim;
        const auto& focus=world.cameraFocus;
        const bool remoteFocus = version >= 62 && state.mission.vcs && focus.kind == 3 &&
            focus.mode == 16 && (focus.switchStyle == 1 || focus.switchStyle == 2);
        if(focus.kind>3||(focus.mode!=15&&!remoteFocus)||focus.switchStyle<0||focus.switchStyle>2||
            (focus.kind!=0&&focus.switchStyle!=2&&!remoteFocus)||
            world.cameraVehicleZoom<0||world.cameraVehicleZoom>5||world.cameraVehicleZoom==4||
            (focus.kind==2&&!world.missionCharacters.count(focus.handle))||
            (focus.kind==3&&!world.missionVehicles.count(focus.handle)))throw std::runtime_error("Invalid saved camera focus.");
        const bool passengerMode=version>=51&&!aim.forced&&aim.mode==11;
        if((aim.mode!=0&&aim.mode!=7&&aim.mode!=8&&aim.mode!=11&&aim.mode!=46&&aim.mode!=47)||
            (!passengerMode&&(aim.forced!=(aim.mode!=0)||(aim.initialized&&!aim.forced)))||
            aim.fieldOfView<4.8f||aim.fieldOfView>70||
            (aim.mode==0&&aim.fieldOfView!=70))throw std::runtime_error("Invalid saved manual aim.");
        if(world.cameraShake.strength<0||world.cameraShake.randomState<0)
            throw std::runtime_error("Invalid saved camera shake.");
        std::set<int32_t> panelItems;
        if(world.nextScriptPanelItem<0)throw std::runtime_error("Invalid panel item counter.");
        for(const auto& panel:world.scriptPanels) {
            if(panel.items.size()>4096)throw std::runtime_error("Invalid panel item count.");
            for(const auto& item:panel.items)if(item.handle<0||item.handle>=world.nextScriptPanelItem||
                !panelItems.insert(item.handle).second||item.numbers.size()>6||item.scale<0||item.key.size()>256)
                throw std::runtime_error("Invalid saved panel item.");
        }
        if(a.reading) {
            world.scriptFrameText.clear();world.scriptFrameTextStyle={};world.frameScriptCoronas.clear();
            world.scriptSounds.clear();
            world.cameraShake.sampled=false;world.manualAim.initialized=false;
            world.nativeFrameMilliseconds=0;world.hudFlashLastFrame={UINT32_MAX,UINT32_MAX};
            for(auto& entry:world.scriptFires)entry.second.suppressionPending=false;
        }
    }
    static void validateCrouch(const StorylandCrouchState& crouch) {
        if(crouch.poseFade.weight<0||crouch.poseFade.weight>1||
            (crouch.poseAnimation!=153&&crouch.poseAnimation!=155)||
            (crouch.movementState!=1&&crouch.movementState!=2&&crouch.movementState!=4&&
             crouch.movementState!=5&&crouch.movementState!=6))throw std::runtime_error("Invalid saved crouch.");
    }
};

struct OpcodeBatchExtensions {
    State& state;

    template<class A> void saveState(A& archive) {
        auto& world = state.environment;
        archive(world.playerFireproofReward, world.playerSpeechEnabled,
            world.playerHealthLimit, world.playerArmourLimit,
            world.requestedAnimationBanks, world.specialModelNames, world.specialModelOwners,
            world.continuousSounds, world.nextContinuousSoundHandle,
            world.radarMapAnchor, world.radarLocalOrigin, world.toggleableRadarBlips, world.cameraZoom);
        if (world.continuousSounds.size() > 250 || world.nextContinuousSoundHandle <= 0) {
            throw std::runtime_error("Invalid saved continuous audio pool.");
        }
        for (const auto& sound : world.continuousSounds) {
            if (sound.first <= 0 || sound.first >= world.nextContinuousSoundHandle) {
                throw std::runtime_error("Invalid saved continuous audio handle.");
            }
        }
        if (world.requestedAnimationBanks.size() > 4096 || world.specialModelNames.size() > 65536 ||
            world.specialModelOwners.size() > world.specialModelNames.size()) {
            throw std::runtime_error("Invalid saved named streaming state.");
        }
        for (const auto& name : world.requestedAnimationBanks) {
            validateAssetName(name);
        }
        for (const auto& alias : world.specialModelNames) {
            if (alias.first < 0 || alias.first > 65535) {
                throw std::runtime_error("Invalid saved special model identity.");
            }
            validateAssetName(alias.second);
        }
        for (const auto& owner : world.specialModelOwners) {
            if (!world.specialModelNames.count(owner.first) || (owner.second & ~0x82u) != 0) {
                throw std::runtime_error("Invalid saved special model ownership.");
            }
        }
        for (const auto& layer : world.toggleableRadarBlips) {
            if (layer.size() > 100) {
                throw std::runtime_error("Invalid saved collectible radar layer.");
            }
        }
        const auto& zoom = world.cameraZoom;
        for (const float offset : {zoom.pedTarget, zoom.pedCurrent, zoom.vehicleTarget, zoom.vehicleCurrent}) {
            if (offset < -3.2f || offset > 15.9f) {
                throw std::runtime_error("Invalid saved camera zoom offset.");
            }
        }
        for (auto& check : world.stuckCarChecks) {
            // Native slots retain duplicate and stale raw vehicle handles
            // until their next due sample. They are not strong references.
            archive(check.vehicle, check.position, check.sampledAt, check.distance, check.interval, check.stuck);
        }
        uint32_t count = uint32_t(world.missionCharacters.size());
        archive(count);
        if (count != world.missionCharacters.size() || count > 4096) {
            throw std::runtime_error("Invalid SC21 character count.");
        }
        std::set<int32_t> seen;
        auto character = world.missionCharacters.begin();
        for (uint32_t index = 0; index < count; ++index, ++character) {
            int32_t handle = character->first;
            archive(handle);
            const auto found = world.missionCharacters.find(handle);
            if (found == world.missionCharacters.end() || !seen.insert(handle).second) {
                throw std::runtime_error("Invalid SC21 character identity.");
            }
            archive(found->second.speechEnabled);
        }
        count = uint32_t(world.radarBlips.size());
        archive(count);
        if (count != world.radarBlips.size() || count > 4096) {
            throw std::runtime_error("Invalid SC21 radar blip count.");
        }
        seen.clear();
        for (uint32_t index = 0; index < count; ++index) {
            int32_t handle = world.radarBlips[index].handle;
            archive(handle);
            const auto found = std::find_if(world.radarBlips.begin(), world.radarBlips.end(),
                [handle](const auto& blip) { return blip.handle == handle; });
            if (found == world.radarBlips.end() || !seen.insert(handle).second) {
                throw std::runtime_error("Invalid SC21 radar blip identity.");
            }
            archive(found->exterior);
        }
    }

    static void validateAssetName(const std::string& name) {
        if (name.empty() || name.size() > 256 || name.find('\0') != std::string::npos) {
            throw std::runtime_error("Invalid saved native asset name.");
        }
    }
};

struct SecondOpcodeBatchExtensions {
    State& state;

    template<class A> void saveState(A& archive) {
        auto& world = state.environment;
        archive(world.characterCollectives, world.playerCarBrakes,
            world.playerNeverTired, world.playerFastReload,
            world.playerSprintEnergy, world.playerSprintMaximum, world.playerSprintTraining,
            world.playerSavedWeapon,
            world.weatherState, world.storedWeatherState,
            world.weatherStored, world.weatherRestoreRequested,
            world.areaNameKey, world.areaNameForced);
        if (world.playerSprintEnergy < (state.mission.vcs ? -102.5f : -152.5f) ||
            world.playerSprintEnergy > 2002.5f ||
            world.playerSprintMaximum < 150 || world.playerSprintMaximum > 2000 ||
            world.playerSprintTraining < 0 || world.playerSprintTraining >= 500 ||
            world.playerSavedWeapon < 0 || world.playerSavedWeapon > 48) {
            throw std::runtime_error("Invalid SC22 player reward state.");
        }
        validateWeather(world.weatherState);
        validateWeather(world.storedWeatherState);
        if (world.areaNameKey.size() > 8 || world.areaNameKey.find('\0') != std::string::npos) {
            throw std::runtime_error("Invalid SC22 area name.");
        }
        size_t memberCount = 0;
        for (const auto& collective : world.characterCollectives) {
            std::set<int32_t> seen;
            memberCount += collective.size();
            for (int32_t handle : collective) {
                const bool player = state.mission.vcs && handle == 0;
                if ((!player && (handle <= 0 || !world.missionCharacters.count(handle))) ||
                    !seen.insert(handle).second) {
                    throw std::runtime_error("Invalid SC22 collective member.");
                }
            }
        }
        if (memberCount > 32) {
            throw std::runtime_error("SC22 collective membership exceeds the native pool.");
        }
        uint32_t count = archive.count(world.characterRoutePoints.size());
        if (count > 200) {
            throw std::runtime_error("SC22 character routes exceed the native pool.");
        }
        if (archive.reading) {
            world.characterRoutePoints.resize(count);
        }
        for (auto& point : world.characterRoutePoints) {
            archive(point.route, point.position);
            if (point.route == -1) {
                throw std::runtime_error("Invalid SC22 character route identity.");
            }
        }
        count = uint32_t(world.missionCharacters.size());
        archive(count);
        if (count != world.missionCharacters.size() || count > 4096) {
            throw std::runtime_error("Invalid SC22 character count.");
        }
        std::set<int32_t> seen;
        auto character = world.missionCharacters.begin();
        for (uint32_t index = 0; index < count; ++index, ++character) {
            int32_t handle = character->first;
            archive(handle);
            const auto found = world.missionCharacters.find(handle);
            if (found == world.missionCharacters.end() || !seen.insert(handle).second) {
                throw std::runtime_error("Invalid SC22 character identity.");
            }
            auto& value = found->second;
            archive(value.stayInSamePlace, value.cannotBeDraggedOut,
                value.stayInCarWhenJacked, value.canBeShotInVehicle, value.neverTargeted,
                value.stopShooting, value.playerFriend, value.canFightInGroup,
                value.drownsInWater, value.gangDamageMask, value.ceaseAttackDelay,
                value.attackStartedAt, value.objectiveCompleted, value.threatVisible,
                value.routeId, value.routeMode, value.routeCursor, value.routeDirection,
                value.objectiveHeight);
            if (value.routeMode < 1 || value.routeMode > 3 || value.routeCursor < 0 ||
                value.routeCursor >= 200 || (value.routeDirection != -1 && value.routeDirection != 1)) {
                throw std::runtime_error("Invalid SC22 character route cursor.");
            }
        }
        count = uint32_t(world.missionVehicles.size());
        archive(count);
        if (count != world.missionVehicles.size() || count > 4096) {
            throw std::runtime_error("Invalid SC22 script vehicle count.");
        }
        seen.clear();
        auto vehicle = world.missionVehicles.begin();
        for (uint32_t index = 0; index < count; ++index, ++vehicle) {
            int32_t handle = vehicle->first;
            archive(handle);
            const auto found = world.missionVehicles.find(handle);
            if (found == world.missionVehicles.end() || !seen.insert(handle).second) {
                throw std::runtime_error("Invalid SC22 script vehicle identity.");
            }
            auto& value = found->second;
            archive(value.colorSet, value.ambientOrigin, value.watertight, value.strong,
                value.upsideDownDamageDisabled, value.canBurstTyres, value.visiblyDamaged,
                value.boatAnchored, value.boatAutopilot, value.clumpVisible, value.tractionMultiplier,
                value.tyreStatus, value.doorDamage, value.doorOpenRatios,
                value.bootAngularVelocity, value.doorsChangedByScript, value.rotation,
                value.angularVelocity, value.rotationChangedByScript, value.angularVelocityChangedByScript);
            validateVehicle(value);
            for (float ratio : value.doorOpenRatios) {
                validateDoorRatio(ratio);
            }
        }
        count = uint32_t(state.vehicles.size());
        archive(count);
        if (count != state.vehicles.size() || count > 4096) {
            throw std::runtime_error("Invalid SC22 physical vehicle count.");
        }
        for (auto& value : state.vehicles) {
            archive(value.watertight, value.strong, value.upsideDownDamageDisabled,
                value.canBurstTyres, value.visiblyDamaged, value.boatAnchored, value.clumpVisible,
                value.tractionMultiplier, value.tyreStatus, value.doorDamage,
                value.bonnetOpenRatio, value.bootOpenRatio, value.bootAngularVelocity,
                value.angularVelocity, value.colorSet, value.scriptColorSet);
            validateVehicle(value);
            validateDoorRatio(value.bonnetOpenRatio);
            validateDoorRatio(value.bootOpenRatio);
        }
        count = uint32_t(world.objects.size());
        archive(count);
        if (count != world.objects.size() || count > 4096) {
            throw std::runtime_error("Invalid SC22 object count.");
        }
        seen.clear();
        std::set<int32_t> poolIndices;
        for (uint32_t index = 0; index < count; ++index) {
            int32_t handle = world.objects[index].handle;
            archive(handle);
            const auto found = std::find_if(world.objects.begin(), world.objects.end(),
                [handle](const auto& object) { return object.handle == handle; });
            if (found == world.objects.end() || !seen.insert(handle).second) {
                throw std::runtime_error("Invalid SC22 object identity.");
            }
            archive(found->nativePoolIndex, found->creationSource);
            if (found->nativePoolIndex < -1 || found->nativePoolIndex >= 4096 ||
                (found->creationSource > 3 && found->creationSource != 5) || (found->nativePoolIndex >= 0 &&
                !poolIndices.insert(found->nativePoolIndex).second)) {
                throw std::runtime_error("Invalid SC22 object pool state.");
            }
        }
        archive(state.mission.messageWaitSkippable);
        auto messageWaitStates = state.program.messageWaitStates();
        archive(messageWaitStates);
        if (archive.reading && !state.program.restoreMessageWaitStates(messageWaitStates)) {
            throw std::runtime_error("Invalid SC22 script message wait states.");
        }
    }

    static void validateWeather(const StorylandMissionEnvironment::WeatherState& value) {
        if (value.previous < 0 || value.previous > 7 || value.current < 0 || value.current > 7 ||
            value.interpolation < 0 || value.interpolation > 1 || value.rain < 0 || value.rain > 1) {
            throw std::runtime_error("Invalid SC22 weather state.");
        }
    }

    template<class T> static void validateVehicle(const T& value) {
        if (value.colorSet < -1 || value.colorSet > 127 ||
            value.scriptColorSet < -1 || value.scriptColorSet > 127) {
            throw std::runtime_error("Invalid SC22 native vehicle colour set.");
        }
        for (uint8_t status : value.tyreStatus) {
            if (status > 2) {
                throw std::runtime_error("Invalid SC22 tyre damage state.");
            }
        }
        for (uint8_t status : value.doorDamage) {
            if (status > 4) {
                throw std::runtime_error("Invalid SC22 door damage state.");
            }
        }
    }

    static void validateDoorRatio(float value) {
        if (value < 0 || value > 1) {
            throw std::runtime_error("Invalid SC22 vehicle door angle.");
        }
    }
};

struct ThirdOpcodeBatchExtensions {
    State& state;

    template<class A, class V> static void vehicle(A& archive, V& value) {
        archive(value.aiActive, value.drivingStyle, value.aiMission, value.aiTargetVehicle,
            value.aiMissionStartedAt, value.routeSeed, value.alarmMilliseconds,
            value.avoidLevelTransitions, value.fastLane, value.lanePolicySet, value.convoy,
            value.straightLineDistance, value.massMultiplier, value.turnMassMultiplier,
            value.panelDamage, value.lightDamage, value.heliOrientation, value.rotorSpeed,
            value.heliStabiliser, value.heliCrashing);
        if (value.drivingStyle < 0 || value.drivingStyle > 4 ||
            (value.aiMission != 0 && value.aiMission != 1 && value.aiMission != 3 &&
             value.aiMission != 5 && value.aiMission != 6 && value.aiMission != 8 &&
             value.aiMission != 9 && value.aiMission != 10 &&
             value.aiMission != 11 && value.aiMission != 12 && value.aiMission != 13 &&
             value.aiMission != 15 && value.aiMission != 16 && value.aiMission != 18 && value.aiMission != 19 &&
             value.aiMission != 20 && value.aiMission != 22 &&
             !(value.model == 261 && (value.aiMission == 27 || value.aiMission == 30))) ||
            value.aiTargetVehicle < -1 ||
            value.massMultiplier <= 0 || value.massMultiplier > 1000 ||
            value.turnMassMultiplier <= 0 || value.turnMassMultiplier > 1000 ||
            (value.heliOrientation != -1 && (value.heliOrientation < 0 || value.heliOrientation > 6.283186f)) ||
            value.rotorSpeed < 0) {
            throw std::runtime_error("Invalid SC23 vehicle policy or physical state.");
        }
        for (uint8_t status : value.panelDamage) {
            if (status > 3) {
                throw std::runtime_error("Invalid SC23 vehicle panel damage.");
            }
        }
        for (uint8_t status : value.lightDamage) {
            if (status > 3) {
                throw std::runtime_error("Invalid SC23 vehicle light damage.");
            }
        }
    }

    template<class A> void saveState(A& archive) {
        auto& world = state.environment;
        archive(world.collectivePolicies, world.forceCollectiveUpdate,
            world.enterCarRangeMultiplier, world.playerHavoc,
            world.forcedRandomPedType, world.lastRandomCharacter,
            world.drunkInputDelay, world.credits, world.pointerArrowActive, world.pointerArrowPosition,
            world.scriptParticleEmitters, world.dollarParticleEnabled,
            world.nextRestartOverride, world.nextRestartOverrideActive,
            world.criticalMissionRestartRequested, world.getOutOfJailFree);
        for (const auto& policy : world.collectivePolicies) {
            if (policy.mode < 0 || policy.mode > 2 || policy.targetGroup < 0 || policy.targetGroup >= 4) {
                throw std::runtime_error("Invalid SC23 collective policy.");
            }
        }
        if (world.lastRandomCharacter < -1 || world.drunkInputDelay > 9 ||
            world.credits.group < 0 || world.credits.group > 98 || world.credits.scroll > 1001024 ||
            world.credits.lines.size() > 1024 || world.scriptParticleEmitters.size() > 256) {
            throw std::runtime_error("Invalid SC23 world presentation state.");
        }
        for (const auto& line : world.credits.lines) {
            if (line.key.empty() || line.key.size() > 8 || line.key.find('\0') != std::string::npos ||
                line.offset < 0 || line.offset > 1000000) {
                throw std::runtime_error("Invalid SC23 credits line.");
            }
        }
        for (const auto& emitter : world.scriptParticleEmitters) {
            const int32_t type = emitter.particle.type;
            if ((type < 0 || type > 4) && type != 6 && !(type == 8 && state.mission.vcs) &&
                type != 12 && type != 13 && type != 17 && type != 18) {
                throw std::runtime_error("Unsupported SC23 native particle emitter subtype.");
            }
            if (!emitter.particle.emitterParametersSupported()) {
                throw std::runtime_error("SC23 particle emitter exceeds the supported runtime domain.");
            }
        }
        uint32_t count = uint32_t(world.missionCharacters.size());
        archive(count);
        if (count != world.missionCharacters.size() || count > 4096) {
            throw std::runtime_error("Invalid SC23 character count.");
        }
        std::set<int32_t> seen;
        auto character = world.missionCharacters.begin();
        for (uint32_t index = 0; index < count; ++index, ++character) {
            int32_t handle = character->first;
            archive(handle);
            const auto found = world.missionCharacters.find(handle);
            if (found == world.missionCharacters.end() || !seen.insert(handle).second) {
                throw std::runtime_error("Invalid SC23 character identity.");
            }
            auto& value = found->second;
            archive(value.colorSet, value.scriptColorSet, value.clothingColors,
                value.clothingColorsSet, value.followingScriptPath, value.removeFromWorld);
            if (value.colorSet < -128 || value.colorSet > 127 ||
                value.scriptColorSet < -128 || value.scriptColorSet > 127) {
                throw std::runtime_error("Invalid SC23 signed native character colour set.");
            }
        }
        count = uint32_t(world.missionVehicles.size());
        archive(count);
        if (count != world.missionVehicles.size() || count > 4096) {
            throw std::runtime_error("Invalid SC23 script vehicle count.");
        }
        seen.clear();
        auto scripted = world.missionVehicles.begin();
        for (uint32_t index = 0; index < count; ++index, ++scripted) {
            int32_t handle = scripted->first;
            archive(handle);
            const auto found = world.missionVehicles.find(handle);
            if (found == world.missionVehicles.end() || !seen.insert(handle).second) {
                throw std::runtime_error("Invalid SC23 script vehicle identity.");
            }
            auto& value = found->second;
            vehicle(archive, value);
            archive(value.routeSeedChanged, value.alarmChangedByScript, value.rotorChangedByScript);
        }
        count = uint32_t(state.vehicles.size());
        archive(count);
        if (count != state.vehicles.size() || count > 4096) {
            throw std::runtime_error("Invalid SC23 physical vehicle count.");
        }
        for (auto& value : state.vehicles) {
            vehicle(archive, value);
            archive(value.rotorAngle);
            if (value.rotorAngle < 0 || value.rotorAngle > 6.283186f) {
                throw std::runtime_error("Invalid SC23 rotor pose.");
            }
        }
        count = uint32_t(state.dropped.size());
        archive(count);
        if (count != state.dropped.size() || count > 4096) {
            throw std::runtime_error("Invalid SC23 dropped pickup count.");
        }
        for (auto& pickup : state.dropped) {
            archive(pickup.randomlyGenerated);
        }
    }
};

struct FourthOpcodeBatchExtensions {
    State& state;
    uint32_t version = 0;

    template<class A> void saveState(A& archive) {
        auto& world=state.environment;
        archive(world.generateCarsAroundCamera,world.freeHealthCare,world.photographTaken,
            world.photographedCharacters,world.securityCamera,world.drunkCameraIntensity,
            world.drunkVisualIntensity,world.drunkVisualStartedAt,world.drunkVisualPeriod,
            world.drunkVisualAmplitude,world.cameraLookAt,world.playerAttachedPitchLimits,
            world.playerCutsceneSafe,world.playerCutscenePreviousControl,
            world.playerCutscenePreviousPoliceIgnore,world.playerCutscenePreviousDamageProofs,
            world.freeBombs,world.freeResprays,world.taxiBoost,
            world.importGarageMasks,world.remoteVehicle,world.remoteDetonationEnabled,
            world.remoteContactDetonation,world.rcHeliHeightLimit,world.vehicleRopes,
            world.scriptedCharacterAnimations,state.betaWeapons);
        if(world.cameraLookAt.kind>3||world.playerCutscenePreviousDamageProofs>31||world.remoteVehicle < -1||
            (world.remoteVehicle>=0&&!world.missionVehicles.count(world.remoteVehicle))||
            world.drunkVisualAmplitude<0||world.drunkVisualAmplitude>127.5f||
            (world.drunkVisualIntensity&&(world.drunkVisualPeriod==0||
                world.drunkVisualPeriod>(version>=62?24576u:16384u)))||
            world.photographedCharacters.size()>4096||world.vehicleRopes.size()>8)
            throw std::runtime_error("Invalid SC24 world or camera state.");
        if(world.scriptedCharacterAnimations.size()>4096)
            throw std::runtime_error("Invalid SC24 animation count.");
        for(const auto& entry:world.scriptedCharacterAnimations) {
            const auto& animation=entry.second;
            if(entry.first<0||(entry.first!=0&&!world.missionCharacters.count(entry.first))||
                animation.group<0||animation.group>=200||animation.animation<0||animation.time<0||
                animation.weight<0||animation.weight>1)
                throw std::runtime_error("Invalid SC24 character animation.");
        }
        for(int32_t handle:world.photographedCharacters)
            if(handle<0)throw std::runtime_error("Invalid SC24 photograph target.");
        for(const auto& entry:world.vehicleRopes) {
            const auto& rope=entry.second;
            if(entry.first!=rope.owner||!world.missionVehicles.count(rope.owner)||rope.type<1||rope.type>4||
                rope.length<=0||rope.length>100||rope.hookObject < -1||rope.attachedObject < -1||rope.attachedVehicle < -1)
                throw std::runtime_error("Invalid SC24 vehicle rope.");
        }
        for(size_t index=0;index<state.betaWeapons.size();++index) {
            const auto& grant=state.betaWeapons[index];
            const int32_t expected=storylandBetaWeaponId(StorylandBetaWeapon(index));
            if(grant.remainingGranted<0||grant.observedAmmunition<0||grant.observedAmmunition>19998||
                grant.remainingGranted>grant.observedAmmunition||grant.previousWeapon < -1||grant.previousWeapon>48||
                (grant.enabled&&grant.nativeWeapon!=expected)||
                (!grant.enabled&&(grant.originallyOwned||grant.externalGrant||grant.nativeWeapon!=-1||
                    grant.remainingGranted||grant.observedAmmunition||grant.previousWeapon!=-1)))
                throw std::runtime_error("Invalid SC24 beta weapon grant.");
            if(grant.enabled) {
                const auto weapon=std::find(state.weaponIds.begin(),state.weaponIds.end(),grant.nativeWeapon);
                if(weapon==state.weaponIds.end())throw std::runtime_error("SC24 beta weapon is absent from the saved inventory table.");
                const size_t slot=size_t(weapon-state.weaponIds.begin());
                if(slot>=state.inventory.size())throw std::runtime_error("Invalid SC24 beta inventory slot.");
                if(state.inventory[slot].ammunition<0||state.inventory[slot].ammunition>9999||
                    (slot<state.confiscated.size()&&(state.confiscated[slot].ammunition<0||state.confiscated[slot].ammunition>9999)))
                    throw std::runtime_error("Invalid SC24 beta ammunition.");
                const int64_t ammunition=int64_t(state.inventory[slot].ammunition)+
                    (slot<state.confiscated.size()?state.confiscated[slot].ammunition:0);
                if(ammunition!=grant.observedAmmunition)
                    throw std::runtime_error("SC24 beta grant does not match saved ammunition.");
            }
        }
        uint32_t count=uint32_t(world.roadChanges.size());archive(count);
        if(count!=world.roadChanges.size()||count>4096)throw std::runtime_error("Invalid SC24 road count.");
        for(auto& road:world.roadChanges)archive(road.angled,road.width);
        count=uint32_t(world.missionCharacters.size());archive(count);
        if(count!=world.missionCharacters.size()||count>4096)throw std::runtime_error("Invalid SC24 character count.");
        std::set<int32_t> seen;
        auto character=world.missionCharacters.begin();
        for(uint32_t index=0;index<count;++index,++character) {
            int32_t handle=character->first;archive(handle);
            const auto found=world.missionCharacters.find(handle);
            if(found==world.missionCharacters.end()||!seen.insert(handle).second)
                throw std::runtime_error("Invalid SC24 character identity.");
            auto& behavior=found->second.behavior;archive(behavior);
            if(behavior.conversationPartner < -1||behavior.forcedThreat < -1||behavior.iceCreamVehicle < -1||
                behavior.iceCreamSlot < -1||behavior.iceCreamSlot>3||behavior.statFleeDistance<0||
                behavior.statHeadingRate<0||behavior.statAttackStrength<0||behavior.statDefendWeakness<0||
                (behavior.areaEnabled&&(behavior.area[0]>behavior.area[2]||behavior.area[1]>behavior.area[3])))
                throw std::runtime_error("Invalid SC24 character behavior.");
        }
        count=uint32_t(world.missionVehicles.size());archive(count);
        if(count!=world.missionVehicles.size()||count>4096)throw std::runtime_error("Invalid SC24 script vehicle count.");
        seen.clear();auto vehicle=world.missionVehicles.begin();
        for(uint32_t index=0;index<count;++index,++vehicle) {
            int32_t handle=vehicle->first;archive(handle);
            const auto found=world.missionVehicles.find(handle);
            if(found==world.missionVehicles.end()||!seen.insert(handle).second)
                throw std::runtime_error("Invalid SC24 script vehicle identity.");
            auto& value=found->second;
            if(!archive.reading&&!value.bikeStandingChangedByScript) {
                const auto physical=std::find_if(state.vehicles.begin(),state.vehicles.end(),
                    [handle](const auto& car){return car.scriptHandle==handle;});
                if(physical!=state.vehicles.end())value.bikeStandingOverride=physical->bikeStandingOverride;
            }
            archive(value.bombOwner,value.previousBombDriver,value.bombDeadline,value.lastHunterShot,
                value.contactExplosion,value.ropeLiftable,value.bikeStandingOverride,value.ropeType,value.ropeOffset);
            if(archive.reading)value.bikeStandingChangedByScript=true;
            if(value.bombOwner < -1||value.previousBombDriver < -1||value.ropeType<0||value.ropeType>4)
                throw std::runtime_error("Invalid SC24 vehicle policy.");
        }
        count=uint32_t(state.vehicles.size());archive(count);
        if(count!=state.vehicles.size()||count>4096)throw std::runtime_error("Invalid SC24 physical vehicle count.");
        for(auto& value:state.vehicles)archive(value.taxiLight,value.bikeStandingOverride);
        count=uint32_t(world.garages.size());archive(count);
        if(count!=world.garages.size()||count>4096)throw std::runtime_error("Invalid SC24 garage count.");
        seen.clear();
        for(auto& garage:world.garages) {
            int32_t handle=garage.handle;archive(handle);
            if(handle!=garage.handle||!seen.insert(handle).second)throw std::runtime_error("Invalid SC24 garage identity.");
            archive(garage.state,garage.doorPosition,garage.targetVehicle,garage.serviceVehicle,
                garage.serviceUntil,garage.deliveredModelMask,garage.resprayDone,garage.cameraFollowsPlayer);
            if(garage.state>5||garage.doorPosition<0||garage.doorPosition>1||
                garage.targetVehicle < -1||garage.serviceVehicle < -1)
                throw std::runtime_error("Invalid SC24 garage state.");
        }
        count=uint32_t(world.objects.size());archive(count);
        if(count!=world.objects.size()||count>4096)throw std::runtime_error("Invalid SC24 object count.");
        seen.clear();
        for(auto& object:world.objects) {
            int32_t handle=object.handle;archive(handle);
            if(handle!=object.handle||!seen.insert(handle).second)throw std::runtime_error("Invalid SC24 object identity.");
            archive(object.ignoredVehicle,object.collidedVehicles,object.ropeLiftable);
            if(object.ignoredVehicle < -1||object.collidedVehicles.size()>6)
                throw std::runtime_error("Invalid SC24 object contact state.");
            for(int32_t vehicleHandle:object.collidedVehicles)
                if(vehicleHandle<0)throw std::runtime_error("Invalid SC24 object contact handle.");
        }
    }
};

struct RadarTraceExtensions {
    StorylandMissionEnvironment& world;
    StorylandRestartTaxiState& restartTaxi;
    template<class A> void saveState(A& archive) {
        if(world.radarBlips.size()>world.radarBlipGenerations.size())
            throw std::runtime_error("Saved radar pool exceeds native capacity.");
        std::array<bool,75> occupied{};
        for(const auto& blip:world.radarBlips) {
            const uint32_t handle=uint32_t(blip.handle);
            const size_t slot=handle&0xffffu;
            const uint16_t generation=uint16_t(handle>>16);
            if(slot>=occupied.size()||occupied[slot]||generation==0||generation==0xffffu)
                throw std::runtime_error("Invalid saved radar trace identity.");
            occupied[slot]=true;
            if(!archive.reading&&world.radarBlipGenerations[slot]==0)
                world.radarBlipGenerations[slot]=generation;
        }
        archive(world.radarBlipGenerations,world.playerCanDoDriveBy,world.playerInStadium,
            world.restartMissionTaxiDestination,world.restartMissionTaxiDestinationSet);
        std::map<int32_t,std::array<bool,8>> characterFlags;
        if(!archive.reading)for(const auto& entry:world.missionCharacters) {
            const auto& behavior=entry.second.behavior;
            characterFlags.emplace(entry.first,std::array<bool,8>{behavior.canDoDriveBy,
                behavior.canAttackPlayerWithCops,behavior.seekNodesForMission,behavior.onlyFrontPassenger,
                behavior.localAvoidanceSolver,behavior.alternateObstructionHandling,
                behavior.allowDuckAgainstArmedTargets,behavior.useUncontrolledWeaponDamage});
        }
        archive(characterFlags);
        if(characterFlags.size()!=world.missionCharacters.size())
            throw std::runtime_error("Invalid saved mission character flag count.");
        for(const auto& entry:characterFlags) {
            const auto found=world.missionCharacters.find(entry.first);
            if(found==world.missionCharacters.end())
                throw std::runtime_error("Invalid saved mission character flag identity.");
            if(archive.reading) {
                auto& behavior=found->second.behavior;
                behavior.canDoDriveBy=entry.second[0];
                behavior.canAttackPlayerWithCops=entry.second[1];
                behavior.seekNodesForMission=entry.second[2];
                behavior.onlyFrontPassenger=entry.second[3];
                behavior.localAvoidanceSolver=entry.second[4];
                behavior.alternateObstructionHandling=entry.second[5];
                behavior.allowDuckAgainstArmedTargets=entry.second[6];
                behavior.useUncontrolledWeaponDamage=entry.second[7];
            }
        }
        archive(restartTaxi);
        if(!storylandValidRestartTaxiState(restartTaxi))
            throw std::runtime_error("Invalid saved restart taxi state.");
        if((restartTaxi.vehicle!=-1&&!world.missionVehicles.count(restartTaxi.vehicle))||
            (restartTaxi.driver!=-1&&!world.missionCharacters.count(restartTaxi.driver))||
            (restartTaxi.blip!=-1&&std::none_of(world.radarBlips.begin(),world.radarBlips.end(),
                [&](const auto& blip){return blip.handle==restartTaxi.blip;})))
            throw std::runtime_error("Invalid saved restart taxi entity reference.");
        const auto& destination=world.restartMissionTaxiDestination;
        if(!std::isfinite(destination.x)||!std::isfinite(destination.y)||
            !std::isfinite(destination.z)||!std::isfinite(destination.heading))
            throw std::runtime_error("Invalid saved mission taxi destination.");
        for(uint16_t generation:world.radarBlipGenerations)
            if(generation==0xffffu)throw std::runtime_error("Invalid saved radar slot generation.");
        for(const auto& blip:world.radarBlips) {
            const uint32_t handle=uint32_t(blip.handle);
            if(world.radarBlipGenerations[handle&0xffffu]!=uint16_t(handle>>16))
                throw std::runtime_error("Saved radar trace generation does not match its slot.");
        }
        archive(world.playerWalkTask);
        if(!storylandValidScriptPlayerWalkTask(world.playerWalkTask))
            throw std::runtime_error("Invalid saved player walk task.");
        archive(world.playerVehicleEntryTarget,world.playerVehicleEntryAsPassenger);
        if(world.playerVehicleEntryTarget < -1)
            throw std::runtime_error("Invalid saved player vehicle entry target.");
        if(world.playerVehicleEntryTarget>=0) {
            const auto vehicle=world.missionVehicles.find(world.playerVehicleEntryTarget);
            if(vehicle==world.missionVehicles.end()||vehicle->second.deleted||!vehicle->second.alive)
                throw std::runtime_error("Saved player vehicle entry target is unavailable.");
        }
        archive(world.playerAllowFrontPassenger);
    }
};

struct RetailParityExtensions {
    State& state;
    template<class A> void saveState(A& archive) {
        auto& world=state.environment;
        archive(world.introMissionSlot,world.introMissionProtected,
            world.animatedEmpireEntities,world.animatedEmpireFreeCursor,state.populationRandom);
        if(world.introMissionSlot < -1 || world.introMissionSlot >= 4096 ||
            (world.introMissionProtected && (!state.mission.vcs || world.introMissionSlot < 0)))
            throw std::runtime_error("Invalid saved intro mission protection.");
        if(!storylandValidAnimatedEmpireEntities(world))
            throw std::runtime_error("Invalid saved animated empire pool.");
        if(state.populationRandom==0 || state.populationRandom>=2147483647u)
            throw std::runtime_error("Invalid saved native population random state.");
        std::vector<int32_t> classes;
        if(!archive.reading)for(const auto& vehicle:state.vehicles)classes.push_back(vehicle.creationClass);
        archive(classes);
        if(classes.size()!=state.vehicles.size())
            throw std::runtime_error("Saved vehicle population class count does not match its pool.");
        for(size_t index=0;index<classes.size();++index) {
            if(classes[index] < -1 || classes[index] > 255)
                throw std::runtime_error("Invalid saved native vehicle population class.");
            if(archive.reading)state.vehicles[index].creationClass=classes[index];
        }
        std::map<int32_t,bool> deferredObjects;
        if(!archive.reading)for(const auto& object:world.objects)
            deferredObjects.emplace(object.handle,object.deferredRender);
        archive(deferredObjects);
        if(deferredObjects.size()!=world.objects.size())
            throw std::runtime_error("Saved deferred object count does not match its pool.");
        for(const auto& entry:deferredObjects) {
            const auto object=std::find_if(world.objects.begin(),world.objects.end(),
                [&](const auto& item){return item.handle==entry.first;});
            if(object==world.objects.end())throw std::runtime_error("Invalid saved deferred object identity.");
            if(archive.reading)object->deferredRender=entry.second;
        }
        std::vector<VehicleTransmission> transmissions;
        if(!archive.reading)for(const auto& vehicle:state.vehicles)
            transmissions.push_back(vehicle.transmission);
        archive(transmissions);
        if(transmissions.size()!=state.vehicles.size())
            throw std::runtime_error("Saved transmission count does not match its vehicle pool.");
        for(size_t index=0;index<transmissions.size();++index) {
            const auto& transmission=transmissions[index];
            if(transmission.gear>5 || transmission.inertiaFactor<.1f || transmission.inertiaFactor>1)
                throw std::runtime_error("Invalid saved native vehicle transmission state.");
            if(archive.reading)state.vehicles[index].transmission=transmission;
        }
        if(archive.reading) {
            world.introMissionRestartQueued=false;
            world.empireRequestedModels.clear();
        }
    }
};

struct NativeWorldExtensions {
    State& state;
    template<class A> void saveState(A& archive) {
        auto& world = state.environment;
        if (!state.mission.vcs) {
            archive(world.photographTaken, world.photographedCharacters,
                world.freeHealthCare, world.getOutOfJailFree);
            if (world.photographedCharacters.size() > 4096)
                throw std::runtime_error("Invalid saved photograph target count.");
            for (int32_t handle : world.photographedCharacters)
                if (handle < 0) throw std::runtime_error("Invalid saved photograph target.");
        }
        archive(state.generatorStates);
        if (state.generatorStates.size() > world.carGenerators.size())
            throw std::runtime_error("Invalid saved car generator state count.");
        for (const auto& entry : state.generatorStates) {
            if (entry.first < 0 || std::none_of(world.carGenerators.begin(),world.carGenerators.end(),
                [&](const auto& generator){return generator.handle==entry.first;}))
                throw std::runtime_error("Invalid saved car generator state handle.");
        }
    }
};

struct LcsOpcodeBatchExtensions {
    State& state;
    std::vector<StorylandMissionEnvironment::Pickup>& pickups;

    template<class A> void saveState(A& archive) {
        auto& world = state.environment;
        archive(world.cutsceneSkipped, world.zoneCarDensities, world.zonePedDensities,
            world.playerBleeding, world.playerGangDamageMask);
        const auto validateDensity = [](const auto& densities) {
            if (densities.size() > 4096) throw std::runtime_error("Invalid saved zone density count.");
            for (const auto& entry : densities)
                if (entry.first.first.empty() || entry.first.first.size() > 8 ||
                    entry.first.first.find('\0') != std::string::npos ||
                    (entry.first.second != 0 && entry.first.second != 1))
                    throw std::runtime_error("Invalid saved zone density identity.");
        };
        validateDensity(world.zoneCarDensities);
        validateDensity(world.zonePedDensities);
        uint32_t pickupCount = uint32_t(pickups.size());
        archive(pickupCount);
        if (pickupCount != pickups.size()) throw std::runtime_error("Invalid saved pickup extension count.");
        for (auto& pickup : pickups) {
            int32_t handle = pickup.handle;
            archive(handle, pickup.revenue, pickup.regenerationModel);
            if (handle != pickup.handle || pickup.regenerationModel < -1)
                throw std::runtime_error("Invalid saved pickup extension identity.");
        }
        if (!state.mission.vcs) {
            archive(world.playerNeverTired, world.playerFastReload,
                world.playerHealthLimit, world.playerArmourLimit, world.playerMaximumHealth,
                world.playerFireproofReward, world.playerSpeechEnabled, world.playerSavedWeapon,
                world.cameraShake, world.cameraZoom, world.cameraControlEnabled,
                world.securityCamera, world.wantsHud, world.drunkInputDelay,
                world.credits, world.musicDoesFade, world.remoteDetonationEnabled,
                world.targettableObjects, world.scriptExplosions);
            if (world.playerSavedWeapon < 0 || world.playerSavedWeapon > 48 ||
                world.drunkInputDelay > 9 || world.cameraShake.strength < 0 ||
                world.credits.group < 0 || world.credits.group > 98 ||
                world.credits.scroll > 1001024 || world.credits.lines.size() > 1024)
                throw std::runtime_error("Invalid saved LCS player or presentation state.");
            for (const auto& explosion : world.scriptExplosions)
                if (explosion.type < 0 || explosion.type > 11 || !explosion.lifetime ||
                    explosion.lifetime > 4250)
                    throw std::runtime_error("Invalid saved LCS explosion.");
            uint32_t characterCount = uint32_t(world.missionCharacters.size());
            archive(characterCount);
            if (characterCount != world.missionCharacters.size())
                throw std::runtime_error("Invalid saved LCS character extension count.");
            for (auto& entry : world.missionCharacters) {
                int32_t handle = entry.first;
                auto& character = entry.second;
                archive(handle, character.creationSource, character.maximumHealth,
                    character.attackTimers.shootOverride, character.attackTimers.attackOverride,
                    character.attackTimers.shootDeadline, character.attackTimers.attackDeadline,
                    character.bleeding, character.heedThreats, character.stayInSamePlace,
                    character.playerFriend, character.canFightInGroup, character.gangDamageMask,
                    character.ceaseAttackDelay, character.attackStartedAt, character.speechEnabled);
                if (handle != entry.first)
                    throw std::runtime_error("Invalid saved LCS character extension identity.");
            }
            uint32_t objectCount = uint32_t(world.objects.size());
            archive(objectCount);
            if (objectCount != world.objects.size())
                throw std::runtime_error("Invalid saved LCS object extension count.");
            for (auto& object : world.objects) {
                int32_t handle = object.handle;
                auto& damage = object.damage;
                archive(handle, object.nativePoolIndex, object.creationSource, object.velocity, object.rotationVelocity, object.dynamic,
                    object.gravity, object.positionFrozen, object.drawLast, object.ignoredVehicle,
                    object.attachedVehicle, object.attachedCharacter, object.attachedBone,
                    object.attachmentOffset, object.attachmentRotation,
                    damage.propertiesLoaded, damage.mass, damage.turnMass, damage.airResistance,
                    damage.elasticity, damage.buoyancy, damage.uprootLimit, damage.multiplier,
                    damage.damageEffect, damage.collisionResponse, damage.effect1, damage.effect2,
                    damage.proofs, damage.cameraAvoid, damage.disableMoveForce, damage.disableTurnForce,
                    damage.health, damage.lastWeapon, damage.visible, damage.renderDamaged);
                if (handle != object.handle || damage.mass <= 0 || damage.turnMass <= 0 ||
                    damage.airResistance < 0 || damage.airResistance > 1 || damage.proofs > 31 ||
                    object.attachedVehicle < -1 || object.attachedCharacter < -1 ||
                    (object.attachedVehicle >= 0 && !world.missionVehicles.count(object.attachedVehicle)) ||
                    (object.attachedCharacter >= 0 && object.attachedCharacter != 1 &&
                        !world.missionCharacters.count(object.attachedCharacter)) ||
                    (object.attachedVehicle >= 0 && object.attachedCharacter >= 0))
                    throw std::runtime_error("Invalid saved LCS object extension state.");
            }
            uint32_t vehicleCount = uint32_t(world.missionVehicles.size());
            archive(vehicleCount);
            if (vehicleCount != world.missionVehicles.size())
                throw std::runtime_error("Invalid saved LCS vehicle extension count.");
            for (auto& entry : world.missionVehicles) {
                int32_t handle = entry.first;
                auto& vehicle = entry.second;
                archive(handle, vehicle.alarmMilliseconds, vehicle.alarmChangedByScript,
                    vehicle.doorDamage, vehicle.doorOpenRatios, vehicle.bootAngularVelocity,
                    vehicle.doorsChangedByScript, vehicle.visiblyDamaged, vehicle.angularVelocity,
                    vehicle.angularVelocityChangedByScript, vehicle.healthChangedByScript,
                    vehicle.velocity, vehicle.velocityChangedByScript, vehicle.wrecked);
                if (handle != entry.first)
                    throw std::runtime_error("Invalid saved LCS vehicle extension identity.");
                for (uint8_t status : vehicle.doorDamage)
                    if (status > 3) throw std::runtime_error("Invalid saved LCS vehicle door damage.");
            }
            uint32_t physicalVehicleCount = uint32_t(state.vehicles.size());
            archive(physicalVehicleCount);
            if (physicalVehicleCount != state.vehicles.size())
                throw std::runtime_error("Invalid saved LCS physical vehicle count.");
            for (auto& vehicle : state.vehicles) {
                uint32_t model = vehicle.model;
                int32_t handle = vehicle.scriptHandle;
                archive(model, handle, vehicle.alarmMilliseconds, vehicle.doorDamage,
                    vehicle.bonnetOpenRatio, vehicle.bootOpenRatio, vehicle.bootAngularVelocity,
                    vehicle.angularVelocity, vehicle.visiblyDamaged, vehicle.wrecked);
                if (model != vehicle.model || handle != vehicle.scriptHandle)
                    throw std::runtime_error("Invalid saved LCS physical vehicle identity.");
                for (uint8_t status : vehicle.doorDamage)
                    if (status > 3) throw std::runtime_error("Invalid saved LCS physical door damage.");
            }
            if (archive.reading) world.cameraShake.sampled = false;
        }
        uint32_t creditCount = uint32_t(world.credits.lines.size());
        archive(creditCount);
        if (creditCount != world.credits.lines.size() || creditCount > 1024)
            throw std::runtime_error("Invalid saved credit layout count.");
        for (auto& line : world.credits.lines) {
            std::string key = line.key;
            archive(key, line.height, line.text);
            if (key != line.key || key.empty() || key.size() > 8 ||
                key.find('\0') != std::string::npos || line.offset < 0 || line.offset > 1000000 ||
                line.height < 0 || line.height > 1000000 || line.text.size() > 65536)
                throw std::runtime_error("Invalid saved credit layout.");
        }
    }
};

struct NativeMarkerExtensions {
    StorylandMissionEnvironment& world;

    template<class A> void saveState(A& archive) {
        archive(world.markerGenerations);
        for(uint16_t generation:world.markerGenerations)
            if(generation==0xffffu)
                throw std::runtime_error("Invalid saved native marker generation.");
        const uint32_t count=archive.count(world.markers.size());
        if(count!=world.markers.size())
            throw std::runtime_error("Saved native marker count does not match the world.");
        for(auto& marker:world.markers) {
            std::array<uint32_t,3> saved{uint32_t(marker.handle),marker.type,marker.createdAt};
            archive(saved);
            if(saved[0]!=uint32_t(marker.handle)||(saved[1]!=5u&&saved[1]!=6u&&saved[1]!=7u))
                throw std::runtime_error("Invalid saved native marker identity or type.");
            const size_t slot=saved[0]&0xffffu;
            const uint16_t generation=uint16_t(saved[0]>>16);
            if(marker.persistent&&generation!=0u&&
                (slot>=world.markerGenerations.size()||world.markerGenerations[slot]!=generation))
                throw std::runtime_error("Saved native marker generation does not match its handle.");
            marker.type=uint8_t(saved[1]);
            marker.createdAt=saved[2];
        }
    }
};

struct LcsNativeBonusExtensions {
    State& state;
    template<class Archive> void saveState(Archive& archive) {
        if(!state.mission.vcs)archive(state.adrenaline,state.killFrenzy);
    }
};

inline void validateLcsNativeBonuses(State& state,
    const std::vector<StorylandMissionEnvironment::Pickup>& pickups,bool reading) {
    if(state.mission.vcs)return;
    auto& world=state.environment;
    const auto& adrenaline=state.adrenaline;
    const uint32_t remaining=adrenaline.expiresAt-world.gameMilliseconds;
    if(adrenaline.pickupHandle < -1||
        (adrenaline.active&&(!adrenaline.enabled||!remaining||
            remaining>StorylandAdrenalineState::durationMilliseconds))||
        (!adrenaline.active&&adrenaline.expiresAt!=0))
        throw std::runtime_error("Invalid saved LCS adrenaline effect.");
    if(adrenaline.pickupHandle>=0&&std::none_of(pickups.begin(),pickups.end(),
        [&](const auto& pickup){return pickup.handle==adrenaline.pickupHandle&&pickup.type==3&&pickup.model>=0;}))
        throw std::runtime_error("Invalid saved LCS adrenaline pickup.");
    const auto& frenzy=state.killFrenzy;
    const auto& grant=frenzy.grant;
    const auto& saved=world.lcsExtendedWorld.rampageWeapon;
    if(frenzy.status>3||frenzy.custom||frenzy.kills<0||frenzy.targetKills<0||
        frenzy.targetKills>65535||frenzy.kills>frenzy.targetKills||frenzy.text.size()>128||
        (frenzy.status&&(frenzy.nativeWeapon<0||frenzy.nativeWeapon>47))||
        saved.active!=frenzy.active()||
        (saved.active&&saved.physicalWeapon!=storylandLcsRampagePhysicalWeapon(frenzy.nativeWeapon))||
        (grant.enabled&&(!frenzy.active()||grant.nativeWeapon!=saved.physicalWeapon))||
        (frenzy.active()&&saved.physicalWeapon<37&&!grant.enabled))
        throw std::runtime_error("Invalid saved native LCS rampage.");
    if(grant.remainingGranted<0||grant.observedAmmunition<0||grant.observedAmmunition>199998||
        grant.remainingGranted>grant.observedAmmunition||grant.previousWeapon < -1||
        (!grant.enabled&&(grant.originallyOwned||grant.externalGrant||grant.nativeWeapon!=-1||
            grant.remainingGranted||grant.observedAmmunition||grant.previousWeapon!=-1)))
        throw std::runtime_error("Invalid saved LCS rampage weapon grant.");
    if(grant.enabled) {
        const auto weapon=std::find(state.weaponIds.begin(),state.weaponIds.end(),grant.nativeWeapon);
        if(weapon==state.weaponIds.end())throw std::runtime_error("Saved LCS rampage weapon is unavailable.");
        const size_t index=size_t(weapon-state.weaponIds.begin());
        if(index>=state.inventory.size()||state.inventory[index].ammunition<0||
            (index<state.confiscated.size()&&state.confiscated[index].ammunition<0))
            throw std::runtime_error("Invalid saved LCS rampage inventory.");
        const int64_t ammunition=int64_t(state.inventory[index].ammunition)+
            (index<state.confiscated.size()?state.confiscated[index].ammunition:0);
        if(ammunition!=grant.observedAmmunition)
            throw std::runtime_error("Saved LCS rampage grant does not match the inventory.");
    }
    if(reading)world.killFrenzyStatus=frenzy.status;
    else if(world.killFrenzyStatus!=frenzy.status)
        throw std::runtime_error("Saved LCS rampage status is inconsistent.");
}

enum class LegacySc35Layout { NativeMarkers, LcsOpcodes };

inline LegacySc35Layout savedLegacySc35Layout(const Archive& source,const State& state,
    const std::vector<StorylandMissionEnvironment::Pickup>& pickups) {
    bool nativeMarkers=false,lcsOpcodes=false;
    {
        auto candidate=state.environment;
        Archive reader{source.bytes,true,source.cursor,source.budget};
        NativeMarkerExtensions extension{candidate};
        try {
            subsystem(reader,0x35334353u,extension);
            nativeMarkers=true;
        } catch(const std::runtime_error&) {}
    }
    {
        auto candidate=std::make_unique<State>(state);
        auto candidatePickups=pickups;
        Archive reader{source.bytes,true,source.cursor,source.budget};
        LcsOpcodeBatchExtensions extension{*candidate,candidatePickups};
        try {
            subsystem(reader,0x35334353u,extension);
            lcsOpcodes=true;
        } catch(const std::runtime_error&) {}
    }
    if(nativeMarkers&&lcsOpcodes)
        throw std::runtime_error("Ambiguous legacy SC35 save: both marker and LCS opcode layouts are valid.");
    if(!nativeMarkers&&!lcsOpcodes)
        throw std::runtime_error("Invalid legacy SC35 save: neither extension layout is valid.");
    return nativeMarkers?LegacySc35Layout::NativeMarkers:LegacySc35Layout::LcsOpcodes;
}

struct CharacterPhysicsExtensions {
    State& state;

    template<class A> void saveState(A& archive) {
        auto& boundary=state.environment.vcsSwimmingBoundary;
        auto& world=state.environment;
        std::map<int32_t,std::array<bool,3>> characterPhysics;
        if(!archive.reading)for(const auto& entry:world.missionCharacters) {
            const auto& character=entry.second;
            characterPhysics.emplace(entry.first,std::array<bool,3>{character.collisionEnabled,
                character.disableVerticalMovement,character.testShotInVehicle});
        }
        archive(boundary,characterPhysics,world.playerCollisionEnabled,
            world.playerDisableVerticalMovement,world.playerTestShotInVehicle);
        if(boundary.strength < -1.0f || boundary.strength > 1.0f ||
            boundary.waveStrength < 0 || boundary.waveStrength > 1.0f ||
            std::fabs(boundary.velocity) > 4.0f * StorylandNativePhysicsRate)
            throw std::runtime_error("Invalid saved VCS swimming boundary state.");
        if(characterPhysics.size()!=world.missionCharacters.size())
            throw std::runtime_error("Saved character physics count differs from the character pool.");
        for(const auto& entry:characterPhysics) {
            const auto found=world.missionCharacters.find(entry.first);
            if(found==world.missionCharacters.end())
                throw std::runtime_error("Invalid saved character physics identity.");
            if(archive.reading) {
                auto& character=found->second;
                character.collisionEnabled=entry.second[0];
                character.disableVerticalMovement=entry.second[1];
                character.testShotInVehicle=entry.second[2];
            }
        }
    }
};

enum class LegacySc37Layout { CharacterPhysics, LcsOpcodes };

inline LegacySc37Layout savedLegacySc37Layout(const Archive& source,const State& state,
    const std::vector<StorylandMissionEnvironment::Pickup>& pickups) {
    bool characterPhysics=false,lcsOpcodes=false;
    {
        auto candidate=std::make_unique<State>(state);
        Archive reader{source.bytes,true,source.cursor,source.budget};
        CharacterPhysicsExtensions extension{*candidate};
        try {
            subsystem(reader,0x37334353u,extension);
            characterPhysics=true;
        } catch(const std::runtime_error&) {}
    }
    {
        auto candidate=std::make_unique<State>(state);
        auto candidatePickups=pickups;
        Archive reader{source.bytes,true,source.cursor,source.budget};
        LcsOpcodeBatchExtensions extension{*candidate,candidatePickups};
        try {
            subsystem(reader,0x37334353u,extension);
            lcsOpcodes=true;
        } catch(const std::runtime_error&) {}
    }
    if(characterPhysics&&lcsOpcodes)
        throw std::runtime_error("Ambiguous legacy SC37 save: both character physics and LCS opcode layouts are valid.");
    if(!characterPhysics&&!lcsOpcodes)
        throw std::runtime_error("Invalid legacy SC37 save: neither extension layout is valid.");
    return characterPhysics?LegacySc37Layout::CharacterPhysics:LegacySc37Layout::LcsOpcodes;
}

struct VcsPedNativeState {
    bool dontReenterAfterEjection=false;
    bool avoidUnsupportedForwardStep=false;
    bool mountedGunInitialized=false;
    uint8_t randomSeed=0;
    bool randomSeedInitialized=false;

    template<class ArchiveType> void saveState(ArchiveType& archive) {
        archive(dontReenterAfterEjection,avoidUnsupportedForwardStep,mountedGunInitialized,
            randomSeed,randomSeedInitialized);
    }
};

inline void persistentVcsPedNativeState(Archive& archive,State& state,uint32_t version) {
    auto& world=state.environment;
    std::map<int32_t,VcsPedNativeState> characters;
    std::map<int32_t,bool> pausedAnimations;
    if(!archive.reading) {
        for(const auto& entry:world.missionCharacters) {
            const auto& character=entry.second;
            characters[entry.first]={character.dontReenterAfterEjection,
                character.avoidUnsupportedForwardStep,character.mountedGunInitialized,
                character.nativePedRandomSeed,character.nativePedRandomSeedInitialized};
        }
        for(const auto& entry:world.scriptedCharacterAnimations)
            pausedAnimations[entry.first]=entry.second.paused;
    }
    subsystem(archive,0x36354353,world.playerDontReenterAfterEjection,
        world.playerAvoidUnsupportedForwardStep,characters,pausedAnimations,
        world.vcsPedFalls,world.vcsCharacterAttachments);
    if(characters.size()!=world.missionCharacters.size()||
        pausedAnimations.size()!=world.scriptedCharacterAnimations.size())
        throw std::runtime_error("Saved VCS native character state has inconsistent references.");
    if(!state.mission.vcs&&(world.playerDontReenterAfterEjection||
        world.playerAvoidUnsupportedForwardStep||!world.vcsPedFalls.empty()||
        !world.vcsCharacterAttachments.empty()))
        throw std::runtime_error("Saved VCS native character state is present in an LCS game.");
    for(const auto& entry:characters) {
        const auto character=world.missionCharacters.find(entry.first);
        const auto& saved=entry.second;
        if(character==world.missionCharacters.end()||
            (!saved.randomSeedInitialized&&saved.randomSeed!=0)||
            (!state.mission.vcs&&(saved.dontReenterAfterEjection||
                saved.avoidUnsupportedForwardStep||saved.mountedGunInitialized||
                saved.randomSeedInitialized||saved.randomSeed!=0)))
            throw std::runtime_error("Invalid saved VCS native character flags.");
        if(archive.reading) {
            character->second.dontReenterAfterEjection=saved.dontReenterAfterEjection;
            character->second.avoidUnsupportedForwardStep=saved.avoidUnsupportedForwardStep;
            character->second.mountedGunInitialized=saved.mountedGunInitialized;
            character->second.nativePedRandomSeed=saved.randomSeed;
            character->second.nativePedRandomSeedInitialized=saved.randomSeedInitialized;
        }
    }
    for(const auto& entry:pausedAnimations) {
        const auto animation=world.scriptedCharacterAnimations.find(entry.first);
        const auto character=world.missionCharacters.find(entry.first);
        if(animation==world.scriptedCharacterAnimations.end()||
            (entry.second&&(!state.mission.vcs||character==world.missionCharacters.end()||
                !character->second.mountedGunInitialized||
                animation->second.group!=storylandVcsMountedGunGroup||
                animation->second.animation!=storylandVcsMountedGunAimAnimation)))
            throw std::runtime_error("Invalid saved paused mounted-gun association.");
        if(archive.reading)animation->second.paused=entry.second;
    }
    for(const auto& entry:world.vcsPedFalls) {
        const auto character=world.missionCharacters.find(entry.first);
        const auto vehicle=world.missionVehicles.find(entry.second.sourceVehicle);
        if(entry.first<=0||character==world.missionCharacters.end()||
            !storylandValidVcsPedFall(entry.second)||character->second.vehicle>=0||
            character->second.attached||!character->second.nativePedRandomSeedInitialized||
            (entry.second.sourceVehicle>=0&&(vehicle==world.missionVehicles.end()||vehicle->second.deleted)))
            throw std::runtime_error("Invalid saved VCS bike-rider recovery state.");
    }
    if(world.vcsCharacterAttachments.size()>world.missionCharacters.size()+1)
        throw std::runtime_error("Too many saved VCS character attachments.");
    for(const auto& entry:world.vcsCharacterAttachments) {
        const bool player=entry.first==0;
        const auto character=world.missionCharacters.find(entry.first);
        const auto& attachment=entry.second;
        const int32_t vehicleHandle=player?world.playerAttachedVehicle:
            character==world.missionCharacters.end()?-1:character->second.vehicle;
        const auto vehicle=world.missionVehicles.find(vehicleHandle);
        const auto attachedDefinition=world.playerWeapons.find(attachment.attachedWeapon);
        const auto previousDefinition=world.playerWeapons.find(attachment.savedWeapon);
        const bool invalidLegacyParent = version < 63 &&
            ((!player && (character == world.missionCharacters.end() || !character->second.attached)) ||
                vehicleHandle < 0 || vehicle == world.missionVehicles.end() || vehicle->second.deleted);
        if((!player&&character==world.missionCharacters.end())||invalidLegacyParent||
            attachment.rotationLimit<0||attachedDefinition==world.playerWeapons.end()||
            previousDefinition==world.playerWeapons.end()||attachment.savedSlotWeapon<-1||
            (attachment.savedSlotWeapon>=0&&!world.playerWeapons.count(attachment.savedSlotWeapon)))
            throw std::runtime_error("Invalid saved VCS character attachment.");
    }
    if(archive.reading)world.mountedGunRequests.clear();
}

struct VcsPlaneControlState {
    State& state;

    template<class ArchiveType> void saveState(ArchiveType& archive) {
        uint32_t count=uint32_t(state.vehicles.size());
        archive(count);
        if(count!=state.vehicles.size()||count>4096)
            throw std::runtime_error("Saved VCS plane-control count differs from the physical vehicle pool.");
        for(auto& vehicle:state.vehicles) {
            auto axes=vehicle.vcsPlaneControlAxes;
            archive(axes);
            if(!validVcsPlaneControlAxes(axes)||
                (!state.mission.vcs&&axes!=std::array<float,3>{}))
                throw std::runtime_error("Invalid saved VCS plane-control axes.");
            if(archive.reading)vehicle.vcsPlaneControlAxes=axes;
        }
    }
};

struct VcsJetSkiControlState {
    State& state;

    template<class ArchiveType> void saveState(ArchiveType& archive) {
        uint32_t count=uint32_t(state.vehicles.size());archive(count);
        if(count!=state.vehicles.size()||count>4096)
            throw std::runtime_error("Saved JetSki axis count differs from the physical vehicle pool.");
        for(auto& vehicle:state.vehicles) {
            auto axes=vehicle.vcsJetSkiAxes;archive(axes);
            if(!storylandVcsJetSkiAxesValid(axes)||
                (!state.mission.vcs&&axes!=std::array<float,4>{}))
                throw std::runtime_error("Invalid saved VCS JetSki control or pose axis.");
            if(archive.reading)vehicle.vcsJetSkiAxes=axes;
        }
    }
};

struct VcsCharacterScriptState {
    State& state;

    template<class ArchiveType> void saveState(ArchiveType& archive) {
        auto& world = state.environment;
        std::pair<uint8_t, bool> player{world.playerVcsScriptFlags.evasiveDirection,
            world.playerVcsScriptFlags.acceptsPairedActions};
        std::map<int32_t, std::pair<uint8_t, bool>> characters;
        if (!archive.reading) {
            for (const auto& entry : world.missionCharacters) {
                const auto& flags = entry.second.vcsScriptFlags;
                characters.emplace(entry.first,
                    std::make_pair(flags.evasiveDirection, flags.acceptsPairedActions));
            }
        }
        bool playerRunning=world.playerScriptRunning;
        archive(player, characters, playerRunning);
        const auto valid = [&](const std::pair<uint8_t, bool>& flags) {
            return flags.first <= 2 && (state.mission.vcs ||
                (flags.first == 0 && flags.second));
        };
        if (!valid(player) || (!state.mission.vcs&&playerRunning) ||
            characters.size() != world.missionCharacters.size() ||
            characters.size() > 4096)
            throw std::runtime_error("Invalid saved native character script flags.");
        for (const auto& entry : characters) {
            if (!world.missionCharacters.count(entry.first) || !valid(entry.second))
                throw std::runtime_error("Invalid saved native character script flag owner or value.");
        }
        if (archive.reading) {
            world.playerScriptRunning=playerRunning;
            world.playerVcsScriptFlags = {player.first, player.second};
            for (const auto& entry : characters)
                world.missionCharacters.at(entry.first).vcsScriptFlags = {
                    entry.second.first, entry.second.second};
        }
    }
};

inline bool defaultVcsVehicleCaption(const StorylandVcsVehicleCaption& caption) {
    return caption.previousKey.empty() && caption.displayedKey.empty() &&
        caption.displayMilliseconds == 0 && caption.fadeMilliseconds == 0 &&
        caption.phase == StorylandVcsVehicleCaption::Phase::Hidden && !caption.drawing;
}

inline bool defaultVcsMissionPresentation(const StorylandMissionEnvironment& world) {
    const auto defaultMarks = [](const StorylandVcsFootstepMarkState& marks) {
        return marks.mode == 0 && marks.remaining == 0;
    };
    return defaultMarks(world.playerVcsFootstepMarks) &&
        std::all_of(world.missionCharacters.begin(), world.missionCharacters.end(),
            [&](const auto& entry) { return defaultMarks(entry.second.vcsFootstepMarks); }) &&
        world.vcsHudHiddenComponents == 0 && world.vcsAreaCaption.visible &&
        world.vcsScript0556Value == 1.0f && defaultVcsVehicleCaption(world.vcsVehicleCaption) &&
        world.vcsRemoteCameraUpsideDownOffset == 0 &&
        world.drunkSteeringDrift == 0 &&
        !(world.cameraFocus.kind == 3 && world.cameraFocus.mode == 16);
}

struct VcsMissionPresentationState {
    State& state;

    template<class ArchiveType> void saveState(ArchiveType& archive) {
        auto& world = state.environment;
        std::pair<uint8_t, uint32_t> playerMarks{
            world.playerVcsFootstepMarks.mode, world.playerVcsFootstepMarks.remaining};
        uint32_t characterCount = uint32_t(world.missionCharacters.size());
        archive(playerMarks, characterCount);
        if (characterCount != world.missionCharacters.size() || characterCount > 4096)
            throw std::runtime_error("Saved VCS footstep owners differ from the mission character pool.");

        std::map<int32_t, std::pair<uint8_t, uint32_t>> characterMarks;
        if (archive.reading) {
            for (uint32_t index = 0; index < characterCount; ++index) {
                int32_t handle = -1;
                std::pair<uint8_t, uint32_t> marks{};
                archive(handle, marks);
                if (!world.missionCharacters.count(handle) ||
                    !characterMarks.emplace(handle, marks).second)
                    throw std::runtime_error("Invalid or duplicate saved VCS footstep owner.");
            }
        } else {
            for (const auto& entry : world.missionCharacters) {
                int32_t handle = entry.first;
                const auto& source = entry.second.vcsFootstepMarks;
                std::pair<uint8_t, uint32_t> marks{source.mode, source.remaining};
                archive(handle, marks);
                characterMarks.emplace(handle, marks);
            }
        }

        uint8_t hiddenComponents = world.vcsHudHiddenComponents;
        bool areaCaptionVisible = world.vcsAreaCaption.visible;
        float script0556Value = world.vcsScript0556Value;
        float remoteCameraOffset = world.vcsRemoteCameraUpsideDownOffset;
        int16_t steeringDrift = world.drunkSteeringDrift;
        auto caption = world.vcsVehicleCaption;
        archive(hiddenComponents, areaCaptionVisible, script0556Value,
            caption.previousKey, caption.displayedKey, caption.displayMilliseconds,
            caption.fadeMilliseconds, caption.phase, caption.drawing, remoteCameraOffset, steeringDrift);

        const auto validMarks = [&](const std::pair<uint8_t, uint32_t>& marks) {
            return marks.first <= 3 && (state.mission.vcs ||
                (marks.first == 0 && marks.second == 0));
        };
        const auto validKey = [](const std::string& key) {
            return key.size() <= 8 && std::all_of(key.begin(), key.end(),
                [](unsigned char character) { return character >= 32 && character <= 126; });
        };
        if (!validMarks(playerMarks) ||
            !std::all_of(characterMarks.begin(), characterMarks.end(),
                [&](const auto& entry) { return validMarks(entry.second); }))
            throw std::runtime_error("Invalid saved VCS footstep-mark state.");
        if ((hiddenComponents & uint8_t(~uint8_t(0xBF))) != 0 ||
            (script0556Value != 1.0f && script0556Value != 0.1f) ||
            !validKey(caption.previousKey) || !validKey(caption.displayedKey) ||
            caption.fadeMilliseconds < 0 || caption.fadeMilliseconds > 1000 ||
            uint8_t(caption.phase) > uint8_t(StorylandVcsVehicleCaption::Phase::Replace) ||
            remoteCameraOffset < 0 || remoteCameraOffset > .53f ||
            steeringDrift < -96 || steeringDrift > 96)
            throw std::runtime_error("Invalid saved VCS HUD or vehicle-caption state.");
        if (!state.mission.vcs && (hiddenComponents != 0 || !areaCaptionVisible ||
            script0556Value != 1.0f || !defaultVcsVehicleCaption(caption) ||
            remoteCameraOffset != 0 || steeringDrift != 0))
            throw std::runtime_error("VCS mission presentation cannot be restored into an LCS game.");
        const auto& focus = world.cameraFocus;
        if (focus.kind == 3 && focus.mode == 16 &&
            (!state.mission.vcs || world.remoteVehicle < 0 || focus.handle != world.remoteVehicle ||
                !world.missionVehicles.count(focus.handle) ||
                world.missionVehicles.at(focus.handle).deleted))
            throw std::runtime_error("Saved VCS remote camera does not identify the active remote vehicle.");

        if (archive.reading) {
            world.playerVcsFootstepMarks = {playerMarks.first, playerMarks.second};
            for (const auto& entry : characterMarks)
                world.missionCharacters.at(entry.first).vcsFootstepMarks = {
                    entry.second.first, entry.second.second};
            world.vcsHudHiddenComponents = hiddenComponents;
            world.vcsAreaCaption.visible = areaCaptionVisible;
            world.vcsScript0556Value = script0556Value;
            world.vcsVehicleCaption = std::move(caption);
            world.vcsRemoteCameraUpsideDownOffset = remoteCameraOffset;
            world.drunkSteeringDrift = steeringDrift;
        }
    }
};

struct VcsCharacterObjectParentState {
    State& state;

    template<class ArchiveType> void saveState(ArchiveType& archive) {
        auto& world = state.environment;
        uint32_t count = uint32_t(world.vcsCharacterObjectParents.size());
        archive(count);
        if (count > 4096 || count > world.vcsCharacterAttachments.size() ||
            (!state.mission.vcs && count != 0))
            throw std::runtime_error("Invalid saved VCS character-object parent count.");

        std::map<int32_t, int32_t> parents;
        if (archive.reading) {
            for (uint32_t index = 0; index < count; ++index) {
                int32_t owner = -1;
                int32_t object = -1;
                archive(owner, object);
                if (!parents.emplace(owner, object).second)
                    throw std::runtime_error("Duplicate saved VCS character-object parent owner.");
            }
        } else {
            parents = world.vcsCharacterObjectParents;
            for (const auto& entry : parents) {
                int32_t owner = entry.first;
                int32_t object = entry.second;
                archive(owner, object);
            }
        }

        for (const auto& entry : parents) {
            const bool player = entry.first == 0;
            const auto character = world.missionCharacters.find(entry.first);
            if ((!player && character == world.missionCharacters.end()) ||
                !world.vcsCharacterAttachments.count(entry.first) ||
                entry.second < -1 || entry.second == 0)
                throw std::runtime_error("Invalid saved VCS character-object parent owner or reference.");
            if (entry.second == -1) continue;
            const auto object = std::find_if(world.objects.begin(), world.objects.end(),
                [&](const auto& value) { return value.handle == entry.second; });
            if (object == world.objects.end() ||
                (player && (world.playerAttachedVehicle != -1 || world.inVehicle)) ||
                (!player && (!character->second.attached || character->second.vehicle != -1 ||
                    character->second.passengerSeat != -1)))
                throw std::runtime_error("Saved VCS object attachment does not identify a live parent and attached actor.");
        }

        for (const auto& entry : world.vcsCharacterAttachments) {
            if (parents.count(entry.first)) continue;
            const bool player = entry.first == 0;
            const auto character = world.missionCharacters.find(entry.first);
            const int32_t handle = player ? world.playerAttachedVehicle :
                character == world.missionCharacters.end() ? -1 : character->second.vehicle;
            const auto vehicle = world.missionVehicles.find(handle);
            if ((!player && (character == world.missionCharacters.end() || !character->second.attached)) ||
                handle < 0 || vehicle == world.missionVehicles.end() || vehicle->second.deleted)
                throw std::runtime_error("Saved VCS vehicle attachment has no live parent.");
        }
        if (archive.reading) world.vcsCharacterObjectParents = std::move(parents);
    }
};

struct VcsSeekAndMusicState {
    State& state;

    template<class ArchiveType> void saveState(ArchiveType& archive) {
        auto& world=state.environment;
        std::map<int32_t,uint8_t> radii;
        if(!archive.reading)for(const auto& entry:world.missionCharacters)
            radii.emplace(entry.first,entry.second.vcsSeekNodeRadius);
        archive(world.playerVcsSeekNodeRadius,radii,world.scriptMusicStartPositions);
        if(radii.size()!=world.missionCharacters.size()||radii.size()>4096)
            throw std::runtime_error("Invalid saved VCS seek-node radius table.");
        if(std::any_of(world.scriptMusicStartPositions.begin(),world.scriptMusicStartPositions.end(),
            [](int32_t value){return value<0;}))
            throw std::runtime_error("Invalid saved VCS music start position.");
        if(!state.mission.vcs&&(world.playerVcsSeekNodeRadius!=5||
            std::any_of(world.scriptMusicStartPositions.begin(),world.scriptMusicStartPositions.end(),
                [](int32_t value){return value!=0;})))
            throw std::runtime_error("Saved VCS seek or music state is present in an LCS game.");
        for(const auto& entry:radii) {
            const auto found=world.missionCharacters.find(entry.first);
            if(found==world.missionCharacters.end()||(!state.mission.vcs&&entry.second!=5))
                throw std::runtime_error("Invalid saved VCS seek-node radius owner or value.");
            if(archive.reading)found->second.vcsSeekNodeRadius=entry.second;
        }
    }
};

inline bool legacySprintReservePresent(uint32_t version,bool vcs) {
    return version>=22&&(version<25||vcs);
}

inline bool defaultSprintReserve(const StorylandMissionEnvironment& world) {
    return world.playerSprintEnergy==150.0f&&world.playerSprintMaximum==150.0f&&
        world.playerSprintTraining==0.0f;
}

inline void validateSprintReserve(const StorylandMissionEnvironment& world,bool vcs) {
    if(!std::isfinite(world.playerSprintEnergy)||!std::isfinite(world.playerSprintMaximum)||
        !std::isfinite(world.playerSprintTraining)||
        world.playerSprintEnergy<(vcs?-102.5f:-152.5f)||world.playerSprintEnergy>2002.5f||
        world.playerSprintMaximum<150.0f||world.playerSprintMaximum>2000.0f||
        world.playerSprintTraining<0.0f||world.playerSprintTraining>=500.0f)
        throw std::runtime_error("Invalid saved native sprint reserve.");
}

inline void persistentSubsystems(Archive& archive,State& state,uint32_t version=Version) {
    if(archive.reading)state.environment.photographedVehicles.clear();
    if(version<66&&!archive.reading&&!legacySprintReservePresent(version,state.mission.vcs)&&
        !defaultSprintReserve(state.environment))
        throw std::runtime_error("This older save schema cannot retain native sprint reserve.");
    if(version<65&&!archive.reading&&
        (!state.environment.playerDrownsInWater||state.environment.playerInfiniteSwim))
        throw std::runtime_error("An older save schema cannot retain native water permissions.");
    if(version<64&&!archive.reading) {
        const auto& world=state.environment;
        const bool changedCharacter=std::any_of(world.missionCharacters.begin(),world.missionCharacters.end(),
            [](const auto& entry){return entry.second.vcsSeekNodeRadius!=5;});
        const bool changedMusic=std::any_of(world.scriptMusicStartPositions.begin(),
            world.scriptMusicStartPositions.end(),[](int32_t value){return value!=0;});
        if(world.playerVcsSeekNodeRadius!=5||changedCharacter||changedMusic)
            throw std::runtime_error("An older save schema cannot retain VCS seek radii or script music positions.");
    }
    if (version < 63 && !archive.reading && !state.environment.vcsCharacterObjectParents.empty())
        throw std::runtime_error("An older save schema cannot retain native character-object parent references.");
    if (version < 62 && !archive.reading && !defaultVcsMissionPresentation(state.environment))
        throw std::runtime_error("An older save schema cannot retain VCS footstep or HUD presentation state.");
    if (version < 62 && !archive.reading && state.environment.drunkVisualIntensity != 0 &&
        state.environment.drunkVisualPeriod > 16384)
        throw std::runtime_error("An older save schema cannot retain the active native intoxication period.");
    if(version<61&&!archive.reading) {
        const auto defaultFlags=[](const StorylandVcsCharacterScriptFlags& flags) {
            return flags.evasiveDirection==0&&flags.acceptsPairedActions;
        };
        if(state.environment.playerScriptRunning||!defaultFlags(state.environment.playerVcsScriptFlags)||
            std::any_of(state.environment.missionCharacters.begin(),state.environment.missionCharacters.end(),
                [&](const auto& entry){return !defaultFlags(entry.second.vcsScriptFlags);}))
            throw std::runtime_error("An older save schema cannot retain native character script flags.");
    }
    if(version<60)for(auto& vehicle:state.vehicles) {
        if(archive.reading)vehicle.vcsJetSkiAxes={};
        else if(vehicle.vcsJetSkiAxes!=std::array<float,4>{})
            throw std::runtime_error("An older save schema cannot retain native JetSki axes.");
    }
    if(version<59) {
        if(archive.reading) {
            state.environment.playerWalkTask.moveMode=StorylandScriptPlayerMoveMode::Walk;
            state.environment.vcsPedColorSetCursors.clear();
        } else if(state.environment.playerWalkTask.moveMode!=StorylandScriptPlayerMoveMode::Walk||
            !state.environment.vcsPedColorSetCursors.empty())
            throw std::runtime_error("An older save schema cannot retain native gait or colour cursors.");
    }
    // Retail saves preserve script offsets and globals, then independent world/player pools.
    // No process pointers, renderer objects or host handles enter the PC file.
    auto pickups=std::move(state.environment.pickups);
    try {
        subsystem(archive,0x00524353,state.mission,state.program); // SCR
        subsystem(archive,0x444c5257,state.environment); // WRLD
        subsystem(archive,0x52594c50,state.inventory,state.confiscated,state.weaponIds,state.equipped,
            state.elapsed,state.fortBaxterHeat,state.buyback,state.armyFatigues,state.buybackPosition); // PLYR
        subsystem(archive,0x50554b50,pickups,state.dropped); // PKUP
        if(version>=3) {
            subsystem(archive,0x534c4856,state.vehicles); // VHLS: model IDs, never model-array indices.
            subsystem(archive,0x54415453,state.environment.statistics); // STAT
            std::map<int32_t,int32_t> statuses,animationGroups;
            if(!archive.reading) {
                for(const auto& entry:state.environment.missionVehicles)statuses[entry.first]=entry.second.scriptStatus;
                for(const auto& entry:state.environment.pedAnimationRequests)animationGroups[entry.first]=entry.second.animationGroup;
            }
            subsystem(archive,0x41525458,statuses,animationGroups,state.environment.importedGarageHandles); // XTRA, preserving old WRLD layout.
            if(archive.reading) {
                for(const auto handle:state.environment.importedGarageHandles)
                    if(std::none_of(state.environment.garages.begin(),state.environment.garages.end(),
                        [&](const auto& garage){return garage.handle==handle;}))throw std::runtime_error("Invalid imported garage handle.");
                for(const auto& entry:statuses) {
                    auto found=state.environment.missionVehicles.find(entry.first);
                    if(found==state.environment.missionVehicles.end()||entry.second<-1||entry.second>31)
                        throw std::runtime_error("Invalid saved vehicle status reference.");
                    found->second.scriptStatus=entry.second;
                }
                for(const auto& entry:animationGroups) {
                    auto found=state.environment.pedAnimationRequests.find(entry.first);
                    if(found==state.environment.pedAnimationRequests.end()||entry.second<-1||entry.second>199)
                        throw std::runtime_error("Invalid saved animation association.");
                    found->second.animationGroup=entry.second;
                }
            }
        }
        if(version>=4) {
            std::map<int32_t,std::array<int32_t,3>> vehicleOverrides;
            if(!archive.reading)for(const auto& entry:state.environment.missionVehicles)
                vehicleOverrides[entry.first]={entry.second.primaryColor,entry.second.secondaryColor,
                    entry.second.healthChangedByScript?1:0};
            subsystem(archive,0x34524856,vehicleOverrides); // VHR4; older WRLD and XTRA layouts stay intact.
            if(archive.reading)for(const auto& entry:vehicleOverrides) {
                auto found=state.environment.missionVehicles.find(entry.first);
                const auto& values=entry.second;
                if(found==state.environment.missionVehicles.end()||values[0]<-1||values[0]>255||
                    values[1]<-1||values[1]>255||values[2]<0||values[2]>1)
                    throw std::runtime_error("Invalid saved vehicle override.");
                found->second.primaryColor=values[0];found->second.secondaryColor=values[1];
                found->second.healthChangedByScript=values[2]!=0;
            }
        }
        if(version>=5) {
            std::map<int32_t,uint8_t> scriptedProofs;
            std::vector<uint8_t> vehicleProofs;
            if(!archive.reading) {
                for(const auto& entry:state.environment.missionVehicles)scriptedProofs[entry.first]=entry.second.damageProofs;
                for(const auto& vehicle:state.vehicles)vehicleProofs.push_back(vehicle.damageProofs);
            }
            subsystem(archive,0x35465250,scriptedProofs,vehicleProofs); // PRF5
            if(archive.reading) {
                if(vehicleProofs.size()!=state.vehicles.size())throw std::runtime_error("Invalid vehicle proof count.");
                for(size_t i=0;i<vehicleProofs.size();++i) {
                    if(vehicleProofs[i]>31)throw std::runtime_error("Invalid vehicle proofs.");
                    state.vehicles[i].damageProofs=vehicleProofs[i];
                }
                for(const auto& entry:scriptedProofs) {
                    auto found=state.environment.missionVehicles.find(entry.first);
                    if(found==state.environment.missionVehicles.end()||entry.second>31)
                        throw std::runtime_error("Invalid script vehicle proofs.");
                    found->second.damageProofs=entry.second;
                }
            }
        }
        if(version>=6)subsystem(archive,0x36504D45,state.environment.empireStatsEnabled,state.environment.empireSites); // EMP6
        if(version>=7) {
            std::array<std::array<int32_t,3>,30> visuals{};
            if(!archive.reading)for(size_t index=0;index<visuals.size();++index) {
                const auto& site=state.environment.empireSites[index];
                visuals[index]={site.type,site.state,site.damaged?1:0};
            }
            subsystem(archive,0x37504D45,visuals); // EMP7
            if(archive.reading)for(size_t index=0;index<visuals.size();++index) {
                auto& site=state.environment.empireSites[index];
                if(visuals[index][2]!=0&&visuals[index][2]!=1)
                    throw std::runtime_error("Invalid empire damage state.");
                site.type=visuals[index][0];site.state=visuals[index][1];site.damaged=visuals[index][2]!=0;
            }
        }
        if(version>=8) {
            subsystem(archive,0x3843554D,state.environment.musicEmitters); // MUC8
            subsystem(archive,0x38504D45,state.environment.empireSitesOwned); // EMP8
        }
        if(version>=9) {
            subsystem(archive,0x394B5354,state.environment.characterWanderTasks,
                state.environment.killFrenzyStatus);
            if(state.environment.killFrenzyStatus>3)
                throw std::runtime_error("Invalid kill frenzy status.");
            for(const auto& task:state.environment.characterWanderTasks)
                if(task.second.direction<0||task.second.direction>7)
                    throw std::runtime_error("Invalid pedestrian wander direction.");
        }
        if(version>=10) {
            subsystem(archive,0x3047534D,state.environment.messageNumbers);
            subsystem(archive,0x30464352,state.environment.upsideDownCarChecks);
            if(state.environment.upsideDownCarChecks.size()>6)
                throw std::runtime_error("Invalid upside-down car check pool.");
            auto deathArrestStates=state.program.deathArrestStates();
            subsystem(archive,0x30415244,state.mission.deathArrestEnabled,
                state.mission.deathArrestExecuted,deathArrestStates);
            if(archive.reading&&!state.program.restoreDeathArrestStates(deathArrestStates))
                throw std::runtime_error("Invalid script death/arrest states.");
            if(state.environment.messageNumbers.size()>6)
                throw std::runtime_error("Invalid numbered message parameters.");
        }
        if(version>=11) {
            std::map<int32_t,std::array<int32_t,4>> characters;
            std::map<int32_t,std::array<float,2>> rotations;
            std::set<int32_t> heavyVehicles;
            if(!archive.reading) {
                for(const auto& entry:state.environment.missionCharacters) {
                    const auto& c=entry.second;
                    characters[entry.first]={c.accuracy,int32_t(c.fearFlags),c.damageProofs,c.passengerSeat};
                }
                for(const auto& object:state.environment.objects)rotations[object.handle]={object.rotationX,object.rotationY};
                for(const auto& entry:state.environment.missionVehicles)if(entry.second.heavy)heavyVehicles.insert(entry.first);
            }
            subsystem(archive,0x31314E45,characters,rotations,heavyVehicles,state.environment.playerDamageProofs);
            if(state.environment.playerDamageProofs>31)throw std::runtime_error("Invalid player damage proofs.");
            if(archive.reading) {
                for(const auto& entry:characters) {
                    auto found=state.environment.missionCharacters.find(entry.first);
                    if(found==state.environment.missionCharacters.end()||entry.second[0]<0||entry.second[0]>100||
                        entry.second[2]<0||entry.second[2]>31||entry.second[3]<-1||entry.second[3]>7)
                        throw std::runtime_error("Invalid saved character attributes.");
                    auto& c=found->second;c.accuracy=uint8_t(entry.second[0]);c.fearFlags=uint32_t(entry.second[1]);
                    c.damageProofs=uint8_t(entry.second[2]);c.passengerSeat=entry.second[3];
                }
                for(auto& object:state.environment.objects) {
                    const auto found=rotations.find(object.handle);
                    if(found!=rotations.end()) {
                        if(!std::isfinite(found->second[0])||!std::isfinite(found->second[1]))throw std::runtime_error("Invalid saved object rotation.");
                        object.rotationX=found->second[0];object.rotationY=found->second[1];
                    }
                }
                for(int32_t handle:heavyVehicles) {
                    auto found=state.environment.missionVehicles.find(handle);
                    if(found==state.environment.missionVehicles.end())throw std::runtime_error("Invalid saved heavy vehicle.");
                    found->second.heavy=true;
                }
            }
        }
        const bool lcsExtensions=(version>=12&&version<=18)||(version>=25&&!state.mission.vcs);
        const bool vcsExtensions=version>=20&&(version<25||state.mission.vcs);
        if(lcsExtensions&&version>=12) {
            auto& world=state.environment;
            std::map<int32_t,LegacyScriptFire> legacyScriptFires;
            if(!archive.reading)for(const auto& entry:world.scriptFires) {
                const auto& fire=entry.second;
                legacyScriptFires[entry.first]={fire.position,fire.attachedKind,fire.attachedHandle,fire.active};
            }
            auto messageWaitStates=state.program.messageWaitStates();
            std::map<int32_t,std::array<bool,2>> scriptVehicleDamage;
            std::map<int32_t,MissionVehicleControl> scriptVehicleControl;
            std::vector<uint8_t> vehicleDamage;
            std::map<int32_t,MissionCharacterTask> tasks;
            if(!archive.reading)for(const auto& entry:world.missionCharacters) {
                const auto& character=entry.second;
                tasks[entry.first]={character.objectiveCompleted,character.targetRadius,
                    character.objectiveTarget,character.positionFrozen,character.onlyDamagedByPlayer,
                    character.suffersCriticalHits,character.canBeShotInVehicle,character.cantBeDraggedOut,
                    character.money,character.moneyGivenByScript,character.deathDropsCreated};
            }
            if(!archive.reading) {
                for(const auto& entry:world.missionVehicles) {
                    scriptVehicleDamage[entry.first]={entry.second.canBeDamaged,entry.second.extinguishFireRequested};
                    const auto& vehicle=entry.second;
                    scriptVehicleControl[entry.first]={vehicle.autoPilotMission,vehicle.autoPilotRoutePending,
                        vehicle.boatAnchored,vehicle.boatAnchorHeading,vehicle.doorLock,vehicle.positionFrozen};
                }
                for(const auto& vehicle:state.vehicles)vehicleDamage.push_back(uint8_t(vehicle.canBeDamaged));
            }
            subsystem(archive,0x3231434C,world.onscreenClock,world.lcsOnscreenCounters,
                world.onscreenTimersFrozen,world.counterColorPending,world.counterBackgroundPending,
                world.specialModelNames,world.specialCharacterSlots,world.payphones,
                world.fortStauntonDestroyedGlobal,world.fortStauntonDestroyed,
                world.storedClockHour,world.storedWeather,world.clockStored,world.weatherStored,
                world.characterWeapons,world.characterCurrentWeapons,tasks,world.messageStyle,
                world.camera.interpolationScripted,world.camera.transitionPending,
                world.camera.pedZoomIndicator,world.buildingModelSwaps,
                world.missionCleanupCharacters,world.missionCleanupVehicles,world.missionCleanupObjects,
                world.storedMissionVehicleHandle,world.storedMissionVehicleWasAmbient,
                world.lcsAnimationBanks,world.lcsWaitAnimations,
                state.mission.messageWaitSkippable,messageWaitStates,
                world.allowedCollisionLevels,world.lcsCollisionLevel,
                world.playerOnlyDamagedByPlayer,world.applyPlayerCarBrakes,scriptVehicleDamage,vehicleDamage,
                world.lcsTextCommandMode,world.lcsScriptTexts,world.lcsNextScriptText,
                world.lcsBuildingSwapGroups,world.lcsEnterCarRangeMultiplier,world.characterVehicleTasks,
                world.lcsMessageQueuesUsed,world.lcsBriefMessages,world.lcsBigMessages,
                world.lcsPreviousBriefs,world.camera.pedZoomSwitchFrozen,
                world.scriptCoronas,world.nextScriptCoronaId,legacyScriptFires,
                world.remoteControl,world.lcsNavigationArrowVisible,
                world.lcsNavigationArrowTarget,world.lcsHudFlashItem,
                world.lcsGarageDoors,world.camera.lcsTrackedKind,world.camera.lcsTrackedHandle,
                world.camera.lcsFrontRequested,scriptVehicleControl,
                world.invisibleScriptObjects,world.invisibleNativeObjects,
                world.characterAttachments,world.playerSuffersCriticalHits,
                world.playerCanBeShotInVehicle,world.playerCantBeDraggedOut,
                world.playerPedMoney,world.playerPedMoneyGivenByScript); // LC12; keep all earlier subsystem layouts intact.
            if(archive.reading) {
                world.scriptFires.clear();
                for(const auto& entry:legacyScriptFires) {
                    auto& fire=world.scriptFires[entry.first];
                    fire.position=entry.second.position;
                    fire.attachedKind=entry.second.attachedKind;
                    fire.attachedHandle=entry.second.attachedHandle;
                    fire.active=entry.second.active;
                }
            }
            if(archive.reading&&!state.program.restoreMessageWaitStates(messageWaitStates))
                throw std::runtime_error("Invalid script message-wait states.");
            if(world.lcsCollisionLevel<0||world.lcsCollisionLevel>3||
                world.allowedCollisionLevels[0]<0||world.allowedCollisionLevels[0]>3||
                world.allowedCollisionLevels[1]<0||world.allowedCollisionLevels[1]>3)
                throw std::runtime_error("Invalid saved LCS collision level.");
            if(world.lcsTextCommandMode>2||world.lcsScriptTexts.size()>48)
                throw std::runtime_error("Invalid saved LCS script text queue.");
            const auto textValid=[](const StorylandMissionEnvironment::LcsScriptText& text) {
                return text.key.size()<=8&&text.missionText.size()<=8&&text.numbers.size()<=2;
            };
            if(!textValid(world.lcsNextScriptText)||std::any_of(world.lcsScriptTexts.begin(),world.lcsScriptTexts.end(),
                [&](const auto& text){return !textValid(text);}))
                throw std::runtime_error("Invalid saved LCS script text.");
            if(world.lcsBuildingSwapGroups.size()>256)
                throw std::runtime_error("Invalid saved LCS building group count.");
            for(const auto& group:world.lcsBuildingSwapGroups)
                if(group.first>255||group.second<0||group.second>255)
                    throw std::runtime_error("Invalid saved LCS building group state.");
            if(world.characterVehicleTasks.size()>world.missionCharacters.size())
                throw std::runtime_error("Invalid saved LCS vehicle task count.");
            const auto vehicleValid=[&](int32_t handle) {
                return handle==-1||world.missionVehicles.count(handle)!=0;
            };
            for(const auto& entry:world.characterVehicleTasks) {
                const auto& task=entry.second;
                if(!world.missionCharacters.count(entry.first)||!vehicleValid(task.vehicle)||
                    !vehicleValid(task.destinationVehicle)||task.seat<-1||task.seat>7||
                    task.destinationSeat<-1||task.destinationSeat>7||task.route.size()>64||
                    task.routeIndex>task.route.size()||std::any_of(task.phaseSeconds.begin(),task.phaseSeconds.end(),
                        [](float seconds){return seconds<0;}))
                    throw std::runtime_error("Invalid saved LCS character vehicle task.");
            }
            const auto messagesValid=[](const auto& messages,size_t maximum) {
                if(messages.size()>maximum)return false;
                return std::all_of(messages.begin(),messages.end(),[](const auto& message) {
                    return message.key.size()<=8&&message.missionText.size()<=8&&
                        message.stringKey.size()<=8&&message.numbers.size()<=6;
                });
            };
            if(!messagesValid(world.lcsBriefMessages,8)||!messagesValid(world.lcsPreviousBriefs,5)||
                std::any_of(world.lcsBigMessages.begin(),world.lcsBigMessages.end(),
                    [&](const auto& messages){return !messagesValid(messages,4);}))
                throw std::runtime_error("Invalid saved LCS message queue.");
            if(world.scriptCoronas.size()>4096||world.scriptFires.size()>40||world.nextScriptCoronaId<0)
                throw std::runtime_error("Invalid saved LCS effect pool.");
            for(const auto& entry:world.scriptCoronas)
                if(entry.first<0||entry.first>=world.nextScriptCoronaId||entry.second.type<0||entry.second.type>8||
                    entry.second.flareType<0||entry.second.flareType>2)
                    throw std::runtime_error("Invalid saved LCS corona.");
            for(const auto& entry:world.scriptFires) {
                const auto& fire=entry.second;
                if(entry.first<0||entry.first>=40||fire.attachedKind<0||fire.attachedKind>2||
                    (fire.active&&fire.attachedKind==1&&fire.attachedHandle!=1&&!world.missionCharacters.count(fire.attachedHandle))||
                    (fire.active&&fire.attachedKind==2&&!world.missionVehicles.count(fire.attachedHandle)))
                    throw std::runtime_error("Invalid saved LCS fire attachment.");
            }
            if(!vehicleValid(world.remoteControl.vehicle))
                throw std::runtime_error("Invalid saved remote-control vehicle.");
            if(world.camera.lcsTrackedKind<0||world.camera.lcsTrackedKind>3||
                (world.camera.lcsTrackedKind==1&&world.camera.lcsTrackedHandle!=0)||
                (world.camera.lcsTrackedKind==2&&world.camera.lcsTrackedHandle!=1&&!world.missionCharacters.count(world.camera.lcsTrackedHandle))||
                (world.camera.lcsTrackedKind==3&&!world.missionVehicles.count(world.camera.lcsTrackedHandle)))
                throw std::runtime_error("Invalid saved LCS camera tracking target.");
            if(world.characterAttachments.size()>world.missionCharacters.size()+1)
                throw std::runtime_error("Invalid saved character attachment pool.");
            for(const auto& entry:world.characterAttachments)
                if((entry.first!=1&&!world.missionCharacters.count(entry.first))||entry.second.rotationLimit<0||
                    entry.second.savedWeapon<-1||entry.second.savedWeapon>37||entry.second.savedAmmo<0)
                    throw std::runtime_error("Invalid saved character attachment.");
            if(world.lcsGarageDoors.size()>world.garages.size())
                throw std::runtime_error("Invalid saved LCS garage-door pool.");
            for(const auto& entry:world.lcsGarageDoors) {
                const auto& state=entry.second;
                const auto garage=std::find_if(world.garages.begin(),world.garages.end(),
                    [&](const auto& candidate){return candidate.handle==entry.first;});
                const bool crusher=garage!=world.garages.end()&&garage->type==13;
                const bool validState=crusher?(state.state==0||state.state==1||state.state==2||
                    state.state==3||state.state==6||state.state==7):state.state>=0&&state.state<=3;
                if(garage==world.garages.end()||!validState||state.height<=0||state.position<0||
                    state.position>state.height||state.doors.size()>2||
                    (crusher&&state.height!=1.5707963267948966192f))
                    throw std::runtime_error("Invalid saved LCS garage-door state.");
                std::set<uint64_t> keys;
                for(const auto& door:state.doors)
                    if(!keys.insert(door.worldKey).second||door.model>uint32_t(INT32_MAX)||
                        door.originalMatrix[3]!=0||door.originalMatrix[7]!=0||door.originalMatrix[11]!=0||door.originalMatrix[15]!=1)
                        throw std::runtime_error("Invalid saved LCS garage-door identity or matrix.");
            }
            if(scriptVehicleDamage.size()!=world.missionVehicles.size()||scriptVehicleControl.size()!=world.missionVehicles.size()||
                vehicleDamage.size()!=state.vehicles.size())
                throw std::runtime_error("Invalid saved vehicle damage state count.");
            for(const auto& entry:scriptVehicleControl) {
                const auto found=world.missionVehicles.find(entry.first);
                const auto& control=entry.second;
                if(found==world.missionVehicles.end()||(control.mission!=-1&&control.mission!=0&&control.mission!=1&&
                    control.mission!=10&&control.mission!=11&&!(version>=13&&control.mission==14))||
                    (control.doorLock!=0&&control.doorLock!=1&&control.doorLock!=2&&control.doorLock!=3&&
                     control.doorLock!=4&&control.doorLock!=5&&control.doorLock!=9))
                    throw std::runtime_error("Invalid saved native vehicle control state.");
                if(archive.reading) {
                    found->second.autoPilotMission=control.mission;found->second.autoPilotRoutePending=control.routePending;
                    found->second.boatAnchored=control.anchored;found->second.boatAnchorHeading=control.anchorHeading;
                    found->second.doorLock=control.doorLock;found->second.positionFrozen=control.positionFrozen;
                }
            }
            for(const auto& entry:scriptVehicleDamage) {
                const auto found=world.missionVehicles.find(entry.first);
                if(found==world.missionVehicles.end())throw std::runtime_error("Invalid saved vehicle damage state reference.");
                if(archive.reading) {
                    found->second.canBeDamaged=entry.second[0];
                    found->second.extinguishFireRequested=entry.second[1];
                }
            }
            for(size_t index=0;index<vehicleDamage.size();++index) {
                if(vehicleDamage[index]>1)throw std::runtime_error("Invalid saved native vehicle damage state.");
                if(archive.reading)state.vehicles[index].canBeDamaged=vehicleDamage[index]!=0;
            }
            const auto globalValid=[&](int32_t variable) {
                return variable==-1||(variable>=0&&size_t(variable)<state.mission.globals.size());
            };
            if(!globalValid(world.onscreenClock.variable)||world.onscreenClock.label.size()>8||
                !globalValid(world.fortStauntonDestroyedGlobal))
                throw std::runtime_error("Invalid saved LCS global reference.");
            for(const auto& counter:world.lcsOnscreenCounters)
                if(!globalValid(counter.variable)||counter.type<0||counter.type>65535||
                    counter.totalType<0||counter.totalType>65535||counter.label.size()>8||counter.totalLabel.size()>8)
                    throw std::runtime_error("Invalid saved onscreen counter.");
            if(world.specialModelNames.size()>4096||world.specialCharacterSlots.size()>21||world.payphones.size()>64||
                world.messageStyle<0||world.messageStyle>8)
                throw std::runtime_error("Invalid saved LCS subsystem size or message style.");
            for(const auto& entry:world.specialModelNames)
                if(entry.first<0||entry.second.empty()||entry.second.size()>8)
                    throw std::runtime_error("Invalid saved special model binding.");
            for(const auto& entry:world.specialCharacterSlots)
                if(entry.first<1||entry.first>21||world.specialModelNames.find(entry.second)==world.specialModelNames.end())
                    throw std::runtime_error("Invalid saved special character slot.");
            std::set<uint64_t> phoneKeys;
            for(const auto& entry:world.payphones)
                if(entry.first<0||entry.first>=64||!phoneKeys.insert(entry.second.worldKey).second)
                    throw std::runtime_error("Invalid saved payphone reference.");
            for(const auto& entry:world.buildingModelSwaps)
                if(entry.second.originalModel>uint32_t(INT32_MAX)||entry.second.replacementModel>uint32_t(INT32_MAX))
                    throw std::runtime_error("Invalid saved building model replacement.");
            for(int32_t handle:world.missionCleanupCharacters)
                if(!world.missionCharacters.count(handle))
                    throw std::runtime_error("Invalid saved mission character cleanup reference.");
            for(int32_t handle:world.missionCleanupVehicles)
                if(!world.missionVehicles.count(handle))
                    throw std::runtime_error("Invalid saved mission vehicle cleanup reference.");
            for(int32_t handle:world.missionCleanupObjects)
                if(std::none_of(world.objects.begin(),world.objects.end(),
                    [&](const auto& object){return object.handle==handle;}))
                    throw std::runtime_error("Invalid saved mission object cleanup reference.");
            if(world.storedMissionVehicleHandle<-1||(world.storedMissionVehicleHandle>=0&&
                !world.missionVehicles.count(world.storedMissionVehicleHandle)))
                throw std::runtime_error("Invalid saved stored mission vehicle reference.");
            if(world.lcsAnimationBanks.size()>StorylandLcsWaitAnimations.size()+1||
                world.lcsWaitAnimations.size()>world.missionCharacters.size()+1)
                throw std::runtime_error("Invalid saved LCS animation pool.");
            for(const auto& bank:world.lcsAnimationBanks)
                if(bank!="ped"&&storylandLcsAnimationBankClips(bank).empty())
                    throw std::runtime_error("Invalid saved LCS animation bank.");
            for(const auto& entry:world.lcsWaitAnimations) {
                const auto* definition=storylandLcsWaitAnimation(entry.second.state);
                if((entry.first!=1&&!world.missionCharacters.count(entry.first))||!definition||
                    ((entry.second.state==3||entry.second.state==14||entry.second.state==19||entry.second.state==24||entry.second.state==25||entry.second.state==35)?entry.second.durationMilliseconds<0:
                        entry.second.durationMilliseconds<-1||entry.second.durationMilliseconds==0||
                        (version<13&&entry.second.durationMilliseconds==-1&&entry.second.state!=37))||
                    !world.lcsAnimationBanks.count(definition->bank))
                    throw std::runtime_error("Invalid saved LCS wait-state animation.");
            }
            if(world.characterWeapons.size()>world.missionCharacters.size()||
                world.characterCurrentWeapons.size()>world.missionCharacters.size())
                throw std::runtime_error("Invalid saved character weapon pool.");
            for(const auto& character:world.characterWeapons) {
                if(!world.missionCharacters.count(character.first)||character.second.size()>256)
                    throw std::runtime_error("Invalid saved character weapon owner.");
                for(const auto& entry:character.second) {
                    const auto& weapon=entry.second;
                    if(entry.first<0||entry.first>255||weapon.slot<-1||weapon.slot>31||weapon.model<-1||
                        weapon.ammunition<0)
                        throw std::runtime_error("Invalid saved character weapon.");
                }
            }
            for(const auto& entry:world.characterCurrentWeapons) {
                if(!world.missionCharacters.count(entry.first)||entry.second<0||entry.second>255)
                    throw std::runtime_error("Invalid saved character weapon selection.");
                if(entry.second>0) {
                    const auto inventory=world.characterWeapons.find(entry.first);
                    if(inventory==world.characterWeapons.end())
                        throw std::runtime_error("Saved character weapon has no inventory.");
                    const auto weapon=inventory->second.find(entry.second);
                    if(weapon==inventory->second.end()||!weapon->second.owned)
                        throw std::runtime_error("Saved character selected an unowned weapon.");
                }
            }
            if(tasks.size()!=world.missionCharacters.size())
                throw std::runtime_error("Invalid saved character task count.");
            for(const auto& entry:tasks) {
                const auto found=world.missionCharacters.find(entry.first);
                if(found==world.missionCharacters.end()||entry.second.radius<0||entry.second.target<-1)
                    throw std::runtime_error("Invalid saved character task.");
                if(archive.reading) {
                    auto& character=found->second;
                    character.objectiveCompleted=entry.second.completed;
                    character.targetRadius=entry.second.radius;
                    character.objectiveTarget=entry.second.target;
                    character.positionFrozen=entry.second.frozen;
                    character.onlyDamagedByPlayer=entry.second.onlyDamagedByPlayer;
                    character.suffersCriticalHits=entry.second.suffersCriticalHits;
                    character.canBeShotInVehicle=entry.second.canBeShotInVehicle;
                    character.cantBeDraggedOut=entry.second.cantBeDraggedOut;
                    character.money=entry.second.money;character.moneyGivenByScript=entry.second.moneyGivenByScript;
                    character.deathDropsCreated=entry.second.deathDropsCreated;
                }
            }
            if(archive.reading) {
                world.buildingModelSwapsChanged=!world.buildingModelSwaps.empty();
                world.nativeObjectVisibilityChanged=!world.invisibleNativeObjects.empty();
                for(const auto& entry:world.specialModelNames) {
                    world.residentModels.erase(entry.first);
                    world.requestedModels.insert(entry.first);
                    world.changedSpecialModels.insert(entry.first);
                }
            }
        }
        if(lcsExtensions&&version>=13) {
            auto& world=state.environment;
            std::map<int32_t,StorylandPedStats> personalities;
            std::map<int32_t,bool> engines;
            std::map<int32_t,uint8_t> drivingStyles;
            std::map<int32_t,bool> vehicleDamageOwners,waitRepeats;
            std::vector<uint8_t> runtimeDamageOwners;
            if(!archive.reading) {
                for(const auto& entry:world.missionCharacters)
                    if(entry.second.pedStats.valid)personalities.emplace(entry.first,entry.second.pedStats);
                for(const auto& entry:world.missionVehicles) {
                    engines.emplace(entry.first,entry.second.engineOn);
                    drivingStyles.emplace(entry.first,entry.second.drivingStyle);
                    vehicleDamageOwners.emplace(entry.first,entry.second.onlyDamagedByPlayer);
                }
                for(const auto& entry:world.lcsWaitAnimations)waitRepeats.emplace(entry.first,entry.second.repeat);
                for(const auto& vehicle:state.vehicles)runtimeDamageOwners.push_back(uint8_t(vehicle.onlyDamagedByPlayer));
            }
            subsystem(archive,0x3331434C,personalities,engines,drivingStyles,
                vehicleDamageOwners,runtimeDamageOwners,waitRepeats); // LC13; retain the previously shipped LC12 block.
            if(personalities.size()>world.missionCharacters.size()||engines.size()!=world.missionVehicles.size()||
                drivingStyles.size()!=world.missionVehicles.size()||vehicleDamageOwners.size()!=world.missionVehicles.size()||
                runtimeDamageOwners.size()!=state.vehicles.size()||waitRepeats.size()!=world.lcsWaitAnimations.size())
                throw std::runtime_error("Invalid saved LCS personality or engine pool.");
            for(const auto& entry:personalities) {
                const auto found=world.missionCharacters.find(entry.first);
                if(found==world.missionCharacters.end()||!storylandValidLcsPedStats(entry.second)||
                    entry.second.modelId!=uint32_t(found->second.model))
                    throw std::runtime_error("Invalid saved LCS pedestrian personality.");
                if(archive.reading)found->second.pedStats=entry.second;
            }
            for(const auto& entry:engines) {
                const auto found=world.missionVehicles.find(entry.first);
                if(found==world.missionVehicles.end())throw std::runtime_error("Invalid saved boat engine reference.");
                if(archive.reading)found->second.engineOn=entry.second;
            }
            for(const auto& entry:drivingStyles) {
                const auto found=world.missionVehicles.find(entry.first);
                if(found==world.missionVehicles.end()||(entry.second!=0&&entry.second!=1&&entry.second!=4))
                    throw std::runtime_error("Invalid saved car driving style.");
                if(archive.reading)found->second.drivingStyle=entry.second;
            }
            for(const auto& entry:vehicleDamageOwners) {
                const auto found=world.missionVehicles.find(entry.first);
                if(found==world.missionVehicles.end())throw std::runtime_error("Invalid saved vehicle damage owner.");
                if(archive.reading)found->second.onlyDamagedByPlayer=entry.second;
            }
            for(size_t index=0;index<runtimeDamageOwners.size();++index) {
                if(runtimeDamageOwners[index]>1)throw std::runtime_error("Invalid saved runtime vehicle damage owner.");
                if(archive.reading)state.vehicles[index].onlyDamagedByPlayer=runtimeDamageOwners[index]!=0;
            }
            for(const auto& entry:waitRepeats) {
                const auto found=world.lcsWaitAnimations.find(entry.first);
                if(found==world.lcsWaitAnimations.end())throw std::runtime_error("Invalid saved repeating animation reference.");
                if(archive.reading)found->second.repeat=entry.second;
                const auto& animation=found->second;
                const bool seatedTransition=animation.state==21||animation.state==23;
                if((animation.state<36&&animation.durationMilliseconds<0)||
                    (seatedTransition&&(animation.durationMilliseconds==0||
                        (animation.repeat&&animation.durationMilliseconds!=100000)))||
                    (animation.state>=36&&((animation.repeat||animation.state==37)?
                        animation.durationMilliseconds!=-1:animation.durationMilliseconds<=0)))
                    throw std::runtime_error("Invalid saved repeating animation duration.");
            }
        }
        if(lcsExtensions&&version>=14) {
            auto& world=state.environment;
            auto foreground=state.mission.outcomeState();
            auto threads=state.program.outcomeStates();
            subsystem(archive,0x3431434C,world.missionResult,world.lastMissionPassedName,
                world.failCurrentMission,foreground,threads);
            const auto& result=world.missionResult;
            if(unsigned(result.outcome)>unsigned(StorylandMissionOutcome::Failed)||
                unsigned(result.failure)>unsigned(StorylandMissionFailure::Arrest)||
                result.missionSlot<-1||result.missionSlot>65535||
                result.scriptName.size()>256||result.key.size()>256||result.reason.size()>1024||
                world.lastMissionPassedName.size()>256||world.failCurrentMission>2||
                ((result.outcome==StorylandMissionOutcome::Failed)!=
                    (result.failure!=StorylandMissionFailure::None))||
                (result.outcome==StorylandMissionOutcome::None&&
                    (result.sequence!=0||result.missionSlot!=-1||!result.scriptName.empty()||
                        !result.key.empty()||!result.reason.empty()||result.gameMilliseconds!=0))||
                (result.outcome!=StorylandMissionOutcome::None&&result.sequence==0))
                throw std::runtime_error("Invalid saved mission result.");
            if(!state.mission.restoreOutcomeState(foreground)||
                !state.program.restoreOutcomeStates(threads)||
                foreground.sequence>result.sequence||foreground.missionSlot>65535)
                throw std::runtime_error("Invalid saved mission execution outcome.");
            for(const auto& thread:threads)
                if(thread.sequence>result.sequence||thread.missionSlot!=-1)
                    throw std::runtime_error("Invalid saved background mission outcome.");
            if(state.program.activeMission()>=0&&
                (foreground.missionSlot!=state.program.activeMission()||
                    !foreground.internalMission||!state.mission.isMissionThread))
                throw std::runtime_error("Saved mission result belongs to a different mission slot.");
        } else if(archive.reading) {
            state.program.initializeLegacyOutcomes(state.mission);
        }
        if(lcsExtensions&&version>=15) {
            auto& world=state.environment;
            std::vector<std::pair<bool,StorylandLcsCraneMatrix>> carriedFrames;
            if(!archive.reading)for(const auto& vehicle:state.vehicles)
                carriedFrames.emplace_back(vehicle.craneCarried,vehicle.craneFrame);
            subsystem(archive,0x3531434C,world.lcsStuckCarChecks,world.lcsCranes,carriedFrames,
                world.lcsGarageFollowPlayer); // LC15
            if(!storylandValidLcsCraneSystem(world.lcsCranes)||carriedFrames.size()!=state.vehicles.size())
                throw std::runtime_error("Invalid saved crane subsystem.");
            for(int32_t handle:world.lcsGarageFollowPlayer)
                if(std::none_of(world.garages.begin(),world.garages.end(),
                    [&](const auto& garage){return garage.handle==handle;}))
                    throw std::runtime_error("Invalid saved garage camera reference.");
            for(const auto& check:world.lcsStuckCarChecks) {
                if(check.vehicle<-1||!std::isfinite(check.radius)||
                    !std::all_of(check.position.begin(),check.position.end(),
                        [](float value){return std::isfinite(value);}))
                    throw std::runtime_error("Invalid saved stuck-car check.");
                if(check.vehicle==-1&&(check.position!=std::array<float,3>{-5000,-5000,-5000}||
                    check.lastCheck!=UINT32_MAX||check.radius!=0||check.interval!=0||check.stuck))
                    throw std::runtime_error("Invalid empty stuck-car slot.");
            }
            for(const auto& crane:world.lcsCranes.cranes) {
                if(crane.vehicle<0)continue;
                const auto target=world.missionVehicles.find(crane.vehicle);
                if(target==world.missionVehicles.end())
                    throw std::runtime_error("Invalid saved crane vehicle reference.");
                const bool carrying=crane.state==StorylandLcsCrane::Lifting||
                    crane.state==StorylandLcsCrane::Raising||crane.state==StorylandLcsCrane::Aligning;
                if(carrying&&!target->second.deleted) {
                    const auto physical=std::find_if(state.vehicles.begin(),state.vehicles.end(),
                        [&](const auto& vehicle){return vehicle.scriptHandle==crane.vehicle;});
                    if(physical==state.vehicles.end()||
                        !carriedFrames[size_t(physical-state.vehicles.begin())].first)
                        throw std::runtime_error("Saved crane has no carried physical vehicle.");
                }
            }
            for(size_t index=0;index<carriedFrames.size();++index) {
                const auto& frame=carriedFrames[index];
                auto& vehicle=state.vehicles[index];
                if(!storylandLcsCraneFiniteMatrix(frame.second))
                    throw std::runtime_error("Invalid saved crane vehicle transform.");
                if(frame.first&&std::none_of(world.lcsCranes.cranes.begin(),world.lcsCranes.cranes.end(),
                    [&](const auto& crane) {
                        return crane.vehicle==vehicle.scriptHandle&&crane.vehicle>=0&&
                            (crane.state==StorylandLcsCrane::Lifting||crane.state==StorylandLcsCrane::Raising||
                                crane.state==StorylandLcsCrane::Aligning);
                    }))throw std::runtime_error("Carried vehicle has no saved crane owner.");
                if(archive.reading) {vehicle.craneCarried=frame.first;vehicle.craneFrame=frame.second;}
            }
        }
        if(lcsExtensions&&version>=16) {
            auto& world=state.environment;
            std::map<int32_t,MissionCharacterMotion> characters;
            if(!archive.reading)for(const auto& entry:world.missionCharacters) {
                const auto& ped=entry.second;
                characters[entry.first]={ped.movementSpeed,ped.walkedDistance,ped.movementState,ped.deathStartedAt,ped.deathRecorded};
            }
            subsystem(archive,0x3631434C,characters); // LC16; preserve the original character record.
            if(characters.size()!=world.missionCharacters.size())
                throw std::runtime_error("Invalid saved character motion pool.");
            for(const auto& entry:characters) {
                const auto found=world.missionCharacters.find(entry.first);
                const auto& motion=entry.second;
                if(found==world.missionCharacters.end()||motion.speed<0||motion.distance<0||motion.movementState>4||
                    (!motion.deathRecorded&&motion.deathStartedAt!=0)||
                    (motion.deathRecorded&&found->second.alive&&found->second.health>0))
                    throw std::runtime_error("Invalid saved character motion.");
                if(archive.reading) {
                    auto& ped=found->second;
                    ped.movementSpeed=motion.speed;ped.walkedDistance=motion.distance;
                    ped.movementState=motion.movementState;
                    ped.deathStartedAt=motion.deathStartedAt;ped.deathRecorded=motion.deathRecorded;
                }
            }
        } else if(archive.reading) {
            for(auto& entry:state.environment.missionCharacters) {
                auto& ped=entry.second;
                ped.movementSpeed=0;ped.walkedDistance=0;ped.movementState=0;
                ped.deathRecorded=!ped.alive||ped.health<=0;
                ped.deathStartedAt=ped.deathRecorded?state.environment.gameMilliseconds-60000u:0;
            }
        }
        if(lcsExtensions&&version>=17) {
            auto& world=state.environment;
            std::vector<uint8_t> roadModes;
            std::map<int32_t,bool> avoidLevelTransitions;
            std::vector<float> roadWidths;
            if(!archive.reading) {
                for(const auto& road:world.roadChanges) {
                    roadModes.push_back(uint8_t(road.mode));roadWidths.push_back(road.width);
                }
                for(const auto& entry:world.missionVehicles)
                    avoidLevelTransitions.emplace(entry.first,entry.second.avoidLevelTransitions);
            }
            subsystem(archive,0x3731434C,roadModes,avoidLevelTransitions,roadWidths); // LC17; keep WRLD road and vehicle records unchanged.
            if(roadModes.size()!=world.roadChanges.size()||roadWidths.size()!=world.roadChanges.size()||
                avoidLevelTransitions.size()!=world.missionVehicles.size())
                throw std::runtime_error("Invalid saved road transition pool.");
            for(size_t index=0;index<roadModes.size();++index) {
                auto& road=world.roadChanges[index];
                const bool angled=roadModes[index]==uint8_t(StorylandRoadChange::Mode::AngledAvailability);
                const float length=std::hypot(road.bounds[3]-road.bounds[0],road.bounds[4]-road.bounds[1]);
                if(roadModes[index]>uint8_t(StorylandRoadChange::Mode::AngledAvailability)||
                    (roadModes[index]==uint8_t(StorylandRoadChange::Mode::LevelTransitions)&&!road.vehicle)||
                    (angled&&(road.vehicle||road.enabled||!std::isfinite(length)||length<=0||road.bounds[2]>road.bounds[5]))||
                    (!angled&&roadWidths[index]!=0))
                    throw std::runtime_error("Invalid saved road transition mode.");
                if(archive.reading) {
                    road.mode=StorylandRoadChange::Mode(roadModes[index]);road.width=roadWidths[index];
                }
            }
            for(const auto& entry:avoidLevelTransitions) {
                const auto found=world.missionVehicles.find(entry.first);
                if(found==world.missionVehicles.end())throw std::runtime_error("Invalid saved road transition vehicle reference.");
                if(archive.reading)found->second.avoidLevelTransitions=entry.second;
            }
        } else if(archive.reading) {
            for(auto& road:state.environment.roadChanges) {
                road.mode=StorylandRoadChange::Mode::Availability;road.width=0;
            }
            for(auto& entry:state.environment.missionVehicles)entry.second.avoidLevelTransitions=false;
        }
        if(lcsExtensions&&version>=18) {
            auto& world=state.environment;
            subsystem(archive,0x3831434C,world.restartOverrideEnabled,world.restartOverride,
                world.frenchGame,world.germanGame); // LC18; retain WRLD restart and region compatibility.
            if(world.frenchGame&&world.germanGame)
                throw std::runtime_error("Invalid saved LCS script region.");
            if(!state.mission.vcs&&(world.hospitalRestarts.size()>8||world.policeRestarts.size()>8))
                throw std::runtime_error("Saved LCS restart pool exceeds native capacity.");
        } else if(archive.reading) {
            state.environment.restartOverrideEnabled=false;
            state.environment.restartOverride={};
            state.environment.frenchGame=false;
            state.environment.germanGame=false;
        }
        if(vcsExtensions&&version>=20) {RecoveryExtensions extension{state,version};subsystem(archive,0x30324353,extension);} // SC20
        if(vcsExtensions&&version>=21) {
            OpcodeBatchExtensions extension{state};
            subsystem(archive,0x31324353,extension); // SC21
        }
        if (vcsExtensions && version >= 22) {
            SecondOpcodeBatchExtensions extension{state};
            subsystem(archive, 0x32324353, extension); // SC22
        }
        if (vcsExtensions && version >= 23) {
            ThirdOpcodeBatchExtensions extension{state};
            subsystem(archive, 0x33324353, extension); // SC23
        }
        if (vcsExtensions && version >= 24) {
            FourthOpcodeBatchExtensions extension{state,version};
            subsystem(archive,0x34324353,extension); // SC24
        }
        if(vcsExtensions&&version>=26) {
            auto& world=state.environment;
            std::map<int32_t,std::array<bool,2>> bombs;
            if(!archive.reading)for(const auto& entry:world.missionVehicles)
                bombs.emplace(entry.first,std::array<bool,2>{entry.second.betaBombInstalled,entry.second.bombCountdownActive});
            subsystem(archive,0x36324353,state.adrenaline,state.killFrenzy,state.betaVehicleBombsEnabled,bombs); // SC26
            if(bombs.size()!=world.missionVehicles.size())
                throw std::runtime_error("Invalid saved bomb pool.");
            for(const auto& entry:bombs) {
                const auto found=world.missionVehicles.find(entry.first);
                if(found==world.missionVehicles.end())throw std::runtime_error("Invalid saved bomb vehicle.");
                auto& car=found->second;
                const bool installed=entry.second[0],countdown=entry.second[1];
                if((installed&&(!state.betaVehicleBombsEnabled||
                    (car.bombType!=1&&car.bombType!=2&&car.bombType!=4&&car.bombType!=5)))||
                    (countdown&&car.bombType!=4&&car.bombType!=5))
                    throw std::runtime_error("Invalid saved bomb state.");
                if(archive.reading) {car.betaBombInstalled=installed;car.bombCountdownActive=countdown;}
            }
            const auto& adrenaline=state.adrenaline;
            const uint32_t remaining=adrenaline.expiresAt-world.gameMilliseconds;
            if(adrenaline.pickupHandle < -1||
                (adrenaline.active&&(!adrenaline.enabled||!remaining||
                    remaining>StorylandAdrenalineState::durationMilliseconds))||
                (!adrenaline.active&&adrenaline.expiresAt!=0))
                throw std::runtime_error("Invalid saved adrenaline effect.");
            if(adrenaline.pickupHandle>=0) {
                const auto pickup=std::find_if(pickups.begin(),pickups.end(),
                    [&](const auto& value){return value.handle==adrenaline.pickupHandle;});
                if(pickup==pickups.end()||pickup->type!=3||pickup->model<0||
                    (!adrenaline.enabled&&!pickup->removed))
                    throw std::runtime_error("Invalid saved adrenaline pickup.");
            }
            const auto& frenzy=state.killFrenzy;
            const auto& grant=frenzy.grant;
            if(frenzy.status>3||frenzy.kills<0||frenzy.targetKills<0||frenzy.targetKills>100000||
                frenzy.kills>frenzy.targetKills||frenzy.text.size()>128||
                (frenzy.timeLimitMilliseconds<0&&frenzy.timeLimitMilliseconds!=-1)||
                frenzy.timeLimitMilliseconds>3600000||
                (frenzy.status&&(frenzy.targetKills==0||frenzy.nativeWeapon<0||
                    frenzy.timeLimitMilliseconds==0||world.killFrenzyStatus!=frenzy.status))||
                (frenzy.active()&&(!grant.enabled||(frenzy.custom&&(!frenzy.enabled||!frenzy.onlyHeadshots))||
                    (frenzy.timeLimitMilliseconds>0&&world.gameMilliseconds-frenzy.startedAt>=
                        uint32_t(frenzy.timeLimitMilliseconds))))||
                (grant.enabled&&!frenzy.active()))
                throw std::runtime_error("Invalid saved rampage.");
            if(grant.remainingGranted<0||grant.observedAmmunition<0||grant.observedAmmunition>19998||
                grant.remainingGranted>grant.observedAmmunition||grant.previousWeapon < -1||
                (grant.enabled&&grant.nativeWeapon!=frenzy.nativeWeapon)||
                (!grant.enabled&&(grant.originallyOwned||grant.externalGrant||grant.nativeWeapon!=-1||
                    grant.remainingGranted||grant.observedAmmunition||grant.previousWeapon!=-1)))
                throw std::runtime_error("Invalid saved rampage weapon grant.");
            if(grant.enabled) {
                const auto weapon=std::find(state.weaponIds.begin(),state.weaponIds.end(),grant.nativeWeapon);
                if(weapon==state.weaponIds.end())throw std::runtime_error("Saved rampage weapon is unavailable.");
                const size_t slot=size_t(weapon-state.weaponIds.begin());
                if(slot>=state.inventory.size()||state.inventory[slot].ammunition<0||
                    (slot<state.confiscated.size()&&state.confiscated[slot].ammunition<0))
                    throw std::runtime_error("Invalid saved rampage inventory.");
                const int64_t ammunition=int64_t(state.inventory[slot].ammunition)+
                    (slot<state.confiscated.size()?state.confiscated[slot].ammunition:0);
                if(ammunition!=grant.observedAmmunition)
                    throw std::runtime_error("Saved rampage grant does not match the inventory.");
            }
        } else if(archive.reading) {
            state.adrenaline={};state.killFrenzy={};state.betaVehicleBombsEnabled=false;
            for(auto& entry:state.environment.missionVehicles) {
                auto& car=entry.second;
                car.betaBombInstalled=false;
                car.bombCountdownActive=car.bombType==4&&car.bombDeadline!=0;
            }
        }
        if(version>=27) {
            RadarTraceExtensions extension{state.environment,state.restartTaxi};
            subsystem(archive,0x37324353,extension); // SC27; WRLD and VHLS retain their original layouts.
        } else if(archive.reading) {
            auto& world=state.environment;
            state.restartTaxi={};
            world.radarBlipGenerations.fill(0);
            world.playerCanDoDriveBy=true;
            world.playerInStadium=false;
            world.restartMissionTaxiDestination={};
            world.restartMissionTaxiDestinationSet=false;
            world.playerWalkTask.clear();
            world.playerVehicleEntryTarget=-1;
            world.playerVehicleEntryAsPassenger=false;
            if(!(vcsExtensions&&version>=20))world.playerAllowFrontPassenger=true;
            for(auto& entry:world.missionCharacters) {
                auto& behavior=entry.second.behavior;
                behavior.canDoDriveBy=false;
                behavior.canAttackPlayerWithCops=false;
                behavior.seekNodesForMission=false;
                behavior.onlyFrontPassenger=false;
                behavior.localAvoidanceSolver=false;
                behavior.alternateObstructionHandling=false;
                behavior.allowDuckAgainstArmedTargets=false;
                behavior.useUncontrolledWeaponDamage=false;
            }
            for(const auto& blip:world.radarBlips) {
                const uint32_t handle=uint32_t(blip.handle);
                const size_t slot=handle&0xffffu;
                if(slot<world.radarBlipGenerations.size())
                    world.radarBlipGenerations[slot]=uint16_t(handle>>16);
            }
        }
        if(version>=28) {
            RetailParityExtensions extension{state};
            subsystem(archive,0x38324353,extension); // SC28; earlier subsystem layouts stay intact.
        } else if(archive.reading) {
            auto& world=state.environment;
            world.introMissionSlot=-1;
            world.introMissionProtected=world.introMissionRestartQueued=false;
            world.animatedEmpireEntities={};
            world.animatedEmpireFreeCursor=-1;
            world.empireRequestedModels.clear();
            state.populationRandom=0x4C435350u;
            for(auto& vehicle:state.vehicles) {
                vehicle.creationClass=-1;
                vehicle.transmission={};
            }
            for(auto& object:world.objects)object.deferredRender=false;
        }
        if(version>=29) {
            NativeWorldExtensions extension{state};
            subsystem(archive,0x39324353,extension); // SC29, appended after SC28.
        } else if(archive.reading) {
            state.generatorStates.clear();
            if (!state.mission.vcs) {
                auto& world=state.environment;
                world.photographTaken=false;
                world.photographedCharacters.clear();
                world.freeHealthCare=false;
                world.getOutOfJailFree=false;
            }
        }
        if(vcsExtensions&&version>=30) {
            auto& world=state.environment;
            subsystem(archive,0x30334353,world.vcsArrayScratch,world.vcsCashHudRaised,
                world.vcsPedFlag28); // SC30
            if(world.vcsPedFlag28.size()>world.missionCharacters.size()+1)
                throw std::runtime_error("Invalid saved pedestrian path flags.");
            for(int32_t handle:world.vcsPedFlag28) {
                if(handle<0||(handle!=0&&!world.missionCharacters.count(handle)))
                    throw std::runtime_error("Invalid saved pedestrian path flag reference.");
            }
        } else if(archive.reading) {
            state.environment.vcsArrayScratch=0;
            state.environment.vcsCashHudRaised=false;
            state.environment.vcsPedFlag28.clear();
        }
        if(vcsExtensions&&version>=31) {
            auto& world=state.environment;
            auto savedFlags=world.vcsVehicleFlag28;
            if(!archive.reading) {
                for(auto flag=savedFlags.begin();flag!=savedFlags.end();) {
                    const auto vehicle=world.missionVehicles.find(*flag);
                    if(vehicle!=world.missionVehicles.end()&&vehicle->second.deleted)
                        flag=savedFlags.erase(flag);
                    else ++flag;
                }
            }
            auto savedPedFlags=world.vcsPedFlag14;
            subsystem(archive,0x31334353,savedFlags,savedPedFlags,
                world.camera.interpolationScripted,world.camera.transitionPending); // SC31
            if(savedFlags.size()>world.missionVehicles.size())
                throw std::runtime_error("Invalid saved vehicle path flags.");
            for(int32_t handle:savedFlags) {
                const auto vehicle=world.missionVehicles.find(handle);
                if(vehicle==world.missionVehicles.end()||vehicle->second.deleted)
                    throw std::runtime_error("Invalid saved vehicle path flag reference.");
            }
            if(savedPedFlags.size()>world.missionCharacters.size()+1)
                throw std::runtime_error("Invalid saved pedestrian flags.");
            for(int32_t handle:savedPedFlags)
                if(handle<0||(handle!=0&&!world.missionCharacters.count(handle)))
                    throw std::runtime_error("Invalid saved pedestrian flag reference.");
            if(archive.reading) {
                world.vcsVehicleFlag28=std::move(savedFlags);
                world.vcsPedFlag14=std::move(savedPedFlags);
            }
        } else if(archive.reading) {
            state.environment.vcsVehicleFlag28.clear();
            state.environment.vcsPedFlag14.clear();
            if(vcsExtensions) {
                state.environment.camera.interpolationScripted=false;
                state.environment.camera.transitionPending=false;
            }
        }
        if(vcsExtensions&&version>=32) {
            std::array<uint32_t,2> missionWait{uint32_t(state.mission.vcsAbsoluteWait),state.mission.vcsWakeTime};
            auto threadWaits=state.program.vcsWaitStates();
            subsystem(archive,0x32334353,missionWait,threadWaits); // SC32
            if(missionWait[0]>1||(missionWait[0]&&
                (!state.mission.vcs||state.mission.state!=StorylandMissionState::Waiting))||
                !state.program.restoreVcsWaitStates(threadWaits))
                throw std::runtime_error("Invalid saved script wait state.");
            if(archive.reading) {
                state.mission.vcsAbsoluteWait=missionWait[0]!=0;
                state.mission.vcsWakeTime=missionWait[1];
            }
        } else if(archive.reading) {
            state.mission.vcsAbsoluteWait=false;
            state.mission.vcsWakeTime=0;
            auto threadWaits=state.program.vcsWaitStates();
            for(auto& wait:threadWaits)wait={0,0};
            if(!state.program.restoreVcsWaitStates(threadWaits))
                throw std::runtime_error("Invalid legacy script wait state.");
        }
        if(version>=33) {
            subsystem(archive,0x33334353,state.environment.invertedMap); // SC33
        } else if(archive.reading) {
            state.environment.invertedMap=false;
        }
        auto& intro=state.environment;
        if(version>=34) {
            subsystem(archive,0x34334353,intro.introRestartPosition,intro.introRestartHeading,
                intro.introRestartPositionValid,intro.introRestartMartinezCaptured); // SC34
            if(intro.introRestartMartinezCaptured&&!intro.introRestartPositionValid)
                throw std::runtime_error("Invalid saved Martinez meeting checkpoint.");
            if(intro.introRestartPositionValid&&(!std::isfinite(intro.introRestartPosition[0])||
                !std::isfinite(intro.introRestartPosition[1])||
                !std::isfinite(intro.introRestartPosition[2])||
                !std::isfinite(intro.introRestartHeading)))
                throw std::runtime_error("Invalid saved intro restart coordinates.");
        } else if(archive.reading) {
            intro.introRestartPosition={intro.x,intro.y,intro.z};
            intro.introRestartHeading=intro.heading;
            intro.introRestartPositionValid=std::isfinite(intro.x)&&std::isfinite(intro.y)&&
                std::isfinite(intro.z)&&std::isfinite(intro.heading)&&
                std::hypot(intro.x,intro.y)>=1.0f;
            intro.introRestartMartinezCaptured=false;
        }
        auto& markerWorld=state.environment;
        const bool legacyLcsOpcodes=archive.reading&&version==35&&
            savedLegacySc35Layout(archive,state,pickups)==LegacySc35Layout::LcsOpcodes;
        if(version>=35&&!legacyLcsOpcodes) {
            NativeMarkerExtensions extension{markerWorld};
            subsystem(archive,0x35334353u,extension);
        } else if(archive.reading) {
            markerWorld.markerGenerations.fill(0);
            for(auto& marker:markerWorld.markers) {
                marker.type=state.mission.vcs?6u:5u;
                marker.createdAt=markerWorld.gameMilliseconds;
                const uint32_t handle=uint32_t(marker.handle);
                const size_t slot=handle&0xffffu;
                const uint16_t generation=uint16_t(handle>>16);
                if(marker.persistent&&slot<markerWorld.markerGenerations.size()&&generation!=0xffffu)
                    markerWorld.markerGenerations[slot]=generation;
            }
        }
        if(version>=36) {
            std::set<int32_t> criminals;
            if(!archive.reading)for(const auto& entry:state.environment.missionCharacters)
                if(entry.second.isChrisCriminal)criminals.insert(entry.first);
            subsystem(archive,0x36334353,criminals); // SC36
            for(int32_t handle:criminals)
                if(!state.environment.missionCharacters.count(handle))
                    throw std::runtime_error("Invalid saved criminal character reference.");
            if(archive.reading)for(auto& entry:state.environment.missionCharacters)
                entry.second.isChrisCriminal=criminals.count(entry.first)!=0;
        } else if(archive.reading) {
            for(auto& entry:state.environment.missionCharacters)entry.second.isChrisCriminal=false;
        }
        const bool legacyCombinedLcsOpcodes=archive.reading&&version==37&&
            savedLegacySc37Layout(archive,state,pickups)==LegacySc37Layout::LcsOpcodes;
        if(version>=37&&!legacyCombinedLcsOpcodes) {
            CharacterPhysicsExtensions extension{state};
            subsystem(archive,0x37334353,extension); // SC37
        } else if(archive.reading) {
            auto& world=state.environment;
            world.vcsSwimmingBoundary={};
            world.playerCollisionEnabled=true;
            world.playerDisableVerticalMovement=false;
            world.playerTestShotInVehicle=false;
            for(auto& entry:world.missionCharacters) {
                entry.second.collisionEnabled=true;
                entry.second.disableVerticalMovement=false;
                entry.second.testShotInVehicle=false;
            }
        }
        if(version>=38) {
            auto& wheel=state.environment.ferrisWheel;
            subsystem(archive,0x38334353,wheel); // SC38
            if(wheel.state>3||wheel.speed<0||wheel.speed>.050001f||wheel.angle<0||
                wheel.angle>6.283186f||wheel.remainingStopAngle<-.01f||wheel.remainingStopAngle>1.047198f||
                (wheel.wheelFound&&(!wheel.configured||wheel.originalMatrix[15]!=1||wheel.wheelMatrix[15]!=1||
                    wheel.wheelModel>=uint32_t(INT32_MAX)||wheel.cabinModel>=uint32_t(INT32_MAX))))
                throw std::runtime_error("Invalid saved VCS Ferris wheel state.");
            std::set<int32_t> cabins;
            for(int32_t handle:wheel.cabins) {
                if(handle==-1)continue;
                const auto object=std::find_if(state.environment.objects.begin(),state.environment.objects.end(),
                    [&](const auto& value){return value.handle==handle;});
                if(handle<0||!wheel.cabinsResident||!wheel.wheelFound||!cabins.insert(handle).second||
                    object==state.environment.objects.end()||object->model!=int32_t(wheel.cabinModel)||
                    object->creationSource!=5)
                    throw std::runtime_error("Invalid saved VCS Ferris wheel cabin identity.");
            }
        } else if(archive.reading)state.environment.ferrisWheel={};
        if(version>=39) {
            subsystem(archive,0x39334353,state.environment.vcsCharacterHandleBridge); // SC39
            if(state.environment.vcsCharacterHandleBridge&&!state.mission.vcs)
                throw std::runtime_error("VCS character handles cannot be used in an LCS save.");
        } else if(archive.reading)state.environment.vcsCharacterHandleBridge=false;
        if(version>=40) {
            auto& characters=state.environment.missionCharacters;
            std::map<int32_t,std::array<uint32_t,3>> leaderPolicy;
            if(!archive.reading)for(const auto& entry:characters)
                leaderPolicy[entry.first]={uint32_t(entry.second.leaderOutOfRange),
                    entry.second.leaderUpdateAfter,entry.second.leaderVehicleAfter};
            subsystem(archive,0x30344353,leaderPolicy); // SC40
            if(leaderPolicy.size()!=characters.size())
                throw std::runtime_error("Invalid saved leader policy count.");
            for(const auto& entry:leaderPolicy) {
                const auto character=characters.find(entry.first);
                if(character==characters.end()||entry.second[0]>1||
                    (!state.mission.vcs&&(entry.second[0]!=0||entry.second[1]!=0||entry.second[2]!=0)))
                    throw std::runtime_error("Invalid saved VCS leader policy.");
                character->second.leaderOutOfRange=entry.second[0]!=0;
                character->second.leaderUpdateAfter=entry.second[1];
                character->second.leaderVehicleAfter=entry.second[2];
            }
        } else if(archive.reading)for(auto& entry:state.environment.missionCharacters) {
            entry.second.leaderOutOfRange=false;
            entry.second.leaderUpdateAfter=0;
            entry.second.leaderVehicleAfter=0;
        }
        if(version>=41) {
            auto& navigation=state.environment.playerNavigation;
            subsystem(archive,0x31344353,navigation); // SC41
            if(!storylandValidScriptPlayerNavigation(navigation)||
                (!state.mission.vcs&&(navigation.seekNodes||navigation.localAvoidanceSolver||
                    navigation.alternateObstructionHandling||navigation.seekNode!=UINT32_MAX||
                    navigation.seekWaitUntil!=0||navigation.seekRouteCursor!=0||
                    !navigation.seekRoute.empty()||navigation.seekDestination[0]!=0||
                    navigation.seekDestination[1]!=0)))
                throw std::runtime_error("Invalid saved VCS player navigation.");
        } else if(archive.reading)state.environment.playerNavigation={};
        if(version>=42) {
            std::vector<StorylandForkliftState> forklifts;
            if(!archive.reading) {
                forklifts.reserve(state.vehicles.size());
                for(const auto& vehicle:state.vehicles)forklifts.push_back(vehicle.forklift);
            }
            subsystem(archive,0x32344353,forklifts); // SC42
            if(forklifts.size()!=state.vehicles.size())
                throw std::runtime_error("Invalid saved forklift state count.");
            std::set<int32_t> carriedObjects;
            for(size_t index=0;index<forklifts.size();++index) {
                const auto& forklift=forklifts[index];
                auto& vehicle=state.vehicles[index];
                if(!storylandValidForkliftState(forklift)||
                    ((!state.mission.vcs||vehicle.model!=251)&&!storylandDefaultForkliftState(forklift)))
                    throw std::runtime_error("Invalid saved native forklift state.");
                if(forklift.carriedObject>=0) {
                    const auto object=std::find_if(state.environment.objects.begin(),state.environment.objects.end(),
                        [&](const auto& candidate){return candidate.handle==forklift.carriedObject;});
                    if(vehicle.scriptHandle<0||object==state.environment.objects.end()||
                        object->attachedVehicle!=vehicle.scriptHandle||object->attachedCharacter>=0||
                        !carriedObjects.insert(forklift.carriedObject).second)
                        throw std::runtime_error("Invalid saved forklift object attachment.");
                }
                vehicle.forklift=forklift;
                vehicle.forklift.moving=false;
            }
        } else if(archive.reading)for(auto& vehicle:state.vehicles)vehicle.forklift={};
        if(version>=43) {
            auto& world=state.environment;
            subsystem(archive,0x33344353,world.savePosition,world.savePositionValid,
                world.savePositionExterior,world.saveLevel); // SC43
            if((!world.savePositionValid&&
                    (world.savePosition!=std::array<float,3>{}||world.savePositionExterior||world.saveLevel!=0))||
                (!state.mission.vcs&&world.savePositionValid))
                throw std::runtime_error("Invalid saved native save-point metadata.");
        } else if(archive.reading) {
            auto& world=state.environment;
            world.savePosition={};world.savePositionValid=false;
            world.savePositionExterior=false;world.saveLevel=0;
        }
        if(version>=44) {
            subsystem(archive,0x34344353,state.environment.playerStayInCarWhenJacked); // SC44
            if(!state.mission.vcs&&state.environment.playerStayInCarWhenJacked)
                throw std::runtime_error("VCS jacking policy cannot be used in an LCS save.");
        } else if(archive.reading)state.environment.playerStayInCarWhenJacked=false;
        if(version>=45||legacyLcsOpcodes||legacyCombinedLcsOpcodes) {
            LcsOpcodeBatchExtensions extension{state,pickups};
            const uint32_t tag=legacyLcsOpcodes?0x35334353u:
                legacyCombinedLcsOpcodes?0x37334353u:0x35344353u;
            subsystem(archive,tag,extension);
        }
        if(version>=46) {
            auto& world=state.environment;
            LcsNativeBonusExtensions bonuses{state};
            subsystem(archive,0x36344353,world.lcsEntityExtension,world.lcsExtendedWorld,
                world.lcsPickupDetails,bonuses); // SC46
            if(!world.lcsEntityExtension.valid()||!world.lcsExtendedWorld.valid()||
                world.lcsPickupDetails.size()>pickups.size())
                throw std::runtime_error("Invalid saved LCS command state.");
            const auto playerObjective=world.lcsEntityExtension.tasks.find(1);
            if(playerObjective!=world.lcsEntityExtension.tasks.end()&&
                (state.mission.vcs||!world.missionCharacters.count(playerObjective->second.target)||
                    (!world.playerWalkTask.active&&!world.playerWalkTask.completed)))
                throw std::runtime_error("Invalid saved LCS player target objective.");
            for(const auto& entry:world.lcsPickupDetails) {
                if(!entry.second.valid()||std::none_of(pickups.begin(),pickups.end(),
                    [&](const auto& pickup){return pickup.handle==entry.first;}))
                    throw std::runtime_error("Invalid saved LCS pickup extension.");
            }
            validateLcsNativeBonuses(state,pickups,archive.reading);
        } else if(archive.reading) {
            state.environment.lcsEntityExtension={};
            state.environment.lcsExtendedWorld={};
            state.environment.lcsPickupDetails.clear();
        }
        if (version >= 47) {
            std::vector<uint8_t> owners;
            std::vector<uint8_t> stored;
            if (!archive.reading) for (const auto& vehicle : state.vehicles) {
                owners.push_back(vehicle.garageOwner);
                stored.push_back(uint8_t(vehicle.garage >= 0 && !vehicle.visible));
            }
            subsystem(archive, 0x37344353, state.environment.coopFriendlyFire,
                state.environment.coopGuestStage, owners, stored);
            if (owners.size() != state.vehicles.size() || stored.size() != state.vehicles.size() ||
                state.environment.coopGuestStage > 2)
                throw std::runtime_error("Invalid saved co-op garage ownership.");
            for (size_t index = 0; index < owners.size(); ++index) {
                if (owners[index] != 0 && owners[index] != 1 && owners[index] != 255)
                    throw std::runtime_error("Invalid saved co-op vehicle owner.");
                if (stored[index] > 1 || (stored[index] && state.vehicles[index].garage < 0))
                    throw std::runtime_error("Invalid saved co-op garage storage state.");
                if (archive.reading) {
                    auto& vehicle = state.vehicles[index];
                    vehicle.garageOwner = owners[index];
                    if (stored[index]) {
                        vehicle.visible = false; vehicle.collisionEnabled = false; vehicle.positionFrozen = true;
                    }
                }
            }
        } else if (archive.reading) {
            state.environment.coopFriendlyFire = false;
            state.environment.coopGuestStage = 0;
            for (auto& vehicle : state.vehicles) vehicle.garageOwner = vehicle.garage >= 0 ? 0 : 255;
        }
        if (version >= 48) {
            auto& world = state.environment;
            subsystem(archive, 0x38344353, world.vcsPedFlag1D8Bit6); // SC48
            if ((!state.mission.vcs && !world.vcsPedFlag1D8Bit6.empty()) ||
                world.vcsPedFlag1D8Bit6.size() > world.missionCharacters.size() + 1)
                throw std::runtime_error("Invalid saved VCS pedestrian flag state.");
            for (int32_t handle : world.vcsPedFlag1D8Bit6) {
                if (handle < 0 || (handle != 0 && !world.missionCharacters.count(handle)))
                    throw std::runtime_error("Invalid saved VCS pedestrian flag reference.");
            }
        } else if (archive.reading) {
            state.environment.vcsPedFlag1D8Bit6.clear();
        }
        if(version>=49) {
            std::vector<std::array<float,2>> nozzles;
            if(!archive.reading)for(const auto& vehicle:state.vehicles)
                nozzles.push_back({vehicle.waterCannonYaw,vehicle.waterCannonPitch});
            subsystem(archive,0x39344353,nozzles); // SC49
            if(nozzles.size()!=state.vehicles.size())
                throw std::runtime_error("Invalid saved water-cannon nozzle count.");
            for(size_t index=0;index<nozzles.size();++index) {
                const auto& nozzle=nozzles[index];auto& vehicle=state.vehicles[index];
                if(nozzle[0]<-3.141592741f||nozzle[0]>3.141592741f||nozzle[1]<-.06f||nozzle[1]>.3f||
                    ((!state.mission.vcs||vehicle.model!=194)&&nozzle!=std::array<float,2>{0,.05f}))
                    throw std::runtime_error("Invalid saved native water-cannon orientation.");
                if(archive.reading) {vehicle.waterCannonYaw=nozzle[0];vehicle.waterCannonPitch=nozzle[1];}
            }
        } else if(archive.reading)for(auto& vehicle:state.vehicles) {
            vehicle.waterCannonYaw=0;vehicle.waterCannonPitch=.05f;
        }
        if(version>=50) {
            auto& world=state.environment;
            subsystem(archive,0x30354353,world.playerSeatedDriveByEnabled,
                world.manualAim.resetRequested,world.vcsStopSniperBulletsOnBuildings); // SC50
            if(!state.mission.vcs&&(world.playerSeatedDriveByEnabled||world.manualAim.resetRequested||
                world.vcsStopSniperBulletsOnBuildings))
                throw std::runtime_error("Invalid saved VCS aiming or sniper collision state.");
        } else if(archive.reading) {
            state.environment.playerSeatedDriveByEnabled=false;
            state.environment.manualAim.resetRequested=false;
            state.environment.vcsStopSniperBulletsOnBuildings=0;
        }
        if(version>=51) {
            subsystem(archive,0x31354353,state.environment.manualAim.seatedActive); // SC51
            if(!state.mission.vcs&&state.environment.manualAim.seatedActive)
                throw std::runtime_error("Invalid saved VCS passenger aiming state.");
        } else if(archive.reading) {
            state.environment.manualAim.seatedActive=false;
        }
        if(version>=52) {
            subsystem(archive,0x32354353,state.environment.vcsAmmoHudVisible); // SC52
            if(!state.mission.vcs&&!state.environment.vcsAmmoHudVisible)
                throw std::runtime_error("Invalid saved VCS ammunition display state.");
        } else if(archive.reading) {
            state.environment.vcsAmmoHudVisible=true;
        }
        if(version>=53) {
            auto& world=state.environment;
            std::map<int32_t,std::pair<int32_t,uint32_t>> fleeing;
            if(!archive.reading)for(const auto& entry:world.missionCharacters)
                fleeing[entry.first]={entry.second.fleeVehicle,entry.second.fleeVehicleDeadline};
            std::map<int32_t,StorylandMissionEnvironment::CharacterVehicleTask> vehicleTasks;
            if(!archive.reading&&state.mission.vcs)vehicleTasks=world.characterVehicleTasks;
            subsystem(archive,0x33354353,fleeing,vehicleTasks); // SC53
            if(fleeing.size()!=world.missionCharacters.size())
                throw std::runtime_error("Invalid saved vehicle-flee character count.");
            for(const auto& entry:fleeing) {
                const auto character=world.missionCharacters.find(entry.first);
                const int32_t handle=entry.second.first;
                const uint32_t deadline=entry.second.second;
                const auto vehicle=world.missionVehicles.find(handle);
                if(character==world.missionCharacters.end()||handle<-1||
                    (handle==-1&&deadline!=0)||
                    (handle>=0&&(vehicle==world.missionVehicles.end()||vehicle->second.deleted))||
                    (!state.mission.vcs&&(handle!=-1||deadline!=0)))
                    throw std::runtime_error("Invalid saved VCS vehicle-flee state.");
                if(archive.reading) {
                    character->second.fleeVehicle=handle;
                    character->second.fleeVehicleDeadline=deadline;
                }
            }
            if((!state.mission.vcs&&!vehicleTasks.empty())||vehicleTasks.size()>world.missionCharacters.size())
                throw std::runtime_error("Invalid saved VCS vehicle task count.");
            const auto vehicleValid=[&](int32_t handle) {
                if(handle==-1)return true;
                const auto vehicle=world.missionVehicles.find(handle);
                return vehicle!=world.missionVehicles.end()&&!vehicle->second.deleted;
            };
            for(const auto& entry:vehicleTasks) {
                const auto& task=entry.second;
                if(!world.missionCharacters.count(entry.first)||task.vehicle<0||
                    !vehicleValid(task.vehicle)||!vehicleValid(task.destinationVehicle)||
                    task.seat<-1||task.seat>7||task.destinationSeat<-1||task.destinationSeat>7||
                    task.route.size()>64||task.routeIndex>task.route.size()||
                    std::any_of(task.phaseSeconds.begin(),task.phaseSeconds.end(),
                        [](float seconds){return seconds<0;}))
                    throw std::runtime_error("Invalid saved VCS character vehicle task.");
            }
            if(archive.reading&&state.mission.vcs)world.characterVehicleTasks=std::move(vehicleTasks);
        } else if(archive.reading)for(auto& entry:state.environment.missionCharacters) {
            entry.second.fleeVehicle=-1;
            entry.second.fleeVehicleDeadline=0;
        }
        if(version>=54) {
            auto& world=state.environment;
            std::map<int32_t,std::array<bool,2>> carjacking;
            if(!archive.reading)for(const auto& entry:world.missionCharacters)
                carjacking[entry.first]={entry.second.canBeDraggedFromPassengerSide,
                    entry.second.passengerSideCarjack};
            subsystem(archive,0x34354353,world.playerCanBeDraggedFromPassengerSide,
                carjacking,world.scriptProjectiles); // SC54
            if(carjacking.size()!=world.missionCharacters.size()||
                (!state.mission.vcs&&world.playerCanBeDraggedFromPassengerSide))
                throw std::runtime_error("Invalid saved VCS passenger-side carjacking state.");
            for(const auto& entry:carjacking) {
                const auto character=world.missionCharacters.find(entry.first);
                if(character==world.missionCharacters.end()||
                    (!state.mission.vcs&&(entry.second[0]||entry.second[1])))
                    throw std::runtime_error("Invalid saved passenger-side carjacking character.");
                if(entry.second[1]) {
                    const auto& ped=character->second;
                    const auto vehicle=world.missionVehicles.find(ped.vehicle);
                    if(ped.passengerSeat!=0||ped.attached||vehicle==world.missionVehicles.end()||
                        vehicle->second.deleted||vehicle->second.driver==entry.first)
                        throw std::runtime_error("Invalid saved passenger-side carjacking transition.");
                }
                if(archive.reading) {
                    character->second.canBeDraggedFromPassengerSide=entry.second[0];
                    character->second.passengerSideCarjack=entry.second[1];
                }
            }
            for(const auto& projectile:world.scriptProjectiles) {
                if((projectile.weaponType!=-1&&(projectile.weaponType<12||projectile.weaponType>18))||
                    (projectile.active&&projectile.weaponType==-1)||
                    (projectile.active&&projectile.targeted&&projectile.weaponType!=18)||
                    projectile.age<0||projectile.trailTime<0||
                    uint8_t(projectile.source.kind)>uint8_t(StorylandMissionEnvironment::DamageSourceKind::Object)||
                    projectile.source.handle<-1||
                    (!state.mission.vcs&&(projectile.weaponType!=-1||projectile.active||
                        projectile.targeted||projectile.sequence!=0)))
                    throw std::runtime_error("Invalid saved native script projectile.");
            }
        } else if(archive.reading) {
            auto& world=state.environment;
            world.playerCanBeDraggedFromPassengerSide=false;
            for(auto& entry:world.missionCharacters) {
                entry.second.canBeDraggedFromPassengerSide=false;
                entry.second.passengerSideCarjack=false;
            }
            world.scriptProjectiles={};
        }
        if(version>=55) {
            auto& world=state.environment;
            bool playerCanBeShotInVehicle=world.playerCanBeShotInVehicle;
            subsystem(archive,0x35354353,world.scriptBinocularOverlay,playerCanBeShotInVehicle); // SC55
            if(!state.mission.vcs&&world.scriptBinocularOverlay)
                throw std::runtime_error("Invalid saved VCS binocular overlay in an LCS game.");
            if(state.mission.vcs)world.playerCanBeShotInVehicle=playerCanBeShotInVehicle;
            else if(playerCanBeShotInVehicle!=world.playerCanBeShotInVehicle)
                throw std::runtime_error("Saved player vehicle-shot eligibility disagrees with LC12.");
        } else if(archive.reading) {
            state.environment.scriptBinocularOverlay=false;
            if(state.mission.vcs)state.environment.playerCanBeShotInVehicle=true;
        }
        if(version>=56) {
            persistentVcsPedNativeState(archive,state,version);
        } else if(archive.reading) {
            auto& world=state.environment;
            world.playerDontReenterAfterEjection=false;
            world.playerAvoidUnsupportedForwardStep=false;
            world.vcsPedFalls.clear();
            world.vcsCharacterAttachments.clear();
            world.mountedGunRequests.clear();
            if(state.mission.vcs&&world.cameraSpecialMode==0)world.cameraSpecialMode=2;
            for(auto& entry:world.missionCharacters) {
                auto& character=entry.second;
                character.dontReenterAfterEjection=false;
                character.avoidUnsupportedForwardStep=false;
                character.mountedGunInitialized=false;
                character.nativePedRandomSeed=0;
                character.nativePedRandomSeedInitialized=false;
            }
            for(auto& entry:world.scriptedCharacterAnimations)entry.second.paused=false;
        }
        if(version>=57) {
            VcsPlaneControlState controls{state};
            subsystem(archive,0x37354353,controls);
        } else if(archive.reading) {
            for(auto& vehicle:state.vehicles)vehicle.vcsPlaneControlAxes={};
        }
        if(version>=58) {
            subsystem(archive,0x38354353,state.environment.vcsRandomBoatsEnabled); // SC58
            if(!state.mission.vcs&&!state.environment.vcsRandomBoatsEnabled)
                throw std::runtime_error("Invalid saved VCS random-boat policy in an LCS game.");
        }
        if(version>=59) {
            auto& world=state.environment;
            subsystem(archive,0x39354353,world.playerWalkTask.moveMode,
                world.vcsPedColorSetCursors); // SC59
            if(!storylandValidScriptPlayerWalkTask(world.playerWalkTask)||
                (!state.mission.vcs&&world.playerWalkTask.moveMode!=StorylandScriptPlayerMoveMode::Walk))
                throw std::runtime_error("Invalid saved native player coordinate gait.");
            if(world.vcsPedColorSetCursors.size()>65536||
                (!state.mission.vcs&&!world.vcsPedColorSetCursors.empty()))
                throw std::runtime_error("Invalid saved native character colour cursor table.");
            for(const auto& entry:world.vcsPedColorSetCursors)
                if(entry.first<0||entry.first>65535||entry.second>15)
                    throw std::runtime_error("Invalid saved native character colour cursor.");
        }
        if(version>=60) {
            VcsJetSkiControlState controls{state};
            subsystem(archive,0x30364353,controls); // SC60
        }
        if(version>=61) {
            VcsCharacterScriptState characters{state};
            subsystem(archive,0x31364353,characters); // SC61
        } else if(archive.reading) {
            state.environment.playerScriptRunning=false;
            state.environment.playerVcsScriptFlags={};
            for(auto& entry:state.environment.missionCharacters)entry.second.vcsScriptFlags={};
        }
        if (version >= 62) {
            VcsMissionPresentationState presentation{state};
            subsystem(archive, 0x32364353, presentation); // SC62
        } else if (archive.reading) {
            auto& world = state.environment;
            world.playerVcsFootstepMarks = {};
            for (auto& entry : world.missionCharacters) entry.second.vcsFootstepMarks = {};
            world.vcsHudHiddenComponents = 0;
            world.vcsAreaCaption.visible = true;
            world.vcsScript0556Value = 1.0f;
            world.vcsVehicleCaption = {};
            world.vcsRemoteCameraUpsideDownOffset = 0;
            world.drunkSteeringDrift = 0;
        }
        if (version >= 63) {
            VcsCharacterObjectParentState parents{state};
            subsystem(archive, 0x33364353, parents); // SC63
        } else if (archive.reading) {
            state.environment.vcsCharacterObjectParents.clear();
        }
        if(version>=64) {
            VcsSeekAndMusicState values{state};
            subsystem(archive,0x34364353,values); // SC64
        } else if(archive.reading) {
            state.environment.playerVcsSeekNodeRadius=5;
            state.environment.scriptMusicStartPositions={};
            for(auto& entry:state.environment.missionCharacters)entry.second.vcsSeekNodeRadius=5;
        }
        if(version>=65) {
            subsystem(archive,0x35364353,state.environment.playerDrownsInWater,
                state.environment.playerInfiniteSwim); // SC65
        } else if(archive.reading) {
            state.environment.playerDrownsInWater=true;
            state.environment.playerInfiniteSwim=false;
        }
        if(version>=66&&!state.mission.vcs) {
            auto& world=state.environment;
            subsystem(archive,0x36364353,world.playerSprintEnergy,world.playerSprintMaximum,
                world.playerSprintTraining); // SC66; LCS lacks VCS's historical SC22 fields.
            validateSprintReserve(world,false);
        } else if(archive.reading&&!legacySprintReservePresent(version,state.mission.vcs)) {
            state.environment.playerSprintEnergy=150.0f;
            state.environment.playerSprintMaximum=150.0f;
            state.environment.playerSprintTraining=0.0f;
        }
        if(vcsExtensions&&version>=67) {
            auto& world=state.environment;
            auto flags=world.vcsGarageStorableMissionVehicles;
            if(!archive.reading) {
                for(auto it=flags.begin();it!=flags.end();) {
                    const auto vehicle=world.missionVehicles.find(*it);
                    if(vehicle!=world.missionVehicles.end()&&vehicle->second.deleted)it=flags.erase(it);
                    else ++it;
                }
            }
            std::vector<uint8_t> storedFlags;
            if(!archive.reading) {
                storedFlags.reserve(state.vehicles.size());
                for(const auto& vehicle:state.vehicles)storedFlags.push_back(vehicle.vcsGarageStorable?1:0);
            }
            subsystem(archive,0x37364353,flags,storedFlags); // SC67
            if(flags.size()>world.missionVehicles.size()||storedFlags.size()!=state.vehicles.size())
                throw std::runtime_error("Invalid saved garage vehicle flags.");
            for(int32_t handle:flags) {
                const auto vehicle=world.missionVehicles.find(handle);
                if(vehicle==world.missionVehicles.end()||vehicle->second.deleted)
                    throw std::runtime_error("Invalid saved garage vehicle flag reference.");
            }
            for(uint8_t flag:storedFlags)if(flag>1)
                throw std::runtime_error("Invalid saved garage vehicle flag value.");
            if(archive.reading) {
                world.vcsGarageStorableMissionVehicles=std::move(flags);
                for(size_t i=0;i<storedFlags.size();++i)state.vehicles[i].vcsGarageStorable=storedFlags[i]!=0;
            }
        } else if(archive.reading) {
            state.environment.vcsGarageStorableMissionVehicles.clear();
            for(auto& vehicle:state.vehicles)vehicle.vcsGarageStorable=false;
        } else if(vcsExtensions) {
            if(!state.environment.vcsGarageStorableMissionVehicles.empty())
                throw std::runtime_error("This save version cannot store mission-vehicle garage flags.");
            for(const auto& vehicle:state.vehicles)if(vehicle.vcsGarageStorable)
                throw std::runtime_error("This save version cannot store garage vehicle flags.");
        }
        if(vcsExtensions&&version>=68) {
            auto& world=state.environment;
            uint8_t player=world.playerVcsScriptFlags.ignoreMeleeImpacts?1:0;
            std::set<int32_t> characters;
            if(!archive.reading)for(const auto& entry:world.missionCharacters)
                if(entry.second.vcsScriptFlags.ignoreMeleeImpacts)characters.insert(entry.first);
            subsystem(archive,0x38364353,player,characters); // SC68
            if(player>1||characters.size()>world.missionCharacters.size())
                throw std::runtime_error("Invalid saved VCS melee impact flags.");
            for(int32_t handle:characters)if(!world.missionCharacters.count(handle))
                throw std::runtime_error("Invalid saved VCS melee impact flag owner.");
            if(archive.reading) {
                world.playerVcsScriptFlags.ignoreMeleeImpacts=player!=0;
                for(auto& entry:world.missionCharacters)
                    entry.second.vcsScriptFlags.ignoreMeleeImpacts=characters.count(entry.first)!=0;
            }
        } else if(archive.reading) {
            state.environment.playerVcsScriptFlags.ignoreMeleeImpacts=false;
            for(auto& entry:state.environment.missionCharacters)
                entry.second.vcsScriptFlags.ignoreMeleeImpacts=false;
        } else {
            const auto& world=state.environment;
            if(world.playerVcsScriptFlags.ignoreMeleeImpacts||
                std::any_of(world.missionCharacters.begin(),world.missionCharacters.end(),
                    [](const auto& entry){return entry.second.vcsScriptFlags.ignoreMeleeImpacts;}))
                throw std::runtime_error("This save version cannot retain VCS melee impact flags.");
        }
        if(vcsExtensions&&version>=69) {
            auto& world=state.environment;
            uint8_t player=world.playerVcsScriptFlags.disablePlayerLeaderAnimationGroup?1:0;
            std::set<int32_t> characters;
            if(!archive.reading)for(const auto& entry:world.missionCharacters)
                if(entry.second.vcsScriptFlags.disablePlayerLeaderAnimationGroup)characters.insert(entry.first);
            subsystem(archive,0x39364353,player,characters); // SC69
            if(player>1||characters.size()>world.missionCharacters.size())
                throw std::runtime_error("Invalid saved VCS follower animation flags.");
            for(int32_t handle:characters)if(!world.missionCharacters.count(handle))
                throw std::runtime_error("Invalid saved VCS follower animation flag owner.");
            if(archive.reading) {
                world.playerVcsScriptFlags.disablePlayerLeaderAnimationGroup=player!=0;
                for(auto& entry:world.missionCharacters)
                    entry.second.vcsScriptFlags.disablePlayerLeaderAnimationGroup=characters.count(entry.first)!=0;
            }
        } else if(archive.reading) {
            state.environment.playerVcsScriptFlags.disablePlayerLeaderAnimationGroup=false;
            for(auto& entry:state.environment.missionCharacters)
                entry.second.vcsScriptFlags.disablePlayerLeaderAnimationGroup=false;
        } else {
            const auto& world=state.environment;
            if(world.playerVcsScriptFlags.disablePlayerLeaderAnimationGroup||
                std::any_of(world.missionCharacters.begin(),world.missionCharacters.end(),
                    [](const auto& entry){return entry.second.vcsScriptFlags.disablePlayerLeaderAnimationGroup;}))
                throw std::runtime_error("This save version cannot retain VCS follower animation flags.");
        }
        if(vcsExtensions&&version>=70) {
            auto& world=state.environment;
            std::set<int32_t> flags;
            if(!archive.reading)for(const auto& entry:world.missionVehicles)
                if(!entry.second.deleted&&entry.second.vcsSeaMineLauncher)flags.insert(entry.first);
            subsystem(archive,0x30374353,flags); // SC70
            if(flags.size()>world.missionVehicles.size())
                throw std::runtime_error("Invalid saved VCS vehicle sea-mine flags.");
            for(int32_t handle:flags) {
                const auto vehicle=world.missionVehicles.find(handle);
                if(vehicle==world.missionVehicles.end()||vehicle->second.deleted)
                    throw std::runtime_error("Invalid saved VCS vehicle sea-mine flag owner.");
            }
            if(archive.reading)for(auto& entry:world.missionVehicles)
                entry.second.vcsSeaMineLauncher=flags.count(entry.first)!=0;
        } else if(archive.reading) {
            for(auto& entry:state.environment.missionVehicles)entry.second.vcsSeaMineLauncher=false;
        } else if(std::any_of(state.environment.missionVehicles.begin(),state.environment.missionVehicles.end(),
            [](const auto& entry){return !entry.second.deleted&&entry.second.vcsSeaMineLauncher;}))
            throw std::runtime_error("This save version cannot retain VCS vehicle sea-mine flags.");
        if(vcsExtensions&&version>=71) {
            std::array<uint8_t,5> flags{};
            if(!archive.reading)for(size_t index=0;index<flags.size();++index) {
                const auto& slot=state.environment.audioSlots[index];
                flags[index]=uint8_t((slot.vcsExtendedLoad?1:0)|(slot.vcsSubtitlePresent?2:0)|
                    (slot.vcsReleasePending?4:0)|(slot.vcsResourceActive?8:0));
            }
            subsystem(archive,0x31374353,flags); // SC71; WRLD's audio slot layout stays unchanged.
            for(size_t index=0;index<flags.size();++index) {
                if(flags[index]&~15u)throw std::runtime_error("Invalid saved VCS audio release flags.");
                if((flags[index]&2u)&&!(flags[index]&1u))
                    throw std::runtime_error("Invalid saved VCS audio subtitle pointer state.");
                auto& slot=state.environment.audioSlots[index];
                if((flags[index]&4u)&&!(flags[index]&8u)&&slot.name.empty())
                    throw std::runtime_error("Saved VCS audio release has no active resource.");
                if(archive.reading) {
                    slot.vcsExtendedLoad=(flags[index]&1u)!=0;
                    slot.vcsSubtitlePresent=(flags[index]&2u)!=0;
                    slot.vcsReleasePending=(flags[index]&4u)!=0;
                    slot.vcsResourceActive=(flags[index]&8u)!=0;
                    slot.vcsBackendLoading=false;
                }
            }
        } else if(archive.reading) {
            for(auto& slot:state.environment.audioSlots) {
                slot.vcsExtendedLoad=false;slot.vcsSubtitlePresent=false;
                slot.vcsReleasePending=false;slot.vcsBackendLoading=false;
                slot.vcsResourceActive=!slot.name.empty();
            }
        } else if(state.mission.vcs&&std::any_of(state.environment.audioSlots.begin(),
            state.environment.audioSlots.end(),[](const auto& slot) {
                return slot.vcsExtendedLoad||slot.vcsSubtitlePresent||slot.vcsReleasePending||
                    (slot.vcsResourceActive&&slot.name.empty());
            }))throw std::runtime_error("This save version cannot retain VCS audio release state.");
        if(vcsExtensions&&version>=72) {
            std::vector<Vehicle::Crew> crews;
            if(!archive.reading)for(const auto& vehicle:state.vehicles)crews.push_back(vehicle.crew);
            subsystem(archive,0x32374353,crews); // SC72: real ambient vehicle occupants
            if(crews.size()!=state.vehicles.size())throw std::runtime_error("Invalid saved service crew count.");
            for(size_t index=0;index<crews.size();++index) {
                const auto& crew=crews[index];
                if(crew.model < -1 || crew.seat>2 || crew.health<0 || crew.health>10000 ||
                    crew.armour<0 || crew.armour>10000)
                    throw std::runtime_error("Invalid saved service crew state.");
                if(archive.reading)state.vehicles[index].crew=crew;
            }
        } else if(archive.reading) {
            for(auto& vehicle:state.vehicles)vehicle.crew={};
        }
        if (!state.mission.vcs && version>=74) {
            auto& world=state.environment;
            auto flags=world.vcsGarageStorableMissionVehicles;
            if (!archive.reading) {
                for (auto it=flags.begin();it!=flags.end();) {
                    const auto vehicle=world.missionVehicles.find(*it);
                    if (vehicle!=world.missionVehicles.end()&&vehicle->second.deleted)it=flags.erase(it);
                    else ++it;
                }
            }
            std::vector<uint8_t> storedFlags;
            if (!archive.reading) {
                storedFlags.reserve(state.vehicles.size());
                for (const auto& vehicle:state.vehicles)storedFlags.push_back(vehicle.vcsGarageStorable?1:0);
            }
            subsystem(archive,0x3141474c,flags,storedFlags); // LGA1
            if(flags.size()>world.missionVehicles.size()||storedFlags.size()!=state.vehicles.size())
                throw std::runtime_error("Invalid saved LCS garage vehicle flags.");
            for (int32_t handle:flags) {
                const auto vehicle=world.missionVehicles.find(handle);
                if(vehicle==world.missionVehicles.end()||vehicle->second.deleted)
                    throw std::runtime_error("Invalid saved LCS garage vehicle flag reference.");
            }
            for(uint8_t flag:storedFlags)if(flag>1)
                throw std::runtime_error("Invalid saved LCS stored garage flag.");
            if(archive.reading) {
                world.vcsGarageStorableMissionVehicles=std::move(flags);
                for(size_t i=0;i<storedFlags.size();++i)state.vehicles[i].vcsGarageStorable=storedFlags[i]!=0;
            }
        } else if (!state.mission.vcs && !archive.reading) {
            if(!state.environment.vcsGarageStorableMissionVehicles.empty())
                throw std::runtime_error("This save version cannot retain LCS garage vehicle permissions.");
            for(const auto& vehicle:state.vehicles)if(vehicle.vcsGarageStorable)
                throw std::runtime_error("This save version cannot retain LCS stored garage permissions.");
        }
        if(vcsExtensions&&version>=73) {
            auto& dispatch=state.environment.vcsViceDispatch;
            std::vector<std::array<uint8_t,2>> cars;
            if(!archive.reading)for(const auto& vehicle:state.vehicles)
                cars.push_back({uint8_t(vehicle.vcsViceUnit),vehicle.vcsViceCrewSequence});
            subsystem(archive,0x33374353,dispatch.lastCreatedAt,dispatch.crewSequence,cars); // SC73
            if(dispatch.crewSequence>=4||cars.size()!=state.vehicles.size())
                throw std::runtime_error("Invalid saved VCS undercover dispatch state.");
            for(size_t index=0;index<cars.size();++index) {
                const auto& entry=cars[index];
                if(entry[0]>1||entry[1]>=4||(entry[0]&&state.vehicles[index].model!=204))
                    throw std::runtime_error("Invalid saved VCS undercover vehicle.");
                if(archive.reading) {
                    state.vehicles[index].vcsViceUnit=entry[0]!=0;
                    state.vehicles[index].vcsViceCrewSequence=entry[1];
                }
            }
        } else if(archive.reading) {
            state.environment.vcsViceDispatch={};
            for(auto& vehicle:state.vehicles) {vehicle.vcsViceUnit=false;vehicle.vcsViceCrewSequence=0;}
        } else if(state.environment.vcsViceDispatch.lastCreatedAt||state.environment.vcsViceDispatch.crewSequence||
            std::any_of(state.vehicles.begin(),state.vehicles.end(),
                [](const auto& vehicle){return vehicle.vcsViceUnit;}))
            throw std::runtime_error("This save version cannot retain VCS undercover dispatch state.");
        if(vcsExtensions&&version>=75) {
            auto& meters=state.environment.vcsHudMeters;
            subsystem(archive,0x35374353,meters); // SC75
            if(!meters.valid())throw std::runtime_error("Invalid saved VCS HUD meter controller.");
            if(archive.reading)meters.lastFrame=UINT32_MAX;
        } else if(archive.reading) {
            state.environment.vcsHudMeters={};
            if(state.mission.vcs)
                state.environment.vcsHudMeters.flash(state.environment.flashingHudObject);
        }
    }catch(...) {state.environment.pickups=std::move(pickups);throw;}
    state.environment.pickups=std::move(pickups);
}
inline bool encode(State& state,const StorylandMissionVm& source,std::vector<uint8_t>& result,std::string& error) {
    try {
        if(state.mission.vcs!=source.vcs)throw std::runtime_error("Saved script game does not match MAIN.SCM.");
        if(!state.environment.undressedCharacters.empty())
            throw std::runtime_error("Finish the native character clothing transition before saving.");
        if(state.environment.hasScriptedLookTransition())
            throw std::runtime_error("Finish the native scripted looking transition before saving.");
        std::vector<uint8_t> payload;Archive a{payload};persistentSubsystems(a,state);
        if(payload.size()>MaximumBytes-37)throw std::runtime_error("Save is too large.");
        result.clear();Archive header{result};uint64_t magic=0x31564153444e4c53ull,script=hash(source.scriptBytes()),checksum=hash(payload);
        uint32_t version=Version,word=sizeof(size_t),size=uint32_t(payload.size());bool vcs=source.vcs;
        header(magic,version,word,vcs,script,size,checksum);result.insert(result.end(),payload.begin(),payload.end());return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
inline bool decode(std::vector<uint8_t> bytes,const StorylandMissionVm& source,State& result,std::string& error) {
    try {
        if(bytes.size()>MaximumBytes)throw std::runtime_error("Save is too large.");
        Archive h{bytes,true};uint64_t magic=0,script=0,checksum=0;uint32_t version=0,word=0,size=0;bool vcs=false;
        h(magic,version,word,vcs,script,size,checksum);
        if(magic!=0x31564153444e4c53ull||(version<1||version>Version)||word!=sizeof(size_t))throw std::runtime_error("Unsupported Storyland save format.");
        if(version==19)throw std::runtime_error("Storyland save schema 19 is unavailable.");
        if(vcs!=source.vcs||script!=hash(source.scriptBytes()))throw std::runtime_error("Save belongs to a different game or MAIN.SCM.");
        if(size!=bytes.size()-h.cursor)throw std::runtime_error("Invalid save length.");
        uint64_t payloadChecksum = 14695981039346656037ull;
        for (size_t index = h.cursor; index < bytes.size(); ++index) {
            payloadChecksum ^= bytes[index];
            payloadChecksum *= 1099511628211ull;
        }
        if(payloadChecksum!=checksum)throw std::runtime_error("Save checksum failed.");
        State candidate;Archive a{bytes,true,h.cursor};
        candidate.loadedVersion=version;
        if(version==1) {
            a(candidate);
            candidate.program.initializeLegacyOutcomes(candidate.mission);
        } else persistentSubsystems(a,candidate,version);
        if(version<58) {
            candidate.environment.vcsRandomBoatsEnabled=candidate.mission.vcs?
                candidate.environment.carGeneratorsDisabled:true;
        }
        if(!candidate.mission.vcs&&version<12) {
            candidate.environment.nextScriptCoronaId=1;
            for(auto& entry:candidate.environment.missionCharacters)entry.second.canBeShotInVehicle=true;
            for(auto& entry:candidate.environment.missionVehicles) {
                entry.second.boatAnchored=true;entry.second.doorLock=1;
            }
        }
        if(candidate.mission.vcs!=source.vcs||a.cursor!=bytes.size()||candidate.environment.health<=0||candidate.environment.inVehicle||
            candidate.equipped>=candidate.inventory.size()||candidate.inventory.size()!=candidate.weaponIds.size()||
            !candidate.mission.validSavedExecution(source.scriptBytes().size())||
            !candidate.program.restoreBindings(source,candidate.environment))throw std::runtime_error("Invalid saved game state.");
        if(candidate.vehicles.size()>StorylandPlayTestRuntime::vehicleCapacity(source.vcs))throw std::runtime_error("Saved vehicle pool exceeds capacity.");
        std::set<int32_t> vehicleHandles;
        for(const auto& car:candidate.vehicles) {
            if(car.scriptHandle>=0&&(!candidate.environment.missionVehicles.count(car.scriptHandle)||
                !vehicleHandles.insert(car.scriptHandle).second))throw std::runtime_error("Invalid saved vehicle handle.");
            if(car.garage>=0&&std::none_of(candidate.environment.garages.begin(),candidate.environment.garages.end(),
                [&](const auto& garage){return garage.handle==car.garage;}))throw std::runtime_error("Invalid saved garage handle.");
        }
        candidate.mission.sharedRetailScript=source.sharedRetailScript;candidate.mission.script=source.script;
        candidate.mission.opcodeDefinitions=source.opcodeDefinitions;candidate.mission.provisionalOpcodes=source.provisionalOpcodes;
        result=std::move(candidate);
        result.mission.environment=&result.environment;
        result.environment.zones=source.environment?source.environment->zones:nullptr;
        result.program.restoreBindings(source,result.environment);
        return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
inline bool write(const std::filesystem::path& path,State& state,const StorylandMissionVm& source,std::string& error) {
    std::vector<uint8_t> bytes;if(!encode(state,source,bytes,error))return false;
    return storylandWriteFilesTransaction({{path,&bytes}},error);
}
inline bool read(const std::filesystem::path& path,const StorylandMissionVm& source,State& state,std::string& error) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in){error="Save slot is empty or unreadable.";return false;}
    auto length=in.tellg();if(length<0||uint64_t(length)>MaximumBytes){error="Invalid save size.";return false;}
    std::vector<uint8_t> bytes(static_cast<size_t>(length));in.seekg(0);
    if(!in.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size()))){error="Cannot read save.";return false;}
    if((bytes.size()<8||std::memcmp(bytes.data(),"SLNDSAV1",8)!=0)&&
        storyland_psp_save::describeUnsupportedImport(path,error))return false;
    return decode(std::move(bytes),source,state,error);
}
}
