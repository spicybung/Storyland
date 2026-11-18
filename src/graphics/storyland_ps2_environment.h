#pragma once

#include "src/graphics/storyland_ps2_lighting.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

inline constexpr int StorylandPs2EnvironmentSize = 128;
inline constexpr int StorylandPs2FrameWidth = 640;
inline constexpr int StorylandPs2FrameHeight = 448;
inline constexpr std::uint64_t StorylandPs2EnvironmentDimx = 0x0716524334256170ULL;
inline constexpr std::uint64_t StorylandPs2EnvironmentCoronaBlend = 0xFF00000068ULL;
using StorylandPs2EnvironmentPixels = std::array<std::uint8_t, 128 * 128 * 4>;

struct StorylandPs2EnvironmentImageView {
    const std::uint8_t* pixels = nullptr;
    int width = 0;
    int height = 0;
    std::size_t stride = 0;
    bool bottomUp = false;
    bool bgra = false;
    int storageWidth = 0;
    int storageHeight = 0;
};

struct StorylandPs2EnvironmentRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

struct StorylandPs2EnvironmentCapturePlan {
    std::array<StorylandPs2EnvironmentRect, 4> corona{};
    int headingShift = 64;
};

struct StorylandPs2EnvironmentState {
    bool dither = false;
    std::uint64_t dimx = StorylandPs2EnvironmentDimx;
};

inline float storylandPs2EnvironmentHeading(const std::array<float, 3>& nativeCameraAxis) {
    if (!std::isfinite(nativeCameraAxis[0]) || !std::isfinite(nativeCameraAxis[1]) ||
        (nativeCameraAxis[0] == 0.0f && nativeCameraAxis[1] == 0.0f)) return 0.0f;
    return std::atan2(-nativeCameraAxis[0], nativeCameraAxis[1]);
}

inline StorylandPs2EnvironmentCapturePlan storylandPs2EnvironmentPlanForShift(int headingShift) {
    StorylandPs2EnvironmentCapturePlan result;
    headingShift = std::clamp(headingShift, 0, StorylandPs2EnvironmentSize);
    result.headingShift = headingShift;
    const std::array<std::array<std::uint32_t, 3>, 2> source{{
        {{0x3E4CCCCEu, 0x00000000u, 0x3F333333u}},
        {{0x3F666666u, 0x3DCCCCCDu, 0x3E99999Au}}
    }};
    std::size_t index = 0;
    for (const auto& quad : source) {
        const int left = static_cast<int>(storylandPs2FloatConstant(quad[0]) * 128.0f);
        const int top = static_cast<int>(storylandPs2FloatConstant(quad[1]) * 128.0f);
        const int size = static_cast<int>(storylandPs2FloatConstant(quad[2]) * 128.0f);
        for (int offset : {headingShift, headingShift - StorylandPs2EnvironmentSize})
            result.corona[index++] = {left + offset, top, left + offset + size, top + size};
    }
    return result;
}

inline StorylandPs2EnvironmentCapturePlan storylandPs2EnvironmentPlan(float headingRadians) {
    if (!std::isfinite(headingRadians)) headingRadians = 0.0f;
    const float fraction = headingRadians * storylandPs2FloatConstant(0x3E22F983u);
    const float shifted = fraction + 0.5f;
    const float coordinate = shifted * 128.0f;
    const int shift = coordinate > -2147483648.0f && coordinate < 2147483648.0f
        ? static_cast<int>(coordinate) : 64;
    return storylandPs2EnvironmentPlanForShift(shift);
}

inline int storylandPs2EnvironmentDither(const StorylandPs2EnvironmentState& state, int x, int y) {
    if (!state.dither) return 0;
    const unsigned index = (static_cast<unsigned>(y) & 3u) * 4u + (static_cast<unsigned>(x) & 3u);
    const unsigned entry = static_cast<unsigned>((state.dimx >> (index * 4u)) & 7u);
    return entry & 4u ? static_cast<int>(entry) - 8 : static_cast<int>(entry);
}

