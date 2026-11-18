#include "src/storyland/storyland_police.h"
#include "src/re/reLCS2/services/storyland_lcs_police_response.h"
#include "src/re/reVCS/services/storyland_vcs_police_response.h"
#include "src/re/reLCS2/runtime/storyland_lcs_host_queries.h"
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_service_population.h"
#include "src/storyland/storyland_native_npc_aim.h"
#include "src/storyland/storyland_native_random.h"
#include <cmath>

namespace {
bool selectPoliceWeapon(const StorylandPlayTestRuntime& runtime,StorylandRuntimePed& officer,int32_t nativeWeapon) {
    for(size_t index=0;index<runtime.weapons.size();++index) {
        const auto& weapon=runtime.weapons[index];
        if(weapon.nativeId!=nativeWeapon)continue;
        if(officer.weapon!=index) {
            officer.weapon=index;
            officer.melee={};officer.ranged={};officer.fireFlash=0;officer.reloading=0;
            officer.weaponMuzzleValid=false;
        }
        officer.magazine=weapon.clipSize;
        officer.ammunition=1000;
        return true;
    }
    return false;
}

bool playerOnVehicleSurface(const StorylandPlayTestRuntime& runtime) {
    if(!runtime.grounded||runtime.occupiedVehicle>=0)return false;
    for(const auto& car:runtime.movementCars) {
        if(!car.collisionEnabled||std::fabs(runtime.playerPosition.z-car.position.z-car.height)>.2f)continue;
        const float angle=car.yaw*.01745329252f;
        const float cosine=std::cos(angle),sine=std::sin(angle);
        const float dx=runtime.playerPosition.x-car.position.x,dy=runtime.playerPosition.y-car.position.y;
        if(std::fabs(dx*cosine+dy*sine)<=car.halfLength&&
            std::fabs(-dx*sine+dy*cosine)<=car.halfWidth)return true;
    }
    return false;
}

bool playerCarAlarmActive(const StorylandPlayTestRuntime& runtime) {
    if(runtime.occupiedVehicle<0||size_t(runtime.occupiedVehicle)>=runtime.vehicles.size())return false;
    const auto& vehicle=runtime.vehicles[size_t(runtime.occupiedVehicle)];
    return vehicle.nativeDefinition.nativeType==0&&vehicle.health>0&&!vehicle.wrecked&&
        vehicle.alarmMilliseconds!=0&&vehicle.alarmMilliseconds!=UINT16_MAX;
}
}

StorylandNativePoint storylandPoliceInstantHitEnd(StorylandPlayTestRuntime& runtime,
    const StorylandRuntimePed& officer,const StorylandNativePoint& origin,
    const StorylandNativePoint& target,float range,bool playerTarget,float spreadScale) {
    float targetSpeed=0;
    if(playerTarget) {
        if(runtime.occupiedVehicle>=0&&size_t(runtime.occupiedVehicle)<runtime.vehicles.size()) {
            const auto& vehicle=runtime.vehicles[size_t(runtime.occupiedVehicle)];
            targetSpeed=std::hypot(vehicle.speed,vehicle.lateralSpeed,vehicle.verticalSpeed);
        } else {
            const float vertical=runtime.playerWater.active?
                runtime.playerWater.verticalSpeed:runtime.verticalSpeed;
            targetSpeed=std::hypot(runtime.movementSpeed,vertical);
        }
    }
    return storylandNativeNpcInstantHitEnd(origin,target,range,
        storylandPoliceWeaponAccuracy(officer.serviceRole),playerTarget,
        targetSpeed/StorylandNativePhysicsRate,spreadScale,
        [&](){return storylandNativeRandomUnit(runtime.combatRandom);});
}

bool storylandPoliceOfficerInControl(const StorylandPlayTestRuntime& runtime,const StorylandRuntimePed& officer) {
    return std::isfinite(runtime.elapsed)&&officer.health>0&&officer.renderAlpha>0&&
        officer.state!=StorylandPedState::Dead&&!officer.zombie.active&&!officer.water.active&&
        !officer.bulletReaction.knockdown&&!officer.bulletReaction.active(runtime.elapsed)&&officer.coughTime<=0;
}

