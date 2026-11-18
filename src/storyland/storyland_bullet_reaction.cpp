#include "storyland_play_test_runtime.h"
#include "storyland_ped_movement.h"
#include "storyland_swimming_runtime.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float radians = 0.01745329251994329577f;

bool knockdownTouchesWater(const StorylandNativeWorld& world,const StorylandNativePoint& position,
    float& surface) {
    if(!storylandNativeWaterHeight(world,position.x,position.y,surface)||
        !storylandVcsWaterDepthAllowsContact(position.z,surface))return false;
    uint8_t material=0;
    const StorylandNativePoint top{position.x,position.y,surface+.1f};
    const StorylandNativePoint bottom{position.x,position.y,surface-3};
    const float fraction=storylandTraceNativeGround(world,top,bottom,material);
    return storylandWaterIsDeep(position.z,surface,fraction<1,
        top.z+(bottom.z-top.z)*fraction);
}
}

bool storylandBeginBulletReaction(StorylandPlayTestRuntime& runtime,
    StorylandBulletReaction& reaction,uint8_t direction,const StorylandBulletImpact& impact) {
    const auto duration=[&](int32_t group,int32_t animation) {
        if(runtime.missionEnvironment&&runtime.missionEnvironment->animationDuration) {
            const float value=runtime.missionEnvironment->animationDuration(group,animation);
            if(std::isfinite(value)&&value>0)return value;
        }
        return runtime.reactionBindings.duration?runtime.reactionBindings.duration(animation,group):0.0f;
    };
    const auto flags=[&](int32_t animation,uint16_t& value) {
        if(runtime.missionEnvironment&&runtime.missionEnvironment->animationAssociationFlags&&
            runtime.missionEnvironment->animationAssociationFlags(0,animation,value))return true;
        return runtime.reactionBindings.associationFlags&&
            runtime.reactionBindings.associationFlags(animation,0,value);
    };
    if(direction>3||!std::isfinite(runtime.elapsed)||reaction.knockdown)return false;
    if(runtime.crime.vcs&&impact.canKnockDown&&
        storylandVcsBulletKnockdownWeapon(impact.weapon,impact.player)) {
        StorylandVcsPedFallState fall;
        fall.fallAnimation=28+direction;
        uint16_t fallFlags=0,getUpFlags=0;
        if(flags(fall.fallAnimation,fallFlags)&&(fallFlags&2u)==0&&runtime.reactionBindings.rootDelta) {
            fall.getUpAnimation=storylandVcsPedGetUpAnimation(fallFlags);
            fall.fallDuration=duration(0,fall.fallAnimation);
            fall.getUpDuration=duration(0,fall.getUpAnimation);
            if(flags(fall.getUpAnimation,getUpFlags)&&(getUpFlags&2u)==0&&
                std::isfinite(fall.fallDuration)&&fall.fallDuration>0&&
                std::isfinite(fall.getUpDuration)&&fall.getUpDuration>0&&
                std::isfinite(impact.heading)&&std::isfinite(impact.speed)&&
                std::isfinite(impact.verticalSpeed)) {
                auto* environment=runtime.missionEnvironment;
                if(!impact.player&&!reaction.entitySeedInitialized) {
                    reaction.entitySeed=environment?uint8_t(environment->nextNativeRandom()):0;
                    reaction.entitySeedInitialized=true;
                }
                const uint32_t now=environment?environment->gameMilliseconds:runtime.trafficClockMilliseconds;
                fall.getUpAfter=storylandVcsPedGetUpDeadline(now,fall.fallDuration,
                    impact.player?0:reaction.entitySeed,
                    impact.player||!environment?0:environment->nativeFrameCounter,
                    impact.player?500:1500);
                float dx=impact.sourceDirection.x,dy=impact.sourceDirection.y;
                const float length=std::hypot(dx,dy);
                if(std::isfinite(length)&&length>.00001f) {dx/=length;dy/=length;}
                else {
                    const float angle=(impact.heading+float(direction)*90)*radians;
                    dx=std::cos(angle);dy=std::sin(angle);
                }
                const float mass=impact.player&&std::isfinite(runtime.playerMass)&&runtime.playerMass>0?
                    runtime.playerMass:StorylandNativePedMass;
                const float force=5*StorylandNativePhysicsRate/mass;
                const float heading=impact.heading*radians;
                fall.velocity={std::cos(heading)*impact.speed-dx*force,
                    std::sin(heading)*impact.speed-dy*force,impact.verticalSpeed+force};
                reaction.fall=fall;reaction.knockdown=true;reaction.dying=false;
                reaction.vehicleImpact=false;reaction.recoveryBlocked=false;
                reaction.sourceVehicleIdentity=0;
                reaction.animation=fall.fallAnimation;reaction.startedAt=runtime.elapsed;
                reaction.duration=fall.fallDuration;
                return true;
            }
        }
    }
    return storylandStartNativeBulletReaction(reaction,runtime.crime.vcs,direction,runtime.elapsed,duration);
}

