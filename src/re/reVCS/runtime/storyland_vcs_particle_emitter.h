#pragma once

#include "src/storyland/storyland_mission_vm.h"
#include <cmath>
#include <vector>

template<class Random>
std::vector<StorylandMissionEnvironment::ScriptParticle> storylandVcsParticleSubtype8(
    const StorylandMissionEnvironment::ScriptParticle& emitter,Random random) {
    using Particle=StorylandMissionEnvironment::ScriptParticle;
    std::vector<Particle> particles;
    particles.reserve(28);
    const auto direction=[](unsigned index) {
        const float angle=(float(index)*.3515625f)*.01745329424738884f;
        return std::array<float,2>{std::cos(angle),std::sin(angle)};
    };
    const auto place=[&](Particle& particle,float x,float y,float radius) {
        particle.position={emitter.position[0]+x*radius,emitter.position[1]+y*radius,
            emitter.position[2]};
    };
    for(unsigned ring=0;ring<3;++ring) {
        const auto axis=direction(ring*90);
        for(unsigned quadrant=0;quadrant<4;++quadrant) {
            const float x=quadrant<2?axis[0]:-axis[0];
            const float y=(quadrant&1)?-axis[1]:axis[1];
            auto particle=emitter;
            particle.type=66;particle.color={255,255,255,196};
            particle.size=.1f+(.8f-.1f)*random();
            place(particle,x,y,.75f);
            particle.velocity={emitter.velocity[0]+x*.05f,emitter.velocity[1]+y*.05f,
                emitter.velocity[2]+(.04f+(.08f-.04f)*random())};
            particles.push_back(particle);
            particle.type=44;particle.color=emitter.color;
            particle.size=.1f+(.5f-.1f)*random();
            particles.push_back(particle);
        }
    }
    const auto axis=direction(180);
    for(unsigned quadrant=0;quadrant<4;++quadrant) {
        const float x=quadrant<2?axis[0]:-axis[0];
        const float y=(quadrant&1)?-axis[1]:axis[1];
        auto particle=emitter;
        particle.type=44;particle.size=.4f+(1.0f-.4f)*random();
        place(particle,x,y,.5f);
        particle.velocity={emitter.velocity[0]+x*(-.25f+.5f*random()),
            emitter.velocity[1]+y*(-.25f+.5f*random()),
            emitter.velocity[2]+(.05f+(.25f-.05f)*random())};
        particles.push_back(particle);
    }
    return particles;
}
