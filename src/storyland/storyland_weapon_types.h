#pragma once
#include "src/leeds/leeds_native_world.h"
#include <string>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

struct StorylandMeleeAttack {
    bool active=false,moving=false,contactApplied=false;
    uint32_t sequence=0;
    float elapsed=0,duration=.55f,contactTime=.22f,recovery=1;
    size_t weapon=0;
    int target=-1;
    int32_t missionTarget=-1;
};

struct StorylandWeaponDefinition {
    uint32_t nativeFlags=0;
    int animationGroup=-1;
    float animationLoopStart=0,animationLoopEnd=0,animationFireTime=0;
    float crouchLoopStart=0,crouchLoopEnd=0,crouchFireTime=0;
    std::array<float,3> meleeDurations{.55f,.55f,.55f},meleeContactTimes{.22f,.22f,.22f};
    float runningMeleeDuration=.6f,runningMeleeContactTime=.22f;
    bool meleeTimingFromMotion=false;
    int nativeId=-1,modelId=-1,slot=0,fireType=0;
    std::string name="Fists";
    float targetRange=0; // Authored CWeaponInfo range; separate from melee contact reach.
    std::array<float,3> fireOffset{};
    bool fireOffsetValid=false;
    float range=1.8f,damage=10,interval=0.55f,reloadSeconds=1.5f,speed=35,radius=3;
    int clipSize=1;
    bool gravity=true;
    float lifetime=4;
    size_t visual=SIZE_MAX,projectileVisual=SIZE_MAX;
};
struct StorylandWeaponInventory {
    int ammunition=0,magazine=0;
    float cooldown=0,reloading=0;
    bool owned=false;
};
enum class StorylandBetaWeapon : uint8_t { TearGas, LandMine, SeaMine, Count };

struct StorylandBetaWeaponGrant {
    bool enabled=false,originallyOwned=false,externalGrant=false;
    int32_t nativeWeapon=-1,remainingGranted=0,observedAmmunition=0,previousWeapon=-1;
    template<class Archive> void saveState(Archive& archive) {
        archive(enabled,originallyOwned,externalGrant,nativeWeapon,remainingGranted,
            observedAmmunition,previousWeapon);
    }
};

struct StorylandKillFrenzy {
    // The switch controls the custom launcher; authored SCM starts remain available.
    bool enabled=false,custom=false,onlyHeadshots=false,completedText=false;
    uint16_t status=0; // Native: 0 inactive, 1 running, 2 passed, 3 failed.
    int32_t nativeWeapon=-1,timeLimitMilliseconds=0,targetKills=0,kills=0;
    uint32_t startedAt=0;
    std::array<int32_t,4> models{{-1,-1,-1,-1}};
    std::string text;
    StorylandBetaWeaponGrant grant;
    bool active() const {return status==1;}
    template<class Archive> void saveState(Archive& archive) {
        archive(enabled,custom,onlyHeadshots,completedText,status,nativeWeapon,
            timeLimitMilliseconds,targetKills,kills,startedAt,models,text,grant);
    }
};

inline bool storylandKillFrenzyMatches(const StorylandKillFrenzy& frenzy,
    int32_t model,int32_t nativeWeapon,bool headshot) {
    if(!frenzy.active()||nativeWeapon!=frenzy.nativeWeapon||
        (frenzy.onlyHeadshots&&!headshot))return false;
    bool filtered=false;
    for(const int32_t target:frenzy.models)if(target>=0) {
        filtered=true;
        if(target==model)return true;
    }
    return !filtered;
}

inline int32_t storylandLcsRampagePhysicalWeapon(int32_t weapon) {
    if (weapon == 42) return 23;
    if (weapon == 39 || weapon == 40) return 0;
    return weapon;
}

inline bool storylandLcsKillFrenzyMatches(const StorylandKillFrenzy& frenzy,
    int32_t model, int32_t weapon, bool headshot, bool vehicle) {
    if (!frenzy.active()) return false;
    bool modelMatches = frenzy.models[0] == (vehicle ? -2 : -1);
    for (const int32_t target : frenzy.models) modelMatches |= target == model;
    if (!modelMatches) return false;
    if (vehicle) return true;
    if (frenzy.onlyHeadshots && !headshot) return false;
    return weapon == frenzy.nativeWeapon ||
        (weapon == 42 && frenzy.nativeWeapon == 23) ||
        (weapon == 39 && frenzy.nativeWeapon == 40) ||
        (weapon == 40 && frenzy.nativeWeapon == 39) ||
        (weapon == 31 && frenzy.nativeWeapon == 15);
}