bool storylandPoliceOfficerCanArrest(const StorylandPlayTestRuntime& runtime,size_t officerIndex) {
    if(officerIndex>=runtime.pedestrians.size())return false;
    const auto& officer=runtime.pedestrians[officerIndex];
    return storylandPoliceOfficerInControl(runtime,officer)&&officer.isPolice(runtime.crime.vcs)&&
        officer.policePursuing&&officer.scriptHandle<0&&!officer.serviceVehicleIdentity&&
        !officer.fortBaxterSoldier&&!officer.fleeActive&&!officer.threatWaiting&&
        !officer.melee.active&&!officer.ranged.active&&officer.fireFlash<=0&&
        !storylandPlayerReactionHoldsTarget(runtime.playerReaction,
            StorylandReactionTargetKind::Ambient,int32_t(officerIndex),officer.stats.modelId);
}

void storylandInitializePoliceLoadout(const StorylandPlayTestRuntime& runtime,StorylandRuntimePed& officer) {
    if(!officer.isPolice(runtime.crime.vcs)||officer.scriptHandle>=0||officer.fortBaxterSoldier)return;
    const auto role=officer.serviceRole;
    const bool tactical=role==StorylandServiceRole::Swat||role==StorylandServiceRole::Vice;
    const bool federal=role==StorylandServiceRole::Fbi||role==StorylandServiceRole::Army;
    const int32_t nativeWeapon=tactical?(runtime.crime.vcs?25:23):
        federal?(runtime.crime.vcs?27:25):0;
    officer.armour=role==StorylandServiceRole::Swat?50.0f:
        federal||role==StorylandServiceRole::Vice?100.0f:0.0f;
    selectPoliceWeapon(runtime,officer,nativeWeapon);
}

void storylandChoosePolicePursuitWeapon(const StorylandPlayTestRuntime& runtime,StorylandRuntimePed& officer) {
    if(!officer.policePursuing||!storylandPoliceOfficerInControl(runtime,officer)||
        officer.weapon>=runtime.weapons.size()||runtime.weapons[officer.weapon].nativeId!=0)return;
    // CopAI changes an unarmed cop's selection; it does not demote an issued gun.
    if(runtime.wanted>1)selectPoliceWeapon(runtime,officer,runtime.crime.vcs?19:17);
    else if(runtime.wanted==1&&!playerOnVehicleSurface(runtime))selectPoliceWeapon(runtime,officer,4);
}

bool storylandPolicePresence(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,const StorylandNativePoint& position,float radius) {
    if(!std::isfinite(radius)||radius<0)return false;
    const auto inside=[&](const StorylandNativePoint& point) {
        return std::hypot(point.x-position.x,point.y-position.y,point.z-position.z)<radius;
    };
    for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size();++index) {
        const auto& ped=runtime.pedestrians[index];
        if(ped.health>0&&ped.renderAlpha>0&&ped.state!=StorylandPedState::Dead&&!ped.zombie.active&&
            !ped.fortBaxterSoldier&&ped.isPolice(runtime.crime.vcs)&&inside(pedestrians[index].position))return true;
    }
    for(size_t index=0;index<runtime.vehiclePositions.size()&&index<runtime.vehicles.size();++index) {
        const auto& vehicle=runtime.vehicles[index];
        if(int(index)!=runtime.occupiedVehicle&&vehicle.health>0&&!vehicle.wrecked&&!vehicle.parked&&
            !vehicle.coopControlled&&vehicle.hasDriver&&storylandLawEnforcementRole(storylandVehicleServiceRole(runtime,vehicle))&&
            inside(runtime.vehiclePositions[index]))return true;
    }
    if(runtime.missionEnvironment)for(const auto& entry:runtime.missionEnvironment->missionCharacters) {
        const auto& ped=entry.second;
        if(ped.alive&&ped.health>0&&ped.visible&&!ped.removeFromWorld&&ped.type==6&&inside({ped.x,ped.y,ped.z}))return true;
    }
    return false;
}

