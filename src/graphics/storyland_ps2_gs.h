#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

enum class StorylandPs2TextureFunction { Modulate, Decal, Highlight, Highlight2 };

inline std::uint8_t storylandPs2GsClamp(int value) {
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

inline std::array<std::uint8_t, 4> storylandPs2GsTexture(
    const std::array<std::uint8_t, 4>& texel,
    const std::array<std::uint8_t, 4>& fragment,
    StorylandPs2TextureFunction function = StorylandPs2TextureFunction::Modulate,
    bool textureAlpha = true) {
    std::array<std::uint8_t, 4> result{};
    for (std::size_t channel = 0; channel < 3; ++channel) {
        int value = (unsigned(texel[channel]) * unsigned(fragment[channel])) >> 7u;
        if (function == StorylandPs2TextureFunction::Decal) value = texel[channel];
        else if (function == StorylandPs2TextureFunction::Highlight ||
                 function == StorylandPs2TextureFunction::Highlight2) value += fragment[3];
        result[channel] = storylandPs2GsClamp(value);
    }
    int alpha = fragment[3];
    if (textureAlpha) {
        if (function == StorylandPs2TextureFunction::Modulate)
            alpha = (unsigned(texel[3]) * unsigned(fragment[3])) >> 7u;
        else if (function == StorylandPs2TextureFunction::Highlight)
            alpha = unsigned(texel[3]) + unsigned(fragment[3]);
        else alpha = texel[3];
    }
    result[3] = storylandPs2GsClamp(alpha);
    return result;
}

inline std::array<std::uint8_t, 4> storylandPs2GsFog(
    const std::array<std::uint8_t, 4>& color,
    const std::array<std::uint8_t, 3>& fogColor,
    std::uint8_t fog, bool enabled = true) {
    if (!enabled) return color;
    auto result = color;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        const unsigned source = (unsigned(color[channel]) * unsigned(fog)) >> 8u;
        const unsigned destination = (unsigned(fogColor[channel]) * (255u - unsigned(fog))) >> 8u;
        result[channel] = static_cast<std::uint8_t>(source + destination);
    }
    return result;
}

inline int storylandPs2GsShift7(int value) {
    return value >= 0 ? value / 128 : -((-value + 127) / 128);
}

inline std::uint8_t storylandPs2GsBlendChannel(
    std::uint8_t a, std::uint8_t b, std::uint8_t coefficient, std::uint8_t d) {
    const int product = (int(a) - int(b)) * int(coefficient);
    return storylandPs2GsClamp(storylandPs2GsShift7(product) + int(d));
}
