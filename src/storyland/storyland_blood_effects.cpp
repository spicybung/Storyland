#include "src/storyland/storyland_blood_effects.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_native_random.h"
#include "src/storyland/storyland_ped_damage.h"
#include "src/re/reVCS/runtime/storyland_vcs_retail_quirks.h"
#include "src/re/reVCS/runtime/storyland_vcs_blood.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float Radians=.01745329251994329577f;
constexpr size_t MaximumBloodPools=128;
constexpr float TyreBloodDistance=12.0f;

bool finitePoint(const StorylandNativePoint& point) {
    return std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z);
}

float visualRandom(StorylandPlayTestRuntime& runtime) {
    return storylandNativeRandomUnit(runtime.combatRandom);
}

bool drySurface(const StorylandNativeWorld& world,const StorylandNativePoint& point) {
    float water=0;
    return !storylandNativeWaterHeight(world,point.x,point.y,water)||point.z>water+.025f;
}

bool bloodGround(const StorylandNativeWorld& world,const StorylandNativePoint& feet,
    StorylandNativePoint& point,StorylandNativePoint& normal) {
    if(!finitePoint(feet))return false;
    const StorylandNativePoint start{feet.x,feet.y,feet.z+.35f};
    const StorylandNativePoint end{feet.x,feet.y,feet.z-.65f};
    uint8_t surface=0;
    const float hit=storylandTraceNativeGround(world,start,end,surface,&normal);
    if(!std::isfinite(hit)||hit<0||hit>=1||normal.z<.35f)return false;
    point={feet.x,feet.y,start.z+(end.z-start.z)*hit};
    return drySurface(world,point);
}

float supportedRadius(const StorylandNativeWorld& world,const StorylandNativePoint& center,
    const StorylandNativePoint& normal,float requested) {
    for(float radius=requested;radius>=.15f;radius*=.65f) {
        bool supported=true;
        for(unsigned sample=0;sample<8&&supported;++sample) {
            const float angle=float(sample)*.78539816339f;
            StorylandNativePoint edge{center.x+std::cos(angle)*radius,
                center.y+std::sin(angle)*radius,center.z};
            edge.z-=((edge.x-center.x)*normal.x+(edge.y-center.y)*normal.y)/normal.z;
            StorylandNativePoint point{},edgeNormal{};
            supported=bloodGround(world,edge,point,edgeNormal)&&
                std::fabs(point.z-edge.z)<.09f&&
                normal.x*edgeNormal.x+normal.y*edgeNormal.y+normal.z*edgeNormal.z>.9f;
        }
        if(supported)return radius;
    }
    return 0;
}

void reserveBloodPool(StorylandPlayTestRuntime& runtime) {
    size_t count=0;
    auto oldest=runtime.particles.end();
    for(auto item=runtime.particles.begin();item!=runtime.particles.end();++item) {
        if(item->kind!=StorylandParticleKind::BloodPool)continue;
        ++count;
        if(oldest==runtime.particles.end()||item->remaining<oldest->remaining)oldest=item;
    }
    if(count>=MaximumBloodPools&&oldest!=runtime.particles.end())runtime.particles.erase(oldest);
    if(runtime.particles.size()>=2048) {
        const auto expendable=std::find_if(runtime.particles.begin(),runtime.particles.end(),
            [](const auto& item) {
                return item.kind==StorylandParticleKind::BloodPool||
                    item.kind==StorylandParticleKind::Smoke||item.kind==StorylandParticleKind::Exhaust;
            });
        if(expendable!=runtime.particles.end())runtime.particles.erase(expendable);
    }
}