inline int32_t storylandBetaWeaponId(StorylandBetaWeapon feature) {
    switch(feature) {
    case StorylandBetaWeapon::TearGas:return 16;
    case StorylandBetaWeapon::LandMine:return 14;
    case StorylandBetaWeapon::SeaMine:return 15;
    default:return -1;
    }
}

inline const char* storylandBetaWeaponName(StorylandBetaWeapon feature) {
    switch(feature) {
    case StorylandBetaWeapon::TearGas:return "Tear gas";
    case StorylandBetaWeapon::LandMine:return "Land mines";
    case StorylandBetaWeapon::SeaMine:return "Sea mines";
    default:return "Unknown weapon";
    }
}
struct StorylandWeaponPickup {
    StorylandNativePoint position;
    size_t weapon=0;
    int ammunition=30;
    float respawn=0;
    bool randomlyGenerated=false;
};
struct StorylandWeaponTrace {
    StorylandNativePoint from,to;
    float remaining=0.075f;
    float lifetime=0.075f;
    int32_t nativeWeapon=-1;
    bool vcs=false;
    float radiusScale=0.4f;
    uint8_t opacity=150;
};
enum class StorylandProjectileKind { Grenade, Molotov, Rocket, TearGas, LandMine, SeaMine };

struct StorylandWeaponProjectile {
    StorylandNativePoint position,velocity;
    float remaining=4,damage=0,radius=3;
    int owner=-1;
    bool gravity=true;
    StorylandProjectileKind kind=StorylandProjectileKind::Grenade;
    size_t weapon=SIZE_MAX;
    float age=0,trailTime=0;
    float yaw=0;
    bool settled=false;
    uint8_t betaFeature=0;
    uint64_t sequence=0;
    int32_t scriptSlot=-1;
};

inline bool storylandProjectileIsMine(StorylandProjectileKind kind) {
    return kind==StorylandProjectileKind::LandMine||kind==StorylandProjectileKind::SeaMine;
}

inline float storylandTearGasExposure(uint32_t endMilliseconds,float seconds) {
    // Native18153C gates the one-point-per-step damage on timer bit0x200.
    // Integrate that duty cycle at the host's50Hz conversion across long frames.
    const uint64_t elapsed=uint64_t(std::max(0.0f,std::round(seconds*1000.0f)));
    const uint64_t end=uint64_t(endMilliseconds)+(uint64_t(1)<<32);
    const auto active=[](uint64_t time) {return time/1024*512+std::max<int64_t>(0,int64_t(time%1024)-512);};
    return float(active(end)-active(end-std::min(end,elapsed)))*.001f;
}

inline bool storylandMineTouchesBounds(const StorylandNativePoint& point,
    const std::array<float,16>& matrix,const StorylandNativePoint& minimum,
    const StorylandNativePoint& maximum) {
    const float x=point.x-matrix[12],y=point.y-matrix[13],z=point.z-matrix[14];
    const std::array<float,3> local{x*matrix[0]+y*matrix[1]+z*matrix[2],
        x*matrix[4]+y*matrix[5]+z*matrix[6],x*matrix[8]+y*matrix[9]+z*matrix[10]};
    const std::array<float,3> lower{minimum.x,minimum.y,minimum.z};
    const std::array<float,3> upper{maximum.x,maximum.y,maximum.z};
    for(size_t axis=0;axis<3;++axis) {
        const size_t basis=axis*4;
        const float length=matrix[basis]*matrix[basis]+matrix[basis+1]*matrix[basis+1]+matrix[basis+2]*matrix[basis+2];
        if(!std::isfinite(length)||length<.000001f||!std::isfinite(lower[axis])||
            !std::isfinite(upper[axis])||lower[axis]>upper[axis]||!std::isfinite(local[axis])||local[axis]<lower[axis]-1.0f||
            local[axis]>upper[axis]+1.0f)return false;
    }
    return true;
}

enum class StorylandMuzzleOwner : uint8_t { None, Player, AmbientPed, MissionPed, Vehicle };

enum class StorylandParticleKind { Flame, Smoke, Exhaust, EngineSmoke, Gas, Steam, Water, Flash, Spark, Debris, Leaves, Blood, BloodPool, Headlight, Taillight,
    MuzzleFlash, Explosion, Dollar, PickupGlow, WheelDirt, FootDust };

