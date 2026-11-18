#pragma once
#include "src/storyland/storyland_script_corona.h"
#include <map>
#include <utility>

struct StorylandScriptCoronaRegistration {
    std::array<float,3> position{};
    std::array<uint8_t,3> color{};
    float size=1;
    int type=0,flareType=0;
};

struct StorylandScriptCoronaSlot {
    StorylandScriptCoronaRegistration corona;
    float alpha=0,range=150;
    bool registered=false;
};

struct StorylandScriptCoronaFrame {
    using Key=std::pair<uintptr_t,int32_t>;
    std::map<Key,StorylandScriptCoronaSlot> slots;
    uint32_t milliseconds=0;
    bool initialized=false,vcs=true;
};

template<class Retained,class Transient>
void storylandUpdateScriptCoronaFrame(StorylandScriptCoronaFrame& frame,const Retained& retained,
    const Transient& transient,bool vcs,uint32_t milliseconds,float nativeTimeStep,
    const float* view,const float* projection) {
    if((frame.initialized&&milliseconds<frame.milliseconds)||frame.vcs!=vcs)frame={};
    const float elapsed=float(milliseconds-frame.milliseconds)/1000.0f;
    const bool newTick=milliseconds!=frame.milliseconds;
    const float change=vcs?(newTick?15.0f*std::max(0.0f,nativeTimeStep):0.0f):
        std::min(.1f,elapsed)*750.0f;
    frame.vcs=vcs;frame.milliseconds=milliseconds;frame.initialized=true;
    for(auto& entry:frame.slots)entry.second.registered=false;
    const auto registerCorona=[&](StorylandScriptCoronaFrame::Key key,const auto& corona,float range) {
        if(!storylandCoronaInRegistrationRange(corona.position,view,range))return;
        if(!frame.slots.count(key)) {
            StorylandCoronaProjection projected;
            if(frame.slots.size()>=56||!storylandProjectCorona(corona.position,view,projection,projected,range)||
                !projected.onScreen)return;
        }
        auto& slot=frame.slots[key];
        slot.corona={corona.position,corona.color,corona.size,corona.type,corona.flareType};
        slot.range=range;slot.registered=true;
    };
    if(vcs) {
        for(auto entry=retained.rbegin();entry!=retained.rend();++entry)
            registerCorona({1,entry->first},entry->second,450);
    } else {
        for(const auto& entry:retained)registerCorona({1,entry.first},entry.second,450);
    }
    for(const auto& entry:transient)registerCorona(entry.first,entry.second,vcs?150:450);
    for(auto entry=frame.slots.begin();entry!=frame.slots.end();) {
        auto& slot=entry->second;
        StorylandCoronaProjection projected;
        const bool onScreen=slot.registered&&storylandProjectCorona(slot.corona.position,view,projection,
            projected,slot.range)&&projected.onScreen;
        const float nextAlpha=std::clamp(slot.alpha+(onScreen?change:-change),0.0f,255.0f);
        slot.alpha=vcs?float(uint8_t(nextAlpha)):nextAlpha;
        if(!onScreen&&slot.alpha<=0)entry=frame.slots.erase(entry);else ++entry;
    }
}

struct StorylandScriptCoronaSprite {
    std::array<float,3> position{},color{};
    float halfWidth=0,halfHeight=0,rotation=0;
    int texture=0;
};

template<class DrawSprite>
void storylandForEachScriptCoronaSprite(const StorylandScriptCoronaFrame& frame,
    const float* view,const float* projection,int viewportHeight,float spriteSize,float fogginess,
    DrawSprite draw) {
    if(viewportHeight<=0||std::fabs(projection[5])<1.0e-6f)return;
    for(const auto& entry:frame.slots) {
        const auto& slot=entry.second;
        const auto& corona=slot.corona;
        StorylandCoronaProjection projected;
        if(!storylandProjectCorona(corona.position,view,projection,projected,slot.range))continue;
        const float alpha=slot.alpha/255.0f*storylandCoronaDistanceFade(projected.depth,slot.range);
        if(alpha<=0)continue;
        const uint8_t nativeAlpha=storylandCoronaFadedAlpha(uint8_t(slot.alpha),projected.depth,slot.range);
        const float fogScale=frame.vcs?storylandCoronaFogScale(projected.depth,fogginess):1;
        const float registeredSize=frame.vcs?(corona.size>0?corona.size*spriteSize:-corona.size):corona.size;
        const float projectionScale=frame.vcs?2/std::fabs(projection[5]):1;
        const float size=(frame.vcs?registeredSize:-registeredSize)*projected.depthScale*projectionScale*fogScale;
        StorylandScriptCoronaSprite sprite;
        sprite.position=projected.center;sprite.texture=corona.type;
        sprite.halfWidth=size*(corona.type==8?6.5f:1);
        sprite.halfHeight=size*(corona.type==8?.15f:1);
        sprite.rotation=corona.type==8?0:20.0f/projected.depth;
        if(frame.vcs)sprite.color=storylandCoronaSpriteColor(storylandCoronaFogColor(corona.color,fogScale),
            nativeAlpha,projected.biasedDepth,corona.type!=8);
        else {
            const float nearFade=corona.type==8?1:std::clamp(projected.biasedDepth-1.3f,0.0f,1.0f);
            for(size_t channel=0;channel<3;++channel)
                sprite.color[channel]=corona.color[channel]/255.0f*alpha*nearFade*nearFade;
        }
        draw(sprite);
        size_t count=0;
        const auto& flares=storylandCoronaFlares(corona.flareType,count,frame.vcs);
        for(size_t index=0;index<count;++index) {
            const auto& flare=flares[index];
            StorylandScriptCoronaSprite ghost;
            const float x=projected.cameraX*(flare.position-1)*projected.depthScale;
            const float y=projected.cameraY*(flare.position-1)*projected.depthScale;
            for(size_t axis=0;axis<3;++axis)
                ghost.position[axis]=projected.center[axis]+view[axis*4]*x+view[axis*4+1]*y;
            ghost.halfWidth=ghost.halfHeight=8*flare.size*projected.biasedDepth/
                ((frame.vcs?448:viewportHeight)*std::fabs(projection[5]));
            ghost.texture=flare.texture;
            if(frame.vcs)ghost.color=storylandCoronaFlareColor(corona.color,nativeAlpha,flare);
            else for(size_t channel=0;channel<3;++channel)
                ghost.color[channel]=corona.color[channel]/255.0f*flare.color[channel]/256.0f*alpha*200/256;
            draw(ghost);
        }
    }
}
