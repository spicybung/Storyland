#pragma once

#include "src/storyland/storyland_play_test.h"
#include "src/storyland/storyland_water.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

inline std::string storylandSaDivingClipKey(std::string name) {
    for(char& letter:name)if(letter>='A'&&letter<='Z')letter=char(letter-'A'+'a');
    return name;
}

struct StorylandSaDivingBank {
    StorylandAnimFile animation;
    bool available=false;
    float entryDuration=0,strokeDuration=0;

    bool load(const std::wstring& path,std::string& error) {
        available=false;entryDuration=0;strokeDuration=0;
        if(!animation.loadFromFile(path,error))return false;
        const std::array<const char*,6> names{
            "Swim_Breast","SWIM_crawl","Swim_Dive_Under","Swim_Glide","Swim_jumpout","Swim_Under"};
        for(const char* name:names) {
            const auto found=std::find_if(animation.clips().begin(),animation.clips().end(),
                [&](const auto& clip){return storylandSaDivingClipKey(clip.name)==storylandSaDivingClipKey(name);});
            if(found==animation.clips().end()||!std::isfinite(found->duration)||found->duration<=0||
                !animation.setActiveClipIndex(found->index)||!animation.hasDecodedMotion()) {
                error="The converted SA SWIM bank is missing a decoded clip: "+std::string(name);
                return false;
            }
            if(storylandSaDivingClipKey(name)=="swim_dive_under")entryDuration=found->duration;
            if(storylandSaDivingClipKey(name)=="swim_under")strokeDuration=found->duration;
        }
        const auto root=std::find_if(animation.tracks().begin(),animation.tracks().end(),
            [](const auto& track){return track.boneId==0&&(track.channelFlags&2u)&&track.keys.size()>1;});
        if(root==animation.tracks().end()) {
            error="The converted SA underwater stroke has no authored root translation.";
            return false;
        }
        available=true;error.clear();return true;
    }

    float rootDistance(float begin,float end) {
        if(!available||!std::isfinite(begin)||!std::isfinite(end)||begin<0||end<=begin)return 0;
        const auto clip=std::find_if(animation.clips().begin(),animation.clips().end(),
            [](const auto& value){return storylandSaDivingClipKey(value.name)=="swim_under";});
        if(clip==animation.clips().end()||!animation.setActiveClipIndex(clip->index))return 0;
        for(const auto& track:animation.tracks()) {
            if(track.boneId!=0||!(track.channelFlags&2u)||track.keys.size()<2)continue;
            const auto& first=track.keys.front();const auto& last=track.keys.back();
            const float x=last.tx-first.tx,y=last.ty-first.ty,stride=std::hypot(x,y);
            if(!std::isfinite(stride)||stride<=0||strokeDuration<=0)return 0;
            const auto sample=[&](float seconds) {
                const float cycles=std::floor(seconds/strokeDuration);
                const float phase=std::clamp(seconds-cycles*strokeDuration,0.0f,strokeDuration);
                const auto right=std::upper_bound(track.keys.begin(),track.keys.end(),phase,
                    [](float value,const auto& key){return value<key.time;});
                if(right==track.keys.begin())return cycles*stride;
                const auto& left=*(right-1);
                float tx=left.tx,ty=left.ty;
                if(right!=track.keys.end()&&right->time>left.time) {
                    const float weight=std::clamp((phase-left.time)/(right->time-left.time),0.0f,1.0f);
                    tx+=(right->tx-left.tx)*weight;ty+=(right->ty-left.ty)*weight;
                }
                return cycles*stride+((tx-first.tx)*x+(ty-first.ty)*y)/stride;
            };
            return std::max(0.0f,sample(end)-sample(begin));
        }
        return 0;
    }
};

inline bool storylandSampleSaDivingPose(StorylandPlayTestAnimation& sampler,
    const StorylandModelFile& model,StorylandSaDivingBank& bank,
    const StorylandCharacterWaterState& water,std::vector<StorylandModelPoint>& points,
    std::string& error,std::vector<std::array<float,16>>* frames=nullptr) {
    if(!bank.available||!water.diving) {
        error="SA diving motion is not active or its original animation bank is unavailable.";
        return false;
    }
    const bool entry=water.diveTime<bank.entryDuration;
    const char* clip=entry?"Swim_Dive_Under":water.diveStroke?"Swim_Under":"Swim_Glide";
    const float time=entry?water.diveTime:water.diveAnimationTime;
    std::vector<std::array<float,16>> sampledFrames;
    if(!sampler.sampleExternalNamedClip(model,bank.animation,clip,time,!entry,points,error,
        &sampledFrames,true))return false;
    if(sampledFrames.size()!=model.armatureBones().size()||sampledFrames.empty()) {
        error="SA diving pose did not produce the complete native skeleton.";return false;
    }
    size_t root=0;
    for(size_t index=0;index<model.armatureBones().size();++index)
        if(model.armatureBones()[index].boneId==0) {root=index;break;}
    const auto pivot=sampledFrames[root];
    const float pitch=std::clamp(water.divePitch,-1.3962634015954636f,1.3962634015954636f);
    const float cosine=std::cos(pitch),sine=std::sin(pitch);
    const auto rotate=[&](float& y,float& z) {
        const float previousY=y;y=previousY*cosine-z*sine;z=previousY*sine+z*cosine;
    };
    for(auto& point:points) {
        point.y-=pivot[13];point.z-=pivot[14];rotate(point.y,point.z);
        point.y+=pivot[13];point.z+=pivot[14];
        if(point.hasNormal)rotate(point.ny,point.nz);
        if(point.hasPs2Normal)rotate(point.ps2Ny,point.ps2Nz);
    }
    for(auto& frame:sampledFrames) {
        for(size_t column=0;column<12;column+=4)rotate(frame[column+1],frame[column+2]);
        frame[13]-=pivot[13];frame[14]-=pivot[14];rotate(frame[13],frame[14]);
        frame[13]+=pivot[13];frame[14]+=pivot[14];
    }
    if(frames)*frames=std::move(sampledFrames);
    error.clear();return true;
}