struct StorylandNativeParticlePresentation {
    bool enabled=false;
    uint32_t flags=0;
    uint8_t intensity=255,alpha=255;
    int16_t intensityStep=0,alphaStep=0;
    int16_t rotation=0,rotationStep=0;
    int32_t rotationError=0;
    float fractionalTicks=0,trailLength=1;
    std::array<float,2> textureStretch{};
};

struct StorylandRuntimeParticle {
    StorylandNativePoint position,velocity;
    StorylandParticleKind kind=StorylandParticleKind::Smoke;
    float remaining=1,lifetime=1,size=0.25f;
    uint16_t animationCounter=0;
    uint8_t nativeFrame=0,nativeFirstFrame=0;
    bool nativePhysics=false,nativeVcs=false;
    float nativeAnimationTicks=0;
    int32_t nativeType=-1;
    std::array<uint8_t,4> nativeColor{255,255,255,255};
    float nativeExpansion=0,nativeGravity=0;
    uint32_t nativeFriction=1;
    uint16_t nativeFrameThreshold=0;
    uint8_t nativeLastFrame=0;
    bool nativeFrameLoop=false;
    uint8_t betaFeature=0;
    // Presentation events are sent once. Received particles never echo to their owner.
    bool coopSent=false;
    StorylandMuzzleOwner muzzleOwner=StorylandMuzzleOwner::None;
    int32_t muzzleHandle=-1,muzzleWeapon=-1;
    uint64_t muzzleIdentity=0;
    bool swimmingParticle=false,waterSurface=false,waterAnchoredSplash=false;
    float waterHeading=0;
    StorylandNativePoint bloodSurfaceNormal{0,0,1};
    float bloodSurfaceRotation=0,bloodPoolTargetSize=0;
    int bloodSurfaceArea=-1;
    bool corpseBloodPool=false;
    StorylandNativeParticlePresentation nativePresentation;
};

struct StorylandNativeParticleDescriptor {
    int32_t type=-1;
    StorylandParticleKind kind=StorylandParticleKind::Smoke;
    float size=0,expansion=0,gravity=0;
    uint32_t lifetime=0,friction=1;
    std::array<uint8_t,4> color{};
    uint8_t firstFrame=0,lastFrame=0;
    uint16_t frameThreshold=0;
    bool frameLoop=false;
    StorylandNativeParticlePresentation nativePresentation;
};

// GAME.DTZ +0xBC points at 90 native CParticleSystemData records, stride0x84.
// The host renders the supported sprite families and consumes their actual
// size, colour, lifetime, gravity, friction and frame configuration.
inline bool storylandNativeParticleKind(int32_t type,StorylandParticleKind& kind) {
    if(type>=0&&type<=2)kind=StorylandParticleKind::Spark;
    else if(type==3||type==4)kind=StorylandParticleKind::WheelDirt;
    else if(type==9||type==10||type==25||type==62||type==63||type==64)kind=StorylandParticleKind::Debris;
    else if(type==51)kind=StorylandParticleKind::FootDust;
    else if(type==60)kind=StorylandParticleKind::Leaves;
    else if(type>=6&&type<=8)kind=StorylandParticleKind::Blood;
    else if(type==13||type==14||type==27||type==74)kind=StorylandParticleKind::Flame;
    else if(type==15||type==16)kind=StorylandParticleKind::MuzzleFlash;
    else if(type==55||type==56)kind=StorylandParticleKind::EngineSmoke;
    else if(type==65)kind=StorylandParticleKind::Exhaust;
    else if((type>=17&&type<=21)||type==52||type==53||type==57||type==58||type==59||type==61||
        type==66||type==67||type==68||type==73||type==86||type==89)kind=StorylandParticleKind::Smoke;
    else if(type==22||(type>=28&&type<=33)||type==49||type==88)kind=StorylandParticleKind::Steam;
    else if(type==23||type==24)kind=StorylandParticleKind::Gas;
    else if((type>=40&&type<=43)||type==79)kind=StorylandParticleKind::Explosion;
    else if(type==54||type==80||type==87)kind=StorylandParticleKind::Flash;
    else if(type==5||type==12||type==26||(type>=34&&type<=39)||(type>=44&&type<=48)||
        type==50||(type>=75&&type<=78))kind=StorylandParticleKind::Water;
    else return false;
    return true;
}