inline std::uint16_t storylandPs2Environment5551(
    const std::array<int, 4>& rgba, int x, int y,
    const StorylandPs2EnvironmentState& state = {}) {
    const int dither = storylandPs2EnvironmentDither(state, x, y);
    std::uint16_t result = static_cast<std::uint16_t>((std::clamp(rgba[3], 0, 255) & 128) << 8);
    for (unsigned channel = 0; channel < 3; ++channel) {
        const int color = std::clamp(rgba[channel] + dither, 0, 255);
        result |= static_cast<std::uint16_t>((color >> 3) << (channel * 5u));
    }
    return result;
}

inline std::array<int, 4> storylandPs2EnvironmentExpand5551(std::uint16_t pixel) {
    return {static_cast<int>((pixel & 31u) << 3u), static_cast<int>(((pixel >> 5u) & 31u) << 3u),
        static_cast<int>(((pixel >> 10u) & 31u) << 3u), static_cast<int>((pixel >> 8u) & 128u)};
}

inline bool storylandPs2EnvironmentImageValid(const StorylandPs2EnvironmentImageView& image) {
    const int storageWidth = image.storageWidth ? image.storageWidth : image.width;
    const int storageHeight = image.storageHeight ? image.storageHeight : image.height;
    if ((image.storageWidth == 0) != (image.storageHeight == 0) || !image.pixels ||
        image.width <= 0 || image.height <= 0 || storageWidth < image.width ||
        storageHeight < image.height || storageWidth > 16384 || storageHeight > 16384) return false;
    const std::size_t row = static_cast<std::size_t>(storageWidth) * 4;
    const std::size_t stride = image.stride ? image.stride : row;
    return stride >= row && stride <= std::numeric_limits<std::size_t>::max() /
        static_cast<std::size_t>(storageHeight);
}

inline int storylandPs2EnvironmentWrap(int coordinate, int extent) {
    const int remainder = coordinate % extent;
    return remainder < 0 ? remainder + extent : remainder;
}

inline std::array<int, 4> storylandPs2EnvironmentPixel(
    const StorylandPs2EnvironmentImageView& image, int x, int y, bool repeat) {
    const int width = image.storageWidth ? image.storageWidth : image.width;
    const int height = image.storageHeight ? image.storageHeight : image.height;
    x = repeat ? storylandPs2EnvironmentWrap(x, width) : std::clamp(x, 0, width - 1);
    y = repeat ? storylandPs2EnvironmentWrap(y, height) : std::clamp(y, 0, height - 1);
    if (image.bottomUp) y = height - 1 - y;
    const std::size_t stride = image.stride ? image.stride : static_cast<std::size_t>(width) * 4;
    const auto* pixel = image.pixels + static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) * 4;
    return {pixel[image.bgra ? 2 : 0], pixel[1], pixel[image.bgra ? 0 : 2], pixel[3]};
}

// Finite four-bit filtering follows the GS coordinate domain. Exact GS filter
// rounding and non-visible framebuffer VRAM contents are separate proof limits.
inline std::array<int, 4> storylandPs2EnvironmentSample(
    const StorylandPs2EnvironmentImageView& image, double u, double v, bool repeat) {
    const double sampleX = u - 0.5;
    const double sampleY = v - 0.5;
    const int left = static_cast<int>(std::floor(sampleX));
    const int top = static_cast<int>(std::floor(sampleY));
    const unsigned horizontal = static_cast<unsigned>(std::floor((sampleX - left) * 16.0));
    const unsigned vertical = static_cast<unsigned>(std::floor((sampleY - top) * 16.0));
    const auto upperLeft = storylandPs2EnvironmentPixel(image, left, top, repeat);
    const auto upperRight = storylandPs2EnvironmentPixel(image, left + 1, top, repeat);
    const auto lowerLeft = storylandPs2EnvironmentPixel(image, left, top + 1, repeat);
    const auto lowerRight = storylandPs2EnvironmentPixel(image, left + 1, top + 1, repeat);
    std::array<int, 4> result{};
    for (std::size_t channel = 0; channel < result.size(); ++channel) {
        const unsigned sum = (16u - horizontal) * (16u - vertical) * upperLeft[channel] +
            horizontal * (16u - vertical) * upperRight[channel] +
            (16u - horizontal) * vertical * lowerLeft[channel] + horizontal * vertical * lowerRight[channel];
        result[channel] = static_cast<int>(sum >> 8u);
    }
    return result;
}

