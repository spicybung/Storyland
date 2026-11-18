#pragma once

#include "src/storyland/storyland_vehicle_lights.h"
#include "src/storyland/storyland_muzzle_flash.h"
#include "src/storyland/storyland_weapon_visuals.h"

template<class Actors,class Emit>
void storylandVisitAeroEmissionLights(const StorylandPlayTestRuntime& runtime,
    const Actors& actors,const std::array<float,3>& camera,float clockSeconds,
    float trafficDensity,Emit emit) {
    const auto submit=[&](const StorylandPs2PointLight& light) {
        if(!(light.radius>0)||!std::isfinite(light.radius))return;
        for(float component:light.position)if(!std::isfinite(component))return;
        for(float component:light.color)if(!std::isfinite(component))return;
        const float distance=std::hypot(light.position[0]-camera[0],
            light.position[1]-camera[1],light.position[2]-camera[2]);
        if(distance>60.0f+light.radius)return;
        if(light.color[0]>0||light.color[1]>0||light.color[2]>0)emit(light);
    };
    for(size_t index=0;index<runtime.vehicles.size()&&index<actors.size();++index) {
        const auto& vehicle=runtime.vehicles[index];
        const auto& actor=actors[index];
        if(!vehicle.siren||actor.water||vehicle.health<=0||vehicle.charred||
            !vehicle.visible||!vehicle.clumpVisible||!vehicle.renderAlpha)continue;
        if(trafficDensity<=0&&vehicle.scriptHandle<0&&vehicle.generator<0&&vehicle.savedGarage<0&&
            !vehicle.coopControlled&&int(index)!=runtime.occupiedVehicle)continue;
        const auto& definition=vehicle.nativeDefinition;
        const auto mesh=storylandVehicleRenderFrame(definition,
            {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
            vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
            false,vehicle.craneCarried?&vehicle.craneFrame:nullptr,vehicle.bikeVisualLean);
        StorylandVehicleLampState state;
        state.siren=true;state.serviceRole=storylandVehicleSirenRole(runtime,vehicle);
        state.clockSeconds=clockSeconds;state.steeringAngle=vehicle.steeringAngle;
        const auto frame=storylandVehicleLights(definition,
            storylandVehicleMatrixProduct(mesh,definition.previewRoot),state,camera);
        if(frame.pointLightCount>frame.emergencyPointLightBegin) {
            for(size_t index=frame.emergencyPointLightBegin;index<frame.pointLightCount;++index) {
                auto light=frame.pointLights[index];
                for(float& channel:light.color)channel*=1.8f;
                submit(light);
            }
        } else if(frame.emergency.coronaCount) {
            const auto& corona=frame.emergency.coronas[0];
            StorylandPs2PointLight light;
            light.position=corona.position;light.radius=6;
            for(size_t axis=0;axis<3;++axis)light.color[axis]=float(corona.color[axis])/180.0f;
            submit(light);
        }
    }
    for(const auto& particle:runtime.particles) {
        if(particle.kind!=StorylandParticleKind::MuzzleFlash||particle.remaining<=0||
            particle.lifetime<=0||!std::isfinite(particle.remaining)||!std::isfinite(particle.lifetime))continue;
        const auto position=storylandMuzzleFlashPosition(particle,runtime);
        const float fade=std::clamp(particle.remaining/particle.lifetime,0.0f,1.0f);
        StorylandPs2PointLight light;
        light.position={position.x,position.y,position.z};light.radius=6;
        light.color={1.8f*fade,1.05f*fade,.35f*fade};
        submit(light);
    }
    for(const auto& trace:runtime.shotTraces) {
        if(!std::isfinite(trace.remaining)||!std::isfinite(trace.lifetime)||
            trace.remaining<=0||trace.lifetime<=0||trace.remaining>=trace.lifetime)continue;
        auto from=trace.from,to=trace.to;
        float fade=std::clamp(trace.remaining/trace.lifetime,0.0f,1.0f);
        if(!trace.vcs) {
            const auto segment=storylandWeaponTraceSegment(trace);
            if(!segment.visible)continue;
            from=segment.tail;to=segment.head;fade=segment.alpha;
        }
        for(float fraction:{.125f,.5f,.875f}) {
            StorylandPs2PointLight light;
            light.position={from.x+(to.x-from.x)*fraction,from.y+(to.y-from.y)*fraction,
                from.z+(to.z-from.z)*fraction};
            light.color={.85f*fade,.42f*fade,.10f*fade};light.radius=2.5f;
            submit(light);
        }
    }
}
