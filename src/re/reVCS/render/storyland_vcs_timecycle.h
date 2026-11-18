#pragma once

#include "src/storyland/storyland_clock.h"
#include <algorithm>
#include <array>
#include <cstdint>

struct StorylandVcsTimecycleBlend {
    unsigned hour=0,nextHour=1;
    float time=0;
    std::array<float,4> weights{1,0,0,0};
};

inline StorylandVcsTimecycleBlend storylandVcsTimecycleBlend(float hour,float weather) {
    StorylandVcsTimecycleBlend result;
    const auto clock=storylandClockParts(hour);
    const float interpolation=std::isfinite(weather)?std::clamp(weather,0.0f,1.0f):0.0f;
    result.hour=clock.hour;result.nextHour=(clock.hour+1u)%24u;
    result.time=(float(clock.minute)+float(clock.second)*(1.0f/60.0f))*(1.0f/60.0f);
    result.weights={(1.0f-result.time)*(1.0f-interpolation),(1.0f-result.time)*interpolation,
        result.time*(1.0f-interpolation),result.time*interpolation};
    return result;
}

inline uint8_t storylandVcsTimecycleByte(const std::array<uint8_t,4>& corners,
    const StorylandVcsTimecycleBlend& blend) {
    // Native multiply/add rounding and order precede the integer sky-color conversion.
    const volatile float products[]={float(corners[0])*blend.weights[0],
        float(corners[1])*blend.weights[1],float(corners[2])*blend.weights[2],
        float(corners[3])*blend.weights[3]};
    volatile float value=products[0]+products[2];
    value=value+products[1];
    value=value+products[3];
    return uint8_t(std::clamp(int(value),0,255));
}
