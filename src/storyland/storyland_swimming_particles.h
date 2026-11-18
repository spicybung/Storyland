#pragma once

#include "src/storyland/storyland_water.h"
#include "src/storyland/storyland_weapon_types.h"
#include <algorithm>
#include <cmath>

struct StorylandSwimmingParticleRequest {
    StorylandNativePoint position,velocity;
    int32_t nativeType=75;
    bool surface=false;
    float heading=0;
    float size=0;
    uint32_t lifetimeMilliseconds=0;
};

struct StorylandSwimmingParticleContext {
    StorylandNativePoint position;
    float surface=0,heading=0,speed=0,verticalSpeed=0,seconds=0;
    bool vcs=false,entered=false,alive=true;
};

template<class Emit>
void storylandAdvanceSwimmingParticles(StorylandCharacterWaterState& water,
    const StorylandSwimmingParticleContext& context,Emit emit) {
    if(!std::isfinite(context.seconds)||context.seconds<=0)return;
    auto& state=water.particles;
    if(!water.active||!std::isfinite(context.surface)||
        !std::isfinite(context.heading)||!std::isfinite(context.position.x)||
        !std::isfinite(context.position.y)||!std::isfinite(context.position.z)) {
        state={};return;
    }
    const float dt=std::min(context.seconds,.1f);
    const float angle=context.heading*.0174532925199433f;
    const float forwardX=std::cos(angle),forwardY=std::sin(angle);
    const float rightX=-forwardY,rightY=forwardX;
    const float travelled=state.positioned?std::hypot(context.position.x-state.previousX,
        context.position.y-state.previousY):0.0f;
    const bool continuous=state.positioned&&travelled<=2.0f&&
        std::fabs(context.position.z-state.previousZ)<=3.0f;
    state.previousX=context.position.x;state.previousY=context.position.y;
    state.previousZ=context.position.z;state.positioned=true;
    if(!continuous) {
        state.strokeTime=0;state.rippleTime=0;state.wakeDistance=0;state.wakeTime=0;
        state.limbStateMask=state.limbUnderwaterMask=0;
    }
    const auto surfaceParticle=[&](int32_t type,float forward,float side) {
        StorylandSwimmingParticleRequest request;
        request.nativeType=type;request.surface=true;request.heading=angle;
        request.velocity.z=-.05f;
        request.lifetimeMilliseconds=700;
        request.position={context.position.x+forwardX*forward+rightX*side,
            context.position.y+forwardY*forward+rightY*side,context.surface+.018f};
        emit(request);
    };
    const auto splash=[&](float forward,float side,float outward,float upward) {
        StorylandSwimmingParticleRequest request;
        request.nativeType=context.vcs?75:26;
        request.heading=angle;
        if(context.vcs) {request.size=.3f;request.lifetimeMilliseconds=400;}
        request.position={context.position.x+forwardX*forward+rightX*side,
            context.position.y+forwardY*forward+rightY*side,context.surface+.025f};
        request.velocity={-forwardX*.2f+rightX*outward,-forwardY*.2f+rightY*outward,upward};
        emit(request);
    };
    if(context.entered) {
        const float impact=std::isfinite(context.verticalSpeed)?
            std::clamp(std::fabs(context.verticalSpeed)*.18f,0.0f,1.8f):0.0f;
        for(unsigned index=0;index<8;++index) {
            const float ring=float(index)*.7853981633974483f;
            const float radialX=std::cos(ring),radialY=std::sin(ring);
            StorylandSwimmingParticleRequest request;
            request.nativeType=context.vcs?37:26;request.heading=angle;
            request.position={context.position.x+radialX*.2f,
                context.position.y+radialY*.2f,context.surface+.035f};
            request.velocity={radialX*(.5f+impact*.35f),radialY*(.5f+impact*.35f),
                .75f+impact+float(index%3)*.16f};
            emit(request);
        }
        if(context.vcs)surfaceParticle(77,0,0);
    }
    if(!context.alive||water.diving||water.underwater||
        context.position.z<context.surface-1.55f) {
        state.strokeTime=0;state.rippleTime=0;state.wakeDistance=0;state.wakeTime=0;
        state.limbSampleMask=state.limbStateMask=state.limbUnderwaterMask=0;
        return;
    }
    bool limbSamples=false;
    if(context.vcs) {
        for(unsigned index=0;index<state.limbPositions.size();++index) {
            const uint8_t bit=uint8_t(1u<<index);
            if(!(state.limbSampleMask&bit)) {state.limbStateMask&=uint8_t(~bit);continue;}
            limbSamples=true;
            const auto& point=state.limbPositions[index];
            const bool submerged=point[2]<context.surface-.1f;
            const bool wasSubmerged=(state.limbUnderwaterMask&bit)!=0;
            if((state.limbStateMask&bit)&&submerged!=wasSubmerged) {
                StorylandSwimmingParticleRequest request;
                request.position={point[0],point[1],context.surface+.018f};
                request.heading=angle;
                if(submerged) {
                    request.nativeType=75;request.size=.3f;
                    request.lifetimeMilliseconds=400;
                } else {
                    request.nativeType=44;request.size=.006f;
                    request.velocity={forwardX*5.0f,forwardY*5.0f,1.0f};
                }
                emit(request);
            }
            state.limbStateMask|=bit;
            if(submerged)state.limbUnderwaterMask|=bit;
            else state.limbUnderwaterMask&=uint8_t(~bit);
        }
        state.limbSampleMask=0;
    }
    const float speed=continuous?std::min(4.0f,travelled/dt):0.0f;
    const bool swimming=!water.drowning&&speed>.12f&&
        std::isfinite(context.speed)&&context.speed>.1f;
    if(swimming) {
        state.strokeTime+=dt;
        const float interval=speed>1.3f?.30f:.46f;
        if(!limbSamples&&state.strokeTime>=interval) {
            state.strokeTime=std::fmod(state.strokeTime,interval);
            const float side=(state.strokeIndex++&1u)?-.34f:.34f;
            for(unsigned index=0;index<3;++index)
                splash(.16f-float(index)*.09f,side,side*(1.5f+float(index)*.3f),
                    .45f+float(index)*.16f);
        }
        if(context.vcs) {
            // SLUS-21590 0x1F3F58: alternating SWIMWAKE rows, 700 ms.
            state.wakeTime+=dt*50.0f;
            const unsigned ticks=std::min(5u,unsigned(state.wakeTime+.00001f));
            state.wakeTime=std::max(0.0f,state.wakeTime-float(ticks));
            for(unsigned tick=0;tick<ticks;++tick) {
                const float along=-.38f-speed*dt*(1.0f-float(tick+1)/std::max(1u,ticks));
                surfaceParticle((state.wakeIndex++&1u)?76:77,along,0);
            }
        }
        state.rippleTime=0;
    } else {
        state.strokeTime=0;state.wakeDistance=0;state.wakeTime=0;
        state.rippleTime+=dt;
        const float interval=water.drowning?.55f:.9f;
        if(state.rippleTime>=interval) {
            state.rippleTime=std::fmod(state.rippleTime,interval);
            if(context.vcs)surfaceParticle(77,0,0);
            if(water.drowning) {
                splash(0,.3f,.5f,.65f);
                splash(0,-.3f,-.5f,.65f);
            }
        }
    }
}

