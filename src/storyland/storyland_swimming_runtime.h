#pragma once

#include "src/storyland/storyland_swimming_particles.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_mission_vm.h"

inline void storylandEmitSwimmingParticle(StorylandPlayTestRuntime& runtime,
    const StorylandSwimmingParticleRequest& request) {
    if(runtime.particles.size()>=2048)return;
    const size_t before=runtime.particles.size();
    if(runtime.missionEnvironment&&runtime.missionEnvironment->createScriptParticles) {
        StorylandMissionEnvironment::ScriptParticle native;
        native.type=request.nativeType;
        native.position={request.position.x,request.position.y,request.position.z};
        native.velocity={request.velocity.x/50.0f,request.velocity.y/50.0f,request.velocity.z/50.0f};
        native.size=request.size;native.lifetimeMilliseconds=request.lifetimeMilliseconds;
        std::string error;
        runtime.missionEnvironment->createScriptParticles({native},error);
    }
    if(runtime.particles.size()==before) {
        StorylandRuntimeParticle particle;
        particle.position=request.position;particle.velocity=request.velocity;
        particle.kind=StorylandParticleKind::Water;
        particle.nativeType=request.nativeType;particle.nativeVcs=runtime.crime.vcs;
        particle.size=request.size>0?request.size:request.surface?.22f:.11f;
        particle.remaining=particle.lifetime=request.lifetimeMilliseconds?
            float(request.lifetimeMilliseconds)*.001f:request.surface?.7f:.8f;
        runtime.particles.push_back(particle);
    }
    for(size_t index=before;index<runtime.particles.size();++index) {
        auto& particle=runtime.particles[index];
        particle.swimmingParticle=true;particle.waterSurface=request.surface;
        particle.waterAnchoredSplash=request.nativeType==75&&request.velocity.x==0&&
            request.velocity.y==0&&request.velocity.z==0;
        particle.waterHeading=request.heading;
        if(request.surface||particle.waterAnchoredSplash)particle.nativeGravity=0;
    }
}

inline void storylandUpdateSwimmingParticles(StorylandPlayTestRuntime& runtime,
    StorylandCharacterWaterState& water,const StorylandNativePoint& position,
    float surface,float heading,float speed,float verticalSpeed,float seconds,
    bool entered,bool alive) {
    StorylandSwimmingParticleContext context;
    context.position=position;context.surface=surface;context.heading=heading;
    context.speed=speed;context.verticalSpeed=verticalSpeed;context.seconds=seconds;
    context.vcs=runtime.crime.vcs;context.entered=entered;context.alive=alive;
    storylandAdvanceSwimmingParticles(water,context,[&](const auto& request) {
        storylandEmitSwimmingParticle(runtime,request);
    });
}
