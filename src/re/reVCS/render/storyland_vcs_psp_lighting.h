#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

inline float storylandPspFloat24(float value) {
    uint32_t bits=0;
    std::memcpy(&bits,&value,sizeof(bits));
    bits&=0xFFFFFF00u;
    std::memcpy(&value,&bits,sizeof(value));
    return value;
}

inline std::array<float,3> storylandPspExtraLightDirection(
    std::array<float,3> direction={0.7f,0.7f,1.0f}) {
    const float length=std::sqrt(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
    if(!std::isfinite(length)||length<=0.0f)return {};
    for(float& component:direction)component=storylandPspFloat24(component/length);
    return direction;
}

inline float storylandPspEarlyEnvironmentStrength(float hour) {
    if(!std::isfinite(hour))return 1.0f;
    const float normalizedHour=std::fmod(std::fmod(hour,24.0f)+24.0f,24.0f);
    const float strength=1.0f-std::abs(std::floor(normalizedHour)/24.0f-0.5f);
    return strength*strength;
}

inline std::array<float,3> storylandPspSunDirection(float hour) {
    constexpr double pi=3.14159265358979323846;
    double minutes=std::fmod(double(hour)*60.0,1440.0);
    if(minutes<0.0)minutes+=1440.0;
    if(minutes>=360.0&&minutes<720.0)
        minutes=360.0+360.0*(1.0-std::cos((minutes-360.0)*0.25*pi/180.0));
    else if(minutes>=720.0&&minutes<1080.0)
        minutes=720.0+360.0*std::sin((minutes-720.0)*0.25*pi/180.0);
    const double angle=minutes*2.0*pi/1440.0;
    const double x=std::sin(angle),y=1.0,z=0.4-std::cos(angle);
    const double length=std::sqrt(x*x+y*y+z*z);
    return {storylandPspFloat24(float(x/length)),storylandPspFloat24(float(y/length)),
        storylandPspFloat24(float(z/length))};
}

struct StorylandPspFogParameters {
    float farClip=0.0f;
    float slope=0.0f;
};

inline StorylandPspFogParameters storylandPspFogParameters(float farClip,float fogStart) {
    return {storylandPspFloat24(farClip),
        storylandPspFloat24(1.0f/std::max(farClip-fogStart,0.000001f))};
}

struct StorylandPspLightingColors {
    std::array<float,3> worldEmissive{};
    std::array<float,3> objectAmbient{};
};

inline StorylandPspLightingColors storylandPspLightingColors(
    const std::array<float,3>& ambientBlur,const std::array<float,3>& objectAmbientBlur,
    unsigned brightness=288u) {
    StorylandPspLightingColors result;
    const float brightnessIncrease=std::max(float(brightness)/256.0f-1.0f,0.0f)*0.6f;
    for(size_t channel=0;channel<3;++channel) {
        result.worldEmissive[channel]=std::clamp(ambientBlur[channel]*(1.0f+3.0f*brightnessIncrease),0.0f,1.0f);
        result.objectAmbient[channel]=std::clamp(objectAmbientBlur[channel]*(1.0f+2.0f*brightnessIncrease),0.0f,1.0f);
    }
    return result;
}

inline std::array<float,3> storylandPspFogColor(const std::array<float,3>& skyTop,
    const std::array<float,3>& skyBottom) {
    std::array<float,3> result{};
    for(size_t channel=0;channel<result.size();++channel) {
        const unsigned top=unsigned(std::floor(std::clamp(skyTop[channel],0.0f,1.0f)*255.0f+0.0001f));
        const unsigned bottom=unsigned(std::floor(std::clamp(skyBottom[channel],0.0f,1.0f)*255.0f+0.0001f));
        result[channel]=float((top+2u*bottom)/3u)/255.0f;
    }
    return result;
}