void updateCorpse(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    StorylandCorpseBloodState& state,const StorylandNativePoint& position,float heading,
    bool alive,bool eligible,float seconds) {
    if(alive) {state={};return;}
    if(state.poolEmitted||state.suppressPool||!finitePoint(position))return;
    if(!state.observed) {
        state.previousPosition=position;state.observed=true;
    }
    state.age+=seconds;
    const float distance=std::hypot(position.x-state.previousPosition.x,
        position.y-state.previousPosition.y,position.z-state.previousPosition.z);
    state.stationaryTime=distance<.025f?state.stationaryTime+seconds:0;
    state.previousPosition=position;
    if(!eligible||state.age<=StorylandVcsBlood::CorpseDelay||state.stationaryTime<.35f)return;
    if(std::hypot(position.x-runtime.playerPosition.x,position.y-runtime.playerPosition.y)>100)return;
    StorylandNativePoint center{},normal{};
    if(!bloodGround(world,position,center,normal))return;
    const float radius=supportedRadius(world,center,normal,StorylandVcsBlood::CorpseRadius);
    if(radius<=0)return;
    reserveBloodPool(runtime);
    if(runtime.particles.size()>=2048)return;
    StorylandRuntimeParticle pool;
    pool.kind=StorylandParticleKind::BloodPool;
    pool.position={center.x+normal.x*.012f,center.y+normal.y*.012f,center.z+normal.z*.012f};
    pool.velocity={};pool.remaining=pool.lifetime=
        StorylandVcsBlood::CorpseGrowthSeconds+StorylandVcsBlood::CorpseRestSeconds;
    pool.size=std::clamp((state.age-StorylandVcsBlood::CorpseDelay)*
        StorylandVcsBlood::CorpseGrowthPerSecond,.005f,radius);
    pool.bloodPoolTargetSize=radius;
    pool.bloodSurfaceNormal=normal;pool.bloodSurfaceRotation=heading*Radians;
    pool.bloodSurfaceArea=runtime.visibleArea;pool.corpseBloodPool=true;
    runtime.particles.push_back(pool);
    state.poolEmitted=true;
}

bool tyreContact(const StorylandRuntimeVehicle& vehicle,const StorylandNativeTrafficActor& actor,
    const StorylandNativeWorld& world,size_t wheel,StorylandNativePoint& point,
    StorylandNativePoint& normal,uint8_t& surface) {
    const auto& definition=vehicle.nativeDefinition;
    const bool bike=definition.twoWheels();
    const size_t count=bike?2:4;
    const size_t line=bike?wheel*2:wheel;
    if(vehicle.contactsValid) {
        unsigned contacts=0;
        point={};normal={};
        for(size_t offset=0;offset<(bike?2u:1u);++offset) {
            const size_t index=line+offset;
            if(!vehicle.wheelContacts[index])continue;
            const auto& contact=vehicle.wheelContactPoints[index];
            const auto& contactNormal=vehicle.wheelContactNormals[index];
            point.x+=contact.x;point.y+=contact.y;point.z+=contact.z;
            normal.x+=contactNormal.x;normal.y+=contactNormal.y;normal.z+=contactNormal.z;
            surface=vehicle.wheelSurfaces[index];++contacts;
        }
        if(!contacts)return false;
        const float scale=1.0f/float(contacts);
        point.x*=scale;point.y*=scale;point.z*=scale;
        normal.x*=scale;normal.y*=scale;normal.z*=scale;
        return normal.z>=.35f;
    }
    const float angle=actor.yaw*Radians,c=std::cos(angle),s=std::sin(angle);
    const bool front=definition.wheels.size()==count?definition.wheels[wheel].front:wheel%2==0;
    const float along=(front?1:-1)*vehicle.halfLength*.7f;
    const float lateral=bike?0:(wheel<2?1:-1)*vehicle.halfWidth*.8f;
    point={actor.position.x+c*along-s*lateral,actor.position.y+s*along+c*lateral,actor.position.z};
    const StorylandNativePoint start{point.x,point.y,point.z+.45f},end{point.x,point.y,point.z-1.1f};
    const float hit=storylandTraceNativeGround(world,start,end,surface,&normal);
    if(!std::isfinite(hit)||hit<0||hit>=1||normal.z<.35f)return false;
    point.z=start.z+(end.z-start.z)*hit;
    return true;
}

