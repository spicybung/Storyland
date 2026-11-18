#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

enum class StorylandPs2LightingGame {
    LibertyCityStories,
    ViceCityStories
};

inline float storylandPs2FloatConstant(std::uint32_t bits) {
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline std::uint8_t storylandPs2ColorByte(float value) {
    if (!(value > 0.0f)) return 0;
    if (value >= 255.0f) return 255;
    return static_cast<std::uint8_t>(value);
}

inline std::uint8_t storylandPs2UnitColorByte(float value) {
    return storylandPs2ColorByte(value * 255.0f);
}

inline std::array<float, 3> storylandPs2NormalizeLightDirection(
    const std::array<float, 3>& direction) {
    for (float component : direction)
        if (!std::isfinite(component)) return {};
    const float planarSquared = direction[0] * direction[0] + direction[1] * direction[1];
    const float lengthSquared = planarSquared + direction[2] * direction[2];
    if (!(lengthSquared > 0.0f) || !std::isfinite(lengthSquared)) return {};
    const float reciprocalLength = 1.0f / std::sqrt(lengthSquared);
    return {direction[0] * reciprocalLength, direction[1] * reciprocalLength,
        direction[2] * reciprocalLength};
}

inline std::uint8_t storylandPs2MaterialAlpha(std::uint8_t alpha) {
    return static_cast<std::uint8_t>((static_cast<unsigned>(alpha) + 1u) >> 1u);
}

inline std::uint32_t storylandPs2MaterialColor(std::uint32_t rgba) {
    const auto alpha = storylandPs2MaterialAlpha(static_cast<std::uint8_t>(rgba >> 24));
    return (rgba & 0x00FFFFFFu) | (static_cast<std::uint32_t>(alpha) << 24);
}

struct StorylandPs2WorldLighting {
    std::array<float, 4> multiplier{1.0f, 1.0f, 1.0f, 1.0f};
    std::array<float, 4> additive{};
    std::array<float, 4> ceiling{255.0f, 255.0f, 255.0f, 255.0f};
};

inline StorylandPs2WorldLighting storylandPs2WorldLighting(
    const std::array<std::uint8_t, 3>& ambient,
    const std::array<std::uint8_t, 3>& emissive,
    std::uint8_t alpha = 255,
    bool packedColorCorrection = true) {
    StorylandPs2WorldLighting result;
    const float reciprocal255 = storylandPs2FloatConstant(0x3B808081u);
    const float packedColorScale = storylandPs2FloatConstant(0x3F839CE7u);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        result.multiplier[channel] = static_cast<float>(ambient[channel]) * reciprocal255;
        if (packedColorCorrection) result.multiplier[channel] *= packedColorScale;
        result.additive[channel] = static_cast<float>(emissive[channel]) * 0.5f;
    }
    result.multiplier[3] = static_cast<float>(alpha) * reciprocal255;
    return result;
}

inline std::array<std::uint8_t, 4> storylandPs2WorldVertexColor(
    const std::array<std::uint8_t, 4>& nativePrelight,
    const StorylandPs2WorldLighting& lighting) {
    std::array<std::uint8_t, 4> result{};
    for (std::size_t channel = 0; channel < result.size(); ++channel) {
        const float product = static_cast<float>(nativePrelight[channel]) * lighting.multiplier[channel];
        const float sum = product + lighting.additive[channel];
        result[channel] = storylandPs2ColorByte(std::min(sum, lighting.ceiling[channel]));
    }
    return result;
}

struct StorylandPs2ActorLighting {
    std::array<std::array<float, 3>, 4> directions{};
    std::array<std::array<float, 3>, 4> colors{};
    std::array<float, 3> ambient{};
    std::array<float, 3> ceiling{255.0f, 255.0f, 255.0f};
};

struct StorylandPs2PointLight {
    std::array<float, 3> position{};
    std::array<float, 3> direction{};
    std::array<float, 3> color{};
    float radius = 0.0f;
    std::uint8_t type = 0;
};

