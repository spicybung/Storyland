#pragma once

#include "imgui.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace StorylandLcsBikeGui {

enum class Motion { Driving, Braking, Waiting, Away };
enum class Stunt { None, Raising, Wheelie, Lowering, FrontTip, FrontHold, Settling };

struct AnimationState {
    Motion motion=Motion::Driving;
    Stunt stunt=Stunt::None;
    float width=0,position=-160,speed=0,cruiseSpeed=150;
    float stopPosition=0,stopDuration=0,waitTime=0,returnDelay=0;
    float stuntPosition=0,stuntTime=0,wheelieDuration=0,angle=0;
    int direction=1;
    bool initialized=false,stopPlanned=false,wheeliePlanned=false,frontTipPlanned=false;
    uint32_t randomSeed=0x54F163B9u;
};

inline float Random01(AnimationState& state) {
    state.randomSeed=state.randomSeed*1664525u+1013904223u;
    return float(state.randomSeed>>8)/16777216.0f;
}

inline float Ease(float fraction) {
    const float value=std::clamp(fraction,0.0f,1.0f);
    return value*value*(3.0f-2.0f*value);
}

inline void BeginPass(AnimationState& state) {
    state.motion=Motion::Driving;state.stunt=Stunt::None;
    state.speed=0;state.angle=0;state.waitTime=0;state.stuntTime=0;
    state.cruiseSpeed=135+Random01(state)*45;
    state.stopPlanned=state.width>260&&Random01(state)<.58f;
    state.stopPosition=state.width*(.32f+Random01(state)*.36f);
    state.stopDuration=1.1f+Random01(state)*2.4f;
    state.wheeliePlanned=state.width>300&&Random01(state)<.48f;
    state.stuntPosition=state.width*(.28f+Random01(state)*.40f);
    state.wheelieDuration=.45f+Random01(state)*.55f;
    state.frontTipPlanned=Random01(state)<.45f;
}

inline void AdvanceStunt(AnimationState& state,float seconds) {
    if(state.stunt==Stunt::None)return;
    state.stuntTime+=seconds;
    float duration=0;
    Stunt following=Stunt::None;
    switch(state.stunt) {
    case Stunt::Raising:
        duration=.32f;state.angle=-.44f*Ease(state.stuntTime/duration);following=Stunt::Wheelie;break;
    case Stunt::Wheelie:
        duration=state.wheelieDuration;state.angle=-.44f;following=Stunt::Lowering;break;
    case Stunt::Lowering:
        duration=.34f;state.angle=-.44f*(1-Ease(state.stuntTime/duration));
        following=state.frontTipPlanned?Stunt::FrontTip:Stunt::None;break;
    case Stunt::FrontTip:
        duration=.28f;state.angle=.36f*Ease(state.stuntTime/duration);following=Stunt::FrontHold;break;
    case Stunt::FrontHold:
        duration=.30f;state.angle=.36f;following=Stunt::Settling;break;
    case Stunt::Settling:
        duration=.38f;state.angle=.36f*(1-Ease(state.stuntTime/duration));break;
    default:break;
    }
    if(state.stuntTime>=duration) {
        state.stunt=following;state.stuntTime=0;
        if(following==Stunt::None||following==Stunt::FrontTip)state.angle=0;
    }
}

inline void Advance(AnimationState& state,float deltaTime,float width) {
    if(!std::isfinite(width)||width<=0)return;
    if(!state.initialized) {
        state.initialized=true;state.width=width;BeginPass(state);
    } else if(state.width!=width) {
        const float progress=(state.position+160)/(state.width+320);
        state.position=progress*(width+320)-160;
        state.stopPosition*=width/state.width;state.stuntPosition*=width/state.width;state.width=width;
    }
    if(!std::isfinite(deltaTime)||deltaTime<=0)return;
    float remaining=std::min(deltaTime,.25f);
    while(remaining>0) {
        const float step=std::min(remaining,1.0f/120.0f);remaining-=step;
        if(state.motion==Motion::Away) {
            state.waitTime+=step;
            if(state.waitTime>=state.returnDelay) {
                // Reversing returns through the exited edge. Keeping direction
                // starts a new pass from the opposite edge, while fully hidden.
                if(Random01(state)<.5f)state.direction=-state.direction;
                state.position=state.direction>0?-160:width+160;
                BeginPass(state);
            }
            continue;
        }
        if(state.motion==Motion::Waiting) {
            state.waitTime+=step;
            if(state.waitTime>=state.stopDuration) {
                state.motion=Motion::Driving;state.waitTime=0;
            }
            continue;
        }
        if(state.motion==Motion::Driving&&state.stunt==Stunt::None&&state.wheeliePlanned&&
            (state.position-state.stuntPosition)*state.direction>=0&&
            state.position>65&&state.position<width-65) {
            state.wheeliePlanned=false;state.stunt=Stunt::Raising;state.stuntTime=0;
        }
        AdvanceStunt(state,step);
        const float distanceToStop=(state.stopPosition-state.position)*state.direction;
        if(state.stopPlanned&&state.stunt==Stunt::None&&
            distanceToStop<=state.speed*state.speed/300+state.speed*step) {
            state.stopPlanned=false;state.motion=Motion::Braking;
        }
        const bool frontWheelBalance=state.stunt==Stunt::FrontTip||state.stunt==Stunt::FrontHold||
            state.stunt==Stunt::Settling;
        const float previous=state.speed;
        if(state.motion==Motion::Braking||frontWheelBalance)state.speed=std::max(0.0f,state.speed-150*step);
        else state.speed=std::min(state.cruiseSpeed,state.speed+175*step);
        state.position+=float(state.direction)*(previous+state.speed)*.5f*step;
        if(state.motion==Motion::Braking&&state.speed==0) {
            state.motion=Motion::Waiting;state.waitTime=0;
        }
        if((state.direction>0&&state.position>width+160)||(state.direction<0&&state.position<-160)) {
            state.position=state.direction>0?width+160:-160;
            state.motion=Motion::Away;state.stunt=Stunt::None;state.angle=0;state.speed=0;
            state.waitTime=0;state.returnDelay=.9f+Random01(state)*2.1f;
        }
    }
}

