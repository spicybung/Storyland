#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

enum class StorylandVehicleAction {None,Enter,Hijack,Exit};
enum class StorylandVehicleEntryVariant {
    Normal, PassengerSideDriver, BikeFront, BikePickup, BikePullup,
    CrawlOut, MovingExit
};
enum class StorylandVehicleExitRecovery { None, Fallen, GetUp };
struct StorylandVehicleExitPose {
    bool active=false;
    int32_t animation=-1,group=-1,sourceVehicle=-1;
    float elapsed=0,duration=0;
    std::array<float,3> velocity{};
    bool nativeBailout=false;
    StorylandVehicleExitRecovery recovery=StorylandVehicleExitRecovery::None;
    int32_t getUpAnimation=-1;
    float getUpDuration=0;
    uint32_t getUpAfter=0;
};
enum class StorylandVehiclePhase : unsigned {Approach,Align,Open,PullOut,GetIn,CloseInside,GetOut,CloseOutside,Done};
struct StorylandVehiclePhaseTiming {
    std::array<float,8> seconds{0,0,0,1,1.2f,0,1.2f,0};
};
struct StorylandVehiclePhaseSample {
    StorylandVehiclePhase phase=StorylandVehiclePhase::Done;
    float elapsed=0,duration=0,fraction=1,total=0;
};
inline StorylandVehiclePhaseSample storylandSampleVehicleTransition(StorylandVehicleAction action,
    const StorylandVehiclePhaseTiming& timing,float elapsed) {
    using P=StorylandVehiclePhase;
    std::array<P,6> phases{P::Done,P::Done,P::Done,P::Done,P::Done,P::Done};
    if(action==StorylandVehicleAction::Enter)phases={P::Approach,P::Align,P::Open,P::GetIn,P::CloseInside,P::Done};
    else if(action==StorylandVehicleAction::Hijack)phases={P::Approach,P::Align,P::Open,P::PullOut,P::GetIn,P::CloseInside};
    else if(action==StorylandVehicleAction::Exit)phases={P::GetOut,P::CloseOutside,P::Done,P::Done,P::Done,P::Done};
    StorylandVehiclePhaseSample result;float cursor=0;
    for(P p:phases) {
        if(p==P::Done)continue;
        const float duration=timing.seconds[unsigned(p)];
        if(!std::isfinite(duration)||duration<=0)continue;
        if(result.phase==P::Done&&elapsed<cursor+duration) {
            result.phase=p;result.elapsed=std::max(0.0f,elapsed-cursor);result.duration=duration;
            result.fraction=result.elapsed/duration;
        }
        cursor+=duration;
    }
    result.total=cursor;return result;
}
// Never sample a car-entry association while the ped is still walking to
// the door. The transition has not acquired a native seat frame until the
// approach is complete, and rendering GetIn here makes the ped skate forward.
inline bool storylandVehicleApproachLocomotion(StorylandVehicleAction action,
    bool prepared,StorylandVehiclePhase phase) {
    return (action==StorylandVehicleAction::Enter||action==StorylandVehicleAction::Hijack)&&
        (!prepared||phase==StorylandVehiclePhase::Approach);
}

inline const char* storylandVehiclePhaseAction(StorylandVehiclePhase phase) {
    using P=StorylandVehiclePhase;
    switch(phase) {
        case P::Align:return "align";case P::Open:return "open";case P::PullOut:return "hijack";
        case P::GetIn:return "enter";case P::CloseInside:return "close_inside";
        case P::GetOut:return "exit";case P::CloseOutside:return "close_outside";
        default:return "sit";
    }
}
struct StorylandVehicleDoorCueTimes {
    float open=-1,close=-1;
};
inline StorylandVehicleDoorCueTimes storylandVehicleDoorCueTimes(StorylandVehicleAction action,
    const StorylandVehiclePhaseTiming& timing) {
    using Phase=StorylandVehiclePhase;
    StorylandVehicleDoorCueTimes result;
    const auto open=timing.seconds[unsigned(Phase::Open)];
    const auto closeInside=timing.seconds[unsigned(Phase::CloseInside)];
    const auto closeOutside=timing.seconds[unsigned(Phase::CloseOutside)];
    if(action==StorylandVehicleAction::Enter||action==StorylandVehicleAction::Hijack) {
        const float start=timing.seconds[unsigned(Phase::Approach)]+
            timing.seconds[unsigned(Phase::Align)];
        if(open>.41f)result.open=start+.41f;
        const float insideStart=start+open+
            (action==StorylandVehicleAction::Hijack?timing.seconds[unsigned(Phase::PullOut)]:0)+
            timing.seconds[unsigned(Phase::GetIn)];
        if(closeInside>=.63f)result.close=insideStart+.63f;
    } else if(action==StorylandVehicleAction::Exit) {
        if(timing.seconds[unsigned(Phase::GetOut)]>.06f)result.open=.06f;
        if(closeOutside>=.5f)
            result.close=timing.seconds[unsigned(Phase::GetOut)]+.5f;
    }
    return result;
}
inline bool storylandVehicleCrossedCue(float previous,float current,float cue) {
    return cue>=0&&previous<cue&&current>=cue;
}

