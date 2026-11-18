#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

inline bool storylandVcsVehicleLampsRequired(unsigned hour,unsigned minute,uint8_t phase,
    float fogginess,float wetRoads,bool motorcycle) {
    if(hour>=21||hour<(motorcycle?7u:6u))return true;
    const unsigned transition=phase&63u;
    if(hour==20&&transition<minute)return true;
    if(hour==(motorcycle?7u:6u)&&minute<transition)return true;
    const float weatherThreshold=float(phase)*.00002f;
    return weatherThreshold<fogginess||weatherThreshold<wetRoads;
}

template<class Runtime,class Vehicle>
inline bool storylandVehicleLampsRequired(const Runtime& runtime,const Vehicle& vehicle) {
    if(!vehicle.nativeDefinition.vcsModelLayout)
        return runtime.clockHour<6.5f||runtime.clockHour>=19.0f;
    const float clock=std::isfinite(runtime.clockHour)?std::clamp(runtime.clockHour,0.0f,23.99999f):12.0f;
    const unsigned hour=unsigned(clock);
    unsigned minute=std::min(59u,unsigned((clock-hour)*60.0f+.0001f));
    float wetRoads=0,fogginess=0;
    if(runtime.missionEnvironment) {
        const auto& environment=*runtime.missionEnvironment;
        wetRoads=environment.weatherState.rain;
        const auto& weather=environment.weatherState;
        const float blend=std::clamp(weather.interpolation,0.0f,1.0f);
        fogginess=(weather.previous==3?1.0f-blend:0.0f)+(weather.current==3?blend:0.0f);
    }
    return storylandVcsVehicleLampsRequired(hour,minute,uint8_t(vehicle.routeSeed),fogginess,wetRoads,
        vehicle.nativeDefinition.twoWheels());
}
