#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

struct StorylandPs2PostEffects {
    unsigned radiosityIntensity = 0;
    unsigned radiosityThreshold = 0;
    unsigned glowMix = 0;
    float glowOffset = 0;
    std::array<unsigned,3> filter{};

    bool enabled() const {
        return radiosityIntensity != 0 || glowMix != 0 || filter[0] || filter[1] || filter[2];
    }
};

inline unsigned storylandPs2PostEffectInteger(float value, unsigned maximum) {
    if (!std::isfinite(value) || value <= 0) return 0;
    return static_cast<unsigned>(std::min(std::trunc(value), float(maximum)));
}

inline StorylandPs2PostEffects storylandPs2PostEffects(float intensity, float limit,
    float glowIntensity, float glowOffset, const std::array<float,3>& filter) {
    StorylandPs2PostEffects result;
    result.radiosityIntensity = std::min(255u, storylandPs2PostEffectInteger(intensity, 255u) * 4u);
    result.radiosityThreshold = std::min(128u, storylandPs2PostEffectInteger(limit, 512u) / 4u);
    result.glowMix = storylandPs2PostEffectInteger(glowIntensity * 0.4f, 255u) / 2u;
    result.glowOffset = std::isfinite(glowOffset) ? std::clamp(glowOffset, 0.0f, 64.0f) : 0;
    for (unsigned channel = 0; channel < 3; ++channel)
        result.filter[channel] = storylandPs2PostEffectInteger(filter[channel] * 255.0f, 255u) / 2u;
    return result;
}

inline unsigned storylandPs2PostEffectFilter(const std::array<unsigned,4>& samples) {
    unsigned value = 0;
    for (unsigned sample : samples) value += std::min(255u, sample) * 19u / 128u;
    return std::min(255u, value);
}

inline unsigned storylandPs2PostEffectExtract(unsigned source, unsigned threshold) {
    const unsigned subtraction = 255u * std::min(128u, threshold) / 128u;
    return source > subtraction ? source - subtraction : 0;
}

inline unsigned storylandPs2PostEffectAdd(unsigned destination, unsigned source, unsigned coefficient) {
    return std::min(255u, destination + source * coefficient / 128u);
}

inline constexpr const char* StorylandPs2PostEffectsVertexGl = R"GLSL(
#version 120
varying vec2 uv;
void main() { gl_Position = gl_Vertex; uv = gl_MultiTexCoord0.xy; }
)GLSL";

inline constexpr const char* StorylandPs2PostEffectsFragmentGl = R"GLSL(
#version 120
uniform sampler2D sourceTexture;
uniform sampler2D originalTexture;
uniform int operation;
uniform vec4 parameters;
uniform vec4 filterColor;
varying vec2 uv;
vec3 bytesAt(vec2 position) { return floor(texture2D(sourceTexture, position).rgb * 255.0 + 0.0001); }
void main() {
    vec3 color;
    if (operation == 0) {
        color = max(bytesAt(uv) - floor(vec3(255.0 * parameters.x / 128.0)), vec3(0.0));
    } else if (operation == 1 || operation == 2) {
        vec2 stepSize = vec2(1.0 / 256.0);
        vec2 a = operation == 1 ? vec2(-1.0, 0.0) : vec2(-1.0, -1.0);
        vec2 b = operation == 1 ? vec2(1.0, 0.0) : vec2(1.0, -1.0);
        vec2 c = operation == 1 ? vec2(0.0, -1.0) : vec2(-1.0, 1.0);
        vec2 d = operation == 1 ? vec2(0.0, 1.0) : vec2(1.0, 1.0);
        color = floor(bytesAt(uv + a * stepSize) * (19.0 / 128.0))
              + floor(bytesAt(uv + b * stepSize) * (19.0 / 128.0))
              + floor(bytesAt(uv + c * stepSize) * (19.0 / 128.0))
              + floor(bytesAt(uv + d * stepSize) * (19.0 / 128.0));
    } else if (operation == 3) {
        vec3 original = floor(texture2D(originalTexture, uv).rgb * 255.0 + 0.0001);
        color = original + floor(bytesAt(uv) * (parameters.x / 128.0));
    } else if (operation == 4) {
        vec3 original = floor(texture2D(originalTexture, uv).rgb * 255.0 + 0.0001);
        color = original;
        vec2 shifted = vec2((uv.x - parameters.x) / (1.0 - parameters.x), uv.y);
        if (uv.x >= parameters.x)
            color += floor((floor(bytesAt(shifted) * (127.0 / 128.0)) - color) * (parameters.z / 128.0));
        shifted.y = uv.y / (1.0 - parameters.y);
        if (uv.x >= parameters.x && uv.y <= 1.0 - parameters.y)
            color += floor((floor(bytesAt(shifted) * (127.0 / 128.0)) - color) * (parameters.z / 128.0));
        shifted.x = uv.x;
        if (uv.y <= 1.0 - parameters.y)
            color += floor((floor(bytesAt(shifted) * (127.0 / 128.0)) - color) * (parameters.z / 128.0));
        color += floor(bytesAt(uv) * filterColor.xyz / 128.0);
    } else {
        color = floor(bytesAt(uv) * (127.0 / 128.0));
    }
    gl_FragColor = vec4(clamp(color, 0.0, 255.0) / 255.0, texture2D(originalTexture, uv).a);
}
)GLSL";
