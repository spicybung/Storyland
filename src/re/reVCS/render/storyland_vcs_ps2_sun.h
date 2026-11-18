#pragma once
#include <array>
#include <cmath>

inline std::array<float,3> storylandVcsPs2SunDirection(float hour) {
    if(!std::isfinite(hour))return {0,1,0};
    hour=std::fmod(hour,24.0f);
    if(hour<0)hour+=24.0f;
    float minutes=hour*60.0f;
    constexpr float degreesToRadians=.01745329424738884f;
    constexpr float minutesToRadians=.00436332356184721f;
    if(minutes>=360.0f&&minutes<720.0f) {
        const float angle=(minutes-360.0f)*.25f*degreesToRadians;
        minutes=360.0f+(1.0f-std::cos(angle))*360.0f;
    } else if(minutes>=720.0f&&minutes<1080.0f) {
        const float angle=(minutes-720.0f)*.25f*degreesToRadians;
        minutes=720.0f+std::sin(angle)*360.0f;
    }
    const float angle=minutes*minutesToRadians;
    const float x=std::sin(angle),z=.4f-std::cos(angle);
    const float inverseLength=1.0f/std::sqrt(x*x+1.0f+z*z);
    return {x*inverseLength,inverseLength,z*inverseLength};
}
