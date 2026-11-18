#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Wobble follows custom reLCS v6.9.41 core/Camera.cpp. VCS bikes
// enter it at the retail high-speed threshold (0x37DF48), not a low gear's
// downshift speed. Handling speeds use native 50 Hz units.
namespace StorylandStoriesSpeedMotion {

struct Presentation {
    float lateral=0,vertical=0,aimWobble=0;
    uint8_t blur=0;
};

inline float target(float forwardSpeed,float cruiseSpeed,float topGearDownshift,bool motorcycle) {
    if(!std::isfinite(forwardSpeed)||!std::isfinite(cruiseSpeed)||cruiseSpeed<=0)
        return 0;
    if(motorcycle) {
        const float effect=std::clamp((forwardSpeed-.65f)/.35f,0.0f,1.0f);
        return effect*effect*(3.0f-2.0f*effect);
    }
    const float cruise=std::max(cruiseSpeed,.10f);
    const float reachableStart=cruise*.64f;
    const float reachableFull=cruise*.86f;
    const float start=std::isfinite(topGearDownshift)&&topGearDownshift>0?
        std::min(topGearDownshift,reachableStart):reachableStart;
    const float full=std::max(reachableFull,start+.05f);
    const float effect=std::clamp((std::max(forwardSpeed,0.0f)-start)/(full-start),0.0f,1.0f);
    return effect*effect*(3.0f-2.0f*effect);
}

inline float advance(float intensity,float wanted,float seconds) {
    if(!std::isfinite(intensity))intensity=0;
    if(!std::isfinite(wanted))wanted=0;
    intensity=std::clamp(intensity,0.0f,1.0f);
    wanted=std::clamp(wanted,0.0f,1.0f);
    if(!std::isfinite(seconds)||seconds<=0)return intensity;
    const float retention=wanted>intensity?.82f:.93f;
    const float blend=1.0f-std::pow(retention,std::min(seconds,.1f)*50.0f);
    intensity+=(wanted-intensity)*blend;
    return intensity<.0001f?0.0f:intensity;
}

inline Presentation sample(float intensity,bool motorcycle,uint32_t milliseconds) {
    Presentation result;
    if(!std::isfinite(intensity)||intensity<=0)return result;
    const float effect=std::min(intensity,1.0f);
    const double time=double(milliseconds)*.001;
    const float strength=effect*effect*(motorcycle?1.45f:1.0f);
    result.lateral=float(std::sin(time*23.0)+.45*std::sin(time*41.0+.7))*.010f*strength;
    result.vertical=float(std::sin(time*29.0+1.9)+.35*std::sin(time*53.0))*.007f*strength;
    result.aimWobble=float(std::sin(time*17.0+.3)+.30*std::sin(time*37.0+2.2))*.008f*strength;
    result.blur=uint8_t(32u+unsigned(effect*96.0f));
    return result;
}

}
