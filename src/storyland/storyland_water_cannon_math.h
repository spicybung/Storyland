#pragma once
#include "src/storyland/storyland_water_cannon.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

inline bool storylandWaterCannonActive(const std::array<StorylandWaterCannon,3>& cannons) {
    return std::any_of(cannons.begin(),cannons.end(),[](const auto& cannon) {
        return cannon.ownerSlot>=0;
    });
}

inline void storylandAimWaterCannon(float& yaw,float& pitch,float nativeStep,
    int horizontal,int vertical,const std::array<float,16>& frame,
    const std::array<float,3>* cameraForward=nullptr) {
    constexpr float pi=3.141592741f;
    if(cameraForward) {
        std::array<float,3> local{};
        for(size_t axis=0;axis<3;++axis)
            for(size_t component=0;component<3;++component)
                local[axis]+=frame[axis*4+component]*(*cameraForward)[component];
        float targetYaw=local[0]==0&&local[1]==0?0:std::atan2(-local[0],local[1]);
        const float horizontalLength=std::hypot(local[0],local[1]);
        const float targetPitch=(local[2]==0&&horizontalLength==0?0:
            std::atan2(local[2],horizontalLength))+.2617993951f;
        if(targetYaw>yaw+pi)targetYaw-=2*pi;
        else if(targetYaw<yaw-pi)targetYaw+=2*pi;
        yaw+=std::clamp(targetYaw-yaw,-.05f*nativeStep,.05f*nativeStep);
        pitch+=std::clamp(targetPitch-pitch,-.02f*nativeStep,.02f*nativeStep);
    } else {
        yaw-=.05f*nativeStep*float(horizontal)/128.0f;
        pitch+=.02f*nativeStep*float(vertical)/128.0f;
    }
    if(yaw<-pi)yaw+=2*pi;
    else if(yaw>pi)yaw-=2*pi;
    pitch=std::clamp(pitch,-.06f,.3f);
}

inline StorylandWaterCannonSample storylandWaterCannonNozzle(
    const std::array<float,16>& frame,float yaw,float pitch,uint32_t random) {
    const float sine=std::sin(yaw),cosine=std::cos(yaw),pitchCosine=std::cos(pitch);
    const std::array<float,3> localPosition{-.75f*sine,1.05f+.75f*cosine,2.02f};
    const std::array<float,3> localVelocity{-sine*pitchCosine,cosine*pitchCosine,std::sin(pitch)};
    StorylandWaterCannonSample sample;
    sample.active=true;
    for(size_t axis=0;axis<3;++axis) {
        sample.position[axis]=frame[12+axis];
        for(size_t component=0;component<3;++component) {
            sample.position[axis]+=frame[component*4+axis]*localPosition[component];
            sample.nativeVelocity[axis]+=frame[component*4+axis]*localVelocity[component];
        }
    }
    sample.nativeVelocity[2]+=float(random&15u)*.001f;
    return sample;
}

inline bool storylandEmitWaterCannon(std::array<StorylandWaterCannon,3>& cannons,
    int32_t ownerSlot,int32_t ownerScriptHandle,uint32_t now,const StorylandWaterCannonSample& sample) {
    auto found=std::find_if(cannons.begin(),cannons.end(),[&](const auto& cannon) {
        return cannon.ownerSlot==ownerSlot&&cannon.ownerScriptHandle==ownerScriptHandle;
    });
    if(found==cannons.end()) {
        found=std::find_if(cannons.begin(),cannons.end(),[](const auto& cannon) {
            return cannon.ownerSlot<0;
        });
        if(found==cannons.end())return false;
        *found={};
        found->ownerSlot=ownerSlot;
        found->ownerScriptHandle=ownerScriptHandle;
        found->startedAt=now;
    }
    found->samples[found->current]=sample;
    return true;
}

template<class Suppress> inline void storylandAdvanceWaterCannon(StorylandWaterCannon& cannon,
    uint32_t now,float nativeStep,Suppress suppress) {
    if(cannon.ownerSlot<0)return;
    if(uint32_t(cannon.startedAt+150u)<now) {
        cannon.current=uint8_t((cannon.current+1u)&15u);
        cannon.samples[cannon.current].active=false;
    }
    bool active=false;
    for(auto& sample:cannon.samples)if(sample.active) {
        active=true;
        sample.nativeVelocity[2]-=.009f*nativeStep;
        for(size_t axis=0;axis<3;++axis)
            sample.position[axis]+=sample.nativeVelocity[axis]*nativeStep;
    }
    for(const auto& sample:cannon.samples)
        if(sample.active&&suppress(sample.position,4.0f))break;
    if(!active)cannon={};
}

