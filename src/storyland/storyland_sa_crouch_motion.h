#pragma once

#include "src/leeds/leeds_anim.h"
#include "src/leeds/leeds_model.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

struct StorylandSaCrouchClipMotion {
    float duration = 0.0f;
    float stride = 0.0f;
    bool valid = false;
};

inline float storylandSaCrouchAnimationStep(float seconds,float amount) {
    if(!std::isfinite(seconds)||!std::isfinite(amount)||seconds<=0.0f)return 0.0f;
    return seconds*std::max(.6f,amount)*0.92f;
}

inline float storylandSaCrouchModelScale(const StorylandModelFile& model,float height=1.8f) {
    if(model.previewPoints().empty()||!std::isfinite(height)||height<=0)return 1.0f;
    std::array<float,3> minimum{INFINITY,INFINITY,INFINITY};
    std::array<float,3> maximum{-INFINITY,-INFINITY,-INFINITY};
    for(const auto& point:model.previewPoints()) {
        minimum[0]=std::min(minimum[0],point.x);minimum[1]=std::min(minimum[1],point.y);
        minimum[2]=std::min(minimum[2],point.z);maximum[0]=std::max(maximum[0],point.x);
        maximum[1]=std::max(maximum[1],point.y);maximum[2]=std::max(maximum[2],point.z);
    }
    const float span=std::max({maximum[0]-minimum[0],maximum[1]-minimum[1],maximum[2]-minimum[2],.001f});
    return std::clamp(height/span,.001f,10.0f);
}

inline StorylandSaCrouchClipMotion storylandSaCrouchClipMotion(
    StorylandAnimFile& animation, const std::string& clipName) {
    for (const auto& clip : animation.clips()) {
        if (clip.name != clipName) continue;
        if (!animation.setActiveClipIndex(clip.index) || !animation.hasDecodedMotion()) return {};
        StorylandSaCrouchClipMotion result;
        result.duration = clip.duration;
        for (const auto& track : animation.tracks()) {
            if (track.boneId != 0u || !(track.channelFlags & 2u) || track.keys.size() < 2u) continue;
            const auto& first = track.keys.front();
            const auto& last = track.keys.back();
            result.stride = std::hypot(last.tx - first.tx, last.ty - first.ty);
            result.valid = std::isfinite(result.stride) && result.stride > 0.01f &&
                std::isfinite(result.duration) && result.duration > 0.0f;
            break;
        }
        return result;
    }
    return {};
}

inline float storylandSaCrouchPhase(float travelled, const StorylandSaCrouchClipMotion& clip) {
    if (!clip.valid || !std::isfinite(travelled) || travelled <= 0.0f) return 0.0f;
    const float cycle = std::fmod(travelled, clip.stride);
    return std::clamp(cycle / clip.stride * clip.duration, 0.0f, clip.duration);
}

inline float storylandSaCrouchMoveCommand(float previous,float requested,float seconds) {
    if (!std::isfinite(previous) || !std::isfinite(requested) ||
        !std::isfinite(seconds) || seconds < 0.0f) return 0.0f;
    const float amount = std::clamp(previous, -1.0f, 1.0f);
    const float target = std::clamp(requested, -1.0f, 1.0f);
    // SA's native timestep is milliseconds * 0.05, not render frames.
    const float change = std::min(seconds, 0.1f) * 3.5f;
    return amount + std::clamp(target - amount, -change, change);
}

inline float storylandSaCrouchMoveAmount(float previous,float requested,float seconds) {
    return storylandSaCrouchMoveCommand(std::clamp(previous,0.0f,1.0f),
        std::clamp(requested,0.0f,1.0f),seconds);
}

inline float storylandSaCrouchAssociationWeight(float previous,bool active,float seconds,
    float blendIn=8.0f,float blendOut=4.0f) {
    if(!std::isfinite(previous)||!std::isfinite(seconds)||seconds<0)return 0;
    return std::clamp(previous+(active?blendIn:-blendOut)*seconds,0.0f,1.0f);
}

inline float storylandSaCrouchRootDistance(StorylandAnimFile& animation,
    const std::string& clipName,float begin,float end) {
    if(!std::isfinite(begin)||!std::isfinite(end)||begin<0||end<=begin)return 0;
    const auto motion=storylandSaCrouchClipMotion(animation,clipName);
    if(!motion.valid)return 0;
    for(const auto& track:animation.tracks()) {
        if(track.boneId!=0u||!(track.channelFlags&2u)||track.keys.size()<2u)continue;
        const auto& first=track.keys.front();const auto& last=track.keys.back();
        const float forwardX=(last.tx-first.tx)/motion.stride;
        const float forwardY=(last.ty-first.ty)/motion.stride;
        const auto distanceAt=[&](float seconds) {
            const float cycles=std::floor(seconds/motion.duration);
            const float phase=std::clamp(seconds-cycles*motion.duration,0.0f,motion.duration);
            auto right=std::upper_bound(track.keys.begin(),track.keys.end(),phase,
                [](float time,const auto& key){return time<key.time;});
            if(right==track.keys.begin())return cycles*motion.stride;
            const auto& left=*(right-1);
            float x=left.tx,y=left.ty;
            if(right!=track.keys.end()&&right->time>left.time) {
                const float fraction=std::clamp((phase-left.time)/(right->time-left.time),0.0f,1.0f);
                x+=(right->tx-left.tx)*fraction;y+=(right->ty-left.ty)*fraction;
            }
            return cycles*motion.stride+(x-first.tx)*forwardX+(y-first.ty)*forwardY;
        };
        return std::max(0.0f,distanceAt(end)-distanceAt(begin));
    }
    return 0;
}
