#pragma once

#include "src/storyland/storyland_c2d_lights.h"
#include "src/graphics/storyland_ps2_lighting.h"

#include <array>
#include <cmath>

inline StorylandPs2PointLights storylandCollectPs2PointLights(
    const std::vector<StorylandDtz2dfxEffect>& effects,
    const std::vector<StorylandDtz2dfxWorldInstance>& instances,
    const StorylandNativeWorld& world, const StorylandC2dLightClock& clock,
    const StorylandNativePoint& camera, float spriteBrightness) {
    StorylandPs2PointLights result;
    const std::array<float,3> cameraPosition{camera.x,camera.y,camera.z};
    storylandForEachC2dPointLight(effects,instances,world,clock,camera,
        [&](const StorylandC2dPointLight& source) {
            std::array<std::uint8_t,3> authoredColor{};
            for(std::size_t channel=0;channel<authoredColor.size();++channel)
                authoredColor[channel]=static_cast<std::uint8_t>(std::clamp(
                    std::lround(source.color[channel]*255.0f),0l,255l));
            StorylandPs2PointLight light;
            light.position={source.position.x,source.position.y,source.position.z};
            light.color=storylandPs2PointLightColor(authoredColor,spriteBrightness);
            light.radius=source.radius;
            storylandPs2RegisterPointLight(result,light,cameraPosition);
        });
    return result;
}

inline StorylandPs2LightingColors storylandPs2EntityLighting(
    StorylandPs2LightingGame game, const std::array<float,3>& regularAmbient,
    const StorylandPs2SelectedLighting& selected, float ambientMultiplier) {
    auto objectAmbient=selected.objectAmbient;
    auto directional=selected.directional;
    if(ambientMultiplier!=1.0f) {
        for(std::size_t channel=0;channel<objectAmbient.size();++channel) {
            objectAmbient[channel]=selected.worldAmbient[channel]*ambientMultiplier;
            directional[channel]*=ambientMultiplier;
        }
    }
    return storylandPs2LightingColors(game,regularAmbient,selected.worldAmbient,
        objectAmbient,directional);
}

inline std::array<float,3> storylandPs2LocalLightBytes(const std::array<float,3>& color) {
    std::array<float,3> result{};
    for(std::size_t channel=0;channel<result.size();++channel)
        result[channel]=static_cast<float>(storylandPs2UnitColorByte(color[channel]));
    return result;
}

inline std::array<float,3> storylandPs2EyeDirection(
    const float* view, const std::array<float,3>& direction) {
    std::array<float,3> result{};
    for(std::size_t axis=0;axis<result.size();++axis)
        result[axis]=view[axis]*direction[0]+view[4+axis]*direction[1]+view[8+axis]*direction[2];
    return result;
}

inline std::array<float,3> storylandPs2EntityPosition(const float* view,const float* modelView) {
    const std::array<float,3> eyeTranslation{modelView[12]-view[12],
        modelView[13]-view[13],modelView[14]-view[14]};
    std::array<float,3> result{};
    for(std::size_t axis=0;axis<result.size();++axis)
        result[axis]=view[axis*4]*eyeTranslation[0]+view[axis*4+1]*eyeTranslation[1]+
            view[axis*4+2]*eyeTranslation[2];
    return result;
}
