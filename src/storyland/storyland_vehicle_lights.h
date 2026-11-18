#pragma once
#include "src/leeds/leeds_vehicle.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include "src/storyland/storyland_service_roles.h"
#include "src/storyland/storyland_vehicle_siren.h"
#include "src/storyland/storyland_pickup_visual.h"
#include "src/graphics/storyland_ps2_lighting.h"
#include <algorithm>
#include <array>
#include <cmath>

struct StorylandVehicleLampAppearance {
    std::array<uint8_t,3> color{};
    float radius=0,range=0;
    uint8_t alpha=255,texture=8;
};

inline StorylandVehicleLampAppearance storylandVcsHeadlampAppearance(float facing,bool xenon=false) {
    const float intensity=-facing*.5f+.3f;
    const float red=(xenon?190.0f:210.0f)*intensity;
    const float blue=(xenon?255.0f:195.0f)*intensity;
    return {{{uint8_t(std::clamp(int(red),0,255)),uint8_t(std::clamp(int(red),0,255)),
        uint8_t(std::clamp(int(blue),0,255))}},-facing*.5f+.5f,100,255,8};
}

inline StorylandVehicleLampAppearance storylandVcsTaillampAppearance(float facing,
    bool braking,bool reversing=false) {
    float intensity=facing*.4f+.4f;
    float radius=(facing+1)*.5f;
    if(reversing||braking) {intensity+=.4f;radius+=.3f;}
    const uint8_t red=uint8_t(std::clamp(int(128.0f*intensity),0,255));
    return {{{red,uint8_t(reversing?red:0),uint8_t(reversing?red:0)}},radius,50,255,8};
}

struct StorylandVehicleLampState {
    bool headlights=false,braking=false,reversing=false,engine=true;
    std::array<uint8_t,4> damage{};
    bool siren=false;
    StorylandServiceRole serviceRole=StorylandServiceRole::Civilian;
    float clockSeconds=0,steeringAngle=0;
};

struct StorylandVehicleLightFrame {
    StorylandVcsPickupVisual sprites,emergency;
    StorylandPickupGroundGlow beam;
    std::array<StorylandPs2PointLight,4> pointLights{};
    size_t pointLightCount=0,emergencyPointLightBegin=0;
};