bool crossesBloodPool(const StorylandRuntimeParticle& pool,const StorylandNativePoint& start,
    const StorylandNativePoint& end,float& firstFraction) {
    const auto& normal=pool.bloodSurfaceNormal;
    const StorylandNativePoint offset{start.x-pool.position.x,start.y-pool.position.y,start.z-pool.position.z};
    const StorylandNativePoint delta{end.x-start.x,end.y-start.y,end.z-start.z};
    const float height=offset.x*normal.x+offset.y*normal.y+offset.z*normal.z;
    const float vertical=delta.x*normal.x+delta.y*normal.y+delta.z*normal.z;
    const StorylandNativePoint flatOffset{offset.x-normal.x*height,offset.y-normal.y*height,offset.z-normal.z*height};
    const StorylandNativePoint flatDelta{delta.x-normal.x*vertical,delta.y-normal.y*vertical,delta.z-normal.z*vertical};
    const float a=flatDelta.x*flatDelta.x+flatDelta.y*flatDelta.y+flatDelta.z*flatDelta.z;
    const float b=flatOffset.x*flatDelta.x+flatOffset.y*flatDelta.y+flatOffset.z*flatDelta.z;
    const float c=flatOffset.x*flatOffset.x+flatOffset.y*flatOffset.y+flatOffset.z*flatOffset.z-pool.size*pool.size;
    float hit=0;
    if(c>0) {
        const float discriminant=b*b-a*c;
        if(a<.000001f||discriminant<0)return false;
        hit=(-b-std::sqrt(discriminant))/a;
        if(hit<0||hit>1)return false;
    }
    if(std::fabs(height+vertical*hit)>.12f)return false;
    firstFraction=std::min(firstFraction,hit);
    return true;
}
}

bool storylandBloodDamageAllowed(bool vcs,uint8_t kind,int32_t weapon) {
    return kind!=2&&weapon!=storylandNativeDrowningWeapon(vcs)&&
        weapon!=(vcs?16:14);
}

void storylandMarkBloodRunover(StorylandPlayTestRuntime& runtime,
    const StorylandNativePoint& position,int32_t weapon) {
    const int32_t runover=storylandNativeRunoverWeapon(runtime.crime.vcs);
    if((weapon!=runover&&weapon!=runover-1)||!finitePoint(position))return;
    if(runtime.tyreBloodContacts.size()>=64)runtime.tyreBloodContacts.erase(runtime.tyreBloodContacts.begin());
    runtime.tyreBloodContacts.push_back({position,.6f,runtime.visibleArea});
}

void storylandEmitBloodImpact(StorylandPlayTestRuntime& runtime,
    const StorylandNativePoint& impact,float damage,const StorylandNativePoint* source,bool heavy) {
    if(!finitePoint(impact)||!std::isfinite(damage)||damage<=0)return;
    StorylandNativePoint direction{};
    if(source&&finitePoint(*source)) {
        direction={impact.x-source->x,impact.y-source->y,impact.z-source->z};
        const float length=std::hypot(direction.x,direction.y,direction.z);
        if(length>.001f) {direction.x/=length;direction.y/=length;direction.z/=length;}
        else direction={};
    }
    const unsigned count=unsigned(std::clamp(damage*.12f+float(heavy?8:3),3.0f,18.0f));
    std::vector<StorylandMissionEnvironment::ScriptParticle> requests;
    std::vector<StorylandRuntimeParticle> fallback;
    requests.reserve(count);fallback.reserve(count);
    for(unsigned index=0;index<count;++index) {
        const float angle=visualRandom(runtime)*6.28318530718f;
        const float spread=.35f+visualRandom(runtime)*.9f;
        const float impulse=.7f+visualRandom(runtime)*(heavy?2.0f:1.1f);
        const StorylandNativePoint velocity{direction.x*impulse+std::cos(angle)*spread,
            direction.y*impulse+std::sin(angle)*spread,
            direction.z*impulse+.35f+visualRandom(runtime)*1.2f};
        StorylandMissionEnvironment::ScriptParticle request;
        request.type=heavy&&index==0?8:index%3==0?6:7;
        request.position={impact.x,impact.y,impact.z};
        request.velocity={velocity.x/50.0f,velocity.y/50.0f,velocity.z/50.0f};
        requests.push_back(request);
        StorylandRuntimeParticle particle;
        particle.kind=StorylandParticleKind::Blood;particle.position=impact;particle.velocity=velocity;
        particle.size=request.type==8?.065f:request.type==6?.04f:.025f;
        particle.remaining=particle.lifetime=.75f;
        fallback.push_back(particle);
    }
    if(runtime.missionEnvironment&&runtime.missionEnvironment->createScriptParticles) {
        std::string error;
        if(runtime.missionEnvironment->createScriptParticles(requests,error))return;
    }
    for(const auto& particle:fallback) {
        if(runtime.particles.size()>=2048)break;
        runtime.particles.push_back(particle);
    }
}

