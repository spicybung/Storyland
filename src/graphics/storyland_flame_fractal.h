#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

inline float storylandFlameFractalEdge(float rise, float seconds, uint32_t seed) {
    const float frequency=float(seed % 101u)*0.039f;
    const float slow=std::sin(rise*7.0f+seconds*2.1f+frequency);
    const float mid=std::sin(rise*16.0f-seconds*3.7f+frequency*1.77f);
    const float fine=std::sin(rise*33.0f+seconds*6.6f+frequency*3.12f);
    const float fractal=slow*.5714286f+mid*.2857143f+fine*.1428571f;
    return std::clamp(fractal,-1.0f,1.0f)*0.15f;
}
