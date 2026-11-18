#pragma once

#include <algorithm>
#include <cmath>

struct StorylandGraphicsQualitySettings {
    bool fxaa=false;
    bool anisotropic16x=false;
};

inline float storylandRequestedAnisotropy(const StorylandGraphicsQualitySettings& settings,
    bool targetProfile,float maximum) {
    if(!targetProfile||!settings.anisotropic16x)return 1.0f;
    if(!std::isfinite(maximum)||maximum<1.0f)return 1.0f;
    return std::min(16.0f,maximum);
}