void storylandUpdateCorpseBlood(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const std::vector<StorylandNativeTrafficActor>& actors,
    const StorylandNativePoint& player,float seconds) {
    if(!std::isfinite(seconds)||seconds<=0)return;
    for(auto& contact:runtime.tyreBloodContacts)contact.remaining-=seconds;
    runtime.tyreBloodContacts.erase(std::remove_if(runtime.tyreBloodContacts.begin(),runtime.tyreBloodContacts.end(),
        [](const auto& contact){return contact.remaining<=0;}),runtime.tyreBloodContacts.end());
    const auto* environment=runtime.missionEnvironment;
    const bool playerBleeding=!environment||environment->playerBleeding;
    updateCorpse(runtime,world,runtime.playerCorpseBlood,player,environment?environment->heading+90:0,
        runtime.health>0,playerBleeding&&!runtime.playerWater.active&&runtime.occupiedVehicle<0,seconds);
    for(size_t slot=0;slot<actors.size()&&slot<runtime.pedestrians.size();++slot) {
        auto& ped=runtime.pedestrians[slot];
        if(ped.scriptHandle>=0)continue;
        const auto& actor=actors[slot];
        updateCorpse(runtime,world,ped.corpseBlood,actor.position,actor.yaw,
            ped.health>0&&ped.state!=StorylandPedState::Dead,
            !ped.water.active&&!actor.water&&!actor.vehicle,seconds);
    }
    if(environment)for(const auto& entry:environment->missionCharacters) {
        const auto& character=entry.second;
        auto& combat=runtime.missionCombat[entry.first];
        const bool bleeding=character.bleeding&&character.visible&&!character.removeFromWorld&&
            character.vehicle<0&&!character.attached&&!combat.water.active&&
            character.lastWeaponDamage!=storylandNativeDrowningWeapon(runtime.crime.vcs);
        updateCorpse(runtime,world,combat.corpseBlood,{character.x,character.y,character.z},
            character.heading+90,character.alive&&character.health>0,bleeding,seconds);
    }
}

