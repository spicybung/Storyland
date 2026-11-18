#pragma once
#include <cstdint>
#include "src/storyland/storyland_vehicle_transition.h"
#include "src/re/reVCS/runtime/storyland_vcs_retail_quirks.h"
#include "src/re/reVCS/vehicles/storyland_vcs_jetski_entry.h"
#include "src/re/reVCS/vehicles/storyland_vcs_jetski_pose.h"
#include "src/re/reVCS/vehicles/storyland_vcs_jetski_frame.h"
#include "src/re/reVCS/vehicles/storyland_vcs_helicopter_door.h"
#include "src/storyland/storyland_forklift_state.h"
#include "src/leeds/leeds_native_world.h"
#include <memory>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstddef>
#include <vector>

// Native CVehicleModelInfo layout: aap/librwgta tools/storiesconv/leedsgta.h.
// LCS vehicle types: reLCS src/modelinfo/VehicleModelInfo.h.
enum class StorylandVehicleDomain { Unknown, Road, Water, Other, Air };

inline bool storylandLcsVehicleComponentRules(const std::vector<uint8_t>& data,
    uint32_t model, uint32_t& rules) {
    const auto word = [&](size_t offset) -> uint32_t {
        if (offset > data.size() || data.size() - offset < 4) return 0;
        return uint32_t(data[offset]) | (uint32_t(data[offset+1]) << 8) |
            (uint32_t(data[offset+2]) << 16) | (uint32_t(data[offset+3]) << 24);
    };
    const uint32_t table = word(0x3C);
    if (!table || model >= word(0x38) || uint64_t(table) + uint64_t(model)*4 + 4 > data.size())
        return false;
    const uint32_t info = word(size_t(table) + size_t(model)*4);
    if (!info || uint64_t(info) + 0xA4 > data.size() || data[size_t(info)+0x10] != 6)
        return false;
    // Native CreateInstance2B7DD4 excludes this model before consuming overrides.
    if (std::memcmp(data.data() + size_t(info) + 0x2E, "POLICAR", 8) == 0) return false;
    rules = word(size_t(info) + 0xA0);
    return true;
}

template<class Random>
inline std::array<int32_t, 2> storylandChooseLcsVehicleComponents(int count, uint32_t rules,
    const std::array<int8_t, 2>& requested, int16_t previousWeather, int16_t currentWeather,
    Random&& random) {
    std::array<int32_t, 2> selected{-1, -1};
    const auto validRule = [&](unsigned rule) {
        return rule != 2 || previousWeather == 2 || currentWeather == 2;
    };
    const auto randomIndex = [&](unsigned limit) -> unsigned {
        return unsigned((uint64_t(random() & 0x7FFFFFFFu)*limit) >> 31);
    };
    const auto chooseRule = [&](unsigned rule, unsigned components) -> int32_t {
        unsigned choices = 0;
        for (unsigned value = components; value; value >>= 4)
            if ((value & 15u) != 15u) ++choices;
        if (rule == 1 || rule == 2) {
            return int32_t((components >> (4*randomIndex(choices))) & 15u);
        }
        if (rule == 3) {
            const unsigned index = randomIndex(choices+1);
            return index ? int32_t((components >> (4*(index-1))) & 15u) : -1;
        }
        // PS2 uses five, including the native rule4 omission of component5.
        if (rule == 4) return int32_t(randomIndex(5));
        return -1;
    };
    const unsigned firstRule = (rules >> 12) & 15u;
    std::array<int32_t, 8> unused{};
    size_t unusedCount = 0;
    for (int component = 0; component < std::min(count, 8); ++component) {
        bool used = false;
        for (unsigned half : {0u, 16u}) {
            const unsigned rule = (rules >> (half+12)) & 15u;
            if (rule && validRule(rule)) {
                for (unsigned index = 0; index < 3; ++index)
                    if (((rules >> (half+index*4)) & 15u) == unsigned(component)) used = true;
            }
        }
        if (!used) unused[unusedCount++] = component;
    }
    for (unsigned slot = 0; slot < 2; ++slot) {
        if (requested[slot] != -2) {
            selected[slot] = requested[slot];
            continue;
        }
        const unsigned rule = (rules >> (16*slot+12)) & 15u;
        if (rule && validRule(rule)) selected[slot] = chooseRule(rule, (rules >> (16*slot)) & 0xFFFu);
        else if ((slot == 0 || (firstRule && validRule(firstRule))) && randomIndex(3) < 2 && unusedCount)
            selected[slot] = unused[randomIndex(unsigned(unusedCount))];
    }
    return selected;
}

inline int storylandLcsVehicleHandlingClass(uint32_t flags) {
    switch(flags&0x000F0000u) {
    case 0:return 1;
    case 0x00010000u:return 2;
    case 0x00020000u:return 3;
    case 0x00040000u:return 5;
    case 0x00080000u:return 4;
    default:return 0;
    }
}