template<class SampleSurface,class Obstructed>
bool storylandAdvanceSwimmingParticle(StorylandRuntimeParticle& particle,float seconds,
    SampleSurface sampleSurface,Obstructed obstructed) {
    if(!particle.swimmingParticle)return false;
    if(!std::isfinite(seconds)||seconds<=0)return true;
    const float dt=std::min(seconds,.1f);
    const auto previous=particle.position;
    particle.position.x+=particle.velocity.x*dt;
    particle.position.y+=particle.velocity.y*dt;
    if(!particle.waterSurface&&!particle.waterAnchoredSplash) {
        particle.position.z+=particle.velocity.z*dt;
        particle.velocity.z-=(particle.nativePhysics?particle.nativeGravity*2500.0f:4.0f)*dt;
        if(particle.nativePhysics&&particle.nativeFriction>1&&particle.nativeFriction<=100) {
            const float drag=std::pow(float(particle.nativeFriction)*.01f,dt*50.0f);
            particle.velocity.x*=drag;particle.velocity.y*=drag;particle.velocity.z*=drag;
        }
    }
    float surface=0;
    if(!sampleSurface(particle.position.x,particle.position.y,surface)||
        !std::isfinite(surface)||(!particle.waterSurface&&!particle.waterAnchoredSplash&&obstructed(previous,particle.position))) {
        particle.remaining=0;return true;
    }
    if(particle.waterSurface||particle.waterAnchoredSplash) {
        particle.position.z=surface+.018f;
    } else if(particle.position.z<=surface&&particle.velocity.z<=0) {
        particle.remaining=0;return true;
    }
    if(particle.nativePhysics)
        particle.size=std::max(0.0f,particle.size+particle.nativeExpansion*50.0f*dt);
    else if(particle.waterSurface)particle.size+=dt*.24f;
    else {
        const float age=1.0f-particle.remaining/std::max(.001f,particle.lifetime);
        const int frames=particle.nativeType==36||particle.nativeType==37?5:
            particle.nativeType==44||particle.nativeType==26?4:3;
        particle.nativeFrame=uint8_t(std::clamp(int(age*float(frames)),0,frames-1));
    }
    return true;
}
