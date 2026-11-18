#pragma once

#include "src/storyland/storyland_aero_emission.h"
#include "src/graphics/storyland_pickup_render.h"

template<class Texture>
void storylandDrawMuzzleEmission(const StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const std::array<float,3>& camera,Texture textureFor) {
    std::vector<StorylandVcsPickupVisual> visuals;
    for(const auto& particle:runtime.particles) {
        if(particle.kind!=StorylandParticleKind::MuzzleFlash||particle.remaining<=0||
            particle.lifetime<=0||!std::isfinite(particle.remaining)||!std::isfinite(particle.lifetime))continue;
        const auto position=storylandMuzzleFlashPosition(particle,runtime);
        if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z))continue;
        if(visuals.empty()||visuals.back().coronaCount==visuals.back().coronas.size())visuals.emplace_back();
        auto& visual=visuals.back();
        auto& corona=visual.coronas[visual.coronaCount++];
        const float fade=std::clamp(particle.remaining/particle.lifetime,0.0f,1.0f);
        corona.position={position.x,position.y,position.z};
        corona.color={uint8_t(115*fade),uint8_t(72*fade),uint8_t(24*fade)};
        corona.radius=.23f;corona.range=60;corona.pullAway=0;corona.texture=1;corona.alpha=255;
    }
    storylandDrawVcsPickupVisuals(visuals,world,camera,textureFor);
}