inline std::array<int, 4> storylandPs2EnvironmentCoronaColor(
    const std::array<int, 4>& texel, std::uint16_t destination) {
    const auto previous = storylandPs2EnvironmentExpand5551(destination);
    std::array<int, 4> result{};
    for (std::size_t channel = 0; channel < result.size(); ++channel) {
        const int source = std::clamp((texel[channel] * 127) >> 7, 0, 255);
        result[channel] = channel < 3 ? ((source * 255) >> 7) + previous[channel] : source;
    }
    return result;
}

inline bool storylandPs2ComposeEnvironment(
    const StorylandPs2EnvironmentImageView& framebuffer,
    const StorylandPs2EnvironmentImageView& coronastar,
    float headingRadians, StorylandPs2EnvironmentPixels& output, std::string& error,
    const StorylandPs2EnvironmentState& state = {}) {
    if (!storylandPs2EnvironmentImageValid(framebuffer) || !storylandPs2EnvironmentImageValid(coronastar) ||
        (coronastar.width & (coronastar.width - 1)) || (coronastar.height & (coronastar.height - 1))) {
        error = "The PS2 environment requires a visible framebuffer and the original power-of-two coronastar raster.";
        return false;
    }
    const auto plan = storylandPs2EnvironmentPlan(headingRadians);
    std::array<std::uint16_t, 128 * 128> target{};
    const bool completeFrameStorage = framebuffer.storageWidth > 0 && framebuffer.storageHeight > 0;
    for (int y = 0; y < StorylandPs2EnvironmentSize; ++y) {
        for (int x = 0; x < StorylandPs2EnvironmentSize; ++x) {
            const double u = static_cast<double>(x) * framebuffer.width / StorylandPs2EnvironmentSize;
            const double v = static_cast<double>(y) * framebuffer.height / StorylandPs2EnvironmentSize;
            auto color = storylandPs2EnvironmentSample(framebuffer, u, v, completeFrameStorage);
            color[3] = 128;
            target[static_cast<std::size_t>(y) * StorylandPs2EnvironmentSize + x] =
                storylandPs2Environment5551(color, x, y, state);
        }
    }
    for (const auto& quad : plan.corona) {
        const int width = quad.right - quad.left;
        const int height = quad.bottom - quad.top;
        for (int y = std::max(0, quad.top); y < std::min(StorylandPs2EnvironmentSize, quad.bottom); ++y) {
            for (int x = std::max(0, quad.left); x < std::min(StorylandPs2EnvironmentSize, quad.right); ++x) {
                const double u = static_cast<double>(x - quad.left) * coronastar.width / width;
                const double v = static_cast<double>(y - quad.top) * coronastar.height / height;
                const auto texel = storylandPs2EnvironmentSample(coronastar, u, v, true);
                const std::size_t index = static_cast<std::size_t>(y) * StorylandPs2EnvironmentSize + x;
                target[index] = storylandPs2Environment5551(
                    storylandPs2EnvironmentCoronaColor(texel, target[index]), x, y, state);
            }
        }
    }
    for (std::size_t index = 0; index < target.size(); ++index) {
        const auto color = storylandPs2EnvironmentExpand5551(target[index]);
        for (std::size_t channel = 0; channel < color.size(); ++channel)
            output[index * 4 + channel] = static_cast<std::uint8_t>(color[channel]);
    }
    error.clear();
    return true;
}
