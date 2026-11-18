#pragma once

#include "src/storyland/storyland_c2d_lights.h"
#include "src/re/reVCS/render/storyland_vcs_psp_lighting.h"

#include <array>
#include <vector>

struct StorylandPspLocalLight {
    std::array<float,3> direction{};
    std::array<float,3> color{};
};

struct StorylandPspLocalLights {
    std::array<StorylandPspLocalLight,2> lights{};
    size_t count=0;
};

inline std::vector<StorylandC2dPointLight> storylandCollectPspPointLights(
    const std::vector<StorylandDtz2dfxEffect>& effects,
    const std::vector<StorylandDtz2dfxWorldInstance>& instances,
    const StorylandNativeWorld& world,const StorylandC2dLightClock& clock,
    const StorylandNativePoint& camera) {
    std::vector<StorylandC2dPointLight> result;
    result.reserve(32);
    storylandForEachC2dPointLight(effects,instances,world,clock,camera,
        [&](const StorylandC2dPointLight& light) {
            if(result.size()>=32)return;
            const float x=light.position.x-camera.x,y=light.position.y-camera.y,z=light.position.z-camera.z;
            const float distanceSquared=x*x+y*y+z*z;
            if(distanceSquared>=22.0f*22.0f)return;
            auto registered=light;
            if(distanceSquared>16.5f*16.5f) {
                const float fade=4.0f*(1.0f-std::sqrt(distanceSquared)/22.0f);
                for(float& component:registered.color)component*=fade;
            }
            result.push_back(registered);
        });
    return result;
}

inline StorylandPspLocalLights storylandPspSelectLocalLights(
    const std::vector<StorylandC2dPointLight>& sources,
    const std::array<float,3>& entityPosition,float spriteBrightness,
    const std::array<float,3>* overrideDirection=nullptr) {
    std::array<StorylandPspLocalLight,3> candidates{};
    std::array<int,3> scores{};
    size_t count=0;
    if(!std::isfinite(spriteBrightness)||spriteBrightness<0.0f)spriteBrightness=0.0f;
    for(const auto& source:sources) {
        if(!std::isfinite(source.radius)||source.radius<=0.0f)continue;
        std::array<float,3> direction{source.position.x-entityPosition[0],
            source.position.y-entityPosition[1],source.position.z-entityPosition[2]};
        const float distance=std::sqrt(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
        if(!std::isfinite(distance)||distance>=source.radius)continue;
        const float attenuation=0.5f*std::clamp(2.0f-2.0f*distance/source.radius,0.0f,1.0f);
        StorylandPspLocalLight candidate;
        candidate.direction=overrideDirection?*overrideDirection:storylandPspExtraLightDirection(direction);
        for(size_t channel=0;channel<3;++channel)
            candidate.color[channel]=source.color[channel]*(255.0f/256.0f)*spriteBrightness*attenuation;
        const int score=int(std::max({candidate.color[0],candidate.color[1],candidate.color[2]}));
        size_t slot=count;
        if(count==candidates.size()) {
            slot=size_t(std::min_element(scores.begin(),scores.end())-scores.begin());
            if(score<=scores[slot])continue;
        } else ++count;
        candidates[slot]=candidate;
        scores[slot]=score;
    }
    StorylandPspLocalLights result;
    result.count=std::min(count,result.lights.size());
    std::copy_n(candidates.begin(),result.count,result.lights.begin());
    return result;
}

inline std::array<float,3> storylandPspEntityPosition(const float* view,const float* modelView) {
    const std::array<float,3> eyeTranslation{modelView[12]-view[12],
        modelView[13]-view[13],modelView[14]-view[14]};
    std::array<float,3> result{};
    for(size_t axis=0;axis<3;++axis)
        result[axis]=view[axis*4]*eyeTranslation[0]+view[axis*4+1]*eyeTranslation[1]+
            view[axis*4+2]*eyeTranslation[2];
    return result;
}