inline StorylandVehicleLightFrame storylandVehicleLights(const StorylandVehicleDefinition& definition,
    const StorylandVehicleMatrix& frame,const StorylandVehicleLampState& state,
    const std::array<float,3>& camera) {
    StorylandVehicleLightFrame result;
    if(definition.domain!=StorylandVehicleDomain::Road)return result;
    for(float value:frame)if(!std::isfinite(value))return result;
    const bool motorcycle=definition.twoWheels();
    const auto forward=std::array<float,3>{frame[4],frame[5],frame[6]};
    std::array<float,3> relative{frame[12]-camera[0],frame[13]-camera[1],frame[14]-camera[2]};
    const float distance=std::hypot(relative[0],relative[1],relative[2]);
    float facing=0;
    if(distance>0)for(size_t axis=0;axis<3;++axis)facing+=relative[axis]/distance*forward[axis];
    facing=std::clamp(facing,-1.0f,1.0f);
    const auto transform=[&](std::array<float,3> point) {
        return storylandVehicleTransformPoint(frame,point);
    };
    const auto valid=[](const std::array<float,3>& point) {
        return std::any_of(point.begin(),point.end(),[](float value){return value!=0;})&&
            std::all_of(point.begin(),point.end(),[](float value){return std::isfinite(value);});
    };
    const auto add=[&](const std::array<float,3>& position,const StorylandVehicleLampAppearance& appearance) {
        if(result.sprites.coronaCount>=result.sprites.coronas.size())return;
        auto& corona=result.sprites.coronas[result.sprites.coronaCount++];
        corona.position=position;corona.color=appearance.color;corona.radius=appearance.radius;
        corona.range=appearance.range;corona.alpha=appearance.alpha;corona.texture=appearance.texture;
        corona.pullAway=1.5f;
    };
    const bool headlights=state.headlights&&state.engine;
    const bool frontIntact=motorcycle?!state.damage[0]:!state.damage[0]||!state.damage[1];
    const auto& head=definition.dummyPositions[0];
    if(definition.hasNativeDummies&&headlights&&frontIntact&&valid(head)) {
        const bool xenon=(definition.handlingFlags&0x400000u)!=0;
        const auto appearance=storylandVcsHeadlampAppearance(facing,xenon);
        const auto appendHead=[&](int side) {
            if(state.damage[size_t(side)])return;
            auto local=head;
            if(!motorcycle)local[0]=side?head[0]:-head[0];
            const auto position=transform(local);
            float lampFacing=0;
            for(size_t axis=0;axis<3;++axis)lampFacing+=(position[axis]-camera[axis])*forward[axis];
            if(lampFacing>=0)return;
            add(position,appearance);
            if(facing<-.97f&&distance<50) {
                const std::array<uint8_t,3> tint=xenon?std::array<uint8_t,3>{150,150,195}:
                    std::array<uint8_t,3>{160,160,140};
                add(position,{tint,motorcycle?1.2f:.3f,90,uint8_t(motorcycle&&!xenon?192:255),4});
            }
        };
        appendHead(0);if(!motorcycle)appendHead(1);
        auto& light=result.pointLights[result.pointLightCount++];
        light.position={frame[12],frame[13],frame[14]};light.direction=forward;
        light.color={1,1,1};light.radius=20;light.type=1;
        const float planar=std::hypot(forward[0],forward[1]);
        if(planar>.0001f) {
            const float x=forward[0]/planar,y=forward[1]/planar;
            const float length=motorcycle?4.5f:7.0f;
            float halfWidth=motorcycle?2.0f:3.5f;
            auto& beam=result.beam;
            beam.position={frame[12]+x*(head[1]+length),frame[13]+y*(head[1]+length),frame[14]+2};
            if(!motorcycle&&bool(state.damage[0])!=bool(state.damage[1])) {
                const float side=state.damage[0]?1.0f:-1.0f;
                halfWidth*=.5f;
                beam.position[0]+=frame[0]*halfWidth*side;
                beam.position[1]+=frame[1]*halfWidth*side;
            }
            beam.frontX=x*length;beam.frontY=y*length;
            beam.sideX=-y*halfWidth;beam.sideY=x*halfWidth;
            beam.color={145,145,145};beam.downDistance=7;beam.upDistance=0;beam.range=40;
            beam.visible=true;
        }
    }
    const auto& tail=definition.dummyPositions[1];
    const bool tailIntact=motorcycle?!state.damage[2]:!state.damage[2]||!state.damage[3];
    if(definition.hasNativeDummies&&(headlights||state.braking||state.reversing)&&tailIntact&&valid(tail)) {
        {
            const auto appearance=storylandVcsTaillampAppearance(facing,state.braking,
                !motorcycle&&state.reversing);
            const auto appendTail=[&](int side) {
                if(state.damage[size_t(side+2)])return;
                auto local=tail;
                if(!motorcycle)local[0]=side?tail[0]:-tail[0];
                const auto position=transform(local);
                float lampFacing=0;
                for(size_t axis=0;axis<3;++axis)lampFacing+=(position[axis]-camera[axis])*forward[axis];
                if(lampFacing>0)add(position,appearance);
            };
            appendTail(0);if(!motorcycle)appendTail(1);
        }
        auto& light=result.pointLights[result.pointLightCount++];
        light.position={frame[12]-forward[0]*4,frame[13]-forward[1]*4,frame[14]-forward[2]*4};
        light.color={state.braking?1.0f:.6f,0,0};
        light.radius=state.braking?10.0f:7.0f;light.type=0;
    }
    result.emergencyPointLightBegin=result.pointLightCount;
    auto role=state.serviceRole;
    if(role==StorylandServiceRole::Civilian)
        role=storylandNativeVehicleSirenRole(definition.modelId,definition.vcsModelLayout);
    if(!state.siren||!storylandEmergencySirenRole(role)||!std::isfinite(state.clockSeconds))return result;
    const uint32_t milliseconds=uint32_t(std::fmod(std::max(0.0,double(state.clockSeconds))*1000.0,1024.0));
    const auto appendEmergency=[&](std::array<float,3> position,std::array<uint8_t,3> color) {
        if(result.emergency.coronaCount>=result.emergency.coronas.size())return;
        auto& corona=result.emergency.coronas[result.emergency.coronaCount++];
        corona.position=position;corona.color=color;corona.radius=.4f;corona.range=50;
        corona.alpha=255;corona.texture=0;corona.pullAway=1.5f;
    };
    std::array<uint8_t,3> first{255,0,0},second{0,0,255};
    const bool undercover=role==StorylandServiceRole::Fbi||role==StorylandServiceRole::Vice||
        role==StorylandServiceRole::Dea;
    if(undercover) {
        // VCS 144F54: the unmarked car's lamp is on its dashboard and faces forward.
        if((milliseconds&0x100u)&&facing<0)
            appendEmergency(transform({.4f,.6f,.3f}),role==StorylandServiceRole::Vice?
                std::array<uint8_t,3>{255,70,70}:std::array<uint8_t,3>{0,0,255});
        return result;
    }
    if(motorcycle) {
        if(!definition.vcsModelLayout||definition.modelId!=191)return result;
        // SLUS-21590 343478..3435AC; the first two lamps belong to the fork node.
        constexpr std::array<std::array<float,3>,5> positions{{
            {{-.237f,.423f,0}},{{.234f,.423f,0}},{{.310f,-1.043f,.954f}},
            {{-.275f,-1.386f,.200f}},{{.268f,-1.386f,.200f}}}};
        for(size_t index=0;index<positions.size();++index) {
            const unsigned phase=((milliseconds+uint32_t(index)*64u)>>8u)&3u;
            if(phase!=0&&phase!=2)continue;
            auto local=positions[index];
            if(index<2) {
                if(!definition.hasSirenForkFrame)continue;
                const float angle=std::isfinite(state.steeringAngle)?
                    -state.steeringAngle*.01745329251994329577f:0.0f;
                const float cosine=std::cos(angle),sine=std::sin(angle);
                local={local[0]*cosine-local[1]*sine,local[0]*sine+local[1]*cosine,local[2]};
                local=storylandVehicleTransformPoint(definition.sirenForkFrame,local);
            }
            appendEmergency(transform(local),phase==0?first:second);
        }
    } else {
        // SLUS-21590 144340..1444E0 and reLCS CAutomobile::PreRender.
        std::array<float,3> endpoint{.47f,-.4f,1.0f};
        if(role==StorylandServiceRole::Swat)endpoint={.7f,.98f,1.55f};
        else if(role==StorylandServiceRole::Paramedic) {
            endpoint={.95f,.64f,1.6f};second={255,255,255};
        } else if(role==StorylandServiceRole::Firefighter) {
            endpoint={.91f,1.7f,1.86f};second={255,255,0};
        }
        for(unsigned index=0;index<4;++index) {
            const unsigned phase=((milliseconds+index*64u)>>8u)&3u;
            if(phase!=0&&phase!=2)continue;
            auto local=endpoint;
            local[0]=endpoint[0]*(float(index)*2.0f-3.0f)/3.0f;
            appendEmergency(transform(local),phase==0?first:second);
        }
    }
    if(result.pointLightCount<result.pointLights.size()) {
        const auto& color=(milliseconds&0x200u)?second:first;
        const uint32_t half=milliseconds&0x1FFu;
        const float envelope=half<100u?float(half)*.01f:half>412u?float(512u-half)*.01f:1.0f;
        auto& light=result.pointLights[result.pointLightCount++];
        light.position=transform({0,0,2});light.radius=12;light.type=0;
        for(size_t axis=0;axis<3;++axis)
            light.color[axis]=float(uint8_t(float(color[axis]/6u)*envelope))*.02f;
    }
    return result;
}

