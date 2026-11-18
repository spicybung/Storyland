#pragma once

#include <array>

enum class StorylandDirectXDepthFormat { Depth24Stencil8, Depth24, Depth16 };

// Preserve stencil whenever supported, but do not fall back to 16-bit depth
// if the adapter supports a 24-bit format without stencil instead.
inline constexpr std::array<StorylandDirectXDepthFormat,3> storylandDirectXDepthFormats = {
    StorylandDirectXDepthFormat::Depth24Stencil8,
    StorylandDirectXDepthFormat::Depth24,
    StorylandDirectXDepthFormat::Depth16
};

inline constexpr bool storylandDirectXDepthHasStencil(StorylandDirectXDepthFormat format) {
    return format == StorylandDirectXDepthFormat::Depth24Stencil8;
}

// D3D9 uses a normalized depth bias, while glPolygonOffset's units are
// measured in representable depth steps. Storyland's two selected D3D9
// depth formats are D24S8, D24X8 and the D16 fallback.
inline float storylandDirectXDepthBias(float units,bool sixteenBit) {
    return units * (sixteenBit ? (1.0f / 65536.0f) : (1.0f / 16777216.0f));
}