struct StorylandPs2PointLights {
    std::array<StorylandPs2PointLight, 32> lights{};
    std::size_t count = 0;
};

inline std::array<float, 3> storylandPs2PointLightColor(
    const std::array<std::uint8_t, 3>& authoredColor, float spriteBrightness) {
    std::array<float, 3> result{};
    for (std::size_t channel = 0; channel < result.size(); ++channel) {
        const float bright = static_cast<float>(authoredColor[channel]) * spriteBrightness;
        result[channel] = bright * (1.0f / 256.0f);
    }
    return result;
}

struct StorylandPs2LocalLights {
    std::array<std::array<float, 3>, 3> directions{};
    std::array<std::array<float, 3>, 3> colors{};
    std::array<int, 3> priorities{};
    std::size_t count = 0;
    float ambientMultiplier = 1.0f;
};

inline bool storylandPs2RegisterPointLight(
    StorylandPs2PointLights& output, const StorylandPs2PointLight& input,
    const std::array<float, 3>& cameraPosition) {
    const float x = input.position[0] - cameraPosition[0];
    const float y = input.position[1] - cameraPosition[1];
    if (!(x < 22.0f && x > -22.0f && y < 22.0f && y > -22.0f) || output.count >= output.lights.size())
        return false;
    const float z = input.position[2] - cameraPosition[2];
    const float planarSquared = x * x + y * y;
    const float distance = std::sqrt(planarSquared + z * z);
    if (!(distance < 22.0f)) return false;
    auto registered = input;
    if (!(distance < 16.5f)) {
        const float fraction = distance * storylandPs2FloatConstant(0x3D3A2E8Cu);
        const float fade = 1.0f - (fraction - 0.75f) * 4.0f;
        for (float& channel : registered.color) channel *= fade;
    }
    output.lights[output.count++] = registered;
    return true;
}

inline StorylandPs2LocalLights storylandPs2SelectLocalLights(
    const StorylandPs2PointLight* lights, std::size_t count,
    const std::array<float, 3>& entityPosition) {
    StorylandPs2LocalLights result;
    if (!lights) return result;
    for (std::size_t index = 0; index < count; ++index) {
        const auto& light = lights[index];
        if (light.type == 3 || light.type == 4) continue;
        const float x = light.position[0] - entityPosition[0];
        const float y = light.position[1] - entityPosition[1];
        const float z = light.position[2] - entityPosition[2];
        const float planarSquared = x * x + y * y;
        const float distance = std::sqrt(planarSquared + z * z);
        if (!(distance < light.radius)) continue;
        const float ratio = distance / light.radius;
        if (light.type == 2) {
            result.ambientMultiplier *= ratio;
            continue;
        }
        float attenuation = 1.0f;
        if (!(ratio < 0.5f)) {
            const float beyondHalf = ratio - 0.5f;
            attenuation = 1.0f - (beyondHalf + beyondHalf);
        }
        if (distance == 0.0f) continue;
        const float reciprocalDistance = 1.0f / distance;
        const std::array<float, 3> direction{x * reciprocalDistance, y * reciprocalDistance, z * reciprocalDistance};
        if (light.type == 1) {
            const float first = direction[0] * light.direction[0];
            const float second = first + direction[1] * light.direction[1];
            const float dot = second + direction[2] * light.direction[2];
            const float cone = -dot - 0.5f;
            attenuation *= std::max(cone + cone, 0.0f);
        }
        if (!(attenuation > 0.0f)) continue;
        attenuation *= 0.5f;
        std::array<float, 3> color{};
        for (std::size_t channel = 0; channel < color.size(); ++channel)
            color[channel] = light.color[channel] * attenuation;
        const float maximum = std::max(std::max(color[0], color[1]), color[2]);
        if (!std::isfinite(maximum) || maximum < -2147483648.0f || maximum >= 2147483648.0f) continue;
        const int priority = static_cast<int>(maximum);
        std::size_t slot = result.count;
        if (slot == result.colors.size()) {
            int lowest = priority;
            for (std::size_t candidate = 0; candidate < result.count; ++candidate) {
                if (result.priorities[candidate] < lowest) {
                    lowest = result.priorities[candidate];
                    slot = candidate;
                }
            }
            if (slot == result.colors.size()) continue;
        }
        result.directions[slot] = direction;
        result.colors[slot] = color;
        result.priorities[slot] = priority;
        result.count = std::min(result.count + 1, result.colors.size());
    }
    return result;
}

