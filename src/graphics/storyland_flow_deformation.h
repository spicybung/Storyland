#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>

struct StorylandFlow3 {
    float x=0, y=0, z=0;
};

// Analytic curl of a smooth three-component vector potential.
// Every component is continuous and the field is divergence-free.
inline StorylandFlow3 storylandCurlNoise(float x,float y,float z,float seconds,float phase=0) {
    const float t=seconds;
    const float ay=1.4f*y+.8f*t+phase;
    const float az=1.1f*z-.9f*t+phase*.7f;
    const float bz=1.25f*z+.67f*t+phase*.2f;
    const float bx=.95f*x+.43f*t+phase;
    const float cx=1.1f*x-.73f*t+phase*.4f;
    const float cy=1.35f*y+.51f*t+phase*.6f;
    return {
        (-1.35f*std::sin(cx)*std::sin(cy)-1.25f*std::cos(bz)*std::cos(bx))*.35f,
        (-1.1f*std::sin(ay)*std::sin(az)-1.1f*std::cos(cx)*std::cos(cy))*.35f,
        (-.95f*std::sin(bz)*std::sin(bx)-1.4f*std::cos(ay)*std::cos(az))*.35f
    };
}

inline StorylandFlow3 storylandFireCurlOffset(float x,float y,float z,float height,float seconds,size_t seed) {
    const float rise=std::clamp(height,0.0f,1.0f);
    const float phase=float(seed%997)*.117f;
    const auto flow=storylandCurlNoise(x*.71f,y*.71f,z*.64f+rise*.7f,seconds*1.2f,phase);
    const float envelope=rise*rise*(.17f+.35f*rise);
    return {flow.x*envelope,flow.y*envelope,flow.z*envelope*.22f};
}
