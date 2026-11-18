#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

// Rsl materials belong to the Leeds engine. Unlike RenderWare DFF materials,
// they do not store animation tracks in an RpMaterialUVAnim plugin. The engine
// supplies their material transforms separately, so these tracks are bound by
// material slot and advanced with the game clock.
namespace StorylandRslUV {

struct Keyframe {
    float seconds=0.0f;
    float offsetU=0.0f;
    float offsetV=0.0f;
    float scaleU=1.0f;
    float scaleV=1.0f;
    float rotation=0.0f;
    float shear=0.0f;
};

struct Track {
    std::vector<Keyframe> frames;
    float duration=0.0f;
    bool looping=true;

    bool valid() const {
        if(frames.empty()||!std::isfinite(duration)||duration<0.0f)return false;
        float last=-1.0f;
        for(const auto& key:frames){
            if(!std::isfinite(key.seconds)||key.seconds<0.0f||key.seconds<last ||
               !std::isfinite(key.offsetU)||!std::isfinite(key.offsetV)||
               !std::isfinite(key.scaleU)||!std::isfinite(key.scaleV)||
               !std::isfinite(key.rotation)||!std::isfinite(key.shear))return false;
            last=key.seconds;
        }
        return duration>=frames.back().seconds;
    }

    std::array<float,6> matrix(float seconds) const {
        if(!valid())return {{1,0,0,1,0,0}};
        float clock=std::max(0.0f,seconds);
        if(looping&&duration>0.0f) {
            clock=std::fmod(seconds,duration);
            if(clock<0.0f)clock+=duration;
        } else clock=std::min(clock,duration);
        const auto after=std::upper_bound(frames.begin(),frames.end(),clock,
            [](float value,const Keyframe& key){return value<key.seconds;});
        Keyframe sampled=frames.front();
        if(after==frames.end())sampled=frames.back();
        else if(after!=frames.begin()){
            const Keyframe& previous=*(after-1);
            const float difference=after->seconds-previous.seconds;
            const float factor=difference>1e-6f?
                std::clamp((clock-previous.seconds)/difference,0.0f,1.0f):0.0f;
            const auto lerp=[&](float lhs,float rhs){return lhs+(rhs-lhs)*factor;};
            sampled.offsetU=lerp(previous.offsetU,after->offsetU);
            sampled.offsetV=lerp(previous.offsetV,after->offsetV);
            sampled.scaleU=lerp(previous.scaleU,after->scaleU);
            sampled.scaleV=lerp(previous.scaleV,after->scaleV);
            sampled.shear=lerp(previous.shear,after->shear);
            constexpr float pi=3.14159265358979323846f;
            float delta=after->rotation-previous.rotation;
            while(delta>pi)delta-=2.0f*pi;
            while(delta< -pi)delta+=2.0f*pi;
            sampled.rotation=previous.rotation+delta*factor;
        }
        const float cs=std::cos(sampled.rotation),sn=std::sin(sampled.rotation);
        const float a=cs*sampled.scaleU;
        const float b=sn*sampled.scaleU;
        const float c=cs*sampled.shear-sn*sampled.scaleV;
        const float d=sn*sampled.shear+cs*sampled.scaleV;
        const float tx=sampled.offsetU+0.5f-0.5f*(a+c);
        const float ty=sampled.offsetV+0.5f-0.5f*(b+d);
        return {{a,b,c,d,tx,ty}};
    }

    std::array<float,2> transform(float u,float v,float seconds) const {
        const auto m=matrix(seconds);
        return {{u*m[0]+v*m[2]+m[4],u*m[1]+v*m[3]+m[5]}};
    }
};

struct Material {
    std::array<Track,2> channels;
    bool animated(size_t channel=0) const {
        return channel<channels.size()&&channels[channel].valid();
    }
    std::array<float,2> transform(float u,float v,float seconds,size_t channel=0) const {
        if(!animated(channel))return {{u,v}};
        return channels[channel].transform(u,v,seconds);
    }
};

}
