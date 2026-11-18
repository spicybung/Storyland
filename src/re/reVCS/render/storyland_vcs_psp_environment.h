#pragma once

#include "src/re/reVCS/render/storyland_vcs_psp_blit_uv.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

inline constexpr int StorylandPspEnvironmentSize=64;
inline constexpr int StorylandPspFrameWidth=512;
inline constexpr int StorylandPspFrameHeight=320;
using StorylandPspEnvironmentPixels=std::array<uint8_t,64*64*4>;

struct StorylandPspEnvironmentImageView {
    const uint8_t* pixels=nullptr;
    int width=0,height=0;
    size_t stride=0;
    bool bottomUp=false,bgra=false;
};

struct StorylandPspEnvironmentRect {
    int left=0,top=0,right=0,bottom=0;
};

struct StorylandPspEnvironmentStrip {
    StorylandPspEnvironmentRect source,target;
    bool corona=false;
};

struct StorylandPspEnvironmentCapturePlan {
    std::array<StorylandPspEnvironmentStrip,19> strips{};
    size_t count=0;
    int headingShift=32;
};

inline float storylandPspEnvironmentHeading(const std::array<float,3>& cameraForward) {
    if(!std::isfinite(cameraForward[0])||!std::isfinite(cameraForward[1])||
        (cameraForward[0]==0.0f&&cameraForward[1]==0.0f))return 0.0f;
    return std::atan2(-cameraForward[0],cameraForward[1]);
}

inline StorylandPspEnvironmentCapturePlan storylandPspEnvironmentPlanForShift(int headingShift) {
    StorylandPspEnvironmentCapturePlan result;
    result.headingShift=headingShift;
    const auto append=[&](StorylandPspEnvironmentRect source,StorylandPspEnvironmentRect target,bool corona) {
        const int sourceWidth=source.right-source.left,targetWidth=target.right-target.left;
        const int step=targetWidth*32/sourceWidth;
        for(int offset=0,part=0;offset<sourceWidth;offset+=32,++part) {
            const int sourceRight=std::min(source.left+offset+32,source.right);
            const int targetLeft=target.left+part*step;
            const int targetRight=sourceRight==source.right?target.right:targetLeft+step;
            result.strips[result.count++]={{source.left+offset,source.top,sourceRight,source.bottom},
                {targetLeft,target.top,targetRight,target.bottom},corona};
        }
    };
    const int cropWidth=StorylandPspFrameWidth*StorylandPspFrameHeight/480;
    const int cropLeft=(StorylandPspFrameWidth-cropWidth)/2;
    append({cropLeft,0,cropLeft+cropWidth,StorylandPspFrameHeight},{0,0,64,64},false);
    for(const auto& quad:std::array<std::array<float,3>,2>{{{{.2f,0,.7f}},{{.9f,.1f,.3f}}}}) {
        const int x=int(quad[0]*64),y=int(quad[1]*64),size=int(quad[2]*64);
        for(int offset:{headingShift,headingShift-64})
            append({0,0,64,64},{x+offset,y,x+offset+size,y+size},true);
    }
    return result;
}

inline StorylandPspEnvironmentCapturePlan storylandPspEnvironmentPlan(float headingRadians) {
    constexpr float pi=3.14159265358979323846f;
    if(!std::isfinite(headingRadians))headingRadians=0.0f;
    headingRadians=std::remainder(headingRadians,2.0f*pi);
    return storylandPspEnvironmentPlanForShift(int((headingRadians/(2.0f*pi)+0.5f)*64.0f));
}

inline uint16_t storylandPspEnvironment565(const std::array<unsigned,3>& color) {
    return uint16_t((std::min(color[0],255u)>>3)|((std::min(color[1],255u)>>2)<<5)|
        ((std::min(color[2],255u)>>3)<<11));
}

inline std::array<unsigned,3> storylandPspEnvironmentExpand565(uint16_t packed) {
    const unsigned red=packed&31u,green=(packed>>5)&63u,blue=(packed>>11)&31u;
    return {(red<<3)|(red>>2),(green<<2)|(green>>4),(blue<<3)|(blue>>2)};
}