inline StorylandPs2LocalLights storylandPs2SelectLocalLights(
    const StorylandPs2PointLights& lights, const std::array<float, 3>& entityPosition) {
    return storylandPs2SelectLocalLights(lights.lights.data(), std::min(lights.count, lights.lights.size()), entityPosition);
}

inline std::uint8_t storylandPs2ActorAmbientByte(
    float normalizedAmbient, StorylandPs2LightingGame game) {
    const unsigned first = storylandPs2UnitColorByte(normalizedAmbient);
    if (game == StorylandPs2LightingGame::LibertyCityStories)
        return static_cast<std::uint8_t>(std::min(first << 1u, 255u));
    const float gain = storylandPs2FloatConstant(0x3FCCCCCDu);
    return storylandPs2ColorByte(static_cast<float>(first) * gain);
}

inline std::array<std::uint8_t, 4> storylandPs2ActorVertexColor(
    const std::array<std::int8_t, 3>& packedNormal,
    const std::array<std::uint8_t, 4>& materialColor,
    const StorylandPs2ActorLighting& lighting) {
    std::array<float, 4> diffuse{};
    for (std::size_t light = 0; light < diffuse.size(); ++light) {
        const float x = lighting.directions[light][0] * (1.0f / 128.0f);
        const float y = lighting.directions[light][1] * (1.0f / 128.0f);
        const float z = lighting.directions[light][2] * (1.0f / 128.0f);
        const float first = x * static_cast<float>(packedNormal[0]);
        const float second = first + y * static_cast<float>(packedNormal[1]);
        diffuse[light] = std::max(second + z * static_cast<float>(packedNormal[2]), 0.0f);
    }
    std::array<std::uint8_t, 4> result{};
    const float reciprocal510 = storylandPs2FloatConstant(0x3B008081u);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        const float material = static_cast<float>(materialColor[channel]) * reciprocal510;
        float sum = (lighting.colors[0][channel] * material) * diffuse[0];
        for (std::size_t light = 1; light < diffuse.size(); ++light)
            sum += (lighting.colors[light][channel] * material) * diffuse[light];
        sum += lighting.ambient[channel] * material;
        result[channel] = storylandPs2ColorByte(std::min(sum, lighting.ceiling[channel]));
    }
    result[3] = storylandPs2MaterialAlpha(materialColor[3]);
    return result;
}

struct StorylandPs2LightingColors {
    StorylandPs2WorldLighting world;
    std::array<float, 3> actorAmbient{};
    std::array<float, 3> actorDirectional{};
};

struct StorylandPs2SelectedLighting {
    std::array<float, 3> worldAmbient{};
    std::array<float, 3> objectAmbient{};
    std::array<float, 3> directional{};
};

