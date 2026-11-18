#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

struct StorylandVcsMoonBrightness {
    uint8_t raw=0;
    uint8_t tint=0;
};

inline StorylandVcsMoonBrightness storylandVcsMoonBrightness(float hour,
    unsigned oldWeather,unsigned newWeather,float interpolation) {
    if(!std::isfinite(hour)||!std::isfinite(interpolation))return {};
    hour=std::fmod(hour,24.0f);
    if(hour<0)hour+=24;
    // Recover CClock's integer seconds without float-hour error at exact minute boundaries.
    const unsigned seconds=unsigned(std::lround(double(hour)*3600.0))%86400u;
    const float minutes=float(seconds/60u)+float(seconds%60u)*(1.0f/60.0f);
    const int distance=int(std::fabs(minutes-180.0f));
    if(distance>=180)return {};
    const auto clear=[](unsigned weather) {return weather==0||weather==4||weather==7;};
    const float blend=std::clamp(interpolation,0.0f,1.0f);
    const float cloud=(clear(oldWeather)?0.0f:1.0f-blend)+(clear(newWeather)?0.0f:blend);
    // VCS tests the old weather in both fog terms, including transitions out of fog.
    const float fog=oldWeather==3?1.0f:0.0f;
    const unsigned brightness=unsigned((1.0f-std::max(fog,cloud))*float(180-distance));
    return {uint8_t(brightness),uint8_t((brightness*255u)>>8)};
}

inline float storylandVcsMoonFovScale(float fieldOfView) {
    if(!std::isfinite(fieldOfView)||fieldOfView<=0||fieldOfView>=180)return 0;
    return .7002065181732178f/std::tan(fieldOfView*.00872664712369442f);
}

inline std::array<float,2> storylandVcsMoonHalfSizeWithScale(float depth,float aspect,
    float lensScale,unsigned moonSize=3) {
    if(!std::isfinite(depth)||depth<=0||!std::isfinite(aspect)||aspect<=0||
        !std::isfinite(lensScale)||lensScale<=0)return {};
    const float scale=float(2u*std::min(moonSize,255u)+4u)*lensScale;
    const float vertical=448.0f/depth;
    return {vertical*(640.0f/448.0f)/aspect*scale,vertical*scale};
}

inline std::array<float,2> storylandVcsMoonHalfSize(float depth,float aspect,
    float fieldOfView,unsigned moonSize=3) {
    return storylandVcsMoonHalfSizeWithScale(depth,aspect,storylandVcsMoonFovScale(fieldOfView),moonSize);
}

struct StorylandVcsMoonSprite {
    float x=0,y=0,halfWidth=0,halfHeight=0;
    bool visible=false;
};

inline StorylandVcsMoonSprite storylandVcsMoonSprite(const float* view,
    const float* projection,float nativeFieldOfView=0.0f,unsigned moonSize=3) {
    StorylandVcsMoonSprite result;
    if(!view||!projection)return result;
    for(size_t index=0;index<16;++index)
        if(!std::isfinite(view[index])||!std::isfinite(projection[index]))return result;
    if(std::fabs(projection[0])<1e-6f||std::fabs(projection[5])<1e-6f||
        std::fabs(projection[11]+1.0f)>.001f||std::fabs(projection[15])>.001f)return result;

    // Clouds::Render adds this fixed offset to the camera, independently of the sun.
    const float x=-100.0f*view[4]+15.0f*view[8];
    const float y=-100.0f*view[5]+15.0f*view[9];
    const float depth=100.0f*view[6]-15.0f*view[10];
    const float denominator=projection[10]-1.0f;
    if(std::fabs(denominator)<1e-8f)return result;
    const float nearClip=projection[14]/denominator;
    if(nearClip<=0||depth<nearClip||depth<=0)return result;

    const float lensScale=nativeFieldOfView>0?storylandVcsMoonFovScale(nativeFieldOfView):
        .7002065181732178f*std::fabs(projection[0]);
    if(!std::isfinite(lensScale)||lensScale<=0)return result;
    const auto halfSize=storylandVcsMoonHalfSizeWithScale(depth,
        std::fabs(projection[5]/projection[0]),lensScale,moonSize);
    result.x=projection[0]*x/depth-projection[8];
    result.y=projection[5]*y/depth-projection[9];
    result.halfHeight=halfSize[1]/224.0f;
    result.halfWidth=halfSize[0]/320.0f;
    result.visible=std::fabs(result.x)<1.0f+result.halfWidth&&
        std::fabs(result.y)<1.0f+result.halfHeight;
    return result;
}