inline int32_t storylandRetailParticleType(StorylandParticleKind kind) {
    switch(kind) {
    case StorylandParticleKind::Spark:return 0;
    case StorylandParticleKind::WheelDirt:return 3;
    case StorylandParticleKind::FootDust:return 51;
    case StorylandParticleKind::Leaves:return 60;
    case StorylandParticleKind::Blood:return 6;
    case StorylandParticleKind::Flame:return 13;
    case StorylandParticleKind::Smoke:return 20;
    case StorylandParticleKind::Exhaust:return 65;
    case StorylandParticleKind::EngineSmoke:return 55;
    case StorylandParticleKind::Gas:return 23;
    case StorylandParticleKind::Steam:return 28;
    case StorylandParticleKind::Water:return 26;
    case StorylandParticleKind::Explosion:return 40;
    case StorylandParticleKind::Flash:return 54;
    case StorylandParticleKind::MuzzleFlash:return 15;
    case StorylandParticleKind::Debris:return 9;
    default:return -1;
    }
}

inline bool storylandReadNativeParticleDescriptor(const std::vector<uint8_t>& bytes,
    int32_t type,StorylandNativeParticleDescriptor& result) {
    StorylandNativeParticleDescriptor value;
    if(type<0||type>=90||!storylandNativeParticleKind(type,value.kind)||bytes.size()<0xC0)return false;
    const auto word=[&](size_t offset) {return uint32_t(bytes[offset])|(uint32_t(bytes[offset+1])<<8)|
        (uint32_t(bytes[offset+2])<<16)|(uint32_t(bytes[offset+3])<<24);};
    // Retail CParticleSystemData is not identical between the two games.
    // VCS: root +0xBC, 0x84-byte records. LCS: root +0xB8, 0x94-byte
    // records with an extra 0x10-byte region before the frame metadata.
    // Both layouts were checked against the native GUNFLASH row in GAME.DTZ.
    const auto validRecord=[&](size_t header,size_t stride)->bool {
        if(header+4>bytes.size())return false;
        const size_t table=word(header);
        return table!=0&&table<=bytes.size()&&
            size_t(type)<(bytes.size()-table)/stride&&
            word(table+size_t(type)*stride)==uint32_t(type);
    };
    size_t table=0,stride=0,extra=0;
    if(validRecord(0xBC,0x84)) {
        table=word(0xBC);stride=0x84;
    } else if(validRecord(0xB8,0x94)) {
        table=word(0xB8);stride=0x94;extra=0x10;
    } else return false;
    const size_t record=table+size_t(type)*stride;
    const auto real=[&](size_t offset) {const uint32_t bits=word(record+offset);float value=0;
        std::memcpy(&value,&bits,sizeof(value));return value;};
    const auto half=[&](size_t offset) {return uint16_t(bytes[record+offset])|
        (uint16_t(bytes[record+offset+1])<<8);};
    value.type=type;value.size=real(0x1C);value.expansion=real(0x20);
    value.gravity=real(0x38+extra);
    value.friction=word(record+0x3C+extra);value.lifetime=word(record+0x40+extra);
    value.firstFrame=bytes[record+0x30+extra];
    value.lastFrame=bytes[record+0x31+extra];
    value.frameThreshold=uint16_t(bytes[record+0x32+extra])|
        (uint16_t(bytes[record+0x33+extra])<<8);
    value.frameLoop=(word(record+0x5C+extra)&0x40u)!=0;
    value.color={bytes[record+0x60+extra],bytes[record+0x61+extra],
        bytes[record+0x62+extra],bytes[record+0x26+extra]};
    const bool nativeWater=value.kind==StorylandParticleKind::Water&&(extra==0||type<75);
    const bool nativeVehicleEffect=type==13||type==14||type==27||type==33||
        (type>=40&&type<=43)||(type>=55&&type<=58)||(type>=65&&type<=67)||(extra==0&&type==74);
    if(nativeWater||nativeVehicleEffect) {
        auto& presentation=value.nativePresentation;
        presentation.enabled=true;presentation.flags=word(record+0x5C+extra);
        presentation.intensity=bytes[record+0x26+extra];presentation.alpha=bytes[record+0x2A+extra];
        const uint16_t intensityTime=half(0x24+extra),alphaTime=half(0x2C+extra);
        presentation.intensityStep=int16_t(half(0x28+extra));
        presentation.alphaStep=int16_t(half(0x2E + extra));
        if(intensityTime)presentation.intensityStep=int16_t(presentation.intensityStep/intensityTime);
        if(alphaTime)presentation.alphaStep=int16_t(presentation.alphaStep/alphaTime);
        presentation.rotationStep=int16_t(half(0x34+extra));
        presentation.rotationError=int32_t(word(record+0x50+extra));
        presentation.trailLength=real(0x58+extra);
        presentation.textureStretch={real(0x70+extra),real(0x74+extra)};
        value.color[3]=presentation.alpha;
        if(!std::isfinite(presentation.trailLength)||!std::isfinite(presentation.textureStretch[0])||
            !std::isfinite(presentation.textureStretch[1])||presentation.rotationError<0||
            presentation.rotationError>32767)return false;
    }
    if(!std::isfinite(value.size)||!std::isfinite(value.expansion)||!std::isfinite(value.gravity)||
        value.size<0||value.lifetime==0||value.lifetime>3600000||value.firstFrame>value.lastFrame)return false;
    result=value;return true;
}

