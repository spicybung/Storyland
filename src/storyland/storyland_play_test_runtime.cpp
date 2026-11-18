#include "src/storyland/storyland_rubbish_runtime.h"
#include "src/storyland/storyland_blood_effects.h"
#include "src/re/reVCS/camera/storyland_vcs_camera_fov.h"
#include "src/re/reVCS/runtime/storyland_vcs_039a.h"
#include "src/storyland/storyland_vehicle_route.h"
#include "src/storyland/storyland_vehicle_road_follow.h"
#include "src/storyland/storyland_vehicle_lamp_clock.h"
#include "src/re/reVCS/runtime/storyland_vcs_retail_quirks.h"
#include "src/re/reVCS/vehicles/storyland_vcs_vehicle_steering.h"
#include "src/re/reVCS/vehicles/storyland_vcs_vehicle_intercept.h"
#include "src/re/reVCS/vehicles/storyland_vcs_helicopter.h"
#include "src/re/reVCS/vehicles/storyland_vcs_scripted_helicopter_rotor.h"
#include "src/re/reVCS/vehicles/storyland_vcs_helicopter_coordinates.h"
#include "src/re/reVCS/vehicles/storyland_vcs_hovercraft_water.h"
#include "src/re/reVCS/vehicles/storyland_vcs_vehicle_water_occupants.h"
#include "src/re/reVCS/vehicles/storyland_vcs_helicopter_exit.h"
#include "src/re/reVCS/vehicles/storyland_vcs_helicopter_entry_height.h"
#include "src/re/reVCS/runtime/storyland_vcs_pad.h"
#include "src/re/reVCS/vehicles/storyland_vcs_taxi_boost.h"
#include "src/re/reVCS/vehicles/storyland_vcs_jetski_knockoff.h"
#include "src/re/reVCS/vehicles/storyland_vcs_jetski_placement.h"
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_service_vehicle.h"
#include "src/re/reVCS/services/storyland_vcs_vice_crew.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_wait_state.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_extended_sprint_jump.h"
#include "src/re/reVCS/vehicles/storyland_vcs_vehicle_contact_timers.h"
#include "src/re/reVCS/vehicles/storyland_vcs_traffic_support.h"
#include "src/re/reVCS/vehicles/storyland_vcs_vehicle_exhaust.h"
#include "src/storyland/storyland_retail_ground_effects.h"
#include "src/storyland/storyland_swimming_runtime.h"
#include "src/storyland/storyland_character_sound_events.h"
#include "src/re/reVCS/runtime/storyland_vcs_seek_radius.h"
#include "src/storyland/storyland_play_test_script.h"
#include "src/storyland/storyland_time.h"
#include "src/storyland/storyland_mission_marker_priority.h"
#include "src/re/reVCS/services/storyland_military_population.h"
#include "src/re/reVCS/services/storyland_army_tolerance.h"
#include "src/storyland/storyland_gang_response.h"
#include "src/storyland/storyland_weapon_animation_policy.h"
#include "src/storyland/storyland_play_test_weapons.h"
#include "src/storyland/storyland_vehicle_suspension.h"
#include "src/storyland/storyland_vehicle_passive_controls.h"
#include "src/storyland/storyland_bike_dynamics.h"
#include "src/storyland/storyland_vehicle_ground_support.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include "src/storyland/storyland_vehicle_forklift.h"
#include "src/storyland/storyland_vehicle_entry.h"
#include "src/re/reLCS2/runtime/storyland_lcs_population.h"
#include "src/storyland/storyland_population.h"
#include "src/storyland/storyland_population_lifecycle.h"
#include "src/re/reVCS/services/storyland_vcs_emergency_medic.h"
#include "src/re/reVCS/services/storyland_vcs_service_crew.h"
#include "src/re/reVCS/services/storyland_vcs_firefighter_response.h"
#include "src/re/reVCS/services/storyland_vcs_firetruck_handoff.h"
#include "src/re/reVCS/services/storyland_vcs_ambulance_handoff.h"
#include "src/storyland/storyland_water_cannon_math.h"
#include "src/re/reVCS/runtime/storyland_vcs_ambient_selection.h"
#include "src/re/reLCS2/vehicles/storyland_lcs_vehicle_access.h"
#include "src/re/reLCS2/vehicles/storyland_lcs_vehicle_damage.h"
#include "src/re/reLCS2/vehicles/storyland_lcs_boat.h"
#include "src/re/reVCS/vehicles/storyland_vcs_boat_water.h"
#include "src/re/reVCS/vehicles/storyland_vcs_boat_propulsion.h"
#include "src/re/reVCS/vehicles/storyland_vcs_boat_entry.h"
#include "src/re/reVCS/world/storyland_vcs_water_depth.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_ped_damage_contact.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_ped_motion_runtime.h"
#include "src/storyland/storyland_script_object.h"
#include "src/storyland/storyland_script_vehicle_capacity.h"
#include "src/storyland/storyland_vehicle_systems.h"
#include "src/storyland/storyland_vehicle_destruction.h"
#include "src/storyland/storyland_pickups.h"
#include "src/storyland/storyland_service_population.h"
#include "src/storyland/storyland_vehicle_victim_recovery.h"
#include "src/storyland/storyland_vehicle_bailout.h"
#include "src/storyland/storyland_vehicle_population.h"
#include "src/storyland/storyland_traffic_population.h"
#include "src/storyland/storyland_police.h"
#include "src/re/reLCS2/services/storyland_lcs_police_response.h"
#include "src/re/reVCS/services/storyland_vcs_police_response.h"
#include "src/storyland/storyland_falling_safety.h"
#include "src/storyland/storyland_sa_crouch_motion.h"
#include "src/storyland/storyland_native_body.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <cfloat>

static constexpr int32_t StorylandVcsPoliceHelicopterHandle = -261;

void storylandResetPlayerSprint(StorylandPlayTestRuntime& runtime) {
    runtime.playerSprint = {};
    runtime.playerAnimationMoveDelta={};
    runtime.playerAnimationMoveDeltaValid=false;
    runtime.playerSprintActive = false;
    runtime.playerSprintBlend = 0;
    runtime.playerSprintFading = false;
    runtime.playerMoveBlend = 0;
    runtime.playerStopVariant = -1;
    runtime.playerStopTime = 0;
}

bool storylandGangPlayerProtected(const StorylandPlayTestRuntime& runtime,int32_t pedestrianType) {
    if(!runtime.crime.vcs)return false;
    if(runtime.missionEnvironment) {
        const auto flags=runtime.missionEnvironment->characterFlags.find(0);
        if(flags!=runtime.missionEnvironment->characterFlags.end())
            return storylandNativeGangProtectsPlayer(pedestrianType,flags->second);
    }
    return pedestrianType==7+StorylandFortBaxterRetailResponse::GangId &&
        StorylandFortBaxterRetailResponse::uniformProtects(runtime.fortBaxterHeat,
            runtime.armyFatigues,runtime.fortBaxterAlarm,runtime.fortBaxterScriptStateObserved);
}

static bool storylandIsAdditionalFortBaxterPatrol(const StorylandRuntimePed& pedestrian) {
    // Only the additional Play Test spawner assigns a patrol anchor.
    const auto& anchor=pedestrian.militaryPatrolAnchor;
    return pedestrian.fortBaxterSoldier&&(anchor.x!=0||anchor.y!=0||anchor.z!=0);
}

void storylandUpdateMilitaryResponse(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld* world,
    const StorylandNativePoint& player) {
    if(!runtime.crime.vcs)return;
    bool insideScriptZone=false;
    if(world)for(const auto& zone:world->zones.information) {
        if((zone.name=="ARMYBAS"||zone.name=="AIRPORT")&&zone.contains(player)) {
            insideScriptZone=true;
            break;
        }
    }
    StorylandFortBaxterRetailResponse::tick(runtime.elapsed,runtime.fortBaxterHeat,
        runtime.fortBaxterLastUpdate,runtime.fortBaxterPendingDeaths,insideScriptZone,
        StorylandFortBaxterRetailResponse::inHostilityRectangle(player.x,player.y),
        runtime.fortBaxterWasInScriptZone);
    if(runtime.missionEnvironment) {
        const auto& mission=*runtime.missionEnvironment;
        runtime.armyFatigues=mission.playerOutfit=="PLR"||mission.playerOutfit=="plr";
        const auto flags=mission.characterFlags.find(0);
        runtime.fortBaxterScriptStateObserved=flags!=mission.characterFlags.end();
        runtime.militaryScriptControlled=runtime.fortBaxterScriptStateObserved;
        runtime.militaryScriptHostile=runtime.militaryScriptControlled &&
            StorylandFortBaxterRetailResponse::hostileFromRetailScript(
                mission.gangs[StorylandFortBaxterRetailResponse::GangId].attackWithPolice,flags->second);
    } else {
        runtime.fortBaxterScriptStateObserved=false;
        runtime.militaryScriptControlled=false;
        runtime.militaryScriptHostile=false;
    }
    runtime.fortBaxterAlarm=runtime.fortBaxterScriptStateObserved?runtime.militaryScriptHostile:
        StorylandFortBaxterRetailResponse::alarmFromMissionHeat(runtime.fortBaxterHeat);
    for(auto& pedestrian:runtime.pedestrians) {
        if(pedestrian.scriptHandle>=0||pedestrian.health<=0)continue;
        const bool protectedPlayer=storylandGangPlayerProtected(runtime,pedestrian.stats.pedType);
        if(protectedPlayer) {
            pedestrian.hostile=false;
            pedestrian.personallyProvoked=false;
            pedestrian.playOptionHostile=false;
            pedestrian.melee={};
            pedestrian.ranged={};
            pedestrian.fireFlash=0;
            if(pedestrian.state==StorylandPedState::Combat)pedestrian.state=StorylandPedState::Walk;
        } else if(pedestrian.fortBaxterSoldier) {
            pedestrian.hostile=runtime.fortBaxterAlarm||!runtime.armyFatigues||
                runtime.militaryScriptHostile;
            if(pedestrian.hostile)pedestrian.state=StorylandPedState::Combat;
            else if(pedestrian.state==StorylandPedState::Combat)pedestrian.state=StorylandPedState::Walk;
        }
    }
}

static bool storylandReadStoppedVehicleSlot(const StorylandPlayTestRuntime& runtime,
    int32_t slot,bool& stopped) {
    stopped=false;
    if(!runtime.missionEnvironment||slot<0||size_t(slot)>=runtime.vehicles.size())return false;
    const auto& travel=runtime.vehicles[size_t(slot)].scriptTravel;
    if(!travel.measured)return false;
    stopped=storylandLcsVehicleStopped(travel.distance,runtime.missionEnvironment->nativeScriptTimeStep);
    return true;
}

static bool storylandReadStoppedMissionVehicle(const StorylandPlayTestRuntime& runtime,
    int32_t handle,bool& stopped) {
    stopped=false;
    if(!runtime.missionEnvironment)return false;
    const auto& vehicles=runtime.missionEnvironment->missionVehicles;
    const auto found=vehicles.find(handle);
    if(found==vehicles.end()||found->second.deleted)return false;
    const int32_t slot=found->second.spawnedSlot;
    if(slot<0||size_t(slot)>=runtime.vehicles.size()||
        runtime.vehicles[size_t(slot)].scriptHandle!=handle)return false;
    return storylandReadStoppedVehicleSlot(runtime,slot,stopped);
}

static bool storylandReadStoppedCharacter(const StorylandPlayTestRuntime& runtime,
    int32_t handle,bool& stopped) {
    stopped=false;
    if(!runtime.missionEnvironment)return false;
    const auto& world=*runtime.missionEnvironment;
    StorylandLcsStoppedPedState state;
    if(handle==0) {
        if(runtime.occupiedVehicle>=0)
            return storylandReadStoppedVehicleSlot(runtime,runtime.occupiedVehicle,stopped);
        state.movementState=runtime.playerSprint.movementState;
        state.airborne=!runtime.grounded;
        state.landing=runtime.landingTime>0;
        state.standing=runtime.grounded;
        state.animationMoveDelta=runtime.playerAnimationMoveDelta;
        state.runStop=runtime.playerStopVariant>=0;
        state.jumpLaunch=runtime.jumpLaunching;
        state.jumpGlide=!runtime.grounded&&runtime.airborneTime>0;
        if(!storylandLcsPlayerStopped(state))return true;
        const int32_t playerHandle=runtime.crime.vcs?0:1;
        if(!runtime.playerAnimationMoveDeltaValid||runtime.health<=0||runtime.playerWater.active||
            runtime.vehicleAction!=StorylandVehicleAction::None||runtime.playerReaction.active()||
            runtime.playerMelee.active||storylandPlayerBulletReactionActive(runtime)||
            runtime.extendedMovement.busy()||world.cutscene.playing||world.playerAttachedVehicle>=0||
            world.activeVcsCharacterAttachment(playerHandle)||
            world.scriptedCharacterAnimations.count(playerHandle)||
            world.pedAnimationRequests.count(playerHandle))return false;
        stopped=true;
        return true;
    }
    const auto found=world.missionCharacters.find(handle);
    if(found==world.missionCharacters.end())return false;
    const auto& character=found->second;
    if(character.vehicle>=0&&!character.attached)
        return storylandReadStoppedMissionVehicle(runtime,character.vehicle,stopped);
    state.movementState=character.movementState;
    if(state.movementState!=0&&state.movementState!=1)return true;
    const auto physical=runtime.missionCombat.find(handle);
    if(physical==runtime.missionCombat.end())return false;
    const auto& combat=physical->second;
    state.airborne=combat.airborneTime>0;
    state.landing=combat.landingTime>0;
    if(state.airborne||state.landing)return true;
    // Ordinary LCS mission peds do not yet publish native standing/root state.
    if(!combat.gravityPositionValid||!character.alive||character.health<=0||character.attached||character.animationId>=0||
        world.scriptedCharacterAnimations.count(handle)||world.pedAnimationRequests.count(handle)||
        storylandMissionBulletReactionActive(runtime,handle)||combat.melee.active||combat.water.active||
        world.cutscene.playing)return false;
    state.standing=combat.grounded;
    stopped=storylandLcsPedStopped(state);
    return true;
}

void storylandBindAmbientCharacterCallbacks(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& pedestrians) {
    if (!runtime.missionEnvironment) return;
    auto& world = *runtime.missionEnvironment;
    world.nativeVehicleStopped=[&runtime](int32_t handle,bool& stopped) {
        return storylandReadStoppedMissionVehicle(runtime,handle,stopped);
    };
    world.nativeCharacterStopped=[&runtime](int32_t handle,bool& stopped) {
        return storylandReadStoppedCharacter(runtime,handle,stopped);
    };
    const auto charactersInArea = [&runtime, &pedestrians](const std::array<float,6>& bounds, bool includeDead) {
        std::vector<StorylandMissionEnvironment::AmbientCharacter> result;
        for (size_t count = std::min(pedestrians.size(), runtime.pedestrians.size()); count > 0; --count) {
            const size_t slot = count - 1;
            const auto& ped = runtime.pedestrians[slot];
            const auto& stats=ped.zombie.active?ped.zombie.originalStats:ped.stats;
            const auto& point = pedestrians[slot].position;
            if (ped.scriptHandle >= 0 || ped.serviceVehicleIdentity || (!includeDead && ped.health <= 0) || !ped.stats.valid ||
                point.x < bounds[0] || point.x > bounds[3] ||
                point.y < bounds[1] || point.y > bounds[4] ||
                point.z < bounds[2] || point.z > bounds[5]) continue;
            result.push_back({int32_t(slot), int32_t(stats.modelId), stats.pedType,
                {point.x, point.y, point.z}, ped.health, !ped.serviceVehicleIdentity, true});
        }
        return result;
    };
    world.ambientCharactersInArea = [charactersInArea](const std::array<float,6>& bounds) {
        return charactersInArea(bounds, false);
    };
    world.allAmbientCharactersInArea = [charactersInArea](const std::array<float,6>& bounds) {
        return charactersInArea(bounds, true);
    };
    world.nativePlayerState = [&runtime]() -> int32_t {
        return runtime.wastedActive ? 1 : runtime.busted ? 2 : 0;
    };
    world.acquireAmbientCharacters = [&runtime, &pedestrians](const std::vector<int32_t>& slots,
        bool missionOwned, std::vector<int32_t>& handles) {
        auto& live = *runtime.missionEnvironment;
        if (slots.size() > 128 || live.missionCharacters.size() + slots.size() > 128 ||
            live.nextMissionCharacterHandle < 1 ||
            uint64_t(live.nextMissionCharacterHandle) + slots.size() > uint64_t(INT32_MAX)) return false;
        for (size_t index = 0; index < slots.size(); ++index)
            if (live.missionCharacters.count(live.nextMissionCharacterHandle + int32_t(index))) return false;
        std::set<int32_t> distinct;
        for (int32_t slot : slots) {
            if (slot < 0 || size_t(slot) >= runtime.pedestrians.size() ||
                size_t(slot) >= pedestrians.size() || !distinct.insert(slot).second) return false;
            const auto& ped = runtime.pedestrians[size_t(slot)];
            if (ped.scriptHandle >= 0 || !ped.stats.valid) return false;
        }
        std::vector<int32_t> acquired;
        acquired.reserve(slots.size());
        for (int32_t slot : slots) {
            auto& ambient = runtime.pedestrians[size_t(slot)];
            storylandRestoreZombiePedestrian(ambient);
            const auto& actor = pedestrians[size_t(slot)];
            StorylandMissionEnvironment::Character ped;
            if(!ambient.speechIdentity)ambient.speechIdentity=storylandNewCharacterSpeechIdentity();
            ped.speechIdentity=ambient.speechIdentity;
            ped.canBeShotInVehicle=true;
            ped.pedStats=ambient.stats;
            ped.model = int32_t(ambient.stats.modelId);
            ped.type = ambient.stats.pedType;
            ped.pedStat = ambient.stats.statIndex;
            ped.animationGroup = ambient.stats.animationGroup;
            ped.x = actor.position.x; ped.y = actor.position.y; ped.z = actor.position.z;
            ped.heading = actor.yaw - 90.0f;
            ped.health = ambient.health; ped.armour = ambient.armour;
            ped.alive = ambient.health > 0;
            ped.persistent = missionOwned; ped.creationSource = missionOwned ? 2 : 1;
            ped.provokedByPlayer = ambient.playerDamaged &&
                !storylandGangPlayerProtected(runtime,ped.type);
            ped.playerKillCounted = ambient.playerKillCounted;
            if(runtime.crime.vcs) {
                ped.nativePedRandomSeed=uint8_t(live.nextNativeRandom());
                ped.nativePedRandomSeedInitialized=true;
            }
            const uint16_t draw = uint16_t(live.nextNativeRandom()) % 25;
            ped.walletMoney = draw == 23 ? 400 : draw; ped.walletInitialized = true;
            ped.speechProfile = live.nativePedSpeechProfile ? live.nativePedSpeechProfile(ped.model) : -1;
            const auto threats = live.pedTypeThreats.find(ped.type);
            if (threats != live.pedTypeThreats.end()) ped.fearFlags = threats->second;
            if (ambient.weapon < runtime.weapons.size()) {
                const auto& weapon = runtime.weapons[ambient.weapon];
                ped.currentWeapon = std::max(0, weapon.nativeId);
                ped.weapons[ped.currentWeapon] = {weapon.slot, weapon.modelId, ambient.ammunition, true};
            }
            const int32_t handle = live.nextMissionCharacterHandle++;
            live.missionCharacters.emplace(handle, std::move(ped));
            if (!runtime.crime.vcs) {
                const auto& promoted = live.missionCharacters.at(handle);
                live.characterCurrentWeapons[handle] = promoted.currentWeapon;
                for (const auto& entry : promoted.weapons) {
                    const auto& weapon = entry.second;
                    live.characterWeapons[handle][entry.first] =
                        {weapon.slot, weapon.model, weapon.ammunition, weapon.owned};
                }
            }
            auto& combat = runtime.missionCombat[handle];
            combat.weapon = ambient.weapon; combat.nativeWeapon = live.missionCharacters.at(handle).currentWeapon;
            combat.magazine = ambient.magazine; combat.cooldown = ambient.weaponCooldown;
            combat.reloading = ambient.reloading; combat.shotSerial = ambient.shotSerial;
            combat.melee = ambient.melee; combat.water = ambient.water;
            ambient.scriptHandle = handle;
            // The promoted actor has one simulation owner. The dormant pool
            // record cannot collide, fire, render a duplicate, or be recycled.
            ambient.health = 0; ambient.state = StorylandPedState::Dead; ambient.deathTime = 15;
            ambient.hostile = false; ambient.melee = {}; ambient.water = {};
            if (runtime.targetedPed == slot) runtime.targetedPed = -1;
            acquired.push_back(handle);
        }
        handles = std::move(acquired);
        return true;
    };
    world.acquireAmbientVehiclePassenger=[&runtime,&pedestrians](int32_t vehicleHandle,int32_t seat)->int32_t {
        if(seat<0||!runtime.missionEnvironment)return -1;
        auto& live=*runtime.missionEnvironment;
        const auto car=live.missionVehicles.find(vehicleHandle);
        if(car==live.missionVehicles.end()||car->second.deleted||car->second.spawnedSlot<0)return -1;
        const size_t vehicleSlot=size_t(car->second.spawnedSlot);
        if(vehicleSlot>=runtime.vehicles.size())return -1;
        const auto& vehicle=runtime.vehicles[vehicleSlot];
        if(vehicle.scriptHandle!=vehicleHandle||!vehicle.serviceCrewIdentity)return -1;
        for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size();++index) {
            auto& ped=runtime.pedestrians[index];
            if(ped.serviceVehicleIdentity!=vehicle.serviceCrewIdentity||int(ped.servicePassengerSeat)!=seat||ped.scriptHandle>=0)continue;
            std::vector<int32_t> handles;
            if(!live.acquireAmbientCharacters||!live.acquireAmbientCharacters({int32_t(index)},false,handles)||handles.size()!=1)return -2;
            auto& character=live.missionCharacters.at(handles[0]);
            character.vehicle=vehicleHandle;character.passengerSeat=seat;character.attached=false;
            character.canBeShotInVehicle=true;
            ped.serviceVehicleIdentity=0;
            return handles[0];
        }
        return -1;
    };
    world.ambientVehiclePassengerPresent=[&runtime,&pedestrians](int32_t handle,int32_t seat) {
        if(seat<0||!runtime.missionEnvironment)return false;
        const auto found=runtime.missionEnvironment->missionVehicles.find(handle);
        if(found==runtime.missionEnvironment->missionVehicles.end()||found->second.deleted||found->second.spawnedSlot<0)return false;
        const size_t slot=size_t(found->second.spawnedSlot);
        if(slot>=runtime.vehicles.size()||runtime.vehicles[slot].scriptHandle!=handle)return false;
        const auto identity=runtime.vehicles[slot].serviceCrewIdentity;
        if(!identity)return false;
        for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size();++index) {
            const auto& ped=runtime.pedestrians[index];
            if(ped.serviceVehicleIdentity==identity&&int(ped.servicePassengerSeat)==seat&&ped.scriptHandle<0)return true;
        }
        return false;
    };
    world.characterInWater = [&runtime](int32_t handle) {
        if(handle==0)return runtime.playerWater.active;
        if(!runtime.missionEnvironment||!runtime.missionEnvironment->missionCharacters.count(handle))return false;
        const auto found=runtime.missionCombat.find(handle);
        return found!=runtime.missionCombat.end()&&found->second.water.active;
    };
    world.characterDrowning = [&runtime](int32_t handle) {
        if (handle == 0) return runtime.playerWater.drowning;
        const auto found = runtime.missionCombat.find(handle);
        return found != runtime.missionCombat.end() && found->second.water.drowning;
    };
    world.characterFighting = [&runtime](int32_t handle) {
        if (handle == 0) return runtime.playerMelee.active;
        const auto found = runtime.missionCombat.find(handle);
        return found != runtime.missionCombat.end() && found->second.melee.active;
    };
    world.characterInControl = [&runtime](int32_t handle) {
        if (!runtime.missionEnvironment) return false;
        const auto& world = *runtime.missionEnvironment;
        const auto found = world.missionCharacters.find(handle);
        if (found == world.missionCharacters.end()) return false;
        const auto& character = found->second;
        if (!character.alive || !(character.health > 0) || character.vehicle >= 0 ||
            world.vcsPedFalls.count(handle)) return false;
        const auto vehicleTask = world.characterVehicleTasks.find(handle);
        if (vehicleTask != world.characterVehicleTasks.end() && vehicleTask->second.animating) return false;
        if (character.behavior.diveDeadline && character.animationId == 146) return false;
        const auto physical = runtime.missionCombat.find(handle);
        if (physical != runtime.missionCombat.end()) {
            const auto& state = physical->second;
            if (storylandMissionBulletReactionActive(runtime,handle)) return false;
            if (state.gravityPositionValid && (!state.grounded || state.landingTime > 0)) return false;
        }
        return true;
    };
    world.playerEvasiveDive = [&runtime](int32_t vehicle, std::string& error) {
        return storylandRequestScriptedPlayerEvasiveDive(runtime, vehicle, error);
    };
    world.characterShooting = [&runtime](int32_t handle) {
        if (handle == 0) return runtime.elapsed >= runtime.playerLastShotTime &&
            runtime.elapsed - runtime.playerLastShotTime <= .1f;
        const auto& world = *runtime.missionEnvironment;
        const auto found = world.missionCharacters.find(handle);
        return found != world.missionCharacters.end() && found->second.hasShot &&
            world.gameMilliseconds - found->second.shotAt <= 100;
    };
    world.explodeCharacterHead = [&runtime](int32_t handle) {
        return storylandExplodeMissionCharacterHead(runtime, handle);
    };
    world.countReturnableWeapons = [&runtime]() { return storylandCountReturnableWeapons(runtime); };
    world.returnPlayerWeapons = [&runtime]() { return storylandReturnPlayerWeapons(runtime); };
}

bool storylandScriptLineOfSightHit(const StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, const std::vector<StorylandNativeTrafficActor>& cars,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,
    const StorylandNativePoint& start, const StorylandNativePoint& end, uint32_t mask,
    int32_t ignoredCharacter) {
    const auto insideWorld = [](const StorylandNativePoint& point) {
        return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) &&
            point.x > -2400 && point.x < 1600 && point.y > -2000 && point.y < 2000;
    };
    // ProcessLineOfSight(0x190950) rejects either endpoint outside the world.
    if (!insideWorld(start) || !insideWorld(end)) {
        return false;
    }
    if ((mask & 17u) && storylandTraceNativeSegmentCategories(world, start, end,
        (mask & 1u) != 0, (mask & 16u) != 0) < 1) {
        return true;
    }
    const auto hits = [&](const StorylandNativeCollisionModel& model,
        const std::array<float, 16>& matrix) {
        float fraction = 1;
        StorylandNativePoint normal;
        return storylandTraceCollisionModel(model, matrix, start, end, fraction, normal);
    };
    if (mask & 2u) {
        for (size_t index = 0; index < runtime.vehicles.size() && index < cars.size(); ++index) {
            const auto& vehicle = runtime.vehicles[index];
            if (!vehicle.collisionEnabled || !vehicle.nativeDefinition.collision) {
                continue;
            }
            const auto& actor = cars[index];
            const auto matrix = index < runtime.vehicleWorldMatrices.size() ? runtime.vehicleWorldMatrices[index] :
                storylandScriptObjectMatrix(actor.position.x, actor.position.y, actor.position.z,
                    vehicle.bodyPitch + vehicle.roadPitch, vehicle.bodyRoll - vehicle.roadRoll, actor.yaw - 90);
            if (hits(*vehicle.nativeDefinition.collision, matrix)) {
                return true;
            }
        }
    }
    const auto pedHit = [&](int32_t model, const StorylandNativePoint& point, float heading) {
        const auto found = world.collisionModels.find(uint32_t(model));
        const auto matrix = storylandScriptObjectMatrix(point.x, point.y, point.z, 0, 0, heading);
        if (found != world.collisionModels.end()) {
            return hits(found->second, matrix);
        }
        // The host's on-foot collision body is the same capsule-sized box
        // used by movement and object contacts when no ped COL is resident.
        StorylandNativeCollisionModel body;
        body.minimum = {-.35f, -.35f, 0};
        body.maximum = {.35f, .35f, 1.8f};
        body.boxes.push_back({body.minimum, body.maximum, 0, 0});
        return hits(body, matrix);
    };
    if (mask & 4u) {
        if (ignoredCharacter!=0&&runtime.occupiedVehicle < 0 && pedHit(-1, runtime.playerPosition, 0)) {
            return true;
        }
        for (size_t index = 0; index < pedestrians.size() && index < runtime.pedestrians.size(); ++index) {
            const auto& actor = pedestrians[index];
            const auto& pedestrian = runtime.pedestrians[index];
            if (pedestrian.scriptHandle >= 0 || pedestrian.renderAlpha==0) {
                continue;
            }
            if (pedHit(-1, actor.position, actor.yaw - 90)) {
                return true;
            }
        }
    }
    if (!runtime.missionEnvironment) {
        return false;
    }
    const auto& mission = *runtime.missionEnvironment;
    if (mask & 2u) {
        for (const auto& entry : mission.missionVehicles) {
            const auto& vehicle = entry.second;
            if (vehicle.deleted || !vehicle.collisionEnabled || vehicle.spawnedSlot >= 0) {
                continue;
            }
            const auto model = world.collisionModels.find(uint32_t(vehicle.model));
            if (model != world.collisionModels.end() && hits(model->second,
                storylandScriptObjectMatrix(vehicle.x, vehicle.y, vehicle.z, 0, 0, vehicle.heading))) {
                return true;
            }
        }
    }
    if (mask & 4u) {
        for (const auto& entry : mission.missionCharacters) {
            const auto& pedestrian = entry.second;
            if (entry.first!=ignoredCharacter&&pedestrian.vehicle < 0 && pedHit(pedestrian.model,
                {pedestrian.x, pedestrian.y, pedestrian.z}, pedestrian.heading)) {
                return true;
            }
        }
    }
    if (mask & 8u) {
        for (const auto& object : mission.objects) {
            float fraction = 1;
            StorylandNativePoint normal;
            if (storylandTraceScriptObject(world, object, start, end, fraction, normal)) {
                return true;
            }
        }
    }
    return false;
}

bool storylandPhotographLineClear(const StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const std::vector<StorylandNativeTrafficActor>& cars,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,int32_t character,
    const StorylandNativePoint& start,const StorylandNativePoint& end) {
    if(!runtime.missionEnvironment)return false;
    const auto found=runtime.missionEnvironment->missionCharacters.find(character);
    if(found==runtime.missionEnvironment->missionCharacters.end())return false;
    const auto& target=found->second;
    auto matrix=storylandScriptObjectMatrix(target.x,target.y,target.z,0,0,target.heading);
    StorylandNativeCollisionModel fallback;
    fallback.minimum={-.35f,-.35f,0};fallback.maximum={.35f,.35f,1.8f};
    fallback.boxes.push_back({fallback.minimum,fallback.maximum,0,0});
    const auto native=world.collisionModels.find(uint32_t(target.model));
    if(world.paths.vcs&&native!=world.collisionModels.end()) {
        const auto& frame=runtime.missionEnvironment->characterWorldMatrix;
        if(!frame||!frame(character,matrix)||
            !std::all_of(matrix.begin(),matrix.end(),[](float value){return std::isfinite(value);}))return false;
    }
    float fraction=1;StorylandNativePoint normal;
    storylandTraceCollisionModel(native==world.collisionModels.end()?fallback:native->second,
        matrix,start,end,fraction,normal);
    // Native accepts the target itself as the nearest hit. Test all other
    // categories only up to its front surface, preserving nearer occluders.
    fraction=std::max(0.0f,fraction-.00001f);
    const StorylandNativePoint limit{start.x+(end.x-start.x)*fraction,
        start.y+(end.y-start.y)*fraction,start.z+(end.z-start.z)*fraction};
    return !storylandScriptLineOfSightHit(runtime,world,cars,pedestrians,start,limit,31,character);
}

bool storylandPhotographVehicleLineClear(const StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const std::vector<StorylandNativeTrafficActor>& cars,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,int32_t vehicle,
    const StorylandNativePoint& start,const StorylandNativePoint& end) {
    if(!runtime.missionEnvironment)return false;
    const auto& mission=*runtime.missionEnvironment;
    const auto found=mission.missionVehicles.find(vehicle);
    if(found==mission.missionVehicles.end()||found->second.deleted||!mission.vehicleWorldMatrix)return false;
    std::array<float,16> matrix{};
    if(!mission.vehicleWorldMatrix(vehicle,matrix)||
        !std::all_of(matrix.begin(),matrix.end(),[](float value){return std::isfinite(value);}))return false;
    const auto collision=world.collisionModels.find(uint32_t(found->second.model));
    if(collision==world.collisionModels.end())return false;
    float fraction=1;
    if(found->second.collisionEnabled) {
        StorylandNativePoint normal;
        storylandTraceCollisionModel(collision->second,matrix,start,end,fraction,normal);
        // Only a colliding photographed subject can be the nearest hit.
        fraction=std::max(0.0f,fraction-.00001f);
    }
    const StorylandNativePoint limit{start.x+(end.x-start.x)*fraction,
        start.y+(end.y-start.y)*fraction,start.z+(end.z-start.z)*fraction};
    return !storylandScriptLineOfSightHit(runtime,world,cars,pedestrians,start,limit,31);
}

float storylandVehicleTyreGrip(const StorylandRuntimeVehicle& vehicle) {
    const size_t count = vehicle.nativeDefinition.twoWheels() ? 2 : 4;
    float grip = 0;
    for (size_t wheel = 0; wheel < count; ++wheel) {
        grip += vehicle.tyreStatus[wheel] == 0 ? 1.0f : 0.5f;
    }
    return grip / float(count);
}

float storylandVehicleCollisionDamage(const StorylandRuntimeVehicle& vehicle,
    float damage, bool occupied) {
    if (!vehicle.canBeDamaged || (vehicle.damageProofs & 8u) || !(damage > 0)) {
        return 0;
    }
    constexpr float radians = 0.0174532925199433f;
    const float up = std::cos((vehicle.bodyRoll - vehicle.roadRoll) * radians) *
        std::cos((vehicle.bodyPitch + vehicle.roadPitch) * radians);
    // 0x136DB0 skips the upside-down guard for the player's current car.
    if (!occupied && vehicle.upsideDownDamageDisabled && up < 0) {
        return 0;
    }
    // 0x13787C..0x137914: strong cars receive one third of normal impact
    // health loss, for both player (1/6 vs 1/2) and ambient (1/12 vs 1/4).
    return vehicle.strong ? damage / 3.0f : damage;
}

void storylandApplyMissionVehicleOverrides(StorylandRuntimeVehicle& vehicle,
    StorylandNativeTrafficActor& actor, StorylandMissionEnvironment& world, int32_t handle) {
    const auto found = world.missionVehicles.find(handle);
    if (found == world.missionVehicles.end()) {
        return;
    }
    auto& scripted = found->second;
    if(scripted.spawnedSlot<0&&!scripted.doorsChangedByScript&&
        vehicle.nativeDefinition.vcsHovercraft()&&(vehicle.nativeDefinition.modelFlags&0x100u)!=0)
        for(size_t door=2;door<scripted.doorDamage.size();++door)scripted.doorDamage[door]=3;
    if (scripted.alarmChangedByScript || scripted.spawnedSlot < 0) {
        vehicle.alarmMilliseconds = scripted.alarmMilliseconds;
        scripted.alarmChangedByScript = false;
    }
    if (scripted.doorsChangedByScript || scripted.spawnedSlot < 0) {
        vehicle.doorDamage = scripted.doorDamage;
        vehicle.bonnetOpenRatio = scripted.doorOpenRatios[0];
        vehicle.bootOpenRatio = scripted.doorOpenRatios[1];
        for (size_t door = 0; door < vehicle.doorOpenRatios.size(); ++door) {
            vehicle.doorOpenRatios[door] = scripted.doorOpenRatios[door + 2];
        }
        vehicle.bootAngularVelocity = scripted.bootAngularVelocity;
        vehicle.visiblyDamaged = vehicle.visiblyDamaged || scripted.visiblyDamaged;
        scripted.doorsChangedByScript = false;
    }
    if (scripted.angularVelocityChangedByScript || scripted.spawnedSlot < 0) {
        vehicle.angularVelocity = scripted.angularVelocity;
        scripted.angularVelocityChangedByScript = false;
    }
    const bool changedMission = vehicle.aiActive != scripted.aiActive ||
        vehicle.aiMission != scripted.aiMission || vehicle.aiMissionStartedAt != scripted.aiMissionStartedAt;
    vehicle.aiActive = scripted.aiActive;
    vehicle.aiMission = scripted.aiMission;
    vehicle.aiMissionStartedAt = scripted.aiMissionStartedAt;
    vehicle.aiTargetVehicle = scripted.aiTargetVehicle;
    if (!vehicle.nativeDefinition.vcsModelLayout) {
        vehicle.explicitComponents = world.lcsExtendedWorld.explicitVehicleComponents.count(handle) != 0;
        vehicle.components = scripted.components;
    }
    vehicle.drivingStyle = scripted.drivingStyle;
    vehicle.avoidLevelTransitions = scripted.avoidLevelTransitions;
    vehicle.fastLane = scripted.fastLane;
    vehicle.lanePolicySet = scripted.lanePolicySet;
    vehicle.convoy = scripted.convoy;
    vehicle.straightLineDistance = scripted.straightLineDistance;
    if (scripted.driver >= 0 || (vehicle.nativeDefinition.vcsModelLayout && !scripted.ambientOrigin))
        vehicle.hasDriver = scripted.driver >= 0 && vehicle.aiActive;
    if (scripted.routeSeedChanged || scripted.spawnedSlot < 0) {
        vehicle.routeSeed = scripted.routeSeed;
        actor.routeSeed = scripted.routeSeed;
        scripted.routeSeedChanged = false;
    }
    if (vehicle.aiActive) {
        vehicle.parked = false;
        if ((!vehicle.nativeDefinition.vcsModelLayout || !scripted.drivingToTarget) && world.pedestrianPaths &&
            vehicle.nativeDefinition.domain == StorylandVehicleDomain::Road &&
            (changedMission || scripted.spawnedSlot < 0 ||
             (vehicle.nativeDefinition.vcsModelLayout && (vehicle.aiMission == 16 || vehicle.aiMission == 18 || vehicle.aiMission == 19) &&
              scripted.autoPilotRoutePending))) {
            if (vehicle.nativeDefinition.vcsModelLayout && (vehicle.aiMission == 16 || vehicle.aiMission == 18 || vehicle.aiMission == 19)) {
                storylandJoinMissionCarWithRoad(*world.pedestrianPaths, actor);
                scripted.autoPilotRoutePending = false;
            } else storylandJoinNativeVehicleRoute(*world.pedestrianPaths, actor, world.zones,
                vehicle.avoidLevelTransitions);
        }
    }
    vehicle.massMultiplier = scripted.massMultiplier;
    vehicle.turnMassMultiplier = scripted.turnMassMultiplier;
    vehicle.heavy = scripted.heavy;
    vehicle.mass = vehicle.nativeDefinition.mass * vehicle.massMultiplier * (scripted.heavy ? 3.0f : 1.0f);
    vehicle.panelDamage = scripted.panelDamage;
    vehicle.lightDamage = scripted.lightDamage;
    vehicle.heliOrientation = scripted.heliOrientation;
    vehicle.heliStabiliser = scripted.heliStabiliser;
    vehicle.heliCrashing = scripted.heliCrashing;
    if (scripted.rotorChangedByScript || scripted.spawnedSlot < 0) {
        vehicle.rotorSpeed = scripted.rotorSpeed;
        scripted.rotorChangedByScript = false;
    }
    if (scripted.primaryColor < 0 && scripted.secondaryColor < 0 &&
        scripted.colorSet < 0 && scripted.scriptColorSet < 0 && vehicle.colorSet >= 0) {
        scripted.primaryColor = vehicle.primaryColor;
        scripted.secondaryColor = vehicle.secondaryColor;
        scripted.colorSet = vehicle.colorSet;
    }
    vehicle.watertight = scripted.watertight;
    vehicle.taxiLight = scripted.taxiLight;
    if(scripted.bikeStandingChangedByScript||scripted.spawnedSlot<0) {
        vehicle.bikeStandingOverride=scripted.bikeStandingOverride;
        scripted.bikeStandingChangedByScript=false;
    }
    vehicle.strong = scripted.strong;
    vehicle.upsideDownDamageDisabled = scripted.upsideDownDamageDisabled;
    vehicle.canBurstTyres = scripted.canBurstTyres;
    vehicle.visiblyDamaged = scripted.visiblyDamaged;
    vehicle.boatAnchored = scripted.boatAnchored;
    vehicle.tractionMultiplier = scripted.tractionMultiplier;
    vehicle.tyreStatus = scripted.tyreStatus;
    vehicle.traction = std::max(0.0f, vehicle.nativeDefinition.traction * vehicle.tractionMultiplier) *
        storylandVehicleTyreGrip(vehicle);
    vehicle.colorSet = scripted.colorSet;
    vehicle.scriptColorSet = scripted.scriptColorSet;
    vehicle.visible = scripted.visible;
    vehicle.clumpVisible = scripted.clumpVisible;
    if (scripted.rotationChangedByScript || scripted.spawnedSlot < 0) {
        vehicle.bodyRoll = scripted.rotation[0] + vehicle.roadRoll;
        vehicle.bodyPitch = scripted.rotation[1] - vehicle.roadPitch;
        actor.yaw = scripted.heading + 90.0f;
        scripted.rotationChangedByScript = false;
    }
    if (vehicle.boatAnchored&&vehicle.nativeDefinition.domain==StorylandVehicleDomain::Water) {
        vehicle.speed = 0;
        vehicle.lateralSpeed = 0;
        vehicle.throttle = 0;
    }
}

void storylandCaptureMissionVehicleOverrides(const StorylandRuntimeVehicle& vehicle,
    const StorylandNativeTrafficActor& actor, StorylandMissionEnvironment& world, int32_t handle) {
    const auto found = world.missionVehicles.find(handle);
    if (found == world.missionVehicles.end()) {
        return;
    }
    auto& scripted = found->second;
    if(!scripted.bikeStandingChangedByScript)
        scripted.bikeStandingOverride=vehicle.bikeStandingOverride;
    if (!scripted.alarmChangedByScript) scripted.alarmMilliseconds = vehicle.alarmMilliseconds;
    if (!scripted.doorsChangedByScript) {
        scripted.doorDamage = vehicle.doorDamage;
        scripted.doorOpenRatios[0] = vehicle.bonnetOpenRatio;
        scripted.doorOpenRatios[1] = vehicle.bootOpenRatio;
        for (size_t door = 0; door < vehicle.doorOpenRatios.size(); ++door) {
            scripted.doorOpenRatios[door + 2] = vehicle.doorOpenRatios[door];
        }
        scripted.bootAngularVelocity = vehicle.bootAngularVelocity;
    }
    if (!scripted.angularVelocityChangedByScript) scripted.angularVelocity = vehicle.angularVelocity;
    if (!scripted.rotorChangedByScript) scripted.rotorSpeed = vehicle.rotorSpeed;
    scripted.visiblyDamaged = scripted.visiblyDamaged || vehicle.visiblyDamaged;
    scripted.primaryColor = vehicle.primaryColor;
    scripted.secondaryColor = vehicle.secondaryColor;
    scripted.colorSet = vehicle.colorSet;
    scripted.scriptColorSet = vehicle.scriptColorSet;
    scripted.clumpVisible = vehicle.clumpVisible;
    for (size_t wheel = 0; wheel < scripted.tyreStatus.size(); ++wheel) {
        scripted.tyreStatus[wheel] = std::max(scripted.tyreStatus[wheel], vehicle.tyreStatus[wheel]);
    }
    if (!scripted.rotationChangedByScript) {
        scripted.rotation = {vehicle.bodyRoll - vehicle.roadRoll,
            vehicle.bodyPitch + vehicle.roadPitch, actor.yaw - 90.0f};
    }
}

namespace {
StorylandVehicleAccessState vehicleAccessState(const StorylandRuntimeVehicle& vehicle);
bool vcs039aTurnForce(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& actors,int32_t car,int32_t character,uint32_t random);
bool vcsHelicopterHeightAboveSurface(const StorylandNativeWorld& world,
    const StorylandNativeTrafficActor& actor,StorylandRuntimeVehicle& vehicle,float& height) {
    const auto& definition=vehicle.nativeDefinition;
    const auto frame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
        {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
        vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch),definition.previewRoot);
    if(!std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);}))return false;
    const StorylandNativePoint from{frame[12],frame[13],frame[14]},to{frame[12],frame[13],-1000.0f};
    const float fraction=storylandTraceNativeBuildings(world,from,to);
    if(!std::isfinite(fraction)||fraction<0||fraction>1)return false;
    float surface=fraction<1?from.z+(to.z-from.z)*fraction:0.0f;
    float water=0;
    if(storylandNativeWaterHeight(world,from.x,from.y,water)&&
        storylandVcsWaterDepthAllowsContact(from.z,water))surface=std::max(surface,water);
    height=from.z-surface;
    if(!std::isfinite(height))return false;
    if(height>2.0f)vehicle.wheelContacts={};
    return true;
}
}

void storylandBindVehicleAutopilotCallbacks(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& actors, const StorylandNativeWorld& nativeWorld) {
    if (!runtime.missionEnvironment) return;
    auto& world = *runtime.missionEnvironment;
    world.vcsCharacterVehicleTurnForce=[&runtime,&actors](int32_t vehicle,int32_t character,uint32_t random) {
        return vcs039aTurnForce(runtime,actors,vehicle,character,random);
    };
    world.vehicleCanExit = [&runtime,&actors,&nativeWorld](int32_t handle) {
        const auto& live = *runtime.missionEnvironment;
        const auto found = live.missionVehicles.find(handle);
        if (found == live.missionVehicles.end() || found->second.deleted) return false;
        const int32_t slot = found->second.spawnedSlot;
        if (slot < 0 || size_t(slot) >= runtime.vehicles.size() ||
            runtime.vehicles[size_t(slot)].scriptHandle != handle) return false;
        auto& vehicle=runtime.vehicles[size_t(slot)];
        auto access = vehicleAccessState(vehicle);
        if(access.vcs&&(access.helicopter||access.hovercraft)&&(size_t(slot)>=actors.size()||
            !vcsHelicopterHeightAboveSurface(nativeWorld,actors[size_t(slot)],vehicle,access.heightAboveGround)))return false;
        access.doorLock = found->second.doorLock;
        return storylandRetailVehicleCanExit(access);
    };
    world.vehicleBoatBuoyant = [&runtime](int32_t handle) {
        const auto found = runtime.missionEnvironment->missionVehicles.find(handle);
        if (found == runtime.missionEnvironment->missionVehicles.end() || found->second.deleted) return false;
        const int32_t slot = found->second.spawnedSlot;
        return slot >= 0 && size_t(slot) < runtime.vehicles.size() &&
            runtime.vehicles[size_t(slot)].scriptHandle == handle && runtime.vehicles[size_t(slot)].water.buoyant;
    };
    world.vehicleInWater = [&runtime](int32_t handle) {
        const auto found=runtime.missionEnvironment->missionVehicles.find(handle);
        if(found==runtime.missionEnvironment->missionVehicles.end()||found->second.deleted)return false;
        const int32_t slot=found->second.spawnedSlot;
        if(slot<0||size_t(slot)>=runtime.vehicles.size())return false;
        const auto& vehicle=runtime.vehicles[size_t(slot)];
        if(vehicle.scriptHandle!=handle)return false;
        return vehicle.nativeDefinition.domain==StorylandVehicleDomain::Water||vehicle.nativeDefinition.vcsHovercraft()?
            vehicle.water.buoyant:vehicle.water.immersion>0;
    };
    world.vehicleWorldCorners = [&runtime,&nativeWorld](int32_t handle,
        std::array<std::array<float,3>,8>& output) {
        const auto& live=*runtime.missionEnvironment;
        const auto found=live.missionVehicles.find(handle);
        if(found==live.missionVehicles.end()||found->second.deleted||!live.vehicleWorldMatrix)return false;
        const auto model=nativeWorld.collisionModels.find(uint32_t(found->second.model));
        std::array<float,16> matrix{};
        if(model==nativeWorld.collisionModels.end()||!live.vehicleWorldMatrix(handle,matrix))return false;
        for(size_t corner=0;corner<8;++corner) {
            const std::array<float,3> local{corner&1?model->second.maximum.x:model->second.minimum.x,
                corner&2?model->second.maximum.y:model->second.minimum.y,
                corner&4?model->second.maximum.z:model->second.minimum.z};
            for(size_t axis=0;axis<3;++axis)
                output[corner][axis]=matrix[12+axis]+matrix[axis]*local[0]+matrix[4+axis]*local[1]+matrix[8+axis]*local[2];
        }
        return true;
    };
    world.unlockVehicleDoorsInArea = [&runtime, &actors](const std::array<float,4>& bounds) {
        for (size_t slot = 0; slot < actors.size() && slot < runtime.vehicles.size(); ++slot) {
            const auto& position = actors[slot].position;
            if (position.x >= bounds[0] && position.x <= bounds[2] &&
                position.y >= bounds[1] && position.y <= bounds[3]) runtime.vehicles[slot].doorLock = 1;
        }
    };
    world.vehicleEntryInProgress = [&runtime](int32_t handle) {
        const int32_t slot = runtime.transitionVehicle;
        return slot >= 0 && size_t(slot) < runtime.vehicles.size() &&
            runtime.vehicles[size_t(slot)].scriptHandle == handle &&
            (runtime.vehicleAction == StorylandVehicleAction::Enter ||
                runtime.vehicleAction == StorylandVehicleAction::Hijack) &&
            runtime.vehiclePhase.phase != StorylandVehiclePhase::Approach;
    };
    world.vehiclePathCoordinate = [&runtime, &actors, &nativeWorld](int32_t handle, int32_t index,
        std::array<float,3>& output) {
        const auto& live = *runtime.missionEnvironment;
        const auto found = live.missionVehicles.find(handle);
        if (found == live.missionVehicles.end()) return false;
        const int32_t slot = found->second.spawnedSlot;
        if (slot < 0 || size_t(slot) >= actors.size() || size_t(slot) >= runtime.vehicles.size() ||
            runtime.vehicles[size_t(slot)].scriptHandle != handle) return false;
        StorylandNativePoint point;
        if (!storylandNativeVehiclePathCoordinate(nativeWorld.paths, actors[size_t(slot)], index, point,
            &nativeWorld.zones, found->second.avoidLevelTransitions)) return false;
        output = {point.x, point.y, point.z};
        return true;
    };
    world.consumeVehicleCollision = [&runtime, &actors](int32_t handle) -> int32_t {
        auto& live = *runtime.missionEnvironment;
        const auto found = live.missionVehicles.find(handle);
        if (found == live.missionVehicles.end()) return -1;
        const int32_t slot = found->second.spawnedSlot;
        if (slot < 0 || size_t(slot) >= actors.size() || size_t(slot) >= runtime.vehicles.size() ||
            runtime.vehicles[size_t(slot)].scriptHandle != handle) return -1;
        auto& source = runtime.vehicles[size_t(slot)];
        const int32_t contact = source.lastCollidedSlot;
        source.lastCollidedSlot = -1;
        if (contact < 0 || size_t(contact) >= actors.size() || size_t(contact) >= runtime.vehicles.size()) return -1;
        auto& vehicle = runtime.vehicles[size_t(contact)];
        if (vehicle.scriptHandle >= 0) {
            const auto existing = live.missionVehicles.find(vehicle.scriptHandle);
            return existing != live.missionVehicles.end() && !existing->second.deleted ? vehicle.scriptHandle : -1;
        }
        const bool available = vehicle.nativeDefinition.vcsModelLayout ?
            storylandScriptVehicleCapacityAvailable(live.missionVehicles, 128) :
            live.missionVehicles.size() < 128;
        if (!available || live.nextMissionVehicleHandle == INT32_MAX) return -1;
        const auto& actor = actors[size_t(contact)];
        StorylandMissionEnvironment::VehicleScriptState state;
        if(!vehicle.nativeDefinition.vcsModelLayout) {
            state.doorLock=1;
            state.boatAnchored=vehicle.boatAnchored;
        }
        state.model = int32_t(vehicle.nativeDefinition.modelId);
        state.spawnedSlot = contact;
        state.x = actor.position.x;
        state.y = actor.position.y;
        state.z = actor.position.z;
        state.heading = actor.yaw - 90;
        state.health = vehicle.health;
        state.alive = vehicle.health > 0;
        state.speed = vehicle.speed;
        state.ambientOrigin = true;
        state.persistent = false;
        state.scriptStatus = vehicle.parked ? 4 : 3;
        const int32_t result = live.nextMissionVehicleHandle++;
        live.missionVehicles.emplace(result, state);
        vehicle.scriptHandle = result;
        storylandCaptureMissionVehicleOverrides(vehicle, actor, live, result);
        return result;
    };
    world.createRandomParkedVehicle = [&runtime, &actors, &nativeWorld](const std::array<float,3>& point,
        float heading) {
        size_t parked = 0;
        for (size_t slot = 0; slot < actors.size() && slot < runtime.vehicles.size(); ++slot) {
            if (runtime.vehicles[slot].parked && runtime.vehicles[slot].health > 0) ++parked;
        }
        if (parked >= 30 || actors.size() >= runtime.vehicles.size()) return false;
        // Retail3D41B8..3D4488 whitelist, recovered by executing every model ID
        // through the actual branch tree. The host resident model list supplies
        // the native streaming pool's twenty candidate slots.
        constexpr std::array<uint16_t,24> allowed{172,175,176,180,192,195,196,199,201,203,206,210,
            211,212,223,224,226,229,253,254,270,271,272,276};
        // LCS 229BE4..229E00 has a different model whitelist and skips the
        // first resident bike before considering the remaining twenty slots.
        constexpr std::array<uint16_t,19> lcsAllowed{132,133,135,136,141,142,144,146,
            149,150,151,153,160,167,168,205,206,207,210};
        auto& live = *runtime.missionEnvironment;
        const size_t start = live.nextNativeRandom() % 20;
        const size_t count = std::min(size_t(20), runtime.modelDefinitions.size());
        size_t chosen = SIZE_MAX;
        bool skipFirstBike = !runtime.crime.vcs;
        for (size_t step = 0; step < 20; ++step) {
            const size_t candidate = (start + step) % 20;
            if (candidate >= count) continue;
            const auto& definition = runtime.modelDefinitions[candidate];
            if (skipFirstBike && definition.twoWheels()) {
                skipFirstBike = false;
                continue;
            }
            const bool eligible = runtime.crime.vcs ?
                std::find(allowed.begin(), allowed.end(), definition.modelId) != allowed.end() :
                std::find(lcsAllowed.begin(), lcsAllowed.end(), definition.modelId) != lcsAllowed.end();
            if (definition.domain == StorylandVehicleDomain::Road &&
                eligible) {
                chosen = candidate;
                break;
            }
        }
        if (chosen == SIZE_MAX) return false;
        StorylandNativeTrafficActor actor;
        actor.vehicle = true;
        actor.position = {point[0], point[1], point[2]};
        actor.yaw = heading + 90;
        StorylandRuntimeVehicle vehicle;
        vehicle.model = chosen;
        vehicle.parked = true;
        vehicle.hasDriver = false;
        storylandApplyVehicleDefinition(vehicle, runtime.modelDefinitions[chosen]);
        if (chosen < runtime.modelExtents.size()) {
            vehicle.halfWidth = runtime.modelExtents[chosen][0];
            vehicle.halfLength = runtime.modelExtents[chosen][1];
            vehicle.bodyHeight = runtime.modelExtents[chosen][2];
        }
        if (!vehicle.nativeDefinition.colors.empty()) {
            vehicle.colorSet = int32_t(live.nextNativeRandom() % vehicle.nativeDefinition.colors.size());
            vehicle.primaryColor = vehicle.nativeDefinition.colors[size_t(vehicle.colorSet)][0];
            vehicle.secondaryColor = vehicle.nativeDefinition.colors[size_t(vehicle.colorSet)][1];
        }
        storylandJoinNativeVehicleRoute(nativeWorld.paths, actor, &nativeWorld.zones);
        runtime.vehicles[actors.size()] = vehicle;
        actors.push_back(actor);
        return true;
    };
}

void storylandStepVehicleScriptPhysics(StorylandRuntimeVehicle& vehicle,
    StorylandNativeTrafficActor& actor, float seconds) {
    if (!std::isfinite(seconds) || seconds <= 0 || vehicle.positionFrozen) {
        return;
    }
    if (vehicle.bootAngularVelocity != 0 && vehicle.doorDamage[1] != 3) {
        // Native POP_CAR_BOOT_USING_PHYSICS starts the boot hinge at -2.
        vehicle.bootOpenRatio = std::clamp(vehicle.bootOpenRatio - vehicle.bootAngularVelocity * seconds,
            0.0f, 1.0f);
        vehicle.bootAngularVelocity *= std::exp(-2.0f * seconds);
        if (vehicle.bootOpenRatio >= 1) {
            vehicle.bootAngularVelocity = 0;
        }
    }
    const auto& omega = vehicle.angularVelocity;
    const double magnitude = std::hypot(double(omega[0]), double(omega[1]), double(omega[2]));
    if (!(magnitude > 0) || !std::isfinite(magnitude)) {
        return;
    }
    constexpr double radians = 0.01745329251994329577;
    const double angle = std::remainder(magnitude * seconds, 6.283185307179586);
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);
    const std::array<double,3> axis{omega[0] / magnitude, omega[1] / magnitude, omega[2] / magnitude};
    const auto frame = storylandVehicleMeshFrame(vehicle.nativeDefinition,
        {actor.position.x, actor.position.y, actor.position.z}, actor.yaw, 0, 0,
        vehicle.bodyRoll - vehicle.roadRoll, vehicle.bodyPitch + vehicle.roadPitch, false);
    const auto rotate = [&](size_t offset) {
        const std::array<double,3> vector{frame[offset], frame[offset + 1], frame[offset + 2]};
        const double dot = axis[0] * vector[0] + axis[1] * vector[1] + axis[2] * vector[2];
        const std::array<double,3> cross{axis[1] * vector[2] - axis[2] * vector[1],
            axis[2] * vector[0] - axis[0] * vector[2], axis[0] * vector[1] - axis[1] * vector[0]};
        std::array<double,3> result{};
        for (size_t component = 0; component < result.size(); ++component) {
            result[component] = vector[component] * cosine + cross[component] * sine +
                axis[component] * dot * (1 - cosine);
        }
        return result;
    };
    const auto right = rotate(0);
    const auto forward = rotate(4);
    const auto up = rotate(8);
    vehicle.bodyRoll = float(std::atan2(-right[2], up[2]) / radians) + vehicle.roadRoll;
    vehicle.bodyPitch = float(std::asin(std::clamp(forward[2], -1.0, 1.0)) / radians) - vehicle.roadPitch;
    const float yaw=float(std::atan2(-forward[0],forward[1])/radians)+90.0f;
    const float c=float(std::cos((yaw-actor.yaw)*radians)),s=float(std::sin((yaw-actor.yaw)*radians));
    const float speed=vehicle.speed,lateral=vehicle.lateralSpeed;
    vehicle.speed=c*speed+s*lateral;
    vehicle.lateralSpeed=c*lateral-s*speed;
    actor.yaw=yaw;
}

int32_t storylandFindAmbientVehicleNoSave(StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& actors, StorylandMissionEnvironment& world,
    const std::array<float,4>& bounds, int32_t model, bool aliveOnly) {
    for (size_t count = std::min(actors.size(), runtime.vehicles.size()); count > 0; --count) {
        const size_t slot = count - 1;
        auto& vehicle = runtime.vehicles[slot];
        const auto& actor = actors[slot];
        const uint32_t flags = vehicle.nativeDefinition.modelFlags;
        const int handlingClass = flags & 0x01000000u ? 2 : flags & 0x02000000u ? 3 :
            flags & 0x04000000u ? 5 : flags & 0x08000000u ? 4 : 1;
        if (!vehicle.nativeDefinition.hasNativeHandling || (handlingClass != 1 && handlingClass != 2) ||
            (aliveOnly && vehicle.health == 0) || (model >= 0 && uint32_t(model) != vehicle.nativeDefinition.modelId)) {
            continue;
        }
        if (actor.position.x < std::min(bounds[0], bounds[2]) || actor.position.x > std::max(bounds[0], bounds[2]) ||
            actor.position.y < std::min(bounds[1], bounds[3]) || actor.position.y > std::max(bounds[1], bounds[3])) {
            continue;
        }
        if (vehicle.scriptHandle >= 0) {
            const auto existing = world.missionVehicles.find(vehicle.scriptHandle);
            if (existing != world.missionVehicles.end() && existing->second.ambientOrigin && !existing->second.deleted) {
                return vehicle.scriptHandle;
            }
            continue;
        }
        if (world.nextMissionVehicleHandle == INT32_MAX) {
            return -1;
        }
        const int32_t handle = world.nextMissionVehicleHandle++;
        StorylandMissionEnvironment::VehicleScriptState state;
        if(!vehicle.nativeDefinition.vcsModelLayout) {
            state.doorLock=1;
            state.boatAnchored=vehicle.boatAnchored;
        }
        state.model = int32_t(vehicle.nativeDefinition.modelId);
        state.spawnedSlot = int32_t(slot);
        state.x = actor.position.x;
        state.y = actor.position.y;
        state.z = actor.position.z;
        state.heading = actor.yaw - 90.0f;
        state.health = vehicle.health;
        state.alive = vehicle.health > 0;
        state.speed = vehicle.speed;
        state.ambientOrigin = true;
        state.persistent = false;
        state.primaryColor = vehicle.primaryColor;
        state.secondaryColor = vehicle.secondaryColor;
        state.colorSet = vehicle.colorSet;
        state.scriptColorSet = vehicle.scriptColorSet;
        world.missionVehicles.emplace(handle, state);
        vehicle.scriptHandle = handle;
        storylandCaptureMissionVehicleOverrides(vehicle, actor, world, handle);
        return handle;
    }
    return -1;
}

namespace {
constexpr float radians=0.01745329251994329577f;
float distance(StorylandNativePoint a,StorylandNativePoint b) { return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z); }
float approach(float value,float target,float step) { return value<target?std::min(target,value+step):std::max(target,value-step); }
bool storylandMissionDriverProtected(const StorylandPlayTestRuntime& runtime,
    const StorylandRuntimeVehicle& vehicle) {
    if (!runtime.missionEnvironment || vehicle.scriptHandle < 0) {
        return false;
    }
    const auto& world = *runtime.missionEnvironment;
    const auto car = world.missionVehicles.find(vehicle.scriptHandle);
    if (car == world.missionVehicles.end()) {
        return false;
    }
    const auto driver = world.missionCharacters.find(car->second.driver);
    return driver != world.missionCharacters.end() &&
        (driver->second.cannotBeDraggedOut || driver->second.cantBeDraggedOut);
}
void storylandShiftPassengerSideCarjackDriver(StorylandPlayTestRuntime& runtime,
    const StorylandRuntimeVehicle& vehicle) {
    if(storylandVcsServiceSeatOccupied(runtime,vehicle,0))return;
    if (!vehicle.nativeDefinition.vcsModelLayout || !runtime.missionEnvironment ||
        vehicle.scriptHandle < 0) return;
    auto& world = *runtime.missionEnvironment;
    const auto car = world.missionVehicles.find(vehicle.scriptHandle);
    if (car == world.missionVehicles.end()) return;
    const auto driver = world.missionCharacters.find(car->second.driver);
    if (driver == world.missionCharacters.end() || !driver->second.alive ||
        !driver->second.canBeDraggedFromPassengerSide || driver->second.passengerSideCarjack) return;
    if (world.inVehicle && world.occupiedMissionVehicle == vehicle.scriptHandle &&
        world.playerPassengerSeat == 0) return;
    for (const auto& entry : world.missionCharacters)
        if (entry.second.alive && entry.second.vehicle == vehicle.scriptHandle &&
            !entry.second.attached && entry.second.passengerSeat == 0) return;
    // Retail20BDBC: door8, ENTER_CAR_AS_DRIVER and an empty passenger0.
    // State51 is deliberately distinct from the sitting-query state55.
    driver->second.passengerSeat = 0;
    driver->second.passengerSideCarjack = true;
    car->second.driver = -1;
}
void storylandCancelPassengerSideCarjack(StorylandPlayTestRuntime& runtime,
    const StorylandRuntimeVehicle& vehicle) {
    if (!runtime.missionEnvironment || vehicle.scriptHandle < 0) return;
    auto& world = *runtime.missionEnvironment;
    const auto car = world.missionVehicles.find(vehicle.scriptHandle);
    if (car == world.missionVehicles.end()) return;
    for (auto& entry : world.missionCharacters) {
        auto& character = entry.second;
        if (character.vehicle != vehicle.scriptHandle || !character.passengerSideCarjack) continue;
        character.passengerSideCarjack = false;
        if (car->second.driver < 0 && character.alive) {
            character.passengerSeat = -1;
            car->second.driver = entry.first;
        }
    }
}
void storylandDisplaceScriptDriver(StorylandPlayTestRuntime& runtime,
    const StorylandRuntimeVehicle& vehicle, const StorylandNativePoint& position) {
    if (!runtime.missionEnvironment || vehicle.scriptHandle < 0) {
        return;
    }
    auto& world = *runtime.missionEnvironment;
    const auto car = world.missionVehicles.find(vehicle.scriptHandle);
    if (car == world.missionVehicles.end()) {
        return;
    }
    auto driver = world.missionCharacters.find(car->second.driver);
    if (driver == world.missionCharacters.end()) {
        driver = std::find_if(world.missionCharacters.begin(), world.missionCharacters.end(),
            [&](const auto& entry) {
                return entry.second.vehicle == vehicle.scriptHandle && entry.second.passengerSideCarjack;
            });
    }
    if (driver != world.missionCharacters.end()) {
        auto& character = driver->second;
        character.x = position.x;
        character.y = position.y;
        character.z = position.z;
        character.vehicle = -1;
        character.passengerSeat = -1;
        character.passengerSideCarjack = false;
        character.objective = runtime.crime.vcs ? 0x00E0 : 0x0470;
        character.objectiveTarget = runtime.crime.vcs ? 0 : vehicle.scriptHandle;
        character.objectiveCompleted = false;
        if(runtime.crime.vcs&&vehicle.nativeDefinition.nativeType==0)
            storylandBeginVcsCarjackedGetUp(world,driver->first,vehicle.scriptHandle,
                vehicle.nativeDefinition.lowVehicle(),runtime.vehicleExitRight,
                std::max(0.0f,runtime.vehicleActionTime-runtime.vehicleTiming.seconds[0]-
                    runtime.vehicleTiming.seconds[1]-runtime.vehicleTiming.seconds[2]));
        if (!runtime.crime.vcs) {
            StorylandLcsEntityExtensionState::Task task;
            task.objective = 41; task.target = vehicle.scriptHandle;
            world.lcsEntityExtension.tasks[driver->first] = task;
        }
    }
    car->second.driver = -1;
    if (world.inVehicle && world.occupiedMissionVehicle == vehicle.scriptHandle &&
        world.playerPassengerSeat >= 0 && !world.playerStayInCarWhenJacked) {
        world.playerExitVehicleRequested = true;
        world.playerVehicleTransition = true;
    }
    for (auto& entry : world.missionCharacters) {
        auto& passenger = entry.second;
        if (passenger.vehicle == vehicle.scriptHandle && !passenger.attached &&
            !passenger.stayInCarWhenJacked && (runtime.crime.vcs || !passenger.behavior.frightenedWhenJacked)) {
            passenger.objective = runtime.crime.vcs ? 0x00F1 : 0x03E7;
            passenger.objectiveTarget = vehicle.scriptHandle;
            passenger.objectiveCompleted = false;
            if (!runtime.crime.vcs) {
                StorylandLcsEntityExtensionState::Task task;
                task.objective = 16; task.target = vehicle.scriptHandle;
                world.lcsEntityExtension.tasks[entry.first] = task;
            }
        }
    }
}
unsigned pedestrianPathDirection(float x,float y) {
    constexpr float pi=3.14159265358979323846f;
    x=-x;y=-y;
    if(y==0)y=.0001f;
    float angle=x>0?(y>0?pi-std::atan2(x/y,1.0f):-std::atan2(x/y,1.0f)):
        (y>0?-(pi+std::atan2(x/y,1.0f)):-std::atan2(x/y,1.0f));
    if(angle<0)angle+=2*pi;
    angle=pi/8+2*pi-angle;
    if(angle>=2*pi)angle-=2*pi;
    return unsigned(std::floor(angle/(pi/4)))&7u;
}
StorylandNativePoint pedestrianFleeDestination(StorylandRuntimePed& ped,
    const StorylandNativeTrafficActor& actor,const StorylandNativePaths& paths,
    const StorylandNativeWorld* world,float seconds) {
    constexpr float quarterTurn=.78539816339744830962f;
    const float awayX=actor.position.x-ped.fleePosition.x;
    const float awayY=actor.position.y-ped.fleePosition.y;
    const unsigned direction=pedestrianPathDirection(awayX,awayY);
    const float desiredX=std::sin(float(direction)*quarterTurn);
    const float desiredY=std::cos(float(direction)*quarterTurn);
    ped.fleeRouteTime=std::max(0.0f,ped.fleeRouteTime-seconds);
    if(ped.fleeNextNode<paths.nodes.size()) {
        const auto& target=paths.nodes[ped.fleeNextNode].position;
        const unsigned toward=pedestrianPathDirection(target.x-actor.position.x,target.y-actor.position.y);
        const unsigned difference=(toward+8-direction)&7u;
        if(std::hypot(target.x-actor.position.x,target.y-actor.position.y)<.5f||
            (ped.fleeRouteTime==0&&difference>2&&difference<6)) {
            ped.fleeLastNode=ped.fleeNextNode;ped.fleeNextNode=UINT32_MAX;
        }
    }
    if(ped.fleeNextNode>=paths.nodes.size()&&ped.fleeRouteTime==0) {
        ped.fleeRouteTime=2;
        uint32_t node=ped.fleeLastNode;
        const float reuseDistanceSquared=paths.vcs?49.0f:7.0f;
        if(node>=paths.nodes.size()||distance(actor.position,paths.nodes[node].position)*
            distance(actor.position,paths.nodes[node].position)>reuseDistanceSquared) {
            float nearest=std::numeric_limits<float>::max();node=UINT32_MAX;
            for(size_t index=0;index<paths.nodes.size();++index) {
                const auto& candidate=paths.nodes[index];
                if(candidate.vehicle||candidate.water)continue;
                const float separation=distance(actor.position,candidate.position);
                if(separation<nearest) {nearest=separation;node=uint32_t(index);}
            }
        }
        if(node<paths.nodes.size()) {
            const auto& from=paths.nodes[node];
            float best=-std::numeric_limits<float>::max();uint32_t selected=UINT32_MAX;
            for(const auto& link:from.links) {
                if(link.target>=paths.nodes.size())continue;
                const auto& candidate=paths.nodes[link.target];
                if(candidate.vehicle||candidate.water||(!from.disabled&&candidate.disabled))continue;
                if(world&&storylandTraceNativeSegment(*world,
                    {actor.position.x,actor.position.y,actor.position.z+1},
                    {candidate.position.x,candidate.position.y,candidate.position.z+1})<1)continue;
                const float x=candidate.position.x-from.position.x,y=candidate.position.y-from.position.y;
                const float length=std::hypot(x,y);
                if(length<=.0001f)continue;
                const float alignment=(x*desiredX+y*desiredY)/length;
                if(alignment>=best) {best=alignment;selected=link.target;}
            }
            if(selected<paths.nodes.size()) {
                const auto& target=paths.nodes[selected].position;
                const unsigned toward=pedestrianPathDirection(target.x-from.position.x,target.y-from.position.y);
                if(((direction+8-toward)&7u)!=4&&selected!=ped.fleeLastNode) {
                    ped.fleeLastNode=node;ped.fleeNextNode=selected;ped.fleeRouteTime=2;
                }
            }
        }
    }
    if(ped.fleeNextNode<paths.nodes.size())return paths.nodes[ped.fleeNextNode].position;
    const float length=std::hypot(awayX,awayY);
    return {actor.position.x+(length>.0001f?awayX/length:desiredX)*10,
        actor.position.y+(length>.0001f?awayY/length:desiredY)*10,actor.position.z};
}
bool scriptVehicleFrozen(const StorylandPlayTestRuntime& runtime,const StorylandRuntimeVehicle& vehicle) {
    return vehicle.coopControlled||vehicle.coopSeatReserved||vehicle.positionFrozen||vehicle.craneCarried||storylandLcsVehiclePositionFrozen(runtime.missionEnvironment,vehicle.scriptHandle);
}
bool craneOwnsVehicle(const StorylandPlayTestRuntime& runtime,const StorylandRuntimeVehicle& vehicle) {
    if(vehicle.craneCarried)return true;
    if(vehicle.scriptHandle<0||!runtime.missionEnvironment)return false;
    const auto& cranes=runtime.missionEnvironment->lcsCranes.cranes;
    return std::any_of(cranes.begin(),cranes.end(),[&](const auto& crane) {
        // Native IsThisCarBeingTargettedByAnyCrane protects the reference
        // throughout approach, carrying and deactivation until it is cleared.
        return crane.vehicle==vehicle.scriptHandle;
    });
}
bool lcsBoatPhysicsAvailable(const StorylandRuntimeVehicle& vehicle) {
    return vehicle.nativeDefinition.nativeType==1&&
        !vehicle.nativeDefinition.vcsModelLayout&&vehicle.nativeDefinition.hasNativeHandling&&
        vehicle.nativeDefinition.hasNativeBoatHandling&&vehicle.nativeDefinition.collision;
}
bool lcsScriptBoatPhysics(const StorylandRuntimeVehicle& vehicle,
    const StorylandMissionEnvironment::VehicleScriptState* state) {
    return state&&state->scriptStatus==3&&lcsBoatPhysicsAvailable(vehicle)&&
        (state->autoPilotMission==14||state->autoPilotMission==0);
}
void stopFrozenVehicle(StorylandRuntimeVehicle& vehicle) {
    // CPhysical::ApplyMoveSpeed/ApplyTurnSpeed zero all velocity on a frozen body.
    vehicle.speed=0;vehicle.lateralSpeed=0;vehicle.verticalSpeed=0;vehicle.yawVelocity=0;
    vehicle.suspensionVelocity=0;vehicle.springPitchVelocity=0;vehicle.springRollVelocity=0;
}
bool scriptVehicleHasDriver(const StorylandPlayTestRuntime& runtime,const StorylandRuntimeVehicle& vehicle) {
    if(vehicle.coopControlled||vehicle.hasDriver||vehicle.missionDriver)return true;
    if(!runtime.missionEnvironment)return false;
    const auto& world=*runtime.missionEnvironment;
    const auto car=world.missionVehicles.find(vehicle.scriptHandle);
    if(car!=world.missionVehicles.end()&&car->second.driver>=2&&
        world.missionCharacters.count(car->second.driver)>0)return true;
    return std::any_of(world.missionCharacters.begin(),world.missionCharacters.end(),
        [&](const auto& entry) {
            return entry.second.vehicle==vehicle.scriptHandle&&entry.second.passengerSideCarjack;
        });
}
StorylandVehicleAccessState vehicleAccessState(const StorylandRuntimeVehicle& vehicle) {
    StorylandVehicleAccessState state;
    state.vcs=vehicle.nativeDefinition.vcsModelLayout;
    state.twoWheels=vehicle.nativeDefinition.twoWheels();
    state.boat=vehicle.nativeDefinition.domain==StorylandVehicleDomain::Water;
    state.helicopter=state.vcs&&vehicle.nativeDefinition.nativeType==4;
    state.hovercraft=vehicle.nativeDefinition.vcsHovercraft();
    state.jetSki=vehicle.nativeDefinition.vcsJetSki();
    state.wrecked=vehicle.wrecked||vehicle.health<=0;
    state.carried=vehicle.craneCarried;
    state.visible=vehicle.visible&&vehicle.clumpVisible;
    state.doorLock=vehicle.doorLock;
    const float roll=(vehicle.bodyRoll-vehicle.roadRoll)*radians;
    const float pitch=(vehicle.bodyPitch+vehicle.roadPitch)*radians;
    state.upZ=std::cos(roll)*std::cos(pitch);
    state.rightZ=-std::sin(roll)*std::cos(pitch);
    state.velocity={vehicle.speed,vehicle.lateralSpeed,vehicle.verticalSpeed};
    state.angularVelocity={vehicle.angularVelocity[0]+vehicle.springRollVelocity,
        vehicle.angularVelocity[1]+vehicle.springPitchVelocity,vehicle.yawVelocity*radians};
    if(state.hovercraft)state.angularVelocity[2]+=vehicle.angularVelocity[2];
    return state;
}
bool scriptVehicleAccessBlocked(const StorylandPlayTestRuntime& runtime,const StorylandRuntimeVehicle& vehicle,bool exiting) {
    if(vehicle.craneCarried)return true;
    if(!exiting&&!storylandRetailVehicleLocksOpen(vehicle.nativeDefinition.vcsModelLayout,vehicle.doorLock))return true;
    if(exiting&&vehicle.doorLock==4)return true;
    if(!runtime.missionEnvironment)return false;
    const auto& world=*runtime.missionEnvironment;
    const auto car=world.missionVehicles.find(vehicle.scriptHandle);
    if(car==world.missionVehicles.end())return false;
    if(!exiting&&!storylandRetailVehicleLocksOpen(vehicle.nativeDefinition.vcsModelLayout,car->second.doorLock))return true;
    return exiting?car->second.doorLock==4:
        !vehicle.nativeDefinition.vcsModelLayout&&storylandLcsVehicleEntryBlocked(world,vehicle.scriptHandle,-1,1);
}
bool displaceScriptVehicleDriver(StorylandPlayTestRuntime& runtime,StorylandRuntimeVehicle& vehicle,
    const StorylandNativeTrafficActor& actor,const StorylandNativePoint& door) {
    if (vehicle.coopControlled) {
        if (!runtime.coopDisplaceDriver || !runtime.coopDisplaceDriver(vehicle.coopIdentity, door)) return false;
        vehicle.coopControlled = false; vehicle.garageOwner = runtime.coopPlayerIndex;
        vehicle.hasDriver = vehicle.missionDriver = false;
        return true;
    }
    if(vehicle.hasDriver&&!vehicle.missionDriver&&runtime.crime.vcs&&
        vehicle.nativeDefinition.nativeType==0&&runtime.missionEnvironment) {
        const size_t slot=size_t(&vehicle-runtime.vehicles.data());
        const int32_t handle=storylandPromoteCarjackDriver(runtime,slot,actor,{door.x,door.y,door.z});
        const auto car=runtime.missionEnvironment->missionVehicles.find(vehicle.scriptHandle);
        if(handle>=0&&car!=runtime.missionEnvironment->missionVehicles.end()) {
            car->second.driver=handle;vehicle.hasDriver=false;vehicle.missionDriver=true;
        }
    }
    if(vehicle.hasDriver&&!vehicle.missionDriver) {vehicle.displacedDriverTime=4;vehicle.displacedDriverPosition=door;}
    if(vehicle.nativeDefinition.vcsModelLayout) {
        storylandDisplaceScriptDriver(runtime,vehicle,door);
        vehicle.hasDriver=false;vehicle.missionDriver=false;
        return true;
    }
    if(runtime.missionEnvironment) {
        auto& world=*runtime.missionEnvironment;
        const auto car=world.missionVehicles.find(vehicle.scriptHandle);
        if(car!=world.missionVehicles.end()) {
            const int32_t handle=car->second.driver;
            const auto found=world.missionCharacters.find(handle);
            if(found!=world.missionCharacters.end()) {
                auto& ped=found->second;ped.vehicle=-1;ped.passengerSeat=-1;
                ped.x=door.x;ped.y=door.y;ped.z=door.z;ped.walkingToTarget=false;
                ped.objective=0;ped.objectiveTarget=-1;ped.objectiveCompleted=false;ped.animationId=-1;
                world.characterVehicleTasks.erase(handle);world.pedAnimationRequests.erase(handle);
                world.pedWaitStateMilliseconds.erase(handle);world.lcsWaitAnimations.erase(handle);
            }
            car->second.driver=-1;
        }
    }
    vehicle.hasDriver=false;vehicle.missionDriver=false;
    return true;
}
void releaseVehicleDriverInput(StorylandRuntimeVehicle& vehicle) {
    vehicle.throttle=0;
    vehicle.steeringInput=0;
    vehicle.steeringInputValid=true;
    vehicle.bikeLeanInput=0;
}
void scriptPlayerLeavesVehicle(StorylandPlayTestRuntime& runtime,StorylandRuntimeVehicle& vehicle) {
    if(runtime.playerVehicleSeat<0)releaseVehicleDriverInput(vehicle);
    const bool boat=!vehicle.nativeDefinition.vcsModelLayout&&vehicle.nativeDefinition.nativeType==1;
    if(boat)vehicle.parked=true;
    if(!runtime.missionEnvironment)return;
    if(vehicle.nativeDefinition.vcsModelLayout) {
        runtime.missionEnvironment->manualAim.seatedActive=false;
        runtime.missionEnvironment->manualAim.initialized=false;
    }
    const auto car=runtime.missionEnvironment->missionVehicles.find(vehicle.scriptHandle);
    if(car!=runtime.missionEnvironment->missionVehicles.end()) {
        if(car->second.doorLock==5)car->second.doorLock=1;
        if(!vehicle.nativeDefinition.vcsModelLayout&&runtime.playerVehicleSeat<0) {
            car->second.scriptStatus=4;
            if(car->second.driver==1)car->second.driver=-1;
        } else if(runtime.playerVehicleSeat<0&&car->second.driver==0) {
            // 1FE72C..1FE764 / 3E8F50: only the departing driver abandons it.
            car->second.scriptStatus=4;
            car->second.driver=-1;
        }
        if(boat)car->second.drivingToTarget=false;
    }
    runtime.missionEnvironment->inVehicle=false;
    runtime.missionEnvironment->occupiedMissionVehicle=-1;
    runtime.missionEnvironment->playerPassengerSeat=-1;
}
void scriptPlayerEntersVehicle(StorylandPlayTestRuntime& runtime,StorylandRuntimeVehicle& vehicle) {
    if(runtime.playerVehicleSeat<0)vehicle.vcsTrafficRail.active=false;
    vehicle.engineOn=true;
    if(vehicle.alarmMilliseconds==UINT16_MAX)vehicle.alarmMilliseconds=15000;
    if(vehicle.nativeDefinition.vcsModelLayout) {
        if(runtime.missionEnvironment) {
            const auto car=runtime.missionEnvironment->missionVehicles.find(vehicle.scriptHandle);
            if(car!=runtime.missionEnvironment->missionVehicles.end()) {
                auto& state=car->second;
                state.engineOn=true;
                if(runtime.playerVehicleSeat<0) {
                    // 1FF2E4..1FF380: player driver takes STATUS_PLAYER. Only
                    // the old STATUS_SIMPLE body has its native speeds reset.
                    if(state.scriptStatus==2) {
                        vehicle.speed=0;vehicle.lateralSpeed=0;vehicle.verticalSpeed=0;
                        vehicle.angularVelocity={};vehicle.yawVelocity=0;
                        vehicle.springRollVelocity=0;vehicle.springPitchVelocity=0;
                        vehicle.suspensionVelocity=0;
                        state.speed=0;state.velocity={};state.angularVelocity={};
                        state.velocityChangedByScript=true;
                        state.angularVelocityChangedByScript=true;
                    }
                    state.scriptStatus=0;state.driver=0;
                }
            }
        }
        return;
    }
    if(!runtime.missionEnvironment)return;
    const auto car=runtime.missionEnvironment->missionVehicles.find(vehicle.scriptHandle);
    if(car!=runtime.missionEnvironment->missionVehicles.end()) {
        if(runtime.playerVehicleSeat<0)car->second.scriptStatus=0; // CPed::SetInCar: STATUS_PLAYER.
        car->second.engineOn=true;
    }
}
bool storylandPedSupportContinuity(bool grounded,bool launching,float verticalSpeed,
    float position,float floor) {
    if(!grounded||launching||!std::isfinite(verticalSpeed)||verticalSpeed>0||
        !std::isfinite(position)||!std::isfinite(floor))return false;
    const float clearance=position-floor;
    return clearance>=-.005f&&clearance<=.05f;
}
bool deepCharacterWater(const StorylandNativeWorld& world,const StorylandNativePoint& feet,float& height) {
    if(!storylandNativeWaterHeight(world,feet.x,feet.y,height))return false;
    if(world.paths.vcs&&!storylandVcsWaterDepthAllowsContact(feet.z,height))return false;
    uint8_t surface=0;
    const StorylandNativePoint top{feet.x,feet.y,height+.1f},bottom{feet.x,feet.y,height-3};
    const float hit=storylandTraceNativeGround(world,top,bottom,surface);
    const float floor=top.z+(bottom.z-top.z)*hit;
    return storylandWaterIsDeep(feet.z,height,hit<1,floor);
}
bool applyVehicleWater(StorylandRuntimeVehicle& vehicle,StorylandNativePoint& position,
    const StorylandNativeWorld& world,float seconds,float gravity,bool boat,float yaw,
    bool preserveHorizontal=false) {
    float height=0;
    const bool water=storylandNativeWaterHeight(world,position.x,position.y,height)&&
        (!world.paths.vcs||boat||storylandVcsWaterDepthAllowsContact(position.z,height));
    const auto previous=position;
    float unusedSpeed=0,unusedLateral=0;
    if(!storylandStepWaterVehicle(vehicle.water,position.z,vehicle.verticalSpeed,
        preserveHorizontal?unusedSpeed:vehicle.speed,preserveHorizontal?unusedLateral:vehicle.lateralSpeed,
        vehicle.bodyHeight,height,water,boat,vehicle.health<=0,gravity,seconds,
        !boat||!vehicle.nativeDefinition.vcsModelLayout))return false;
    const StorylandNativeBody body{vehicle.halfLength,vehicle.halfWidth,.2f,vehicle.bodyHeight,yaw};
    const auto movement=storylandMoveNativeBody(world,previous,position,body);
    position=movement.clear?movement.position:previous;
    if(movement.blocked)vehicle.verticalSpeed=0;
    vehicle.grounded=false;vehicle.burning=false;vehicle.engineFireSpent=true;
    if(vehicle.water.engineFlooded)vehicle.throttle=0;
    return true;
}
bool vehicleGroundHeight(const StorylandNativeWorld& world,float x,float y,float reference,float& height,
    StorylandNativePoint* normal=nullptr) {
    const StorylandNativePoint top{x,y,reference+0.85f};
    const StorylandNativePoint bottom{x,y,reference-5.0f};
    uint8_t surface=0;
    const float fraction=storylandTraceNativeGround(world,top,bottom,surface,normal);
    if(fraction>=1.0f)return false;
    height=top.z+(bottom.z-top.z)*fraction;
    return std::isfinite(height);
}
namespace VehicleContact=StorylandVehicleContact;

float vehicleVerticalVelocity(const StorylandRuntimeVehicle& vehicle) {
    return vehicle.nativeDefinition.hasNativeSuspension&&vehicle.grounded?
        vehicle.suspensionVelocity:vehicle.verticalSpeed;
}

float& vehicleVerticalVelocity(StorylandRuntimeVehicle& vehicle) {
    return vehicle.nativeDefinition.hasNativeSuspension&&vehicle.grounded?
        vehicle.suspensionVelocity:vehicle.verticalSpeed;
}

StorylandNativePoint vehicleAngularVelocity(const StorylandRuntimeVehicle& vehicle,float yaw) {
    const auto physical=VehicleContact::angularVelocityFromEulerRates(
        {vehicle.springPitchVelocity,vehicle.springRollVelocity,vehicle.yawVelocity*radians},
        yaw*radians,(vehicle.bodyPitch+vehicle.roadPitch)*radians);
    return {physical.x+vehicle.angularVelocity[0],physical.y+vehicle.angularVelocity[1],
        physical.z+vehicle.angularVelocity[2]};
}

void applyVehicleAngularDelta(StorylandRuntimeVehicle& vehicle,float yaw,StorylandNativePoint delta) {
    const auto rates=VehicleContact::eulerRatesFromAngularVelocity(delta,
        yaw*radians,(vehicle.bodyPitch+vehicle.roadPitch)*radians);
    vehicle.springPitchVelocity+=rates.x;
    vehicle.springRollVelocity+=rates.y;
    vehicle.yawVelocity+=rates.z/radians;
}

bool vcs039aTurnForce(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& actors,int32_t car,int32_t character,uint32_t random) {
    auto& world=*runtime.missionEnvironment;
    const auto found=world.missionVehicles.find(car);
    if(found==world.missionVehicles.end()||found->second.deleted||!world.vehicleWorldMatrix||
        (runtime.crime.vcs&&!world.characterWorldMatrix))return false;
    const int32_t slot=found->second.spawnedSlot;
    if(slot<0||size_t(slot)>=runtime.vehicles.size()||size_t(slot)>=actors.size())return false;
    auto& vehicle=runtime.vehicles[size_t(slot)];
    if(vehicle.scriptHandle!=car)return false;
    std::array<float,16> vehicleMatrix{},characterMatrix{};
    if(!world.vehicleWorldMatrix(car,vehicleMatrix))return false;
    if(runtime.crime.vcs) {
        if(!world.characterWorldMatrix(character,characterMatrix))return false;
    } else if(character==1) {
        characterMatrix[12]=world.x;characterMatrix[13]=world.y;characterMatrix[14]=world.z;
    } else {
        const auto pedestrian=world.missionCharacters.find(character);
        if(pedestrian==world.missionCharacters.end())return false;
        characterMatrix[12]=pedestrian->second.x;characterMatrix[13]=pedestrian->second.y;
        characterMatrix[14]=pedestrian->second.z;
    }
    const auto& actor=actors[size_t(slot)];
    const auto old=vehicleAngularVelocity(vehicle,actor.yaw);
    std::array<float,3> angular{old.x/StorylandNativePhysicsRate,
        old.y/StorylandNativePhysicsRate,old.z/StorylandNativePhysicsRate};
    const auto before=angular;
    const float turnMass=vehicle.nativeDefinition.turnMass*vehicle.turnMassMultiplier*(vehicle.heavy?5.0f:1.0f);
    const bool playerCharacter=runtime.crime.vcs?character==0:character==1;
    const float characterMass=playerCharacter?runtime.playerMass:StorylandNativePedMass;
    if(!storylandVcs039aTurnVelocity(vehicleMatrix,vehicle.nativeDefinition.centerOfMass,
        {characterMatrix[12],characterMatrix[13],characterMatrix[14]},characterMass,turnMass,random,angular))return false;
    applyVehicleAngularDelta(vehicle,actor.yaw,{(angular[0]-before[0])*StorylandNativePhysicsRate,
        (angular[1]-before[1])*StorylandNativePhysicsRate,(angular[2]-before[2])*StorylandNativePhysicsRate});
    return true;
}

VehicleContact::Body vehicleContactBody(const StorylandRuntimeVehicle& vehicle,
    const StorylandNativeTrafficActor& actor,StorylandNativePoint point,bool fixed=false) {
    if(fixed)return {};
    const auto& definition=vehicle.nativeDefinition;
    const auto frame=storylandVehicleMeshFrame(definition,{actor.position.x,actor.position.y,actor.position.z},
        actor.yaw,definition.previewMinimumZ,vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,
        vehicle.bodyPitch+vehicle.roadPitch,actor.water);
    const auto center=storylandVehicleTransformPoint(frame,{
        definition.previewRoot[12]+definition.centerOfMass[0],
        definition.previewRoot[13]+definition.centerOfMass[1]+
            StorylandBikeDynamics::centerOfMassShift(definition,vehicle.bikeLeanInput),
        definition.previewRoot[14]+definition.centerOfMass[2]});
    const float c=std::cos(actor.yaw*radians),s=std::sin(actor.yaw*radians);
    return {{c*vehicle.speed-s*vehicle.lateralSpeed,s*vehicle.speed+c*vehicle.lateralSpeed,
            vehicleVerticalVelocity(vehicle)},vehicleAngularVelocity(vehicle,actor.yaw),
        {point.x-center[0],point.y-center[1],point.z-center[2]},
        1/std::max(1.0f,vehicle.mass),definition.turnMass>0?
            1/std::max(1.0f,definition.turnMass*vehicle.turnMassMultiplier*(vehicle.heavy?5.0f:1.0f)):0};
}

void applyVehicleImpulse(StorylandRuntimeVehicle& vehicle,float yaw,const VehicleContact::Body& body,
    StorylandNativePoint linear,StorylandNativePoint turn) {
    const float c=std::cos(yaw*radians),s=std::sin(yaw*radians);
    vehicle.speed+=(c*linear.x+s*linear.y)*body.inverseMass;
    vehicle.lateralSpeed+=(c*linear.y-s*linear.x)*body.inverseMass;
    vehicleVerticalVelocity(vehicle)+=linear.z*body.inverseMass;
    const auto torque=VehicleContact::scale(VehicleContact::cross(body.arm,turn),body.inverseTurnMass);
    applyVehicleAngularDelta(vehicle,yaw,torque);
}

void recordVehicleContactImpulse(StorylandRuntimeVehicle& vehicle,float impulse,
    const StorylandNativePoint& normal) {
    const auto& definition=vehicle.nativeDefinition;
    if(definition.twoWheels()&&vehicle.canBeDamaged&&
        impulse>20.0f*StorylandNativePhysicsRate)vehicle.bikeStandingOverride=false;
    const bool riderVehicle=definition.vcsJetSki()||
        (definition.vcsModelLayout&&definition.twoWheels());
    if(!riderVehicle||!std::isfinite(impulse)||impulse<=0||
        !std::isfinite(normal.x)||!std::isfinite(normal.y)||!std::isfinite(normal.z))return;
    const float nativeImpulse=impulse/StorylandNativePhysicsRate;
    if(nativeImpulse>vehicle.vcsJetSkiContactImpulse) {
        vehicle.vcsJetSkiContactImpulse=nativeImpulse;
        vehicle.vcsJetSkiContactNormal={normal.x,normal.y,normal.z};
    }
}

VehicleContact::Impact applyVehicleWorldImpulse(StorylandRuntimeVehicle& vehicle,
    const StorylandNativeTrafficActor& actor,const StorylandNativeContact& contact) {
    if(!contact.valid)return {};
    const auto body=vehicleContactBody(vehicle,actor,contact.point);
    const auto hit=VehicleContact::impact(body,{},contact.normal);
    recordVehicleContactImpulse(vehicle,hit.impulse,contact.normal);
    const auto impulse=VehicleContact::scale(contact.normal,hit.impulse);
    applyVehicleImpulse(vehicle,actor.yaw,body,impulse,impulse);
    return hit;
}

void resolveVehicleBodyMovement(StorylandRuntimeVehicle& vehicle, StorylandNativeTrafficActor& actor,
    const StorylandNativePoint& previous, const StorylandNativeMoveResult& movement) {
    actor.position=movement.clear?movement.position:previous;
    if(!movement.contact.valid)return;
    vehicle.bodyContact=true;
    vehicle.pendingBodyImpact=true;
    applyVehicleWorldImpulse(vehicle,actor,movement.contact);
}

StorylandNativeBody vehicleBody(const StorylandRuntimeVehicle& vehicle,float yaw);

bool usesNativeVehicleSuspension(const StorylandVehicleDefinition& definition) {
    return definition.domain==StorylandVehicleDomain::Road&&definition.hasNativeHandling&&
        definition.hasNativeSuspension&&definition.suspensionForce>0&&
        definition.physicalWheels().size()==4;
}

bool vehicleAttachedToRope(const StorylandPlayTestRuntime& runtime,const StorylandRuntimeVehicle& vehicle) {
    if(!runtime.missionEnvironment||vehicle.scriptHandle<0)return false;
    const auto& ropes=runtime.missionEnvironment->vehicleRopes;
    return std::any_of(ropes.begin(),ropes.end(),
        [&](const auto& entry){return entry.second.attachedVehicle==vehicle.scriptHandle;});
}

void updateVehicleRoadReference(StorylandRuntimeVehicle& vehicle,float pitch,float roll) {
    if(usesNativeVehicleSuspension(vehicle.nativeDefinition)) {
        if(!vehicle.grounded)return;
        vehicle.bodyPitch+=vehicle.roadPitch-pitch;
        vehicle.bodyRoll+=roll-vehicle.roadRoll;
    }
    vehicle.roadPitch=pitch;
    vehicle.roadRoll=roll;
}

void updateVehicleWheelContacts(StorylandRuntimeVehicle& vehicle,StorylandNativeTrafficActor& actor,
    const StorylandNativeWorld& world,float seconds,bool playerControlled=false,
    float gravity=StorylandGravityPerSecond,bool riderBalancing=false) {
    const auto& definition=vehicle.nativeDefinition;
    vehicle.contactsValid=false;
    if(!std::isfinite(seconds)||seconds<0)return;
    if(seconds>0&&!playerControlled)
        vehicle.bikeLeanInput=StorylandBikeDynamics::leanInput(vehicle.bikeLeanInput,0,seconds);
    const auto& wheels=definition.physicalWheels();
    if(!definition.hasNativeSuspension||wheels.size()!=4||actor.water) {
        vehicle.wheelContactHistoryValid={};
        return;
    }
    vehicle.contactsValid=true;
    const bool physical=usesNativeVehicleSuspension(definition);
    bool waterStep=false;
    if(physical&&seconds>0&&!vehicle.positionFrozen) {
        const float velocity=vehicleVerticalVelocity(vehicle);
        vehicle.suspensionVelocity=velocity;
        vehicle.verticalSpeed=velocity;
        waterStep=applyVehicleWater(vehicle,actor.position,world,seconds,gravity,false,actor.yaw);
        if(waterStep)vehicle.suspensionVelocity=vehicle.verticalSpeed;
    }
    vehicle.suspensionAnchorZ=actor.position.z;vehicle.suspensionAnchorValid=true;
    const unsigned steps=std::max(1u,unsigned(std::ceil(seconds/.02f)));
    const float dt=seconds/steps,mass=std::max(1.0f,definition.mass*vehicle.massMultiplier*(vehicle.heavy?3.0f:1.0f)),inertia=std::max(1.0f,definition.turnMass*vehicle.turnMassMultiplier*(vehicle.heavy?5.0f:1.0f));
    for(unsigned step=0;step<steps;++step) {
        const auto frame=storylandVehicleMeshFrame(definition,{actor.position.x,actor.position.y,actor.position.z},
            actor.yaw,definition.previewMinimumZ,vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,
            vehicle.bodyPitch+vehicle.roadPitch);
        const auto nativeFrame=storylandVehicleMatrixProduct(frame,definition.previewRoot);
        const StorylandNativePoint forward{nativeFrame[4],nativeFrame[5],nativeFrame[6]};
        const StorylandNativePoint right{nativeFrame[0],nativeFrame[1],nativeFrame[2]};
        const StorylandNativePoint up{nativeFrame[8],nativeFrame[9],nativeFrame[10]};
        if(definition.twoWheels()&&vehicle.bikeStandingOverride&&
            (playerControlled||riderBalancing||vehicle.hasDriver||vehicle.missionDriver||
             std::fabs(right.z)>.35f||std::fabs(forward.z)>.5f))
            vehicle.bikeStandingOverride=false;
        std::array<StorylandNativePoint,4> normals{},axes{},dampingAxes{},arms{};
        std::array<float,4> speeds{};
        const auto center=storylandVehicleTransformPoint(frame,{definition.previewRoot[12]+definition.centerOfMass[0],
            definition.previewRoot[13]+definition.centerOfMass[1]+
                StorylandBikeDynamics::centerOfMassShift(definition,vehicle.bikeLeanInput),
            definition.previewRoot[14]+definition.centerOfMass[2]});
        bool contact=false;
        const float headingCosine=std::cos(actor.yaw*radians);
        const float headingSine=std::sin(actor.yaw*radians);
        const auto pointSpeed=[&](size_t i) {
            const auto& r=arms[i];
            const auto omega=vehicleAngularVelocity(vehicle,actor.yaw);
            return StorylandNativePoint{
                headingCosine*vehicle.speed-headingSine*vehicle.lateralSpeed+omega.y*r.z-omega.z*r.y,
                headingSine*vehicle.speed+headingCosine*vehicle.lateralSpeed+omega.z*r.x-omega.x*r.z,
                vehicle.suspensionVelocity+omega.x*r.y-omega.y*r.x};
        };
        const auto projection=[](StorylandNativePoint a,StorylandNativePoint b) {return a.x*b.x+a.y*b.y+a.z*b.z;};
        const auto apply=[&](size_t i,StorylandNativePoint direction,float impulse) {
            const auto& r=arms[i];
            const StorylandNativePoint force{direction.x*impulse,direction.y*impulse,direction.z*impulse};
            const StorylandNativePoint torque{r.y*force.z-r.z*force.y,r.z*force.x-r.x*force.z,r.x*force.y-r.y*force.x};
            vehicle.speed+=(headingCosine*force.x+headingSine*force.y)/mass;
            vehicle.lateralSpeed+=(headingCosine*force.y-headingSine*force.x)/mass;
            vehicle.suspensionVelocity+=force.z/mass;
            applyVehicleAngularDelta(vehicle,actor.yaw,VehicleContact::scale(torque,1/inertia));
        };
        for(size_t i=0;i<wheels.size()&&i<4;++i) {
            const auto& wheel=wheels[i];auto top=wheel.previewPivot,bottom=wheel.previewPivot;
            top[2]=wheel.upperZ+definition.previewRoot[14];bottom[2]=wheel.lowerZ+definition.previewRoot[14];
            const auto from=storylandVehicleTransformPoint(frame,top),to=storylandVehicleTransformPoint(frame,bottom);
            axes[i]=VehicleContact::unit({to[0]-from[0],to[1]-from[1],to[2]-from[2]});
            uint8_t surface=0;const float fraction=storylandTraceNativeVehicleGround(world,{from[0],from[1],from[2]},
                {to[0],to[1],to[2]},surface,&normals[i]);
            vehicle.wheelLineFractions[i]=fraction;
            vehicle.wheelContacts[i]=fraction<1;contact|=vehicle.wheelContacts[i];
            const bool retainNativeContact=definition.motorcycle()||
                (definition.vcsModelLayout&&definition.nativeType==0);
            if(vehicle.wheelContacts[i]||!retainNativeContact) {
                vehicle.wheelContactPoints[i]={from[0]+(to[0]-from[0])*fraction,
                    from[1]+(to[1]-from[1])*fraction,from[2]+(to[2]-from[2])*fraction};
                vehicle.wheelContactNormals[i]=normals[i];vehicle.wheelSurfaces[i]=surface;
                vehicle.wheelContactHistoryValid[i]=vehicle.wheelContacts[i]&&
                    std::isfinite(normals[i].x)&&std::isfinite(normals[i].y)&&
                    std::isfinite(normals[i].z)&&VehicleContact::dot(normals[i],normals[i])>0.25f;
            } else if(vehicle.wheelContactHistoryValid[i]) {
                const StorylandNativePoint rayStart{from[0],from[1],from[2]};
                const StorylandNativePoint rayEnd{to[0],to[1],to[2]};
                const float speed=std::hypot(vehicle.speed,vehicle.lateralSpeed,
                    vehicleVerticalVelocity(vehicle));
                if(!StorylandVcsVehicleContactTimers::retainedContactNearby(
                    vehicle.wheelContactPoints[i],rayStart,rayEnd,speed))
                    vehicle.wheelContactHistoryValid[i]=false;
            }
            vehicle.wheelSpringRatios[i]=storylandVehicleSpringRatio(wheel,fraction);
            dampingAxes[i]=storylandVehicleDampingDirection(axes[i],normals[i],
                vehicle.wheelSpringRatios[i]);
            arms[i]={from[0]+(to[0]-from[0])*fraction-center[0],from[1]+(to[1]-from[1])*fraction-center[1],
                from[2]+(to[2]-from[2])*fraction-center[2]};
            vehicle.wheelSpringImpulses[i]=vehicle.wheelDampingImpulses[i]=0;
        }
        if(definition.motorcycle()||(definition.vcsModelLayout&&definition.nativeType==0))
            StorylandVcsVehicleContactTimers::advanceAll(vehicle.wheelContactTimers,
                vehicle.wheelLineFractions,dt*StorylandVehicleStepsPerSecond);
        if(definition.motorcycle()||(definition.vcsModelLayout&&definition.nativeType==0))
            for(size_t i=0;i<4;++i)
                if(vehicle.wheelContactTimers[i]<=0&&!vehicle.wheelContacts[i])
                    vehicle.wheelContactHistoryValid[i]=false;
        if(vehicle.positionFrozen||seconds<=0)continue;
        const auto averageNormal=StorylandBikeDynamics::averageContactNormal(
            vehicle.wheelContactNormals,vehicle.wheelContactTimers,up);
        // Player entry clears the ambient driver flags; the seated rider still balances the bike.
        const bool balancedBike=playerControlled||riderBalancing||vehicle.hasDriver||vehicle.missionDriver;
        if(physical&&!waterStep) {
            vehicle.grounded=contact;
            vehicle.verticalSpeed=vehicle.suspensionVelocity;
        }
        if(definition.motorcycle()&&definition.hasNativeBikeDynamics&&
            (balancedBike||vehicle.bikeBeingPickedUp||vehicle.bikeStandingOverride)) {
            const bool front=vehicle.wheelContactTimers[0]>0||vehicle.wheelContactTimers[1]>0;
            const bool rear=vehicle.wheelContactTimers[2]>0||vehicle.wheelContactTimers[3]>0;
            const auto angularDamping=StorylandBikeDynamics::angularDampingDelta(definition,
                vehicleAngularVelocity(vehicle,actor.yaw),forward,right,front,rear,dt);
            applyVehicleAngularDelta(vehicle,actor.yaw,angularDamping);
            if(playerControlled&&!vehicle.bodyContact) {
                const auto balance=StorylandBikeDynamics::balance(definition,forward.z,
                    std::hypot(vehicle.speed,vehicle.lateralSpeed,vehicleVerticalVelocity(vehicle)),
                    vehicle.steeringAngle*radians,
                    vehicle.bikeLeanInput,vehicle.throttle,vehicle.braking,vehicle.handbrake,front,rear,dt);
                const auto lateBalance=StorylandBikeDynamics::balance(definition,forward.z,
                    std::hypot(vehicle.speed,vehicle.lateralSpeed,vehicleVerticalVelocity(vehicle)),
                    vehicle.steeringAngle*radians,0,vehicle.throttle,
                    vehicle.braking,vehicle.handbrake,front,rear,dt);
                applyVehicleAngularDelta(vehicle,actor.yaw,VehicleContact::add(
                    VehicleContact::scale(right,balance.pitch-lateBalance.pitch),
                    VehicleContact::scale(up,balance.yaw-lateBalance.yaw)));
            }
        }
        for(size_t visual=0;visual<definition.wheels.size()&&visual<4;++visual) {
            const size_t line=definition.twoWheels()?
                StorylandVcsVehicleContactTimers::bikeSelectedContactLine(vehicle.wheelLineFractions,visual):visual;
            const float ratio=vehicle.wheelSpringRatios[line];
            const float nativeTarget=storylandVehicleWheelTarget(wheels[line],ratio);
            const float target=nativeTarget+definition.wheels[visual].radius-wheels[line].radius;
            vehicle.wheelPositions[visual]=storylandVehicleWheelFollow(vehicle.wheelPositions[visual],
                target,dt);
        }
        if(!physical)continue;
        if(waterStep) {
            vehicle.bodyPitch=std::remainder(vehicle.bodyPitch+vehicle.springPitchVelocity*dt/radians,360.0f);
            vehicle.bodyRoll=std::remainder(vehicle.bodyRoll+vehicle.springRollVelocity*dt/radians,360.0f);
            continue;
        }
        vehicle.suspensionVelocity-=gravity*dt;
        for(size_t i=0;i<wheels.size()&&i<4;++i)if(vehicle.wheelContacts[i]) {
            const auto& wheel=wheels[i];
            vehicle.wheelSpringImpulses[i]=storylandNativeSpringImpulse(mass,definition.suspensionForce,
                vehicle.wheelSpringRatios[i],wheel.front?definition.suspensionBias:1-definition.suspensionBias,dt);
            const auto direction=storylandVehicleSpringDirection(axes[i],normals[i],
                definition.twoWheels());
            apply(i,direction,vehicle.wheelSpringImpulses[i]);
        }
        // Native damping snapshots contact speeds after all spring impulses.
        for(size_t i=0;i<wheels.size()&&i<4;++i)
            speeds[i]=projection(pointSpeed(i),dampingAxes[i]);
        for(size_t i=0;i<wheels.size()&&i<4;++i)if(vehicle.wheelContacts[i]) {
            const auto& r=arms[i];const auto& n=dampingAxes[i];
            const float crossX=r.y*n.z-r.z*n.y,crossY=r.z*n.x-r.x*n.z,crossZ=r.x*n.y-r.y*n.x;
            const float effective=1/(1/mass+(crossX*crossX+crossY*crossY+crossZ*crossZ)/inertia);
            const float impulse=storylandNativeSpringDamping(definition.suspensionDamping,vehicle.wheelSpringImpulses[i],
                speeds[i],projection(pointSpeed(i),n),effective,dt);
            vehicle.wheelDampingImpulses[i]=impulse;apply(i,n,impulse);
        }
        if(definition.motorcycle()&&definition.hasNativeBikeDynamics) {
            StorylandBikeDynamics::VisualLeanInput lean;
            lean.current=vehicle.bikeVisualLean;
            lean.tyreAcceleration={vehicle.bikeTyreAccelerationWorld[0],
                vehicle.bikeTyreAccelerationWorld[1],vehicle.bikeTyreAccelerationWorld[2]};
            lean.forward=forward;lean.right=right;lean.averageNormal=averageNormal;
            lean.steeringRadians=vehicle.bikeRawSteeringAngle*radians;
            lean.hasContact=std::any_of(vehicle.wheelContactTimers.begin(),vehicle.wheelContactTimers.end(),
                [](float timer){return timer>0;});
            lean.balancedByRider=balancedBike;lean.beingPickedUp=vehicle.bikeBeingPickedUp;
            lean.standing=vehicle.bikeStandingOverride;lean.frontTyreBurst=vehicle.tyreStatus[0]==1;
            vehicle.bikeVisualLean=StorylandBikeDynamics::visualLean(definition,lean,dt);
            const auto upright=StorylandBikeDynamics::uprightAngularDelta(right,forward,averageNormal,
                balancedBike,vehicle.bikeBeingPickedUp,vehicle.bikeStandingOverride,dt);
            applyVehicleAngularDelta(vehicle,actor.yaw,upright);
            if(playerControlled&&!vehicle.bodyContact&&
                (balancedBike||vehicle.bikeBeingPickedUp||vehicle.bikeStandingOverride)) {
                const bool front=vehicle.wheelContactTimers[0]>0||vehicle.wheelContactTimers[1]>0;
                const bool rear=vehicle.wheelContactTimers[2]>0||vehicle.wheelContactTimers[3]>0;
                const auto lateBalance=StorylandBikeDynamics::balance(definition,forward.z,
                    std::hypot(vehicle.speed,vehicle.lateralSpeed,vehicleVerticalVelocity(vehicle)),
                    vehicle.steeringAngle*radians,0,vehicle.throttle,
                    vehicle.braking,vehicle.handbrake,front,rear,dt);
                applyVehicleAngularDelta(vehicle,actor.yaw,VehicleContact::add(
                    VehicleContact::scale(right,lateBalance.pitch),VehicleContact::scale(up,lateBalance.yaw)));
            }
        }
        vehicle.bodyPitch=std::remainder(vehicle.bodyPitch+vehicle.springPitchVelocity*dt/radians,360.0f);
        vehicle.bodyRoll=std::remainder(vehicle.bodyRoll+vehicle.springRollVelocity*dt/radians,360.0f);
        // The chassis has one vertical velocity in contact and in flight.
        // Road-anchor rebasing below changes its representation, not its motion.
        vehicle.verticalSpeed=vehicle.suspensionVelocity;
        const auto previous=actor.position;
        auto next=previous;
        next.z+=vehicle.suspensionVelocity*dt;
        if(vehicle.collisionEnabled) {
            const auto movement=storylandMoveNativeBody(world,previous,next,vehicleBody(vehicle,actor.yaw));
            resolveVehicleBodyMovement(vehicle,actor,previous,movement);
            vehicle.suspensionVelocity=vehicleVerticalVelocity(vehicle);
        } else actor.position=next;
        vehicle.verticalSpeed=vehicle.suspensionVelocity;
        if(contact)storylandUpdateVehicleWheelContactPose(vehicle,actor);
        // Keep the physical orientation through wheelies, stoppies and rolls.
        vehicle.bodyPitch=std::remainder(vehicle.bodyPitch,360.0f);
        vehicle.bodyRoll=std::remainder(vehicle.bodyRoll,360.0f);
    }
    if(physical&&seconds>0&&!vehicle.positionFrozen&&!waterStep) {
        const auto springImpulses=vehicle.wheelSpringImpulses;
        const auto dampingImpulses=vehicle.wheelDampingImpulses;
        // Publish contacts at the integrated pose without a second force step.
        updateVehicleWheelContacts(vehicle,actor,world,0,playerControlled,gravity,riderBalancing);
        vehicle.wheelSpringImpulses=springImpulses;
        vehicle.wheelDampingImpulses=dampingImpulses;
        vehicle.grounded=std::any_of(vehicle.wheelContacts.begin(),vehicle.wheelContacts.end(),
            [](bool contact){return contact;});
        if(vehicle.grounded) {
            const auto support=storylandMeasureWheelGround(definition,
                {actor.position.x,actor.position.y,actor.position.z},actor.yaw,vehicle.suspension,
                vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
                vehicle.wheelPositions,
                [&](float x,float y,float centerZ,float radius,float& height,std::array<float,3>& planeNormal) {
                    const StorylandNativePoint top{x,y,centerZ+radius+.65f};
                    const StorylandNativePoint bottom{x,y,centerZ-radius-1.65f};
                    uint8_t surface=0;StorylandNativePoint normal{};
                    const float fraction=storylandTraceNativeVehicleGround(world,top,bottom,surface,&normal);
                    if(!(fraction>=0&&fraction<1))return false;
                    height=top.z+(bottom.z-top.z)*fraction;
                    planeNormal={normal.x,normal.y,normal.z};
                    return true;
                },vehicle.wheelRotation,vehicle.rearWheelRotation,vehicle.steeringAngle,
                vehicle.tyreStatus,vehicle.handbrake,vehicle.bikeVisualLean);
            if(support.valid) {
                vehicle.suspension+=actor.position.z-support.targetPositionZ;
                actor.position.z=support.targetPositionZ;
            }
        }
        vehicle.verticalSpeed=vehicle.suspensionVelocity;
        vehicle.suspensionAnchorZ=actor.position.z;
    }
}

void applyVehicleTyreForces(StorylandRuntimeVehicle& vehicle,const StorylandNativeTrafficActor& actor,
    float thrust,float brake,float rain,float dt,bool burnout=false,bool playerControlled=false,
    unsigned controllerMode=0) {
    if(!(dt>0)||!std::isfinite(dt))return;
    const auto& definition=vehicle.nativeDefinition;
    const bool bike=definition.twoWheels();
    const bool vcs=definition.vcsModelLayout;
    const bool wheelSupport=vehicle.grounded||((definition.motorcycle()||(vcs&&definition.nativeType==0))&&vehicle.contactsValid&&
        definition.hasNativeSuspension&&definition.physicalWheels().size()==4);
    const size_t count=std::min(definition.wheels.size(),bike?size_t(2):size_t(4));
    const unsigned steps=std::max(1u,unsigned(std::ceil(dt/.02f)));
    const float nativeStep=dt/steps*StorylandVehicleStepsPerSecond;
    const float priorLateral=vehicle.lateralSpeed;
    const float headingCosine=std::cos(actor.yaw*radians),headingSine=std::sin(actor.yaw*radians);
    const StorylandNativePoint priorVelocity{
        headingCosine*vehicle.speed-headingSine*vehicle.lateralSpeed,
        headingSine*vehicle.speed+headingCosine*vehicle.lateralSpeed,vehicleVerticalVelocity(vehicle)};
    const auto bikeFrame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
        {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
        vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch),
        definition.previewRoot);
    const auto bikeForward=VehicleContact::unit({bikeFrame[4],bikeFrame[5],bikeFrame[6]});
    const auto bikeRight=VehicleContact::unit({bikeFrame[0],bikeFrame[1],bikeFrame[2]});
    const float centerY=definition.centerOfMass[1]+
        StorylandBikeDynamics::centerOfMassShift(definition,vehicle.bikeLeanInput);
    const StorylandNativePoint bikeCenterOffset{
        bikeFrame[0]*definition.centerOfMass[0]+bikeFrame[4]*centerY+bikeFrame[8]*definition.centerOfMass[2],
        bikeFrame[1]*definition.centerOfMass[0]+bikeFrame[5]*centerY+bikeFrame[9]*definition.centerOfMass[2],
        bikeFrame[2]*definition.centerOfMass[0]+bikeFrame[6]*centerY+bikeFrame[10]*definition.centerOfMass[2]};
    const bool rearBikeContact=StorylandVcsVehicleContactTimers::bikeRearWheelContact(vehicle.wheelContactTimers);
    if(bike)vehicle.bikeRawSteeringAngle=vehicle.steeringAngle;
    if(bike&&playerControlled) {
        const size_t rearLine=StorylandVcsVehicleContactTimers::bikeSelectedContactLine(vehicle.wheelLineFractions,1);
        const unsigned surface=VehicleContact::adhesionGroup(vehicle.wheelSurfaces[rearLine]);
        const bool frontContact=definition.motorcycle()?
            (vehicle.wheelContactTimers[0]>0||vehicle.wheelContactTimers[1]>0):
            (vehicle.wheelContacts[0]||vehicle.wheelContacts[1]);
        vehicle.steeringAngle=StorylandBikeDynamics::steeringAngle(definition,vehicle.steeringAngle,
            vehicle.speed,vehicle.bikeVisualLean,vehicle.tractionMultiplier,
            definition.tyreAdhesion[VehicleContact::adhesionGroup(1)],surface,
            frontContact);
    }
    const float brakeBias=std::clamp(definition.brakeBias,0.0f,1.0f);
    const float tractionBias=std::clamp(definition.tractionBias,0.0f,1.0f);
    vehicle.tyreForcesApplied=true;
    vehicle.wheelTyreSlip={};
    std::array<size_t,4> contactLines{};
    unsigned groundedTyres=0;
    for(size_t wheel=0;wheel<count;++wheel) {
        size_t line=bike?wheel*2:wheel;
        if(bike&&definition.motorcycle())
            line=StorylandVcsVehicleContactTimers::bikeSelectedContactLine(vehicle.wheelLineFractions,wheel);
        else if(bike&&vehicle.wheelLineFractions[line+1]<vehicle.wheelLineFractions[line])++line;
        contactLines[wheel]=line;
        if(wheelSupport&&vehicle.wheelContacts[line])++groundedTyres;
    }
    for(unsigned step=0;step<steps;++step) {
        std::array<VehicleContact::Body,4> bodies{};
        if(!bike)for(size_t wheel=0;wheel<count;++wheel)
            bodies[wheel]=vehicleContactBody(vehicle,actor,vehicle.wheelContactPoints[contactLines[wheel]]);
        for(size_t order=0;order<count;++order) {
            const size_t wheel=bike&&StorylandBikeDynamics::rearTyreFirst(definition)?count-1-order:order;
            const size_t line=contactLines[wheel];
            const bool retainedBikeContact=definition.motorcycle()&&
                StorylandVcsVehicleContactTimers::bikeTyreContactAvailable(
                    vehicle.wheelContactTimers,vehicle.wheelContactHistoryValid,wheel,line)&&
                StorylandVcsVehicleContactTimers::retainedContactNearVehicle(
                    vehicle.wheelContactPoints[line],actor.position,vehicle.halfLength,
                    std::hypot(vehicle.speed,vehicle.lateralSpeed,vehicleVerticalVelocity(vehicle)));
            const bool retainedCarContact=vcs&&definition.nativeType==0&&
                StorylandVcsVehicleContactTimers::automobileTyreContactAvailable(
                    vehicle.wheelContactTimers,vehicle.wheelContactHistoryValid,line)&&
                StorylandVcsVehicleContactTimers::retainedContactNearVehicle(
                    vehicle.wheelContactPoints[line],actor.position,vehicle.halfLength,
                    std::hypot(vehicle.speed,vehicle.lateralSpeed,vehicleVerticalVelocity(vehicle)));
            if(!wheelSupport||(!vehicle.wheelContacts[line]&&!retainedBikeContact&&!retainedCarContact)) {
                vehicle.wheelTyreStates[wheel]=VehicleContact::TyreState::Rolling;
                continue;
            }
            const bool front=definition.wheels[wheel].front;
            const bool driven=bike?!front:definition.driveType=='4'||(front==(definition.driveType=='F'));
            const auto normal=vehicle.wheelContactNormals[line];
            const float wheelSteer=StorylandVcsRetailQuirks::wheelSteerDegrees(
                vcs,definition.handlingFlags,front,vehicle.steeringAngle,vehicle.handbrake);
            const float heading=(actor.yaw+wheelSteer)*radians;
            auto forward=StorylandNativePoint{std::cos(heading),std::sin(heading),0};
            if(bike)forward=VehicleContact::subtract(
                VehicleContact::scale(bikeForward,std::cos(wheelSteer*radians)),
                VehicleContact::scale(bikeRight,std::sin(wheelSteer*radians)));
            forward=VehicleContact::unit(VehicleContact::subtract(forward,
                VehicleContact::scale(normal,VehicleContact::dot(forward,normal))));
            auto left=VehicleContact::unit(VehicleContact::cross(normal,forward));
            if(!bike) {
                const auto basis=VehicleContact::wheelBasis(bikeForward,normal,wheelSteer*radians);
                if(!basis.valid)continue;
                forward=basis.forward;
                left=basis.left;
            }
            auto body=bike?vehicleContactBody(vehicle,actor,vehicle.wheelContactPoints[line]):bodies[wheel];
            if(bike) {
                std::array<float,3> localPoint{};
                if(!storylandNativeBikeTyreContactPoint(definition,vehicle.wheelSpringRatios,wheel,localPoint))continue;
                body.arm={
                    bikeFrame[0]*localPoint[0]+bikeFrame[4]*localPoint[1]+bikeFrame[8]*localPoint[2],
                    bikeFrame[1]*localPoint[0]+bikeFrame[5]*localPoint[1]+bikeFrame[9]*localPoint[2],
                    bikeFrame[2]*localPoint[0]+bikeFrame[6]*localPoint[1]+bikeFrame[10]*localPoint[2]};
            }
            const auto velocity=body.pointVelocity();
            const float forwardSpeed=VehicleContact::dot(velocity,forward);
            const float lateralSpeed=bike?StorylandBikeDynamics::tyreLateralSpeed(
                velocity,left,front,rearBikeContact):VehicleContact::dot(velocity,left);
            const auto group=VehicleContact::adhesionGroup(vehicle.wheelSurfaces[line]);
            const float bias=2*(front?tractionBias:1-tractionBias);
            const float burst=vehicle.tyreStatus[wheel]==1?.6f:1.0f;
            const float wet=VehicleContact::wetGrip(vehicle.wheelSurfaces[line],rain);
            float adhesion=.004f*.25f*StorylandVehicleStepsPerSecond*nativeStep*
                definition.traction*vehicle.tractionMultiplier*definition.tyreAdhesion[group]*bias*burst*wet;
            if(!bike&&playerControlled) {
                const float suspensionBias=2*(front?definition.suspensionBias:1-definition.suspensionBias);
                adhesion*=VehicleContact::retailTyreLoad(vehicle.wheelSpringRatios[line],
                    definition.suspensionForce,suspensionBias);
            }
            if(vcs&&!bike&&group>=4) {
                if(definition.handlingFlags&0x200000u)adhesion*=2.5f;
                else if(definition.handlingFlags&0x100000u)adhesion*=1.15f;
            }
            const float wheelThrust=!driven?0:(vcs||!bike)?storylandVehicleNativeTyreThrust(thrust,steps):
                storylandVehicleDriveThrustPerWheel(definition,thrust,steps);
            const float coast=vcs?VehicleContact::retailCoastingBrake(definition.mass,bike):
                bike?.54f/(definition.mass+200):(definition.mass<500?.09f:.9f)/definition.mass;
            const bool handbrake=(playerControlled?vehicle.handbrake:
                StorylandVcsRetailQuirks::handbrakeEnabled(
                    vcs,definition.handlingFlags,vehicle.handbrake))&&!front&&
                (!vcs||VehicleContact::retailHandbrakeActive(vehicle.speed,bike,controllerMode));
            float wheelBrake=handbrake?20000.0f*((vcs||bike)?2*(1-brakeBias):1):
                brake>0?brake/steps*2*(front?brakeBias:1-brakeBias):0;
            if(vcs&&burnout&&!front&&driven&&!handbrake) {
                wheelBrake=0;
                adhesion=0;
            }
            const bool braking=wheelBrake>0;
            const bool driving=!braking&&wheelThrust!=0;
            if(!braking&&!driving&&(vcs?std::fabs(vehicle.throttle)<.01f:wheelThrust==0))
                wheelBrake=coast*StorylandVehicleStepsPerSecond*nativeStep;
            VehicleContact::RetailTyreParameters parameters;
            parameters.automobile=!bike;
            parameters.front=front;
            parameters.playerControlled=playerControlled;
            parameters.throttle=vehicle.throttle;
            parameters.modelId=int32_t(definition.modelId);
            const bool vcsAutomobile=vcs&&definition.nativeType==0;
            const unsigned forceWheelCount=StorylandVcsVehicleContactTimers::tyreForceWheelCount(
                vehicle.wheelContactTimers,!bike?groundedTyres:unsigned(count),vcsAutomobile);
            const auto force=(vcs||!bike)?VehicleContact::retailTyre(forwardSpeed,
                lateralSpeed,wheelThrust,wheelBrake,adhesion,
                vehicle.handlingTractionLoss,forceWheelCount,vehicle.wheelTyreStates[wheel],parameters):
                VehicleContact::tyre(forwardSpeed,lateralSpeed,
                wheelThrust,wheelBrake,adhesion,vehicle.handlingTractionLoss,
                forceWheelCount,vehicle.wheelTyreStates[wheel]);
            vehicle.wheelTyreStates[wheel]=force.state;
            vehicle.wheelTyreSlip[wheel]=std::max(vehicle.wheelTyreSlip[wheel],force.slip);
            const auto delta=VehicleContact::add(VehicleContact::scale(forward,force.forward),
                VehicleContact::scale(left,force.lateral));
            const auto turnDelta=!bike?VehicleContact::retailTyreTurnDelta(force,forward,left,
                definition.suspensionAntiDive,braking,driving):delta;
            const auto direction=VehicleContact::unit(turnDelta);
            const float turnMass=body.inverseContactMass(direction);
            if(!(body.inverseMass>0)||!(turnMass>0))continue;
            const auto impulse=VehicleContact::scale(delta,1/body.inverseMass);
            const auto turnImpulse=VehicleContact::scale(turnDelta,1/turnMass);
            if(bike) {
                const auto linearVelocity=StorylandBikeDynamics::tyreLinearVelocity(body.velocity,delta);
                const auto limitedImpulse=VehicleContact::scale(
                    VehicleContact::subtract(linearVelocity,body.velocity),1/body.inverseMass);
                applyVehicleImpulse(vehicle,actor.yaw,body,limitedImpulse,{});
                const auto angular=StorylandBikeDynamics::tyreAngularVelocity(body.angularVelocity,body.arm,turnImpulse,
                    bikeForward,bikeRight,body.inverseTurnMass,front,braking,vehicle.throttle<0,
                    bikeCenterOffset);
                applyVehicleAngularDelta(vehicle,actor.yaw,VehicleContact::subtract(angular,body.angularVelocity));
            } else applyVehicleImpulse(vehicle,actor.yaw,body,impulse,turnImpulse);
        }
    }
    vehicle.bikeTyreAcceleration=(vehicle.lateralSpeed-priorLateral)/dt;
    vehicle.bikeTyreAccelerationWorld={
        (headingCosine*vehicle.speed-headingSine*vehicle.lateralSpeed-priorVelocity.x)/dt,
        (headingSine*vehicle.speed+headingCosine*vehicle.lateralSpeed-priorVelocity.y)/dt,
        (vehicleVerticalVelocity(vehicle)-priorVelocity.z)/dt};
}

void applyVehicleGravity(StorylandRuntimeVehicle& vehicle, StorylandNativePoint& position,
    const StorylandNativeWorld& world, float seconds, float gravity,float yaw) {
    // Native suspension integrates gravity, contact forces and flight together.
    if(usesNativeVehicleSuspension(vehicle.nativeDefinition))return;
    if(applyVehicleWater(vehicle,position,world,seconds,gravity,false,yaw))return;
    float speed=vehicleVerticalVelocity(vehicle)-gravity*seconds;
    float floor=0;
    bool contact=false;
    if(vehicle.nativeDefinition.hasNativeSuspension &&
       !vehicle.nativeDefinition.wheels.empty()) {
        const auto support=storylandMeasureWheelGround(vehicle.nativeDefinition,
            {position.x,position.y,position.z},yaw,vehicle.suspension,
            vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
            vehicle.wheelPositions,
            [&](float x,float y,float centerZ,float radius,float& height,std::array<float,3>& planeNormal) {
                const StorylandNativePoint top{x,y,centerZ+radius+0.65f};
                const StorylandNativePoint bottom{x,y,centerZ-radius-1.65f};
                uint8_t surface=0;StorylandNativePoint normal{};
                const float fraction=storylandTraceNativeGround(world,top,bottom,surface,&normal);
                if(!(fraction>=0.0f&&fraction<1.0f))return false;
                height=top.z+(bottom.z-top.z)*fraction;
                planeNormal={normal.x,normal.y,normal.z};
                return true;
            },vehicle.wheelRotation,vehicle.rearWheelRotation,vehicle.steeringAngle,
            vehicle.tyreStatus,vehicle.handbrake,vehicle.bikeVisualLean);
        contact=support.valid;
        floor=support.targetPositionZ;
    } else {
        contact=vehicleGroundHeight(world,position.x,position.y,position.z,floor);
    }
    const float nextHeight=position.z+speed*seconds;
    // Support is measured under actual tyre contact points, never at the
    // centre of the chassis. Do not mount a wall or bridge roof as a step.
    const bool canContactSupport=!vehicle.nativeDefinition.vcsModelLayout ||
        vehicle.grounded || speed<=0 || (contact&&floor>=nextHeight);
    if(canContactSupport && contact && floor-position.z<=0.85f &&
        (nextHeight<=floor+0.12f || (vehicle.grounded && floor>=position.z-0.30f))) {
        position.z=floor;
        if(vehicle.nativeDefinition.hasNativeSuspension&&!vehicle.grounded)
            vehicle.suspensionVelocity=vehicle.verticalSpeed;
        vehicle.verticalSpeed=0.0f;
        vehicle.grounded=true;
        return;
    }
    position.z=nextHeight;
    vehicle.verticalSpeed=speed;
    vehicle.grounded=false;
}
float vehicleRandom(StorylandPlayTestRuntime& runtime) {
    runtime.combatRandom=runtime.combatRandom*1664525u+1013904223u;
    return float(runtime.combatRandom>>8)/16777216.0f;
}
void vehicleParticle(StorylandPlayTestRuntime& runtime,StorylandParticleKind kind,StorylandNativePoint position,
    StorylandNativePoint velocity,float size,float lifetime,int32_t preferredNativeType=-1,
    bool nativeDefaults=false,std::array<uint8_t,4> color={}) {
    if(runtime.particles.size()>=2048)return;
    const int32_t nativeType=preferredNativeType>=0?preferredNativeType:storylandRetailParticleType(kind);
    if(nativeType>=0&&runtime.missionEnvironment&&runtime.missionEnvironment->createScriptParticles) {
        StorylandMissionEnvironment::ScriptParticle request;
        request.type=nativeType;
        request.position={position.x,position.y,position.z};
        request.velocity={velocity.x/50.0f,velocity.y/50.0f,velocity.z/50.0f};
        request.size=nativeDefaults?0.0f:size;
        request.color=color;
        std::string error;
        const size_t before=runtime.particles.size();
        if(runtime.missionEnvironment->createScriptParticles({request},error)&&runtime.particles.size()>before) {
            auto& created=runtime.particles.back();
            // Surface wakes stay on top of the water in the host simulation.
            if(runtime.crime.vcs&&(nativeType==76||nativeType==77))created.nativeGravity=0;
            if(!nativeDefaults&&lifetime>0)created.remaining=created.lifetime=lifetime;
            return;
        }
    }
    const float fallbackLifetime=lifetime>0?lifetime:kind==StorylandParticleKind::Flame?.65f:1.5f;
    const float fallbackSize=size>0?size:kind==StorylandParticleKind::Exhaust?.03f:.35f;
    runtime.particles.push_back({position,velocity,kind,fallbackLifetime,fallbackLifetime,fallbackSize});
}
void vehicleImpact(StorylandPlayTestRuntime& runtime,const StorylandNativeTrafficActor& actor,
    StorylandRuntimeVehicle& vehicle,float speed,const StorylandNativePoint* collisionPoint=nullptr) {
    vehicle.bodyContact=true;vehicle.pendingBodyImpact=true;
    if(speed<1.0f||vehicle.impactCooldown>0)return;
    vehicle.impactCooldown=0.2f;
    const float forwardX=std::cos(actor.yaw*radians),forwardY=std::sin(actor.yaw*radians);
    const float end=vehicle.speed>=0?vehicle.halfLength:-vehicle.halfLength;
    const StorylandNativePoint contact=collisionPoint?*collisionPoint:StorylandNativePoint{actor.position.x+forwardX*end,
        actor.position.y+forwardY*end,actor.position.z+0.45f};
    const int count=std::clamp(int(speed*2.0f),4,30);
    for(int index=0;index<count;++index) {
        const float angle=vehicleRandom(runtime)*6.28318530718f;
        const float impulse=1.0f+vehicleRandom(runtime)*std::min(8.0f,speed);
        vehicleParticle(runtime,actor.water?StorylandParticleKind::Water:
            index%4?StorylandParticleKind::Spark:StorylandParticleKind::Debris,contact,
            {-forwardX*speed*0.15f+std::cos(angle)*impulse,-forwardY*speed*0.15f+std::sin(angle)*impulse,
                1.0f+vehicleRandom(runtime)*3},index%4?0.045f:0.10f,0.25f+vehicleRandom(runtime)*0.7f);
    }
    if(!actor.water && speed>5)vehicleParticle(runtime,StorylandParticleKind::Smoke,contact,{0,0,0.8f},0.45f,1.2f,61);
    StorylandPlayTestSoundEvent event(StorylandPlayTestSound::VehicleImpact);
    event.position=contact;
    event.positioned=true;
    event.volume=std::clamp(speed/12.0f,0.1f,1.0f);
    runtime.soundEvents.push_back(event);

}
void vehicleRoadEffects(StorylandPlayTestRuntime& runtime,const StorylandNativeTrafficActor& actor,
    StorylandRuntimeVehicle& vehicle,const StorylandNativeWorld* world,float dt) {
    const auto& definition=vehicle.nativeDefinition;
    vehicle.skidRoadContact=false;
    const bool valid=world&&!actor.water&&vehicle.water.immersion<.1f&&vehicle.grounded&&!vehicle.craneCarried&&
        !vehicle.positionFrozen&&vehicle.collisionEnabled&&definition.domain==StorylandVehicleDomain::Road;
    const bool physicalSlip=vehicle.tyreForcesApplied&&!vehicle.vcsTrafficRail.active;
    const auto tyreSmoke=[&](size_t wheel) {
        if(!valid||!physicalSlip)return false;
        if(definition.twoWheels())return wheel==1&&
            vehicle.wheelTyreStates[wheel]!=VehicleContact::TyreState::Rolling;
        return storylandVcsAutomobileRubberSmoke(vehicle.wheelTyreStates,wheel,vehicle.speed);
    };
    if(physicalSlip)vehicle.tyreSlip=valid?*std::max_element(vehicle.wheelTyreSlip.begin(),vehicle.wheelTyreSlip.end()):0;
    else if(definition.hasNativeSuspension)vehicle.tyreSlip=0;
    else vehicle.tyreSlip=storylandVehicleTyreSlip(vehicle.speed,vehicle.lateralSpeed,vehicle.steeringAngle,
        vehicle.yawVelocity,storylandVehicleWheelbase(definition,vehicle.halfLength*1.6f),
        vehicle.handbrake,vehicle.braking,valid);
    const size_t count=definition.twoWheels()?2:4;
    bool smokingTyre=false;
    for(size_t wheel=0;wheel<count;++wheel)smokingTyre=smokingTyre||tyreSmoke(wheel);
    if((vehicle.tyreSlip<=.03f&&!smokingTyre)||dt<=0) {
        vehicle.skidContactValid={};vehicle.skidSmokeTime=0;return;
    }
    const float c=std::cos(actor.yaw*radians),s=std::sin(actor.yaw*radians);
    const auto frame=storylandVehicleMeshFrame(definition,
        {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
        vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch);
    vehicle.skidSmokeTime+=dt*50.0f;
    const unsigned smokeTicks=std::min(5u,unsigned(vehicle.skidSmokeTime+.00001f));
    vehicle.skidSmokeTime=std::max(0.0f,vehicle.skidSmokeTime-float(smokeTicks));
    for(size_t index=0;index<count;++index) {
        const bool rubberSmoke=tyreSmoke(index);
        const float slip=std::max(physicalSlip?vehicle.wheelTyreSlip[index]:vehicle.tyreSlip,
            rubberSmoke?.25f:0.0f);
        if(slip<=.03f) {vehicle.skidContactValid[index]=false;continue;}
        const bool front=definition.wheels.size()==count?definition.wheels[index].front:index%2==0;
        const size_t line=definition.twoWheels()?index*2:index;
        if((vehicle.contactsValid&&!vehicle.wheelContacts[line]&&
            (!definition.twoWheels()||!vehicle.wheelContacts[line+1]))||
            (vehicle.handbrake&&front&&std::fabs(vehicle.lateralSpeed)<1&&
                !(definition.vcsModelLayout&&definition.nativeType==0&&physicalSlip))) {
            vehicle.skidContactValid[index]=false;continue;
        }
        StorylandNativePoint point;
        if(definition.wheels.size()==count) {
            const auto local=definition.wheels[index].previewPivot;
            const auto at=storylandVehicleTransformPoint(frame,local);
            point={at[0],at[1],at[2]};
        } else {
            const float along=(front?1:-1)*vehicle.halfLength*.7f;
            const float left=definition.twoWheels()?0:(index<2?1:-1)*vehicle.halfWidth*.8f;
            point={actor.position.x+c*along-s*left,actor.position.y+s*along+c*left,actor.position.z};
        }
        uint8_t surface=0;
        StorylandNativePoint normal{0,0,1};
        bool tyreContact=false;
        if(definition.hasNativeSuspension&&vehicle.contactsValid) {
            // Use the actual suspension collision points for smoke, sparks and
            // skid marks. A motorcycle has two longitudinal probes per tyre.
            const size_t physicalLines=definition.twoWheels()?2u:1u;
            unsigned hits=0;
            point={};normal={};
            for(size_t contact=0;contact<physicalLines;++contact) {
                const size_t physical=line+contact;
                if(!vehicle.wheelContacts[physical])continue;
                const auto& at=vehicle.wheelContactPoints[physical];
                const auto& n=vehicle.wheelContactNormals[physical];
                point.x+=at.x;point.y+=at.y;point.z+=at.z;
                normal.x+=n.x;normal.y+=n.y;normal.z+=n.z;
                surface=vehicle.wheelSurfaces[physical];
                ++hits;
            }
            if(hits) {
                const float scale=1.0f/float(hits);
                point.x*=scale;point.y*=scale;point.z*=scale;
                normal.x*=scale;normal.y*=scale;normal.z*=scale;
                if(normal.z<.35f) {
                    vehicle.skidContactValid[index]=false;
                    continue;
                }
                tyreContact=true;
            }
        }
        if(!tyreContact) {
            const StorylandNativePoint top{point.x,point.y,point.z+.45f};
            const StorylandNativePoint bottom{point.x,point.y,point.z-1.1f};
            const float hit=storylandTraceNativeGround(*world,top,bottom,surface,&normal);
            if(hit>=1||normal.z<.35f) {vehicle.skidContactValid[index]=false;continue;}
            point.z=top.z+(bottom.z-top.z)*hit;
        }
        if(definition.twoWheels()&&index==1&&definition.collision&&
            std::isfinite(vehicle.bikeVisualLean)&&std::isfinite(definition.collision->minimum.z)) {
            const auto nativeFrame=storylandVehicleMatrixProduct(frame,definition.previewRoot);
            const float offset=std::sin(vehicle.bikeVisualLean)*.8f*definition.collision->minimum.z;
            point.x+=offset*nativeFrame[0];point.y+=offset*nativeFrame[1];point.z+=offset*nativeFrame[2];
        }
        point.z+=.012f;
        const bool road=storylandTyreLeavesRoadMark(surface);
        const bool leaveMark=road&&StorylandVcsRetailQuirks::skidmarksEnabled(
            definition.vcsModelLayout,definition.hasVcsModelInfoFlags,definition.vcsModelInfoFlags);
        // Tyre squeal remains independent of the visual effect gate.
        vehicle.skidRoadContact=vehicle.skidRoadContact||road;
        if(leaveMark&&vehicle.skidContactValid[index]&&storylandSkidCanJoin(vehicle.skidContacts[index],point,dt,
            std::hypot(vehicle.speed,vehicle.lateralSpeed))) {
            if(runtime.roadSkids.size()>=4096)runtime.roadSkids.erase(runtime.roadSkids.begin(),runtime.roadSkids.begin()+128);
            StorylandRoadSkid skid;
            skid.start=vehicle.skidContacts[index];skid.end=point;skid.normal=normal;
            skid.width=definition.twoWheels()?.13f:std::clamp(vehicle.halfWidth*.22f,.15f,.35f);
            skid.opacity=slip;skid.area=runtime.visibleArea;
            runtime.roadSkids.push_back(skid);
        }
        if(!vehicle.skidContactValid[index]||std::hypot(point.x-vehicle.skidContacts[index].x,
            point.y-vehicle.skidContacts[index].y)>=.06f)vehicle.skidContacts[index]=point;
        vehicle.skidContactValid[index]=leaveMark;
        const bool tyreParticles=StorylandVcsRetailQuirks::automobileTyreParticlesEnabled(
            definition.vcsModelLayout,definition.twoWheels(),
            definition.hasVcsModelInfoFlags,definition.vcsModelInfoFlags);
        if(smokeTicks&&surface!=19&&tyreParticles) {
            auto smokePoint=point;smokePoint.z+=.25f-.012f;
            const float wetRoads=runtime.missionEnvironment?runtime.missionEnvironment->weatherState.rain:0;
            const unsigned adhesion=VehicleContact::adhesionGroup(surface);
            const bool burningRubber=rubberSmoke&&vehicle.wheelTyreStates[index]==VehicleContact::TyreState::Spinning&&
                (definition.twoWheels()?(adhesion==1||adhesion==2):
                    std::fabs(vehicle.speed)*3.6f<90&&surface!=2&&surface!=4&&
                    surface!=18&&surface!=33&&(surface==3||wetRoads<=.01f));
            for(unsigned tick=0;tick<smokeTicks;++tick) {
                if(rubberSmoke) {
                    if(definition.twoWheels())
                        vehicleParticle(runtime,StorylandParticleKind::Smoke,smokePoint,{},.2f,0,66);
                    if(burningRubber) {
                        vehicleParticle(runtime,StorylandParticleKind::Smoke,smokePoint,{},0,0,67,true);
                        vehicleParticle(runtime,StorylandParticleKind::Smoke,smokePoint,{0,0,2.5f},0,0,67,true);
                    }
                    if(!definition.twoWheels())
                        vehicleParticle(runtime,StorylandParticleKind::Smoke,smokePoint,{},0,0,66,true);
                }
                if(surface==2&&std::fabs(vehicle.speed)>1.3888889f)
                    vehicleParticle(runtime,StorylandParticleKind::WheelDirt,point,
                        {-c*vehicle.speed*.05f,-s*vehicle.speed*.05f,1.5f+vehicleRandom(runtime)*1.5f},
                        .02f+vehicleRandom(runtime)*.08f,0,3,false,{8,24,8,255});
            }
        }
    }
}

StorylandNativePoint vehicleDamageParticlePosition(const StorylandPlayTestRuntime& runtime,
    const StorylandNativeTrafficActor& actor,const StorylandRuntimeVehicle& vehicle,
    const StorylandVehicleMatrix& frame,bool fire,bool occupied) {
    const auto& definition=vehicle.nativeDefinition;
    if(!definition.hasNativeDummies)return {actor.position.x+frame[4]*vehicle.halfLength*.6f,
        actor.position.y+frame[5]*vehicle.halfLength*.6f,actor.position.z+.9f};
    std::array<float,3> local;
    if(definition.twoWheels()) {
        local=definition.seatPosition(fire?1:0);
        local[2]-=fire?.3f:.4f;
        if(fire&&occupied&&runtime.vehicleEffectsFirstPerson&&runtime.vehicleEffectsLookingForward)
            local={0,1.2f,-.4f};
    } else {
        local=definition.dummyPositions[0];
        const bool openBonnet=vehicle.doorDamage[0]>=2;
        local[0]-=openBonnet?.5f:.1f;
        local[1]-=openBonnet?.3f:.1f;
        if(!fire&&!definition.vcsModelLayout&&definition.modelId==155u) {
            local={.3f,-1.5f,-.1f};
        } else if(!fire&&definition.collision) {
            if(definition.handlingFlags&(definition.vcsModelLayout?0x02000000u:0x00020000u)) {
                if(!definition.vcsModelLayout||definition.modelId!=173) {
                    local[0]=.4f*definition.collision->maximum.x;
                    local[1]=.2f*definition.collision->minimum.y;
                }
                local[2]=.3f*definition.collision->maximum.z;
            } else local[2]+=.4f*(definition.collision->maximum.z-local[2])*vehicle.speed*.02f;
        }
        if(fire&&occupied&&runtime.vehicleEffectsFirstPerson&&runtime.vehicleEffectsLookingForward) {
            const bool firetruck=definition.modelId==(definition.vcsModelLayout?194u:138u);
            local[1]+=firetruck?3.0f:1.2f;local[2]-=firetruck?.2f:.8f;
        }
    }
    const auto position=storylandVehicleTransformPoint(frame,local);
    return {position[0],position[1],position[2]+(definition.twoWheels()?0:.15f)};
}

void vehicleDamageSmoke(StorylandPlayTestRuntime& runtime,const StorylandNativeTrafficActor& actor,
    const StorylandRuntimeVehicle& vehicle,const StorylandVehicleMatrix& frame,
    const StorylandNativePoint& velocity,bool occupied,uint32_t phase) {
    if(vehicle.health>=650||vehicle.health<=0||vehicle.wrecked||
        (occupied&&runtime.vehicleEffectsFirstPerson&&runtime.vehicleEffectsLookingForward)||
        (!occupied&&(phase&1u)))return;
    const auto& definition=vehicle.nativeDefinition;
    const bool bike=definition.twoWheels(),vcs=definition.vcsModelLayout;
    const auto position=vehicleDamageParticlePosition(runtime,actor,vehicle,frame,false,occupied);
    const auto scaled=[&](float amount) {
        return StorylandNativePoint{velocity.x*amount,velocity.y*amount,velocity.z*amount};
    };
    const auto emit=[&](int32_t type,StorylandNativePoint direction,unsigned count=1,
        float size=0,float lifetime=0,StorylandNativePoint offset={}) {
        for(unsigned index=0;index<count;++index)
            vehicleParticle(runtime,type==33?StorylandParticleKind::Steam:StorylandParticleKind::EngineSmoke,
                {position.x+offset.x,position.y+offset.y,position.z+offset.z},direction,size,lifetime,type);
    };
    const auto smokeFrame=bike?storylandVehicleMatrixProduct(storylandVehicleRenderFrame(definition,
        {},0,definition.previewMinimumZ,0,0,0,false,&frame,vehicle.bikeVisualLean),definition.previewRoot):frame;
    const auto rightOffset=[&](float amount) {
        return StorylandNativePoint{smokeFrame[0]*amount,smokeFrame[1]*amount,smokeFrame[2]*amount};
    };
    const auto withRight=[&](StorylandNativePoint direction,float amount) {
        const auto right=rightOffset(amount);
        return StorylandNativePoint{direction.x+right.x,direction.y+right.y,direction.z+right.z};
    };
    const bool electric=definition.engineType=='E'&&!bike;
    const auto electricSparks=[&](unsigned mask,unsigned minimum) {
        auto direction=scaled(.85f);
        const float movement=std::hypot(velocity.x,velocity.y,velocity.z)*.02f;
        direction=withRight(direction,vehicleRandom(runtime)*2.0f*(1-2*movement));
        direction.z+=.05f;
        const unsigned count=(uint32_t(vehicleRandom(runtime)*65536.0f)&mask)+minimum;
        for(unsigned index=0;index<count;++index)
            vehicleParticle(runtime,StorylandParticleKind::Spark,position,direction,0,0,1,true);
    };
    if(electric&&vehicle.health<320&&vehicle.health>1) {
        electricSparks(7,2);
        if((phase&7u)==0)emit(56,scaled(.8f),1,.1f,1);
        return;
    }
    if(electric&&vehicle.health<460) {
        electricSparks(3,1);
        if((phase&15u)==0)emit(55,scaled(.8f),1,.1f,1);
        return;
    }
    if(vehicle.health<250)return;
    if(vehicle.health<320) {
        emit(56,bike?withRight(scaled(.1f),1.0f):scaled(.68f),vcs&&!bike?2u:1u);
    } else if(vehicle.health<390) {
        if(bike) {
            if((phase&1u)==0)emit(33,withRight(scaled(.5f),2.5f));
            emit(55,withRight(scaled(.15f),2.0f));
        } else {
            emit(33,scaled(.6375f),vcs?2u:1u);
            emit(55,scaled(.7225f),vcs?2u:1u);
        }
    } else if(vehicle.health<460&&!bike) {
        if((phase&1u)==0)emit(33,scaled(.765f),vcs?2u:1u);
        if(vcs)emit(33,scaled(.765f),2);
    } else if(bike&&vehicle.health<460) {
        if((phase&1u)==0)emit(33,scaled(.5f),1,0,0,rightOffset(.06f));
    } else {
        const unsigned cycle=phase&255u;
        if(cycle<10||(cycle>25&&cycle<70)||(cycle>100&&cycle<160)||
            (cycle>175&&cycle<200)||cycle>235)return;
        if(electric) {
            electricSparks(2,2);
            if((phase&15u)==0)emit(55,scaled(.8f),1,.1f,1);
            return;
        }
        if(runtime.vehicleEffectsLookingForward&&(phase&1u))return;
        auto direction=scaled(bike?(runtime.vehicleEffectsLookingForward?.8f:.5f):.85f);
        if(!runtime.vehicleEffectsLookingForward)
            direction.z+=bike?2.5f:
                2.5f*std::max(1.0f-1.6f*std::hypot(velocity.x,velocity.y,velocity.z)*.02f,0.0f);
        if(bike)direction=withRight(direction,runtime.vehicleEffectsLookingForward?3.5f:4.0f);
        emit(33,direction,vcs&&!bike&&!runtime.vehicleEffectsLookingForward?2u:1u,.1f,1.0f);
    }
}

void vehicleEngineFireParticles(StorylandPlayTestRuntime& runtime,const StorylandNativeTrafficActor& actor,
    const StorylandRuntimeVehicle& vehicle,const StorylandVehicleMatrix& frame,bool occupied) {
    if(!vehicle.burning||vehicle.wrecked||!vehicle.canBeDamaged||vehicle.health>=250||vehicle.health<=0)return;
    const bool bike=vehicle.nativeDefinition.twoWheels();
    const auto position=vehicleDamageParticlePosition(runtime,actor,vehicle,frame,true,occupied);
    StorylandNativePoint direction{0,0,.5625f+vehicleRandom(runtime)*3.9375f};
    if(bike&&!(occupied&&runtime.vehicleEffectsFirstPerson&&runtime.vehicleEffectsLookingForward)) {
        const float right=1.0125f+vehicleRandom(runtime)*3.4875f;
        const float backward=1.0125f+vehicleRandom(runtime)*7.9875f;
        direction={frame[0]*right-frame[4]*backward,frame[1]*right-frame[5]*backward,
            .1125f+vehicleRandom(runtime)*.7875f};
    }
    vehicleParticle(runtime,StorylandParticleKind::Flame,position,direction,bike?.9f:.63f,0,27);
    vehicleParticle(runtime,StorylandParticleKind::EngineSmoke,position,{},.5f,0,56);
    const StorylandNativePoint smoke{position.x+(vehicleRandom(runtime)-.5f)*1.125f,
        position.y+(vehicleRandom(runtime)-.5f)*1.125f,
        position.z+.5625f+vehicleRandom(runtime)*1.6875f};
    vehicleParticle(runtime,StorylandParticleKind::Smoke,smoke,{},0,0,57,true);
}

void vehicleExhaustParticles(StorylandPlayTestRuntime& runtime,const StorylandNativeTrafficActor& actor,
    const StorylandRuntimeVehicle& vehicle,const StorylandVehicleMatrix& frame,
    const StorylandNativePoint& velocity) {
    const auto& definition=vehicle.nativeDefinition;
    if(!vehicle.engineOn||vehicle.health<=0||vehicle.wrecked||vehicle.speed*3.6f>=130||
        (definition.vcsModelLayout?!StorylandVcsRetailQuirks::exhaustEnabled(true,definition.modelFlags):
            (definition.handlingFlags&0x100000u)!=0))return;
    const bool twin=definition.vcsModelLayout?
        StorylandVcsRetailQuirks::doubleExhaust(true,definition.modelFlags):(definition.handlingFlags&0x100u)!=0;
    std::array<StorylandNativePoint,2> nozzles;
    if(definition.hasNativeDummies) {
        auto local=definition.dummyPositions[definition.vcsModelLayout?6u:4u];
        if(local[0]==0&&local[1]==0&&local[2]==0)return;
        const auto first=storylandVehicleTransformPoint(frame,local);
        nozzles[0]={first[0],first[1],first[2]};
        local[0]=-local[0];
        const auto second=storylandVehicleTransformPoint(frame,local);
        nozzles[1]={second[0],second[1],second[2]};
    } else {
        nozzles[0]={actor.position.x-frame[4]*vehicle.halfLength,
            actor.position.y-frame[5]*vehicle.halfLength,actor.position.z+.2f};
        nozzles[1]=nozzles[0];
    }
    StorylandNativePoint direction{velocity.x,velocity.y,0};
    if(vehicle.speed*3.6f<10) {
        const float steer=vehicle.steeringAngle*radians;
        const float movement=-1.5f-vehicleRandom(runtime)*1.5f;
        direction={(frame[4]*std::cos(steer)-frame[0]*std::sin(steer))*movement,
            (frame[5]*std::cos(steer)-frame[1]*std::sin(steer))*movement,0};
    }
    if((1.0f+vehicleRandom(runtime)*2.0f)*(vehicle.throttle+1.1f)<=2.0f)return;
    vehicleParticle(runtime,StorylandParticleKind::Exhaust,nozzles[0],direction,0,0,65,true);
    if(twin)vehicleParticle(runtime,StorylandParticleKind::Exhaust,nozzles[1],direction,0,0,65,true);
    if(!definition.twoWheels()&&vehicle.throttle>.5f&&vehicle.transmission.gear<3) {
        const bool second=vehicleRandom(runtime)>=.5f;
        if(!second||twin)
            vehicleParticle(runtime,StorylandParticleKind::Exhaust,nozzles[second?1:0],direction,0,0,65,true);
    }
}

void vehicleRunningEffectsImpl(StorylandPlayTestRuntime& runtime,const StorylandNativeTrafficActor& actor,
    StorylandRuntimeVehicle& vehicle,float dt,const StorylandNativeWorld* world=nullptr) {
    vehicleRoadEffects(runtime,actor,vehicle,world,dt);
    vehicle.impactCooldown=std::max(0.0f,vehicle.impactCooldown-dt);
    const bool occupied = runtime.occupiedVehicle >= 0 &&
        size_t(runtime.occupiedVehicle) < runtime.vehicles.size() &&
        &runtime.vehicles[size_t(runtime.occupiedVehicle)] == &vehicle;
    const float up = std::cos((vehicle.bodyRoll - vehicle.roadRoll) * radians) *
        std::cos((vehicle.bodyPitch + vehicle.roadPitch) * radians);
    if (!occupied && !actor.water && up < 0 && !vehicle.upsideDownDamageDisabled &&
        vehicle.canBeDamaged && !vehicle.onlyDamagedByPlayer && vehicle.health > 0 && !(vehicle.damageProofs & 8u)) {
        // 0x136E04: four health units per native 50-Hz step while overturned.
        vehicle.health = std::max(0.0f, vehicle.health - 200.0f * dt);
    }
    const float speed=std::fabs(vehicle.speed);
    bool scriptFire=false;
    if(runtime.missionEnvironment&&vehicle.scriptHandle>=0)
        for(auto& entry:runtime.missionEnvironment->scriptFires)
            if(entry.second.active&&entry.second.attachedKind==2&&entry.second.attachedHandle==vehicle.scriptHandle) {
                if(vehicle.water.immersion>.1f&&!actor.water)entry.second.active=false;
                else scriptFire=true;
            }
    const bool nativeBoat=actor.water&&vehicle.nativeDefinition.vcsModelLayout;
    if(nativeBoat&&world&&dt>0) {
        const auto& definition=vehicle.nativeDefinition;
        const auto frame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
            {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
            vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,true),
            definition.previewRoot);
        const float c=std::cos(actor.yaw*radians),s=std::sin(actor.yaw*radians);
        const auto angular=vehicleAngularVelocity(vehicle,actor.yaw);
        const float nativeStep=runtime.missionEnvironment?
            runtime.missionEnvironment->nativeScriptTimeStep:dt*StorylandNativePhysicsRate;
        const auto contact=storylandEvaluateVcsBoatWater(definition,frame,
            std::array<float,3>{(c*vehicle.speed-s*vehicle.lateralSpeed)/StorylandNativePhysicsRate,
                (s*vehicle.speed+c*vehicle.lateralSpeed)/StorylandNativePhysicsRate,
                vehicleVerticalVelocity(vehicle)/StorylandNativePhysicsRate},
            std::array<float,3>{angular.x/StorylandNativePhysicsRate,
                angular.y/StorylandNativePhysicsRate,angular.z/StorylandNativePhysicsRate},
            vehicle.mass,nativeStep,[&](float x,float y) {
                StorylandBoatWaterSample sample;
                sample.available=storylandNativeWaterHeight(*world,x,y,sample.height);
                return sample;
            });
        vehicle.water.buoyant=contact.strongContact;
        if(occupied)storylandStepWaterCharacter(runtime.playerWater,0,0,false,true,false,false,dt,true);
        if(contact.drowningDriver) {
            if(occupied&&runtime.playerVehicleSeat<0) {
                runtime.inflictDamage(nativeStep,0,46,false);
            } else if(runtime.missionEnvironment&&vehicle.scriptHandle>=0) {
                auto& environment=*runtime.missionEnvironment;
                const auto scripted=environment.missionVehicles.find(vehicle.scriptHandle);
                if(scripted!=environment.missionVehicles.end()&&scripted->second.driver>0) {
                    const auto found=environment.missionCharacters.find(scripted->second.driver);
                    if(found!=environment.missionCharacters.end()) {
                        const auto& driver=found->second;
                        if(driver.vehicle==vehicle.scriptHandle&&!driver.attached&&driver.drownsInWater) {
                            const StorylandMissionCharacterDamage damage{
                                found->first,driver.model,nativeStep,false,0,46};
                            storylandApplyMissionCharacterDamage(runtime,damage,false);
                        }
                    }
                }
            }
        }
    } else if(world&&runtime.crime.vcs&&!actor.water&&
        !vehicle.nativeDefinition.vcsHovercraft()&&runtime.missionEnvironment) {
        namespace H=StorylandVcsHovercraft;
        const auto& definition=vehicle.nativeDefinition;
        if(definition.collision&&definition.hasNativeBuoyancyCoefficient&&vehicle.mass>0&&dt>0) {
            const auto matrix=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
                {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
                vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,false),
                definition.previewRoot);
            const H::Frame frame{{matrix[0],matrix[1],matrix[2]},
                {matrix[4],matrix[5],matrix[6]},{matrix[8],matrix[9],matrix[10]}};
            const auto& collision=*definition.collision;
            H::WaterResult water;
            const float nativeStep=dt*StorylandNativePhysicsRate;
            const float c=std::cos(actor.yaw*radians),sine=std::sin(actor.yaw*radians);
            const bool valid=H::waterForces({collision.minimum.x,collision.minimum.y,collision.minimum.z},
                {collision.maximum.x,collision.maximum.y,collision.maximum.z},frame,
                {matrix[12],matrix[13],matrix[14]},
                {(c*vehicle.speed-sine*vehicle.lateralSpeed)/StorylandNativePhysicsRate,
                    (sine*vehicle.speed+c*vehicle.lateralSpeed)/StorylandNativePhysicsRate,
                    vehicleVerticalVelocity(vehicle)/StorylandNativePhysicsRate},vehicle.mass,
                definition.nativeBuoyancyCoefficient,nativeStep,[&](float x,float y,float z) {
                    H::WaterSample sample;
                    sample.available=storylandNativeWaterHeight(*world,x,y,sample.height)&&
                        storylandVcsWaterDepthAllowsContact(z,sample.height);
                    return sample;
                },water);
            const float denominator=vehicle.mass*(std::max(nativeStep,.5f)*.00800000037997961f);
            const auto occupants=storylandVcsVehicleWaterOccupants(definition.twoWheels(),
                valid&&water.hasBuoyancy?water.force[2]/denominator:0,vehicle.wheelLineFractions);
            vehicle.water.engineFlooded=occupants.drowning;
            if(occupants.releaseRiders) {
                StorylandBikeFallDecision release;
                release.eject=true;release.animation=139;release.inheritVelocity=.2f;release.nativeWaterFall=true;
                storylandEjectVcsMissionBikeRiders(runtime,actor,vehicle,release);
            }
            auto& environment=*runtime.missionEnvironment;
            if(occupied&&!occupants.releaseRiders) {
                storylandStepWaterCharacter(runtime.playerWater,0,0,false,true,false,false,dt,true);
                if(occupants.drowning&&runtime.vehicleAction!=StorylandVehicleAction::Exit)
                    runtime.inflictDamage(nativeStep,0,46,false);
            }
            if(occupants.drowning&&!occupants.releaseRiders&&vehicle.scriptHandle>=0)
                for(auto& entry:environment.missionCharacters) {
                    const auto& character=entry.second;
                    if(character.vehicle!=vehicle.scriptHandle||character.attached||!character.alive||
                        !character.drownsInWater||(vehicle.watertight&&character.type!=0)||
                        environment.vcsCharacterExitingVehicle(entry.first))continue;
                    const StorylandMissionCharacterDamage damage{entry.first,character.model,nativeStep,false,0,46};
                    storylandApplyMissionCharacterDamage(runtime,damage,false);
                }
        }
    } else if(!nativeBoat&&!vehicle.nativeDefinition.vcsHovercraft()&&
        vehicle.water.immersion>=1&&runtime.missionEnvironment) {
        for(auto& entry:runtime.missionEnvironment->missionCharacters) {
            auto& character=entry.second;
            if(character.vehicle==vehicle.scriptHandle&&vehicle.scriptHandle>=0&&!character.attached&&
                character.alive&&(!vehicle.watertight||character.type==0)) {
                character.health=std::max(0.0f,character.health-50.0f*dt);
                character.alive=character.health>0;
            }
        }
    }
    if(vehicle.water.immersion>.1f&&!actor.water) {
        vehicle.burning=false;vehicle.engineFireSpent=true;
        for(auto& effect:runtime.effects)
            if(effect.kind==StorylandParticleKind::Flame&&effect.vehicle==int(&vehicle-runtime.vehicles.data()))
                effect.remaining=0;
        return;
    }
    const bool ignitionDamage=vehicle.health<250.0f&&vehicle.health>0;
    if(!ignitionDamage&&!vehicle.wrecked&&vehicle.health>0) {
        vehicle.engineFireSpent=false;
        vehicle.burning=false;
        for(auto& effect:runtime.effects)
            if(effect.vehicle==int(&vehicle-runtime.vehicles.data())&&effect.kind==StorylandParticleKind::Flame)
                effect.remaining=0;
    }
    if(!scriptFire&&!actor.water&&vehicle.canBeDamaged&&!vehicle.burning&&!vehicle.wrecked&&
        !vehicle.engineFireSpent&&!(vehicle.damageProofs&2u)&&ignitionDamage&&runtime.effects.size()<128) {
        vehicle.burning=true;
        vehicle.emergencyFireIdentity=0;vehicle.emergencyFireStrength=1;
        const float forwardX=std::cos(actor.yaw*radians),forwardY=std::sin(actor.yaw*radians);
        StorylandRuntimeEffect fire;
        fire.position={actor.position.x+forwardX*vehicle.halfLength*0.65f,
            actor.position.y+forwardY*vehicle.halfLength*0.65f,actor.position.z+0.4f};
        fire.kind=StorylandParticleKind::Flame;
        fire.remaining=10.0f;
        fire.radius=0.9f;
        fire.damage=7.0f;
        fire.vehicle=int(&vehicle-runtime.vehicles.data());
        fire.owner=fire.vehicle==runtime.occupiedVehicle?-1:INT32_MAX;
        runtime.effects.push_back(fire);

    }
    if(!actor.water) {
        const auto& definition=vehicle.nativeDefinition;
        const auto frame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
            {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
            vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch),
            definition.previewRoot);
        if(vehicle.burning&&!vehicle.wrecked) {
            const auto firePosition=vehicleDamageParticlePosition(runtime,actor,vehicle,frame,true,occupied);
            const int slot=int(&vehicle-runtime.vehicles.data());
            for(auto& effect:runtime.effects)
                if(effect.vehicle==slot&&effect.kind==StorylandParticleKind::Flame&&effect.scriptFireHandle<0)
                    effect.position=firePosition;
        }
        const float cosine=std::cos(actor.yaw*radians),sine=std::sin(actor.yaw*radians);
        const StorylandNativePoint velocity{cosine*vehicle.speed-sine*vehicle.lateralSpeed,
            sine*vehicle.speed+cosine*vehicle.lateralSpeed,vehicleVerticalVelocity(vehicle)};
        vehicle.particleTime+=dt*50.0f;
        const unsigned ticks=std::min(5u,unsigned(vehicle.particleTime+.00001f));
        vehicle.particleTime=std::max(0.0f,vehicle.particleTime-float(ticks));
        for(unsigned tick=0;tick<ticks;++tick) {
            const uint32_t phase=++vehicle.particleFrame+vehicle.routeSeed;
            vehicleEngineFireParticles(runtime,actor,vehicle,frame,occupied);
            vehicleDamageSmoke(runtime,actor,vehicle,frame,velocity,occupied,phase);
            vehicleExhaustParticles(runtime,actor,vehicle,frame,velocity);
        }
        return;
    }
    vehicle.particleTime+=dt*std::min(24.0f,speed*2.0f);
    const int count=std::min(5,int(vehicle.particleTime));vehicle.particleTime-=count;
    const float x=std::cos(actor.yaw*radians),y=std::sin(actor.yaw*radians);
    for(int index=0;index<count;++index) {
        if(speed<1)continue;
        const StorylandNativePoint position{actor.position.x-x*vehicle.halfLength,
            actor.position.y-y*vehicle.halfLength,actor.position.z+.2f};
        vehicleParticle(runtime,StorylandParticleKind::Water,position,
            {-x*speed*.08f+(vehicleRandom(runtime)-.5f),-y*speed*.08f+(vehicleRandom(runtime)-.5f),
                .5f+vehicleRandom(runtime)},.2f,.8f,45);
    }
}

StorylandNativeBody vehicleBody(const StorylandRuntimeVehicle& vehicle,float yaw) {
    StorylandNativeBody body{std::max(0.3f,vehicle.halfLength),std::max(0.25f,vehicle.halfWidth),
        0.20f,std::max(.5f,vehicle.bodyHeight),yaw};
    const auto& definition=vehicle.nativeDefinition;
    if(vehicle.vcsTrafficRail.active)body.sectorLists=0x198;
    if(definition.collision) {
        body.collision=definition.collision.get();
        body.collisionTransform=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,{0,0,0},yaw,
            definition.previewMinimumZ,vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,
            vehicle.bodyPitch+vehicle.roadPitch,definition.domain==StorylandVehicleDomain::Water),definition.previewRoot);
    }
    return body;
}
StorylandNativeBody supportedVehicleBody(const StorylandNativeWorld& world,
    const StorylandRuntimeVehicle& vehicle,const StorylandNativeTrafficActor& actor,
    bool placeOnGround=false) {
    auto body=vehicleBody(vehicle,actor.yaw);
    if(actor.water)return body;
    const float c=std::cos(actor.yaw*radians),s=std::sin(actor.yaw*radians);
    const auto& definition=vehicle.nativeDefinition;
    const auto sample=[&](float x,float y,float& z,StorylandNativePoint* normal=nullptr) {
        return vehicleGroundHeight(world,actor.position.x+c*x-s*y,actor.position.y+s*x+c*y,actor.position.z,z,normal);
    };
    if(definition.hasNativeSuspension&&!definition.wheels.empty()) {
        struct Support {
            float offset=0,height=0;
            unsigned count=0;
            void add(float position,float z) {offset+=position;height+=z;++count;}
        } front,rear,left,right;
        StorylandNativePoint tyreNormal{};
        for(const auto& wheel:definition.wheels) {
            const float longitudinal=wheel.previewPivot[1]-definition.previewRoot[13];
            const float lateral=definition.previewRoot[12]-wheel.previewPivot[0];
            StorylandNativePoint normal;
            float height=0;
            if(!sample(longitudinal,lateral,height,&normal)||normal.z<.8f)continue;
            (wheel.front?front:rear).add(longitudinal,height);
            (lateral>=0?left:right).add(lateral,height);
            tyreNormal.x+=normal.x;tyreNormal.y+=normal.y;tyreNormal.z+=normal.z;
        }
        const auto slope=[](const Support& a,const Support& b) {
            if(!a.count||!b.count)return 0.0f;
            const float span=a.offset/a.count-b.offset/b.count;
            const float rise=a.height/a.count-b.height/b.count;
            return span>.1f&&std::fabs(rise)<span*.7f?std::atan2(rise,span)/radians:0.0f;
        };
        body.pitch=slope(front,rear);
        if(definition.twoWheels()&&tyreNormal.z>0)
            body.roll=std::atan2(tyreNormal.x*s-tyreNormal.y*c,tyreNormal.z)/radians;
        else body.roll=slope(left,right);
    } else {
        float front=0,back=0,left=0,right=0;
        const float l=std::max(0.5f,vehicle.halfLength*0.8f),w=std::max(0.3f,vehicle.halfWidth*0.8f);
        if(sample(l,0,front)&&sample(-l,0,back)&&std::fabs(front-back)<l*1.4f)
            body.pitch=std::atan2(front-back,2*l)/radians;
        if(sample(0,w,left)&&sample(0,-w,right)&&std::fabs(left-right)<w*1.4f)
            body.roll=std::atan2(left-right,2*w)/radians;
    }
    if(definition.collision) {
        const bool dynamic=usesNativeVehicleSuspension(definition)&&!placeOnGround;
        body.collision=vehicle.nativeDefinition.collision.get();
        body.collisionTransform=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,{0,0,0},
            actor.yaw,definition.previewMinimumZ,vehicle.suspension,
            dynamic?vehicle.bodyRoll-vehicle.roadRoll:vehicle.bodyRoll+body.roll,
            dynamic?vehicle.bodyPitch+vehicle.roadPitch:vehicle.bodyPitch+body.pitch),definition.previewRoot);
    }
    return body;
}
void moveRoadVehicle(StorylandRuntimeVehicle& vehicle,StorylandNativeTrafficActor& actor,float seconds) {
    if(!vehicle.positionFrozen&&seconds>0) {
        const float angle=actor.yaw*radians;
        actor.position.x+=(std::cos(angle)*vehicle.speed-std::sin(angle)*vehicle.lateralSpeed)*seconds;
        actor.position.y+=(std::sin(angle)*vehicle.speed+std::cos(angle)*vehicle.lateralSpeed)*seconds;
        const float rotation=vehicle.yawVelocity*seconds;
        const float c=std::cos(rotation*radians),s=std::sin(rotation*radians);
        const float speed=vehicle.speed,lateral=vehicle.lateralSpeed;
        vehicle.speed=c*speed+s*lateral;vehicle.lateralSpeed=c*lateral-s*speed;
        actor.yaw+=rotation;
    }
}
void resolveRoadVehicleSupport(StorylandRuntimeVehicle& vehicle,StorylandNativeTrafficActor& actor,
    const StorylandNativeWorld& world,const StorylandNativePaths& paths,
    const StorylandNativePoint& previous,float seconds,float gravity) {
    applyVehicleGravity(vehicle,actor.position,world,seconds,gravity,actor.yaw);
    const auto moved=storylandMoveNativeBody(world,previous,actor.position,supportedVehicleBody(world,vehicle,actor));
    resolveVehicleBodyMovement(vehicle,actor,previous,moved);
    const auto support=supportedVehicleBody(world,vehicle,actor);
    updateVehicleRoadReference(vehicle,support.pitch,-support.roll);
    if(actor.node<paths.nodes.size()&&actor.nextNode<paths.nodes.size()) {
        const auto& from=paths.nodes[actor.node].position;
        const auto& to=paths.nodes[actor.nextNode].position;
        const float length=distance(from,to);
        const float ratio=length>.001f?std::clamp(actor.distance/length,0.0f,1.0f):0.0f;
        actor.groundOffset=actor.position.z-(from.z+(to.z-from.z)*ratio);
    }
}
void settleRoadVehicle(StorylandRuntimeVehicle& vehicle,StorylandNativeTrafficActor& actor,
    const StorylandNativeWorld& world,const StorylandNativePaths& paths,float seconds,float gravity,
    bool applyCoastingResistance=true) {
    const auto previous=actor.position;
    moveRoadVehicle(vehicle,actor,seconds);
    if(!vehicle.positionFrozen&&seconds>0) {
        vehicle.yawVelocity*=std::pow(vehicle.grounded?.92f:.99f,seconds*StorylandVehicleStepsPerSecond);
        const float resistance=vehicle.grounded&&applyCoastingResistance?
            (vehicle.handbrake?vehicle.handlingBrake:.7f):0;
        vehicle.speed=approach(vehicle.speed,0,resistance*seconds);
        vehicle.lateralSpeed*=std::exp(-seconds*(vehicle.grounded?5.0f:.05f));
    }
    resolveRoadVehicleSupport(vehicle,actor,world,paths,previous,seconds,gravity);
}
void advancePassiveRoadVehicle(StorylandPlayTestRuntime& runtime,StorylandRuntimeVehicle& vehicle,
    StorylandNativeTrafficActor& actor,const StorylandNativeWorld& world,const StorylandNativePaths& paths,
    StorylandPassiveVehicleStatus status,bool driverMustBrake,float seconds) {
    const auto& definition=vehicle.nativeDefinition;
    const float nativeForward=vehicle.speed/StorylandVehicleStepsPerSecond;
    const float nativeLateral=vehicle.lateralSpeed/StorylandVehicleStepsPerSecond;
    const float nativeVertical=vehicleVerticalVelocity(vehicle)/StorylandVehicleStepsPerSecond;
    const auto controls=storylandPassiveVehicleControls(paths.vcs,definition.twoWheels(),status,
        nativeForward*nativeForward+nativeLateral*nativeLateral+nativeVertical*nativeVertical,
        vehicle.steeringAngle,vehicle.bikeStandingOverride,false,driverMustBrake,definition.handlingFlags);
    vehicle.throttle=controls.throttle;
    vehicle.steeringAngle=controls.steering;
    vehicle.steeringInput=0;
    vehicle.steeringInputValid=true;
    vehicle.handbrake=controls.handbrake;
    vehicle.braking=controls.brake>0;
    if(controls.clearBikeLean)vehicle.bikeLeanInput=0;
    const float previousSpeed=vehicle.speed;
    if(!vehicle.contactsValid)updateVehicleWheelContacts(vehicle,actor,world,0);
    const float rain=runtime.missionEnvironment?
        std::clamp(runtime.missionEnvironment->weatherState.rain,0.0f,1.0f):0.0f;
    const float brake=storylandRetailVehicleBrakeDelta(definition,controls.brake,seconds);
    applyVehicleTyreForces(vehicle,actor,0,brake,rain,seconds,false,false,
        runtime.missionEnvironment?runtime.missionEnvironment->controllerMode:0);
    float& vertical=vehicleVerticalVelocity(vehicle);
    const float resistance=storylandRetailVehicleAirResistance(definition.dragMultiplier,
        std::hypot(vehicle.speed,vehicle.lateralSpeed,vertical),seconds);
    vehicle.speed*=resistance;
    vehicle.lateralSpeed*=resistance;
    vertical*=resistance;
    const float angularResistance=storylandNativeVehicleAngularResistance(definition.dragMultiplier,seconds);
    vehicle.yawVelocity*=angularResistance;
    vehicle.springPitchVelocity*=angularResistance;
    vehicle.springRollVelocity*=angularResistance;
    for(float& angularSpeed:vehicle.angularVelocity)angularSpeed*=angularResistance;
    vehicle.longitudinalAcceleration=(vehicle.speed-previousSpeed)/seconds;
    const auto previous=actor.position;
    moveRoadVehicle(vehicle,actor,seconds);
    resolveRoadVehicleSupport(vehicle,actor,world,paths,previous,seconds,runtime.gravity);
}
void recoverVehicle(const StorylandNativeWorld& world,StorylandNativeTrafficActor& actor,
    const StorylandRuntimeVehicle& vehicle) {
    const auto body=supportedVehicleBody(world,vehicle,actor);
    if(storylandNativeBodyClear(world,actor.position,body))return;
    const auto move=storylandMoveNativeBody(world,actor.position,actor.position,body);
    if(move.clear)actor.position=move.position;
}
bool characterCarClear(StorylandNativePoint point,const std::vector<StorylandNativeTrafficActor>& cars,
    const StorylandPlayTestRuntime& runtime) {
    for(size_t i=0;i<cars.size()&&i<runtime.vehicles.size();++i) {
        const auto& car=cars[i];const auto& v=runtime.vehicles[i];
        if(v.craneCarried||!v.collisionEnabled)continue;
        if(std::fabs(point.z-car.position.z)>1.6f)continue;
        const float x=point.x-car.position.x,y=point.y-car.position.y,c=std::cos(car.yaw*radians),s=std::sin(car.yaw*radians);
        const float dx=std::max(0.0f,std::fabs(x*c+y*s)-v.halfLength);
        const float dy=std::max(0.0f,std::fabs(-x*s+y*c)-v.halfWidth);
        if(dx*dx+dy*dy<0.33f*0.33f)return false;
    }
    return true;
}
bool overlapVehicles(const StorylandNativeTrafficActor& a,const StorylandRuntimeVehicle& av,
    const StorylandNativeTrafficActor& b,const StorylandRuntimeVehicle& bv,StorylandNativeContact* contact=nullptr) {
    if(av.craneCarried||bv.craneCarried||!av.collisionEnabled||!bv.collisionEnabled)return false;
    if(av.nativeDefinition.collision&&bv.nativeDefinition.collision)
        return storylandNativeVehicleBodiesOverlap(a.position,vehicleBody(av,a.yaw),b.position,vehicleBody(bv,b.yaw),contact);
    if(std::fabs(a.position.z-b.position.z)>2.0f) return false;
    const float ax=std::cos(a.yaw*radians),ay=std::sin(a.yaw*radians);
    const float bx=std::cos(b.yaw*radians),by=std::sin(b.yaw*radians);
    const float dx=b.position.x-a.position.x,dy=b.position.y-a.position.y;
    float smallestOverlap=std::numeric_limits<float>::infinity();
    StorylandNativePoint contactNormal{};
    for(const auto axis: {std::array<float,2>{ax,ay},std::array<float,2>{-ay,ax},
        std::array<float,2>{bx,by},std::array<float,2>{-by,bx}}) {
        const float ar=av.halfLength*std::fabs(ax*axis[0]+ay*axis[1])+av.halfWidth*std::fabs(-ay*axis[0]+ax*axis[1]);
        const float br=bv.halfLength*std::fabs(bx*axis[0]+by*axis[1])+bv.halfWidth*std::fabs(-by*axis[0]+bx*axis[1]);
        const float projected=dx*axis[0]+dy*axis[1];
        const float penetration=ar+br-std::fabs(projected);
        if(penetration<=-0.05f) return false;
        if(penetration<smallestOverlap) {
            smallestOverlap=penetration;
            contactNormal=VehicleContact::contactAxisAwayFromSecond(
                {axis[0],axis[1],0.0f},projected);
        }
    }
    if(contact) {
        contact->valid=true;
        contact->normal=contactNormal;
        contact->point={(a.position.x+b.position.x)*0.5f,
            (a.position.y+b.position.y)*0.5f,
            (a.position.z+b.position.z)*0.5f+0.5f};
    }
    return true;
}
StorylandBikeFallDecision vehicleBikeImpact(const StorylandRuntimeVehicle& vehicle,
    const StorylandNativeTrafficActor& actor,const StorylandNativeContact& contact,float deltaSpeed,
    bool playerDriven) {
    if(!contact.valid||!vehicle.nativeDefinition.twoWheels()||!vehicle.canBeDamaged||
        (vehicle.damageProofs&8u))return {};
    const auto body=vehicleBody(vehicle,actor.yaw);
    const auto& frame=body.collisionTransform;
    const auto& normal=contact.normal;
    const auto projection=[&](size_t axis) {
        return frame[axis]*normal.x+frame[axis+1]*normal.y+frame[axis+2]*normal.z;
    };
    return storylandBikeCollisionFall(vehicle.nativeDefinition.vcsModelLayout,
        vehicle.nativeDefinition.modelId,deltaSpeed/StorylandNativePhysicsRate,playerDriven,
        vehicle.scriptHandle>=0,projection(4),projection(0),projection(8),normal.z,
        vehicle.speed*vehicle.speed+vehicle.lateralSpeed*vehicle.lateralSpeed+
        vehicle.verticalSpeed*vehicle.verticalSpeed);
}
StorylandBikeFallDecision vehicleBikeSpin(const StorylandRuntimeVehicle& vehicle,
    const StorylandNativeTrafficActor& actor) {
    if(vehicle.bikeStandingOverride)return {};
    if(vehicle.nativeDefinition.vcsModelLayout) {
        if(!vehicle.nativeDefinition.twoWheels())return {};
        const auto angular=vehicleAngularVelocity(vehicle,actor.yaw);
        return storylandVcsBikeAngularFall({angular.x/StorylandNativePhysicsRate,
            angular.y/StorylandNativePhysicsRate,angular.z/StorylandNativePhysicsRate});
    }
    const float c=std::cos(actor.yaw*radians),s=std::sin(actor.yaw*radians);
    const auto& script=vehicle.angularVelocity;
    const float scriptRoll=script[0]*c+script[1]*s;
    const float scriptPitch=script[0]*s-script[1]*c;
    const bool supported=vehicle.grounded&&(!vehicle.contactsValid||
        std::any_of(vehicle.wheelContacts.begin(),vehicle.wheelContacts.end(),[](bool wheel){return wheel;}));
    return storylandBikeSpinFall(vehicle.nativeDefinition.vcsModelLayout,vehicle.nativeDefinition.twoWheels(),
        std::hypot(vehicle.speed,vehicle.lateralSpeed),vehicle.yawVelocity*radians+script[2],
        vehicle.springRollVelocity+scriptRoll,vehicle.springPitchVelocity+scriptPitch,
        std::remainder(vehicle.bodyRoll-vehicle.roadRoll,360.0f),supported,vehicle.bodyContact,
        scriptRoll!=0||scriptPitch!=0);
}

float transferVehicleContact(StorylandPlayTestRuntime& runtime,
    const StorylandNativeTrafficActor& firstActor,StorylandRuntimeVehicle& firstVehicle,
    const StorylandNativeTrafficActor& secondActor,StorylandRuntimeVehicle& secondVehicle,
    const StorylandNativeContact& contact={},float* firstDeltaSpeed=nullptr) {
    const bool firstFixed=scriptVehicleFrozen(runtime,firstVehicle);
    const bool secondFixed=scriptVehicleFrozen(runtime,secondVehicle);
    auto point=contact.point;
    auto normal=contact.normal;
    if(!contact.valid) {
        normal=VehicleContact::unit({firstActor.position.x-secondActor.position.x,
            firstActor.position.y-secondActor.position.y,0});
        point=VehicleContact::scale(VehicleContact::add(firstActor.position,secondActor.position),.5f);
    }
    normal=VehicleContact::orientedPairNormal(firstActor.position,secondActor.position,normal);
    auto first=vehicleContactBody(firstVehicle,firstActor,point,firstFixed);
    auto second=vehicleContactBody(secondVehicle,secondActor,point,secondFixed);
    if(!contact.valid)first.arm=second.arm={};
    const auto hit=VehicleContact::impact(first,second,normal);
    if (hit.impulse <= 0.0f) return 0.0f;
    const auto firstSlot = size_t(&firstVehicle - runtime.vehicles.data());
    const auto secondSlot = size_t(&secondVehicle - runtime.vehicles.data());
    const uint64_t pairKey = (uint64_t(std::min(firstSlot,secondSlot)) << 32) |
        uint64_t(std::max(firstSlot,secondSlot));
    if (!runtime.vehicleContactPairsThisFrame.insert(pairKey).second) return 0.0f;
    recordVehicleContactImpulse(firstVehicle,hit.impulse,normal);
    recordVehicleContactImpulse(secondVehicle,hit.impulse,{-normal.x,-normal.y,-normal.z});
    const auto firstImpulse=VehicleContact::scale(normal,hit.impulse);
    const auto secondImpulse=VehicleContact::scale(firstImpulse,-1);
    const float db=hit.impulse*second.inverseMass;
    if(firstDeltaSpeed)*firstDeltaSpeed=hit.impulse*first.inverseMass;
    applyVehicleImpulse(firstVehicle,firstActor.yaw,first,firstImpulse,firstImpulse);
    applyVehicleImpulse(secondVehicle,secondActor.yaw,second,secondImpulse,secondImpulse);
    const auto* playerVehicle = runtime.occupiedVehicle >= 0 && size_t(runtime.occupiedVehicle) < runtime.vehicles.size() ?
        &runtime.vehicles[size_t(runtime.occupiedVehicle)] : nullptr;
    if (runtime.coopConnected && !runtime.coopFriendlyFire &&
        ((firstVehicle.coopControlled && &secondVehicle == playerVehicle) ||
         (secondVehicle.coopControlled && &firstVehicle == playerVehicle))) {
        if (firstDeltaSpeed) *firstDeltaSpeed = 0;
        return 0;
    }
    if(contact.valid&&runtime.crime.vcs) {
        const bool firstPlayerDriver=&firstVehicle==playerVehicle&&runtime.playerVehicleSeat<0;
        const bool secondPlayerDriver=&secondVehicle==playerVehicle&&runtime.playerVehicleSeat<0;
        storylandEjectVcsMissionBikeRiders(runtime,firstActor,firstVehicle,
            vehicleBikeImpact(firstVehicle,firstActor,contact,
                std::fabs(hit.impulse*first.inverseMass),firstPlayerDriver));
        auto reverse=contact;
        reverse.normal={-contact.normal.x,-contact.normal.y,-contact.normal.z};
        storylandEjectVcsMissionBikeRiders(runtime,secondActor,secondVehicle,
            vehicleBikeImpact(secondVehicle,secondActor,reverse,std::fabs(db),secondPlayerDriver));
    }
    if(runtime.occupiedVehicle>=0&&size_t(runtime.occupiedVehicle)<runtime.vehicles.size()&&
        &secondVehicle==&runtime.vehicles[size_t(runtime.occupiedVehicle)]) {
        auto reverse=contact;
        reverse.normal={-contact.normal.x,-contact.normal.y,-contact.normal.z};
        const auto fall=vehicleBikeImpact(secondVehicle,secondActor,reverse,std::fabs(db),runtime.playerVehicleSeat<0);
        if(fall.eject&&(runtime.pendingBikeImpactVehicle!=runtime.occupiedVehicle||
            fall.damage>runtime.pendingBikeImpact.damage)) {
            runtime.pendingBikeImpactVehicle=runtime.occupiedVehicle;
            runtime.pendingBikeImpact=fall;
        }
    }
    const float impact=hit.closingSpeed;
    if(impact>1&&secondVehicle.impactCooldown<=0) {
        const bool sourcePlayer=&firstVehicle==
            (runtime.occupiedVehicle>=0&&size_t(runtime.occupiedVehicle)<runtime.vehicles.size()?
                &runtime.vehicles[size_t(runtime.occupiedVehicle)]:nullptr);
        const size_t other=size_t(&secondVehicle-runtime.vehicles.data());
        const float damage=secondVehicle.nativeDefinition.vcsModelLayout?
            storylandVehicleCollisionDamage(secondVehicle,impact*5,sourcePlayer):
            (secondVehicle.canBeDamaged&&!(secondVehicle.damageProofs&8u)&&
                storylandLcsVehicleDamageSourceAllowed(runtime,other,sourcePlayer)?impact*5:0);
        secondVehicle.health=std::max(0.0f,secondVehicle.health-damage);
        if(damage>0)secondVehicle.visiblyDamaged=true;
        vehicleImpact(runtime,secondActor,secondVehicle,impact);
    }
    return impact;
}

void separateVehiclePairs(StorylandPlayTestRuntime& runtime,std::vector<StorylandNativeTrafficActor>& cars,
    const StorylandNativeWorld* world) {
    for(int pass=0;pass<4;++pass) {
        bool moved=false;
        for(size_t i=0;i<cars.size()&&i<runtime.vehicles.size();++i)
            for(size_t j=i+1;j<cars.size()&&j<runtime.vehicles.size();++j) {
                const auto& a=cars[i];const auto& b=cars[j];const auto& av=runtime.vehicles[i];const auto& bv=runtime.vehicles[j];
                const float separationX=a.position.x-b.position.x;
                const float separationY=a.position.y-b.position.y;
                const float pairReach=av.halfLength+av.halfWidth+bv.halfLength+bv.halfWidth+8.0f;
                if(separationX*separationX+separationY*separationY>pairReach*pairReach)continue;
                const bool frozenA=scriptVehicleFrozen(runtime,av),frozenB=scriptVehicleFrozen(runtime,bv);
                if(frozenA&&frozenB)continue;
                StorylandNativeContact contact;
                if(!av.collisionEnabled||!bv.collisionEnabled||!overlapVehicles(a,av,b,bv,&contact))continue;
                runtime.vehicles[i].bodyContact=runtime.vehicles[j].bodyContact=true;
                runtime.vehicles[i].lastCollidedSlot = int32_t(j);
                runtime.vehicles[j].lastCollidedSlot = int32_t(i);
                runtime.vehicles[i].pendingContactSlot=int32_t(j);
                runtime.vehicles[j].pendingContactSlot=int32_t(i);
                runtime.vehicles[i].pendingBodyImpact=runtime.vehicles[j].pendingBodyImpact=true;
                const float ac=std::cos(a.yaw*radians),as=std::sin(a.yaw*radians),bc=std::cos(b.yaw*radians),bs=std::sin(b.yaw*radians);
                StorylandNativePoint push{};
                if(contact.valid&&std::isfinite(contact.penetration)&&contact.penetration>0.0f) {
                    // Use the actual sphere contact depth when available. The
                    // visual extents can be smaller than the collision shape.
                    const float planar=contact.normal.x*contact.normal.x+contact.normal.y*contact.normal.y;
                    if(std::isfinite(planar)&&planar>=0.01f) {
                        const float correction=contact.penetration/planar;
                        push={contact.normal.x*correction,contact.normal.y*correction,0};
                    }
                } else {
                    float best=1e30f;
                    for(const auto axis:{std::array<float,2>{ac,as},std::array<float,2>{-as,ac},
                        std::array<float,2>{bc,bs},std::array<float,2>{-bs,bc}}) {
                        const float ar=av.halfLength*std::fabs(ac*axis[0]+as*axis[1])+av.halfWidth*std::fabs(-as*axis[0]+ac*axis[1]);
                        const float br=bv.halfLength*std::fabs(bc*axis[0]+bs*axis[1])+bv.halfWidth*std::fabs(-bs*axis[0]+bc*axis[1]);
                        const float d=(a.position.x-b.position.x)*axis[0]+(a.position.y-b.position.y)*axis[1];
                        const float penetration=ar+br+0.06f-std::fabs(d);
                        if(penetration<best) {
                            best=penetration;
                            const float correction=std::copysign(std::max(0.0f,penetration),d);
                            push={axis[0]*correction,axis[1]*correction,0};
                        }
                    }
                }
                const float pushLength=std::hypot(push.x,push.y);
                if(pushLength>0.5f) {push.x*=0.5f/pushLength;push.y*=0.5f/pushLength;}
                const auto attempt=[&](float share) {
                    auto pa=a.position,pb=b.position;pa.x+=push.x*share;pa.y+=push.y*share;
                    pb.x-=push.x*(1-share);pb.y-=push.y*(1-share);
                    if(world) {
                        if(share>0) {
                            const auto ma=storylandMoveNativeBody(*world,a.position,pa,supportedVehicleBody(*world,av,a));
                            if(!ma.clear||distance(ma.position,pa)>.01f)return false;
                        }
                        if(share<1) {
                            const auto mb=storylandMoveNativeBody(*world,b.position,pb,supportedVehicleBody(*world,bv,b));
                            if(!mb.clear||distance(mb.position,pb)>.01f)return false;
                        }
                    }
                    moved=moved||pa.x!=a.position.x||pa.y!=a.position.y||pb.x!=b.position.x||pb.y!=b.position.y;
                    float deltaSpeed=0;
                    transferVehicleContact(runtime,a,runtime.vehicles[i],b,runtime.vehicles[j],contact,&deltaSpeed);
                    if(int(i)==runtime.occupiedVehicle) {
                        const auto fall=vehicleBikeImpact(av,a,contact,deltaSpeed,runtime.playerVehicleSeat<0);
                        if(fall.eject&&(runtime.pendingBikeImpactVehicle!=runtime.occupiedVehicle||
                            fall.damage>runtime.pendingBikeImpact.damage)) {
                            runtime.pendingBikeImpactVehicle=runtime.occupiedVehicle;
                            runtime.pendingBikeImpact=fall;
                        }
                    }
                    cars[i].position=pa;cars[j].position=pb;
                    return true;
                };
                if(frozenA)attempt(0);
                else if(frozenB)attempt(1);
                else {
                    const float firstInverseMass=1.0f/std::max(1.0f,av.mass);
                    const float secondInverseMass=1.0f/std::max(1.0f,bv.mass);
                    const float firstShare=firstInverseMass/(firstInverseMass+secondInverseMass);
                    if(!attempt(firstShare)&&!attempt(1.0f))attempt(0.0f);
                }
            }
        // Contact flags and pair order are unchanged on another pass unless a
        // vehicle moved. Keep all four passes available for dense pileups.
        if(!moved)break;
    }
}
StorylandNativeMoveResult moveNativeVehicleBody(const StorylandNativeWorld& world,
    const StorylandNativeTrafficActor& from,const StorylandNativeTrafficActor& desired,
    const StorylandRuntimeVehicle& vehicle) {
    StorylandNativeMoveResult result;
    result.position=from.position;
    const float travel=distance(from.position,desired.position);
    const float turn=std::remainder(desired.yaw-from.yaw,360.0f);
    const float radius=std::hypot(vehicle.halfLength,vehicle.halfWidth);
    const float rotation=std::fabs(turn)*radians*radius;
    if(!std::isfinite(travel)||travel>50||!std::isfinite(rotation))return result;
    const unsigned steps=std::max(1u,unsigned(std::ceil((travel+rotation)/.08f)));
    const StorylandNativePoint increment{(desired.position.x-from.position.x)/steps,
        (desired.position.y-from.position.y)/steps,(desired.position.z-from.position.z)/steps};
    auto pose=desired;
    pose.position=from.position;
    pose.yaw=from.yaw;
    for(unsigned step=0;step<steps;++step) {
        auto candidate=pose;
        candidate.position={pose.position.x+increment.x,pose.position.y+increment.y,
            pose.position.z+increment.z};
        candidate.yaw=step+1==steps?desired.yaw:from.yaw+turn*float(step+1)/steps;
        const auto moved=storylandMoveNativeBody(world,pose.position,candidate.position,
            supportedVehicleBody(world,vehicle,candidate));
        if(moved.contact.valid&&(!result.contact.valid||result.contact.normal.z>.65f))
            result.contact=moved.contact;
        result.blocked=result.blocked||moved.blocked;
        result.recovered=result.recovered||moved.recovered;
        if(!moved.clear) {
            result.blocked=true;
            return result;
        }
        candidate.position=moved.position;
        pose=candidate;
    }
    result.position=pose.position;
    result.clear=true;
    return result;
}

bool vehicleWorldBlocked(const StorylandNativeWorld& world,const StorylandNativeTrafficActor& from,
    const StorylandNativeTrafficActor& to,const StorylandRuntimeVehicle& vehicle,StorylandNativePoint* collisionPoint=nullptr,
    StorylandNativeContact* contact=nullptr) {
    if(vehicle.nativeDefinition.collision) {
        const float travel=distance(from.position,to.position);
        const float turn=std::remainder(to.yaw-from.yaw,360.0f);
        const float rotation=std::fabs(turn)*radians*vehicle.halfLength;
        const int steps=std::max(1,int(std::ceil((travel+rotation)/.08f)));
        auto body=supportedVehicleBody(world,vehicle,to);
        std::vector<StorylandNativeBodySample> samples;
        samples.reserve(size_t(steps));
        const auto& definition=vehicle.nativeDefinition;
        for(int step=1;step<=steps;++step) {
            const float t=float(step)/steps;
            const StorylandNativePoint position{
                from.position.x+(to.position.x-from.position.x)*t,
                from.position.y+(to.position.y-from.position.y)*t,
                from.position.z+(to.position.z-from.position.z)*t};
            const float yaw=from.yaw+turn*t;
            body.yaw=yaw;
            body.collisionTransform=storylandVehicleMatrixProduct(
                storylandVehicleMeshFrame(definition,{0,0,0},yaw,definition.previewMinimumZ,
                    vehicle.suspension,usesNativeVehicleSuspension(definition)?
                        vehicle.bodyRoll-vehicle.roadRoll:vehicle.bodyRoll+body.roll,
                    usesNativeVehicleSuspension(definition)?
                        vehicle.bodyPitch+vehicle.roadPitch:vehicle.bodyPitch+body.pitch),
                definition.previewRoot);
            samples.push_back({position,body});
        }
        StorylandNativeContact hit;
        size_t blockedSample=0;
        const bool clear=steps==1?
            storylandNativeBodyClear(world,samples.front().position,samples.front().body,&hit):
            storylandNativeBodySweepClear(world,samples,&hit,&blockedSample);
        if(!clear) {
            if(contact&&hit.valid)*contact=hit;
            if(collisionPoint)*collisionPoint=hit.valid?hit.point:samples[blockedSample].position;
            return true;
        }
        return false;
    }
    const float length=std::hypot(to.position.x-from.position.x,to.position.y-from.position.y);
    const float rotation=std::fabs(std::remainder(to.yaw-from.yaw,360.0f))*radians*vehicle.halfLength;
    const int steps=std::clamp(int(std::ceil((length+rotation)/0.2f)),1,64);
    auto previous=from;
    const auto oldBody=supportedVehicleBody(world,vehicle,previous);
    const auto components=[&](const StorylandNativeTrafficActor& actor,const StorylandNativeBody& body) {
        return std::array<float,4>{std::cos(actor.yaw*radians),std::sin(actor.yaw*radians),
            std::tan(body.pitch*radians),std::tan(body.roll*radians)};
    };
    auto oldComponents=components(previous,oldBody);
    for(int step=1;step<=steps;++step) {
        auto current=to;
        const float t=float(step)/steps;
        current.position={from.position.x+(to.position.x-from.position.x)*t,
            from.position.y+(to.position.y-from.position.y)*t,from.position.z+(to.position.z-from.position.z)*t};
        current.yaw=from.yaw+std::remainder(to.yaw-from.yaw,360.0f)*t;
        const auto newBody=supportedVehicleBody(world,vehicle,current);
        const auto newComponents=components(current,newBody);
        for(float height:{0.4f,1.0f}) for(float side:{-vehicle.halfWidth,0.0f,vehicle.halfWidth})
            for(float longitudinal:{-vehicle.halfLength,0.0f,vehicle.halfLength}) {
                const auto point=[&](const StorylandNativeTrafficActor& actor,const std::array<float,4>& frame) {
                    const float x=frame[0],y=frame[1];
                    return StorylandNativePoint{actor.position.x+x*longitudinal-y*side,
                        actor.position.y+y*longitudinal+x*side,actor.position.z+height+
                            frame[2]*longitudinal+frame[3]*side};
                };
                const auto start=point(previous,oldComponents),end=point(current,newComponents);
                const float hit=storylandTraceNativeSegment(world,start,end);
                if(hit<1) {
                    if(collisionPoint)*collisionPoint={start.x+(end.x-start.x)*hit,start.y+(end.y-start.y)*hit,start.z+(end.z-start.z)*hit};
                    return true;
                }
            }
        previous=current;oldComponents=newComponents;
    }
    return false;
}

void advanceLcsScriptBoat(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    StorylandNativeTrafficActor& actor,StorylandRuntimeVehicle& vehicle,
    StorylandMissionEnvironment::VehicleScriptState& state,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t slot,float seconds,
    const StorylandPlayTestInput* playerInput=nullptr) {
    const auto& definition=vehicle.nativeDefinition;
    const auto& bounds=*definition.collision;
    const float mass=std::max(1.0f,definition.mass);
    // CBoat halves the general handling turn mass at construction.
    const float turnMass=std::max(1.0f,definition.turnMass*.5f);
    const float initialSpeed=vehicle.speed;
    const unsigned steps=std::max(1u,unsigned(std::ceil(seconds/.02f)));
    const float dt=seconds/steps,nativeStep=dt*StorylandVehicleStepsPerSecond;
    vehicle.parked=false;vehicle.handbrake=playerInput?playerInput->handbrake:state.autoPilotMission==0;
    vehicle.braking=playerInput?playerInput->backward:state.autoPilotMission==0;
    vehicle.boatAnchored=false;vehicle.boatAnchorHeading=10000;
    state.boatAnchored=false;state.boatAnchorHeading=10000;
    vehicle.engineOn=state.engineOn&&vehicle.health>0;
    for(unsigned step=0;step<steps;++step) {
        const auto previous=actor;
        const float angle=actor.yaw*radians,c=std::cos(angle),s=std::sin(angle);
        auto controls=StorylandLcsBoatControls{};
        if(playerInput&&vehicle.engineOn) {
            // CBoat::ProcessControlInputs filters the native brake and gas
            // separately. Reverse is 30% brake, not a car transmission gear.
            const float pedalBlend=1-std::pow(.9f,nativeStep),steerBlend=1-std::pow(.8f,nativeStep);
            vehicle.boatBrakeInput+=(float(playerInput->backward)-vehicle.boatBrakeInput)*pedalBlend;
            vehicle.boatBrakeInput=std::clamp(vehicle.boatBrakeInput,0.0f,1.0f);
            if(vehicle.boatBrakeInput<.05f) {
                vehicle.boatBrakeInput=0;
                vehicle.boatAcceleratorInput+=(float(playerInput->forward)-vehicle.boatAcceleratorInput)*pedalBlend;
                vehicle.boatAcceleratorInput=std::clamp(vehicle.boatAcceleratorInput,0.0f,1.0f);
            } else vehicle.boatAcceleratorInput=-vehicle.boatBrakeInput*.3f;
            vehicle.boatSteeringInput+=(float(playerInput->left)-float(playerInput->right)-
                vehicle.boatSteeringInput)*steerBlend;
            vehicle.boatSteeringInput=std::clamp(vehicle.boatSteeringInput,-1.0f,1.0f);
            controls.throttle=vehicle.boatAcceleratorInput;
            controls.steering=definition.steeringLock*radians*vehicle.boatSteeringInput*
                std::fabs(vehicle.boatSteeringInput);
        } else if(vehicle.engineOn&&state.autoPilotMission==14)
            controls=storylandLcsBoatControls(angle,state.targetX-actor.position.x,state.targetY-actor.position.y,
                std::hypot(vehicle.speed,vehicle.lateralSpeed,vehicle.verticalSpeed),state.cruiseSpeed);
        vehicle.throttle=controls.throttle;vehicle.steeringAngle=controls.steering/radians;
        const float steerFactor=playerInput?std::clamp(1-vehicle.speed/StorylandVehicleStepsPerSecond*
            definition.tractionBias*(playerInput->handbrake?.5f:1.0f),0.0f,1.0f):1.0f;
        const float propellerSteering=controls.steering*steerFactor;
        const auto frame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
            {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
            0,0,0,true),definition.previewRoot);
        const auto propeller=storylandVehicleTransformPoint(frame,
            {0,bounds.minimum.y*definition.boatThrustY,bounds.minimum.z*definition.boatThrustZ});
        float propellerWater=0;
        if(storylandNativeWaterHeight(world,propeller[0],propeller[1],propellerWater)&&
            propeller[2]-.5f<propellerWater&&
            (std::fabs(controls.throttle)>.05f||std::hypot(vehicle.speed,vehicle.lateralSpeed)>.5f)) {
            float depth=propellerWater-(propeller[2]-.5f);
            depth=depth>1?1:depth*depth;
            const float armX=-definition.centerOfMass[0];
            const float armY=bounds.minimum.y*definition.boatThrustY-definition.centerOfMass[1];
            const float unitAcceleration=StorylandVehicleStepsPerSecond*StorylandVehicleStepsPerSecond;
            // Native velocity is metres per 1/50-second step. Convert the
            // propeller force once to SI before integrating the physical body.
            if(std::fabs(controls.throttle)>.01f) {
                const float acceleration=depth*controls.throttle*40*definition.engineAcceleration*unitAcceleration;
                const float rightAcceleration=std::sin(propellerSteering)*acceleration;
                const float forwardAcceleration=std::cos(propellerSteering)*acceleration;
                vehicle.speed+=forwardAcceleration*dt;
                vehicle.lateralSpeed-=rightAcceleration*dt;
                // CBoat also applies a right-force couple at GetUp(). With an
                // authored longitudinal centre of mass this contributes yaw.
                const float rightCouple=-rightAcceleration*definition.traction;
                vehicle.yawVelocity+=mass*(armX*forwardAcceleration-armY*rightAcceleration+
                    definition.centerOfMass[1]*rightCouple)/turnMass*dt/radians;
            }
            if(std::fabs(controls.throttle)<.05f) {
                // The native slow-ahead rudder remains effective when cruise
                // control reduces gas near the target speed.
                float force=std::min(definition.tractionLoss*vehicle.speed/StorylandVehicleStepsPerSecond,
                    .01f*turnMass);
                if(controls.throttle>.01f)force*= (.55f-std::fabs(controls.throttle))*(playerInput?1.3f:2.5f);
                if((controls.throttle<0&&force>0)||(controls.throttle>0&&force<0))force=-force;
                const float rightForce=depth*std::sin(propellerSteering)*force;
                vehicle.lateralSpeed-=rightForce/mass*unitAcceleration*dt;
                const float rightCouple=steerFactor>0?std::sin(propellerSteering)*force*.75f/steerFactor:0;
                vehicle.yawVelocity+=(-armY*rightForce+definition.centerOfMass[1]*rightCouple)/
                    turnMass*unitAcceleration*dt/radians;
            }
        }
        float surface=0;
        const bool inWater=storylandNativeWaterHeight(world,actor.position.x,actor.position.y,surface);
        if(inWater) {
            const float volume=storylandLcsBoatSubmergedVolume(definition,bounds.minimum.z,bounds.maximum.z,
                surface-frame[14]);
            if(volume>0) {
                if(playerInput&&playerInput->handbrake&&vehicle.speed>0)
                    vehicle.speed-=.1f*definition.suspensionLower*volume*vehicle.speed*nativeStep;
                const float nativeForward=vehicle.speed/StorylandVehicleStepsPerSecond;
                const float resistance=.001f*definition.suspensionForce*volume*volume*mass;
                const float magic=std::fabs((nativeForward*nativeForward+.05f)*resistance+1);
                vehicle.speed*=std::pow(definition.boatMoveResistance[1]/magic,.5f*nativeStep);
                vehicle.lateralSpeed*=std::pow(definition.boatMoveResistance[0]/magic,.5f*nativeStep);
            }
        }
        vehicle.yawVelocity*=std::pow(definition.boatTurnResistance[2],nativeStep);
        const float velocityX=c*vehicle.speed-s*vehicle.lateralSpeed;
        const float velocityY=s*vehicle.speed+c*vehicle.lateralSpeed;
        actor.position.x+=velocityX*dt;actor.position.y+=velocityY*dt;
        actor.yaw=std::remainder(actor.yaw+vehicle.yawVelocity*dt,360.0f);
        // Turning changes the local velocity components, not world momentum.
        const float nextC=std::cos(actor.yaw*radians),nextS=std::sin(actor.yaw*radians);
        vehicle.speed=nextC*velocityX+nextS*velocityY;
        vehicle.lateralSpeed=-nextS*velocityX+nextC*velocityY;
        float unusedForward=0,unusedSide=0;
        // Keep the existing vertical water body integration; horizontal drag
        // above is the native CBoat resistance and must not be applied twice.
        if(!storylandStepWaterVehicle(vehicle.water,actor.position.z,vehicle.verticalSpeed,
            unusedForward,unusedSide,vehicle.bodyHeight,surface,inWater,true,vehicle.health<=0,
            runtime.gravity,dt)) {
            vehicle.verticalSpeed-=runtime.gravity*dt;
            actor.position.z+=vehicle.verticalSpeed*dt;
        }
        const auto movement=storylandMoveNativeBody(world,previous.position,actor.position,vehicleBody(vehicle,actor.yaw));
        StorylandNativePoint collisionPoint=movement.position;
        const bool worldBlocked=!movement.clear||movement.blocked||
            vehicleWorldBlocked(world,previous,actor,vehicle,&collisionPoint);
        bool blocked=worldBlocked,playerCollision=false;
        const float travel=distance(previous.position,actor.position)+
            std::fabs(std::remainder(actor.yaw-previous.yaw,360.0f))*radians*vehicle.halfLength;
        const unsigned collisionSteps=std::max(1u,unsigned(std::ceil(travel/.1f)));
        for(unsigned collisionStep=1;collisionStep<=collisionSteps&&!blocked;++collisionStep) {
            auto sample=actor;const float fraction=float(collisionStep)/collisionSteps;
            sample.position={previous.position.x+(actor.position.x-previous.position.x)*fraction,
                previous.position.y+(actor.position.y-previous.position.y)*fraction,
                previous.position.z+(actor.position.z-previous.position.z)*fraction};
            sample.yaw=previous.yaw+std::remainder(actor.yaw-previous.yaw,360.0f)*fraction;
            for(size_t other=0;other<cars.size()&&other<runtime.vehicles.size();++other)
                if(other!=slot&&overlapVehicles(sample,vehicle,cars[other],runtime.vehicles[other])) {
                    blocked=true;playerCollision=int(other)==runtime.occupiedVehicle;break;
                }
        }
        if(blocked) {
            const float impact=std::hypot(vehicle.speed,vehicle.lateralSpeed);
            actor=previous;vehicleImpact(runtime,actor,vehicle,impact,worldBlocked?&collisionPoint:nullptr);
            if(vehicle.canBeDamaged&&storylandLcsVehicleDamageSourceAllowed(runtime,slot,playerCollision)&&
                !(vehicle.damageProofs&8u))vehicle.health=std::max(0.0f,vehicle.health-impact*5);
            vehicle.speed=vehicle.lateralSpeed=vehicle.verticalSpeed=vehicle.yawVelocity=0;
            break;
        }
        actor.position=movement.position;
    }
    vehicle.longitudinalAcceleration=(vehicle.speed-initialSpeed)/seconds;
    vehicle.stoppedSeconds=std::fabs(vehicle.speed)<.2f?vehicle.stoppedSeconds+seconds:0;
    state.x=actor.position.x;state.y=actor.position.y;state.z=actor.position.z;
    state.heading=actor.yaw-90;state.speed=vehicle.speed;
}

}
int storylandRemoteVehicleSlot(const StorylandPlayTestRuntime& runtime) {
    if(!runtime.missionEnvironment)return -1;
    const auto& environment=*runtime.missionEnvironment;
    const auto car=environment.missionVehicles.find(environment.remoteControl.vehicle);
    if(car==environment.missionVehicles.end()||car->second.deleted)return -1;
    const int32_t slot=car->second.spawnedSlot;
    return slot>=0&&size_t(slot)<runtime.vehicles.size()&&
        runtime.vehicles[size_t(slot)].scriptHandle==car->first?slot:-1;
}

int storylandVcsRemoteVehicleSlot(const StorylandPlayTestRuntime& runtime) {
    if(!runtime.missionEnvironment)return -1;
    const auto& environment=*runtime.missionEnvironment;
    const auto car=environment.missionVehicles.find(environment.remoteVehicle);
    if(car==environment.missionVehicles.end()||car->second.deleted||car->second.wrecked)return -1;
    const int32_t slot=car->second.spawnedSlot;
    return slot>=0&&size_t(slot)<runtime.vehicles.size()&&
        runtime.vehicles[size_t(slot)].scriptHandle==car->first?slot:-1;
}

bool storylandDetonateRemoteVehicle(StorylandPlayTestRuntime& runtime,bool contact) {
    if(!runtime.missionEnvironment)return false;
    auto& environment=*runtime.missionEnvironment;auto& remote=environment.remoteControl;
    const auto car=environment.missionVehicles.find(remote.vehicle);
    if(remote.ending||(!contact&&(remote.detonationDisabled||!environment.remoteDetonationEnabled))||
        car==environment.missionVehicles.end()||car->second.deleted||
        !environment.explodeMissionVehicle)return false;
    environment.explodeMissionVehicle(remote.vehicle);
    car->second.persistent=false;remote.ending=true;remote.restoreCamera=true;remote.lostAt=environment.gameMilliseconds;
    return true;
}

static bool storylandRcBanditHitsWheels(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t slot) {
    const auto& rc=runtime.vehicles[slot];const auto& definition=rc.nativeDefinition;
    if(rc.craneCarried||definition.modelId!=169||!definition.collision)return false;
    const auto& actor=cars[slot];
    const auto frame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
        {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
        rc.suspension,rc.bodyRoll-rc.roadRoll,rc.bodyPitch+rc.roadPitch),definition.previewRoot);
    const auto& bounds=*definition.collision;
    for(size_t other=0;other<cars.size()&&other<runtime.vehicles.size();++other) {
        if(other==slot)continue;
        const auto& car=runtime.vehicles[other];const auto& model=car.nativeDefinition;
        if(car.craneCarried||model.nativeType!=0||model.modelId==169||std::fabs(cars[other].position.x-actor.position.x)>=10||
            std::fabs(cars[other].position.y-actor.position.y)>=10)continue;
        const auto carFrame=storylandVehicleMeshFrame(model,
            {cars[other].position.x,cars[other].position.y,cars[other].position.z},cars[other].yaw,model.previewMinimumZ,
            car.suspension,car.bodyRoll-car.roadRoll,car.bodyPitch+car.roadPitch);
        for(size_t wheel=0;wheel<model.wheels.size()&&wheel<4;++wheel) {
            if(car.wheelSpringRatios[wheel]>=1.0f&&(!car.hasDriver||car.parked))continue;
            const auto center=storylandVehicleInversePoint(frame,
                storylandVehicleTransformPoint(carFrame,model.wheels[wheel].previewPivot));
            const float radius=model.wheelScale*.25f;
            const float x=center[0]-std::clamp(center[0],bounds.minimum.x,bounds.maximum.x);
            const float y=center[1]-std::clamp(center[1],bounds.minimum.y,bounds.maximum.y);
            const float z=center[2]-std::clamp(center[2],bounds.minimum.z,bounds.maximum.z);
            if(x*x+y*y+z*z<radius*radius)return true;
        }
    }
    return false;
}

StorylandNativeMoveResult storylandMovePlayTestCharacter(const StorylandNativeWorld& world,
    const StorylandPlayTestRuntime& runtime,const StorylandNativePoint& from,const StorylandNativePoint& desired,
    int pedestrian,bool avoidPlayer,bool followGround,bool collideWithPedestrians,int ignoredVehicle) {
    if(pedestrian<0&&!avoidPlayer&&runtime.missionEnvironment&&
        !runtime.missionEnvironment->playerCollisionEnabled) {
        StorylandNativeMoveResult result{from};
        if(runtime.missionEnvironment->playerDisableVerticalMovement) {
            result.position.x=desired.x;result.position.y=desired.y;
        }
        result.clear=true;
        return result;
    }
    const auto* environment=runtime.missionEnvironment;
    const StorylandNativeCollisionModel* playerCollision=nullptr;
    StorylandVehicleMatrix playerCollisionFrame{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    if(world.paths.vcs&&pedestrian<0&&!avoidPlayer&&environment&&environment->modelIdByName&&
        environment->characterOriginOffset&&std::isfinite(environment->heading)) {
        const int32_t model=environment->modelIdByName(environment->playerOutfit);
        const auto found=world.collisionModels.find(uint32_t(model));
        float originOffset=0;
        if(model>=0&&found!=world.collisionModels.end()&&!found->second.spheres.empty()&&
            found->second.boxes.empty()&&found->second.triangles.empty()&&
            environment->characterOriginOffset(0,originOffset)&&std::isfinite(originOffset)) {
            playerCollision=&found->second;
            const float angle=environment->heading*radians;
            playerCollisionFrame[0]=playerCollisionFrame[5]=std::cos(angle);
            playerCollisionFrame[1]=std::sin(angle);playerCollisionFrame[4]=-playerCollisionFrame[1];
            playerCollisionFrame[14]=originOffset;
        }
    }
    auto separated=from;
    const auto sweep=[&](const StorylandNativePoint& a,const StorylandNativePoint& b) {
        return followGround?storylandMoveNativeCharacter(world,a,b):
            storylandMoveNativeBody(world,a,b,storylandNativeCharacterBody());
    };
    const auto constrain=[&](StorylandNativePoint p) {
        for(size_t carIndex=0;carIndex<runtime.movementCars.size();++carIndex) {
            const auto& car=runtime.movementCars[carIndex];
            if(!car.collisionEnabled||int32_t(carIndex)==ignoredVehicle||
                (pedestrian<0&&runtime.vehicleExitPose.active&&!runtime.vehicleExitPose.nativeBailout&&
                 int32_t(carIndex)==runtime.vehicleExitPose.sourceVehicle))continue;
            if(p.z>=car.position.z+car.height-.02f||p.z+1.8f<car.position.z+.2f)continue;
            const float c=std::cos(car.yaw*radians),s=std::sin(car.yaw*radians);
            float x=(p.x-car.position.x)*c+(p.y-car.position.y)*s,y=-(p.x-car.position.x)*s+(p.y-car.position.y)*c;
            const float l=car.halfLength+0.33f,w=car.halfWidth+0.33f;
            if(std::fabs(x)>=l||std::fabs(y)>=w)continue;
            if(carIndex<runtime.vehicles.size()) {
                const auto& vehicle=runtime.vehicles[carIndex];
                if(vehicle.nativeDefinition.vcsModelLayout&&vehicle.nativeDefinition.collision) {
                    const auto body=vehicleBody(vehicle,car.yaw);
                    auto matrix=body.collisionTransform;
                    matrix[12]+=car.position.x;matrix[13]+=car.position.y;matrix[14]+=car.position.z;
                    bool clear=false;
                    if(playerCollision&&storylandVcsOrdinaryBoat(vehicle.nativeDefinition)) {
                        // VCS 1FA018..1FA058 collides the actual ped and vehicle model COLs.
                        clear=true;
                        for(const auto& sphere:playerCollision->spheres) {
                            const auto center=storylandVehicleTransformPoint(playerCollisionFrame,
                                {sphere.center.x,sphere.center.y,sphere.center.z});
                            bool hit=false;
                            if(!storylandNativeSphereModelContact({p.x+center[0],p.y+center[1],p.z+center[2]},
                                sphere.radius,*body.collision,matrix,hit)||hit) {clear=false;break;}
                        }
                    } else clear=storylandNativeCharacterModelClear(p,*body.collision,matrix);
                    if(clear)continue;
                }
            }
            if(l-std::fabs(x)<w-std::fabs(y))x=std::copysign(l+0.002f,x==0?1:x);
            else y=std::copysign(w+0.002f,y==0?1:y);
            p.x=car.position.x+c*x-s*y;p.y=car.position.y+s*x+c*y;
        }
        const auto avoid=[&](StorylandNativePoint other,float radius,unsigned tie) {
            if(std::fabs(p.z-other.z)>1.4f)return;
            float x=p.x-other.x,y=p.y-other.y,l=std::hypot(x,y);
            if(l>=radius)return;
            if(l<1e-5f) {const float angle=float(tie%8)*0.78539816339f;x=std::cos(angle);y=std::sin(angle);l=1;}
            p.x=other.x+x/l*(radius+0.002f);p.y=other.y+y/l*(radius+0.002f);
        };
        if(collideWithPedestrians) {
            for(size_t i=0;i<runtime.movementPedestrians.size()&&i<runtime.pedestrians.size();++i) {
                const auto& other=runtime.pedestrians[i];
                if(int(i)==pedestrian||other.health<=0||other.renderAlpha==0||other.serviceVehicleIdentity)continue;
                if(other.scriptHandle>=0&&runtime.missionEnvironment&&
                    runtime.missionEnvironment->missionCharacters.count(other.scriptHandle))continue;
                avoid(runtime.movementPedestrians[i],0.62f,unsigned(i+pedestrian+2));
            }
            if(!avoidPlayer&&runtime.missionEnvironment)for(const auto& item:runtime.missionEnvironment->missionCharacters) {
                const auto& other=item.second;
                if(!other.alive||other.health<=0||!other.visible||other.renderAlpha==0||
                    other.removeFromWorld||!other.collisionEnabled||other.vehicle>=0||other.attached)continue;
                if(storylandPlayerReactionHoldsTarget(runtime.playerReaction,StorylandReactionTargetKind::Mission,
                    item.first,uint32_t(other.model)))continue;
                avoid({other.x,other.y,other.z},0.62f,uint32_t(item.first));
            }
            if(avoidPlayer&&runtime.occupiedVehicle<0)avoid(runtime.playerPosition,0.64f,unsigned(pedestrian+3));
        }
        return p;
    };
    StorylandNativeMoveResult result{from};
    // Resolve pre-existing dynamic overlap before sweeping normal motion.
    for(int iteration=0;iteration<8;++iteration) {
        const auto target=constrain(separated);
        if(distance(target,separated)<0.0001f)break;
        const auto move=sweep(separated,target);
        if(!move.clear)break;
        separated=move.position;result.recovered=true;
    }
    const StorylandNativePoint travel{desired.x-from.x,desired.y-from.y,desired.z-from.z};
    const float l=distance(desired,from);if(!std::isfinite(l)||l>50) {result.clear=false;result.blocked=true;return result;}
    const int steps=std::max(1,int(std::ceil(l/.1f)));
    auto position=separated;
    for(int step=0;step<steps;++step) {
        auto target=position;target.x+=travel.x/steps;target.y+=travel.y/steps;target.z+=travel.z/steps;
        const auto constrained=constrain(target);
        if(distance(target,constrained)>0.0001f) {
            result.blocked=true;
            if(distance(position,constrained)>l/steps+0.02f)continue;
        }
        const auto move=sweep(position,constrained);
        result.blocked|=move.blocked;result.recovered|=move.recovered;
        if(move.clear)position=move.position;
    }
    result.position=position;
    result.clear=storylandNativeBodyClear(world,position,storylandNativeCharacterBody())&&distance(position,constrain(position))<0.001f;
    return result;
}

static bool storylandAmbientPedContactMovable(const StorylandPlayTestRuntime& runtime,size_t index) {
    if(index>=runtime.pedestrians.size())return false;
    const auto& ped=runtime.pedestrians[index];
    return ped.health>0&&ped.renderAlpha>0&&ped.scriptHandle<0&&!ped.serviceVehicleIdentity&&!ped.water.active&&
        ped.coughTime<=0&&!ped.threatWaiting&&ped.threatAnimation<0&&!ped.bulletReaction.active(runtime.elapsed)&&
        !storylandPlayerReactionHoldsTarget(runtime.playerReaction,StorylandReactionTargetKind::Ambient,
            int32_t(index),ped.stats.modelId)&&
        (!runtime.missionEnvironment||!runtime.missionEnvironment->cutscene.playing);
}

static bool storylandAmbientPedContactAnimationAllowed(const StorylandRuntimePed& ped) {
    return !ped.melee.active&&!ped.ranged.active&&ped.fireFlash<=0&&
        ped.weaponCooldown<=0&&ped.reloading<=0;
}

static uint8_t storylandAmbientPedContactMoveState(const StorylandRuntimePed& ped,
    float requestedSpeed = 0) {
    if (ped.state == StorylandPedState::Walk) return 2;
    if (ped.state == StorylandPedState::Flee) return ped.fleeWalking ? 2 : 4;
    if (ped.state == StorylandPedState::Combat) {
        const float speed = std::max(ped.combatSpeed, requestedSpeed);
        return speed > 2.0f ? 4 : speed > .01f ? 2 : 1;
    }
    return 1;
}

static uint8_t storylandMissionPedContactMoveState(
    const StorylandMissionEnvironment::Character& character) {
    if (!character.walkingToTarget) return 1;
    return character.walkSpeed > 4 ? 5 : character.walkSpeed > 2 ? 4 : 2;
}

static bool storylandMissionPedContactBody(const StorylandMissionEnvironment::Character& character) {
    return character.alive&&character.health>0&&character.visible&&character.renderAlpha>0&&
        !character.removeFromWorld&&character.collisionEnabled&&character.vehicle<0&&!character.attached;
}

static bool storylandMissionPedContactMovable(const StorylandPlayTestRuntime& runtime,int32_t handle,
    const StorylandMissionEnvironment::Character& character) {
    if(!storylandMissionPedContactBody(character)||!runtime.missionEnvironment)return false;
    const auto& environment=*runtime.missionEnvironment;
    if(environment.cutscene.playing||character.networkControlled||character.positionFrozen||
        character.stayInSamePlace||character.disableVerticalMovement||character.animationId>=0||
        character.waitState!=0||character.crouch.active||character.crouch.poseFade.present||
        environment.characterVehicleTasks.count(handle)||environment.characterAttachments.count(handle)||
        environment.pedAnimationRequests.count(handle)||environment.scriptedCharacterAnimations.count(handle)||
        environment.mobilePhones.count(handle)||environment.vcsPedFalls.count(handle)||
        (environment.characterReactionOwned&&environment.characterReactionOwned(handle))||
        storylandPlayerReactionHoldsTarget(runtime.playerReaction,StorylandReactionTargetKind::Mission,
            handle,uint32_t(character.model)))return false;
    const auto combat=runtime.missionCombat.find(handle);
    return combat==runtime.missionCombat.end()||(!combat->second.water.active&&combat->second.coughTime<=0&&
        !storylandMissionBulletReactionActive(runtime,handle)&&
        (!combat->second.gravityPositionValid||(combat->second.grounded&&combat->second.landingTime<=0)));
}

static bool storylandMissionPedContactAnimationAllowed(const StorylandMissionCombatState& combat) {
    return !combat.melee.active&&combat.cooldown<=0&&combat.reloading<=0;
}

std::vector<StorylandPedContactObstacle> storylandPedContactObstacles(
    const StorylandPlayTestRuntime& runtime) {
    std::vector<StorylandPedContactObstacle> obstacles;
    obstacles.reserve(runtime.movementPedestrians.size()+
        (runtime.missionEnvironment?runtime.missionEnvironment->missionCharacters.size():0)+1);
    for(size_t index=0;index<runtime.movementPedestrians.size()&&index<runtime.pedestrians.size();++index) {
        const auto& ped=runtime.pedestrians[index];
        if(ped.health<=0||ped.renderAlpha==0||ped.water.active||ped.serviceVehicleIdentity)continue;
        if(ped.scriptHandle>=0&&runtime.missionEnvironment&&
            runtime.missionEnvironment->missionCharacters.count(ped.scriptHandle))continue;
        const auto& point=runtime.movementPedestrians[index];
        if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))continue;
        const auto velocity=storylandFinitePedContactPoint(ped.contact.velocity)?
            ped.contact.velocity:StorylandNativePoint{};
        const uint8_t movement = storylandAmbientPedContactMoveState(ped);
        obstacles.push_back({uint64_t(index)+1,point,velocity,.31f,ped.contact.waitCredit,
            storylandAmbientPedContactMovable(runtime,index), movement == 4 || movement == 5, false});
    }
    if(runtime.missionEnvironment)for(const auto& item:runtime.missionEnvironment->missionCharacters) {
        const auto& character=item.second;
        if(!storylandMissionPedContactBody(character))continue;
        const StorylandNativePoint point{character.x,character.y,character.z};
        if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))continue;
        const auto combat=runtime.missionCombat.find(item.first);
        const float credit=combat==runtime.missionCombat.end()?0:combat->second.contact.waitCredit;
        const bool movable=storylandMissionPedContactMovable(runtime,item.first,character);
        const float speed=movable&&character.walkingToTarget&&std::isfinite(character.walkSpeed)?
            std::max(0.0f,character.walkSpeed):0;
        const float heading=(character.heading+90)*radians;
        const StorylandNativePoint velocity=std::isfinite(heading)&&speed>0?
            StorylandNativePoint{std::cos(heading)*speed,std::sin(heading)*speed,0}:StorylandNativePoint{};
        const uint8_t movement = storylandMissionPedContactMoveState(character);
        obstacles.push_back({UINT64_C(0x100000000)|uint32_t(item.first),point,
            velocity,.31f,credit,movable, movement == 4 || movement == 5, character.creationSource == 2});
    }
    if(runtime.occupiedVehicle<0&&runtime.health>0&&runtime.vehicleAction==StorylandVehicleAction::None&&
        (!runtime.missionEnvironment||(runtime.missionEnvironment->playerCollisionEnabled&&
            !runtime.missionEnvironment->cutscene.playing))) {
        const auto point=runtime.missionEnvironment?StorylandNativePoint{runtime.missionEnvironment->x,
            runtime.missionEnvironment->y,runtime.missionEnvironment->z}:runtime.playerPosition;
        if(std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z))
            obstacles.push_back({UINT64_MAX,point,runtime.pedestrianContactPlayerVelocity,.33f,0,false,
                runtime.playerMoveBlend > 1.8f, false});
    }
    return obstacles;
}

static void storylandStartPedContactAnimation(StorylandPlayTestRuntime& runtime,
    StorylandPedContactState& state,const StorylandPedContactPlan& plan,float heading,uint8_t movement) {
    if(!plan.contact||!plan.incomingBump||state.cooldown>0||state.active())return;
    const int32_t animation=storylandPedContactAnimation(runtime.crime.vcs,movement,heading,plan.normal);
    if(animation<0)return;
    const float duration=runtime.missionEnvironment&&runtime.missionEnvironment->animationDuration?
        runtime.missionEnvironment->animationDuration(0,animation):
        runtime.reactionBindings.duration?runtime.reactionBindings.duration(animation,0):0;
    if(!std::isfinite(duration)||duration<=0)return;
    state.animation=animation;
    state.animationTime=0;
    state.animationMotionTime=0;
    state.animationDuration=duration;
    state.cooldown=1.0f;
    state.contactNormal=plan.normal;
}

static void storylandCancelPedContactAnimation(StorylandPedContactState& state) {
    state.animation=-1;
    state.animationTime=0;
    state.animationDuration=0;
    state.animationMotionTime=0;
}

static bool storylandPedContactAreaContains(const StorylandMissionEnvironment::Character& character,
    const StorylandNativePoint& point) {
    const auto& area=character.behavior.area;
    return std::all_of(area.begin(),area.end(),[](float value){return std::isfinite(value);})&&
        point.x>=area[0]&&point.x<=area[2]&&point.y>=area[1]&&point.y<=area[3];
}

static StorylandNativeMoveResult storylandSweepPedContact(const StorylandNativeWorld& world,
    const StorylandPlayTestRuntime& runtime,const StorylandNativePoint& from,StorylandNativePoint desired,
    const StorylandMissionEnvironment::Character* character=nullptr) {
    const bool constrained=character&&character->behavior.areaEnabled&&
        storylandPedContactAreaContains(*character,from);
    const auto limit=[&](StorylandNativePoint point) {
        if(constrained) {
            const auto& area=character->behavior.area;
            point.x=std::clamp(point.x,area[0],area[2]);
            point.y=std::clamp(point.y,area[1],area[3]);
        }
        return point;
    };
    auto moved=storylandMovePlayTestCharacter(world,runtime,from,limit(desired),INT32_MAX,false,true,false);
    if(constrained&&moved.clear&&!storylandPedContactAreaContains(*character,moved.position)) {
        moved=storylandMovePlayTestCharacter(world,runtime,from,limit(moved.position),INT32_MAX,false,true,false);
        if(!moved.clear||!storylandPedContactAreaContains(*character,moved.position)) {
            moved={from};
            moved.clear=storylandNativeBodyClear(world,from,storylandNativeCharacterBody());
            moved.blocked=true;
        }
    }
    return moved;
}

static StorylandNativeMoveResult storylandMovePedContact(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,uint64_t identity,StorylandPedContactState& state,
    const StorylandNativePoint& from,const StorylandNativePoint& desired,float heading,float seconds,
    const std::vector<StorylandPedContactObstacle>& obstacles,bool animationAllowed,uint8_t movement,
    bool missionCharacter,const StorylandMissionEnvironment::Character* character=nullptr,
    const StorylandPedContactGoal* goal=nullptr) {
    const float step=std::clamp(seconds,0.0f,.1f);
    if(step<=0)return {from};
    if(!animationAllowed)storylandCancelPedContactAnimation(state);
    auto plan=storylandPlanPedContact(identity,state,from,desired,obstacles,step,.31f,goal,missionCharacter);
    if(animationAllowed)storylandStartPedContactAnimation(runtime,state,plan,heading,movement);
    if(state.active()||(state.animation>=0&&state.animationMotionTime<state.animationTime)) {
        if (storylandPedContactOwnsRoot(runtime.crime.vcs,state.animation)) {
            const float weight = state.weight();
            plan.destination.x = from.x + (plan.destination.x - from.x) * (1 - weight);
            plan.destination.y = from.y + (plan.destination.y - from.y) * (1 - weight);
            uint16_t flags = storylandPedContactAssociationFlags(runtime.crime.vcs,state.animation);
            if (runtime.missionEnvironment && runtime.missionEnvironment->animationAssociationFlags)
                runtime.missionEnvironment->animationAssociationFlags(0,state.animation,flags);
            if ((flags & 0x40u) && runtime.reactionBindings.rootDelta &&
                state.animationMotionTime < state.animationTime) {
                const auto root=runtime.reactionBindings.rootDelta(state.animation,0,
                    std::min(state.animationMotionTime,state.animationDuration),
                    std::min(state.animationTime,state.animationDuration));
                if(std::isfinite(root.x)&&std::isfinite(root.y)&&std::hypot(root.x,root.y)<2.0f) {
                    const float angle=heading*radians,c=std::cos(angle),s=std::sin(angle);
                    plan.destination.x+=(c*root.x+s*root.y)*weight;
                    plan.destination.y+=(s*root.x-c*root.y)*weight;
                }
            }
        }
        state.animationMotionTime=state.animationTime;
        if(!state.active())state.animation=-1;
    }
    const auto candidate=storylandClipPedContactMotion(identity,from,plan.destination,obstacles);
    auto result=storylandSweepPedContact(world,runtime,from,candidate,character);
    if(result.clear)state.velocity={(result.position.x-from.x)/step,(result.position.y-from.y)/step,0};
    else state.velocity={};
    return result;
}

StorylandNativeMoveResult storylandMoveAmbientPedestrian(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,size_t index,const StorylandNativePoint& from,
    const StorylandNativePoint& desired,float heading,float seconds,
    const std::vector<StorylandPedContactObstacle>& obstacles) {
    if(index>=runtime.pedestrians.size())return {from};
    auto& ped=runtime.pedestrians[index];
    if(ped.health>0&&!ped.water.active&&ped.bulletReaction.active(runtime.elapsed)) {
        ped.contact.clear();ped.combatSpeed=0;
        return {from};
    }
    if(!storylandAmbientPedContactMovable(runtime,index)) {
        ped.contact.clear();
        return storylandMovePlayTestCharacter(world,runtime,from,desired,int(index),true);
    }
    return storylandMovePedContact(runtime,world,uint64_t(index)+1,ped.contact,from,desired,
        heading,seconds,obstacles,storylandAmbientPedContactAnimationAllowed(ped),
        storylandAmbientPedContactMoveState(ped,seconds>0?
            std::hypot(desired.x-from.x,desired.y-from.y)/seconds:0),false);
}

bool storylandMoveMissionPedestrian(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,int32_t handle,
    const std::array<float,3>& desired,std::array<float,3>& result) {
    if(!runtime.missionEnvironment)return false;
    auto& environment=*runtime.missionEnvironment;
    const auto found=environment.missionCharacters.find(handle);
    if(found==environment.missionCharacters.end())return false;
    auto& character=found->second;
    const StorylandNativePoint from{character.x,character.y,character.z};
    const StorylandNativePoint destination{desired[0],desired[1],desired[2]};
    if(!std::isfinite(destination.x)||!std::isfinite(destination.y)||!std::isfinite(destination.z))return false;
    if(storylandMissionBulletReactionActive(runtime,handle)) {
        character.movementSpeed=0;
        result={from.x,from.y,from.z};
        return true;
    }
    if(world.paths.vcs) {
        float originOffset=0;
        if(character.avoidUnsupportedForwardStep&&(!environment.characterOriginOffset||
            !environment.characterOriginOffset(handle,originOffset)||!std::isfinite(originOffset)))return false;
        const auto support=storylandVcsPedForwardSupportProbe(world,
            character.avoidUnsupportedForwardStep,character.alive&&character.health>0,
            character.vehicle>=0,from,destination,character.heading,originOffset);
        if(environment.vcsPedFalls.count(handle)||!support.supported) {
            result={from.x,from.y,from.z};return true;
        }
    }
    auto& combat=runtime.missionCombat[handle];
    auto& state=combat.contact;
    if(!character.collisionEnabled) {
        state.clear();
        if(world.paths.vcs) {
            result=character.disableVerticalMovement?desired:std::array<float,3>{from.x,from.y,from.z};
            return true;
        }
        const auto moved=storylandMovePlayTestCharacter(world,runtime,from,destination,-1,true);
        if(!moved.clear||!std::isfinite(moved.position.x)||!std::isfinite(moved.position.y)||
            !std::isfinite(moved.position.z))return false;
        result={moved.position.x,moved.position.y,moved.position.z};
        return true;
    }
    const float step=std::min(.1f,float(environment.nativeFrameMilliseconds)*.001f);
    StorylandNativeMoveResult moved;
    if(!storylandMissionPedContactMovable(runtime,handle,character)) {
        state.clear();
        if(!storylandMissionPedContactBody(character)||environment.cutscene.playing||
            character.networkControlled||character.positionFrozen||character.stayInSamePlace) {
            result={from.x,from.y,from.z};
            return true;
        }
        moved=storylandMovePlayTestCharacter(world,runtime,from,destination,INT32_MAX,false,true,false);
    } else {
        const auto obstacles=storylandPedContactObstacles(runtime);
        StorylandPedContactGoal seekGoal;
        const StorylandPedContactGoal* goal=nullptr;
        if(world.paths.vcs&&character.objective==0x0119&&character.walkingToTarget&&
            character.leader>=0&&character.objectiveTarget==character.leader) {
            seekGoal.position={character.targetX,character.targetY,character.objectiveHeight};
            seekGoal.radius=character.targetRadius;
            goal=&seekGoal;
        }
        moved=storylandMovePedContact(runtime,world,UINT64_C(0x100000000)|uint32_t(handle),state,
            from,destination,character.heading+90,step,obstacles,
            storylandMissionPedContactAnimationAllowed(combat),
            storylandMissionPedContactMoveState(character),character.creationSource==2,&character,goal);
        storylandRecordPedContactFrame(state,std::hypot(destination.x-from.x,destination.y-from.y),
            std::hypot(moved.position.x-from.x,moved.position.y-from.y),step,environment.nativeFrameCounter);
        if(state.blockedSeconds>.8f) {
            character.seekRoute.clear();
            character.seekRouteCursor=0;
            character.seekNode=UINT32_MAX;
        }
    }
    if(!moved.clear||!std::isfinite(moved.position.x)||!std::isfinite(moved.position.y)||
        !std::isfinite(moved.position.z))return false;
    result={moved.position.x,moved.position.y,moved.position.z};
    return true;
}

static void storylandResolvePedestrianContacts(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,std::vector<StorylandNativeTrafficActor>& pedestrians,float seconds,
    std::vector<StorylandPedContactObstacle>& obstacles) {
    if(runtime.missionEnvironment&&runtime.missionEnvironment->cutscene.playing)return;
    const float correction=std::clamp(seconds,0.0f,.1f)*1.25f;
    const auto resolve=[&](uint64_t identity,StorylandPedContactState& state,
        StorylandNativePoint position,float heading,bool animationAllowed,uint8_t movement,
        bool missionCharacter,const StorylandMissionEnvironment::Character* character) {
        if(!animationAllowed)storylandCancelPedContactAnimation(state);
        const auto plan=storylandPlanPedContact(identity,state,position,position,obstacles,seconds,
            .31f,nullptr,missionCharacter);
        if(animationAllowed)storylandStartPedContactAnimation(runtime,state,plan,heading,movement);
        if(state.animation>=0&&state.animationMotionTime<state.animationTime) {
            const auto moved=storylandMovePedContact(runtime,world,identity,state,position,position,
                heading,seconds,obstacles,animationAllowed,movement,missionCharacter,character);
            if(moved.clear)position=moved.position;
        }
        const auto target=storylandSeparatePedContact(identity,position,obstacles,correction);
        if(std::hypot(target.x-position.x,target.y-position.y)<.00001f)return position;
        const auto moved=storylandSweepPedContact(world,runtime,position,target,character);
        return moved.clear?moved.position:position;
    };
    for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size();++index) {
        auto& ped=runtime.pedestrians[index];
        if(!storylandAmbientPedContactMovable(runtime,index)) {
            ped.contact.clear();
            continue;
        }
        pedestrians[index].position=resolve(uint64_t(index)+1,ped.contact,pedestrians[index].position,
            pedestrians[index].yaw,storylandAmbientPedContactAnimationAllowed(ped),
            storylandAmbientPedContactMoveState(ped),false,nullptr);
        if(index<runtime.movementPedestrians.size())runtime.movementPedestrians[index]=pedestrians[index].position;
        storylandPublishPedContact(obstacles,uint64_t(index)+1,pedestrians[index].position,ped.contact);
    }
    if(runtime.missionEnvironment)for(auto& item:runtime.missionEnvironment->missionCharacters) {
        auto& character=item.second;
        auto& combat=runtime.missionCombat[item.first];
        auto& state=combat.contact;
        if(!storylandMissionPedContactMovable(runtime,item.first,character)) {
            state.clear();
            continue;
        }
        const auto position=resolve(UINT64_C(0x100000000)|uint32_t(item.first),state,
            {character.x,character.y,character.z},character.heading+90,
            storylandMissionPedContactAnimationAllowed(combat),
            storylandMissionPedContactMoveState(character),character.creationSource==2,&character);
        character.x=position.x;character.y=position.y;character.z=position.z;
        storylandPublishPedContact(obstacles,UINT64_C(0x100000000)|uint32_t(item.first),position,state);
    }
}

static void refreshMovementCars(StorylandPlayTestRuntime& runtime,const std::vector<StorylandNativeTrafficActor>& cars) {
    runtime.movementCars.clear();
    for(size_t i=0;i<cars.size()&&i<runtime.vehicles.size();++i)
        runtime.movementCars.push_back({cars[i].position,cars[i].yaw,runtime.vehicles[i].halfLength,
            runtime.vehicles[i].halfWidth,runtime.vehicles[i].bodyHeight,!runtime.vehicles[i].craneCarried});
}
#include "src/re/reVCS/vehicles/storyland_vcs_jetski_runtime.inl"

static bool updateDetachedVehicleExit(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    StorylandNativePoint& player,float heading,float seconds,
    const std::vector<StorylandNativeTrafficActor>& cars) {
    if(runtime.playerJetSkiFallActive)
        return updateVcsJetSkiPlayerFall(runtime,world,player,heading,seconds);
    auto& pose=runtime.vehicleExitPose;
    if(!pose.active)return false;
    float waterSurface=0;
    if((pose.nativeBailout||(runtime.crime.vcs&&pose.animation==139))&&
        deepCharacterWater(world,player,waterSurface)) {
        runtime.jumpHorizontalVelocity={pose.velocity[0],pose.velocity[1]};
        runtime.jumpHorizontalValid=true;
        runtime.verticalSpeed=pose.velocity[2];
        pose={};return false;
    }
    if(runtime.missionEnvironment&&(runtime.missionEnvironment->cutscene.playing||
        runtime.missionEnvironment->playerWarped||runtime.missionEnvironment->activeVcsCharacterAttachment(0))) {
        pose={};return false;
    }
    if(!std::isfinite(seconds)||seconds<=0)return true;
    const float step=std::min(seconds,.1f);
    const uint32_t now=runtime.missionEnvironment?runtime.missionEnvironment->gameMilliseconds:
        uint32_t(std::max(0.0f,runtime.elapsed)*1000.0f);
    if(storylandVehicleExitCanGetUp(pose,now,runtime.grounded,runtime.health>0)) {
        const bool clear=cars.empty()?storylandNativeBodyClear(world,player,storylandNativeCharacterBody()):
            storylandVehicleExitPositionClear(runtime,world,cars,0,player,true);
        if(clear) {
            pose.recovery=StorylandVehicleExitRecovery::GetUp;
            pose.animation=pose.getUpAnimation;pose.group=0;
            pose.elapsed=0;pose.duration=pose.getUpDuration;
            pose.velocity={};pose.sourceVehicle=-1;
        }
    }
    const float duration=std::max(0.0f,pose.duration);
    const float elapsed=std::min(duration,pose.elapsed+step);
    const auto authored=runtime.vehicleExitRootDelta?
        runtime.vehicleExitRootDelta(pose.animation,pose.group,pose.elapsed,elapsed):StorylandNativePoint{};
    const float angle=heading*radians,c=std::cos(angle),sine=std::sin(angle);
    StorylandNativePoint horizontal{pose.velocity[0]*step,pose.velocity[1]*step,0};
    if(pose.nativeBailout) {
        // Both native UpdatePosition functions replace supported velocity with
        // the association's motion. Airborne peds retain inherited velocity.
        if(runtime.grounded) {
            horizontal={c*authored.x+sine*authored.y,sine*authored.x-c*authored.y,0};
            pose.velocity[0]=horizontal.x/step;pose.velocity[1]=horizontal.y/step;
        }
    } else {
        horizontal.x+=c*authored.x+sine*authored.y;
        horizontal.y+=sine*authored.x-c*authored.y;
    }
    const bool wasGrounded=runtime.grounded;
    const float previousVerticalSpeed=pose.velocity[2];
    const auto vertical=storylandPedVerticalStep(previousVerticalSpeed,runtime.gravity,step);
    StorylandNativePoint target{player.x+horizontal.x,player.y+horizontal.y,
        player.z+vertical.displacement};
    pose.velocity[2]=vertical.velocity;
    float floor=0;
    uint8_t surface=runtime.groundSurface;
    bool hasFloor=world.collisionModels.empty();
    if(!hasFloor) {
        const float reference=std::max(player.z,target.z);
        const StorylandNativePoint top{target.x,target.y,reference+.85f};
        const StorylandNativePoint bottom{target.x,target.y,reference-5.0f};
        const float fraction=storylandTraceNativeGround(world,top,bottom,surface);
        if(fraction<1.0f) {
            floor=top.z+(bottom.z-top.z)*fraction;
            hasFloor=std::isfinite(floor);
        }
    }
    const bool landed=hasFloor&&target.z<=floor+.005f&&pose.velocity[2]<=0;
    if(landed) {target.z=floor;pose.velocity[2]=0;}
    const auto before=player;
    const auto moved=storylandMovePlayTestCharacter(world,runtime,player,target,-1,false,false);
    if(moved.clear)player=moved.position;
    if(moved.blocked&&std::hypot(player.x-before.x,player.y-before.y)<
        .15f*std::hypot(target.x-before.x,target.y-before.y)) {
        pose.velocity[0]=0;pose.velocity[1]=0;
    }
    runtime.grounded=landed&&std::fabs(player.z-floor)<.05f;
    if(runtime.grounded) {
        runtime.groundSurface=surface;
        if(pose.nativeBailout&&world.paths.vcs&&!wasGrounded) {
            const float contactSeconds=storylandVcsFallContactSeconds(
                before.z-floor,previousVerticalSpeed,runtime.gravity,step);
            const float impactVelocity=previousVerticalSpeed-runtime.gravity*contactSeconds;
            const float impact=storylandVcsFallImpactDamage(
                std::hypot(pose.velocity[0],pose.velocity[1]),impactVelocity,surface);
            const uint8_t direction=std::fabs(pose.velocity[0])>.5f||std::fabs(pose.velocity[1])>.5f?
                storylandNativeHitDirection(heading,-pose.velocity[0],-pose.velocity[1]):2;
            if(impact>0)runtime.inflictDamage(impact,8,storylandNativeFallWeapon(true),false,0,direction);
        }
    }
    runtime.verticalSpeed=pose.velocity[2];
    runtime.movementSpeed=step>0?std::hypot(player.x-before.x,player.y-before.y)/step:0;
    runtime.playerPosition=player;
    if(runtime.grounded)runtime.airborneTime=0;
    else runtime.airborneTime+=step;
    pose.elapsed=elapsed;
    if(elapsed>=duration&&pose.recovery!=StorylandVehicleExitRecovery::Fallen) {
        runtime.jumpHorizontalVelocity={pose.velocity[0],pose.velocity[1]};
        runtime.jumpHorizontalValid=!runtime.grounded;
        runtime.jumpFromLaunch=false;
        pose={};
    }
    return true;
}
static void ejectBikeRider(StorylandPlayTestRuntime& runtime,StorylandNativePoint& player,float& heading,
    std::vector<StorylandNativeTrafficActor>& cars,size_t slot,const StorylandBikeFallDecision& decision) {
    auto& bike=runtime.vehicles[slot];
    const auto& actor=cars[slot];
    const float angle=actor.yaw*radians,c=std::cos(angle),s=std::sin(angle);
    auto& pose=runtime.vehicleExitPose;
    pose.animation=decision.animation>=0?decision.animation:decision.right?121:120;
    pose.group=0;pose.sourceVehicle=int32_t(slot);
    pose.elapsed=0;
    const float duration=runtime.reactionBindings.duration?runtime.reactionBindings.duration(pose.animation,pose.group):0;
    if(decision.nativeAngularFall&&(!std::isfinite(duration)||duration<=0)) {
        runtime.vehiclePhysicsError="Native VCS bike angular fall: STD30 has no decoded duration.";
        return;
    }
    pose.duration=std::isfinite(duration)&&duration>0?duration:1.0f;
    pose.velocity={c*bike.speed-s*bike.lateralSpeed,s*bike.speed+c*bike.lateralSpeed,
        std::max(2.0f,bike.verticalSpeed)};
    if(decision.nativeWaterFall) {
        pose.velocity={pose.velocity[0]*.2f,pose.velocity[1]*.2f,0};
    } else if(decision.nativeAngularFall) {
        storyland_vcs_jetski_knockoff::Input input;
        input.pedestrianMass=runtime.playerMass;
        input.vehicleMass=bike.mass;
        input.nativeNormalImpulse=bike.vcsJetSkiContactImpulse;
        input.contactNormal=bike.vcsJetSkiContactNormal;
        input.nativeVehicleVelocity={pose.velocity[0]/StorylandNativePhysicsRate,
            pose.velocity[1]/StorylandNativePhysicsRate,
            vehicleVerticalVelocity(bike)/StorylandNativePhysicsRate};
        const uint32_t draw=runtime.missionEnvironment?runtime.missionEnvironment->nextNativeRandom():
            storylandNativeRandom(runtime.populationRandom);
        input.nativeRandom=float(draw)/2147483648.0f;
        storyland_vcs_jetski_knockoff::Output impulse;
        if(!storyland_vcs_jetski_knockoff::force(input,impulse)) {
            runtime.vehiclePhysicsError="Native VCS bike angular fall: cause47 contact force is invalid.";
            return;
        }
        for(size_t axis=0;axis<3;++axis)
            pose.velocity[axis]=impulse.nativeVelocity[axis]*StorylandNativePhysicsRate;
    } else if(decision.animation>=0) {
        const auto& definition=bike.nativeDefinition;
        const auto frame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,{0,0,0},
            actor.yaw,definition.previewMinimumZ,bike.suspension,bike.bodyRoll-bike.roadRoll,
            bike.bodyPitch+bike.roadPitch),definition.previewRoot);
        const float forceScale=StorylandNativePhysicsRate/std::max(1.0f,runtime.playerMass);
        const float velocity[]={pose.velocity[0],pose.velocity[1],bike.verticalSpeed};
        for(size_t axis=0;axis<3;++axis) {
            pose.velocity[axis]=velocity[axis]*decision.inheritVelocity+forceScale*
                (frame[axis]*decision.sideForce+frame[4+axis]*decision.forwardForce+
                frame[8+axis]*decision.upForce);
        }
        if(decision.inheritVelocity==0)pose.velocity[2]+=.1f*StorylandNativePhysicsRate;
    }
    pose.active=true;
    player=actor.position;player.z+=0.5f;heading=actor.yaw;
    const bool passenger=runtime.playerVehicleSeat>=0;
    scriptPlayerLeavesVehicle(runtime,bike);
    runtime.occupiedVehicle=-1;runtime.playerVehicleSeat=-1;
    runtime.vehicleAction=StorylandVehicleAction::None;runtime.transitionVehicle=-1;
    runtime.vehicleTransitionPrepared=false;runtime.vehicleEntryVariant=StorylandVehicleEntryVariant::Normal;
    if(!passenger) {bike.hasDriver=false;bike.missionDriver=false;bike.parked=false;}
    runtime.grounded=false;runtime.verticalSpeed=pose.velocity[2];runtime.airborneTime=0;
    runtime.jumpLaunching=false;runtime.jumpFromLaunch=false;runtime.jumpHorizontalValid=false;
    runtime.playerPosition=player;
    if(runtime.missionEnvironment)runtime.missionEnvironment->playerVehicleTransition=false;
    if(decision.damage>0)runtime.inflictDamage(decision.damage,8,
        runtime.crime.vcs?42:39,false);
    refreshMovementCars(runtime,cars);
}
static bool updateVcsHelicopterBailout(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    StorylandNativePoint& player,float& heading,std::vector<StorylandNativeTrafficActor>& cars,size_t target,float dt) {
    using Phase=StorylandVehiclePhase;
    auto& vehicle=runtime.vehicles[target];const auto& car=cars[target];
    auto* environment=runtime.missionEnvironment;
    const auto fail=[&](const std::string& reason) {
        runtime.vehiclePhysicsError="Native VCS helicopter bailout: "+reason;
        return false;
    };
    float originOffset=0;
    if(!environment||!environment->characterOriginOffset||
        !environment->characterOriginOffset(0,originOffset)||!std::isfinite(originOffset)||
        !environment->characterBoneMatrix)return fail("original player origin/bone binding is unavailable.");
    if(!runtime.vehicleTransitionPrepared) {
        std::string error;
        if(!environment->ensureAnimationGroup||
            !environment->ensureAnimationGroup(StorylandVcsHelicopterBailoutGroup,error))
            return fail(error.empty()?"original heli animation bank is unavailable.":error);
        const float duration=environment->animationDuration?
            environment->animationDuration(StorylandVcsHelicopterBailoutGroup,StorylandVcsHelicopterBailoutAnimation):0;
        if(!std::isfinite(duration)||duration<=0||duration>=10)
            return fail("original heli_bail association has no valid duration.");
        vehicle.nativeDefinition.movingExitSeconds={duration,duration};
        vehicle.nativeDefinition.movingExitAssociations={StorylandVcsHelicopterBailoutAnimation,
            StorylandVcsHelicopterBailoutAnimation};
        vehicle.nativeDefinition.movingExitGroups={StorylandVcsHelicopterBailoutGroup,StorylandVcsHelicopterBailoutGroup};
        runtime.vehicleTiming.seconds.fill(0);
        runtime.vehicleTiming.seconds[unsigned(Phase::GetOut)]=duration;
        runtime.vehicleActionDuration=duration;runtime.vehicleActionTime=0;
        runtime.vehicleExitRight=false;runtime.vehicleExitRear=false;runtime.vehicleApproach.clear();
        runtime.vehicleTransitionPrepared=true;
        vehicle.parked=runtime.vehicleWasParked;
    }
    StorylandVehicleMatrix frame;
    if(!storylandVcsHelicopterBailoutFrame(vehicle.nativeDefinition,
        {car.position.x,car.position.y,car.position.z},car.yaw,vehicle.suspension,
        vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,frame))
        return fail("original vehicle frame or front-seat dummy is unavailable.");
    runtime.vehicleActionTime+=dt;
    runtime.vehiclePhase=storylandSampleVehicleTransition(runtime.vehicleAction,runtime.vehicleTiming,
        runtime.vehicleActionTime);
    player={frame[12],frame[13],frame[14]-originOffset};
    heading=std::atan2(frame[1],frame[0])/radians+90.0f;
    runtime.playerPosition=player;
    environment->x=player.x;environment->y=player.y;environment->z=player.z;environment->heading=heading-90;
    runtime.movementSpeed=0;runtime.playerMoveBlend=0;runtime.playerSprintBlend=0;
    storylandResetPlayerSprint(runtime);
    StorylandVehicleMatrix bone;
    if(!environment->characterBoneMatrix(0,StorylandVcsHelicopterBailoutCompletionBone,bone)||
        !std::all_of(bone.begin(),bone.end(),[](float value){return std::isfinite(value);}))
        return fail("actual heli_bail spine pose is unavailable.");
    if(runtime.vehiclePhase.phase!=Phase::Done)return true;

    // 2FE298/406EF8: the type-4 finish retains entity Z, except for water
    // below an unsupported exit. 219DD8 then takes bone-tag-3 world X/Y.
    std::array<float,2> ground{-100.0f,-100.0f};
    for(size_t index=0;index<ground.size();++index) {
        const float offset=index==0?0.0f:.1f;
        const StorylandNativePoint from{frame[12]+offset,frame[13]+offset,frame[14]+1};
        const StorylandNativePoint to{from.x,from.y,frame[14]-100};
        uint8_t surface=0;
        const float fraction=storylandTraceNativeGround(world,from,to,surface);
        if(std::isfinite(fraction)&&fraction>=0&&fraction<1)
            ground[index]=from.z+(to.z-from.z)*fraction;
    }
    const bool supported=storylandVcsHelicopterBailoutGroundAvailable(ground[0],ground[1]);
    float entityZ=frame[14],water=0;
    if(!supported&&storylandNativeWaterHeight(world,frame[12],frame[13],water)&&
        storylandVcsWaterDepthAllowsContact(frame[14],water)&&entityZ<water)entityZ=water-1;
    player={bone[12],bone[13],entityZ-originOffset};runtime.playerPosition=player;
    const bool passenger=runtime.playerVehicleSeat>=0;
    scriptPlayerLeavesVehicle(runtime,vehicle);
    runtime.occupiedVehicle=-1;runtime.playerVehicleSeat=-1;
    runtime.vehicleAction=StorylandVehicleAction::None;runtime.transitionVehicle=-1;
    runtime.vehicleTransitionPrepared=false;runtime.vehicleEntryVariant=StorylandVehicleEntryVariant::Normal;
    runtime.vehicleExitPose={};runtime.grounded=false;runtime.verticalSpeed=0;runtime.airborneTime=0;
    runtime.jumpLaunching=false;runtime.jumpFromLaunch=false;runtime.jumpHorizontalValid=false;
    runtime.jumpHorizontalVelocity={};runtime.jumpLaunchTime=0;
    if(!passenger) {vehicle.hasDriver=false;vehicle.missionDriver=false;vehicle.parked=false;}
    environment->playerVehicleTransition=false;
    environment->x=player.x;environment->y=player.y;environment->z=player.z;
    refreshMovementCars(runtime,cars);
    return true;
}

enum class StorylandVcsRoomSupport { Ground,RequiredWater,WaterOrGround,NativeWaterOutput };

static bool storylandVcsVehicleRoomClear(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t target,
    const StorylandVcsVehicleRoomQuery& query,StorylandVcsRoomSupport support) {
    if(target>=cars.size()||target>=runtime.vehicles.size())return false;
    const auto point=[](const std::array<float,3>& value) {
        return StorylandNativePoint{value[0],value[1],value[2]};
    };
    const auto trace=[&](StorylandNativePoint start,StorylandNativePoint end) {
        float fraction=storylandTraceNativeSectorSegment(world,start,end,0x7Bu);
        if(runtime.missionEnvironment)for(const auto& object:runtime.missionEnvironment->objects) {
            if(!object.collisionEnabled||object.area!=runtime.missionEnvironment->area||object.model<0)continue;
            const auto found=world.collisionModels.find(uint32_t(object.model));
            if(found==world.collisionModels.end()||!world.collisionModelEnabled(found->second))continue;
            StorylandNativePoint normal;
            storylandTraceCollisionModel(found->second,storylandScriptObjectMatrix(object.x,object.y,object.z,
                object.rotationX,object.rotationY,object.heading),start,end,fraction,normal);
        }
        return fraction;
    };
    if(trace(point(query.lineStart),point(query.lineEnd))<1)return false;
    const auto center=point(query.sphereCenter);
    bool hit=false;
    if(!storylandNativeWorldSphereContact(world,center,query.radius,0x7Bu,true,hit,
        runtime.vehicles[target].scriptHandle)||hit)return false;
    for(size_t index=0;index<cars.size()&&index<runtime.vehicles.size();++index) {
        const auto& vehicle=runtime.vehicles[index];
        if(index==target||!vehicle.collisionEnabled||vehicle.craneCarried)continue;
        const auto& actor=cars[index];
        const auto body=vehicleBody(vehicle,actor.yaw);
        if(body.collision) {
            auto matrix=body.collisionTransform;
            matrix[12]+=actor.position.x;matrix[13]+=actor.position.y;matrix[14]+=actor.position.z;
            if(!storylandNativeSphereModelContact(center,query.radius,*body.collision,matrix,hit)||hit)return false;
        } else {
            StorylandNativeCollisionModel bounds;
            bounds.minimum={-body.halfLength,-body.halfWidth,body.lower};
            bounds.maximum={body.halfLength,body.halfWidth,body.height};
            bounds.boxes.push_back({bounds.minimum,bounds.maximum,0,0});
            const auto matrix=storylandScriptObjectMatrix(actor.position.x,actor.position.y,actor.position.z,
                body.pitch,body.roll,actor.yaw);
            if(!storylandNativeSphereModelContact(center,query.radius,bounds,matrix,hit)||hit)return false;
        }
    }
    if(runtime.missionEnvironment)for(const auto& item:runtime.missionEnvironment->missionVehicles) {
        const auto& vehicle=item.second;
        if(item.first==runtime.vehicles[target].scriptHandle||vehicle.deleted||!vehicle.collisionEnabled||
            vehicle.spawnedSlot>=0)continue;
        const auto found=world.collisionModels.find(uint32_t(vehicle.model));
        if(found==world.collisionModels.end())continue;
        const auto matrix=storylandScriptObjectMatrix(vehicle.x,vehicle.y,vehicle.z,0,0,vehicle.heading);
        if(!storylandNativeSphereModelContact(center,query.radius,found->second,matrix,hit)||hit)return false;
    }
    const StorylandNativePoint top{center.x,center.y,1000};
    const float ceilingFraction=trace(center,top);
    const float ceiling=ceilingFraction<1?center.z+(top.z-center.z)*ceilingFraction:0;
    if(ceilingFraction<1&&center.z<ceiling&&ceiling<center.z+query.radius)return false;
    // Original 26EE50 initializes height to 6; the helicopter strong-contact
    // branch at 14EBC8 consumes that output without testing the return value.
    float ground=support==StorylandVcsRoomSupport::NativeWaterOutput?6.0f:0.0f;
    const bool water=support!=StorylandVcsRoomSupport::Ground&&
        storylandNativeWaterHeight(world,center.x,center.y,ground);
    if(support==StorylandVcsRoomSupport::RequiredWater&&!water)return false;
    if(support==StorylandVcsRoomSupport::Ground||(support==StorylandVcsRoomSupport::WaterOrGround&&!water)) {
        const StorylandNativePoint bottom{center.x,center.y,-1000};
        const float floorFraction=trace(center,bottom);
        if(floorFraction>=1)return false;
        ground=center.z+(bottom.z-center.z)*floorFraction;
    }
    return storylandVcsVehicleRoomSupport(ceiling,ground);
}

bool storylandVcsJetSkiRoomClear(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t target,
    const StorylandVcsJetSkiRoomQuery& query,bool strongWaterContact) {
    return storylandVcsVehicleRoomClear(runtime,world,cars,target,query,strongWaterContact?
        StorylandVcsRoomSupport::RequiredWater:StorylandVcsRoomSupport::Ground);
}

bool storylandVcsAutomobileRoomClear(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t target,
    const StorylandVcsVehicleRoomQuery& query) {
    if(target>=runtime.vehicles.size())return false;
    const auto& vehicle=runtime.vehicles[target];
    const auto& native=vehicle.nativeDefinition;
    if(!native.vcsModelLayout||(native.nativeType!=0&&!storylandVcsHelicopter(native)))return false;
    const bool amphibious=native.hasVcsModelInfoFlags&&(native.vcsModelInfoFlags&0x40u);
    const auto support=amphibious?StorylandVcsRoomSupport::WaterOrGround:
        vehicle.water.buoyant?StorylandVcsRoomSupport::NativeWaterOutput:StorylandVcsRoomSupport::Ground;
    return storylandVcsVehicleRoomClear(runtime,world,cars,target,query,support);
}

bool storylandVcsHelicopterRoomClear(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t target,
    const StorylandVcsVehicleRoomQuery& query) {
    return target<runtime.vehicles.size()&&storylandVcsHelicopter(runtime.vehicles[target].nativeDefinition)&&
        storylandVcsAutomobileRoomClear(runtime,world,cars,target,query);
}

bool storylandAutomobileRoomClear(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t target,
    const StorylandVcsVehicleRoomQuery& query) {
    if(target>=runtime.vehicles.size())return false;
    const auto& native=runtime.vehicles[target].nativeDefinition;
    if(native.vcsModelLayout)return storylandVcsAutomobileRoomClear(runtime,world,cars,target,query);
    return native.nativeType==0&&storylandVcsVehicleRoomClear(runtime,world,cars,target,query,
        StorylandVcsRoomSupport::Ground);
}

bool storylandVehicleExitDoorReserved(const StorylandPlayTestRuntime& runtime,size_t target,
    unsigned nativeSeat,bool rightHandSide,int32_t character) {
    if(target>=runtime.vehicles.size())return true;
    const unsigned door=storylandVehicleSeatDoor(nativeSeat,rightHandSide);
    if(character!=0&&runtime.transitionVehicle==int(target)&&runtime.vehicleTransitionPrepared&&
        runtime.vehicleAction!=StorylandVehicleAction::None&&
        storylandVehicleSeatDoor(runtime.playerVehicleSeat<0?0u:unsigned(runtime.playerVehicleSeat+1),
            runtime.vehicleExitRight)==door)return true;
    if(!runtime.missionEnvironment)return false;
    for(const auto& entry:runtime.missionEnvironment->characterVehicleTasks) {
        const auto& task=entry.second;
        if(entry.first==character||task.vehicle!=runtime.vehicles[target].scriptHandle||!task.animating)continue;
        if(storylandVehicleSeatDoor(task.seat<0?0u:unsigned(task.seat+1),task.rightHandSide)==door)return true;
    }
    return false;
}

bool storylandVehicleExitSeatOccupied(const StorylandPlayTestRuntime& runtime,size_t target,
    unsigned nativeSeat,int32_t character) {
    if(target>=runtime.vehicles.size())return true;
    const auto& vehicle=runtime.vehicles[target];
    if(character!=0&&runtime.occupiedVehicle==int(target)&&
        (runtime.playerVehicleSeat<0?0u:unsigned(runtime.playerVehicleSeat+1))==nativeSeat)return true;
    if(runtime.missionEnvironment)for(const auto& entry:runtime.missionEnvironment->missionCharacters) {
        const auto& ped=entry.second;
        if(entry.first!=character&&ped.alive&&ped.vehicle==vehicle.scriptHandle&&
            (ped.passengerSeat<0?0u:unsigned(ped.passengerSeat+1))==nativeSeat)return true;
    }
    return nativeSeat==0&&vehicle.hasDriver;
}

static bool vehicleExitPositionClear(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t target,const StorylandNativePoint& position,
    bool allowMotorcycleOverlap,bool ignoreSourceVehicle) {
    if(target>=runtime.vehicles.size()||target>=cars.size()||
        !storylandNativeBodyClear(world,position,storylandNativeCharacterBody()))return false;
    for(size_t index=0;index<cars.size()&&index<runtime.vehicles.size();++index) {
        const auto& vehicle=runtime.vehicles[index];
        if(ignoreSourceVehicle&&index==target)continue;
        if(allowMotorcycleOverlap&&vehicle.nativeDefinition.motorcycle())continue;
        if(!vehicle.collisionEnabled||vehicle.craneCarried)continue;
        const auto& actor=cars[index];
        if(position.z>=actor.position.z+vehicle.bodyHeight-.02f||
            position.z+1.8f<actor.position.z+.2f)continue;
        if(vehicle.nativeDefinition.collision) {
            const auto body=vehicleBody(vehicle,actor.yaw);
            auto matrix=body.collisionTransform;
            matrix[12]+=actor.position.x;matrix[13]+=actor.position.y;matrix[14]+=actor.position.z;
            if(!storylandNativeCharacterModelClear(position,*body.collision,matrix))return false;
        } else {
            const float cosine=std::cos(actor.yaw*radians),sine=std::sin(actor.yaw*radians);
            const float dx=position.x-actor.position.x,dy=position.y-actor.position.y;
            if(std::fabs(dx*cosine+dy*sine)<vehicle.halfLength+.33f&&
                std::fabs(-dx*sine+dy*cosine)<vehicle.halfWidth+.33f)return false;
        }
    }
    return true;
}

bool storylandVehicleExitPositionClear(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t target,const StorylandNativePoint& position,
    bool allowMotorcycleOverlap) {
    return vehicleExitPositionClear(runtime,world,cars,target,position,allowMotorcycleOverlap,false);
}

bool storylandVehicleExitDestinationClear(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t target,const StorylandNativePoint& position) {
    return vehicleExitPositionClear(runtime,world,cars,target,position,false,true);
}

bool storylandFindVehicleExitPosition(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,size_t target,bool rightHandSide,
    const StorylandNativePoint& originalDoor,StorylandNativePoint& result) {
    if(target>=cars.size()||target>=runtime.vehicles.size())return false;
    const auto& actor=cars[target];const auto& vehicle=runtime.vehicles[target];
    const auto& native=vehicle.nativeDefinition;
    auto frame=vehicleBody(vehicle,actor.yaw).collisionTransform;
    frame[12]+=actor.position.x;frame[13]+=actor.position.y;frame[14]+=actor.position.z;
    const auto accept=[&](StorylandNativePoint point,bool roof=false) {
        float ground=0;
        if(!roof&&vehicleGroundHeight(world,point.x,point.y,actor.position.z,ground)&&
            std::fabs(ground-actor.position.z)<1.5f)point.z=ground;
        if(!storylandVehicleExitDestinationClear(runtime,world,cars,target,point))return false;
        const StorylandNativePoint start{actor.position.x,actor.position.y,actor.position.z+.65f};
        const StorylandNativePoint end{point.x,point.y,point.z+.65f};
        if(storylandTraceNativeSectorSegment(world,start,end,0x7Bu)<1)return false;
        result=point;return true;
    };
    if(accept(originalDoor))return true;
    const auto minimum=native.collision?native.collision->minimum:
        StorylandNativePoint{-vehicle.halfWidth,-vehicle.halfLength,0};
    const auto maximum=native.collision?native.collision->maximum:
        StorylandNativePoint{vehicle.halfWidth,vehicle.halfLength,vehicle.bodyHeight};
    const auto local=[&](float x,float y,float z) {
        if(native.collision) {
            const auto point=storylandVehicleTransformPoint(frame,{x,y,z});
            return StorylandNativePoint{point[0],point[1],point[2]};
        }
        const float cosine=std::cos(actor.yaw*radians),sine=std::sin(actor.yaw*radians);
        return StorylandNativePoint{actor.position.x+cosine*y+sine*x,
            actor.position.y+sine*y-cosine*x,actor.position.z+z};
    };
    const float side=rightHandSide?maximum.x+.355f:minimum.x-.355f;
    for(unsigned section=0;section<4;++section)
        if(accept(local(side,minimum.y+(maximum.y-minimum.y)*float(section)/3,0)))return true;
    if(accept(local(0,minimum.y-.355f,0))||accept(local(0,maximum.y+.355f,0)))return true;
    if(accept(local(-side,minimum.y,0))||accept(local(-side,maximum.y,0)))return true;
    if(native.nativeType==0) {
        auto roof=local(0,0,maximum.z);
        roof.z+=.02f;
        if(accept(roof,true))return true;
    }
    return false;
}

static bool exitVcsOrdinaryBoat(StorylandPlayTestRuntime& runtime,StorylandNativePoint& player,
    float& heading,const std::vector<StorylandNativeTrafficActor>& cars,size_t target) {
    const auto& car=cars[target];auto& vehicle=runtime.vehicles[target];
    const auto& definition=vehicle.nativeDefinition;
    float originOffset=0;
    if(!runtime.missionEnvironment||!runtime.missionEnvironment->characterOriginOffset||
        !runtime.missionEnvironment->characterOriginOffset(0,originOffset)||!std::isfinite(originOffset)||
        !storylandVcsBoatEntryAvailable(definition)) {
        runtime.vehicleTransitionError="VCS boat exit requires the original player origin, seat and collision model.";
        vehicle.parked=runtime.vehicleWasParked;
        runtime.vehicleAction=StorylandVehicleAction::None;runtime.transitionVehicle=-1;
        runtime.vehicleTransitionPrepared=false;
        if(runtime.missionEnvironment)runtime.missionEnvironment->playerVehicleTransition=false;
        return false;
    }
    auto frame=vehicleBody(vehicle,car.yaw).collisionTransform;
    frame[12]+=car.position.x;frame[13]+=car.position.y;frame[14]+=car.position.z;
    const auto seat=storylandVehicleTransformPoint(frame,definition.dummyPositions[0]);
    auto origin=storylandVcsBoatExitOrigin(frame,seat,definition.collision->minimum.z);
    if(frame[10]>-.9f) {
        const StorylandNativePoint start{origin[0],origin[1],origin[2]},end{origin[0],origin[1],origin[2]-2.4f};
        float fraction=1;StorylandNativePoint normal;
        for(size_t index=0;index<cars.size()&&index<runtime.vehicles.size();++index) {
            const auto& other=runtime.vehicles[index];
            if(!other.collisionEnabled||other.craneCarried||!other.nativeDefinition.collision)continue;
            auto otherFrame=vehicleBody(other,cars[index].yaw).collisionTransform;
            otherFrame[12]+=cars[index].position.x;otherFrame[13]+=cars[index].position.y;
            otherFrame[14]+=cars[index].position.z;
            storylandTraceCollisionModel(*other.nativeDefinition.collision,otherFrame,start,end,fraction,normal);
        }
        if(fraction<1)origin[2]=start.z+(end.z-start.z)*fraction+1.04f;
    }
    const float cosine=std::cos(car.yaw*radians),sine=std::sin(car.yaw*radians);
    runtime.jumpHorizontalVelocity={cosine*vehicle.speed-sine*vehicle.lateralSpeed,
        sine*vehicle.speed+cosine*vehicle.lateralSpeed};
    runtime.jumpHorizontalValid=true;runtime.jumpLaunching=false;runtime.jumpFromLaunch=false;
    runtime.verticalSpeed=vehicle.verticalSpeed;runtime.grounded=true;runtime.airborneTime=0;
    player={origin[0],origin[1],origin[2]-originOffset};heading=car.yaw;
    scriptPlayerLeavesVehicle(runtime,vehicle);
    runtime.occupiedVehicle=-1;runtime.playerVehicleSeat=-1;
    vehicle.hasDriver=false;vehicle.missionDriver=false;vehicle.parked=false;
    runtime.vehicleAction=StorylandVehicleAction::None;runtime.transitionVehicle=-1;
    runtime.vehicleTransitionPrepared=false;runtime.vehiclePhase={};
    runtime.vehicleEntryVariant=StorylandVehicleEntryVariant::Normal;
    runtime.missionEnvironment->playerVehicleTransition=false;
    runtime.playerPosition=player;refreshMovementCars(runtime,cars);
    return true;
}

void storylandUpdatePlayTestVehicleDoor(StorylandPlayTestRuntime& runtime,size_t slot,
    const StorylandNativePoint& position,unsigned nativeSeat,bool rightHandSide,
    StorylandVehiclePhase phase,float seconds) {
    if(slot>=runtime.vehicles.size())return;
    auto& vehicle=runtime.vehicles[slot];
    const auto& definition=vehicle.nativeDefinition;
    if(definition.twoWheels()||definition.vcsJetSki()||storylandVcsOrdinaryBoat(definition)||
        (definition.vcsModelLayout&&definition.nativeType==9))return;
    const unsigned door=storylandVehicleSeatDoor(nativeSeat,rightHandSide);
    const bool rearVan=definition.vanVehicle()&&nativeSeat>=2;
    const auto update=[&](unsigned selected) {
        if(selected>=vehicle.doorOpenRatios.size()||selected+2>=vehicle.doorDamage.size())return;
        if(vehicle.doorDamage[selected+2]==3)return;
        auto& ratio=vehicle.doorOpenRatios[selected];
        const float previous=std::clamp(ratio,0.0f,1.0f);
        ratio=storylandVehicleDoorPhaseRatio(previous,phase,seconds,definition.busVehicle(),rearVan);
        const bool opened=previous==0&&ratio>0;
        const bool closed=previous>0&&ratio==0;
        if(!opened&&!closed)return;
        StorylandPlayTestSoundEvent sound(opened?StorylandPlayTestSound::VehicleDoorOpen:
            StorylandPlayTestSound::VehicleDoorClose);
        sound.vehicle=int32_t(slot);sound.position=position;sound.positioned=true;
        runtime.soundEvents.push_back(sound);
    };
    update(door);
    if(rearVan&&(phase==StorylandVehiclePhase::Open||phase==StorylandVehiclePhase::CloseInside))
        update(door^1u);
}

static bool updateRetailVehicleTransition(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    StorylandNativePoint& player,float& heading,std::vector<StorylandNativeTrafficActor>& cars,size_t target,float dt) {
    using A=StorylandVehicleAction;using P=StorylandVehiclePhase;
    auto& car=cars[target];auto& vehicle=runtime.vehicles[target];
    const bool ordinaryBoat=storylandVcsOrdinaryBoat(vehicle.nativeDefinition);
    if(ordinaryBoat&&runtime.vehicleAction==A::Exit)
        return exitVcsOrdinaryBoat(runtime,player,heading,cars,target);
    if(runtime.vehicleAction==A::Exit&&storylandVcsHelicopterBailout(vehicle.nativeDefinition,runtime.vehicleEntryVariant))
        return updateVcsHelicopterBailout(runtime,world,player,heading,cars,target,dt);
    const bool bike=vehicle.nativeDefinition.twoWheels();
    const bool quad=vehicle.nativeDefinition.vcsModelLayout&&vehicle.nativeDefinition.nativeType==9;
    const bool saddle=bike||quad;
    const bool jetSki=vehicle.nativeDefinition.vcsJetSki();
    const bool helicopterEntry=storylandVcsHelicopter(vehicle.nativeDefinition)&&runtime.vehicleAction!=A::Exit;
    const bool automobileDoor=vehicle.nativeDefinition.nativeType==0&&vehicle.nativeDefinition.hasNativeDoorRoots&&
        (vehicle.nativeDefinition.vcsModelLayout||runtime.vehicleAction==A::Exit);
    const bool nativeDoorTransition=helicopterEntry||automobileDoor;
    const bool motorcycleDoor=vehicle.nativeDefinition.motorcycle()&&
        vehicle.nativeDefinition.hasNativeMotorcycleEntryRoot;
    const bool motorcycleEntry=motorcycleDoor&&vehicle.nativeDefinition.vcsModelLayout&&runtime.vehicleAction!=A::Exit;
    const bool nativeSeek=ordinaryBoat||jetSki||nativeDoorTransition||motorcycleEntry;
    const auto seekReached=[&](float x,float y,float targetX,float targetY) {
        if(ordinaryBoat)return storylandVcsBoatSeekReached(x,y,targetX,targetY);
        if(motorcycleEntry)return std::hypot(x-targetX,y-targetY)<=.3f;
        return nativeDoorTransition?storylandVcsHelicopterSeekReached(x,y,targetX,targetY):
            storylandVcsJetSkiSeekReached(x,y,targetX,targetY);
    };
    const bool noDoors=saddle||jetSki||ordinaryBoat;
    const auto cancel=[&]() {
        storylandCancelPassengerSideCarjack(runtime, vehicle);
        vehicle.parked=runtime.vehicleWasParked;
        if(runtime.occupiedVehicle>=0)player=car.position;
        else {
            const auto recover=storylandMovePlayTestCharacter(world,runtime,player,player);
            if(recover.clear)player=recover.position;
        }
        runtime.vehicleAction=A::None;
        if(runtime.occupiedVehicle<0)runtime.playerVehicleSeat=-1;
        runtime.transitionVehicle=-1;runtime.vehicleTransitionPrepared=false;
        runtime.vehicleEntryVariant=StorylandVehicleEntryVariant::Normal;
        if(runtime.missionEnvironment)runtime.missionEnvironment->playerVehicleTransition=false;
    };
    const float c=std::cos(car.yaw*radians),s=std::sin(car.yaw*radians);
    const auto localPoint=[&](float along,float left) {
        return StorylandNativePoint{car.position.x+c*along-s*left,car.position.y+s*along+c*left,car.position.z};
    };
    if(ordinaryBoat&&runtime.vehicleTransitionPrepared) {
        auto frame=vehicleBody(vehicle,car.yaw).collisionTransform;
        frame[12]+=car.position.x;frame[13]+=car.position.y;frame[14]+=car.position.z;
        const auto goal=storylandVcsBoatSeekPosition(frame,vehicle.nativeDefinition.dummyPositions[0]);
        if(std::hypot(goal[0]-runtime.vehicleActionDoor.x,goal[1]-runtime.vehicleActionDoor.y)>.0001f)
            runtime.vehicleTransitionPrepared=false;
        if(scriptVehicleHasDriver(runtime,vehicle)) {cancel();return false;}
    }
    if(!runtime.vehicleTransitionPrepared) {
        runtime.vehicleTiming=vehicle.nativeDefinition.vehicleTimings();
        runtime.vehicleActionTime=0;runtime.vehicleApproach.clear();
        runtime.vehicleExitRight=false;runtime.vehicleExitRear=false;
        runtime.vehicleJetSkiWaterContact=jetSki&&vehicle.water.buoyant;
        runtime.vehicleJetSkiHighAlign=false;
        float playerOriginOffset=0;
        if((ordinaryBoat||jetSki||nativeDoorTransition||motorcycleDoor)&&(!runtime.missionEnvironment||!runtime.missionEnvironment->characterOriginOffset||
            !runtime.missionEnvironment->characterOriginOffset(vehicle.nativeDefinition.vcsModelLayout?0:1,playerOriginOffset)||
            !std::isfinite(playerOriginOffset))) {
            runtime.vehicleTransitionError="Native vehicle transition requires the live player collision origin.";
            cancel();return false;
        }
        const unsigned nativeSeat=runtime.playerVehicleSeat<0?0u:unsigned(runtime.playerVehicleSeat+1);
        StorylandVehicleMatrix helicopterFrame{};
        std::array<std::array<float,3>,2> helicopterDoors{};
        if(nativeDoorTransition) {
            const auto& native=vehicle.nativeDefinition;
            if(!native.hasNativeDummies||!native.hasNativeDoorRoots||!native.collision) {
                runtime.vehicleTransitionError="Native vehicle transition requires original seat, root and collision metadata.";
                cancel();return false;
            }
            const bool rear=nativeSeat>=2;
            const bool omitSeatOffset=native.vanVehicle()&&rear;
            helicopterFrame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(native,
                {car.position.x,car.position.y,car.position.z},car.yaw,native.previewMinimumZ,
                vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
                car.water,vehicle.craneCarried?&vehicle.craneFrame:nullptr),native.previewRoot);
            for(size_t right=0;right<helicopterDoors.size();++right)
                if(!storylandVcsHelicopterDoorPosition(helicopterFrame,native.nativeDoorRoots[rear?1u:0u],
                    native.dummyPositions[native.vcsModelLayout?(rear?5u:4u):(rear?3u:2u)],omitSeatOffset?0:native.seatOffset,right!=0,
                    helicopterDoors[right])) {
                    runtime.vehicleTransitionError="The original vehicle door frame is invalid.";
                    cancel();return false;
                }
        }
        const float along=vehicle.nativeDefinition.seatPosition(nativeSeat)[1]-.45f;
        bool found=false;
        bool repositioned=false;
        StorylandNativePoint originalExitDoor{};
        const float playerSide=-(player.x-car.position.x)*s+(player.y-car.position.y)*c;
        float preferredSide=runtime.vehicleAction!=A::Exit&&runtime.playerVehicleSeat<0?(playerSide<0?-1.0f:1.0f):
            runtime.playerVehicleSeat>=0&&runtime.playerVehicleSeat%2==0?-1.0f:1.0f;
        if(nativeDoorTransition&&runtime.vehicleAction!=A::Exit&&runtime.playerVehicleSeat<0)
            preferredSide=storylandVcsHelicopterPreferLeft(player.x,player.y,helicopterDoors[0],helicopterDoors[1])?1.0f:-1.0f;
        const auto access=vehicleAccessState(vehicle);
        const bool exiting=runtime.vehicleAction==A::Exit;
        if(!exiting&&!storylandRetailVehicleCanEnter(access)) {cancel();return false;}
        const bool movingExit=exiting&&runtime.vehicleEntryVariant==StorylandVehicleEntryVariant::MovingExit;
        if(exiting&&storylandVehicleExitDoorReserved(runtime,target,nativeSeat,preferredSide<0)) {
            cancel();return false;
        }
        const bool mayCross=storylandVehicleExitMayCrossSeats(vehicle.nativeDefinition.vcsModelLayout,saddle,true,
            storylandVehicleExitSeatOccupied(runtime,target,
                storylandVehicleExitOppositeSeat(nativeSeat,preferredSide<0)),
            storylandVehicleExitDoorReserved(runtime,target,nativeSeat,preferredSide>=0));
        if(exiting&&access.upZ<=-.8f&&!bike&&!jetSki)runtime.vehicleEntryVariant=StorylandVehicleEntryVariant::CrawlOut;
        for(float side:{preferredSide,-preferredSide}) {
            if(exiting&&side!=preferredSide&&(movingExit||!mayCross))continue;
            if (!exiting && vehicle.coopControlled && runtime.playerVehicleSeat < 0 && side < 0) continue;
            if(!exiting&&runtime.playerVehicleSeat>=0&&side!=preferredSide)continue;
            if(!exiting&&runtime.missionEnvironment) {
                const unsigned door=storylandVehicleSeatDoor(nativeSeat,side<0);
                const auto& tasks=runtime.missionEnvironment->characterVehicleTasks;
                if(std::any_of(tasks.begin(),tasks.end(),[&](const auto& entry) {
                    const auto& task=entry.second;
                    return task.vehicle==vehicle.scriptHandle&&!task.exiting&&
                        storylandVehicleSeatDoor(unsigned(task.seat+1),task.rightHandSide)==door;
                }))continue;
            }
            if(!exiting&&runtime.playerVehicleSeat<0&&side<0&&!noDoors&&runtime.missionEnvironment) {
                const auto& world=*runtime.missionEnvironment;
                const bool occupied=std::any_of(world.missionCharacters.begin(),world.missionCharacters.end(),
                    [&](const auto& entry){return entry.second.alive&&entry.second.vehicle==vehicle.scriptHandle&&
                        entry.second.passengerSeat==0;});
                if(occupied)continue;
            }
            auto variant=runtime.vehicleEntryVariant;
            if(!exiting&&bike)variant=storylandRetailBikeMountVariant(access.rightZ,access.upZ,
                (player.x-car.position.x)*c+(player.y-car.position.y)*s,playerSide,
                runtime.playerMoveBlend,side<0);
            float doorAlong=along,doorLeft=side*(vehicle.halfWidth+.5f);
            if(exiting&&variant==StorylandVehicleEntryVariant::MovingExit&&bike) {
                doorAlong=-vehicle.halfLength-.7f;doorLeft=0;
                runtime.vehicleExitRear=true;
            }
            if(variant==StorylandVehicleEntryVariant::BikeFront) {
                if(vehicle.nativeDefinition.frontEntrySeconds<=0)continue;
                doorAlong=vehicle.halfLength+.5f;doorLeft=0;
            }
            if(variant==StorylandVehicleEntryVariant::BikePickup&&
                vehicle.nativeDefinition.pickupSeconds[side<0?1u:0u]<=0)continue;
            if(variant==StorylandVehicleEntryVariant::BikePullup&&
                vehicle.nativeDefinition.pullupSeconds[side<0?1u:0u]<=0)continue;
            if(!exiting&&!noDoors&&runtime.playerVehicleSeat<0&&side<0&&
                vehicle.nativeDefinition.shuffleSeconds<=0)continue;
            auto point=localPoint(doorAlong,doorLeft);float floor=0;
            if(exiting&&side==preferredSide)originalExitDoor=point;
            bool highAlign=false;
            if(ordinaryBoat) {
                if(!storylandVcsBoatEntryAvailable(vehicle.nativeDefinition)||scriptVehicleHasDriver(runtime,vehicle)) {
                    runtime.vehicleTransitionError="Native SeekBoat requires an available ordinary boat with an empty driver seat.";
                    cancel();return false;
                }
                auto frame=vehicleBody(vehicle,car.yaw).collisionTransform;
                frame[12]+=car.position.x;frame[13]+=car.position.y;frame[14]+=car.position.z;
                const auto anchor=storylandVcsBoatSeekPosition(frame,vehicle.nativeDefinition.dummyPositions[0]);
                point={anchor[0],anchor[1],anchor[2]-playerOriginOffset};
            } else if(jetSki) {
                const auto& native=vehicle.nativeDefinition;
                if(!native.hasNativeDummies||!native.hasVcsJetSkiEntryRoots||!native.collision) {
                    runtime.vehicleTransitionError="VCS JetSki requires original seat, root and collision metadata.";
                    cancel();return false;
                }
                const auto frame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(native,
                    {car.position.x,car.position.y,car.position.z},car.yaw,native.previewMinimumZ,
                    vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
                    car.water,vehicle.craneCarried?&vehicle.craneFrame:nullptr),native.previewRoot);
                std::array<float,3> anchor{};
                if(!storylandVcsJetSkiDoorPosition(frame,
                    native.vcsJetSkiEntryRoots[runtime.vehicleJetSkiWaterContact?1u:0u],
                    native.dummyPositions[nativeSeat==0?0u:5u],
                    {native.collision->maximum.x,native.collision->maximum.y,native.collision->maximum.z},
                    side<0,anchor)) {
                    runtime.vehicleTransitionError="VCS JetSki native door frame is invalid.";
                    cancel();return false;
                }
                highAlign=storylandVcsJetSkiHighAlign(anchor[2],player.z+playerOriginOffset);
                point={anchor[0],anchor[1],anchor[2]-playerOriginOffset};
                StorylandVcsJetSkiRoomQuery query;
                if(!storylandVcsJetSkiRoomQuery(frame,native.dummyPositions[nativeSeat==0?0u:5u],
                    anchor,side<0,query)||!storylandVcsJetSkiRoomClear(runtime,world,cars,target,query,
                    runtime.vehicleJetSkiWaterContact))continue;
            } else if(nativeDoorTransition) {
                const auto& native=vehicle.nativeDefinition;
                const auto& anchor=helicopterDoors[side<0?1u:0u];
                point={anchor[0],anchor[1],anchor[2]-playerOriginOffset};
                if(exiting&&side==preferredSide)originalExitDoor=point;
                StorylandVcsVehicleRoomQuery query;
                if(!movingExit&&(!storylandVcsVehicleRoomQuery(helicopterFrame,native.dummyPositions[native.vcsModelLayout?(nativeSeat>=2?5u:4u):(nativeSeat>=2?3u:2u)],anchor,side<0,query)||
                    !storylandAutomobileRoomClear(runtime,world,cars,target,query)))continue;
            } else if(motorcycleDoor&&variant==StorylandVehicleEntryVariant::Normal) {
                const auto& native=vehicle.nativeDefinition;
                if(!native.hasNativeDummies||!native.collision) {
                    runtime.vehicleTransitionError="Motorcycle transition requires original seat and collision metadata.";
                    cancel();return false;
                }
                const auto frame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(native,
                    {car.position.x,car.position.y,car.position.z},car.yaw,native.previewMinimumZ,
                    vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
                    car.water,vehicle.craneCarried?&vehicle.craneFrame:nullptr),native.previewRoot);
                std::array<float,3> anchor{};
                const auto& maximum=native.collision->maximum;
                if(!storylandMotorcycleDoorPosition(frame,native.nativeMotorcycleEntryRoot,
                    native.seatPosition(nativeSeat),{maximum.x,maximum.y,maximum.z},side<0,anchor)) {
                    runtime.vehicleTransitionError="The original motorcycle door frame is invalid.";
                    cancel();return false;
                }
                point={anchor[0],anchor[1],anchor[2]-playerOriginOffset};
                if(exiting&&side==preferredSide)originalExitDoor=point;
            }
            if(!movingExit&&!ordinaryBoat&&!jetSki&&!nativeDoorTransition) {
                const bool ground=vehicleGroundHeight(world,point.x,point.y,car.position.z,floor);
                if(ground&&std::fabs(floor-car.position.z)<1.2f)point.z=floor;
                else if(!world.collisionModels.empty()&&!car.water) {
                    float surface=0;
                    if(!exiting||!storylandNativeWaterHeight(world,point.x,point.y,surface)||point.z>=surface)continue;
                }
            }
            if(!movingExit&&!ordinaryBoat&&!jetSki&&!nativeDoorTransition) {
                if(!storylandNativeBodyClear(world,point,storylandNativeCharacterBody()))continue;
                if(motorcycleDoor&&variant==StorylandVehicleEntryVariant::Normal) {
                    if(!exiting) {
                        const auto approach=storylandMovePlayTestCharacter(world,runtime,point,point);
                        if(!approach.clear||approach.blocked||distance(approach.position,point)>.3f)continue;
                    }
                } else if(!characterCarClear(point,cars,runtime))continue;
            }
            if(exiting&&!movingExit&&!jetSki&&
                !storylandVehicleExitDestinationClear(runtime,world,cars,target,point))continue;
            const bool right=side<0;
            const auto& native=vehicle.nativeDefinition;
            const float actionSeconds=exiting?
                (jetSki&&runtime.vehicleJetSkiWaterContact?native.vcsJetSkiWetExitSeconds[right?1u:0u]:
                    (right?native.rightExitSeconds:native.exitSeconds)):
                (right?native.rightEntrySeconds:native.entrySeconds);
            const float jackSeconds=native.sideTransitionTiming[right?1u:0u].seconds[unsigned(P::PullOut)];
            if((!ordinaryBoat&&actionSeconds<=0)||(!exiting&&runtime.vehicleAction==A::Hijack&&jackSeconds<=0))continue;
            runtime.vehicleActionDoor=point;runtime.vehicleExitRight=right;
            runtime.vehicleJetSkiHighAlign=jetSki&&highAlign;
            runtime.vehicleEntryVariant=variant;found=true;break;
        }
        if(!found&&exiting&&!movingExit&&!jetSki&&mayCross&&
            storylandFindVehicleExitPosition(runtime,world,cars,target,preferredSide<0,
                originalExitDoor,runtime.vehicleActionDoor)) {
            runtime.vehicleExitRight=preferredSide<0;found=true;repositioned=true;
        }
        if(!found) {
            runtime.vehicleTransitionError="No accessible door with a decoded native vehicle action for model "+
                std::to_string(vehicle.nativeDefinition.modelId);
            cancel();return false;
        }
        const unsigned side=runtime.vehicleExitRight?1u:0u;
        if(!saddle)runtime.vehicleTiming=vehicle.nativeDefinition.sideTransitionTiming[side];
        runtime.vehicleTiming.seconds[unsigned(P::GetIn)]=runtime.vehicleExitRight?
            vehicle.nativeDefinition.rightEntrySeconds:vehicle.nativeDefinition.entrySeconds;
        runtime.vehicleTiming.seconds[unsigned(P::GetOut)]=runtime.vehicleExitRight?
            vehicle.nativeDefinition.rightExitSeconds:vehicle.nativeDefinition.exitSeconds;
        runtime.vehicleTiming.seconds[unsigned(P::PullOut)]=
            vehicle.nativeDefinition.sideTransitionTiming[side].seconds[unsigned(P::PullOut)];
        if(jetSki) {
            if(runtime.vehicleJetSkiHighAlign)runtime.vehicleTiming.seconds[unsigned(P::Align)]=
                vehicle.nativeDefinition.vcsJetSkiHighAlignSeconds[side];
            if(runtime.vehicleJetSkiWaterContact)runtime.vehicleTiming.seconds[unsigned(P::GetOut)]=
                vehicle.nativeDefinition.vcsJetSkiWetExitSeconds[side];
        }
        using Variant=StorylandVehicleEntryVariant;
        if(!exiting&&bike) {
            if(runtime.vehicleEntryVariant==Variant::BikePickup)
                runtime.vehicleTiming.seconds[unsigned(P::Open)]=vehicle.nativeDefinition.pickupSeconds[side];
            else if(runtime.vehicleEntryVariant==Variant::BikePullup)
                runtime.vehicleTiming.seconds[unsigned(P::Open)]=vehicle.nativeDefinition.pullupSeconds[side];
            else if(runtime.vehicleEntryVariant==Variant::BikeFront) {
                runtime.vehicleTiming.seconds[unsigned(P::GetIn)]=vehicle.nativeDefinition.frontEntrySeconds;
                runtime.vehicleTiming.seconds[unsigned(P::PullOut)]=0;
            }
            runtime.vehicleTransitionInitialRoll=vehicle.bodyRoll-vehicle.roadRoll;
            runtime.vehicleTransitionInitialPitch=vehicle.bodyPitch+vehicle.roadPitch;
        } else if(!exiting&&!ordinaryBoat&&!jetSki&&runtime.vehicleExitRight&&runtime.playerVehicleSeat<0&&
            !vehicle.nativeDefinition.busVehicle()&&vehicle.nativeDefinition.shuffleSeconds>0) {
            runtime.vehicleEntryVariant=Variant::PassengerSideDriver;
            runtime.vehicleTiming.seconds[unsigned(P::CloseInside)]+=vehicle.nativeDefinition.shuffleSeconds;
        } else if(exiting&&runtime.vehicleEntryVariant==Variant::MovingExit) {
            const float seconds=vehicle.nativeDefinition.movingExitSeconds[side];
            if(seconds<=0||vehicle.nativeDefinition.movingExitAssociations[side]<0) {cancel();return false;}
            runtime.vehicleTiming.seconds[unsigned(P::GetOut)]=seconds;
            runtime.vehicleTiming.seconds[unsigned(P::CloseOutside)]=0;
        } else if(exiting&&runtime.vehicleEntryVariant==Variant::CrawlOut) {
            const float seconds=vehicle.nativeDefinition.crawlExitSeconds[side];
            if(seconds<=0) {cancel();return false;}
            runtime.vehicleTiming.seconds[unsigned(P::GetOut)]=seconds;
            runtime.vehicleTiming.seconds[unsigned(P::CloseOutside)]=0;
        }
        if(!noDoors) {
            const unsigned door=storylandVehicleSeatDoor(nativeSeat,runtime.vehicleExitRight);
            const bool missing=door+2<vehicle.doorDamage.size()&&vehicle.doorDamage[door+2]==3;
            if(missing||vehicle.doorOpenRatios[door]>=.99f)
                runtime.vehicleTiming.seconds[unsigned(P::Open)]=0;
            if(missing) {
                runtime.vehicleTiming.seconds[unsigned(P::CloseOutside)]=0;
                runtime.vehicleTiming.seconds[unsigned(P::CloseInside)]=
                    runtime.vehicleEntryVariant==Variant::PassengerSideDriver?vehicle.nativeDefinition.shuffleSeconds:0;
            }
        }
        if(runtime.vehicleAction!=A::Exit) {
            const auto clear=[&](StorylandNativePoint a,StorylandNativePoint b) {
                const auto move=storylandMovePlayTestCharacter(world,runtime,a,b);
                return move.clear&&!move.blocked&&distance(move.position,b)<.04f;
            };
            float best=std::numeric_limits<float>::infinity();
            std::vector<StorylandNativePoint> chosen;
            const auto consider=[&](std::vector<StorylandNativePoint> route) {
                float length=0;auto previous=player;
                for(size_t index=0;index<route.size();++index) {
                    auto& point=route[index];
                    if(nativeSeek&&index+1==route.size()) {
                        const auto door=point;
                        const float dx=previous.x-door.x,dy=previous.y-door.y;
                        const float separation=std::hypot(dx,dy);
                        if(seekReached(previous.x,previous.y,door.x,door.y))point=previous;
                        else if(std::isfinite(separation)&&separation>0) {
                            const float radius=std::nextafter(motorcycleEntry?.3f:.5f,0.0f);
                            point={door.x+dx*(radius/separation),door.y+dy*(radius/separation),previous.z};
                            for(unsigned attempt=0;attempt<4&&!seekReached(point.x,point.y,door.x,door.y);++attempt) {
                                point.x=std::nextafter(point.x,door.x);point.y=std::nextafter(point.y,door.y);
                            }
                        } else return;
                        if(!seekReached(point.x,point.y,door.x,door.y))return;
                    }
                    float floor=0;if(!ordinaryBoat&&!jetSki&&vehicleGroundHeight(world,point.x,point.y,car.position.z,floor)&&std::fabs(floor-car.position.z)<1.2f)point.z=floor;
                    if(!clear(previous,point))return;
                    length+=distance(previous,point);previous=point;
                }
                if(length<best) {best=length;chosen=std::move(route);}
            };
            consider({runtime.vehicleActionDoor});
            const float dx=player.x-car.position.x,dy=player.y-car.position.y;
            const float playerSide=(-dx*s+dy*c)>=0?1.0f:-1.0f;
            const float doorSide=runtime.vehicleExitRight?-1.0f:1.0f;
            for(float end:{1.0f,-1.0f})consider({localPoint(end*(vehicle.halfLength+.7f),playerSide*(vehicle.halfWidth+.7f)),
                localPoint(end*(vehicle.halfLength+.7f),doorSide*(vehicle.halfWidth+.7f)),runtime.vehicleActionDoor});
            if(chosen.empty()) {
                if(ordinaryBoat)runtime.vehicleTransitionError="No collision-clear deck approach to the native boat seat for model "+
                    std::to_string(vehicle.nativeDefinition.modelId);
                cancel();return false;
            }
            runtime.vehicleApproach.push_back(player);
            runtime.vehicleApproach.insert(runtime.vehicleApproach.end(),chosen.begin(),chosen.end());
            runtime.vehicleApproachSpeed=!ordinaryBoat&&best>2.0f?runtime.runSpeed:runtime.walkSpeed;
            runtime.vehicleTiming.seconds[unsigned(P::Approach)]=best/std::max(.5f,runtime.vehicleApproachSpeed);
        }
        if(repositioned)runtime.vehicleTiming.seconds.fill(0);
        runtime.vehicleActionDuration=storylandSampleVehicleTransition(runtime.vehicleAction,runtime.vehicleTiming,0).total;
        runtime.vehicleTransitionPrepared=true;
    }
    if(runtime.vehicleAction==A::Exit&&runtime.vehicleEntryVariant==StorylandVehicleEntryVariant::Normal&&
        runtime.vehicleActionDuration>0&&(automobileDoor||motorcycleDoor)) {
        const auto& native=vehicle.nativeDefinition;
        float originOffset=0;
        if(!runtime.missionEnvironment||!runtime.missionEnvironment->characterOriginOffset||
            !runtime.missionEnvironment->characterOriginOffset(native.vcsModelLayout?0:1,originOffset)||
            !std::isfinite(originOffset)) {
            runtime.vehicleTransitionError="Native vehicle exit lost the live player collision origin.";
            cancel();return false;
        }
        const unsigned seat=runtime.playerVehicleSeat<0?0u:unsigned(runtime.playerVehicleSeat+1);
        const bool rear=seat>=2;
        const auto frame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(native,
            {car.position.x,car.position.y,car.position.z},car.yaw,native.previewMinimumZ,
            vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
            car.water,vehicle.craneCarried?&vehicle.craneFrame:nullptr),native.previewRoot);
        std::array<float,3> anchor{};
        bool available=false;
        if(automobileDoor)
            available=storylandVcsHelicopterDoorPosition(frame,native.nativeDoorRoots[rear?1u:0u],
                native.dummyPositions[native.vcsModelLayout?(rear?5u:4u):(rear?3u:2u)],
                native.vanVehicle()&&rear?0:native.seatOffset,runtime.vehicleExitRight,anchor);
        else if(native.collision) {
            const auto& maximum=native.collision->maximum;
            available=storylandMotorcycleDoorPosition(frame,native.nativeMotorcycleEntryRoot,
                native.seatPosition(seat),{maximum.x,maximum.y,maximum.z},runtime.vehicleExitRight,anchor);
        }
        if(!available) {
            runtime.vehicleTransitionError="The live vehicle exit door frame is invalid.";
            cancel();return false;
        }
        runtime.vehicleActionDoor={anchor[0],anchor[1],anchor[2]-originOffset};
        if(motorcycleDoor) {
            float floor=0;
            if(vehicleGroundHeight(world,anchor[0],anchor[1],car.position.z,floor)&&
                std::fabs(floor-car.position.z)<1.2f)runtime.vehicleActionDoor.z=floor;
        }
    }
    const float previousActionTime=runtime.vehicleActionTime;
    const auto previousPhase=runtime.vehiclePhase.phase;
    runtime.vehicleActionTime+=dt;
    runtime.vehiclePhase=storylandSampleVehicleTransition(runtime.vehicleAction,runtime.vehicleTiming,runtime.vehicleActionTime);
    const auto phase=runtime.vehiclePhase.phase;
    const float fraction=runtime.vehiclePhase.fraction;
    const auto lerp=[&](StorylandNativePoint a,StorylandNativePoint b,float t) {
        return StorylandNativePoint{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};
    };
    auto next=runtime.vehicleActionDoor;
    if(vehicle.nativeDefinition.twoWheels()&&
        (runtime.vehicleEntryVariant==StorylandVehicleEntryVariant::BikePickup||
         runtime.vehicleEntryVariant==StorylandVehicleEntryVariant::BikePullup)) {
        const float lift=phase==P::Open?std::clamp((runtime.vehiclePhase.elapsed-.4667f)/
            std::max(.001f,runtime.vehiclePhase.duration-.4667f),0.0f,1.0f):
            phase==P::GetIn||phase==P::CloseInside||phase==P::Done?1.0f:0.0f;
        vehicle.bodyRoll=vehicle.roadRoll+runtime.vehicleTransitionInitialRoll*(1-lift);
        vehicle.bodyPitch=-vehicle.roadPitch+runtime.vehicleTransitionInitialPitch*(1-lift);
    }
    if(nativeSeek&&runtime.vehicleAction!=A::Exit&&phase!=P::Approach&&
        (previousPhase==P::Approach||previousActionTime==0)) {
        if(runtime.vehicleApproach.empty()) {cancel();return false;}
        const auto movement=storylandMovePlayTestCharacter(world,runtime,player,runtime.vehicleApproach.back());
        if(!movement.clear||movement.blocked||!seekReached(movement.position.x,
            movement.position.y,runtime.vehicleActionDoor.x,runtime.vehicleActionDoor.y)) {cancel();return false;}
        player=movement.position;
    }
    if(phase==P::Approach) {
        float travel=runtime.vehiclePhase.elapsed*std::max(.5f,runtime.vehicleApproachSpeed);
        next=runtime.vehicleApproach.front();
        for(size_t i=1;i<runtime.vehicleApproach.size();++i) {
            const auto a=runtime.vehicleApproach[i-1],b=runtime.vehicleApproach[i];const float length=distance(a,b);
            if(travel<=length) {next=lerp(a,b,length>.0001f?travel/length:1);break;}
            travel-=length;next=b;
        }
        const auto movement=storylandMovePlayTestCharacter(world,runtime,player,next);
        if(!movement.clear||movement.blocked) {cancel();return false;}
        if(distance(player,next)>.001f)heading=std::atan2(next.y-player.y,next.x-player.x)/radians;
        player=movement.position;
    } else {
        heading=car.yaw;
        if(phase==P::GetIn)next=lerp(runtime.vehicleActionDoor,car.position,fraction);
        else if(phase==P::CloseInside)next=car.position;
        else if(phase==P::GetOut)next=lerp(car.position,runtime.vehicleActionDoor,fraction);
        else if(phase==P::Done)next=runtime.vehicleAction==A::Exit?runtime.vehicleActionDoor:car.position;
        if(phase==P::GetIn||phase==P::CloseInside||phase==P::Done)
            next.z=storylandVcsHelicopterEntryHeight(next.z,runtime.vehicleActionDoor.z,
                helicopterEntry,vehicle.grounded);
        if(runtime.vehicleAction==A::Exit&&!jetSki) {
            if(phase==P::Done&&!storylandVehicleExitDestinationClear(runtime,world,cars,target,next)&&
                !storylandFindVehicleExitPosition(runtime,world,cars,target,runtime.vehicleExitRight,next,next)) {
                cancel();return false;
            }
            player=next;
        } else {
            const auto movement=storylandMoveNativeBody(world,player,next,storylandNativeCharacterBody());
            if(!movement.clear||movement.blocked) {cancel();return false;}
            player=movement.position;
        }
    }
    if(runtime.vehicleAction==A::Exit&&runtime.vehicleEntryVariant==StorylandVehicleEntryVariant::MovingExit&&phase==P::GetOut) {
        const auto& definition=vehicle.nativeDefinition;
        if(storylandVehicleBailoutDetaches(definition.vcsModelLayout,saddle,
            definition.modelId,runtime.vehiclePhase.elapsed)) {
            const unsigned side=runtime.vehicleExitRight?1u:0u;
            auto& pose=runtime.vehicleExitPose;
            pose={};pose.animation=definition.movingExitAssociations[side];
            pose.group=definition.movingExitGroups[side];pose.sourceVehicle=int32_t(target);
            pose.elapsed=std::min(runtime.vehiclePhase.elapsed,runtime.vehiclePhase.duration);
            pose.duration=runtime.vehiclePhase.duration;
            if(pose.animation==130||pose.animation==131) {
                uint16_t flags=0;
                auto* environment=runtime.missionEnvironment;
                if(!environment||!environment->animationAssociationFlags||
                    !environment->animationAssociationFlags(pose.group,pose.animation,flags)||
                    !runtime.reactionBindings.duration) {
                    pose={};runtime.vehicleTransitionError="Native road rollout requires its original association and get-up bindings.";
                    cancel();return false;
                }
                pose.getUpAnimation=storylandVehicleExitGetUpAnimation(flags);
                pose.getUpDuration=runtime.reactionBindings.duration(pose.getUpAnimation,0);
                if(!std::isfinite(pose.getUpDuration)||pose.getUpDuration<=0) {
                    pose={};runtime.vehicleTransitionError="Native road rollout get-up association has no decoded duration.";
                    cancel();return false;
                }
                pose.getUpAfter=storylandVehiclePlayerFallDeadline(environment->gameMilliseconds,
                    pose.duration,pose.elapsed,true);
                pose.recovery=StorylandVehicleExitRecovery::Fallen;
            }
            pose.active=true;pose.nativeBailout=true;
            pose.velocity={c*vehicle.speed-s*vehicle.lateralSpeed,s*vehicle.speed+c*vehicle.lateralSpeed,
                vehicleVerticalVelocity(vehicle)};
            const bool passenger=runtime.playerVehicleSeat>=0;
            scriptPlayerLeavesVehicle(runtime,vehicle);
            runtime.occupiedVehicle=-1;runtime.playerVehicleSeat=-1;
            runtime.vehicleAction=A::None;runtime.transitionVehicle=-1;runtime.vehicleTransitionPrepared=false;
            runtime.vehicleEntryVariant=StorylandVehicleEntryVariant::Normal;
            if(!passenger) {vehicle.hasDriver=false;vehicle.missionDriver=false;vehicle.parked=false;}
            runtime.grounded=false;runtime.verticalSpeed=pose.velocity[2];runtime.jumpLaunching=false;
            runtime.jumpFromLaunch=false;runtime.jumpHorizontalValid=false;
            runtime.airborneTime=0;runtime.playerPosition=player;
            if(runtime.missionEnvironment)runtime.missionEnvironment->playerVehicleTransition=false;
            refreshMovementCars(runtime,cars);
            return true;
        }
    }
    const unsigned door=storylandVehicleSeatDoor(runtime.playerVehicleSeat<0?0u:
        unsigned(runtime.playerVehicleSeat+1),runtime.vehicleExitRight);
    if(!noDoors&&runtime.vehicleEntryVariant!=StorylandVehicleEntryVariant::MovingExit&&
        runtime.vehicleEntryVariant!=StorylandVehicleEntryVariant::CrawlOut)
        storylandUpdatePlayTestVehicleDoor(runtime,target,car.position,
            runtime.playerVehicleSeat<0?0u:unsigned(runtime.playerVehicleSeat+1),runtime.vehicleExitRight,
            phase,runtime.vehiclePhase.elapsed);
    const bool driverContact=runtime.vehicleEntryVariant!=StorylandVehicleEntryVariant::BikeFront||
        phase!=P::GetIn||runtime.vehiclePhase.elapsed>=.4f;
    if(runtime.vehicleAction==A::Hijack&&!noDoors&&runtime.vehicleExitRight&&
        runtime.playerVehicleSeat<0&&(phase==P::Open||phase==P::PullOut||
            phase==P::GetIn||phase==P::CloseInside||phase==P::Done))
        storylandShiftPassengerSideCarjackDriver(runtime,vehicle);
    if(runtime.vehicleAction==A::Hijack&&scriptVehicleHasDriver(runtime,vehicle)&&driverContact&&
        (phase==P::GetIn||phase==P::CloseInside||phase==P::Done)) {
        if (!displaceScriptVehicleDriver(runtime,vehicle,car,runtime.vehicleActionDoor)) return true;
        runtime.crime.report(StorylandCrime::VehicleTheft,player,runtime.elapsed,false,0x80000000u|uint32_t(target));
    }
    if(phase==P::Done) {
        const bool exiting=runtime.vehicleAction==A::Exit;
        const bool engineWasOn=vehicle.engineOn;
        if(exiting)scriptPlayerLeavesVehicle(runtime,vehicle);
        else scriptPlayerEntersVehicle(runtime,vehicle);
        const bool passenger=runtime.playerVehicleSeat>=0;
        runtime.occupiedVehicle=exiting?-1:int(target);
        if(exiting)runtime.playerVehicleSeat=-1;
        if(!passenger)vehicle.hasDriver=false;
        if(passenger)vehicle.parked=runtime.vehicleWasParked;
        if(exiting&&!passenger&&bike&&
            std::fabs(c*vehicle.speed-s*vehicle.lateralSpeed)<.1f*StorylandNativePhysicsRate&&
            std::fabs(s*vehicle.speed+c*vehicle.lateralSpeed)<.1f*StorylandNativePhysicsRate)
            vehicle.bikeStandingOverride=true;
        vehicle.doorOpenRatios[door]=0;
        if(!exiting&&!passenger&&!engineWasOn) {
            StorylandPlayTestSoundEvent starter(StorylandPlayTestSound::VehicleStarter);
            starter.vehicle=int(target);starter.position=car.position;starter.positioned=true;
            runtime.soundEvents.push_back(starter);
        }
        runtime.vehicleAction=A::None;runtime.transitionVehicle=-1;runtime.vehicleTransitionPrepared=false;runtime.verticalSpeed=0;
        runtime.vehicleEntryVariant=StorylandVehicleEntryVariant::Normal;
        if(runtime.missionEnvironment)runtime.missionEnvironment->playerVehicleTransition=false;
    }
    return true;
}
bool storylandBeginPlayTestVehicleEntry(StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& cars,const StorylandNativePoint& player,
    int32_t slot,int32_t passengerSeat) {
    using A=StorylandVehicleAction;
    const bool retarget=passengerSeat>=0&&runtime.playerVehicleSeat<0&&runtime.transitionVehicle==slot&&
        (runtime.vehicleAction==A::Enter||runtime.vehicleAction==A::Hijack)&&
        (!runtime.vehicleTransitionPrepared||runtime.vehiclePhase.phase==StorylandVehiclePhase::Approach||
         runtime.vehiclePhase.phase==StorylandVehiclePhase::Align||runtime.vehiclePhase.phase==StorylandVehiclePhase::Open||
         runtime.vehiclePhase.phase==StorylandVehiclePhase::PullOut);
    if(slot<0||size_t(slot)>=cars.size()||size_t(slot)>=runtime.vehicles.size()||
        runtime.health<=0||runtime.occupiedVehicle>=0||(runtime.vehicleAction!=A::None&&!retarget)||
        runtime.vehicleExitPose.active||passengerSeat< -1||passengerSeat>2)return false;
    auto& vehicle=runtime.vehicles[size_t(slot)];
    if(passengerSeat>=0&&storylandVcsServiceSeatOccupied(runtime,vehicle,passengerSeat))return false;
    bool shortcutEntry=false;
    if(runtime.missionEnvironment&&!vehicle.nativeDefinition.vcsModelLayout&&passengerSeat<0) {
        const auto& shortcut=runtime.missionEnvironment->lcsExtendedWorld.taxiShortcut;
        if(shortcut.state==2&&shortcut.vehicle==vehicle.scriptHandle) {
            passengerSeat=runtime.missionEnvironment->playerAllowFrontPassenger?0:1;
            shortcutEntry=true;
        }
    }
    if(!vehicle.nativeDefinition.retailTransitions) {
        runtime.vehicleTransitionError="Native vehicle entry/exit animations are not loaded for model "+
            std::to_string(vehicle.nativeDefinition.modelId);
        return false;
    }
    if(storylandVcsOrdinaryBoat(vehicle.nativeDefinition)&&
        (passengerSeat>=0||scriptVehicleHasDriver(runtime,vehicle)))return false;
    auto access=vehicleAccessState(vehicle);
    access.driverProtected=passengerSeat<0&&storylandMissionDriverProtected(runtime,vehicle);
    if(!storylandRetailVehicleCanEnter(access)||scriptVehicleAccessBlocked(runtime,vehicle,false)||
        distance(player,cars[size_t(slot)].position)>4.0f||
        (access.twoWheels&&passengerSeat>0))return false;
    if (runtime.coopVehicleEntryAllowed && !runtime.coopVehicleEntryAllowed(slot, passengerSeat, player)) return false;
    if(passengerSeat>=0&&runtime.missionEnvironment) {
        if(!vehicle.nativeDefinition.vcsModelLayout&&
            ((!runtime.missionEnvironment->playerAllowFrontPassenger&&passengerSeat==0)||
             (runtime.missionEnvironment->lcsExtendedWorld.frontSeatOnlyCharacters.count(1)&&passengerSeat>0)))
            return false;
        for(const auto& entry:runtime.missionEnvironment->missionCharacters)
            if(entry.second.alive&&entry.second.vehicle==vehicle.scriptHandle&&
                entry.second.passengerSeat==passengerSeat)return false;
    }
    if(runtime.missionEnvironment)
        for(const auto& entry:runtime.missionEnvironment->characterVehicleTasks)
            if(!entry.second.exiting&&entry.second.vehicle==vehicle.scriptHandle&&
                entry.second.seat==passengerSeat)return false;
    if(shortcutEntry) {
        auto& environment=*runtime.missionEnvironment;
        auto& shortcut=environment.lcsExtendedWorld.taxiShortcut;
        shortcut.previousControl=environment.control;
        environment.control=false;
        environment.playerVehicleEntryAsPassenger=true;
        shortcut.state=3;
    }
    if(retarget)storylandCancelPassengerSideCarjack(runtime,vehicle);
    runtime.playerVehicleSeat=passengerSeat;
    runtime.vehicleAction=passengerSeat<0&&scriptVehicleHasDriver(runtime,vehicle)?A::Hijack:A::Enter;
    runtime.transitionVehicle=slot;runtime.vehicleActionTime=0;
    runtime.vehicleTransitionPrepared=false;runtime.vehiclePhase={};
    runtime.vehicleEntryVariant=StorylandVehicleEntryVariant::Normal;
    runtime.vehicleActionDuration=vehicle.nativeDefinition.entrySeconds+
        (runtime.vehicleAction==A::Hijack?vehicle.nativeDefinition.jackSeconds:0);
    runtime.vehicleActionStart=player;runtime.vehicleExitRight=false;
    if(!retarget)runtime.vehicleWasParked=vehicle.parked;
    if(!storylandVcsOrdinaryBoat(vehicle.nativeDefinition)) {
        vehicle.parked=true;
        if(passengerSeat<0&&!vehicle.positionFrozen)vehicle.speed=0;
    }
    if(runtime.missionEnvironment) {
        runtime.missionEnvironment->playerVehicleTransition=true;
        runtime.missionEnvironment->playerVehicleEntryTarget=vehicle.scriptHandle;
        runtime.missionEnvironment->playerVehicleEntryAsPassenger=passengerSeat>=0;
    }
    return true;
}
void storylandExtinguishPlayTestVehicle(StorylandPlayTestRuntime& runtime,size_t slot) {
    if(slot>=runtime.vehicles.size())return;
    auto& vehicle=runtime.vehicles[slot];
    vehicle.health=std::max(300.0f,vehicle.health);vehicle.burning=false;vehicle.engineFireSpent=true;
    for(auto& effect:runtime.effects)
        if(effect.vehicle==int(slot)&&effect.kind==StorylandParticleKind::Flame)effect.remaining=0;
    if(runtime.missionEnvironment&&vehicle.scriptHandle>=0)
        for(auto& entry:runtime.missionEnvironment->scriptFires)
            if(entry.second.attachedKind==2&&entry.second.attachedHandle==vehicle.scriptHandle)
                entry.second.active=false;
}

bool storylandFindDeadCharacterPickupPosition(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, const std::vector<StorylandNativeTrafficActor>& cars,
    int32_t handle, std::array<float,3>& result) {
    if (!runtime.missionEnvironment) {
        return false;
    }
    auto& environment = *runtime.missionEnvironment;
    StorylandNativePoint origin{environment.x, environment.y, environment.z};
    if (handle != 0) {
        const auto character = environment.missionCharacters.find(handle);
        if (character == environment.missionCharacters.end()) {
            return false;
        }
        origin = {character->second.x, character->second.y, character->second.z};
    }
    StorylandNativeCollisionModel clearance;
    clearance.minimum = {-1.2f, -1.2f, -1.2f};
    clearance.maximum = {1.2f, 1.2f, 1.2f};
    clearance.boundingRadius = 1.2f;
    clearance.spheres.push_back({{}, 1.2f, 0, 0});
    StorylandNativeBody sphere;
    sphere.collision = &clearance;
    const auto closePickup = [&](const StorylandNativePoint& point) {
        for (const auto& pickup : environment.pickups) {
            if (!pickup.removed && pickup.type != 0 &&
                distance(point, {pickup.x, pickup.y, pickup.z}) < 1.3f) {
                return true;
            }
        }
        for (const auto& pickup : runtime.pickups) {
            if (pickup.ammunition > 0 && distance(point, pickup.position) < 1.3f) {
                return true;
            }
        }
        return false;
    };
    const auto vehicleOverlap = [&](const StorylandNativePoint& point) {
        for (size_t index = 0; index < runtime.vehicles.size() && index < cars.size(); ++index) {
            if (!runtime.vehicles[index].collisionEnabled) {
                continue;
            }
            if (storylandNativeVehicleBodiesOverlap(point, sphere, cars[index].position,
                vehicleBody(runtime.vehicles[index], cars[index].yaw))) {
                return true;
            }
        }
        for (const auto& entry : environment.missionVehicles) {
            const auto& vehicle = entry.second;
            if (vehicle.deleted || !vehicle.collisionEnabled || vehicle.spawnedSlot >= 0) {
                continue;
            }
            const auto model = world.collisionModels.find(uint32_t(vehicle.model));
            if (model == world.collisionModels.end()) {
                continue;
            }
            StorylandNativeBody body;
            body.collision = &model->second;
            body.collisionTransform = storylandScriptObjectMatrix(0, 0, 0, 0, 0, vehicle.heading);
            if (storylandNativeVehicleBodiesOverlap(point, sphere,
                {vehicle.x, vehicle.y, vehicle.z}, body)) {
                return true;
            }
        }
        return false;
    };
    // Retail 0x1E1AF0 consumes two independent byte angles per attempt and
    // progressively relaxes pickup/vehicle avoidance before the final fallback.
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
        const float xAngle = float(environment.nextNativeRandom() & 255u) * 0.024543693289f;
        const float yAngle = float(environment.nextNativeRandom() & 255u) * 0.024543693289f;
        StorylandNativePoint point{origin.x + std::sin(xAngle) * 1.5f,
            origin.y + std::cos(yAngle) * 1.5f, origin.z};
        const StorylandNativePoint bottom{point.x, point.y, -1000.0f};
        const float ground = storylandTraceNativeSegmentCategories(world, point, bottom, true, false);
        if (ground >= 1) {
            continue;
        }
        point.z += (bottom.z - point.z) * ground + 0.5f;
        const StorylandNativePoint start{origin.x, origin.y, origin.z + 0.3f};
        const float length = distance(start, point);
        if (!std::isfinite(length) || length <= 0.0001f) {
            continue;
        }
        const float extension = (length + 0.4f) / length;
        const StorylandNativePoint end{start.x + (point.x - start.x) * extension,
            start.y + (point.y - start.y) * extension,
            start.z + (point.z - start.z) * extension};
        StorylandNativePoint player{environment.x, environment.y, environment.z};
        if (runtime.occupiedVehicle >= 0 && size_t(runtime.occupiedVehicle) < cars.size()) {
            player = cars[size_t(runtime.occupiedVehicle)].position;
        }
        if (attempt < 17 && (std::hypot(point.x - player.x, point.y - player.y) <= 2.0f ||
            closePickup(point) || vehicleOverlap(point))) {
            continue;
        }
        if (storylandScriptLineOfSightHit(runtime, world, cars, {}, start, end,
            attempt < 16 ? 11u : 1u)) {
            continue;
        }
        result = {point.x, point.y, point.z};
        return true;
    }
    result = {origin.x, origin.y, origin.z + 0.4f};
    return true;
}

uint32_t storylandAdrenalineRemainingMilliseconds(const StorylandPlayTestRuntime& runtime) {
    const auto& state=runtime.adrenaline;
    const auto* world=runtime.missionEnvironment;
    if(!state.enabled||!state.active||!world||runtime.health<=0||world->health<=0||
        runtime.wastedActive||runtime.busted)return 0;
    const uint32_t remaining=state.expiresAt-world->gameMilliseconds;
    // The native deadline is game time + 20000, including uint32 clock wrap.
    // Values beyond that interval have expired or belong to another timeline.
    return remaining<=StorylandAdrenalineState::durationMilliseconds?remaining:0;
}

void storylandClearAdrenaline(StorylandPlayTestRuntime& runtime) {
    runtime.adrenaline.active=false;
    runtime.adrenaline.expiresAt=0;
}

void storylandUpdateAdrenaline(StorylandPlayTestRuntime& runtime) {
    if(!storylandAdrenalineRemainingMilliseconds(runtime))storylandClearAdrenaline(runtime);
}

bool storylandSetAdrenalineEnabled(StorylandPlayTestRuntime& runtime,bool enabled,bool vcs,std::string& error) {
    error.clear();
    if(enabled&&!vcs) {error="Adrenaline requires Vice City Stories.";return false;}
    runtime.adrenaline.enabled=enabled;
    if(!enabled) {
        storylandClearAdrenaline(runtime);
        storylandRemoveAdrenalinePickup(runtime);
    }
    return true;
}

bool storylandActivateAdrenaline(StorylandPlayTestRuntime& runtime,bool vcs,std::string& error) {
    error.clear();
    if(vcs&&!runtime.adrenaline.enabled) {error="Enable adrenaline before collecting the pickup.";return false;}
    auto* world=runtime.missionEnvironment;
    if(!world||runtime.health<=0||world->health<=0||runtime.wastedActive||runtime.busted) {
        error="Adrenaline requires a living player in Play Test.";return false;
    }
    // VCS 1E1FA8 and LCS 1CB7E8 refill stamina for twenty game seconds.
    // LCS 348CDC applies one-third time and double locomotion animation rate.
    if(!vcs)runtime.adrenaline.enabled=true;
    world->playerSprintEnergy=world->playerSprintMaximum;
    runtime.adrenaline.expiresAt=world->gameMilliseconds+StorylandAdrenalineState::durationMilliseconds;
    runtime.adrenaline.active=true;
    return true;
}

float storylandAdrenalineTimeScale(const StorylandPlayTestRuntime& runtime,float scriptTimeScale) {
    // Keep the script's own scale intact so its latest value resumes on expiry.
    return storylandAdrenalineRemainingMilliseconds(runtime)?1.0f/3.0f:scriptTimeScale;
}

float storylandAdrenalineAnimationRate(const StorylandPlayTestRuntime& runtime) {
    return storylandAdrenalineRemainingMilliseconds(runtime)?2.0f:1.0f;
}

void storylandRestoreZombiePedestrian(StorylandRuntimePed& ped) {
    if(!ped.zombie.active)return;
    const auto saved=std::move(ped.zombie);
    ped.zombie={};
    ped.model=saved.originalModel;ped.stats=saved.originalStats;
    ped.weapon=saved.originalWeapon;
    ped.ammunition=saved.originalAmmunition;ped.magazine=saved.originalMagazine;
    if(ped.health>0) {
        const float damage=std::max(0.0f,saved.initialHealth-ped.health);
        ped.health=std::max(0.0f,saved.originalHealth-damage);
        ped.armour=saved.originalArmour;
    }
    ped.hostile=ped.health>0&&saved.originalHostile;
    ped.playOptionHostile=false;ped.melee={};ped.ranged={};
    ped.weaponCooldown=0;ped.reloading=0;ped.fireFlash=0;ped.combatSpeed=0;
    ped.clearFlee();ped.clearGunThreat();
    ped.state=ped.health<=0?StorylandPedState::Dead:
        ped.hostile?StorylandPedState::Combat:StorylandPedState::Walk;
    ped.headValid=false;ped.torsoValid=false;ped.nativeRootValid=false;ped.weaponMuzzleValid=false;
}

void storylandUpdateZombiePedestrian(StorylandPlayTestRuntime& runtime,StorylandRuntimePed& ped) {
    if(!runtime.zombiePedestrians&&!ped.zombie.active)return;
    const auto& original=ped.zombie.active?ped.zombie.originalStats:ped.stats;
    const bool civilian=ped.scriptHandle<0&&!ped.fortBaxterSoldier&&!ped.policePursuing&&
        ped.serviceRole==StorylandServiceRole::Civilian&&original.valid&&
        (original.pedType==4||original.pedType==5||original.pedType==18||original.pedType==20);
    const bool available=runtime.zombiePedestrianModel<runtime.pedestrianModelStats.size()&&
        runtime.zombiePedestrianWeapon<runtime.weapons.size();
    if(!runtime.zombiePedestrians||!runtime.crime.vcs||!civilian||!available) {
        storylandRestoreZombiePedestrian(ped);return;
    }
    if(ped.zombie.active||ped.health<=0||ped.renderAlpha==0||ped.water.active)return;
    auto& saved=ped.zombie;
    saved.originalModel=ped.model;saved.originalStats=ped.stats;saved.originalWeapon=ped.weapon;
    saved.originalAmmunition=ped.ammunition;saved.originalMagazine=ped.magazine;
    saved.originalHealth=ped.health;saved.originalArmour=ped.armour;
    saved.originalHostile=ped.hostile&&!ped.playOptionHostile;
    saved.initialHealth=std::min(ped.health,StorylandBrawnZombie::shootingHealth);
    saved.active=true;
    ped.model=runtime.zombiePedestrianModel;
    ped.stats=runtime.pedestrianModelStats[ped.model];
    ped.stats.animationGroup=StorylandBrawnZombie::animationGroup;
    ped.stats.pedType=StorylandBrawnZombie::pedestrianType;
    ped.weapon=runtime.zombiePedestrianWeapon;ped.ammunition=0;ped.magazine=0;
    ped.health=saved.initialHealth;ped.armour=0;
    ped.hostile=false;ped.playOptionHostile=false;ped.melee={};ped.ranged={};
    ped.weaponCooldown=0;ped.reloading=0;ped.fireFlash=0;ped.combatSpeed=0;ped.timer=0;
    ped.clearFlee();ped.clearGunThreat();ped.state=StorylandPedState::Walk;
    ped.headValid=false;ped.torsoValid=false;ped.nativeRootValid=false;ped.weaponMuzzleValid=false;
}

void StorylandPlayTestRuntime::reset(size_t pedModels,size_t vehicleModels,float hour) {
    const bool extended=extendedAndSaFeatures;
    const bool lcsCrouch=extendedLcsCrouch;
    *this={};clockHour=hour;
    lcsHud.resetMoney(money);
    extendedAndSaFeatures=extended;extendedLcsCrouch=lcsCrouch;
    for(size_t index=0;index<pedestrians.size();++index) pedestrians[index].model=pedModels?index%pedModels:0;
    for(size_t index=0;index<vehicles.size();++index) vehicles[index].model=vehicleModels?index%vehicleModels:0;
}
bool storylandPlayerBulletReactionAllowed(const StorylandPlayTestRuntime& runtime) {
    if(runtime.playerBulletReaction.vehicleImpact&&runtime.playerBulletReaction.knockdown) {
        if(runtime.health<=0||runtime.occupiedVehicle>=0||runtime.playerWater.active||
            runtime.vehicleAction!=StorylandVehicleAction::None)return false;
        return !runtime.missionEnvironment||(!runtime.missionEnvironment->cutscene.playing&&
            !runtime.missionEnvironment->playerWarped&&!runtime.missionEnvironment->playerPositionFrozen&&
            runtime.missionEnvironment->playerCollisionEnabled&&
            !runtime.missionEnvironment->activeVcsCharacterAttachment(runtime.crime.vcs?0:1)&&
            runtime.missionEnvironment->playerAttachedVehicle<0);
    }
    if(runtime.health<=0||!runtime.playerControl||runtime.occupiedVehicle>=0||
        runtime.vehicleAction!=StorylandVehicleAction::None||runtime.entryTransition>=0||
        runtime.playerWater.active||(!runtime.grounded&&!runtime.playerBulletReaction.knockdown)||runtime.jumpLaunching||
        runtime.playerJetSkiFallActive||runtime.vehicleExitPose.active||runtime.extendedMovement.busy()||
        runtime.debugMovement!=StorylandDebugMovement::Walking||
        runtime.playerReaction.action==StorylandPlayerReaction::EvadeDive)return false;
    if(!runtime.missionEnvironment)return true;
    const auto& environment=*runtime.missionEnvironment;
    const int32_t handle=runtime.crime.vcs?0:1;
    return environment.control&&!environment.cutscene.playing&&!environment.playerCutsceneSafe&&!environment.playerWarped&&
        !environment.playerPositionFrozen&&!environment.playerWalkTask.active&&
        !environment.activeVcsCharacterAttachment(handle)&&environment.playerAttachedVehicle<0&&
        !environment.scriptedCharacterAnimations.count(handle)&&
        !environment.pedAnimationRequests.count(handle)&&!environment.mobilePhones.count(handle);
}

bool storylandPlayerBulletReactionActive(const StorylandPlayTestRuntime& runtime) {
    return (runtime.playerBulletReaction.active(runtime.elapsed)||
        (runtime.playerBulletReaction.vehicleImpact&&runtime.playerBulletReaction.knockdown))&&
        storylandPlayerBulletReactionAllowed(runtime);
}

bool storylandMissionBulletReactionAllowed(const StorylandPlayTestRuntime& runtime,int32_t handle) {
    if(!runtime.missionEnvironment)return false;
    const auto& environment=*runtime.missionEnvironment;
    const auto found=environment.missionCharacters.find(handle);
    if(found==environment.missionCharacters.end())return false;
    const auto& character=found->second;
    const auto combat=runtime.missionCombat.find(handle);
    if(combat!=runtime.missionCombat.end()&&combat->second.bulletReaction.vehicleImpact&&
        combat->second.bulletReaction.knockdown) {
        return character.alive&&character.health>0&&!character.networkControlled&&!character.removeFromWorld&&
            character.vehicle<0&&!character.attached&&!character.positionFrozen&&character.collisionEnabled&&
            !environment.cutscene.playing&&!environment.vcsPedFalls.count(handle)&&
            !environment.characterVehicleTasks.count(handle)&&!combat->second.water.active;
    }
    if(!character.alive||character.health<=0||character.networkControlled||character.removeFromWorld||
        character.vehicle>=0||character.attached||character.positionFrozen||character.animationId>=0||
        character.waitState!=0||environment.cutscene.playing||environment.vcsPedFalls.count(handle)||
        environment.characterVehicleTasks.count(handle)||environment.scriptedCharacterAnimations.count(handle)||
        environment.pedAnimationRequests.count(handle)||environment.mobilePhones.count(handle)||
        storylandPlayerReactionHoldsTarget(runtime.playerReaction,StorylandReactionTargetKind::Mission,
            handle,uint32_t(character.model)))return false;
    return combat==runtime.missionCombat.end()||(!combat->second.water.active&&
        (combat->second.bulletReaction.knockdown||!combat->second.gravityPositionValid||combat->second.grounded));
}

bool storylandMissionBulletReactionActive(const StorylandPlayTestRuntime& runtime,int32_t handle) {
    const auto combat=runtime.missionCombat.find(handle);
    return combat!=runtime.missionCombat.end()&&(combat->second.bulletReaction.active(runtime.elapsed)||
        (combat->second.bulletReaction.vehicleImpact&&combat->second.bulletReaction.knockdown))&&
        storylandMissionBulletReactionAllowed(runtime,handle);
}

bool storylandBeginPlayerDeath(StorylandPlayTestRuntime& runtime,
    StorylandDeathAssociation association,bool force) {
    if(runtime.health>0||(!force&&runtime.playerDeath.animation>=0))return false;
    if(association.animation<0)association=storylandNativeDeathAssociation(runtime.crime.vcs,0,-1);
    const bool fallen=runtime.playerJetSkiFallActive||runtime.playerBulletReaction.knockdown||
        (runtime.vehicleExitPose.active&&runtime.vehicleExitPose.recovery!=StorylandVehicleExitRecovery::None);
    if(fallen)association.blendRate*=.5f;
    runtime.playerDeath=association;
    runtime.playerDeathTime=0;
    storylandRetireBulletReaction(runtime.playerBulletReaction);
    storylandQueuePlayerDeathSounds(runtime);
    runtime.movementSpeed=0;
    runtime.playerMelee={};
    runtime.playerReaction.reset();
    runtime.equalizerScope=false;runtime.equalizerScopeHeld=false;
    runtime.pcGrabHeld=false;runtime.pcCancelAim=false;
    runtime.playerCrouchPose={};
    runtime.vehicleExitPose={};
    runtime.playerJetSkiFallActive=false;
    runtime.playerJetSkiFall={};
    runtime.playerFall={};
    runtime.jumpLaunching=false;
    runtime.manualAimActive=false;
    runtime.freeAimActive=false;
    storylandClearPlayTestTarget(runtime);
    storylandClearAdrenaline(runtime);
    if(runtime.vehicleAction!=StorylandVehicleAction::None) {
        if(runtime.occupiedVehicle<0&&runtime.transitionVehicle>=0&&
            size_t(runtime.transitionVehicle)<runtime.vehicles.size()) {
            auto& vehicle=runtime.vehicles[size_t(runtime.transitionVehicle)];
            storylandCancelPassengerSideCarjack(runtime,vehicle);
            vehicle.parked=runtime.vehicleWasParked;
        }
        runtime.vehicleAction=StorylandVehicleAction::None;
        runtime.transitionVehicle=-1;
        runtime.vehicleTransitionPrepared=false;
        runtime.vehiclePhase={};
        runtime.vehicleApproach.clear();
        runtime.vehicleActionTime=runtime.vehicleActionDuration=0;
        runtime.vehicleEntryVariant=StorylandVehicleEntryVariant::Normal;
        if(runtime.occupiedVehicle<0)runtime.playerVehicleSeat=-1;
    }
    if(runtime.missionEnvironment) {
        auto& world=*runtime.missionEnvironment;
        world.playerWalkTask.clear();
        world.playerVehicleTransition=false;
        world.playerVehicleEntryTarget=-1;
        world.playerVehicleEntryAsPassenger=false;
    }
    return true;
}

static void storylandFinishPlayerDamage(StorylandPlayTestRuntime& runtime,float previousHealth,bool bleed,
    int32_t weapon=-1,const StorylandDeathAssociation* death=nullptr,uint8_t kind=0) {
    const float lost=std::max(0.0f,previousHealth-runtime.health);
    if (runtime.health<=0 && previousHealth>0) {
        storylandBeginPlayerDeath(runtime,death?*death:
            storylandNativeDeathAssociation(runtime.crime.vcs,0,-1),true);
    }
    const bool bleeding = !runtime.missionEnvironment || runtime.missionEnvironment->playerBleeding;
    const bool bloodAllowed=bleed&&bleeding&&storylandBloodDamageAllowed(runtime.crime.vcs,kind,weapon);
    if(runtime.health<=0&&previousHealth>0)runtime.playerCorpseBlood.suppressPool=!bloodAllowed;
    if(bloodAllowed&&lost>0) {
        storylandMarkBloodRunover(runtime,runtime.playerPosition,weapon);
        const StorylandNativePoint impact=runtime.playerTorsoValid?runtime.playerTorso:
            StorylandNativePoint{runtime.playerPosition.x,runtime.playerPosition.y,runtime.playerPosition.z+1.05f};
        storylandEmitBloodImpact(runtime,impact,lost);
    }
    const int nativeReaction=runtime.crime.vcs?
        (weapon==storylandNativeFallWeapon(true)?0x69:-1):storylandLcsPlayerDamageSpeech(weapon,kind);
    if(lost>0&&runtime.health>0&&nativeReaction>=0) {
        StorylandPlayTestSoundEvent reaction(StorylandPlayTestSound::Damage);
        reaction.weapon=weapon;
        reaction.nativeSpeech=nativeReaction;
        storylandStampCharacterSoundEvent(runtime,reaction);
        runtime.soundEvents.push_back(reaction);
    }
}
void StorylandPlayTestRuntime::damage(float amount,bool bleed,bool bypassArmour) {
    if(!std::isfinite(amount)||amount<=0||health<=0) return;
    const float previousHealth=health;
    const float absorbed=bypassArmour?0.0f:std::min(armour,amount);
    if(absorbed>0&&!crime.vcs)
        lcsHud.armourLostAt=missionEnvironment?missionEnvironment->gameMilliseconds:trafficClockMilliseconds;
    armour-=absorbed;health=std::max(0.0f,health-(amount-absorbed));
    storylandFinishPlayerDamage(*this,previousHealth,bleed);
}
bool StorylandPlayTestRuntime::inflictDamage(float amount,uint8_t kind,int32_t weapon,bool bleed,
    uint8_t piece,uint8_t direction,bool playerAttack,const StorylandNativePoint* source) {
    if((kind!=0&&kind!=1&&kind!=2&&kind!=4&&kind!=8&&kind!=16)||piece>6||direction>3)return false;
    if(missionEnvironment) {
        const auto& world=*missionEnvironment;
        if(world.playerCutsceneSafe||(world.playerDamageProofs&storylandNativePedDamageProof(crime.vcs,weapon,kind)))
            return false;
        if(crime.vcs) {
            const bool residentSeated=world.inVehicle&&occupiedVehicle>=0&&
                size_t(occupiedVehicle)<vehicles.size()&&vehicles[size_t(occupiedVehicle)].nativeDefinition.domain!=StorylandVehicleDomain::Unknown;
            if(!storylandVcsPedDamageContactAllowed(world.playerCollisionEnabled&&!world.inVehicle,
                residentSeated,world.playerDrownsInWater,weapon))return false;
        } else if(kind==1&&occupiedVehicle>=0&&!world.playerCanBeShotInVehicle)return false;
        if(world.playerFireproofReward&&(weapon==(crime.vcs?17:15)||weapon==(crime.vcs?33:31)))return false;
        if(world.playerOnlyDamagedByPlayer&&!playerAttack&&
            weapon!=storylandNativeDrowningWeapon(crime.vcs)&&
            weapon!=storylandNativeExplosionWeapon(crime.vcs))return false;
    }
    const auto result=storylandApplyNativePedDamage(health,armour,amount,true,1,
        storylandNativeDamageBypassesArmour(crime.vcs,weapon));
    if(!result.accepted)return false;
    const float previousHealth=health;
    if(result.armour<armour&&!crime.vcs)
        lcsHud.armourLostAt=missionEnvironment?missionEnvironment->gameMilliseconds:trafficClockMilliseconds;
    health=result.health;armour=result.armour;
    const auto death=storylandNativeDeathAssociation(crime.vcs,kind,weapon,piece,direction);
    storylandFinishPlayerDamage(*this,previousHealth,bleed,weapon,result.fatal?&death:nullptr,kind);
    StorylandBulletImpact impact;
    impact.weapon=weapon;impact.player=true;
    impact.canKnockDown=!missionEnvironment||missionEnvironment->playerCanBeShotInVehicle;
    impact.heading=missionEnvironment?missionEnvironment->heading+90.0f:0.0f;
    impact.speed=movementSpeed;impact.verticalSpeed=verticalSpeed;
    if(source)impact.sourceDirection={source->x-playerPosition.x,source->y-playerPosition.y,0};
    if(kind==1&&storylandPlayerBulletReactionAllowed(*this)&&
        storylandBeginBulletReaction(*this,playerBulletReaction,direction,impact)) {
        if(!playerBulletReaction.knockdown)
            playerBulletReaction.nextAllowedAt=elapsed+storylandPlayerBulletReactionInterval(crime.vcs,weapon);
        else {grounded=false;verticalSpeed=playerBulletReaction.fall.velocity[2];}
        playerMelee={};playerReaction.clearAction();playerCrouchPose={};
        movementSpeed=0;playerMoveBlend=0;playerStopVariant=-1;playerStopTime=0;
        storylandResetPlayerSprint(*this);
    }
    if(storylandCriticalPlayerScreenBlood(extendedAndSaFeatures,crime.vcs,kind,bleed,
        previousHealth,health,maximumHealth(),piece)) {
        playerCameraBloodDroplets=std::min(1.0f,playerCameraBloodDroplets+0.60f);
        ++playerCameraBloodSerial;
    }
    if(missionEnvironment) {
        missionEnvironment->health=health;missionEnvironment->armour=armour;
        if(result.applied>0)missionEnvironment->playerLastWeaponDamage=weapon;
    }
    return true;
}
void StorylandPlayTestRuntime::recordCloseExecutionBlood(const StorylandNativePoint& victim,
    uint8_t kind,bool headHit,bool localPlayer) {
    const std::array<float,3> impact{victim.x,victim.y,victim.z+1.1f};
    if(!storylandCloseExecutionScreenBlood(extendedAndSaFeatures,crime.vcs,localPlayer,
        kind,headHit,screenBloodView,{playerPosition.x,playerPosition.y,playerPosition.z},impact))return;
    if(missionEnvironment&&missionEnvironment->sphereOnScreen&&
        !missionEnvironment->sphereOnScreen(impact,.35f))return;
    playerCameraBloodDroplets=.80f;
    ++playerCameraBloodSerial;
}

float storylandZonePopulationScale(const StorylandMissionEnvironment& world,
    const StorylandNativePoint& point,bool vehicles) {
    const auto population=storylandNativePopulationAt(world,point,world.hour);
    if(!population.available)return 0;
    const int32_t density=vehicles?population.values.cars[0]:population.values.pedestrians[0];
    return density>0?1.0f:0.0f;
}

size_t storylandAmbientPedestrianTarget(const StorylandPlayTestRuntime& runtime,
    const StorylandNativePoint& point) {
    if(!runtime.missionEnvironment||runtime.visibleArea!=0)return 0;
    const auto& world=*runtime.missionEnvironment;
    const auto population=storylandNativePopulationAt(world,point,runtime.clockHour);
    size_t scripted=0;
    for(const auto& vehicle:runtime.vehicles)
        if(vehicle.nativeDefinition.modelId!=UINT32_MAX&&vehicle.hasDriver&&vehicle.health>0&&
            vehicle.scriptHandle<0&&vehicle.generator<0)++scripted;
    for(const auto& entry:world.missionCharacters)
        if(!entry.second.removeFromWorld)++scripted;
    const size_t occupied=scripted+1; // The player occupies a pool slot, but is not an ambient count.
    const size_t available=runtime.pedestrians.size()>occupied?runtime.pedestrians.size()-occupied:0;
    if(population.vcs)return storylandVcsPedestrianTarget(population,world.pedDensityMultiplier,
        world.weatherState.rain,scripted,available);
    return storylandPopulationDensityTarget(population.available,population.values.pedestrians[0],
        world.pedDensityMultiplier,available);
}

void storylandRefreshPlayTestVehicles(StorylandPlayTestRuntime& runtime,const StorylandNativePaths& paths,
    std::vector<StorylandNativeTrafficActor>& cars,const StorylandNativePoint& player,size_t maximum,
    const StorylandNativeWorld* world,std::vector<StorylandNativeTrafficActor>* pedestrians) {
    if(maximum==0||paths.nodes.empty()||runtime.paused||runtime.pauseMenu||runtime.visibleArea!=0||
        (!paths.vcs&&runtime.elapsed<runtime.nextVehicleRefresh))return;
    runtime.nextVehicleRefresh=runtime.elapsed+0.8f;
    StorylandNativePoint focus=player;
    if(runtime.missionEnvironment&&runtime.missionEnvironment->generateCarsAroundCamera&&
        runtime.missionEnvironment->activeCameraVectors) {
        std::array<float,3> eye{},target{};
        if(runtime.missionEnvironment->activeCameraVectors(eye,target)&&
            std::all_of(eye.begin(),eye.end(),[](float value){return std::isfinite(value);}))focus={eye[0],eye[1],eye[2]};
    }
    float fieldOfView=70;
    StorylandNativePoint cameraDirection{};
    bool cameraDirectionValid=false,steepDownwardCamera=false;
    if(runtime.missionEnvironment) {
        const auto& mission=*runtime.missionEnvironment;
        if(mission.camera.active)fieldOfView=mission.camera.fieldOfView;
        else if(runtime.manualAimActive)fieldOfView=mission.manualAim.fieldOfView;
        if(runtime.crime.vcs)fieldOfView=storylandVcsCameraFieldOfView(fieldOfView,
            mission.camera.vcsFieldOfViewOverride);
        std::array<float,3> eye{},target{};
        if(mission.activeCameraVectors&&mission.activeCameraVectors(eye,target)) {
            cameraDirection={target[0]-eye[0],target[1]-eye[1],target[2]-eye[2]};
            const float length=std::sqrt(cameraDirection.x*cameraDirection.x+
                cameraDirection.y*cameraDirection.y+cameraDirection.z*cameraDirection.z);
            cameraDirectionValid=std::isfinite(length)&&length>0;
            steepDownwardCamera=cameraDirectionValid&&cameraDirection.z/length<-.9f;
        }
    }
    const float generationDistance=storylandVcsCameraGenerationDistance(fieldOfView);
    bool playerVehicle=false;
    float nativeSpeed=0;
    StorylandNativePoint vehicleDirection{};
    if(runtime.occupiedVehicle>=0&&size_t(runtime.occupiedVehicle)<cars.size()&&
        size_t(runtime.occupiedVehicle)<runtime.vehicles.size()) {
        playerVehicle=true;
        const auto& vehicle=runtime.vehicles[size_t(runtime.occupiedVehicle)];
        const float angle=cars[size_t(runtime.occupiedVehicle)].yaw*radians;
        vehicleDirection={std::cos(angle)*vehicle.speed-std::sin(angle)*vehicle.lateralSpeed,
            std::sin(angle)*vehicle.speed+std::cos(angle)*vehicle.lateralSpeed,0};
        nativeSpeed=std::hypot(vehicleDirection.x,vehicleDirection.y)/StorylandVehicleStepsPerSecond;
    }
    const uint32_t frame=runtime.missionEnvironment?runtime.missionEnvironment->nativeFrameCounter:runtime.fallenWorldFrame;
    const auto generation=storylandVcsTrafficGenerationProfile(playerVehicle,nativeSpeed,frame,
        generationDistance,steepDownwardCamera);
    const auto direction=steepDownwardCamera?StorylandNativePoint{.707f,.707f,0}:
        generation.velocityDirection?vehicleDirection:cameraDirection;
    const float removalDistance=storylandVcsTrafficRemovalDistance(generationDistance,false,steepDownwardCamera,false);
    const auto population=runtime.missionEnvironment?
        storylandNativePopulationAt(*runtime.missionEnvironment,focus,runtime.clockHour):StorylandLcsPopulation{};
    const float densityMultiplier=runtime.missionEnvironment&&std::isfinite(runtime.missionEnvironment->carDensityMultiplier)?
        std::max(0.0f,runtime.missionEnvironment->carDensityMultiplier):0.0f;
    size_t protectedCount=0;
    for(size_t slot=0;slot<cars.size()&&slot<runtime.vehicles.size();++slot) {
        const auto& car=runtime.vehicles[slot];
        if(car.serviceAutopilot||car.coopControlled||car.scriptHandle>=0||car.scriptHandle==StorylandVcsPoliceHelicopterHandle||car.generator>=0||car.savedGarage>=0||car.craneCarried||
            int(slot)==runtime.occupiedVehicle||int(slot)==runtime.transitionVehicle)++protectedCount;
    }
    const size_t available=runtime.vehicles.size()>protectedCount?runtime.vehicles.size()-protectedCount:0;
    size_t densityLimit=population.vcs?storylandVcsVehicleTarget(population,densityMultiplier,
        protectedCount,std::min(maximum,available)):
        storylandPopulationDensityTarget(population.available,population.values.cars[0],
            densityMultiplier,std::min(maximum,available));
    const bool policeResponse=runtime.wanted>0&&!runtime.policeIgnorePlayer&&runtime.playerControl&&runtime.visibleArea==0;
    const auto responseBudget=storylandPoliceBudget(policeResponse?runtime.wanted:0,paths.vcs);
    const size_t responseCapacity=population.vcs?std::min(available,protectedCount<16?16-protectedCount:0):available;
    densityLimit=std::max(densityLimit,std::min(responseCapacity,size_t(responseBudget.cars)));
    if(population.vcs) {
        size_t ambient=0;
        bool replacement=false;
        for(size_t slot=0;slot<cars.size()&&slot<runtime.vehicles.size();++slot) {
            const auto& vehicle=runtime.vehicles[slot];
            if(vehicle.serviceAutopilot||vehicle.coopControlled||vehicle.scriptHandle>=0||vehicle.scriptHandle==StorylandVcsPoliceHelicopterHandle||vehicle.generator>=0||vehicle.savedGarage>=0||vehicle.craneCarried||
                int(slot)==runtime.occupiedVehicle||int(slot)==runtime.transitionVehicle||
                int(slot)==storylandRemoteVehicleSlot(runtime))continue;
            ++ambient;
            const bool onScreen=runtime.missionEnvironment&&runtime.missionEnvironment->sphereOnScreen&&
                runtime.missionEnvironment->sphereOnScreen({cars[slot].position.x,cars[slot].position.y,
                    cars[slot].position.z+1},2.0f);
            const float range=std::hypot(cars[slot].position.x-focus.x,cars[slot].position.y-focus.y);
            if(!onScreen&&(!std::isfinite(range)||range>removalDistance))replacement=true;
        }
        if(!replacement&&(ambient>=densityLimit||cars.size()>=runtime.vehicles.size()))return;
    }
    if(population.vcs&&!generation.velocityDirection&&!cameraDirectionValid)return;
    const auto road=population.vcs?storylandVcsTrafficCreationCandidates(paths,focus,direction,generation,false,
        runtime.vehicles.size()*2):storylandCreateNativeTraffic(paths,focus,true,runtime.vehicles.size()*2,20.0f,160.0f,false);
    const auto water=population.vcs?storylandVcsTrafficCreationCandidates(paths,focus,direction,generation,true,
        runtime.vehicles.size()*2):storylandCreateNativeTraffic(paths,focus,true,runtime.vehicles.size()*2,25.0f,160.0f,true);
    uint64_t roadWeight=0,waterWeight=0;
    if(!road.empty())for(size_t model:runtime.roadModels)
        if(model<runtime.modelDefinitions.size())roadWeight+=runtime.modelDefinitions[model].frequency;
    if(!water.empty())for(size_t model:runtime.waterModels)
        if(model<runtime.modelDefinitions.size())waterWeight+=runtime.modelDefinitions[model].frequency;
    // VCS chooses the vehicle class after the path domain is known. Resident
    // boat frequencies must not take away slots from the road population.
    if(population.vcs) {roadWeight=road.size();waterWeight=water.size();}
    const uint64_t totalWeight=roadWeight+waterWeight;
    const size_t waterTarget=totalWeight?size_t(uint64_t(densityLimit)*waterWeight/totalWeight):0;
    const size_t roadTarget=roadWeight?densityLimit-waterTarget:0;
    unsigned creationAttempts=0;
    bool viceCreationUnavailable=pedestrians==nullptr;
    const auto place=[&](size_t index,bool boat) {
        if(population.vcs&&creationAttempts++>=6)return false;
        // Native430988..4309A0 rejects only an ambient water-node creation attempt.
        if(paths.vcs&&boat&&runtime.missionEnvironment&&
            !runtime.missionEnvironment->vcsRandomBoatsEnabled)return false;
        const auto& candidates=boat?water:road;
        const auto& models=boat?runtime.waterModels:runtime.roadModels;
        if(models.empty()||index>=runtime.vehicles.size()||
            (population.vcs?densityLimit==0:(boat?waterTarget:roadTarget)==0))return false;
        size_t selectedModel=models[index%models.size()];
        int nativeClass=-1;
        if(population.available&&!paths.vcs) {
            size_t nearby=0;
            for(size_t other=0;other<cars.size()&&other<runtime.vehicles.size();++other)
                if(other!=index&&cars[other].water==boat&&runtime.vehicles[other].scriptHandle<0&&
                    runtime.vehicles[other].generator<0&&distance(cars[other].position,player)<=160.0f)++nearby;
            if(nearby>=(boat?waterTarget:roadTarget))return false;
            const auto random=[&] {return storylandNativeRandom(runtime.populationRandom);};
            selectedModel=SIZE_MAX;
            int attempts=10;
            for(;attempts>0&&selectedModel==SIZE_MAX;--attempts) {
                const int kind=boat?-1:storylandLcsPopulationKind(population.values.cars,random());
                int civilianClass=-1;
                if(kind<0)civilianClass=boat?9+storylandLcsPopulationClass(population.values.boats,random()):
                    storylandLcsPopulationClass(population.values.civilianCars,random());
                const int32_t gangModel=kind>=0&&kind<9?runtime.missionEnvironment->gangs[size_t(kind)].carModel:0;
                const bool configuredGang=kind>=0&&kind<9&&gangModel>0;
                std::vector<size_t> choices;
                uint64_t totalWeight=0;
                for(size_t model:models)if(model<runtime.modelDefinitions.size()) {
                    const auto& definition=runtime.modelDefinitions[model];
                    const bool matches=kind==9?definition.vehicleClass==11:
                        kind>=0?(gangModel>0?definition.modelId==uint32_t(gangModel):definition.vehicleClass==12+kind):
                        definition.vehicleClass==civilianClass;
                    const uint16_t weight=configuredGang?1:definition.frequency;
                    if(matches&&weight>0) {choices.push_back(model);totalWeight+=weight;}
                }
                if(choices.empty())continue;
                uint64_t choice=random()%totalWeight;
                for(size_t model:choices) {
                    const uint16_t frequency=configuredGang?1:runtime.modelDefinitions[model].frequency;
                    if(choice<frequency) {selectedModel=model;break;}
                    choice-=frequency;
                }
            }
            if(selectedModel==SIZE_MAX)return false;
        }
        if(paths.vcs) {
            const bool canCreateViceCrew=!viceCreationUnavailable&&pedestrians&&runtime.missionEnvironment&&
                pedestrians->size()+runtime.missionEnvironment->missionCharacters.size()<runtime.pedestrians.size();
            const auto choice=storylandChooseVcsPopulationVehicle(runtime,population,boat,
                runtime.populationRandom,world,canCreateViceCrew);
            if(choice.model==SIZE_MAX)return false;
            selectedModel=choice.model;nativeClass=choice.nativeClass;
        }
        if(policeResponse&&!boat) {
            size_t officersInCars=0;
            for(size_t other=0;other<cars.size()&&other<runtime.vehicles.size();++other) {
                if(other==index)continue;
                const auto& present=runtime.vehicles[other];
                if(present.health>0&&present.hasDriver&&present.scriptHandle<0&&
                    present.scriptHandle!=StorylandVcsPoliceHelicopterHandle&&
                    storylandLawEnforcementRole(storylandVehicleServiceRole(runtime,present)))++officersInCars;
            }
            if(officersInCars<responseBudget.cars) {
                if(paths.vcs) {
                    const bool canCreateViceCrew=!viceCreationUnavailable&&pedestrians&&runtime.missionEnvironment&&
                        pedestrians->size()+runtime.missionEnvironment->missionCharacters.size()<runtime.pedestrians.size();
                    const uint32_t model=storylandVcsPopulationPoliceModel(runtime,runtime.populationRandom,canCreateViceCrew);
                    const size_t resident=storylandResidentPopulationVehicle(runtime,model);
                    if(resident!=SIZE_MAX) {selectedModel=resident;nativeClass=model==191?24:13;}
                } else {
                    const auto wantedRole=storylandWantedPoliceRole(runtime.wanted);
                    std::vector<size_t> responseModels;
                    for(size_t model:models)if(model<runtime.modelServiceRoles.size()&&
                        runtime.modelServiceRoles[model]==wantedRole&&model<runtime.modelDefinitions.size()&&
                        storylandChooseVehicleDriver(wantedRole,runtime.modelDefinitions[model].vehicleClass,
                            runtime.pedestrianModelStats,runtime.pedestrianModelRoles,runtime.populationRandom)!=SIZE_MAX)
                        responseModels.push_back(model);
                    if(!responseModels.empty())selectedModel=responseModels[(runtime.vehicleRespawns+index)%responseModels.size()];
                }
            }
        }
        const size_t candidateCount=population.vcs?std::min(size_t(1),candidates.size()):candidates.size();
        for(size_t attempt=0;attempt<candidateCount;++attempt) {
            const size_t ordinal=population.vcs?(uint64_t(frame)*6+creationAttempts-1)%candidates.size():attempt;
            const auto& candidate=candidates[ordinal];
            if(population.vcs&&candidate.node<paths.nodes.size()&&candidate.nextNode<paths.nodes.size()&&
                (storylandNativeRandom(runtime.populationRandom)&15u)>
                    storylandVcsTrafficSpawnRate(paths.nodes[candidate.node].flags,paths.nodes[candidate.nextNode].flags))continue;
            if(runtime.missionEnvironment) {
                if(!population.vcs&&runtime.missionEnvironment->sphereOnScreen&&runtime.missionEnvironment->sphereOnScreen(
                    {candidate.position.x,candidate.position.y,candidate.position.z+1},2.0f))continue;
                const float rate=storylandZonePopulationScale(*runtime.missionEnvironment,candidate.position,true);
                if(float((candidate.node*2654435761u+uint32_t(index)*2246822519u)%65536u)/65536.0f>=rate)continue;
            }
            bool occupied=false;
            const float clearance=population.vcs?storylandVcsTrafficCreationClearance(boat):18.0f;
            const auto within=[&](const StorylandNativePoint& point) {
                return population.vcs?std::hypot(point.x-candidate.position.x,point.y-candidate.position.y)<clearance:
                    distance(point,candidate.position)<clearance;
            };
            for(size_t other=0;other<cars.size();++other) {
                if(other!=index&&within(cars[other].position)) {occupied=true;break;}
            }
            if(population.vcs&&!occupied) {
                occupied=runtime.occupiedVehicle<0&&within(player);
                for(size_t other=0;!occupied&&other<runtime.movementPedestrians.size()&&other<runtime.pedestrians.size();++other)
                    if(runtime.pedestrians[other].scriptHandle<0&&!runtime.pedestrians[other].serviceVehicleIdentity&&runtime.pedestrians[other].renderAlpha>0&&
                        within(runtime.movementPedestrians[other]))occupied=true;
                if(runtime.missionEnvironment)for(const auto& entry:runtime.missionEnvironment->missionCharacters)
                    if(!entry.second.removeFromWorld&&within({entry.second.x,entry.second.y,entry.second.z}))occupied=true;
            }
            if(occupied)continue;
            auto placed=candidate;
            StorylandRuntimeVehicle spawnedVehicle;
            spawnedVehicle.model=selectedModel;spawnedVehicle.speed=4.0f;
            if(selectedModel<runtime.modelDefinitions.size())
                storylandApplyVehicleDefinition(spawnedVehicle,runtime.modelDefinitions[selectedModel]);
            if(world&&!boat) {
                if(population.vcs) {
                    if(!storylandPlaceVcsTrafficVehicle(spawnedVehicle,placed,*world))continue;
                } else {
                    const StorylandNativePoint top{placed.position.x,placed.position.y,placed.position.z+1.25f};
                    const StorylandNativePoint bottom{top.x,top.y,placed.position.z-2.0f};
                    uint8_t surface=0;
                    const float hit=storylandTraceNativeGround(*world,top,bottom,surface);
                    if(hit>=1)continue;
                    const float ground=top.z+(bottom.z-top.z)*hit;
                    placed.groundOffset+=ground-placed.position.z;
                    placed.position.z=ground;
                }
            }
            if(population.vcs&&runtime.missionEnvironment&&runtime.missionEnvironment->sphereOnScreen&&
                runtime.missionEnvironment->sphereOnScreen(
                    {placed.position.x,placed.position.y,placed.position.z+1},2.0f))continue;
            const bool vice=paths.vcs&&nativeClass==13&&runtime.missionEnvironment&&
                selectedModel<runtime.modelDefinitions.size()&&runtime.modelDefinitions[selectedModel].modelId==204;
            const bool replacing=index<cars.size();
            StorylandNativeTrafficActor previousActor;
            StorylandRuntimeVehicle previousVehicle;
            if(vice&&replacing) {previousActor=cars[index];previousVehicle=runtime.vehicles[index];}
            if(index==cars.size()) cars.push_back(placed);
            else if(index<cars.size()) cars[index]=placed;
            else return false;
            auto& vehicle=runtime.vehicles[index];
            vehicle=std::move(spawnedVehicle);
            if(vehicle.model<runtime.modelDefinitions.size()) {
                const auto& definition=runtime.modelDefinitions[vehicle.model];
                if(nativeClass>=0)vehicle.nativeDefinition.vehicleClass=int8_t(nativeClass);
                if(vice) {
                    vehicle.vcsViceUnit=true;
                    vehicle.vcsViceCrewSequence=uint8_t(runtime.missionEnvironment->vcsViceDispatch.crewSequence);
                }
                if(!storylandAssignVehicleDriver(runtime,vehicle,runtime.vehicleRespawns+uint32_t(index))) {
                    vehicle.hasDriver=false;
                    vehicle.parked=true;
                    vehicle.speed=0;
                }
                if(vehicle.vcsTrafficRail.active) {
                    const int rating=nativeClass>=0?nativeClass:definition.vehicleClass;
                    const bool simple=StorylandVcsTrafficSupport::simpleSpawn(rating,boat,policeResponse?3:1);
                    const bool mad=StorylandVcsTrafficSupport::madDriver(definition.motorcycle()?2:1,
                        storylandNativeRandom(runtime.populationRandom),false);
                    vehicle.vcsTrafficRail.active=simple&&!mad&&vehicle.hasDriver;
                }
                if(!definition.colors.empty()) {
                    const size_t selected = (runtime.vehicleRespawns+index)%definition.colors.size();
                    const auto& pair=definition.colors[selected];
                    vehicle.colorSet = int32_t(selected);
                    vehicle.primaryColor=pair[0];vehicle.secondaryColor=pair[1];
                }
            }
            if(!vehicle.nativeDefinition.hasNativeWheelDimensions&&vehicle.model<runtime.modelWheelRadii.size()&&runtime.modelWheelRadii[vehicle.model]>0)
                vehicle.wheelRadius=runtime.modelWheelRadii[vehicle.model];
            if(vehicle.model<runtime.modelExtents.size()) {
                vehicle.halfWidth=runtime.modelExtents[vehicle.model][0];
                vehicle.halfLength=runtime.modelExtents[vehicle.model][1];
                if(runtime.modelExtents[vehicle.model][2]>.3f)vehicle.bodyHeight=runtime.modelExtents[vehicle.model][2];
            }
            if(vice) {
                if(!pedestrians||!storylandCompleteVcsViceCrew(runtime,*pedestrians,cars,index)) {
                    if(replacing) {cars[index]=previousActor;runtime.vehicles[index]=previousVehicle;}
                    else {cars.pop_back();runtime.vehicles[index]=StorylandRuntimeVehicle{};}
                    viceCreationUnavailable=true;
                    return false;
                }
            }
            for (auto& contact : runtime.vehicles) {
                if (contact.lastCollidedSlot == int32_t(index)) contact.lastCollidedSlot = -1;
                if (contact.pendingContactSlot == int32_t(index)) {
                    contact.pendingContactSlot = -1;contact.pendingBodyImpact = false;
                }
            }
            ++runtime.vehicleRespawns;
            return true;
        }
        return false;
    };
    size_t roadCount=0,waterCount=0;
    for(size_t index=0;index<cars.size()&&index<runtime.vehicles.size();++index) {
        const bool boat=cars[index].water;
        const int32_t scriptHandle=runtime.vehicles[index].scriptHandle;
        const auto& live=runtime.vehicles[index];
        if(scriptHandle<0&&scriptHandle!=StorylandVcsPoliceHelicopterHandle&&
            live.generator<0&&live.savedGarage<0&&!live.craneCarried&&
            int(index)!=runtime.occupiedVehicle&&int(index)!=runtime.transitionVehicle) {
            if(boat)++waterCount;else ++roadCount;
        }
        bool missionOwned=scriptHandle>=0;
        if(missionOwned&&runtime.missionEnvironment) {
            const auto found=runtime.missionEnvironment->missionVehicles.find(scriptHandle);
            if(found!=runtime.missionEnvironment->missionVehicles.end())missionOwned=found->second.persistent&&!found->second.deleted;
            for(const auto& character:runtime.missionEnvironment->missionCharacters)
                if(character.second.persistent&&character.second.alive&&character.second.vehicle==scriptHandle)missionOwned=true;
        }
        if(runtime.vehicles[index].serviceAutopilot||int(index)==runtime.occupiedVehicle||int(index)==runtime.transitionVehicle||
            int(index)==storylandRemoteVehicleSlot(runtime)||scriptHandle==StorylandVcsPoliceHelicopterHandle||runtime.vehicles[index].generator>=0||
            missionOwned||runtime.vehicles[index].savedGarage>=0||craneOwnsVehicle(runtime,runtime.vehicles[index]))continue;
        const bool onScreen=runtime.missionEnvironment&&runtime.missionEnvironment->sphereOnScreen&&
            runtime.missionEnvironment->sphereOnScreen({cars[index].position.x,cars[index].position.y,
                cars[index].position.z+1},2.0f);
        const float range=population.vcs?std::hypot(cars[index].position.x-focus.x,cars[index].position.y-focus.y):
            distance(cars[index].position,focus);
        const bool expired=population.vcs?range>removalDistance:
            range>160.0f||(runtime.vehicles[index].stoppedSeconds>12.0f&&
                !runtime.vehicles[index].waitingForSignal&&range>35.0f);
        if(!onScreen&&(!std::isfinite(range)||expired)) {
            if(place(index,boat)&&scriptHandle>=0&&runtime.missionEnvironment) {
                const auto found=runtime.missionEnvironment->missionVehicles.find(scriptHandle);
                if(found!=runtime.missionEnvironment->missionVehicles.end()) {
                    found->second.deleted=true;found->second.alive=false;found->second.spawnedSlot=-1;
                }
            }
        }
    }
    if(population.vcs) {
        size_t count=roadCount+waterCount;
        while(count<densityLimit&&cars.size()<runtime.vehicles.size()&&creationAttempts<6) {
            if(road.empty()&&water.empty())break;
            const bool boat=road.empty()||(!water.empty()&&
                storylandNativeRandomRange(runtime.populationRandom,uint32_t(road.size()+water.size()))>=road.size());
            if(place(cars.size(),boat))++count;
        }
        return;
    }
    while(roadCount<roadTarget&&cars.size()<runtime.vehicles.size()) {
        if(!place(cars.size(),false)) break;
        ++roadCount;
    }
    while(waterCount<waterTarget&&cars.size()<runtime.vehicles.size()) {
        if(!place(cars.size(),true)) break;
        ++waterCount;
    }
}
static int storylandPatrolFaction(const StorylandRuntimePed& ped) {
    if (ped.scriptHandle >= 0 || ped.serviceVehicleIdentity || ped.turfWarActorId ||
        ped.serviceRole != StorylandServiceRole::Civilian || ped.policePursuing ||
        ped.health <= 0 || ped.renderAlpha == 0 || ped.populationFadeOut || ped.water.active ||
        ped.hostile || ped.fleeActive || ped.zombie.active) return -1;
    if (ped.extendedGangIdentity >= 0) return 100 + ped.extendedGangIdentity;
    return ped.stats.valid && ped.stats.pedType >= 7 && ped.stats.pedType <= 15 ?
        ped.stats.pedType - 7 : -1;
}

static uint64_t storylandAllocatePatrolGroup(StorylandPlayTestRuntime& runtime) {
    for (;;) {
        const uint64_t group = runtime.nextPatrolGroup++;
        if (!group) continue;
        const bool used = std::any_of(runtime.pedestrians.begin(), runtime.pedestrians.end(),
            [&](const auto& ped) { return ped.patrol.group == group; });
        if (!used) return group;
    }
}

void storylandUpdateAmbientPatrols(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld* world, const std::vector<StorylandNativeTrafficActor>& actors) {
    if (!runtime.crime.vcs) {
        for (auto& ped : runtime.pedestrians) ped.patrol.clear();
        return;
    }
    const size_t count = std::min(actors.size(), runtime.pedestrians.size());
    for (size_t index = 0; index < count; ++index) {
        auto& ped = runtime.pedestrians[index];
        ped.patrol.running = false;
        if (storylandPatrolFaction(ped) < 0 || actors[index].vehicle || actors[index].water ||
            storylandPlayerReactionHoldsTarget(runtime.playerReaction,
                StorylandReactionTargetKind::Ambient, int32_t(index), ped.stats.modelId)) ped.patrol.clear();
    }
    for (size_t index = 0; index < count; ++index) {
        const uint64_t group = runtime.pedestrians[index].patrol.group;
        if (!group) continue;
        bool processed = false;
        for (size_t before = 0; before < index; ++before)
            if (runtime.pedestrians[before].patrol.group == group) { processed = true; break; }
        if (processed) continue;
        std::vector<size_t> members;
        for (size_t other = index; other < count; ++other)
            if (runtime.pedestrians[other].patrol.group == group) members.push_back(other);
        if (members.size() < 2) { runtime.pedestrians[index].patrol.clear(); continue; }
        const auto leader = std::find_if(members.begin(), members.end(),
            [&](size_t slot) { return runtime.pedestrians[slot].patrol.role == 0; });
        const size_t leaderSlot = leader == members.end() ? members.front() : *leader;
        const int faction = storylandPatrolFaction(runtime.pedestrians[leaderSlot]);
        uint8_t role = 1;
        for (const size_t slot : members) {
            auto& member = runtime.pedestrians[slot];
            if (storylandPatrolFaction(member) != faction ||
                distance(actors[slot].position, actors[leaderSlot].position) > 25.0f) {
                member.patrol.clear(); continue;
            }
            member.patrol.role = slot == leaderSlot ? 0 : role++;
        }
    }
    if (!world || runtime.elapsed < runtime.patrolRecruitAt ||
        (runtime.missionEnvironment && runtime.missionEnvironment->cutscene.playing)) return;
    runtime.patrolRecruitAt = runtime.elapsed + 1.0f;
    for (size_t leader = 0; leader < count; ++leader) {
        auto& first = runtime.pedestrians[leader];
        const int faction = storylandPatrolFaction(first);
        if (faction < 0 || first.patrol.group || actors[leader].vehicle || actors[leader].water) continue;
        std::array<size_t,3> members{leader, SIZE_MAX, SIZE_MAX};
        size_t found = 1;
        for (size_t slot = leader + 1; slot < count && found < members.size(); ++slot) {
            const auto& ped = runtime.pedestrians[slot];
            if (ped.patrol.group || storylandPatrolFaction(ped) != faction ||
                actors[slot].vehicle || actors[slot].water ||
                distance(actors[leader].position, actors[slot].position) > 12.0f ||
                std::fabs(actors[leader].position.z - actors[slot].position.z) > 1.0f) continue;
            auto from = actors[leader].position, to = actors[slot].position;
            from.z += 1.0f; to.z += 1.0f;
            if (storylandTraceNativeSegment(*world, from, to) < .99f) continue;
            members[found++] = slot;
        }
        if (found != members.size()) continue;
        const uint64_t group = storylandAllocatePatrolGroup(runtime);
        for (size_t role = 0; role < members.size(); ++role) {
            auto& member = runtime.pedestrians[members[role]];
            member.patrol = {group, uint8_t(role), storylandNativeRandom(runtime.populationRandom), 0, false};
        }
    }
}

void storylandRefreshLcsPedestrians(StorylandPlayTestRuntime& runtime,const StorylandNativePaths& paths,
    const StorylandNativeWorld& world,std::vector<StorylandNativeTrafficActor>& pedestrians,
    const StorylandNativePoint& player,float cameraYaw) {
    if(runtime.paused||runtime.pauseMenu||runtime.health<=0||runtime.visibleArea!=0||
        !runtime.missionEnvironment||runtime.pedestrianModelStats.empty()||
        (!paths.vcs&&runtime.elapsed<runtime.nextLcsPedestrianRefresh))return;
    runtime.nextLcsPedestrianRefresh=runtime.elapsed+0.8f;
    const auto population=storylandNativePopulationAt(*runtime.missionEnvironment,player,runtime.clockHour);
    if(!population.available)return;
    const size_t target=storylandAmbientPedestrianTarget(runtime,player);
    size_t nearby=0,slot=SIZE_MAX;
    for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size();++index) {
        const auto& ped=runtime.pedestrians[index];
        const float range=distance(pedestrians[index].position,player);
        if(ped.scriptHandle<0&&!ped.serviceVehicleIdentity&&ped.renderAlpha>0&&range<115.0f)++nearby;
        const bool onScreen=runtime.missionEnvironment->sphereOnScreen&&
            runtime.missionEnvironment->sphereOnScreen({pedestrians[index].position.x,pedestrians[index].position.y,
                pedestrians[index].position.z+.9f},.9f);
        if(ped.scriptHandle<0&&!ped.serviceVehicleIdentity&&ped.turfWarActorId==0&&int(index)!=runtime.targetedPed&&
            (ped.renderAlpha==0||(!ped.policePursuing&&!ped.hostile&&!storylandIsAdditionalFortBaxterPatrol(ped)&&
                range>120.0f&&!onScreen)))slot=index;
    }
    if(nearby>=target)return;
    if(slot==SIZE_MAX&&pedestrians.size()<runtime.pedestrians.size())slot=pedestrians.size();
    if(slot==SIZE_MAX)return;
    const auto random=[&] {return storylandNativeRandom(runtime.populationRandom);};
    const auto range=[&](uint32_t limit) {
        return paths.vcs?storylandNativeRandomRange(runtime.populationRandom,limit):random()%limit;
    };
    const auto candidates=storylandCreateNativeTraffic(paths,player,false,runtime.pedestrians.size(),40.0f,100.0f);
    const StorylandNativeZone* armyBase=nullptr;
    if(paths.vcs) {
        for(const auto& zone:world.zones.information)
            if(zone.name=="ARMYBAS") {armyBase=&zone;break;}
        if(!armyBase)armyBase=world.zones.named("ARMYB");
    }

    // Choose the population table and PEDGRP for the actual spawn point. The
    // player's zone can be Fort Baxter while the candidate is on a public
    // pavement on the opposite side of the base wall.
    const auto selectPedForPosition=[&](const StorylandNativePoint& position,
        StorylandRuntimePed& spawned)->bool {
        const auto localPopulation=storylandNativePopulationAt(*runtime.missionEnvironment,
            position,runtime.clockHour);
        if(!localPopulation.available)return false;
        int kind=storylandPopulationKind(localPopulation,false,random());
        if(paths.vcs&&runtime.extendedAndSaFeatures&&runtime.extendedGangSlotAt&&kind>=0&&kind<9) {
            const int32_t replacement=runtime.extendedGangSlotAt(position,kind);
            if((replacement>=0&&replacement<9)||
                (replacement==StorylandExtendedHaitianGang::populationKind&&runtime.extendedHaitianGang.available))
                kind=replacement;
        }
        if(!storylandArmyPopulationPositionAllowed(paths.vcs,kind,armyBase,position))
            kind=-1;

        const auto& mission=*runtime.missionEnvironment;
        const bool customGang=paths.vcs&&runtime.extendedAndSaFeatures&&
            kind==StorylandExtendedHaitianGang::populationKind&&runtime.extendedHaitianGang.available;
        const bool nativeGang=kind>=0&&kind<9;
        std::vector<size_t> models;
        const auto addModel=[&](uint32_t modelId) {
            for(size_t index=0;index<runtime.pedestrianModelStats.size();++index) {
                const auto& stats=runtime.pedestrianModelStats[index];
                if(stats.valid&&stats.modelId==modelId)models.push_back(index);
            }
        };
        if(nativeGang||customGang) {
            const auto& gangModels=customGang?runtime.extendedHaitianGang.pedModels:
                mission.gangs[size_t(kind)].pedModels;
            for(int32_t modelId:gangModels)
                if(modelId>0)addModel(uint32_t(modelId));
        } else if(kind==9) {
            for(size_t index=0;index<runtime.pedestrianModelStats.size();++index) {
                const auto& stats=runtime.pedestrianModelStats[index];
                const bool ordinaryVcsPolice=index<runtime.pedestrianModelRoles.size()&&
                    runtime.pedestrianModelRoles[index]==StorylandServiceRole::Police;
                if(stats.valid&&(paths.vcs?ordinaryVcsPolice:stats.pedType==6))
                    models.push_back(index);
            }
        } else {
            const size_t group=localPopulation.values.pedestrianGroup;
            if(group<runtime.lcsPedestrianGroups.size()) {
                // Repeated entries in the retail group are selection weights.
                // Consider all *resident* models before choosing, instead of
                // choosing one unloaded ID and repeatedly failing to spawn.
                for(uint32_t modelId:runtime.lcsPedestrianGroups[group]) {
                    for(size_t index=0;index<runtime.pedestrianModelStats.size();++index) {
                        const auto& stats=runtime.pedestrianModelStats[index];
                        const bool civilianRole=!paths.vcs||
                            (index<runtime.pedestrianModelRoles.size()&&
                             runtime.pedestrianModelRoles[index]==StorylandServiceRole::Civilian);
                        if(stats.valid&&stats.modelId==modelId&&civilianRole&&
                            (stats.pedType==4||stats.pedType==5||
                             stats.pedType==18||stats.pedType==20))
                            models.push_back(index);
                    }
                }
            } else if(!paths.vcs) {
                for(size_t index=0;index<runtime.pedestrianModelStats.size();++index) {
                    const auto& stats=runtime.pedestrianModelStats[index];
                    if(stats.valid&&(stats.pedType==4||stats.pedType==5||
                                    stats.pedType==18||stats.pedType==20))
                        models.push_back(index);
                }
            }
        }
        if(models.empty())return false;

        std::vector<size_t> nearbyModels;
        nearbyModels.reserve(pedestrians.size());
        for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size();++index) {
            if(index==slot||runtime.pedestrians[index].renderAlpha==0||
                runtime.pedestrians[index].health<=0)continue;
            if(std::hypot(pedestrians[index].position.x-position.x,
                          pedestrians[index].position.y-position.y)<70.0f)
                nearbyModels.push_back(runtime.pedestrians[index].model);
        }
        size_t selected=SIZE_MAX;
        if(paths.vcs&&kind<0) {
            std::vector<uint32_t> nearbyModelIds;
            nearbyModelIds.reserve(pedestrians.size());
            for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size();++index) {
                const auto& existing=runtime.pedestrians[index];
                if(index!=slot&&existing.renderAlpha>0&&existing.health>0&&
                    existing.stats.valid&&distance(pedestrians[index].position,player)<115.0f)
                    nearbyModelIds.push_back(existing.stats.modelId);
            }
            selected=storylandChooseVcsAmbientCivilian(
                runtime.lcsPedestrianGroups[localPopulation.values.pedestrianGroup],
                runtime.pedestrianModelStats,runtime.pedestrianModelRoles,nearbyModelIds,random());
        } else selected=storylandSelectVariedPedestrianModel(models,nearbyModels,random());
        if(selected==SIZE_MAX)return false;
        spawned={};
        spawned.model=selected;
        spawned.stats=runtime.pedestrianModelStats[selected];
        if(nativeGang||customGang) {
            spawned.stats.pedType=customGang?StorylandExtendedHaitianGang::pedestrianType:7+kind;
            spawned.extendedGangIdentity=customGang?StorylandExtendedHaitianGang::populationKind:-1;
            spawned.fortBaxterSoldier=paths.vcs&&!customGang&&
                kind==StorylandFortBaxterRetailResponse::GangId;
            spawned.health=float(customGang?runtime.extendedHaitianGang.health:
                mission.gangs[size_t(kind)].health);
            spawned.armour=float(customGang?runtime.extendedHaitianGang.armour:
                mission.gangs[size_t(kind)].armour);
            const auto& weapons=customGang?runtime.extendedHaitianGang.weapons:
                mission.gangs[size_t(kind)].weapons;
            const int32_t nativeWeapon=weapons[range(2)];
            bool weaponAvailable=false;
            for(size_t weapon=0;weapon<runtime.weapons.size();++weapon)
                if(runtime.weapons[weapon].nativeId==nativeWeapon) {
                    spawned.weapon=weapon;
                    spawned.magazine=runtime.weapons[weapon].clipSize;
                    weaponAvailable=true;
                    break;
                }
            if(!weaponAvailable)return false;
        } else if(kind==9) {
            spawned.serviceRole=selected<runtime.pedestrianModelRoles.size()?
                runtime.pedestrianModelRoles[selected]:StorylandServiceRole::Police;
            storylandInitializePoliceLoadout(runtime,spawned);
        }
        return true;
    };
    for(auto candidate:candidates) {
        const float angle=std::atan2(candidate.position.y-player.y,candidate.position.x-player.x)/radians;
        if(std::fabs(std::remainder(angle-cameraYaw,360.0f))<100.0f&&
            storylandTraceNativeSegment(world,{player.x,player.y,player.z+1},
                {candidate.position.x,candidate.position.y,candidate.position.z+1})>=0.99f)continue;
        bool occupied=false;
        for(size_t index=0;index<pedestrians.size();++index)
            if(index!=slot&&index<runtime.pedestrians.size()&&runtime.pedestrians[index].renderAlpha>0&&
                distance(pedestrians[index].position,candidate.position)<3.0f) {occupied=true;break;}
        if(occupied)continue;
        uint8_t surface=0;
        const StorylandNativePoint top{candidate.position.x,candidate.position.y,candidate.position.z+1},
            bottom{candidate.position.x,candidate.position.y,candidate.position.z-2};
        const float hit=storylandTraceNativeGround(world,top,bottom,surface);
        if(hit>=1)continue;
        candidate.position.z=top.z+(bottom.z-top.z)*hit;
        const auto placement=storylandMoveNativeBody(world,candidate.position,candidate.position,storylandNativeCharacterBody());
        if(!placement.clear)continue;
        candidate.position=placement.position;
        StorylandRuntimePed spawned;
        if(!selectPedForPosition(candidate.position,spawned))continue;
        std::array<size_t,3> squadSlots{slot,SIZE_MAX,SIZE_MAX};
        std::array<StorylandNativeTrafficActor,3> squadActors{candidate,candidate,candidate};
        std::array<StorylandRuntimePed,3> squadPeds{spawned,spawned,spawned};
        bool squad = paths.vcs && storylandPatrolFaction(spawned) >= 0 && nearby + 3 <= target &&
            !runtime.missionEnvironment->introMissionProtected;
        size_t appendSlot = pedestrians.size() + (slot == pedestrians.size() ? 1 : 0);
        for (size_t role = 1; role < 3 && squad; ++role) {
            if (appendSlot < runtime.pedestrians.size()) squadSlots[role] = appendSlot++;
            else {
                for (size_t other = 0; other < pedestrians.size(); ++other) {
                    const auto& ped = runtime.pedestrians[other];
                    if (other == slot || other == squadSlots[1] || ped.scriptHandle >= 0 ||
                        ped.serviceVehicleIdentity || ped.turfWarActorId || int(other) == runtime.targetedPed ||
                        ped.renderAlpha != 0) continue;
                    squadSlots[role] = other; break;
                }
            }
            if (squadSlots[role] == SIZE_MAX) { squad = false; break; }
            const uint32_t variation = random();
            const auto formation = storylandVcsFormationTarget(int32_t(role),
                {candidate.position.x,candidate.position.y,candidate.position.z},candidate.yaw-90,variation);
            auto& follower = squadActors[role];
            follower.position = {formation[0],formation[1],formation[2]};
            uint8_t followerSurface = 0;
            StorylandNativePoint normal{};
            const StorylandNativePoint high{formation[0],formation[1],candidate.position.z+1},
                low{formation[0],formation[1],candidate.position.z-1};
            const float floor = storylandTraceNativeGround(world,high,low,followerSurface,&normal);
            if (floor >= 1 || floor < 0 || !std::isfinite(floor) || normal.z < .65f) { squad=false; break; }
            follower.position.z = high.z + (low.z-high.z)*floor;
            if (std::fabs(follower.position.z-candidate.position.z)>1 ||
                !storylandNativeBodyClear(world,follower.position,storylandNativeCharacterBody()) ||
                storylandTraceNativeSegment(world,{candidate.position.x,candidate.position.y,candidate.position.z+1},
                    {follower.position.x,follower.position.y,follower.position.z+1}) < .99f) { squad=false; break; }
            for(size_t other=0;other<pedestrians.size();++other)
                if(other!=slot&&runtime.pedestrians[other].renderAlpha>0&&
                    distance(follower.position,pedestrians[other].position)<1.0f) {squad=false;break;}
            if (distance(follower.position, player) < 2.0f) squad=false;
            for (size_t previous=0;previous<role&&squad;++previous)
                if (distance(follower.position,squadActors[previous].position)<1.0f) squad=false;
            for (const auto& entry:runtime.missionEnvironment->missionCharacters) {
                const auto& character=entry.second;
                if (character.visible&&!character.removeFromWorld&&character.vehicle<0&&
                    distance(follower.position,{character.x,character.y,character.z})<1.0f) squad=false;
            }
            for (size_t car=0;car<runtime.vehiclePositions.size()&&car<runtime.vehicles.size();++car) {
                if (!runtime.vehicles[car].visible) continue;
                if (distance(follower.position,runtime.vehiclePositions[car])<
                    runtime.vehicles[car].halfLength+0.4f) squad=false;
            }
            if (!squad) break;
            follower.groundOffset += follower.position.z-candidate.position.z;
            follower.routeSeed ^= uint32_t(role)*0x9E3779B9u;
            auto& member = squadPeds[role];
            member.patrol.spacingRandom = variation;
            const int kind=storylandPatrolFaction(spawned);
            if(kind>=0&&kind<9) {
                const auto& group=runtime.missionEnvironment->gangs[size_t(kind)];
                const int32_t model=group.pedModels[role%2];
                for(size_t resident=0;resident<runtime.pedestrianModelStats.size();++resident)
                    if(runtime.pedestrianModelStats[resident].valid&&
                        int32_t(runtime.pedestrianModelStats[resident].modelId)==model) {
                        member.model=resident;member.stats=runtime.pedestrianModelStats[resident];
                        member.stats.pedType=spawned.stats.pedType;break;
                    }
            }
        }
        if (squad) {
            const uint64_t group=storylandAllocatePatrolGroup(runtime);
            for(size_t role=0;role<3;++role) {
                auto& member=squadPeds[role];
                member.patrol.group=group;member.patrol.role=uint8_t(role);
                if(squadSlots[role]>=pedestrians.size())pedestrians.resize(squadSlots[role]+1);
                pedestrians[squadSlots[role]]=squadActors[role];
                runtime.pedestrians[squadSlots[role]]=std::move(member);
            }
        } else {
            if(slot==pedestrians.size())pedestrians.push_back(candidate);else pedestrians[slot]=candidate;
            runtime.pedestrians[slot]=std::move(spawned);
        }
        return;
    }
}

void storylandAdvancePlayTestClock(StorylandPlayTestRuntime& runtime, float hours) {
    storylandAdvancePlayTestClockSeconds(runtime,double(hours)*3600.0);
}

void storylandAdvancePlayTestClockSeconds(StorylandPlayTestRuntime& runtime, double seconds) {
    const double passed=runtime.clockProgress.advance(runtime.clockHour,seconds);
    if (passed > 0) {
        auto& days = runtime.statistics["DaysPassed"];
        if (std::isfinite(days) && days >= 0 && days <= UINT32_MAX && std::floor(days) == days) {
            // Both the ordinary clock (0x228954) and skipped-time helper
            // (0x24D610) increment the same wrapping unsigned word.
            days = std::fmod(days + passed, 4294967296.0);
        } else {
            days += passed;
        }
    }
}

void storylandAdvanceVehicleAlarm(StorylandPlayTestRuntime& runtime,
    StorylandRuntimeVehicle& vehicle, const StorylandNativeTrafficActor& actor, size_t slot,
    uint32_t milliseconds) {
    if (vehicle.alarmMilliseconds == UINT16_MAX && int32_t(slot) == runtime.transitionVehicle &&
        (runtime.vehicleAction == StorylandVehicleAction::Enter || runtime.vehicleAction == StorylandVehicleAction::Hijack) &&
        runtime.vehiclePhase.phase != StorylandVehiclePhase::Approach) {
        vehicle.alarmMilliseconds = 15000;
    }
    if (!vehicle.alarmMilliseconds || vehicle.alarmMilliseconds == UINT16_MAX) return;
    const uint16_t before = vehicle.alarmMilliseconds;
    vehicle.alarmMilliseconds = uint16_t(milliseconds >= before ? 0 : before - milliseconds);
    if (before == 15000 || before / 1000 != vehicle.alarmMilliseconds / 1000) {
        StorylandPlayTestSoundEvent event(StorylandPlayTestSound::VehicleAlarm);
        event.position = actor.position;
        event.positioned = true;
        event.vehicle = int(slot);
        runtime.soundEvents.push_back(event);
    }
    vehicle.headlightsOn = vehicle.health > 0 && (vehicle.alarmMilliseconds / 500) % 2 != 0;
}

static bool storylandVcsPlayerHelicopter(const StorylandPlayTestRuntime& runtime,
    const StorylandRuntimeVehicle& vehicle, size_t slot) {
    const auto& definition=vehicle.nativeDefinition;
    if(!definition.vcsModelLayout||definition.nativeType!=4||
        definition.domain!=StorylandVehicleDomain::Air||int32_t(slot)!=runtime.occupiedVehicle||
        runtime.playerVehicleSeat>=0||vehicle.heliCrashing)return false;
    if(runtime.missionEnvironment) {
        const auto& environment=*runtime.missionEnvironment;
        if(vehicle.scriptHandle>=0&&environment.remoteVehicle==vehicle.scriptHandle)return false;
        const auto found=environment.missionVehicles.find(vehicle.scriptHandle);
        if(found!=environment.missionVehicles.end())
            return found->second.scriptStatus!=3&&(found->second.scriptStatus==0||!vehicle.aiActive);
    }
    return !vehicle.aiActive;
}

static bool storylandAdvanceVcsPlayerHelicopter(StorylandPlayTestRuntime& runtime,
    StorylandRuntimeVehicle& vehicle, StorylandNativeTrafficActor& actor,
    const StorylandNativeWorld& world, float seconds, std::string& error) {
    namespace H=StorylandVcsHelicopter;
    const auto& definition=vehicle.nativeDefinition;
    const auto fail=[&](const char* reason) {
        error="Native VCS player helicopter "+std::to_string(definition.modelId)+": "+reason;
        return false;
    };
    if(!runtime.missionEnvironment||!definition.hasNativeHandling||
        !definition.hasNativeFlyingHandling||!definition.collision)
        return fail("original handling, collision or mission input is unavailable");
    auto& environment=*runtime.missionEnvironment;
    const float nativeStep=environment.nativeScriptTimeStep;
    if(!std::isfinite(nativeStep)||nativeStep<0||!std::isfinite(seconds)||seconds<0)
        return fail("nonfinite or negative physical time step");
    if(!std::isfinite(vehicle.rotorSpeed)||!std::isfinite(vehicle.rotorAngle)||
        !std::isfinite(vehicle.health))return fail("nonfinite rotor or health state");
    if(nativeStep==0||vehicle.positionFrozen)return true;
    const float physicalSeconds=nativeStep/StorylandNativePhysicsRate;
    const auto matrix=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
        {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
        vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch),
        definition.previewRoot);
    if(!std::all_of(matrix.begin(),matrix.end(),[](float value){return std::isfinite(value);}))
        return fail("original entity frame is nonfinite");
    const H::Frame frame{{matrix[0],matrix[1],matrix[2]},
        {matrix[4],matrix[5],matrix[6]},{matrix[8],matrix[9],matrix[10]}};
    const float angle=actor.yaw*radians,c=std::cos(angle),s=std::sin(angle);
    const auto angular=vehicleAngularVelocity(vehicle,actor.yaw);
    H::State state{{(c*vehicle.speed-s*vehicle.lateralSpeed)/StorylandNativePhysicsRate,
        (s*vehicle.speed+c*vehicle.lateralSpeed)/StorylandNativePhysicsRate,
        vehicleVerticalVelocity(vehicle)/StorylandNativePhysicsRate},
        {angular.x/StorylandNativePhysicsRate,angular.y/StorylandNativePhysicsRate,
            angular.z/StorylandNativePhysicsRate}};
    H::Pad pad;
    if(runtime.playerControl&&runtime.health>0) {
        pad.horizontal=int16_t(storylandVcsPadState(environment,2));
        pad.vertical=int16_t(storylandVcsPadState(environment,3));
        pad.accelerator=uint8_t(std::clamp(storylandVcsPadState(environment,22),0,255));
        pad.brake=uint8_t(std::clamp(storylandVcsPadState(environment,1),0,255));
        pad.lookLeft=storylandVcsPadState(environment,8)!=0;
        pad.lookRight=storylandVcsPadState(environment,9)!=0;
        pad.stabilise=storylandVcsPadState(environment,12)!=0;
    }
    const auto input=H::controls(pad,frame,state.velocity,definition.flyingHandling,vehicle.health);
    updateVehicleWheelContacts(vehicle,actor,world,0);
    const unsigned contactCount=unsigned(std::count(vehicle.wheelContacts.begin(),
        vehicle.wheelContacts.end(),true));
    const bool holdingGroundRotor=vehicle.grounded&&
        (definition.handlingFlags&0x80000u)!=0&&input.thrust<=0;
    const bool rotorDisabled=vehicle.health<=0||vehicle.wrecked||vehicle.water.engineFlooded;
    if(holdingGroundRotor)vehicle.rotorSpeed=.15f;
    else if(!rotorDisabled)vehicle.rotorSpeed=H::occupiedRotor(vehicle.rotorSpeed);
    vehicle.rotorAngle=std::fmod(vehicle.rotorAngle+vehicle.rotorSpeed*nativeStep,
        6.283185307179586f);
    bool missionCreated=false;
    const auto scripted=environment.missionVehicles.find(vehicle.scriptHandle);
    if(scripted!=environment.missionVehicles.end())missionCreated=scripted->second.persistent;
    const bool powered=H::flightEnabled(vehicle.rotorSpeed,contactCount,input.thrust,state.velocity,
        rotorDisabled,holdingGroundRotor);
    if(!H::flyingForces(definition.flyingHandling,frame,
            definition.centerOfMass,vehicle.mass,
            definition.turnMass*vehicle.turnMassMultiplier*(vehicle.heavy?5.0f:1.0f),
            {runtime.windVelocity.x,runtime.windVelocity.y,runtime.windVelocity.z},
            missionCreated,matrix[14],input,powered?nativeStep:0,state))
        return fail("native mode-6 inputs or force result are invalid");

    const auto previous=actor;
    const float previousSpeed=vehicle.speed;
    vehicle.speed=(c*state.velocity[0]+s*state.velocity[1])*StorylandNativePhysicsRate;
    vehicle.lateralSpeed=(c*state.velocity[1]-s*state.velocity[0])*StorylandNativePhysicsRate;
    vehicle.verticalSpeed=state.velocity[2]*StorylandNativePhysicsRate;
    for(size_t axis=0;axis<3;++axis)
        vehicle.angularVelocity[axis]=state.angularVelocity[axis]*StorylandNativePhysicsRate;
    // These fields represent the same world angular velocity on other host
    // vehicle paths. Transfer their contribution once, without applying it twice.
    vehicle.springRollVelocity=0;vehicle.springPitchVelocity=0;vehicle.yawVelocity=0;
    vehicle.grounded=false;
    storylandStepVehicleScriptPhysics(vehicle,actor,physicalSeconds);
    actor.position.x+=state.velocity[0]*nativeStep;
    actor.position.y+=state.velocity[1]*nativeStep;
    if(!applyVehicleWater(vehicle,actor.position,world,physicalSeconds,runtime.gravity,false,actor.yaw)) {
        vehicle.verticalSpeed-=runtime.gravity*physicalSeconds;
        actor.position.z+=vehicle.verticalSpeed*physicalSeconds;
    }
    if(vehicle.collisionEnabled) {
        const auto movement=storylandMoveNativeBody(world,previous.position,actor.position,
            vehicleBody(vehicle,actor.yaw));
        resolveVehicleBodyMovement(vehicle,actor,previous.position,movement);
        if(movement.contact.valid&&movement.contact.normal.z>.35f&&vehicle.verticalSpeed<=0) {
            vehicle.suspensionVelocity=vehicle.verticalSpeed;
            vehicle.verticalSpeed=0;
            vehicle.grounded=true;
        }
    }
    vehicle.throttle=input.thrust;
    vehicle.longitudinalAcceleration=(vehicle.speed-previousSpeed)/physicalSeconds;
    updateVehicleWheelContacts(vehicle,actor,world,0);
    return true;
}

#include "src/re/reVCS/vehicles/storyland_vcs_hovercraft_runtime.inl"

bool storylandAdvanceVehicleFlight(StorylandPlayTestRuntime& runtime,
    StorylandRuntimeVehicle& vehicle, StorylandNativeTrafficActor& actor,
    const StorylandNativeWorld* world, float seconds) {
    const auto& definition = vehicle.nativeDefinition;
    if (definition.domain != StorylandVehicleDomain::Air || !definition.hasNativeFlyingHandling) return false;
    const bool helicopter = definition.vcsModelLayout ? definition.nativeType == 4 :
        (definition.handlingFlags & 0x000F0000u) == 0x00020000u;
    const auto& profile = definition.flyingHandling;
    const bool policeHelicopter = definition.vcsModelLayout && definition.nativeType == 4 &&
        definition.modelId == 261 && vehicle.scriptHandle == StorylandVcsPoliceHelicopterHandle &&
        (vehicle.aiMission == 27 || vehicle.aiMission == 30);
    const bool navigating = vehicle.aiActive && (vehicle.aiMission == 20 || vehicle.aiMission == 22 ||
        (policeHelicopter && (vehicle.aiMission == 27 || vehicle.aiMission == 30)));
    StorylandMissionEnvironment::VehicleScriptState policeTask;
    const StorylandMissionEnvironment::VehicleScriptState* flightTask = nullptr;
    if (navigating && runtime.missionEnvironment) {
        if (policeHelicopter) {
            auto target = StorylandNativePoint{runtime.playerPosition.x, runtime.playerPosition.y,
                runtime.playerPosition.z + 50};
            if (vehicle.aiMission == 30) {
                float dx = actor.position.x - runtime.playerPosition.x;
                float dy = actor.position.y - runtime.playerPosition.y;
                const float length = std::hypot(dx, dy);
                if (length > .001f) {dx /= length; dy /= length;} else {dx = 1; dy = 0;}
                target = {actor.position.x + dx * 250, actor.position.y + dy * 250, actor.position.z + 100};
            }
            policeTask.targetX = target.x; policeTask.targetY = target.y; policeTask.targetZ = target.z;
            policeTask.cruiseSpeed = 25.0f;
            flightTask = &policeTask;
        } else {
            const auto found = runtime.missionEnvironment->missionVehicles.find(vehicle.scriptHandle);
            if (found != runtime.missionEnvironment->missionVehicles.end()) flightTask = &found->second;
        }
    }
    const bool nativeScriptedHelicopter = [&]() {
        if (!helicopter || !definition.vcsModelLayout || !navigating || vehicle.heliCrashing ||
            vehicle.health <= 0 || !runtime.missionEnvironment ||
            runtime.missionEnvironment->remoteVehicle == vehicle.scriptHandle) return false;
        const auto& vehicles = runtime.missionEnvironment->missionVehicles;
        const auto found = vehicles.find(vehicle.scriptHandle);
        return found != vehicles.end() && found->second.scriptStatus == 3;
    }();
    const float rotorTarget = (navigating || vehicle.heliCrashing) && vehicle.health > 0 ? .22f : 0;
    if (nativeScriptedHelicopter)
        vehicle.rotorSpeed = StorylandVcsScriptedHelicopterRotor::advance(vehicle.rotorSpeed);
    else
        vehicle.rotorSpeed = approach(vehicle.rotorSpeed, rotorTarget, seconds * .11f);
    vehicle.rotorAngle = std::fmod(vehicle.rotorAngle + vehicle.rotorSpeed * StorylandVehicleStepsPerSecond * seconds,
        6.283185307179586f);
    if (vehicle.positionFrozen || vehicle.health <= 0) return false;
    if (nativeScriptedHelicopter && !StorylandVcsScriptedHelicopterRotor::flightReady(vehicle.rotorSpeed))
        return false;
    if (!navigating && !vehicle.heliCrashing &&
        (!helicopter || vehicle.rotorSpeed <= 0) && vehicle.verticalSpeed == 0) return false;
    const auto previous = actor;
    const auto targetOrigin=[&](const StorylandMissionEnvironment::VehicleScriptState& task,
        std::array<float,3>& origin) {
        // Original43A6A0 consumes the native entity origin. The remote input
        // producer retains its separate actor-space destination contract.
        const bool nativeDestination=definition.vcsModelLayout&&helicopter&&task.scriptStatus==3&&
            runtime.missionEnvironment&&runtime.missionEnvironment->remoteVehicle!=vehicle.scriptHandle;
        if(storylandVcsHelicopterTargetOrigin(definition,
            {actor.position.x,actor.position.y,actor.position.z},actor.yaw,vehicle.suspension,
            vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,nativeDestination,origin))return true;
        runtime.vehiclePhysicsError="Native VCS helicopter destination requires a finite decoded entity frame.";
        return false;
    };
    float desiredRoll = 0;
    float desiredPitch = 0;
    if (vehicle.heliCrashing) {
        vehicle.verticalSpeed -= runtime.gravity * seconds;
        vehicle.yawVelocity = approach(vehicle.yawVelocity, 90, seconds * 30);
        actor.yaw += vehicle.yawVelocity * seconds;
        desiredRoll = 35;
        desiredPitch = -25;
    } else if (navigating && runtime.missionEnvironment) {
        if (flightTask) {
            const auto& task = *flightTask;
            std::array<float,3> origin{};
            if(!targetOrigin(task,origin))return false;
            const float dx = task.targetX - origin[0];
            const float dy = task.targetY - origin[1];
            const float dz = task.targetZ - origin[2];
            const float horizontal = std::hypot(dx, dy);
            const float wantedSpeed = std::max(0.0f, task.cruiseSpeed);
            const float targetSpeed = helicopter ? std::min(wantedSpeed, horizontal) : wantedSpeed;
            const float acceleration = std::max(.1f, profile[0] * 20 / vehicle.massMultiplier);
            vehicle.speed = approach(vehicle.speed, targetSpeed, acceleration * seconds);
            float desiredYaw = horizontal > .1f ? std::atan2(dy, dx) / radians : actor.yaw;
            if (helicopter && vehicle.heliOrientation >= 0) desiredYaw = vehicle.heliOrientation / radians + 180;
            const float yawError = std::remainder(desiredYaw - actor.yaw, 360.0f);
            const float turnRate = std::clamp(std::fabs(profile[2]) * 50000 / vehicle.turnMassMultiplier, 5.0f, 90.0f);
            const float turn = std::clamp(yawError, -turnRate * seconds, turnRate * seconds);
            actor.yaw += turn;
            vehicle.yawVelocity = turn / seconds;
            const float climbTarget = std::clamp(dz, -12.0f, 12.0f);
            const float lift = helicopter ? std::clamp(vehicle.rotorSpeed / .22f, 0.0f, 1.0f) :
                std::clamp(vehicle.speed / std::max(8.0f, wantedSpeed * .5f), 0.0f, 1.0f);
            vehicle.verticalSpeed = approach(vehicle.verticalSpeed, climbTarget,
                acceleration * lift * seconds);
            vehicle.verticalSpeed -= runtime.gravity * (1 - lift) * seconds;
            // Retail43ABEC uses .5 for stabilized helicopter attitude feedback
            // and3 otherwise. Native flight rows supply thrust/yaw and damping.
            const float attitude = vehicle.heliStabiliser ? .5f : 3.0f;
            desiredRoll = std::clamp(-vehicle.yawVelocity * (helicopter ? attitude * .15f : .6f), -35.0f, 35.0f);
            desiredPitch = helicopter ? -std::clamp(vehicle.speed * attitude * .35f, 0.0f, 25.0f) :
                std::atan2(vehicle.verticalSpeed, std::max(1.0f, vehicle.speed)) / radians;
            if (helicopter && horizontal < .1f) vehicle.speed = 0;
        }
    } else {
        vehicle.verticalSpeed -= runtime.gravity * seconds;
    }
    const float response = std::clamp(std::fabs(profile[5]) * 1000 / vehicle.turnMassMultiplier, .5f, 12.0f);
    const float damping = 1 - std::exp(-response * seconds);
    vehicle.bodyRoll += (desiredRoll - vehicle.bodyRoll) * damping;
    vehicle.bodyPitch += (desiredPitch - vehicle.bodyPitch) * damping;
    vehicle.angularVelocity[2] = vehicle.yawVelocity * radians;
    const float angle = actor.yaw * radians;
    if (helicopter && navigating && vehicle.heliOrientation >= 0 && runtime.missionEnvironment) {
        if (flightTask) {
            std::array<float,3> origin{};
            if(!targetOrigin(*flightTask,origin))return false;
            const float dx = flightTask->targetX - origin[0];
            const float dy = flightTask->targetY - origin[1];
            const float length = std::hypot(dx, dy);
            if (length > .001f) {
                const float travel = std::min(length, vehicle.speed * seconds);
                actor.position.x += dx / length * travel;
                actor.position.y += dy / length * travel;
            }
        }
    } else {
        actor.position.x += std::cos(angle) * vehicle.speed * seconds;
        actor.position.y += std::sin(angle) * vehicle.speed * seconds;
    }
    actor.position.z += vehicle.verticalSpeed * seconds;
    vehicle.grounded = false;
    vehicle.roadRoll = 0;
    vehicle.roadPitch = 0;
    if (world && vehicle.collisionEnabled) {
        const float fraction = storylandTraceNativeSegment(*world, previous.position, actor.position);
        const auto movement = storylandMoveNativeBody(*world, previous.position, actor.position,
            vehicleBody(vehicle, actor.yaw));
        if (fraction < 1 || !movement.clear || movement.blocked) {
            actor.position = {previous.position.x + (actor.position.x - previous.position.x) * fraction,
                previous.position.y + (actor.position.y - previous.position.y) * fraction,
                previous.position.z + (actor.position.z - previous.position.z) * fraction};
            if (movement.clear) actor.position = movement.position;
            else actor.position = previous.position;
            vehicle.bodyContact = true;
            vehicle.pendingBodyImpact = true;
            vehicle.speed = 0;
            vehicle.verticalSpeed = 0;
            vehicle.grounded = true;
            return vehicle.heliCrashing;
        }
    }
    return false;
}

void storylandUpdateScriptVehicleSystems(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& cars,float seconds) {
    if(!runtime.missionEnvironment||seconds<=0||!std::isfinite(seconds))return;
    auto& world=*runtime.missionEnvironment;
    const bool bombDown=world.padState[17]!=0;
    const bool bombPressed=bombDown&&!runtime.vehicleBombButtonWasDown;
    runtime.vehicleBombButtonWasDown=bombDown;
    if(runtime.crime.vcs)storylandAdvanceForklift(runtime,cars,seconds);
    std::vector<int32_t> detonate;
    for(auto& entry:world.missionVehicles) {
        auto& car=entry.second;
        if(car.deleted||car.wrecked)continue;
        if(storylandAdvanceVehicleBomb(world,entry.first,car,bombPressed))
            detonate.push_back(entry.first);
        const int slot=car.spawnedSlot;
        if(slot<0||size_t(slot)>=runtime.vehicles.size()||size_t(slot)>=cars.size())continue;
        auto& live=runtime.vehicles[size_t(slot)];
        if(live.scriptHandle!=entry.first)continue;
        if(car.contactExplosion&&live.pendingContactSlot>=0&&size_t(live.pendingContactSlot)<runtime.vehicles.size()&&
            size_t(live.pendingContactSlot)<cars.size()) {
            auto& target=runtime.vehicles[size_t(live.pendingContactSlot)];
            if(target.scriptHandle<0&&world.acquireAmbientVehicle) {
                const auto& point=cars[size_t(live.pendingContactSlot)].position;
                world.acquireAmbientVehicle({point.x-.1f,point.y-.1f,point.x+.1f,point.y+.1f},int32_t(target.nativeDefinition.modelId));
            }
            if(target.scriptHandle>=0)detonate.push_back(target.scriptHandle);
        }
        const bool remoteButton=bombPressed&&(!runtime.crime.vcs||world.control);
        const bool remoteContactModel=!runtime.crime.vcs||
            int16_t(uint16_t(live.nativeDefinition.modelId))==-943;
        if(world.remoteVehicle==entry.first&&
            ((world.remoteDetonationEnabled&&remoteButton)||
             (world.remoteContactDetonation&&remoteContactModel&&live.pendingBodyImpact)))
            detonate.push_back(entry.first);
    }
    for(auto& car:runtime.vehicles) {car.pendingContactSlot=-1;car.pendingBodyImpact=false;}
    std::sort(detonate.begin(),detonate.end());detonate.erase(std::unique(detonate.begin(),detonate.end()),detonate.end());
    for(int32_t handle:detonate) {std::string error;storylandExplodeScriptVehicle(world,handle,error);}
    std::vector<int32_t> remoteBombServices;
    for(const auto& garage:world.garages)
        if(garage.type==4&&garage.serviceUntil&&garage.serviceVehicle>=0)
            remoteBombServices.push_back(garage.serviceVehicle);
    std::string garageError;
    if(!storylandAdvanceGarages(world,seconds,runtime.crime.vcs,&garageError)) {
        runtime.vehiclePhysicsError=std::move(garageError);
        return;
    }
    const int32_t detonatorId=runtime.crime.vcs?36:34;
    for(int32_t handle:remoteBombServices) {
        const auto car=world.missionVehicles.find(handle);
        if(car==world.missionVehicles.end()||car->second.bombType!=3)continue;
        const auto detonator=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
            [&](const auto& weapon){return weapon.nativeId==detonatorId && weapon.fireType!=4;});
        if(detonator==runtime.weapons.end())continue;
        const size_t index=size_t(detonator-runtime.weapons.begin());
        if(storylandGiveWeapon(runtime,index,1,true)) {
            auto& weapon=world.playerWeapons[detonatorId];weapon.slot=detonator->slot;weapon.model=detonator->modelId;
            weapon.owned=true;weapon.ammunition=runtime.inventory[index].ammunition;world.currentWeapon=detonatorId;
        }
    }
    runtime.money=world.money;runtime.wanted=world.wanted;runtime.crime.synchronize(world.wanted);
    storylandAdvanceVehicleRopes(world,seconds,runtime.crime.vcs);
    // Services run after the normal SCM-to-actor publication in the frame.
    // Publish their repairs now so the next capture cannot restore old damage.
    for(auto& entry:world.missionVehicles) {
        auto& scripted=entry.second;const int slot=scripted.spawnedSlot;
        if(slot<0||size_t(slot)>=runtime.vehicles.size()||size_t(slot)>=cars.size())continue;
        auto& vehicle=runtime.vehicles[size_t(slot)];auto& actor=cars[size_t(slot)];
        if(vehicle.scriptHandle!=entry.first)continue;
        storylandApplyMissionVehicleOverrides(vehicle,actor,world,entry.first);
        if(scripted.healthChangedByScript)vehicle.health=scripted.health;
        if(scripted.primaryColor>=0)vehicle.primaryColor=scripted.primaryColor;
        if(scripted.secondaryColor>=0)vehicle.secondaryColor=scripted.secondaryColor;
        if(scripted.extinguishRequested) {
            vehicle.burning=false;vehicle.engineFireSpent=true;
            for(auto& effect:runtime.effects)
                if(effect.vehicle==slot&&effect.kind==StorylandParticleKind::Flame)effect.remaining=0;
        }
        if(scripted.deleted) {vehicle.visible=false;vehicle.collisionEnabled=false;vehicle.health=0;}
        if(scripted.movedByScript) {
            actor.position={scripted.x,scripted.y,scripted.z};
            vehicle.pedestrianSweep={};
            vehicle.wheelContactHistoryValid={};
            vehicle.wheelContactTimers={};
            vehicle.contactsValid=false;
        }
    }
}

static void advanceMissionCharacterGravity(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,float seconds) {
    if(!runtime.missionEnvironment)return;
    auto& environment=*runtime.missionEnvironment;
    for(auto& entry:environment.missionCharacters) {
        auto& character=entry.second;
        auto& state=runtime.missionCombat[entry.first];
        const auto task=environment.characterVehicleTasks.find(entry.first);
        const bool vehicleAnimation=task!=environment.characterVehicleTasks.end()&&task->second.animating;
        const bool vehicleFall=environment.vcsPedFalls.count(entry.first)!=0;
        const bool falling=vehicleFall||state.bulletReaction.knockdown;
        const bool diving=character.behavior.diveDeadline&&character.animationId==146;
        if(world.paths.vcs&&character.alive&&character.health>0&&!character.networkControlled&&!character.removeFromWorld&&
            !vehicleAnimation&&!falling&&!diving)
            storylandAdvanceVcsFootstepMarkCounter(character.vcsFootstepMarks);
        if(vehicleFall)continue;
        if(state.bulletReaction.knockdown) {
            const bool owned=state.bulletReaction.dying?
                (!character.removeFromWorld&&character.vehicle<0&&!character.attached&&
                    !character.positionFrozen&&!character.networkControlled&&!environment.cutscene.playing):
                storylandMissionBulletReactionAllowed(runtime,entry.first);
            if(!owned)state.bulletReaction={};
            else {
                StorylandNativePoint position{character.x,character.y,character.z};
                if(storylandAdvanceBulletKnockdown(runtime,world,state.bulletReaction,position,
                    character.heading+90,seconds,INT32_MAX,&state.water)) {
                    character.x=position.x;character.y=position.y;character.z=position.z;
                    character.movementSpeed=0;
                    state.verticalSpeed=state.bulletReaction.fall.velocity[2];
                    state.grounded=state.bulletReaction.fall.grounded;
                    state.gravityPosition=position;state.gravityPositionValid=true;
                    state.fall={};state.landingTime=0;
                    state.airborneTime=state.grounded||state.water.active?0:state.airborneTime+seconds;
                    continue;
                }
            }
        }
        if(!world.paths.vcs)continue;
        const bool scriptedAnimation=character.animationId>=0||
            environment.scriptedCharacterAnimations.count(entry.first)>0;
        // Attachments, root-motion animations and frozen actors own their
        // transforms. Free characters use the same feet/body convention and
        // swept collision as the player; a dismount is not an instant Z snap.
        float waterHeight=0;
        if(!character.alive||character.health<=0||character.removeFromWorld||
            character.vehicle>=0||character.attached||character.positionFrozen||
            !character.loadCollision||!character.collisionEnabled||vehicleAnimation||scriptedAnimation||environment.cutscene.playing||
            deepCharacterWater(world,{character.x,character.y,character.z},waterHeight)) {
            state.verticalSpeed=0;state.grounded=false;state.gravityPositionValid=false;
            state.airborneTime=state.landingTime=0;
            state.fall={};
            continue;
        }
        if(state.fall.landingAnimationId>=0) {
            auto& fall=state.fall;
            fall.landingAnimationTime=std::min(fall.landingAnimationDuration,
                fall.landingAnimationTime+seconds);
            state.landingTime=std::max(0.0f,fall.landingAnimationDuration-fall.landingAnimationTime);
            if(state.landingTime<=0)fall={};
        }
        const StorylandNativePoint before{character.x,character.y,character.z};
        if(!state.gravityPositionValid||std::fabs(before.z-state.gravityPosition.z)>.25f)
            state.verticalSpeed=0;
        const auto vertical=storylandPedVerticalStep(state.verticalSpeed,runtime.gravity,seconds);
        const float velocity=vertical.velocity;
        StorylandNativePoint desired=before;desired.z+=vertical.displacement;
        const StorylandNativePoint top{before.x,before.y,before.z+.08f};
        const StorylandNativePoint bottom{before.x,before.y,std::min(before.z-3.0f,desired.z-.15f)};
        uint8_t material=0;
        const float fraction=storylandTraceNativeGround(world,top,bottom,material);
        bool hasFloor=fraction<1;
        float floor=top.z+(bottom.z-top.z)*fraction;
        for(const auto& car:runtime.movementCars) {
            if(!car.collisionEnabled)continue;
            const float roof=car.position.z+car.height;
            if(before.z<roof-.15f||roof<bottom.z||roof>top.z)continue;
            const float c=std::cos(car.yaw*radians),s=std::sin(car.yaw*radians);
            const float dx=before.x-car.position.x,dy=before.y-car.position.y;
            if(std::fabs(dx*c+dy*s)>car.halfLength||std::fabs(-dx*s+dy*c)>car.halfWidth)continue;
            if(!hasFloor||roof>floor) {floor=roof;hasFloor=true;}
        }
        const bool landing=hasFloor&&velocity<=0&&(desired.z<=floor+.005f||
            storylandPedSupportContinuity(state.grounded,false,state.verticalSpeed,before.z,floor));
        if(!landing||state.airborneTime>0) {
            auto& fall=state.fall;
            if(!fall.active)storylandBeginVcsFall(fall,false);
            const auto& previous=state.gravityPositionValid?state.gravityPosition:before;
            storylandAdvanceVcsFall(fall,std::hypot(before.x-previous.x,before.y-previous.y,
                vertical.displacement),velocity<0,hasFloor,seconds);
            float originOffset=1.04f;
            if(environment.characterOriginOffset) {
                float decoded=0;
                if(environment.characterOriginOffset(entry.first,decoded)&&std::isfinite(decoded))
                    originOffset=decoded;
            }
            const bool nearFloor=hasFloor&&velocity<=0&&before.z+originOffset-floor<1.3f;
            if(fall.landingAnimationId<0&&(nearFloor||landing)) {
                fall.landingAnimationId=storylandVcsFallLandingAnimation(fall,0,false);
                const float duration=environment.animationDuration?
                    environment.animationDuration(fall.landingAnimationId,0):
                    runtime.reactionBindings.duration?runtime.reactionBindings.duration(fall.landingAnimationId,0):0;
                fall.landingAnimationDuration=std::isfinite(duration)?std::max(0.0f,duration):0;
                fall.landingAnimationTime=0;
                state.landingTime=fall.landingAnimationDuration;
                if(fall.landingAnimationId!=139)
                    storylandQueuePedLandingSound(runtime,int(runtime.pedestrians.size())+entry.first,
                        {before.x,before.y,floor},material);
            }
        }
        if(landing)desired.z=floor;
        const auto move=storylandMoveNativeBody(world,before,desired,storylandNativeCharacterBody());
        character.z=move.position.z;
        state.grounded=landing&&std::fabs(character.z-floor)<.05f;
        if(state.grounded) {
            if(state.airborneTime>0) {
                storylandConsumeVcsFootstepMarks(character.vcsFootstepMarks,40);
            }
            state.airborneTime=0;
        } else state.airborneTime+=seconds;
        state.verticalSpeed=state.grounded?0:velocity;
        if(move.blocked&&desired.z>before.z)state.verticalSpeed=std::min(0.0f,state.verticalSpeed);
        state.gravityPosition={character.x,character.y,character.z};state.gravityPositionValid=true;
    }
}

void storylandUpdateVehicleWheelContactPose(StorylandRuntimeVehicle& vehicle,
    const StorylandNativeTrafficActor& actor) {
    const auto& definition=vehicle.nativeDefinition;
    const auto& wheels=definition.physicalWheels();
    if(!vehicle.grounded||!vehicle.contactsValid||vehicle.positionFrozen||vehicle.coopControlled||
        actor.water||vehicle.craneCarried||
        !definition.hasNativeSuspension||wheels.size()!=4)return;
    const auto physicalPose=storylandVehicleMeshFrame(definition,
        {actor.position.x,actor.position.y,actor.position.z},actor.yaw,
        definition.previewMinimumZ,vehicle.suspension,
        vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch);
    const auto pose=storylandVehicleRenderFrame(definition,
        {actor.position.x,actor.position.y,actor.position.z},actor.yaw,
        definition.previewMinimumZ,vehicle.suspension,
        vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,false,nullptr,vehicle.bikeVisualLean);
    if(pose[10]<.35f)return;
    for(size_t index=0;index<definition.wheels.size()&&index<4;++index) {
        const size_t physical=definition.twoWheels()?
            StorylandVcsVehicleContactTimers::bikeSelectedContactLine(vehicle.wheelLineFractions,index):index;
        if(!vehicle.wheelContacts[physical])continue;
        const auto& contactPoint=vehicle.wheelContactPoints[physical];
        const auto& contactNormal=vehicle.wheelContactNormals[physical];
        const auto& wheel=definition.wheels[index];
        const size_t tyre=storylandVehicleTyreIndex(definition.twoWheels(),wheel.front,
            wheel.previewPivot[0]<definition.previewRoot[12]);
        const float steer=StorylandVcsRetailQuirks::wheelSteerDegrees(definition.vcsModelLayout,
            definition.handlingFlags,wheel.front,vehicle.steeringAngle,vehicle.handbrake);
        vehicle.wheelPositions[index]=storylandWheelPlaneCorrection(wheel,wheels[physical].radius,
            vehicle.wheelPositions[index],pose,
            {contactPoint.x,contactPoint.y,contactPoint.z},
            {contactNormal.x,contactNormal.y,contactNormal.z},
            wheel.front?vehicle.wheelRotation:vehicle.rearWheelRotation,steer,
            vehicle.tyreStatus[tyre]!=0,&physicalPose,&wheels[physical]);
    }
}

bool storylandPlaceVcsTrafficVehicle(StorylandRuntimeVehicle& vehicle,
    StorylandNativeTrafficActor& actor,const StorylandNativeWorld& world) {
    const auto& definition=vehicle.nativeDefinition;
    if(!world.paths.vcs||!definition.vcsModelLayout||actor.water||
        definition.domain!=StorylandVehicleDomain::Road||!definition.collision||
        !std::isfinite(actor.position.x)||!std::isfinite(actor.position.y)||
        !std::isfinite(actor.position.z)||!std::isfinite(actor.yaw))return false;
    const auto start=StorylandVcsTrafficSupport::queryStart(actor.position);
    const auto query=[&](float endZ) {
        StorylandVcsTrafficSupport::Hit hit;
        const StorylandNativePoint end{start.x,start.y,endZ};
        const float fraction=storylandTraceNativeTrafficSurface(world,start,end,hit.surface,hit.normal);
        hit.found=fraction>=0&&fraction<1;
        hit.point={start.x,start.y,start.z+(end.z-start.z)*fraction};
        return hit;
    };
    const float yaw=(actor.yaw-90)*radians;
    const auto upward=query(1000);
    const auto downward=query(-1000);
    float collisionMinimum=definition.collision->minimum.z;
    if(definition.hasNativeSuspension&&!definition.physicalWheels().empty())
        collisionMinimum=std::min(collisionMinimum,definition.physicalWheels().front().lowerZ);
    const auto placement=StorylandVcsTrafficSupport::resolve(actor.position,upward,downward,
        {std::cos(yaw),std::sin(yaw),0},collisionMinimum);
    if(!placement.valid)return false;

    // The free-body solver stores a rigid pose. Preserve the selected surface
    // and native origin height while making its up axis orthogonal to right.
    auto frame=placement.matrix;
    const auto up=VehicleContact::unit({frame[8],frame[9],frame[10]});
    const StorylandNativePoint nativeRight{frame[0],frame[1],frame[2]};
    const auto right=VehicleContact::unit(VehicleContact::subtract(nativeRight,
        VehicleContact::scale(up,VehicleContact::dot(nativeRight,up))));
    if(VehicleContact::dot(up,up)<.5f||VehicleContact::dot(right,right)<.5f)return false;
    const auto forward=VehicleContact::cross(up,right);
    frame[0]=right.x;frame[1]=right.y;frame[2]=right.z;
    frame[4]=forward.x;frame[5]=forward.y;frame[6]=forward.z;
    frame[8]=up.x;frame[9]=up.y;frame[10]=up.z;
    StorylandVehiclePose pose;
    if(!storylandVehiclePoseFromNativeFrame(definition,frame,vehicle.suspension,pose))return false;
    actor.groundOffset+=pose.position[2]-actor.position.z;
    actor.position={pose.position[0],pose.position[1],pose.position[2]};
    actor.yaw=pose.heading;
    vehicle.bodyPitch=pose.pitch;vehicle.bodyRoll=pose.roll;
    vehicle.roadPitch=0;vehicle.roadRoll=0;
    vehicle.grounded=true;vehicle.verticalSpeed=0;vehicle.suspensionVelocity=0;
    vehicle.contactsValid=false;vehicle.suspensionAnchorValid=false;
    vehicle.vcsTrafficRail={placement.upward,0,0};
    return true;
}

bool storylandUpdateVcsTrafficSurface(StorylandRuntimeVehicle& vehicle,
    StorylandNativeTrafficActor& actor,const StorylandNativeWorld& world,uint32_t frameNumber) {
    auto& rail=vehicle.vcsTrafficRail;
    if(!rail.active)return false;
    const auto& definition=vehicle.nativeDefinition;
    if(!world.paths.vcs||!definition.vcsModelLayout||actor.water||!vehicle.hasDriver||
        vehicle.parked||vehicle.health<=0||vehicle.scriptHandle>=0||vehicle.generator>=0||
        vehicle.savedGarage>=0||vehicle.serviceAutopilot||vehicle.coopControlled||
        vehicle.craneCarried||vehicle.policePursuing||vehicle.aiActive||
        vehicle.pendingContactSlot>=0||vehicle.bodyContact) {
        rail.active=false;return false;
    }
    const auto nativeFrame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
        {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
        vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch),
        definition.previewRoot);
    const StorylandNativePoint origin{nativeFrame[12],nativeFrame[13],nativeFrame[14]};
    if(StorylandVcsTrafficSupport::groundCheckDue(uint16_t(actor.routeSeed),frameNumber)&&
        storylandTraceNativeMapGround(world,origin,{origin.x,origin.y,-2})>=1) {
        rail.active=false;return false;
    }
    const StorylandNativePoint forward{nativeFrame[4],nativeFrame[5],nativeFrame[6]};
    const StorylandNativePoint target{origin.x+1.5f*forward.x,origin.y+1.5f*forward.y,origin.z};
    const auto footprint=StorylandVcsTrafficSupport::railFootprint(origin,forward,target);
    if(!footprint.valid) {rail.active=false;return false;}
    const auto query=[&](const StorylandNativePoint& point,bool retry) {
        StorylandVcsTrafficSupport::Hit hit;
        const auto ray=StorylandVcsTrafficSupport::railRay(point,origin.z,retry);
        const float fraction=storylandTraceNativeBuildings(world,ray.start,ray.end);
        hit.found=fraction>=0&&fraction<1;
        hit.point={point.x,point.y,ray.start.z+(ray.end.z-ray.start.z)*fraction};
        return hit;
    };
    const auto height=[&](const StorylandNativePoint& point,float previous) {
        const auto first=query(point,false);
        const auto retry=first.found?StorylandVcsTrafficSupport::Hit{}:query(point,true);
        return StorylandVcsTrafficSupport::railHeight(first,retry,previous);
    };
    rail.frontHeight=height(footprint.front,rail.frontHeight);
    rail.rearHeight=height(footprint.rear,rail.rearHeight);
    const float rideHeight=definition.hasNativeSuspension?definition.nativeRideHeight:
        definition.collision?-definition.collision->minimum.z:0;
    const auto supported=StorylandVcsTrafficSupport::railMatrix(footprint,
        rail.frontHeight,rail.rearHeight,rideHeight);
    StorylandVehiclePose pose;
    if(!storylandVehiclePoseFromNativeFrame(definition,supported,0,pose)) {
        rail.active=false;return false;
    }
    // Only an upward-selected ambient spawn enters this compatibility path.
    // Its ordinary horizontal route solver remains responsible for road position.
    actor.groundOffset+=pose.position[2]-actor.position.z;
    actor.position.z=pose.position[2];
    vehicle.suspension=0;vehicle.suspensionVelocity=0;vehicle.verticalSpeed=0;
    vehicle.bodyPitch=pose.pitch;vehicle.bodyRoll=pose.roll;
    vehicle.roadPitch=0;vehicle.roadRoll=0;
    vehicle.springPitchVelocity=0;vehicle.springRollVelocity=0;
    vehicle.grounded=true;vehicle.contactsValid=false;vehicle.suspensionAnchorValid=false;
    return true;
}

bool storylandPlaceParkedRoadVehicle(StorylandRuntimeVehicle& vehicle,
    StorylandNativeTrafficActor& actor,const StorylandNativeWorld& world) {
    if(!std::isfinite(actor.position.x)||!std::isfinite(actor.position.y)||
        !std::isfinite(actor.position.z)||!std::isfinite(actor.yaw))return false;
    const StorylandNativePoint top{actor.position.x,actor.position.y,actor.position.z+1.25f};
    const StorylandNativePoint bottom{top.x,top.y,actor.position.z-2.0f};
    uint8_t surface=0;
    const float hit=storylandTraceNativeGround(world,top,bottom,surface);
    if(!(hit>=0&&hit<1))return false;
    auto candidate=actor;
    candidate.position.z=top.z+(bottom.z-top.z)*hit;
    auto body=supportedVehicleBody(world,vehicle,candidate,true);
    if(vehicle.nativeDefinition.hasNativeSuspension &&
       !vehicle.nativeDefinition.wheels.empty()) {
        const auto support=storylandMeasureWheelGround(vehicle.nativeDefinition,
            {candidate.position.x,candidate.position.y,candidate.position.z},candidate.yaw,
            vehicle.suspension,vehicle.bodyRoll+body.roll,vehicle.bodyPitch+body.pitch,
            vehicle.wheelPositions,
            [&](float x,float y,float centerZ,float radius,float& height,std::array<float,3>& planeNormal) {
                const StorylandNativePoint from{x,y,centerZ+radius+.65f};
                const StorylandNativePoint to{x,y,centerZ-radius-1.65f};
                uint8_t surface=0;StorylandNativePoint normal{};
                const float fraction=storylandTraceNativeGround(world,from,to,surface,&normal);
                if(!(fraction>=0.0f&&fraction<1.0f))return false;
                height=from.z+(to.z-from.z)*fraction;
                planeNormal={normal.x,normal.y,normal.z};
                return true;
            },vehicle.wheelRotation,vehicle.rearWheelRotation,vehicle.steeringAngle,
            vehicle.tyreStatus,vehicle.handbrake,vehicle.bikeVisualLean);
        if(support.valid && std::fabs(support.targetPositionZ-candidate.position.z)<0.65f) {
            candidate.position.z=support.targetPositionZ;
            body=supportedVehicleBody(world,vehicle,candidate,true);
        }
    }
    if(!storylandNativeBodyClear(world,candidate.position,body))return false;
    actor=candidate;
    vehicle.roadPitch=body.pitch;vehicle.roadRoll=-body.roll;
    vehicle.grounded=true;vehicle.verticalSpeed=0;
    vehicle.suspensionAnchorValid=false;
    return true;
}

static bool storylandFallenRecoveryDestination(StorylandPlayTestRuntime& runtime,
    const StorylandNativePaths& paths,const StorylandNativeWorld& world,
    const StorylandNativePoint& from,bool vehicle,StorylandNativePoint& destination,uint32_t& node) {
    const auto* environment=runtime.missionEnvironment;
    const auto* marked=environment&&paths.vcs?&environment->pathSearchMarkedNodes[vehicle?0:1]:nullptr;
    node=storylandFindNativeFallenNode(paths,from,vehicle,marked);
    if(node>=paths.nodes.size())return false;
    destination=paths.nodes[node].position;destination.z+=vehicle?3.0f:2.0f;
    if(environment) {
        const std::array<float,3> point{destination.x,destination.y,destination.z};
        std::string error;
        if(environment->requestCollision&&!environment->requestCollision(point,error))return false;
        if(environment->loadSceneCollision&&!environment->loadSceneCollision(point,error))return false;
    }
    const StorylandNativePoint bottom{destination.x,destination.y,paths.nodes[node].position.z-6.0f};
    return storylandTraceNativeMapGround(world,destination,bottom)<1;
}
static void storylandRemapFallenSlot(int& reference,size_t removed) {
    if(reference==int(removed))reference=-1;
    else if(reference>int(removed))--reference;
}
static void storylandRemoveFallenPed(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& actors,size_t slot) {
    actors.erase(actors.begin()+slot);
    for(size_t index=slot;index<actors.size();++index)runtime.pedestrians[index]=std::move(runtime.pedestrians[index+1]);
    runtime.pedestrians[actors.size()]={};
    storylandRemapFallenSlot(runtime.targetedPed,slot);
    storylandRemapFallenSlot(runtime.arrestOfficer,slot);
    storylandRemapFallenSlot(runtime.targetIndicator.ambient,slot);
    storylandRemapFallenSlot(runtime.playerMelee.target,slot);
    if(runtime.playerReaction.target.kind==StorylandReactionTargetKind::Ambient) {
        storylandRemapFallenSlot(runtime.playerReaction.target.handle,slot);
        if(runtime.playerReaction.target.handle<0)runtime.playerReaction.clearAction();
    }
    const auto owner=[&](int& reference) {
        if(reference==int(slot))reference=INT32_MAX;
        else if(reference>int(slot)&&size_t(reference)<runtime.pedestrians.size())--reference;
    };
    for(auto& ped:runtime.pedestrians)storylandRemapFallenSlot(ped.melee.target,slot);
    for(auto& state:runtime.missionCombat)storylandRemapFallenSlot(state.second.melee.target,slot);
    for(auto& effect:runtime.effects)owner(effect.owner);
    for(auto& effect:runtime.worldEffects)owner(effect.owner);
    for(auto& projectile:runtime.projectiles)owner(projectile.owner);
}
static void storylandRemoveFallenCar(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& actors,size_t slot) {
    auto* environment=runtime.missionEnvironment;
    const int32_t handle=runtime.vehicles[slot].scriptHandle;
    if(environment&&handle>=0) {
        auto found=environment->missionVehicles.find(handle);
        if(found!=environment->missionVehicles.end()) {
            found->second.deleted=true;found->second.alive=false;found->second.spawnedSlot=-1;
        }
    }
    actors.erase(actors.begin()+slot);
    for(size_t index=slot;index<actors.size();++index)runtime.vehicles[index]=std::move(runtime.vehicles[index+1]);
    runtime.vehicles[actors.size()]={};
    storylandRemapFallenSlot(runtime.occupiedVehicle,slot);
    storylandRemapFallenSlot(runtime.pendingBikeImpactVehicle,slot);
    if(runtime.pendingBikeImpactVehicle<0)runtime.pendingBikeImpact={};
    storylandRemapFallenSlot(runtime.transitionVehicle,slot);
    storylandRemapFallenSlot(runtime.playerReaction.evadingVehicle.slot,slot);
    for(auto& vehicle:runtime.vehicles) {
        storylandRemapFallenSlot(vehicle.lastCollidedSlot,slot);
        storylandRemapFallenSlot(vehicle.pendingContactSlot,slot);
    }
    for(auto& effect:runtime.effects)storylandRemapFallenSlot(effect.vehicle,slot);
    for(auto& effect:runtime.worldEffects)storylandRemapFallenSlot(effect.vehicle,slot);
    for(auto& event:runtime.soundEvents)storylandRemapFallenSlot(event.vehicle,slot);
    if(environment) {
        for(auto& entry:environment->missionVehicles)storylandRemapFallenSlot(entry.second.spawnedSlot,slot);
        for(auto& entry:environment->carGenerators)storylandRemapFallenSlot(entry.spawnedSlot,slot);
    }
}
void storylandUpdateFallenWorld(StorylandPlayTestRuntime& runtime,const StorylandNativePaths& paths,
    const StorylandNativeWorld& world,std::vector<StorylandNativeTrafficActor>& pedestrians,
    std::vector<StorylandNativeTrafficActor>& cars) {
    auto* environment=runtime.missionEnvironment;
    if(environment&&(environment->cutscene.playing||environment->criticalMissionRestartRequested||
        environment->introMissionRestartQueued))return;
    if(storylandNativeFallenScanDue(runtime.fallenWorldFrame,false)) {
        for(size_t count=std::min(pedestrians.size(),runtime.pedestrians.size());count>0;--count) {
            const size_t slot=count-1;auto& ped=runtime.pedestrians[slot];auto& actor=pedestrians[slot];
            if(ped.scriptHandle>=0||ped.serviceVehicleIdentity||!storylandNativeEntityHasFallen(paths.vcs,actor.position.z))continue;
            if(!storylandIsAdditionalFortBaxterPatrol(ped)) {
                storylandRemoveFallenPed(runtime,pedestrians,slot);continue;
            }
            StorylandNativePoint destination;uint32_t node=UINT32_MAX;
            if(storylandFallenRecoveryDestination(runtime,paths,world,actor.position,false,destination,node)&&
                storylandNativeBodyClear(world,destination,storylandNativeCharacterBody())) {
                actor.position=destination;actor.node=node;actor.nextNode=node;actor.distance=0;ped.water={};
            }
        }
        if(environment)for(auto& entry:environment->missionCharacters) {
            auto& ped=entry.second;
            if(ped.removeFromWorld||ped.vehicle>=0||ped.attached||ped.positionFrozen||ped.networkControlled||
                !storylandNativeEntityHasFallen(paths.vcs,ped.z))continue;
            if(ped.creationSource==1) {ped.removeFromWorld=true;ped.visible=false;continue;}
            StorylandNativePoint destination;uint32_t node=UINT32_MAX;
            if(!storylandFallenRecoveryDestination(runtime,paths,world,{ped.x,ped.y,ped.z},false,destination,node)||
                !storylandNativeBodyClear(world,destination,storylandNativeCharacterBody()))continue;
            ped.x=destination.x;ped.y=destination.y;ped.z=destination.z;ped.movementSpeed=0;
            const auto state=runtime.missionCombat.find(entry.first);
            if(state!=runtime.missionCombat.end()) {
                state->second.verticalSpeed=0;state->second.airborneTime=0;
                state->second.grounded=false;state->second.gravityPositionValid=false;state->second.water={};
            }
        }
    }
    if(!storylandNativeFallenScanDue(runtime.fallenWorldFrame,true))return;
    for(size_t count=std::min(cars.size(),runtime.vehicles.size());count>0;--count) {
        const size_t slot=count-1;auto& vehicle=runtime.vehicles[slot];auto& actor=cars[slot];
        if(!storylandNativeEntityHasFallen(paths.vcs,actor.position.z)||vehicle.craneCarried||
            vehicle.positionFrozen)continue;
        StorylandMissionEnvironment::VehicleScriptState* scripted=nullptr;
        if(environment) {
            const auto found=environment->missionVehicles.find(vehicle.scriptHandle);
            if(found!=environment->missionVehicles.end())scripted=&found->second;
        }
        if(scripted&&(scripted->deleted||scripted->movedByScript||scripted->positionFrozen))continue;
        const bool occupied=int(slot)==runtime.occupiedVehicle;
        const bool missionOwned=vehicle.scriptHandle>=0&&(!scripted||scripted->persistent||
            (scripted->alive&&!scripted->deleted&&
                (scripted->drivingToTarget||scripted->aiActive||
                (environment&&environment->introMissionProtected))));
        const bool retained=occupied||missionOwned||vehicle.generator>=0||vehicle.savedGarage>=0||
            int(slot)==runtime.transitionVehicle||int(slot)==storylandRemoteVehicleSlot(runtime);
        if(!retained) {storylandRemoveFallenCar(runtime,cars,slot);continue;}
        StorylandNativePoint destination;uint32_t node=UINT32_MAX;
        if(!storylandFallenRecoveryDestination(runtime,paths,world,actor.position,true,destination,node)||
            !storylandNativeBodyClear(world,destination,vehicleBody(vehicle,actor.yaw)))continue;
        actor.position=destination;actor.node=node;actor.nextNode=node;actor.previousNode=UINT32_MAX;
        actor.distance=0;actor.groundOffset=3;actor.blockedSeconds=0;
        vehicle.speed=0;vehicle.lateralSpeed=0;vehicle.verticalSpeed=0;vehicle.grounded=false;
        vehicle.contactsValid=false;vehicle.wheelContacts={};vehicle.wheelContactTimers={};
        vehicle.wheelContactHistoryValid={};
        vehicle.suspensionAnchorValid=false;
        vehicle.wheelTravelInitialized=false;vehicle.skidContactValid={};vehicle.skidRoadContact=false;
        vehicle.bloodyTyres={};
        if(scripted) {
            scripted->x=destination.x;scripted->y=destination.y;scripted->z=destination.z;
            scripted->speed=0;scripted->velocity={0,0,0};scripted->velocityChangedByScript=false;
        }
        if(environment&&vehicle.scriptHandle>=0)for(auto& entry:environment->missionCharacters)
            if(entry.second.vehicle==vehicle.scriptHandle) {
                entry.second.x=destination.x;entry.second.y=destination.y;entry.second.z=destination.z;
            }
        if(occupied)runtime.fallenPlayerVehicleRepositioned=true;
    }
}
bool storylandRecoverFallenPlayer(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    StorylandNativePoint& player,std::vector<StorylandNativeTrafficActor>& cars) {
    auto* environment=runtime.missionEnvironment;
    if(runtime.paused||runtime.pauseMenu||runtime.health<=0||runtime.wastedActive||runtime.busted||
        runtime.entryTransition>=0||runtime.debugMovement!=StorylandDebugMovement::Walking||
        (environment&&(environment->cutscene.playing||environment->playerWarped||
            environment->playerPositionFrozen||environment->activeVcsCharacterAttachment(0)||
            environment->criticalMissionRestartRequested||environment->introMissionRestartQueued)))return false;
    StorylandNativePoint destination;
    if(runtime.occupiedVehicle>=0) {
        if(!runtime.fallenPlayerVehicleRepositioned||size_t(runtime.occupiedVehicle)>=cars.size())return false;
        destination=cars[size_t(runtime.occupiedVehicle)].position;
    } else {
        if(!storylandNativeFallenScanDue(runtime.fallenWorldFrame,false)||
            !storylandNativeEntityHasFallen(world.paths.vcs,player.z))return false;
        uint32_t node=UINT32_MAX;
        const auto& paths=runtime.livePaths.nodes.empty()?world.paths:runtime.livePaths;
        if(!storylandFallenRecoveryDestination(runtime,paths,world,player,false,destination,node)||
            !storylandNativeBodyClear(world,destination,storylandNativeCharacterBody()))return false;
    }
    player=destination;runtime.playerPosition=destination;runtime.fallenPlayerVehicleRepositioned=false;
    runtime.verticalSpeed=0;runtime.grounded=false;runtime.airborneTime=0;runtime.landingTime=0;
    runtime.playerFall={};
    runtime.jumpLaunching=false;runtime.jumpHorizontalValid=false;runtime.jumpFromLaunch=false;
    runtime.jumpHorizontalVelocity={0,0};runtime.jumpLaunchTime=0;runtime.playerWater={};
    runtime.statisticsPosition=destination;runtime.statisticsPositionValid=true;
    if(environment) {
        environment->x=destination.x;environment->y=destination.y;environment->z=destination.z;
        environment->cameraBehindPlayerRequested=true;
    }
    return true;
}

void storylandCarryEscalatorActors(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    std::vector<StorylandNativeTrafficActor>& pedestrians,StorylandNativePoint& player) {
    const auto carry=[&](StorylandNativePoint& position,bool grounded,StorylandNativePoint* anchor) {
        StorylandNativePoint delta,velocity;
        if(!storylandEscalatorCarry(runtime.escalators,world,position,grounded,delta,velocity))return;
        position={position.x+delta.x,position.y+delta.y,position.z+delta.z};
        if(anchor)*anchor={anchor->x+delta.x,anchor->y+delta.y,anchor->z+delta.z};
    };
    if(runtime.occupiedVehicle<0&&runtime.vehicleAction==StorylandVehicleAction::None&&!runtime.playerWater.active) {
        carry(player,runtime.grounded&&!runtime.jumpLaunching,nullptr);
        runtime.playerPosition=player;
        if(runtime.missionEnvironment) {
            runtime.missionEnvironment->x=player.x;runtime.missionEnvironment->y=player.y;
            runtime.missionEnvironment->z=player.z;
        }
    }
    for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size();++index) {
        auto& ped=runtime.pedestrians[index];
        if(ped.scriptHandle>=0||ped.water.active||ped.renderAlpha==0||ped.serviceVehicleIdentity)continue;
        carry(pedestrians[index].position,true,ped.nativeRootValid?&ped.nativeRoot:nullptr);
    }
    if(!runtime.missionEnvironment)return;
    for(auto& entry:runtime.missionEnvironment->missionCharacters) {
        auto& character=entry.second;
        if(character.vehicle>=0||character.attached||character.positionFrozen)continue;
        auto& combat=runtime.missionCombat[entry.first];
        if(combat.water.active)continue;
        StorylandNativePoint position{character.x,character.y,character.z};
        carry(position,!combat.gravityPositionValid||combat.grounded,
            combat.nativeRootValid?&combat.nativeRoot:nullptr);
        character.x=position.x;character.y=position.y;character.z=position.z;
        if(combat.gravityPositionValid)combat.gravityPosition=position;
    }
}

static bool storylandVcsRoadSteeringLinks(const StorylandNativePaths& paths,
    StorylandNativeTrafficActor& actor, const StorylandNativeZones* zones,
    bool avoidLevelTransitions, const std::vector<StorylandRoadChange>* roadChanges,
    uint8_t straightLineDistance, bool ignoreDisabledRoads,
    StorylandVcsVehicleSteering::RoadLink& current,
    StorylandVcsVehicleSteering::RoadLink& next) {
    namespace Steering = StorylandVcsVehicleSteering;
    if (!actor.destinationRouteValid || actor.destinationRouteFinished ||
        actor.destinationRouteCursor >= actor.destinationRoute.size()) return false;
    if (!Steering::roadLink(paths, actor.node, actor.nextNode, actor.lane, current)) return false;
    if (actor.destinationRouteCursor + 1 >= actor.destinationRoute.size()) {
        auto continuation = actor;
        continuation.node = continuation.nextNode;
        continuation.destinationRouteFinished = true;
        if (!storylandPrepareVehicleDestinationRoute(paths, continuation, actor.destinationRouteTarget,
            zones, avoidLevelTransitions, roadChanges, straightLineDistance, ignoreDisabledRoads) ||
            continuation.destinationRouteFinished || continuation.destinationRoute.empty()) {
            actor.previousNode = actor.node;
            actor.node = actor.nextNode;
            actor.destinationRouteCursor = uint32_t(actor.destinationRoute.size());
            actor.destinationRouteFinished = true;
            actor.distance = 0;
            return false;
        }
        actor.destinationRoute.assign(1, actor.nextNode);
        actor.destinationRoute.insert(actor.destinationRoute.end(),
            continuation.destinationRoute.begin(), continuation.destinationRoute.end());
        actor.destinationRouteCursor = 0;
    }
    const uint32_t following = actor.destinationRoute[actor.destinationRouteCursor + 1];
    if (!Steering::roadLink(paths, actor.nextNode, following, actor.lane, next)) return false;
    if (Steering::passedRoadLink(actor.position, current, next)) {
        actor.previousNode = actor.node;
        actor.node = actor.nextNode;
        actor.nextNode = following;
        ++actor.destinationRouteCursor;
        actor.routeSeed = actor.routeSeed * 1664525u + 1013904223u + actor.node;
        actor.crossingCommitted = false;
    }
    const auto& from = paths.nodes[actor.node].position;
    const auto& to = paths.nodes[actor.nextNode].position;
    const float x = to.x - from.x, y = to.y - from.y, z = to.z - from.z;
    const float length = std::hypot(x, y, z);
    actor.distance = length > 0.0f ? std::clamp(((actor.position.x - from.x) * x +
        (actor.position.y - from.y) * y + (actor.position.z - from.z) * z) / length, 0.0f, length) : 0.0f;
    return true;
}

static StorylandVcsVehicleSteering::Controls storylandVcsTrafficControls(
    const StorylandPlayTestRuntime& runtime, const StorylandNativeTrafficActor& actor,
    StorylandRuntimeVehicle& vehicle, StorylandMissionEnvironment::VehicleScriptState& scripted,
    size_t index, const std::vector<StorylandNativeTrafficActor>& cars,
    const std::vector<StorylandNativeTrafficActor>& pedestrians, const StorylandNativePoint& player,
    const StorylandVcsVehicleSteering::RoadLink* road = nullptr,
    const StorylandVcsVehicleSteering::RoadLink* followingRoad = nullptr, float trafficSpeedFactor = 1.0f) {
    namespace Steering = StorylandVcsVehicleSteering;
    const auto nativeFrame = [](const StorylandRuntimeVehicle& state,
        const StorylandNativeTrafficActor& traffic) {
        const auto& definition = state.nativeDefinition;
        return storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
            {traffic.position.x, traffic.position.y, traffic.position.z}, traffic.yaw,
            definition.previewMinimumZ, state.suspension, state.bodyRoll - state.roadRoll,
            state.bodyPitch + state.roadPitch), definition.previewRoot);
    };
    const auto nativeVelocity = [](const StorylandRuntimeVehicle& state,
        const StorylandNativeTrafficActor& traffic) {
        const float angle = traffic.yaw * radians;
        return StorylandNativePoint{
            (std::cos(angle) * state.speed - std::sin(angle) * state.lateralSpeed) / StorylandVehicleStepsPerSecond,
            (std::sin(angle) * state.speed + std::cos(angle) * state.lateralSpeed) / StorylandVehicleStepsPerSecond,
            vehicleVerticalVelocity(state) / StorylandVehicleStepsPerSecond};
    };
    const auto velocity = nativeVelocity(vehicle, actor);
    const auto frame = nativeFrame(vehicle, actor);
    const bool targetedRam = scripted.aiMission == 16;
    const bool targetedIntercept = scripted.aiMission == 18;
    const bool targetedHold = scripted.aiMission == 19;
    const bool targeted = targetedRam || targetedIntercept || targetedHold;
    const StorylandNativePoint origin{targeted ? frame[12] : actor.position.x,
        targeted ? frame[13] : actor.position.y, frame[14]};
    const float heading = frame[4] == 0.0f && frame[5] == 0.0f ? 0.0f : Steering::bearing(frame[4], frame[5]);
    const float forward = velocity.x * frame[4] + velocity.y * frame[5] + velocity.z * frame[6];
    const auto finish = [&](Steering::Controls result) {
        return scripted.temporaryAction == 2 ? Steering::reverseControls(result, forward) : result;
    };
    StorylandNativePoint target = road ? Steering::roadTarget(actor.position, *road) :
        StorylandNativePoint{scripted.targetX, scripted.targetY, scripted.targetZ};
    int32_t excludedVehicle = -1;
    if (targeted) {
        if (!runtime.missionEnvironment) return finish({});
        const auto& missionVehicles = runtime.missionEnvironment->missionVehicles;
        const auto found = missionVehicles.find(scripted.aiTargetVehicle);
        if (found == missionVehicles.end() || found->second.deleted) return finish({});
        const int32_t slot = found->second.spawnedSlot;
        if (slot < 0 || size_t(slot) >= cars.size() || size_t(slot) >= runtime.vehicles.size() ||
            runtime.vehicles[size_t(slot)].scriptHandle != found->first || size_t(slot) == index)
            return finish({});
        const auto targetFrame = nativeFrame(runtime.vehicles[size_t(slot)], cars[size_t(slot)]);
        target = {targetFrame[12], targetFrame[13], targetFrame[14]};
        if (targetedRam) excludedVehicle = slot;
        if (targetedHold) {
            if (StorylandVcsVehicleIntercept::targetEscaped(origin, target))
                vehicle.aiMission = scripted.aiMission = 18;
            return finish({0, 0, 1, true});
        }
        if (targetedIntercept) {
            target = StorylandVcsVehicleIntercept::predictedTarget(target,
                nativeVelocity(runtime.vehicles[size_t(slot)], cars[size_t(slot)]));
            vehicle.drivingStyle = scripted.drivingStyle = 2;
            if (StorylandVcsVehicleIntercept::reachedIntercept(origin, target))
                vehicle.aiMission = scripted.aiMission = 19;
        }
    }
    const float destination = Steering::bearing(target.x - origin.x, target.y - origin.y);
    const auto controls = [&](float avoidedBearing) {
        const int8_t cruise = int8_t(std::clamp(std::lround(scripted.cruiseSpeed * (6.0f / 5.0f)), -128l, 127l));
        Steering::Controls result;
        if (road && followingRoad) {
            const float original = Steering::bearing(road->position.x - actor.position.x,
                road->position.y - actor.position.y);
            const float turn = Steering::bearing(road->direction.x, road->direction.y) -
                Steering::bearing(followingRoad->direction.x, followingRoad->direction.y);
            result = Steering::roadControls(heading, original, avoidedBearing,
                std::hypot(velocity.x, velocity.y, velocity.z), forward, cruise,
                vehicle.nativeDefinition.modelId,
                std::hypot(road->position.x - actor.position.x, road->position.y - actor.position.y),
                turn, trafficSpeedFactor);
        } else result = Steering::directControls(heading, destination, avoidedBearing,
            std::hypot(velocity.x, velocity.y, velocity.z), cruise, vehicle.nativeDefinition.modelId);
        return finish(result);
    };
    if (vehicle.drivingStyle != 2) return controls(destination);
    Steering::Bearings bearings(destination);
    const auto& collision = *vehicle.nativeDefinition.collision;
    const float radius = Steering::searchRadius(std::hypot(velocity.x, velocity.y));
    for (size_t other = 0; other < cars.size() && other < runtime.vehicles.size(); ++other) {
        if (other == index || int32_t(other) == excludedVehicle) continue;
        const auto& obstacle = runtime.vehicles[other];
        if (!obstacle.collisionEnabled || !obstacle.nativeDefinition.collision ||
            (vehicle.convoy && obstacle.convoy)) continue;
        const auto& bounds = *obstacle.nativeDefinition.collision;
        const auto obstacleFrame = nativeFrame(obstacle, cars[other]);
        const auto center = storylandVehicleTransformPoint(obstacleFrame,
            {bounds.boundingCenter.x, bounds.boundingCenter.y, bounds.boundingCenter.z});
        if (std::fabs(center[0] - origin.x) >= radius ||
            std::fabs(center[1] - origin.y) >= radius ||
            std::fabs(obstacleFrame[14] - origin.z) >= 4.0f) continue;
        const StorylandNativePoint obstacleOrigin = targetedRam ?
            StorylandNativePoint{obstacleFrame[12], obstacleFrame[13], obstacleFrame[14]} : cars[other].position;
        Steering::avoidVehicle(bearings, origin, heading, velocity,
            collision.boundingRadius, collision.maximum.x, obstacleOrigin,
            nativeVelocity(obstacle, cars[other]), bounds);
    }
    const auto pedestrian = [&](StorylandNativePoint position) {
        if (Steering::inSearchSquare(origin, position, radius))
            Steering::avoidPedestrian(bearings, origin, collision.maximum.x, position);
    };
    for (size_t slot = 0; slot < pedestrians.size() && slot < runtime.pedestrians.size(); ++slot) {
        const auto& state = runtime.pedestrians[slot];
        if (state.scriptHandle >= 0 || state.renderAlpha == 0 || state.serviceVehicleIdentity) continue;
        auto position = pedestrians[slot].position;
        position.z += 1.0f;
        pedestrian(position);
    }
    if (runtime.missionEnvironment) {
        const auto& environment = *runtime.missionEnvironment;
        for (const auto& entry : environment.missionCharacters) {
            const auto& character = entry.second;
            const auto entering = environment.characterVehicleTasks.find(entry.first);
            if (!character.collisionEnabled || character.removeFromWorld ||
                (targetedRam && character.vehicle >= 0 && !character.attached) ||
                character.vehicle == vehicle.scriptHandle ||
                (entering != environment.characterVehicleTasks.end() && entering->second.vehicle == vehicle.scriptHandle))
                continue;
            const bool onFoot = character.vehicle < 0 && !character.attached;
            pedestrian({character.x, character.y, character.z + (onFoot ? 1.0f : 0.0f)});
        }
        const bool playerEntering = runtime.transitionVehicle == int32_t(index) &&
            (runtime.vehicleAction == StorylandVehicleAction::Enter ||
             runtime.vehicleAction == StorylandVehicleAction::Hijack);
        if (environment.playerCollisionEnabled && runtime.occupiedVehicle != int32_t(index) && !playerEntering &&
            (!targetedRam || (!environment.inVehicle && runtime.occupiedVehicle < 0))) {
            auto position = player;
            if (runtime.occupiedVehicle < 0) position.z += 1.0f;
            pedestrian(position);
        }
    }
    return controls(bearings.select(destination, heading));
}

int storylandSpawnVcsServiceVehicle(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const StorylandNativePaths& paths,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,
    std::vector<StorylandNativeTrafficActor>& cars,const StorylandNativePoint& player,
    const StorylandNativePoint& destination,StorylandServiceRole role,
    uint32_t modelId,uint32_t driverModelId,float arrivalRadius) {
    auto* environment=runtime.missionEnvironment;
    if(!paths.vcs||!environment||!environment->emergencyServicesEnabled||runtime.visibleArea!=0||
        runtime.paused||runtime.pauseMenu||environment->cutscene.playing||
        ((role==StorylandServiceRole::Firefighter||role==StorylandServiceRole::Paramedic)&&runtime.wanted>=4)||
        !std::isfinite(destination.x)||!std::isfinite(destination.y)||!std::isfinite(destination.z)||
        !std::isfinite(arrivalRadius)||arrivalRadius<=0||arrivalRadius>50||
        // Native service admission compares a shared vehicle count to a limit
        // initialized to 16, allowing equality. Host slots approximate the
        // original source counters and can undercount law-random vehicles.
        cars.size()>16||cars.size()>=runtime.vehicles.size()||!environment->residentModels.count(int32_t(modelId))||
        !environment->residentModels.count(int32_t(driverModelId)))return -1;
    size_t model=SIZE_MAX,driver=SIZE_MAX;
    for(size_t i=0;i<runtime.modelDefinitions.size();++i)
        if(runtime.modelDefinitions[i].modelId==modelId&&i<runtime.modelServiceRoles.size()&&
            runtime.modelServiceRoles[i]==role) {model=i;break;}
    for(size_t i=0;i<runtime.pedestrianModelStats.size()&&i<runtime.pedestrianModelRoles.size();++i)
        if(runtime.pedestrianModelStats[i].valid&&runtime.pedestrianModelStats[i].modelId==driverModelId&&
            runtime.pedestrianModelRoles[i]==role) {driver=i;break;}
    if(model==SIZE_MAX||driver==SIZE_MAX)return -1;
    StorylandRuntimeVehicle vehicle;
    vehicle.model=model;vehicle.serviceRole=role;vehicle.driverModel=driver;vehicle.hasDriver=true;
    storylandApplyVehicleDefinition(vehicle,runtime.modelDefinitions[model]);
    if(model<runtime.modelExtents.size()) {
        vehicle.halfWidth=runtime.modelExtents[model][0];
        vehicle.halfLength=runtime.modelExtents[model][1];
        if(runtime.modelExtents[model][2]>.3f)vehicle.bodyHeight=runtime.modelExtents[model][2];
    }
    if(!vehicle.nativeDefinition.hasNativeWheelDimensions&&model<runtime.modelWheelRadii.size()&&
        runtime.modelWheelRadii[model]>0)vehicle.wheelRadius=runtime.modelWheelRadii[model];
    const auto candidates=storylandCreateNativeTraffic(paths,player,true,5,30.0f,110.0f,false);
    for(auto candidate:candidates) {
        if(candidate.node>=paths.nodes.size()||paths.nodes[candidate.node].flags&0x100u)continue;
        const auto blocked=[&](const StorylandNativePoint& point) {
            return std::hypot(point.x-candidate.position.x,point.y-candidate.position.y)<10.0f;
        };
        if(blocked(player)||std::any_of(cars.begin(),cars.end(),[&](const auto& other){return blocked(other.position);})||
            std::any_of(pedestrians.begin(),pedestrians.end(),[&](const auto& other){return blocked(other.position);}))continue;
        if(environment->sphereOnScreen&&environment->sphereOnScreen(
            {candidate.position.x,candidate.position.y,candidate.position.z+1},2.0f))continue;
        const StorylandNativePoint top{candidate.position.x,candidate.position.y,candidate.position.z+1.25f};
        const StorylandNativePoint bottom{top.x,top.y,candidate.position.z-2.0f};
        uint8_t surface=0;
        const float hit=storylandTraceNativeGround(world,top,bottom,surface);
        if(hit>=1)continue;
        const float ground=top.z+(bottom.z-top.z)*hit;
        candidate.groundOffset+=ground-candidate.position.z;
        candidate.position.z=ground;
        if(!storylandNativeBodyClear(world,candidate.position,vehicleBody(vehicle,candidate.yaw)))continue;
        // Never commit a partially initialized vehicle into the traffic pool.
        if(!storylandPrepareVehicleDestinationRoute(paths,candidate,destination,&world.zones,false,
            &environment->roadChanges,0,false))continue;
        auto autopilot=std::make_shared<StorylandServiceVehicleAutopilot>();
        autopilot->role=role;autopilot->arrivalRadius=arrivalRadius;
        auto& task=autopilot->task;
        task.persistent=false;task.ambientOrigin=true;task.scriptStatus=3;
        task.aiMission=8;task.aiActive=true;task.drivingToTarget=true;
        task.targetX=destination.x;task.targetY=destination.y;task.targetZ=destination.z;
        task.cruiseSpeed=25.0f*5.0f/6.0f;task.aiMissionStartedAt=environment->gameMilliseconds;
        vehicle.serviceAutopilot=std::move(autopilot);vehicle.aiActive=true;vehicle.aiMission=8;
        vehicle.aiMissionStartedAt=environment->gameMilliseconds;
        vehicle.siren=true;vehicle.parked=false;vehicle.speed=0;
        const size_t slot=cars.size();
        runtime.vehicles[slot]=std::move(vehicle);
        cars.push_back(std::move(candidate));
        ++runtime.vehicleRespawns;
        return int(slot);
    }
    return -1;
}

bool storylandRetargetVcsServiceVehicle(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& cars,size_t slot,const StorylandNativePoint& destination) {
    if(slot>=cars.size()||slot>=runtime.vehicles.size()||!runtime.vehicles[slot].serviceAutopilot||
        !std::isfinite(destination.x)||!std::isfinite(destination.y)||!std::isfinite(destination.z))return false;
    auto& vehicle=runtime.vehicles[slot];
    auto& route=*vehicle.serviceAutopilot;
    if(route.arrived) {
        const float range=std::hypot(destination.x-cars[slot].position.x,
            destination.y-cars[slot].position.y);
        if(range<=route.arrivalRadius+1.0f)return true;
        if(!vehicle.hasDriver) {
            // The crew is already on foot. Release a stale vehicle if its
            // incident has moved far away so later responses are not blocked.
            if(range>30.0f) {storylandCancelVcsServiceVehicle(runtime,cars,slot);return false;}
            return true;
        }
        route.arrived=false;route.task.drivingToTarget=true;
        vehicle.parked=false;vehicle.siren=true;
    }
    auto& task=route.task;
    if(std::hypot(destination.x-task.targetX,destination.y-task.targetY)>.25f||
        std::fabs(destination.z-task.targetZ)>.25f) {
        task.targetX=destination.x;task.targetY=destination.y;task.targetZ=destination.z;
        task.drivingToTarget=true;task.autoPilotRoutePending=true;
        storylandInvalidateVehicleDestinationRoute(cars[slot]);
    }
    return true;
}

void storylandCancelVcsServiceVehicle(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& cars,size_t slot) {
    if(slot>=cars.size()||slot>=runtime.vehicles.size()||!runtime.vehicles[slot].serviceAutopilot)return;
    auto& vehicle=runtime.vehicles[slot];
    const bool transferred=vehicle.scriptHandle>=0||vehicle.coopControlled||
        int(slot)==runtime.occupiedVehicle||int(slot)==runtime.transitionVehicle;
    vehicle.serviceAutopilot.reset();
    if(!transferred) {vehicle.siren=false;vehicle.aiActive=false;vehicle.aiMission=0;}
    storylandInvalidateVehicleDestinationRoute(cars[slot]);
}

static bool storylandVcsAmbientVehicleFire(const StorylandPlayTestRuntime& runtime,
    const StorylandRuntimeVehicle& vehicle,size_t slot) {
    // 2BD7C8 creates an unreserved entity-fire below 225 health. The host
    // does not own the native record, so only adapt a live ambient engine fire.
    // Preserve script/generator/garage, player, coop and already-wrecked cars.
    if(!vehicle.nativeDefinition.vcsModelLayout||!vehicle.burning||vehicle.health<=0||vehicle.health>=225||
        vehicle.wrecked||vehicle.charred||vehicle.water.engineFlooded||vehicle.water.immersion>.1f||
        vehicle.scriptHandle>=0||vehicle.missionDriver||vehicle.generator>=0||vehicle.savedGarage>=0||
        vehicle.coopControlled||vehicle.craneCarried||int(slot)==runtime.occupiedVehicle||
        int(slot)==runtime.transitionVehicle||!vehicle.canBeDamaged||(vehicle.damageProofs&2u))return false;
    const auto protectedEffect=[&](const StorylandRuntimeEffect& effect) {
        return effect.vehicle==int(slot)&&(effect.permanent||effect.scriptFireHandle>=0);
    };
    return !std::any_of(runtime.effects.begin(),runtime.effects.end(),protectedEffect)&&
        !std::any_of(runtime.worldEffects.begin(),runtime.worldEffects.end(),protectedEffect);
}

void storylandDispatchVcsFireEngine(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const StorylandNativePaths& paths,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,
    std::vector<StorylandNativeTrafficActor>& cars,const StorylandNativePoint& player) {
    auto* environment=runtime.missionEnvironment;
    if(!paths.vcs||!environment)return;
    if(!environment->emergencyServicesEnabled||runtime.visibleArea!=0) {
        for(size_t slot=0;slot<cars.size()&&slot<runtime.vehicles.size();++slot) {
            const auto& route=runtime.vehicles[slot].serviceAutopilot;
            if(route&&route->role==StorylandServiceRole::Firefighter)
                storylandCancelVcsServiceVehicle(runtime,cars,slot);
        }
        return;
    }
    if(environment->cutscene.playing)return;
    unsigned engines=0;
    for(size_t slot=0;slot<cars.size()&&slot<runtime.vehicles.size();++slot)
        if(runtime.vehicles[slot].nativeDefinition.modelId==194) {
            ++engines;
            auto& route=runtime.vehicles[slot].serviceAutopilot;
            if(route&&route->role==StorylandServiceRole::Firefighter&&
                (route->incidentHandle>=0||route->incidentVehicleIdentity)) {
                StorylandNativePoint position{};
                bool available=false;
                if(route->incidentVehicleIdentity) {
                    for(size_t target=0;target<cars.size()&&target<runtime.vehicles.size();++target) {
                        const auto& candidate=runtime.vehicles[target];
                        if(candidate.emergencyFireIdentity==route->incidentVehicleIdentity&&
                            storylandVcsAmbientVehicleFire(runtime,candidate,target)) {
                            position=cars[target].position;available=true;break;
                        }
                    }
                } else {
                    const auto incident=environment->scriptFires.find(route->incidentHandle);
                    if(incident!=environment->scriptFires.end()&&incident->second.active&&
                        !incident->second.scriptOwned&&(incident->second.attachedKind==1||incident->second.attachedKind==2)&&
                        int32_t(environment->gameMilliseconds-incident->second.endsAt)<0) {
                        available=storylandVcsFireTargetPosition(*environment,incident->second,position);
                    }
                }
                if(!available)storylandCancelVcsServiceVehicle(runtime,cars,slot);
                else {
                    storylandRetargetVcsServiceVehicle(runtime,cars,slot,position);
                    return;
                }
            }
        }
    if(engines>=2||environment->vcsLastFireDispatch+35000u>=environment->gameMilliseconds)return;
    unsigned active=0;int32_t target=-1;uint64_t targetVehicle=0;
    StorylandNativePoint destination{};float best=999999.0f;
    for(const auto& entry:environment->scriptFires) {
        const auto& fire=entry.second;
        if(entry.first<0||entry.first>=40||!fire.active||fire.scriptOwned||
            int32_t(environment->gameMilliseconds-fire.endsAt)>=0)continue;
        ++active;
        // The original flag-1 query selects fires linked to a physical entity.
        // `attachedKind` is the host representation of an entity attachment.
        if(fire.scriptOwned||(fire.attachedKind!=1&&fire.attachedKind!=2))continue;
        StorylandNativePoint position{};
        if(!storylandVcsFireTargetPosition(*environment,fire,position))continue;
        const float d=std::hypot(position.x-player.x,position.y-player.y);
        if(std::isfinite(d)&&d<best) {
            best=d;target=entry.first;targetVehicle=0;destination=position;
        }
    }
    for(size_t slot=0;slot<cars.size()&&slot<runtime.vehicles.size();++slot) {
        auto& vehicle=runtime.vehicles[slot];
        if(!storylandVcsAmbientVehicleFire(runtime,vehicle,slot))continue;
        if(!vehicle.emergencyFireIdentity) {
            if(!runtime.nextEmergencyIdentity)++runtime.nextEmergencyIdentity;
            vehicle.emergencyFireIdentity=runtime.nextEmergencyIdentity++;
        }
        ++active;
        const float d=std::hypot(cars[slot].position.x-player.x,cars[slot].position.y-player.y);
        if(std::isfinite(d)&&d<best) {
            best=d;target=-1;targetVehicle=vehicle.emergencyFireIdentity;destination=cars[slot].position;
        }
    }
    if(active<3||(target<0&&!targetVehicle))return;
    const int slot=storylandSpawnVcsServiceVehicle(runtime,world,paths,pedestrians,cars,player,destination,
        StorylandServiceRole::Firefighter,194,13,10.0f);
    if(slot>=0) {
        runtime.vehicles[size_t(slot)].serviceAutopilot->incidentHandle=target;
        runtime.vehicles[size_t(slot)].serviceAutopilot->incidentVehicleIdentity=targetVehicle;
        environment->vcsLastFireDispatch=environment->gameMilliseconds;
    }
}

void storylandDispatchVcsAmbulance(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const StorylandNativePaths& paths,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,
    std::vector<StorylandNativeTrafficActor>& cars,const StorylandNativePoint& player) {
    auto* environment=runtime.missionEnvironment;
    if(!paths.vcs||!environment)return;
    if(!environment->emergencyServicesEnabled||runtime.visibleArea!=0) {
        for(size_t slot=0;slot<cars.size()&&slot<runtime.vehicles.size();++slot) {
            const auto& route=runtime.vehicles[slot].serviceAutopilot;
            if(route&&route->role==StorylandServiceRole::Paramedic)
                storylandCancelVcsServiceVehicle(runtime,cars,slot);
        }
        return;
    }
    if(environment->cutscene.playing)return;
    const auto supported=[&](const StorylandNativePoint& position) {
        uint8_t surface=0;
        return storylandTraceNativeGround(world,{position.x,position.y,position.z+.36f},
            {position.x,position.y,position.z-1.64f},surface)<1.0f;
    };
    unsigned ambulances=0;
    for(size_t slot=0;slot<cars.size()&&slot<runtime.vehicles.size();++slot) {
        auto& vehicle=runtime.vehicles[slot];
        if(vehicle.nativeDefinition.modelId!=227)continue;
        ++ambulances;
        auto& route=vehicle.serviceAutopilot;
        if(!route||route->role!=StorylandServiceRole::Paramedic||!route->incidentPedIdentity)continue;
        bool found=false;
        for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size();++index) {
            const auto& ped=runtime.pedestrians[index];
            if(ped.emergencyIdentity==route->incidentPedIdentity&&
                storylandVcsAmbientMedicCasualty(ped)&&supported(pedestrians[index].position)) {
                storylandRetargetVcsServiceVehicle(runtime,cars,slot,pedestrians[index].position);
                found=true;break;
            }
        }
        if(found)return;
        storylandCancelVcsServiceVehicle(runtime,cars,slot);
    }
    if(ambulances>=2||environment->vcsLastAmbulanceDispatch+30000u>=environment->gameMilliseconds)return;
    // The host has not decoded all native casualty admission flags; restrict
    // dispatch to the same grounded ambient casualty adapter as the medic.
    std::array<StorylandVcsMedicCasualty,20> casualties{};
    std::array<size_t,20> slots{};
    size_t used=0;
    for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size()&&used<casualties.size();++index) {
        auto& ped=runtime.pedestrians[index];
        if(!storylandVcsAmbientMedicCasualty(ped)||!supported(pedestrians[index].position))continue;
        auto& entry=casualties[used];entry.occupied=true;entry.health=ped.health;
        const auto& p=pedestrians[index].position;
        entry.nativePosition={p.x,p.y,p.z};
        if(ped.emergencyIdentity)for(const auto& medic:runtime.pedestrians)
            if(medic.health>0&&medic.scriptHandle<0&&medic.emergencyTreating&&
                medic.emergencyTarget==ped.emergencyIdentity)++entry.assignedResponders;
        slots[used++]=index;
    }
    if(used<2)return;
    const auto selected=storylandSelectVcsMedicCasualty(casualties,{player.x,player.y,player.z});
    if(selected.index<0)return;
    const size_t casualty=slots[size_t(selected.index)];
    auto& ped=runtime.pedestrians[casualty];
    if(!ped.emergencyIdentity) {
        if(!runtime.nextEmergencyIdentity)++runtime.nextEmergencyIdentity;
        ped.emergencyIdentity=runtime.nextEmergencyIdentity++;
    }
    const int slot=storylandSpawnVcsServiceVehicle(runtime,world,paths,pedestrians,cars,player,
        pedestrians[casualty].position,StorylandServiceRole::Paramedic,227,12,24.5f);
    if(slot>=0) {
        runtime.vehicles[size_t(slot)].serviceAutopilot->incidentPedIdentity=ped.emergencyIdentity;
        environment->vcsLastAmbulanceDispatch=environment->gameMilliseconds;
    }
}

// POLMAV spawn admission follows the native wanted budget and manager timer.
// Pursuit and departure use Storyland's existing flight integrator pending a
// native mission-27 controller; they are not a retail flight-parity claim.
static void storylandUpdateVcsPoliceHelicopter(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& cars,const StorylandNativePoint& player,
    const StorylandNativeWorld& world,float seconds) {
    auto* environment = runtime.missionEnvironment;
    if (!environment || !runtime.crime.vcs || runtime.paused || runtime.pauseMenu ||
        environment->cutscene.playing || runtime.visibleArea != 0) return;
    const bool wanted = runtime.wanted >= 3 && runtime.wanted <= 6 && !runtime.policeIgnorePlayer &&
        runtime.health > 0 && !runtime.busted && !runtime.wastedActive;
    size_t active = 0;
    for (size_t count = std::min(cars.size(), runtime.vehicles.size()); count > 0; --count) {
        const size_t index = count - 1;
        auto& vehicle = runtime.vehicles[index];
        if (vehicle.nativeDefinition.modelId != 261 ||
            vehicle.scriptHandle != StorylandVcsPoliceHelicopterHandle) continue;
        if (int(index) == runtime.occupiedVehicle) {
            vehicle.scriptHandle = -1;
            vehicle.aiActive = false;
            vehicle.aiMission = 0;
            continue;
        }
        if (int(index) == runtime.transitionVehicle || int(index) == storylandRemoteVehicleSlot(runtime) ||
            int(index) == storylandVcsRemoteVehicleSlot(runtime) ||
            vehicle.coopControlled || vehicle.coopSeatReserved) {
            ++active;
            continue;
        }
        if (!vehicle.aiActive || vehicle.health <= 0 || vehicle.wrecked || !vehicle.visible) {
            vehicle.scriptHandle = -1;
            continue;
        }
        if (vehicle.aiMission != 27 && vehicle.aiMission != 30) continue;
        if (!wanted && vehicle.aiMission != 30) {
            vehicle.aiMission = 30;
            cars[index].blockedSeconds = 0;
        }
        if (vehicle.aiMission == 30) {
            // Keep the departure visible for at least a second before applying
            // the native 170-unit retirement distance.
            cars[index].blockedSeconds += std::min(seconds, .1f);
            const auto& position = cars[index].position;
            const float rawRadius = vehicle.nativeDefinition.collision ?
                vehicle.nativeDefinition.collision->boundingRadius : 1.0f;
            const float radius = std::isfinite(rawRadius) ? std::max(1.0f, rawRadius) : 1.0f;
            const bool onScreen = environment->sphereOnScreen &&
                environment->sphereOnScreen({position.x, position.y, position.z}, radius);
            if (cars[index].blockedSeconds >= 1.0f && !onScreen &&
                std::hypot(position.x - player.x, position.y - player.y, position.z - player.z) > 170.0f)
                storylandRemoveFallenCar(runtime, cars, index);
            continue;
        }
        ++active;
    }
    if (!wanted && storylandResidentPopulationVehicle(runtime, 261) == SIZE_MAX) return;
    std::array<float,3> originOffset{};
    if (!environment->nativeModelOriginOffset ||
        !environment->nativeModelOriginOffset(261, originOffset) ||
        !std::isfinite(originOffset[0]) || !std::isfinite(originOffset[1]) ||
        !std::isfinite(originOffset[2]) || std::fabs(originOffset[0]) > .001f ||
        std::fabs(originOffset[1]) > .001f) return;
    const size_t model = storylandResidentPopulationVehicle(runtime, 261);
    if (model == SIZE_MAX || model >= runtime.modelDefinitions.size()) return;
    const auto& definition = runtime.modelDefinitions[model];
    if (definition.modelId != 261 || !definition.vcsModelLayout || definition.nativeType != 4 ||
        definition.domain != StorylandVehicleDomain::Air || !definition.hasNativeFlyingHandling ||
        !definition.collision) return;
    const uint32_t now = environment->gameMilliseconds;
    if (now <= runtime.policeHelicopterNextDispatch) return;
    runtime.policeHelicopterNextDispatch = now + 15000;
    if (!wanted || active || cars.size() >= runtime.vehicles.size()) return;
    StorylandRuntimeVehicle vehicle;
    vehicle.model = model;
    storylandApplyVehicleDefinition(vehicle, definition);
    if (!storylandAssignVehicleDriver(runtime, vehicle, runtime.vehicleRespawns)) return;
    vehicle.scriptHandle = StorylandVcsPoliceHelicopterHandle;
    vehicle.aiActive = true;
    vehicle.aiMission = 27;
    vehicle.parked = false;
    vehicle.rotorSpeed = .165f;
    vehicle.hasDriver = true;
    if (model < runtime.modelExtents.size()) {
        vehicle.halfWidth = runtime.modelExtents[model][0];
        vehicle.halfLength = runtime.modelExtents[model][1];
        vehicle.bodyHeight = runtime.modelExtents[model][2];
    }
    float angle = float(environment->nextNativeRandom() & 255u) * 0.02453125f;
    StorylandNativePoint position{player.x + std::cos(angle) * 250.0f,
        player.y + std::sin(angle) * 250.0f, player.z + 50.0f};
    if (position.x < -2400 || position.x > 1600 || position.y < -2000 || position.y > 2000) {
        angle += 3.14159265358979323846f;
        position.x = player.x + std::cos(angle) * 250.0f;
        position.y = player.y + std::sin(angle) * 250.0f;
    }
    float ground = 0;
    if (!environment->groundHeightAt ||
        !environment->groundHeightAt({position.x, position.y, 1000.0f}, ground) || !std::isfinite(ground)) return;
    position.z = std::max(position.z, ground + 20.0f) - originOffset[2];
    StorylandNativeTrafficActor actor;
    actor.vehicle = true;
    actor.node = actor.nextNode = UINT32_MAX;
    actor.position = position;
    actor.yaw = std::atan2(player.y - position.y, player.x - position.x) / radians;
    const auto placement = storylandMoveNativeBody(world, position, position, vehicleBody(vehicle, actor.yaw));
    if (!placement.clear) return;
    const auto body = vehicleBody(vehicle, actor.yaw);
    for (size_t other = 0; other < cars.size() && other < runtime.vehicles.size(); ++other) {
        const auto& existing = runtime.vehicles[other];
        if (existing.collisionEnabled && existing.visible &&
            storylandNativeVehicleBodiesOverlap(position, body, cars[other].position,
                vehicleBody(existing, cars[other].yaw))) return;
    }
    const size_t slot = cars.size();
    cars.push_back(actor);
    runtime.vehicles[slot] = std::move(vehicle);
    ++runtime.vehicleRespawns;
}

void storylandUpdatePlayTestVehicleEffects(StorylandPlayTestRuntime& runtime,
    const StorylandNativeTrafficActor& actor,StorylandRuntimeVehicle& vehicle,float seconds,
    const StorylandNativeWorld* world) {
    if(runtime.paused||runtime.pauseMenu||!std::isfinite(seconds)||seconds<=0)return;
    vehicleRunningEffectsImpl(runtime,actor,vehicle,std::min(seconds,.1f),world);
}

void storylandUpdatePlayTestTraffic(StorylandPlayTestRuntime& runtime,const StorylandNativePaths& paths,
    std::vector<StorylandNativeTrafficActor>& pedestrians,std::vector<StorylandNativeTrafficActor>& cars,
    const StorylandNativePoint& player,float seconds,const StorylandNativeWorld* world) {
    if(!runtime.vehiclePhysicsError.empty()||runtime.paused||runtime.pauseMenu||
        !std::isfinite(seconds)||seconds<=0)return;
    for(size_t index=0;index<cars.size()&&index<runtime.vehicles.size();++index) {
        const auto& position=cars[index].position;
        runtime.vehicles[index].scriptTravel.begin({position.x,position.y,position.z});
    }
    if(!world||!paths.vcs)for(auto& ped:runtime.pedestrians) {
        storylandClearVcsMedicTask(ped);
        storylandClearVcsFirefighterTask(ped);
    }
    const bool rubbishAllowed=world && runtime.visibleArea==0 && !runtime.playerWater.active &&
        (!runtime.missionEnvironment || (runtime.missionEnvironment->rubbishEnabled &&
                                          !runtime.missionEnvironment->cutscene.playing));
    const auto rubbishGround=[&](const StorylandNativePoint& position,float& height) {
        if(!world)return false;
        uint8_t surface=0;
        StorylandNativePoint normal{};
        const StorylandNativePoint top{position.x,position.y,position.z+5.0f};
        const StorylandNativePoint bottom{position.x,position.y,position.z-5.0f};
        const float hit=storylandTraceNativeGround(*world,top,bottom,surface,&normal);
        if(!std::isfinite(hit)||hit<0.0f||hit>=1.0f||!std::isfinite(normal.z)||normal.z<.75f)return false;
        height=top.z+(bottom.z-top.z)*hit;
        float waterHeight=0.0f;
        if(storylandNativeWaterHeight(*world,position.x,position.y,waterHeight)&&
           waterHeight>height+.02f)return false;
        return std::isfinite(height);
    };
    storylandUpdateRubbishRuntime(runtime,cars,player,seconds,rubbishAllowed,rubbishGround);
    runtime.fallenWorldFrame=paths.vcs&&runtime.missionEnvironment?
        runtime.missionEnvironment->nativeFrameCounter:runtime.fallenWorldFrame+1;
    if(world)storylandUpdateFallenWorld(runtime,paths,*world,pedestrians,cars);
    if(world&&paths.vcs) {
        storylandUpdateVcsPoliceHelicopter(runtime,cars,player,*world,seconds);
        storylandDispatchVcsFireEngine(runtime,*world,paths,pedestrians,cars,player);
        storylandDispatchVcsAmbulance(runtime,*world,paths,pedestrians,cars,player);
        for(size_t slot=0;slot<cars.size()&&slot<runtime.vehicles.size();++slot) {
            auto& vehicle=runtime.vehicles[slot];
            if(vehicle.pendingCrewModel>=0) {
                storylandRestorePendingVcsCrew(runtime,pedestrians,cars,slot);
                continue;
            }
            if(!vehicle.serviceAutopilot||!vehicle.hasDriver||vehicle.serviceCrewIdentity)continue;
            const auto model=vehicle.nativeDefinition.modelId;
            if(model==194||model==227)
                storylandCreateVcsAmbientPassenger(runtime,pedestrians,cars,slot,vehicle.driverModel,model==194?0:1);
        }
    }
    const float dt=std::min(seconds,0.1f);
    const bool cinematic=runtime.missionEnvironment&&runtime.missionEnvironment->cutscene.playing;
    for(auto& ped:runtime.pedestrians) {
        if(cinematic)ped.contact.clear();
        else storylandAdvancePedContact(ped.contact,dt);
    }
    for(auto& item:runtime.missionCombat) {
        if(cinematic)item.second.contact.clear();
        else storylandAdvancePedContact(item.second.contact,dt);
    }
    runtime.pedestrianContactPlayerVelocity={};
    if(runtime.statisticsPositionValid&&runtime.occupiedVehicle<0) {
        const float x=player.x-runtime.statisticsPosition.x,y=player.y-runtime.statisticsPosition.y;
        if(std::hypot(x,y)<=dt*15.0f)
            runtime.pedestrianContactPlayerVelocity={x/dt,y/dt,0};
    }
    storylandUpdateScriptVehicleSystems(runtime,cars,dt);
    if(!runtime.vehiclePhysicsError.empty())return;
    if(runtime.statisticsPositionValid&&runtime.health>0&&runtime.debugMovement==StorylandDebugMovement::Walking) {
        const float travelled=distance(player,runtime.statisticsPosition);
        // Ignore teleports and script warps rather than crediting them as travel.
        const char* statistic="DistanceTravelledOnFoot";
        if(runtime.occupiedVehicle>=0&&size_t(runtime.occupiedVehicle)<runtime.vehicles.size()) {
            const auto& definition=runtime.vehicles[size_t(runtime.occupiedVehicle)].nativeDefinition;
            statistic=definition.domain==StorylandVehicleDomain::Water?"DistanceTravelledByBoat":
                definition.twoWheels()?"DistanceTravelledByBike":"DistanceTravelledByCar";
        }
        if(travelled<100.0f*dt+1.0f)runtime.statistics[statistic]+=travelled;
    }
    runtime.statisticsPosition=player;runtime.statisticsPositionValid=true;
    runtime.radioPopupTime=std::max(0.0f,runtime.radioPopupTime-dt);
    const double clockSeconds=runtime.missionEnvironment?
        double(runtime.missionEnvironment->nativeFrameMilliseconds)*.001:double(dt);
    runtime.trafficClockRemainder+=double(std::llround(double(clockSeconds)*1000000))*.001;
    const auto clockAdvance=uint32_t(runtime.trafficClockRemainder);
    runtime.trafficClockMilliseconds+=clockAdvance;runtime.trafficClockRemainder-=clockAdvance;
    runtime.elapsed+=clockSeconds;
    if(!runtime.crime.vcs&&clockSeconds>0)runtime.lcsHud.advanceMoney(runtime.money);
    storylandUpdateMilitaryResponse(runtime,world,player);
    for(auto& skid:runtime.roadSkids)skid.remaining-=dt;
    runtime.roadSkids.erase(std::remove_if(runtime.roadSkids.begin(),runtime.roadSkids.end(),
        [](const auto& skid){return skid.remaining<=0;}),runtime.roadSkids.end());
    storylandAdvanceClockSeconds(runtime,clockSeconds);
    runtime.respawnFade=std::max(0.0f,runtime.respawnFade-dt);
    if(runtime.weaponBuybackAvailable&&std::hypot(player.x-runtime.weaponBuybackPosition.x,
        player.y-runtime.weaponBuybackPosition.y,player.z-runtime.weaponBuybackPosition.z)>30.0f) {
        runtime.weaponBuybackAvailable=false;
        runtime.confiscatedWeapons.clear();
    }
    if (runtime.health<=0) runtime.playerDeathTime+=dt;
    else {runtime.playerDeathTime=0;runtime.playerDeath={};}
    runtime.vehicleContactPairsThisFrame.clear();
    for(auto& vehicle:runtime.vehicles) {
        vehicle.vcsJetSkiContactImpulse=0;vehicle.vcsJetSkiContactNormal={};
        vehicle.bikeTyreAcceleration=0;vehicle.bikeTyreAccelerationWorld={};
        vehicle.bikeBeingPickedUp=false;
    }
    if(runtime.transitionVehicle>=0&&size_t(runtime.transitionVehicle)<runtime.vehicles.size()&&
        (runtime.vehicleAction==StorylandVehicleAction::Enter||runtime.vehicleAction==StorylandVehicleAction::Hijack))
        runtime.vehicles[size_t(runtime.transitionVehicle)].bikeBeingPickedUp=true;
    if(world)for(size_t i=0;i<cars.size()&&i<runtime.vehicles.size();++i) {
        runtime.vehicles[i].bodyContact=false;
        if(!scriptVehicleFrozen(runtime,runtime.vehicles[i]) && runtime.vehicles[i].nativeDefinition.domain != StorylandVehicleDomain::Air)
            recoverVehicle(*world,cars[i],runtime.vehicles[i]);
    }
    separateVehiclePairs(runtime,cars,world);
    const auto carPositions=cars;
    for(size_t index=0;index<cars.size()&&index<runtime.vehicles.size();++index) {
        auto& actor=cars[index];auto& vehicle=runtime.vehicles[index];
        if((int(index)==runtime.occupiedVehicle&&runtime.playerVehicleSeat<0)||
            int(index)==storylandRemoteVehicleSlot(runtime))vehicle.vcsTrafficRail.active=false;
        if (vehicle.coopControlled) {
            storylandUpdatePlayTestVehicleEffects(runtime, actor, vehicle, dt, world);
            continue;
        }
        if(storylandVehicleExplosionDue(vehicle,actor.water,dt))
            storylandDetonatePlayTestVehicle(runtime,world,cars,pedestrians,player,index);
        if(runtime.missionEnvironment) {
            const auto& mission=*runtime.missionEnvironment;
            if(vehicleAttachedToRope(runtime,vehicle))continue;
            if(vehicle.scriptHandle>=0&&mission.remoteVehicle==vehicle.scriptHandle)continue;
        }
        const bool playerHelicopter=storylandVcsPlayerHelicopter(runtime,vehicle,index);
        const bool nativeHovercraft=storylandVcsHovercraftPhysical(runtime,vehicle,index);
        if (!playerHelicopter&&!nativeHovercraft&&(vehicle.nativeDefinition.domain != StorylandVehicleDomain::Air ||
            (!vehicle.aiActive && !vehicle.heliCrashing))) storylandStepVehicleScriptPhysics(vehicle, actor, dt);
        vehicle.traction = std::max(0.0f, vehicle.nativeDefinition.traction * vehicle.tractionMultiplier) *
            storylandVehicleTyreGrip(vehicle);
        vehicle.displacedDriverTime = std::max(0.0f,vehicle.displacedDriverTime-dt);
        vehicle.headlightsOn=!vehicle.craneCarried&&storylandVehicleLampsRequired(runtime,vehicle)&&vehicle.health>0&&
            (!vehicle.parked||int(index)==runtime.occupiedVehicle);
        storylandAdvanceVehicleAlarm(runtime, vehicle, actor, index, clockAdvance);
        vehicle.waitingForSignal=false;
        const bool hasDriver=vehicle.hasDriver||vehicle.missionDriver;
        StorylandMissionEnvironment::VehicleScriptState* scripted=nullptr;
        if(runtime.missionEnvironment) {
            const auto found=runtime.missionEnvironment->missionVehicles.find(vehicle.scriptHandle);
            if(found!=runtime.missionEnvironment->missionVehicles.end())scripted=&found->second;
        }
        // Service cars remain ambient, but use the same physical destination
        // solver as other routed vehicles. Drop the task on ownership changes.
        if(vehicle.serviceAutopilot) {
            const bool transferred=vehicle.scriptHandle>=0||vehicle.coopControlled||
                int(index)==runtime.occupiedVehicle||int(index)==runtime.transitionVehicle;
            if(transferred) {
                // The new owner may already have installed a task or siren.
                vehicle.serviceAutopilot.reset();
                storylandInvalidateVehicleDestinationRoute(actor);
            } else if(!paths.vcs||(!vehicle.hasDriver&&!vehicle.serviceAutopilot->arrived)||
                vehicle.health<=0||vehicle.wrecked) {
                storylandCancelVcsServiceVehicle(runtime,cars,index);
            } else if(!scripted&&!vehicle.serviceAutopilot->arrived)
                scripted=&vehicle.serviceAutopilot->task;
        }
        const bool ignoreDisabledRoads=paths.vcs&&runtime.missionEnvironment&&
            runtime.missionEnvironment->vcsVehicleFlag28.count(vehicle.scriptHandle)!=0;
        if(scriptVehicleFrozen(runtime,vehicle)) {
            stopFrozenVehicle(vehicle);
            if(scripted)scripted->speed=0;
            if(vehicle.craneCarried) {
                vehicle.throttle=0;vehicle.longitudinalAcceleration=0;
                vehicle.wheelContacts={};vehicle.suspensionAnchorValid=false;vehicle.grounded=false;
            }
            storylandUpdatePlayTestVehicleEffects(runtime,actor,vehicle,dt,world);continue;
        }
        if(!paths.vcs&&world&&actor.water&&lcsScriptBoatPhysics(vehicle,scripted)&&
            int(index)!=runtime.transitionVehicle&&int(index)!=storylandRemoteVehicleSlot(runtime)) {
            advanceLcsScriptBoat(runtime,*world,actor,vehicle,*scripted,carPositions,index,dt);
            storylandUpdatePlayTestVehicleEffects(runtime,actor,vehicle,dt,world);continue;
        }
        if(!paths.vcs&&world&&actor.water&&lcsBoatPhysicsAvailable(vehicle)&&!vehicle.boatAnchored&&
            (vehicle.parked||!hasDriver||vehicle.health<=0)&&int(index)!=runtime.occupiedVehicle&&
            int(index)!=runtime.transitionVehicle&&int(index)!=storylandRemoteVehicleSlot(runtime)) {
            const bool parked=vehicle.parked;
            auto control=scripted?*scripted:StorylandMissionEnvironment::VehicleScriptState{};
            control.autoPilotMission=0;control.engineOn=vehicle.engineOn;
            advanceLcsScriptBoat(runtime,*world,actor,vehicle,control,carPositions,index,dt);
            vehicle.parked=parked;
            if(scripted) {
                scripted->x=actor.position.x;scripted->y=actor.position.y;scripted->z=actor.position.z;
                scripted->heading=actor.yaw-90;scripted->speed=vehicle.speed;
                scripted->boatAnchored=false;scripted->boatAnchorHeading=10000;
            }
            storylandUpdatePlayTestVehicleEffects(runtime,actor,vehicle,dt,world);continue;
        }
        if(!paths.vcs&&actor.water) {
            // CBoat::ProcessControl clears anchoring for player/simple/physics
            // boats. Abandoned and wrecked boats retain the authored anchor.
            if(int(index)==runtime.occupiedVehicle||!vehicle.parked) {
                vehicle.boatAnchored=false;vehicle.boatAnchorHeading=10000.0f;
            }
            if(vehicle.boatAnchored) {
                vehicle.speed=0;vehicle.lateralSpeed=0;
                if(vehicle.boatAnchorHeading==10000.0f)vehicle.boatAnchorHeading=actor.yaw;
                else actor.yaw=vehicle.boatAnchorHeading;
            } else if(vehicle.parked&&int(index)!=runtime.occupiedVehicle) {
                // Released boats retain their horizontal momentum; the water
                // integrator supplies buoyancy and resistance independently.
                auto next=actor.position;
                const float angle=actor.yaw*radians;
                next.x+=(std::cos(angle)*vehicle.speed-std::sin(angle)*vehicle.lateralSpeed)*dt;
                next.y+=(std::sin(angle)*vehicle.speed+std::cos(angle)*vehicle.lateralSpeed)*dt;
                if(!world||storylandNativeWaterSegment(*world,actor.position,next)) {
                    const auto moved=world?storylandMoveNativeBody(*world,actor.position,next,vehicleBody(vehicle,actor.yaw)):
                        StorylandNativeMoveResult{};
                    if(!world)actor.position=next;
                    else if(moved.clear)actor.position=moved.position;
                    else {vehicle.speed=0;vehicle.lateralSpeed=0;}
                    actor.yaw+=vehicle.yawVelocity*dt;
                } else {vehicle.speed=0;vehicle.lateralSpeed=0;}
            }
            if(scripted) {
                scripted->boatAnchored=vehicle.boatAnchored;
                scripted->boatAnchorHeading=vehicle.boatAnchorHeading;
            }
        }
        if (vehicle.nativeDefinition.domain == StorylandVehicleDomain::Air) {
            if(nativeHovercraft) {
                if(!world) {
                    runtime.vehiclePhysicsError="Native VCS hovercraft requires the live collision world.";
                    return;
                }
                if(!storylandAdvanceVcsHovercraft(runtime,vehicle,actor,*world,index,dt,
                    runtime.vehiclePhysicsError))return;
                storylandUpdatePlayTestVehicleEffects(runtime,actor,vehicle,dt,world);
                continue;
            }
            if(playerHelicopter) {
                if(!world) {
                    runtime.vehiclePhysicsError="Native VCS player helicopter requires the live collision world.";
                    return;
                }
                if(!storylandAdvanceVcsPlayerHelicopter(runtime,vehicle,actor,*world,dt,
                    runtime.vehiclePhysicsError))return;
                storylandUpdatePlayTestVehicleEffects(runtime,actor,vehicle,dt,world);
                continue;
            }
            const bool crashed = storylandAdvanceVehicleFlight(runtime, vehicle, actor, world, dt);
            if (crashed) {
                storylandDetonatePlayTestVehicle(runtime,world,cars,pedestrians,player,index);
            }
            continue;
        }
        if (vehicle.boatAnchored&&vehicle.nativeDefinition.domain==StorylandVehicleDomain::Water) {
            vehicle.speed = 0;
            vehicle.lateralSpeed = 0;
            vehicle.throttle = 0;
        }
        const bool passengerMovingExit=runtime.vehicleAction==StorylandVehicleAction::Exit&&
            runtime.vehicleEntryVariant==StorylandVehicleEntryVariant::MovingExit&&
            runtime.playerVehicleSeat>=0&&int(index)==runtime.transitionVehicle;
        const bool controlled=(int(index)==runtime.occupiedVehicle&&runtime.playerVehicleSeat<0)||
            (int(index)==runtime.transitionVehicle&&!passengerMovingExit)||
            int(index)==storylandRemoteVehicleSlot(runtime);
        const bool responseVehicle=storylandLawEnforcementRole(storylandVehicleServiceRole(runtime,vehicle));
        const bool wasPursuing=vehicle.policePursuing;
        const bool canRespond=responseVehicle&&!controlled&&vehicle.scriptHandle<0&&vehicle.hasDriver&&
            runtime.wanted>0&&!runtime.policeIgnorePlayer&&runtime.health>0&&
            runtime.playerControl&&runtime.visibleArea==0;
        if(paths.vcs) {
            const float playerRange=distance(actor.position,player);
            const bool seen=canRespond&&world&&
                playerRange<storylandVcsPoliceResponse::VehicleSightDistance&&
                storylandTraceNativeSegment(*world,
                    {actor.position.x,actor.position.y,actor.position.z+1.2f},
                    {player.x,player.y,player.z+1.2f})>=.99f;
            const bool noticed=canRespond&&
                storylandVcsPoliceResponse::noticedVehicle(playerRange,seen);
            if(noticed)vehicle.policeLastSeenAt=runtime.elapsed;
            vehicle.policePursuing=canRespond&&(noticed||
                (wasPursuing&&storylandVcsPoliceResponse::continueVehicle(playerRange,
                    runtime.elapsed,vehicle.policeLastSeenAt)));
        } else {
            const float playerRange=distance(actor.position,player);
            const bool sighted=canRespond&&world&&playerRange<
                storylandLcsPoliceResponse::VehicleSightDistance&&
                storylandTraceNativeSegment(*world,
                    {actor.position.x,actor.position.y,actor.position.z+1.2f},
                    {player.x,player.y,player.z+1.2f})>=.99f;
            const bool noticed=canRespond&&storylandLcsPoliceResponse::noticedVehicle(playerRange,sighted);
            if(noticed)vehicle.policeLastSeenAt=runtime.elapsed;
            vehicle.policePursuing=canRespond&&(noticed||
                (wasPursuing&&storylandLcsPoliceResponse::continueVehicle(playerRange,
                    runtime.elapsed,vehicle.policeLastSeenAt)));
        }
        if(vehicle.policePursuing)vehicle.siren=true;
        else if(wasPursuing&&!controlled&&vehicle.scriptHandle<0)vehicle.siren=false;
        if(paths.vcs&&vehicle.vcsViceUnit)
            storylandUpdateVcsViceExitRequest(runtime,cars,index,player,dt);
        const bool driverExiting=runtime.vehicleAction==StorylandVehicleAction::Exit&&
            int(index)==runtime.transitionVehicle&&runtime.playerVehicleSeat<0;
        const bool scriptedPassive=scripted&&(scripted->scriptStatus==4||scripted->scriptStatus==5||
            scripted->scriptStatus==13);
        const bool scriptedPhysics=scripted&&(scripted->scriptStatus==2||scripted->scriptStatus==3);
        const bool passiveRoad=world&&!actor.water&&vehicle.nativeDefinition.domain==StorylandVehicleDomain::Road&&
            vehicle.nativeHandling&&vehicle.nativeDefinition.hasNativeSuspension&&
            (driverExiting||(!controlled&&(scriptedPassive||
                (!scriptedPhysics&&(vehicle.parked||!hasDriver||vehicle.wrecked)))));
        if(passiveRoad) {
            const auto status=driverExiting||(scripted&&scripted->scriptStatus==13)?
                StorylandPassiveVehicleStatus::PlayerDisabled:
                vehicle.wrecked||(scripted&&scripted->scriptStatus==5)?
                    StorylandPassiveVehicleStatus::Wrecked:StorylandPassiveVehicleStatus::Abandoned;
            const bool driverMustBrake=driverExiting&&
                runtime.vehicleEntryVariant!=StorylandVehicleEntryVariant::MovingExit;
            advancePassiveRoadVehicle(runtime,vehicle,actor,*world,paths,status,driverMustBrake,dt);
            if(scripted) {
                scripted->x=actor.position.x;scripted->y=actor.position.y;scripted->z=actor.position.z;
                scripted->heading=actor.yaw-90.0f;scripted->speed=vehicle.speed;
                scripted->movedByScript=false;
            }
            storylandUpdatePlayTestVehicleEffects(runtime,actor,vehicle,dt,world);
            continue;
        }
        const bool nativeTargetedMission=paths.vcs&&scripted&&vehicle.aiActive&&
            (vehicle.aiMission==16||vehicle.aiMission==18||vehicle.aiMission==19);
        const bool physicalReverseFrame=paths.vcs&&scripted&&scripted->temporaryAction==2&&
            (scripted->drivingToTarget||nativeTargetedMission)&&!controlled&&world&&!actor.water&&vehicle.health>0&&
            !vehicle.water.engineFlooded&&vehicle.nativeHandling&&vehicle.nativeDefinition.hasNativeSuspension&&
            vehicle.nativeDefinition.collision&&(nativeTargetedMission||vehicle.aiMission==8||vehicle.aiMission==12||
                ((vehicle.aiMission==9||vehicle.aiMission==13)&&
                    (vehicle.drivingStyle==2||vehicle.drivingStyle==3)));
        if(scripted&&scripted->temporaryAction!=0&&!physicalReverseFrame&&runtime.missionEnvironment&&
            int32_t(runtime.missionEnvironment->gameMilliseconds-scripted->temporaryActionUntil)>0) {
            if(paths.vcs&&scripted->temporaryAction==5)vehicle.handbrake=false;
            scripted->temporaryAction=0;
        }
        const bool temporaryHandbrake=paths.vcs&&scripted&&scripted->temporaryAction==5;
        if(temporaryHandbrake&&!controlled&&world&&!actor.water&&
            vehicle.nativeDefinition.domain==StorylandVehicleDomain::Road) {
            vehicle.steeringAngle=0;vehicle.steeringInput=0;vehicle.steeringInputValid=true;
            vehicle.handbrake=true;vehicle.throttle=0;vehicle.braking=false;
            const bool nativeTyres=vehicle.nativeHandling&&vehicle.nativeDefinition.hasNativeSuspension;
            if(nativeTyres) {
                if(!vehicle.contactsValid)updateVehicleWheelContacts(vehicle,actor,*world,0);
                const float rain=std::clamp(runtime.missionEnvironment->weatherState.rain,0.0f,1.0f);
                applyVehicleTyreForces(vehicle,actor,0,0,rain,dt,false,false,
                    runtime.missionEnvironment->controllerMode);
            }
            settleRoadVehicle(vehicle,actor,*world,paths,dt,runtime.gravity,!nativeTyres);
            scripted->x=actor.position.x;scripted->y=actor.position.y;scripted->z=actor.position.z;
            scripted->heading=actor.yaw-90;scripted->speed=vehicle.speed;
            scripted->movedByScript=false;
            storylandUpdatePlayTestVehicleEffects(runtime,actor,vehicle,dt,world);
            continue;
        }
        const bool temporaryForward=scripted&&scripted->temporaryAction==8;
        const bool scriptDriven=scripted&&scripted->drivingToTarget&&!temporaryForward;
        const bool routeDriven=scriptDriven&&!controlled&&!actor.water&&
            vehicle.nativeDefinition.domain==StorylandVehicleDomain::Road&&vehicle.health>0&&
            !vehicle.water.engineFlooded;
        const bool accurateRoute=paths.vcs&&scriptDriven&&
            (scripted->aiMission==12||scripted->aiMission==13);
        const float destinationRadius=vehicle.serviceAutopilot?
            vehicle.serviceAutopilot->arrivalRadius:(accurateRoute?1.0f:3.0f);
        if (!routeDriven&&actor.destinationRouteValid) {
            actor.destinationRouteValid=false;
            actor.destinationRouteFinished=false;
            actor.destinationRoute.clear();
        }
        if(world&&!controlled&&!scriptDriven&&!temporaryForward&&
            (vehicle.parked||!hasDriver||vehicle.health<=0||vehicle.water.engineFlooded)) {
            if(actor.water)applyVehicleWater(vehicle,actor.position,*world,dt,runtime.gravity,true,actor.yaw);
            else settleRoadVehicle(vehicle,actor,*world,paths,dt,runtime.gravity);
        }
        storylandUpdatePlayTestVehicleEffects(runtime,actor,vehicle,dt,world);
        if (routeDriven) {
            const StorylandNativePoint destination{scripted->targetX,scripted->targetY,scripted->targetZ};
            if(paths.vcs&&scripted->autoPilotRoutePending) {
                storylandInvalidateVehicleDestinationRoute(actor);
                scripted->autoPilotRoutePending=false;
            }
            const bool reached=paths.vcs?
                std::hypot(actor.position.x-destination.x,actor.position.y-destination.y)<destinationRadius:
                storylandVehicleDestinationReached(actor.position,destination);
            if (reached) {
                scripted->drivingToTarget=false;
                scripted->movedByScript=false;
                scripted->speed=0;
                if(paths.vcs) {
                    vehicle.aiMission=scripted->aiMission=0;
                    scripted->temporaryAction=0;
                }
                vehicle.speed=0;
                vehicle.parked=true;
                vehicle.braking=true;
                vehicle.throttle=0;
                if(vehicle.serviceAutopilot) {
                    vehicle.siren=false;
                    vehicle.serviceAutopilot->arrived=true;
                }
                continue;
            }
            const auto* roadChanges=runtime.missionEnvironment?&runtime.missionEnvironment->roadChanges:nullptr;
            const uint64_t roadRevision=storylandVehicleRoadRevision(roadChanges,ignoreDisabledRoads);
            if(paths.vcs&&actor.destinationRouteValid&&actor.destinationRouteFinished&&
                actor.destinationRoute.empty()&&(runtime.fallenWorldFrame&7u)==0&&
                std::hypot(destination.x-actor.position.x,destination.y-actor.position.y)>
                    float(vehicle.straightLineDistance)+5.0f)
                actor.destinationRouteValid=false;
            const bool sameRequest=actor.destinationRouteValid==false &&
                storylandVehicleRouteDistance(destination,actor.destinationRouteTarget)<=0.25f &&
                actor.destinationRoadRevision==roadRevision &&
                actor.destinationAvoidLevelTransitions==vehicle.avoidLevelTransitions;
            const bool retryLater=sameRequest&&actor.destinationRetryAfter>runtime.elapsed;
            if (retryLater || !storylandPrepareVehicleDestinationRoute(paths,actor,destination,
                world?&world->zones:nullptr,vehicle.avoidLevelTransitions,roadChanges,
                vehicle.straightLineDistance,ignoreDisabledRoads)) {
                actor.destinationRouteTarget=destination;
                actor.destinationRoadRevision=roadRevision;
                actor.destinationAvoidLevelTransitions=vehicle.avoidLevelTransitions;
                if (!retryLater) actor.destinationRetryAfter=runtime.elapsed+1.0f;
                vehicle.speed=0;
                vehicle.braking=true;
                vehicle.throttle=0;
                continue;
            }
            actor.destinationRetryAfter=0;
            if(paths.vcs)vehicle.aiMission=scripted->aiMission=
                int8_t((accurateRoute?12:8)+(actor.destinationRouteFinished?1:0));
            vehicle.parked=false;
        } else if (scriptDriven) {
            if(!controlled&&!actor.water) {
                if(world)settleRoadVehicle(vehicle,actor,*world,paths,dt,runtime.gravity);
                scripted->z=actor.position.z;
            }
            continue;
        }
        if(controlled||(vehicle.parked&&!routeDriven&&!temporaryForward)||
            (!hasDriver&&!routeDriven&&!temporaryForward)||vehicle.health<=0||
            vehicle.water.engineFlooded||(actor.water&&vehicle.boatAnchored))continue;
        const bool missionStop=!temporaryForward&&!paths.vcs&&
            (vehicle.autoPilotMission==0||vehicle.autoPilotMission==10||vehicle.autoPilotMission==11);
        if(!paths.vcs&&vehicle.autoPilotMission>=0)vehicle.handbrake=missionStop;
        const uint8_t drivingStyle=!paths.vcs&&!actor.water&&scripted?scripted->drivingStyle:0;
        const bool obeyLights=!vehicle.policePursuing&&!nativeTargetedMission&&
            (paths.vcs?vehicle.drivingStyle<=1&&vehicle.aiMission!=5:drivingStyle!=4);
        if (vehicle.aiActive &&
            (vehicle.aiMission == 0 || vehicle.aiMission == 10 || vehicle.aiMission == 11) &&
            !temporaryForward && !routeDriven) {
            vehicle.speed = approach(vehicle.speed, 0, vehicle.handlingBrake * dt);
            vehicle.braking = vehicle.speed > 0;
            if(world&&!actor.water)settleRoadVehicle(vehicle,actor,*world,paths,dt,runtime.gravity);
            continue;
        }
        float signalSpeedLimit=std::numeric_limits<float>::infinity();
        float target=routeDriven?std::max(0.0f,scripted->cruiseSpeed):8.0f;
        float cruiseTarget=target;
        StorylandVcsVehicleSteering::RoadLink currentRoad, followingRoad;
        bool nativeRoadTyres=paths.vcs&&world&&routeDriven&&
            (vehicle.aiMission==8||vehicle.aiMission==12)&&vehicle.nativeHandling&&
            vehicle.nativeDefinition.hasNativeSuspension&&vehicle.nativeDefinition.collision;
        if(nativeRoadTyres) {
            nativeRoadTyres=storylandVcsRoadSteeringLinks(paths,actor,&world->zones,
                vehicle.avoidLevelTransitions,&runtime.missionEnvironment->roadChanges,
                vehicle.straightLineDistance,ignoreDisabledRoads,currentRoad,followingRoad);
            if(actor.destinationRouteFinished)
                vehicle.aiMission=scripted->aiMission=int8_t(accurateRoute?13:9);
        }
        const bool nativeDirectTyres=paths.vcs&&world&&
            (nativeTargetedMission||(routeDriven&&(vehicle.aiMission==9||vehicle.aiMission==13)&&
                (vehicle.drivingStyle==2||vehicle.drivingStyle==3)))&&
            vehicle.nativeHandling&&vehicle.nativeDefinition.hasNativeSuspension&&vehicle.nativeDefinition.collision;
        if(nativeTargetedMission&&!nativeDirectTyres) {
            runtime.vehiclePhysicsError="Native targeted car controls require original handling, suspension and collision.";
            return;
        }
        const bool nativeTyres=nativeDirectTyres||nativeRoadTyres;
        const StorylandMissionEnvironment::VehicleScriptState* scriptTask = nullptr;
        if (vehicle.aiActive && runtime.missionEnvironment) {
            const auto task = runtime.missionEnvironment->missionVehicles.find(vehicle.scriptHandle);
            if (task != runtime.missionEnvironment->missionVehicles.end()) scriptTask = &task->second;
        }
        StorylandNativePoint blockTarget{};
        bool blockingVehicle = false;
        bool directBlock = false;
        const bool ramPlayer=paths.vcs&&scriptTask&&vehicle.aiMission==3;
        const bool interceptPlayer=paths.vcs&&scriptTask&&vehicle.aiMission==5;
        const bool holdPlayer=paths.vcs&&scriptTask&&vehicle.aiMission==6;
        const bool nativePlayerPursuit=ramPlayer||interceptPlayer||holdPlayer;
        if(nativePlayerPursuit) {
            blockTarget=player;
            auto velocity=runtime.pedestrianContactPlayerVelocity;
            if(runtime.occupiedVehicle>=0&&size_t(runtime.occupiedVehicle)<carPositions.size()&&
                size_t(runtime.occupiedVehicle)<runtime.vehicles.size()) {
                const size_t playerSlot=size_t(runtime.occupiedVehicle);
                const auto& playerCar=carPositions[playerSlot];
                const auto& playerVehicle=runtime.vehicles[playerSlot];
                const float angle=playerCar.yaw*radians;
                blockTarget=playerCar.position;
                velocity={std::cos(angle)*playerVehicle.speed-std::sin(angle)*playerVehicle.lateralSpeed,
                    std::sin(angle)*playerVehicle.speed+std::cos(angle)*playerVehicle.lateralSpeed,0};
            }
            if(interceptPlayer) {
                // Native 0x439928 leads by 60 simulation steps, with planar
                // velocity capped at 0.13 world units per native step.
                const float speed=std::hypot(velocity.x,velocity.y);
                const float scale=std::min(1.0f,.13f*StorylandVehicleStepsPerSecond/
                    std::max(.0001f,speed))*60.0f/StorylandVehicleStepsPerSecond;
                blockTarget.x+=velocity.x*scale;
                blockTarget.y+=velocity.y*scale;
                vehicle.drivingStyle=scripted->drivingStyle=2;
            }
            const float dx=blockTarget.x-actor.position.x,dy=blockTarget.y-actor.position.y;
            const float separationSquared=dx*dx+dy*dy;
            if(holdPlayer&&separationSquared>100.0f)
                vehicle.aiMission=scripted->aiMission=5;
            else if(interceptPlayer&&separationSquared<25.0f)
                vehicle.aiMission=scripted->aiMission=6;
            vehicle.handbrake=holdPlayer;
            blockingVehicle=true;
            directBlock=!holdPlayer;
            target=cruiseTarget=std::max(0.0f,scriptTask->cruiseSpeed);
        }
        if(vehicle.policePursuing) {
            blockTarget=player;
            blockingVehicle=true;
            if(paths.vcs) {
                const float playerRange=distance(actor.position,player);
                const bool clearRoad=world&&
                    storylandTraceNativeSegment(*world,
                        {actor.position.x,actor.position.y,actor.position.z+1.0f},
                        {player.x,player.y,player.z+1.0f})>=.99f;
                directBlock=storylandVcsPoliceResponse::directIntercept(playerRange,clearRoad,
                    runtime.wanted,runtime.occupiedVehicle>=0,
                    float(vehicle.straightLineDistance));
            } else {
                const bool roadClear=world&&
                    storylandTraceNativeSegment(*world,
                        {actor.position.x,actor.position.y,actor.position.z+1.0f},
                        {player.x,player.y,player.z+1.0f})>=.99f;
                directBlock=storylandLcsPoliceResponse::directIntercept(
                    distance(actor.position,player),roadClear,runtime.wanted,
                    runtime.occupiedVehicle>=0,float(vehicle.straightLineDistance));
            }
        }
        if (scriptTask && (vehicle.aiMission == 15 || (!paths.vcs && vehicle.aiMission == 16))) {
            const auto victim = runtime.missionEnvironment->missionVehicles.find(vehicle.aiTargetVehicle);
            if (victim != runtime.missionEnvironment->missionVehicles.end() && victim->second.alive && !victim->second.deleted) {
                const auto& other = victim->second;
                const float forward = (other.heading + 90) * radians;
                const float anticipation = vehicle.halfLength + 3 + std::max(0.0f, other.speed) * .8f;
                blockTarget = {other.x + std::cos(forward) * anticipation,
                    other.y + std::sin(forward) * anticipation, other.z};
                blockingVehicle = true;
                directBlock = vehicle.aiMission == 16 ||
                    distance(actor.position, blockTarget) <= float(vehicle.straightLineDistance);
            }
        }
        if (!paths.vcs && scriptTask && (vehicle.aiMission == 5 || vehicle.aiMission == 26)) {
            blockTarget = player; blockingVehicle = true; directBlock = true;
        }
        if(actor.node<paths.nodes.size()) {
            const auto& node=paths.nodes[actor.node];
            target=node.speedLimit==1?5.0f:node.speedLimit>=3?8.0f:6.5f;
            if (scriptTask) target = std::max(0.0f, scriptTask->cruiseSpeed);
            if (routeDriven) target=paths.vcs?std::max(0.0f,scripted->cruiseSpeed):
                scripted->cruiseSpeed>0?scripted->cruiseSpeed:5.0f;
            cruiseTarget=target;
            if(vehicle.policePursuing&&!actor.water)
                target=std::max(target,paths.vcs?
                    storylandVcsPoliceResponse::pursuitCruise(runtime.wanted):
                    storylandLcsPoliceResponse::pursuitCruise(runtime.wanted));
            for(const auto& link:node.links)if(obeyLights&&!actor.water&&!link.nativeLanes&&link.target==actor.nextNode&&link.trafficLight&&
                std::fmod(runtime.elapsed+float(actor.node%2)*12,24.0f)<10)target=0;
            for(const auto& link:node.links)if(obeyLights&&!actor.water&&link.target==actor.nextNode&&link.signalApplies&&
                actor.nextNode<paths.nodes.size()&&storylandRetailTrafficLight(link.signalGroup,
                    runtime.trafficClockMilliseconds,std::hypot(runtime.windVelocity.x,runtime.windVelocity.y),
                    runtime.trafficLightsForcedGreen,paths.vcs)!=StorylandTrafficLight::Green) {
                const float edge=distance(node.position,paths.nodes[actor.nextNode].position);
                // Native stop test uses the car-link midpoint and the approaching
                // direction, with an eight-unit approach interval (0x312E84..ED8).
                const float remaining=link.nativeStopLine?
                    (link.stopPosition.x-actor.position.x)*link.stopDirection.x+
                    (link.stopPosition.y-actor.position.y)*link.stopDirection.y:
                    edge*.5f-actor.distance;
                if(remaining>0&&remaining<8.0f) {
                    vehicle.waitingForSignal=true;
                    const float clearance=std::max(0.0f,remaining-vehicle.halfLength-.25f);
                    signalSpeedLimit=std::min(signalSpeedLimit,clearance/dt);
                    target=std::min(target,std::min(clearance/dt,
                        std::sqrt(2*std::max(1.0f,vehicle.handlingBrake)*clearance)));
                }
            }
        }
        const float forwardX=std::cos(actor.yaw*radians),forwardY=std::sin(actor.yaw*radians);
        const auto ahead=[&](StorylandNativePoint point,float limit) {
            const float dx=point.x-actor.position.x,dy=point.y-actor.position.y;
            return std::fabs(point.z-actor.position.z)<2.5f&&dx*forwardX+dy*forwardY>0&&
                std::fabs(-dx*forwardY+dy*forwardX)<vehicle.halfWidth+.9f&&distance(point,actor.position)<limit;
        };
        const float brakingDistance=vehicle.speed*vehicle.speed/std::max(2.0f,vehicle.handlingBrake*1.6f);
        const bool playerAhead = !ramPlayer&&runtime.occupiedVehicle<0&&
            ahead(player,vehicle.halfLength+1.0f+brakingDistance);
        bool trafficAhead = false;
        float followingSpeedLimit = std::numeric_limits<float>::infinity();
        for(size_t other=0;other<cars.size();++other)
            if(other!=index&&other<runtime.vehicles.size()&&!(ramPlayer&&int(other)==runtime.occupiedVehicle)&&
                !runtime.vehicles[other].craneCarried&&cars[other].water==actor.water&&
                !(vehicle.convoy && runtime.vehicles[other].convoy) &&
                ahead(cars[other].position,vehicle.halfLength+runtime.vehicles[other].halfLength+1.5f+brakingDistance)) {
                trafficAhead=true;
                vehicle.waitingForSignal=vehicle.waitingForSignal||runtime.vehicles[other].waitingForSignal;
                // CarCtrl follows a moving car by closing-speed braking, not
                // by cutting its cruise target to zero every simulation step.
                // Work in actual front/rear bumper clearance, preserving the
                // lead vehicle's velocity and a small speed-dependent gap.
                const auto& lead=runtime.vehicles[other];
                const float dx=cars[other].position.x-actor.position.x;
                const float dy=cars[other].position.y-actor.position.y;
                const float clearance=std::max(0.0f,dx*forwardX+dy*forwardY-
                    vehicle.halfLength-lead.halfLength);
                const float leadYaw=cars[other].yaw*radians;
                const float leadVelocity=std::max(0.0f,lead.speed*(
                    std::cos(leadYaw)*forwardX+std::sin(leadYaw)*forwardY));
                const float gap=1.0f+std::max(0.0f,vehicle.speed)*0.22f;
                const float closing=std::sqrt(2.0f*std::max(0.5f,vehicle.handlingBrake)*
                    std::max(0.0f,clearance-gap));
                followingSpeedLimit=std::min(followingSpeedLimit,leadVelocity+closing);
            }
        bool pedestrianAhead=playerAhead;
        for(size_t ped=0;ped<pedestrians.size()&&ped<runtime.pedestrians.size();++ped) {
            if(runtime.pedestrians[ped].serviceVehicleIdentity)continue;
            if(ahead(pedestrians[ped].position,vehicle.halfLength+1.0f+brakingDistance)) {pedestrianAhead=true;break;}
        }
        if(!paths.vcs) {
            if(actor.water)target=!paths.vcs&&scripted&&scripted->autoPilotMission>=0?
                std::max(0.0f,scripted->cruiseSpeed):5.0f;
            if(vehicle.autoPilotMission==1&&scripted)cruiseTarget=std::max(0.0f,scripted->cruiseSpeed);
            // Native styles 0/4 use the traffic-limited speed, while style 1
            // averages that limit with cruise speed. The existing danger scan's
            // blocked limit is zero. Red lights remain a separate, strict cap.
            // Style 4 ignores pedestrian anticipation in native CarCtrl, too;
            // physical collision handling still runs below.
            if(trafficAhead)target=std::min(target,actor.water?
                (drivingStyle==1?cruiseTarget*.5f:0.0f):followingSpeedLimit);
            if(drivingStyle!=4&&pedestrianAhead)
                target=std::min(target,drivingStyle==1?cruiseTarget*.5f:0.0f);
        } else if(!nativeTyres||(nativeRoadTyres&&vehicle.drivingStyle!=2&&vehicle.drivingStyle!=3)) {
            if(actor.water) target=5.0f;
            if (playerAhead && vehicle.drivingStyle != 3) target=0;
            else if (vehicle.drivingStyle != 3) {
                if(trafficAhead)target=std::min(target,actor.water?
                    (vehicle.drivingStyle==2?1.0f:vehicle.drivingStyle==1?2.0f:0.0f):
                    followingSpeedLimit);
                if(pedestrianAhead)target=std::min(target,vehicle.drivingStyle==2?1.0f:
                    vehicle.drivingStyle==1?2.0f:0.0f);
            }
        }
        if (blockingVehicle&&!nativePlayerPursuit)
            target = std::min(target, std::max(0.0f, distance(actor.position, blockTarget) - 1));
        float cornerDistance=0;
        const float corner=storylandNativeTrafficTurn(paths,actor,cornerDistance,ignoreDisabledRoads);
        if(!nativeTyres&&!nativePlayerPursuit&&!actor.water&&corner>.15f&&cornerDistance<5.0f+brakingDistance) {
            const float radius=std::max(1.0f,3.0f/std::max(.2f,std::tan(std::min(corner*.5f,1.4f))));
            const float cornerSpeed=std::sqrt(std::max(.5f,vehicle.traction)*6.0f*radius);
            target=std::min(target,cornerSpeed);
        }
        if(vehicle.autoPilotMission==1&&scripted&&!routeDriven)target=std::min(target,std::max(0.0f,scripted->cruiseSpeed));
        if (!nativeTyres&&routeDriven&&actor.destinationRouteFinished&&!actor.destinationRoute.empty()) {
            const float dx=scripted->targetX-actor.position.x;
            const float dy=scripted->targetY-actor.position.y;
            const float desired=std::atan2(dy,dx)*57.295779513f;
            const float angle=std::fabs(std::remainder(desired-actor.yaw,360.0f));
            target=std::min(target,std::clamp(6.0f-angle*0.065f,0.0f,3.5f));
        }
        if (!nativeTyres&&routeDriven&&actor.destinationRouteCursor+1>=actor.destinationRoute.size()) {
            const float remaining=std::hypot(actor.position.x-scripted->targetX,
                actor.position.y-scripted->targetY);
            const float braking=std::max(1.0f,vehicle.handlingBrake);
            target=std::min(target,std::sqrt(2.0f*braking*std::max(0.0f,remaining-1.5f)));
        }
        if((missionStop&&!routeDriven)||holdPlayer||vehicle.vcsViceExitRequested) {target=0;vehicle.throttle=0;}
        if(temporaryForward) {
            target=std::max(0.0f,vehicle.handlingTopSpeed);
            signalSpeedLimit=std::numeric_limits<float>::infinity();
            vehicle.throttle=1;vehicle.braking=false;vehicle.handbrake=false;
        }
        if(!paths.vcs&&world&&actor.water&&lcsBoatPhysicsAvailable(vehicle)) {
            // Retain the existing traffic route sampler for ambient boats,
            // but let CBoat propulsion move their hull. A signed boat torque
            // coefficient must never enter the automobile tyre-grip clamp.
            const auto advanceRoute=[&](StorylandNativeTrafficActor& route,float metres) {
                for(unsigned step=0;step<32&&metres>0;++step) {
                    const float distance=std::min(2.4f,metres);
                    storylandAdvanceNativeTraffic(paths,route,distance/8);metres-=distance;
                }
            };
            auto targetRoute=actor,progress=actor;
            advanceRoute(targetRoute,std::max(6.0f,std::fabs(vehicle.speed)*.8f+2));
            auto control=scripted?*scripted:StorylandMissionEnvironment::VehicleScriptState{};
            control.scriptStatus=3;control.engineOn=vehicle.engineOn;
            control.autoPilotMission=target>0&&distance(targetRoute.position,actor.position)>.01f?14:0;
            control.cruiseSpeed=target;control.targetX=targetRoute.position.x;
            control.targetY=targetRoute.position.y;control.targetZ=targetRoute.position.z;
            const auto previousPosition=actor.position;
            advanceLcsScriptBoat(runtime,*world,actor,vehicle,control,carPositions,index,dt);
            if(vehicle.speed>=0)advanceRoute(progress,std::hypot(actor.position.x-previousPosition.x,
                actor.position.y-previousPosition.y));
            actor.node=progress.node;actor.nextNode=progress.nextNode;actor.previousNode=progress.previousNode;
            actor.routeSeed=progress.routeSeed;actor.distance=progress.distance;
            actor.crossingCommitted=progress.crossingCommitted;
            if(scripted) {
                scripted->x=actor.position.x;scripted->y=actor.position.y;scripted->z=actor.position.z;
                scripted->heading=actor.yaw-90;scripted->speed=vehicle.speed;
                scripted->boatAnchored=false;scripted->boatAnchorHeading=10000;
            }
            continue;
        }
        if(routeDriven&&vehicle.serviceAutopilot) {
            // Service destinations are incidents, often a physical car or
            // casualty. Brake before the standoff radius instead of driving
            // through the target and only stopping at its centre.
            const float remaining=std::hypot(actor.position.x-scripted->targetX,
                actor.position.y-scripted->targetY)-vehicle.serviceAutopilot->arrivalRadius;
            const float brake=std::max(.5f,std::min(vehicle.handlingBrake*.2f,2.0f));
            target=std::min(target,std::sqrt(2.0f*brake*std::max(0.0f,remaining)));
        }
        const float previousSpeed=vehicle.speed;
        const float engine=std::clamp(vehicle.handlingEngine*0.72f*
            std::sqrt(1500.0f/std::max(100.0f,vehicle.mass)),1.5f,14.0f);
        const bool nativeTraffic=paths.vcs&&vehicle.nativeHandling&&!actor.water&&
            vehicle.nativeDefinition.domain==StorylandVehicleDomain::Road;
        StorylandVcsVehicleSteering::ReverseState recovery;
        const uint8_t creationSource=scripted&&scripted->persistent?2:1;
        if(nativeTyres) {
            const uint32_t now=runtime.missionEnvironment->gameMilliseconds;
            if(!vehicle.contactsValid) {
                updateVehicleWheelContacts(vehicle,actor,*world,0);
                if(vehicle.bodyContact||std::any_of(vehicle.wheelContacts.begin(),vehicle.wheelContacts.end(),
                    [](bool contact){return contact;}))vehicle.lastNativeVehicleContactAt=now;
            }
            recovery={scripted->aiMissionStartedAt,vehicle.lastNativeTrafficMovementAt,
                vehicle.lastNativeVehicleContactAt,scripted->temporaryActionUntil,
                scripted->temporaryAction,vehicle.drivingStyle};
            const float nativeForward=vehicle.speed/StorylandVehicleStepsPerSecond;
            const float nativeLateral=vehicle.lateralSpeed/StorylandVehicleStepsPerSecond;
            const int8_t cruise=int8_t(std::clamp(std::lround(scripted->cruiseSpeed*(6.0f/5.0f)),-128l,127l));
            StorylandVcsVehicleSteering::requestReverse(recovery,now,
                nativeForward*nativeForward+nativeLateral*nativeLateral,scripted->scriptStatus,
                vehicle.aiMission,cruise,creationSource);
            vehicle.aiMissionStartedAt=scripted->aiMissionStartedAt=recovery.missionStartedAt;
            vehicle.lastNativeTrafficMovementAt=recovery.lastMovingAt;
            vehicle.drivingStyle=scripted->drivingStyle=recovery.drivingStyle;
            scripted->temporaryAction=recovery.action;
            scripted->temporaryActionUntil=recovery.deadline;
        }
        const auto nativeControls=nativeTyres?
            storylandVcsTrafficControls(runtime,actor,vehicle,*scripted,index,carPositions,pedestrians,player,
                nativeRoadTyres?&currentRoad:nullptr,nativeRoadTyres?&followingRoad:nullptr,
                cruiseTarget>0?std::clamp(target/cruiseTarget,0.0f,1.0f):0.0f):
            StorylandVcsVehicleSteering::Controls{};
        if(nativeTyres) {
            recovery.drivingStyle=vehicle.drivingStyle;
            vehicle.steeringAngle=nativeControls.steering/radians;
            vehicle.steeringInputValid=false;
            vehicle.handbrake=nativeControls.handbrake;
            vehicle.throttle=vehicle.engineOn&&vehicle.health>0&&!vehicle.water.engineFlooded?
                nativeControls.accelerator:0.0f;
            vehicle.braking=nativeControls.brake>0.0f;
            if(!vehicle.contactsValid)updateVehicleWheelContacts(vehicle,actor,*world,0);
            const bool nativeContactRetention=vehicle.nativeDefinition.vcsModelLayout&&
                vehicle.nativeDefinition.nativeType==0&&vehicle.contactsValid;
            const bool nativeBikeRetention=vehicle.nativeDefinition.motorcycle()&&vehicle.contactsValid;
            const bool drivenWheelsOnGround=nativeContactRetention?
                StorylandVcsVehicleContactTimers::contactCounts(vehicle.wheelContactTimers,
                    vehicle.nativeDefinition.driveType).driven>0:
                nativeBikeRetention?StorylandVcsVehicleContactTimers::bikeRearWheelContact(
                    vehicle.wheelContactTimers):
                storylandVehicleDriveGrip(vehicle.nativeDefinition,vehicle.wheelContacts,
                    vehicle.grounded,vehicle.contactsValid)>0;
            const float drive=storylandVehicleTransmissionDrive(vehicle.nativeDefinition,vehicle.transmission,
                vehicle.speed,vehicle.throttle,dt,drivenWheelsOnGround)/vehicle.massMultiplier;
            const float brake=storylandRetailVehicleBrakeDelta(vehicle.nativeDefinition,nativeControls.brake,dt);
            const float rain=runtime.missionEnvironment?
                std::clamp(runtime.missionEnvironment->weatherState.rain,0.0f,1.0f):0.0f;
            applyVehicleTyreForces(vehicle,actor,drive*dt,brake,rain,dt,false,false,
                runtime.missionEnvironment?runtime.missionEnvironment->controllerMode:0);
            StorylandVcsVehicleSteering::expireReverse(recovery,
                runtime.missionEnvironment->gameMilliseconds,creationSource);
            scripted->temporaryAction=recovery.action;
            vehicle.drivingStyle=scripted->drivingStyle=recovery.drivingStyle;
        } else if(nativeTraffic) {
            if(vehicle.nativeDefinition.maximumAiVelocity>0)
                target=std::min(target,vehicle.nativeDefinition.maximumAiVelocity*StorylandVehicleStepsPerSecond);
            vehicle.throttle=vehicle.engineOn&&vehicle.health>0&&!vehicle.water.engineFlooded&&
                target>vehicle.speed?1.0f:0.0f;
        }
        float trafficEngine=nativeTraffic?engine*vehicle.throttle:engine;
        if(!nativeTyres&&nativeTraffic&&world&&vehicle.nativeDefinition.hasNativeSuspension&&
            !vehicle.contactsValid)
            updateVehicleWheelContacts(vehicle,actor,*world,0);
        if(!nativeTyres&&vehicle.nativeHandling&&(paths.vcs||!actor.water||vehicle.nativeDefinition.nativeType!=1)) {
            const float grip=vehicle.traction*9.81f*(actor.water?(vehicle.nativeDefinition.driveType=='4'?1.0f:.5f):storylandVehicleDriveGrip(vehicle.nativeDefinition,vehicle.wheelContacts,
                vehicle.grounded,paths.vcs?vehicle.contactsValid:vehicle.suspensionAnchorValid));
            const float nativeDrive=storylandVehicleTransmissionDrive(vehicle.nativeDefinition,vehicle.transmission,
                vehicle.speed,nativeTraffic?vehicle.throttle:1.0f,dt,grip>0);
            trafficEngine=std::clamp(nativeDrive/vehicle.massMultiplier,0.0f,grip);
            if(!actor.water&&vehicle.grounded&&!vehicle.nativeDefinition.hasNativeSuspension)
                trafficEngine=std::max(0.0f,trafficEngine-runtime.gravity*std::sin(vehicle.roadPitch*radians));
            if(vehicle.nativeDefinition.maximumAiVelocity>0)target=std::min(target,vehicle.nativeDefinition.maximumAiVelocity*StorylandVehicleStepsPerSecond);
        }
        if(!nativeTyres) {
            if(nativeTraffic && target < vehicle.speed) {
                // An AI target-speed reduction requests the native brake rate.
                // The approach operation itself limits braking to the target;
                // scaling pedal pressure by relative speed error causes traffic
                // to coast into stopped cars instead of yielding.
                const float nativeBrake=storylandRetailVehicleBrakeDelta(vehicle.nativeDefinition,1.0f,dt);
                const float availableGrip=std::max(0.0f,vehicle.traction*9.81f)*dt;
                vehicle.speed=approach(vehicle.speed,target,std::min(nativeBrake,availableGrip));
            } else vehicle.speed=approach(vehicle.speed,target,trafficEngine*dt);
        }
        if(!nativeTyres&&nativeTraffic&&world&&routeDriven&&(vehicle.aiMission==8||vehicle.aiMission==12)&&
            vehicle.nativeDefinition.hasNativeSuspension) {
            vehicle.handbrake=false;
            if(!vehicle.contactsValid)updateVehicleWheelContacts(vehicle,actor,*world,0);
            const float rain=runtime.missionEnvironment?
                std::clamp(runtime.missionEnvironment->weatherState.rain,0.0f,1.0f):0.0f;
            applyVehicleTyreForces(vehicle,actor,0,0,rain,dt,false,false,
                runtime.missionEnvironment?runtime.missionEnvironment->controllerMode:0);
        }
        if(vehicle.nativeHandling&&!actor.water) {
            float& vertical=vehicleVerticalVelocity(vehicle);
            const float resistance=storylandRetailVehicleAirResistance(vehicle.nativeDefinition.dragMultiplier,
                std::hypot(vehicle.speed,vehicle.lateralSpeed,vertical),dt);
            vehicle.speed*=resistance;vehicle.lateralSpeed*=resistance;vertical*=resistance;
            const float angularResistance=storylandNativeVehicleAngularResistance(
                vehicle.nativeDefinition.dragMultiplier,dt);
            vehicle.yawVelocity*=angularResistance;
            vehicle.springPitchVelocity*=angularResistance;
            vehicle.springRollVelocity*=angularResistance;
            for(float& angularSpeed:vehicle.angularVelocity)angularSpeed*=angularResistance;
        }
        vehicle.speed=std::min(vehicle.speed,signalSpeedLimit);
        const bool physicalRoadFollow = !nativeTyres && !actor.water && !missionStop &&
            !holdPlayer && !temporaryForward && !directBlock &&
            actor.node < paths.nodes.size() && actor.nextNode < paths.nodes.size() &&
            actor.node != actor.nextNode;
        vehicle.longitudinalAcceleration=(vehicle.speed-previousSpeed)/dt;
        if(!nativeTyres && !physicalRoadFollow)
            vehicle.steeringAngle=approach(vehicle.steeringAngle,0,dt*80);
        if(!vehicle.nativeDefinition.hasNativeSuspension) {
        vehicle.suspensionVelocity+=(-vehicle.suspension*(25.0f*vehicle.handlingSuspensionForce)-
            vehicle.suspensionVelocity*(18.0f*vehicle.handlingSuspensionDamping))*dt;
        vehicle.suspension+=vehicle.suspensionVelocity*dt;
        }
        if(!vehicle.nativeDefinition.hasNativeSuspension)vehicle.bodyRoll=approach(vehicle.bodyRoll,0,dt*9);
        if(!vehicle.nativeDefinition.hasNativeSuspension)vehicle.bodyPitch=approach(vehicle.bodyPitch,0,dt*9);
        if(vehicle.speed<0.2f) vehicle.stoppedSeconds+=dt;
        else vehicle.stoppedSeconds=0;
        if(actor.blockedSeconds>0.0f)actor.blockedSeconds=std::max(0.0f,actor.blockedSeconds-dt);
        const auto previousActor=actor;
        StorylandNativePoint roadFollowTarget{};
        bool physicalCornerBraking = false;
        if (physicalRoadFollow) {
            const float lookAhead = std::clamp(3.5f + std::max(0.0f, vehicle.speed) * 0.55f,
                4.0f, 17.0f);
            if (!StorylandVehicleRoadFollow::lookAhead(paths, actor, lookAhead,
                world ? &world->zones : nullptr, vehicle.avoidLevelTransitions,
                vehicle.lanePolicySet && vehicle.fastLane, ignoreDisabledRoads, roadFollowTarget))
                roadFollowTarget = paths.nodes[actor.nextNode].position;
            const float wheelbase = storylandVehicleWheelbase(vehicle.nativeDefinition,
                vehicle.halfLength * 1.6f);
            const float grip = std::max(0.2f, vehicle.traction);
            const float turnLimit = StorylandVehicleRoadFollow::cornerSpeedLimit(target,
                actor.yaw, actor.position, roadFollowTarget, grip, wheelbase);
            if (vehicle.speed > turnLimit) {
                const float brake = std::max(0.5f, vehicle.handlingBrake) * dt;
                vehicle.speed = approach(vehicle.speed, turnLimit, brake);
                physicalCornerBraking = true;
            }
        }
        const bool physicalCoasting=vehicle.speed<0||nativeTyres;
        if(physicalCoasting) {
            const float headingCosine=std::cos(actor.yaw*radians),headingSine=std::sin(actor.yaw*radians);
            actor.position.x+=(headingCosine*vehicle.speed-headingSine*vehicle.lateralSpeed)*dt;
            actor.position.y+=(headingSine*vehicle.speed+headingCosine*vehicle.lateralSpeed)*dt;
            const float rotation=vehicle.yawVelocity*dt;
            const float c=std::cos(rotation*radians),s=std::sin(rotation*radians);
            const float speed=vehicle.speed,lateral=vehicle.lateralSpeed;
            vehicle.speed=c*speed+s*lateral;vehicle.lateralSpeed=c*lateral-s*speed;
            actor.yaw+=rotation;
        } else if(missionStop||holdPlayer||temporaryForward) {
            actor.position.x+=std::cos(actor.yaw*radians)*vehicle.speed*dt;
            actor.position.y+=std::sin(actor.yaw*radians)*vehicle.speed*dt;
        } else if (physicalRoadFollow) {
            StorylandVehicleRoadFollow::integrate(actor, vehicle.steeringAngle,
                vehicle.yawVelocity, vehicle.speed, vehicle.lateralSpeed,
                storylandVehicleWheelbase(vehicle.nativeDefinition, vehicle.halfLength * 1.6f),
                std::max(1.0f, vehicle.handlingSteerLock), dt, roadFollowTarget,
                vehicle.nativeDefinition.twoWheels());
            StorylandVehicleRoadFollow::updateProgress(paths, actor,
                world ? &world->zones : nullptr, vehicle.avoidLevelTransitions,
                vehicle.lanePolicySet && vehicle.fastLane, ignoreDisabledRoads);
        } else if (routeDriven && actor.destinationRouteFinished) {
            const float response=std::clamp(vehicle.handlingSteerLock*2.0f /
                std::max(0.5f,vehicle.turnMassMultiplier),12.0f,90.0f);
            storylandAdvanceVehicleTerminalApproach(actor,
                {scripted->targetX,scripted->targetY,scripted->targetZ},response,vehicle.speed,dt);
        } else if (directBlock) {
            const float dx = blockTarget.x - actor.position.x;
            const float dy = blockTarget.y - actor.position.y;
            const float desired = std::atan2(dy, dx) / radians;
            const float response = std::max(5.0f, vehicle.handlingSteerLock * 2 / vehicle.turnMassMultiplier);
            actor.yaw += std::clamp(std::remainder(desired - actor.yaw, 360.0f), -response * dt, response * dt);
            actor.position.x += std::cos(actor.yaw * radians) * vehicle.speed * dt;
            actor.position.y += std::sin(actor.yaw * radians) * vehicle.speed * dt;
        } else {
            if (blockingVehicle && actor.node < paths.nodes.size() && actor.nextNode < paths.nodes.size()) {
                float nearest = std::numeric_limits<float>::infinity();
                for (const auto& link : paths.nodes[actor.node].links) {
                    if (link.target >= paths.nodes.size() || !link.lanes ||
                        paths.nodes[link.target].water != actor.water || paths.nodes[link.target].disabled) continue;
                    if (vehicle.avoidLevelTransitions && world) {
                        const auto* first = world->zones.at(paths.nodes[actor.node].position, false);
                        const auto* second = world->zones.at(paths.nodes[link.target].position, false);
                        if (first && second && first->level != second->level) continue;
                    }
                    const float remaining = distance(paths.nodes[link.target].position, blockTarget);
                    if (remaining < nearest && actor.distance < .01f) {
                        nearest = remaining;
                        actor.nextNode = link.target;
                    }
                }
            }
            storylandAdvanceNativeTraffic(paths,actor,dt*vehicle.speed/8,world ? &world->zones : nullptr,
                vehicle.avoidLevelTransitions,vehicle.lanePolicySet && vehicle.fastLane,ignoreDisabledRoads);
        }
        if(!physicalCoasting && !physicalRoadFollow) {
            actor.position.x-=std::sin(actor.yaw*radians)*vehicle.lateralSpeed*dt;
            actor.position.y+=std::cos(actor.yaw*radians)*vehicle.lateralSpeed*dt;
        }
        const float turn=std::remainder(actor.yaw-previousActor.yaw,360.0f)*radians;
        vehicle.yawVelocity=turn/(dt*radians);
        const float steer=std::atan(turn*storylandVehicleWheelbase(vehicle.nativeDefinition,vehicle.halfLength*1.6f)/
            std::max(0.01f,std::fabs(vehicle.speed)*dt))/radians;
        if(!nativeTyres && !physicalRoadFollow)vehicle.steeringAngle=approach(vehicle.steeringAngle,
            std::clamp(steer,-vehicle.handlingSteerLock,vehicle.handlingSteerLock),dt*180);
        if(holdPlayer)vehicle.steeringAngle=0;
        vehicle.braking=physicalCornerBraking||(nativeTyres?nativeControls.brake>0.0f:
            missionStop||holdPlayer||vehicle.longitudinalAcceleration<-1||
                (nativeTraffic&&target<previousSpeed-0.01f));
        if (!vehicle.alarmMilliseconds || vehicle.alarmMilliseconds == UINT16_MAX)
            vehicle.headlightsOn=storylandVehicleLampsRequired(runtime,vehicle)&&vehicle.health>0;
        StorylandNativePoint collisionPoint;
        bool volumeBlocked=false;
        StorylandNativeContact worldContact;
        if(world) {
            float floor=0;
            if(!actor.water && !vehicle.nativeDefinition.hasNativeSuspension &&
               vehicleGroundHeight(*world,actor.position.x,actor.position.y,previousActor.position.z,floor) &&
               std::fabs(floor-previousActor.position.z)<0.85f)actor.position.z=floor;
            const auto move=nativeTyres?moveNativeVehicleBody(*world,previousActor,actor,vehicle):
                storylandMoveNativeBody(*world,previousActor.position,actor.position,
                    supportedVehicleBody(*world,vehicle,actor));
            const float wanted=distance(actor.position,previousActor.position);
            const float made=distance(move.position,previousActor.position);
            volumeBlocked=!move.clear||(!nativeTyres&&move.blocked&&made<wanted*0.15f);
            worldContact=move.contact;
            if(move.blocked&&!worldContact.valid)vehicle.speed=std::min(vehicle.speed,2.0f);
            if(move.clear)actor.position=move.position;
        }
        const bool worldBlocked=volumeBlocked||(world&&!nativeTyres&&
            vehicleWorldBlocked(*world,previousActor,actor,vehicle,&collisionPoint,&worldContact));
        bool blocked=worldBlocked,playerCollision=false;
        int32_t collidedVehicle=-1;
        StorylandNativeContact pairContact;
        StorylandNativeTrafficActor lastSafeActor=previousActor;
        const float planarTravel=std::hypot(actor.position.x-previousActor.position.x,
            actor.position.y-previousActor.position.y);
        const unsigned sweepSteps=std::clamp(unsigned(std::ceil(planarTravel/0.30f)),1u,100u);
        for(unsigned sweep=1;sweep<=sweepSteps;++sweep) {
            const float fraction=float(sweep)/sweepSteps;
            auto candidate=previousActor;
            candidate.position={
                previousActor.position.x+(actor.position.x-previousActor.position.x)*fraction,
                previousActor.position.y+(actor.position.y-previousActor.position.y)*fraction,
                previousActor.position.z+(actor.position.z-previousActor.position.z)*fraction};
            candidate.yaw=previousActor.yaw+
                std::remainder(actor.yaw-previousActor.yaw,360.0f)*fraction;
            bool pairBlocked=false;
            for(size_t other=0;other<cars.size()&&other<runtime.vehicles.size();++other) {
                if(other==index||!vehicle.collisionEnabled||!runtime.vehicles[other].collisionEnabled)continue;
                const auto& obstacle=cars[other];
                const auto& otherVehicle=runtime.vehicles[other];
                const float dx=candidate.position.x-obstacle.position.x;
                const float dy=candidate.position.y-obstacle.position.y;
                const float reach=vehicle.halfLength+vehicle.halfWidth+
                    otherVehicle.halfLength+otherVehicle.halfWidth+2.0f;
                if(dx*dx+dy*dy>reach*reach)continue;
                StorylandNativeContact sampledContact;
                if(!overlapVehicles(candidate,vehicle,obstacle,otherVehicle,&sampledContact))continue;
                pairBlocked=true;blocked=true;
                playerCollision=playerCollision||int(other)==runtime.occupiedVehicle;
                if(collidedVehicle<0) {
                    collidedVehicle=int32_t(other);
                    pairContact=sampledContact;
                }
                vehicle.lastCollidedSlot=int32_t(other);
                runtime.vehicles[other].lastCollidedSlot=int32_t(index);
                vehicle.pendingContactSlot=int32_t(other);
                runtime.vehicles[other].pendingContactSlot=int32_t(index);
            }
            if(pairBlocked)break;
            lastSafeActor=candidate;
        }
        if(vehicle.vcsTrafficRail.active&&(worldContact.valid||collidedVehicle>=0))
            vehicle.vcsTrafficRail.active=false;
        if(blocked||worldContact.valid) {
            float impact=std::fabs(vehicle.speed);
            if(collidedVehicle>=0&&!worldBlocked)
                impact=transferVehicleContact(runtime,previousActor,vehicle,cars[size_t(collidedVehicle)],
                    runtime.vehicles[size_t(collidedVehicle)],pairContact);
            else if(worldContact.valid) {
                const auto hit=applyVehicleWorldImpulse(vehicle,previousActor,worldContact);
                impact=hit.closingSpeed;
                storylandEjectVcsMissionBikeRiders(runtime,previousActor,vehicle,
                    vehicleBikeImpact(vehicle,previousActor,worldContact,
                        hit.impulse/std::max(1.0f,vehicle.mass),false));
            }
            const bool damagingImpact=impact>1.0f&&vehicle.impactCooldown<=0;
            if(blocked) {
                actor=worldBlocked?previousActor:lastSafeActor;
                if(collidedVehicle>=0)actor.blockedSeconds=std::max(actor.blockedSeconds,0.65f);
            }
            vehicleImpact(runtime,actor,vehicle,impact,worldContact.valid?&worldContact.point:
                worldBlocked?&collisionPoint:nullptr);
            const float damage=!damagingImpact?0.0f:paths.vcs?
                (vehicle.onlyDamagedByPlayer?0:storylandVehicleCollisionDamage(vehicle,impact*5,false)):
                (vehicle.canBeDamaged&&!(vehicle.damageProofs&8u)&&
                    storylandLcsVehicleDamageSourceAllowed(runtime,index,playerCollision)?impact*5:0);
            vehicle.health = std::max(0.0f, vehicle.health - damage);
            if (damage > 0) {
                vehicle.visiblyDamaged = true;
                if (vehicle.doorDamage[0] == 0) vehicle.doorDamage[0] = 1;
            }
            if(!worldContact.valid&&!pairContact.valid)vehicle.suspensionVelocity+=std::min(2.0f,impact*0.16f);
            if(worldBlocked&&!worldContact.valid) {vehicle.speed=0;vehicle.lateralSpeed=0;}
        }

        if(world&&!actor.water) {
            const auto supportedPosition=actor.position;
            actor.position.z=previousActor.position.z;
            applyVehicleGravity(vehicle,actor.position,*world,dt,runtime.gravity,actor.yaw);
            const auto settled=storylandMoveNativeBody(*world,supportedPosition,actor.position,supportedVehicleBody(*world,vehicle,actor));
            resolveVehicleBodyMovement(vehicle,actor,supportedPosition,settled);
            const auto support=supportedVehicleBody(*world,vehicle,actor);
            updateVehicleRoadReference(vehicle,support.pitch,-support.roll);
            if(actor.node<paths.nodes.size()&&actor.nextNode<paths.nodes.size()) {
                const auto& from=paths.nodes[actor.node].position;
                const auto& to=paths.nodes[actor.nextNode].position;
                const float length=distance(from,to);
                const float ratio=length>0.001f?std::clamp(actor.distance/length,0.0f,1.0f):0.0f;
                actor.groundOffset=actor.position.z-(from.z+(to.z-from.z)*ratio);
            }
        } else if(world&&actor.water) {
            actor.position.z=previousActor.position.z;
            applyVehicleWater(vehicle,actor.position,*world,dt,runtime.gravity,true,actor.yaw);
        }
        if (routeDriven) {
            scripted->x=actor.position.x;
            scripted->y=actor.position.y;
            scripted->z=actor.position.z;
            scripted->heading=actor.yaw-90.0f;
            scripted->speed=vehicle.speed;
            scripted->movedByScript=false;
            const bool reached=paths.vcs?
                std::hypot(actor.position.x-scripted->targetX,actor.position.y-scripted->targetY)<destinationRadius:
                storylandVehicleDestinationReached(actor.position,
                    {scripted->targetX,scripted->targetY,scripted->targetZ});
            if (reached) {
                scripted->drivingToTarget=false;
                if(paths.vcs) {
                    vehicle.aiMission=scripted->aiMission=0;
                    scripted->temporaryAction=0;
                }
                vehicle.speed=0;
                scripted->speed=0;
                vehicle.parked=true;
                vehicle.braking=true;
                if(vehicle.serviceAutopilot) {
                    vehicle.siren=false;
                    vehicle.serviceAutopilot->arrived=true;
                }
            }
        }
    }
    separateVehiclePairs(runtime,cars,world);
    if(world)for(size_t i=0;i<cars.size()&&i<runtime.vehicles.size();++i)
        if(runtime.vehicles[i].nativeDefinition.domain!=StorylandVehicleDomain::Air&&
            !scriptVehicleFrozen(runtime,runtime.vehicles[i])&&
            (((int(i)!=runtime.occupiedVehicle||(paths.vcs&&runtime.playerVehicleSeat>=0))&&
                int(i)!=storylandRemoteVehicleSlot(runtime))||runtime.vehicleAction!=StorylandVehicleAction::None))
            {
                auto& vehicle=runtime.vehicles[i];
                const bool retailTrafficSurface=storylandUpdateVcsTrafficSurface(vehicle,cars[i],*world,
                    runtime.missionEnvironment?runtime.missionEnvironment->nativeFrameCounter:runtime.fallenWorldFrame);
                const float physicsSeconds=retailTrafficSurface||vehicleAttachedToRope(runtime,vehicle)?0.0f:dt;
                const bool playerDriver=int(i)==runtime.occupiedVehicle&&runtime.playerVehicleSeat<0&&
                    int(i)!=storylandRemoteVehicleSlot(runtime);
                const bool driverInput=playerDriver&&runtime.vehicleAction!=StorylandVehicleAction::Exit;
                updateVehicleWheelContacts(vehicle,cars[i],*world,physicsSeconds,driverInput,
                    runtime.gravity,playerDriver);
                if(paths.vcs&&runtime.missionEnvironment&&(vehicle.bodyContact||
                    std::any_of(vehicle.wheelContacts.begin(),vehicle.wheelContacts.end(),
                        [](bool contact){return contact;})))
                    vehicle.lastNativeVehicleContactAt=runtime.missionEnvironment->gameMilliseconds;
            }
    if(runtime.vehicleAction==StorylandVehicleAction::Exit&&runtime.transitionVehicle>=0&&
        (runtime.playerVehicleSeat<0||runtime.vehicleEntryVariant==StorylandVehicleEntryVariant::MovingExit)&&
        size_t(runtime.transitionVehicle)<cars.size()&&
        size_t(runtime.transitionVehicle)<carPositions.size()) {
        const size_t slot=size_t(runtime.transitionVehicle);
        const auto& previous=carPositions[slot];
        const auto& current=cars[slot];
        const float angle=(current.yaw-previous.yaw)*radians;
        const float cosine=std::cos(angle),sine=std::sin(angle);
        const float x=runtime.vehicleActionDoor.x-previous.position.x;
        const float y=runtime.vehicleActionDoor.y-previous.position.y;
        runtime.vehicleActionDoor.x=current.position.x+cosine*x-sine*y;
        runtime.vehicleActionDoor.y=current.position.y+sine*x+cosine*y;
        runtime.vehicleActionDoor.z+=current.position.z-previous.position.z;
    }
    runtime.playerPosition=player;
    refreshMovementCars(runtime,cars);
    if(world)storylandUpdateVehiclePedestrianImpacts(runtime,*world,pedestrians,cars,carPositions,player,dt);
    for(size_t index=0;index<cars.size()&&index<runtime.vehicles.size();++index) {
        auto& vehicle=runtime.vehicles[index];
        storylandUpdateBloodyTyres(runtime,cars[index],vehicle,world,dt);
        if(paths.vcs&&!scriptVehicleFrozen(runtime,vehicle))
            storylandEjectVcsMissionBikeRiders(runtime,cars[index],vehicle,vehicleBikeSpin(vehicle,cars[index]));
        const bool eligible=runtime.occupiedVehicle<0&&runtime.vehicleAction==StorylandVehicleAction::None&&
            runtime.health>0&&runtime.playerControl&&vehicle.hasDriver&&vehicle.health>0;
        const bool contact=eligible&&index<carPositions.size()&&storylandSweptVehicleTouchesPlayer(
            storylandPlayerReactionVehicle(runtime,carPositions,index),
            storylandPlayerReactionVehicle(runtime,cars,index),player,player);
        if(contact&&!vehicle.playerBodyContact) {
            ++runtime.playerVehicleAnnoyanceSerial;
            if(!runtime.playerVehicleAnnoyanceSerial)++runtime.playerVehicleAnnoyanceSerial;
            storylandNotifyPlayerVehicleAnnoyance(runtime,runtime.playerReaction,cars,index,
                runtime.playerVehicleAnnoyanceSerial);
        }
        vehicle.playerBodyContact=contact;
    }
    refreshMovementCars(runtime,cars);
    if(world) {
        storylandAdvanceVcsMissionPedFalls(runtime,*world,dt);
        advanceMissionCharacterGravity(runtime,*world,dt);
    }
    if(paths.vcs)storylandSyncVcsServicePassengers(runtime,pedestrians,cars);
    runtime.movementPedestrians.clear();for(const auto& actor:pedestrians)runtime.movementPedestrians.push_back(actor.position);
    if (world && paths.vcs && !cinematic) {
        const auto placeServiceOccupant = [&](size_t truckSlot, size_t pedSlot, StorylandNativePoint& position,int seat) {
                const auto& truck = runtime.vehicles[truckSlot];
                const auto& actor = cars[truckSlot];
                if (int(truckSlot) == storylandRemoteVehicleSlot(runtime)) return false;
                const float angle = actor.yaw * radians;
                const float side=(seat>=0&&seat%2==0?-1.0f:1.0f)*(truck.halfWidth+0.65f);
                const float along=seat<0?0.0f:truck.nativeDefinition.seatPosition(unsigned(seat+1))[1];
                const StorylandNativePoint top{actor.position.x - std::sin(angle) * side+std::cos(angle)*along,
                    actor.position.y + std::cos(angle) * side+std::sin(angle)*along, actor.position.z + 1.0f};
                const StorylandNativePoint bottom{top.x, top.y, actor.position.z - 2.0f};
                uint8_t surface = 0;
                const float hit = storylandTraceNativeGround(*world, top, bottom, surface);
                if (!std::isfinite(hit) || hit < 0 || hit >= 1) return false;
                position = {top.x, top.y, top.z + (bottom.z - top.z) * hit};
                if (!storylandNativeBodyClear(*world, position, storylandNativeCharacterBody())) return false;
                const auto tooClose = [&](const StorylandNativePoint& other) {
                    return std::hypot(position.x - other.x, position.y - other.y, position.z - other.z) < 1.0f;
                };
                if (runtime.occupiedVehicle < 0 && tooClose(player)) return false;
                for (size_t i=0;i<pedestrians.size();++i)if(i!=pedSlot&&tooClose(pedestrians[i].position))return false;
                for (const auto& entry : runtime.missionEnvironment->missionCharacters) {
                    const auto& existing = entry.second;
                    if (!existing.removeFromWorld && tooClose({existing.x, existing.y, existing.z})) return false;
                }
                const auto dynamic = storylandMovePlayTestCharacter(*world, runtime, position, position,
                    int(pedSlot), true, false, true);
                if (!dynamic.clear || distance(position, dynamic.position) > 0.001f) return false;
                return true;
            };
        const auto placeServiceDriver=[&](size_t vehicle,size_t pedestrian,StorylandNativePoint& position) {
            return placeServiceOccupant(vehicle,pedestrian,position,-1);
        };
        storylandHandoffVcsFiretruckDrivers(runtime, pedestrians, cars,placeServiceDriver);
        const size_t beforeMedicHandoff=pedestrians.size();
        storylandHandoffVcsAmbulanceDrivers(runtime,pedestrians,cars,
            [&](const StorylandNativePoint& position) {
                uint8_t surface=0;
                return storylandTraceNativeGround(*world,{position.x,position.y,position.z+.36f},
                    {position.x,position.y,position.z-1.64f},surface)<1.0f;
            },placeServiceDriver);
        for(size_t index=beforeMedicHandoff;index<pedestrians.size();++index) {
            StorylandPlayTestSoundEvent speech(StorylandPlayTestSound::Damage);
            speech.speaker=int(index);speech.position=pedestrians[index].position;speech.positioned=true;
            speech.nativeSpeech=0x8c;
            storylandStampCharacterSoundEvent(runtime,speech);
            runtime.soundEvents.push_back(speech);
        }
        storylandHandoffVcsViceDrivers(runtime,pedestrians,cars,placeServiceDriver);
        std::vector<uint64_t> seatedCrew;
        seatedCrew.reserve(pedestrians.size());
        for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size();++index)
            seatedCrew.push_back(runtime.pedestrians[index].serviceVehicleIdentity);
        storylandReleaseVcsServicePassengers(runtime,pedestrians,cars,
            [&](size_t vehicle,size_t pedestrian,StorylandNativePoint& position) {
                return placeServiceOccupant(vehicle,pedestrian,position,int(runtime.pedestrians[pedestrian].servicePassengerSeat));
            });
        for(size_t index=0;index<seatedCrew.size()&&index<pedestrians.size();++index) {
            const auto& ped=runtime.pedestrians[index];
            if(!seatedCrew[index]||ped.serviceVehicleIdentity||ped.health<=0||
                ped.serviceRole!=StorylandServiceRole::Paramedic)continue;
            StorylandPlayTestSoundEvent speech(StorylandPlayTestSound::Damage);
            speech.speaker=int(index);speech.position=pedestrians[index].position;speech.positioned=true;
            speech.nativeSpeech=0x8c;storylandStampCharacterSoundEvent(runtime,speech);
            runtime.soundEvents.push_back(speech);
        }
    }
    runtime.movementPedestrians.clear();
    for (const auto& actor : pedestrians) runtime.movementPedestrians.push_back(actor.position);

    storylandUpdateAmbientPatrols(runtime,world,pedestrians);
    auto contactObstacles=storylandPedContactObstacles(runtime);
    for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size();++index) {
        auto& actor=pedestrians[index];auto& ped=runtime.pedestrians[index];
        if(ped.serviceVehicleIdentity)continue;
        if (ped.naturalPedType < 0) ped.naturalPedType = ped.stats.pedType;
        storylandUpdateZombiePedestrian(runtime,ped);
        if(ped.scriptHandle>=0) {
            ped.emergencyScriptOwned=true;
            storylandClearVcsMedicTask(ped);
            storylandClearVcsFirefighterTask(ped);
        }
        if (ped.scriptHandle >= 0 && runtime.missionEnvironment &&
            runtime.missionEnvironment->missionCharacters.count(ped.scriptHandle)) continue;
        if (ped.scriptHandle >= 0) ped.scriptHandle = -1;
        storylandAdvancePopulationPedestrian(ped,runtime.missionEnvironment&&
            runtime.missionEnvironment->killFrenzyStatus==1);
        if(ped.renderAlpha==0) {
            storylandClearVcsFirefighterTask(ped);
            continue;
        }
        float waterHeight=0;
        if(world&&deepCharacterWater(*world,actor.position,waterHeight)) {
            if(ped.bulletReaction.knockdown)ped.bulletReaction={};
            storylandClearVcsFirefighterTask(ped);
            const auto water=storylandStepWaterCharacter(ped.water,actor.position.z,waterHeight,true,
                false,false,true,dt,runtime.crime.vcs,ped.combatSpeed,
                ped.naturalPedType>=0?ped.naturalPedType:ped.stats.pedType);
            const auto previous=actor.position;
            auto next=previous;next.z=water.height;
            const auto movement=storylandMovePlayTestCharacter(*world,runtime,previous,next,int(index),true);
            if(movement.clear)actor.position=movement.position;
            if(movement.blocked)ped.water.verticalSpeed=0;
            const auto damage=storylandApplyNativePedDamage(ped.health,ped.armour,water.damage,false,
                ped.stats.valid?ped.stats.defendWeakness:1.0f,true);
            if(damage.accepted) {
                if(damage.fatal) {
                    ped.deathTime=0;
                    ped.death=storylandNativeDeathAssociation(runtime.crime.vcs,0,
                        storylandNativeDrowningWeapon(runtime.crime.vcs));
                    StorylandPlayTestSoundEvent death(StorylandPlayTestSound::Damage);
                    death.speaker=int(index);death.position=actor.position;death.positioned=true;
                    death.weapon=storylandNativeDrowningWeapon(runtime.crime.vcs);death.fatal=true;
                    if(runtime.crime.vcs)death.nativeSpeech=int(StorylandSpeechEvent::Death);
                    storylandStampCharacterSoundEvent(runtime,death);
                    runtime.soundEvents.push_back(death);
                }
                ped.health=damage.health;ped.armour=damage.armour;
            }
            ped.state=ped.health<=0?StorylandPedState::Dead:StorylandPedState::Idle;
            ped.combatSpeed=0;ped.melee={};ped.hostile=false;ped.clearFlee();ped.clearGunThreat();
            if(ped.health<=0)ped.deathTime+=dt;
            if(water.entered)runtime.soundEvents.emplace_back(StorylandPlayTestSound::WaterSplash);
            storylandUpdateSwimmingParticles(runtime,ped.water,actor.position,waterHeight,
                actor.yaw,ped.combatSpeed,ped.water.verticalSpeed,dt,water.entered,ped.health>0);
            runtime.movementPedestrians[index]=actor.position;
            continue;
        }
        storylandStepWaterCharacter(ped.water,actor.position.z,0,false,false,false,true,dt,runtime.crime.vcs);
        if(ped.health<=0) {
            if(world&&ped.bulletReaction.knockdown)
                storylandAdvanceBulletKnockdown(runtime,*world,ped.bulletReaction,
                    actor.position,actor.yaw,dt,int32_t(index));
            runtime.movementPedestrians[index]=actor.position;
            storylandClearVcsFirefighterTask(ped);
            ped.state=StorylandPedState::Dead;
            ped.deathTime+=dt;
            ped.combatSpeed=0;ped.clearFlee();ped.clearGunThreat();
            continue;
        }
        if(world) {
            const auto recovery=storylandMoveNativeBody(*world,actor.position,actor.position,storylandNativeCharacterBody());
            if(recovery.clear)actor.position=recovery.position;
            runtime.movementPedestrians[index]=actor.position;
        }
        if(world&&ped.bulletReaction.knockdown&&
            storylandAdvanceBulletKnockdown(runtime,*world,ped.bulletReaction,
                actor.position,actor.yaw,dt,int32_t(index))) {
            runtime.movementPedestrians[index]=actor.position;
            ped.melee={};ped.ranged={};ped.contact.clear();ped.combatSpeed=0;
            storylandClearVcsMedicTask(ped);storylandClearVcsFirefighterTask(ped);
            continue;
        }
        if(ped.bulletReaction.active(runtime.elapsed)) {
            ped.melee={};ped.ranged={};ped.contact.clear();ped.combatSpeed=0;
            storylandClearVcsMedicTask(ped);
            storylandClearVcsFirefighterTask(ped);
            continue;
        }
        if(storylandPlayerReactionHoldsTarget(runtime.playerReaction,StorylandReactionTargetKind::Ambient,
            int32_t(index),ped.stats.modelId)) {
            ped.melee={};ped.ranged={};ped.combatSpeed=0;
            storylandClearVcsMedicTask(ped);
            storylandClearVcsFirefighterTask(ped);
            continue;
        }
        const bool fleeTimerExpired=ped.fleeActive&&ped.timer<dt;
        ped.timer=std::max(0.0f,ped.timer-dt);
        const bool civilian=ped.scriptHandle<0&&!ped.fortBaxterSoldier&&!ped.policePursuing&&
            ped.serviceRole==StorylandServiceRole::Civilian&&ped.stats.valid&&
            (ped.stats.pedType==4||ped.stats.pedType==5||
             ped.stats.pedType==18||ped.stats.pedType==20);
        const bool aggressiveTarget=runtime.aggressivePedestrians&&civilian&&ped.health>0&&
            runtime.health>0&&!runtime.wastedActive&&
            (!runtime.missionEnvironment||!runtime.missionEnvironment->cutscene.playing)&&
            distance(actor.position,player)<28.0f&&
            std::fabs(actor.position.z-player.z)<3.0f;
        if(ped.playOptionHostile&&!aggressiveTarget) {
            ped.playOptionHostile=false;
            if(!ped.playerDamaged) {
                ped.hostile=false;ped.melee={};ped.ranged={};ped.combatSpeed=0.0f;
                ped.state=StorylandPedState::Walk;
            }
        } else if(aggressiveTarget&&!ped.playOptionHostile&&!ped.hostile) {
            ped.playOptionHostile=true;
            ped.hostile=true;ped.clearFlee();ped.clearGunThreat();
            ped.weaponCooldown=std::max(ped.weaponCooldown,.25f);
        }
        const bool zombieTarget=ped.zombie.active&&runtime.health>0&&!runtime.wastedActive&&
            (!runtime.missionEnvironment||!runtime.missionEnvironment->cutscene.playing)&&
            std::fabs(actor.position.z-player.z)<3.0f;
        if(ped.zombie.active) {
            ped.hostile=false;ped.playOptionHostile=false;ped.melee={};ped.ranged={};
            ped.clearFlee();ped.clearGunThreat();
        }
        bool danger=false;
        StorylandNativePoint dangerPosition;
        // Running is not an attack. Only an approaching vehicle on a collision
        // course causes an automatic flee response.
        if(!ped.zombie.active)for(size_t car=0;car<cars.size()&&car<runtime.vehicles.size();++car) {
            if(runtime.vehicles[car].craneCarried||!runtime.vehicles[car].collisionEnabled||
                !runtime.vehicles[car].visible)continue;
            const float signedSpeed=runtime.vehicles[car].speed;
            const float speed=std::fabs(signedSpeed);
            if(speed<4.0f) continue;
            if(std::fabs(actor.position.z-cars[car].position.z)>1.5f)continue;
            const float angle=cars[car].yaw*radians;
            const float dx=actor.position.x-cars[car].position.x;
            const float dy=actor.position.y-cars[car].position.y;
            const float along=(dx*std::cos(angle)+dy*std::sin(angle))*
                (signedSpeed<0?-1.0f:1.0f);
            const float lateral=std::fabs(-dx*std::sin(angle)+dy*std::cos(angle));
            if(along>0&&along<std::max(4.0f,speed*1.2f)&&lateral<2.1f) {
                danger=true;
                dangerPosition=cars[car].position;
                break;
            }
        }
        if(danger||cinematic) {
            storylandClearVcsMedicTask(ped);
            storylandClearVcsFirefighterTask(ped);
        }
        const bool medicWasTreating=ped.emergencyTreating;
        if(!danger&&!cinematic&&world&&paths.vcs&&storylandAdvanceVcsEmergencyMedic(runtime,pedestrians,index,dt,
            [&](const StorylandNativePoint& position) {
                uint8_t surface=0;
                return storylandTraceNativeGround(*world,
                    {position.x,position.y,position.z+.36f},
                    {position.x,position.y,position.z-1.64f},surface)<1.0f;
            },
            [&](const StorylandNativePoint& from,const StorylandNativePoint& desired) {
                const auto movement=storylandMoveAmbientPedestrian(runtime,*world,index,
                    from,desired,actor.yaw,dt,contactObstacles);
                if(movement.clear)actor.position=movement.position;
                runtime.movementPedestrians[index]=actor.position;
            })) {
            if(!medicWasTreating&&ped.emergencyTreating) {
                StorylandPlayTestSoundEvent speech(StorylandPlayTestSound::Damage);
                speech.speaker=int(index);speech.position=actor.position;speech.positioned=true;
                speech.nativeSpeech=0x8b;
                storylandStampCharacterSoundEvent(runtime,speech);
                runtime.soundEvents.push_back(speech);
            }
            continue;
        }
        if(!danger&&!cinematic&&world&&paths.vcs&&storylandAdvanceVcsFirefighter(
            runtime,pedestrians,cars,index,dt,
            [&](const StorylandNativePoint& from,const StorylandNativePoint& desired) {
                const auto movement=storylandMoveAmbientPedestrian(runtime,*world,index,
                    from,desired,actor.yaw,dt,contactObstacles);
                if(movement.clear)actor.position=movement.position;
                runtime.movementPedestrians[index]=actor.position;
            },
            [&](const StorylandNativePoint& from,const StorylandNativePoint& target) {
                return storylandTraceNativeSegment(*world,{from.x,from.y,from.z+1.0f},
                    {target.x,target.y,target.z+.35f})>=.99f;
            },
            [&](int32_t fireHandle,int32_t vehicleSlot) {
                auto& environment=*runtime.missionEnvironment;
                // The producer for a retail firefighter's spray is not yet mapped.
                // Use the native water-hit strength update as a bounded approximation.
                if(vehicleSlot>=0&&size_t(vehicleSlot)<runtime.vehicles.size()) {
                    auto& vehicle=runtime.vehicles[size_t(vehicleSlot)];
                    StorylandMissionEnvironment::ScriptFire waterTarget;
                    const auto& position=cars[size_t(vehicleSlot)].position;
                    waterTarget.position={position.x,position.y,position.z+.4f};
                    waterTarget.strength=vehicle.emergencyFireStrength;
                    storylandApplyWaterToFire(environment,waterTarget);
                    vehicle.emergencyFireStrength=waterTarget.strength;
                    if(waterTarget.suppressionPending)
                        vehicleParticle(runtime,StorylandParticleKind::Steam,
                            {position.x,position.y,position.z+.4f},{0,0,.15f},.8f,1.0f);
                    if(vehicle.emergencyFireStrength<0) {
                        storylandExtinguishPlayTestVehicle(runtime,size_t(vehicleSlot));
                        vehicle.emergencyFireIdentity=0;vehicle.emergencyFireStrength=1;
                    }
                } else {
                    const auto fire=environment.scriptFires.find(fireHandle);
                    if(fire!=environment.scriptFires.end()&&fire->second.active&&!fire->second.scriptOwned) {
                        StorylandNativePoint target{};
                        if(storylandVcsFireTargetPosition(environment,fire->second,target)) {
                            fire->second.position={target.x,target.y,target.z};
                            storylandApplyWaterToFire(environment,fire->second);
                        }
                    }
                }
            }))continue;
        if(ped.fleeActive) {
            if(ped.fleeFromPlayer)ped.fleePosition=player;
            if(fleeTimerExpired) {
                const float range=std::hypot(actor.position.x-ped.fleePosition.x,
                    actor.position.y-ped.fleePosition.y);
                if(ped.fleeFromPlayer&&range<30.0f)ped.timer=5;
                else ped.clearFlee();
            }
        }
        if(ped.zombie.active) {
            ped.state=zombieTarget&&ped.coughTime<=0&&
                std::hypot(actor.position.x-player.x,actor.position.y-player.y)>StorylandBrawnZombie::arrivalRadius?
                StorylandPedState::Walk:StorylandPedState::Idle;
        } else if(danger)ped.startFlee(dangerPosition,2);
        else if(ped.coughTime>0||ped.threatWaiting)ped.state=StorylandPedState::Idle;
        else if(ped.fleeActive)ped.state=StorylandPedState::Flee;
        else if(ped.hostile) ped.state=StorylandPedState::Combat;
        else if(ped.timer==0)ped.state=StorylandPedState::Walk;
        float speed=ped.state==StorylandPedState::Flee?
            (ped.fleeWalking?runtime.pedestrianWalkSpeed:runtime.pedestrianRunSpeed):
            ped.state==StorylandPedState::Walk?runtime.pedestrianWalkSpeed:0;
        if(speed>0&&ped.stats.valid&&runtime.missionEnvironment&&
            runtime.missionEnvironment->animationMovementSpeed) {
            const int32_t movement=ped.state==StorylandPedState::Flee&&!ped.fleeWalking?1:0;
            const int32_t weapon=ped.weapon<runtime.weapons.size()?runtime.weapons[ped.weapon].nativeId:0;
            const int32_t group=storylandNativePedWeaponLocomotionGroup(paths.vcs,weapon,ped.stats.animationGroup);
            const float nativeSpeed=runtime.missionEnvironment->animationMovementSpeed(group,movement);
            if(std::isfinite(nativeSpeed)&&nativeSpeed>0)speed=nativeSpeed;
        }
        bool controlledCrossing=false;
        if(!ped.zombie.active&&actor.node<paths.nodes.size()&&actor.nextNode<paths.nodes.size()) {
            const auto& links=paths.nodes[actor.node].links;
            const auto crossing=std::find_if(links.begin(),links.end(),[&](const auto& l){return l.target==actor.nextNode&&l.crossesRoad;});
            controlledCrossing=crossing!=links.end()&&crossing->trafficLight;
            if(controlledCrossing&&!actor.crossingCommitted&&ped.state==StorylandPedState::Walk&&
                storylandRetailPedestrianLight(runtime.trafficClockMilliseconds,paths.vcs)!=StorylandTrafficLight::Green) {
                speed=0;ped.state=StorylandPedState::Idle;
            }
            if(crossing!=links.end())for(size_t car=0;car<cars.size()&&car<runtime.vehicles.size();++car) {
                if(runtime.vehicles[car].craneCarried||!runtime.vehicles[car].collisionEnabled||
                    !runtime.vehicles[car].visible)continue;
                if(std::fabs(actor.position.z-cars[car].position.z)>1.5f)continue;
                const float c=std::cos(cars[car].yaw*radians),s=std::sin(cars[car].yaw*radians);
                const auto& end=paths.nodes[actor.nextNode].position;
                const float x=(actor.position.x+end.x)*.5f-cars[car].position.x,
                    y=(actor.position.y+end.y)*.5f-cars[car].position.y;
                const float along=x*c+y*s,lateral=std::fabs(-x*s+y*c);
                if(along>-runtime.vehicles[car].halfLength&&along<runtime.vehicles[car].speed*2.0f+4&&
                    lateral<runtime.vehicles[car].halfWidth+1.5f) {speed=0;break;}
            }
        }
        const auto previous=actor;
        const bool fleeing=ped.state==StorylandPedState::Flee&&ped.fleeActive;
        if(ped.zombie.active&&zombieTarget&&speed>0) {
            const float dx=player.x-actor.position.x,dy=player.y-actor.position.y;
            const float length=std::hypot(dx,dy);
            const float advance=std::min(speed*dt,std::max(0.0f,length-StorylandBrawnZombie::arrivalRadius));
            if(length>0&&advance>0) {
                actor.position.x+=dx*advance/length;actor.position.y+=dy*advance/length;
                actor.yaw=std::atan2(dy,dx)/radians;
            }
        } else if(fleeing&&speed>0) {
            const auto target=pedestrianFleeDestination(ped,actor,paths,world,dt);
            const float dx=target.x-actor.position.x,dy=target.y-actor.position.y;
            const float length=std::hypot(dx,dy);
            if(length>.0001f) {
                const float advance=std::min(speed*dt,length);
                actor.position.x+=dx*advance/length;actor.position.y+=dy*advance/length;
                actor.yaw=std::atan2(dy,dx)/radians;
            }
        } else if(!ped.zombie.active) {
            size_t leader=SIZE_MAX;
            if(ped.patrol.group&&ped.patrol.role&&storylandPatrolFaction(ped)>=0)
                for(size_t other=0;other<pedestrians.size()&&other<runtime.pedestrians.size();++other)
                    if(runtime.pedestrians[other].patrol.group==ped.patrol.group&&
                        runtime.pedestrians[other].patrol.role==0&&storylandPatrolFaction(runtime.pedestrians[other])==storylandPatrolFaction(ped)) {
                        leader=other;break;
                    }
            if(leader!=SIZE_MAX) {
                const auto& lead=pedestrians[leader];
                const auto target=storylandVcsFormationTarget(ped.patrol.role,
                    {lead.position.x,lead.position.y,lead.position.z},lead.yaw-90,ped.patrol.spacingRandom);
                float runSpeed=runtime.pedestrianRunSpeed;
                if(runtime.missionEnvironment&&runtime.missionEnvironment->animationMovementSpeed) {
                    const int32_t weapon=ped.weapon<runtime.weapons.size()?runtime.weapons[ped.weapon].nativeId:0;
                    const int32_t group=storylandNativePedWeaponLocomotionGroup(paths.vcs,weapon,ped.stats.animationGroup);
                    const float nativeRun=runtime.missionEnvironment->animationMovementSpeed(group,1);
                    if(std::isfinite(nativeRun)&&nativeRun>0)runSpeed=nativeRun;
                }
                const auto next=storylandStepPatrolFollower({actor.position.x,actor.position.y,actor.position.z},
                    actor.yaw,target,speed,speed>0?runSpeed:0,dt);
                actor.position={next.position[0],next.position[1],next.position[2]};
                actor.yaw=next.heading;
                const float requestedDistance=std::hypot(actor.position.x-previous.position.x,
                    actor.position.y-previous.position.y);
                // Arrival and turning in place are not failed movement attempts.
                speed=dt>0?requestedDistance/dt:0;
                ped.patrol.running=next.running&&requestedDistance>.00001f;
                if(requestedDistance<.00001f)ped.state=StorylandPedState::Idle;
                // Keep navigation references current without moving the body to a node.
                actor.node=lead.node;actor.nextNode=lead.nextNode;actor.distance=lead.distance;
            } else storylandAdvanceNativeTraffic(paths,actor,dt*speed/1.25f);
        }
        if(paths.vcs&&ped.fortBaxterSoldier&&world) {
            const StorylandNativeZone* base=nullptr;
            for(const auto& zone:world->zones.information)
                if(zone.name=="ARMYBAS") {base=&zone;break;}
            if(!base)base=world->zones.named("ARMYB");
            if(base&&!base->contains(actor.position)) {
                // Turn back on the same retail path; never let a generated
                // base patrol wander onto the streets outside the fence.
                actor=previous;
                if(previous.node<paths.nodes.size()&&previous.nextNode<paths.nodes.size()) {
                    const auto& backlinks=paths.nodes[previous.nextNode].links;
                    if(std::any_of(backlinks.begin(),backlinks.end(),[&](const auto& link) {
                        return link.target==previous.node;
                    })) {
                        const float edge=distance(paths.nodes[previous.node].position,
                            paths.nodes[previous.nextNode].position);
                        std::swap(actor.node,actor.nextNode);
                        actor.distance=std::max(0.0f,edge-previous.distance);
                        actor.yaw=std::remainder(actor.yaw+180.0f,360.0f);
                    }
                }
            }
        }
        if(world) {
            auto candidate=actor.position;
            uint8_t surface=0;
            const StorylandNativePoint top{candidate.x,candidate.y,previous.position.z+0.36f},bottom{candidate.x,candidate.y,previous.position.z-0.75f};
            const float hit=storylandTraceNativeGround(*world,top,bottom,surface);
            bool supported=true;
            if(hit<1)candidate.z=top.z+(bottom.z-top.z)*hit;
            else if(!world->collisionModels.empty()) {
                const StorylandNativePoint oldTop{previous.position.x,previous.position.y,previous.position.z+0.36f},
                    oldBottom{previous.position.x,previous.position.y,previous.position.z-0.75f};
                supported=storylandTraceNativeGround(*world,oldTop,oldBottom,surface)>=1;
                if(!supported)candidate=previous.position;
            }
            auto move=storylandMoveAmbientPedestrian(runtime,*world,index,
                previous.position,candidate,previous.yaw,dt,contactObstacles);
            if(speed>0&&(move.blocked||!supported)) {
                const float angle=std::atan2(candidate.y-previous.position.y,candidate.x-previous.position.x);
                for(float turn:{0.7f,-0.7f,1.4f,-1.4f}) {
                    StorylandNativePoint side{previous.position.x+std::cos(angle+turn)*speed*dt,
                        previous.position.y+std::sin(angle+turn)*speed*dt,previous.position.z};
                    const StorylandNativePoint sideTop{side.x,side.y,side.z+0.36f},sideBottom{side.x,side.y,side.z-0.75f};
                    const float sideFloor=storylandTraceNativeGround(*world,sideTop,sideBottom,surface);
                    if(sideFloor>=1&&!world->collisionModels.empty())continue;
                    if(sideFloor<1)side.z=sideTop.z+(sideBottom.z-sideTop.z)*sideFloor;
                    const auto detour=storylandMoveAmbientPedestrian(runtime,*world,index,
                        previous.position,side,previous.yaw,dt,contactObstacles);
                    if(detour.clear&&!detour.blocked) {move=detour;break;}
                }
            }
            const float progress=std::hypot(move.position.x-previous.position.x,move.position.y-previous.position.y);
            if(ped.patrol.group&&ped.patrol.role) {
                if(speed>0&&(!move.clear||progress<speed*dt*.2f))ped.patrol.blockedSeconds+=dt;
                else ped.patrol.blockedSeconds=0;
                if(ped.patrol.blockedSeconds>3.0f)ped.patrol.clear();
            }
            if(ped.state!=StorylandPedState::Combat)
                storylandRecordPedContactFrame(ped.contact,speed*dt,progress,dt,
                    runtime.missionEnvironment?runtime.missionEnvironment->nativeFrameCounter:runtime.fallenWorldFrame);
            if(!move.clear || (speed>0&&!ped.contact.active()&&progress<speed*dt*0.2f)) {
                const float facing=actor.yaw;
                actor=previous;actor.blockedSeconds+=dt;
                if(ped.patrol.group&&ped.patrol.role)actor.yaw=facing;
                if(ped.state==StorylandPedState::Walk&&!ped.contact.obstructed&&!ped.contact.active()) {
                    ped.state=StorylandPedState::Idle;ped.timer=.2f;
                }
                // Backtrack the same pedestrian edge without jumping to a node.
                if(fleeing&&actor.blockedSeconds>1.0f) {
                    ped.fleeNextNode=UINT32_MAX;ped.fleeRouteTime=0;
                } else if(actor.blockedSeconds>1.0f&&!ped.contact.active()&&actor.node<paths.nodes.size()&&actor.nextNode<paths.nodes.size()) {
                    const auto& links=paths.nodes[actor.nextNode].links;
                    if(std::any_of(links.begin(),links.end(),[&](const auto& l){return l.target==actor.node;})) {
                        const float edge=distance(paths.nodes[actor.node].position,paths.nodes[actor.nextNode].position);
                        std::swap(actor.node,actor.nextNode);actor.distance=std::max(0.0f,edge-actor.distance);
                        actor.previousNode=UINT32_MAX;actor.blockedSeconds=0;
                    }
                }
            } else {
                actor.position=move.position;actor.blockedSeconds=0;
                if(fleeing&&ped.fleeLastNode<paths.nodes.size()&&ped.fleeNextNode<paths.nodes.size()) {
                    actor.node=ped.fleeLastNode;actor.nextNode=ped.fleeNextNode;
                    actor.previousNode=UINT32_MAX;actor.crossingCommitted=false;
                    const auto& from=paths.nodes[actor.node].position;
                    const auto& to=paths.nodes[actor.nextNode].position;
                    const float length=distance(from,to);
                    actor.distance=length>.0001f?std::clamp(((actor.position.x-from.x)*(to.x-from.x)+
                        (actor.position.y-from.y)*(to.y-from.y)+(actor.position.z-from.z)*(to.z-from.z))/length,0.0f,length):0;
                }
                if(controlledCrossing&&actor.node==previous.node&&progress>.001f)actor.crossingCommitted=true;
                if(actor.node<paths.nodes.size()&&actor.nextNode<paths.nodes.size()) {
                    const auto& a=paths.nodes[actor.node].position;const auto& b=paths.nodes[actor.nextNode].position;
                    const float edge=distance(a,b),t=edge>0?std::clamp(actor.distance/edge,0.0f,1.0f):0;
                    actor.groundOffset=actor.position.z-(a.z+(b.z-a.z)*t);
                }
            }
            runtime.movementPedestrians[index]=actor.position;
            storylandPublishPedContact(contactObstacles,uint64_t(index)+1,actor.position,ped.contact);
        }
    }
    if(world)storylandResolvePedestrianContacts(runtime,*world,pedestrians,dt,contactObstacles);
    for(size_t index=0;index<cars.size()&&index<runtime.vehicles.size();++index) {
        const auto& position=cars[index].position;
        runtime.vehicles[index].scriptTravel.finish({position.x,position.y,position.z});
    }
    storylandRefreshPlayTestVehicles(runtime, paths, cars, player, runtime.vehicles.size(),world,&pedestrians);
    refreshMovementCars(runtime,cars);
}
static bool storylandUpdateEntryExitTransition(StorylandPlayTestRuntime& runtime,StorylandNativePoint& player,
    float& heading,float dt) {
    runtime.entryCooldown=std::max(0.0f,runtime.entryCooldown-dt);
    if(runtime.entryTransition>=0) {
        if(size_t(runtime.entryTransition)>=runtime.entryExits.size()) {
            runtime.entryTransition=-1;runtime.entryFade=0;return false;
        }
        runtime.entryTime+=dt;
        const auto& entry=runtime.entryExits[size_t(runtime.entryTransition)];
        if(runtime.entryTime<0.3f)runtime.entryFade=runtime.entryTime/0.3f;
        else {
            if(!runtime.entryWarped) {
                StorylandEntryExitWarp warp;
                if(!storylandResolveEntryExitWarp(entry,runtime.entryReverse,warp)) {
                    runtime.entryError="ENEX contains invalid destination coordinates or an area ID.";
                    runtime.entryErrorUntil=runtime.elapsed+5.0f;
                    runtime.entryTransition=-1;runtime.entryFade=0;runtime.entryCooldown=.5f;
                    return false;
                }
                const auto& destination=warp.position;
                const int area=warp.area;
                const bool needsStreaming = area!=runtime.visibleArea ||
                    std::hypot(destination.x-player.x,destination.y-player.y,
                        destination.z-player.z)>12.0f;
                if(needsStreaming) {
                    auto* environment=runtime.missionEnvironment;
                    if(!environment||!environment->loadScene) {
                        runtime.entryError="Interior scene streaming is unavailable.";
                        runtime.entryErrorUntil=runtime.elapsed+5.0f;
                        runtime.entryTransition=-1;runtime.entryFade=0;runtime.entryCooldown=.5f;
                        return false;
                    }
                    std::string error;
                    const int previousArea=environment->area;
                    environment->area=area;
                    if(!environment->loadScene({destination.x,destination.y,destination.z},error)) {
                        environment->area=previousArea;
                        runtime.entryError=error.empty()?"Interior scene could not be loaded.":error;
                        runtime.entryErrorUntil=runtime.elapsed+5.0f;
                        runtime.entryTransition=-1;runtime.entryFade=0;runtime.entryCooldown=.5f;
                        return false;
                    }
                }
                player=destination;
                heading=warp.heading;
                runtime.visibleArea=area;
                runtime.verticalSpeed=0;runtime.movementSpeed=0;runtime.grounded=true;
                runtime.entryWarped=true;runtime.entryNeedsLeave=true;
                runtime.entryCooldown=1;runtime.entryCameraResetPending=true;
                runtime.entryError.clear();
                if(runtime.missionEnvironment) {
                    auto& environment=*runtime.missionEnvironment;
                    environment.x=player.x;environment.y=player.y;environment.z=player.z;
                    environment.heading=heading-90.0f;environment.area=area;
                    environment.playerWalkTask.clear();
                }
            }
            runtime.entryFade=std::clamp(1.0f-(runtime.entryTime-0.3f)/0.3f,0.0f,1.0f);
        }
        if(runtime.entryTime>=0.6f) {runtime.entryTransition=-1;runtime.entryFade=0;}
        return true;
    }
    if(runtime.entryNeedsLeave) {
        bool withinMarker=false;
        for(const auto& entry:runtime.entryExits) {
            const bool forward=runtime.visibleArea==entry.sourceArea&&
                storylandEntryExitContains(entry,player);
            const bool reverse=runtime.visibleArea==entry.destinationArea&&
                storylandEntryExitContains(entry,player,true);
            if(forward||reverse) {withinMarker=true;break;}
        }
        if(!withinMarker)runtime.entryNeedsLeave=false;
        return false;
    }
    if(!runtime.playerControl||runtime.entryCooldown>0||runtime.occupiedVehicle>=0||
        runtime.vehicleAction!=StorylandPlayTestRuntime::VehicleAction::None)return false;
    for(size_t index=0;index<runtime.entryExits.size();++index) {
        const auto& entry=runtime.entryExits[index];
        const bool forward=runtime.visibleArea==entry.sourceArea&&
            storylandEntryExitOpen(entry,runtime.clockHour)&&storylandEntryExitContains(entry,player);
        const bool reverse=runtime.visibleArea==entry.destinationArea&&
            storylandEntryExitOpen(entry,runtime.clockHour,true)&&storylandEntryExitContains(entry,player,true);
        if(!forward&&!reverse)continue;
        runtime.entryTransition=int(index);runtime.entryReverse=!forward;
        runtime.entryTime=0;runtime.entryFade=0;runtime.entryWarped=false;
        return true;
    }
    return false;
}

StorylandPlayTestInput storylandDelayedMovementInput(StorylandPlayTestRuntime& runtime,
    const StorylandPlayTestInput& input,uint8_t delay) {
    auto& samples=runtime.drunkMovementHistory;
    for(size_t index=samples.size()-1;index>0;--index)samples[index]=samples[index-1];
    samples[0]={input.horizontal(),input.forwardAmount(),input.analogMovement};
    auto result=input;
    const auto& selected=samples[std::min(size_t(delay),samples.size()-1)];
    result.left=selected.horizontal<0;result.right=selected.horizontal>0;
    result.analogHorizontal=selected.horizontal;
    // CPad delays movement/steering, not accelerator, brake, fire or buttons.
    if(runtime.occupiedVehicle<0) {
        result.forward=selected.forward>0;result.backward=selected.forward<0;
        result.analogMovement=selected.analogMovement;
        result.analogForward=selected.forward;
    } else {
        result.analogMovement=selected.analogMovement||input.analogMovement;
        result.analogForward=input.forwardAmount();
    }
    return result;
}

bool storylandConsumeRestartOverride(StorylandMissionEnvironment& world,StorylandNativePoint& position,float& heading) {
    if(!world.nextRestartOverrideActive)return false;
    const auto& restart=world.nextRestartOverride;
    position={restart.x,restart.y,restart.z};heading=restart.heading+90.0f;
    world.nextRestartOverrideActive=false;return true;
}

bool storylandSniperBulletInArea(const StorylandPlayTestRuntime& runtime,const std::array<float,6>& bounds) {
    if(!runtime.sniperTraceActive)return false;
    const std::array<float,3> from{runtime.sniperTraceFrom.x,runtime.sniperTraceFrom.y,runtime.sniperTraceFrom.z};
    const std::array<float,3> to{runtime.sniperTraceTo.x,runtime.sniperTraceTo.y,runtime.sniperTraceTo.z};
    float lower=0,upper=1;
    // Preserve retail3C20F8's sorted-axis fractions (including its unusual
    // upper fraction and the Y-axis bypass), rather than replacing the test.
    for(size_t axis=0;axis<3;++axis) {
        const float minimum=std::min(from[axis],to[axis]),maximum=std::max(from[axis],to[axis]);
        if(!(minimum<bounds[axis+3]||bounds[axis]<maximum)) {
            if(axis==1)continue;
            return false;
        }
        const float extent=maximum-minimum;
        if(minimum<bounds[axis])lower=std::max(lower,extent>0?(bounds[axis]-minimum)/extent:FLT_MAX);
        if(bounds[axis+3]<maximum)upper=std::min(upper,extent>0?(maximum-bounds[axis+3])/extent:FLT_MAX);
    }
    return lower<=upper;
}

static bool updateScriptedPlayerVehicleEntry(StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& cars,const StorylandNativePoint& player) {
    auto* environment=runtime.missionEnvironment;
    if(!environment||environment->playerVehicleEntryTarget<0)return false;
    const int32_t handle=environment->playerVehicleEntryTarget;
    const auto found=environment->missionVehicles.find(handle);
    const auto cancel=[&]() {
        environment->playerVehicleEntryTarget=-1;
        environment->playerVehicleEntryAsPassenger=false;
        environment->playerWalkTask.clear();
    };
    if(found==environment->missionVehicles.end()||found->second.deleted||!found->second.alive||
        found->second.wrecked||runtime.busted||runtime.wastedActive) {cancel();return false;}
    const auto& scripted=found->second;
    const int32_t slot=scripted.spawnedSlot;
    if(slot<0||size_t(slot)>=cars.size()||size_t(slot)>=runtime.vehicles.size()||
        runtime.vehicles[size_t(slot)].scriptHandle!=handle) {
        environment->requestedModels.insert(scripted.model);
        return true;
    }
    const bool passenger=environment->playerVehicleEntryAsPassenger;
    if(runtime.occupiedVehicle==slot&&((runtime.playerVehicleSeat>=0)==passenger)) {
        cancel();return false;
    }
    if(runtime.occupiedVehicle>=0) {
        environment->playerWalkTask.clear();
        environment->playerExitVehicleRequested=true;
        return true;
    }
    auto& vehicle=runtime.vehicles[size_t(slot)];
    const auto& actor=cars[size_t(slot)];
    int32_t seat=-1;
    if(passenger) {
        const int32_t seats=vehicle.nativeDefinition.twoWheels()?1:
            vehicle.nativeDefinition.numberOfDoors>2?3:1;
        for(int32_t candidate=environment->playerAllowFrontPassenger?0:1;candidate<seats;++candidate) {
            if(!vehicle.nativeDefinition.vcsModelLayout&&
                environment->lcsExtendedWorld.frontSeatOnlyCharacters.count(1)&&candidate>0)continue;
            const bool occupied=storylandVcsServiceSeatOccupied(runtime,vehicle,candidate)||
                std::any_of(environment->missionCharacters.begin(),environment->missionCharacters.end(),
                [&](const auto& entry){return entry.second.alive&&entry.second.vehicle==handle&&
                    entry.second.passengerSeat==candidate;});
            if(!occupied) {seat=candidate;break;}
        }
        if(seat<0) {environment->playerWalkTask.clear();return true;}
    }
    if(runtime.vehicleAction!=StorylandVehicleAction::None) {
        environment->playerWalkTask.clear();
        if(passenger&&runtime.transitionVehicle==slot&&runtime.playerVehicleSeat<0)
            storylandBeginPlayTestVehicleEntry(runtime,cars,player,slot,seat);
        return true;
    }
    if(distance(player,actor.position)<=4.0f) {
        environment->playerWalkTask.clear();
        storylandBeginPlayTestVehicleEntry(runtime,cars,player,slot,seat);
        return true;
    }
    if(environment->cutscene.playing||environment->playerWarped||environment->playerPositionFrozen||
        environment->activeVcsCharacterAttachment(0)||runtime.playerWater.active)return true;
    const float c=std::cos(actor.yaw*radians),s=std::sin(actor.yaw*radians);
    const float dx=player.x-actor.position.x,dy=player.y-actor.position.y;
    const float along=dx*c+dy*s,left=-dx*s+dy*c;
    const float side=passenger?(seat%2==0?-1.0f:1.0f):(left<0?-1.0f:1.0f);
    const unsigned nativeSeat=seat<0?0u:unsigned(seat+1);
    float goalAlong=vehicle.nativeDefinition.seatPosition(nativeSeat)[1]-.45f;
    float goalLeft=side*(vehicle.halfWidth+.7f);
    if(left*side<0) {
        const float end=along<0?-1.0f:1.0f;
        goalAlong=end*(vehicle.halfLength+1.3f);
        goalLeft=(std::fabs(along)<vehicle.halfLength+.7f?(left<0?-1.0f:1.0f):side)*(vehicle.halfWidth+1.3f);
    } else if(std::fabs(along)>vehicle.halfLength+.7f&&std::fabs(left)<vehicle.halfWidth+.7f) {
        goalAlong=(along<0?-1.0f:1.0f)*(vehicle.halfLength+1.3f);
        goalLeft=side*(vehicle.halfWidth+1.3f);
    }
    auto& walk=environment->playerWalkTask;
    walk.active=true;walk.completed=false;walk.radius=.5f;
    walk.moveMode=vehicle.nativeDefinition.vcsModelLayout&&environment->playerScriptRunning?
        StorylandScriptPlayerMoveMode::Run:StorylandScriptPlayerMoveMode::Walk;
    walk.targetX=actor.position.x+c*goalAlong-s*goalLeft;
    walk.targetY=actor.position.y+s*goalAlong+c*goalLeft;
    walk.targetZ=actor.position.z;
    return true;
}

static bool storylandPlayerNavigationNodeUsable(const StorylandMissionEnvironment& environment,
    const StorylandNativePaths& paths, uint32_t index) {
    if (index >= paths.nodes.size()) return false;
    const auto& node = paths.nodes[index];
    if (node.vehicle || node.water) return false;
    bool enabled = !node.disabled;
    for (const auto& change : environment.roadChanges) {
        if (!change.vehicle && node.position.x >= change.bounds[0] && node.position.x <= change.bounds[3] &&
            node.position.y >= change.bounds[1] && node.position.y <= change.bounds[4] &&
            node.position.z >= change.bounds[2] && node.position.z <= change.bounds[5]) {
            enabled = change.enabled;
        }
    }
    return enabled;
}

static void storylandWaitForPlayerNavigation(StorylandMissionEnvironment& environment) {
    auto& navigation = environment.playerNavigation;
    navigation.clearRoute();
    navigation.seekWaitUntil = environment.gameMilliseconds + 500;
    if (!navigation.alternateObstructionHandling) return;
    environment.playerWaitState = 1;
    environment.playerWaitDeadline = navigation.seekWaitUntil;
    environment.playerWaitVersionB = false;
    environment.pedWaitStateMilliseconds[0] = 500;
}

static bool storylandResolvePlayerWalkSteering(StorylandMissionEnvironment& environment,
    const StorylandNativeWorld& world, const StorylandNativePoint& player,
    StorylandNativePoint& steering) {
    auto& navigation = environment.playerNavigation;
    const auto& task = environment.playerWalkTask;
    steering = {task.targetX, task.targetY, task.targetZ};
    if (!world.paths.vcs || !environment.playerCollisionEnabled) return true;
    const bool skipNodeSearch=navigation.seekNodes&&storylandVcsWithinSeekRadius(
        environment.playerVcsSeekNodeRadius,steering.x-player.x,steering.y-player.y);
    if (!skipNodeSearch && navigation.seekWaitUntil &&
        int32_t(environment.gameMilliseconds - navigation.seekWaitUntil) < 0) return false;
    if(!skipNodeSearch)navigation.seekWaitUntil = 0;
    const auto clear = [&](const StorylandNativePoint& from, const StorylandNativePoint& to) {
        return storylandTraceNativeSegment(world, {from.x, from.y, from.z + 1.0f},
            {to.x, to.y, to.z + 1.0f}) >= 1.0f;
    };
    const auto blocked = [&]() {
        storylandWaitForPlayerNavigation(environment);
        return false;
    };
    if (navigation.seekNodes && !skipNodeSearch) {
        const auto* paths = environment.pedestrianPaths;
        if (!paths) return blocked();
        if (clear(player, steering)) {
            navigation.clearRoute();
        } else {
            const auto& nodes = paths->nodes;
            if (navigation.seekRoute.empty() ||
                std::hypot(navigation.seekDestination[0] - steering.x,
                    navigation.seekDestination[1] - steering.y) > 1.0f) {
                navigation.clearRoute();
                navigation.seekDestination = {steering.x, steering.y};
                uint32_t start = UINT32_MAX, finish = UINT32_MAX;
                float startDistance = std::numeric_limits<float>::max();
                float finishDistance = startDistance;
                for (size_t index = 0; index < nodes.size(); ++index) {
                    if (!storylandPlayerNavigationNodeUsable(environment, *paths, uint32_t(index))) continue;
                    const auto& point = nodes[index].position;
                    const float from = std::hypot(player.x - point.x, player.y - point.y, player.z - point.z);
                    const float to = std::hypot(steering.x - point.x, steering.y - point.y, steering.z - point.z);
                    if (from < startDistance && clear(player, point)) {
                        start = uint32_t(index);
                        startDistance = from;
                    }
                    if (to < finishDistance && clear(point, steering)) {
                        finish = uint32_t(index);
                        finishDistance = to;
                    }
                }
                if (start == UINT32_MAX || finish == UINT32_MAX) return blocked();
                std::vector<float> distances(nodes.size(), std::numeric_limits<float>::max());
                std::vector<uint32_t> previous(nodes.size(), UINT32_MAX);
                std::set<std::pair<float,uint32_t>> pending;
                distances[start] = 0;
                pending.emplace(0.0f, start);
                while (!pending.empty()) {
                    const uint32_t node = pending.begin()->second;
                    pending.erase(pending.begin());
                    if (node == finish) break;
                    for (const auto& link : nodes[node].links) {
                        if (!storylandPlayerNavigationNodeUsable(environment, *paths, link.target)) continue;
                        const auto& from = nodes[node].position;
                        const auto& to = nodes[link.target].position;
                        if (!clear(from, to)) continue;
                        const float next = distances[node] +
                            std::hypot(from.x - to.x, from.y - to.y, from.z - to.z);
                        if (next >= distances[link.target]) continue;
                        pending.erase({distances[link.target], link.target});
                        distances[link.target] = next;
                        previous[link.target] = node;
                        pending.emplace(next, link.target);
                    }
                }
                if (start != finish && previous[finish] == UINT32_MAX) return blocked();
                for (uint32_t node = finish; node != UINT32_MAX; node = previous[node]) {
                    navigation.seekRoute.push_back(node);
                    if (node == start) break;
                }
                std::reverse(navigation.seekRoute.begin(), navigation.seekRoute.end());
            }
            while (navigation.seekRouteCursor < navigation.seekRoute.size()) {
                const uint32_t node = navigation.seekRoute[navigation.seekRouteCursor];
                if (!storylandPlayerNavigationNodeUsable(environment, *paths, node)) return blocked();
                const auto& point = nodes[node].position;
                if (std::hypot(player.x - point.x, player.y - point.y) < .35f) {
                    ++navigation.seekRouteCursor;
                    continue;
                }
                navigation.seekNode = node;
                steering = point;
                break;
            }
            if (navigation.seekRouteCursor == navigation.seekRoute.size()) {
                navigation.clearRoute();
                if (!clear(player, steering)) return blocked();
            }
        }
    }
    if (!navigation.localAvoidanceSolver || clear(player, steering)) return true;
    const float dx = steering.x - player.x, dy = steering.y - player.y;
    const float distance = std::hypot(dx, dy);
    if (distance <= .05f) return blocked();
    const float heading = std::atan2(dy, dx);
    const float radius = std::min(2.0f, distance);
    float shortest = std::numeric_limits<float>::max();
    StorylandNativePoint detour = steering;
    // The host uses the same collision-tested local alternatives as its NPC
    // consumer; the retail navigation flags do not disable body collision.
    for (float turn : {.785398163f, -.785398163f, 1.570796327f, -1.570796327f,
        2.356194490f, -2.356194490f, 3.141592654f}) {
        const StorylandNativePoint candidate{player.x + std::cos(heading + turn) * radius,
            player.y + std::sin(heading + turn) * radius, player.z};
        if (!clear(player, candidate) || !clear(candidate, steering)) continue;
        const float length = radius + std::hypot(steering.x - candidate.x, steering.y - candidate.y);
        if (length < shortest) {
            shortest = length;
            detour = candidate;
        }
    }
    if (shortest == std::numeric_limits<float>::max()) return blocked();
    steering = detour;
    return true;
}

static StorylandLcsEntityExtensionState::Task* storylandRefreshLcsPlayerWalkTarget(
    StorylandMissionEnvironment& environment) {
    auto found=environment.lcsEntityExtension.tasks.find(1);
    if(found==environment.lcsEntityExtension.tasks.end()||found->second.objective!=13)return nullptr;
    auto& walk=environment.playerWalkTask;
    const auto target=environment.missionCharacters.find(found->second.target);
    if((!walk.active&&!walk.completed)||target==environment.missionCharacters.end()) {
        walk.clear();environment.playerNavigation.clearRoute();
        environment.lcsEntityExtension.tasks.erase(found);
        return nullptr;
    }
    const auto& character=target->second;
    auto& task=found->second;
    task.destination={character.x,character.y,character.z};
    task.radius=character.vehicle>=0&&!character.attached?3.0f:2.0f;
    if(character.vehicle>=0&&!character.attached) {
        const auto vehicle=environment.missionVehicles.find(character.vehicle);
        if(vehicle!=environment.missionVehicles.end()&&!vehicle->second.deleted)
            task.destination={vehicle->second.x,vehicle->second.y,vehicle->second.z};
    }
    walk.targetX=task.destination[0];walk.targetY=task.destination[1];walk.targetZ=task.destination[2];
    return &task;
}

void storylandUpdatePlayTestPlayer(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    StorylandNativePoint& player,float& heading,float cameraYaw,const StorylandPlayTestInput& rawInput,
    bool jump,bool interact,std::vector<StorylandNativeTrafficActor>& cars,float seconds) {
    if(!runtime.paused&&!runtime.pauseMenu&&std::isfinite(seconds)&&seconds>0) {
        runtime.playerAnimationMoveDelta={};
        runtime.playerAnimationMoveDeltaValid=false;
    }
    if(!runtime.paused&&!runtime.pauseMenu&&std::isfinite(seconds)&&seconds>0)
        storylandApplyPlayerVehicleDisplacement(runtime,world,player);
    runtime.playerSprintActive = false;
    storylandUpdateAdrenaline(runtime);
    if(world.paths.vcs&&runtime.missionEnvironment&&!runtime.paused&&!runtime.pauseMenu&&
        runtime.health>0&&std::isfinite(seconds)&&seconds>0&&!runtime.playerJetSkiFallActive&&
        !runtime.playerBulletReaction.knockdown&&
        runtime.vehicleAction==StorylandVehicleAction::None&&
        runtime.playerReaction.action!=StorylandPlayerReaction::EvadeDive)
        storylandAdvanceVcsFootstepMarkCounter(runtime.missionEnvironment->playerVcsFootstepMarks);
    if(runtime.playerBulletReaction.knockdown&&!runtime.paused&&!runtime.pauseMenu&&
        std::isfinite(seconds)&&seconds>0) {
        auto* environment=runtime.missionEnvironment;
        const bool owned=runtime.playerBulletReaction.dying?
            (runtime.occupiedVehicle<0&&(!environment||(!environment->cutscene.playing&&
                !environment->playerWarped&&!environment->activeVcsCharacterAttachment(0)))):
            storylandPlayerBulletReactionAllowed(runtime);
        if(!owned)runtime.playerBulletReaction={};
        else {
            refreshMovementCars(runtime,cars);
            if(storylandAdvanceBulletKnockdown(runtime,world,runtime.playerBulletReaction,
                player,heading,seconds,-1,&runtime.playerWater)) {
                runtime.grounded=runtime.playerBulletReaction.fall.grounded;
                runtime.verticalSpeed=runtime.playerBulletReaction.fall.velocity[2];
                runtime.airborneTime=runtime.grounded||runtime.playerWater.active?0:
                    runtime.airborneTime+std::min(seconds,.1f);
                runtime.playerPosition=player;runtime.movementSpeed=0;runtime.playerMoveBlend=0;
                runtime.playerFall={};runtime.landingTime=0;runtime.jumpButtonHeld=jump;
                return;
            }
        }
    }
    if(runtime.playerJetSkiFallActive&&!runtime.paused&&!runtime.pauseMenu&&
        std::isfinite(seconds)&&seconds>0) {
        refreshMovementCars(runtime,cars);
        if(updateVcsJetSkiPlayerFall(runtime,world,player,heading,seconds))return;
    }
    if(runtime.paused||runtime.pauseMenu||runtime.health<=0||!std::isfinite(seconds)||seconds<=0) return;
    const float dt=std::min(seconds,0.1f);
    const bool jumpPressed=jump&&!runtime.jumpButtonHeld;
    runtime.jumpButtonHeld=jump;
    runtime.playerPosition=player;
    auto* environment=runtime.missionEnvironment;
    auto& sprintState=runtime.playerSprint;
    if(!sprintState.association&&runtime.playerSprintBlend>0) {
        sprintState.association=true;
        sprintState.blendDelta=runtime.playerSprintFading?-1.0f:2.0f;
    }
    runtime.playerSprintBlend=StorylandSprint::advanceBlend(sprintState,runtime.playerSprintBlend,dt);
    runtime.playerSprintFading=sprintState.blendDelta<0;
    if(environment&&sprintState.movementState!=5) {
        StorylandSprint::recover(environment->playerSprintEnergy,environment->playerSprintMaximum,
            dt*StorylandNativePhysicsRate,sprintState.movementState==4?.3f:1.0f,
            world.paths.vcs?environment->drunkVisualIntensity:0);
    }
    sprintState.movementState=1;
    const bool ordinaryFootPose=runtime.occupiedVehicle<0&&
        runtime.vehicleAction==StorylandVehicleAction::None&&runtime.grounded&&
        !runtime.jumpLaunching&&!runtime.playerWater.active&&!runtime.extendedMovement.busy()&&
        runtime.debugMovement==StorylandDebugMovement::Walking;
    if(!ordinaryFootPose) {
        sprintState.walkStartTime=-1;
        sprintState.tiredBlend=0;
        sprintState.tiredTime=-1;
        sprintState.tiredBlendDelta=0;
    } else if(sprintState.walkStartRunning()) {
        sprintState.walkStartTime+=dt;
        if(!sprintState.walkStartRunning())sprintState.walkStartTime=-1;
    }
    StorylandSprint::advanceTired(sprintState,dt);
    if(world.paths.vcs&&environment&&!environment->playerWalkTask.active)
        environment->playerNavigation.clearRoute();
    int32_t remoteSlot=-1;
    bool vcsRemote=false;
    if(world.paths.vcs&&environment&&environment->remoteVehicle>=0) {
        const auto owner=environment->missionVehicles.find(environment->remoteVehicle);
        if(owner==environment->missionVehicles.end()||owner->second.deleted||owner->second.wrecked)
            environment->remoteVehicle=-1;
        else remoteSlot=storylandVcsRemoteVehicleSlot(runtime);
        if(remoteSlot<0||size_t(remoteSlot)>=cars.size()) {runtime.movementSpeed=0;return;}
        vcsRemote=true;
    }
    if(world.paths.vcs&&environment&&!vcsRemote&&
        (environment->playerPositionFrozen||environment->cutscene.playing||
            environment->activeVcsCharacterAttachment(0))) {
        environment->playerWalkTask.clear();
        environment->playerNavigation.clearRoute();
        runtime.movementSpeed=0;
        return;
    }
    if(!vcsRemote)storylandRecoverFallenPlayer(runtime,world,player,cars);
    if (world.paths.vcs && environment && !vcsRemote) {
        float waterHeight = 0;
        const bool inWater = runtime.occupiedVehicle < 0 &&
            deepCharacterWater(world, player, waterHeight);
        auto& boundary = environment->vcsSwimmingBoundary;
        const float force = boundary.advance(player.x, player.y, inWater);
        if (inWater) {
            boundary.velocity += force * StorylandNativePhysicsRate /
                std::max(1.0f, runtime.playerMass);
            boundary.velocity *= std::exp(-4.0f * dt);
            boundary.velocity = std::clamp(boundary.velocity,
                -4.0f * StorylandNativePhysicsRate, 4.0f * StorylandNativePhysicsRate);
        }
    }
    auto input=storylandDelayedMovementInput(runtime,rawInput,environment?environment->drunkInputDelay:0);
    if(!storylandPlayerBulletReactionAllowed(runtime)) {
        runtime.playerBulletReaction.animation=-1;
        runtime.playerBulletReaction.duration=0;
    }
    if(storylandPlayerBulletReactionActive(runtime)) {
        input={};jump=false;interact=false;
        runtime.movementSpeed=0;runtime.playerMoveBlend=0;
        runtime.playerStopVariant=-1;runtime.playerStopTime=0;
        runtime.extendedMovement.crouchMoveBlend=0;
    }
    if(!runtime.playerControl||(!world.paths.vcs&&environment&&environment->lcsExtendedWorld.scriptInputPause)) {
        input={};jump=false;interact=false;
    }
    if(!world.paths.vcs)remoteSlot=storylandRemoteVehicleSlot(runtime);
    const bool remote=remoteSlot>=0&&size_t(remoteSlot)<cars.size();
    if(!world.paths.vcs&&runtime.missionEnvironment&&runtime.missionEnvironment->remoteControl.vehicle>=0&&
        (!remote||runtime.missionEnvironment->remoteControl.ending))return;
    if(!world.paths.vcs&&remote&&runtime.vehicles[size_t(remoteSlot)].nativeDefinition.modelId==169&&
        (runtime.vehicles[size_t(remoteSlot)].water.immersion>0||
            storylandRcBanditHitsWheels(runtime,cars,size_t(remoteSlot)))) {
        if(storylandDetonateRemoteVehicle(runtime,true))return;
    }
    const int controlledVehicle=remote?remoteSlot:runtime.occupiedVehicle;
    if(runtime.pendingBikeImpactVehicle>=0&&runtime.pendingBikeImpactVehicle!=runtime.occupiedVehicle) {
        runtime.pendingBikeImpactVehicle=-1;
        runtime.pendingBikeImpact={};
    }
    const bool scriptedVehicleEntry=!remote&&updateScriptedPlayerVehicleEntry(runtime,cars,player);
    if(scriptedVehicleEntry) {input={};jump=false;interact=false;}
    const bool scriptedExit=environment&&environment->playerExitVehicleRequested&&runtime.occupiedVehicle>=0;
    if(scriptedExit)interact=true;
    if(environment&&environment->inVehicle&&std::any_of(environment->garages.begin(),environment->garages.end(),
        [&](const auto& garage){return garage.serviceVehicle==environment->occupiedMissionVehicle&&garage.serviceUntil!=0;})) {
        input={};input.handbrake=true;interact=false;jump=false;
    }
    const bool manualMovementLock=remoteSlot<0&&environment&&environment->manualAim.forced&&
        !environment->camera.active&&!environment->cutscene.playing&&
        runtime.occupiedVehicle<0&&!environment->activeVcsCharacterAttachment(0)&&
        !runtime.playerWater.active&&runtime.entryTransition<0&&
        runtime.vehicleAction==StorylandPlayTestRuntime::VehicleAction::None;
    const bool scriptedWait=!remote&&environment&&
        storylandVcsWaitBlocksMovement(environment->playerWaitState);
    if(manualMovementLock||scriptedWait) {input={};jump=false;interact=false;}
    bool scriptedWalk=false,scriptedWalkSteering=false;
    StorylandNativePoint scriptedWalkTarget;
    auto* trackedWalk=!world.paths.vcs&&environment?storylandRefreshLcsPlayerWalkTarget(*environment):nullptr;
    const auto walkArrived=[&]() {
        const auto& task=environment->playerWalkTask;
        return trackedWalk?environment->gameMilliseconds>0&&
            std::hypot(task.targetX-player.x,task.targetY-player.y,task.targetZ-player.z)<=trackedWalk->radius:
            storylandScriptPlayerWalkArrived(task,player.x,player.y);
    };
    if(environment&&(environment->playerWalkTask.active||trackedWalk)) {
        auto& task=environment->playerWalkTask;
        const bool invalid=!storylandValidScriptPlayerWalkTask(task)||runtime.busted||runtime.wastedActive||
            environment->playerWarped||environment->playerPositionFrozen||environment->cutscene.playing||
            (world.paths.vcs?environment->activeVcsCharacterAttachment(0):
                environment->playerAttachedVehicle>=0)||runtime.playerWater.active||remote||
            runtime.vehicleAction==StorylandVehicleAction::Enter||runtime.vehicleAction==StorylandVehicleAction::Hijack;
        if(invalid) {
            task.clear();
            if(trackedWalk) {
                environment->lcsEntityExtension.tasks.erase(1);
                trackedWalk=nullptr;
                environment->playerNavigation.clearRoute();
            }
            if(world.paths.vcs)environment->playerNavigation.clearRoute();
        }
        else if(controlledVehicle<0&&runtime.vehicleAction==StorylandVehicleAction::None&&
            runtime.entryTransition<0&&!scriptedWait&&!manualMovementLock) {
            scriptedWalk=true;
            input={};
            const auto running=environment->lcsEntityExtension.runningOverrides.find(1);
            input.running=world.paths.vcs?task.moveMode!=StorylandScriptPlayerMoveMode::Walk:
                !trackedWalk&&running!=environment->lcsEntityExtension.runningOverrides.end()&&running->second;
            input.sprinting=world.paths.vcs&&task.moveMode==StorylandScriptPlayerMoveMode::Sprint;
            jump=false;interact=false;
            runtime.playerStopVariant=-1;runtime.playerStopTime=0;
            runtime.playerAiming=false;runtime.manualAimActive=false;runtime.freeAimActive=false;
            storylandClearPlayTestTarget(runtime);
            if(walkArrived()) {
                task.active=false;task.completed=true;
                if(trackedWalk&&std::hypot(task.targetX-player.x,task.targetY-player.y)>.001f) {
                    const float desired=std::atan2(task.targetY-player.y,task.targetX-player.x)/radians;
                    heading+=std::clamp(std::remainder(desired-heading,360.0f),-420.0f*dt,420.0f*dt);
                }
                if(world.paths.vcs)environment->playerNavigation.clearRoute();
            } else if((!trackedWalk||environment->gameMilliseconds>0)&&
                storylandResolvePlayerWalkSteering(*environment,world,player,scriptedWalkTarget)) {
                scriptedWalkSteering=true;
                input.forward=true;
                if(world.paths.vcs&&environment->playerAvoidUnsupportedForwardStep) {
                    float originOffset=0;
                    const bool metadata=environment->characterOriginOffset&&
                        environment->characterOriginOffset(0,originOffset)&&std::isfinite(originOffset);
                    const bool supported=metadata&&storylandVcsPedForwardSupportProbe(world,true,
                        runtime.health>0,false,player,scriptedWalkTarget,heading-90.0f,originOffset).supported;
                    if(!supported) {input.forward=false;scriptedWalkSteering=false;}
                }
                cameraYaw=std::atan2(scriptedWalkTarget.y-player.y,scriptedWalkTarget.x-player.x)/radians;
            }
        }
    }
    if(!scriptedWalk&&remoteSlot<0&&runtime.debugMovement!=StorylandDebugMovement::Walking && runtime.playerControl &&
        runtime.occupiedVehicle<0 && runtime.entryTransition<0 &&
        runtime.vehicleAction==StorylandPlayTestRuntime::VehicleAction::None) {
        const auto previous=player;
        player=storylandMoveDebugPlayer(world,player,cameraYaw,input,runtime.debugMovement,
            runtime.debugMoveSpeed,dt);
        runtime.movementSpeed=std::hypot(player.x-previous.x,player.y-previous.y,player.z-previous.z)/dt;
        runtime.playerPosition=player;
        runtime.verticalSpeed=0;
        runtime.airborneTime=0;
        runtime.landingTime=0;
        runtime.playerFall={};
        runtime.grounded=false;
        runtime.jumpLaunching=false;runtime.jumpFromLaunch=false;runtime.jumpHorizontalValid=false;
        runtime.jumpLaunchTime=0;
        runtime.playerStopVariant=-1;runtime.playerStopTime=0;
        storylandStepWaterCharacter(runtime.playerWater,player.z,0,false,true,false,false,dt,world.paths.vcs);
        if(std::hypot(player.x-previous.x,player.y-previous.y)>0.001f)
            heading=std::atan2(player.y-previous.y,player.x-previous.x)/radians;
        return;
    }
    refreshMovementCars(runtime,cars);
    if(!remote&&updateDetachedVehicleExit(runtime,world,player,heading,dt,cars))return;
    if(!remote&&storylandUpdateEntryExitTransition(runtime,player,heading,dt))return;
    if(!world.paths.vcs&&!runtime.playerControl&&!scriptedWalk&&!scriptedVehicleEntry&&
        !scriptedExit&&runtime.vehicleAction!=StorylandVehicleAction::Exit) {
        if(runtime.missionEnvironment&&(runtime.missionEnvironment->applyPlayerCarBrakes||runtime.missionEnvironment->playerCarBrakes)&&controlledVehicle>=0&&
            size_t(controlledVehicle)<runtime.vehicles.size()) {
            auto& vehicle=runtime.vehicles[size_t(controlledVehicle)];
            vehicle.handbrake=true;vehicle.braking=true;vehicle.throttle=0;
            const float speed=std::hypot(vehicle.speed,vehicle.lateralSpeed,vehicle.verticalSpeed);
            if(speed>14.0f) {
                const float scale=14.0f/speed;
                vehicle.speed*=scale;vehicle.lateralSpeed*=scale;vehicle.verticalSpeed*=scale;
            }
            vehicle.speed=approach(vehicle.speed,0,dt*vehicle.handlingBrake*1.4f*std::max(0.5f,vehicle.traction));
        }
        if(!remote&&controlledVehicle>=0&&size_t(controlledVehicle)<cars.size()&&
            size_t(controlledVehicle)<runtime.vehicles.size()&&runtime.missionEnvironment) {
            const auto& vehicle=runtime.vehicles[size_t(controlledVehicle)];
            const auto state=runtime.missionEnvironment->missionVehicles.find(vehicle.scriptHandle);
            if(state!=runtime.missionEnvironment->missionVehicles.end()&&lcsScriptBoatPhysics(vehicle,&state->second)) {
                // Input suppression does not detach a seated player from the
                // STATUS_PHYSICS boat already advanced by the traffic tick.
                player=cars[size_t(controlledVehicle)].position;
                heading=cars[size_t(controlledVehicle)].yaw;
                runtime.playerPosition=player;
            }
        }
        return;
    }
    if(!remote) {
        using VehicleAction = StorylandPlayTestRuntime::VehicleAction;
        if(runtime.missionEnvironment&&runtime.occupiedVehicle<0&&
            runtime.vehicleAction==VehicleAction::None&&
            !runtime.missionEnvironment->activeVcsCharacterAttachment(0)&&!runtime.playerWater.active&&
            !runtime.missionEnvironment->cutscene.playing&&!runtime.missionEnvironment->camera.active&&
            !manualMovementLock&&!scriptedWait&&(jump||interact||input.sprinting)) {
            auto& crouch=runtime.missionEnvironment->playerCrouch;
            if(crouch.requested||crouch.active) {
                storylandClearCrouch(crouch,runtime.missionEnvironment->gameMilliseconds);
                crouch.requested=false;
            }
        }
        if (interact && runtime.vehicleAction == VehicleAction::None &&
            !runtime.extendedMovement.climbing()) {
            int target = runtime.occupiedVehicle;
            if (target < 0) {
                float nearest = 4.0f;
                for (size_t index = 0; index < cars.size() && index < runtime.vehicles.size(); ++index) {
                    const float separation = distance(cars[index].position, player);
                    const auto& candidate=runtime.vehicles[index];
                    auto access=vehicleAccessState(candidate);
                    access.driverProtected=storylandMissionDriverProtected(runtime,candidate);
                    if (separation < nearest&&storylandRetailVehicleCanEnter(access)&&
                        !scriptVehicleAccessBlocked(runtime,candidate,false)) {
                        nearest = separation;
                        target = int(index);
                    }
                }
            }
            if (target >= 0 && size_t(target) < cars.size() && size_t(target) < runtime.vehicles.size()) {
                auto& vehicle = runtime.vehicles[size_t(target)];
                if(!vehicle.nativeDefinition.retailTransitions) {
                    runtime.vehicleTransitionError="Missing native vehicle entry/exit associations for model "+
                        std::to_string(vehicle.nativeDefinition.modelId);
                    return;
                }
                runtime.vehicleTransitionError.clear();
                if(scriptVehicleAccessBlocked(runtime,vehicle,runtime.occupiedVehicle>=0))return;
                runtime.vehicleEntryVariant=StorylandVehicleEntryVariant::Normal;
                if(runtime.occupiedVehicle>=0) {
                    auto access=vehicleAccessState(vehicle);
                    float floor=0;
                    if(access.vcs&&(access.helicopter||access.hovercraft)) {
                        if(!vcsHelicopterHeightAboveSurface(world,cars[size_t(target)],vehicle,access.heightAboveGround)) {
                            runtime.vehiclePhysicsError="Native VCS helicopter exit requires a valid entity/surface query.";
                            return;
                        }
                    } else if(vehicleGroundHeight(world,cars[size_t(target)].position.x,cars[size_t(target)].position.y,
                        cars[size_t(target)].position.z,floor))
                        access.heightAboveGround=std::max(0.0f,cars[size_t(target)].position.z-floor);
                    if(!storylandRetailVehicleCanExit(access)) {
                        bool passenger=runtime.playerVehicleSeat>=0;
                        if(environment)passenger=passenger||std::any_of(environment->missionCharacters.begin(),environment->missionCharacters.end(),
                            [&](const auto& entry){return entry.second.alive&&entry.second.vehicle==vehicle.scriptHandle&&entry.second.passengerSeat==0;});
                        if(!storylandRetailVehicleCanJumpOut(access,passenger))return;
                        runtime.vehicleEntryVariant=StorylandVehicleEntryVariant::MovingExit;
                    }
                }
                if (runtime.occupiedVehicle < 0) {
                    const int32_t seat = runtime.coopPreferredVehicleSeat ? runtime.coopPreferredVehicleSeat(target, player) : -1;
                    if (!storylandBeginPlayTestVehicleEntry(runtime, cars, player, target, seat)) return;
                } else {
                runtime.vehicleAction = VehicleAction::Exit;
                runtime.transitionVehicle = target;
                if(runtime.playerVehicleSeat<0)releaseVehicleDriverInput(vehicle);
                runtime.vehicleActionTime = 0;
                runtime.vehicleTransitionPrepared=false;runtime.vehiclePhase={};
                const auto& definition=vehicle.nativeDefinition;
                runtime.vehicleActionDuration=runtime.vehicleAction==VehicleAction::Exit?definition.exitSeconds:
                    definition.entrySeconds+(runtime.vehicleAction==VehicleAction::Hijack?definition.jackSeconds:0);
                runtime.vehicleActionStart = player;
                runtime.vehicleExitRight=false;
                runtime.vehicleWasParked=vehicle.parked;
                if(environment) {environment->playerExitVehicleRequested=false;environment->playerVehicleTransition=true;}
                if(!storylandVcsOrdinaryBoat(vehicle.nativeDefinition)&&
                    (runtime.playerVehicleSeat<0||runtime.vehicleEntryVariant!=StorylandVehicleEntryVariant::MovingExit))
                    vehicle.parked = true;
                if(!vehicle.positionFrozen&&runtime.vehicleEntryVariant!=StorylandVehicleEntryVariant::MovingExit&&
                    runtime.playerVehicleSeat<0&&!storylandVcsOrdinaryBoat(vehicle.nativeDefinition))vehicle.speed=0;
                }
            }
        }
        if (!remote&&runtime.vehicleAction != VehicleAction::None) {
            const size_t target = size_t(runtime.transitionVehicle);
            if (target >= cars.size() || target >= runtime.vehicles.size()) {
                runtime.vehicleAction = VehicleAction::None;
                runtime.transitionVehicle = -1;
                return;
            }
            auto& car = cars[target];
            auto& vehicle = runtime.vehicles[target];
            if(scriptVehicleAccessBlocked(runtime,vehicle,runtime.vehicleAction==VehicleAction::Exit)) {
                storylandCancelPassengerSideCarjack(runtime,vehicle);
                vehicle.parked=runtime.vehicleWasParked;runtime.vehicleAction=VehicleAction::None;
                if(runtime.occupiedVehicle<0)runtime.playerVehicleSeat=-1;
                runtime.transitionVehicle=-1;runtime.vehicleTransitionPrepared=false;return;
            }
            if (runtime.vehicleAction == VehicleAction::Hijack &&
                storylandMissionDriverProtected(runtime, vehicle)) {
                vehicle.parked = runtime.vehicleWasParked;
                runtime.vehicleAction = VehicleAction::None;
                runtime.transitionVehicle = -1;
                if (environment) {
                    environment->playerVehicleTransition = false;
                }
                return;
            }
            if(!vehicle.positionFrozen&&runtime.vehicleEntryVariant!=StorylandVehicleEntryVariant::MovingExit&&
                runtime.playerVehicleSeat<0&&!storylandVcsOrdinaryBoat(vehicle.nativeDefinition))vehicle.speed=0;
            if(!vehicle.nativeDefinition.retailTransitions) {
                runtime.vehicleTransitionError="Native vehicle entry/exit animations are not loaded for model "+
                    std::to_string(vehicle.nativeDefinition.modelId);
                vehicle.parked=runtime.vehicleWasParked;
                runtime.vehicleAction=VehicleAction::None;runtime.transitionVehicle=-1;
                runtime.vehicleTransitionPrepared=false;
                if(environment)environment->playerVehicleTransition=false;
                return;
            }
            updateRetailVehicleTransition(runtime,world,player,heading,cars,target,dt);
            return;
        }
    }
    if(controlledVehicle>=0&&size_t(controlledVehicle)<cars.size()&&size_t(controlledVehicle)<runtime.vehicles.size()) {
        runtime.jumpLaunching=false;runtime.jumpHorizontalValid=false;runtime.jumpFromLaunch=false;runtime.jumpLaunchTime=0;
        if(!remote) {runtime.playerFall={};runtime.landingTime=0;}
        runtime.playerStopVariant=-1;runtime.playerStopTime=0;
        auto& car=cars[size_t(controlledVehicle)];auto& vehicle=runtime.vehicles[size_t(controlledVehicle)];
        if(remote||runtime.playerVehicleSeat<0)vehicle.vcsTrafficRail.active=false;
        if(!remote&&runtime.pendingBikeImpactVehicle==controlledVehicle) {
            const auto impact=runtime.pendingBikeImpact;
            runtime.pendingBikeImpactVehicle=-1;
            runtime.pendingBikeImpact={};
            ejectBikeRider(runtime,player,heading,cars,size_t(controlledVehicle),impact);
            return;
        }
        if(remote&&!runtime.playerControl&&!world.paths.vcs) {runtime.movementSpeed=0;return;}
        if (vehicle.coopControlled) {
            player = car.position; heading = car.yaw; runtime.playerPosition = player;
            runtime.movementSpeed = 0;
            return;
        }
        if(scriptVehicleFrozen(runtime,vehicle)) {
            stopFrozenVehicle(vehicle);
            if(vehicle.craneCarried)vehicle.headlightsOn=false;
            refreshMovementCars(runtime,cars);
            if(!remote) {player=car.position;heading=car.yaw;runtime.playerPosition=player;}
            return;
        }
        if(!remote&&vehicle.nativeDefinition.vcsModelLayout&&
            (vehicle.nativeDefinition.nativeType==4||vehicle.nativeDefinition.vcsHovercraft())&&
            vehicle.nativeDefinition.domain==StorylandVehicleDomain::Air) {
            // Traffic advances the actual player body once, including control-
            // off frames. Later scripted STATUS_PHYSICS retains its own AI.
            player=car.position;heading=car.yaw;runtime.playerPosition=player;
            runtime.movementSpeed=0;
            return;
        }
        if(remote&&vehicle.nativeDefinition.domain==StorylandVehicleDomain::Air) {
            auto& task=environment->missionVehicles.at(vehicle.scriptHandle);
            const auto oldTask=task;
            const bool oldAi=vehicle.aiActive;const int8_t oldMission=vehicle.aiMission;
            const float turn=-input.horizontal();
            const float angle=(car.yaw+turn*90*dt)*radians;
            const bool helicopter=vehicle.nativeDefinition.nativeType==4;
            const float command=helicopter?float(input.forward)-float(input.backward):1.0f;
            task.targetX=car.position.x+std::cos(angle)*command*40;
            task.targetY=car.position.y+std::sin(angle)*command*40;
            task.targetZ=car.position.z+(float(input.ascend)-float(input.descend))*12;
            if(helicopter)task.targetZ=std::min(environment->rcHeliHeightLimit,task.targetZ);
            task.cruiseSpeed=helicopter?12.0f:input.forward?28.0f:18.0f;
            vehicle.aiActive=true;vehicle.aiMission=20;
            const bool crashed=storylandAdvanceVehicleFlight(runtime,vehicle,car,&world,dt);
            if(helicopter&&car.position.z>environment->rcHeliHeightLimit) {
                car.position.z=environment->rcHeliHeightLimit;
                vehicle.verticalSpeed=std::min(0.0f,vehicle.verticalSpeed);
            }
            task=oldTask;vehicle.aiActive=oldAi;vehicle.aiMission=oldMission;
            if(crashed) {
                task.x=car.position.x;task.y=car.position.y;task.z=car.position.z;
                std::string error;storylandExplodeScriptVehicle(*environment,vehicle.scriptHandle,error);
            }
            runtime.movementSpeed=0;return;
        }
        if(!remote) {
            auto fall=vehicleBikeSpin(vehicle,car);
            if(runtime.crime.vcs&&vehicle.nativeDefinition.twoWheels()&&vehicle.water.engineFlooded&&
                std::all_of(vehicle.wheelLineFractions.begin(),vehicle.wheelLineFractions.end(),
                    [](float fraction){return fraction==1.0f;})) {
                fall={};fall.eject=true;fall.animation=139;fall.inheritVelocity=.2f;fall.nativeWaterFall=true;
            }
            if(fall.eject) {ejectBikeRider(runtime,player,heading,cars,size_t(controlledVehicle),fall);return;}
        }
        if(!remote&&runtime.playerVehicleSeat>=0) {
            float headWater = 0;
            if (!vehicle.nativeDefinition.vcsModelLayout&&
                storylandNativeWaterHeight(world, car.position.x, car.position.y, headWater) &&
                headWater > car.position.z + vehicle.bodyHeight) {
                runtime.playerWater.air = std::max(0.0f, runtime.playerWater.air - dt);
                if (runtime.playerWater.air <= 0) runtime.inflictDamage(20*dt,0,
                    storylandNativeDrowningWeapon(runtime.crime.vcs),false);
            } else if(!runtime.crime.vcs)runtime.playerWater = {};
            player=car.position;heading=car.yaw;runtime.playerPosition=player;
            return;
        }
        if(!remote&&car.water&&lcsBoatPhysicsAvailable(vehicle)) {
            StorylandMissionEnvironment::VehicleScriptState local;
            auto* state=&local;local.engineOn=vehicle.engineOn;
            if(runtime.missionEnvironment) {
                const auto found=runtime.missionEnvironment->missionVehicles.find(vehicle.scriptHandle);
                if(found!=runtime.missionEnvironment->missionVehicles.end())state=&found->second;
            }
            // Script STATUS_PHYSICS takes control even with a seated player.
            // Otherwise native boat input drives the same propeller body.
            if(!lcsScriptBoatPhysics(vehicle,state)) {
                state->scriptStatus=0;state->drivingToTarget=false;
                advanceLcsScriptBoat(runtime,world,car,vehicle,*state,cars,size_t(controlledVehicle),dt,&input);
                refreshMovementCars(runtime,cars);
            }
            float headWater=0;
            if(storylandNativeWaterHeight(world,car.position.x,car.position.y,headWater)&&
                headWater>car.position.z+vehicle.bodyHeight) {
                runtime.playerWater.air=std::max(0.0f,runtime.playerWater.air-dt);
                if(runtime.playerWater.air<=0)runtime.inflictDamage(20*dt,0,
                    storylandNativeDrowningWeapon(runtime.crime.vcs),false);
            } else runtime.playerWater={};
            player=car.position;heading=car.yaw;runtime.playerPosition=player;
            return;
        }
        bool releaseBoatAnchor=!vehicle.nativeDefinition.vcsModelLayout;
        if(!remote&&car.water&&vehicle.nativeDefinition.vcsModelLayout&&
            (vehicle.nativeDefinition.nativeType==1||vehicle.nativeDefinition.nativeType==2)) {
            releaseBoatAnchor=true;
            if(environment&&vehicle.scriptHandle>=0) {
                const auto owner=environment->missionVehicles.find(vehicle.scriptHandle);
                releaseBoatAnchor=owner!=environment->missionVehicles.end()&&
                    owner->second.scriptStatus==0&&owner->second.driver==0;
            }
        }
        // Original CBoat status 0, 19FFC8..1A0008, releases the player driver's anchor.
        if(releaseBoatAnchor&&car.water) {
            vehicle.boatAnchored=false;vehicle.boatAnchorHeading=10000.0f;
            if(runtime.missionEnvironment) {
                const auto found=runtime.missionEnvironment->missionVehicles.find(vehicle.scriptHandle);
                if(found!=runtime.missionEnvironment->missionVehicles.end()) {
                    found->second.boatAnchored=false;found->second.boatAnchorHeading=10000.0f;
                }
            }
        }
        bool temporaryHandbrake=false;
        if(world.paths.vcs&&environment&&vehicle.scriptHandle>=0) {
            const auto scripted=environment->missionVehicles.find(vehicle.scriptHandle);
            temporaryHandbrake=scripted!=environment->missionVehicles.end()&&
                scripted->second.scriptStatus==3&&scripted->second.temporaryAction==5&&
                int32_t(environment->gameMilliseconds-scripted->second.temporaryActionUntil)<=0;
            if(temporaryHandbrake) {input={};input.handbrake=true;}
        }
        const bool scriptBrakes=runtime.missionEnvironment&&(runtime.missionEnvironment->applyPlayerCarBrakes||runtime.missionEnvironment->playerCarBrakes);
        const bool handbrake=StorylandVcsRetailQuirks::playerVehicleHandbrake(
            vehicle.nativeDefinition.vcsModelLayout,vehicle.nativeDefinition.handlingFlags,
            input.handbrake&&!temporaryHandbrake,temporaryHandbrake||scriptBrakes)||
            (car.water&&vehicle.boatAnchored);
        if(scriptBrakes) {
            const float speed=std::hypot(vehicle.speed,vehicle.lateralSpeed,vehicle.verticalSpeed);
            if(speed>14.0f) {
                const float scale=14.0f/speed;
                vehicle.speed*=scale;vehicle.lateralSpeed*=scale;vehicle.verticalSpeed*=scale;
            }
        }
        vehicle.handbrake=handbrake;
        const bool taxiDown=runtime.playerControl&&environment&&environment->padState[18]!=0;
        const bool oldTaxiPressed=taxiDown&&!runtime.vehicleTaxiButtonWasDown;
        runtime.vehicleTaxiButtonWasDown=taxiDown;
        bool taxiPressed=oldTaxiPressed;
        bool taxiMotion=vehicle.grounded&&std::fabs(vehicle.speed)>10;
        if(world.paths.vcs&&environment) {
            taxiPressed=runtime.playerControl&&storylandVcsPadState(*environment,13)!=0;
            // Preserve the legacy contact fallback where no native wheel trace is available.
            const float firstWheel=vehicle.contactsValid?vehicle.wheelLineFractions[0]:
                (vehicle.grounded?0.0f:1.0f);
            taxiMotion=storylandVcsTaxiBoostGate(vehicle.speed,vehicle.lateralSpeed,
                vehicleVerticalVelocity(vehicle),firstWheel,taxiPressed);
        }
        if(!remote&&environment&&environment->taxiBoost&&taxiPressed&&taxiMotion) {
            const int32_t model=int32_t(vehicle.nativeDefinition.modelId);
            const auto resolve=[&](int32_t alias){return environment->resolveObjectModel?environment->resolveObjectModel(alias):alias;};
            if(model==226||model==183||model==resolve(-959)||model==resolve(-937)) {
                // SLUS 0x13C550 queues event 20 for the taxi jump.
                if(world.paths.vcs&&environment->audioSubsystemEnabled&&environment->scriptSounds.size()<4096) {
                    StorylandMissionEnvironment::ScriptSound sound;
                    sound.id=20;
                    sound.position={car.position.x,car.position.y,car.position.z};
                    sound.vcsVehicleAudioEvent=20;
                    environment->scriptSounds.push_back(sound);
                }
                vehicle.verticalSpeed+=12.5f;vehicle.grounded=false;
                vehicle.angularVelocity[1]+=.5f;
            }
        }
        vehicle.headlightsOn=storylandVehicleLampsRequired(runtime,vehicle)&&vehicle.health>0;
        const bool engineRunning=!vehicle.water.engineFlooded&&vehicle.health>0&&vehicle.engineOn;
        const bool nativeBoatPropulsion=!remote&&car.water&&
            storylandVcsBoatPropulsionAvailable(vehicle.nativeDefinition);
        const bool nativePedals=vehicle.nativeHandling&&
            !car.water&&vehicle.nativeDefinition.domain==StorylandVehicleDomain::Road;
        const float command=engineRunning?float(input.forward)-float(input.backward):0.0f;
        StorylandVehiclePedals pedals;
        if(nativePedals) {
            const auto pressure=input.vehiclePedalPressures();
            pedals=storylandRetailVehiclePedals(vehicle.speed,pressure[0],pressure[1]);
            if(!engineRunning)pedals.throttle=0;
            if(scriptBrakes)pedals={0,1,false};
            if(temporaryHandbrake)pedals={0,0,false};
            vehicle.throttle=pedals.throttle;
        } else if(nativeBoatPropulsion) {
            const auto pressure=input.vehiclePedalPressures();
            vehicle.boatAcceleratorInput=storylandVcsBoatThrottle(pressure[0],pressure[1],
                input.handbrake,vehicle.boatBrakeInput);
            vehicle.throttle=engineRunning&&runtime.playerControl&&!scriptBrakes&&!vehicle.boatAnchored?
                vehicle.boatAcceleratorInput:0;
        } else vehicle.throttle=approach(vehicle.throttle,command,dt*4.0f);
        const float previousSpeed=vehicle.speed;
        const float traction=std::clamp(vehicle.traction,0.0f,5.0f);
        const float engine=std::clamp(vehicle.handlingEngine*
            std::sqrt(1500.0f/std::max(100.0f,vehicle.mass)),2.0f,28.0f)*
            (car.water?0.6f:1.0f);
        const float maximum=car.water?std::min(18.0f,vehicle.handlingTopSpeed):vehicle.handlingTopSpeed;
        const float reverseMaximum=car.water?std::min(5.0f,vehicle.handlingReverse):vehicle.handlingReverse;
        const bool braking=nativePedals?pedals.brake>0:
            command*vehicle.speed<0&&std::fabs(vehicle.speed)>0.3f;
        vehicle.braking=!temporaryHandbrake&&(braking||handbrake);
        const bool nativeTyres=vehicle.nativeHandling&&vehicle.nativeDefinition.hasNativeSuspension&&
            !car.water&&vehicle.nativeDefinition.domain==StorylandVehicleDomain::Road;
        vehicle.tyreForcesApplied=false;
        vehicle.bikeTyreAcceleration=0;vehicle.bikeTyreAccelerationWorld={};
        vehicle.bikeLeanInput=vehicle.nativeDefinition.motorcycle()?
            StorylandBikeDynamics::leanInput(vehicle.bikeLeanInput,input.bikeLean,dt):0;
        float turn=-input.horizontal();
        if(world.paths.vcs&&environment) {
            const int16_t rawAxis=int16_t(std::lround(rawInput.horizontal()*128.0f));
            const int16_t drift=StorylandVcsIntoxication::advanceSteeringDrift(
                environment->drunkSteeringDrift,environment->drunkVisualIntensity,rawAxis);
            if(runtime.playerControl)turn-=float(drift)/128.0f;
        }
        if(temporaryHandbrake) {
            vehicle.steeringAngle=0;vehicle.steeringInput=0;vehicle.steeringInputValid=true;
        } else if(nativePedals) {
            const auto& definition=vehicle.nativeDefinition;
            if(!vehicle.steeringInputValid) {
                vehicle.steeringInput=storylandVehicleSteeringInputFromAngle(
                    vehicle.steeringAngle,definition.steeringLock);
                vehicle.steeringInputValid=true;
            }
            vehicle.steeringAngle=storylandVehicleSteeringAngle(vehicle.steeringInput,
                turn,definition.steeringLock,dt);
        } else {
            const float steeringLimit=vehicle.handlingSteerLock/(1+std::fabs(vehicle.speed)*0.06f);
            vehicle.steeringAngle=approach(vehicle.steeringAngle,turn*steeringLimit,dt*120);
            vehicle.steeringInputValid=false;
        }
        if(nativeBoatPropulsion) {
            const auto& definition=vehicle.nativeDefinition;
            const auto frame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
                {car.position.x,car.position.y,car.position.z},car.yaw,definition.previewMinimumZ,
                vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,true),
                definition.previewRoot);
            const float c=std::cos(car.yaw*radians),s=std::sin(car.yaw*radians);
            std::array<float,3> velocity{(c*vehicle.speed-s*vehicle.lateralSpeed)/StorylandNativePhysicsRate,
                (s*vehicle.speed+c*vehicle.lateralSpeed)/StorylandNativePhysicsRate,
                vehicleVerticalVelocity(vehicle)/StorylandNativePhysicsRate};
            const auto angular=vehicleAngularVelocity(vehicle,car.yaw);
            const float nativeStep=environment?environment->nativeScriptTimeStep:dt*StorylandNativePhysicsRate;
            const auto contact=storylandEvaluateVcsBoatWater(definition,frame,velocity,
                std::array<float,3>{angular.x/StorylandNativePhysicsRate,angular.y/StorylandNativePhysicsRate,
                    angular.z/StorylandNativePhysicsRate},vehicle.mass,nativeStep,[&](float x,float y) {
                    StorylandBoatWaterSample water;
                    water.available=storylandNativeWaterHeight(world,x,y,water.height);
                    return water;
                });
            const auto propeller=storylandVcsBoatPropellerPoint(definition,frame);
            float propellerHeight=0,centerHeight=0;
            const bool propellerWater=storylandNativeWaterHeight(world,propeller[0],propeller[1],propellerHeight);
            const bool centerWater=storylandNativeWaterHeight(world,frame[12],frame[13],centerHeight);
            const auto force=storylandVcsBoatPropellerForce(definition,frame,velocity,
                contact.drowningDriver?0:vehicle.throttle,vehicle.steeringAngle*radians,input.handbrake,
                vehicle.mass,nativeStep,propellerWater,propellerHeight,centerWater,centerHeight);
            for(size_t axis=0;axis<2;++axis)velocity[axis]+=force[axis]/vehicle.mass;
            if(contact.strongContact)velocity=storylandVcsBoatResistedVelocity(definition,frame,velocity,
                contact.submergedVolume,vehicle.mass,vehicle.vcsJetSkiAxes[0],input.handbrake,nativeStep);
            // Preserve the existing vertical body and collision integration.
            vehicle.speed=(c*velocity[0]+s*velocity[1])*StorylandNativePhysicsRate;
            vehicle.lateralSpeed=(-s*velocity[0]+c*velocity[1])*StorylandNativePhysicsRate;
        } else if(nativeTyres) {
            if(!vehicle.contactsValid)updateVehicleWheelContacts(vehicle,car,world,0);
            if(nativePedals&&!vehicle.nativeDefinition.twoWheels()) {
                const auto& definition=vehicle.nativeDefinition;
                const float nativeTraction=definition.traction*.001f*vehicle.tractionMultiplier/vehicle.massMultiplier;
                const bool rearSteering=StorylandVcsRetailQuirks::rearSteeringEnabled(
                    definition.vcsModelLayout,definition.handlingFlags,handbrake);
                const bool steeringContact=vehicle.contactsValid&&
                    (definition.vcsModelLayout?
                        StorylandVcsVehicleContactTimers::steeringContact(
                            vehicle.wheelContactTimers,rearSteering):
                        (vehicle.wheelContacts[0]||vehicle.wheelContacts[2]));
                vehicle.steeringAngle=storylandRetailAutomobileSteeringLimit(vehicle.steeringAngle,
                    vehicle.speed,vehicle.lateralSpeed,definition.steeringLock,nativeTraction,
                    definition.tyreAdhesion[VehicleContact::adhesionGroup(1)],steeringContact,handbrake);
            }
            const bool nativeContactRetention=vehicle.nativeDefinition.vcsModelLayout&&
                vehicle.nativeDefinition.nativeType==0&&vehicle.contactsValid;
            const bool nativeBikeRetention=vehicle.nativeDefinition.motorcycle()&&vehicle.contactsValid;
            const bool drivenWheelsOnGround=nativeContactRetention?
                StorylandVcsVehicleContactTimers::contactCounts(vehicle.wheelContactTimers,
                    vehicle.nativeDefinition.driveType).driven>0:
                nativeBikeRetention?StorylandVcsVehicleContactTimers::bikeRearWheelContact(
                    vehicle.wheelContactTimers):
                storylandVehicleDriveGrip(vehicle.nativeDefinition,vehicle.wheelContacts,
                    vehicle.grounded,vehicle.contactsValid)>0;
            if(!nativePedals&&(handbrake||braking))vehicle.throttle=0;
            const float drive=storylandVehicleTransmissionDrive(vehicle.nativeDefinition,vehicle.transmission,
                vehicle.speed,vehicle.throttle,dt,drivenWheelsOnGround)/vehicle.massMultiplier;
            const float brake=storylandRetailVehicleBrakeDelta(vehicle.nativeDefinition,
                nativePedals?pedals.brake:float(braking),dt);
            const float rain=environment?std::clamp(environment->weatherState.rain,0.0f,1.0f):0;
            applyVehicleTyreForces(vehicle,car,drive*dt,brake,rain,dt,
                nativePedals&&pedals.burnout,true,environment?environment->controllerMode:0);
            if(!nativePedals)vehicle.speed=std::clamp(vehicle.speed,-reverseMaximum,maximum);
        } else if(handbrake||braking) {
            const float deceleration=handbrake?(car.water?6.0f+std::fabs(vehicle.speed)*0.7f:
                vehicle.handlingBrake*1.4f*std::max(0.5f,traction)):
                (car.water?3.5f:vehicle.handlingBrake*std::max(0.5f,traction));
            const float brakeGrip=car.water?1.0f:storylandVehicleBrakeGrip(vehicle.nativeDefinition,
                vehicle.wheelContacts,vehicle.grounded,vehicle.suspensionAnchorValid,handbrake);
            vehicle.speed=approach(vehicle.speed,0,dt*deceleration*brakeGrip);
            vehicle.throttle=0;
        } else {
            const float speed=std::fabs(vehicle.speed);
            const float limit=vehicle.throttle>=0?maximum:reverseMaximum;
            const float torque=std::clamp(1.0f-speed/std::max(1.0f,limit)*0.8f,0.12f,1.0f);
            float drive=vehicle.throttle*engine*torque*std::min(1.3f,traction);
            if(vehicle.nativeHandling) {
                const float driveGrip=car.water?(vehicle.nativeDefinition.driveType=='4'?1.0f:.5f):storylandVehicleDriveGrip(vehicle.nativeDefinition,
                    vehicle.wheelContacts,vehicle.grounded,vehicle.nativeDefinition.vcsModelLayout?
                        vehicle.contactsValid:vehicle.suspensionAnchorValid);
                drive=storylandVehicleTransmissionDrive(vehicle.nativeDefinition,vehicle.transmission,vehicle.speed,vehicle.throttle,dt,driveGrip>0)/vehicle.massMultiplier;
                drive=std::clamp(drive,-traction*9.81f*driveGrip,traction*9.81f*driveGrip);
            }
            if(!car.water) {
                if(!vehicle.grounded)drive=0;
                else if(!vehicle.nativeDefinition.hasNativeSuspension)
                    drive-=runtime.gravity*std::sin(vehicle.roadPitch*radians);
            }
            const float drag=((car.water?0.28f:0.12f)*speed+
                (car.water?0.012f:0.002f)*speed*speed)*vehicle.handlingDrag;
            const float rolling=car.water?0.15f:0.35f;
            vehicle.speed+=drive*dt;
            if(!vehicle.nativeHandling||car.water)vehicle.speed=approach(vehicle.speed,0,(drag+rolling)*dt);
            if(!nativePedals)vehicle.speed=std::clamp(vehicle.speed,-reverseMaximum,maximum);
        }
        if(vehicle.nativeHandling&&!car.water) {
            float& vertical=vehicleVerticalVelocity(vehicle);
            const float factor=storylandRetailVehicleAirResistance(vehicle.nativeDefinition.dragMultiplier,
                std::hypot(vehicle.speed,vehicle.lateralSpeed,vertical),dt);
            vehicle.speed*=factor;vehicle.lateralSpeed*=factor;vertical*=factor;
            const float angularResistance=storylandNativeVehicleAngularResistance(
                vehicle.nativeDefinition.dragMultiplier,dt);
            vehicle.yawVelocity*=angularResistance;
            vehicle.springPitchVelocity*=angularResistance;
            vehicle.springRollVelocity*=angularResistance;
            for(float& angularSpeed:vehicle.angularVelocity)angularSpeed*=angularResistance;
        }
        vehicle.longitudinalAcceleration=(vehicle.speed-previousSpeed)/dt;
        if(!vehicle.nativeDefinition.hasNativeSuspension) {
        vehicle.suspensionVelocity+=(-vehicle.suspension*(25.0f*vehicle.handlingSuspensionForce)-
            vehicle.suspensionVelocity*(18.0f*vehicle.handlingSuspensionDamping))*dt;
        vehicle.suspension+=vehicle.suspensionVelocity*dt;
        }
        if(!vehicle.nativeDefinition.hasNativeSuspension)vehicle.bodyPitch=approach(vehicle.bodyPitch,std::clamp(-vehicle.longitudinalAcceleration*0.35f,-4.0f,4.0f),dt*10);
        if(!vehicle.nativeDefinition.hasNativeSuspension)vehicle.bodyRoll=approach(vehicle.bodyRoll,-turn*std::min(5.0f,std::fabs(vehicle.speed)*0.22f),dt*12);
        const float steer=vehicle.steeringAngle;
        const float wheelbase=storylandVehicleWheelbase(vehicle.nativeDefinition,vehicle.halfLength*1.6f);
        const float yawRate=vehicle.speed/wheelbase*std::tan(steer*radians)/radians;
        recoverVehicle(world,car,vehicle);
        const auto previousCar=car;
        float desiredYawRate=std::clamp(yawRate,-75.0f,75.0f)*(handbrake&&!car.water?1.2f:1.0f);
        const float contactGrip=car.water?1.0f:storylandVehicleContactGrip(vehicle.nativeDefinition,
            vehicle.wheelContacts,vehicle.grounded,vehicle.suspensionAnchorValid);
        if(!nativeTyres&&vehicle.nativeHandling) {
            const float gripRate=traction*9.81f*contactGrip/std::max(1.0f,std::fabs(vehicle.speed))/radians;
            desiredYawRate=contactGrip>0?std::clamp(desiredYawRate,-gripRate,gripRate):vehicle.yawVelocity;
            const float inertia=std::max(100.0f,vehicle.nativeDefinition.turnMass*vehicle.turnMassMultiplier*(vehicle.heavy?5.0f:1.0f));
            const float response=std::clamp(vehicle.mass*wheelbase*wheelbase/inertia,1.0f,20.0f);
            vehicle.yawVelocity+=(desiredYawRate-vehicle.yawVelocity)*(1-std::exp(-dt*response*contactGrip));
        } else if(!nativeTyres&&contactGrip>0)vehicle.yawVelocity=desiredYawRate;
        const float yawChange=vehicle.yawVelocity*dt;
        car.yaw+=yawChange;
        if(!car.water) {
            const float c=std::cos(yawChange*radians),s=std::sin(yawChange*radians);
            const float forward=vehicle.speed,lateral=vehicle.lateralSpeed;
            vehicle.speed=forward*c+lateral*s;
            vehicle.lateralSpeed=-forward*s+lateral*c;
        }
        if(!nativeTyres&&handbrake&&!car.water&&contactGrip>0&&std::fabs(vehicle.speed)>3)
            vehicle.lateralSpeed+=turn*vehicle.speed*0.45f*dt;
        if(!nativeTyres&&!nativeBoatPropulsion&&(car.water||vehicle.grounded)) {
            if(vehicle.nativeHandling&&!car.water) {
                const float lateralGrip=traction*9.81f*contactGrip*vehicle.handlingTractionLoss*(handbrake?.35f:1.0f);
                vehicle.lateralSpeed=approach(vehicle.lateralSpeed,0,dt*lateralGrip);
            } else vehicle.lateralSpeed*=std::exp(-dt*traction*contactGrip*vehicle.handlingTractionLoss*
                (handbrake?0.8f:4.5f));
        }
        StorylandNativePoint candidate{car.position.x+(std::cos(car.yaw*radians)*vehicle.speed-
            std::sin(car.yaw*radians)*vehicle.lateralSpeed)*dt,
            car.position.y+(std::sin(car.yaw*radians)*vehicle.speed+
            std::cos(car.yaw*radians)*vehicle.lateralSpeed)*dt,car.position.z};
        float support=0;
        if(!car.water&&!vehicle.nativeDefinition.hasNativeSuspension&&
            vehicleGroundHeight(world,candidate.x,candidate.y,car.position.z,support)&&
            std::fabs(support-car.position.z)<0.85f)candidate.z=support;
        const auto volumeMove=storylandMoveNativeBody(world,previousCar.position,candidate,supportedVehicleBody(world,vehicle,car));
        if(volumeMove.clear)candidate=volumeMove.position;
        auto candidateCar=car;candidateCar.position=candidate;
        StorylandNativePoint collisionPoint;
        auto worldContact=volumeMove.contact;
        StorylandNativeContact pairContact;
        const float requestedTravel=std::hypot(vehicle.speed,vehicle.lateralSpeed)*dt;
        const float resolvedTravel=std::hypot(candidate.x-previousCar.position.x,candidate.y-previousCar.position.y);
        const bool worldBlocked=!volumeMove.clear||
            (volumeMove.blocked&&resolvedTravel<requestedTravel*0.15f)||
            vehicleWorldBlocked(world,previousCar,candidateCar,vehicle,&collisionPoint,&worldContact);
        bool blocked=worldBlocked,playerCollision=false;
        int32_t collidedVehicle=-1;
        const float forwardX=std::cos(car.yaw*radians),forwardY=std::sin(car.yaw*radians);
        const float nose=vehicle.speed>=0?vehicle.halfLength:-vehicle.halfLength;
        const int steps=std::max(1,int(std::ceil(distance(previousCar.position,candidate)/0.2f)));
        float lastSafeFraction=0.0f;
        auto impactCar=car;
        for(int step=1;step<=steps;++step) {
            auto sample=candidateCar;const float t=float(step)/steps;
            sample.position.x=previousCar.position.x+(candidate.x-previousCar.position.x)*t;
            sample.position.y=previousCar.position.y+(candidate.y-previousCar.position.y)*t;
            sample.position.z=previousCar.position.z+(candidate.z-previousCar.position.z)*t;
            sample.yaw=previousCar.yaw+std::remainder(car.yaw-previousCar.yaw,360.0f)*t;
            bool collision=false;
            for(size_t other=0;other<cars.size()&&other<runtime.vehicles.size();++other) {
                if(int(other)==controlledVehicle||!runtime.vehicles[other].collisionEnabled||
                    !vehicle.collisionEnabled)continue;
                if(!overlapVehicles(sample,vehicle,cars[other],runtime.vehicles[other],&pairContact))continue;
                blocked=true;
                collision=true;
                collidedVehicle=int32_t(other);
                playerCollision=int(other)==runtime.occupiedVehicle;
                vehicle.lastCollidedSlot=int32_t(other);
                runtime.vehicles[other].lastCollidedSlot=controlledVehicle;
                vehicle.pendingContactSlot=int32_t(other);
                runtime.vehicles[other].pendingContactSlot=controlledVehicle;
                impactCar=sample;
                break;
            }
            if(collision)break;
            lastSafeFraction=t;
        }
        float waterHeight=car.position.z;
        if(car.water && (!storylandNativeWaterHeight(world,candidate.x,candidate.y,waterHeight)||
            !storylandNativeWaterSegment(world,car.position,{candidate.x,candidate.y,waterHeight}))) blocked=true;
        if(car.water && !blocked) {
            for(float side:{-vehicle.halfWidth,0.0f,vehicle.halfWidth}) {
                const StorylandNativePoint start{car.position.x-forwardY*side,
                    car.position.y+forwardX*side,car.position.z};
                const StorylandNativePoint end{candidate.x-forwardY*side+forwardX*nose,
                    candidate.y+forwardX*side+forwardY*nose,waterHeight};
                if(!storylandNativeWaterSegment(world,start,end)) {blocked=true;break;}
                if(storylandTraceNativeSegment(world,{end.x,end.y,waterHeight+0.3f},
                    {end.x,end.y,waterHeight-0.5f})<1) {blocked=true;break;}
            }
        }
        const bool hitWorld=worldBlocked||(volumeMove.blocked&&worldContact.valid&&collidedVehicle<0);
        if(blocked||hitWorld) {
            const auto movingCar=car;
            float bikeDeltaSpeed=0;
            float impact=std::fabs(vehicle.speed);
            if(hitWorld&&worldContact.valid) {
                const auto hit=applyVehicleWorldImpulse(vehicle,movingCar,worldContact);
                impact=hit.closingSpeed;
                bikeDeltaSpeed=hit.impulse/std::max(1.0f,vehicle.mass);
            } else if(collidedVehicle>=0&&!hitWorld) {
                impact=transferVehicleContact(runtime,impactCar,vehicle,cars[size_t(collidedVehicle)],
                    runtime.vehicles[size_t(collidedVehicle)],pairContact,&bikeDeltaSpeed);
            }
            if(blocked) {
                auto safeCar=previousCar;
                if(!worldBlocked&&collidedVehicle>=0&&lastSafeFraction>0.0f) {
                    safeCar.position.x+=(candidate.x-previousCar.position.x)*lastSafeFraction;
                    safeCar.position.y+=(candidate.y-previousCar.position.y)*lastSafeFraction;
                    safeCar.position.z+=(candidate.z-previousCar.position.z)*lastSafeFraction;
                    safeCar.yaw+=std::remainder(movingCar.yaw-previousCar.yaw,360.0f)*lastSafeFraction;
                    const auto clip=storylandMoveNativeBody(world,previousCar.position,safeCar.position,
                        supportedVehicleBody(world,vehicle,safeCar));
                    if(!clip.clear||clip.blocked||distance(clip.position,safeCar.position)>.03f)
                        safeCar=previousCar;
                }
                const float turn=(movingCar.yaw-safeCar.yaw)*radians;
                const float c=std::cos(turn),s=std::sin(turn);
                const float speed=vehicle.speed,lateral=vehicle.lateralSpeed;
                vehicle.speed=c*speed-s*lateral;vehicle.lateralSpeed=s*speed+c*lateral;
                car=safeCar;
            }
            const bool damagingImpact=impact>1.0f&&vehicle.impactCooldown<=0;
            const auto& contact=hitWorld?worldContact:pairContact;
            vehicleImpact(runtime,previousCar,vehicle,impact,contact.valid?&contact.point:
                hitWorld?&collisionPoint:nullptr);
            if(!contact.valid)vehicle.suspensionVelocity+=std::min(3.5f,impact*0.23f);
            const float damage=!damagingImpact?0.0f:vehicle.nativeDefinition.vcsModelLayout?
                storylandVehicleCollisionDamage(vehicle,impact*5,true):
                (vehicle.canBeDamaged&&!(vehicle.damageProofs&8u)&&
                    storylandLcsVehicleDamageSourceAllowed(runtime,size_t(controlledVehicle),playerCollision)?impact*5:0);
            vehicle.health = std::max(0.0f, vehicle.health - damage);
            if (damage > 0) {
                vehicle.visiblyDamaged = true;
                if (vehicle.doorDamage[0] == 0) vehicle.doorDamage[0] = 1;
            }
            const auto bikeFall=remote?StorylandBikeFallDecision{}:vehicleBikeImpact(vehicle,movingCar,
                hitWorld?worldContact:pairContact,bikeDeltaSpeed,true);
            storylandEjectVcsMissionBikeRiders(runtime,movingCar,vehicle,bikeFall);
            if(bikeFall.eject) {
                ejectBikeRider(runtime,player,heading,cars,size_t(controlledVehicle),bikeFall);
                if(worldBlocked&&!worldContact.valid) {vehicle.speed=0;vehicle.lateralSpeed=0;vehicle.yawVelocity=0;}
                return;
            }
            if(runtime.collisionOccupantDamage&&damagingImpact&&!remote&&impact>8&&(!runtime.missionEnvironment||!(runtime.missionEnvironment->playerDamageProofs&8))) runtime.damage((impact-8)*2);
            if(worldBlocked&&!worldContact.valid) {vehicle.speed=0;vehicle.lateralSpeed=0;vehicle.yawVelocity=0;}
        }
        if(!blocked) {
            if(car.water)applyVehicleWater(vehicle,candidate,world,dt,runtime.gravity,true,car.yaw,nativeBoatPropulsion);
            else {
                const auto beforeGravity=candidate;
                applyVehicleGravity(vehicle,candidate,world,dt,runtime.gravity,car.yaw);
                auto groundedCar=car;groundedCar.position=candidate;
                const auto settled=storylandMoveNativeBody(world,beforeGravity,candidate,supportedVehicleBody(world,vehicle,groundedCar));
                resolveVehicleBodyMovement(vehicle,groundedCar,beforeGravity,settled);
                candidate=groundedCar.position;
            }
            car.position=candidate;
        }
        const auto road=supportedVehicleBody(world,vehicle,car);
        updateVehicleRoadReference(vehicle,road.pitch,-road.roll);
        updateVehicleWheelContacts(vehicle,car,world,dt,!remote,runtime.gravity);
        if(!remote) {
            auto fall=vehicleBikeSpin(vehicle,car);
            if(runtime.crime.vcs&&vehicle.nativeDefinition.twoWheels()&&vehicle.water.engineFlooded&&
                std::all_of(vehicle.wheelLineFractions.begin(),vehicle.wheelLineFractions.end(),
                    [](float fraction){return fraction==1.0f;})) {
                fall={};fall.eject=true;fall.animation=139;fall.inheritVelocity=.2f;fall.nativeWaterFall=true;
            }
            if(fall.eject) {ejectBikeRider(runtime,player,heading,cars,size_t(controlledVehicle),fall);return;}
        }
        refreshMovementCars(runtime,cars);
        if(remote) {runtime.movementSpeed=0;return;}
        float headWater=0;
        if(!vehicle.nativeDefinition.vcsModelLayout&&
            storylandNativeWaterHeight(world,car.position.x,car.position.y,headWater)&&
            headWater>car.position.z+vehicle.bodyHeight) {
            runtime.playerWater.air=std::max(0.0f,runtime.playerWater.air-dt);
            if(runtime.playerWater.air<=0)runtime.inflictDamage(20*dt,0,
                storylandNativeDrowningWeapon(runtime.crime.vcs),false);
        } else if(!runtime.crime.vcs)runtime.playerWater={};
        player=car.position;heading=car.yaw;return;
    }
    float surface=0;
    if(!runtime.extendedMovement.climbing() && deepCharacterWater(world,player,surface)) {
        runtime.jumpLaunching=false;runtime.jumpHorizontalValid=false;runtime.jumpFromLaunch=false;runtime.jumpLaunchTime=0;
        runtime.playerFall={};runtime.landingTime=0;
        runtime.extendedMovement.cancel();
        runtime.playerStopVariant=-1;runtime.playerStopTime=0;
        const bool moving=input.forward||input.backward||input.left||input.right;
        const float surfaceSpeed=runtime.playerCanSwim&&moving?(input.sprinting?1.8f:.9f):0;
        const float previousVertical=runtime.playerWater.verticalSpeed;
        const float waterEntryVelocity=runtime.playerWater.active?previousVertical:runtime.verticalSpeed;
        const bool divingAllowed=world.paths.vcs&&runtime.extendedAndSaFeatures&&
            runtime.playerDivingAvailable&&bool(runtime.playerDiveRootDistance);
        if(divingAllowed&&runtime.playerDiveDown&&!runtime.playerWater.diving) {
            runtime.playerWater.diving=true;runtime.playerWater.diveTime=0;
            runtime.playerWater.diveAnimationTime=0;runtime.playerWater.divePitch=-35*radians;
        }
        const auto water=storylandStepWaterCharacter(runtime.playerWater,player.z,surface,true,
            runtime.playerCanSwim,input.sprinting,false,dt,world.paths.vcs,
            runtime.playerWater.diving?runtime.movementSpeed:surfaceSpeed,0,
            !environment||environment->playerDrownsInWater,
            environment&&environment->playerInfiniteSwim);
        if(water.entered) {
            runtime.soundEvents.emplace_back(StorylandPlayTestSound::WaterSplash);
            if(runtime.extendedAndSaFeatures && world.paths.vcs) {
                runtime.playerCameraWaterDroplets=0.85f;
                ++runtime.playerCameraWaterSerial;
            }
        }
        storylandClearPlayTestTarget(runtime);runtime.playerMelee={};
        runtime.inflictDamage(water.damage,0,storylandNativeDrowningWeapon(runtime.crime.vcs),false);
        const auto direction=storylandMovePlayTestPlayer(player.x,player.y,heading,cameraYaw,input,dt);
        float nextHeight=water.height;
        if(runtime.playerWater.diving&&!runtime.playerWater.drowning) {
            auto& state=runtime.playerWater;
            const float nativeStep=dt*StorylandNativePhysicsRate;
            const float retention=std::pow(.9f,nativeStep);
            state.diveTime+=dt;
            state.diveStroke=divingAllowed&&input.sprinting;
            const float before=state.diveAnimationTime;
            state.diveAnimationTime+=dt;
            const float distance=runtime.playerDiveRootDistance?
                runtime.playerDiveRootDistance(before,state.diveAnimationTime):0;
            const float strokeSpeed=state.diveStroke&&std::isfinite(distance)&&distance>=0?distance/dt:0;
            if(divingAllowed) {
                state.divePitch+=dt*(float(runtime.playerDiveUp)-float(runtime.playerDiveDown));
                state.divePitch+=nativeStep/1000.0f;
                state.divePitch=std::clamp(state.divePitch,-80*radians,80*radians);
            }
            float targetVertical=std::sin(state.divePitch)*strokeSpeed+.01f*StorylandNativePhysicsRate;
            if(state.diveTime<64.0f/60.0f&&divingAllowed)
                targetVertical=-.1f*StorylandNativePhysicsRate*(state.diveTime/(64.0f/60.0f));
            if(!divingAllowed)targetVertical=.05f*StorylandNativePhysicsRate;
            state.verticalSpeed=previousVertical*retention+targetVertical*(1-retention);
            runtime.movementSpeed=runtime.movementSpeed*retention+
                std::cos(state.divePitch)*strokeSpeed*(1-retention);
            nextHeight=player.z+state.verticalSpeed*dt;
            state.underwater=nextHeight<surface-1.15f;
            if(nextHeight>=surface-1.0f&&state.verticalSpeed>=0&&
                (!divingAllowed||runtime.playerDiveUp||state.diveTime>=64.0f/60.0f)) {
                nextHeight=surface-1.0f;state.diving=false;state.underwater=false;
                state.diveTime=0;state.diveAnimationTime=0;state.divePitch=0;state.verticalSpeed=0;
            }
        } else runtime.movementSpeed=runtime.playerCanSwim&&!runtime.playerWater.drowning?surfaceSpeed:0;
        if(moving)heading=direction.heading;
        const float current = world.paths.vcs && environment ? environment->vcsSwimmingBoundary.velocity : 0;
        const StorylandNativePoint next{player.x+(std::cos(heading*radians)*runtime.movementSpeed+current)*dt,
            player.y+std::sin(heading*radians)*runtime.movementSpeed*dt,nextHeight};
        const auto movement=storylandMovePlayTestCharacter(world,runtime,player,next);
        if(movement.clear)player=movement.position;
        if(movement.blocked)runtime.playerWater.verticalSpeed=0;
        storylandUpdateSwimmingParticles(runtime,runtime.playerWater,player,surface,heading,
            runtime.movementSpeed,waterEntryVelocity,dt,water.entered,runtime.health>0);
        runtime.verticalSpeed=0;runtime.grounded=false;runtime.airborneTime=0;
        if(jump&&!runtime.playerWater.drowning&&!runtime.playerWater.diving) {
            StorylandNativePoint bank{player.x+std::cos(heading*radians)*.8f,
                player.y+std::sin(heading*radians)*.8f,surface+.6f};
            uint8_t floorSurface=0;
            const StorylandNativePoint bottom{bank.x,bank.y,surface-.7f};
            const float hit=storylandTraceNativeGround(world,bank,bottom,floorSurface);
            if(hit<1) {
                bank.z=bank.z+(bottom.z-bank.z)*hit;
                const StorylandNativePoint raised{player.x,player.y,bank.z};
                const auto lift=storylandMoveNativeBody(world,player,raised,{});
                const auto climb=storylandMovePlayTestCharacter(world,runtime,raised,bank);
                if(lift.clear&&!lift.blocked&&climb.clear&&!climb.blocked) {
                    player=climb.position;runtime.playerWater={};runtime.grounded=true;
                    runtime.landingTime=.35f;runtime.groundSurface=floorSurface;
                }
            }
        }
        runtime.playerPosition=player;
        return;
    }
    storylandStepWaterCharacter(runtime.playerWater,player.z,0,false,true,false,false,dt,world.paths.vcs);
    auto& extended = runtime.extendedMovement;
    const bool extendedAllowed = (world.paths.vcs ? runtime.extendedAndSaFeatures : runtime.extendedLcsCrouch) &&
        !vcsRemote && !remote && !scriptedWalk && !scriptedWait &&
        !manualMovementLock && runtime.playerControl &&
        runtime.vehicleAction == StorylandVehicleAction::None &&
        runtime.entryTransition < 0 && !runtime.busted &&
        !runtime.wastedActive && runtime.playerReaction.action == StorylandPlayerReaction::None;
    if (!extendedAllowed) {
        if (extended.busy()) {
            if(extended.climbing()) {
                extended.climbDropRequested=true;
                storylandStepExtendedMovement(extended,runtime,world,player,heading,dt);
            } else extended.cancel();
        }
        // Turning the master feature off must never stand Victor up through a
        // low roof. Keep the crouched collision/pose until there is headroom.
        if (!(world.paths.vcs ? runtime.extendedAndSaFeatures : runtime.extendedLcsCrouch) && extended.crouched &&
            storylandNativeBodyClear(world, player, storylandNativeCharacterBody()))
            extended.crouched = false;
    } else {
        const bool crouchDown = extended.crouchPressed;
        if (crouchDown && !extended.crouchHeld && !extended.busy() &&
            runtime.grounded && !runtime.jumpLaunching && runtime.landingTime <= 0) {
            if (extended.crouched || storylandNativeBodyClear(world, player,
                    storylandNativeCharacterBody()))
                extended.crouched = !extended.crouched;
            extended.crouchMoveAmount = 0.0f;
            extended.crouchMoveCommand = 0.0f;
            extended.crouchMoveBlend = 0.0f;
            extended.crouchAnimationTime = 0.0f;
            extended.crouchTravelDistance = 0.0f;
            extended.crouchMotionDirection = 0;
        }
        extended.crouchHeld = crouchDown;
        if (extended.crouched && (jump || input.sprinting)) {
            if (storylandNativeBodyClear(world, player, storylandNativeCharacterBody()))
                extended.crouched = false;
            else jump = false;
        }
        extended.leftHeld = input.left;
        extended.rightHeld = input.right;
        const bool crouchRollRequested=std::fabs(input.forwardAmount())<.1f&&
            std::fabs(input.horizontal())>=.9f;
        if (world.paths.vcs && extended.crouched && runtime.playerAiming && runtime.grounded &&
            runtime.landingTime <= 0 && !extended.busy() && crouchRollRequested && runtime.extendedRollRootDelta) {
            extended.action = input.horizontal()<0 ? StorylandExtendedAction::CrouchRollLeft :
                StorylandExtendedAction::CrouchRollRight;
            extended.elapsed = 0.0f;
            extended.crouchMoveCommand=extended.crouchMoveAmount=extended.crouchMoveBlend=0;
            // SA sideways rolls return to a crouch, not an unrequested stand.
            extended.crouched = true;
        }
        // SA's jump-glide scan remains active while vertical speed is above
        // -0.2 native units and horizontal speed exceeds 0.05 (50 Hz units).
        // It does not stop after an arbitrary elapsed-jump time or require
        // forward to remain held once the launch momentum is established.
        const bool airborneGrab=runtime.jumpFromLaunch && !runtime.grounded &&
            runtime.verticalSpeed > -10.0f && runtime.jumpHorizontalValid &&
            std::hypot(runtime.jumpHorizontalVelocity[0],runtime.jumpHorizontalVelocity[1])>2.5f;
        const bool groundedGrab=jump && runtime.grounded &&
            !runtime.jumpLaunching;
        if (world.paths.vcs && (airborneGrab || groundedGrab) && !extended.busy() &&
            !extended.crouched && runtime.landingTime <= 0 &&
            storylandTryStartClimb(extended,world,player,heading,!runtime.grounded)) {
            jump=false;
            runtime.jumpLaunching=false;
            runtime.jumpHorizontalValid=false;
            runtime.jumpFromLaunch=false;
            runtime.playerFall={};
        }
        if (extended.busy()) {
            if(extended.climbing()) {
                extended.climbPullRequested=jump;
                extended.climbDropRequested=interact;
                heading=extended.climbHeading;
            }
            if (storylandStepExtendedMovement(extended, runtime, world, player, heading, dt)) {
                runtime.jumpLaunching = false;
                runtime.jumpHorizontalValid = false;
                runtime.jumpFromLaunch = false;
                runtime.playerPosition = player;
                return;
            }
        }
        if (extended.crouched) {
            input.running = false;
            input.sprinting = false;
        } else {
            extended.crouchMoveAmount = 0.0f;
            extended.crouchMoveCommand = 0.0f;
            extended.crouchMoveBlend = 0.0f;
            extended.crouchAnimationTime = 0.0f;
            extended.crouchTravelDistance = 0.0f;
            extended.crouchMotionDirection = 0;
        }
    }
    extended.crouchStanceBlend=storylandSaCrouchAssociationWeight(
        extended.crouchStanceBlend,extended.crouched,dt,4.0f,4.0f);
    const bool aimLocomotion = storylandPlayTestAimKeepsHeading(runtime);
    if(aimLocomotion) {
        // PC free aim can use the ordinary run gait while keeping the camera
        // heading. A native target or manual aim still uses aimed walking.
        if(!runtime.freeAimActive)input.running=false;
        input.sprinting=false;
        runtime.playerStopVariant=-1;runtime.playerStopTime=0;
    }
    bool moving=input.forward||input.backward||input.left||input.right;
    const bool sprintWeaponAllowed=runtime.equippedWeapon>=runtime.weapons.size()||
        (runtime.weapons[runtime.equippedWeapon].nativeFlags&0x200u)==0;
    if(moving&&input.sprinting&&runtime.equippedWeapon<runtime.weapons.size()&&
        runtime.weapons[runtime.equippedWeapon].fireType!=0&&sprintWeaponAllowed) {
        // Retail releases a firearm lock on entering sprint (VCS0x2364A8,
        // LCS0x34B598), without fabricating a fresh target press afterward.
        const bool held=runtime.targetLockRequested;
        storylandClearPlayTestTarget(runtime);runtime.targetLockRequested=held;
    }
    if(storylandPlayTestAimBlocksMovement(runtime)) {
        input.forward=input.backward=input.left=input.right=false;
        moving=false;runtime.movementSpeed=0;
    }
    if(environment) {
        const float minimumEnergy=world.paths.vcs?-100.0f:-150.0f;
        const bool enoughStick=!input.analogMovement||
            (std::fabs(input.horizontal())+std::fabs(input.forwardAmount()))*128.0f>=65.0f;
        int requested=moving&&input.sprinting&&enoughStick&&sprintWeaponAllowed?5:1;
        bool startedWalk=false;
        if(ordinaryFootPose&&moving&&!aimLocomotion&&!extended.crouched&&
            runtime.playerMoveBlend<=0&&runtime.playerSprintBlend<=0&&
            runtime.playerStopVariant<0&&!sprintState.walkStartRunning()&&environment->animationDuration) {
            const float duration=environment->animationDuration(0,world.paths.vcs?7:4);
            if(std::isfinite(duration)&&duration>0) {
                sprintState.walkStartTime=0;
                sprintState.walkStartDuration=duration;
                startedWalk=true;
            }
        }
        if(!moving)sprintState.walkStartTime=-1;
        if(startedWalk) {
            requested=2;
            sprintState.tiredTime=-1;
            sprintState.tiredBlend=0;
            sprintState.tiredBlendDelta=0;
        }
        sprintState.movementState=StorylandSprint::moveState(requested,
            environment->playerSprintEnergy,sprintState.association,
            sprintState.blendDelta<0,sprintState.walkStartRunning(),minimumEnergy);
        const float gaitTarget=moving&&runtime.playerCoughTime<=0?storylandPedMoveTarget(
            input.movementAmount(),input.running||sprintState.movementState==5,input.analogMovement):0;
        const float gaitRatio=storylandStepPedMoveBlend(runtime.playerMoveBlend,gaitTarget,dt);
        sprintState.movementState=StorylandSprint::finishMoveState(sprintState.movementState,
            sprintState.association,sprintState.walkStartRunning(),gaitRatio);
        input.sprinting=sprintState.movementState==5;
        const bool drain=StorylandSprint::controlBlend(sprintState,runtime.playerSprintBlend,gaitRatio);
        runtime.playerSprintFading=sprintState.blendDelta<0;
        if(drain)StorylandSprint::drain(environment->playerSprintEnergy,
            environment->playerSprintMaximum,environment->playerSprintTraining,
            dt*StorylandNativePhysicsRate,environment->playerNeverTired,
            storylandAdrenalineRemainingMilliseconds(runtime)>0,minimumEnergy);
        bool tired=ordinaryFootPose&&!moving&&runtime.playerMoveBlend<=0&&
            !aimLocomotion&&!extended.crouched&&!scriptedWait&&
            (environment->playerSprintEnergy<0||
                (environment->playerSprintEnergy==0&&sprintState.tiredTime>=0));
        if(tired) {
            float origin=1.04f;
            if(environment->characterOriginOffset) {
                float decoded=0;
                if(environment->characterOriginOffset(0,decoded)&&std::isfinite(decoded))origin=decoded;
            }
            bool blocked=false;
            storylandNativeBuildingSphereContact(world,{player.x,player.y,player.z+origin},.5f,blocked);
            tired=!blocked;
        }
        StorylandSprint::controlTired(sprintState,tired,runtime.playerStopVariant>=0?8.0f:4.0f);
    }
    runtime.playerSprintActive = moving && input.sprinting;
    if(runtime.missionEnvironment&&!runtime.missionEnvironment->activeVcsCharacterAttachment(0)&&
        !runtime.missionEnvironment->camera.active&&!runtime.missionEnvironment->cutscene.playing) {
        const int moveState=moving?(input.sprinting?5:input.running?4:2):1;
        storylandObserveCrouchMovement(runtime.missionEnvironment->playerCrouch,
            runtime.missionEnvironment->gameMilliseconds,moveState,
            storylandCrouchWeaponAnimation(runtime,0));
    }
    const bool landingMotion=world.paths.vcs&&runtime.playerFall.landingAnimationId>=0&&
        runtime.landingTime>0;
    StorylandNativePoint landingRootDelta;
    runtime.landingTime=std::max(0.0f,runtime.landingTime-dt);
    if(landingMotion) {
        auto& fall=runtime.playerFall;
        const float begin=fall.landingAnimationTime;
        fall.landingAnimationTime=std::min(fall.landingAnimationDuration,begin+dt);
        runtime.landingTime=std::max(0.0f,fall.landingAnimationDuration-fall.landingAnimationTime);
        if(runtime.reactionBindings.rootDelta)
            landingRootDelta=runtime.reactionBindings.rootDelta(fall.landingAnimationId,0,
                begin,fall.landingAnimationTime);
    } else if(world.paths.vcs&&runtime.playerFall.landingAnimationId>=0&&runtime.landingTime<=0) {
        runtime.playerFall={};
    }
    const auto movement=storylandMovePlayTestPlayer(player.x,player.y,heading,cameraYaw,input,dt);
    const float horizontalHeading=aimLocomotion&&moving?movement.heading:heading;
    if(jumpPressed&&jump&&runtime.grounded&&!runtime.jumpLaunching&&runtime.landingTime<=0&&!landingMotion) {
        // Capture a real sprint at launch. Releasing the sprint button during
        // the launch animation must not remove the earned airborne impulse.
        extended.sprintJumpBoost=storylandExtendedSprintJumpEligible(world.paths.vcs,
            extendedAllowed,runtime.playerSprintActive,runtime.playerSprintBlend,
            runtime.movementSpeed,runtime.runSpeed);
        runtime.jumpLaunching=true;runtime.jumpFromLaunch=true;runtime.jumpLaunchTime=0;
        runtime.playerFall={};
        runtime.playerStopVariant=-1;runtime.playerStopTime=0;
        runtime.jumpHorizontalVelocity={std::cos(horizontalHeading*radians)*runtime.movementSpeed,
            std::sin(horizontalHeading*radians)*runtime.movementSpeed};
        runtime.jumpHorizontalValid=true;
    }
    const bool launchMotion=runtime.jumpLaunching;
    float verticalSeconds=dt,launchSeconds=0;
    StorylandNativePoint launchRootDelta;
    if(launchMotion) {
        const float begin=runtime.jumpLaunchTime;
        verticalSeconds=storylandAdvanceJumpLaunch(runtime.jumpLaunchTime,runtime.jumpLaunchDuration,
            dt,runtime.jumpLaunching);
        launchSeconds=dt-verticalSeconds;
        if(world.paths.vcs&&runtime.reactionBindings.rootDelta&&launchSeconds>0) {
            launchRootDelta=runtime.reactionBindings.rootDelta(136,0,begin,
                std::min(runtime.jumpLaunchTime,runtime.jumpLaunchDuration));
            const float forward=heading*radians;
            runtime.jumpHorizontalVelocity={
                (std::cos(forward)*launchRootDelta.x+std::sin(forward)*launchRootDelta.y)/launchSeconds,
                (std::sin(forward)*launchRootDelta.x-std::cos(forward)*launchRootDelta.y)/launchSeconds};
        } else {
            launchRootDelta={runtime.jumpHorizontalVelocity[0]*launchSeconds,
                runtime.jumpHorizontalVelocity[1]*launchSeconds,0};
        }
        if(!runtime.jumpLaunching) {
            const float run=runtime.jumpRunBlend>=0?runtime.jumpRunBlend:
                std::clamp(runtime.playerMoveBlend-1,0.0f,1.0f)*(1-runtime.playerSprintBlend);
            const float sprint=runtime.jumpSprintBlend>=0?runtime.jumpSprintBlend:runtime.playerSprintBlend;
            const auto& horizontal=runtime.jumpHorizontalVelocity;
            const float jumpHeading=!world.paths.vcs&&std::hypot(horizontal[0],horizontal[1])>0.0001f?
                std::atan2(horizontal[1],horizontal[0])/radians:heading;
            runtime.jumpHorizontalVelocity=storylandPedJumpHorizontal(runtime.jumpHorizontalVelocity[0],
                runtime.jumpHorizontalVelocity[1],jumpHeading,run,sprint);
            runtime.jumpHorizontalValid=true;
            const float nativeJump=storylandPedJumpImpulse(true,runtime.playerMass);
            runtime.verticalSpeed+=storylandExtendedSprintJumpImpulse(nativeJump,
                extended.sprintJumpBoost&&runtime.extendedAndSaFeatures&&world.paths.vcs);
            runtime.grounded=false;runtime.airborneTime=0;
            runtime.soundEvents.emplace_back(StorylandPlayTestSound::Jump);
        }
    }
    const bool airborne=!runtime.grounded;
    StorylandNativePoint candidate=player;
    bool crouchAdvance=false;
    if(landingMotion) {
        heading=runtime.playerFall.landingHeading;
        const float facing=heading*radians;
        if(std::isfinite(landingRootDelta.x)&&std::isfinite(landingRootDelta.y)) {
            candidate.x+=std::cos(facing)*landingRootDelta.x+std::sin(facing)*landingRootDelta.y;
            candidate.y+=std::sin(facing)*landingRootDelta.x-std::cos(facing)*landingRootDelta.y;
        }
        runtime.movementSpeed=std::hypot(candidate.x-player.x,candidate.y-player.y)/dt;
        runtime.playerSprintActive=false;
        if(airborne)runtime.jumpHorizontalVelocity={(candidate.x-player.x)/dt,(candidate.y-player.y)/dt};
    } else if(launchMotion) {
        if(world.paths.vcs&&runtime.reactionBindings.rootDelta) {
            const float facing=heading*radians;
            candidate.x+=std::cos(facing)*launchRootDelta.x+std::sin(facing)*launchRootDelta.y;
            candidate.y+=std::sin(facing)*launchRootDelta.x-std::cos(facing)*launchRootDelta.y;
        } else {
            candidate.x+=launchRootDelta.x;
            candidate.y+=launchRootDelta.y;
        }
        candidate.x+=runtime.jumpHorizontalVelocity[0]*verticalSeconds;
        candidate.y+=runtime.jumpHorizontalVelocity[1]*verticalSeconds;
        runtime.movementSpeed=std::hypot(candidate.x-player.x,candidate.y-player.y)/dt;
    } else if(airborne) {
        // Input release must not cancel the horizontal velocity of an airborne ped.
        if(!runtime.jumpHorizontalValid) {
            runtime.jumpHorizontalVelocity={std::cos(horizontalHeading*radians)*runtime.movementSpeed,
                std::sin(horizontalHeading*radians)*runtime.movementSpeed};
            runtime.jumpHorizontalValid=true;
        }
        candidate.x+=runtime.jumpHorizontalVelocity[0]*dt;
        candidate.y+=runtime.jumpHorizontalVelocity[1]*dt;
        runtime.movementSpeed=std::hypot(runtime.jumpHorizontalVelocity[0],runtime.jumpHorizontalVelocity[1]);
    } else {
        const float target=moving&&runtime.playerCoughTime<=0?storylandPedMoveTarget(
            input.movementAmount(),input.running||input.sprinting,input.analogMovement):0;
        if(runtime.playerStopVariant>=0&&(runtime.playerStopVariant>1||
            runtime.playerStopTime>=runtime.playerRunStopDuration[size_t(runtime.playerStopVariant)])) {
            runtime.playerStopVariant=-1;runtime.playerStopTime=0;
        }
        if(!runtime.jumpLaunching) {
            runtime.playerMoveBlend=storylandStepPedMoveBlend(runtime.playerMoveBlend,target,dt);
            const int stop=storylandNativeSprintStopVariant(runtime.playerSprintBlend,
                runtime.playerSprintFading,runtime.playerMoveBlend,runtime.playerLocomotionPhase,moving&&input.sprinting);
            if(runtime.playerStopVariant<0&&stop>=0&&runtime.playerRunStopDistance&&
                runtime.playerRunStopDuration[size_t(stop)]>0) {
                runtime.playerStopVariant=stop;runtime.playerStopTime=0;
            }
            if(runtime.playerStopVariant>=0) {
                // The native run-stop association replaces walk/run/sprint.
                // Its finish callback restores turning; translation comes
                // from decoded root keys, without a second deceleration law.
                runtime.playerMoveBlend=runtime.playerSprintBlend=0;runtime.playerSprintFading=false;
                sprintState.association=false;sprintState.blendDelta=0;
            } else {
                runtime.playerSprintFading=sprintState.blendDelta<0;
            }
            runtime.movementSpeed=storylandPedGaitSpeed(runtime.playerMoveBlend,runtime.playerSprintBlend,
                runtime.walkSpeed,runtime.runSpeed,runtime.sprintSpeed);
            if (aimLocomotion && !extended.crouched && runtime.equippedWeapon < runtime.weapons.size() &&
                storylandNativeWeaponUsesTwoHands(world.paths.vcs,runtime.weapons[runtime.equippedWeapon].nativeId))
                runtime.movementSpeed *= 1.05f;
            if (extendedAllowed && extended.crouched) {
                const float requested = moving ? input.movementAmount() : 0.0f;
                const float forwardAlignment=std::cos(std::remainder(movement.heading-heading,360.0f)*radians);
                const int requestedDirection=aimLocomotion&&forwardAlignment<-.08f?-1:1;
                extended.crouchMoveCommand=storylandSaCrouchMoveCommand(
                    extended.crouchMoveCommand,requested*requestedDirection,dt);
                extended.crouchMoveAmount=std::fabs(extended.crouchMoveCommand);
                const bool moveAssociation=extended.crouchMoveAmount>0.0f&&
                    extended.crouchStanceBlend>.99f;
                const int direction=extended.crouchMoveCommand<0?-1:1;
                if(moving) {
                    extended.crouchMoveHeading=movement.heading+
                        (requestedDirection==direction?0.0f:180.0f);
                }
                if(moveAssociation&&extended.crouchMotionDirection!=direction) {
                    extended.crouchAnimationTime=0;
                    extended.crouchMoveBlend=0;
                    extended.crouchMotionDirection=direction;
                }
                extended.crouchMoveBlend=storylandSaCrouchAssociationWeight(
                    extended.crouchMoveBlend,moveAssociation,dt);
                runtime.playerMoveBlend = extended.crouchMoveAmount;
                runtime.playerSprintBlend = 0.0f;
                runtime.playerSprintFading = false;
                sprintState.association=false;sprintState.blendDelta=0;
                runtime.playerStopVariant = -1;
                runtime.playerStopTime = 0.0f;
                runtime.movementSpeed=0;
                if(moveAssociation) {
                    const float begin=extended.crouchAnimationTime;
                    extended.crouchAnimationTime+=storylandSaCrouchAnimationStep(dt,extended.crouchMoveAmount);
                    const float distance=runtime.playerCrouchRootDistance?
                        runtime.playerCrouchRootDistance(extended.crouchMotionDirection<0,
                            begin,extended.crouchAnimationTime):
                        runtime.walkSpeed*(extended.crouchAnimationTime-begin);
                    if(std::isfinite(distance)&&distance>0)
                        runtime.movementSpeed=distance*extended.crouchMoveBlend/dt;
                    crouchAdvance=runtime.movementSpeed>0;
                }
            }
        }
        if(runtime.playerStopVariant>=0&&!runtime.jumpLaunching) {
            const unsigned variant=unsigned(runtime.playerStopVariant);
            const float until=std::min(runtime.playerStopTime+dt,runtime.playerRunStopDuration[variant]);
            const float authored=runtime.playerRunStopDistance?
                runtime.playerRunStopDistance(variant,runtime.playerStopTime,until):0;
            const float travel=std::isfinite(authored)?authored:0;
            candidate.x+=std::cos(heading*radians)*travel;candidate.y+=std::sin(heading*radians)*travel;
            runtime.movementSpeed=std::fabs(travel)/dt;runtime.playerStopTime=until;
            runtime.playerSprintActive=false;
        } else if((moving||crouchAdvance)&&!runtime.jumpLaunching) {
            const float desiredHeading=crouchAdvance?extended.crouchMoveHeading:movement.heading;
            if(aimLocomotion) {
                candidate.x+=std::cos(desiredHeading*radians)*runtime.movementSpeed*dt;
                candidate.y+=std::sin(desiredHeading*radians)*runtime.movementSpeed*dt;
            } else {
                const float turn=std::remainder(desiredHeading-heading,360.0f);
                heading+=std::clamp(turn,-420.0f*dt,420.0f*dt);
                const float alignment=std::max(0.0f,std::cos(std::remainder(desiredHeading-heading,360.0f)*radians));
                candidate.x+=std::cos(heading*radians)*runtime.movementSpeed*dt*alignment;
                candidate.y+=std::sin(heading*radians)*runtime.movementSpeed*dt*alignment;
            }
        } else if(runtime.jumpLaunching) {
            candidate.x+=runtime.jumpHorizontalVelocity[0]*dt;
            candidate.y+=runtime.jumpHorizontalVelocity[1]*dt;
        } else runtime.movementSpeed=0;
    }
    if(scriptedWalk&&scriptedWalkSteering&&environment&&(environment->playerWalkTask.active||trackedWalk)) {
        const float remaining=std::hypot(scriptedWalkTarget.x-player.x,scriptedWalkTarget.y-player.y);
        const float travel=std::hypot(candidate.x-player.x,candidate.y-player.y);
        if(travel>remaining&&travel>0) {
            const float fraction=remaining/travel;
            candidate.x=player.x+(candidate.x-player.x)*fraction;
            candidate.y=player.y+(candidate.y-player.y)*fraction;
        }
    }
    const auto beforeHorizontal=player;
    runtime.playerAnimationMoveDelta={candidate.x-player.x,candidate.y-player.y};
    runtime.playerAnimationMoveDeltaValid=true;
    // Stair support belongs to grounded locomotion. Applying it during ascent
    // repeatedly snaps the first jump step back down into its support probe.
    const auto bodyMove=storylandMovePlayTestCharacter(world,runtime,player,candidate,-1,false,runtime.grounded);
    if(bodyMove.clear)player=bodyMove.position;
    if (extendedAllowed && extended.crouched && runtime.grounded && !extended.busy()) {
        const float travelled = std::hypot(player.x - beforeHorizontal.x,
            player.y - beforeHorizontal.y);
        if (travelled > 0.0001f) {
            extended.crouchTravelDistance += travelled;
            if (!std::isfinite(extended.crouchTravelDistance) ||
                extended.crouchTravelDistance > 100000.0f)
                extended.crouchTravelDistance = std::fmod(extended.crouchTravelDistance, 1000.0f);
        } else if (!moving&&extended.crouchMoveAmount<=0&&extended.crouchMoveBlend<=0) {
            extended.crouchMotionDirection = 0;
            extended.crouchTravelDistance = 0.0f;
        }
    }
    if(bodyMove.blocked&&distance(player,candidate)>0.04f) {
        runtime.movementSpeed=std::hypot(player.x-beforeHorizontal.x,player.y-beforeHorizontal.y)/dt;
        if(airborne)runtime.jumpHorizontalVelocity={(player.x-beforeHorizontal.x)/dt,(player.y-beforeHorizontal.y)/dt};
        if(world.paths.vcs&&scriptedWalk&&environment&&environment->playerWalkTask.active&&
            (environment->playerNavigation.seekNodes||environment->playerNavigation.localAvoidanceSolver||
             environment->playerNavigation.alternateObstructionHandling))
            storylandWaitForPlayerNavigation(*environment);
    }
    const auto beforeVertical=player;
    if(runtime.grounded) {
        extended.highestFallZ=player.z;
        extended.fallingDistance=0;
    }
    const bool characterCollision=!environment||environment->playerCollisionEnabled;
    const auto vertical=characterCollision?
        storylandPedVerticalStep(runtime.verticalSpeed,runtime.gravity,verticalSeconds):StorylandPedVerticalStep{};
    const float nextZ=player.z+vertical.displacement;
    const StorylandNativePoint top{player.x,player.y,player.z+0.08f},
        bottom{player.x,player.y,std::min(player.z-3.0f,nextZ-.08f)};
    const float fraction=characterCollision?storylandTraceNativeGround(world,top,bottom,runtime.groundSurface):1.0f;
    bool hasFloor=fraction<1;
    float floor=top.z+(bottom.z-top.z)*fraction;
    for(size_t index=0;index<runtime.movementCars.size();++index) {
        const auto& car=runtime.movementCars[index];
        if(!characterCollision||!car.collisionEnabled)continue;
        if(index<runtime.vehicles.size()&&storylandVcsOrdinaryBoat(runtime.vehicles[index].nativeDefinition)) {
            const auto& vehicle=runtime.vehicles[index];
            if(!vehicle.nativeDefinition.collision)continue;
            auto matrix=vehicleBody(vehicle,car.yaw).collisionTransform;
            matrix[12]+=car.position.x;matrix[13]+=car.position.y;matrix[14]+=car.position.z;
            float hit=1;StorylandNativePoint normal;
            if(storylandTraceCollisionModel(*vehicle.nativeDefinition.collision,matrix,top,bottom,hit,normal)&&normal.z>.01f) {
                float originOffset=1.04f;
                if(environment&&environment->characterOriginOffset) {
                    float originalOffset=0;
                    if(environment->characterOriginOffset(0,originalOffset)&&std::isfinite(originalOffset))
                        originOffset=originalOffset;
                }
                // VCS 1F9C4C..1F9C74 retains the support hit plus1.04 at the native entity origin.
                const float deck=top.z+(bottom.z-top.z)*hit+1.04f-originOffset;
                if(!hasFloor||deck>floor) {floor=deck;hasFloor=true;runtime.groundSurface=0;}
            }
            continue;
        }
        const float roof=car.position.z+car.height;
        if(player.z<roof-.15f||roof<bottom.z||roof>top.z)continue;
        const float c=std::cos(car.yaw*radians),s=std::sin(car.yaw*radians);
        const float dx=player.x-car.position.x,dy=player.y-car.position.y;
        if(std::fabs(dx*c+dy*s)>car.halfLength||std::fabs(-dx*s+dy*c)>car.halfWidth)continue;
        if(!hasFloor||roof>floor) {floor=roof;hasFloor=true;runtime.groundSurface=0;}
    }
    if(world.paths.vcs&&characterCollision&&!hasFloor&&!runtime.jumpLaunching&&
        vertical.velocity<0&&nextZ<player.z) {
        const auto body=storylandNativeCharacterBody();
        const auto downward=storylandMoveNativeBody(world,player,
            {player.x,player.y,nextZ},body);
        if(downward.clear&&downward.blocked&&!downward.recovered) {
            const StorylandNativePoint raisedTop{player.x,player.y,player.z+.36f};
            const StorylandNativePoint raisedBottom{player.x,player.y,player.z};
            StorylandNativePoint normal;
            uint8_t raisedSurface=0;
            const float raisedFraction=storylandTraceNativeGround(world,raisedTop,raisedBottom,
                raisedSurface,&normal);
            const float raisedFloor=raisedTop.z+(raisedBottom.z-raisedTop.z)*raisedFraction;
            if(raisedFraction<1&&std::fabs(normal.z)>=.5f&&raisedFloor>=player.z&&
                raisedFloor<=player.z+.36f) {
                const auto upward=storylandMoveNativeBody(world,player,
                    {player.x,player.y,raisedFloor},body);
                if(upward.clear&&!upward.blocked&&!upward.recovered) {
                    floor=raisedFloor;hasFloor=true;runtime.groundSurface=raisedSurface;
                }
            }
        }
    }
    // The downward body sweep covers the character's width, while the ground
    // ray above only checks its centre. At a ledge the body can catch on the
    // surface even after that ray misses it. Probe within the foot box before
    // contact so the position and downward speed agree about support.
    if(characterCollision&&!runtime.jumpLaunching&&vertical.velocity<=0&&
        (!hasFloor||floor<nextZ-.005f)) {
        const auto body=storylandNativeCharacterBody();
        const float x=body.halfLength*.9f,y=body.halfWidth*.9f;
        const std::array<StorylandNativePoint,12> offsets{{
            {x,0,0},{-x,0,0},{0,y,0},{0,-y,0},
            {x*.5f,0,0},{-x*.5f,0,0},{0,y*.5f,0},{0,-y*.5f,0},
            {x,y,0},{x,-y,0},{-x,y,0},{-x,-y,0}
        }};
        const float minimumUp=world.paths.vcs?.5f:.70710678f;
        for(const auto& offset:offsets) {
            const StorylandNativePoint probeTop{top.x+offset.x,top.y+offset.y,top.z};
            const StorylandNativePoint probeBottom{bottom.x+offset.x,bottom.y+offset.y,bottom.z};
            StorylandNativePoint normal;
            uint8_t surface=0;
            const float hit=storylandTraceNativeGround(world,probeTop,probeBottom,surface,&normal);
            if(hit>=1||std::fabs(normal.z)<minimumUp)continue;
            const float support=probeTop.z+(probeBottom.z-probeTop.z)*hit;
            if(support<nextZ-.005f||support>player.z+.051f)continue;
            if(!hasFloor||support>floor) {
                floor=support;hasFloor=true;runtime.groundSurface=surface;
            }
        }
    }
    const bool landing=hasFloor&&vertical.velocity<=0&&(nextZ<=floor+.005f||
        (world.paths.vcs&&storylandPedSupportContinuity(runtime.grounded,runtime.jumpLaunching,
            runtime.verticalSpeed,player.z,floor)));
    if(world.paths.vcs&&(!runtime.grounded||(!landing&&!runtime.jumpLaunching))) {
        auto& fall=runtime.playerFall;
        if(!fall.active)storylandBeginVcsFall(fall,runtime.jumpFromLaunch);
        // The native ray starts at the entity origin. Storyland stores feet
        // support positions, so preserve the model's decoded origin datum.
        float originOffset=1.04f;
        if(environment&&environment->characterOriginOffset) {
            float decodedOffset=0;
            if(environment->characterOriginOffset(0,decodedOffset)&&std::isfinite(decodedOffset))
                originOffset=decodedOffset;
        }
        const float verticalTravel=nextZ-beforeVertical.z;
        storylandAdvanceVcsFall(fall,std::hypot(player.x-beforeHorizontal.x,
            player.y-beforeHorizontal.y,verticalTravel),vertical.velocity<0,hasFloor,verticalSeconds);
        if(!hasFloor&&vertical.velocity < -5.0f)fall.relcsRollCandidate=true;
        const bool nearFloor=hasFloor&&vertical.velocity<=0&&
            player.z+originOffset-floor<1.3f;
        if(fall.landingAnimationId<0&&(nearFloor||landing)) {
            fall.landingAnimationId=storylandVcsFallLandingAnimation(fall,runtime.playerMoveBlend,moving);
            const float horizontalSpeed=std::hypot(runtime.jumpHorizontalVelocity[0],
                runtime.jumpHorizontalVelocity[1]);
            if(storylandRelcsLandingRollAllowed(fall,extendedAllowed&&!extended.crouched,
                horizontalSpeed,vertical.velocity,runtime.grounded,runtime.groundSurface,runtime.health)&&
                runtime.reactionBindings.duration&&runtime.reactionBindings.rootDelta&&
                runtime.reactionBindings.duration(147,0)>0) {
                fall.landingAnimationId=147;
                heading=std::atan2(runtime.jumpHorizontalVelocity[1],runtime.jumpHorizontalVelocity[0])/radians;
            }
            const float duration=runtime.reactionBindings.duration?
                runtime.reactionBindings.duration(fall.landingAnimationId,0):
                fall.landingAnimationId==138?runtime.jumpLandingDuration:0;
            fall.landingAnimationDuration=std::isfinite(duration)?std::max(0.0f,duration):0;
            fall.landingAnimationTime=0;
            fall.landingHeading=heading;
            runtime.landingTime=fall.landingAnimationDuration;
            storylandQueuePedLandingSound(runtime,-1,{player.x,player.y,floor},runtime.groundSurface,
                35,fall.landingAnimationId==139);
        }
    }
    if(landing) {
        if(!runtime.grounded) {
            if(world.paths.vcs) {
                const float contactSeconds=storylandVcsFallContactSeconds(
                    beforeVertical.z-floor,runtime.verticalSpeed,runtime.gravity,verticalSeconds);
                const float impactVelocity=runtime.verticalSpeed-runtime.gravity*contactSeconds;
                const float horizontalSpeed=std::hypot(runtime.jumpHorizontalVelocity[0],
                    runtime.jumpHorizontalVelocity[1]);
                const float impact=storylandVcsFallImpactDamage(horizontalSpeed,impactVelocity,
                    runtime.groundSurface);
                const auto& horizontal=runtime.jumpHorizontalVelocity;
                const uint8_t direction=std::fabs(horizontal[0])>.5f||std::fabs(horizontal[1])>.5f?
                    storylandNativeHitDirection(heading,-horizontal[0],-horizontal[1]):2;
                if(impact>0)runtime.inflictDamage(impact,8,storylandNativeFallWeapon(true),false,0,direction);
            } else {
                runtime.landingTime=std::max(0.0f,runtime.jumpLandingDuration);
                storylandQueuePedLandingSound(runtime,-1,{player.x,player.y,floor},runtime.groundSurface);
            }
            if(world.paths.vcs&&environment)
                storylandConsumeVcsFootstepMarks(environment->playerVcsFootstepMarks,40);
        }
        player.z=floor;runtime.verticalSpeed=0;runtime.grounded=true;runtime.airborneTime=0;
        extended.fallingDistance=0;
        extended.highestFallZ=player.z;
        if(!runtime.jumpLaunching) {runtime.jumpHorizontalValid=false;runtime.jumpFromLaunch=false;extended.sprintJumpBoost=false;}
    } else if(runtime.jumpLaunching&&hasFloor&&std::fabs(player.z-floor)<.1f) {
        player.z=floor;runtime.verticalSpeed=0;runtime.grounded=true;
    } else {
        runtime.grounded=false;runtime.verticalSpeed=vertical.velocity;player.z=nextZ;
        if (runtime.verticalSpeed<0) {
            extended.fallingDistance=std::max(extended.fallingDistance,
                extended.highestFallZ-player.z);
        } else extended.highestFallZ=std::max(extended.highestFallZ,player.z);
    }
    const auto verticalMove=characterCollision?
        storylandMoveNativeBody(world,beforeVertical,player,storylandNativeCharacterBody()):
        StorylandNativeMoveResult{player,true};
    if(verticalMove.blocked&&player.z>beforeVertical.z)runtime.verticalSpeed=std::min(0.0f,runtime.verticalSpeed);
    // A thin surface can lie between all foot probes. If it stops the actual
    // downward sweep, do not keep accumulating imaginary falling velocity.
    // Leave grounded false so moving clear of the obstacle resumes real fall.
    if(verticalMove.clear&&verticalMove.blocked&&player.z<beforeVertical.z&&
        verticalMove.position.z>player.z+.0001f)
        runtime.verticalSpeed=0;
    player=verticalMove.position;
    if(!runtime.grounded)runtime.airborneTime+=verticalSeconds;
    if(landingMotion&&runtime.landingTime<=0)runtime.playerFall={};
    runtime.playerPosition=player;
    if(scriptedWalk&&environment&&(environment->playerWalkTask.active||trackedWalk)&&walkArrived()) {
        environment->playerWalkTask.active=false;
        environment->playerWalkTask.completed=true;
        if(world.paths.vcs)environment->playerNavigation.clearRoute();
    }
}


int storylandDispatchFootPolice(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const StorylandNativePaths& paths,std::vector<StorylandNativeTrafficActor>& actors,
    const StorylandRuntimePed& officer,const StorylandNativePoint& player,float cameraYaw,float seconds) {
    if(runtime.paused||runtime.pauseMenu||runtime.health<=0||!runtime.playerControl||
        runtime.policeIgnorePlayer||runtime.wanted<=0||runtime.visibleArea!=0||
        !officer.isPolice(paths.vcs)||!std::isfinite(seconds)||seconds<=0)return -1;
    runtime.policeDispatchTime=std::max(0.0f,runtime.policeDispatchTime-std::min(seconds,0.1f));
    if(runtime.policeDispatchTime>0)return -1;
    runtime.policeDispatchTime=0.75f;
    const auto distance=[&](const StorylandNativePoint& point) {
        return std::hypot(point.x-player.x,point.y-player.y,point.z-player.z);
    };
    const auto offscreen=[&](const StorylandNativePoint& point) {
        const float angle=std::atan2(point.y-player.y,point.x-player.x)*57.2957795f;
        if(std::fabs(std::remainder(angle-cameraYaw,360.0f))>110.0f)return true;
        return storylandTraceNativeSegment(world,{player.x,player.y,player.z+1.0f},
            {point.x,point.y,point.z+1.0f})<0.99f;
    };
    int nearbyOfficers=0;
    size_t slot=SIZE_MAX;
    for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index) {
        const auto& ped=runtime.pedestrians[index];
        if (ped.scriptHandle >= 0) continue;
        const float range=distance(actors[index].position);
        if(ped.health>0&&!ped.serviceVehicleIdentity&&ped.isPolice(paths.vcs)&&range<100.0f&&
            ped.policePursuing)++nearbyOfficers;
        if(!ped.serviceVehicleIdentity&&ped.turfWarActorId==0&&!ped.policePursuing&&!ped.hostile&&
            (ped.health>0||ped.deathTime>=10.0f)&&range>110.0f&&
            offscreen(actors[index].position)&&int(index)!=runtime.targetedPed)
            slot=index;
    }
    if(nearbyOfficers>=storylandPoliceBudget(runtime.wanted,paths.vcs).officers)return -1;
    const size_t missionCount=runtime.missionEnvironment?runtime.missionEnvironment->missionCharacters.size():0;
    if(slot==SIZE_MAX&&actors.size()+missionCount<runtime.pedestrians.size())slot=actors.size();
    if(slot==SIZE_MAX)return -1;
    if(paths.nodes.empty())return -1;
    const size_t firstNode=size_t(storylandNativeRandom(runtime.populationRandom))%paths.nodes.size();
    for(size_t offset=0;offset<paths.nodes.size();++offset) {
        const size_t nodeIndex=(firstNode+offset)%paths.nodes.size();
        const auto& node=paths.nodes[nodeIndex];
        const float range=distance(node.position);
        if(node.vehicle||node.water||node.disabled||node.links.empty()||range<40.0f||range>80.0f||
            !offscreen(node.position))continue;
        bool occupied=false;
        for(const auto& actor:actors)
            if(std::hypot(actor.position.x-node.position.x,actor.position.y-node.position.y,
                actor.position.z-node.position.z)<3.0f) {occupied=true;break;}
        if(occupied)continue;
        const StorylandNativePoint top{node.position.x,node.position.y,node.position.z+1.0f};
        const StorylandNativePoint bottom{node.position.x,node.position.y,node.position.z-2.0f};
        const float ground=storylandTraceNativeSegment(world,top,bottom);
        if(ground>=1.0f)continue;
        StorylandNativePoint position{node.position.x,node.position.y,top.z+(bottom.z-top.z)*ground};
        const auto movement=storylandMoveNativeBody(world,position,position,storylandNativeCharacterBody());
        if(!movement.clear)continue;
        if(slot==actors.size())actors.emplace_back();
        auto& actor=actors[slot];actor={};
        actor.node=uint32_t(nodeIndex);actor.nextNode=uint32_t(nodeIndex);
        actor.position=movement.position;
        actor.yaw=std::atan2(player.y-position.y,player.x-position.x)*57.2957795f;
        runtime.pedestrians[slot]=officer;
        runtime.pedestrians[slot].policePursuing=true;
        runtime.pedestrians[slot].policeLastSeenAt=runtime.elapsed;
        runtime.pedestrians[slot].policeLastKnownPosition=player;
        runtime.pedestrians[slot].policeLastKnownPositionValid=true;
        runtime.pedestrians[slot].hostile=true;
        runtime.pedestrians[slot].state=StorylandPedState::Combat;
        runtime.policeDispatchTime=3.0f;
        return int(slot);
    }
    return -1;
}


float StorylandPlayTestRuntime::maximumHealth() const {
    return missionEnvironment ? float(missionEnvironment->playerHealthLimit) : 100.0f;
}

float StorylandPlayTestRuntime::maximumArmour() const {
    return missionEnvironment ? float(missionEnvironment->playerArmourLimit) : 100.0f;
}

bool StorylandPlayTestRuntime::buyBackWeapons() {
    if(!weaponBuybackAvailable||health<=0||!playerControl||paused||pauseMenu||money<2000||
        occupiedVehicle>=0||std::hypot(playerPosition.x-weaponBuybackPosition.x,
            playerPosition.y-weaponBuybackPosition.y,playerPosition.z-weaponBuybackPosition.z)>=4.0f)return false;
    bool restored=false;
    for(size_t index=1;index<inventory.size()&&index<confiscatedWeapons.size();++index) {
        const auto& saved=confiscatedWeapons[index];
        if(!saved.owned)continue;
        auto& current=inventory[index];
        current.owned=true;
        current.ammunition=std::max(current.ammunition,saved.ammunition);
        current.magazine=std::max(current.magazine,saved.magazine);
        current.cooldown=0;current.reloading=0;
        restored=true;
    }
    if(!restored)return false;
    money-=2000;weaponBuybackAvailable=false;confiscatedWeapons.clear();
    return true;
}


int storylandPopulateFortBaxter(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    std::vector<StorylandNativeTrafficActor>& actors,const StorylandRuntimePed& soldier,
    const StorylandNativePoint& player,float cameraYaw) {
    if(!runtime.crime.vcs||runtime.paused||runtime.pauseMenu||runtime.visibleArea!=0)return -1;
    const StorylandNativeZone* base=nullptr;
    for(const auto& zone:world.zones.information)if(zone.name=="ARMYBAS") {base=&zone;break;}
    if(!base)base=world.zones.named("ARMYB");
    if(!base)return -1;
    storylandUpdateMilitaryResponse(runtime,&world,player);
    unsigned population=0;
    for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index) {
        auto& ped=runtime.pedestrians[index];
        if(!ped.fortBaxterSoldier)continue;
        if(ped.health<=0)continue;
        // The authored path network can cross the ARMYBAS boundary. Remove
        // only our ambient patrols when they leave it; never touch a mission
        // soldier with a valid script handle.
        if(ped.scriptHandle<0&&storylandIsAdditionalFortBaxterPatrol(ped)&&
            !base->contains(actors[index].position)) {
            ped.health=0;
            ped.state=StorylandPedState::Dead;
            ped.renderAlpha=0;
            ped.populationFadeOut=true;
            continue;
        }
        ++population;
    }
    // Script-owned Army characters already populate the opening mission.
    // Ambient patrols are restricted to a small number of on-base actors;
    // player coordinates outside the authored ARMYBAS zone never qualify.
    const size_t target=std::min<size_t>(3u,storylandAmbientPedestrianTarget(runtime,player));
    if(!storylandAllowAdditionalFortBaxterPatrol(runtime.crime.vcs,false,
        runtime.missionEnvironment && runtime.missionEnvironment->introMissionProtected,
        base->contains(player),runtime.militaryPopulationEnabled,population,target)||
       runtime.health<=0)return -1;
    if(runtime.elapsed<runtime.fortBaxterPopulationAt)return -1;
    runtime.fortBaxterPopulationAt=runtime.elapsed+1.0f;
    // Populate from the shipped pedestrian network, not from whichever civilian
    // happened to wander through the zone. Exact retail population timing is separate.
    const auto offscreen=[&](const StorylandNativePoint& point) {
        const float angle=std::atan2(point.y-player.y,point.x-player.x)*57.2957795f;
        return std::fabs(std::remainder(angle-cameraYaw,360.0f))>110.0f ||
            storylandTraceNativeSegment(world,{player.x,player.y,player.z+1.0f},
                {point.x,point.y,point.z+1.0f})<0.99f;
    };
    size_t slot=SIZE_MAX;
    for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index) {
        const auto& ped=runtime.pedestrians[index];
        if(ped.scriptHandle>=0||(ped.fortBaxterSoldier&&ped.health>0)||ped.policePursuing||ped.stats.name=="STAT_COP"||
            (ped.health<=0&&ped.deathTime<10.0f)||int(index)==runtime.targetedPed)continue;
        const auto& point=actors[index].position;
        const float range=std::hypot(point.x-player.x,point.y-player.y,point.z-player.z);
        if((range>80.0f||(range>30.0f&&base->contains(point)))&&offscreen(point)) {
            slot=index;break;
        }
    }
    if(slot==SIZE_MAX&&actors.size()<target&&actors.size()<runtime.pedestrians.size())slot=actors.size();
    if(slot==SIZE_MAX)return -1;
    for(size_t nodeIndex=0;nodeIndex<world.paths.nodes.size();++nodeIndex) {
        const auto& node=world.paths.nodes[nodeIndex];
        const float range=std::hypot(node.position.x-player.x,node.position.y-player.y,node.position.z-player.z);
        if(node.vehicle||node.water||node.disabled||!base->contains(node.position)||
            range<30.0f||range>100.0f||!offscreen(node.position))continue;
        uint32_t next=UINT32_MAX;
        for(const auto& link:node.links)if(link.target<world.paths.nodes.size()) {
            const auto& target=world.paths.nodes[link.target];
            if(!target.vehicle&&!target.water&&!target.disabled&&base->contains(target.position)) {
                next=link.target;break;
            }
        }
        if(next==UINT32_MAX)continue;
        bool occupied=false;
        for(size_t other=0;other<actors.size();++other) {
            if(other==slot)continue;
            const auto& position=actors[other].position;
            if(std::hypot(position.x-node.position.x,position.y-node.position.y,
                position.z-node.position.z)<3.0f) {occupied=true;break;}
        }
        if(occupied)continue;
        uint8_t surface=0;
        const StorylandNativePoint top{node.position.x,node.position.y,node.position.z+1.0f};
        const StorylandNativePoint bottom{node.position.x,node.position.y,node.position.z-3.0f};
        const float ground=storylandTraceNativeGround(world,top,bottom,surface);
        if(ground>=1.0f)continue;
        const StorylandNativePoint position{node.position.x,node.position.y,top.z+(bottom.z-top.z)*ground};
        if(!base->contains(position)||!storylandMoveNativeBody(world,position,position,
            storylandNativeCharacterBody()).clear)continue;
        if(slot==actors.size())actors.emplace_back();
        auto& actor=actors[slot];actor={};
        actor.node=uint32_t(nodeIndex);actor.nextNode=next;
        actor.routeSeed=uint32_t(slot+1)*0x9E3779B9u^actor.node;
        actor.position=position;actor.groundOffset=position.z-node.position.z;
        const auto& target=world.paths.nodes[next].position;
        actor.yaw=std::atan2(target.y-position.y,target.x-position.x)*57.2957795f;
        auto& ped=runtime.pedestrians[slot];
        ped=soldier;ped.fortBaxterSoldier=true;ped.militaryPatrolAnchor=position;
        ped.hostile=!storylandGangPlayerProtected(runtime,ped.stats.pedType)&&
            (runtime.fortBaxterAlarm||!runtime.armyFatigues||runtime.militaryScriptHostile);
        ped.state=ped.hostile?StorylandPedState::Combat:StorylandPedState::Walk;
        return int(slot);
    }
    return -1;
}

bool storylandCharacterTouchesVehicle(const StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, const std::vector<StorylandNativeTrafficActor>& cars,
    int32_t character, int32_t vehicle) {
    if (!runtime.missionEnvironment) return false;
    const auto& mission = *runtime.missionEnvironment;
    const auto target = mission.missionVehicles.find(vehicle);
    if (target == mission.missionVehicles.end() || target->second.deleted || !target->second.collisionEnabled) return false;
    const auto carBody = [&](int32_t handle, StorylandNativePoint& position, StorylandNativeBody& body) {
        const auto found = mission.missionVehicles.find(handle);
        if (found == mission.missionVehicles.end() || found->second.deleted || !found->second.collisionEnabled) return false;
        const auto& car = found->second;
        if (car.spawnedSlot >= 0 && size_t(car.spawnedSlot) < cars.size() &&
            size_t(car.spawnedSlot) < runtime.vehicles.size()) {
            const auto& actor = cars[size_t(car.spawnedSlot)];
            const auto& live = runtime.vehicles[size_t(car.spawnedSlot)];
            if (live.scriptHandle != handle || !live.collisionEnabled) return false;
            position = actor.position; body = vehicleBody(live, actor.yaw); return true;
        }
        const auto model = world.collisionModels.find(uint32_t(car.model));
        if (model == world.collisionModels.end()) return false;
        position = {car.x, car.y, car.z};
        body.collision = &model->second;
        body.collisionTransform = storylandScriptObjectMatrix(0, 0, 0, 0, 0, car.heading);
        return true;
    };
    StorylandNativePoint sourcePosition, targetPosition;
    StorylandNativeBody sourceBody = storylandNativeCharacterBody(), targetBody;
    if (!carBody(vehicle, targetPosition, targetBody) || !targetBody.collision) return false;
    int32_t sourceVehicle = -1;
    if (character == 0) {
        sourcePosition = runtime.playerPosition;
        if (mission.inVehicle) sourceVehicle = mission.occupiedMissionVehicle;
        else if (!mission.playerCollisionEnabled) return false;
    } else {
        const auto found = mission.missionCharacters.find(character);
        if (found == mission.missionCharacters.end() || found->second.removeFromWorld) return false;
        const auto& ped = found->second;
        sourcePosition = {ped.x, ped.y, ped.z};
        if (!ped.attached) sourceVehicle = ped.vehicle;
        if (sourceVehicle < 0 && !ped.collisionEnabled) return false;
    }
    if (sourceVehicle >= 0 && (sourceVehicle == vehicle || !carBody(sourceVehicle, sourcePosition, sourceBody))) return false;
    // Seated actors use their vehicle's native COL spheres. On foot, the
    // movement controller uses a box envelope without a COL pointer, so it
    // must use the pedestrian box/triangle/sphere narrow phase instead.
    if (sourceVehicle >= 0)
        return storylandNativeVehicleBodiesOverlap(sourcePosition, sourceBody, targetPosition, targetBody);
    if (!std::isfinite(sourcePosition.x) || !std::isfinite(sourcePosition.y) ||
        !std::isfinite(sourcePosition.z) ||
        std::max({std::fabs(sourcePosition.x), std::fabs(sourcePosition.y), std::fabs(sourcePosition.z)}) > 1e7f)
        return false;
    StorylandNativeWorld contactWorld;
    contactWorld.collisionModels.emplace(0, *targetBody.collision);
    StorylandNativeCollisionInstance instance;
    instance.modelId = 0;
    instance.matrix = targetBody.collisionTransform;
    instance.matrix[12] += targetPosition.x;
    instance.matrix[13] += targetPosition.y;
    instance.matrix[14] += targetPosition.z;
    contactWorld.instances.push_back(instance);
    return !storylandNativeBodyClear(contactWorld, sourcePosition, sourceBody);
}

void storylandUpdatePlayerVehicleStunts(StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& vehicles) {
    if(!runtime.missionEnvironment||runtime.paused||runtime.pauseMenu)return;
    auto& environment=*runtime.missionEnvironment;
    auto& stunt=environment.playerVehicleStunt;
    const int slot=runtime.occupiedVehicle;
    const auto clearActive=[&] {
        stunt.kind=0;stunt.activeMilliseconds={};stunt.graceMilliseconds=0;
        stunt.vehicle=-1;stunt.previousPositionValid=false;
    };
    if(slot<0||runtime.playerVehicleSeat>=0||size_t(slot)>=runtime.vehicles.size()||size_t(slot)>=vehicles.size()||runtime.health<=0) {
        clearActive();return;
    }
    const auto& vehicle=runtime.vehicles[size_t(slot)];
    const auto& actor=vehicles[size_t(slot)];
    const int type=vehicle.nativeDefinition.nativeType;
    if(!vehicle.contactsValid||(type!=0&&type!=6&&type!=8)||vehicle.health<=0) {clearActive();return;}
    if(stunt.vehicle!=vehicle.scriptHandle) {
        clearActive();stunt.vehicle=vehicle.scriptHandle;stunt.activeDistances={};
    }
    float travelled=0;
    if(stunt.previousPositionValid)travelled=std::hypot(actor.position.x-stunt.previousPosition[0],
        actor.position.y-stunt.previousPosition[1],actor.position.z-stunt.previousPosition[2]);
    stunt.previousPosition={actor.position.x,actor.position.y,actor.position.z};stunt.previousPositionValid=true;
    const float frameMilliseconds=environment.nativeScriptTimeStep*20.0f;
    if(!std::isfinite(frameMilliseconds)||frameMilliseconds<0||frameMilliseconds>1000)return;
    const uint32_t elapsed=uint32_t(frameMilliseconds);
    const auto complete=[&](size_t index,uint32_t threshold) {
        const uint32_t duration=stunt.activeMilliseconds[index];
        if(duration>=threshold) {
            stunt.milliseconds[index]=int32_t(duration);stunt.distances[index]=stunt.activeDistances[index];
            static const char* timeNames[]={"Longest2WheelTime","LongestWheelieTime","LongestStoppieTime"};
            static const char* distanceNames[]={"Longest2WheelDistance","LongestWheelieDistance","LongestStoppieDistance"};
            runtime.statistics[timeNames[index]]=std::max(runtime.statistics[timeNames[index]],double(duration/1000));
            runtime.statistics[distanceNames[index]]=std::max(runtime.statistics[distanceNames[index]],double(stunt.activeDistances[index]));
        }
        stunt.activeMilliseconds[index]=0;stunt.activeDistances[index]=0;
        stunt.graceMilliseconds=0;stunt.kind=0;stunt.distance=0;
    };
    const auto advance=[&](size_t index,float recovery) {
        if(!stunt.activeMilliseconds[index])stunt.startedAt=environment.gameMilliseconds;
        stunt.kind=int32_t(index)+1;stunt.activeMilliseconds[index]+=elapsed;
        stunt.activeDistances[index]+=travelled;stunt.distance=stunt.activeDistances[index];
        stunt.graceMilliseconds=uint32_t(std::max(0.0f,float(stunt.graceMilliseconds)-float(elapsed)*recovery));
    };
    const auto grace=[&](size_t index,uint32_t threshold,bool allowed) {
        if(stunt.activeMilliseconds[index]&&allowed&&stunt.graceMilliseconds<500)
            stunt.graceMilliseconds+=elapsed;
        else complete(index,threshold);
    };
    const auto& contact=vehicle.wheelContacts;
    // CPlayerInfo::Process, 0x247520..0x247BFC. Physical contacts retain
    // the native ordering; a bike has four longitudinal suspension lines.
    if(type==0) {
        stunt.activeMilliseconds[1]=stunt.activeMilliseconds[2]=0;
        if(!contact[2]&&!contact[3]&&contact[0]&&contact[1]&&!vehicle.bodyContact)
            advance(0,.5f);
        else grace(0,2000,(!contact[2]&&!contact[3])||(!contact[0]&&!contact[1]));
        return;
    }
    stunt.activeMilliseconds[0]=0;
    if(!contact[0]&&!contact[1]) {
        if(contact[2]||(contact[3]&&!vehicle.bodyContact))advance(1,.2f);
        else grace(1,3000,true);
        return;
    }
    if(stunt.activeMilliseconds[1]) {complete(1,3000);return;}
    if(!contact[2]&&!contact[3]&&!vehicle.bodyContact)advance(2,.2f);
    else grace(2,2000,true);
}