inline bool storylandLcsVehicleHandlingFlags(const std::vector<uint8_t>& data,
    uint32_t modelId,uint32_t& flags) {
    const auto word=[&](size_t offset) -> uint32_t {
        if(offset>data.size()||data.size()-offset<4)return 0;
        return uint32_t(data[offset])|(uint32_t(data[offset+1])<<8)|
            (uint32_t(data[offset+2])<<16)|(uint32_t(data[offset+3])<<24);
    };
    const uint32_t table=word(0x3C),manager=word(0xA8);
    if(!table||!manager||modelId>=word(0x38)||
        uint64_t(table)+uint64_t(modelId)*4+4>data.size())return false;
    const uint32_t info=word(size_t(table)+size_t(modelId)*4);
    if(!info||uint64_t(info)+0x44>data.size()||data[size_t(info)+0x10]!=6)return false;
    const uint16_t handling=uint16_t(data[size_t(info)+0x42])|
        (uint16_t(data[size_t(info)+0x43])<<8);
    if(handling>=81)return false;
    const uint64_t row=uint64_t(manager)+0x20+uint64_t(handling)*0xF0;
    if(row+0xF0>data.size()||word(size_t(row)+0x10)!=handling)return false;
    // GetVehicleAppearance reads CHandlingData::Flags. It does not require
    // an imported render model, a collision hull, or supported driving physics.
    flags=word(size_t(row)+0xD0);return true;
}

inline StorylandVehicleDomain storylandVehicleDomain(const std::vector<uint8_t>& data,
    uint32_t modelId, bool vcs) {
    const auto word=[&](size_t offset) -> uint32_t {
        if(offset>data.size() || data.size()-offset<4) return 0;
        return uint32_t(data[offset]) | (uint32_t(data[offset+1])<<8) |
            (uint32_t(data[offset+2])<<16) | (uint32_t(data[offset+3])<<24);
    };
    const uint32_t table=word(0x3C);
    if(!table || modelId>=word(0x38) || uint64_t(table)+uint64_t(modelId)*4+4>data.size())
        return StorylandVehicleDomain::Unknown;
    const uint32_t info=word(size_t(table)+size_t(modelId)*4);
    const size_t typeOffset=vcs?0x54:0x38;
    if(!info || uint64_t(info)+typeOffset+4>data.size() || data[size_t(info)+0x10]!=6)
        return StorylandVehicleDomain::Unknown;
    const uint32_t type=word(size_t(info)+typeOffset);
    if (!vcs) {
        uint32_t flags = 0;
        if (storylandLcsVehicleHandlingFlags(data, modelId, flags) &&
            ((flags & 0x000F0000u) == 0x00020000u || (flags & 0x000F0000u) == 0x00040000u))
            return StorylandVehicleDomain::Air;
    }
    // VCS inserts additional subclasses; its plane type is 5, not the LCS bike type.
    if(type==0 || (vcs ? (type==6 || type==8 || type==9) : type==5))
        return StorylandVehicleDomain::Road;
    if(type==1 || (vcs ? type==2 : type==6)) return StorylandVehicleDomain::Water;
    if (vcs && (type == 4 || type == 5)) return StorylandVehicleDomain::Air;
    return StorylandVehicleDomain::Other;
}