inline float storylandVehicleDoorPhaseRatio(float previous,StorylandVehiclePhase phase,
    float seconds,bool bus=false,bool rearVan=false) {
    using Phase=StorylandVehiclePhase;
    float begin=0,end=0;
    bool opening=true;
    if(phase==Phase::Open) {
        begin=bus?.66f:rearVan?.37f:.41f;
        end=bus?.8f:rearVan?.55f:.89f;
    } else if(phase==Phase::CloseInside) {
        begin=rearVan?.5f:.2f;end=rearVan?.8f:.63f;opening=false;
    } else if(phase==Phase::GetOut) {
        begin=bus?0:rearVan?.5f:.06f;
        end=bus?.3f:rearVan?.6f:.43f;
    } else if(phase==Phase::CloseOutside) {
        begin=.35f;end=.5f;opening=false;
    } else if(phase==Phase::PullOut)return 1;
    else if(phase==Phase::Done)return 0;
    else return previous;
    if(!std::isfinite(seconds)||seconds<=begin)return previous;
    if(seconds>=end)return opening?1.0f:0.0f;
    const float ratio=(seconds-begin)/(end-begin);
    return opening?std::max(previous,ratio):std::min(previous,1-ratio);
}

inline uint32_t storylandVehicleAssociation(const std::string& action,bool low,bool right,bool vcs=true) {
    if(action=="align")return right?85:(vcs?65:69);
    if(action=="open")return right?87:(vcs?67:71);
    if(action=="hijack")return right?(low?90:89):(vcs?(low?70:69):(low?74:73));
    if(action=="jacked")return (vcs?(right?59u:61u):(right?63u:65u))+(low?1u:0u);
    if(action=="enter")return right?(low?92:91):(vcs?(low?72:71):(low?76:75));
    if(action=="close_inside")return right?(low?94:93):(vcs?(low?74:73):(low?78:77));
    if(action=="exit")return right?(low?123:122):(low?83:82);
    if(action=="close_outside")return right?124:84;
    return UINT32_MAX;
}
// CAutomobile::ProcessOpenDoor, SLUS_215.90 0x1360E0..0x136668.
// These are clip seconds, not fractions of the complete entry sequence.
inline float storylandRetailDoorAnimation(float previous,uint32_t association,float seconds,bool vcs=true) {
    if(!vcs) {
        if(association==71)association=67;
        else if(association==73||association==74)association+=16;
        else if(association==77||association==78)association-=4;
    } else if(association==69||association==70)association+=20;
    float begin=0,end=0;bool opening=true;
    if(association==63||association==67||association==87){begin=.41f;end=.89f;}
    else if(association==73||association==74||association==93||association==94){begin=.2f;end=.63f;opening=false;}
    else if(association==82||association==83||association==122||association==123){begin=.06f;end=.43f;}
    else if(association==84||association==124){begin=.35f;end=.5f;opening=false;}
    else if(association==89||association==90)return 1;
    else return previous;
    if(seconds<=begin)return previous;
    if(seconds>=end)return opening?1.0f:0.0f;
    const float value=(seconds-begin)/(end-begin);
    return opening?std::max(previous,value):std::min(previous,1-value);
}