inline void storylandInitializeNativeParticlePresentation(StorylandRuntimeParticle& particle,
    const StorylandNativeParticleDescriptor& descriptor) {
    auto& presentation=particle.nativePresentation;
    presentation=descriptor.nativePresentation;
    if(!presentation.enabled||(!presentation.rotationStep&&!presentation.rotationError))return;
    uint32_t seed=uint32_t(particle.nativeType)*0x9E3779B9u;
    for(float component:{particle.position.x,particle.position.y,particle.position.z,
        particle.velocity.x,particle.velocity.y,particle.velocity.z,particle.lifetime}) {
        uint32_t bits=0;std::memcpy(&bits,&component,sizeof(bits));
        seed^=bits+0x9E3779B9u+(seed<<6)+(seed>>2);
    }
    seed^=seed>>16;seed*=0x7FEB352Du;seed^=seed>>15;seed*=0x846CA68Bu;seed^=seed>>16;
    if(seed&1u)presentation.rotationStep=int16_t(-presentation.rotationStep);
    if(presentation.rotationError)
        presentation.rotationStep=int16_t(int(presentation.rotationStep)+
            int((seed>>1)%uint32_t(presentation.rotationError*2))-presentation.rotationError);
}

inline void storylandAdvanceNativeParticlePresentation(StorylandRuntimeParticle& particle,float seconds) {
    auto& presentation=particle.nativePresentation;
    if(!presentation.enabled||!std::isfinite(seconds)||seconds<=0)return;
    presentation.fractionalTicks+=std::min(seconds,60.0f)*50.0f;
    const unsigned ticks=unsigned(presentation.fractionalTicks+.00001f);
    presentation.fractionalTicks=std::max(0.0f,presentation.fractionalTicks-float(ticks));
    presentation.intensity=uint8_t(std::clamp(int(presentation.intensity)-int(ticks)*presentation.intensityStep,0,255));
    presentation.alpha=uint8_t(std::clamp(int(presentation.alpha)-int(ticks)*presentation.alphaStep,0,255));
    presentation.rotation=int16_t(uint16_t(int(presentation.rotation)+int(ticks)*presentation.rotationStep));
    if(presentation.alpha==0)particle.remaining=0;
}

