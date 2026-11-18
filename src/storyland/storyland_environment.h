#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// Stories CPhysical applies 0.008 native velocity units per 50 Hz timestep.
// Play Test stores velocity in world units/second, hence the second factor of 50.
inline constexpr float StorylandNativePhysicsRate = 50.0f;
inline constexpr float StorylandNativeGravity = 0.008f;
inline constexpr float StorylandGravityPerSecond =
    StorylandNativeGravity * StorylandNativePhysicsRate * StorylandNativePhysicsRate;

inline const char* storylandNativeWeatherName(unsigned index, bool vcs) {
    static const char* common[] = {"Sunny", "Cloudy", "Rainy", "Foggy", "Extra sunny",
        "Hurricane", "Extra colours"};
    return index < 7 ? common[index] : (vcs ? "Ultra sunny" : "Snow");
}
inline float storylandWeatherWind(unsigned weather) {
    // CWeather's weather-dependent wind scale; wind is visual/particle motion,
    // not an additional force on a grounded pedestrian or a heavy road vehicle.
    constexpr std::array<float,8> strength{{0.25f,0.7f,1.0f,0.0f,0.0f,2.0f,0.0f,0.0f}};
    return strength[std::min(weather,7u)];
}

inline std::array<float,3> storylandWeatherWindVector(unsigned previous,unsigned current,
    float interpolation,float directionDegrees) {
    if(!std::isfinite(interpolation))interpolation=0.0f;
    if(!std::isfinite(directionDegrees))directionDegrees=45.0f;
    const float blend=std::clamp(interpolation,0.0f,1.0f);
    const float first=storylandWeatherWind(previous);
    const float strength=first+(storylandWeatherWind(current)-first)*blend;
    const float angle=directionDegrees*.01745329251994329577f;
    return {std::cos(angle)*strength,std::sin(angle)*strength,0.0f};
}