inline std::array<unsigned,3> storylandPspEnvironmentPixel(
    const StorylandPspEnvironmentImageView& image,int x,int y) {
    x=std::clamp(x,0,image.width-1);
    y=std::clamp(y,0,image.height-1);
    if(image.bottomUp)y=image.height-1-y;
    const size_t stride=image.stride?image.stride:size_t(image.width)*4;
    const uint8_t* pixel=image.pixels+size_t(y)*stride+size_t(x)*4;
    return {pixel[image.bgra?2:0],pixel[1],pixel[image.bgra?0:2]};
}

inline std::array<unsigned,3> storylandPspEnvironmentSample(
    const StorylandPspEnvironmentImageView& image,double u,double v) {
    const double x=u-0.5,y=v-0.5;
    const int left=int(std::floor(x)),top=int(std::floor(y));
    const unsigned horizontal=unsigned(std::floor((x-left)*16.0));
    const unsigned vertical=unsigned(std::floor((y-top)*16.0));
    const auto a=storylandPspEnvironmentPixel(image,left,top);
    const auto b=storylandPspEnvironmentPixel(image,left+1,top);
    const auto c=storylandPspEnvironmentPixel(image,left,top+1);
    const auto d=storylandPspEnvironmentPixel(image,left+1,top+1);
    std::array<unsigned,3> result{};
    for(size_t channel=0;channel<3;++channel) {
        const unsigned upper=((16-horizontal)*a[channel]+horizontal*b[channel])>>4;
        const unsigned lower=((16-horizontal)*c[channel]+horizontal*d[channel])>>4;
        result[channel]=((16-vertical)*upper+vertical*lower)>>4;
    }
    return result;
}

inline bool storylandPspComposeEnvironment(const StorylandPspEnvironmentImageView& framebuffer,
    const StorylandPspEnvironmentImageView& coronastar,float headingRadians,
    StorylandPspEnvironmentPixels& output,std::string& error) {
    const auto valid=[](const StorylandPspEnvironmentImageView& image) {
        return image.pixels&&image.width>0&&image.height>0&&image.width<=16384&&image.height<=16384&&
            (!image.stride||image.stride>=size_t(image.width)*4);
    };
    if(!valid(framebuffer)||!valid(coronastar)||coronastar.width!=64||coronastar.height!=64) {
        error="The PSP environment requires a visible framebuffer and the original 64x64 coronastar raster.";
        return false;
    }
    const auto plan=storylandPspEnvironmentPlan(headingRadians);
    std::array<uint16_t,64*64> target{};
    for(size_t index=0;index<plan.count;++index) {
        const auto& strip=plan.strips[index];
        const auto& image=strip.corona?coronastar:framebuffer;
        const auto& source=strip.source;
        const auto& destination=strip.target;
        const double scaleX=strip.corona?1.0:double(image.width)/StorylandPspFrameWidth;
        const double scaleY=strip.corona?1.0:double(image.height)/StorylandPspFrameHeight;
        const int nativeTextureSize=strip.corona?64:512;
        const auto plane=StorylandPspBlitUvPlane::fromRects(
            {source.left,source.top,source.right,source.bottom},
            {destination.left,destination.top,destination.right,destination.bottom},
            nativeTextureSize,nativeTextureSize);
        if(!plane.valid()) {
            error="The PSP environment contains an unsupported native blit geometry.";
            return false;
        }
        for(int y=std::max(0,destination.top);y<std::min(64,destination.bottom);++y) {
            for(int x=std::max(0,destination.left);x<std::min(64,destination.right);++x) {
                const auto uv=plane.sample(x,y);
                auto color=storylandPspEnvironmentSample(image,uv[0]*scaleX,uv[1]*scaleY);
                const size_t pixel=size_t(y)*64+size_t(x);
                if(strip.corona) {
                    const auto previous=storylandPspEnvironmentExpand565(target[pixel]);
                    for(size_t channel=0;channel<3;++channel)color[channel]+=previous[channel];
                }
                target[pixel]=storylandPspEnvironment565(color);
            }
        }
    }
    for(size_t index=0;index<target.size();++index) {
        const auto color=storylandPspEnvironmentExpand565(target[index]);
        for(size_t channel=0;channel<3;++channel)output[index*4+channel]=uint8_t(color[channel]);
        output[index*4+3]=255;
    }
    error.clear();
    return true;
}