template<class World> inline void storylandApplyWaterToFire(World& world,
    typename World::ScriptFire& fire) {
    fire.strength-=.012f*world.nativeScriptTimeStep;
    std::vector<typename World::ScriptParticle> steam(3);
    auto position=fire.position;
    const auto offset=[&]() {
        position[0]+=float(int(world.nextNativeRandom()&255u)-128)*.015f;
        position[1]+=float(int(world.nextNativeRandom()&255u)-128)*.015f;
        position[2]+=float(world.nextNativeRandom()&255u)*.005f;
    };
    offset();
    for(size_t index=0;index<steam.size();++index) {
        if(index==2)offset();
        auto& particle=steam[index];
        particle.type=28;
        particle.position=position;
        particle.velocity={0,0,index==0?.1f:index==1?.05f:.15f};
        particle.size=index==0?1.2f:index==1?1.4f:1.0f;
        particle.color={210,210,210,90};
    }
    std::string error;
    fire.suppressionPending=!world.createScriptParticles||!world.createScriptParticles(steam,error);
    if(fire.strength<0) {
        fire.active=false;
        fire.attachedKind=0;
        fire.attachedHandle=-1;
    }
}

template<class World> inline bool storylandSuppressFireWithWater(World& world,
    const std::array<float,3>& point,float radius) {
    for(auto& entry:world.scriptFires) {
        auto& fire=entry.second;
        if(!fire.active)continue;
        float distanceSquared=0;
        for(size_t axis=0;axis<3;++axis) {
            const float delta=point[axis]-fire.position[axis];
            distanceSquared+=delta*delta;
        }
        if(!(distanceSquared<radius*radius))continue;
        storylandApplyWaterToFire(world,fire);
        return true;
    }
    return false;
}

struct StorylandWaterCannonVertex {
    std::array<float,3> position{};
    std::array<float,2> uv{};
    uint8_t alpha=0;
};

template<class Draw> inline void storylandWaterCannonRibbons(const StorylandWaterCannon& cannon,
    const std::array<float,3>& cameraForward,float texturePhase,Draw draw) {
    if(cannon.ownerSlot<0)return;
    size_t newer=cannon.current,previous=newer;
    std::array<float,3> side{};
    bool sideKnown=false;
    for(size_t segment=0;segment<15;++segment) {
        const size_t older=(newer+15u)&15u;
        if(cannon.samples[newer].active&&cannon.samples[older].active) {
            const auto& first=cannon.samples[newer].position;
            const auto& second=cannon.samples[older].position;
            if(!sideKnown) {
                const std::array<float,3> direction{
                    second[0]-first[0],second[1]-first[1],second[2]-first[2]};
                side={direction[1]*cameraForward[2]-direction[2]*cameraForward[1],
                    direction[2]*cameraForward[0]-direction[0]*cameraForward[2],
                    direction[0]*cameraForward[1]-direction[1]*cameraForward[0]};
                const float length=std::hypot(side[0],side[1],side[2]);
                if(length>0) {
                    for(float& value:side)value*=.05f/length;
                    sideKnown=true;
                }
            }
            if(sideKnown) {
                const auto alpha=uint8_t(std::lround(255.0f*(1.0f-float(segment*segment)/256.0f)));
                const uint8_t startAlpha=segment==0||!cannon.samples[previous].active?0:alpha;
                std::array<StorylandWaterCannonVertex,4> vertices{};
                for(size_t vertex=0;vertex<4;++vertex) {
                    const bool farEnd=vertex>=2,positive=vertex==1||vertex==2;
                    const float width=float(segment+(farEnd?2:1))*(positive?1.0f:-1.0f);
                    const auto& position=farEnd?second:first;
                    for(size_t axis=0;axis<3;++axis)
                        vertices[vertex].position[axis]=position[axis]+side[axis]*width;
                    vertices[vertex].uv={float(farEnd)-texturePhase,float(positive)};
                    vertices[vertex].alpha=farEnd?alpha:startAlpha;
                }
                draw(vertices);
            }
        }
        previous=newer;
        newer=older;
    }
}