struct StorylandVehicleGear {
    float maximum=0,upshift=0,downshift=0;
};
struct StorylandVehicleWheelDefinition {
    std::array<float,3> previewPivot{};
    float nativeZ=0,radius=0,upperZ=0,lowerZ=0,springLength=0,lineLength=0,restZ=0;
    bool front=false;
    std::array<float,16> previewFrame{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::vector<std::array<float,3>> tyreContactGeometry;
    bool tyreUsesWheelFrame=false;
};
struct StorylandVehicleBoatBuoyancy {
    bool valid=false;
    float coefficient=0;
    std::array<float,9> sliceWeights{};
    std::array<float,3> minimum{},maximum{};
};
struct StorylandVehicleDefinition {
    std::shared_ptr<const StorylandNativeCollisionModel> collision;
    uint32_t modelId=UINT32_MAX;
    StorylandVehicleDomain domain=StorylandVehicleDomain::Unknown;
    float wheelScale=1,rearWheelScale=1,mass=1500,traction=1;
    bool hasNativeWheelDimensions=false;
    bool hasNativeHandling=false;
    bool hasNativeBikeHandling=false;
    float bikeMaximumLeanRadians=0,bikeLeanRetention=0;
    bool hasNativeBikeDynamics=false;
    float bikeLeanForwardCenter=0,bikeLeanForwardForce=0;
    float bikeLeanBackCenter=0,bikeLeanBackForce=0;
    float bikeSpeedSteer=0,bikeSlipSteer=0;
    float bikeWheelieSine=0,bikeStoppieSine=0,bikeWheelieSteer=0;
    float bikeWheelieStability=0,bikeStoppieStability=0;
    bool hasNativeBoatHandling=false;
    float boatThrustY=0,boatThrustZ=0,boatThrustAppZ=0;
    float boatAquaplaneForce=0,boatAquaplaneLimit=0,boatAquaplaneOffset=0;
    std::array<float,3> boatMoveResistance{},boatTurnResistance{};
    StorylandVehicleBoatBuoyancy boatBuoyancy;
    float turnMass=0,dragMultiplier=0,engineAcceleration=0,engineInertia=0;
    std::array<float,3> centerOfMass{};
    float maximumVelocity=0,maximumTransmissionVelocity=0,maximumAiVelocity=0,reverseVelocity=0;
    float brakeDeceleration=0,brakeBias=0,steeringLock=30;
    float tractionLoss=1,tractionBias=0.5f;
    std::array<float,6> tyreAdhesion{6,3.6f,4.5f,3.2f,2,2.4f};
    float suspensionForce=0,suspensionDamping=0;
    float suspensionUpper=0,suspensionLower=0,suspensionBias=0.5f;
    float suspensionAntiDive=0,collisionDamage=0,seatOffset=0;
    uint32_t handlingFlags=0,modelFlags=0;
    bool hasVcsModelInfoFlags=false;
    uint32_t vcsModelInfoFlags=0;
    bool hasVcsSoundFlags=false;
    uint32_t vcsSoundFlags=0;
    bool hasNativeBuoyancyCoefficient=false;
    float nativeBuoyancyCoefficient=0;
    uint8_t driveType=0,engineType=0,numberOfGears=0,transmissionFlags=0;
    int handlingId=-1,vehicleClass=-1,nativeType=-1,wheelModelId=-1;
    uint32_t handlingPointer=0;
    uint32_t flyingHandlingPointer=0;
    bool hasNativeFlyingHandling=false;
    std::array<float,24> flyingHandling{};
    bool vcsModelLayout=true;
    bool hasNativeAudio=false;
    uint8_t audioBank=0,doorSoundType=0;
    uint32_t accelerationSample=0,hornSample=0,hornFrequency=0;
    uint32_t sirenSample=0,sirenFrequency=0;
    std::array<StorylandVehicleGear,6> gears{};
    std::array<std::array<float,3>,15> dummyPositions{};
    bool hasExplosionBurstOrigin=false,hasExplosionJetOrigin=false;
    std::array<float,3> explosionBurstOrigin{},explosionJetOrigin{};
    bool hasSirenForkFrame=false;
    std::array<float,16> sirenForkFrame{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::array<float,16> previewRoot{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    float entrySeconds=1.2f,exitSeconds=1.2f,jackSeconds=1.0f;
    float rightEntrySeconds=1.2f,rightExitSeconds=1.2f,rearExitSeconds=0;
    float frontEntrySeconds=0,shuffleSeconds=0;
    std::array<float,2> pickupSeconds{},pullupSeconds{},crawlExitSeconds{},movingExitSeconds{};
    std::array<int32_t,2> movingExitAssociations{-1,-1},movingExitGroups{-1,-1};
    StorylandVcsJetSkiChassis vcsJetSkiChassis;
    storyland_vcs_jetski_pose::Metadata vcsJetSkiAnimations;
    bool hasVcsJetSkiEntryRoots=false;
    std::array<std::array<float,3>,2> vcsJetSkiEntryRoots{};
    bool hasNativeMotorcycleEntryRoot=false;
    std::array<float,3> nativeMotorcycleEntryRoot{};
    bool hasNativeDoorRoots=false;
    std::array<std::array<float,3>,2> nativeDoorRoots{};
    std::array<float,2> vcsJetSkiHighAlignSeconds{},vcsJetSkiWetExitSeconds{};
    StorylandVehiclePhaseTiming transitionTiming;
    std::array<StorylandVehiclePhaseTiming,2> sideTransitionTiming;
    bool retailTransitions=false;
    float previewMinimumZ=0,previewMaximumZ=0;
    bool hasNativeSuspension=false;
    bool hasNativeForklift=false;
    std::array<float,3> forkliftFramePosition{};
    std::string forkliftPart;
    float nativeRideHeight=0;
    std::vector<StorylandVehicleWheelDefinition> wheels;
    // Bikes retain two rendered wheels and four native physical contact lines.
    std::vector<StorylandVehicleWheelDefinition> suspensionLines;
    const std::vector<StorylandVehicleWheelDefinition>& physicalWheels() const {
        return suspensionLines.empty()?wheels:suspensionLines;
    }
    // Extra visual axles share the nearest rear suspension contact.
    std::vector<StorylandVehicleWheelDefinition> auxiliaryWheels;
    std::vector<int8_t> materialPaintSlots;
    float wheelRadius(bool rear,float fallback) const {
        return hasNativeWheelDimensions?(rear?rearWheelScale:wheelScale)*0.5f:fallback;
    }
    float bodyRoofHeight(float fallback) const {
        return hasNativeSuspension?std::max(.5f,nativeRideHeight+previewMaximumZ-previewRoot[14]):fallback;
    }
    StorylandVehiclePhaseTiming vehicleTimings() const {
        auto result=transitionTiming;
        result.seconds[unsigned(StorylandVehiclePhase::GetIn)]=entrySeconds;
        result.seconds[unsigned(StorylandVehiclePhase::GetOut)]=exitSeconds;
        result.seconds[unsigned(StorylandVehiclePhase::PullOut)]=jackSeconds;
        return result;
    }
    bool hasNativeDummies=false;
    uint8_t numberOfDoors=0;
    bool motorcycle() const {return nativeType==(vcsModelLayout?6:5);}
    bool bicycle() const {return vcsModelLayout&&nativeType==8;}
    bool twoWheels() const {return motorcycle()||bicycle();}
    bool lowVehicle() const {return (modelFlags&(vcsModelLayout?4u:0x80u))!=0;}
    bool vanVehicle() const {return (modelFlags&(vcsModelLayout?1u:0x20u))!=0;}
    bool busVehicle() const {return (modelFlags&(vcsModelLayout?2u:0x40u))!=0;}
    bool vcsJetSki() const {
        return vcsModelLayout&&nativeType==2&&domain==StorylandVehicleDomain::Water;
    }
    bool vcsHovercraft() const {
        return vcsModelLayout&&nativeType==5&&domain==StorylandVehicleDomain::Air&&
            hasVcsModelInfoFlags&&(vcsModelInfoFlags&0x40u)!=0;
    }
    std::array<float,3> seatPosition(unsigned seat=0) const {
        if(!hasNativeDummies)return {seat==0?-.4f:.4f,0,.65f};
        if(motorcycle())return dummyPositions[vcsModelLayout?(seat==0?4u:5u):(seat==0?2u:3u)];
        size_t dummyIndex=0;
        if(vcsModelLayout) {
            if(nativeType!=1&&nativeType!=2)dummyIndex=seat>=2?5u:4u;
        } else {
            if(nativeType==0||nativeType==5)dummyIndex=seat>=2?3u:2u;
            else if(nativeType==3)dummyIndex=4u;
            else if(nativeType==6)dummyIndex=3u;
        }
        auto p=dummyPositions[dummyIndex];
        if(dummyIndex!=0&&(seat==0||seat==2))p[0]=-p[0];
        return p;
    }
    uint16_t frequency=0;
    std::vector<std::array<uint8_t,2>> colors;
    std::vector<std::array<uint8_t,2>> scriptColors;
    std::array<std::array<uint8_t,3>,16> customPaintColors{};
};
inline StorylandVehicleDefinition storylandDecodeVehicleDefinition(const std::vector<uint8_t>& data,uint32_t id,bool vcs) {
    StorylandVehicleDefinition result;result.modelId=id;result.vcsModelLayout=vcs;
    result.domain=storylandVehicleDomain(data,id,vcs);
    if(result.domain==StorylandVehicleDomain::Unknown) return result;
    const auto word=[&](size_t offset) -> uint32_t {
        if(offset>data.size()||data.size()-offset<4) return 0;
        return uint32_t(data[offset])|(uint32_t(data[offset+1])<<8)|(uint32_t(data[offset+2])<<16)|(uint32_t(data[offset+3])<<24);
    };
    const auto real=[&](size_t offset) {const uint32_t bits=word(offset);float value=1;std::memcpy(&value,&bits,4);return value;};
    const auto readBikeDynamics=[&](size_t row) {
        if(row>data.size()||data.size()-row<15*sizeof(float))return;
        std::array<float,15> values{};
        for(size_t index=0;index<values.size();++index) {
            values[index]=real(row+index*sizeof(float));
            if(!std::isfinite(values[index])||std::fabs(values[index])>100)return;
        }
        if(values[7]<=0||values[8]<0||values[10]<=0||values[10]>=1||
            values[11]>=0||values[11]<=-1||values[13]<0||values[14]<0)return;
        result.bikeLeanForwardCenter=values[0];result.bikeLeanForwardForce=values[1];
        result.bikeLeanBackCenter=values[2];result.bikeLeanBackForce=values[3];
        result.bikeSpeedSteer=values[7];result.bikeSlipSteer=values[8];
        result.bikeWheelieSine=values[10];result.bikeStoppieSine=values[11];
        result.bikeWheelieSteer=values[12];result.bikeWheelieStability=values[13];
        result.bikeStoppieStability=values[14];result.hasNativeBikeDynamics=true;
    };
    const uint32_t info=word(size_t(word(0x3C))+size_t(id)*4);
    result.nativeType=int(word(size_t(info)+(vcs?0x54:0x38)));
    if(result.vcsJetSki())result.hasVcsJetSkiEntryRoots=
        storylandReadVcsJetSkiEntryRoots(data,result.vcsJetSkiEntryRoots);
    const size_t needed=vcs?0x278:0x184;
    if(uint64_t(info)+needed>data.size()) return result;
    if(vcs&&result.motorcycle())result.hasNativeMotorcycleEntryRoot=
        storylandReadVcsModelEntryRoot(data,info,result.nativeMotorcycleEntryRoot);
    if(vcs&&uint64_t(info)+0x280<=data.size()) {
        result.hasVcsModelInfoFlags=true;
        result.vcsModelInfoFlags=word(size_t(info)+0x27C);
    }
    if(vcs&&uint64_t(info)+0x284<=data.size()) {
        // The sound-flag word is inferred to follow the extra-flag word.
        // Reject invalid bits rather than interpreting unrelated bytes.
        const uint32_t soundFlags=word(size_t(info)+0x280);
        if((soundFlags&~StorylandVcsRetailQuirks::KnownSoundFlags)==0) {
            result.hasVcsSoundFlags=true;
            result.vcsSoundFlags=soundFlags;
        }
    }
    // SLUS-21590: ProcessVehicleEngine reads +0x288 at 0x1B9B60 and
    // the acceleration sound at +0x284 at 0x1B9244.
    if(vcs&&uint64_t(info)+0x2A0<=data.size()) {
        const size_t audio=size_t(info)+0x284;
        const uint32_t bank=word(audio+4);
        const uint32_t hornRate=word(audio+12),sirenRate=word(audio+20);
        if(bank<40&&word(audio)<292&&word(audio+8)<292&&word(audio+16)<292&&
            hornRate>=2000&&hornRate<=192000&&sirenRate>=2000&&sirenRate<=192000) {
            result.hasNativeAudio=true;
            result.accelerationSample=word(audio);
            result.audioBank=uint8_t(bank);
            result.hornSample=word(audio+8);result.hornFrequency=hornRate;
            result.sirenSample=word(audio+16);result.sirenFrequency=sirenRate;
            result.doorSoundType=data[audio+24];
        }
    }
    StorylandNativeCollisionModel collision;
    std::string collisionError;
    if(storylandDecodeNativeCollisionModel(data,word(size_t(info)+0x14),uint32_t(data.size()),vcs,collision,collisionError)&&
        !collision.spheres.empty())result.collision=std::make_shared<StorylandNativeCollisionModel>(std::move(collision));
    if(vcs) {
        result.wheelModelId=int16_t(uint16_t(data[size_t(info)+0x270])|uint16_t(data[size_t(info)+0x271])<<8);
        result.numberOfDoors=data[size_t(info)+0x274];
    } else {
        result.wheelModelId=int16_t(uint16_t(word(size_t(info)+0x40)));
        result.numberOfDoors=data[size_t(info)+0x44];
    }
    const size_t dummyCount=vcs?15u:5u;
    const size_t dummyOffset=vcs?0x60u:0x50u;
    for(size_t i=0;i<dummyCount;++i) {
        bool valid=true;
        std::array<float,3> position{};
        for(size_t axis=0;axis<3;++axis) {
            const float value=real(size_t(info)+dummyOffset+i*16u+axis*4u);
            if(!std::isfinite(value)||std::fabs(value)>100.0f)valid=false;
            else position[axis]=value;
        }
        if(valid) {
            result.dummyPositions[i]=position;
            if(position[0]!=0||position[1]!=0||position[2]!=0)
                result.hasNativeDummies=true;
        }
    }
    result.wheelScale=real(size_t(info)+(vcs?0x58:0x3C));
    result.hasNativeWheelDimensions=std::isfinite(result.wheelScale)&&result.wheelScale>0&&result.wheelScale<=10;
    if(!result.hasNativeWheelDimensions)result.wheelScale=1;
    result.rearWheelScale=vcs?real(size_t(info)+0x5C):result.wheelScale;
    if(!std::isfinite(result.rearWheelScale)||result.rearWheelScale<=0||result.rearWheelScale>10)
        result.rearWheelScale=result.wheelScale;
    result.handlingPointer=vcs?word(size_t(info)+0x38):0;
    if(vcs&&result.twoWheels()) {
        const uint32_t bike=word(size_t(info)+0x3C);
        if(bike&&uint64_t(bike)+0x3C<=data.size()) {
            readBikeDynamics(bike);
            // Native bike handling stores sin(maximum lean), followed by the
            // animation lean angle and the per-50-Hz-step lean retention.
            const float maximumLean=real(size_t(bike)+0x10);
            const float retention=real(size_t(bike)+0x18);
            if(std::isfinite(maximumLean)&&maximumLean>0&&maximumLean<1&&
                std::isfinite(retention)&&retention>0&&retention<1) {
                result.hasNativeBikeHandling=true;
                result.bikeMaximumLeanRadians=std::asin(maximumLean);
                result.bikeLeanRetention=retention;
            }
        }
    }
    // Automobile constructor133A24 takes CVehicleModelInfo+40 as the flying
    // handling pointer. VCS rows occupy60hex bytes, including aligned vectors.
    if (vcs && result.domain == StorylandVehicleDomain::Air) {
        result.flyingHandlingPointer = word(size_t(info) + 0x40);
        const size_t flying = result.flyingHandlingPointer;
        if (flying && flying <= data.size() && data.size() - flying >= 0x60) {
            bool valid = true;
            for (size_t index = 0; index < result.flyingHandling.size(); ++index) {
                const float value = real(flying + index * 4);
                if (!std::isfinite(value) || std::fabs(value) > 10000) valid = false;
                result.flyingHandling[index] = value;
            }
            result.hasNativeFlyingHandling = valid && result.flyingHandling[0] > 0;
        }
    }
    // SurfaceTable row for surface 29 (wheelbase), LCS 0x1F2F40 / VCS 0x36C4F8.
    const uint32_t adhesion=word(vcs?0xB0:0xAC);
    if(adhesion&&uint64_t(adhesion)+36*sizeof(float)<=data.size()) {
        auto coefficients=result.tyreAdhesion;
        bool valid=true;
        for(size_t group=0;group<coefficients.size();++group) {
            const float coefficient=real(size_t(adhesion)+group*6*sizeof(float));
            if(!std::isfinite(coefficient)||coefficient<=0||coefficient>20)valid=false;
            coefficients[group]=coefficient;
        }
        if(valid)result.tyreAdhesion=coefficients;
    }
    // VCS stores a pointer to the native CHandlingData inside CVehicleModelInfo.
    // Offsets here follow the PS2 struct, not the textual handling.cfg column order.
    // The transmission occupies 0x60 bytes; mass is at +0xBC, not +0.
    if(vcs&&result.handlingPointer&&uint64_t(result.handlingPointer)+0xE0<=data.size()) {
        const size_t h=result.handlingPointer;
        const float nativeMass=real(h+0xBC),nativeTraction=real(h+0xC4);
        const float nativeSteering=real(h+0x68),nativeBrake=real(h+0x60);
        if(std::isfinite(nativeMass)&&nativeMass>=100&&nativeMass<=100000&&
            std::isfinite(nativeTraction)&&nativeTraction>=(result.domain==StorylandVehicleDomain::Air?0.0f:0.1f)&&nativeTraction<=5&&
            std::isfinite(nativeSteering)&&nativeSteering>=1&&nativeSteering<=90&&
            std::isfinite(nativeBrake)&&nativeBrake>=0&&nativeBrake<=1) {
            result.hasNativeHandling=true;
            result.mass=nativeMass;
            result.traction=nativeTraction;
            result.turnMass=real(h+0xC0);
            for(size_t axis=0;axis<3;++axis) {
                const float value=real(h+0xA0+axis*4);
                result.centerOfMass[axis]=std::isfinite(value)&&std::fabs(value)<100?value:0;
            }
            result.dragMultiplier=real(h+0x98);
            result.engineAcceleration=real(h+0x48);
            result.engineInertia=real(h+0x4C);
            // The original handling.cfg velocity is held separately at +0xB8.
            // The transmission's +0x50 field is an internal per-tick velocity.
            result.maximumVelocity=real(h+0xB8);
            result.maximumTransmissionVelocity=real(h+0x50);
            result.maximumAiVelocity=real(h+0x54);
            result.reverseVelocity=real(h+0x58);
            result.driveType=data[h+0x5C];
            result.engineType=data[h+0x5D];
            result.numberOfGears=std::clamp<uint8_t>(data[h+0x5E],1,5);
            result.transmissionFlags=data[h+0x5F];
            for(size_t i=0;i<result.gears.size();++i) {
                result.gears[i]={real(h+i*12),real(h+i*12+4),real(h+i*12+8)};
                if(!std::isfinite(result.gears[i].maximum)||!std::isfinite(result.gears[i].upshift)||
                    !std::isfinite(result.gears[i].downshift))result.gears[i]={};
            }
            result.brakeDeceleration=nativeBrake;
            result.brakeBias=real(h+0x64);
            result.steeringLock=nativeSteering;
            result.tractionLoss=real(h+0x6C);
            result.tractionBias=real(h+0x70);
            result.suspensionForce=real(h+0x74);
            result.suspensionDamping=real(h+0x78);
            result.suspensionUpper=real(h+0x7C);
            result.suspensionLower=real(h+0x80);
            result.suspensionBias=real(h+0x84);
            result.suspensionAntiDive=real(h+0x88);
            result.seatOffset=real(h+0x8C);
            result.collisionDamage=real(h+0xC8);
            result.handlingFlags=word(h+0xCC);
            result.modelFlags=word(h+0xD0);
            // CAutomobile constructor133F64/133F78 retains this coefficient.
            const float buoyancy=real(h+0xB4);
            if(std::isfinite(buoyancy)&&buoyancy>=0) {
                result.nativeBuoyancyCoefficient=buoyancy;
                result.hasNativeBuoyancyCoefficient=true;
            }
        }
    }
    if(vcs&&(result.nativeType==0||(result.nativeType==4&&result.domain==StorylandVehicleDomain::Air))&&
        result.hasNativeHandling&&result.hasNativeDummies) {
        bool valid=true;
        std::array<std::array<float,3>,2> roots{};
        for(size_t rear=0;rear<roots.size();++rear) {
            const auto selected=storylandVcsHelicopterDoorSelection(result.modelFlags,rear!=0);
            valid=storylandReadVcsAnimationRoot(data,selected.group,selected.association,roots[rear])&&valid;
        }
        if(valid) {
            result.nativeDoorRoots=roots;
            result.hasNativeDoorRoots=true;
        }
    }
    if(vcs&&result.domain==StorylandVehicleDomain::Water&&
        (result.nativeType==1||result.nativeType==2)&&result.hasNativeHandling&&result.collision) {
        const uint32_t boat=word(size_t(info)+0x44);
        if(boat&&uint64_t(boat)+0x8C<=data.size()) {
            bool handlingValid=std::isfinite(result.engineAcceleration)&&result.engineAcceleration>=0;
            for(size_t offset:{size_t(0),size_t(4),size_t(8),size_t(12),size_t(16),size_t(20)})
                handlingValid=handlingValid&&std::isfinite(real(size_t(boat)+offset))&&
                    std::fabs(real(size_t(boat)+offset))<=100;
            for(size_t axis=0;axis<3;++axis) {
                const float movement=real(size_t(boat)+0x20+axis*4);
                const float rotation=real(size_t(boat)+0x30+axis*4);
                handlingValid=handlingValid&&std::isfinite(movement)&&movement>0&&movement<=1&&
                    std::isfinite(rotation)&&rotation>0&&rotation<=1;
            }
            if(handlingValid) {
                // SLUS_215.90 CBoat 1A15BC/1A370C reads this model's native boat row.
                result.hasNativeBoatHandling=true;
                result.boatThrustY=real(boat);result.boatThrustZ=real(size_t(boat)+4);
                result.boatThrustAppZ=real(size_t(boat)+8);
                result.boatAquaplaneForce=real(size_t(boat)+12);
                result.boatAquaplaneLimit=real(size_t(boat)+16);
                result.boatAquaplaneOffset=real(size_t(boat)+20);
                for(size_t axis=0;axis<3;++axis) {
                    result.boatMoveResistance[axis]=real(size_t(boat)+0x20+axis*4);
                    result.boatTurnResistance[axis]=real(size_t(boat)+0x30+axis*4);
                }
            }
            StorylandVehicleBoatBuoyancy buoyancy;
            buoyancy.coefficient=real(size_t(result.handlingPointer)+0xB4);
            const auto& bounds=*result.collision;
            buoyancy.minimum={bounds.minimum.x,bounds.minimum.y,bounds.minimum.z};
            buoyancy.maximum={bounds.maximum.x,bounds.maximum.y,bounds.maximum.z};
            bool valid=std::isfinite(buoyancy.coefficient)&&buoyancy.coefficient>=0&&
                std::isfinite(result.suspensionDamping)&&result.suspensionDamping>=0;
            for(size_t index=0;index<buoyancy.sliceWeights.size();++index) {
                const float weight=real(size_t(boat)+0x40+index*4);
                buoyancy.sliceWeights[index]=weight;
                valid=valid&&std::isfinite(weight)&&weight>=0&&weight<=1;
            }
            // Native 369630 adjusts the collision hull from the authored boat row.
            if(result.modelFlags&0x08000000u) {
                for(size_t axis=0;axis<3;++axis) {
                    const float lower=real(size_t(boat)+0x80+axis*4);
                    const float upper=real(size_t(boat)+0x70+axis*4);
                    valid=valid&&std::isfinite(lower)&&std::isfinite(upper);
                    if(axis<2) {
                        buoyancy.minimum[axis]*=lower;
                        buoyancy.maximum[axis]*=upper;
                    } else {
                        buoyancy.minimum[axis]+=lower;
                        buoyancy.maximum[axis]+=upper;
                    }
                }
            }
            for(size_t axis=0;axis<3;++axis)
                valid=valid&&std::isfinite(buoyancy.minimum[axis])&&
                    std::isfinite(buoyancy.maximum[axis])&&
                    buoyancy.maximum[axis]>buoyancy.minimum[axis];
            if(valid) {buoyancy.valid=true;result.boatBuoyancy=buoyancy;}
        }
    }
    result.handlingId=vcs?-1:int16_t(word(size_t(info)+0x42)&0xFFFF);
    if (!vcs && result.domain == StorylandVehicleDomain::Air) {
        // SLUS_214.23 GetFlyingPointer144268: six70-byte rows at manager4DF0.
        const uint32_t manager = word(0xA8);
        const uint32_t index = result.handlingId >= 75 && result.handlingId <= 80 ?
            uint32_t(result.handlingId - 75) : 0;
        const uint64_t row = uint64_t(manager) + 0x4DF0 + uint64_t(index)*0x70;
        if (manager && row + 0x70 <= data.size() && word(size_t(row)) == 75 + index) {
            result.flyingHandlingPointer = uint32_t(row);
            bool valid = true;
            for (size_t parameter = 0; parameter < 24; ++parameter) {
                const size_t offset = parameter < 12 ? 4 + parameter*4 : 0x10 + parameter*4;
                const float value = real(size_t(row) + offset);
                if (!std::isfinite(value) || std::fabs(value) > 10000) valid = false;
                result.flyingHandling[parameter] = value;
            }
            result.hasNativeFlyingHandling = valid && result.flyingHandling[0] > 0;
        }
    }
    if(!vcs&&result.motorcycle()&&result.handlingId>=65&&result.handlingId<=71) {
        // SLUS-21423 CBike constructor at 0x154C58–0x154C80.
        const uint32_t manager=word(0xA8);
        const uint64_t row=uint64_t(manager)+0x3ACCu+uint64_t(result.handlingId)*0x44u;
        if(manager&&row+0x44u<=data.size()&&word(size_t(row))==uint32_t(result.handlingId)) {
            readBikeDynamics(size_t(row)+4);
            const float maximumLean=real(size_t(row)+0x14u);
            const float retention=real(size_t(row)+0x1Cu);
            if(std::isfinite(maximumLean)&&maximumLean>0&&maximumLean<1&&
                std::isfinite(retention)&&retention>0&&retention<1) {
                result.hasNativeBikeHandling=true;
                result.bikeMaximumLeanRadians=std::asin(maximumLean);
                result.bikeLeanRetention=retention;
            }
        }
    }
    if(!vcs&&result.handlingId>=0&&result.handlingId<81) {
        // LCS embeds 81 0xF0-byte handling rows after the 0x20-byte
        // CHandlingDataMgr header at GAME.DTZ +0xA8.
        const uint32_t manager=word(0xA8);
        const uint64_t row=uint64_t(manager)+0x20u+uint64_t(result.handlingId)*0xF0u;
        if(manager!=0&&row+0xF0u<=data.size()&&
           word(size_t(row)+0x10u)==uint32_t(result.handlingId)) {
            const size_t h=size_t(row);
            const float nativeMass=real(h),nativeTraction=real(h+0x08u);
            const float nativeSteering=real(h+0xA8u),nativeBrake=real(h+0x9Cu);
            const float nativeSpring=real(h+0xB8u);
            // Boat traction is a signed propeller torque coefficient: the
            // retail REEFER row is -1.5. The tyre range applies only to cars.
            if(std::isfinite(nativeMass)&&nativeMass>=100&&nativeMass<=100000&&
                std::isfinite(nativeTraction)&&(result.nativeType==1?std::fabs(nativeTraction)<=5:
                    nativeTraction>=0.1f&&nativeTraction<=5)&&
                std::isfinite(nativeSteering)&&nativeSteering>=1&&nativeSteering<=90&&
                std::isfinite(nativeBrake)&&nativeBrake>=0&&nativeBrake<=1&&
                std::isfinite(nativeSpring)&&nativeSpring>0&&nativeSpring<100) {
                result.hasNativeHandling=true;
                result.handlingPointer=uint32_t(h);
                result.mass=nativeMass;
                result.turnMass=real(h+0x04u);
                result.traction=nativeTraction;
                result.collisionDamage=real(h+0x0Cu);
                result.dragMultiplier=real(h+0x18u);
                for(size_t axis=0;axis<3;++axis) {
                    const float value=real(h+0x20u+axis*4u);
                    result.centerOfMass[axis]=std::isfinite(value)&&std::fabs(value)<100?value:0;
                }
                result.driveType=data[h+0x80u];
                result.engineType=data[h+0x81u];
                result.numberOfGears=std::clamp<uint8_t>(data[h+0x82u],1,5);
                result.transmissionFlags=data[h+0x83u];
                for(size_t i=0;i<result.gears.size();++i) {
                    const size_t offset=h+0x38u+i*12u;
                    result.gears[i]={real(offset),real(offset+4u),real(offset+8u)};
                    if(!std::isfinite(result.gears[i].maximum)||
                       !std::isfinite(result.gears[i].upshift)||
                       !std::isfinite(result.gears[i].downshift))result.gears[i]={};
                }
                result.engineAcceleration=real(h+0x84u);
                result.engineInertia=real(h+0x88u);
                result.maximumVelocity=real(h+0x8Cu);
                result.maximumTransmissionVelocity=result.maximumVelocity;
                result.maximumAiVelocity=real(h+0x90u);
                result.reverseVelocity=real(h+0x94u);
                result.brakeDeceleration=nativeBrake;
                result.brakeBias=real(h+0xA0u);
                result.steeringLock=nativeSteering;
                result.tractionLoss=real(h+0xACu);
                result.tractionBias=real(h+0xB0u);
                result.suspensionForce=nativeSpring;
                result.suspensionDamping=real(h+0xBCu);
                result.suspensionUpper=real(h+0xC0u);
                result.suspensionLower=real(h+0xC4u);
                result.suspensionBias=real(h+0xC8u);
                result.suspensionAntiDive=real(h+0xCCu);
                result.modelFlags=word(h+0xD0u);
                result.handlingFlags=0;
                result.seatOffset=real(h+0xD4u);
            }
        }
    }
    if(!vcs&&result.nativeType==1&&result.handlingId>=72&&result.handlingId<=74) {
        // Retail LCS PS2 CHandlingDataMgr: boat rows start at +0x5090,
        // are 0x60 bytes, and align both resistance vectors to 16 bytes.
        // GetBoatPointer's fallback shares row0 with FERRY. HANDLING.CFG's
        // final FERRY line therefore gives retail row0 identifier32.
        const uint32_t manager=word(0xA8);
        const uint64_t row=uint64_t(manager)+0x5090u+uint64_t(result.handlingId-72)*0x60u;
        if(manager&&row+0x60u<=data.size()) {
            const size_t b=size_t(row);const uint32_t identifier=word(b);
            bool valid=identifier==uint32_t(result.handlingId)||(result.handlingId==72&&identifier==32);
            valid=valid&&std::isfinite(result.turnMass)&&result.turnMass>0&&
                std::isfinite(result.engineAcceleration)&&result.engineAcceleration>=0;
            for(size_t offset:{size_t(4),size_t(8),size_t(12),size_t(16),size_t(20),size_t(24)})
                valid=valid&&std::isfinite(real(b+offset))&&std::fabs(real(b+offset))<=100;
            for(size_t axis=0;axis<3;++axis) {
                const float movement=real(b+0x30u+axis*4u),rotation=real(b+0x40u+axis*4u);
                valid=valid&&std::isfinite(movement)&&movement>0&&movement<=1&&
                    std::isfinite(rotation)&&rotation>0&&rotation<=1;
            }
            if(valid) {
                result.hasNativeBoatHandling=true;
                result.boatThrustY=real(b+4);result.boatThrustZ=real(b+8);result.boatThrustAppZ=real(b+12);
                result.boatAquaplaneForce=real(b+16);result.boatAquaplaneLimit=real(b+20);result.boatAquaplaneOffset=real(b+24);
                for(size_t axis=0;axis<3;++axis) {
                    result.boatMoveResistance[axis]=real(b+0x30u+axis*4u);
                    result.boatTurnResistance[axis]=real(b+0x40u+axis*4u);
                }
            }
        }
    }
    result.frequency=uint16_t(word(size_t(info)+(vcs?0x272:0x48))&0xFFFF);
    result.vehicleClass=int8_t(data[size_t(info)+(vcs?0x275:0x45)]);
    // Retail vehicle models 272 and 273 have nine regular sets.
    // Native getters use the byte count, even when the ninth pair overlaps
    // the otherwise reserved script-colour slot at+0x173.
    const unsigned count=vcs?unsigned(data[size_t(info)+0x161]):
        std::min(8u,unsigned(data[size_t(info)+0x180]));
    for(unsigned index=0;index<count;++index) {
        const size_t first=size_t(info)+(vcs?0x163+index*2:0x170+index);
        const size_t second=vcs?first+1:size_t(info)+0x178+index;
        if (second >= data.size()) break;
        result.colors.push_back({data[first],data[second]});
    }
    if (vcs) {
        const unsigned scriptCount = std::min(2u, unsigned(data[size_t(info) + 0x162]));
        for (unsigned index = 0; index < scriptCount; ++index) {
            const size_t first = size_t(info) + 0x173 + index * 2;
            result.scriptColors.push_back({data[first], data[first + 1]});
        }
        for (size_t index = 0; index < result.customPaintColors.size(); ++index) {
            const size_t first = size_t(info) + 0x177 + index * 3;
            result.customPaintColors[index] = {data[first], data[first + 1], data[first + 2]};
        }
    }
    return result;
}