void storylandUpdatePoliceCrime(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,const StorylandNativePoint& player,float seconds) {
    auto& crime=runtime.crime;
    crime.synchronize(runtime.wanted);
    crime.policePresence=[&](const StorylandNativePoint& position,float radius) {
        return storylandPolicePresence(runtime,pedestrians,position,radius);
    };
    crime.policeAtCrime=storylandPolicePresence(runtime,pedestrians,player,14.0f);
    crime.policeNearby=storylandPolicePresence(runtime,pedestrians,player,18.0f);
    crime.policeVisible=false;

    struct OfficerContact {
        size_t index=0;
        float distance=0;
        bool eligible=false;
        bool protectedPursuit=false;
    };
    std::vector<OfficerContact> contacts;
    contacts.reserve(std::min(pedestrians.size(),runtime.pedestrians.size()));
    const bool recentShot=runtime.elapsed>=runtime.playerLastGunshotTime&&
        runtime.elapsed-runtime.playerLastGunshotTime<2.0f;
    const bool alarmActive=playerCarAlarmActive(runtime);
    for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size();++index) {
        auto& ped=runtime.pedestrians[index];
        const auto& position=pedestrians[index].position;
        if(ped.fortBaxterSoldier||!ped.isPolice(crime.vcs))continue;
        ped.policeCanSeePlayer=false;
        if(ped.health<=0||ped.renderAlpha==0||ped.zombie.active) {
            if(ped.scriptHandle<0) {
                ped.policePursuing=false;
                ped.policeLastKnownPositionValid=false;
            }
            continue;
        }
        const float range=std::hypot(position.x-player.x,position.y-player.y,position.z-player.z);
        if(!std::isfinite(range))continue;
        // ScanForCrimes hears an active car alarm within twenty units. It does
        // not require sight, and an armed-but-silent 0xFFFF alarm is not active.
        // LCS 0x354C18 / VCS 0x326E18.
        if(alarmActive&&range<20.0f&&storylandPoliceOfficerInControl(runtime,ped))
            crime.setLcsWantedNoDrop(1,runtime.maxWanted);
        const float sightDistance=crime.vcs?storylandVcsPoliceResponse::OfficerSightDistance:
            storylandLcsPoliceResponse::OfficerSightDistance;
        const bool seen=range<=sightDistance&&storylandPoliceOfficerInControl(runtime,ped)&&
            storylandTraceNativeSegment(world,{position.x,position.y,position.z+1},
                {player.x,player.y,player.z+1})>=.99f;
        if(seen)crime.policeVisible=true;
        if(ped.scriptHandle>=0||ped.serviceVehicleIdentity)continue;
        const auto& soundPosition=runtime.playerLastGunshotPosition;
        const float soundRange=std::hypot(position.x-soundPosition.x,
            position.y-soundPosition.y,position.z-soundPosition.z);
        const bool heard=recentShot&&runtime.playerLastGunshotTime>ped.policeLastHeardAt&&
            storylandPoliceOfficerInControl(runtime,ped)&&soundRange<=
                (crime.vcs?storylandVcsPoliceResponse::OfficerHearingDistance:
                    storylandLcsPoliceResponse::OfficerHearingDistance);
        ped.policeCanSeePlayer=seen;
        if(heard) {
            ped.policeLastHeardAt=runtime.playerLastGunshotTime;
            if(ped.policeLastHeardAt>ped.policeLastSeenAt) {
                ped.policeLastKnownPosition=soundPosition;
                ped.policeLastKnownPositionValid=true;
            }
        }
        if(seen) {
            ped.policeLastSeenAt=runtime.elapsed;
            ped.policeLastKnownPosition=player;
            ped.policeLastKnownPositionValid=true;
        }
        const float lastContact=std::max(ped.policeLastSeenAt,ped.policeLastHeardAt);
        const bool remembered=ped.policePursuing&&ped.policeLastKnownPositionValid&&
            (crime.vcs?storylandVcsPoliceResponse::continueOfficer(range,runtime.elapsed,lastContact):
                storylandLcsPoliceResponse::continueOfficer(range,runtime.elapsed,lastContact));
        const bool eligible=(seen||heard||remembered)&&
            (ped.policePursuing||storylandPoliceOfficerInControl(runtime,ped));
        contacts.push_back({index,range,eligible,ped.policePursuing&&range<=10.0f});
    }
    if(alarmActive) {
        for(size_t index=0;index<runtime.vehicles.size()&&index<runtime.vehiclePositions.size();++index) {
            const auto& vehicle=runtime.vehicles[index];
            if(int(index)==runtime.occupiedVehicle||vehicle.coopControlled||vehicle.health<=0||
                vehicle.wrecked||!vehicle.hasDriver||
                !storylandLawEnforcementRole(storylandVehicleServiceRole(runtime,vehicle)))continue;
            const auto& position=runtime.vehiclePositions[index];
            if(std::hypot(position.x-player.x,position.y-player.y,position.z-player.z)<20.0f) {
                crime.setLcsWantedNoDrop(1,runtime.maxWanted);
                break;
            }
        }
    }
    runtime.wanted=crime.update(seconds,runtime.maxWanted,runtime.policeIgnorePlayer,runtime.wantedMultiplier);
    storylandUpdateLcsWantedSuspension(runtime);
    crime.policePresence={};

    const bool pursuing=runtime.wanted>0&&!runtime.policeIgnorePlayer&&runtime.health>0&&
        runtime.playerControl&&runtime.visibleArea==0;
    const unsigned available=storylandPoliceBudget(pursuing?runtime.wanted:0,crime.vcs).officers;
    // Retail keeps close incumbents, then replaces the farthest pursuer with a
    // nearer eligible officer. Police vehicles have their own wanted budget.
    std::stable_sort(contacts.begin(),contacts.end(),[&](const OfficerContact& a,const OfficerContact& b) {
        if(a.eligible!=b.eligible)return a.eligible;
        if(a.protectedPursuit!=b.protectedPursuit)return a.protectedPursuit;
        if(a.distance!=b.distance)return a.distance<b.distance;
        return runtime.pedestrians[a.index].policePursuing&&!runtime.pedestrians[b.index].policePursuing;
    });

    unsigned activated=0;
    for(const auto& contact:contacts) {
        auto& officer=runtime.pedestrians[contact.index];
        const bool engage=pursuing&&contact.eligible&&
            (activated<available||contact.protectedPursuit);
        if(engage) {
            ++activated;
            officer.policePursuing=true;
            officer.hostile=true;
            if(officer.fleeActive&&officer.fleeFromPlayer&&storylandPoliceOfficerInControl(runtime,officer))
                officer.clearFlee();
            if(!officer.fleeActive&&storylandPoliceOfficerInControl(runtime,officer))
                officer.state=StorylandPedState::Combat;
            storylandChoosePolicePursuitWeapon(runtime,officer);
        } else if(officer.policePursuing) {
            officer.policePursuing=false;
            officer.policeLastKnownPositionValid=false;
            officer.hostile=officer.personallyProvoked||officer.playOptionHostile;
            if(!officer.hostile) {
                officer.melee={};officer.ranged={};officer.fireFlash=0;
                if(officer.state==StorylandPedState::Combat)officer.state=StorylandPedState::Walk;
            }
        }
    }
}

void storylandDispatchPolice(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    std::vector<StorylandNativeTrafficActor>& pedestrians,const StorylandNativePoint& player,float cameraYaw,float seconds) {
    if(runtime.wanted<=0||runtime.policeIgnorePlayer||runtime.health<=0||!runtime.playerControl)return;
    const auto role=storylandWantedPoliceRole(runtime.wanted);
    size_t model=storylandChooseVehicleDriver(role,-1,runtime.pedestrianModelStats,runtime.pedestrianModelRoles,
        runtime.populationRandom);
    if(model==SIZE_MAX&&role!=StorylandServiceRole::Police)
        model=storylandChooseVehicleDriver(StorylandServiceRole::Police,-1,runtime.pedestrianModelStats,
            runtime.pedestrianModelRoles,runtime.populationRandom);
    if(model==SIZE_MAX)return;
    StorylandRuntimePed officer;officer.model=model;officer.stats=runtime.pedestrianModelStats[model];
    officer.serviceRole=runtime.pedestrianModelRoles[model];
    storylandInitializePoliceLoadout(runtime,officer);
    storylandDispatchFootPolice(runtime,world,runtime.livePaths,pedestrians,officer,player,cameraYaw,seconds);
}