// Retail GUNFLASH and EXPLOSION_MEDIUM descriptors have thresholds51/1 and
// final frames3/5. CParticle::Update advances when the old counter exceeds the
// threshold; both descriptors clamp at the final frame (no0x40 loop flag).
inline void storylandAdvanceParticleTexture(StorylandRuntimeParticle& particle,float deltaSeconds=0.02f) {
    if(!particle.nativePhysics&&particle.kind!=StorylandParticleKind::MuzzleFlash&&particle.kind!=StorylandParticleKind::Explosion)return;
    if(particle.nativePhysics&&(particle.nativeVcs||particle.nativePresentation.enabled)) {
        if(!particle.nativeFrameThreshold||!std::isfinite(deltaSeconds)||deltaSeconds<=0)return;
        // Native VCS 0x1EAC70 compares the old counter and advances at most
        // once per simulation update; it is a tick count, not milliseconds.
        particle.nativeAnimationTicks+=std::min(deltaSeconds,.25f)*50.0f;
        const unsigned ticks=unsigned(particle.nativeAnimationTicks+0.00001f);
        particle.nativeAnimationTicks=std::max(0.0f,particle.nativeAnimationTicks-float(ticks));
        for(unsigned tick=0;tick<ticks;++tick) {
            if(particle.animationCounter>particle.nativeFrameThreshold) {
                particle.animationCounter=0;
                if(particle.nativeFrame<particle.nativeLastFrame)++particle.nativeFrame;
                else if(particle.nativeFrameLoop)particle.nativeFrame=particle.nativeFirstFrame;
            } else ++particle.animationCounter;
        }
        return;
    }
    const uint16_t threshold=particle.nativePhysics?particle.nativeFrameThreshold:
        particle.kind==StorylandParticleKind::MuzzleFlash?51:1;
    const uint8_t last=particle.nativePhysics?particle.nativeLastFrame:
        particle.kind==StorylandParticleKind::MuzzleFlash?3:5;
    if(particle.nativePhysics&&threshold==0)return;
    if(!std::isfinite(deltaSeconds)||deltaSeconds<=0)return;
    const unsigned increment=unsigned(std::clamp(std::lround(deltaSeconds*1000.0f),1l,250l));
    unsigned counter=unsigned(particle.animationCounter)+increment;
    const unsigned period=unsigned(threshold)+1u;
    unsigned steps=std::min(32u,counter/period);
    counter-=steps*period;
    particle.animationCounter=uint16_t(std::min(counter,65535u));
    while(steps--) {
        particle.nativeFrame=particle.nativePhysics&&particle.nativeFrameLoop&&particle.nativeFrame>=last?0:
            uint8_t(std::min(unsigned(last),unsigned(particle.nativeFrame)+1u));
    }
}