void storylandUpdateBloodParticle(StorylandPlayTestRuntime& runtime,StorylandRuntimeParticle& particle,
    const StorylandNativeWorld& world,const StorylandNativePoint& previous,int area) {
    if(particle.kind!=StorylandParticleKind::Blood||!finitePoint(particle.position))return;
    if(!drySurface(world,particle.position)) {particle.remaining=0;return;}
    StorylandNativePoint normal{};
    uint8_t surface=0;
    const float fraction=storylandTraceNativeGround(world,previous,particle.position,surface,&normal);
    if(!std::isfinite(fraction)||fraction>=1||fraction<0)return;
    if(normal.z<.35f||particle.velocity.z>0) {particle.remaining=0;return;}
    const StorylandNativePoint position{previous.x+(particle.position.x-previous.x)*fraction,
        previous.y+(particle.position.y-previous.y)*fraction,
        previous.z+(particle.position.z-previous.z)*fraction};
    if(!drySurface(world,position)) {particle.remaining=0;return;}
    float radius=std::clamp(particle.size*1.7f,.035f,.12f),lifetime=20.0f;
    if(runtime.crime.vcs) {
        const uint32_t sequence=++runtime.bloodSplatSequence;
        if((sequence&7u)!=1&&(sequence&7u)!=2) {particle.remaining=0;return;}
        StorylandVcsBlood::groundSplat(sequence,storylandNativeRandom(runtime.combatRandom),radius,lifetime);
    }
    particle.position={position.x+normal.x*.012f,position.y+normal.y*.012f,position.z+normal.z*.012f};
    particle.kind=StorylandParticleKind::BloodPool;particle.velocity={};particle.nativePhysics=false;
    particle.remaining=particle.lifetime=lifetime;
    particle.size=radius;
    particle.bloodSurfaceNormal=normal;particle.bloodSurfaceArea=area;
    particle.bloodPoolTargetSize=particle.size;particle.corpseBloodPool=false;
    particle.bloodSurfaceRotation=std::atan2(previous.y-position.y,previous.x-position.x);
}

void storylandUpdateBloodPool(StorylandRuntimeParticle& particle,float seconds) {
    if(particle.kind!=StorylandParticleKind::BloodPool||!std::isfinite(seconds)||seconds<=0)return;
    if(particle.bloodPoolTargetSize>particle.size)
        particle.size=std::min(particle.bloodPoolTargetSize,
            particle.size+seconds*StorylandVcsBlood::CorpseGrowthPerSecond);
}