template<class Actors>
inline void storylandRegisterVehiclePointLights(StorylandPs2PointLights& output,
    const StorylandPlayTestRuntime& runtime,const Actors& actors,const std::array<float,3>& camera,
    float trafficDensity=1,int occupiedVehicle=-1,float clockSeconds=0.0f) {
    for(size_t index=0;index<runtime.vehicles.size()&&index<actors.size();++index) {
        const auto& vehicle=runtime.vehicles[index];
        const auto& actor=actors[index];
        if(actor.water||vehicle.health<=0||vehicle.charred||!vehicle.visible||!vehicle.clumpVisible||
            !vehicle.renderAlpha)continue;
        if(trafficDensity<=0&&vehicle.scriptHandle<0&&vehicle.generator<0&&vehicle.savedGarage<0&&
            !vehicle.coopControlled&&int(index)!=occupiedVehicle)continue;
        const auto& definition=vehicle.nativeDefinition;
        const auto mesh=storylandVehicleRenderFrame(definition,
            {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
            vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
            false,vehicle.craneCarried?&vehicle.craneFrame:nullptr,vehicle.bikeVisualLean);
        const auto frame=storylandVehicleMatrixProduct(mesh,definition.previewRoot);
        const auto lights=storylandVehicleLights(definition,frame,
            {vehicle.headlightsOn,vehicle.braking,vehicle.throttle<0,vehicle.engineOn||
                (vehicle.alarmMilliseconds!=0&&vehicle.alarmMilliseconds!=UINT16_MAX),
                vehicle.lightDamage,vehicle.siren,storylandVehicleSirenRole(runtime,vehicle),clockSeconds,
                vehicle.steeringAngle},camera);
        for(size_t light=0;light<lights.pointLightCount;++light)
            storylandPs2RegisterPointLight(output,lights.pointLights[light],camera);
    }
}