inline StorylandPs2SelectedLighting storylandPs2SelectLighting(
    const std::array<float, 3>& regularAmbient,
    const std::array<float, 3>& blurAmbient,
    const std::array<float, 3>& regularObjectAmbient,
    const std::array<float, 3>& blurObjectAmbient,
    const std::array<float, 3>& directional,
    bool useBlurAmbient = true,
    float globalIntensity = 1.0f,
    int brightness = 256,
    bool forceWhiteAmbient = false) {
    StorylandPs2SelectedLighting result;
    const auto& world = useBlurAmbient ? blurAmbient : regularAmbient;
    const auto& object = useBlurAmbient ? blurObjectAmbient : regularObjectAmbient;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        result.worldAmbient[channel] = forceWhiteAmbient ? 1.0f : world[channel] * globalIntensity;
        result.objectAmbient[channel] = forceWhiteAmbient ? 1.0f : object[channel] * globalIntensity;
        result.directional[channel] = directional[channel] * globalIntensity;
    }
    if (brightness > 256) {
        const float delta = static_cast<float>(brightness) * (1.0f / 256.0f) - 1.0f;
        const float worldFactor = 1.0f + delta * storylandPs2FloatConstant(0x3FE66667u);
        const float objectFactor = 1.0f + (delta + delta) * storylandPs2FloatConstant(0x3F19999Au);
        for (std::size_t channel = 0; channel < 3; ++channel) {
            result.worldAmbient[channel] = std::min(result.worldAmbient[channel] * worldFactor, 1.0f);
            result.objectAmbient[channel] = std::min(result.objectAmbient[channel] * objectFactor, 1.0f);
        }
    }
    return result;
}

inline StorylandPs2LightingColors storylandPs2LightingColors(
    StorylandPs2LightingGame game,
    const std::array<float, 3>& regularAmbient,
    const std::array<float, 3>& selectedWorldAmbient,
    const std::array<float, 3>& selectedObjectAmbient,
    const std::array<float, 3>& directional) {
    StorylandPs2LightingColors result;
    std::array<std::uint8_t, 3> worldAmbient{};
    std::array<std::uint8_t, 3> worldEmissive{};
    for (std::size_t channel = 0; channel < 3; ++channel) {
        worldAmbient[channel] = storylandPs2UnitColorByte(regularAmbient[channel]);
        worldEmissive[channel] = storylandPs2UnitColorByte(selectedWorldAmbient[channel]);
        result.actorAmbient[channel] = static_cast<float>(storylandPs2ActorAmbientByte(selectedObjectAmbient[channel], game));
        result.actorDirectional[channel] = static_cast<float>(storylandPs2UnitColorByte(directional[channel]));
    }
    result.world = storylandPs2WorldLighting(worldAmbient, worldEmissive);
    return result;
}

inline std::array<std::uint8_t, 3> storylandPs2FogColorBytes(
    const std::array<std::uint8_t, 3>& skyTop,
    const std::array<std::uint8_t, 3>& skyBottom) {
    std::array<std::uint8_t, 3> result{};
    for (std::size_t channel = 0; channel < result.size(); ++channel)
        result[channel] = static_cast<std::uint8_t>((static_cast<unsigned>(skyTop[channel]) +
            2u * static_cast<unsigned>(skyBottom[channel])) / 3u);
    return result;
}

inline std::array<float, 3> storylandPs2FogColor(
    const std::array<float, 3>& skyTop,
    const std::array<float, 3>& skyBottom) {
    std::array<std::uint8_t, 3> top{}, bottom{};
    for (std::size_t channel = 0; channel < top.size(); ++channel) {
        top[channel] = storylandPs2UnitColorByte(skyTop[channel]);
        bottom[channel] = storylandPs2UnitColorByte(skyBottom[channel]);
    }
    const auto bytes = storylandPs2FogColorBytes(top, bottom);
    return {static_cast<float>(bytes[0]) / 255.0f,
        static_cast<float>(bytes[1]) / 255.0f,
        static_cast<float>(bytes[2]) / 255.0f};
}

struct StorylandPs2FogParameters {
    float slope = 0.0f;
    float intercept = 255.0f;
    bool enabled = false;
};

inline StorylandPs2FogParameters storylandPs2FogParameters(float start, float farClip, bool enabled = true) {
    if (!std::isfinite(start) || !std::isfinite(farClip) || farClip <= start)
        return {};
    const float distance = farClip - start;
    const float quotient = start / distance;
    return {-255.0f / distance, (quotient + 1.0f) * 255.0f, enabled};
}

