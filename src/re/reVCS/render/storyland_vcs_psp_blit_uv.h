#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

struct StorylandPspBlitUvPlane {
    static StorylandPspBlitUvPlane fromRects(const std::array<int,4>& source,
        const std::array<int,4>& target,int textureWidth,int textureHeight) {
        StorylandPspBlitUvPlane result;
        if(textureWidth<=0||textureHeight<=0||textureWidth>512||textureHeight>512||
            source[0]<0||source[1]<0||source[2]>textureWidth||source[3]>textureHeight||
            source[2]<=source[0]||source[3]<=source[1])return result;

        const int64_t width=(int64_t(target[2])-target[0])*16;
        const int64_t height=(int64_t(target[3])-target[1])*16;
        if(width<=0||height<=0||width>1024||height>1024)return result;
        const int64_t area=width*height;

        // These are the five strip areas emitted by the native 64x64 environment pass.
        // Their measured GE setup reciprocals equal the integer quotient below.
        // Other triangle areas need the general GE setup reciprocal table.
        constexpr std::array<int64_t,5> supportedAreas={98304,65536,247808,43776,48640};
        if(std::find(supportedAreas.begin(),supportedAreas.end(),area)==supportedAreas.end())
            return result;

        int exponent=0;
        for(int64_t remaining=area;remaining>1;remaining>>=1)++exponent;
        const int64_t reciprocal=(int64_t(1)<<(16+exponent))/area;
        result.horizontal=makeAxis(source[0],source[2],textureWidth,target[0],height,
            reciprocal,exponent);
        result.vertical=makeAxis(source[1],source[3],textureHeight,target[1],width,
            reciprocal,exponent);
        result.initialized=true;
        return result;
    }

    bool valid()const {
        return initialized;
    }

    std::array<double,2> sample(int x,int y)const {
        return {horizontal.sample(x),vertical.sample(y)};
    }

private:
    static int64_t floorPowerOfTwo(int64_t value,int shift) {
        const int64_t divisor=int64_t(1)<<shift;
        const int64_t quotient=value/divisor;
        return quotient-(value%divisor<0?1:0);
    }

    struct Axis {
        int64_t constant=0;
        int64_t gradient=0;
        int shift=0;
        int textureSize=0;

        double sample(int coordinate)const {
            const int64_t subpixel=int64_t(coordinate)*16+8;
            const int64_t fixed=floorPowerOfTwo(constant+gradient*subpixel,14);
            return std::ldexp(double(fixed),-shift)*textureSize;
        }
    };

    static Axis makeAxis(int first,int last,int textureSize,int origin,int64_t edgeLength,
        int64_t reciprocal,int areaExponent) {
        Axis result;
        result.textureSize=textureSize;
        const double firstNormalized=double(first)/textureSize;
        const double lastNormalized=double(last)/textureSize;
        const double largest=std::max(std::abs(firstNormalized),std::abs(lastNormalized));
        int exponent=0;
        if(largest!=0.0) {
            std::frexp(largest,&exponent);
            result.shift=15-exponent;
        }
        const int64_t scaledFirst=int64_t(std::ldexp(firstNormalized,result.shift));
        const int64_t scaledLast=int64_t(std::ldexp(lastNormalized,result.shift));
        const int64_t numerator=(scaledLast-scaledFirst)*edgeLength;
        result.gradient=floorPowerOfTwo(numerator*reciprocal,areaExponent+2);
        result.constant=scaledFirst*16384-result.gradient*int64_t(origin)*16;
        return result;
    }

    Axis horizontal;
    Axis vertical;
    bool initialized=false;
};