void storylandUpdateBloodyTyres(StorylandPlayTestRuntime& runtime,
    const StorylandNativeTrafficActor& actor,StorylandRuntimeVehicle& vehicle,
    const StorylandNativeWorld* world,float seconds) {
    if(!std::isfinite(seconds)||seconds<=0)return;
    if(actor.water||vehicle.water.immersion>.05f) {vehicle.bloodyTyres={};return;}
    if(!world||!vehicle.grounded||vehicle.craneCarried||!vehicle.visible||
        !vehicle.collisionEnabled||vehicle.nativeDefinition.domain!=StorylandVehicleDomain::Road) {
        for(auto& tyre:vehicle.bloodyTyres)tyre.contactValid=false;
        return;
    }
    const auto& definition=vehicle.nativeDefinition;
    const bool marksEnabled=StorylandVcsRetailQuirks::skidmarksEnabled(
        definition.vcsModelLayout,definition.hasVcsModelInfoFlags,definition.vcsModelInfoFlags);
    const size_t count=vehicle.nativeDefinition.twoWheels()?2:4;
    for(size_t wheel=0;wheel<count;++wheel) {
        auto& tyre=vehicle.bloodyTyres[wheel];
        StorylandNativePoint point{},normal{};
        uint8_t surface=0;
        if(!tyreContact(vehicle,actor,*world,wheel,point,normal,surface)) {
            tyre.contactValid=false;continue;
        }
        if(!drySurface(*world,point)) {tyre={};continue;}
        tyre.sampleClock+=seconds;
        const float distance=tyre.contactValid?std::hypot(point.x-tyre.previousContact.x,
            point.y-tyre.previousContact.y,point.z-tyre.previousContact.z):0;
        if(tyre.contactValid&&distance>std::max(1.5f,std::fabs(vehicle.speed)*tyre.sampleClock*3+.5f))tyre={};
        const bool previouslyBloody=runtime.crime.vcs?tyre.bloodSamplesRemaining>0:tyre.distanceRemaining>0;
        float pickup=0,pickupFraction=1;
        for(const auto& pool:runtime.particles) {
            if(pool.kind!=StorylandParticleKind::BloodPool||pool.remaining<=0||
                (pool.bloodSurfaceArea>=0&&pool.bloodSurfaceArea!=runtime.visibleArea))continue;
            if(!crossesBloodPool(pool,tyre.contactValid?tyre.previousContact:point,point,pickupFraction))continue;
            pickup=std::max(pickup,(pool.corpseBloodPool?TyreBloodDistance:2.0f)*
                std::clamp(pool.remaining/10.0f,0.0f,1.0f));
        }
        for(const auto& contact:runtime.tyreBloodContacts) {
            if(contact.area!=runtime.visibleArea||contact.remaining<=0)continue;
            StorylandRuntimeParticle footprint;
            footprint.position=contact.position;footprint.size=.45f;
            if(crossesBloodPool(footprint,tyre.contactValid?tyre.previousContact:point,point,pickupFraction))
                pickup=std::max(pickup,TyreBloodDistance);
        }
        tyre.distanceRemaining=std::max(tyre.distanceRemaining,pickup);
        if(pickup>0&&!previouslyBloody) {
            tyre.bloodSamplesRemaining=StorylandVcsBlood::SkidBloodSamples;
            tyre.markSequence=0;
        }
        point.x+=normal.x*.012f;point.y+=normal.y*.012f;point.z+=normal.z*.012f;
        const bool bloody=runtime.crime.vcs?tyre.bloodSamplesRemaining>0:tyre.distanceRemaining>0;
        const bool sampleReady=!runtime.crime.vcs||tyre.sampleClock>=StorylandVcsBlood::SkidSampleSeconds;
        if(tyre.contactValid&&distance>=.06f&&bloody&&sampleReady&&
            marksEnabled&&storylandTyreLeavesRoadMark(surface)&&
            storylandSkidCanJoin(tyre.previousContact,point,std::max(seconds,tyre.sampleClock),
                std::hypot(vehicle.speed,vehicle.lateralSpeed))) {
            if(runtime.roadSkids.size()>=4096)runtime.roadSkids.erase(runtime.roadSkids.begin(),runtime.roadSkids.begin()+128);
            StorylandRoadSkid mark;
            mark.start=tyre.previousContact;mark.end=point;mark.normal=normal;
            if(!previouslyBloody&&pickupFraction>0) {
                mark.start={mark.start.x+(point.x-mark.start.x)*pickupFraction,
                    mark.start.y+(point.y-mark.start.y)*pickupFraction,
                    mark.start.z+(point.z-mark.start.z)*pickupFraction};
            }
            mark.width=vehicle.nativeDefinition.twoWheels()?.13f:std::clamp(vehicle.halfWidth*.22f,.15f,.35f);
            mark.opacity=runtime.crime.vcs?1:std::clamp(tyre.distanceRemaining/TyreBloodDistance,0.0f,1.0f);
            mark.remaining=mark.lifetime=StorylandVcsBlood::SkidRestSeconds+StorylandVcsBlood::SkidFadeSeconds;
            mark.fadeSeconds=StorylandVcsBlood::SkidFadeSeconds;
            mark.textureStart=float(tyre.markSequence)*5.01f;mark.textureEnd=mark.textureStart+5.01f;
            mark.area=runtime.visibleArea;mark.blood=true;
            runtime.roadSkids.push_back(mark);
        }
        if(sampleReady) {
            tyre.sampleClock=std::fmod(tyre.sampleClock,StorylandVcsBlood::SkidSampleSeconds);
            if(bloody) {
                ++tyre.markSequence;
                if(runtime.crime.vcs&&tyre.bloodSamplesRemaining>0)--tyre.bloodSamplesRemaining;
                if(runtime.crime.vcs&&tyre.bloodSamplesRemaining==0)tyre.distanceRemaining=0;
            }
            if(!runtime.crime.vcs&&tyre.contactValid&&distance>=.06f)
                tyre.distanceRemaining=std::max(0.0f,tyre.distanceRemaining-distance);
        }
        if(!tyre.contactValid||(sampleReady&&distance>=.06f))tyre.previousContact=point;
        tyre.contactValid=true;
    }
}
