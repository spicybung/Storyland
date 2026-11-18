#include "src/storyland/storyland_play_test_weapons.h"
#include "src/storyland/storyland_blood_effects.h"
#include "src/storyland/storyland_character_sound_events.h"
#include "src/storyland/storyland_weapon_visuals.h"
#include "src/storyland/storyland_mission_behavior.h"
#include "src/storyland/storyland_sa_shot_accuracy.h"
#include "src/storyland/storyland_police.h"
#include "src/re/reVCS/services/storyland_vcs_police_assailant.h"
#include "src/storyland/storyland_native_random.h"
#include "src/storyland/storyland_mission_vm.h"
#include "src/re/reLCS2/pedestrians/storyland_lcs_ped_hit.h"
#include "src/re/reLCS2/vehicles/storyland_lcs_vehicle_access.h"
#include "src/re/reLCS2/vehicles/storyland_lcs_vehicle_damage.h"
#include "src/storyland/storyland_script_object.h"
#include "src/storyland/storyland_vehicle_systems.h"
#include "src/storyland/storyland_play_test_camera.h"
#include "src/storyland/storyland_play_test_script.h"
#include "src/re/reVCS/services/storyland_army_tolerance.h"
#include "src/storyland/storyland_weapon_animation_policy.h"
#include "src/re/reVCS/runtime/storyland_vcs_pad.h"
#include "src/storyland/storyland_water_cannon_math.h"
#include "src/storyland/storyland_swimming_runtime.h"
#include "src/re/reVCS/services/storyland_vcs_firetruck_cannon.h"
#include "src/re/reVCS/services/storyland_vcs_police_heli_weapon.h"
#include "src/re/reVCS/runtime/storyland_vcs_projectile.h"
#include "src/re/reVCS/runtime/storyland_vcs_projectile_owner.h"
#include "src/re/reVCS/world/storyland_vcs_water_depth.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_ped_damage_contact.h"
#include "src/re/reVCS/vehicles/storyland_vcs_seated_shooter.h"
#include "src/re/reVCS/runtime/storyland_vcs_npc_aim.h"
#include "src/re/reVCS/vehicles/storyland_vcs_vehicle_damage.h"
#include "src/re/reVCS/runtime/storyland_vcs_particle_emitter.h"
#include "src/re/reVCS/vehicles/storyland_vcs_hunter_cannon.h"
#include "src/re/reVCS/vehicles/storyland_vcs_hunter_rockets.h"
#include "src/re/reVCS/vehicles/storyland_vcs_vehicle_seamine.h"
#include "src/re/reVCS/gangs/storyland_vcs_extended_gang_combat.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <cctype>

static void storylandUpdateKillFrenzyGrant(StorylandPlayTestRuntime& runtime);
static void storylandRegisterKillFrenzyDeath(StorylandPlayTestRuntime& runtime,
    int32_t model,int32_t nativeWeapon,bool headshot,bool vehicle=false);

bool storylandCreateScriptParticles(StorylandPlayTestRuntime& runtime,const std::vector<uint8_t>& game,
    const std::vector<StorylandMissionEnvironment::ScriptParticle>& requests,std::string& error) {
    std::vector<StorylandNativeParticleDescriptor> descriptors;
    descriptors.reserve(requests.size());
    for(const auto& request:requests) {
        StorylandNativeParticleDescriptor descriptor;
        if(!storylandReadNativeParticleDescriptor(game,request.type,descriptor)) {
            error="Particle type has no supported native GAME.DTZ descriptor.";return false;
        }
        if(!std::isfinite(request.size)||request.size<0||request.size>10000||
            !std::all_of(request.position.begin(),request.position.end(),[](float value){return std::isfinite(value);})||
            !std::all_of(request.velocity.begin(),request.velocity.end(),[](float value){return std::isfinite(value)&&std::abs(value)<=100000;})) {
            error="Particle parameters exceed the finite host render domain.";return false;
        }
        descriptors.push_back(descriptor);
    }
    for(size_t index=0;index<requests.size()&&runtime.particles.size()<2048;++index) {
        const auto& request=requests[index];const auto& descriptor=descriptors[index];
        StorylandRuntimeParticle particle;
        particle.position={request.position[0],request.position[1],request.position[2]};
        // Native velocity, expansion and gravity use the 50Hz simulation step.
        particle.velocity={request.velocity[0]*50,request.velocity[1]*50,request.velocity[2]*50};
        particle.kind=descriptor.kind;particle.nativeType=request.type;particle.nativePhysics=true;
        particle.nativeVcs=runtime.crime.vcs;particle.nativeFirstFrame=descriptor.firstFrame;
        particle.size=request.size!=0?request.size:descriptor.size;
        particle.remaining=particle.lifetime=float(request.lifetimeMilliseconds?
            request.lifetimeMilliseconds:descriptor.lifetime)*.001f;
        particle.nativeColor=request.color[3]?request.color:descriptor.color;
        particle.nativeExpansion=descriptor.expansion;particle.nativeGravity=descriptor.gravity;
        particle.nativeFriction=descriptor.friction;particle.nativeFrame=descriptor.firstFrame;
        particle.nativeLastFrame=descriptor.lastFrame;particle.nativeFrameThreshold=descriptor.frameThreshold;
        particle.nativeFrameLoop=descriptor.frameLoop;
        storylandInitializeNativeParticlePresentation(particle,descriptor);
        if(particle.nativeVcs&&request.type>=75&&request.type<=77) {
            particle.swimmingParticle=true;particle.waterSurface=request.type!=75;
            particle.waterAnchoredSplash=request.type==75&&particle.velocity.x==0&&
                particle.velocity.y==0&&particle.velocity.z==0;
            particle.waterHeading=std::atan2(particle.velocity.y,particle.velocity.x);
            if(particle.waterSurface||particle.waterAnchoredSplash)particle.nativeGravity=0;
        }
        runtime.particles.push_back(particle);
    }
    return true;
}

static int storylandScriptEmitterParticle(uint16_t subtype) {
    switch(subtype) {
    case 0:case 2:case 4:return 30;
    case 1:case 3:return 31;
    case 6:return 88;
    case 8:return 44;
    case 12:return 20;
    case 13:return 21;
    case 17:return 58;
    case 18:return 40;
    default:return -1;
    }
}

bool storylandScriptEmitterAvailable(const std::vector<uint8_t>& game,uint16_t subtype,bool vcs) {
    if(subtype==8&&!vcs)return false;
    StorylandNativeParticleDescriptor descriptor;
    if(!storylandReadNativeParticleDescriptor(game,storylandScriptEmitterParticle(subtype),descriptor))return false;
    if(subtype==8)return storylandReadNativeParticleDescriptor(game,66,descriptor);
    return subtype!=18||storylandReadNativeParticleDescriptor(game,58,descriptor);
}

void storylandUpdateScriptParticleEmitters(StorylandPlayTestRuntime& runtime) {
    if(!runtime.missionEnvironment)return;
    auto& world=*runtime.missionEnvironment;
    if(!world.createScriptParticles)return;
    std::array<float,3> camera{},target{};
    const bool cameraKnown=world.activeCameraVectors&&world.activeCameraVectors(camera,target);
    const auto random=[&]() {return float(world.nextNativeRandom())/2147483648.0f;};
    auto& emitters=world.scriptParticleEmitters;
    emitters.erase(std::remove_if(emitters.begin(),emitters.end(),[&](auto& emitter) {
        const int subtype=emitter.particle.type;
        const float dx=emitter.particle.position[0]-camera[0],dy=emitter.particle.position[1]-camera[1];
        if(cameraKnown&&dx*dx+dy*dy>10000)return true;
        const bool expired=emitter.deadline!=0&&emitter.deadline<world.gameMilliseconds;
        const unsigned period=subtype==0||subtype==2||subtype==4?3u:subtype==6?2u:1u;
        const unsigned skipDivisor=subtype<=6?8u:subtype==17?2u:0u;
        if(++emitter.frameCounter<period)return expired;
        emitter.frameCounter=0;
        // Retail emits on nonzero remainder; this is not a one-in-N chance.
        if(skipDivisor&&(uint16_t(world.nextNativeRandom())%skipDivisor)==0)return expired;
        if(subtype==8) {
            if(!runtime.crime.vcs)return true;
            const auto particles=storylandVcsParticleSubtype8(emitter.particle,random);
            std::string error;
            if(world.createScriptParticles(particles,error))emitter.burstStarted=true;
            return expired;
        }
        auto particle=emitter.particle;
        particle.type=storylandScriptEmitterParticle(uint16_t(subtype));
        if(particle.type<0)return true;
        if(subtype==4)particle.color={16,16,16,255};
        if(subtype==12||subtype==13)particle.velocity={};
        if(subtype==17)for(float& value:particle.velocity)if(value!=0)value+=(random()*2-1)*.02f;
        if(subtype==18) {
            if(!emitter.burstStarted) {
                particle.size*=1.2f;for(float& value:particle.velocity)value*=1.2f;
            } else {
                particle.type=58;particle.size*=.2f;particle.color={80,80,80,255};
                for(float& value:particle.velocity)value*=1+(random()*2-1)*.35f;
            }
        }
        const unsigned count=subtype==6?2u:1u;
        std::string error;
        if(world.createScriptParticles(std::vector<StorylandMissionEnvironment::ScriptParticle>(count,particle),error))
            emitter.burstStarted=true;
        return expired;
    }),emitters.end());
}

void storylandResetDollarParticles(StorylandPlayTestRuntime& runtime) {
    runtime.particles.erase(std::remove_if(runtime.particles.begin(),runtime.particles.end(),
        [](const auto& particle){return particle.kind==StorylandParticleKind::Dollar;}),runtime.particles.end());
}

bool storylandCreateDollarParticles(StorylandPlayTestRuntime& runtime,uint8_t count,
    const StorylandMissionEnvironment::ScriptParticle& request,std::string& error) {
    if(!runtime.missionEnvironment||!runtime.missionEnvironment->dollarParticleEnabled) {
        error="Dollar particle manager is disabled.";return false;
    }
    // Retail uses a signed-byte loop counter with an unsigned-byte count;
    // values128..255 never terminate and are outside the supported domain.
    if(count>127||!std::isfinite(request.size)||std::abs(request.size)>10000||
        !std::all_of(request.position.begin(),request.position.end(),[](float value){return std::isfinite(value);})||
        !std::all_of(request.velocity.begin(),request.velocity.end(),[](float value){return std::isfinite(value)&&std::abs(value)<=100000;})) {
        error="Dollar emission parameters exceed the finite native/host domain.";return false;
    }
    auto& world=*runtime.missionEnvironment;
    const auto random=[&]() {return float(world.nextNativeRandom())/2147483648.0f;};
    const float magnitude=std::hypot(request.velocity[0],std::hypot(request.velocity[1],request.velocity[2]));
    const std::array<float,3> direction=magnitude>0?std::array<float,3>{request.velocity[0]/magnitude,
        request.velocity[1]/magnitude,request.velocity[2]/magnitude}:std::array<float,3>{1,0,0};
    const std::array<float,3> right{direction[1],-direction[0],0};
    for(unsigned index=0;index<count;++index) {
        size_t dollars=0;auto oldest=runtime.particles.end();
        for(auto item=runtime.particles.begin();item!=runtime.particles.end();++item)
            if(item->kind==StorylandParticleKind::Dollar) {++dollars;if(oldest==runtime.particles.end())oldest=item;}
        if(dollars>=50&&oldest!=runtime.particles.end())runtime.particles.erase(oldest);
        if(runtime.particles.size()>=2048)continue;
        const float spread=request.size!=0?(random()-.5f)*request.size:0;
        const float vertical=4*magnitude;
        StorylandRuntimeParticle particle;
        particle.position={request.position[0]+right[0]*spread,request.position[1]+right[1]*spread,request.position[2]};
        particle.velocity={request.velocity[0]*.9090909f-right[0]*.4f,
            request.velocity[1]*.9090909f-right[1]*.4f,vertical*.9090909f+(random()*2-1)*vertical/3};
        particle.kind=StorylandParticleKind::Dollar;particle.remaining=particle.lifetime=4;particle.size=.12f;
        runtime.particles.push_back(particle);
    }
    return true;
}

bool storylandMissionVehicleAimPoint(const StorylandPlayTestRuntime& runtime,int32_t handle,
    StorylandNativePoint& point) {
    if(!runtime.missionEnvironment)return false;
    const auto& world=*runtime.missionEnvironment;
    const auto found=world.missionCharacters.find(handle);
    if(found==world.missionCharacters.end())return false;
    const auto& character=found->second;
    if(!character.alive||character.health<=0||!character.threatVisible)return false;
    const bool vcs=runtime.crime.vcs;
    if(vcs&&world.vcsCharacterExitingVehicle(handle))return false;
    int32_t target=-1;
    if(character.objective==(vcs?0x00EC:0x01DE))target=character.objectiveTarget;
    else {
        const bool ownsTarget=vcs?(character.objective==0x00E4||character.objective==0x00E5||
            character.objective==0x00EB||character.objective==0x00F4):
            ((character.objective>=0x01CE&&character.objective<=0x01D1)||
                character.objective==0x01DD||character.objective==0x04CB);
        if(!ownsTarget&&character.behavior.forcedThreatIsVehicle)
            target=character.behavior.forcedThreat;
    }
    const auto vehicle=world.missionVehicles.find(target);
    if(vehicle==world.missionVehicles.end()||vehicle->second.deleted||!vehicle->second.alive)return false;
    const auto& state=vehicle->second;
    if(!std::isfinite(state.x)||!std::isfinite(state.y)||!std::isfinite(state.z))return false;
    point={state.x,state.y,state.z+.5f};
    return std::isfinite(point.z);
}

int32_t storylandCrouchWeaponAnimation(const StorylandPlayTestRuntime& runtime,int32_t handle) {
    if(!runtime.missionEnvironment)return 155;
    const auto& world=*runtime.missionEnvironment;
    const StorylandCrouchWeaponPose* pose=nullptr;
    const StorylandCrouchState* crouch=nullptr;
    int32_t selected=-1;
    if(handle==0) {
        if(runtime.health<=0||world.health<=0||world.inVehicle||world.activeVcsCharacterAttachment(0))return 155;
        pose=&runtime.playerCrouchPose;crouch=&world.playerCrouch;
        if(runtime.equippedWeapon<runtime.weapons.size())selected=runtime.weapons[runtime.equippedWeapon].nativeId;
    } else {
        const auto character=world.missionCharacters.find(handle);
        const auto combat=runtime.missionCombat.find(handle);
        if(character==world.missionCharacters.end()||combat==runtime.missionCombat.end()||
            !character->second.alive||character->second.health<=0||
            (character->second.vehicle>=0&&!character->second.attached)||
            (character->second.attached&&!runtime.crime.vcs))return 155;
        pose=&combat->second.crouchPose;crouch=&character->second.crouch;
        selected=character->second.currentWeapon;
        if(combat->second.nativeWeapon!=selected)return 155;
    }
    if(!crouch->active||pose->nativeWeapon!=selected||!pose->active())return 155;
    const auto weapon=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
        [&](const auto& definition){return definition.nativeId==selected;});
    if(weapon==runtime.weapons.end())return 155;
    if(pose->animation==215&&!(weapon->nativeFlags&0x20000u))return 155;
    if(pose->animation==217&&(!(weapon->nativeFlags&0x8000u)||!crouch->requested))return 155;
    return pose->animation;
}

float storylandCrouchWeaponPoseWeight(const StorylandCrouchWeaponPose& pose,uint32_t now) {
    if(!pose.active())return 0;
    if(pose.blendStartedAt)return std::clamp(1.0f-float(now-pose.blendStartedAt)*.004f,0.0f,1.0f);
    return std::clamp(float(now-pose.startedAt)*.008f,0.0f,1.0f);
}

static StorylandCrouchState* storylandLiveCrouch(StorylandPlayTestRuntime& runtime,int32_t handle) {
    if(!runtime.missionEnvironment)return nullptr;
    auto& world=*runtime.missionEnvironment;
    if(handle==0)return &world.playerCrouch;
    const auto character=world.missionCharacters.find(handle);
    return character==world.missionCharacters.end()?nullptr:&character->second.crouch;
}

void storylandAdvanceCrouchWeaponPose(StorylandPlayTestRuntime& runtime,int32_t handle,
    StorylandCrouchWeaponPose& pose,const StorylandWeaponDefinition& weapon,bool firing) {
    if(!pose.active()||!runtime.missionEnvironment)return;
    auto& world=*runtime.missionEnvironment;
    auto* crouch=storylandLiveCrouch(runtime,handle);
    if(!crouch||storylandCrouchWeaponAnimation(runtime,handle)!=pose.animation) {pose={};return;}
    const uint32_t now=world.gameMilliseconds;
    const float elapsed=float(now-pose.startedAt)*.001f+pose.offset;
    bool deleted=false;
    if(pose.animation==217)deleted=elapsed>=pose.duration;
    else if(!pose.firstShotPending&&!firing&&elapsed>=weapon.crouchLoopEnd) {
        if(!pose.blendStartedAt)pose.blendStartedAt=now;
        deleted=storylandCrouchWeaponPoseWeight(pose,now)<=0;
    }
    if(!deleted)return;
    const bool restore=crouch->active&&
        (pose.animation==215?(weapon.nativeFlags&0x20000u)!=0:
            crouch->requested&&(weapon.nativeFlags&0x8000u)!=0);
    if(restore) {
        const float duration=world.animationDuration?world.animationDuration(0,155):0;
        if(std::isfinite(duration)&&duration>0&&duration<60) {
            crouch->startedAt=now-uint32_t(std::ceil(duration*1000));
            crouch->poseAnimation=155;crouch->poseFade={0,8.0f,now,true};
        }
    }
    pose={};
}

bool storylandBeginCrouchReload(StorylandPlayTestRuntime& runtime,int32_t handle,
    StorylandCrouchWeaponPose& pose,const StorylandWeaponDefinition& weapon) {
    auto* crouch=storylandLiveCrouch(runtime,handle);
    if(!crouch||!crouch->active)return true;
    // Native weapon 30 reload keeps its existing association and ammo timer.
    if(weapon.nativeId==30)return true;
    if(!(weapon.nativeFlags&0x8000u)||!crouch->requested||!runtime.missionEnvironment)return false;
    const auto& world=*runtime.missionEnvironment;
    const float duration=world.animationDuration?world.animationDuration(weapon.animationGroup,217):0;
    if(!std::isfinite(duration)||duration<=0||duration>=60)return false;
    if(pose.animation==217)return false;
    pose={};pose.nativeWeapon=weapon.nativeId;pose.animation=217;
    pose.startedAt=world.gameMilliseconds;pose.duration=duration;
    crouch->poseFade={};
    return true;
}

int storylandPrepareCrouchShot(StorylandPlayTestRuntime& runtime,int32_t handle,
    StorylandCrouchWeaponPose& pose,const StorylandWeaponDefinition& weapon) {
    auto* crouch=storylandLiveCrouch(runtime,handle);
    if(!crouch||!crouch->active)return 1;
    if(!(weapon.nativeFlags&0x20000u)||!runtime.missionEnvironment||pose.animation==217)return -1;
    const auto& world=*runtime.missionEnvironment;
    const float duration=world.animationDuration?world.animationDuration(weapon.animationGroup,215):0;
    if(!std::isfinite(duration)||duration<=0||duration>=60||
        !std::isfinite(weapon.crouchLoopStart)||!std::isfinite(weapon.crouchLoopEnd)||
        !std::isfinite(weapon.crouchFireTime)||weapon.crouchLoopStart<0||
        weapon.crouchLoopEnd<=weapon.crouchLoopStart||weapon.crouchLoopEnd>duration||
        weapon.crouchFireTime<weapon.crouchLoopStart||weapon.crouchFireTime>weapon.crouchLoopEnd)return -1;
    const uint32_t now=world.gameMilliseconds;
    if(pose.animation!=215||pose.nativeWeapon!=weapon.nativeId||pose.blendStartedAt) {
        pose={};pose.nativeWeapon=weapon.nativeId;pose.animation=215;
        pose.startedAt=now;pose.duration=duration;pose.firstShotPending=true;
        crouch->poseFade={};
    } else if(!pose.firstShotPending) {
        const float elapsed=float(now-pose.startedAt)*.001f+pose.offset;
        if(elapsed<weapon.crouchLoopEnd)return 0;
        const float period=weapon.crouchLoopEnd-weapon.crouchLoopStart;
        pose.offset=weapon.crouchLoopStart+std::fmod(elapsed-weapon.crouchLoopStart,period);
        pose.startedAt=now;pose.firstShotPending=true;
    }
    const float elapsed=float(now-pose.startedAt)*.001f+pose.offset;
    if(elapsed<weapon.crouchFireTime)return 0;
    if(handle==0&&!runtime.playerMuzzleValid)return 0;
    if(handle!=0) {
        const auto combat=runtime.missionCombat.find(handle);
        if(combat==runtime.missionCombat.end()||!combat->second.anchorsValid)return 0;
    }
    pose.firstShotPending=false;pose.hasFired=true;
    return 1;
}

void storylandDropMissionCharacterPossessions(StorylandPlayTestRuntime& runtime,int32_t handle) {
    if(!runtime.missionEnvironment)return;
    auto& world=*runtime.missionEnvironment;
    const auto found=world.missionCharacters.find(handle);
    if(found==world.missionCharacters.end())return;
    auto& character=found->second;
    if(character.health>0&&character.alive)return;
    if(!character.weaponDropped) {
        character.weaponDropped=true;
        if(character.behavior.dropsWeapons) {
            const auto dropInventory=[&](const auto& inventory) {
                for(const auto& item:inventory) {
                    if(!item.second.owned||item.second.ammunition<=0||item.first==0)continue;
                    const auto definition=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
                        [&](const auto& weapon){return weapon.nativeId==item.first;});
                    if(definition==runtime.weapons.end()||runtime.pickups.size()>=64)continue;
                    runtime.pickups.push_back({{character.x,character.y,character.z},
                        size_t(definition-runtime.weapons.begin()),item.second.ammunition,0});
                }
            };
            if(runtime.crime.vcs)dropInventory(character.weapons);
            else {
                const auto inventory=world.characterWeapons.find(handle);
                if(inventory!=world.characterWeapons.end())dropInventory(inventory->second);
            }
        }
    }
    if(!runtime.crime.vcs)return;
    if(character.moneyDropped)return;
    character.moneyDropped=true;
    const int model=character.model;
    if((model>=8&&model<=13)||model==119||model==97||model==98||character.vehicle>=0||
        (character.creationSource==2&&!character.walletForced)||character.walletMoney<10)return;
    const auto random=[&]() {return world.nextNativeRandom();};
    const int count=std::min(int(character.walletMoney)/30+1,4);
    const int amount=int(character.walletMoney)/count;
    float x=character.x,y=character.y,z=character.z;
    for(int index=0;index<count;++index) {
        x+=std::sin(float(random()&255u)*.024543693f)*1.5f;
        y+=std::cos(float(random()&255u)*.024543693f)*1.5f;
        float ground=0;
        if(!world.groundHeightAt||!world.groundHeightAt({x,y,z+1},ground))continue;
        z=ground+.5f;
        const int value=amount+int(random()&3u);
        if(world.pickups.size()>=4096||world.nextPickupHandle==INT32_MAX)continue;
        StorylandMissionEnvironment::Pickup pickup;
        pickup.handle=world.nextPickupHandle++;pickup.type=8;
        pickup.model=world.modelIdByName?world.modelIdByName("money"):0;
        pickup.modelNameOverride="money";pickup.ammunition=value;
        pickup.x=x;pickup.y=y;pickup.z=z;pickup.area=world.area;
        pickup.expiresAt=world.gameMilliseconds+22000;
        world.pickups.push_back(std::move(pickup));
    }
    character.walletMoney=0;
}

namespace {
constexpr float radians=0.01745329251994329577f;
float separation(const StorylandNativePoint& a,const StorylandNativePoint& b) {
    return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);
}
StorylandNativePoint chest(StorylandNativePoint point) {point.z+=1.1f;return point;}
float randomUnit(StorylandPlayTestRuntime& runtime);
StorylandCrime lcsMeleeCrime(const StorylandWeaponDefinition& weapon) {
    return weapon.nativeId==5||weapon.nativeId==9||weapon.nativeId==10||weapon.nativeId==11?
        StorylandCrime::NastyAssault:StorylandCrime::Assault;
}
bool vcsMissionResidentSeated(const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::Character& character) {
    if (character.vehicle < 0 || character.attached) return false;
    const auto vehicle = world.missionVehicles.find(character.vehicle);
    return vehicle != world.missionVehicles.end() && !vehicle->second.deleted;
}
bool vcsPlayerPrimaryBulletCollision(const StorylandPlayTestRuntime& runtime,
    bool includeExposedOccupants) {
    if (!runtime.missionEnvironment) return true;
    const auto& world = *runtime.missionEnvironment;
    if (!world.inVehicle) return world.playerCollisionEnabled;
    const int slot = runtime.occupiedVehicle;
    if (slot < 0 || size_t(slot) >= runtime.vehicles.size()) return false;
    const auto& definition = runtime.vehicles[size_t(slot)].nativeDefinition;
    return definition.domain != StorylandVehicleDomain::Unknown && storylandVcsPedPrimaryBulletCollision(false, true,
        includeExposedOccupants, definition.nativeType, int32_t(definition.modelId));
}
bool vcsMissionPrimaryBulletCollision(const StorylandPlayTestRuntime& runtime,
    const StorylandMissionEnvironment::Character& character, bool includeExposedOccupants) {
    if (character.vehicle < 0 || character.attached) return character.collisionEnabled;
    if (!runtime.missionEnvironment || !vcsMissionResidentSeated(*runtime.missionEnvironment, character))
        return false;
    const auto& vehicle = runtime.missionEnvironment->missionVehicles.at(character.vehicle);
    if (vehicle.spawnedSlot < 0 || size_t(vehicle.spawnedSlot) >= runtime.vehicles.size()) return false;
    const auto& physical = runtime.vehicles[size_t(vehicle.spawnedSlot)];
    const auto& definition = physical.nativeDefinition;
    return physical.scriptHandle == character.vehicle && definition.domain != StorylandVehicleDomain::Unknown &&
        storylandVcsPedPrimaryBulletCollision(false, true, includeExposedOccupants,
            definition.nativeType, int32_t(definition.modelId));
}
void hurtMissionCharacter(StorylandPlayTestRuntime& runtime,int32_t handle,float damage,bool playerAttack,
    uint8_t damageKind,int32_t nativeWeapon=-1,StorylandMissionEnvironment::DamageSource source={},
    bool headHit=false,bool critical=false,bool approved=false,bool creditLocalPlayer=true,
    uint8_t hitPiece=0,uint8_t hitDirection=0) {
    if(!runtime.missionEnvironment||!std::isfinite(damage)||damage<=0)return;
    const auto found=runtime.missionEnvironment->missionCharacters.find(handle);
    if(found==runtime.missionEnvironment->missionCharacters.end())return;
    auto& character=found->second;
    if (!approved && runtime.routeMissionCharacterDamage) {
        StorylandMissionCharacterDamage request{handle, character.model, damage, playerAttack,
            damageKind, nativeWeapon, uint8_t(source.kind), source.handle, headHit, critical};
        request.hitPiece=headHit?6:hitPiece;request.hitDirection=hitDirection;
        if (runtime.routeMissionCharacterDamage(request)) return;
    }
    // CPlayerInfo::havoc: InflictDamage adds one before proof checks for
    // player damage greater than three; a player-attributed death adds ten.
    if (creditLocalPlayer && runtime.crime.vcs && playerAttack && damage > 3 && source.kind != StorylandMissionEnvironment::DamageSourceKind::Vehicle)
        ++runtime.missionEnvironment->playerHavoc;
    if(!character.alive||character.health<=0||
        (character.damageProofs&storylandNativePedDamageProof(runtime.crime.vcs,nativeWeapon,damageKind))||
        (character.onlyDamagedByPlayer&&!playerAttack&&
            nativeWeapon!=storylandNativeDrowningWeapon(runtime.crime.vcs)&&
            nativeWeapon!=storylandNativeExplosionWeapon(runtime.crime.vcs)&&
            (runtime.crime.vcs||damageKind!=4)))return;
    if (runtime.crime.vcs) {
        const bool seated = character.vehicle >= 0 && !character.attached;
        if (!storylandVcsPedDamageContactAllowed(character.collisionEnabled && !seated,
                vcsMissionResidentSeated(*runtime.missionEnvironment, character),
                character.drownsInWater, nativeWeapon)) return;
    } else if (damageKind == 1 && character.vehicle >= 0 && !character.attached &&
        !character.canBeShotInVehicle) {
        return;
    }
    if ((runtime.crime.vcs || damageKind == 1 || damageKind == 16) &&
        source.kind == StorylandMissionEnvironment::DamageSourceKind::Character && source.handle > 0) {
        const auto attacker = runtime.missionEnvironment->missionCharacters.find(source.handle);
        if (attacker != runtime.missionEnvironment->missionCharacters.end() &&
            !storylandMissionPedGangDamageAllowed(runtime.crime.vcs, character.gangDamageMask,
                attacker->second.type)) return;
    }
    if (runtime.crime.vcs && damageKind == 1)
        damage = storylandMissionPedInstantHitDamage(character, nativeWeapon, damage);
    if (runtime.crime.vcs && damageKind == 16 && source.kind == StorylandMissionEnvironment::DamageSourceKind::Character && source.handle > 0) {
        const auto attacker = runtime.missionEnvironment->missionCharacters.find(source.handle);
        if (attacker != runtime.missionEnvironment->missionCharacters.end() && attacker->second.pedStat >= 0)
            damage *= attacker->second.behavior.statAttackStrength;
    }
    float weakness=character.pedStat>=0?character.behavior.statDefendWeakness:1.0f;
    if(!runtime.crime.vcs) {
        if(!character.pedStats.valid&&runtime.missionEnvironment->resolvePedStats)
            runtime.missionEnvironment->resolvePedStats(character.model,-1,character.pedStats);
        weakness=character.pedStats.valid?character.pedStats.defendWeakness:1.0f;
    }
    const uint8_t piece=headHit?6:hitPiece;
    const bool enabled=runtime.crime.vcs?character.criticalHits:character.suffersCriticalHits;
    critical=critical&&enabled&&damageKind==1;
    if(damageKind==1&&enabled&&!critical) {
        if(storylandNativePedCriticalNeedsRandom(runtime.crime.vcs,nativeWeapon))randomUnit(runtime);
        critical=storylandNativePedCriticalRoll(runtime.crime.vcs,nativeWeapon,enabled,false,runtime.combatRandom);
    }
    const auto result=storylandApplyNativePedDamage(character.health,character.armour,damage,false,weakness,
        storylandNativeDamageBypassesArmour(runtime.crime.vcs,nativeWeapon),critical&&piece>=2&&piece<=6);
    if(!result.accepted)return;
    character.armour=result.armour;character.health=result.health;
    if (damageKind == 1 && storylandMissionBulletReactionAllowed(runtime,handle)) {
        auto& combat = runtime.missionCombat[handle];
        StorylandBulletImpact impact;
        impact.weapon=nativeWeapon;impact.canKnockDown=character.canBeShotInVehicle;
        impact.heading=character.heading+90;impact.speed=character.movementSpeed;
        impact.verticalSpeed=combat.verticalSpeed;
        if(character.nativePedRandomSeedInitialized) {
            combat.bulletReaction.entitySeed=character.nativePedRandomSeed;
            combat.bulletReaction.entitySeedInitialized=true;
        }
        if(playerAttack||(source.kind==StorylandMissionEnvironment::DamageSourceKind::Character&&
            source.handle==(runtime.crime.vcs?0:1)))
            impact.sourceDirection={runtime.playerPosition.x-character.x,runtime.playerPosition.y-character.y,0};
        else if(source.kind==StorylandMissionEnvironment::DamageSourceKind::Character) {
            const auto attacker=runtime.missionEnvironment->missionCharacters.find(source.handle);
            if(attacker!=runtime.missionEnvironment->missionCharacters.end())
                impact.sourceDirection={attacker->second.x-character.x,attacker->second.y-character.y,0};
        } else if(source.kind==StorylandMissionEnvironment::DamageSourceKind::Vehicle) {
            const auto attacker=runtime.missionEnvironment->missionVehicles.find(source.handle);
            if(attacker!=runtime.missionEnvironment->missionVehicles.end())
                impact.sourceDirection={attacker->second.x-character.x,attacker->second.y-character.y,0};
        }
        if(storylandBeginBulletReaction(runtime,combat.bulletReaction,hitDirection,impact)) {
            combat.melee={};combat.contact.clear();combat.crouchPose={};
            character.movementSpeed=0;
            if(combat.bulletReaction.knockdown) {
                combat.grounded=false;combat.verticalSpeed=combat.bulletReaction.fall.velocity[2];
                combat.fall={};combat.landingTime=0;
            }
        }
    }
    character.lastWeaponDamage=nativeWeapon;
    if(source.kind==StorylandMissionEnvironment::DamageSourceKind::None&&playerAttack)
        source={StorylandMissionEnvironment::DamageSourceKind::Character,runtime.crime.vcs?0:1};
    character.lastDamageSource=source;
    if(playerAttack&&!storylandGangPlayerProtected(runtime,character.type))
        character.provokedByPlayer=true;
    if(character.health<=0) {
        storylandRetireBulletReaction(runtime.missionCombat[handle].bulletReaction);
        character.walkingToTarget=false;
        if(!character.deathRecorded) {
            character.deathStartedAt=runtime.missionEnvironment->gameMilliseconds;
            character.deathRecorded=true;
        }
        const auto death=storylandNativeDeathAssociation(runtime.crime.vcs,damageKind,nativeWeapon,piece,hitDirection,critical);
        character.animationId=death.animation;character.animationGroup=0;character.animationBlend=death.blendRate;
        character.animationStartedAt=character.deathStartedAt;character.animationLoop=false;
        runtime.missionEnvironment->scriptedCharacterAnimations.erase(handle);
        const auto car=runtime.missionEnvironment->missionVehicles.find(character.vehicle);
        const bool canExit=character.vehicle>=0&&!character.attached&&
            car!=runtime.missionEnvironment->missionVehicles.end()&&!car->second.deleted&&
            (car->second.doorLock==0||car->second.doorLock==1);
        if(canExit) {
            character.behavior.leavingVehicleToDie=true;
            character.behavior.dyingExitStartedAt=runtime.missionEnvironment->gameMilliseconds;
            character.objective=runtime.crime.vcs?0x00F1:0x03E7;
            character.objectiveTarget=character.vehicle;
            if(!runtime.crime.vcs) {
                StorylandLcsEntityExtensionState::Task exit;
                exit.objective=16;exit.target=character.vehicle;
                runtime.missionEnvironment->lcsEntityExtension.tasks[handle]=exit;
            }
        } else character.alive=false;
        if(creditLocalPlayer&&playerAttack&&!character.playerKillCounted) {
            runtime.recordCloseExecutionBlood({character.x,character.y,character.z},
                damageKind,headHit,true);
            character.playerKillCounted=true;
            if(runtime.crime.vcs)runtime.missionEnvironment->playerHavoc += 10u;
            ++runtime.modelsKilledByPlayer[character.model];
            if(runtime.crime.vcs)
                StorylandFortBaxterRetailResponse::recordSoldierDeath(runtime.fortBaxterKills,
                    runtime.fortBaxterPendingDeaths,character.model);
            runtime.statistics["PeopleKilledByPlayer"]+=1;
            if(!runtime.crime.vcs) {
                runtime.statistics["KillsSinceLastCheckpoint"]+=1;
                storylandRecordLcsPedTypeKill(runtime.statistics,character.type,character.isChrisCriminal);
            }
            storylandRegisterKillFrenzyDeath(runtime,character.model,nativeWeapon,
                damageKind==1&&headHit);
        }
    }
    if(!runtime.crime.vcs||character.health<=0||nativeWeapon==storylandNativeFallWeapon(true)) {
        StorylandPlayTestSoundEvent reaction(StorylandPlayTestSound::Damage);
        reaction.speaker=int(runtime.pedestrians.size())+handle;
        reaction.position={character.x,character.y,character.z};reaction.positioned=true;
        reaction.weapon=nativeWeapon;reaction.fatal=character.health<=0;
        if(runtime.crime.vcs)reaction.nativeSpeech=storylandCharacterDamageSpeech(true,nativeWeapon,reaction.fatal);
        storylandStampCharacterSoundEvent(runtime,reaction);
        runtime.soundEvents.push_back(reaction);
    }
    const bool bloodAllowed=character.bleeding&&
        storylandBloodDamageAllowed(runtime.crime.vcs,damageKind,nativeWeapon);
    if(character.health<=0)runtime.missionCombat[handle].corpseBlood.suppressPool=!bloodAllowed;
    if(bloodAllowed&&result.applied>result.absorbed) {
        const auto& combat=runtime.missionCombat[handle];
        storylandMarkBloodRunover(runtime,{character.x,character.y,character.z},nativeWeapon);
        const StorylandNativePoint impact=combat.anchorsValid?(headHit?combat.head:combat.torso):
            StorylandNativePoint{character.x,character.y,character.z+(headHit?1.6f:1.05f)};
        StorylandNativePoint origin=runtime.playerMuzzleValid?runtime.playerMuzzle:
            StorylandNativePoint{runtime.playerPosition.x,runtime.playerPosition.y,runtime.playerPosition.z+1.05f};
        const StorylandNativePoint* sourcePosition=playerAttack?&origin:nullptr;
        if(source.kind==StorylandMissionEnvironment::DamageSourceKind::Character&&source.handle>0) {
            const auto attacker=runtime.missionEnvironment->missionCharacters.find(source.handle);
            if(attacker!=runtime.missionEnvironment->missionCharacters.end()) {
                origin={attacker->second.x,attacker->second.y,attacker->second.z+1.0f};
                sourcePosition=&origin;
            }
        }
        storylandEmitBloodImpact(runtime,impact,result.applied-result.absorbed,sourcePosition,headHit);
    }
}
void hurtMissionVehicle(StorylandPlayTestRuntime& runtime,size_t slot,float damage,uint8_t damageKind,
    int32_t nativeWeapon,StorylandMissionEnvironment::DamageSource source) {
    if(slot>=runtime.vehicles.size()||!std::isfinite(damage)||damage<=0)return;
    auto& vehicle=runtime.vehicles[slot];
    const bool player=source.kind==StorylandMissionEnvironment::DamageSourceKind::Character&&
        source.handle==(runtime.crime.vcs?0:1);
    if (player && runtime.coopConnected && !runtime.coopFriendlyFire && vehicle.coopControlled) return;
    if(vehicle.health<=0||!vehicle.canBeDamaged||(vehicle.damageProofs&damageKind)||
        (vehicle.onlyDamagedByPlayer&&!player))return;
    if(runtime.crime.vcs) {
        int32_t status=int(slot)==runtime.occupiedVehicle?0:3;
        if(runtime.missionEnvironment) {
            const auto found=runtime.missionEnvironment->missionVehicles.find(vehicle.scriptHandle);
            if(found!=runtime.missionEnvironment->missionVehicles.end()&&found->second.scriptStatus>=0)
                status=found->second.scriptStatus;
        }
        if(!storylandVcsVehicleWeaponDamageAdmitted(nativeWeapon,status,vehicle.health))return;
    }
    if (damageKind == 8) {
        damage = storylandVehicleCollisionDamage(vehicle, damage, int(slot) == runtime.occupiedVehicle);
    }
    vehicle.health=std::max(0.0f,vehicle.health-damage);
    if(player&&vehicle.health<=0)
        storylandRegisterKillFrenzyDeath(runtime,int32_t(vehicle.nativeDefinition.modelId),nativeWeapon,false,true);
    if (damage > 0) {
        vehicle.visiblyDamaged = true;
        if (vehicle.doorDamage[0] == 0) vehicle.doorDamage[0] = 1;
    }
    if(runtime.missionEnvironment) {
        const auto found=runtime.missionEnvironment->missionVehicles.find(vehicle.scriptHandle);
        if(found!=runtime.missionEnvironment->missionVehicles.end()) {
            found->second.health=vehicle.health;found->second.alive=vehicle.health>0;
            found->second.visiblyDamaged = vehicle.visiblyDamaged;
            found->second.doorDamage = vehicle.doorDamage;
            found->second.lastWeaponDamage=nativeWeapon;found->second.lastDamageSource=source;
        }
    }
}
int traceMissionVehicles(const StorylandPlayTestRuntime& runtime,const StorylandNativePoint& start,
    const StorylandNativePoint& end,float& nearest,StorylandNativePoint& normal,int ignoredVehicle=-1) {
    int result=-1;
    for(size_t index=0;index<runtime.vehicles.size();++index) {
        const auto& vehicle=runtime.vehicles[index];
        if(int(index)==ignoredVehicle||!vehicle.collisionEnabled||!vehicle.nativeDefinition.collision)continue;
        std::array<float,16> matrix{};
        if(index<runtime.vehicleWorldMatrices.size())matrix=runtime.vehicleWorldMatrices[index];
        else if(!runtime.missionEnvironment||!runtime.missionEnvironment->vehicleWorldMatrix||
            !runtime.missionEnvironment->vehicleWorldMatrix(vehicle.scriptHandle,matrix))continue;
        if(storylandTraceCollisionModel(*vehicle.nativeDefinition.collision,matrix,start,end,nearest,normal))
            result=int(index);
    }
    return result;
}
#include "src/re/reVCS/vehicles/storyland_vcs_vehicle_seamine_runtime.inl"

struct VehicleOccupantShot {
    int32_t character=-1;
    float fraction=1;
    uint8_t piece=0;
    int32_t ambient=-1;
};
VehicleOccupantShot traceVcsVehicleOccupantHead(const StorylandPlayTestRuntime& runtime,
    const StorylandNativePoint& start,const StorylandNativePoint& end,int vehicleSlot,
    int32_t ignoredCharacter) {
    VehicleOccupantShot result;
    if(!runtime.crime.vcs||!runtime.missionEnvironment||vehicleSlot<0||
        size_t(vehicleSlot)>=runtime.vehicles.size())return result;
    const auto& world=*runtime.missionEnvironment;
    const int32_t handle=runtime.vehicles[size_t(vehicleSlot)].scriptHandle;
    std::vector<std::pair<int32_t,int32_t>> occupants;
    if(ignoredCharacter!=0&&world.inVehicle&&runtime.occupiedVehicle==vehicleSlot&&
        world.playerCanBeShotInVehicle&&runtime.playerHeadValid)
        occupants.emplace_back(runtime.playerVehicleSeat,0);
    const auto vehicle=world.missionVehicles.find(handle);
    if(vehicle!=world.missionVehicles.end()&&!vehicle->second.deleted) {
        for(const auto& entry:world.missionCharacters) {
            const auto& character=entry.second;
            if(entry.first==ignoredCharacter||character.vehicle!=handle||character.attached||
                character.removeFromWorld||!character.canBeShotInVehicle)continue;
            if(character.passengerSeat<0&&vehicle->second.driver!=entry.first)continue;
            const auto pose=runtime.missionCombat.find(entry.first);
            if(pose!=runtime.missionCombat.end()&&pose->second.anchorsValid)
                occupants.emplace_back(character.passengerSeat,entry.first);
        }
    }
    std::sort(occupants.begin(),occupants.end());
    float nearest=1;
    for(const auto& occupant:occupants) {
        const auto head=occupant.second==0?runtime.playerHead:runtime.missionCombat.at(occupant.second).head;
        if(!std::isfinite(head.x)||!std::isfinite(head.y)||!std::isfinite(head.z))continue;
        if(storylandVcsVehicleHeadIntersection({start.x,start.y,start.z},{end.x,end.y,end.z},
            {head.x,head.y,head.z},nearest))result={occupant.second,nearest,6};
    }
    const uint64_t crew=runtime.vehicles[size_t(vehicleSlot)].serviceCrewIdentity;
    if(crew)for(size_t index=0;index<runtime.pedestrians.size();++index) {
        const auto& passenger=runtime.pedestrians[index];
        if(passenger.serviceVehicleIdentity!=crew||passenger.health<=0||!passenger.headValid||
            !std::isfinite(passenger.head.x)||!std::isfinite(passenger.head.y)||!std::isfinite(passenger.head.z))continue;
        if(storylandVcsVehicleHeadIntersection({start.x,start.y,start.z},{end.x,end.y,end.z},
            {passenger.head.x,passenger.head.y,passenger.head.z},nearest))
            result={-1,nearest,6,int32_t(index)};
    }
    return result;
}
VehicleOccupantShot traceVehicleOccupantShot(const StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const std::vector<StorylandNativeTrafficActor>& pedestrians,
    const StorylandNativePoint& start,const StorylandNativePoint& end,int vehicleSlot,
    int32_t ignoredCharacter,int ignoredPedestrian) {
    VehicleOccupantShot result;
    if(!world.paths.vcs||!runtime.missionEnvironment||vehicleSlot<0||
        size_t(vehicleSlot)>=runtime.vehicles.size())return result;
    const auto& mission=*runtime.missionEnvironment;
    const int32_t vehicle=runtime.vehicles[size_t(vehicleSlot)].scriptHandle;
    if(vehicle<0)return result;
    float nearest=storylandTraceNativeSegment(world,start,end);
    StorylandNativePoint normal{};
    for(const auto& object:mission.objects)
        storylandTraceScriptObject(world,object,start,end,nearest,normal);
    traceMissionVehicles(runtime,start,end,nearest,normal,vehicleSlot);
    const auto sphere=[&](const StorylandNativePoint& center,float radius,
        int32_t candidate,uint8_t piece) {
        const float dx=end.x-start.x,dy=end.y-start.y,dz=end.z-start.z;
        const float ox=start.x-center.x,oy=start.y-center.y,oz=start.z-center.z;
        const float a=dx*dx+dy*dy+dz*dz,b=ox*dx+oy*dy+oz*dz;
        const float c=ox*ox+oy*oy+oz*oz-radius*radius;
        const float discriminant=b*b-a*c;
        if(a<.000001f||discriminant<0)return;
        const float fraction=c<=0?0.0f:(-b-std::sqrt(discriminant))/a;
        if(fraction<0||fraction>=nearest)return;
        nearest=fraction;result={candidate,fraction,piece};
    };
    for(size_t index=0;index<pedestrians.size()&&index<runtime.pedestrians.size();++index) {
        const auto& ped=runtime.pedestrians[index];
        if(int(index)==ignoredPedestrian||ped.health<=0||ped.scriptHandle>=0)continue;
        sphere(chest(pedestrians[index].position),.4f,-1,0);
        if(ped.headValid)sphere(ped.head,.18f,-1,6);
    }
    if(ignoredCharacter!=0&&runtime.health>0) {
        const bool seated=mission.inVehicle;
        const bool selected=seated&&mission.occupiedMissionVehicle==vehicle&&mission.playerTestShotInVehicle;
        if(selected||(!seated&&mission.playerCollisionEnabled)) {
            sphere(runtime.playerTorsoValid?runtime.playerTorso:chest(runtime.playerPosition),
                .4f,selected?0:-1,0);
        }
    }
    for(const auto& entry:mission.missionCharacters) {
        const auto& character=entry.second;
        if(entry.first==ignoredCharacter||!character.alive||character.health<=0||
            !character.visible||character.removeFromWorld)continue;
        const bool seated=character.vehicle>=0&&!character.attached;
        const bool selected=seated&&character.vehicle==vehicle&&character.testShotInVehicle;
        if(!selected&&(seated||!character.collisionEnabled))continue;
        std::vector<StorylandMissionEnvironment::CharacterHitSphere> nativeSpheres;
        const bool native=selected&&mission.characterHitSpheres&&
            mission.characterHitSpheres(entry.first,nativeSpheres);
        if(native) {
            for(const auto& volume:nativeSpheres) {
                if(!std::isfinite(volume.radius)||volume.radius<=0)continue;
                if(storylandVcsPedSphereIntersection({start.x,start.y,start.z},
                    {end.x,end.y,end.z},volume.center,volume.radius,nearest))
                    result={entry.first,nearest,volume.piece};
            }
        } else {
            const auto pose=runtime.missionCombat.find(entry.first);
            if(character.crouch.active&&(pose==runtime.missionCombat.end()||!pose->second.anchorsValid))continue;
            const auto center=pose!=runtime.missionCombat.end()&&pose->second.anchorsValid?
                pose->second.torso:chest({character.x,character.y,character.z});
            sphere(center,.4f,selected?entry.first:-1,0);
            if(pose!=runtime.missionCombat.end()&&pose->second.anchorsValid)
                sphere(pose->second.head,.18f,selected?entry.first:-1,6);
        }
    }
    return result;
}
bool visible(const StorylandNativeWorld& world,const StorylandNativePoint& a,const StorylandNativePoint& b) {
    return storylandTraceNativeSegment(world,chest(a),chest(b))>=0.995f;
}
bool lcsPolicePresence(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& actors,const StorylandNativePoint& position,float radius) {
    const auto policeModel=[](uint32_t model) {return model>=1&&model<=4;};
    for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index) {
        if(policeModel(runtime.pedestrians[index].stats.modelId)&&
            separation(position,actors[index].position)<radius)return true;
    }
    if(runtime.missionEnvironment) {
        for(const auto& entry:runtime.missionEnvironment->missionCharacters) {
            const auto& character=entry.second;
            if(policeModel(uint32_t(character.model))&&
                separation(position,{character.x,character.y,character.z})<radius)return true;
        }
    }
    for(size_t index=0;index<runtime.vehiclePositions.size()&&index<runtime.vehicles.size();++index) {
        const auto& vehicle=runtime.vehicles[index];
        const uint32_t model=vehicle.nativeDefinition.modelId;
        const bool police=model==157||model==158||model==196||model==199;
        const bool activePoliceTraffic=vehicle.scriptHandle<0&&vehicle.generator<0&&!vehicle.parked&&
            (vehicle.nativeDefinition.vehicleClass==11||vehicle.nativeDefinition.vehicleClass==21);
        if(police&&activePoliceTraffic&&int(index)!=runtime.occupiedVehicle&&vehicle.hasDriver&&vehicle.health>0&&
            separation(position,runtime.vehiclePositions[index])<radius)return true;
    }
    return false;
}
float randomUnit(StorylandPlayTestRuntime& runtime) {
    return storylandNativeRandomUnit(runtime.combatRandom);
}
void finishLcsGunThreat(StorylandPlayTestRuntime& runtime,StorylandRuntimePed& ped) {
    ped.threatWaiting=false;ped.threatTimeRemaining=0;
    if(ped.health<=0||ped.water.active||ped.fleeActive||ped.state==StorylandPedState::Combat||
        !ped.stats.valid||!runtime.playerStats.valid)return;
    const bool frightened=int(ped.stats.fear)>100-int(runtime.playerStats.temper);
    const bool melee=ped.weapon>=runtime.weapons.size()||runtime.weapons[ped.weapon].fireType==0;
    if(frightened||(melee&&(ped.stats.flags&0x80))) {
        ped.hostile=false;ped.startFlee(runtime.playerPosition,6,true);
        ped.fleeWalking=!frightened;
    } else if(!melee) {
        ped.hostile=true;ped.clearFlee();ped.state=StorylandPedState::Combat;
    } else {
        ped.hostile=false;ped.clearFlee();ped.state=StorylandPedState::Walk;
    }
}
void updateLcsGunThreats(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    std::vector<StorylandNativeTrafficActor>& actors,const StorylandNativePoint& player,float seconds) {
    if(world.paths.vcs)return;
    const bool pointing=runtime.health>0&&runtime.playerControl&&runtime.occupiedVehicle<0&&
        runtime.targetedPed>=0&&storylandHasPlayTestTarget(runtime)&&runtime.equippedWeapon<runtime.weapons.size()&&
        runtime.weapons[runtime.equippedWeapon].fireType!=0;
    std::array<bool,15> finished{};
    for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index) {
        auto& ped=runtime.pedestrians[index];
        if(ped.threatAnimation<0)continue;
        if(ped.health<=0||ped.water.active) {ped.clearGunThreat();continue;}
        ped.threatAnimationTime+=seconds;
        ped.threatTimeRemaining-=seconds;
        const bool naturalFade=ped.threatAnimation==158&&ped.threatAnimationTime>=ped.threatClipDuration;
        if(ped.threatWaiting&&(!pointing||ped.threatTimeRemaining<0||ped.fleeActive||
            ped.state==StorylandPedState::Combat)) {
            finishLcsGunThreat(runtime,ped);finished[index]=true;
        }
        if(ped.threatWaiting) {
            // CPed::TurnBody turns twenty percent of the shortest angular
            // error per call; its 0.05-radian deadband does not snap the body.
            auto& actor=actors[index];
            const float desired=std::atan2(player.y-actor.position.y,player.x-actor.position.x)/radians;
            const float turn=std::remainder(desired-actor.yaw,360.0f);
            if(std::fabs(turn)*radians>.05f)actor.yaw+=turn*.2f;
        }
        ped.threatBlend=std::clamp(ped.threatBlend+(ped.threatWaiting&&!naturalFade?4.0f:-4.0f)*seconds,0.0f,1.0f);
        if(ped.threatBlend==0&&(!ped.threatWaiting||naturalFade)) {
            if(ped.threatWaiting)finishLcsGunThreat(runtime,ped);
            ped.clearGunThreat();finished[index]=true;
        }
    }
    if(!pointing||size_t(runtime.targetedPed)>=actors.size()||
        size_t(runtime.targetedPed)>=runtime.pedestrians.size())return;
    const size_t index=size_t(runtime.targetedPed);
    auto& ped=runtime.pedestrians[index];
    if(finished[index]||ped.threatWaiting||ped.threatAnimation>=0||ped.health<=0||ped.water.active||
        !ped.stats.valid||ped.stats.pedType==0||ped.stats.pedType==13||
        std::hypot(actors[index].position.x-player.x,actors[index].position.y-player.y)>15.0f)return;
    const bool melee=ped.weapon>=runtime.weapons.size()||runtime.weapons[ped.weapon].fireType==0;
    const bool gunPanic=(ped.stats.flags&0x80)&&(!ped.hostile||melee)&&
        !(ped.fleeActive&&ped.fleeFromPlayer);
    if(!gunPanic&&ped.isPolice(false)) {
        const int wanted=std::max(runtime.wanted,std::min(2,runtime.maxWanted));
        runtime.crime.synchronize(wanted);runtime.wanted=wanted;
        return;
    }
    if(!gunPanic&&(ped.stats.pedType==runtime.playerStats.pedType||ped.hostile||
        (ped.fleeActive&&ped.fleeFromPlayer)))return;
    const int32_t animation=gunPanic?158:157;
    const float duration=runtime.missionEnvironment&&runtime.missionEnvironment->animationDuration?
        runtime.missionEnvironment->animationDuration(0,animation):0;
    if(!std::isfinite(duration)||duration<=0)return;
    const uint32_t milliseconds=3000+uint32_t(randomUnit(runtime)*3000.0f);
    ped.clearFlee();ped.threatAnimation=animation;ped.threatAnimationTime=0;
    ped.threatTimeRemaining=float(milliseconds)*.001f;ped.threatClipDuration=duration;
    ped.threatBlend=0;ped.threatWaiting=true;ped.state=StorylandPedState::Idle;
    ped.combatSpeed=0;ped.melee={};
    StorylandPlayTestSoundEvent event(StorylandPlayTestSound::Damage);
    event.nativeSpeech=gunPanic?0x77:0x76;event.speaker=int(index);
    storylandStampCharacterSoundEvent(runtime,event);
    event.position=actors[index].position;event.positioned=true;runtime.soundEvents.push_back(event);
}
struct LcsRangedAnimation {
    int32_t fireId=201;
    float duration=0,loopStart=0,loopEnd=0,fireTime=0,reloadDuration=0,reloadSoundTime=0;
};
bool lcsRangedAnimation(const StorylandPlayTestRuntime& runtime,const StorylandRuntimePed& ped,
    const StorylandWeaponDefinition& weapon,LcsRangedAnimation& result) {
    if(weapon.fireType!=1||weapon.animationGroup<0||(weapon.nativeFlags&(0x1000u|0x200000u))||
        !runtime.missionEnvironment||!runtime.missionEnvironment->weaponAnimationDuration)return false;
    const bool police=ped.isPolice(false)&&(weapon.nativeFlags&0x40000u);
    result.fireId=police?205:201;
    result.loopStart=police?11.0f/30.0f:weapon.animationLoopStart;
    result.loopEnd=police?19.0f/30.0f:weapon.animationLoopEnd;
    result.fireTime=police?14.0f/30.0f:weapon.animationFireTime;
    result.duration=runtime.missionEnvironment->weaponAnimationDuration(weapon.animationGroup,result.fireId);
    if(!std::isfinite(result.duration)||!std::isfinite(result.loopStart)||
        !std::isfinite(result.loopEnd)||!std::isfinite(result.fireTime)||
        result.duration<=0||result.loopStart<0||result.loopEnd<=result.loopStart||
        result.loopEnd>result.duration||result.fireTime<result.loopStart||result.fireTime>result.loopEnd||
        result.fireTime>=result.duration)return false;
    if(weapon.nativeFlags&0x8000u) {
        const float duration=runtime.missionEnvironment->weaponAnimationDuration(weapon.animationGroup,203);
        if(std::isfinite(duration)&&duration>0) {
            result.reloadDuration=duration;
            // CWeapon::Update synchronizes the sample with the running reload
            // association: Python uses 50%, Colt/TEC/M60 70%, and other groups 75%.
            const float fraction=weapon.animationGroup==13?.5f:
                weapon.animationGroup==14||weapon.animationGroup==17||weapon.animationGroup==20?.7f:.75f;
            result.reloadSoundTime=duration*fraction;
        }
    }
    return true;
}
void cancelLcsRangedAttack(StorylandPlayTestRuntime& runtime,StorylandRuntimePed& ped,float seconds) {
    auto& attack=ped.ranged;
    if(!attack.active)return;
    if(ped.health<=0||ped.water.active||ped.weapon>=runtime.weapons.size()||attack.weapon!=ped.weapon) {
        attack={};return;
    }
    const bool reload=attack.reloadPending||attack.reloading;
    if(reload&&ped.reloading<0&&ped.magazine<=0) {
        ped.magazine=std::min(runtime.weapons[ped.weapon].clipSize,ped.ammunition);
        ped.reloading=0;
    }
    attack.cancelling=true;
    attack.blend=std::max(0.0f,attack.blend-8.0f*seconds);
    if(attack.blend==0&&(!reload||ped.magazine>0||ped.ammunition<=0))attack={};
}
void sound(StorylandPlayTestRuntime& runtime,StorylandPlayTestSound type,
    StorylandNativePoint position,int weapon=-1,int speaker=-1) {
    StorylandPlayTestSoundEvent event(type);event.position=position;event.weapon=weapon;event.positioned=true;event.speaker=speaker;
    storylandStampCharacterSoundEvent(runtime,event);
    runtime.soundEvents.push_back(event);
}
void particle(StorylandPlayTestRuntime& runtime,StorylandParticleKind kind,StorylandNativePoint position,
    StorylandNativePoint velocity,float size,float lifetime);
void hurtPed(StorylandPlayTestRuntime& runtime,size_t index,float damage,
    const StorylandNativePoint& position,bool playerAttack,bool bleed=true,StorylandCrime crime=StorylandCrime::Assault,
    const StorylandWeaponDefinition* attackerWeapon=nullptr,const StorylandNativePoint* source=nullptr,bool directPlayerSource=true,
    bool headHit=false,int32_t nativeWeapon=-1,uint8_t hitPiece=0,uint8_t hitDirection=0,uint8_t damageKind=255,
    float heading=0,float horizontalSpeed=-1) {
    auto& ped=runtime.pedestrians[index];
    if(ped.scriptHandle>=0||ped.health<=0||damage<=0) return;
    if (runtime.crime.vcs && playerAttack && directPlayerSource && damage > 3 && runtime.missionEnvironment)
        ++runtime.missionEnvironment->playerHavoc;
    const bool wasHostile=ped.hostile;
    const bool protectedPlayer=storylandGangPlayerProtected(runtime,ped.stats.pedType);
    const float weakness=ped.stats.valid?ped.stats.defendWeakness:1.0f;
    if(!std::isfinite(weakness)||weakness<0)return;
    if(attackerWeapon)nativeWeapon=attackerWeapon->nativeId;
    if(damageKind==255)damageKind=attackerWeapon?(attackerWeapon->fireType==0?16:1):
        crime==StorylandCrime::Runover?8:16;
    if(runtime.crime.vcs&&(damageKind==1||damageKind==16))
        storylandRequestWeaponHitSpeech(runtime,int(index),position,nativeWeapon);
    const uint8_t piece=headHit?6:hitPiece;
    bool critical=false;
    if(damageKind==1) {
        if(storylandNativePedCriticalNeedsRandom(runtime.crime.vcs,nativeWeapon))randomUnit(runtime);
        critical=storylandNativePedCriticalRoll(runtime.crime.vcs,nativeWeapon,true,false,runtime.combatRandom);
    }
    const auto result=storylandApplyNativePedDamage(ped.health,ped.armour,damage,false,weakness,
        storylandNativeDamageBypassesArmour(runtime.crime.vcs,nativeWeapon),critical&&piece>=2&&piece<=6);
    if(!result.accepted)return;
    ped.armour=result.armour;ped.health=result.health;
    if (damageKind == 1 && ped.health > 0 && !ped.water.active && !ped.zombie.active &&
        !ped.serviceVehicleIdentity&&
        (!runtime.missionEnvironment||!runtime.missionEnvironment->cutscene.playing)&&
        !storylandPlayerReactionHoldsTarget(runtime.playerReaction,StorylandReactionTargetKind::Ambient,
            int32_t(index),ped.stats.modelId)) {
        StorylandBulletImpact impact;
        impact.weapon=nativeWeapon;impact.heading=heading;
        impact.speed=std::isfinite(horizontalSpeed)&&horizontalSpeed>=0?horizontalSpeed:
            std::isfinite(ped.combatSpeed)?std::max(0.0f,ped.combatSpeed):0.0f;
        impact.verticalSpeed=std::isfinite(ped.contact.velocity.z)?ped.contact.velocity.z:0.0f;
        if(source)impact.sourceDirection={source->x-position.x,source->y-position.y,0};
        if(storylandBeginBulletReaction(runtime,ped.bulletReaction,hitDirection,impact)) {
            ped.melee={};ped.ranged={};ped.contact.clear();ped.clearGunThreat();
            ped.combatSpeed=0;ped.fireFlash=0;
        }
    }
    if(playerAttack) {
        ped.playerDamaged=true;
        if(ped.fortBaxterSoldier&&!protectedPlayer)ped.personallyProvoked=true;
    }
    if(playerAttack&&ped.health<=0&&!ped.playerKillCounted) {
        if (runtime.crime.vcs && runtime.missionEnvironment) runtime.missionEnvironment->playerHavoc += 10u;
        ++runtime.modelsKilledByPlayer[int32_t(ped.stats.modelId)];
        if(runtime.crime.vcs)
            StorylandFortBaxterRetailResponse::recordSoldierDeath(runtime.fortBaxterKills,
                runtime.fortBaxterPendingDeaths,int32_t(ped.stats.modelId));
        runtime.statistics["PeopleKilledByPlayer"]+=1;
        if(!runtime.crime.vcs)runtime.statistics["KillsSinceLastCheckpoint"]+=1;
        ped.playerKillCounted=true;
        runtime.recordCloseExecutionBlood(position,damageKind,headHit,directPlayerSource);
        storylandRegisterKillFrenzyDeath(runtime,int32_t(ped.stats.modelId),
            attackerWeapon?attackerWeapon->nativeId:nativeWeapon,headHit);
    }
    if(playerAttack) {
        if(!runtime.crime.vcs&&crime==StorylandCrime::Assault&&attackerWeapon)
            crime=lcsMeleeCrime(*attackerWeapon);
        const uint32_t sourceId=(ped.stats.modelId<<8)|uint32_t(index+1);
        runtime.crime.report(crime,runtime.crime.vcs?position:runtime.playerPosition,
            runtime.elapsed,ped.isPolice(runtime.crime.vcs),sourceId);
    }
    if(runtime.crime.vcs?(ped.health<=0||nativeWeapon==storylandNativeFallWeapon(true)):
        (ped.health<=0||runtime.elapsed>=ped.nextVoiceTime)) {
        StorylandPlayTestSoundEvent reaction(StorylandPlayTestSound::Damage);
        reaction.position=position;reaction.positioned=true;reaction.speaker=int(index);
        reaction.weapon=nativeWeapon;
        if(runtime.crime.vcs)reaction.nativeSpeech=storylandCharacterDamageSpeech(true,nativeWeapon,ped.health<=0);
        storylandStampCharacterSoundEvent(runtime,reaction);
        reaction.fatal=ped.health<=0;runtime.soundEvents.push_back(reaction);
        ped.nextVoiceTime=runtime.elapsed+.5f;
    }
    const bool bloodAllowed=bleed&&storylandBloodDamageAllowed(runtime.crime.vcs,damageKind,nativeWeapon);
    if(ped.health<=0)ped.corpseBlood.suppressPool=!bloodAllowed;
    if(bloodAllowed&&result.applied>result.absorbed) {
        const StorylandNativePoint impact=headHit&&ped.headValid?ped.head:!headHit&&ped.torsoValid?ped.torso:
            StorylandNativePoint{position.x,position.y,position.z+(headHit?1.6f:1.05f)};
        storylandMarkBloodRunover(runtime,position,nativeWeapon);
        const StorylandNativePoint playerOrigin=runtime.playerMuzzleValid?runtime.playerMuzzle:
            StorylandNativePoint{runtime.playerPosition.x,runtime.playerPosition.y,runtime.playerPosition.z+1.05f};
        storylandEmitBloodImpact(runtime,impact,result.applied-result.absorbed,
            source?source:playerAttack?&playerOrigin:nullptr,headHit);
    }
    const bool nativeAttack=!runtime.crime.vcs&&playerAttack&&attackerWeapon&&
        ped.stats.valid&&runtime.playerStats.valid&&ped.stats.pedType!=13;
    if(nativeAttack&&ped.health>0) {
        const bool frightened=int(ped.stats.fear)>100-int(runtime.playerStats.temper);
        const bool samePersonality=ped.stats.statIndex>=0&&ped.stats.statIndex==runtime.playerStats.statIndex;
        const bool victimMelee=ped.weapon>=runtime.weapons.size()||runtime.weapons[ped.weapon].fireType==0;
        if(frightened||(!samePersonality&&attackerWeapon->fireType!=0&&victimMelee)) {
            ped.hostile=false;ped.startFlee(runtime.playerPosition,6,true);
        } else if(!samePersonality) {
            ped.hostile=true;ped.clearFlee();ped.state=StorylandPedState::Combat;
        }
    } else {
        const bool gang=runtime.crime.vcs?ped.stats.name.compare(0,9,"STAT_GANG")==0:
            ped.stats.pedType>=7&&ped.stats.pedType<=15;
        const bool retaliates=ped.fortBaxterSoldier||!ped.stats.valid||ped.isPolice(runtime.crime.vcs)||gang||
            (ped.stats.temper>ped.stats.fear+15&&ped.stats.fear<45);
        ped.hostile=ped.health>0&&!protectedPlayer&&(ped.hostile||(playerAttack&&retaliates));
        ped.state=ped.health<=0?StorylandPedState::Dead:ped.hostile?StorylandPedState::Combat:StorylandPedState::Flee;
        if(ped.health>0&&!ped.hostile)
            ped.startFlee(source?*source:runtime.playerPosition,4,playerAttack&&!source);
    }
    if(ped.zombie.active&&ped.health>0) {
        ped.hostile=false;ped.melee={};ped.ranged={};
        ped.clearFlee();ped.clearGunThreat();ped.state=StorylandPedState::Walk;
    }
    if(!wasHostile&&ped.hostile) ped.weaponCooldown=0.45f;
    if(ped.health<=0) {ped.hostile=false;ped.state=StorylandPedState::Dead;ped.clearFlee();ped.clearGunThreat();}
    if (ped.state==StorylandPedState::Dead) {
        storylandRetireBulletReaction(ped.bulletReaction);
        ped.deathTime=0;
        ped.death=storylandNativeDeathAssociation(runtime.crime.vcs,damageKind,nativeWeapon,piece,hitDirection,critical);
        ped.weaponCooldown=0;
        ped.reloading=0;
    }
    if(runtime.crime.vcs&&!ped.hostile)ped.timer=4;
    if(ped.health<=0&&ped.weapon>0&&runtime.pickups.size()<64) {
        runtime.pickups.push_back({position,ped.weapon,std::max(1,ped.ammunition),0});
        ped.weapon=0;
    }
}

void particle(StorylandPlayTestRuntime& runtime,StorylandParticleKind kind,StorylandNativePoint position,
    StorylandNativePoint velocity,float size,float lifetime) {
    if(runtime.particles.size()>=2048)return;
    const int32_t nativeType=storylandRetailParticleType(kind);
    if(nativeType>=0&&runtime.missionEnvironment&&runtime.missionEnvironment->createScriptParticles) {
        StorylandMissionEnvironment::ScriptParticle request;
        request.type=nativeType;
        request.position={position.x,position.y,position.z};
        request.velocity={velocity.x/50.0f,velocity.y/50.0f,velocity.z/50.0f};
        // GUNFLASH comes from the native particle.cfg descriptor; do not
        // override its authored size or lifespan with a generic host particle.
        request.size=kind==StorylandParticleKind::MuzzleFlash?0.0f:size;
        std::string error;
        const size_t before=runtime.particles.size();
        if(runtime.missionEnvironment->createScriptParticles({request},error)&&runtime.particles.size()>before) {
            auto& created=runtime.particles.back();
            if(lifetime>0&&kind!=StorylandParticleKind::MuzzleFlash)
                created.remaining=created.lifetime=lifetime;
            return;
        }
    }
    runtime.particles.push_back({position,velocity,kind,lifetime,lifetime,size});
}
void muzzleFlash(StorylandPlayTestRuntime& runtime,const StorylandNativePoint& position,
    int32_t nativeWeapon,StorylandMuzzleOwner owner,int32_t handle=-1) {
    bool valid=owner==StorylandMuzzleOwner::Vehicle||owner==StorylandMuzzleOwner::None;
    uint64_t identity=0;
    if(owner==StorylandMuzzleOwner::Player)valid=runtime.health>0&&runtime.playerMuzzleValid;
    else if(owner==StorylandMuzzleOwner::AmbientPed&&handle>=0&&size_t(handle)<runtime.pedestrians.size()) {
        const auto& ped=runtime.pedestrians[size_t(handle)];
        valid=ped.health>0&&ped.weaponMuzzleValid;
        identity=ped.speechIdentity;
    }
    else if(owner==StorylandMuzzleOwner::MissionPed) {
        const auto found=runtime.missionCombat.find(handle);
        valid=found!=runtime.missionCombat.end()&&found->second.muzzleValid;
        if(valid&&runtime.missionEnvironment) {
            const auto character=runtime.missionEnvironment->missionCharacters.find(handle);
            valid=character!=runtime.missionEnvironment->missionCharacters.end()&&
                character->second.alive&&character->second.health>0&&!character->second.removeFromWorld;
            if(valid)identity=character->second.speechIdentity;
        } else valid=false;
    }
    if(!valid)return;
    const size_t first=runtime.particles.size();
    particle(runtime,StorylandParticleKind::MuzzleFlash,position,{},0.12f,0.06f);
    for(size_t index=first;index<runtime.particles.size();++index) {
        auto& flash=runtime.particles[index];
        if(flash.kind!=StorylandParticleKind::MuzzleFlash)continue;
        flash.muzzleOwner=owner;flash.muzzleHandle=handle;flash.muzzleWeapon=nativeWeapon;
        flash.muzzleIdentity=identity;
    }
}

void weaponTrace(StorylandPlayTestRuntime& runtime,const StorylandNativePoint& from,
    const StorylandNativePoint& to,int32_t weapon,bool player=false) {
    float movement=std::fabs(runtime.movementSpeed);
    if(runtime.occupiedVehicle>=0&&size_t(runtime.occupiedVehicle)<runtime.vehicles.size()) {
        const auto& vehicle=runtime.vehicles[size_t(runtime.occupiedVehicle)];
        movement=std::hypot(vehicle.speed,vehicle.lateralSpeed,vehicle.verticalSpeed);
    }
    int cameraMode=runtime.manualAimActive?runtime.manualAimMode:0;
    if(runtime.missionEnvironment&&runtime.missionEnvironment->activeCameraMode)
        cameraMode=runtime.missionEnvironment->activeCameraMode();
    if(runtime.crime.vcs&&!storylandVcsBulletTraceVisibleForShooter(player,cameraMode,movement/50.0f))return;
    storylandAddWeaponTrace(runtime.shotTraces,from,to,weapon,runtime.crime.vcs);
}
// Use the game's original world-hit sound route (SCM one-shot 12) and
// native spark descriptor. Character/vehicle hits are handled elsewhere.
void storylandWeaponWorldImpact(StorylandPlayTestRuntime& runtime,
    const StorylandNativePoint& impact) {
    particle(runtime,StorylandParticleKind::Spark,impact,{0.0f,0.0f,0.0f},0.045f,0.12f);
    auto* environment=runtime.missionEnvironment;
    if(environment&&environment->audioSubsystemEnabled&&environment->scriptSounds.size()<4096u)
        environment->scriptSounds.push_back({12,{impact.x,impact.y,impact.z}});
}
StorylandProjectileKind projectileKind(const StorylandWeaponDefinition& weapon) {
    std::string name;
    for(unsigned char character:weapon.name)if(std::isalnum(character))name.push_back(char(std::tolower(character)));
    if(name=="landmine"&&weapon.nativeId==14)return StorylandProjectileKind::LandMine;
    if(name=="seamine"&&weapon.nativeId==15)return StorylandProjectileKind::SeaMine;
    if(name.find("molotov")!=std::string::npos)return StorylandProjectileKind::Molotov;
    if(name.find("tear")!=std::string::npos||name.find("gas")!=std::string::npos)return StorylandProjectileKind::TearGas;
    if(name.find("rocket")!=std::string::npos||name=="rlaunch"||name=="rpg")return StorylandProjectileKind::Rocket;
    return StorylandProjectileKind::Grenade;
}
bool nativeTearGas(const StorylandPlayTestRuntime& runtime,const StorylandWeaponProjectile& projectile) {
    return projectile.kind==StorylandProjectileKind::TearGas&&projectile.weapon<runtime.weapons.size()&&
        runtime.weapons[projectile.weapon].nativeId==16;
}
void prepareUnusedProjectile(StorylandPlayTestRuntime& runtime,StorylandWeaponProjectile& projectile,float yaw) {
    projectile.yaw=yaw;
    projectile.sequence=runtime.nextProjectileSequence++;
    if(!runtime.nextProjectileSequence)runtime.nextProjectileSequence=1;
    // Retail 17F664 creates mines without a fuse; 17F0DC gives tear gas20s.
    if(storylandProjectileIsMine(projectile.kind))projectile.remaining=1;
    else if(nativeTearGas(runtime,projectile))projectile.remaining=20;
    if(projectile.owner<0&&projectile.weapon<runtime.weapons.size()) {
        storylandUpdateBetaWeaponGrants(runtime);
        for(size_t index=0;index<runtime.betaWeapons.size();++index) {
            const auto& grant=runtime.betaWeapons[index];
            if(grant.enabled&&grant.remainingGranted>0&&
                grant.nativeWeapon==runtime.weapons[projectile.weapon].nativeId)
                projectile.betaFeature=uint8_t(index+1);
        }
    }
}
bool mineTouchesActor(const StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& actors,const StorylandNativePoint& player,
    const StorylandWeaponProjectile& projectile) {
    const auto touchesPed=[&](StorylandNativePoint position,float yaw,int32_t model) {
        const auto matrix=storylandScriptObjectMatrix(position.x,position.y,position.z,0,0,yaw);
        const auto collision=world.collisionModels.find(uint32_t(model));
        if(collision!=world.collisionModels.end())return storylandMineTouchesBounds(projectile.position,matrix,
            collision->second.minimum,collision->second.maximum);
        return storylandMineTouchesBounds(projectile.position,matrix,{-.35f,-.35f,0},{.35f,.35f,1.8f});
    };
    // Native181958 scans the ped and vehicle pools, including the thrower.
    if(runtime.health>0&&runtime.occupiedVehicle<0&&touchesPed(player,0,int32_t(runtime.playerStats.modelId)))return true;
    for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index)
        if(runtime.pedestrians[index].health>0&&touchesPed(actors[index].position,actors[index].yaw,
            int32_t(runtime.pedestrians[index].stats.modelId)))return true;
    if(runtime.missionEnvironment)for(const auto& entry:runtime.missionEnvironment->missionCharacters) {
        const auto& character=entry.second;
        if(character.alive&&character.health>0&&character.vehicle<0&&touchesPed(
            {character.x,character.y,character.z},character.heading,character.model))return true;
    }
    for(size_t index=0;index<runtime.vehicles.size();++index) {
        const auto& vehicle=runtime.vehicles[index];
        if(!vehicle.collisionEnabled)continue;
        std::array<float,16> matrix{};
        if(index<runtime.vehicleWorldMatrices.size())matrix=runtime.vehicleWorldMatrices[index];
        else if(runtime.missionEnvironment&&runtime.missionEnvironment->vehicleWorldMatrix&&
            runtime.missionEnvironment->vehicleWorldMatrix(vehicle.scriptHandle,matrix)) {}
        else if(index<runtime.vehiclePositions.size()) {
            const auto& position=runtime.vehiclePositions[index];
            matrix=storylandScriptObjectMatrix(position.x,position.y,position.z,0,0,0);
        } else continue;
        if(vehicle.nativeDefinition.collision) {
            const auto& collision=*vehicle.nativeDefinition.collision;
            if(storylandMineTouchesBounds(projectile.position,matrix,collision.minimum,collision.maximum))return true;
        } else if(storylandMineTouchesBounds(projectile.position,matrix,
            {-vehicle.halfWidth,-vehicle.halfLength,0},{vehicle.halfWidth,vehicle.halfLength,vehicle.bodyHeight}))return true;
    }
    return false;
}
void updateWaterCannons(StorylandPlayTestRuntime& runtime) {
    auto* environment=runtime.missionEnvironment;
    if(!environment||!runtime.crime.vcs)return;
    const float nativeStep=environment->nativeScriptTimeStep;
    if(!std::isfinite(nativeStep)||nativeStep<=0)return;
    for(auto& cannon:runtime.waterCannons)
        storylandAdvanceWaterCannon(cannon,environment->gameMilliseconds,nativeStep,
            [&](const auto& position,float radius) {
                return storylandSuppressFireWithWater(*environment,position,radius);
            });
    // The original firetruck also directs a cannon autonomously in traffic
    // statuses 3 and 4. Mission-reserved fires are not candidates.
    for(size_t index=0;index<runtime.vehicles.size();++index) {
        if(int32_t(index)==runtime.occupiedVehicle)continue;
        auto& truck=runtime.vehicles[index];
        if(!truck.nativeDefinition.vcsModelLayout||truck.nativeDefinition.modelId!=194)continue;
        int status=truck.parked?4:3;
        if(truck.scriptHandle>=0) {
            const auto scripted=environment->missionVehicles.find(truck.scriptHandle);
            if(scripted==environment->missionVehicles.end()||scripted->second.deleted)continue;
            status=scripted->second.scriptStatus;
        }
        if(status!=3&&status!=4)continue;
        std::array<float,16> frame{};
        const bool liveFrame=truck.scriptHandle>=0&&environment->vehicleWorldMatrix&&
            environment->vehicleWorldMatrix(truck.scriptHandle,frame);
        if(!liveFrame) {
            if(index<runtime.vehicleWorldMatrices.size())frame=runtime.vehicleWorldMatrices[index];
            else if(index<runtime.movementCars.size()) {
                const auto& actor=runtime.movementCars[index];
                const auto& definition=truck.nativeDefinition;
                frame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
                    {actor.position.x,actor.position.y,actor.position.z},actor.yaw,
                    definition.previewMinimumZ,truck.suspension,truck.bodyRoll-truck.roadRoll,
                    truck.bodyPitch+truck.roadPitch),definition.previewRoot);
            } else continue;
        }
        if(!std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);}))continue;
        const int32_t target=storylandSelectVcsFiretruckTarget(environment->scriptFires,
            {frame[12],frame[13],frame[14]});
        if(target<0)continue;
        storylandAimVcsFiretruckAtFire(truck.waterCannonYaw,truck.waterCannonPitch,nativeStep,
            frame,environment->scriptFires.at(target).position,environment->gameMilliseconds);
        if(!storylandVcsFiretruckSprayWindow(environment->gameMilliseconds))continue;
        const auto sample=storylandWaterCannonNozzle(frame,truck.waterCannonYaw,truck.waterCannonPitch,
            environment->nextNativeRandom());
        storylandEmitWaterCannon(runtime.waterCannons,int32_t(index),truck.scriptHandle,
            environment->gameMilliseconds,sample);
    }
    const int32_t slot=runtime.occupiedVehicle;
    if(slot<0||size_t(slot)>=runtime.vehicles.size()||!storylandVcsPadState(*environment,14))return;
    auto& vehicle=runtime.vehicles[size_t(slot)];
    if(!vehicle.nativeDefinition.vcsModelLayout||vehicle.nativeDefinition.modelId!=194)return;
    int32_t handle=vehicle.scriptHandle;
    if(handle<0&&environment->acquireOccupiedVehicle)handle=environment->acquireOccupiedVehicle();
    std::array<float,16> frame{};
    const bool liveFrame=handle>=0&&environment->vehicleWorldMatrix&&
        environment->vehicleWorldMatrix(handle,frame);
    if(!liveFrame) {
        if(size_t(slot)>=runtime.vehicleWorldMatrices.size())return;
        frame=runtime.vehicleWorldMatrices[size_t(slot)];
    }
    std::array<float,3> cameraForward{};
    const std::array<float,3>* cameraAim=nullptr;
    if(environment->activeCameraMode&&environment->activeCameraMode()==18&&environment->activeCameraVectors) {
        std::array<float,3> eye{},target{};
        if(environment->activeCameraVectors(eye,target)) {
            for(size_t axis=0;axis<3;++axis)cameraForward[axis]=target[axis]-eye[axis];
            const float length=std::hypot(cameraForward[0],cameraForward[1],cameraForward[2]);
            if(std::isfinite(length)&&length>0) {
                for(float& value:cameraForward)value/=length;
                cameraAim=&cameraForward;
            }
        }
    }
    storylandAimWaterCannon(vehicle.waterCannonYaw,vehicle.waterCannonPitch,nativeStep,
        storylandVcsPadState(*environment,4),storylandVcsPadState(*environment,37),frame,cameraAim);
    const auto sample=storylandWaterCannonNozzle(frame,vehicle.waterCannonYaw,vehicle.waterCannonPitch,
        environment->nextNativeRandom());
    storylandEmitWaterCannon(runtime.waterCannons,slot,handle,environment->gameMilliseconds,sample);
}

bool vehicleExplosionSourceFrame(const StorylandPlayTestRuntime& runtime,size_t slot,
    const std::vector<StorylandNativeTrafficActor>* cars,StorylandVehicleMatrix& frame) {
    if(slot>=runtime.vehicles.size()||(cars&&slot>=cars->size()))return false;
    const auto& vehicle=runtime.vehicles[slot];
    const auto* environment=runtime.missionEnvironment;
    if(environment&&vehicle.scriptHandle>=0) {
        const auto scripted=environment->missionVehicles.find(vehicle.scriptHandle);
        if(scripted==environment->missionVehicles.end()||scripted->second.deleted||
            scripted->second.spawnedSlot!=int(slot))return false;
        if(environment->vehicleWorldMatrix&&environment->vehicleWorldMatrix(vehicle.scriptHandle,frame))
            return std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);});
    }
    if(cars) {
        const auto& actor=(*cars)[slot];
        const auto& definition=vehicle.nativeDefinition;
        frame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
            {actor.position.x,actor.position.y,actor.position.z},actor.yaw,
            definition.previewMinimumZ,vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,
            vehicle.bodyPitch+vehicle.roadPitch,actor.water),definition.previewRoot);
    } else if(slot<runtime.vehicleWorldMatrices.size())frame=runtime.vehicleWorldMatrices[slot];
    else return false;
    return std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);});
}

StorylandNativePoint vehicleExplosionComponentPoint(const StorylandRuntimeVehicle& vehicle,
    const StorylandVehicleMatrix& frame,const std::array<float,3>& local) {
    const auto& definition=vehicle.nativeDefinition;
    auto componentFrame=frame;
    if(definition.twoWheels())componentFrame=storylandVehicleMatrixProduct(
        storylandVehicleRenderFrame(definition,{},0,definition.previewMinimumZ,0,0,0,false,
            &frame,vehicle.bikeVisualLean),definition.previewRoot);
    const auto point=storylandVehicleTransformPoint(componentFrame,local);
    return {point[0],point[1],point[2]};
}

void appendVehicleExplosionJet(StorylandRuntimeEffect& effect,const StorylandNativePoint& origin,
    float strength,float size,std::vector<StorylandMissionEnvironment::ScriptParticle>& requests) {
    const auto randomRange=[&](float lower,float upper) {
        return lower+(upper-lower)*storylandNativeRandomUnit(effect.explosionRandom);
    };
    const StorylandNativePoint step{2*randomRange(-.4f,.4f),2*randomRange(-.4f,.4f),
        2*randomRange(.1f,.3f)};
    auto position=origin;
    const int nodes=int(std::max(1.0f,strength)*4);
    for(int node=0;node<nodes;++node) {
        for(int layer=0;layer<3;++layer) {
            StorylandMissionEnvironment::ScriptParticle request;
            request.type=42;
            request.position={position.x,position.y,position.z};
            const float spread=layer==0?.02f:.04f;
            request.velocity={randomRange(-spread,spread),randomRange(-spread,spread),
                layer==0?randomRange(-.02f,0):randomRange(0,.07f)};
            request.size=size;
            request.color={240,240,240,255};
            requests.push_back(request);
        }
        position.x+=step.x;position.y+=step.y;position.z+=step.z;
    }
}

bool startVehicleExplosionPresentation(StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& cars,const StorylandNativePoint& position,
    int type,size_t sourceVehicleSlot) {
    auto* environment=runtime.missionEnvironment;
    if(!environment||!environment->createScriptParticles)return false;
    if(runtime.effects.size()>=256||std::count_if(runtime.effects.begin(),runtime.effects.end(),
        [](const auto& effect){return effect.vehicleExplosionType>=3&&effect.vehicleExplosionType<=5;})>=48)
        return true;
    StorylandRuntimeEffect effect;
    effect.kind=StorylandParticleKind::Explosion;
    effect.vehicleExplosionType=type;
    effect.position=position;
    effect.remaining=4.25f;
    effect.radius=9;
    effect.owner=INT32_MAX;
    effect.explosionRandom=storylandNativeRandom(runtime.combatRandom);
    StorylandVehicleMatrix frame{};
    std::vector<StorylandMissionEnvironment::ScriptParticle> requests;
    if(vehicleExplosionSourceFrame(runtime,sourceVehicleSlot,&cars,frame)) {
        effect.vehicle=int(sourceVehicleSlot);
        const auto& vehicle=runtime.vehicles[sourceVehicleSlot];
        const auto& definition=vehicle.nativeDefinition;
        if(definition.hasExplosionBurstOrigin) {
            const auto origin=vehicleExplosionComponentPoint(vehicle,frame,definition.explosionBurstOrigin);
            if(origin.x!=0) {
                const uint32_t count=runtime.crime.vcs?2+(storylandNativeRandom(effect.explosionRandom)&2u):
                    1+(storylandNativeRandom(effect.explosionRandom)&1u);
                for(uint32_t jet=0;jet<count;++jet) {
                    const uint32_t sample=storylandNativeRandom(effect.explosionRandom)&7u;
                    const float strength=runtime.crime.vcs?3+.5f*sample:1.5f+float(sample)/7;
                    appendVehicleExplosionJet(effect,origin,strength,runtime.crime.vcs?.3f:.5f,requests);
                }
            }
        }
    }
    if(!requests.empty()) {
        std::string error;
        environment->createScriptParticles(requests,error);
    }
    runtime.effects.push_back(effect);
    return true;
}

void updateVehicleExplosionPresentation(StorylandPlayTestRuntime& runtime,StorylandRuntimeEffect& effect,
    const std::vector<StorylandNativeTrafficActor>* cars,const StorylandNativePoint& player,float dt) {
    if(effect.remaining<=0)return;
    effect.explosionFractionalTicks+=double(dt)*50;
    const uint32_t ticks=uint32_t(std::floor(effect.explosionFractionalTicks+1.0e-6));
    effect.explosionFractionalTicks=std::max(0.0,effect.explosionFractionalTicks-ticks);
    auto* environment=runtime.missionEnvironment;
    const float dx=effect.position.x-player.x,dy=effect.position.y-player.y;
    const bool visible=dx*dx+dy*dy<=140.0f*140.0f;
    StorylandVehicleMatrix frame{};
    const bool hasVehicle=effect.vehicle>=0&&vehicleExplosionSourceFrame(runtime,
        size_t(effect.vehicle),cars,frame);
    if(!hasVehicle)effect.vehicle=-1;
    std::vector<StorylandMissionEnvironment::ScriptParticle> requests;
    for(uint32_t tick=0;tick<ticks;++tick) {
        const uint32_t milliseconds=++effect.explosionTick*20;
        effect.age=float(milliseconds)*.001f;
        effect.remaining=std::max(0.0f,4.25f-effect.age);
        if(milliseconds>4250)break;
        if(!visible||!environment||!environment->createScriptParticles)continue;
        const auto append=[&](int type,StorylandNativePoint position,StorylandNativePoint velocity,float size) {
            StorylandMissionEnvironment::ScriptParticle request;
            request.type=type;request.position={position.x,position.y,position.z};
            request.velocity={velocity.x,velocity.y,velocity.z};request.size=size;
            requests.push_back(request);
        };
        if(milliseconds<=750) {
            if(!hasVehicle)continue;
            const auto& vehicle=runtime.vehicles[size_t(effect.vehicle)];
            const auto& definition=vehicle.nativeDefinition;
            if((storylandNativeRandom(effect.explosionRandom)&15u)==0&&definition.hasExplosionJetOrigin)
                appendVehicleExplosionJet(effect,vehicleExplosionComponentPoint(vehicle,frame,
                    definition.explosionJetOrigin),.5f,0,requests);
            if(milliseconds>effect.explosionNextFlashMilliseconds) {
                effect.explosionNextFlashMilliseconds=milliseconds+125+
                    (storylandNativeRandom(effect.explosionRandom)&127u);
                const uint32_t count=1+(storylandNativeRandom(effect.explosionRandom)&1u);
                for(uint32_t flash=0;flash<count;++flash)
                    append(40,{frame[12],frame[13],frame[14]},{},3.5f);
            }
        } else if(effect.explosionTick&1u) {
            if(hasVehicle)append(56,{frame[12],frame[13],frame[14]},{},
                .5f+.3f*storylandNativeRandomUnit(effect.explosionRandom));
            auto origin=effect.position;origin.z+=1;
            append(56,origin,{0,0,.11f},.5f+1.5f*storylandNativeRandomUnit(effect.explosionRandom));
        }
    }
    if(!requests.empty()) {
        std::string error;
        environment->createScriptParticles(requests,error);
    }
}

void updateEffects(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& actors,const StorylandNativePoint& player,float dt,
    const std::vector<StorylandNativeTrafficActor>* cars) {
    auto* environment=runtime.missionEnvironment;
    runtime.effects.erase(std::remove_if(runtime.effects.begin(),runtime.effects.end(),[&](const auto& effect) {
        if(effect.scriptFireHandle<0)return false;
        if(!environment)return true;
        const auto fire=environment->scriptFires.find(effect.scriptFireHandle);
        return fire==environment->scriptFires.end()||!fire->second.active;
    }),runtime.effects.end());
    if(environment)for(auto& entry:environment->scriptFires) {
        auto& fire=entry.second;
        // Native explosion fires carry a deadline in both Stories games. LCS
        // does not run the VCS object updater that also checks this deadline.
        if(!fire.scriptOwned&&int32_t(environment->gameMilliseconds-fire.endsAt)>=0)
            fire.active=false;
        if(fire.suppressionPending) {
            particle(runtime,StorylandParticleKind::Steam,{fire.position[0],fire.position[1],fire.position[2]},
                {0,0,.15f},.8f,1.0f);fire.suppressionPending=false;
        }
        if(!fire.active)continue;
        const int32_t playerHandle=runtime.crime.vcs?0:1;
        if(fire.attachedKind==1) {
            if(fire.attachedHandle==playerHandle) {
                if(runtime.health<=0) {fire.active=false;continue;}
                fire.position={player.x,player.y,player.z};
            } else {
                const auto ped=environment->missionCharacters.find(fire.attachedHandle);
                if(ped==environment->missionCharacters.end()||!ped->second.alive||ped->second.health<=0) {
                    fire.active=false;continue;
                }
                fire.position={ped->second.x,ped->second.y,ped->second.z};
            }
        } else if(fire.attachedKind==2) {
            const auto car=environment->missionVehicles.find(fire.attachedHandle);
            if(car==environment->missionVehicles.end()||car->second.deleted||
                car->second.extinguishFireRequested||car->second.extinguishRequested) {
                fire.active=false;continue;
            }
            fire.position={car->second.x,car->second.y,car->second.z};
        }
        if(!runtime.crime.vcs) {
            if(fire.attachedKind==1) {
                const int32_t vehicle=fire.attachedHandle==1?
                    (environment->inVehicle?environment->occupiedMissionVehicle:-1):
                    (environment->missionCharacters.at(fire.attachedHandle).attached?-1:
                        environment->missionCharacters.at(fire.attachedHandle).vehicle);
                const auto car=environment->missionVehicles.find(vehicle);
                bool automobile=false;
                if(car!=environment->missionVehicles.end())
                    for(const auto& definition:runtime.modelDefinitions)
                        if(definition.modelId==uint32_t(car->second.model)) {
                            automobile=!definition.vcsModelLayout&&definition.nativeType==0;
                            break;
                        }
                if(automobile) {
                    // Native burning occupants lower a car's engine health instead
                    // of receiving on-foot fire damage while inside it.
                    car->second.health=75;car->second.healthChangedByScript=true;
                }
            }
        }
        auto found=std::find_if(runtime.effects.begin(),runtime.effects.end(),
            [&](const auto& effect){return effect.scriptFireHandle==entry.first;});
        if(found==runtime.effects.end()) {
            if(runtime.effects.size()>=256)break;
            StorylandRuntimeEffect effect;effect.kind=StorylandParticleKind::Flame;
            effect.permanent=true;effect.remaining=1;effect.radius=.32f;effect.damage=60;
            effect.scriptFireHandle=entry.first;effect.owner=INT32_MAX;
            runtime.effects.push_back(effect);found=runtime.effects.end()-1;
        }
        found->position={fire.position[0],fire.position[1],fire.position[2]};
        found->radius=runtime.crime.vcs?.4f*fire.scale*std::max(0.0f,fire.strength):.32f;
        found->owner=fire.source.kind==StorylandMissionEnvironment::DamageSourceKind::Character?
            fire.source.handle==(runtime.crime.vcs?0:1)?-1:int(runtime.pedestrians.size())+fire.source.handle:INT32_MAX;
    }
    updateWaterCannons(runtime);
    runtime.playerCoughTime=std::max(0.0f,runtime.playerCoughTime-dt);
    for(auto& ped:runtime.pedestrians)ped.coughTime=std::max(0.0f,ped.coughTime-dt);
    storylandUpdateCorpseBlood(runtime,world,actors,player,dt);
    for(auto& item:runtime.particles) {
        item.remaining-=dt;
        storylandAdvanceNativeParticlePresentation(item,dt);
        if(dt>0)storylandAdvanceParticleTexture(item,dt);
        const auto previous=item.position;
        if(storylandAdvanceSwimmingParticle(item,dt,
            [&](float x,float y,float& surface) {
                if(!storylandNativeWaterHeight(world,x,y,surface))return false;
                uint8_t material=0;
                return storylandTraceNativeGround(world,{x,y,surface+.08f},
                    {x,y,surface-.08f},material)>=1.0f;
            },[&](const auto& from,const auto& to) {
                return storylandTraceNativeSegment(world,from,to)<1.0f;
            }))continue;
        if(item.kind==StorylandParticleKind::Dollar) {
            if(!environment||!environment->dollarParticleEnabled) {item.remaining=0;continue;}
            // Dollar manager32FD70 updates once per frame: velocity/1.4,
            // downward0.03, then displacement. Grounded notes remain until TTL.
            if(item.nativeType!=-2) {
                item.velocity.x*=.714285731f;item.velocity.y*=.714285731f;
                item.velocity.z=item.velocity.z*.714285731f-.03f;
                item.position.x+=item.velocity.x;item.position.y+=item.velocity.y;item.position.z+=item.velocity.z;
                const float hit=storylandTraceNativeSegment(world,previous,item.position);
                if(hit<1) {
                    item.position={previous.x+(item.position.x-previous.x)*hit,
                        previous.y+(item.position.y-previous.y)*hit,previous.z+(item.position.z-previous.z)*hit+.01f};
                    item.velocity={};item.nativeType=-2;
                }
            }
            continue;
        }
        if(item.kind==StorylandParticleKind::BloodPool) {
            storylandUpdateBloodPool(item,dt);
            continue;
        }
        if(item.nativePhysics) {
            const float exhaustExpansion=item.nativeType==65&&item.nativeExpansion>0?
                1.0f+std::hypot(item.velocity.x,item.velocity.y,item.velocity.z)*.04f:1.0f;
            item.position.x+=item.velocity.x*dt;item.position.y+=item.velocity.y*dt;item.position.z+=item.velocity.z*dt;
            item.velocity.z-=item.nativeGravity*2500*dt;
            if(item.nativeFriction>1&&item.nativeFriction<=100) {
                const float drag=std::pow(float(item.nativeFriction)*.01f,dt*50);
                item.velocity.x*=drag;item.velocity.y*=drag;item.velocity.z*=drag;
            }
            item.size=std::max(0.0f,item.size+item.nativeExpansion*exhaustExpansion*50*dt);
            if(item.kind!=StorylandParticleKind::Blood)continue;
        } else {
            if (item.kind==StorylandParticleKind::Smoke || item.kind==StorylandParticleKind::Exhaust ||
                item.kind==StorylandParticleKind::EngineSmoke || item.kind==StorylandParticleKind::Gas ||
                item.kind==StorylandParticleKind::Steam || item.kind==StorylandParticleKind::Flame) {
                const float drag=std::min(1.0f,dt*0.8f);
                item.velocity.x+=(runtime.windVelocity.x-item.velocity.x)*drag;
                item.velocity.y+=(runtime.windVelocity.y-item.velocity.y)*drag;
            }
            item.position.x+=item.velocity.x*dt;item.position.y+=item.velocity.y*dt;item.position.z+=item.velocity.z*dt;
            if(item.kind==StorylandParticleKind::Spark||item.kind==StorylandParticleKind::Water||item.kind==StorylandParticleKind::Debris||item.kind==StorylandParticleKind::WheelDirt||item.kind==StorylandParticleKind::Leaves||item.kind==StorylandParticleKind::Blood)
                item.velocity.z-=runtime.gravity*dt;
            else item.velocity.z+=0.2f*dt;
        }
        if(item.kind==StorylandParticleKind::Blood)
            storylandUpdateBloodParticle(runtime,item,world,previous,runtime.visibleArea);
    }
    runtime.particles.erase(std::remove_if(runtime.particles.begin(),runtime.particles.end(),
        [](const auto& item){return item.remaining<=0;}),runtime.particles.end());
    for(size_t effectIndex=0;effectIndex<runtime.effects.size()+runtime.worldEffects.size();++effectIndex) {
        auto& effect=effectIndex<runtime.effects.size()?runtime.effects[effectIndex]:runtime.worldEffects[effectIndex-runtime.effects.size()];
        if(effect.kind==StorylandParticleKind::Explosion&&
            effect.vehicleExplosionType>=3&&effect.vehicleExplosionType<=5) {
            updateVehicleExplosionPresentation(runtime,effect,cars,player,dt);
            continue;
        }
        if(effect.scriptFireHandle>=0&&environment) {
            const auto fire=environment->scriptFires.find(effect.scriptFireHandle);
            if(fire==environment->scriptFires.end()||!fire->second.active) {effect.remaining=0;continue;}
        }
        if(effect.vehicle>=0) {
            const size_t slot=size_t(effect.vehicle);
            if(slot>=runtime.vehicles.size()||!runtime.vehicles[slot].burning||!runtime.vehicles[slot].canBeDamaged) {
                effect.remaining=0;continue;
            }
            if(slot<runtime.vehiclePositions.size()&&
                (runtime.vehicles[slot].wrecked||effect.scriptFireHandle>=0)) {
                effect.position=runtime.vehiclePositions[slot];effect.position.z+=0.4f;
            }
        }
        if(effect.sourceProjectile) {
            const auto source=std::find_if(runtime.projectiles.begin(),runtime.projectiles.end(),
                [&](const auto& projectile){return projectile.sequence==effect.sourceProjectile;});
            if(source==runtime.projectiles.end()) {effect.remaining=0;continue;}
            effect.position=source->position;effect.position.z+=.08f;
            effect.remaining=source->remaining;
        }
        if(!effect.permanent) {
            effect.remaining-=dt;
            if(effect.remaining<=0) {
                if(effect.kind==StorylandParticleKind::Flame&&effect.vehicle>=0&&
                    size_t(effect.vehicle)<runtime.vehicles.size()) {
                    auto& vehicle=runtime.vehicles[size_t(effect.vehicle)];
                    vehicle.burning=false;
                    vehicle.engineFireSpent=true;
                }
                continue;
            }
        }
        effect.age+=dt;
        const bool nativeGas=effect.kind==StorylandParticleKind::Gas&&effect.nativeWeapon==16;
        const bool vehicleFire=effect.kind==StorylandParticleKind::Flame&&effect.vehicle>=0&&
            size_t(effect.vehicle)<runtime.vehicles.size();
        const bool engineFire=vehicleFire&&effect.scriptFireHandle<0&&
            !runtime.vehicles[size_t(effect.vehicle)].wrecked;
        const float dx=effect.position.x-player.x,dy=effect.position.y-player.y;
        const bool nearPlayer=dx*dx+dy*dy<=140.0f*140.0f;
        if(!nearPlayer&&effect.scriptFireHandle<0)continue;
        float water=0;
        if(effect.kind==StorylandParticleKind::Flame&&storylandNativeWaterHeight(world,effect.position.x,effect.position.y,water)&&
            effect.position.z<water+0.05f) {
            if(effect.scriptFireHandle>=0&&environment) {
                environment->scriptFires[effect.scriptFireHandle].active=false;effect.remaining=0;effect.permanent=false;
            } else if(!effect.permanent)effect.remaining=0;
            continue;
        }
        if(engineFire)effect.emission=0;
        else if(nearPlayer)effect.emission+=dt*(nativeGas?150.0f:effect.kind==StorylandParticleKind::Flame?32.0f:18.0f);
        const int count=nearPlayer&&!engineFire?std::min(nativeGas?15:8,int(effect.emission)):0;effect.emission-=count;
        const float extent=nativeGas?effect.radius:
            effect.kind==StorylandParticleKind::Gas?std::min(effect.radius,effect.age*0.65f):effect.radius;
        for(int index=0;index<count;++index) {
            const float angle=randomUnit(runtime)*6.28318530718f;
            const float distance=std::sqrt(randomUnit(runtime))*extent;
            auto position=effect.position;
            if(!nativeGas) {position.x+=std::cos(angle)*distance;position.y+=std::sin(angle)*distance;}
            const float size=effect.kind==StorylandParticleKind::Flame?0.2f+randomUnit(runtime)*0.25f:
                effect.kind==StorylandParticleKind::Gas?0.5f+randomUnit(runtime)*0.5f:0.25f+randomUnit(runtime)*0.25f;
            auto velocity=effect.direction;
            velocity.x+=(randomUnit(runtime)-0.5f)*0.35f;velocity.y+=(randomUnit(runtime)-0.5f)*0.35f;
            velocity.z+=effect.kind==StorylandParticleKind::Flame?0.8f:0.35f;
            const size_t firstParticle=runtime.particles.size();
            bool nativeCreated=false;
            if((nativeGas||vehicleFire)&&environment&&environment->createScriptParticles) {
                StorylandMissionEnvironment::ScriptParticle request;
                request.type=nativeGas?23:27;request.position={position.x,position.y,position.z};
                request.velocity={velocity.x/50,velocity.y/50,velocity.z/50};
                if(vehicleFire)request.size=size;
                std::string error;nativeCreated=environment->createScriptParticles({request},error);
            }
            if(!nativeCreated)particle(runtime,effect.kind,position,velocity,size,effect.kind==StorylandParticleKind::Flame?0.65f:2.4f);
            for(size_t emitted=firstParticle;emitted<runtime.particles.size();++emitted)
                runtime.particles[emitted].betaFeature=effect.betaFeature;
            if(effect.kind==StorylandParticleKind::Flame&&index%3==0) {
                if(vehicleFire&&environment&&environment->createScriptParticles) {
                    StorylandMissionEnvironment::ScriptParticle request;
                    request.type=57;request.position={position.x,position.y,position.z};
                    std::string error;environment->createScriptParticles({request},error);
                } else particle(runtime,StorylandParticleKind::Smoke,position,{0.1f,0.1f,1},0.3f,2);
            }
        }
        if(effect.damage<=0 || effect.remaining<=0)continue;
        effect.damageClock+=dt;
        if(!nativeGas&&effect.damageClock<0.25f)continue;
        const uint32_t now=environment?environment->gameMilliseconds:uint32_t(effect.age*1000);
        const float elapsed=nativeGas?storylandTearGasExposure(now,effect.damageClock):effect.damageClock;
        effect.damageClock=0;
        if(elapsed<=0)continue;
        const float damageRadius=!runtime.crime.vcs&&effect.scriptFireHandle>=0?1.41421356237f:extent;
        const auto exposed=[&](StorylandNativePoint position) {
            if(nativeGas)return std::abs(position.x-effect.position.x)<6&&
                std::abs(position.y-effect.position.y)<6&&std::abs(position.z-effect.position.z)<5;
            return separation(position,effect.position)<=damageRadius &&
                storylandTraceNativeSegment(world,{effect.position.x,effect.position.y,effect.position.z+0.25f},chest(position))>=0.995f;
        };
        const bool playerAttack=effectIndex<runtime.effects.size()&&effect.owner<0;
        for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index) {
            auto& ped=runtime.pedestrians[index];
            if(ped.health<=0||!exposed(actors[index].position))continue;
            hurtPed(runtime,index,effect.damage*elapsed,actors[index].position,playerAttack,false,
                StorylandCrime::Burning,nullptr,&effect.position,true,false,effect.nativeWeapon,0,0,
                effect.kind==StorylandParticleKind::Gas?0:2);
            if(effect.kind==StorylandParticleKind::Gas)ped.coughTime=0.6f;
            else if(ped.health>0) {
                ped.hostile=false;
                ped.startFlee(effect.position,2);
            }
        }
        StorylandMissionEnvironment::DamageSource source;
        if(environment&&effect.scriptFireHandle>=0) {
            const auto fire=environment->scriptFires.find(effect.scriptFireHandle);
            if(fire!=environment->scriptFires.end())source=fire->second.source;
        } else if(effect.owner<0)
            source={StorylandMissionEnvironment::DamageSourceKind::Character,runtime.crime.vcs?0:1};
        else if(environment&&size_t(effect.owner)>=runtime.pedestrians.size()&&
            environment->missionCharacters.count(effect.owner-int(runtime.pedestrians.size())))
            source={StorylandMissionEnvironment::DamageSourceKind::Character,effect.owner-int(runtime.pedestrians.size())};
        if(environment&&(effect.kind==StorylandParticleKind::Flame||effect.kind==StorylandParticleKind::Gas))
            for(const auto& entry:environment->missionCharacters) {
                const auto& character=entry.second;
                if(character.vehicle>=0&&!character.attached&&
                    (effect.kind==StorylandParticleKind::Gas||(!runtime.crime.vcs&&effect.scriptFireHandle>=0)))continue;
                if(character.alive&&character.health>0&&exposed({character.x,character.y,character.z})) {
                    const float previousHealth=character.health;
                    hurtMissionCharacter(runtime,entry.first,effect.damage*elapsed,effect.owner<0,
                        effect.kind==StorylandParticleKind::Gas?0:2,effect.nativeWeapon,source);
                    if(effect.kind==StorylandParticleKind::Gas&&character.health<previousHealth)
                        runtime.missionCombat[entry.first].coughTime=.6f;
                }
            }
        if(runtime.health>0&&exposed(player)&&
            (runtime.crime.vcs||effect.scriptFireHandle<0||runtime.occupiedVehicle<0)&&
            (effect.kind!=StorylandParticleKind::Gas||runtime.occupiedVehicle<0)) {
            if(effect.kind==StorylandParticleKind::Gas) {
                runtime.inflictDamage(effect.damage*elapsed,0,effect.nativeWeapon,false,0,0,playerAttack);
                runtime.playerCoughTime=0.6f;
                if(environment) {environment->playerLastWeaponDamage=effect.nativeWeapon;environment->playerDamageSource=source;}
            } else {
                runtime.inflictDamage(effect.damage*elapsed,2,effect.nativeWeapon,false,0,0,playerAttack);
                if(environment) {environment->playerLastWeaponDamage=effect.nativeWeapon;environment->playerDamageSource=source;}
            }
        }
    }
    runtime.effects.erase(std::remove_if(runtime.effects.begin(),runtime.effects.end(),
        [](const auto& effect){return !effect.permanent&&effect.remaining<=0;}),runtime.effects.end());
    if(environment)for(auto fire=environment->scriptFires.begin();fire!=environment->scriptFires.end();)
        if(!fire->second.active&&!fire->second.scriptOwned)fire=environment->scriptFires.erase(fire);else ++fire;
}

void explode(StorylandPlayTestRuntime& runtime,const std::vector<StorylandNativeTrafficActor>& actors,
    const StorylandNativeWorld& world,const StorylandNativePoint& player,
    const StorylandWeaponProjectile& projectile) {
    if(runtime.missionEnvironment&&runtime.missionEnvironment->createScriptExplosion) {
        StorylandMissionEnvironment::DamageSource source;
        if(projectile.scriptSlot>=0&&size_t(projectile.scriptSlot)<runtime.missionEnvironment->scriptProjectiles.size())
            source=storylandVcsProjectileExplosionSource(*runtime.missionEnvironment,
                runtime.missionEnvironment->scriptProjectiles[size_t(projectile.scriptSlot)]);
        else if(projectile.owner<0)
            source={StorylandMissionEnvironment::DamageSourceKind::Character,runtime.crime.vcs?0:1};
        else if(size_t(projectile.owner)>=runtime.pedestrians.size()) {
            const int32_t handle=projectile.owner-int(runtime.pedestrians.size());
            if(runtime.missionEnvironment->missionCharacters.count(handle))
                source={StorylandMissionEnvironment::DamageSourceKind::Character,handle};
        }
        const int type=projectile.kind==StorylandProjectileKind::Rocket?2:
            projectile.kind==StorylandProjectileKind::Molotov?1:storylandProjectileIsMine(projectile.kind)?8:0;
        std::string error;
        const size_t firstParticle=runtime.particles.size();
        storylandAddScriptExplosion(*runtime.missionEnvironment,type,
            {projectile.position.x,projectile.position.y,projectile.position.z},error,source);
        for(size_t index=firstParticle;index<runtime.particles.size();++index)
            runtime.particles[index].betaFeature=projectile.betaFeature;
        return;
    }
    const float radius=std::max(0.5f,projectile.radius);
    const int32_t nativeWeapon=projectile.weapon<runtime.weapons.size()?runtime.weapons[projectile.weapon].nativeId:-1;
    if(runtime.missionEnvironment)for(const auto& entry:runtime.missionEnvironment->missionCharacters) {
        const auto& character=entry.second;
        const auto target=chest({character.x,character.y,character.z});
        const float distance=separation(projectile.position,target);
        if(character.visible&&distance<radius&&storylandTraceNativeSegment(world,projectile.position,target)>=0.99f)
            hurtMissionCharacter(runtime,entry.first,projectile.damage*(1-distance/radius),projectile.owner<0,4,nativeWeapon);
    }
    for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index) {
        const float distance=separation(projectile.position,chest(actors[index].position));
        if(distance<radius&&storylandTraceNativeSegment(world,projectile.position,chest(actors[index].position))>=0.99f)
            hurtPed(runtime,index,projectile.damage*(1-distance/radius),actors[index].position,projectile.owner<0,
                true,StorylandCrime::Assault,nullptr,&projectile.position,true,false,nativeWeapon,0,0,4);
    }
    const float distance=separation(projectile.position,chest(player));
    if (distance < radius &&
        storylandTraceNativeSegment(world, projectile.position, chest(player)) >= 0.99f) {
        runtime.inflictDamage(projectile.damage * (1 - distance / radius),4,nativeWeapon,true,0,0,projectile.owner<0);
    }
    for(size_t index=0;index<runtime.vehiclePositions.size()&&index<runtime.vehicles.size();++index) {
        const float distance=separation(projectile.position,chest(runtime.vehiclePositions[index]));
        if(distance>=radius||!visible(world,projectile.position,runtime.vehiclePositions[index]))continue;
        auto& vehicle=runtime.vehicles[index];
        if (projectile.owner < 0 && runtime.coopConnected && !runtime.coopFriendlyFire && vehicle.coopControlled) continue;
        if(vehicle.canBeDamaged&&!(vehicle.damageProofs&4u)&&
            (runtime.crime.vcs?(!vehicle.onlyDamagedByPlayer||projectile.owner<0):
                storylandLcsVehicleDamageSourceAllowed(runtime,index,projectile.owner<0))) {
            const float previousHealth=vehicle.health;
            vehicle.health=std::max(0.0f,vehicle.health-projectile.damage*5.0f*(1-distance/radius));
            if(projectile.owner<0&&previousHealth>0&&vehicle.health<=0)
                storylandRegisterKillFrenzyDeath(runtime,int32_t(vehicle.nativeDefinition.modelId),nativeWeapon,false,true);
            vehicle.visiblyDamaged = true;
            if (vehicle.doorDamage[0] == 0) vehicle.doorDamage[0] = 1;
        }
        if(!vehicle.positionFrozen&&!vehicle.craneCarried&&!storylandLcsVehiclePositionFrozen(runtime.missionEnvironment,vehicle.scriptHandle)) {
            vehicle.suspensionVelocity+=3*(1-distance/radius);vehicle.speed*=0.5f;
        }
    }
    if(runtime.missionEnvironment)
        storylandExplodeScriptObjects(*runtime.missionEnvironment,projectile.position,radius,
            projectile.kind==StorylandProjectileKind::Rocket?10:9);
    const size_t firstParticle=runtime.particles.size();
    for(int index=0;index<28;++index) {
        const float angle=randomUnit(runtime)*6.28318530718f;
        const float speed=1+randomUnit(runtime)*5;
        particle(runtime,index<12?StorylandParticleKind::Explosion:StorylandParticleKind::Smoke,projectile.position,
            {std::cos(angle)*speed,std::sin(angle)*speed,1+randomUnit(runtime)*3},index<12?0.5f:0.35f,index<12?0.35f:2.5f);
    }
    for(size_t index=firstParticle;index<runtime.particles.size();++index)
        runtime.particles[index].betaFeature=projectile.betaFeature;
    sound(runtime,StorylandPlayTestSound::Explosion,projectile.position);
}
void refreshRandomStreetWeapons(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const StorylandNativePoint& player) {
    if(!runtime.randomStreetWeapons) {
        runtime.pickups.erase(std::remove_if(runtime.pickups.begin(),runtime.pickups.end(),
            [](const auto& pickup){return pickup.randomlyGenerated;}),runtime.pickups.end());
        return;
    }
    if(runtime.elapsed<runtime.nextPickupRefresh||runtime.weapons.size()<2||world.paths.nodes.empty())return;
    runtime.nextPickupRefresh=runtime.elapsed+2;
    size_t count=std::count_if(runtime.pickups.begin(),runtime.pickups.end(),
        [](const auto& pickup){return pickup.randomlyGenerated;});
    const size_t start=size_t(randomUnit(runtime)*world.paths.nodes.size());
    for(size_t offset=0;offset<world.paths.nodes.size()&&count<12&&runtime.pickups.size()<64;++offset) {
        const auto& node=world.paths.nodes[(start+offset)%world.paths.nodes.size()];
        const float distance=separation(node.position,player);
        if(node.vehicle||node.disabled||node.links.empty()||distance<12||distance>130)continue;
        if(std::any_of(runtime.pickups.begin(),runtime.pickups.end(),[&](const auto& pickup) {
            return separation(pickup.position,node.position)<22;
        }))continue;
        auto position=node.position;
        const float hit=storylandTraceNativeSegment(world,{position.x,position.y,position.z+1},
            {position.x,position.y,position.z-3});
        if(hit>=1)continue;
        position.z+=1-hit*4;
        float water=0;
        if(storylandNativeWaterHeight(world,position.x,position.y,water)&&position.z<water+0.15f)continue;
        const size_t weapon=1+size_t(randomUnit(runtime)*(runtime.weapons.size()-1));
        runtime.pickups.push_back({position,weapon,std::max(1,runtime.weapons[weapon].clipSize*3),0,true});
        ++count;
    }
}
struct WeaponSegmentResult {
    StorylandNativePoint impact{};
    bool hitPedestrian=false;
    bool hitStatic=false;
    bool hitBuilding=false;
};

void queueVcsBulletPass(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const StorylandNativePoint& start,const StorylandNativePoint& end,bool playerShooter) {
    if(!world.paths.vcs||!runtime.missionEnvironment)return;
    auto& environment=*runtime.missionEnvironment;
    if(!environment.audioSubsystemEnabled||!environment.activeBulletCameraFrame||
        environment.scriptSounds.size()>=4096)return;
    const auto velocity=runtime.pedestrianContactPlayerVelocity;
    const float speed=std::hypot(velocity.x,velocity.y,velocity.z);
    const int mode=environment.activeCameraMode?environment.activeCameraMode():-1;
    if(!storylandVcsBulletTraceAllowed(playerShooter,mode,speed))return;
    StorylandVcsBulletCameraFrame camera;
    StorylandVcsBulletPass pass;
    if(!environment.activeBulletCameraFrame(camera)||!storylandVcsBulletPass(camera,
        {start.x,start.y,start.z},{end.x,end.y,end.z},pass))return;
    StorylandMissionEnvironment::ScriptSound event;
    event.id=pass.event;
    event.frontend=true;
    event.frontendParameter=float(pass.volume);
    environment.scriptSounds.push_back(event);
}

WeaponSegmentResult applyWeaponSegment(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, std::vector<StorylandNativeTrafficActor>& actors,
    const StorylandNativePoint& player, const StorylandNativePoint& origin,
    const StorylandNativePoint& start, const StorylandNativePoint& end,
    const StorylandWeaponDefinition& weapon, int owner,
    const StorylandVcsSniperBullet* bullet=nullptr,
    const StorylandNativePoint* visualOrigin=nullptr,bool anonymousShooter=false,
    int firingVehicle=-1) {
    const bool melee=weapon.fireType==0;
    const bool bulletTrace=storylandWeaponUsesBulletTrace(weapon);
    const bool creditPlayer=bullet?bullet->sourceKind==StorylandVcsBulletSource::Player:
        owner<0&&!anonymousShooter;
    const int32_t sourceMissionCharacter=bullet&&
        bullet->sourceKind==StorylandVcsBulletSource::MissionCharacter?bullet->sourceHandle:-1;
    const float damage=bullet?bullet->damage:weapon.damage;
    // Visible script objects are tested separately below. Keep invisible
    // collidable objects in the first trace so they still block shots.
    const bool sharedScriptObjects=runtime.missionEnvironment&&
        world.scriptEnvironment==runtime.missionEnvironment;
    float nearest = bullet ? storylandTraceNativeMapGround(world, start, end) :
        sharedScriptObjects?storylandTraceNativeSegmentWithoutVisibleScriptObjects(world, start, end):
        storylandTraceNativeSegment(world, start, end);
    int hit=-1;
    bool ambientHeadHit=false;
    const auto sphere=[&](StorylandNativePoint center,float radius) {
        const float dx=end.x-start.x,dy=end.y-start.y,dz=end.z-start.z;
        const float ox=start.x-center.x,oy=start.y-center.y,oz=start.z-center.z;
        const float a=dx*dx+dy*dy+dz*dz,b=ox*dx+oy*dy+oz*dz,c=ox*ox+oy*oy+oz*oz-radius*radius;
        if(c<=0) return 0.0f;
        const float discriminant=b*b-a*c;
        if(discriminant<0||a<1e-6f) return 2.0f;
        return (-b-std::sqrt(discriminant))/a;
    };
    for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index) {
        if(int(index)==owner||runtime.pedestrians[index].health<=0||runtime.pedestrians[index].scriptHandle>=0||
            runtime.pedestrians[index].serviceVehicleIdentity) continue;
        const float fraction=sphere(chest(actors[index].position),melee?0.65f:0.4f);
        if(fraction>=0&&fraction<nearest) {nearest=fraction;hit=int(index);ambientHeadHit=false;}
        const auto& ped=runtime.pedestrians[index];
        if(!melee&&world.paths.vcs&&ped.headValid&&std::isfinite(ped.head.x)&&
            std::isfinite(ped.head.y)&&std::isfinite(ped.head.z)) {
            const float head=sphere(ped.head,.18f);
            if(head>=0&&head<nearest) {nearest=head;hit=int(index);ambientHeadHit=true;}
        }
    }
    bool hitPlayer=false;
    uint8_t playerPiece=0;
    const auto* mission=runtime.missionEnvironment;
    const bool playerHasBulletCollision=!mission||(world.paths.vcs?
        vcsPlayerPrimaryBulletCollision(runtime,bulletTrace):
        !mission->inVehicle||mission->playerCanBeShotInVehicle);
    if((owner>=0||anonymousShooter)&&runtime.health>0&&(melee||playerHasBulletCollision)) {
        const float fraction=sphere(runtime.playerTorsoValid?runtime.playerTorso:chest(player),0.4f);
        if(fraction>=0&&fraction<nearest) {nearest=fraction;hit=-1;hitPlayer=true;}
    }
    int32_t missionHit=-1;uint8_t missionPiece=0;
    bool missionHeadHit=false;
    if(runtime.missionEnvironment) {
        for(const auto& entry:runtime.missionEnvironment->missionCharacters) {
            const auto& character=entry.second;
            if(entry.first==sourceMissionCharacter||!character.alive||
                (!character.visible&&world.paths.vcs)||character.health<=0)continue;
            const bool seated=character.vehicle>=0&&!character.attached;
            if(!melee&&(world.paths.vcs?!vcsMissionPrimaryBulletCollision(runtime,character,bulletTrace):
                seated&&!character.canBeShotInVehicle))continue;
            std::vector<StorylandMissionEnvironment::CharacterHitSphere> nativeSpheres;
            const bool native=!melee&&(!world.paths.vcs||bulletTrace)&&runtime.missionEnvironment->characterHitSpheres&&
                runtime.missionEnvironment->characterHitSpheres(entry.first,nativeSpheres);
            if(native) {
                for(const auto& volume:nativeSpheres) {
                    if(!std::isfinite(volume.radius)||volume.radius<=0)continue;
                    float fraction=nearest;
                    if(world.paths.vcs) {
                        if(!storylandVcsPedSphereIntersection({start.x,start.y,start.z},
                            {end.x,end.y,end.z},volume.center,volume.radius,fraction))continue;
                    } else {
                        fraction=sphere({volume.center[0],volume.center[1],volume.center[2]},volume.radius);
                        if(!(fraction>=0&&fraction<nearest))continue;
                    }
                    nearest=fraction;missionHit=entry.first;missionPiece=volume.piece;hit=-1;hitPlayer=false;missionHeadHit=volume.piece==6;
                }
            } else {
                const auto pose=runtime.missionCombat.find(entry.first);
                if(character.crouch.active&&(pose==runtime.missionCombat.end()||!pose->second.anchorsValid))continue;
                const auto center=pose!=runtime.missionCombat.end()&&pose->second.anchorsValid?
                    pose->second.torso:chest({character.x,character.y,character.z});
                const float fraction=sphere(center,melee?0.65f:0.4f);
                if(fraction>=0&&fraction<nearest) {
                    nearest=fraction;missionHit=entry.first;hit=-1;hitPlayer=false;missionHeadHit=false;missionPiece=0;
                }
                if (!melee && pose != runtime.missionCombat.end() && pose->second.anchorsValid) {
                    const float head = sphere(pose->second.head, .18f);
                    if (head >= 0 && head < nearest) {
                        nearest = head; missionHit = entry.first; hit = -1; hitPlayer = false; missionHeadHit = true;missionPiece=6;
                    }
                }
            }
        }
    }
    int32_t objectHit=-1;StorylandNativePoint objectNormal{};
    if(runtime.missionEnvironment)for(const auto& object:runtime.missionEnvironment->objects) {
        if(storylandTraceScriptObject(world,object,start,end,nearest,objectNormal)) {
            objectHit=object.handle;missionHit=-1;hit=-1;hitPlayer=false;
        }
    }
    StorylandNativePoint vehicleNormal{};
    int ignoredVehicle=firingVehicle;
    if(!bullet&&creditPlayer&&world.paths.vcs&&runtime.missionEnvironment&&runtime.manualAimActive&&
        runtime.manualAimMode==11&&runtime.occupiedVehicle>=0&&size_t(runtime.occupiedVehicle)<runtime.vehicles.size()&&
        runtime.vehicleAction==StorylandPlayTestRuntime::VehicleAction::None&&
        storylandPlayerSeatedDriveByVehicle(*runtime.missionEnvironment,weapon.nativeId)) {
        const auto& environment=*runtime.missionEnvironment;
        const auto car=environment.missionVehicles.find(environment.occupiedMissionVehicle);
        if(car!=environment.missionVehicles.end()&&car->second.spawnedSlot==runtime.occupiedVehicle&&
            runtime.vehicles[size_t(runtime.occupiedVehicle)].scriptHandle==car->first&&
            runtime.playerVehicleSeat==environment.playerPassengerSeat)
            ignoredVehicle=runtime.occupiedVehicle;
    }
    int vehicleHit=traceMissionVehicles(runtime,start,end,nearest,vehicleNormal,ignoredVehicle);
    if(vehicleHit>=0) {objectHit=-1;missionHit=-1;hit=-1;hitPlayer=false;}
    bool hitRemote=false;
    uint8_t remotePiece=0;
    if(creditPlayer && runtime.coopFriendlyFire && runtime.coopRemoteAlive && runtime.coopHitPlayer && (melee || weapon.fireType==1)) {
        float fraction=sphere(runtime.coopRemoteTorso,melee?.65f:.4f);
        if(!melee) {
            auto head=runtime.coopRemoteTorso;head.z+=.55f;
            const float headFraction=sphere(head,.2f);
            if(headFraction>=0&&headFraction<fraction) {fraction=headFraction;remotePiece=6;}
        }
        if(fraction>=0 && fraction<nearest) {
            nearest=fraction;hitRemote=true;objectHit=missionHit=hit=vehicleHit=-1;hitPlayer=false;
        }
    }
    if(world.paths.vcs&&bulletTrace&&vehicleHit>=0) {
        const auto occupant=traceVcsVehicleOccupantHead(runtime,start,end,vehicleHit,
            creditPlayer?0:sourceMissionCharacter);
        if(occupant.ambient>=0) {
            nearest=occupant.fraction;hit=occupant.ambient;ambientHeadHit=true;
            objectHit=vehicleHit=missionHit=-1;hitPlayer=false;
        } else if(occupant.character>=0) {
            nearest=occupant.fraction;hit=objectHit=vehicleHit=-1;
            hitPlayer=occupant.character==0;playerPiece=hitPlayer?6:0;
            missionHit=hitPlayer?-1:occupant.character;
            missionPiece=6;missionHeadHit=true;
        }
    }
    if(!bullet&&!melee&&weapon.fireType==1&&vehicleHit>=0) {
        // Native 2C52C0 retries a vehicle hit with only its flagged occupants
        // temporarily collidable. The original vehicle still receives damage.
        const auto occupant=traceVehicleOccupantShot(runtime,world,actors,start,end,vehicleHit,
            creditPlayer?0:-1,owner);
        if(occupant.character>=0) {
            nearest=occupant.fraction;hit=-1;objectHit=-1;
            hitPlayer=occupant.character==0;
            missionHit=hitPlayer?-1:occupant.character;
            missionPiece=occupant.piece;missionHeadHit=occupant.piece==6;
        }
    }
    const StorylandNativePoint impact{start.x+(end.x-start.x)*nearest,
        start.y+(end.y-start.y)*nearest,start.z+(end.z-start.z)*nearest};
    if(hitRemote) {
        StorylandCoopPlayerHit contact;
        contact.weapon=weapon.nativeId;contact.kind=melee?16:1;
        contact.shot=bullet?bullet->playerShotSerial:runtime.playerShotSerial;
        contact.targetLife=runtime.coopRemoteLife;contact.attackTime=runtime.elapsed;
        contact.hitPiece=remotePiece;
        contact.hitDirection=storylandNativeHitDirection(runtime.coopRemoteHeading,
            start.x-runtime.coopRemotePosition.x,start.y-runtime.coopRemotePosition.y);
        contact.origin=start;contact.impact=impact;runtime.coopHitPlayer(contact);
    }
    if(bulletTrace&&!bullet) {
        weaponTrace(runtime,visualOrigin?*visualOrigin:start,impact,weapon.nativeId,creditPlayer);
        queueVcsBulletPass(runtime,world,visualOrigin?*visualOrigin:start,impact,creditPlayer);
        if(nearest<0.999f && !hitRemote && hit<0 && missionHit<0 &&
           !hitPlayer && vehicleHit<0 && objectHit<0)
            storylandWeaponWorldImpact(runtime,impact);
    }
    if(!bullet&&!melee&&(weapon.nativeId==30||weapon.nativeId==31)) {
        runtime.sniperTraceActive=true;runtime.sniperTraceFrom=start;runtime.sniperTraceTo=impact;
    }
    if(objectHit>=0&&runtime.missionEnvironment&&weapon.fireType!=3) {
        auto& environment=*runtime.missionEnvironment;
        float damage=50;
        if(melee) {
            if(weapon.nativeId==11)damage=200;
            else if(environment.nativeObjectMeleeDamage) {
                const auto& attack=creditPlayer?runtime.playerMelee:runtime.pedestrians[size_t(owner)].melee;
                damage=environment.nativeObjectMeleeDamage(weapon.nativeId,attack.sequence,attack.moving);
            } else damage=0;
        }
        if(damage>0)storylandHitScriptObject(environment,objectHit,weapon.nativeId,damage,melee,
            weapon.nativeId>=21&&weapon.nativeId<=23,objectNormal);
    }
    if(missionHit>=0) {
        auto& character=runtime.missionEnvironment->missionCharacters.at(missionHit);
        bool permitted = !(world.paths.vcs && melee && character.vcsScriptFlags.ignoreMeleeImpacts);
        if (permitted && owner >= 0 && size_t(owner) < runtime.pedestrians.size() && runtime.missionEnvironment) {
            const auto victim = runtime.missionEnvironment->missionCharacters.find(missionHit);
            if (victim != runtime.missionEnvironment->missionCharacters.end())
                permitted = storylandMissionPedGangDamageAllowed(world.paths.vcs, victim->second.gangDamageMask,
                    runtime.pedestrians[size_t(owner)].stats.pedType);
        }
        if (permitted) {
            const auto direction=storylandNativeHitDirection(character.heading+90,origin.x-character.x,origin.y-character.y);
            storylandRequestWeaponHitSpeech(runtime,int(runtime.pedestrians.size())+missionHit,
                {character.x,character.y,character.z},weapon.nativeId);
            hurtMissionCharacter(runtime,missionHit,damage,creditPlayer,melee?16:1,weapon.nativeId,
                sourceMissionCharacter>=0?StorylandMissionEnvironment::DamageSource{
                    StorylandMissionEnvironment::DamageSourceKind::Character,sourceMissionCharacter}:
                    StorylandMissionEnvironment::DamageSource{},
                missionHeadHit,false,false,true,missionPiece,direction);
        }
        if(!world.paths.vcs&&creditPlayer) {
            // CWeapon registers contact events independently of InflictDamage's proof flags.
            runtime.crime.report(melee?lcsMeleeCrime(weapon):StorylandCrime::Shooting,
                player,runtime.elapsed,character.type==6,0x40000000u|uint32_t(missionHit));
        }
    }
    if(vehicleHit>=0) {
        StorylandMissionEnvironment::DamageSource source;
        if(creditPlayer)source={StorylandMissionEnvironment::DamageSourceKind::Character,runtime.crime.vcs?0:1};
        else if(sourceMissionCharacter>=0)
            source={StorylandMissionEnvironment::DamageSourceKind::Character,sourceMissionCharacter};
        else if(!bullet&&!anonymousShooter&&size_t(owner)>=runtime.pedestrians.size())
            source={StorylandMissionEnvironment::DamageSourceKind::Character,owner-int(runtime.pedestrians.size())};
        bool ownVehicle=bullet&&creditPlayer&&vehicleHit==runtime.occupiedVehicle;
        if(bullet&&sourceMissionCharacter>=0&&runtime.missionEnvironment) {
            const auto creator=runtime.missionEnvironment->missionCharacters.find(sourceMissionCharacter);
            ownVehicle=creator!=runtime.missionEnvironment->missionCharacters.end()&&
                creator->second.vehicle==runtime.vehicles[size_t(vehicleHit)].scriptHandle;
        }
        if(!ownVehicle)hurtMissionVehicle(runtime,size_t(vehicleHit),damage,melee?16:1,weapon.nativeId,source);
    }
    if(hit>=0) {
        const auto& victim=runtime.pedestrians[size_t(hit)];
        const float previousHealth=victim.health;
        const float previousArmour=victim.armour;
        hurtPed(runtime,size_t(hit),damage,actors[size_t(hit)].position,creditPlayer,true,
            melee?StorylandCrime::Assault:StorylandCrime::Shooting,&weapon,&origin,true,
            !melee&&ambientHeadHit,-1,0,
            storylandNativeHitDirection(actors[size_t(hit)].yaw,origin.x-actors[size_t(hit)].position.x,
                origin.y-actors[size_t(hit)].position.y),255,actors[size_t(hit)].yaw,
            std::hypot(victim.contact.velocity.x,victim.contact.velocity.y));
        const bool damageApplied=victim.health<previousHealth||victim.armour<previousArmour;
        // Sniper bullet source indices may outlive a population slot, so only
        // immediate ambient contacts can safely identify an assailant here.
        storylandVcsPoliceRememberAssailant(runtime,size_t(hit),bullet?-1:owner,
            creditPlayer,damageApplied);

    }
    if(hitPlayer) {
        const bool usesStrength=melee&&(world.paths.vcs||!(weapon.nativeFlags&(1u<<16)));
        const float strength=usesStrength&&owner>=0&&size_t(owner)<runtime.pedestrians.size()&&
            runtime.pedestrians[size_t(owner)].stats.valid?
                runtime.pedestrians[size_t(owner)].stats.attackStrength:1.0f;
        bool permitted = !runtime.missionEnvironment || !(world.paths.vcs && melee &&
            runtime.missionEnvironment->playerVcsScriptFlags.ignoreMeleeImpacts);
        if(permitted && runtime.missionEnvironment && owner >= 0 && size_t(owner) < runtime.pedestrians.size())
            permitted=storylandMissionPedGangDamageAllowed(world.paths.vcs,
                runtime.missionEnvironment->playerGangDamageMask,runtime.pedestrians[size_t(owner)].stats.pedType);
        if (permitted && sourceMissionCharacter >= 0 && runtime.missionEnvironment) {
            const auto attacker = runtime.missionEnvironment->missionCharacters.find(sourceMissionCharacter);
            if (attacker != runtime.missionEnvironment->missionCharacters.end())
                permitted = storylandMissionPedGangDamageAllowed(world.paths.vcs,
                    runtime.missionEnvironment->playerGangDamageMask, attacker->second.type);
        }
        if(permitted)storylandRequestWeaponHitSpeech(runtime,-1,player,weapon.nativeId);
        if(permitted&&runtime.inflictDamage(damage*strength,melee?16:1,weapon.nativeId,true,playerPiece,
                runtime.missionEnvironment?storylandNativeHitDirection(runtime.missionEnvironment->heading+90,
                    origin.x-player.x,origin.y-player.y):0,false,&origin)) {
            if(runtime.missionEnvironment) {
                runtime.missionEnvironment->playerLastWeaponDamage=weapon.nativeId;
                runtime.missionEnvironment->playerDamageSource=sourceMissionCharacter>=0?
                    StorylandMissionEnvironment::DamageSource{
                        StorylandMissionEnvironment::DamageSourceKind::Character,sourceMissionCharacter}:
                    StorylandMissionEnvironment::DamageSource{};
            }
        }
    }
    WeaponSegmentResult result;
    result.impact=impact;
    result.hitPedestrian=hit>=0||missionHit>=0||hitPlayer||hitRemote;
    result.hitStatic=nearest<1&&!result.hitPedestrian&&vehicleHit<0&&objectHit<0;
    if(bullet&&result.hitStatic)
        result.hitBuilding=storylandTraceNativeBuildings(world,start,end)<=nearest;
    return result;
}

bool addVcsSniperBullet(StorylandPlayTestRuntime& runtime,
    const StorylandNativePoint& origin, const StorylandNativePoint& destination,
    size_t weaponIndex, StorylandVcsBulletSource sourceKind, int32_t sourceHandle) {
    if(weaponIndex>=runtime.weapons.size())return false;
    const auto& weapon=runtime.weapons[weaponIndex];
    if(weapon.nativeId!=30&&weapon.nativeId!=31)return false;
    const float distance=separation(origin,destination);
    if(!std::isfinite(distance)||!std::isfinite(weapon.damage))return false;
    const auto free=std::find_if(runtime.vcsSniperBullets.begin(),runtime.vcsSniperBullets.end(),
        [](const auto& item){return !item.active;});
    if(free==runtime.vcsSniperBullets.end())return false;
    StorylandVcsSniperBullet bullet;
    bullet.active=true;
    bullet.position=bullet.origin=origin;
    bullet.velocity=distance>0?StorylandNativePoint{
        (destination.x-origin.x)*16.0f/distance,
        (destination.y-origin.y)*16.0f/distance,
        (destination.z-origin.z)*16.0f/distance}:StorylandNativePoint{16,0,0};
    bullet.weapon=weaponIndex;
    bullet.nativeWeapon=weapon.nativeId;
    bullet.damage=weapon.damage;
    const uint32_t now=runtime.missionEnvironment?runtime.missionEnvironment->gameMilliseconds:
        uint32_t(std::max(0.0f,runtime.elapsed)*1000.0f);
    bullet.expiresAtMilliseconds=float(int32_t(now+1000u));
    bullet.sourceKind=sourceKind;
    bullet.sourceHandle=sourceHandle;
    bullet.playerShotSerial=runtime.playerShotSerial;
    *free=bullet;
    return true;
}

void updateVcsSniperBullets(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, std::vector<StorylandNativeTrafficActor>& actors,
    const StorylandNativePoint& player, float seconds) {
    if(!world.paths.vcs)return;
    const float now=runtime.missionEnvironment?float(runtime.missionEnvironment->gameMilliseconds):
        std::max(0.0f,runtime.elapsed)*1000.0f;
    for(auto& bullet:runtime.vcsSniperBullets) {
        if(!bullet.active)continue;
        if(bullet.weapon>=runtime.weapons.size()||
            runtime.weapons[bullet.weapon].nativeId!=bullet.nativeWeapon||
            (bullet.nativeWeapon!=30&&bullet.nativeWeapon!=31)) {
            bullet.active=false;continue;
        }
        if(bullet.expiresAtMilliseconds<now)bullet.active=false;
        const auto previous=bullet.position;
        StorylandNativePoint next{previous.x+bullet.velocity.x*seconds*25.0f,
            previous.y+bullet.velocity.y*seconds*25.0f,
            previous.z+bullet.velocity.z*seconds*25.0f};
        if(!std::isfinite(next.x)||!std::isfinite(next.y)||!std::isfinite(next.z)||
            next.x<=-2400||next.x>=1600||next.y<=-2000||next.y>=2000) {
            bullet.active=false;continue;
        }
        int owner=INT32_MAX;
        if(bullet.sourceKind==StorylandVcsBulletSource::Player)owner=-1;
        else if(bullet.sourceKind==StorylandVcsBulletSource::AmbientCharacter) {
            if(bullet.sourceHandle>=0&&size_t(bullet.sourceHandle)<runtime.pedestrians.size()&&
                size_t(bullet.sourceHandle)<actors.size())owner=bullet.sourceHandle;
            else {bullet.sourceKind=StorylandVcsBulletSource::None;bullet.sourceHandle=-1;}
        } else if(bullet.sourceKind==StorylandVcsBulletSource::MissionCharacter) {
            if(!runtime.missionEnvironment||runtime.missionEnvironment->missionCharacters.count(bullet.sourceHandle)==0) {
                bullet.sourceKind=StorylandVcsBulletSource::None;bullet.sourceHandle=-1;
            }
        }
        const auto contact=applyWeaponSegment(runtime,world,actors,player,bullet.origin,
            previous,next,runtime.weapons[bullet.weapon],owner,&bullet);
        if(contact.hitStatic)storylandWeaponWorldImpact(runtime,contact.impact);
        if(contact.hitPedestrian) {
            bullet.active=false;
            next=contact.impact;
        } else if(contact.hitBuilding&&runtime.missionEnvironment&&
            runtime.missionEnvironment->vcsStopSniperBulletsOnBuildings!=0)bullet.active=false;
        weaponTrace(runtime,previous,next,bullet.nativeWeapon,
            bullet.sourceKind==StorylandVcsBulletSource::Player);
        queueVcsBulletPass(runtime,world,previous,next,bullet.sourceKind==StorylandVcsBulletSource::Player);
        runtime.sniperTraceActive=true;
        runtime.sniperTraceFrom=previous;
        runtime.sniperTraceTo=next;
        bullet.position=next;
    }
}

void shoot(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    std::vector<StorylandNativeTrafficActor>& actors,const StorylandNativePoint& player,
    StorylandNativePoint origin,float yaw,size_t weaponIndex,int owner,int preferredTarget,bool applyMeleeContact=false,
    int32_t preferredMissionTarget=-1) {
    const auto& weapon=runtime.weapons[weaponIndex];
    const bool melee=weapon.fireType==0;
    const bool nativePoliceAim=weapon.fireType==1&&owner>=0&&
        size_t(owner)<runtime.pedestrians.size()&&
        runtime.pedestrians[size_t(owner)].isPolice(world.paths.vcs);
    if(melee&&!applyMeleeContact) {
        if(owner<0&&runtime.playerWater.active)return;
        if(owner>=0&&(size_t(owner)>=runtime.pedestrians.size()||runtime.pedestrians[size_t(owner)].water.active))return;
        auto& attack=owner<0?runtime.playerMelee:runtime.pedestrians[size_t(owner)].melee;
        if(attack.active)return;
        const bool moving=owner<0?runtime.movementSpeed>.15f:runtime.pedestrians[size_t(owner)].combatSpeed>.15f;
        if(owner<0&&!moving) {
            int target=-1;float nearest=weapon.range+.3f;
            for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index) {
                const float distance=separation(origin,actors[index].position);
                const float angle=std::atan2(actors[index].position.y-origin.y,actors[index].position.x-origin.x)/radians;
                if(runtime.pedestrians[index].health>0&&distance<nearest&&
                    std::fabs(std::remainder(angle-yaw,360.0f))<60&&visible(world,origin,actors[index].position)) {
                    target=int(index);nearest=distance;
                }
            }
            if(!world.paths.vcs&&runtime.missionEnvironment) {
                for(const auto& entry:runtime.missionEnvironment->missionCharacters) {
                    const auto& character=entry.second;
                    if(!character.alive||character.health<=0||character.vehicle>=0)continue;
                    const StorylandNativePoint position{character.x,character.y,character.z};
                    const float distance=separation(origin,position);
                    const float angle=std::atan2(position.y-origin.y,position.x-origin.x)/radians;
                    if(distance<nearest&&std::fabs(std::remainder(angle-yaw,360.0f))<60&&visible(world,origin,position)) {
                        target=-1;preferredMissionTarget=entry.first;nearest=distance;
                    }
                }
            }
            if(runtime.coopFriendlyFire && runtime.coopRemoteAlive && runtime.coopHitPlayer) {
                const auto& position=runtime.coopRemotePosition;
                const float distance=separation(origin,position);
                const float angle=std::atan2(position.y-origin.y,position.x-origin.x)/radians;
                if(distance<nearest && std::fabs(std::remainder(angle-yaw,360.0f))<60 && visible(world,origin,position)) {
                    target=-2;preferredMissionTarget=-1;nearest=distance;
                }
            }
            preferredTarget=target;
        }
        ++attack.sequence;attack.active=true;attack.moving=moving;attack.elapsed=0;attack.recovery=0;
        attack.contactApplied=false;attack.weapon=weaponIndex;attack.target=preferredTarget;
        attack.missionTarget=preferredMissionTarget;
        const size_t move=(attack.sequence-1)%3;
        attack.duration=std::max(.1f,moving?weapon.runningMeleeDuration:weapon.meleeDurations[move]);
        attack.contactTime=std::clamp(moving?weapon.runningMeleeContactTime:weapon.meleeContactTimes[move],0.0f,attack.duration);
        if(owner<0)++runtime.playerShotSerial;else ++runtime.pedestrians[size_t(owner)].shotSerial;
        sound(runtime,StorylandPlayTestSound::Melee,origin,int(weaponIndex),owner);return;
    }
    if(!melee) {
        if(owner<0) {
            ++runtime.playerShotSerial;runtime.statistics["RoundsFiredByPlayer"]+=1;
            runtime.playerLastShotTime=runtime.elapsed;
            runtime.playerLastGunshotTime=runtime.elapsed;
            runtime.playerLastGunshotPosition=origin;
        }
        else if(size_t(owner)<runtime.pedestrians.size())++runtime.pedestrians[size_t(owner)].shotSerial;
        sound(runtime,StorylandPlayTestSound::WeaponShot,origin,int(weaponIndex),owner);
    }
    auto start=chest(origin);
    if(!melee) {
        if(owner<0&&runtime.playerMuzzleValid)start=runtime.playerMuzzle;
        else if(owner>=0&&size_t(owner)<runtime.pedestrians.size()&&runtime.pedestrians[size_t(owner)].weaponMuzzleValid)
            start=runtime.pedestrians[size_t(owner)].weaponMuzzle;
    }
    if(weapon.fireType==2) {
        if(runtime.projectiles.size()>=64)return;
        const auto kind=projectileKind(weapon);
        const bool rocket=kind==StorylandProjectileKind::Rocket;
        const float nativeSpeed=weapon.speed<10?weapon.speed*50.0f:weapon.speed;
        const float speed=std::clamp(nativeSpeed,rocket?25.0f:5.0f,rocket?120.0f:25.0f);
        const float pitch=owner<0?-runtime.aimPitch*radians:0.0f;
        StorylandNativePoint velocity{std::cos(yaw*radians)*std::cos(pitch)*speed,
            std::sin(yaw*radians)*std::cos(pitch)*speed,std::sin(pitch)*speed};
        StorylandNativePoint target{};bool aimed=false;
        if(owner<0&&runtime.freeAimActive) {target=runtime.freeAimTarget;aimed=true;}
        else if(owner<0&&runtime.manualAimActive) {target=runtime.manualAimTarget;aimed=true;}
        else if(owner<0&&storylandHasPlayTestTarget(runtime)) {
            target=runtime.targetPosition;aimed=true;
        } else if(preferredTarget>=0&&size_t(preferredTarget)<actors.size()) {target=chest(actors[size_t(preferredTarget)].position);aimed=true;}
        else if(owner>=0) {target=chest(player);aimed=true;}
        if(aimed) {
            const float distance=std::max(0.01f,separation(start,target));
            velocity={(target.x-start.x)*speed/distance,(target.y-start.y)*speed/distance,(target.z-start.z)*speed/distance};
            if(!rocket) {
                const float flight=std::clamp(std::hypot(target.x-start.x,target.y-start.y)/speed,0.15f,2.0f);
                velocity.z=(target.z-start.z+4.905f*flight*flight)/flight;
            }
        } else if(!rocket)velocity.z=4.0f;
        StorylandWeaponProjectile projectile{start,velocity,weapon.lifetime,weapon.damage,
            std::clamp(weapon.radius,1.0f,20.0f),owner,!rocket};
        projectile.kind=kind;projectile.weapon=weaponIndex;
        prepareUnusedProjectile(runtime,projectile,yaw);
        runtime.projectiles.push_back(projectile);return;
    }
    const bool bulletTrace=storylandWeaponUsesBulletTrace(weapon);
    if(bulletTrace)muzzleFlash(runtime,start,weapon.nativeId,
        owner<0?StorylandMuzzleOwner::Player:StorylandMuzzleOwner::AmbientPed,owner);
    const bool mountedCameraRay=world.paths.vcs&&owner<0&&weapon.nativeId==34&&
        runtime.manualAimActive&&runtime.manualAimMode==45&&!runtime.freeAimActive&&
        runtime.missionEnvironment&&runtime.missionEnvironment->activeVcsCharacterAttachment(0)&&
        !runtime.missionEnvironment->inVehicle&&runtime.occupiedVehicle<0&&
        runtime.vehicleAction==StorylandPlayTestRuntime::VehicleAction::None;
    // Original player mode 45 calls 0x2CB1D0: collision starts at the active camera
    // source and follows its forward vector for the weapon range. Keep the
    // independently sampled hand muzzle for the existing visual effects.
    const auto collisionStart=mountedCameraRay?runtime.manualAimOrigin:start;
    StorylandNativePoint end{start.x+std::cos(yaw*radians)*weapon.range,
        start.y+std::sin(yaw*radians)*weapon.range,start.z};
    if(!world.paths.vcs&&!melee&&owner<0&&preferredTarget<0) {
        const float pitch=-runtime.aimPitch*radians;
        end={start.x+std::cos(yaw*radians)*std::cos(pitch)*weapon.range,
            start.y+std::sin(yaw*radians)*std::cos(pitch)*weapon.range,start.z+std::sin(pitch)*weapon.range};
    }
    if(owner<0&&melee&&preferredTarget==-2&&runtime.coopFriendlyFire&&runtime.coopRemoteAlive)end=chest(runtime.coopRemotePosition);
    else if(owner<0&&runtime.freeAimActive)end=runtime.freeAimTarget;
    else if(owner<0&&runtime.manualAimActive)end=runtime.manualAimTarget;
    else if(owner<0&&storylandHasPlayTestTarget(runtime))
        end=runtime.targetPosition;
    else if((!world.paths.vcs||owner>=0)&&preferredMissionTarget>=0&&runtime.missionEnvironment) {
        const auto found=runtime.missionEnvironment->missionCharacters.find(preferredMissionTarget);
        if(found!=runtime.missionEnvironment->missionCharacters.end()&&found->second.alive&&found->second.health>0)
            end=chest({found->second.x,found->second.y,found->second.z});
    } else if(preferredTarget>=0&&size_t(preferredTarget)<actors.size()) end=chest(actors[size_t(preferredTarget)].position);
    else if(nativePoliceAim)end=runtime.playerTorsoValid?runtime.playerTorso:chest(player);
    else if(owner>=0) {
        const float distance=separation(origin,player);
        end={start.x+std::cos(yaw*radians)*distance,start.y+std::sin(yaw*radians)*distance,chest(player).z};
    }
    const bool extendedAccuracy=world.paths.vcs ? runtime.extendedAndSaFeatures : runtime.extendedLcsCrouch;
    if(nativePoliceAim) {
        const auto& officer=runtime.pedestrians[size_t(owner)];
        end=storylandPoliceInstantHitEnd(runtime,officer,collisionStart,end,weapon.range,
            preferredTarget<0&&preferredMissionTarget<0,
            storylandSaCrouchSpreadScale(extendedAccuracy,officer.extendedCrouch));
    }
    if(weapon.fireType==1 && extendedAccuracy) {
        if(owner<0 && !mountedCameraRay) {
            end=storylandSaFirearmAimPoint(collisionStart,end,true,runtime.extendedMovement.crouched,
                storylandHasPlayTestTarget(runtime),runtime.playerShotSerial);
        } else if(!nativePoliceAim && owner>=0 && size_t(owner)<runtime.pedestrians.size() &&
            (preferredTarget>=0 || preferredMissionTarget>=0)) {
            const auto& ped=runtime.pedestrians[size_t(owner)];
            end=storylandSaFirearmAimPoint(collisionStart,end,true,ped.extendedCrouch,false,
                ped.shotSerial+uint32_t(owner)*0x9e3779b9u);
        }
    }
    const float length=separation(collisionStart,end);
    if(length<0.001f) return;
    if(!nativePoliceAim) {
        const float scale=weapon.range/length;
        end={collisionStart.x+(end.x-collisionStart.x)*scale,
            collisionStart.y+(end.y-collisionStart.y)*scale,
            collisionStart.z+(end.z-collisionStart.z)*scale};
    }
    if(world.paths.vcs&&(weapon.nativeId==30||weapon.nativeId==31)) {
        addVcsSniperBullet(runtime,start,end,weaponIndex,
            owner<0?StorylandVcsBulletSource::Player:StorylandVcsBulletSource::AmbientCharacter,
            owner<0?0:owner);
        return;
    }
    applyWeaponSegment(runtime,world,actors,player,origin,collisionStart,end,weapon,owner,
        nullptr,mountedCameraRay?&start:nullptr);
}
}

bool storylandApplyMissionCharacterDamage(StorylandPlayTestRuntime& runtime,
    const StorylandMissionCharacterDamage& request, bool creditLocalPlayer) {
    if (!runtime.missionEnvironment || !std::isfinite(request.amount) || request.amount <= 0 ||
        request.sourceKind > uint8_t(StorylandMissionEnvironment::DamageSourceKind::Object) ||
        request.hitPiece>6||request.hitDirection>3||
        (request.kind != 0 && request.kind != 1 && request.kind != 2 && request.kind != 4 &&
            request.kind != 8 && request.kind != 16)) return false;
    const auto found = runtime.missionEnvironment->missionCharacters.find(request.handle);
    if (found == runtime.missionEnvironment->missionCharacters.end() || found->second.model != request.model)
        return false;
    hurtMissionCharacter(runtime, request.handle, request.amount, request.playerAttack, request.kind,
        request.weapon, {static_cast<StorylandMissionEnvironment::DamageSourceKind>(request.sourceKind),
            request.sourceHandle}, request.headHit, request.critical, true, creditLocalPlayer,
            request.hitPiece,request.hitDirection);
    return true;
}

bool storylandWreckPlayTestVehicle(StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& vehicles,size_t slot) {
    if(slot>=vehicles.size()||slot>=runtime.vehicles.size())return false;
    auto& vehicle=runtime.vehicles[slot];
    if(vehicle.charred)return false;
    // Commit destruction before applying radial damage; chain reactions must
    // never detonate this same vehicle again on the following frame.
    vehicle.health=0;vehicle.wrecked=true;vehicle.charred=true;
    vehicle.engineOn=false;vehicle.aiActive=false;vehicle.hasDriver=false;vehicle.missionDriver=false;
    vehicle.headlightsOn=false;vehicle.braking=false;vehicle.siren=false;vehicle.taxiLight=false;
    vehicle.throttle=0;vehicle.alarmMilliseconds=0;vehicle.engineFireTime=0;vehicle.engineFireSpent=true;
    vehicle.visiblyDamaged=true;vehicle.tyreStatus.fill(1);
    vehicle.doorDamage.fill(1);vehicle.panelDamage.fill(2);
    vehicle.panelDamage[4]=3;vehicle.lightDamage.fill(1);
    if(!vehicle.craneCarried&&!vehicle.positionFrozen)vehicle.verticalSpeed+=6.5f;
    for(auto& effect:runtime.effects)if(effect.vehicle==int(slot)) {effect.remaining=0;effect.vehicle=-1;}
    vehicle.burning=vehicle.water.immersion<=.1f;
    if(vehicle.burning) {
        StorylandRuntimeEffect fire;
        fire.position=vehicles[slot].position;fire.position.z+=.4f;
        fire.kind=StorylandParticleKind::Flame;fire.vehicle=int(slot);
        fire.remaining=10;fire.radius=1.2f;fire.damage=7;fire.owner=INT32_MAX;
        runtime.effects.push_back(fire);
    }
    if(runtime.missionEnvironment) {
        auto& world=*runtime.missionEnvironment;
        const auto found=world.missionVehicles.find(vehicle.scriptHandle);
        if(found!=world.missionVehicles.end()) {
            auto& car=found->second;
            car.health=0;car.healthChangedByScript=true;car.alive=false;car.wrecked=true;
            car.scriptStatus=5;car.engineOn=false;car.drivingToTarget=false;
            car.aiActive=false;car.siren=false;car.taxiLight=false;car.alarmMilliseconds=0;
            car.visiblyDamaged=true;car.doorDamage=vehicle.doorDamage;car.panelDamage=vehicle.panelDamage;
            car.lightDamage=vehicle.lightDamage;car.tyreStatus=vehicle.tyreStatus;
            storylandClearVehicleBomb(car);
        }
        for(auto& entry:world.missionCharacters) {
            auto& ped=entry.second;
            if(vehicle.scriptHandle>=0&&ped.vehicle==vehicle.scriptHandle&&!ped.attached) {
                ped.health=0;ped.alive=false;ped.walkingToTarget=false;
            }
        }
        if(int(slot)==runtime.occupiedVehicle) {
            runtime.health=0;world.health=0;
        }
        if(vehicle.scriptHandle>=0&&world.remoteVehicle==vehicle.scriptHandle)world.remoteVehicle=-1;
    } else if(int(slot)==runtime.occupiedVehicle)runtime.health=0;
    return true;
}

void storylandDetonatePlayTestVehicle(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld* native,const std::vector<StorylandNativeTrafficActor>& vehicles,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,const StorylandNativePoint& player,size_t slot) {
    if(!storylandWreckPlayTestVehicle(runtime,vehicles,slot))return;
    const auto& vehicle=runtime.vehicles[slot];
    const auto& position=vehicles[slot].position;
    StorylandMissionEnvironment::ScriptExplosion explosion;
    const auto& definition=vehicle.nativeDefinition;
    explosion.type=definition.twoWheels()?3:definition.domain==StorylandVehicleDomain::Water?5:
        definition.domain==StorylandVehicleDomain::Air?6:3;
    explosion.position={position.x,position.y,position.z};explosion.active=true;
    explosion.source={StorylandMissionEnvironment::DamageSourceKind::Vehicle,vehicle.scriptHandle};
    std::string error;
    if(native&&runtime.missionEnvironment)
        storylandCreatePlayTestExplosion(runtime,*native,vehicles,pedestrians,player,explosion,error,slot);
    else if(runtime.missionEnvironment&&runtime.missionEnvironment->createScriptExplosion)
        runtime.missionEnvironment->createScriptExplosion(explosion,error);
    else {
        for(int index=0;index<28;++index) {
            const float angle=randomUnit(runtime)*6.28318530718f,speed=1+randomUnit(runtime)*5;
            particle(runtime,index<12?StorylandParticleKind::Explosion:StorylandParticleKind::Smoke,
                position,{std::cos(angle)*speed,std::sin(angle)*speed,1+randomUnit(runtime)*3},
                index<12?.5f:.35f,index<12?.35f:2.5f);
        }
        sound(runtime,StorylandPlayTestSound::Explosion,position);
    }
}

bool storylandCreatePlayTestExplosion(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& native,
    const std::vector<StorylandNativeTrafficActor>& cars,const std::vector<StorylandNativeTrafficActor>& actors,
    const StorylandNativePoint& player,StorylandMissionEnvironment::ScriptExplosion& explosion,std::string& error,
    size_t sourceVehicleSlot) {
    StorylandExplosionProfile profile;
    if(!runtime.missionEnvironment||!storylandExplosionProfile(explosion.type,profile)) {
        error="Native explosion requires an attached script world and valid type.";return false;
    }
    if(!runtime.crime.vcs)
        profile.weapon=storylandNativeExplosionWeapon(false);
    const auto& p=explosion.position;
    if(!std::all_of(p.begin(),p.end(),[](float v){return std::isfinite(v);})) {
        error="Native explosion coordinates are non-finite.";return false;
    }
    auto& world=*runtime.missionEnvironment;const StorylandNativePoint position{p[0],p[1],p[2]};
    if(sourceVehicleSlot>=cars.size()||sourceVehicleSlot>=runtime.vehicles.size())sourceVehicleSlot=SIZE_MAX;
    if(sourceVehicleSlot==SIZE_MAX&&
        explosion.source.kind==StorylandMissionEnvironment::DamageSourceKind::Vehicle&&explosion.source.handle>=0) {
        const auto source=world.missionVehicles.find(explosion.source.handle);
        if(source!=world.missionVehicles.end()&&!source->second.deleted&&source->second.spawnedSlot>=0) {
            const size_t slot=size_t(source->second.spawnedSlot);
            if(slot<cars.size()&&slot<runtime.vehicles.size()&&
                runtime.vehicles[slot].scriptHandle==explosion.source.handle)sourceVehicleSlot=slot;
        }
    }
    if(explosion.type>=3&&explosion.type<=6&&
        explosion.source.kind==StorylandMissionEnvironment::DamageSourceKind::Vehicle&&explosion.source.handle>=0) {
        const auto source=world.missionVehicles.find(explosion.source.handle);
        if(source!=world.missionVehicles.end()) {
            auto& car=source->second;
            car.health=0;car.healthChangedByScript=true;car.alive=false;car.wrecked=true;
            car.scriptStatus=5;car.engineOn=false;car.drivingToTarget=false;
            car.aiActive=false;car.siren=false;car.taxiLight=false;car.alarmMilliseconds=0;
            storylandClearVehicleBomb(car);
            for(auto& entry:world.missionCharacters)if(entry.second.vehicle==explosion.source.handle&&!entry.second.attached) {
                entry.second.health=0;entry.second.alive=false;entry.second.walkingToTarget=false;
            }
            if(world.inVehicle&&world.occupiedMissionVehicle==explosion.source.handle)runtime.health=world.health=0;
        }
        if(source!=world.missionVehicles.end()&&source->second.spawnedSlot>=0) {
            const size_t slot=size_t(source->second.spawnedSlot);
            if(slot<runtime.vehicles.size()&&runtime.vehicles[slot].scriptHandle==explosion.source.handle)
                storylandWreckPlayTestVehicle(runtime,cars,slot);
        }
    }
    const bool playerAttack=explosion.source.kind==StorylandMissionEnvironment::DamageSourceKind::Character&&
        explosion.source.handle==(runtime.crime.vcs?0:1);
    if(explosion.type==1) {
        float ground=0,water=0;
        if(world.groundHeightAt&&world.groundHeightAt({p[0],p[1],p[2]+2},ground)) {
            if(storylandNativeWaterHeight(native,p[0],p[1],water)&&ground<water&&ground>water-6) {
                explosion.active=false;return true;
            }
            int32_t slot=0;while(slot<40&&world.scriptFires.count(slot))++slot;
            if(slot<40) {
                StorylandMissionEnvironment::ScriptFire fire;
                fire.position={p[0],p[1],ground};fire.scriptOwned=false;
                fire.source=explosion.source;
                fire.endsAt=world.gameMilliseconds+10000;fire.scale=.7f;world.scriptFires[slot]=fire;
            }
        }
    } else {
        const auto factor=[&](const StorylandNativePoint& point) {
            const float distance=separation(position,point);
            return distance<profile.radius?std::min(1.0f,2.0f*(1-distance/profile.radius)):0.0f;
        };
        if (playerAttack && runtime.coopFriendlyFire && runtime.coopRemoteAlive && runtime.coopHitPlayer &&
            factor(runtime.coopRemotePosition) > 0) {
            StorylandCoopPlayerHit hit;
            hit.kind = 4; hit.weapon = profile.weapon; hit.explosionType = explosion.type;
            if (++runtime.coopDamageSerial == 0) ++runtime.coopDamageSerial;
            hit.shot = runtime.coopDamageSerial; hit.targetLife = runtime.coopRemoteLife;
            hit.attackTime = runtime.elapsed; hit.origin = position; hit.impact = runtime.coopRemotePosition;
            runtime.coopHitPlayer(hit);
        }
        for(auto& entry:world.missionCharacters) {
            auto& character=entry.second;
            // 192AE4/219D50 exclude seated vehicle occupants from radial ped
            // damage. Authored entry/exit and exterior attachments are separate.
            const bool exiting=runtime.crime.vcs?world.vcsCharacterExitingVehicle(entry.first):
                character.objective==0x00E8||character.objective==0x00F1;
            if(character.vehicle>=0&&!character.attached&&!exiting)continue;
            const float amount=factor({character.x,character.y,character.z});
            if(amount>0)hurtMissionCharacter(runtime,entry.first,250*amount,playerAttack,4,profile.weapon,explosion.source);
        }
        for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index) {
            const float amount=factor(actors[index].position);
            if(amount>0)hurtPed(runtime,index,250*amount,actors[index].position,playerAttack,
                true,StorylandCrime::Assault,nullptr,&position,true,false,profile.weapon,0,0,4);
        }
        const float playerAmount=factor(player);
        if(playerAmount>0&&(!world.inVehicle||world.playerVehicleTransition)&&
            runtime.inflictDamage(250*playerAmount,4,profile.weapon,true,0,0,playerAttack)) {
            world.health=runtime.health;world.armour=runtime.armour;
            world.playerLastWeaponDamage=profile.weapon;world.playerDamageSource=explosion.source;
        }
        for(size_t index=0;index<cars.size()&&index<runtime.vehicles.size();++index) {
            auto& vehicle=runtime.vehicles[index];const float amount=factor(cars[index].position);
            if (playerAttack && runtime.coopConnected && !runtime.coopFriendlyFire && vehicle.coopControlled) continue;
            if(amount<=0||!vehicle.canBeDamaged||(vehicle.onlyDamagedByPlayer&&!playerAttack)||(vehicle.damageProofs&4u))continue;
            const float previousHealth=vehicle.health;
            vehicle.health=std::max(0.0f,vehicle.health-1100*amount);
            if(playerAttack&&previousHealth>0&&vehicle.health<=0)
                storylandRegisterKillFrenzyDeath(runtime,int32_t(vehicle.nativeDefinition.modelId),profile.weapon,false,true);
            vehicle.visiblyDamaged = true;
            if (vehicle.doorDamage[0] == 0) vehicle.doorDamage[0] = 1;
            const float distance=std::max(.01f,separation(position,cars[index].position));
            const float impulse=profile.force*amount*0.0007142857066355646f*50/distance;
            const auto& at=cars[index].position;const float yaw=cars[index].yaw*.0174532925199433f;
            const float dx=(at.x-position.x)*impulse,dy=(at.y-position.y)*impulse;
            vehicle.speed+=std::cos(yaw)*dx+std::sin(yaw)*dy;
            vehicle.lateralSpeed+=-std::sin(yaw)*dx+std::cos(yaw)*dy;
            vehicle.verticalSpeed+=std::max(0.0f,at.z-position.z)*impulse;
            const auto script=world.missionVehicles.find(vehicle.scriptHandle);
            if(script!=world.missionVehicles.end()) {
                script->second.visiblyDamaged = vehicle.visiblyDamaged;
                script->second.doorDamage = vehicle.doorDamage;
                script->second.health=vehicle.health;script->second.lastWeaponDamage=profile.weapon;
                script->second.lastDamageSource=explosion.source;
                if(vehicle.health<=0)script->second.alive=false;
            }
        }
        for(auto& entry:world.missionVehicles) {
            auto& car=entry.second;
            if(car.spawnedSlot>=0&&size_t(car.spawnedSlot)<runtime.vehicles.size())continue;
            const float amount=factor({car.x,car.y,car.z});
            if(amount<=0||car.deleted||!car.canBeDamaged||(car.onlyDamagedByPlayer&&!playerAttack)||(car.damageProofs&4u))continue;
            const float previousHealth=car.health;
            car.health=std::max(0.0f,car.health-1100*amount);car.healthChangedByScript=true;
            if(playerAttack&&previousHealth>0&&car.health<=0)
                storylandRegisterKillFrenzyDeath(runtime,car.model,profile.weapon,false,true);
            car.lastWeaponDamage=profile.weapon;car.lastDamageSource=explosion.source;if(car.health<=0)car.alive=false;
        }
        storylandExplodeScriptObjects(world,position,profile.radius,profile.weapon,profile.force);
    }
    storylandShakeCameraAt(world,position,explosion.type==1?.2f:.4f);
    const bool vehiclePresentation=explosion.type>=3&&explosion.type<=5&&
        startVehicleExplosionPresentation(runtime,cars,position,explosion.type,sourceVehicleSlot);
    if(!vehiclePresentation)for(int index=0;index<28;++index) {
        const float angle=randomUnit(runtime)*6.28318530718f,speed=1+randomUnit(runtime)*5;
        particle(runtime,index<12?StorylandParticleKind::Explosion:StorylandParticleKind::Smoke,position,
            {std::cos(angle)*speed,std::sin(angle)*speed,1+randomUnit(runtime)*3},index<12?.5f:.35f,index<12?.35f:2.5f);
    }
    if(runtime.crime.vcs||!world.lcsExtendedWorld.suppressExplosionSound)
        sound(runtime,StorylandPlayTestSound::Explosion,position);
    return true;
}

void storylandDamagePlayTestPed(StorylandPlayTestRuntime& runtime,size_t index,float damage,
    const StorylandNativePoint& position,bool playerAttack,bool vehicleImpact,uint8_t hitDirection,
    const StorylandNativePoint* source) {
    if(index>=runtime.pedestrians.size()||!std::isfinite(damage))return;
    hurtPed(runtime,index,damage,position,playerAttack,true,
        vehicleImpact?StorylandCrime::Runover:StorylandCrime::Assault,nullptr,source,!vehicleImpact,
        false,vehicleImpact?storylandNativeRunoverWeapon(runtime.crime.vcs):-1,0,hitDirection,
        vehicleImpact?8:16);
}

void storylandDamagePlayTestPedWithVehicle(StorylandPlayTestRuntime& runtime,size_t index,float damage,
    const StorylandNativePoint& position,bool playerAttack,int32_t nativeWeapon,uint8_t hitDirection,
    const StorylandNativePoint& source,uint8_t hitPiece) {
    if(index>=runtime.pedestrians.size()||!std::isfinite(damage)||
        (nativeWeapon!=storylandNativeRammedWeapon(runtime.crime.vcs)&&
         nativeWeapon!=storylandNativeRunoverWeapon(runtime.crime.vcs)))return;
    hurtPed(runtime,index,damage,position,playerAttack,true,StorylandCrime::Runover,nullptr,
        &source,false,hitPiece==6,nativeWeapon,hitPiece,hitDirection,8);
}

bool storylandBlowUpMissionVehicle(StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& vehicles,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,int32_t handle) {
    if(!runtime.missionEnvironment)return false;
    auto& environment=*runtime.missionEnvironment;
    const auto found=environment.missionVehicles.find(handle);
    if(found==environment.missionVehicles.end()||found->second.deleted||!found->second.canBeDamaged)return false;
    auto& scripted=found->second;
    const int32_t slot=scripted.spawnedSlot;
    const bool spawned=slot>=0&&size_t(slot)<vehicles.size()&&size_t(slot)<runtime.vehicles.size()&&
        runtime.vehicles[size_t(slot)].scriptHandle==handle;
    const StorylandNativePoint position=spawned?vehicles[size_t(slot)].position:
        StorylandNativePoint{scripted.x,scripted.y,scripted.z};
    if(scripted.wrecked)return true;
    if(spawned) {
        storylandDetonatePlayTestVehicle(runtime,nullptr,vehicles,pedestrians,
            {environment.x,environment.y,environment.z},size_t(slot));
    } else {
        scripted.health=0;scripted.alive=false;scripted.healthChangedByScript=true;scripted.wrecked=true;
        scripted.scriptStatus=5;scripted.engineOn=false;scripted.drivingToTarget=false;
        scripted.aiActive=false;scripted.siren=false;scripted.taxiLight=false;
        storylandClearVehicleBomb(scripted);
        for(auto& entry:environment.missionCharacters)if(entry.second.vehicle==handle&&!entry.second.attached) {
            entry.second.health=0;entry.second.alive=false;
        }
        if(environment.inVehicle&&environment.occupiedMissionVehicle==handle)runtime.health=environment.health=0;
        std::string error;
        storylandAddScriptExplosion(environment,3,{position.x,position.y,position.z},error,
            {StorylandMissionEnvironment::DamageSourceKind::Vehicle,handle});
    }
    return true;
}

bool storylandExplodeMissionCharacterHead(StorylandPlayTestRuntime& runtime, int32_t handle) {
    if (!runtime.missionEnvironment) return false;
    const auto found = runtime.missionEnvironment->missionCharacters.find(handle);
    if (found == runtime.missionEnvironment->missionCharacters.end()) return false;
    const float before = found->second.health;
    // Native 0x416548 passes source=null, weapon30, body part6 and 1000
    // damage to InflictDamage. Keep the ordinary armour/proof pipeline.
    hurtMissionCharacter(runtime, handle, 1000.0f, false, 1, 30);
    const auto& ped = found->second;
    if (ped.health < before && ped.bleeding) {
        StorylandNativePoint point{ped.x, ped.y, ped.z + 1.6f};
        const auto combat = runtime.missionCombat.find(handle);
        if (combat != runtime.missionCombat.end() && combat->second.anchorsValid) point = combat->second.head;
        storylandEmitBloodImpact(runtime,point,100,nullptr,true);
    }
    return true;
}

int32_t storylandCountReturnableWeapons(const StorylandPlayTestRuntime& runtime) {
    std::array<bool,10> slots{};
    for (size_t index = 0; index < runtime.confiscatedWeapons.size() && index < runtime.weapons.size(); ++index) {
        const auto& saved = runtime.confiscatedWeapons[index];
        const auto& weapon = runtime.weapons[index];
        if (saved.owned && saved.ammunition > 0 && weapon.nativeId > 0 &&
            weapon.slot >= 0 && weapon.slot < int(slots.size())) slots[size_t(weapon.slot)] = true;
    }
    return int32_t(std::count(slots.begin(), slots.end(), true));
}

bool storylandReturnPlayerWeapons(StorylandPlayTestRuntime& runtime) {
    if (runtime.missionEnvironment) {
        std::string error;
        if (!storylandApplyMissionWeapons(runtime, *runtime.missionEnvironment, error)) return false;
    }
    std::array<size_t,10> savedSlots{};
    savedSlots.fill(SIZE_MAX);
    for (size_t index = 0; index < runtime.confiscatedWeapons.size() && index < runtime.weapons.size(); ++index) {
        const auto& saved = runtime.confiscatedWeapons[index];
        const auto& weapon = runtime.weapons[index];
        if (saved.owned && saved.ammunition > 0 && weapon.nativeId > 0 &&
            weapon.slot >= 0 && weapon.slot < int(savedSlots.size())) savedSlots[size_t(weapon.slot)] = index;
    }
    for (size_t index : savedSlots) {
        if (index == SIZE_MAX) continue;
        const int ammunition = runtime.confiscatedWeapons[index].ammunition;
        for (size_t current = 0; current < runtime.inventory.size() && current < runtime.weapons.size(); ++current)
            if (current != index && runtime.weapons[current].slot == runtime.weapons[index].slot)
                runtime.inventory[current] = {};
        if (!storylandGiveWeapon(runtime, index, ammunition, false)) return false;
    }
    runtime.confiscatedWeapons.clear(); runtime.weaponBuybackAvailable = false;
    if (runtime.equippedWeapon >= runtime.inventory.size() || !runtime.inventory[runtime.equippedWeapon].owned)
        runtime.equippedWeapon = 0;
    if (runtime.missionEnvironment) storylandCaptureMissionWeapons(runtime, *runtime.missionEnvironment);
    return true;
}

bool storylandGiveWeapon(StorylandPlayTestRuntime& runtime,size_t weapon,int ammunition,bool equip) {
    if(weapon>=runtime.weapons.size()||ammunition<0) return false;
    // Reconcile use before an unrelated pickup/script/debug grant can mask it
    // with the same total. An explicit zero-round grant still conveys ownership.
    storylandUpdateBetaWeaponGrants(runtime);
    storylandUpdateKillFrenzyGrant(runtime);
    if(runtime.killFrenzy.grant.enabled&&runtime.killFrenzy.grant.nativeWeapon==runtime.weapons[weapon].nativeId)
        runtime.killFrenzy.grant.externalGrant=true;
    for(auto& grant:runtime.betaWeapons)
        if(grant.enabled&&grant.nativeWeapon==runtime.weapons[weapon].nativeId)grant.externalGrant=true;
    runtime.inventory.resize(runtime.weapons.size());
    auto& inventory=runtime.inventory[weapon];
    inventory.owned=true;
    inventory.ammunition=int(std::min(int64_t(runtime.crime.vcs?9999:99999),int64_t(inventory.ammunition)+ammunition));
    if(!inventory.magazine) inventory.magazine=std::min(runtime.weapons[weapon].clipSize,inventory.ammunition);
    if(equip) runtime.equippedWeapon=weapon;
    return true;
}

bool storylandCyclePlayerWeapon(StorylandPlayTestRuntime& runtime) {
    if(runtime.weapons.empty()||!runtime.playerControl||runtime.paused||runtime.pauseMenu||
        runtime.health<=0||runtime.busted||runtime.playerRestart.active()||
        runtime.occupiedVehicle>=0||runtime.vehicleAction!=StorylandPlayTestRuntime::VehicleAction::None||
        runtime.playerReaction.active()||runtime.playerMelee.active||runtime.playerWater.active||
        runtime.killFrenzy.active())return false;
    auto* world=runtime.missionEnvironment;
    if(world&&(!world->control||world->cutscene.playing||world->playerCutsceneSafe||
        world->playerAttachedVehicle>=0||world->remoteVehicle>=0||world->remoteControl.vehicle>=0||
        world->clearPlayerWeaponsRequested||world->weaponInventoryChanged||world->weaponSelectionRequested||
        world->manualAim.forced))return false;
    const auto selectable=[&](size_t index) {
        const auto& weapon=runtime.weapons[index];
        if(weapon.nativeId==0||(index==0&&weapon.nativeId<0))return true;
        if(index>=runtime.inventory.size()||!runtime.inventory[index].owned)return false;
        return weapon.fireType==0||weapon.nativeId==(runtime.crime.vcs?36:34)||
            runtime.inventory[index].ammunition>0;
    };
    const auto before=[&](size_t first,size_t second) {
        const auto& a=runtime.weapons[first];
        const auto& b=runtime.weapons[second];
        if(a.slot!=b.slot)return a.slot<b.slot;
        if(a.nativeId!=b.nativeId)return a.nativeId<b.nativeId;
        return first<second;
    };
    size_t first=SIZE_MAX,next=SIZE_MAX;
    for(size_t index=0;index<runtime.weapons.size();++index) {
        if(!selectable(index))continue;
        if(first==SIZE_MAX||before(index,first))first=index;
        if(runtime.equippedWeapon<runtime.weapons.size()&&before(runtime.equippedWeapon,index)&&
            (next==SIZE_MAX||before(index,next)))next=index;
    }
    if(next==SIZE_MAX)next=first;
    if(next==SIZE_MAX||next==runtime.equippedWeapon)return false;
    if(runtime.equippedWeapon<runtime.inventory.size())runtime.inventory[runtime.equippedWeapon].reloading=0;
    runtime.equippedWeapon=next;
    runtime.playerCrouchPose={};
    storylandClearPlayTestTarget(runtime);
    runtime.freeAimActive=false;
    runtime.manualAimActive=false;
    runtime.manualAimMode=0;
    runtime.equalizerScope=false;
    runtime.equalizerScopeHeld=false;
    runtime.combatControlWeapon=-1;
    runtime.playerMuzzleValid=false;
    if(world) {
        world->currentWeapon=std::max(0,runtime.weapons[next].nativeId);
        world->manualAim={};
        world->manualAim.resetRequested=true;
    }
    return true;
}

size_t storylandBetaWeaponIndex(const StorylandPlayTestRuntime& runtime,StorylandBetaWeapon feature,bool vcs) {
    if(!vcs||feature>=StorylandBetaWeapon::Count)return SIZE_MAX;
    const int32_t nativeId=storylandBetaWeaponId(feature);
    const char* expected=feature==StorylandBetaWeapon::TearGas?"teargas":
        feature==StorylandBetaWeapon::LandMine?"landmine":"seamine";
    for(size_t index=0;index<runtime.weapons.size();++index) {
        const auto& weapon=runtime.weapons[index];
        if(weapon.nativeId!=nativeId||weapon.fireType!=2||weapon.modelId<0)continue;
        std::string name;
        for(unsigned char character:weapon.name)
            if(std::isalnum(character))name.push_back(char(std::tolower(character)));
        if(name==expected)return index;
    }
    return SIZE_MAX;
}

static int storylandBetaAmmunition(const StorylandPlayTestRuntime& runtime,size_t index) {
    int64_t total=0;
    if(index<runtime.inventory.size())total+=std::max(0,runtime.inventory[index].ammunition);
    if(index<runtime.confiscatedWeapons.size())total+=std::max(0,runtime.confiscatedWeapons[index].ammunition);
    return int(std::min<int64_t>(INT32_MAX,total));
}

static void storylandUpdateKillFrenzyGrant(StorylandPlayTestRuntime& runtime) {
    auto& grant=runtime.killFrenzy.grant;
    if(!grant.enabled)return;
    const auto weapon=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
        [&](const auto& item){return item.nativeId==grant.nativeWeapon;});
    const int ammunition=weapon==runtime.weapons.end()?0:
        storylandBetaAmmunition(runtime,size_t(weapon-runtime.weapons.begin()));
    if(ammunition<grant.observedAmmunition)
        grant.remainingGranted=std::max(0,grant.remainingGranted-(grant.observedAmmunition-ammunition));
    else if(ammunition>grant.observedAmmunition)grant.externalGrant=true;
    grant.remainingGranted=std::min(grant.remainingGranted,ammunition);
    grant.observedAmmunition=ammunition;
}

int32_t storylandKillFrenzyRemainingMilliseconds(const StorylandPlayTestRuntime& runtime) {
    const auto& frenzy=runtime.killFrenzy;
    if(!frenzy.active()||!runtime.missionEnvironment)return 0;
    if(frenzy.timeLimitMilliseconds<0)return -1;
    const uint32_t elapsed=runtime.missionEnvironment->gameMilliseconds-frenzy.startedAt;
    return elapsed>=uint32_t(frenzy.timeLimitMilliseconds)?0:
        frenzy.timeLimitMilliseconds-int32_t(elapsed);
}

static void storylandFinishKillFrenzy(StorylandPlayTestRuntime& runtime,uint16_t status) {
    auto& frenzy=runtime.killFrenzy;
    if(!runtime.crime.vcs&&runtime.missionEnvironment) {
        auto& world=*runtime.missionEnvironment;
        auto& saved=world.lcsExtendedWorld.rampageWeapon;
        if(saved.active&&saved.physicalWeapon>=0&&saved.physicalWeapon<37) {
            const auto physical=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
                [&](const auto& weapon){return weapon.nativeId==saved.physicalWeapon;});
            if(physical!=runtime.weapons.end()) {
                for(size_t index=0;index<runtime.weapons.size()&&index<runtime.inventory.size();++index)
                    if(runtime.weapons[index].slot==physical->slot)runtime.inventory[index]={};
            }
            const auto interrupted=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
                [&](const auto& weapon){return weapon.nativeId==saved.interruptedWeapon;});
            if(interrupted!=runtime.weapons.end()) {
                const size_t index=size_t(interrupted-runtime.weapons.begin());
                runtime.inventory.resize(runtime.weapons.size());
                runtime.inventory[index]={};
                runtime.inventory[index].owned=true;
                runtime.inventory[index].ammunition=saved.ammunition;
                runtime.inventory[index].magazine=std::min(interrupted->clipSize,saved.ammunition);
            }
            runtime.equippedWeapon=0;
            for(size_t index=0;index<runtime.weapons.size()&&index<runtime.inventory.size();++index)
                if(runtime.weapons[index].nativeId==saved.selectedWeapon&&runtime.inventory[index].owned)
                    runtime.equippedWeapon=index;
            if(runtime.occupiedVehicle>=0) {
                runtime.equippedWeapon=0;
                for(size_t index=0;index<runtime.weapons.size()&&index<runtime.inventory.size();++index)
                    if(runtime.weapons[index].slot==5&&runtime.inventory[index].owned)
                        runtime.equippedWeapon=index;
            }
        }
        saved={};frenzy.grant={};frenzy.status=status;world.killFrenzyStatus=status;
        if(status==2) {
            if(frenzy.text=="PAGE_00")runtime.statistics["NumberKillFrenziesPassed"]+=1;
            runtime.wanted=0;world.wanted=0;
            runtime.crime.setWantedLevel(0);
        }
        storylandCaptureMissionWeapons(runtime,world);
        return;
    }
    storylandUpdateKillFrenzyGrant(runtime);
    auto& grant=frenzy.grant;
    if(grant.enabled) {
        const auto weapon=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
            [&](const auto& item){return item.nativeId==grant.nativeWeapon;});
        if(weapon!=runtime.weapons.end()) {
            const size_t index=size_t(weapon-runtime.weapons.begin());
            int remaining=grant.remainingGranted;
            const auto removeGrant=[&](std::vector<StorylandWeaponInventory>& inventory) {
                if(index>=inventory.size())return;
                auto& item=inventory[index];
                const int removed=std::min(remaining,std::max(0,item.ammunition));
                item.ammunition-=removed;remaining-=removed;
                item.magazine=std::min(item.magazine,std::max(0,item.ammunition));
                if(item.ammunition==0&&!grant.originallyOwned&&!grant.externalGrant)item={};
            };
            removeGrant(runtime.inventory);removeGrant(runtime.confiscatedWeapons);
            if(runtime.equippedWeapon==index) {
                for(size_t previous=0;previous<runtime.weapons.size()&&previous<runtime.inventory.size();++previous)
                    if(runtime.weapons[previous].nativeId==grant.previousWeapon&&runtime.inventory[previous].owned) {
                        runtime.equippedWeapon=previous;break;
                    }
                if(runtime.equippedWeapon>=runtime.inventory.size()||!runtime.inventory[runtime.equippedWeapon].owned)
                    runtime.equippedWeapon=0;
            }
        }
        grant={};
    }
    frenzy.status=status;
    if(runtime.missionEnvironment) {
        runtime.missionEnvironment->killFrenzyStatus=status;
        storylandCaptureMissionWeapons(runtime,*runtime.missionEnvironment);
    }
}

void storylandCancelKillFrenzy(StorylandPlayTestRuntime& runtime) {
    if(runtime.killFrenzy.active()||runtime.killFrenzy.grant.enabled)
        storylandFinishKillFrenzy(runtime,3);
}

bool storylandSetHeadshotRampageEnabled(StorylandPlayTestRuntime& runtime,bool enabled,std::string& error) {
    error.clear();
    if(enabled&&!runtime.crime.vcs) {
        error="The retained headshot-only rampage is a VCS feature.";return false;
    }
    if(!enabled&&runtime.killFrenzy.custom)storylandCancelKillFrenzy(runtime);
    runtime.killFrenzy.enabled=enabled;
    return true;
}

bool storylandStartKillFrenzy(StorylandPlayTestRuntime& runtime,const std::string& text,
    int32_t nativeWeapon,int32_t timeLimitMilliseconds,int32_t targetKills,
    const std::array<int32_t,4>& models,bool completedText,bool onlyHeadshots,
    bool custom,std::string& error) {
    error.clear();
    if(!runtime.missionEnvironment) {
        error="The rampage requires a live game world.";return false;
    }
    if(!runtime.crime.vcs) {
        if(custom) {error="The optional headshot challenge is a VCS feature.";return false;}
        if(nativeWeapon<0||nativeWeapon>47||text.size()>128) {
            error="The LCS rampage weapon or message is invalid.";return false;
        }
        if(!storylandApplyMissionWeapons(runtime,*runtime.missionEnvironment,error,false))return false;
        storylandCancelKillFrenzy(runtime);
        auto& world=*runtime.missionEnvironment;
        const int32_t physicalWeapon=storylandLcsRampagePhysicalWeapon(nativeWeapon);
        const auto physical=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
            [&](const auto& weapon){return weapon.nativeId==physicalWeapon;});
        if(physicalWeapon<37&&physical==runtime.weapons.end()) {
            error="The native LCS rampage weapon is not loaded.";return false;
        }
        runtime.killFrenzy={};
        auto& frenzy=runtime.killFrenzy;
        frenzy.nativeWeapon=nativeWeapon;frenzy.timeLimitMilliseconds=timeLimitMilliseconds;
        frenzy.targetKills=uint16_t(targetKills);frenzy.text=text;frenzy.models=models;
        frenzy.completedText=completedText;frenzy.onlyHeadshots=onlyHeadshots;
        frenzy.startedAt=world.gameMilliseconds;frenzy.status=1;
        auto& saved=world.lcsExtendedWorld.rampageWeapon;
        saved={};saved.active=true;saved.physicalWeapon=physicalWeapon;
        if(physicalWeapon<37) {
            const size_t index=size_t(physical-runtime.weapons.begin());
            saved.selectedWeapon=runtime.equippedWeapon<runtime.weapons.size()?
                runtime.weapons[runtime.equippedWeapon].nativeId:0;
            runtime.inventory.resize(runtime.weapons.size());
            for(size_t current=0;current<runtime.weapons.size();++current) {
                if(runtime.weapons[current].slot!=physical->slot||!runtime.inventory[current].owned)continue;
                saved.interruptedWeapon=runtime.weapons[current].nativeId;
                saved.ammunition=runtime.inventory[current].ammunition;
                if(current!=index)runtime.inventory[current]={};
            }
            auto& grant=frenzy.grant;
            grant.enabled=true;grant.nativeWeapon=physicalWeapon;
            grant.originallyOwned=runtime.inventory[index].owned;
            grant.previousWeapon=saved.selectedWeapon;
            const int before=storylandBetaAmmunition(runtime,index);
            if(!storylandGiveWeapon(runtime,index,30000)) {
                saved={};frenzy={};error="The native LCS rampage weapon could not be equipped.";return false;
            }
            grant.externalGrant=false;
            grant.observedAmmunition=storylandBetaAmmunition(runtime,index);
            grant.remainingGranted=grant.observedAmmunition-before;
        }
        world.killFrenzyStatus=1;
        storylandCaptureMissionWeapons(runtime,world);
        return true;
    }
    if(custom&&(!runtime.killFrenzy.enabled||!onlyHeadshots)) {
        error="Enable the custom headshot-only challenge first.";return false;
    }
    if(custom&&runtime.killFrenzy.active()) {
        error="Finish or cancel the current rampage before starting another challenge.";return false;
    }
    if(custom&&(runtime.missionEnvironment->cutscene.playing||
        !runtime.missionEnvironment->control||!runtime.playerControl)) {
        error="Start the custom challenge after the cutscene or scripted player control ends.";return false;
    }
    if((timeLimitMilliseconds<=0&&timeLimitMilliseconds!=-1)||timeLimitMilliseconds>3600000||
        targetKills<=0||targetKills>100000||text.size()>128) {
        error="Rampage duration, target count or text exceeds the supported range.";return false;
    }
    const auto weapon=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
        [&](const auto& item){return item.nativeId==nativeWeapon;});
    if(weapon==runtime.weapons.end()||(onlyHeadshots&&(weapon->fireType!=1||weapon->damage<=0))) {
        error="The rampage requires a loaded weapon; headshots require a firearm.";return false;
    }
    if(runtime.health<=0||runtime.busted||runtime.wastedActive||
        runtime.missionEnvironment->health<=0||runtime.missionEnvironment->playerArrested) {
        error="The player cannot start a rampage while incapacitated.";return false;
    }
    if(!storylandApplyMissionWeapons(runtime,*runtime.missionEnvironment,error))return false;
    storylandCancelKillFrenzy(runtime);
    const size_t index=size_t(weapon-runtime.weapons.begin());
    const bool enabled=runtime.killFrenzy.enabled;
    runtime.killFrenzy={};
    auto& frenzy=runtime.killFrenzy;
    frenzy.enabled=enabled;frenzy.custom=custom;frenzy.onlyHeadshots=onlyHeadshots;
    frenzy.completedText=completedText;frenzy.text=text;frenzy.models=models;
    frenzy.nativeWeapon=nativeWeapon;frenzy.timeLimitMilliseconds=timeLimitMilliseconds;
    frenzy.targetKills=targetKills;frenzy.startedAt=runtime.missionEnvironment->gameMilliseconds;
    auto& grant=frenzy.grant;
    grant.enabled=true;grant.nativeWeapon=nativeWeapon;
    grant.originallyOwned=(index<runtime.inventory.size()&&runtime.inventory[index].owned)||
        (index<runtime.confiscatedWeapons.size()&&runtime.confiscatedWeapons[index].owned);
    grant.previousWeapon=runtime.equippedWeapon<runtime.weapons.size()?
        runtime.weapons[runtime.equippedWeapon].nativeId:-1;
    grant.observedAmmunition=storylandBetaAmmunition(runtime,index);
    const int before=grant.observedAmmunition;
    const int ammunition=custom?int(std::clamp<int64_t>(int64_t(targetKills)*8,100,9999)):9999;
    if(!storylandGiveWeapon(runtime,index,ammunition)) {
        frenzy.grant={};error="The rampage weapon could not be equipped.";return false;
    }
    grant.externalGrant=false;
    grant.observedAmmunition=storylandBetaAmmunition(runtime,index);
    grant.remainingGranted=std::max(0,grant.observedAmmunition-before);
    frenzy.status=1;runtime.missionEnvironment->killFrenzyStatus=1;
    storylandCaptureMissionWeapons(runtime,*runtime.missionEnvironment);
    return true;
}

void storylandUpdateKillFrenzy(StorylandPlayTestRuntime& runtime) {
    auto& frenzy=runtime.killFrenzy;
    if(frenzy.active()) {
        const bool incapacitated=!runtime.missionEnvironment||runtime.health<=0||runtime.busted||runtime.wastedActive||
            runtime.missionEnvironment->health<=0||runtime.missionEnvironment->playerArrested;
        if(incapacitated||storylandKillFrenzyRemainingMilliseconds(runtime)==0)frenzy.status=3;
        if(!runtime.crime.vcs&&!incapacitated&&frenzy.kills>=frenzy.targetKills)frenzy.status=2;
    }
    const bool nativeLcs=runtime.missionEnvironment&&!runtime.crime.vcs&&
        runtime.missionEnvironment->lcsExtendedWorld.rampageWeapon.active;
    if(!frenzy.active()&&(frenzy.grant.enabled||nativeLcs))storylandFinishKillFrenzy(runtime,frenzy.status);
    else storylandUpdateKillFrenzyGrant(runtime);
    if(runtime.missionEnvironment&&frenzy.status)runtime.missionEnvironment->killFrenzyStatus=frenzy.status;
}

static void storylandRegisterKillFrenzyDeath(StorylandPlayTestRuntime& runtime,
    int32_t model,int32_t nativeWeapon,bool headshot,bool vehicle) {
    auto& frenzy=runtime.killFrenzy;
    if(!frenzy.active())return;
    if(!runtime.missionEnvironment||runtime.health<=0||runtime.busted||runtime.wastedActive||
        runtime.missionEnvironment->health<=0||runtime.missionEnvironment->playerArrested||
        storylandKillFrenzyRemainingMilliseconds(runtime)==0)frenzy.status=3;
    else if(runtime.crime.vcs?storylandKillFrenzyMatches(frenzy,model,nativeWeapon,headshot):
        storylandLcsKillFrenzyMatches(frenzy,model,nativeWeapon,headshot,vehicle)) {
        ++frenzy.kills;
        if(frenzy.kills>=frenzy.targetKills)frenzy.status=2;
    }
    if(runtime.missionEnvironment)runtime.missionEnvironment->killFrenzyStatus=frenzy.status;
    // Inventory cleanup occurs after the shot consumes its round, not inside damage callbacks.
}

void storylandUpdateBetaWeaponGrants(StorylandPlayTestRuntime& runtime) {
    for(auto& grant:runtime.betaWeapons) {
        if(!grant.enabled)continue;
        const auto weapon=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
            [&](const auto& definition){return definition.nativeId==grant.nativeWeapon;});
        const size_t index=size_t(weapon-runtime.weapons.begin());
        const int ammunition=weapon==runtime.weapons.end()?0:storylandBetaAmmunition(runtime,index);
        if(ammunition<grant.observedAmmunition)
            grant.remainingGranted=std::max(0,grant.remainingGranted-(grant.observedAmmunition-ammunition));
        else if(ammunition>grant.observedAmmunition)grant.externalGrant=true;
        grant.remainingGranted=std::min(grant.remainingGranted,ammunition);
        grant.observedAmmunition=ammunition;
    }
}

bool storylandGiveBetaWeapon(StorylandPlayTestRuntime& runtime,StorylandBetaWeapon feature,
    int ammunition,bool vcs,std::string& error) {
    error.clear();
    const size_t index=storylandBetaWeaponIndex(runtime,feature,vcs);
    if(index==SIZE_MAX||!runtime.betaWeapons[size_t(feature)].enabled) {
        error="Enable an available unused weapon first.";return false;
    }
    if(ammunition<0||ammunition>9999) {
        error="Ammunition must be between 0 and 9999.";return false;
    }
    if(runtime.missionEnvironment&&
        !storylandApplyMissionWeapons(runtime,*runtime.missionEnvironment,error))return false;
    storylandUpdateBetaWeaponGrants(runtime);
    auto& grant=runtime.betaWeapons[size_t(feature)];
    const int before=storylandBetaAmmunition(runtime,index);
    const bool externalGrant=grant.externalGrant;
    if(!storylandGiveWeapon(runtime,index,ammunition)) {
        error="The unused weapon could not be equipped.";return false;
    }
    grant.externalGrant=externalGrant;
    const int after=storylandBetaAmmunition(runtime,index);
    grant.remainingGranted+=after-before;
    grant.observedAmmunition=after;
    if(runtime.missionEnvironment)storylandCaptureMissionWeapons(runtime,*runtime.missionEnvironment);
    return true;
}

bool storylandSetBetaWeaponEnabled(StorylandPlayTestRuntime& runtime,StorylandBetaWeapon feature,
    bool enabled,bool vcs,std::string& error) {
    error.clear();
    if(feature>=StorylandBetaWeapon::Count) {
        error="Unknown unused weapon.";return false;
    }
    auto& grant=runtime.betaWeapons[size_t(feature)];
    if(enabled==grant.enabled)return true;
    const size_t index=storylandBetaWeaponIndex(runtime,feature,vcs);
    if(enabled&&index==SIZE_MAX) {
        error="This weapon's VCS definition and model are not loaded.";return false;
    }
    if(runtime.missionEnvironment&&
        !storylandApplyMissionWeapons(runtime,*runtime.missionEnvironment,error))return false;
    storylandUpdateBetaWeaponGrants(runtime);
    if(enabled) {
        grant.enabled=true;
        grant.nativeWeapon=runtime.weapons[index].nativeId;
        grant.originallyOwned=(index<runtime.inventory.size()&&runtime.inventory[index].owned)||
            (index<runtime.confiscatedWeapons.size()&&runtime.confiscatedWeapons[index].owned);
        grant.observedAmmunition=storylandBetaAmmunition(runtime,index);
        grant.previousWeapon=runtime.equippedWeapon<runtime.weapons.size()?
            runtime.weapons[runtime.equippedWeapon].nativeId:-1;
        if(storylandGiveBetaWeapon(runtime,feature,5,vcs,error))return true;
        grant={};return false;
    }
    const auto weapon=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
        [&](const auto& definition){return definition.nativeId==grant.nativeWeapon;});
    if(weapon!=runtime.weapons.end()) {
        const size_t current=size_t(weapon-runtime.weapons.begin());
        int remaining=grant.remainingGranted;
        const auto removeGrant=[&](std::vector<StorylandWeaponInventory>& inventory) {
            if(current>=inventory.size())return;
            auto& entry=inventory[current];
            const int removed=std::min(remaining,std::max(0,entry.ammunition));
            entry.ammunition-=removed;remaining-=removed;
            entry.magazine=std::min(entry.magazine,std::max(0,entry.ammunition));
            if(entry.ammunition==0&&!grant.originallyOwned&&!grant.externalGrant)entry={};
        };
        removeGrant(runtime.inventory);
        removeGrant(runtime.confiscatedWeapons);
        if(runtime.equippedWeapon==current&&
            (current>=runtime.inventory.size()||!runtime.inventory[current].owned)) {
            runtime.equippedWeapon=0;
            for(size_t previous=0;previous<runtime.weapons.size()&&previous<runtime.inventory.size();++previous)
                if(runtime.weapons[previous].nativeId==grant.previousWeapon&&runtime.inventory[previous].owned) {
                    runtime.equippedWeapon=previous;break;
                }
        }
    }
    const uint8_t tag=uint8_t(size_t(feature)+1);
    const auto removeTransient=[tag](auto& values) {
        values.erase(std::remove_if(values.begin(),values.end(),
            [tag](const auto& value){return value.betaFeature==tag;}),values.end());
    };
    removeTransient(runtime.projectiles);removeTransient(runtime.effects);removeTransient(runtime.particles);
    grant={};
    if(runtime.missionEnvironment)storylandCaptureMissionWeapons(runtime,*runtime.missionEnvironment);
    return true;
}

static float storylandPlayerNativeReloadDuration(const StorylandPlayTestRuntime& runtime,
    const StorylandWeaponDefinition& weapon) {
    const float timer=std::max(0.05f,weapon.reloadSeconds);
    if(weapon.animationGroup<0||!runtime.missionEnvironment||
        !runtime.missionEnvironment->weaponAnimationDuration)return timer;
    const int32_t reloadAssociation=runtime.crime.vcs?216:203;
    const float clip=runtime.missionEnvironment->weaponAnimationDuration(
        weapon.animationGroup,reloadAssociation);
    return std::isfinite(clip)&&clip>0.05f?std::max(clip,timer):timer;
}

void storylandReloadWeapon(StorylandPlayTestRuntime& runtime) {
    if(runtime.equippedWeapon>=runtime.weapons.size()||runtime.equippedWeapon>=runtime.inventory.size()) return;
    const auto& weapon=runtime.weapons[runtime.equippedWeapon];
    auto& inventory=runtime.inventory[runtime.equippedWeapon];
    if(weapon.fireType&&inventory.reloading<=0&&inventory.magazine<weapon.clipSize&&inventory.ammunition>inventory.magazine) {
        const bool fastReload = runtime.missionEnvironment && runtime.missionEnvironment->playerFastReload;
        if (!fastReload && !storylandBeginCrouchReload(runtime,0,runtime.playerCrouchPose,weapon)) {
            return;
        }
        const float duration=fastReload?0.0f:storylandPlayerNativeReloadDuration(runtime,weapon);
        inventory.reloading=duration;
        runtime.playerReloadWeapon=fastReload?SIZE_MAX:runtime.equippedWeapon;
        runtime.playerReloadClipSeconds=duration;
        runtime.playerReloadElapsed=0.0f;
        if(fastReload)inventory.magazine=std::min(weapon.clipSize,inventory.ammunition);
        runtime.soundEvents.emplace_back(StorylandPlayTestSound::WeaponReload);
        runtime.soundEvents.back().weapon=int(runtime.equippedWeapon);
    }
}
void storylandSetPlayTestWantedLevel(StorylandPlayTestRuntime& runtime,int wanted) {
    runtime.wanted=std::clamp(wanted,0,6);
    for(const auto& event:runtime.crime.events)runtime.crime.registerLcsEvent(event);
    runtime.crime.events.clear();
    runtime.crime.setWantedLevel(runtime.wanted);
}
static void storylandUpdateMissionCombat(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& actors,const StorylandNativePoint& player,float dt,
    const StorylandVcsMountedGunRequest* mountedRequest=nullptr) {
    if(!runtime.missionEnvironment||runtime.coopGuest)return;
    auto& environment=*runtime.missionEnvironment;
    const bool vcs = world.paths.vcs;
    const bool lcsPolicePursuing = !vcs && std::any_of(runtime.pedestrians.begin(),
        runtime.pedestrians.end(), [](const auto& officer) {
            return officer.policePursuing && officer.isPolice(false);
        });
    std::vector<StorylandVcsMountedGunRequest> mountedRequests;
    std::set<int32_t> mountedCommands;
    if (!mountedRequest) {
        mountedRequests.swap(environment.mountedGunRequests);
        for (const auto& request : mountedRequests) mountedCommands.insert(request.character);
    }
    for(auto& entry:environment.missionCharacters) {
        const int32_t handle=entry.first;auto& character=entry.second;
        if (mountedRequest && mountedRequest->character != handle) continue;
        character.threatVisible = false;
        auto& combat=runtime.missionCombat[handle];
        // A script can take over the stance while the autonomous task is
        // active. Its changed request or deadline relinquishes our ownership.
        if(combat.extendedAutoCrouch&&(!character.crouch.requested||
            character.crouch.deadline!=combat.extendedAutoCrouchDeadline))
            combat.extendedAutoCrouch=false;
        struct AutoCrouchGuard {
            StorylandMissionEnvironment::Character& character;
            StorylandMissionCombatState& combat;
            uint32_t now;
            bool keep=false;
            ~AutoCrouchGuard() {
                if(!combat.extendedAutoCrouch||keep)return;
                character.crouch.requested=false;
                storylandClearCrouch(character.crouch,now);
                combat.crouchPose={};
                combat.extendedAutoCrouch=false;
                combat.extendedAutoCrouchDeadline=0;
            }
        } autoCrouchGuard{character,combat,environment.gameMilliseconds};
        if (storylandPlayerReactionHoldsTarget(runtime.playerReaction,
            StorylandReactionTargetKind::Mission, handle, uint32_t(character.model))) {
            combat.melee = {};
            character.movementSpeed = 0;
            continue;
        }
        combat.coughTime=std::max(0.0f,combat.coughTime-dt);
        if (character.alive && character.health > 0 && character.vehicle < 0 && !character.attached) {
            float surface = 0;
            const bool hasWater = storylandNativeWaterHeight(world, character.x, character.y, surface) &&
                (!vcs || storylandVcsWaterDepthAllowsContact(character.z, surface));
            uint8_t material = 0;
            const StorylandNativePoint top{character.x, character.y, surface + 0.1f};
            const StorylandNativePoint bottom{character.x, character.y, surface - 3.0f};
            const float ground = hasWater ? storylandTraceNativeGround(world, top, bottom, material) : 1.0f;
            const bool deep = hasWater && storylandWaterIsDeep(character.z, surface, ground < 1,
                top.z + (bottom.z - top.z) * ground);
            const auto water = storylandStepWaterCharacter(combat.water, character.z, surface, deep,
                false, false, true, dt,runtime.crime.vcs,character.movementSpeed,character.type,
                character.drownsInWater);
            if (combat.water.active) {
                character.z = water.height;
                character.walkingToTarget = false;
                if (character.drownsInWater) {
                    hurtMissionCharacter(runtime,handle,water.damage,false,0,
                        storylandNativeDrowningWeapon(runtime.crime.vcs));
                }
                if(water.entered) {
                    StorylandPlayTestSoundEvent splash(StorylandPlayTestSound::WaterSplash);
                    splash.position={character.x,character.y,surface};splash.positioned=true;
                    runtime.soundEvents.push_back(splash);
                }
                storylandUpdateSwimmingParticles(runtime,combat.water,
                    {character.x,character.y,character.z},surface,character.heading+90.0f,
                    character.movementSpeed,combat.water.verticalSpeed,dt,water.entered,
                    character.alive&&character.health>0);
            }
        }
        if(character.behavior.leavingVehicleToDie) {
            combat.crouchPose={};combat.melee={};environment.characterTargets.erase(handle);continue;
        }
        if(character.health<=0||!character.alive) {
            if(!character.deathRecorded) {
                character.deathRecorded=true;character.deathStartedAt=environment.gameMilliseconds;
                character.animationStartedAt=character.deathStartedAt;
                character.animationId=runtime.crime.vcs?16:13;character.animationGroup=0;
                character.animationBlend=4;character.animationLoop=false;
            }
            character.alive=false;character.walkingToTarget=false;
            combat.crouchPose={};combat.melee={};environment.characterTargets.erase(handle);
            storylandDropMissionCharacterPossessions(runtime,handle);
            continue;
        }
        if(vcs&&environment.vcsPedFalls.count(handle)) {
            combat.crouchPose={};combat.melee={};character.movementSpeed=0;
            continue;
        }
        if(storylandMissionBulletReactionActive(runtime,handle)) {
            combat.melee={};combat.contact.clear();combat.crouchPose={};
            character.movementSpeed=0;
            combat.cooldown=std::max(0.0f,combat.cooldown-dt);
            continue;
        }
        if(!vcs) {
            const auto current=environment.characterCurrentWeapons.find(handle);
            if(current!=environment.characterCurrentWeapons.end())character.currentWeapon=current->second;
        }
        const auto selected=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
            [&](const auto& weapon){return weapon.nativeId==character.currentWeapon;});
        if(selected==runtime.weapons.end())continue;
        const auto& weapon=*selected;
        combat.weapon=size_t(selected-runtime.weapons.begin());combat.nativeWeapon=character.currentWeapon;
        const bool weaponCooling=combat.cooldown>0;
        combat.cooldown=weapon.fireType&&weaponCooling?combat.cooldown-dt:
            std::max(0.0f,combat.cooldown-dt);
        if(combat.melee.active) {
            combat.melee.elapsed+=dt;
            if(combat.melee.elapsed>=combat.melee.duration&&combat.melee.contactApplied)
                combat.melee.active=false;
        }
        int32_t* ammunition=nullptr;
        bool weaponOwned=false;
        const auto useInventory=[&](auto& inventory) {
            const auto slot=inventory.find(character.currentWeapon);
            if(slot!=inventory.end()) {
                ammunition=&slot->second.ammunition;
                weaponOwned=slot->second.owned;
            }
        };
        if(vcs)useInventory(character.weapons);
        else {
            const auto inventory=environment.characterWeapons.find(handle);
            if(inventory!=environment.characterWeapons.end())useInventory(inventory->second);
        }
        if(!vcs&&character.weaponSelectionChanged) {
            combat.magazine=ammunition?std::min(weapon.clipSize,*ammunition):0;
            combat.cooldown=0;combat.reloading=0;combat.melee={};
            character.weaponSelectionChanged=false;
        }
        if(combat.reloading>0) {
            combat.reloading=std::max(0.0f,combat.reloading-dt);
            if(combat.reloading<=0&&ammunition)
                combat.magazine=std::min(weapon.clipSize,*ammunition);
        }
        if (!mountedRequest && mountedCommands.count(handle)) continue;
        int32_t target=-1;
        bool acquiredThreat = false;
        bool objectTarget = !mountedRequest && character.objective == (vcs ? 0x00EB : 0x01DD);
        bool vehicleTarget = mountedRequest ? mountedRequest->vehicle :
            character.objective == (vcs ? 0x00EC : 0x01DE);
        const bool characterAttack=!mountedRequest&&(vcs?
            (character.objective==0x00E4||character.objective==0x00E5):
            (character.objective>=0x01CE&&character.objective<=0x01D1));
        const bool explicitAttack=characterAttack||objectTarget||vehicleTarget;
        const bool aimOnly=!mountedRequest&&character.objective==(vcs?0x00F4:0x04CB);
        if (mountedRequest) target=mountedRequest->target;
        else if(explicitAttack||aimOnly) {
            target=character.objectiveTarget;
            if(!vcs&&!objectTarget&&!vehicleTarget&&(target==1||
                character.objective==0x01CF||character.objective==0x01D1))target=0;
        }
        else if(character.heedThreats&&character.searchThreats&&
            (character.objective==0||(vcs?(character.objective==0x00E1||character.objective==0x00E2||
                character.objective==0x00EF):(character.objective==0x0199||character.objective==0x019A||
                character.objective==0x0310)))) {
            const uint32_t mask = character.fearFlags;
            if(!character.playerFriend&&!environment.everyoneIgnoresPlayer&&(runtime.health>0||runtime.coopRemoteAlive)&&
                !storylandGangPlayerProtected(runtime,character.type)&&
                (character.provokedByPlayer||(mask&1u)))target=0;
            const float view = character.pedStat >= 0 ? float(character.behavior.statView) / 50.0f : 1.0f;
            float nearest=std::max(1.0f,30.0f*environment.threatReactionRangeMultiplier*view);
            if(target==0)nearest=separation({character.x,character.y,character.z},player);
            for(const auto& candidate:environment.missionCharacters) {
                if(candidate.first==handle||!candidate.second.alive||candidate.second.type<0||candidate.second.type>=32||
                    !(mask&(1u<<unsigned(candidate.second.type))))continue;
                const float distance=separation({character.x,character.y,character.z},
                    {candidate.second.x,candidate.second.y,candidate.second.z});
                if(distance<nearest) {nearest=distance;target=candidate.first;}
            }
            acquiredThreat = target >= 0;
        }
        if (!mountedRequest && character.playerFriend && character.canFightInGroup && !explicitAttack && !aimOnly) {
            const auto selectedTarget = environment.playerTarget;
            if (selectedTarget.kind == StorylandMissionEnvironment::TargetKind::Character &&
                selectedTarget.handle > (vcs ? 0 : 1) && selectedTarget.handle != handle) {
                target = selectedTarget.handle;
                acquiredThreat = false;
            }
        }
        if (!mountedRequest && !explicitAttack && !aimOnly && character.behavior.forcedThreat >= 0) {
            target = character.behavior.forcedThreat;
            acquiredThreat = false;
            objectTarget = false; vehicleTarget = character.behavior.forcedThreatIsVehicle;
        }
        const bool seatedDriveBy = runtime.crime.vcs && character.vehicle >= 0 &&
            character.behavior.canDoDriveBy && character.currentWeapon >= 24 && character.currentWeapon <= 27;
        if (!mountedRequest && target == 0 && !objectTarget && !vehicleTarget && runtime.crime.vcs &&
            !storylandMissionPedCanAttackPlayer(environment, handle, weapon.fireType == 0)) target = -1;
        if (!mountedRequest && target == 0 && !objectTarget && !vehicleTarget && !vcs &&
            storylandLcsYieldPlayerAttackToPolice(environment, handle, lcsPolicePursuing)) target = -1;
        if(!mountedRequest&&(character.waitState||(vcs&&environment.vcsCharacterExitingVehicle(handle))||
            (character.vehicle>=0&&!character.attached&&!seatedDriveBy)||
            (character.attached&&!vcs)||character.stopShooting||
            character.behavior.conversationPartner>=0||character.behavior.diveDeadline||
            character.behavior.iceCreamVehicle>=0||combat.water.active||combat.coughTime>0||
            ((character.playerFriend||character.leader>=0)&&!character.canFightInGroup)))target=-1;
        bool remoteTarget=false;
        if(!mountedRequest&&target==0&&!objectTarget&&!vehicleTarget&&runtime.coopChooseTarget) {
            const bool police=(character.type==6||character.pedStats.name=="STAT_COP")&&!explicitAttack&&!character.provokedByPlayer;
            const int selectedPlayer=runtime.coopChooseTarget({character.x,character.y,character.z},police);
            if(selectedPlayer<0)target=-1;
            else remoteTarget=selectedPlayer==1;
        }
        StorylandNativePoint destination;
        if (objectTarget && target >= 0) {
            const auto object = std::find_if(environment.objects.begin(), environment.objects.end(),
                [target](const auto& item) { return item.handle == target; });
            if (object == environment.objects.end() || object->damage.renderDamaged || !object->damage.visible) {
                target = -1;
            } else {
                destination = {object->x, object->y, object->z};
                const auto model = world.collisionModels.find(uint32_t(object->model));
                if (model != world.collisionModels.end()) {
                    const auto& collision = model->second;
                    const std::array<float,3> center{
                        (collision.minimum.x + collision.maximum.x) * 0.5f,
                        (collision.minimum.y + collision.maximum.y) * 0.5f,
                        (collision.minimum.z + collision.maximum.z) * 0.5f};
                    const auto matrix = storylandScriptObjectMatrix(object->x, object->y, object->z,
                        object->rotationX, object->rotationY, object->heading);
                    destination.x = matrix[12] + matrix[0] * center[0] + matrix[4] * center[1] + matrix[8] * center[2];
                    destination.y = matrix[13] + matrix[1] * center[0] + matrix[5] * center[1] + matrix[9] * center[2];
                    destination.z = matrix[14] + matrix[2] * center[0] + matrix[6] * center[1] + matrix[10] * center[2];
                }
            }
        } else if (vehicleTarget && target >= 0) {
            const auto vehicle = environment.missionVehicles.find(target);
            if (vehicle == environment.missionVehicles.end() || !vehicle->second.alive || vehicle->second.deleted) {
                target = -1;
            } else {
                destination = {vehicle->second.x, vehicle->second.y, vehicle->second.z + 0.5f};
            }
        } else if(target==0)destination=remoteTarget?chest(runtime.coopRemotePosition):
            runtime.playerTorsoValid?runtime.playerTorso:chest(player);
        else {
            const auto victim=environment.missionCharacters.find(target);
            if(victim==environment.missionCharacters.end()||!victim->second.alive)target=-1;
            else {
                const auto pose=runtime.missionCombat.find(target);
                if(victim->second.crouch.active&&(pose==runtime.missionCombat.end()||!pose->second.anchorsValid))target=-1;
                else destination=pose!=runtime.missionCombat.end()&&pose->second.anchorsValid?pose->second.torso:
                    chest({victim->second.x,victim->second.y,victim->second.z});
            }
        }
        const auto origin=combat.muzzleValid?combat.muzzle:chest({character.x,character.y,character.z});
        const bool clear=target>=0&&storylandTraceNativeSegment(world,origin,destination)>=.995f;
        character.threatVisible = target >= 0 && clear;
        bool canFire=target>=0&&!aimOnly&&clear&&separation(origin,destination)<=weapon.range;
        if (vcs && !mountedRequest && character.objective == 0 && acquiredThreat && canFire) {
            // Retail processes the current objective before civilian threat AI
            // assigns the next one. A following script update can cancel it.
            character.objective = 0x00E4;
            character.objectiveTarget = target;
            character.objectiveCompleted = false;
            character.attackStartedAt = environment.gameMilliseconds;
            character.walkingToTarget = false;
            environment.characterTargets.erase(handle);
            continue;
        }
        if (!mountedRequest && (explicitAttack || !vcs) && character.attackStartedAt && character.ceaseAttackDelay &&
            uint32_t(character.attackStartedAt + character.ceaseAttackDelay) < environment.gameMilliseconds) {
            character.objective = 0;
            character.objectiveCompleted = true;
            character.walkingToTarget = false;
            character.attackStartedAt = 0;
            character.threatVisible = false;
            environment.characterTargets.erase(handle);
            continue;
        }
        auto& attackTimers=character.attackTimers;
        if(!mountedRequest&&canFire&&(attackTimers.shootOverride||attackTimers.attackOverride)) {
            const uint32_t now=environment.gameMilliseconds;
            if(attackTimers.shootDeadline<now) {
                if(attackTimers.attackDeadline<now) {
                    // Native setters replace the AI's requested duration; weapon
                    // cadence remains independent inside the resulting burst.
                    const uint32_t cadence=uint32_t(std::max(1.0f,weapon.interval*1000.0f));
                    attackTimers.setShoot(now,cadence,character.crouch.requested,
                        [&](){return environment.nextNativeRandom();});
                    attackTimers.setAttack(now,cadence,[&](){return environment.nextNativeRandom();});
                } else canFire=false;
            }
        }
        const float planarDistance=std::hypot(destination.x-character.x,destination.y-character.y);
        bool armedThreat = false;
        if (target == 0 && runtime.equippedWeapon < runtime.weapons.size())
            armedThreat = runtime.weapons[runtime.equippedWeapon].fireType != 0;
        else if (target > 0) {
            const auto threat = environment.missionCharacters.find(target);
            if (threat != environment.missionCharacters.end())
                armedThreat = threat->second.currentWeapon >= (vcs ? 19 : 17);
        }
        // The extended setting lets armed mission combatants use the same
        // supported crouch associations as scripted duck-and-fire tasks.
        const float extendedDuckDuration=runtime.extendedAndSaFeatures&&vcs&&environment.animationDuration?environment.animationDuration(0,155):0;
        const float extendedShotDuration=runtime.extendedAndSaFeatures&&vcs&&environment.animationDuration?environment.animationDuration(weapon.animationGroup,215):0;
        const bool extendedDuck=runtime.extendedAndSaFeatures&&vcs&&!mountedRequest&&canFire&&weapon.fireType!=0&&
            (weapon.nativeFlags&0x20000u)!=0&&character.vehicle<0&&!character.attached&&
            !combat.water.active&&character.animationId<0&&environment.mobilePhones.count(handle)==0&&
            planarDistance>5.0f&&planarDistance<20.0f&&std::isfinite(extendedDuckDuration)&&
            extendedDuckDuration>0&&extendedDuckDuration<60&&std::isfinite(extendedShotDuration)&&
            extendedShotDuration>0&&extendedShotDuration<60&&std::isfinite(weapon.crouchLoopStart)&&
            std::isfinite(weapon.crouchLoopEnd)&&std::isfinite(weapon.crouchFireTime)&&
            weapon.crouchLoopStart>=0&&weapon.crouchLoopEnd>weapon.crouchLoopStart&&
            weapon.crouchLoopEnd<=extendedShotDuration&&weapon.crouchFireTime>=weapon.crouchLoopStart&&
            weapon.crouchFireTime<=weapon.crouchLoopEnd;
        autoCrouchGuard.keep=extendedDuck;
        if(extendedDuck&&combat.extendedAutoCrouch) {
            combat.extendedAutoCrouchDeadline=environment.gameMilliseconds+1000;
            character.crouch.deadline=combat.extendedAutoCrouchDeadline;
        } else if(extendedDuck&&!character.crouch.active&&!character.crouch.requested) {
            auto& crouch=character.crouch;
            combat.extendedAutoCrouchDeadline=environment.gameMilliseconds+1000;
            crouch.requested=true;crouch.active=true;crouch.deadline=combat.extendedAutoCrouchDeadline;
            crouch.poseAnimation=155;crouch.startedAt=environment.gameMilliseconds;
            crouch.poseFade={0,4.0f,environment.gameMilliseconds,true};
            combat.extendedAutoCrouch=true;
        }
        if(!combat.extendedAutoCrouch&&!mountedRequest&&canFire&&character.creationSource==2&&character.crouchWhenThreatened&&
            (!runtime.crime.vcs||storylandMissionPedCanDuckAtThreat(character,armedThreat))&&
            planarDistance>5.0f&&planarDistance<20.0f&&character.animationId<0&&
            environment.mobilePhones.count(handle)==0) {
            auto& crouch=character.crouch;
            const uint32_t deadline=environment.gameMilliseconds+10000;
            if(crouch.active) {
                if(deadline>crouch.deadline)crouch.deadline=deadline;
            } else {
                const int association=crouch.requested?155:153;
                const float duration=environment.animationDuration?environment.animationDuration(0,association):0;
                if(std::isfinite(duration)&&duration>0&&duration<60) {
                    crouch.active=true;crouch.deadline=deadline;crouch.poseAnimation=association;
                    crouch.startedAt=environment.gameMilliseconds;
                    crouch.poseFade={0,4.0f,environment.gameMilliseconds,true};
                }
            }
        }
        if (!mountedRequest) storylandAdvanceCrouchWeaponPose(runtime,handle,combat.crouchPose,weapon,canFire);
        if(target<0||(!clear&&character.ignoreThreatsBehindObjects)) {
            environment.characterTargets.erase(handle);continue;
        }
        if (mountedRequest || vehicleTarget) {
            environment.characterTargets.erase(handle);
        } else {
            environment.characterTargets[handle] = {
                objectTarget ? StorylandMissionEnvironment::TargetKind::Object :
                    StorylandMissionEnvironment::TargetKind::Character,
                !vcs&&!objectTarget&&target==0?1:target};
        }
        if (!mountedRequest)
            character.heading=std::atan2(-(destination.x-character.x),destination.y-character.y)*57.2957795131f;
        if(!canFire||!runtime.weaponAi||combat.cooldown>0||combat.reloading>0)continue;
        if(mountedRequest&&!combat.muzzleValid)continue;
        if(!mountedRequest&&character.crouch.active&&!combat.anchorsValid)continue;
        if(weapon.fireType&&(!ammunition||!weaponOwned||*ammunition<=0))continue;
        if(weapon.fireType&&combat.magazine<=0) {
            if(storylandBeginCrouchReload(runtime,handle,combat.crouchPose,weapon)) {
                combat.reloading=weapon.reloadSeconds;
                sound(runtime,StorylandPlayTestSound::WeaponReload,origin,int(combat.weapon),int(runtime.pedestrians.size())+handle);
            }
            continue;
        }
        if(!mountedRequest&&storylandPrepareCrouchShot(runtime,handle,combat.crouchPose,weapon)!=1)continue;
        if(weapon.fireType==0) {
            if(!combat.melee.active) {
                ++combat.melee.sequence;combat.melee.active=true;combat.melee.elapsed=0;
                combat.melee.contactApplied=false;
                const size_t move=(combat.melee.sequence-1)%3;
                combat.melee.duration=weapon.meleeDurations[move];combat.melee.contactTime=weapon.meleeContactTimes[move];
            }
            if(combat.melee.elapsed<combat.melee.contactTime)continue;
            if(combat.melee.contactApplied) {
                if(combat.melee.elapsed>=combat.melee.duration)combat.melee.active=false;
                continue;
            }
            combat.melee.contactApplied=true;
            if (objectTarget) {
                storylandHitScriptObject(environment, target, weapon.nativeId, 50.0f, true, false, {});
            } else if (vehicleTarget) {
                for (size_t slot = 0; slot < runtime.vehicles.size(); ++slot) {
                    if (runtime.vehicles[slot].scriptHandle == target) {
                        hurtMissionVehicle(runtime, slot, weapon.damage, 16, weapon.nativeId,
                            {StorylandMissionEnvironment::DamageSourceKind::Character, handle});
                        break;
                    }
                }
            } else if(target==0) {
                if(remoteTarget&&runtime.coopDamageRemote)runtime.coopDamageRemote(weapon.damage,weapon.nativeId,16,handle);
                else if(!(runtime.crime.vcs && environment.playerVcsScriptFlags.ignoreMeleeImpacts) &&
                    storylandMissionPedGangDamageAllowed(runtime.crime.vcs, environment.playerGangDamageMask,
                    character.type)) {
                    storylandRequestWeaponHitSpeech(runtime,-1,player,weapon.nativeId);
                    if(runtime.inflictDamage(weapon.damage,16,weapon.nativeId,true,0,
                        storylandNativeHitDirection(environment.heading+90,origin.x-player.x,origin.y-player.y))) {
                        environment.playerLastWeaponDamage=weapon.nativeId;
                        environment.playerDamageSource={StorylandMissionEnvironment::DamageSourceKind::Character,handle};
                    }
                }
            }
            else {
                const auto victim=environment.missionCharacters.find(target);
                if(victim!=environment.missionCharacters.end() &&
                    !(runtime.crime.vcs && victim->second.vcsScriptFlags.ignoreMeleeImpacts)) {
                    storylandRequestWeaponHitSpeech(runtime,int(runtime.pedestrians.size())+target,
                        {victim->second.x,victim->second.y,victim->second.z},weapon.nativeId);
                    hurtMissionCharacter(runtime,target,weapon.damage,false,16,weapon.nativeId,
                        {StorylandMissionEnvironment::DamageSourceKind::Character,handle});
                }
            }
        } else if(weapon.fireType==2) {
            if(runtime.projectiles.size()>=64)continue;
            const float distance=std::max(.001f,separation(origin,destination));
            const float speed=weapon.speed<10?weapon.speed*50.0f:weapon.speed;
            StorylandWeaponProjectile projectile;
            projectile.position=origin;
            projectile.velocity={(destination.x-origin.x)*speed/distance,(destination.y-origin.y)*speed/distance,
                (destination.z-origin.z)*speed/distance};
            projectile.remaining=weapon.lifetime;projectile.damage=weapon.damage;projectile.radius=weapon.radius;
            projectile.kind=projectileKind(weapon);projectile.gravity=projectile.kind!=StorylandProjectileKind::Rocket;
            projectile.owner=int(runtime.pedestrians.size())+handle;projectile.weapon=combat.weapon;
            prepareUnusedProjectile(runtime,projectile,character.heading);
            if(projectile.gravity)projectile.velocity.z+=4;
            runtime.projectiles.push_back(projectile);
        } else if(weapon.fireType==3) {
            if(runtime.effects.size()>=128)continue;
            const float distance=std::max(.001f,separation(origin,destination));
            const float reach=std::min(weapon.range,distance);
            const StorylandNativePoint end{origin.x+(destination.x-origin.x)*reach/distance,
                origin.y+(destination.y-origin.y)*reach/distance,origin.z+(destination.z-origin.z)*reach/distance};
            const float contact=storylandTraceNativeSegment(world,origin,end);
            StorylandRuntimeEffect effect;
            effect.position={origin.x+(end.x-origin.x)*contact,origin.y+(end.y-origin.y)*contact,
                origin.z+(end.z-origin.z)*contact};
            effect.kind=StorylandParticleKind::Flame;effect.remaining=std::max(.1f,weapon.lifetime);
            effect.radius=std::max(.1f,weapon.radius);effect.damage=weapon.damage;
            effect.nativeWeapon=weapon.nativeId;
            effect.owner=int(runtime.pedestrians.size())+handle;runtime.effects.push_back(effect);
        } else if(vcs&&(weapon.nativeId==30||weapon.nativeId==31)) {
            muzzleFlash(runtime,origin,weapon.nativeId,StorylandMuzzleOwner::MissionPed,handle);
            const float accuracy=std::clamp(float(character.accuracy)*.01f,0.0f,1.0f);
            const float deviation=(1.0f-accuracy)*.04f*separation(origin,destination)*
                storylandSaCrouchSpreadScale(vcs ? runtime.extendedAndSaFeatures : runtime.extendedLcsCrouch,
                    character.crouch.active);
            destination.x+=(randomUnit(runtime)-.5f)*deviation;
            destination.y+=(randomUnit(runtime)-.5f)*deviation;
            destination.z+=(randomUnit(runtime)-.5f)*deviation;
            addVcsSniperBullet(runtime,origin,destination,combat.weapon,
                StorylandVcsBulletSource::MissionCharacter,handle);
        } else {
            muzzleFlash(runtime,origin,weapon.nativeId,StorylandMuzzleOwner::MissionPed,handle);
            StorylandNativePoint end;
            if(vcs) {
                const bool playerTarget=target==0&&!objectTarget&&!vehicleTarget;
                float targetSpeed=0;
                if(playerTarget&&!remoteTarget) {
                    if(runtime.occupiedVehicle>=0&&size_t(runtime.occupiedVehicle)<runtime.vehicles.size()) {
                        const auto& vehicle=runtime.vehicles[size_t(runtime.occupiedVehicle)];
                        targetSpeed=std::hypot(vehicle.speed,vehicle.lateralSpeed,vehicle.verticalSpeed);
                    } else {
                        const float verticalSpeed=runtime.playerWater.active?
                            runtime.playerWater.verticalSpeed:runtime.verticalSpeed;
                        targetSpeed=std::hypot(runtime.movementSpeed,verticalSpeed);
                    }
                }
                end=storylandVcsNpcInstantHitEnd(origin,destination,weapon.range,character.accuracy,
                    playerTarget,targetSpeed/StorylandNativePhysicsRate,
                    storylandSaCrouchSpreadScale(runtime.extendedAndSaFeatures,character.crouch.active),
                    [&](){return randomUnit(runtime);});
            } else {
                const float accuracy=std::clamp(float(character.accuracy)*.01f,0.0f,1.0f);
                const float deviation=(1.0f-accuracy)*.04f*separation(origin,destination)*
                    storylandSaCrouchSpreadScale(runtime.extendedLcsCrouch,character.crouch.active);
                destination.x+=(randomUnit(runtime)-.5f)*deviation;
                destination.y+=(randomUnit(runtime)-.5f)*deviation;
                destination.z+=(randomUnit(runtime)-.5f)*deviation;
                const float length=std::max(.001f,separation(origin,destination));
                end={origin.x+(destination.x-origin.x)*weapon.range/length,
                    origin.y+(destination.y-origin.y)*weapon.range/length,origin.z+(destination.z-origin.z)*weapon.range/length};
            }
            float nearest=world.scriptEnvironment==&environment?
                storylandTraceNativeSegmentWithoutVisibleScriptObjects(world,origin,end):
                storylandTraceNativeSegment(world,origin,end);
            int32_t hit=-1;
            bool headHit=false;
            uint8_t hitPiece=0;
            const auto sphere=[&](const StorylandNativePoint& center,float radius=.4f) {
                const float dx=end.x-origin.x,dy=end.y-origin.y,dz=end.z-origin.z;
                const float ox=origin.x-center.x,oy=origin.y-center.y,oz=origin.z-center.z;
                const float a=dx*dx+dy*dy+dz*dz,b=ox*dx+oy*dy+oz*dz,c=ox*ox+oy*oy+oz*oz-radius*radius;
                const float discriminant=b*b-a*c;
                return discriminant<0||a<.00001f?2.0f:(-b-std::sqrt(discriminant))/a;
            };
            if(runtime.health>0&&(!world.paths.vcs||vcsPlayerPrimaryBulletCollision(runtime,true))) {
                const float fraction=sphere(runtime.playerTorsoValid?runtime.playerTorso:chest(player));
                if(fraction>=0&&fraction<nearest) {nearest=fraction;hit=0;}
            }
            if(runtime.coopRemoteAlive&&runtime.coopDamageRemote) {
                const float fraction=sphere(chest(runtime.coopRemotePosition));
                if(fraction>=0&&fraction<nearest) {nearest=fraction;hit=-2;}
            }
            for(const auto& victim:environment.missionCharacters) {
                if(victim.first==handle||!victim.second.alive)continue;
                if(world.paths.vcs&&!vcsMissionPrimaryBulletCollision(runtime,victim.second,true))continue;
                std::vector<StorylandMissionEnvironment::CharacterHitSphere> nativeSpheres;
                const bool native=vcs&&environment.characterHitSpheres&&
                    environment.characterHitSpheres(victim.first,nativeSpheres);
                if(native) {
                    for(const auto& volume:nativeSpheres) {
                        if(!std::isfinite(volume.radius)||volume.radius<=0)continue;
                        if(storylandVcsPedSphereIntersection({origin.x,origin.y,origin.z},
                            {end.x,end.y,end.z},volume.center,volume.radius,nearest)) {
                            hit=victim.first;headHit=volume.piece==6;hitPiece=volume.piece;
                        }
                    }
                } else {
                    const auto pose=runtime.missionCombat.find(victim.first);
                    if(victim.second.crouch.active&&(pose==runtime.missionCombat.end()||!pose->second.anchorsValid))continue;
                    const auto center=pose!=runtime.missionCombat.end()&&pose->second.anchorsValid?pose->second.torso:
                        chest({victim.second.x,victim.second.y,victim.second.z});
                    const float fraction=sphere(center);
                    if(fraction>=0&&fraction<nearest) {nearest=fraction;hit=victim.first;headHit=false;hitPiece=0;}
                    if (pose != runtime.missionCombat.end() && pose->second.anchorsValid) {
                        const float head = sphere(pose->second.head, .18f);
                        if (head >= 0 && head < nearest) { nearest=head;hit=victim.first;headHit=true;hitPiece=6; }
                    }
                }
            }
            int32_t ambientHit=-1;
            if(vcs) for(size_t slot=0;slot<actors.size()&&slot<runtime.pedestrians.size();++slot) {
                const auto& pedestrian=runtime.pedestrians[slot];
                if(pedestrian.scriptHandle>=0||pedestrian.health<=0||pedestrian.renderAlpha==0||
                    pedestrian.serviceVehicleIdentity)continue;
                const float fraction=sphere(chest(actors[slot].position));
                if(fraction>=0&&fraction<nearest) {nearest=fraction;ambientHit=int32_t(slot);hit=-1;}
            }
            int32_t objectHit=-1;StorylandNativePoint normal{};
            for(const auto& object:environment.objects)
                if(storylandTraceScriptObject(world,object,origin,end,nearest,normal)) {objectHit=object.handle;hit=-1;ambientHit=-1;}
            int ignoredVehicle=-1;
            if(vcs&&character.vehicle>=0) {
                const auto occupied=environment.missionVehicles.find(character.vehicle);
                if(occupied!=environment.missionVehicles.end()&&!occupied->second.deleted) {
                    const int slot=occupied->second.spawnedSlot;
                    if(slot>=0&&size_t(slot)<runtime.vehicles.size())
                        ignoredVehicle=storylandVcsSeatedShooterIgnoredVehicle(vcs,weapon.nativeId,character.vehicle,
                            occupied->second.deleted,slot,runtime.vehicles[size_t(slot)].scriptHandle);
                }
            }
            int vehicleHit=traceMissionVehicles(runtime,origin,end,nearest,normal,ignoredVehicle);
            if(vehicleHit>=0) {hit=-1;objectHit=-1;ambientHit=-1;}
            if(world.paths.vcs&&vehicleHit>=0) {
                const auto occupant=traceVcsVehicleOccupantHead(runtime,origin,end,vehicleHit,handle);
                if(occupant.ambient>=0) {
                    ambientHit=occupant.ambient;nearest=occupant.fraction;
                    headHit=true;hitPiece=6;vehicleHit=-1;hit=-1;
                } else if(occupant.character>=0) {
                    hit=occupant.character;nearest=occupant.fraction;
                    headHit=true;hitPiece=6;vehicleHit=-1;ambientHit=-1;
                }
            }
            if(vehicleHit>=0) {
                const auto occupant=traceVehicleOccupantShot(runtime,world,actors,origin,end,
                    vehicleHit,handle,-1);
                if(occupant.character>=0) {
                    hit=occupant.character;nearest=occupant.fraction;
                    headHit=occupant.piece==6;hitPiece=occupant.piece;ambientHit=-1;
                }
            }
            const StorylandMissionEnvironment::DamageSource source{StorylandMissionEnvironment::DamageSourceKind::Character,handle};
            if(hit==-2&&runtime.coopDamageRemote)runtime.coopDamageRemote(weapon.damage,weapon.nativeId,1,handle);
            else if(hit==0) {
                if(storylandMissionPedGangDamageAllowed(runtime.crime.vcs, environment.playerGangDamageMask,
                    character.type)) {
                    storylandRequestWeaponHitSpeech(runtime,-1,player,weapon.nativeId);
                    if(runtime.inflictDamage(weapon.damage,1,weapon.nativeId,true,headHit?6:hitPiece,
                        storylandNativeHitDirection(environment.heading+90,
                            origin.x-player.x,origin.y-player.y),false,&origin)) {
                        environment.playerLastWeaponDamage=weapon.nativeId;
                        environment.playerDamageSource=source;
                    }
                }
            } else if(hit>0) {
                const auto victim=environment.missionCharacters.find(hit);
                if(victim!=environment.missionCharacters.end())
                    storylandRequestWeaponHitSpeech(runtime,int(runtime.pedestrians.size())+hit,
                        {victim->second.x,victim->second.y,victim->second.z},weapon.nativeId);
                hurtMissionCharacter(runtime,hit,weapon.damage,false,1,weapon.nativeId,source,
                    headHit,false,false,true,hitPiece,victim==environment.missionCharacters.end()?0:
                        storylandNativeHitDirection(victim->second.heading+90,
                            origin.x-victim->second.x,origin.y-victim->second.y));
            }
            if(ambientHit>=0) {
                auto& victim=runtime.pedestrians[size_t(ambientHit)];
                const float previousHealth=victim.health,previousArmour=victim.armour;
                hurtPed(runtime,size_t(ambientHit),weapon.damage,actors[size_t(ambientHit)].position,false,
                    true,StorylandCrime::Shooting,&weapon,&origin,true,headHit,weapon.nativeId,hitPiece,
                    storylandNativeHitDirection(actors[size_t(ambientHit)].yaw,
                        origin.x-actors[size_t(ambientHit)].position.x,
                        origin.y-actors[size_t(ambientHit)].position.y),1,actors[size_t(ambientHit)].yaw,
                    std::hypot(victim.contact.velocity.x,victim.contact.velocity.y));
                storylandVcsPoliceRememberMissionAssailant(runtime,size_t(ambientHit),handle,
                    victim.health<previousHealth||victim.armour<previousArmour);
            }
            if(vehicleHit>=0)hurtMissionVehicle(runtime,size_t(vehicleHit),weapon.damage,1,weapon.nativeId,source);
            if(objectHit>=0)storylandHitScriptObject(environment,objectHit,weapon.nativeId,50,false,
                weapon.nativeId>=(vcs?21:19)&&weapon.nativeId<=(vcs?23:21),normal);
            const StorylandNativePoint impact{origin.x+(end.x-origin.x)*nearest,
                origin.y+(end.y-origin.y)*nearest,origin.z+(end.z-origin.z)*nearest};
            weaponTrace(runtime,origin,impact,weapon.nativeId);
            queueVcsBulletPass(runtime,world,origin,impact,false);
            if(nearest<0.999f && hit<0 && ambientHit<0 && vehicleHit<0 && objectHit<0)
                storylandWeaponWorldImpact(runtime,impact);
            if(weapon.nativeId==(vcs?30:28)||weapon.nativeId==(vcs?31:29)) {
                runtime.sniperTraceActive=true;runtime.sniperTraceFrom=origin;runtime.sniperTraceTo=impact;
            }
        }
        ++combat.shotSerial;character.shotAt=environment.gameMilliseconds;character.hasShot=true;
        if (!mountedRequest && (explicitAttack || !vcs) && character.attackStartedAt == 0) {
            character.attackStartedAt = environment.gameMilliseconds;
        }
        float shotDelay=weapon.fireType?std::max(0.0f,-combat.cooldown):0.0f;
        if(!mountedRequest&&weapon.fireType&&character.crouch.active&&combat.crouchPose.animation==215) {
            const auto& pose=combat.crouchPose;
            const float animationDelay=std::clamp(float(environment.gameMilliseconds-pose.startedAt)*.001f+
                pose.offset-weapon.crouchFireTime,0.0f,dt);
            shotDelay=weaponCooling?std::min(shotDelay,animationDelay):animationDelay;
        }
        const float empireRate=storylandExtendedEmpireFireRate(vcs,runtime.extendedAndSaFeatures,
            environment,character.type,character.x,character.y,player.x,player.y);
        combat.cooldown=std::max(0.0f,weapon.interval/empireRate-shotDelay);
        sound(runtime,weapon.fireType?StorylandPlayTestSound::WeaponShot:StorylandPlayTestSound::Melee,
            origin,int(combat.weapon),int(runtime.pedestrians.size())+handle);
        if(weapon.fireType) {--combat.magazine;--*ammunition;}
    }
    // Consume each original0505 call once. Re-entering with dt0 preserves the
    // production weapon cooldown, magazine, range, LOS and hit consumers.
    for (const auto& request : mountedRequests)
        if (request.fire) storylandUpdateMissionCombat(runtime,world,actors,player,0,&request);
}

bool storylandTakePhotograph(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& nativeWorld) {
    if(!runtime.missionEnvironment||runtime.equippedWeapon>=runtime.weapons.size())return false;
    auto& world=*runtime.missionEnvironment;
    const auto& weapon=runtime.weapons[runtime.equippedWeapon];
    const bool cameraWeapon=weapon.fireType==4&&weapon.nativeId==(runtime.crime.vcs?38:36);
    if(!cameraWeapon||!world.activeCameraVectors||!world.screenCoordinates||
        (!runtime.crime.vcs&&!world.sphereOnScreen)||(runtime.crime.vcs&&!world.characterWorldMatrix)||
        !world.photographLineClear||!world.activeCameraMode||world.activeCameraMode()!=46)return false;
    std::array<float,3> eye{},target{};
    if(!world.activeCameraVectors(eye,target))return false;
    const float length=std::hypot(target[0]-eye[0],target[1]-eye[1],target[2]-eye[2]);
    if(!std::isfinite(length)||length<=.0001f||
        (runtime.crime.vcs&&!std::isfinite(world.cameraNearClip)))return false;
    const float nearClip=std::clamp(world.cameraNearClip,.001f,50.0f);
    auto start=eye;
    for(size_t axis=0;axis<3;++axis)start[axis]+=2*(target[axis]-eye[axis])/length;
    world.photographTaken=true;
    world.photographFlashUntil=world.gameMilliseconds+120;
    ++runtime.photographSerial;
    ++runtime.playerShotSerial;
    for(const auto& entry:world.missionCharacters) {
        const auto& character=entry.second;
        std::array<float,3> origin{character.x,character.y,character.z};
        if(runtime.crime.vcs) {
            std::array<float,16> frame{};
            if(!world.characterWorldMatrix(entry.first,frame)||
                !std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);}))continue;
            origin={frame[12],frame[13],frame[14]};
        }
        const std::array<float,3> center{origin[0],origin[1],origin[2]+.8f};
        if(runtime.crime.vcs) {
            float depth=0;
            for(size_t axis=0;axis<3;++axis)
                depth+=(center[axis]-eye[axis])*(target[axis]-eye[axis])/length;
            if(depth<nearClip)continue;
        }
        std::array<float,2> screen{};
        if(std::hypot(origin[0]-eye[0],origin[1]-eye[1],origin[2]-eye[2])>=125||
            (!runtime.crime.vcs&&!world.sphereOnScreen(center,0))||!world.screenCoordinates(center,screen)||
            screen[0]<=48||screen[0]>=432||screen[1]<=27.2f||screen[1]>=244.8f)continue;
        if(world.photographLineClear(entry.first,start,center))world.photographedCharacters.insert(entry.first);
    }
    if(runtime.crime.vcs&&world.vehicleWorldMatrix&&world.photographVehicleLineClear) {
        // Expose existing physical pool identities without making ambient cars mission-owned.
        if(world.findScriptVehiclesInArea)
            world.findScriptVehiclesInArea({eye[0]-125,eye[1]-125,eye[0]+125,eye[1]+125},-1,0);
        for(const auto& entry:world.missionVehicles) {
            if(entry.second.deleted)continue;
            std::array<float,16> frame{};
            if(!world.vehicleWorldMatrix(entry.first,frame)||
                !std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);}))continue;
            const std::array<float,3> origin{frame[12],frame[13],frame[14]};
            const std::array<float,3> center{origin[0],origin[1],origin[2]+.8f};
            float depth=0;
            for(size_t axis=0;axis<3;++axis)
                depth+=(center[axis]-eye[axis])*(target[axis]-eye[axis])/length;
            if(depth<nearClip||std::hypot(origin[0]-eye[0],origin[1]-eye[1],origin[2]-eye[2])>=125)continue;
            std::array<float,2> screen{};
            if(!world.screenCoordinates(center,screen)||screen[0]<=48||screen[0]>=432||
                screen[1]<=27.2f||screen[1]>=244.8f)continue;
            if(world.photographVehicleLineClear(entry.first,start,center))
                world.photographedVehicles.insert(entry.first);
        }
    }
    return true;
}

bool storylandCanAimPlayTestWeapon(const StorylandPlayTestRuntime& runtime) {
    if(!runtime.playerControl||runtime.health<=0||runtime.occupiedVehicle>=0||
        storylandPlayerBulletReactionActive(runtime)||
        runtime.entryTransition>=0||runtime.playerWater.active||runtime.manualAimActive||
        runtime.vehicleAction!=StorylandPlayTestRuntime::VehicleAction::None||
        runtime.equippedWeapon>=runtime.weapons.size())return false;
    if(runtime.missionEnvironment&&(runtime.missionEnvironment->remoteVehicle>=0||
        runtime.missionEnvironment->mobilePhones.count(0)||runtime.missionEnvironment->cutscene.playing))return false;
    const auto& weapon=runtime.weapons[runtime.equippedWeapon];
    // CWeaponInfo::canAim, tested by VCS CPlayerPed at 0x2364E0.
    // Missing native metadata must not turn every weapon into a lock-on gun.
    return weapon.nativeId>=0&&((weapon.nativeFlags&0x40u)!=0||weapon.nativeId==0)&&
        std::isfinite(weapon.targetRange)&&weapon.targetRange>0;
}

bool storylandCanFreeAimPlayTestWeapon(const StorylandPlayTestRuntime& runtime) {
    if(!runtime.playerControl||runtime.health<=0||runtime.paused||runtime.pauseMenu||
        runtime.occupiedVehicle>=0||runtime.entryTransition>=0||runtime.playerWater.active||
        runtime.manualAimActive||runtime.playerReaction.active()||runtime.playerCoughTime>0||
        runtime.vehicleAction!=StorylandVehicleAction::None||
        runtime.debugMovement!=StorylandDebugMovement::Walking||
        runtime.equippedWeapon>=runtime.weapons.size()||runtime.equippedWeapon>=runtime.inventory.size())return false;
    const auto& weapon=runtime.weapons[runtime.equippedWeapon];
    if(!runtime.inventory[runtime.equippedWeapon].owned||weapon.nativeId<0||
        weapon.fireType<1||weapon.fireType>3||!std::isfinite(weapon.range)||weapon.range<=0)return false;
    if(runtime.crime.vcs&&storylandPlayerWeaponManualAimMode(runtime,weapon.nativeId)!=0)return false;
    if(runtime.missionEnvironment) {
        const auto& environment=*runtime.missionEnvironment;
        if(!environment.cameraControlEnabled||!environment.control||environment.camera.active||
            environment.cutscene.playing||environment.playerWalkTask.active||
            environment.playerVehicleEntryTarget>=0||environment.activeVcsCharacterAttachment(0)||
            environment.remoteVehicle>=0||environment.remoteControl.vehicle>=0||
            environment.playerArrested||environment.playerPositionFrozen||environment.manualAim.forced||
            environment.mobilePhones.count(runtime.crime.vcs?0:1)||
            (!runtime.crime.vcs&&environment.lcsExtendedWorld.scriptInputPause))return false;
    }
    return true;
}

static StorylandNativePoint storylandFreeAimPoint(const StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const std::vector<StorylandNativeTrafficActor>& actors,
    const StorylandNativePoint& start,const StorylandNativePoint& end) {
    float nearest=storylandTraceNativeSegment(world,start,end);
    const float dx=end.x-start.x,dy=end.y-start.y,dz=end.z-start.z;
    const float lengthSquared=dx*dx+dy*dy+dz*dz;
    const auto sphere=[&](const StorylandNativePoint& center,float radius) {
        if(!std::isfinite(center.x)||!std::isfinite(center.y)||!std::isfinite(center.z)||
            !std::isfinite(radius)||radius<=0||lengthSquared<=.000001f)return;
        const float x=start.x-center.x,y=start.y-center.y,z=start.z-center.z;
        const float projection=x*dx+y*dy+z*dz;
        const float distance=x*x+y*y+z*z-radius*radius;
        const float discriminant=projection*projection-lengthSquared*distance;
        if(discriminant<0)return;
        const float hit=distance<=0?0:(-projection-std::sqrt(discriminant))/lengthSquared;
        if(hit>=0&&hit<nearest)nearest=hit;
    };
    for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index) {
        const auto& pedestrian=runtime.pedestrians[index];
        if(pedestrian.health<=0||pedestrian.scriptHandle>=0)continue;
        sphere(chest(actors[index].position),.4f);
        if(pedestrian.headValid)sphere(pedestrian.head,.18f);
    }
    StorylandNativePoint normal{};
    if(runtime.missionEnvironment) {
        const auto& environment=*runtime.missionEnvironment;
        for(const auto& entry:environment.missionCharacters) {
            const auto& character=entry.second;
            if(!character.alive||character.health<=0||character.removeFromWorld||
                (world.paths.vcs&&(!character.visible||!vcsMissionPrimaryBulletCollision(runtime,character,
                    runtime.equippedWeapon<runtime.weapons.size()&&
                        storylandWeaponUsesBulletTrace(runtime.weapons[runtime.equippedWeapon]))))||
                (!world.paths.vcs&&character.vehicle>=0&&!character.canBeShotInVehicle))continue;
            std::vector<StorylandMissionEnvironment::CharacterHitSphere> volumes;
            if(!world.paths.vcs&&environment.characterHitSpheres&&environment.characterHitSpheres(entry.first,volumes)) {
                for(const auto& volume:volumes)
                    sphere({volume.center[0],volume.center[1],volume.center[2]},volume.radius);
            } else {
                const auto pose=runtime.missionCombat.find(entry.first);
                if(character.crouch.active&&(pose==runtime.missionCombat.end()||!pose->second.anchorsValid))continue;
                sphere(pose!=runtime.missionCombat.end()&&pose->second.anchorsValid?
                    pose->second.torso:chest({character.x,character.y,character.z}),.4f);
                if(pose!=runtime.missionCombat.end()&&pose->second.anchorsValid)sphere(pose->second.head,.18f);
            }
        }
        for(const auto& object:environment.objects)
            storylandTraceScriptObject(world,object,start,end,nearest,normal);
    }
    traceMissionVehicles(runtime,start,end,nearest,normal);
    if(runtime.coopRemoteAlive) {
        sphere(runtime.coopRemoteTorso,.4f);
        auto head=runtime.coopRemoteTorso;head.z+=.55f;sphere(head,.2f);
    }
    return {start.x+dx*nearest,start.y+dy*nearest,start.z+dz*nearest};
}

void storylandPreparePlayTestFreeAim(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const std::vector<StorylandNativeTrafficActor>& pedestrians,
    const StorylandNativePoint& player,float& heading,StorylandStoriesCamera& camera,
    const StorylandPlayTestCameraInput& input,float seconds) {
    const bool wasActive=camera.freeAimActive;
    runtime.freeAimActive=false;camera.freeAimActive=false;
    if(!input.freeAim||!storylandCanFreeAimPlayTestWeapon(runtime)||storylandHasPlayTestTarget(runtime)) {
        if(wasActive&&!camera.manualAimActive) {
            camera.fieldOfView=70;
            camera.target={player.x,player.y,player.z+1.0f};camera.stringEye=camera.eye;
            if(!runtime.targetLockRequested)runtime.playerAiming=false;
        }
        return;
    }
    const StorylandNativePoint anchor=runtime.playerTorsoValid?runtime.playerTorso:
        StorylandNativePoint{player.x,player.y,player.z+1.25f};
    const float range=runtime.weapons[runtime.equippedWeapon].range;
    if(!storylandUpdateFreeAimCamera(camera,player,anchor,heading,input,seconds,range,world))return;
    runtime.freeAimActive=true;runtime.playerAiming=true;
    runtime.freeAimOrigin=camera.eye;
    runtime.freeAimTarget=storylandFreeAimPoint(runtime,world,pedestrians,camera.eye,camera.target);
    runtime.aimPitch=camera.pitch;heading=camera.yaw;
}

void storylandRefreshPlayTestFreeAim(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const std::vector<StorylandNativeTrafficActor>& pedestrians,
    const StorylandNativePoint& player,float& heading,StorylandStoriesCamera& camera) {
    if(!runtime.freeAimActive)return;
    StorylandPlayTestCameraInput input;input.freeAim=true;
    storylandPreparePlayTestFreeAim(runtime,world,pedestrians,player,heading,camera,input,.02f);
}

static bool storylandTargetCharacterAllowed(const StorylandPlayTestRuntime& runtime,
    const StorylandMissionEnvironment::Character& character) {
    if(!character.alive||character.health<=0||!character.visible||character.renderAlpha==0||
        character.removeFromWorld||character.neverTargeted||character.leader==0)return false;
    if(character.vehicle<0)return true;
    if(!runtime.missionEnvironment)return false;
    const auto& environment=*runtime.missionEnvironment;
    // Both retail selectors exclude enclosed vehicle occupants. Motorcycle
    // riders use vehicle type 6 in VCS and type 5 in LCS (0x34C860).
    const int motorcycleType=runtime.crime.vcs?6:5;
    if(environment.vehicleNativeType)return environment.vehicleNativeType(character.vehicle)==motorcycleType;
    const auto vehicle=environment.missionVehicles.find(character.vehicle);
    return vehicle!=environment.missionVehicles.end()&&vehicle->second.spawnedSlot>=0&&
        size_t(vehicle->second.spawnedSlot)<runtime.vehicles.size()&&
        runtime.vehicles[size_t(vehicle->second.spawnedSlot)].nativeDefinition.nativeType==motorcycleType;
}

bool storylandHasPlayTestTarget(const StorylandPlayTestRuntime& runtime) {
    if(!runtime.playerAiming||!runtime.targetLockRequested||!runtime.targetVisible||
        runtime.targetWeapon!=runtime.equippedWeapon||!storylandCanAimPlayTestWeapon(runtime))return false;
    if(runtime.targetedPed>=0) {
        if(size_t(runtime.targetedPed)>=runtime.pedestrians.size())return false;
        const auto& ped=runtime.pedestrians[size_t(runtime.targetedPed)];
        return ped.scriptHandle<0&&ped.health>0&&ped.state!=StorylandPedState::Dead&&ped.torsoValid;
    }
    if(!runtime.missionEnvironment)return false;
    const auto& environment=*runtime.missionEnvironment;
    const auto target=environment.playerTarget;
    if(target.kind==StorylandMissionEnvironment::TargetKind::Character) {
        const auto character=environment.missionCharacters.find(target.handle);
        const auto pose=runtime.missionCombat.find(target.handle);
        return character!=environment.missionCharacters.end()&&
            storylandTargetCharacterAllowed(runtime,character->second)&&
            pose!=runtime.missionCombat.end()&&pose->second.anchorsValid;
    }
    if(target.kind==StorylandMissionEnvironment::TargetKind::Object) {
        if(std::find(environment.targettableObjects.begin(),environment.targettableObjects.end(),target.handle)==
            environment.targettableObjects.end())return false;
        const auto object=std::find_if(environment.objects.begin(),environment.objects.end(),
            [&](const auto& item){return item.handle==target.handle;});
        return object!=environment.objects.end()&&object->damage.visible&&!object->damage.renderDamaged;
    }
    return false;
}

bool storylandPlayTestAimKeepsHeading(const StorylandPlayTestRuntime& runtime) {
    if(!runtime.extendedAndSaFeatures&&!(runtime.extendedLcsCrouch&&!runtime.crime.vcs))return false;
    if (runtime.health <= 0 || runtime.occupiedVehicle >= 0 ||
        runtime.vehicleAction != StorylandPlayTestRuntime::VehicleAction::None) {
        return false;
    }
    if (runtime.freeAimActive) return true;
    if (runtime.equippedWeapon >= runtime.weapons.size()) return false;
    const auto& weapon = runtime.weapons[runtime.equippedWeapon];
    if (!storylandWeaponSupportsAimedLocomotion(runtime.crime.vcs,
            weapon.nativeId, weapon.fireType, weapon.nativeFlags)) {
        return false;
    }
    return runtime.manualAimActive || storylandHasPlayTestTarget(runtime);
}

bool storylandPlayTestAimBlocksMovement(const StorylandPlayTestRuntime& runtime) {
    if (storylandPlayTestAimKeepsHeading(runtime)) return false;
    if (runtime.manualAimActive) return true;
    return storylandHasPlayTestTarget(runtime) && runtime.equippedWeapon < runtime.weapons.size() &&
        !(runtime.weapons[runtime.equippedWeapon].nativeFlags & 0x80u);
}

bool storylandPlayTestTargetNativeRoot(const StorylandPlayTestRuntime& runtime,
    StorylandNativePoint& result) {
    if(!storylandHasPlayTestTarget(runtime))return false;
    StorylandNativePoint point{};
    if(runtime.targetedPed>=0) {
        const auto& ped=runtime.pedestrians[size_t(runtime.targetedPed)];
        if(!ped.nativeRootValid)return false;
        point=ped.nativeRoot;
    } else {
        const auto& environment=*runtime.missionEnvironment;
        const auto target=environment.playerTarget;
        if(target.kind==StorylandMissionEnvironment::TargetKind::Character) {
            const auto pose=runtime.missionCombat.find(target.handle);
            if(pose==runtime.missionCombat.end()||!pose->second.nativeRootValid)return false;
            point=pose->second.nativeRoot;
        } else if(target.kind==StorylandMissionEnvironment::TargetKind::Object) {
            const auto object=std::find_if(environment.objects.begin(),environment.objects.end(),
                [&](const auto& item){return item.handle==target.handle;});
            if(object==environment.objects.end())return false;
            point={object->x,object->y,object->z};
        } else return false;
    }
    if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))return false;
    result=point;return true;
}

void storylandClearPlayTestTarget(StorylandPlayTestRuntime& runtime) {
    runtime.targetedPed=-1;runtime.targetPosition={};runtime.targetVisible=false;
    runtime.targetLockRequested=false;runtime.playerAiming=false;runtime.targetWeapon=SIZE_MAX;
    runtime.targetIndicator.reset();
    if(runtime.missionEnvironment)runtime.missionEnvironment->playerTarget={};
}

static void storylandUpdatePlayTestTarget(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const std::vector<StorylandNativeTrafficActor>& actors,
    const StorylandNativePoint& player,float& heading,bool targetHeld) {
    using TargetKind=StorylandMissionEnvironment::TargetKind;
    const bool targetPressed=targetHeld&&!runtime.targetLockRequested;
    const bool changedWeapon=runtime.targetWeapon!=SIZE_MAX&&runtime.targetWeapon!=runtime.equippedWeapon;
    // The previous frame may still report sprint when the player presses E
    // during mouse aim. Extended free aim changes that sprint to a run in the
    // same frame, so it must not consume the new lock request.
    const bool releasingSprint=runtime.playerSprintActive&&
        !((runtime.extendedAndSaFeatures||(!runtime.crime.vcs&&runtime.extendedLcsCrouch))&&runtime.freeAimActive);
    const bool aiming=targetHeld&&storylandCanAimPlayTestWeapon(runtime)&&
        !(releasingSprint&&runtime.weapons[runtime.equippedWeapon].fireType!=0);
    const auto clear=[&]() {
        storylandClearPlayTestTarget(runtime);
        // A rejected or lost target is not another target-button press. It
        // cannot silently acquire somebody else while the button stays down.
        runtime.targetLockRequested=targetHeld;
        runtime.playerAiming=aiming||runtime.freeAimActive;
        runtime.targetWeapon=runtime.equippedWeapon;
    };
    if(!aiming||(changedWeapon&&!targetPressed)||!std::isfinite(heading)||!std::isfinite(player.x)||
        !std::isfinite(player.y)||!std::isfinite(player.z)) {clear();return;}
    if(!world.paths.vcs&&runtime.missionEnvironment&&!runtime.missionEnvironment->lcsExtendedWorld.autoAim) {
        clear();return;
    }
    if(!targetPressed&&!runtime.targetVisible) {clear();return;}
    runtime.targetLockRequested=targetHeld;runtime.playerAiming=true;
    runtime.targetWeapon=runtime.equippedWeapon;
    const auto& weapon=runtime.weapons[runtime.equippedWeapon];
    struct Candidate {
        int priority=0,ambient=-1;
        float score=0,distance=0,angle=0,healthRatio=1;
        StorylandMissionEnvironment::Target target;
        StorylandNativePoint position;
    };
    std::vector<Candidate> candidates;
    const auto sameTarget=[&](int ambient,StorylandMissionEnvironment::Target target) {
        if(ambient>=0)return ambient==runtime.targetedPed;
        if(!runtime.missionEnvironment||runtime.targetedPed>=0)return false;
        const auto current=runtime.missionEnvironment->playerTarget;
        return current.kind==target.kind&&current.handle==target.handle;
    };
    const float facing=heading*radians,forwardX=std::cos(facing),forwardY=std::sin(facing);
    const StorylandNativePoint eye{player.x,player.y,player.z+1.0f};
    const auto modelOnScreen=[&](int32_t model,const StorylandNativePoint& position,float rotation) {
        if(!runtime.missionEnvironment||!runtime.missionEnvironment->sphereOnScreen)return false;
        const auto collision=world.collisionModels.find(uint32_t(model));
        if(collision==world.collisionModels.end())return false;
        const auto& bounds=collision->second;
        const float c=std::cos(rotation*radians),s=std::sin(rotation*radians);
        return runtime.missionEnvironment->sphereOnScreen({position.x+c*bounds.boundingCenter.x-s*bounds.boundingCenter.y,
            position.y+s*bounds.boundingCenter.x+c*bounds.boundingCenter.y,position.z+bounds.boundingCenter.z},
            bounds.boundingRadius);
    };
    const auto addCandidate=[&](int ambient,StorylandMissionEnvironment::Target target,
        const StorylandNativePoint& root,const StorylandNativePoint& point,int priority,
        float healthRatio,bool priorityBonus,bool missionCreated,bool onScreen) {
        if(!targetPressed&&!sameTarget(ambient,target))return;
        if(!std::isfinite(root.x)||!std::isfinite(root.y)||!std::isfinite(root.z)||
            !std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))return;
        const float dx=root.x-player.x,dy=root.y-player.y,dz=root.z-player.z;
        const float distance=std::hypot(dx,dy);
        const bool retaining=runtime.targetVisible&&sameTarget(ambient,target);
        if(std::hypot(distance,dz)>weapon.targetRange||
            (!retaining&&world.paths.vcs&&distance>=weapon.targetRange))return;
        const float angle=distance>0?std::acos(std::clamp((dx*forwardX+dy*forwardY)/distance,-1.0f,1.0f)):0;
        // Both native versions use this visibility/reach domain: VCS
        // 0x1EF4C0/0x23A5A8, LCS 0x1D33D8/0x34DA20.
        // Retention instead tests the authored 3D range (VCS0x23A768,
        // LCS0x34D980). Walking while aiming with an arm does not become a
        // new acquisition just because the body turns away from the target.
        if(!retaining&&(dx*forwardX+dy*forwardY<0||distance>=40.0f||
            (!(weapon.nativeFlags&0x80u)&&std::fabs(dz)>distance)))return;
        float score=0;
        if(world.paths.vcs) {
            // SLUS_215.90: AddCandidate 0x2D7628, CPed visibility 0x1EF4C0,
            // and weapon reach 0x23A5A8. Visibility is in front of the player,
            // within 40m; non-arm weapons also require a reachable elevation.
            if(std::fabs(-dx*forwardY+dy*forwardX)<.15f)priority=std::max(priority,7);
            score=angle*distance;
        } else {
            // SLUS_214.23 0x34D330 maximizes -distance -5*angle, with
            // +30 for ordinary/attacking targets, +15 on screen and +40
            // for mission peds. It does not use VCS's priority categories.
            score=distance+5.0f*angle-(priorityBonus?30.0f:0.0f)-
                (onScreen?15.0f:0.0f)-(missionCreated?40.0f:0.0f);
            priority=0;
        }
        const StorylandNativePoint sight{root.x,root.y,root.z+1.0f};
        // Retail's acquisition ray tests buildings and objects, not other
        // pedestrians or cars (0x1EF590). Include live scripted object COLs.
        if(storylandTraceNativeSegmentCategories(world,eye,sight,true,false)<1.0f)return;
        if(runtime.missionEnvironment)for(const auto& object:runtime.missionEnvironment->objects) {
            if(target.kind==TargetKind::Object&&object.handle==target.handle)continue;
            float fraction=1;StorylandNativePoint normal;
            if(storylandTraceScriptObject(world,object,eye,sight,fraction,normal)&&fraction<1)return;
        }
        candidates.push_back({priority,ambient,score,distance,angle,healthRatio,target,point});
    };
    for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index) {
        const auto& ped=runtime.pedestrians[index];
        if(ped.scriptHandle>=0||ped.health<=0||ped.state==StorylandPedState::Dead||!ped.torsoValid)continue;
        const auto& root=actors[index].position;
        const auto& point=ped.torso;
        int priority=ped.isPolice(world.paths.vcs)?3:
            ped.weapon<runtime.weapons.size()&&runtime.weapons[ped.weapon].nativeId>0?2:0;
        if(ped.hostile&&(ped.state==StorylandPedState::Combat||ped.ranged.active||ped.melee.active))priority=9;
        addCandidate(int(index),{},root,point,priority,ped.health/100.0f,true,false,
            !world.paths.vcs&&targetPressed&&modelOnScreen(int32_t(ped.stats.modelId),root,actors[index].yaw-90.0f));
    }
    if(runtime.missionEnvironment) {
        auto& environment=*runtime.missionEnvironment;
        for(const auto& entry:environment.missionCharacters) {
            const auto& character=entry.second;
            if(!storylandTargetCharacterAllowed(runtime,character))continue;
            const StorylandNativePoint root{character.x,character.y,character.z};
            const auto pose=runtime.missionCombat.find(entry.first);
            if(pose==runtime.missionCombat.end()||!pose->second.anchorsValid)continue;
            const auto& point=pose->second.torso;
            const auto aimed=environment.characterTargets.find(entry.first);
            const bool aimingAtPlayer=aimed!=environment.characterTargets.end()&&
                aimed->second.kind==TargetKind::Character&&aimed->second.handle==0;
            const bool attacking=aimingAtPlayer||(character.objectiveTarget==0&&
                (character.objective==0x00E4||character.objective==0x00E5));
            bool attackingAlly=false;
            if(aimed!=environment.characterTargets.end()&&aimed->second.kind==TargetKind::Character) {
                const auto ally=environment.missionCharacters.find(aimed->second.handle);
                attackingAlly=ally!=environment.missionCharacters.end()&&ally->second.alive&&
                    ally->second.health>0&&ally->second.leader==0;
            }
            // Native priorities 9..0 (0x2D66C8): attacking player, mission
            // threat, direct sight corridor, script-only, mission, object,
            // police, armed, motorcycle occupant, everybody else.
            int priority=attacking?9:character.creationSource==2&&attackingAlly?8:
                character.creationSource==2?5:character.type==6?3:
                character.currentWeapon>0?2:character.vehicle>=0?1:0;
            priority=std::max(priority,std::clamp(character.behavior.targetPriority,0,9));
            addCandidate(-1,{TargetKind::Character,entry.first},root,point,priority,
                character.maximumHealth>0?character.health/character.maximumHealth:0,
                !character.playerFriend||attacking,character.creationSource==2,
                !world.paths.vcs&&targetPressed&&(environment.characterOnScreen?
                    environment.characterOnScreen(entry.first)>0:modelOnScreen(character.model,root,character.heading)));
        }
        for(int32_t handle:environment.targettableObjects) {
            if(handle<0)continue;
            const auto object=std::find_if(environment.objects.begin(),environment.objects.end(),
                [handle](const auto& item){return item.handle==handle;});
            if(object==environment.objects.end()||!object->damage.visible||object->damage.renderDamaged)continue;
            const StorylandNativePoint root{object->x,object->y,object->z};
            addCandidate(-1,{TargetKind::Object,handle},root,root,4,1,true,false,
                !world.paths.vcs&&targetPressed&&(environment.objectOnScreen?environment.objectOnScreen(*object):
                    modelOnScreen(object->model,root,object->heading)));
        }
    }
    const auto selected=std::find_if(candidates.begin(),candidates.end(),[&](const auto& candidate) {
        return sameTarget(candidate.ambient,candidate.target);
    });
    const Candidate* chosen=selected==candidates.end()?nullptr:&*selected;
    if(!chosen&&targetPressed) {
        if(runtime.freeAimActive) {
            const auto& origin=runtime.freeAimOrigin;
            const auto& endpoint=runtime.freeAimTarget;
            const float dx=endpoint.x-origin.x,dy=endpoint.y-origin.y,dz=endpoint.z-origin.z;
            const float length=std::hypot(dx,dy,dz);
            if(std::isfinite(length)&&length>.001f) {
                float bestDistance=std::numeric_limits<float>::infinity();
                float bestDepth=std::numeric_limits<float>::infinity();
                for(const auto& candidate:candidates) {
                    const float x=candidate.position.x-origin.x;
                    const float y=candidate.position.y-origin.y;
                    const float z=candidate.position.z-origin.z;
                    const float depth=(x*dx+y*dy+z*dz)/length;
                    if(depth<0||depth>length+1.0f)continue;
                    const float distance=std::sqrt(std::max(0.0f,x*x+y*y+z*z-depth*depth));
                    if(distance>.65f)continue;
                    if(distance<bestDistance-.001f||
                        (std::fabs(distance-bestDistance)<=.001f&&depth<bestDepth)) {
                        chosen=&candidate;bestDistance=distance;bestDepth=depth;
                    }
                }
            }
        }
        // Native non-firearm targeting first selects the nearest candidate in
        // five metres (0x2D6C80). The firearm bitmap is 0xFFFF80000, IDs19..35.
        const bool closePass=world.paths.vcs&&(weapon.nativeId<19||weapon.nativeId>35);
        if(closePass)for(const auto& candidate:candidates)
            if(candidate.distance<5.0f&&candidate.angle<1.570796327f&&
                (!chosen||candidate.distance<chosen->distance))chosen=&candidate;
        if(!chosen)for(const auto& candidate:candidates)
            if(!chosen||candidate.priority>chosen->priority||
                (candidate.priority==chosen->priority&&candidate.score<chosen->score))chosen=&candidate;
    }
    if(!chosen) {clear();return;}
    runtime.targetedPed=chosen->ambient;runtime.targetPosition=chosen->position;runtime.targetVisible=true;
    if(runtime.missionEnvironment)runtime.missionEnvironment->playerTarget=chosen->target;
    const uint32_t frame=runtime.missionEnvironment?runtime.missionEnvironment->nativeFrameCounter:
        uint32_t(std::max(0.0f,runtime.elapsed)*50.0f);
    runtime.targetIndicator.update(chosen->ambient,uint8_t(chosen->target.kind),chosen->target.handle,
        chosen->healthRatio,frame);
    // One-arm firearms leave the body facing movement; native arm IK aims
    // independently (VCS 0x236E08, LCS 0x34BD18).
    if(!(weapon.nativeFlags&0x80u)||weapon.fireType==0||
        ((runtime.extendedAndSaFeatures||(!runtime.crime.vcs&&runtime.extendedLcsCrouch))&&
            storylandNativeWeaponUsesTwoHands(world.paths.vcs,weapon.nativeId)))
        heading=std::atan2(chosen->position.y-player.y,chosen->position.x-player.x)/radians;
    if(targetPressed&&runtime.targetedPed>=0) {
        StorylandPlayTestSoundEvent draw(StorylandPlayTestSound::Damage);draw.nativeSpeech=0x74;
        storylandStampCharacterSoundEvent(runtime,draw);
        runtime.soundEvents.push_back(draw);
        if(world.paths.vcs) {
            StorylandPlayTestSoundEvent threatened(StorylandPlayTestSound::Damage);
            threatened.nativeSpeech=0x76;threatened.speaker=runtime.targetedPed;
            threatened.position=actors[size_t(runtime.targetedPed)].position;threatened.positioned=true;
            storylandStampCharacterSoundEvent(runtime,threatened);
            runtime.soundEvents.push_back(threatened);
        }
    }
}

void storylandPreparePlayTestTarget(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, const std::vector<StorylandNativeTrafficActor>& pedestrians,
    const StorylandNativePoint& player, float& heading, bool targetHeld) {
    if(runtime.missionEnvironment&&(runtime.missionEnvironment->playerWalkTask.active||
        runtime.missionEnvironment->playerVehicleEntryTarget>=0)) {
        storylandClearPlayTestTarget(runtime);return;
    }
    if (!runtime.playerReaction.active())
        storylandUpdatePlayTestTarget(runtime, world, pedestrians, player, heading, targetHeld);
}

void storylandRestoreScriptProjectiles(StorylandPlayTestRuntime& runtime,float seconds) {
    runtime.projectiles.erase(std::remove_if(runtime.projectiles.begin(),runtime.projectiles.end(),
        [](const auto& projectile){return projectile.scriptSlot>=0;}),runtime.projectiles.end());
    if(!runtime.missionEnvironment||!runtime.crime.vcs)return;
    auto& world=*runtime.missionEnvironment;
    for(size_t slot=0;slot<world.scriptProjectiles.size();++slot) {
        auto& saved=world.scriptProjectiles[slot];
        if(!saved.active)continue;
        const auto weapon=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
            [&](const auto& definition){return definition.nativeId==saved.weaponType;});
        if(weapon==runtime.weapons.end())continue;
        StorylandWeaponProjectile projectile;
        projectile.scriptSlot=int32_t(slot);
        projectile.position={saved.position[0],saved.position[1],saved.position[2]};
        projectile.velocity={saved.velocity[0],saved.velocity[1],saved.velocity[2]};
        projectile.weapon=size_t(weapon-runtime.weapons.begin());
        projectile.kind=saved.weaponType==18?StorylandProjectileKind::Rocket:
            saved.weaponType==17?StorylandProjectileKind::Molotov:
            saved.weaponType==16?StorylandProjectileKind::TearGas:
            saved.weaponType==15?StorylandProjectileKind::SeaMine:
            saved.weaponType==14?StorylandProjectileKind::LandMine:StorylandProjectileKind::Grenade;
        if(storylandProjectileIsMine(projectile.kind))projectile.remaining=1.0f;
        else if(saved.deadline<world.gameMilliseconds)projectile.remaining=0;
        else projectile.remaining=float(saved.deadline-world.gameMilliseconds)*.001f+seconds+.000001f;
        projectile.damage=weapon->damage;
        projectile.radius=weapon->radius;
        projectile.gravity=saved.weaponType!=18&&!saved.settled;
        projectile.age=saved.age;
        projectile.trailTime=saved.trailTime;
        projectile.yaw=saved.yaw;
        projectile.settled=saved.settled;
        if(!saved.sequence) {
            saved.sequence=runtime.nextProjectileSequence++;
            if(!runtime.nextProjectileSequence)runtime.nextProjectileSequence=1;
        } else if(saved.sequence>=runtime.nextProjectileSequence) {
            runtime.nextProjectileSequence=saved.sequence+1;
            if(!runtime.nextProjectileSequence)runtime.nextProjectileSequence=1;
        }
        projectile.sequence=saved.sequence;
        projectile.owner=storylandVcsProjectileOwner(world,saved,runtime.pedestrians.size());
        runtime.projectiles.push_back(projectile);
    }
}

void storylandStoreScriptProjectiles(StorylandPlayTestRuntime& runtime) {
    if(!runtime.missionEnvironment||!runtime.crime.vcs)return;
    for(const auto& projectile:runtime.projectiles) {
        if(projectile.scriptSlot<0||size_t(projectile.scriptSlot)>=
            runtime.missionEnvironment->scriptProjectiles.size())continue;
        auto& saved=runtime.missionEnvironment->scriptProjectiles[size_t(projectile.scriptSlot)];
        saved.active=projectile.remaining>0;
        saved.position={projectile.position.x,projectile.position.y,projectile.position.z};
        saved.velocity={projectile.velocity.x,projectile.velocity.y,projectile.velocity.z};
        saved.age=projectile.age;
        saved.trailTime=projectile.trailTime;
        saved.yaw=projectile.yaw;
        saved.settled=projectile.settled;
    }
}

static void storylandUpdateVcsPoliceHeliWeapon(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,std::vector<StorylandNativeTrafficActor>& pedestrians,
    const std::vector<StorylandNativeTrafficActor>& vehicles,const StorylandNativePoint& player) {
    auto* environment=runtime.missionEnvironment;
    if(!environment||!world.paths.vcs)return;
    const auto definition=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
        [](const auto& weapon){return weapon.nativeId==25&&weapon.fireType==1;});
    if(definition==runtime.weapons.end())return;
    for(size_t slot=0;slot<vehicles.size()&&slot<runtime.vehicles.size();++slot) {
        auto& helicopter=runtime.vehicles[slot];
        if(!helicopter.nativeDefinition.vcsModelLayout||helicopter.nativeDefinition.modelId!=261||
            helicopter.nativeDefinition.nativeType!=4||helicopter.scriptHandle!=-261)continue;
        const uint32_t now=environment->gameMilliseconds;
        const bool playerAirborneVehicle=runtime.occupiedVehicle>=0&&
            size_t(runtime.occupiedVehicle)<runtime.vehicles.size()&&
            runtime.vehicles[size_t(runtime.occupiedVehicle)].nativeDefinition.domain==StorylandVehicleDomain::Air;
        if(!helicopter.aiActive||helicopter.aiMission!=27||!helicopter.hasDriver||
            helicopter.health<=0||helicopter.wrecked||!helicopter.visible||
            int(slot)==runtime.occupiedVehicle||int(slot)==runtime.transitionVehicle||
            helicopter.coopControlled||runtime.wanted<3||runtime.wanted>6||
            runtime.policeIgnorePlayer||runtime.health<=0||runtime.busted||runtime.wastedActive||
            runtime.visibleArea!=0||environment->cutscene.playing||playerAirborneVehicle) {
            helicopter.policeHeliAiming=false;continue;
        }
        auto target=runtime.playerTorsoValid?runtime.playerTorso:chest(player);
        if(runtime.occupiedVehicle>=0&&size_t(runtime.occupiedVehicle)<vehicles.size())
            target=vehicles[size_t(runtime.occupiedVehicle)].position;
        const auto& actor=vehicles[slot];
        const float angle=actor.yaw*radians;
        StorylandNativePoint muzzle{actor.position.x+std::cos(angle)*3.5f,
            actor.position.y+std::sin(angle)*3.5f,actor.position.z-1.0f};
        // Use the live transform when available; the yaw-only fallback keeps
        // the weapon functional while the model has not yet been rendered.
        if(slot<runtime.vehicleWorldMatrices.size()) {
            const auto& frame=runtime.vehicleWorldMatrices[slot];
            if(storylandVcsPoliceHeliMuzzleFrameValid(frame,
                {actor.position.x,actor.position.y,actor.position.z}))
                muzzle={frame[12]+frame[4]*3.5f-frame[8],
                    frame[13]+frame[5]*3.5f-frame[9],frame[14]+frame[6]*3.5f-frame[10]};
        }
        const float horizontal=std::hypot(target.x-actor.position.x,target.y-actor.position.y);
        const float distance=separation(muzzle,target);
        if(!std::isfinite(horizontal)||!std::isfinite(distance)||distance<3.01f||
            horizontal>42.0f||storylandTraceNativeSegment(world,muzzle,target)<.995f) {
            helicopter.policeHeliAiming=false;continue;
        }
        if(!helicopter.policeHeliAiming||now<helicopter.policeHeliLastClock) {
            helicopter.policeHeliAiming=true;helicopter.policeHeliAimSince=now;
            helicopter.policeHeliNextShot=now;
        }
        helicopter.policeHeliLastClock=now;
        if(!storylandVcsPoliceHeliCanShoot(now,helicopter.policeHeliAimSince,
            helicopter.policeHeliNextShot,runtime.wanted))continue;
        const StorylandNativePoint unit{(target.x-muzzle.x)/distance,(target.y-muzzle.y)/distance,
            (target.z-muzzle.z)/distance};
        const StorylandNativePoint start{muzzle.x+unit.x*3.0f,muzzle.y+unit.y*3.0f,muzzle.z+unit.z*3.0f};
        const float offsetX=float(int(environment->nextNativeRandom()&255u)-128)*.02f;
        const float offsetY=float(int(environment->nextNativeRandom()&255u)-128)*.02f;
        const StorylandNativePoint end{target.x+offsetX+unit.x*3.0f,
            target.y+offsetY+unit.y*3.0f,target.z+unit.z*3.0f};
        auto weapon=*definition;weapon.damage=20.0f;
        muzzleFlash(runtime,muzzle,weapon.nativeId,StorylandMuzzleOwner::Vehicle,int32_t(slot));
        applyWeaponSegment(runtime,world,pedestrians,player,muzzle,start,end,weapon,-1,
            nullptr,&muzzle,true,int(slot));
        if(environment->audioSubsystemEnabled&&environment->scriptSounds.size()<4096) {
            StorylandMissionEnvironment::ScriptSound sound;
            sound.id=55;sound.vcsVehicleAudioEvent=55;
            sound.position={actor.position.x,actor.position.y,actor.position.z};
            environment->scriptSounds.push_back(sound);
        }
        const float random=float(environment->nextNativeRandom())*(1.0f/2147483648.0f);
        helicopter.policeHeliNextShot=now+(random<.15f?400u:150u);
    }
}

void storylandUpdatePlayTestWeapons(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    std::vector<StorylandNativeTrafficActor>& actors,const StorylandNativePoint& player,
    float& heading,float cameraYaw,bool targetHeld,bool fire,bool reloadPressed,float seconds,
    const std::vector<StorylandNativeTrafficActor>* vehicles) {
    if(runtime.paused||runtime.pauseMenu||!std::isfinite(seconds)||seconds<=0) return;
    storylandUpdateMilitaryResponse(runtime,&world,player);
    if(world.paths.vcs&&runtime.missionEnvironment&&runtime.occupiedVehicle>=0&&
        size_t(runtime.occupiedVehicle)<runtime.vehicles.size()&&runtime.playerVehicleSeat<0) {
        auto& environment=*runtime.missionEnvironment;
        const auto& vehicle=runtime.vehicles[size_t(runtime.occupiedVehicle)];
        if(environment.inVehicle&&vehicle.nativeDefinition.modelId==275) {
            const auto weapon=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
                [](const auto& value){return value.nativeId==35;});
            const auto rocketWeapon=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
                [](const auto& value){return value.nativeId==18;});
            const bool reactionOwnsHunterInput=(runtime.playerReaction.active()&&
                runtime.playerReaction.action!=StorylandPlayerReaction::CarGesture)||
                environment.playerWalkTask.active||environment.playerVehicleEntryTarget>=0||
                environment.remoteVehicle>=0;
            const bool trigger=runtime.playerControl&&environment.control&&runtime.health>0&&!reactionOwnsHunterInput&&
                (fire||storylandVcsPadState(environment,16)!=0);
            const bool rocketTrigger=runtime.playerControl&&environment.control&&runtime.health>0&&!reactionOwnsHunterInput&&
                storylandVcsPadState(environment,15)!=0;
            if((trigger||rocketTrigger)&&vehicle.scriptHandle<0&&
                environment.acquireOccupiedVehicle) {
                const int32_t handle=environment.acquireOccupiedVehicle();
                if(handle>=0&&vehicle.scriptHandle==handle)
                    environment.occupiedMissionVehicle=handle;
            }
            auto missionVehicle=environment.missionVehicles.find(vehicle.scriptHandle);
            if(missionVehicle!=environment.missionVehicles.end()&&!missionVehicle->second.deleted) {
                const bool rocketReady=StorylandVcsHunterRockets::ready(rocketTrigger,
                    environment.gameMilliseconds,missionVehicle->second.lastHunterShot);
                if(rocketReady) {
                    std::string error;
                    if(rocketWeapon!=runtime.weapons.end()&&StorylandVcsHunterRockets::fire(environment,
                        vehicle.scriptHandle,std::min(seconds,0.1f),error))
                        missionVehicle->second.lastHunterShot=environment.gameMilliseconds;
                } else if(vehicles&&weapon!=runtime.weapons.end()&&StorylandVcsHunterCannon::playerShotReady(
                    trigger,environment.gameMilliseconds,missionVehicle->second.lastHunterShot)) {
                    std::string error;
                    if(storylandFireHunterGun(runtime,world,*vehicles,actors,
                        vehicle.scriptHandle,error,&*weapon))
                        missionVehicle->second.lastHunterShot=environment.gameMilliseconds;
                }
            }
            fire=false;
        }
    }
    if(runtime.missionEnvironment&&runtime.occupiedVehicle>=0&&!runtime.missionEnvironment->playerCanDoDriveBy)fire=false;
    const bool scriptedTaskOwnsInput=runtime.missionEnvironment&&(runtime.missionEnvironment->playerWalkTask.active||
        runtime.missionEnvironment->playerVehicleEntryTarget>=0);
    const bool reactionOwnsInput = (runtime.playerReaction.active() &&
        runtime.playerReaction.action != StorylandPlayerReaction::CarGesture)||scriptedTaskOwnsInput||
        storylandPlayerBulletReactionActive(runtime);
    if (reactionOwnsInput) { fire = false; reloadPressed = false; }
    if(scriptedTaskOwnsInput) {targetHeld=false;storylandClearPlayTestTarget(runtime);}
    storylandUpdateKillFrenzy(runtime);
    if(runtime.missionEnvironment&&runtime.missionEnvironment->remoteVehicle>=0) {
        fire=false;targetHeld=false;reloadPressed=false;
    }
    const float dt=std::min(seconds,0.1f);
    if(!world.paths.vcs)runtime.playerPosition=player;
    if(!world.paths.vcs&&runtime.missionEnvironment&&runtime.missionEnvironment->wantedResetRequested) {
        auto& environment=*runtime.missionEnvironment;
        storylandSetPlayTestWantedLevel(runtime,std::clamp(environment.wanted,0,std::clamp(environment.maxWanted,0,6)));
        environment.wantedResetRequested=false;
    }
    runtime.sniperTraceActive=false;
    updateVcsSniperBullets(runtime,world,actors,player,dt);
    updateEffects(runtime,world,actors,player,dt,vehicles);
    storylandUpdateScriptParticleEmitters(runtime);
    if(runtime.weapons.empty()) {storylandClearPlayTestTarget(runtime);return;}
    if(vehicles)storylandUpdateVcsPoliceHeliWeapon(runtime,world,actors,*vehicles,player);
    if (runtime.missionEnvironment) {
        auto& environment = *runtime.missionEnvironment;
        const bool hidden = runtime.occupiedVehicle >= 0 ||
            runtime.vehicleAction != StorylandPlayTestRuntime::VehicleAction::None ||
            environment.mobilePhones.count(0) != 0;
        if (hidden && environment.playerSavedWeapon == 48 && runtime.equippedWeapon < runtime.weapons.size()) {
            environment.playerSavedWeapon = runtime.weapons[runtime.equippedWeapon].nativeId;
        } else if (!hidden && environment.playerSavedWeapon != 48) {
            for (size_t index = 0; index < runtime.weapons.size() && index < runtime.inventory.size(); ++index) {
                if (runtime.weapons[index].nativeId == environment.playerSavedWeapon && runtime.inventory[index].owned) {
                    runtime.equippedWeapon = index;
                    environment.currentWeapon = environment.playerSavedWeapon;
                    break;
                }
            }
            environment.playerSavedWeapon = 48;
        }
        if ((!world.paths.vcs && environment.lcsExtendedWorld.scriptInputPause) ||
            environment.mobilePhones.count(0)) {
            fire = false;
            reloadPressed = false;
            targetHeld = false;
        }
    }
    const auto advanceMelee=[&](StorylandMeleeAttack& attack,int owner) {
        const bool valid=owner<0?runtime.health>0&&!runtime.playerWater.active&&runtime.occupiedVehicle<0&&!reactionOwnsInput:
            size_t(owner)<actors.size()&&runtime.pedestrians[size_t(owner)].health>0&&!runtime.pedestrians[size_t(owner)].water.active&&
                !storylandPlayerReactionHoldsTarget(runtime.playerReaction, StorylandReactionTargetKind::Ambient,
                    owner, runtime.pedestrians[size_t(owner)].stats.modelId);
        if(!valid) {attack.active=false;attack.recovery=1;return;}
        if(!attack.active) {attack.recovery=std::min(1.0f,attack.recovery+dt);return;}
        if(attack.weapon>=runtime.weapons.size()) {attack.active=false;return;}
        attack.elapsed+=dt;
        if(!attack.contactApplied&&attack.elapsed>=attack.contactTime) {
            attack.contactApplied=true;
            const auto origin=owner<0?player:actors[size_t(owner)].position;
            const float facing=owner<0?heading:actors[size_t(owner)].yaw;
            shoot(runtime,world,actors,player,origin,facing,attack.weapon,owner,attack.target,true,attack.missionTarget);
        }
        if(attack.elapsed>=attack.duration) {attack.active=false;attack.recovery=0;}
    };
    advanceMelee(runtime.playerMelee,-1);
    for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index)
        advanceMelee(runtime.pedestrians[index].melee,int(index));
    if(runtime.equippedWeapon<runtime.weapons.size())
        storylandAdvanceCrouchWeaponPose(runtime,0,runtime.playerCrouchPose,runtime.weapons[runtime.equippedWeapon],fire);
    runtime.inventory.resize(runtime.weapons.size());
    if(runtime.playerReloadWeapon!=SIZE_MAX&&
        runtime.playerReloadWeapon!=runtime.equippedWeapon) {
        // Cancelling a reload by swapping weapons must not leave the old
        // weapon's reload association active when it is equipped again.
        if(runtime.playerReloadWeapon<runtime.inventory.size())
            runtime.inventory[runtime.playerReloadWeapon].reloading=0.0f;
        runtime.playerReloadWeapon=SIZE_MAX;
        runtime.playerReloadClipSeconds=runtime.playerReloadElapsed=0.0f;
    }
    for(size_t index=0;index<runtime.inventory.size();++index) {
        auto& inventory=runtime.inventory[index];
        inventory.cooldown=std::max(0.0f,inventory.cooldown-dt);
        if(inventory.reloading>0) {
            if(index==runtime.equippedWeapon&&runtime.playerReloadWeapon==SIZE_MAX&&
                runtime.weapons[index].fireType) {
                // A pre-update SC63 save may contain only the ammunition timer.
                // Restore the visual clock without replaying the reload sound.
                const float clip=storylandPlayerNativeReloadDuration(runtime,runtime.weapons[index]);
                runtime.playerReloadWeapon=index;
                runtime.playerReloadClipSeconds=clip;
                runtime.playerReloadElapsed=std::max(0.0f,clip-inventory.reloading);
                inventory.reloading=std::max(0.0f,clip-runtime.playerReloadElapsed);
            }
            const float previous=inventory.reloading;
            inventory.reloading=std::max(0.0f,previous-dt);
            if(index==runtime.playerReloadWeapon) {
                runtime.playerReloadElapsed=std::min(runtime.playerReloadClipSeconds,
                    runtime.playerReloadElapsed+dt);
                if(runtime.playerReloadElapsed>=runtime.playerReloadClipSeconds*0.9f)
                    inventory.magazine=std::min(runtime.weapons[index].clipSize,inventory.ammunition);
            }
            if(!inventory.reloading)inventory.magazine=std::min(runtime.weapons[index].clipSize,inventory.ammunition);
        }
    }
    if(runtime.playerReloadWeapon!=SIZE_MAX&&
        (runtime.playerReloadWeapon>=runtime.inventory.size()||
        runtime.inventory[runtime.playerReloadWeapon].reloading<=0)) {
        runtime.playerReloadWeapon=SIZE_MAX;
        runtime.playerReloadClipSeconds=runtime.playerReloadElapsed=0.0f;
    }
    for(auto& trace:runtime.shotTraces) trace.remaining-=dt;
    runtime.shotTraces.erase(std::remove_if(runtime.shotTraces.begin(),runtime.shotTraces.end(),[](const auto& trace){return trace.remaining<=0;}),runtime.shotTraces.end());
    if(runtime.pickupsEnabled) {
        refreshRandomStreetWeapons(runtime,world,player);
        for(auto& pickup:runtime.pickups) {
            pickup.respawn=std::max(0.0f,pickup.respawn-dt);
            if(pickup.ammunition>0&&!pickup.respawn&&runtime.health>0&&
               runtime.occupiedVehicle<0&&separation(player,pickup.position)<1.3f) {
                storylandGiveWeapon(runtime,pickup.weapon,pickup.ammunition);pickup.ammunition=0;
                sound(runtime,StorylandPlayTestSound::WeaponPickup,pickup.position,int(pickup.weapon));
            }
        }
    }
    storylandUpdateMissionCombat(runtime,world,actors,player,dt);
    runtime.pickups.erase(std::remove_if(runtime.pickups.begin(),runtime.pickups.end(),
        [&](const auto& pickup){return pickup.ammunition<=0||separation(player,pickup.position)>220;}),runtime.pickups.end());
    if (!runtime.playerReaction.holding())
        storylandUpdatePlayTestTarget(runtime,world,actors,player,heading,targetHeld);
    updateLcsGunThreats(runtime,world,actors,player,dt);
    const bool playerHitReaction=storylandPlayerBulletReactionActive(runtime);
    if(reloadPressed&&!runtime.playerWater.active&&runtime.playerControl&&!playerHitReaction) storylandReloadWeapon(runtime);
    const bool seatedFire=world.paths.vcs&&runtime.missionEnvironment&&runtime.occupiedVehicle>=0&&
        runtime.vehicleAction==StorylandPlayTestRuntime::VehicleAction::None&&runtime.playerVehicleSeat>=0&&
        runtime.manualAimActive&&runtime.manualAimMode==11&&runtime.playerMuzzleValid&&
        runtime.equippedWeapon<runtime.weapons.size()&&
        storylandPlayerSeatedDriveByVehicle(*runtime.missionEnvironment,runtime.weapons[runtime.equippedWeapon].nativeId);
    if(fire&&!playerHitReaction&&!runtime.playerWater.active&&runtime.entryTransition<0&&runtime.playerCoughTime<=0&&runtime.playerControl&&runtime.health>0&&
        (runtime.occupiedVehicle<0||seatedFire)&&runtime.equippedWeapon<runtime.weapons.size()) {
        const auto& weapon=runtime.weapons[runtime.equippedWeapon];
        auto& inventory=runtime.inventory[runtime.equippedWeapon];
        const int32_t detonatorId=runtime.crime.vcs?36:34;
        if(inventory.cooldown<=0&&inventory.reloading<=0&&
            (weapon.fireType!=2||weapon.nativeId==detonatorId||runtime.projectiles.size()<64)) {
            if(weapon.nativeId==detonatorId&&weapon.fireType!=4) {
                // The retail detonator is a trigger, not a thrown projectile.
                if(runtime.missionEnvironment) {
                    auto& environment=*runtime.missionEnvironment;
                    for(auto& entry:environment.missionVehicles) {
                        auto& car=entry.second;
                        if(car.deleted||car.wrecked||car.bombType!=3||car.bombOwner!=0)continue;
                        std::string error;storylandExplodeScriptVehicle(environment,entry.first,error);
                    }
                }
                inventory.cooldown=std::max(.1f,weapon.interval);
                ++runtime.playerShotSerial;runtime.playerLastShotTime=runtime.elapsed;
            }
            else if(weapon.fireType==4&&weapon.nativeId==(runtime.crime.vcs?38:36)) {
                // The native camera has a zero-sized clip, but consumes one film
                // exposure from total ammunition and reloads between photographs.
                if(inventory.owned&&inventory.ammunition>0&&storylandTakePhotograph(runtime,world)) {
                    --inventory.ammunition;
                    inventory.cooldown=std::max(.1f,weapon.interval);
                    runtime.playerLastShotTime=runtime.elapsed;
                    if(inventory.ammunition>0)inventory.reloading=weapon.reloadSeconds;
                    else {
                        runtime.manualAimActive=false;
                        runtime.manualAimMode=0;
                        if(runtime.missionEnvironment) {
                            const bool resetRequested=runtime.missionEnvironment->manualAim.resetRequested;
                            runtime.missionEnvironment->manualAim={};
                            runtime.missionEnvironment->manualAim.resetRequested=resetRequested;
                        }
                    }
                }
            }
            else if(weapon.fireType&&inventory.magazine<=0) storylandReloadWeapon(runtime);
            else if(!(runtime.manualAimActive&&(runtime.manualAimMode==46||runtime.manualAimMode==47))&&
                storylandPrepareCrouchShot(runtime,0,runtime.playerCrouchPose,weapon)==1) {
                shoot(runtime,world,actors,player,player,heading,runtime.equippedWeapon,-1,runtime.targetedPed);
                if(weapon.fireType)runtime.crime.report(StorylandCrime::Gunshot,player,runtime.elapsed);
                if(weapon.fireType) for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index) {
                    auto& witness=runtime.pedestrians[index];
                    if(witness.health>0&&!witness.zombie.active&&!witness.hostile&&!witness.fortBaxterSoldier&&
                        !witness.isPolice(runtime.crime.vcs)&&separation(player,actors[index].position)<30&&
                        !witness.fleeActive)witness.startFlee(player,4,true);
                }
                inventory.cooldown=weapon.interval;
                if(weapon.fireType) {--inventory.magazine;--inventory.ammunition;}
            }
        }
    }
    storylandUpdatePoliceCrime(runtime,world,actors,player,dt);
    StorylandNativePoint arrestPosition=player;
    const bool playerKnockedDown=runtime.playerBulletReaction.knockdown&&
        !runtime.playerBulletReaction.dying&&runtime.elapsed>=runtime.playerBulletReaction.startedAt&&
        (runtime.playerBulletReaction.fall.grounded||runtime.playerBulletReaction.fall.recovering);
    const bool resistingArrest=fire&&!playerKnockedDown;
    bool arrestEligible=runtime.occupiedVehicle<0?
        playerKnockedDown||(runtime.grounded&&runtime.movementSpeed<0.5f):false;
    if(runtime.occupiedVehicle>=0&&size_t(runtime.occupiedVehicle)<runtime.movementCars.size()&&
        size_t(runtime.occupiedVehicle)<runtime.vehicles.size()) {
        const auto& vehicle=runtime.vehicles[size_t(runtime.occupiedVehicle)];
        const auto& obstacle=runtime.movementCars[size_t(runtime.occupiedVehicle)];
        const auto seat=vehicle.nativeDefinition.seatPosition(0);
        const float angle=obstacle.yaw*radians;
        const float side=std::max(vehicle.halfWidth,std::fabs(seat[0]))+0.35f;
        arrestPosition={obstacle.position.x+std::cos(angle)*seat[1]-std::sin(angle)*side,
            obstacle.position.y+std::sin(angle)*seat[1]+std::cos(angle)*side,obstacle.position.z};
        arrestEligible=obstacle.collisionEnabled&&vehicle.nativeDefinition.numberOfDoors>0&&std::fabs(vehicle.speed)<0.35f&&
            runtime.vehicleAction==StorylandPlayTestRuntime::VehicleAction::None;
    }
    auto contactObstacles=storylandPedContactObstacles(runtime);
    const auto* gangGame=runtime.missionEnvironment;
    const bool gangConflict=gangGame&&runtime.extendedHaitianGang.available&&
        storylandVcsTurfUnlocked(world.paths.vcs,runtime.extendedAndSaFeatures,
            gangGame->passedMissions,gangGame->empireSites);
    const auto cuban=[&](const auto& ped) {
        if(ped.stats.pedType==11)return true;
        if(!gangGame)return false;
        const auto& models=gangGame->gangs[4].pedModels;
        return ped.stats.modelId>0&&std::find(models.begin(),models.end(),
            int32_t(ped.stats.modelId))!=models.end();
    };
    const auto haitian=[](const auto& ped) {
        return ped.extendedGangIdentity==StorylandExtendedHaitianGang::populationKind&&
            ped.stats.pedType==StorylandExtendedHaitianGang::pedestrianType;
    };
    for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index) {
        auto& ped=runtime.pedestrians[index];
        // Every early exit releases the autonomous stance. This includes
        // lost targets, disabled weapon AI, and reactions that own the ped.
        struct AmbientCrouchGuard {
            StorylandRuntimePed& ped;
            float dt;
            bool keep=false;
            ~AmbientCrouchGuard() {
                if(keep)return;
                ped.extendedCrouch=false;
                ped.extendedCrouchBlend=std::max(0.0f,ped.extendedCrouchBlend-4.0f*dt);
                if(ped.extendedCrouchBlend==0)ped.extendedCrouchTime=0;
            }
        } crouchGuard{ped,dt};
        ped.combatSpeed=0;
        if(ped.renderAlpha==0||(ped.scriptHandle>=0&&runtime.missionEnvironment&&
            runtime.missionEnvironment->missionCharacters.count(ped.scriptHandle)))continue;
        if(ped.zombie.active) {
            ped.melee={};ped.ranged={};ped.fireFlash=0;ped.reloading=0;
            continue;
        }
        ped.fireFlash=std::max(0.0f,ped.fireFlash-dt);
        ped.weaponCooldown=std::max(0.0f,ped.weaponCooldown-dt);
        if(ped.health>0&&!ped.water.active&&
            (ped.bulletReaction.knockdown||ped.bulletReaction.active(runtime.elapsed))) {
            ped.melee={};ped.ranged={};ped.contact.clear();ped.fireFlash=0;
            continue;
        }
        if (storylandPlayerReactionHoldsTarget(runtime.playerReaction,
            StorylandReactionTargetKind::Ambient, int32_t(index), ped.stats.modelId)) {
            ped.melee = {};
            ped.ranged = {};
            continue;
        }
        const bool nativeReload=!world.paths.vcs&&ped.ranged.active&&
            (ped.ranged.reloadPending||ped.ranged.reloading);
        if(ped.reloading>0||nativeReload) {
            if(nativeReload)ped.reloading-=dt;
            else {
                ped.reloading=std::max(0.0f,ped.reloading-dt);
                if(!ped.reloading&&ped.weapon<runtime.weapons.size())ped.magazine=std::min(runtime.weapons[ped.weapon].clipSize,ped.ammunition);
            }
        }
        int rival=storylandVcsPoliceFindAssailant(runtime,index,actors.size());
        const int32_t missionRival=storylandVcsPoliceFindMissionAssailant(runtime,index);
        if(rival<0&&missionRival<0&&gangConflict&&!ped.personallyProvoked&&!ped.policePursuing&&!ped.fortBaxterSoldier&&ped.scriptHandle<0) {
            float nearest=30.0f;
            for(size_t other=0;other<actors.size()&&other<runtime.pedestrians.size();++other) {
                const auto& target=runtime.pedestrians[other];
                if(other==index||target.health<=0||target.scriptHandle>=0||target.water.active||target.zombie.active||
                    !storylandExtendedGangRivals(true,cuban(ped),haitian(ped),cuban(target),haitian(target)))continue;
                const float distance=separation(actors[index].position,actors[other].position);
                if(distance<nearest&&visible(world,actors[index].position,actors[other].position)) {
                    nearest=distance;rival=int(other);
                }
            }
        }
        const bool fleeingFire=rival>=0&&ped.fleeActive&&std::any_of(runtime.effects.begin(),runtime.effects.end(),
            [&](const auto& effect) {
                return effect.kind==StorylandParticleKind::Flame&&effect.remaining>0&&effect.damage>0&&
                    separation(ped.fleePosition,effect.position)<.01f;
            });
        if(rival>=0&&ped.fleeActive&&!ped.fleeFromPlayer&&!fleeingFire&&
            separation(ped.fleePosition,actors[size_t(rival)].position)<2.0f)
            ped.clearFlee();
        if((rival>=0||missionRival>=0)&&!ped.fleeActive)ped.state=StorylandPedState::Combat;
        else if(!ped.hostile&&ped.state==StorylandPedState::Combat&&!ped.melee.active&&!ped.ranged.active)
            ped.state=StorylandPedState::Walk;
        if(ped.water.active||ped.health<=0||ped.coughTime>0||!runtime.weaponAi||(!ped.hostile&&rival<0&&missionRival<0)||
            ped.fleeActive||ped.threatWaiting||ped.weapon>=runtime.weapons.size()||(runtime.health<=0&&rival<0&&missionRival<0)) {
            if(world.paths.vcs)ped.ranged={};else cancelLcsRangedAttack(runtime,ped,dt);
            continue;
        }
        const auto& weapon=runtime.weapons[ped.weapon];
        if(ped.ranged.active&&ped.ranged.weapon!=ped.weapon)ped.ranged={};
        const bool pursuingPlayer=rival<0&&missionRival<0&&ped.policePursuing;
        const bool arrestApproach=pursuingPlayer&&arrestEligible&&!resistingArrest&&
            (weapon.fireType==0||playerKnockedDown||runtime.occupiedVehicle>=0);
        if(arrestApproach) {
            ped.melee={};ped.ranged={};ped.fireFlash=0;
            if(separation(actors[index].position,arrestPosition)<1.4f)continue;
        }
        StorylandNativePoint pursuitPosition=rival>=0?actors[size_t(rival)].position:arrestApproach?arrestPosition:player;
        if(missionRival>=0&&runtime.missionEnvironment) {
            const auto found=runtime.missionEnvironment->missionCharacters.find(missionRival);
            if(found!=runtime.missionEnvironment->missionCharacters.end())
                pursuitPosition={found->second.x,found->second.y,found->second.z};
        }
        const bool canSee=(!pursuingPlayer||ped.policeCanSeePlayer)&&
            visible(world,actors[index].position,pursuitPosition);
        if(pursuingPlayer&&!canSee&&ped.policeLastKnownPositionValid)
            pursuitPosition=ped.policeLastKnownPosition;
        const float distance=separation(actors[index].position,pursuitPosition);
        const bool arrivedAtSearch=pursuingPlayer&&!canSee&&distance<=1.0f;
        StorylandNativePoint chasePosition=pursuitPosition;
        uint32_t chaseNode=UINT32_MAX;
        if(pursuingPlayer&&!canSee&&!arrivedAtSearch&&
            actors[index].node<world.paths.nodes.size()) {
            // A blocked line to the player is not a traversable foot route.
            // Use the live pedestrian network's visible next connection before
            // attempting the direct approach again. Mission actors remain on
            // their own scripted routes.
            const auto& node=world.paths.nodes[actors[index].node];
            if(!node.vehicle&&!node.water&&!node.disabled&&
                separation(node.position,actors[index].position)<20.0f) {
                float best=std::numeric_limits<float>::infinity();
                for(const auto& link:node.links) {
                    if(link.target>=world.paths.nodes.size())continue;
                    const auto& next=world.paths.nodes[link.target];
                    if(next.vehicle||next.water||next.disabled)continue;
                    const float step=separation(next.position,actors[index].position);
                    if(step<1.0f||step>23.0f||
                        !visible(world,actors[index].position,next.position))continue;
                    const float score=separation(next.position,pursuitPosition)+step*.35f;
                    if(score<best) {best=score;chasePosition=next.position;chaseNode=link.target;}
                }
            }
        }
        if(distance<90)actors[index].yaw=std::atan2(chasePosition.y-actors[index].position.y,
            chasePosition.x-actors[index].position.x)/radians;
        const auto chaseStart=actors[index].position;
        float requestedTravel=0;
        if(distance<90&&!arrivedAtSearch&&
            (distance>(arrestApproach?1.0f:std::min(weapon.range*0.65f,12.0f))||!canSee)) {
            const auto previous=actors[index].position;
            const float angle=std::atan2(chasePosition.y-previous.y,chasePosition.x-previous.x);
            float movementSpeed=1.25f;
            if(ped.stats.valid&&runtime.missionEnvironment&&runtime.missionEnvironment->animationMovementSpeed) {
                const int32_t group=storylandNativePedWeaponLocomotionGroup(world.paths.vcs,
                    weapon.nativeId,ped.stats.animationGroup);
                const float nativeSpeed=runtime.missionEnvironment->animationMovementSpeed(group,0);
                if(std::isfinite(nativeSpeed)&&nativeSpeed>0)movementSpeed=nativeSpeed;
            }
            if(pursuingPlayer)
                movementSpeed=std::max(movementSpeed,distance>4.5f?3.0f:1.55f);
            requestedTravel=movementSpeed*dt;
            for(float offset:{0.0f,0.8f,-0.8f}) {
                const StorylandNativePoint candidate{previous.x+std::cos(angle+offset)*movementSpeed*dt,
                    previous.y+std::sin(angle+offset)*movementSpeed*dt,previous.z};
                if(storylandTraceNativeSegment(world,chest(previous),chest(candidate))<1) continue;
                const StorylandNativePoint top{candidate.x,candidate.y,candidate.z+0.7f},bottom{candidate.x,candidate.y,candidate.z-1.5f};
                uint8_t surface=0;
                const float floor=storylandTraceNativeGround(world,top,bottom,surface);
                if(floor>=1) continue;
                const StorylandNativePoint grounded{candidate.x,candidate.y,top.z+(bottom.z-top.z)*floor};
                const auto movement=storylandMoveAmbientPedestrian(runtime,world,index,previous,grounded,
                    actors[index].yaw,dt,contactObstacles);
                if(!movement.clear||separation(previous,movement.position)<.001f)continue;
                actors[index].position=movement.position;
                if(index<runtime.movementPedestrians.size())runtime.movementPedestrians[index]=movement.position;
                ped.combatSpeed=separation(previous,actors[index].position)/dt;
                if(chaseNode<world.paths.nodes.size()&&
                    separation(actors[index].position,world.paths.nodes[chaseNode].position)<1.5f) {
                    actors[index].previousNode=actors[index].node;
                    actors[index].node=chaseNode;
                    actors[index].nextNode=chaseNode;
                }
                break;
            }
        }
        storylandRecordPedContactFrame(ped.contact,requestedTravel,
            separation(chaseStart,actors[index].position),dt,runtime.missionEnvironment?
                runtime.missionEnvironment->nativeFrameCounter:runtime.fallenWorldFrame);
        storylandPublishPedContact(contactObstacles,uint64_t(index)+1,actors[index].position,ped.contact);
        const bool canAttack=!arrestApproach&&distance<=std::min(weapon.range,65.0f)&&canSee;
        const float duckDuration=runtime.extendedAndSaFeatures&&world.paths.vcs&&runtime.missionEnvironment&&runtime.missionEnvironment->animationDuration?
            runtime.missionEnvironment->animationDuration(0,155):0;
        const float shotDuration=runtime.extendedAndSaFeatures&&world.paths.vcs&&runtime.missionEnvironment&&runtime.missionEnvironment->animationDuration?
            runtime.missionEnvironment->animationDuration(weapon.animationGroup,215):0;
        ped.extendedCrouch=runtime.extendedAndSaFeatures&&world.paths.vcs&&ped.isPolice(world.paths.vcs)&&canAttack&&
            ped.health>0&&!ped.water.active&&!ped.zombie.active&&!ped.serviceVehicleIdentity&&!ped.fleeActive&&
            !ped.threatWaiting&&ped.coughTime<=0&&ped.state==StorylandPedState::Combat&&weapon.fireType!=0&&(weapon.nativeFlags&0x20000u)!=0&&ped.combatSpeed<.15f&&
            distance>5.0f&&distance<20.0f&&std::isfinite(duckDuration)&&duckDuration>0&&duckDuration<60&&
            std::isfinite(shotDuration)&&shotDuration>0&&shotDuration<60&&
            std::isfinite(weapon.crouchLoopStart)&&std::isfinite(weapon.crouchFireTime)&&
            weapon.crouchLoopStart>=0&&weapon.crouchLoopStart<shotDuration&&
            weapon.crouchFireTime>=weapon.crouchLoopStart&&weapon.crouchFireTime<shotDuration;
        if(ped.extendedCrouch) {
            crouchGuard.keep=true;
            ped.extendedCrouchBlend=std::min(1.0f,ped.extendedCrouchBlend+4.0f*dt);
            ped.extendedCrouchTime+=dt;
        }
        LcsRangedAnimation timing;
        const bool nativeRanged=!world.paths.vcs&&lcsRangedAnimation(runtime,ped,weapon,timing);
        if(nativeRanged) {
            auto& attack=ped.ranged;
            const auto reloadAnimation=[&]() {
                attack.reloading=true;attack.reloadPending=false;attack.animationId=203;
                attack.time=0;attack.blend=0;
            };
            if(!canAttack&&!attack.reloading) {cancelLcsRangedAttack(runtime,ped,dt);continue;}
            attack.cancelling=false;
            if(!attack.active) {
                if(ped.weaponCooldown>0)continue;
                if(ped.ammunition<=0) {ped.weapon=0;continue;}
                attack.active=true;attack.weapon=ped.weapon;attack.animationId=timing.fireId;
                attack.time=0;attack.blend=0;ped.burstShots=0;
                if(ped.magazine<=0) {
                    ped.reloading=weapon.reloadSeconds;attack.reloadPending=true;
                    if(timing.reloadDuration>0)reloadAnimation();
                    else if(weapon.nativeId!=33)attack.time=timing.loopEnd;
                }
                continue;
            }
            attack.blend=std::min(1.0f,attack.blend+8.0f*dt);
            if(attack.reloading) {
                if(timing.reloadDuration<=0) {attack={};continue;}
                const float previous=attack.time;
                attack.time=std::min(timing.reloadDuration,attack.time+dt);
                if(attack.blend>.2f&&previous<timing.reloadSoundTime&&attack.time>=timing.reloadSoundTime)
                    sound(runtime,StorylandPlayTestSound::WeaponReload,actors[index].position,int(ped.weapon),int(index));
                if(ped.magazine<=0&&ped.reloading<0&&attack.time>=timing.reloadDuration*0.9f) {
                    ped.magazine=std::min(weapon.clipSize,ped.ammunition);
                    ped.reloading=0;
                }
                if(attack.time>=timing.reloadDuration&&ped.magazine>0) {
                    attack.reloading=false;attack.animationId=timing.fireId;
                    attack.time=(weapon.nativeFlags&0x800u)?std::max(0.0f,timing.loopStart-0.04f):0;
                    attack.blend=0;
                    if(!canAttack)attack={};
                }
                continue;
            }
            if(attack.reloadPending&&ped.reloading<0) {
                ped.magazine=std::min(weapon.clipSize,ped.ammunition);ped.reloading=0;
                attack.reloadPending=false;
                if(attack.time>=timing.loopEnd) {attack.time=timing.loopStart;continue;}
            }
            const float previous=attack.time;
            attack.time=std::min(timing.duration,attack.time+dt);
            if(attack.time>timing.fireTime&&previous<=timing.fireTime&&!attack.reloadPending&&ped.magazine>0) {
                const float aimError=ped.isPolice(world.paths.vcs)?0.0f:
                    (randomUnit(runtime)-0.5f)*std::clamp(distance*0.2f,1.0f,12.0f)*
                    storylandSaCrouchSpreadScale(world.paths.vcs ? runtime.extendedAndSaFeatures : runtime.extendedLcsCrouch,
                        ped.extendedCrouch);
                shoot(runtime,world,actors,player,actors[index].position,actors[index].yaw+aimError,ped.weapon,int(index),rival,false,missionRival);
                ped.fireFlash=0.12f;--ped.magazine;
                if(ped.ammunition<25000)--ped.ammunition;
                if(ped.magazine==0&&ped.ammunition>0) {
                    attack.reloadPending=true;ped.reloading=weapon.reloadSeconds;
                }
            }
            if(attack.time>timing.loopEnd||attack.time>=timing.duration) {
                if(ped.ammunition<=0) {attack={};ped.weapon=0;}
                else if(attack.reloadPending) {
                    if(timing.reloadDuration>0)reloadAnimation();
                    // CPed::Attack keeps the Minigun's fire loop running while
                    // its weapon refill timer is active, although empty fire
                    // marker crossings still cannot discharge a round.
                    else attack.time=weapon.nativeId==33?timing.loopStart:timing.loopEnd;
                } else attack.time=timing.loopStart;
            }
            continue;
        }
        ped.ranged={};
        if(!canAttack)continue;
        actors[index].yaw=std::atan2(pursuitPosition.y-actors[index].position.y,pursuitPosition.x-actors[index].position.x)/radians;
        if(ped.reloading>0)continue;
        if(ped.weaponCooldown>0) continue;
        if(weapon.fireType&&ped.magazine<=0) {
            if(ped.ammunition<=0) {ped.weapon=0;continue;}
            ped.reloading=weapon.reloadSeconds;continue;
        }
        const float error=ped.isPolice(world.paths.vcs)?0.0f:
            (randomUnit(runtime)-0.5f)*std::clamp(distance*0.2f,1.0f,12.0f)*
            storylandSaCrouchSpreadScale(world.paths.vcs ? runtime.extendedAndSaFeatures : runtime.extendedLcsCrouch,
                ped.extendedCrouch);
        shoot(runtime,world,actors,player,actors[index].position,actors[index].yaw+error,ped.weapon,int(index),rival,false,missionRival);
        ++ped.burstShots;
        const bool automatic=weapon.fireType==1&&weapon.clipSize>8;
        if(automatic&&ped.burstShots<3)ped.weaponCooldown=weapon.interval;
        else {ped.burstShots=0;ped.weaponCooldown=weapon.interval+0.15f+randomUnit(runtime)*0.35f;}
        if(gangGame)ped.weaponCooldown/=storylandExtendedEmpireFireRate(world.paths.vcs,
            runtime.extendedAndSaFeatures,*gangGame,ped.stats.pedType,actors[index].position.x,
            actors[index].position.y,player.x,player.y);
        ped.fireFlash=0.12f;
        if(weapon.fireType) {--ped.magazine;--ped.ammunition;}
    }
    storylandUpdateVcsVehicleSeaMines(runtime,world);
    storylandRestoreScriptProjectiles(runtime,dt);
    for(auto& projectile:runtime.projectiles) {
        const bool mine=storylandProjectileIsMine(projectile.kind);
        projectile.age+=dt;projectile.trailTime+=dt;
        if(!mine)projectile.remaining-=dt;
        if(mine&&projectile.settled) {
            float surface=0;
            const bool afloat=projectile.kind==StorylandProjectileKind::SeaMine&&
                storylandNativeWaterHeight(world,projectile.position.x,projectile.position.y,surface)&&
                std::abs(projectile.position.z-surface)<.2f;
            uint8_t material=0;
            const StorylandNativePoint top{projectile.position.x,projectile.position.y,projectile.position.z+.05f};
            const StorylandNativePoint bottom{top.x,top.y,projectile.position.z-.2f};
            const bool supported=afloat||storylandTraceNativeGround(world,top,bottom,material)<1;
            if(!supported) {projectile.settled=false;projectile.gravity=true;}
            else {
                if((afloat||projectile.kind==StorylandProjectileKind::LandMine)&&
                    mineTouchesActor(runtime,world,actors,player,projectile)) {
                    explode(runtime,actors,world,player,projectile);projectile.remaining=0;
                }
                continue;
            }
        }
        const auto previous=projectile.position;
        float movementSeconds=dt,rocketGrowth=1;
        if(projectile.scriptSlot>=0&&projectile.kind==StorylandProjectileKind::Rocket)
            movementSeconds=storylandVcsRocketStep(dt,rocketGrowth);
        const StorylandNativePoint next{previous.x+projectile.velocity.x*movementSeconds,
            previous.y+projectile.velocity.y*movementSeconds,
            previous.z+projectile.velocity.z*movementSeconds-0.5f*(projectile.gravity?runtime.gravity:0.0f)*dt*dt};
        projectile.velocity.x*=rocketGrowth;
        projectile.velocity.y*=rocketGrowth;
        projectile.velocity.z*=rocketGrowth;
        if(projectile.gravity)projectile.velocity.z-=runtime.gravity*dt;
        float hit=storylandTraceNativeSegment(world,previous,next);
        const auto contact=[&](const StorylandNativePoint& center,float radius=0.5f) {
            const float dx=next.x-previous.x,dy=next.y-previous.y,dz=next.z-previous.z;
            const float length=dx*dx+dy*dy+dz*dz;
            const float ox=previous.x-center.x,oy=previous.y-center.y,oz=previous.z-center.z;
            const float c=ox*ox+oy*oy+oz*oz-radius*radius;
            if(c<=0) {hit=0;return;}
            if(length<1e-8f)return;
            const float b=ox*dx+oy*dy+oz*dz,discriminant=b*b-length*c;
            if(discriminant<0)return;
            const float fraction=(-b-std::sqrt(discriminant))/length;
            if(fraction>=0&&fraction<hit)hit=fraction;
        };
        if(projectile.owner>=0&&runtime.health>0)
            contact(runtime.playerTorsoValid?runtime.playerTorso:chest(player));
        for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index)
            if(int(index)!=projectile.owner&&runtime.pedestrians[index].health>0)contact(chest(actors[index].position));
        if(runtime.missionEnvironment) {
            for(const auto& entry:runtime.missionEnvironment->missionCharacters) {
                const auto& character=entry.second;
                if(projectile.owner==int(runtime.pedestrians.size())+entry.first||!character.alive||!character.visible||character.health<=0)continue;
                const auto pose=runtime.missionCombat.find(entry.first);
                if(character.crouch.active&&(pose==runtime.missionCombat.end()||!pose->second.anchorsValid))continue;
                contact(pose!=runtime.missionCombat.end()&&pose->second.anchorsValid?
                    pose->second.torso:chest({character.x,character.y,character.z}));
            }
            StorylandNativePoint normal{};
            for(const auto& object:runtime.missionEnvironment->objects)
                storylandTraceScriptObject(world,object,previous,next,hit,normal);
        }
        StorylandNativePoint vehicleNormal{};
        const int projectileSourceVehicle=runtime.missionEnvironment?
            storylandVcsProjectileSourceVehicle(*runtime.missionEnvironment,projectile.scriptSlot,runtime.vehicles):-1;
        traceMissionVehicles(runtime,previous,next,hit,vehicleNormal,projectileSourceVehicle);
        for(size_t index=0;index<runtime.vehiclePositions.size()&&index<runtime.vehicles.size();++index) {
            const auto& vehicle=runtime.vehicles[index];
            if(int(index)!=projectileSourceVehicle&&!vehicle.craneCarried&&vehicle.collisionEnabled&&!vehicle.nativeDefinition.collision)
                contact(chest(runtime.vehiclePositions[index]),std::clamp(vehicle.halfWidth,0.5f,2.0f));
        }
        const float segmentLength=separation(previous,next);
        const float safeHit=hit<1?std::max(0.0f,hit-0.025f/std::max(0.025f,segmentLength)):hit;
        projectile.position={previous.x+(next.x-previous.x)*safeHit,previous.y+(next.y-previous.y)*safeHit,previous.z+(next.z-previous.z)*safeHit};
        float water=0;
        if(storylandNativeWaterHeight(world,projectile.position.x,projectile.position.y,water)&&projectile.position.z<=water) {
            if(projectile.kind==StorylandProjectileKind::SeaMine) {
                projectile.position.z=water;projectile.velocity={};projectile.gravity=false;projectile.settled=true;
                if(mineTouchesActor(runtime,world,actors,player,projectile)) {
                    explode(runtime,actors,world,player,projectile);projectile.remaining=0;
                }
                continue;
            }
            if(projectile.kind==StorylandProjectileKind::Rocket)explode(runtime,actors,world,player,projectile);
            for(int index=0;index<8;++index)particle(runtime,StorylandParticleKind::Water,
                {projectile.position.x,projectile.position.y,water},{randomUnit(runtime)-0.5f,randomUnit(runtime)-0.5f,1+randomUnit(runtime)*2},0.12f,0.8f);
            projectile.remaining=0;continue;
        }
        if(nativeTearGas(runtime,projectile)&&projectile.age>=.5f&&projectile.remaining>0&&
            std::none_of(runtime.effects.begin(),runtime.effects.end(),
                [&](const auto& effect){return effect.sourceProjectile==projectile.sequence;})&&runtime.effects.size()<256) {
            StorylandRuntimeEffect effect;
            effect.position=projectile.position;effect.position.z+=.08f;
            effect.kind=StorylandParticleKind::Gas;effect.remaining=projectile.remaining;
            effect.radius=6;effect.damage=50;effect.owner=projectile.owner;effect.nativeWeapon=16;
            effect.betaFeature=projectile.betaFeature;effect.sourceProjectile=projectile.sequence;
            runtime.effects.push_back(effect);
        }
        if(projectile.trailTime>=0.025f) {
            const int count=std::clamp(int(projectile.trailTime/0.025f),1,6);projectile.trailTime=0;
            for(int index=0;index<count;++index) {
                const float fraction=float(index+1)/count;
                StorylandNativePoint point{previous.x+(projectile.position.x-previous.x)*fraction,
                    previous.y+(projectile.position.y-previous.y)*fraction,previous.z+(projectile.position.z-previous.z)*fraction};
                if(projectile.kind==StorylandProjectileKind::Rocket) {
                    particle(runtime,StorylandParticleKind::Smoke,point,{0,0,0.2f},0.18f,1.5f);
                    particle(runtime,StorylandParticleKind::Flame,point,{0,0,0},0.12f,0.12f);
                } else if(projectile.kind==StorylandProjectileKind::Molotov)
                    particle(runtime,StorylandParticleKind::Flame,point,{0,0,0.4f},0.15f,0.35f);
            }
        }
        const bool impact=hit<1;
        if(!mine&&((projectile.kind==StorylandProjectileKind::Rocket&&impact)||
            (projectile.kind==StorylandProjectileKind::Molotov&&impact&&projectile.age>=0.1f)||projectile.remaining<=0)) {
            if(projectile.kind==StorylandProjectileKind::Molotov&&runtime.missionEnvironment&&
                runtime.missionEnvironment->createScriptExplosion)explode(runtime,actors,world,player,projectile);
            else if(projectile.kind==StorylandProjectileKind::Molotov||
                (projectile.kind==StorylandProjectileKind::TearGas&&!nativeTearGas(runtime,projectile))) {
                if(runtime.effects.size()<256) {
                    StorylandRuntimeEffect effect;effect.position=projectile.position;effect.position.z+=0.08f;
                    const bool gas=projectile.kind==StorylandProjectileKind::TearGas;
                    effect.kind=gas?StorylandParticleKind::Gas:StorylandParticleKind::Flame;effect.remaining=gas?18.0f:8.0f;
                    effect.radius=std::clamp(projectile.radius,1.5f,5.0f);
                    effect.damage=gas?2.0f:12.0f;effect.owner=projectile.owner;
                    effect.nativeWeapon=projectile.weapon<runtime.weapons.size()?runtime.weapons[projectile.weapon].nativeId:17;
                    effect.betaFeature=projectile.betaFeature;
                    runtime.effects.push_back(effect);
                }
            } else if(projectile.kind!=StorylandProjectileKind::TearGas)explode(runtime,actors,world,player,projectile);
            projectile.remaining=0;
        } else if(impact) {
            // Timed grenades/canisters bounce; contact does not trigger their fuse.
            auto& velocity=projectile.velocity;
            bool reflected=false;
            if(storylandTraceNativeSegment(world,previous,{next.x,previous.y,previous.z})<1) {velocity.x*=-0.45f;reflected=true;}
            if(storylandTraceNativeSegment(world,previous,{previous.x,next.y,previous.z})<1) {velocity.y*=-0.45f;reflected=true;}
            if(storylandTraceNativeSegment(world,previous,{previous.x,previous.y,next.z})<1) {
                velocity.z=std::fabs(velocity.z)*0.45f;projectile.position.z+=0.03f;reflected=true;
            }
            if(!reflected) {velocity.x*=-0.45f;velocity.y*=-0.45f;velocity.z=std::fabs(velocity.z)*0.35f;}
            velocity.x*=0.8f;velocity.y*=0.8f;
            if(std::fabs(velocity.z)<0.35f)velocity.z=0;
            if(mine&&std::hypot(velocity.x,std::hypot(velocity.y,velocity.z))<1) {
                uint8_t material=0;StorylandNativePoint normal{};
                const StorylandNativePoint top{projectile.position.x,projectile.position.y,projectile.position.z+.1f};
                const StorylandNativePoint bottom{top.x,top.y,projectile.position.z-.2f};
                const float ground=storylandTraceNativeGround(world,top,bottom,material,&normal);
                if(ground<1&&normal.z>.4f) {
                    projectile.position.z=top.z+(bottom.z-top.z)*ground+.025f;
                    projectile.velocity={};projectile.gravity=false;projectile.settled=true;
                    if(projectile.kind==StorylandProjectileKind::LandMine&&mineTouchesActor(runtime,world,actors,player,projectile)) {
                        explode(runtime,actors,world,player,projectile);projectile.remaining=0;
                    }
                }
            }
        }
    }
    storylandStoreScriptProjectiles(runtime);
    runtime.projectiles.erase(std::remove_if(runtime.projectiles.begin(),runtime.projectiles.end(),[](const auto& projectile){return projectile.remaining<=0;}),runtime.projectiles.end());
    int arrestOfficer=-1;
    if(runtime.wanted>0&&!runtime.policeIgnorePlayer&&runtime.health>0&&
        runtime.playerControl&&arrestEligible&&!runtime.playerWater.active&&
        !runtime.playerMelee.active&&!resistingArrest) {
        for(size_t index=0;index<actors.size()&&index<runtime.pedestrians.size();++index) {
            if(storylandPoliceOfficerCanArrest(runtime,index)&&
                separation(arrestPosition,actors[index].position)<1.4f&&visible(world,arrestPosition,actors[index].position)) {
                arrestOfficer=int(index);break;
            }
        }
    }
    if(arrestOfficer!=runtime.arrestOfficer)runtime.arrestProgress=0;
    runtime.arrestOfficer=arrestOfficer;
    runtime.arrestProgress=arrestOfficer>=0?runtime.arrestProgress+dt:0.0f;
    if(runtime.arrestProgress>=1.0f) {
        runtime.arrestWanted=runtime.wanted;
        runtime.busted=true;
        runtime.playerControl=false;
        runtime.arrestProgress=0;
    }
    storylandUpdateKillFrenzy(runtime);
}

bool storylandFireHunterGun(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& native,
    const std::vector<StorylandNativeTrafficActor>& cars,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,int32_t handle,std::string& error,
    const StorylandWeaponDefinition* nativeWeapon) {
    if(!runtime.missionEnvironment) {error="Hunter cannon requires a live mission environment.";return false;}
    auto& world=*runtime.missionEnvironment;
    const auto vehicle=world.missionVehicles.find(handle);
    const int nativeId=nativeWeapon?nativeWeapon->nativeId:37;
    const auto loadedWeapon=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
        [nativeId](const auto& value){return value.nativeId==nativeId;});
    const auto* weapon=nativeWeapon?nativeWeapon:loadedWeapon==runtime.weapons.end()?nullptr:&*loadedWeapon;
    std::array<float,16> matrix{};
    if(vehicle==world.missionVehicles.end()||vehicle->second.deleted||!weapon||
        (weapon->nativeId!=35&&weapon->nativeId!=37)||
        !world.vehicleWorldMatrix||!world.vehicleWorldMatrix(handle,matrix)||
        !std::isfinite(weapon->range)||weapon->range<=0||!std::isfinite(weapon->damage)||weapon->damage<=0) {
        error="Hunter cannon requires native weapon37 and the firing vehicle's live matrix.";return false;
    }
    // CHeli initialization0x2EBA4C: local muzzle(0,4.8,-1.3).
    // Script handler0x3D63AC adds one native step of vehicle motion.
    std::array<float,3> velocity=vehicle->second.velocity;
    if(world.vehicleWorldVelocity)world.vehicleWorldVelocity(handle,velocity);
    const float lead=std::max(0.0f,world.nativeScriptTimeStep)/50.0f;
    const StorylandNativePoint start{matrix[12]+matrix[4]*4.8f-matrix[8]*1.3f+velocity[0]*lead,
        matrix[13]+matrix[5]*4.8f-matrix[9]*1.3f+velocity[1]*lead,
        matrix[14]+matrix[6]*4.8f-matrix[10]*1.3f+velocity[2]*lead};
    const float length=std::hypot(matrix[4],matrix[5],matrix[6]);
    if(length<1e-6f) {error="Hunter cannon has a degenerate forward vector.";return false;}
    const StorylandNativePoint end{start.x+matrix[4]*weapon->range/length,
        start.y+matrix[5]*weapon->range/length,start.z+matrix[6]*weapon->range/length};
    float nearest=storylandTraceNativeSegment(native,start,end);
    int32_t pedHit=-1,characterHit=-1,objectHit=-1,vehicleHit=-1;
    bool playerHit=false;
    const auto sphere=[&](const StorylandNativePoint& center,float radius) {
        const float dx=end.x-start.x,dy=end.y-start.y,dz=end.z-start.z;
        const float ox=start.x-center.x,oy=start.y-center.y,oz=start.z-center.z;
        const float a=dx*dx+dy*dy+dz*dz,b=ox*dx+oy*dy+oz*dz,c=ox*ox+oy*oy+oz*oz-radius*radius;
        const float discriminant=b*b-a*c;
        if(c<=0)return 0.0f;
        return discriminant<0||a<1e-6f?2.0f:(-b-std::sqrt(discriminant))/a;
    };
    for(size_t slot=0;slot<pedestrians.size()&&slot<runtime.pedestrians.size();++slot) {
        if(runtime.pedestrians[slot].health<=0||runtime.pedestrians[slot].scriptHandle>=0)continue;
        const float fraction=sphere(chest(pedestrians[slot].position),.4f);
        if(fraction>=0&&fraction<nearest) {nearest=fraction;pedHit=int32_t(slot);}
    }
    for(const auto& entry:world.missionCharacters) {
        const auto& ped=entry.second;if(!ped.alive||ped.vehicle==handle)continue;
        const auto pose=runtime.missionCombat.find(entry.first);
        const auto center=pose!=runtime.missionCombat.end()&&pose->second.anchorsValid?pose->second.torso:
            chest({ped.x,ped.y,ped.z});
        const float fraction=sphere(center,.4f);
        if(fraction>=0&&fraction<nearest) {nearest=fraction;characterHit=entry.first;pedHit=-1;}
    }
    if(runtime.health>0&&(!world.inVehicle||world.occupiedMissionVehicle!=handle)) {
        const float fraction=sphere(runtime.playerTorsoValid?runtime.playerTorso:chest(runtime.playerPosition),.4f);
        if(fraction>=0&&fraction<nearest) {nearest=fraction;playerHit=true;characterHit=pedHit=-1;}
    }
    StorylandNativePoint normal{};
    for(const auto& object:world.objects)if(storylandTraceScriptObject(native,object,start,end,nearest,normal)) {
        objectHit=object.handle;playerHit=false;characterHit=pedHit=-1;
    }
    for(size_t slot=0;slot<cars.size()&&slot<runtime.vehicles.size();++slot) {
        const auto& car=runtime.vehicles[slot];
        if(car.scriptHandle==handle||!car.collisionEnabled||!car.nativeDefinition.collision)continue;
        std::array<float,16> frame{};
        if(slot<runtime.vehicleWorldMatrices.size())frame=runtime.vehicleWorldMatrices[slot];
        else if(!world.vehicleWorldMatrix(car.scriptHandle,frame))continue;
        if(storylandTraceCollisionModel(*car.nativeDefinition.collision,frame,start,end,nearest,normal)) {
            vehicleHit=int32_t(slot);objectHit=characterHit=pedHit=-1;playerHit=false;
        }
    }
    const StorylandMissionEnvironment::DamageSource source{StorylandMissionEnvironment::DamageSourceKind::Vehicle,handle};
    if(characterHit>=0) {
        const auto victim=world.missionCharacters.find(characterHit);
        if(victim!=world.missionCharacters.end())
            storylandRequestWeaponHitSpeech(runtime,int(runtime.pedestrians.size())+characterHit,
                {victim->second.x,victim->second.y,victim->second.z},nativeId);
        hurtMissionCharacter(runtime,characterHit,weapon->damage,false,1,nativeId,source,
            false,false,false,true,0,victim==world.missionCharacters.end()?0:
                storylandNativeHitDirection(victim->second.heading+90,
                    start.x-victim->second.x,start.y-victim->second.y));
    }
    if(pedHit>=0)hurtPed(runtime,size_t(pedHit),weapon->damage,pedestrians[size_t(pedHit)].position,false,
        true,StorylandCrime::Shooting,&*weapon,&start,true,false,nativeId,0,
        storylandNativeHitDirection(pedestrians[size_t(pedHit)].yaw,
            start.x-pedestrians[size_t(pedHit)].position.x,start.y-pedestrians[size_t(pedHit)].position.y),1,
        pedestrians[size_t(pedHit)].yaw,std::hypot(runtime.pedestrians[size_t(pedHit)].contact.velocity.x,
            runtime.pedestrians[size_t(pedHit)].contact.velocity.y));
    if(vehicleHit>=0)hurtMissionVehicle(runtime,size_t(vehicleHit),weapon->damage,1,nativeId,source);
    if(objectHit>=0)storylandHitScriptObject(world,objectHit,nativeId,50,false,false,normal);
    if(playerHit)storylandRequestWeaponHitSpeech(runtime,-1,runtime.playerPosition,nativeId);
    if(playerHit&&runtime.inflictDamage(weapon->damage,1,nativeId,true,0,
        storylandNativeHitDirection(world.heading+90,
            start.x-runtime.playerPosition.x,start.y-runtime.playerPosition.y),false,&start)) {
        world.playerLastWeaponDamage=nativeId;world.playerDamageSource=source;
    }
    const StorylandNativePoint impact{start.x+(end.x-start.x)*nearest,start.y+(end.y-start.y)*nearest,
        start.z+(end.z-start.z)*nearest};
    weaponTrace(runtime,start,impact,nativeId,world.inVehicle&&world.occupiedMissionVehicle==handle);
    queueVcsBulletPass(runtime,native,start,impact,false);
    muzzleFlash(runtime,start,nativeId,StorylandMuzzleOwner::Vehicle,handle);
    if(nearest<1)particle(runtime,StorylandParticleKind::Spark,impact,normal,.04f,.15f);
    sound(runtime,StorylandPlayTestSound::WeaponShot,start,
        loadedWeapon==runtime.weapons.end()?-1:int(loadedWeapon-runtime.weapons.begin()));
    error.clear();return true;
}
