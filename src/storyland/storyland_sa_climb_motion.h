#pragma once

#include "src/leeds/leeds_anim.h"
#include "src/storyland/storyland_extended_movement.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

inline float storylandSaClimbRootProgress(StorylandAnimFile& animation,
    StorylandExtendedAction action,float seconds) {
    const bool vertical=action==StorylandExtendedAction::ClimbPull ||
        action==StorylandExtendedAction::ClimbStand;
    if(!vertical && action!=StorylandExtendedAction::ClimbVault)
        return std::numeric_limits<float>::quiet_NaN();
    if(!std::isfinite(seconds))return std::numeric_limits<float>::quiet_NaN();
    const std::string name=storylandExtendedActionClip(action);
    for(const auto& clip:animation.clips()) {
        if(clip.name!=name)continue;
        if(!animation.setActiveClipIndex(clip.index))break;
        for(const auto& track:animation.tracks()) {
            if(track.boneId!=0 || !(track.channelFlags&2u) || track.keys.size()<2)continue;
            const auto component=[&](const StorylandAnimKey& key) {return vertical?key.tz:key.ty;};
            const float first=component(track.keys.front()),last=component(track.keys.back());
            if(!std::isfinite(first) || !std::isfinite(last) || std::fabs(last-first)<.001f)break;
            const float time=std::clamp(seconds,0.0f,clip.duration);
            if(time>=clip.duration)return 1.0f;
            const auto right=std::upper_bound(track.keys.begin(),track.keys.end(),time,
                [](float value,const StorylandAnimKey& key) {return value<key.time;});
            float value=first;
            if(right==track.keys.end())value=last;
            else if(right!=track.keys.begin()) {
                const auto& left=*(right-1);
                const float weight=right->time>left.time?
                    std::clamp((time-left.time)/(right->time-left.time),0.0f,1.0f):0;
                value=component(left)+(component(*right)-component(left))*weight;
            }
            return std::clamp((value-first)/(last-first),0.0f,1.0f);
        }
        break;
    }
    return std::numeric_limits<float>::quiet_NaN();
}

inline StorylandNativePoint storylandSaCrouchRollRootDelta(StorylandAnimFile& animation,
    StorylandExtendedAction action,float begin,float end) {
    if(action!=StorylandExtendedAction::CrouchRollLeft&&
        action!=StorylandExtendedAction::CrouchRollRight)return {};
    if(!std::isfinite(begin)||!std::isfinite(end)||end<=begin)return {};
    for(const auto& clip:animation.clips()) {
        if(clip.name!=storylandExtendedActionClip(action))continue;
        if(!animation.setActiveClipIndex(clip.index))break;
        for(const auto& track:animation.tracks()) {
            if(track.boneId!=0||!(track.channelFlags&2u)||track.keys.size()<2)continue;
            const auto sample=[&](float seconds) {
                const float time=std::clamp(seconds,0.0f,clip.duration);
                const auto point=[](const StorylandAnimKey& key) {
                    return StorylandNativePoint{key.tx,key.ty,key.tz};
                };
                if(time>=clip.duration)return point(track.keys.back());
                const auto right=std::upper_bound(track.keys.begin(),track.keys.end(),time,
                    [](float value,const StorylandAnimKey& key) {return value<key.time;});
                if(right==track.keys.begin())return point(track.keys.front());
                if(right==track.keys.end())return point(track.keys.back());
                const auto& left=*(right-1);
                const float weight=right->time>left.time?
                    std::clamp((time-left.time)/(right->time-left.time),0.0f,1.0f):0;
                return StorylandNativePoint{left.tx+(right->tx-left.tx)*weight,
                    left.ty+(right->ty-left.ty)*weight,left.tz+(right->tz-left.tz)*weight};
            };
            const auto first=sample(begin),last=sample(end);
            return {last.x-first.x,last.y-first.y,last.z-first.z};
        }
        break;
    }
    return {};
}