inline std::uint8_t storylandPs2FogVertexByte(float depth, const StorylandPs2FogParameters& parameters) {
    if (!parameters.enabled) return 255;
    const float product = depth * parameters.slope;
    const float fog = std::clamp(product + parameters.intercept, 0.0f, 255.0f);
    const unsigned fixed = static_cast<unsigned>(fog * 16.0f);
    return static_cast<std::uint8_t>(fixed >> 4u);
}

struct StorylandPs2EnvironmentBasis {
    std::array<float, 3> s{1.0f, 0.0f, 0.0f};
    std::array<float, 3> t{0.0f, 0.0f, -1.0f};
};

inline StorylandPs2EnvironmentBasis storylandPs2EnvironmentBasis(
    const std::array<float, 3>& nativeCameraAxis) {
    StorylandPs2EnvironmentBasis result;
    const auto& down = result.t;
    result.s[0] = down[1] * nativeCameraAxis[2] - down[2] * nativeCameraAxis[1];
    result.s[1] = (-down[0]) * nativeCameraAxis[2] + down[2] * nativeCameraAxis[0];
    result.s[2] = down[0] * nativeCameraAxis[1] - down[1] * nativeCameraAxis[0];
    const float planarSquared = result.s[0] * result.s[0] + result.s[1] * result.s[1];
    const float squaredLength = planarSquared + result.s[2] * result.s[2];
    if (!(squaredLength > 0.0f)) {
        result.s[0] = 1.0f;
        return result;
    }
    const float reciprocalLength = static_cast<float>(1.0 / std::sqrt(static_cast<double>(squaredLength)));
    for (float& component : result.s) component *= reciprocalLength;
    return result;
}

inline std::array<float, 2> storylandPs2EnvironmentUv(
    const std::array<std::int8_t, 3>& packedNormal,
    const std::array<std::array<float, 2>, 3>& basis) {
    std::array<float, 2> result{};
    for (std::size_t channel = 0; channel < result.size(); ++channel) {
        const float first = (basis[0][channel] * (1.0f / 256.0f)) * static_cast<float>(packedNormal[0]);
        const float second = first + (basis[1][channel] * (1.0f / 256.0f)) * static_cast<float>(packedNormal[1]);
        const float third = second + (basis[2][channel] * (1.0f / 256.0f)) * static_cast<float>(packedNormal[2]);
        result[channel] = 0.5f + third;
    }
    return result;
}

inline float storylandPs2EnvironmentIntensity(unsigned hour) {
    const float dayFraction = static_cast<float>(hour) * storylandPs2FloatConstant(0x3D2AAAABu);
    const float distanceFromNoon = std::fabs(dayFraction - 0.5f);
    const float intensity = 1.0f - distanceFromNoon;
    return intensity * intensity;
}

inline std::uint8_t storylandPs2EnvironmentCoefficient(
    float materialIntensity, float frameIntensity, StorylandPs2LightingGame game,
    bool suppress = false, bool forceFull = false) {
    const float halfFrame = frameIntensity * 0.5f;
    const float product = materialIntensity * halfFrame;
    const float unit = std::clamp(product, 0.0f, 1.0f);
    const auto first = storylandPs2ColorByte(unit * 255.0f);
    const float scale = game == StorylandPs2LightingGame::LibertyCityStories
        ? storylandPs2FloatConstant(0x3F866666u) : 1.0f;
    std::uint8_t result = storylandPs2ColorByte(static_cast<float>(first) * scale);
    if (suppress) result = 0;
    if (forceFull) result = 255;
    return result;
}

inline std::uint8_t storylandPs2EnvironmentBlendFix(std::uint8_t coefficient) {
    return static_cast<std::uint8_t>(coefficient >> 1u);
}

inline std::uint64_t storylandPs2EnvironmentBlendRegister(
    std::uint8_t coefficient, bool additive = false, bool sourceAlpha = false) {
    const std::uint64_t fix = storylandPs2EnvironmentBlendFix(coefficient);
    const std::uint64_t selectors = (additive ? 0x48u : 0x44u) | (sourceAlpha ? 0u : 0x20u);
    return (fix << 32u) | selectors;
}
