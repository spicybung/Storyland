#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Native script-corona presentation shared by the frame and retained pools.
inline const char* storylandCoronaTextureName(int type) {
    static const char* names[]={"coronastar","corona","coronamoon","coronareflect","coronaheadlightline",
        "coronahex","coronacircle","coronaringa","streek"};
    return type>=0&&type<9?names[type]:nullptr;
}
inline float storylandCoronaDistanceFade(float depth,float range=450) {
    if(!std::isfinite(depth)||!std::isfinite(range)||range<=0||depth<=0||depth>=range)return 0;
    return std::clamp((range-depth)/(range*.5f),0.0f,1.0f);
}
struct StorylandCoronaProjection {
    std::array<float,3> center{};
    float cameraX=0,cameraY=0,depth=0,biasedDepth=0,depthScale=1;
    bool onScreen=false;
};
inline bool storylandProjectCorona(const std::array<float,3>& position,const float* view,
    const float* projection,StorylandCoronaProjection& result,float range=450,float pullAway=1.5f) {
    result={};
    if(!view||!projection||!std::isfinite(pullAway))return false;
    for(float value:position)if(!std::isfinite(value))return false;
    for(size_t i=0;i<16;++i)if(!std::isfinite(view[i])||!std::isfinite(projection[i]))return false;
    const float nearDenominator=projection[10]-1.0f,farDenominator=projection[10]+1.0f;
    if(std::fabs(nearDenominator)<1.0e-8f||std::fabs(farDenominator)<1.0e-8f)return false;
    const float nearClip=projection[14]/nearDenominator,farClip=projection[14]/farDenominator;
    result.cameraX=view[0]*position[0]+view[4]*position[1]+view[8]*position[2]+view[12];
    result.cameraY=view[1]*position[0]+view[5]*position[1]+view[9]*position[2]+view[13];
    result.depth=-(view[2]*position[0]+view[6]*position[1]+view[10]*position[2]+view[14]);
    if(!std::isfinite(range)||range<=0||nearClip<=0||farClip<=nearClip||result.depth<nearClip||result.depth>=std::min(farClip,range))return false;
    result.biasedDepth=result.depth-pullAway;
    if(result.biasedDepth<=nearClip)return false;
    result.depthScale=result.biasedDepth/result.depth;
    // Native RenderOneXLUSprite biases only depth after calculating screen X/Y and size.
    // Scale camera-plane coordinates with depth so the 3D GL quad has that same projection.
    for(size_t axis=0;axis<3;++axis)result.center[axis]=position[axis]+view[axis*4+2]*pullAway-
        (view[axis*4]*result.cameraX+view[axis*4+1]*result.cameraY)*(1-result.depthScale);
    const float clipX=projection[0]*result.cameraX+projection[4]*result.cameraY-projection[8]*result.depth+projection[12];
    const float clipY=projection[1]*result.cameraX+projection[5]*result.cameraY-projection[9]*result.depth+projection[13];
    const float clipW=projection[3]*result.cameraX+projection[7]*result.cameraY-projection[11]*result.depth+projection[15];
    result.onScreen=clipW>0&&std::fabs(clipX)<=clipW&&std::fabs(clipY)<=clipW;
    return std::all_of(result.center.begin(),result.center.end(),[](float value){return std::isfinite(value);});
}
// The registration range is on the map plane. Projection also checks lens depth.
inline bool storylandCoronaInRegistrationRange(const std::array<float,3>& position,const float* view,float range=450) {
    const double cameraX=-(double(view[0])*view[12]+double(view[1])*view[13]+double(view[2])*view[14]);
    const double cameraY=-(double(view[4])*view[12]+double(view[5])*view[13]+double(view[6])*view[14]);
    return std::isfinite(range)&&range>0&&std::hypot(double(position[0])-cameraX,double(position[1])-cameraY)<=range;
}
struct StorylandCoronaFlare {
    float position,size;
    std::array<int,3> color;
    int texture;
    int alpha=200;
};
inline uint8_t storylandCoronaFadedAlpha(uint8_t alpha,float depth,float range) {
    return uint8_t(std::clamp(float(alpha)*storylandCoronaDistanceFade(depth,range),0.0f,255.0f));
}
inline float storylandCoronaFogScale(float depth,float fogginess) {
    return 1.0f+std::clamp(fogginess,0.0f,1.0f)*std::min(depth,40.0f)*.025f;
}
inline std::array<uint8_t,3> storylandCoronaFogColor(const std::array<uint8_t,3>& rgb,float scale) {
    std::array<uint8_t,3> result{};
    for(size_t channel=0;channel<3;++channel)
        result[channel]=uint8_t(std::clamp(float(rgb[channel])/scale,0.0f,255.0f));
    return result;
}
inline float storylandCoronaNearFade(float depth,bool lowNearClip=true) {
    const float start=lowNearClip?.2f:1.3f;
    return std::clamp(depth-start,0.0f,1.0f);
}
inline std::array<float,3> storylandCoronaSpriteColor(const std::array<uint8_t,3>& rgb,
    uint8_t alpha,float depth,bool nearFade=true) {
    const float nearIntensity=storylandCoronaNearFade(depth);
    const unsigned fade=nearFade&&nearIntensity<1?unsigned(nearIntensity*255.0f):256u;
    const unsigned intensity=(unsigned(alpha)*fade)>>8;
    std::array<float,3> result{};
    for(size_t channel=0;channel<3;++channel) {
        const unsigned color=(unsigned(rgb[channel])*fade)>>8;
        result[channel]=float((color*intensity)>>8)/255.0f;
    }
    return result;
}
inline std::array<float,3> storylandCoronaFlareColor(const std::array<uint8_t,3>& rgb,
    uint8_t alpha,const StorylandCoronaFlare& flare) {
    const unsigned intensity=(unsigned(alpha)*unsigned(flare.alpha))>>8;
    std::array<float,3> result{};
    for(size_t channel=0;channel<3;++channel) {
        const unsigned color=(unsigned(rgb[channel])*unsigned(flare.color[channel]))>>8;
        result[channel]=float((color*intensity)>>8)/255.0f;
    }
    return result;
}
inline const std::array<StorylandCoronaFlare,9>& storylandCoronaFlares(int type,size_t& count,bool vcs=true) {
    static const std::array<StorylandCoronaFlare,9> sun{{
        {-.5f,15,{50,50,0},7},{-1,10,{50,20,0},7},{-1.5f,15,{50,0,0},7},
        {-2.5f,25,{50,0,0},7},{.5f,12.5f,{40,40,25},7},{.05f,20,{30,22,9},7},{1.3f,7.5f,{50,30,9},7}
    }};
    static const std::array<StorylandCoronaFlare,9> legacySun{{
        {-.5f,15,{50,50,0},5},{-1,10,{50,20,0},6},{-1.5f,15,{50,0,0},7},
        {-2.5f,25,{50,0,0},5},{.5f,12.5f,{40,40,25},5},{.05f,20,{30,22,9},6},{1.3f,7.5f,{50,30,9},7}
    }};
    static const std::array<StorylandCoronaFlare,9> headlights{{
        {-.5f,15.5f,{70,70,70},5},{-1,10,{70,70,70},6},{-1.5f,5.5f,{50,50,50},7},
        {.5f,12,{50,50,50},5},{.05f,20,{40,40,40},6},{1.3f,8,{60,60,60},7},
        {-2,12,{50,50,50},5},{-2.3f,15,{40,40,40},6},{-3,16,{40,40,40},7}
    }};
    count=type==1?7:type==2?9:0;
    return type==1?(vcs?sun:legacySun):headlights;
}

struct StorylandVcsSunRegistration {
    unsigned id=0;
    std::array<float,3> position{};
    std::array<uint8_t,3> color{};
    float registrationSize=0,size=0,range=999999.875f,pullAway=1.5f;
    uint8_t alpha=255,texture=0,flare=0;
    bool checkObstacles=false;
};