// Missing frames can use a related native sprite before the procedural fallback.
inline std::array<const char*,3> storylandParticleTextureCandidates(StorylandParticleKind kind,unsigned frame,
    int32_t nativeType=-1,bool vcs=false) {
    // VCS 0x1E8D20 selects the five rubber textures loaded at 0x1E8358.
    // Other smoke descriptors retain their existing texture selection.
    static const char* rubberFrames[]={"rubber1","rubber2","rubber3","rubber4","rubber5"};
    if(nativeType==66)return {rubberFrames[std::min(4u,frame)],nullptr,nullptr};
    // VCS 0x1E8D18 maps EXHAUST_FUMES to the smokeII_3 raster.
    if(nativeType==33||nativeType==65)return {"smokeii_3",nullptr,nullptr};
    if(nativeType>=55&&nativeType<=58)return {"cloudmasked",nullptr,nullptr};
    if(nativeType==67)return {"collisionsmoke",nullptr,nullptr};
    if(nativeType==13||nativeType==27)return {"flame1",nullptr,nullptr};
    if(nativeType==14||(vcs&&nativeType==74))return {"flame5",nullptr,nullptr};
    // VCS 0x1E8CA4 selects the smoke1..5 raster array for EXTINGUISH_STEAM.
    if(vcs&&nativeType==49) {
        static const char* smokeFrames[]={"smoke1","smoke2","smoke3","smoke4","smoke5"};
        return {smokeFrames[std::min(4u,frame)],nullptr,nullptr};
    }
    if(nativeType==5||nativeType==12||nativeType==48||(vcs&&nativeType==78)) {
        static const char* waterSmokeFrames[]={"smoke1","smoke2","smoke3","smoke4","smoke5"};
        return {waterSmokeFrames[vcs&&nativeType==78?4u:std::min(4u,frame)],nullptr,nullptr};
    }
    if(nativeType==34)return {"raindrop4",nullptr,nullptr};
    if(nativeType==35)return {"rainsmall",nullptr,nullptr};
    if(nativeType==45)return {"boatwake2",nullptr,nullptr};
    if(nativeType==46)return {rubberFrames[std::min(4u,frame)],nullptr,nullptr};
    // VCS particle raster table at 0x1E8BD4, 0x1E8C14 and 0x1E8D7C.
    if(vcs&&nativeType==75) {
        static const char* swimSplashFrames[]={"splash1","splash_up1","splash_up2"};
        return {swimSplashFrames[std::min(2u,frame)],nullptr,nullptr};
    }
    if(vcs&&nativeType==76)return {"swimwake",nullptr,nullptr};
    if(vcs&&nativeType==77)return {"swimwake2",nullptr,nullptr};
    // BALLOON_EXP shares the four carsplash rasters; its native descriptor
    // supplies the red tint rather than using fire-explosion sprites.
    if(nativeType==26||nativeType==44||nativeType==47||nativeType==50||(vcs&&nativeType==79)) {
        static const char* carSplashFrames[]={"carsplash_01","carsplash_02","carsplash_03","carsplash_04"};
        return {carSplashFrames[std::min(3u,frame)],nullptr,nullptr};
    }
    if(nativeType==36||nativeType==37) {
        static const char* splashFrames[]={"splash1","splash2","splash3","splash4","splash5"};
        return {splashFrames[std::min(4u,frame)],nullptr,nullptr};
    }
    if(nativeType==38) {
        static const char* splashUpFrames[]={"splash_up1","splash_up2"};
        return {splashUpFrames[std::min(1u,frame)],nullptr,nullptr};
    }
    if(nativeType==39) {
        static const char* waterSprayFrames[]={"waterspray1","waterspray2","waterspray3"};
        return {waterSprayFrames[std::min(2u,frame)],nullptr,nullptr};
    }
    static const char* gunFrames[]={"gunflash1","gunflash2","gunflash3","gunflash4"};
    static const char* explosionFrames[]={"explo01","explo02","explo03","explo04","explo05","explo06"};
    if(kind==StorylandParticleKind::MuzzleFlash)
        return {gunFrames[std::min(3u,frame)],"muzzleflash","gunflash1"};
    if(kind==StorylandParticleKind::Explosion)
        return {explosionFrames[std::min(5u,frame)],"explo01","flame1"};
    static const char* flameFrames[]={"flame1","flame2","flame3","flame4","flame5"};
    if(kind==StorylandParticleKind::Flame)return {flameFrames[std::min(4u,frame)],"flame1","flame5"};
    if(kind==StorylandParticleKind::PickupGlow)return {"coronastar","corona",nullptr};
    if(kind==StorylandParticleKind::Smoke)return {"smokeii_3","smoke1","smoke3"};
    if(kind==StorylandParticleKind::Exhaust)return {"smoke1","smoke3","smokeii_3"};
    if(kind==StorylandParticleKind::EngineSmoke)return {"smokeii_3","smoke3","smoke1"};
    if(kind==StorylandParticleKind::Gas)return {"smokeii_3","smoke1",nullptr};
    if(kind==StorylandParticleKind::Steam)return {"smoke4","smokeii_3","smoke1"};
    if(kind==StorylandParticleKind::Water)return {"splash1","splash3","waterspray1"};
    if(kind==StorylandParticleKind::Blood)return {"blood","bloodsplat2","bloodpool_64"};
    if(kind==StorylandParticleKind::BloodPool)return {"bloodpool_64","bloodsplat2",nullptr};
    if(kind==StorylandParticleKind::WheelDirt||kind==StorylandParticleKind::FootDust)
        return {"smokeii_3","smoke1","smoke3"};
    if(kind==StorylandParticleKind::Leaves)return {"gameleaf01_64","gameleaf02_64",nullptr};
    if(kind==StorylandParticleKind::Dollar)return {"dollar",nullptr,nullptr};
    return {nullptr,nullptr,nullptr};
}
struct StorylandRuntimeEffect {
    StorylandNativePoint position,direction;
    StorylandParticleKind kind=StorylandParticleKind::Smoke;
    float remaining=1,radius=1,damage=0,emission=0,damageClock=0,age=0;
    int owner=-1;
    int vehicle=-1; // Source vehicle for fire or explosion presentation.
    bool permanent=false;
    int32_t scriptFireHandle=-1;
    int32_t nativeWeapon=17; // Native fire damage; emitted weapon effects override this.
    uint8_t betaFeature=0;
    uint64_t sourceProjectile=0;
    int32_t vehicleExplosionType=-1;
    uint32_t explosionTick=0,explosionRandom=1,explosionNextFlashMilliseconds=0;
    double explosionFractionalTicks=0;
};
inline bool storylandWorldParticleKind(int subtype,StorylandParticleKind& kind) {
    if(subtype==9||subtype==10||subtype==15||subtype==18)kind=StorylandParticleKind::Flame;
    else if(subtype==4||subtype==16||subtype==17)kind=StorylandParticleKind::Smoke;
    else if((subtype>=5&&subtype<=8)||subtype==13||subtype==14)kind=StorylandParticleKind::Water;
    else if((subtype>=0&&subtype<=3)||subtype==11||subtype==12)kind=StorylandParticleKind::Steam;
    else return false;
    return true;
}