bool storylandAdvanceBulletKnockdown(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,StorylandBulletReaction& reaction,
    StorylandNativePoint& position,float heading,float seconds,int32_t pedestrian,
    StorylandCharacterWaterState* water) {
    if(!reaction.knockdown)return false;
    if((!world.paths.vcs&&!reaction.vehicleImpact)||!std::isfinite(heading)) {
        reaction={};return false;
    }
    if(!std::isfinite(seconds)||seconds<=0)return true;
    auto& fall=reaction.fall;
    if(reaction.vehicleImpact&&reaction.sourceVehicleIdentity) {
        const auto matches=[&](int32_t index) {
            return index>=0&&size_t(index)<runtime.vehicles.size()&&
                runtime.vehicles[size_t(index)].pedestrianSweep.identity==reaction.sourceVehicleIdentity;
        };
        if(!matches(fall.sourceVehicle)) {
            fall.sourceVehicle=-1;
            for(size_t index=0;index<runtime.vehicles.size();++index)
                if(matches(int32_t(index))) {fall.sourceVehicle=int32_t(index);break;}
        }
    }
    const int32_t firstFall=world.paths.vcs?28:25;
    if(fall.fallAnimation<firstFall||fall.fallAnimation>firstFall+3||
        !std::isfinite(fall.fallDuration)||(reaction.vehicleImpact?fall.fallDuration<0:fall.fallDuration<=0)||
        !std::isfinite(fall.getUpDuration)||(reaction.vehicleImpact?fall.getUpDuration<0:fall.getUpDuration<=0)||
        !std::all_of(fall.velocity.begin(),fall.velocity.end(),[](float value){return std::isfinite(value);})) {
        reaction={};return false;
    }
    const float step=std::min(seconds,.1f);
    float surface=0;
    if(knockdownTouchesWater(world,position,surface)) {
        if(!reaction.dying||!water) {reaction={};return false;}
        const float impactVelocity=fall.velocity[2];
        if(!water->active)water->verticalSpeed=impactVelocity;
        const auto buoyancy=storylandStepWaterCharacter(*water,position.z,surface,true,
            false,false,pedestrian>=0,step,world.paths.vcs);
        auto desired=position;desired.z=buoyancy.height;
        const auto moved=storylandMovePlayTestCharacter(world,runtime,position,desired,
            pedestrian,false,false,false);
        if(moved.clear)position=moved.position;
        fall.velocity={0,0,water->verticalSpeed};fall.grounded=false;
        if(buoyancy.entered) {
            StorylandPlayTestSoundEvent splash(StorylandPlayTestSound::WaterSplash);
            splash.position={position.x,position.y,surface};splash.positioned=true;
            runtime.soundEvents.push_back(splash);
        }
        storylandUpdateSwimmingParticles(runtime,*water,position,surface,heading,0,
            impactVelocity,step,buoyancy.entered,false);
        return true;
    }
    const uint32_t now=runtime.missionEnvironment?runtime.missionEnvironment->gameMilliseconds:
        runtime.trafficClockMilliseconds;
    if(storylandVcsPedCanGetUp(fall,now,!reaction.dying&&!reaction.recoveryBlocked)) {
        fall.recovering=true;fall.time=0;fall.velocity={};
    }
    const float previousTime=fall.time;
    fall.time=std::min(fall.duration(),fall.time+step);
    const StorylandNativePoint before=position;
    StorylandNativePoint desired{before.x+fall.velocity[0]*step,
        before.y+fall.velocity[1]*step,before.z};
    if(fall.recovering&&runtime.reactionBindings.rootDelta) {
        const auto root=runtime.reactionBindings.rootDelta(fall.animation(),0,previousTime,fall.time);
        if(std::isfinite(root.x)&&std::isfinite(root.y)) {
            const float angle=heading*radians;
            desired.x+=std::cos(angle)*root.x+std::sin(angle)*root.y;
            desired.y+=std::sin(angle)*root.x-std::cos(angle)*root.y;
        }
    }
    const auto vertical=storylandPedVerticalStep(fall.velocity[2],runtime.gravity,step);
    desired.z+=vertical.displacement;
    uint8_t material=0;
    const StorylandNativePoint top{desired.x,desired.y,before.z+.08f};
    const StorylandNativePoint bottom{desired.x,desired.y,std::min(before.z-3,desired.z-.15f)};
    const float fraction=storylandTraceNativeGround(world,top,bottom,material);
    bool supported=fraction<1;
    float floor=top.z+(bottom.z-top.z)*fraction;
    for(const auto& car:runtime.movementCars) {
        if(!car.collisionEnabled)continue;
        const float roof=car.position.z+car.height;
        if(before.z<roof-.15f||roof<bottom.z||roof>top.z)continue;
        const float angle=car.yaw*radians;
        const float dx=desired.x-car.position.x,dy=desired.y-car.position.y;
        if(std::fabs(dx*std::cos(angle)+dy*std::sin(angle))>car.halfLength||
            std::fabs(-dx*std::sin(angle)+dy*std::cos(angle))>car.halfWidth)continue;
        if(!supported||roof>floor) {floor=roof;supported=true;}
    }
    const bool landing=supported&&vertical.velocity<=0&&desired.z<=floor+.005f;
    if(landing)desired.z=floor;
    const auto moved=storylandMovePlayTestCharacter(world,runtime,before,desired,
        pedestrian,false,false,false,reaction.vehicleImpact&&!fall.recovering?fall.sourceVehicle:-1);
    if(moved.clear)position=moved.position;
    fall.grounded=landing&&std::fabs(position.z-floor)<.05f;
    fall.velocity[2]=fall.grounded?0:vertical.velocity;
    if(fall.grounded) {
        if(reaction.vehicleImpact&&!fall.recovering) {
            // Host contact friction bridges the native per-surface solver.
            const float speed=std::hypot(fall.velocity[0],fall.velocity[1]);
            const float scale=speed>0?std::max(0.0f,speed-runtime.gravity*.6f*step)/speed:0;
            fall.velocity[0]*=scale;fall.velocity[1]*=scale;
        } else fall.velocity[0]=fall.velocity[1]=0;
    }
    if(moved.blocked&&desired.z>before.z)fall.velocity[2]=std::min(0.0f,fall.velocity[2]);
    if(moved.blocked&&std::hypot(position.x-before.x,position.y-before.y)<
        .15f*std::hypot(desired.x-before.x,desired.y-before.y))fall.velocity[0]=fall.velocity[1]=0;
    reaction.animation=reaction.dying?-1:fall.animation();
    reaction.duration=reaction.dying?0:fall.duration();
    if((fall.recovering&&fall.time>=fall.getUpDuration)||
        (reaction.dying&&fall.grounded&&fall.time>=fall.fallDuration&&
            (!reaction.vehicleImpact||std::hypot(fall.velocity[0],fall.velocity[1])<.05f))) {
        reaction.knockdown=false;reaction.animation=-1;reaction.duration=0;
        reaction.vehicleImpact=false;reaction.recoveryBlocked=false;
        reaction.sourceVehicleIdentity=0;
    }
    return true;
}