struct SpritePose {
    std::array<ImVec2,4> corners{};
    ImVec2 rearContact{},frontContact{};
};

inline SpritePose BuildPose(const AnimationState& state,float x,float baseline,float scale) {
    constexpr float cropLeft=55,cropTop=185,cropRight=1214,cropBottom=1172;
    constexpr float rearX=240,rearY=1111,frontX=1042,frontY=1169;
    const float pixelScale=90*scale/(cropRight-cropLeft);
    const float neutral=-std::atan2(frontY-rearY,frontX-rearX);
    const float neutralCosine=std::cos(neutral),neutralSine=std::sin(neutral);
    const float wheelbase=std::hypot(frontX-rearX,frontY-rearY)*pixelScale;
    const float pivot=state.angle>0?wheelbase:0;
    const float cosine=std::cos(state.angle),sine=std::sin(state.angle);
    const auto point=[&](float sourceX,float sourceY) {
        const float dx=(sourceX-rearX)*pixelScale,dy=(sourceY-rearY)*pixelScale;
        const float localX=dx*neutralCosine-dy*neutralSine;
        const float localY=dx*neutralSine+dy*neutralCosine;
        const float rotatedX=pivot+(localX-pivot)*cosine-localY*sine;
        const float rotatedY=(localX-pivot)*sine+localY*cosine;
        return ImVec2(x+float(state.direction)*(rotatedX-wheelbase*.5f),baseline+rotatedY);
    };
    return {{{point(cropLeft,cropTop),point(cropRight,cropTop),
        point(cropRight,cropBottom),point(cropLeft,cropBottom)}},
        point(rearX,rearY),point(frontX,frontY)};
}

inline AnimationState& GetState() {
    static AnimationState state;
    return state;
}

inline void Render(bool lcsFormatSelected,bool enabled=true,float uiScale=1,
    ImTextureID riderTexture=ImTextureID(0)) {
    auto& state=GetState();
    if(!lcsFormatSelected||!enabled||!ImGui::GetCurrentContext()) {state=AnimationState{};return;}
    const auto* viewport=ImGui::GetMainViewport();
    if(viewport->Size.x<=0||viewport->Size.y<=0)return;
    const float scale=std::isfinite(uiScale)?std::clamp(uiScale,.5f,3.0f):1.0f;
    const float width=viewport->Size.x/scale;
    Advance(state,ImGui::GetIO().DeltaTime,width);
    auto* draw=ImGui::GetBackgroundDrawList();
    const ImVec2 bottomRight(viewport->Pos.x+viewport->Size.x,viewport->Pos.y+viewport->Size.y);
    const float baseline=bottomRight.y-3*scale;
    draw->PushClipRect(viewport->Pos,bottomRight,true);
    draw->AddRectFilled(ImVec2(viewport->Pos.x,baseline),bottomRight,IM_COL32(28,29,31,230));
    draw->AddLine(ImVec2(viewport->Pos.x,baseline),ImVec2(bottomRight.x,baseline),
        IM_COL32(218,175,69,230),1.4f*scale);
    if(riderTexture&&state.motion!=Motion::Away) {
        const auto pose=BuildPose(state,viewport->Pos.x+state.position*scale,baseline,scale);
        constexpr float imageSize=1254;
        draw->AddImageQuad(riderTexture,pose.corners[0],pose.corners[1],pose.corners[2],pose.corners[3],
            ImVec2(55/imageSize,185/imageSize),ImVec2(1214/imageSize,185/imageSize),
            ImVec2(1214/imageSize,1172/imageSize),ImVec2(55/imageSize,1172/imageSize));
    }
    draw->PopClipRect();
}

}
