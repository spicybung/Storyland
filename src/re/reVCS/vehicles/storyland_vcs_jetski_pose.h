#pragma once

#include "src/re/reVCS/vehicles/storyland_vcs_jetski_driving.h"
#include "src/re/reVCS/vehicles/storyland_vcs_jetski_association.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace storyland_vcs_jetski_pose {

namespace Driving = storyland_vcs_jetski_driving;
namespace Animation = storyland_vcs_jetski_association;

inline constexpr std::array<int32_t,10> Ids{187,189,190,191,192,201,202,203,210,211};

struct Metadata {
    bool available = false;
    std::array<Animation::Clip,Ids.size()> clips{};
};

struct Layer {
    int32_t animation = -1;
    float time = 0;
    float weight = 0;
    uint32_t flags = 0;
};

struct Sample {
    bool available = false;
    std::array<Layer,Ids.size()> layers{};
    uint32_t count = 0;
};

struct State {
    int32_t driver = -1;
    uint32_t frame = 0;
    bool initialised = false;
    bool frameValid = false;
    std::array<Animation::State,Ids.size()> associations{};
    Sample sample;
};

inline size_t index(int32_t animation) {
    for(size_t item=0;item<Ids.size();++item)
        if(Ids[item]==animation)return item;
    return Ids.size();
}

inline bool valid(const Metadata& metadata) {
    if(!metadata.available)return false;
    for(const auto& clip:metadata.clips)
        if(!Animation::valid(clip)||clip.duration>600)return false;
    return true;
}

inline Driving::Association drivingAssociation(const Animation::State& state) {
    return {state.present,state.weight,state.blendDelta};
}

inline Driving::Associations drivingAssociations(const State& state) {
    return {drivingAssociation(state.associations[index(210)]),
        drivingAssociation(state.associations[index(211)]),
        drivingAssociation(state.associations[index(201)]),
        drivingAssociation(state.associations[index(202)]),
        drivingAssociation(state.associations[index(203)])};
}

inline bool blend(const Metadata& metadata,int32_t animation,float rate,State& state) {
    const size_t selected=index(animation);
    if(selected==Ids.size()||!valid(metadata))return false;
    auto result=state;
    bool otherFading=false;
    for(size_t item=0;item<Ids.size();++item)if(item!=selected) {
        bool fading=false;
        if(!Animation::fadeOther(metadata.clips[selected].templateFlags,rate,
            result.associations[item],fading))return false;
        otherFading=otherFading||fading;
    }
    auto& association=result.associations[selected];
    const bool updated=association.present?
        Animation::reblend(metadata.clips[selected],rate,association):
        Animation::initialise(metadata.clips[selected],rate,otherFading,nullptr,association);
    if(!updated)return false;
    state=result;
    return true;
}

inline bool heldAxis(const Metadata& metadata,const Driving::AxisSample& axis,State& state) {
    const size_t selected=index(axis.animation),opposite=index(axis.oppositeAnimation);
    if(selected==Ids.size()||opposite==Ids.size()||!std::isfinite(axis.weight)||
        !std::isfinite(axis.fraction)||axis.fraction<0||axis.fraction>1)return false;
    auto& association=state.associations[selected];
    if(!association.present) {
        if(!Animation::initialise(metadata.clips[selected],0,false,nullptr,association))return false;
    }
    association.weight=axis.weight;
    association.blendDelta=0;
    association.time=metadata.clips[selected].duration*axis.fraction;
    association.timeAdvance=0;
    if(axis.stopAnimationClock)association.flags&=~Animation::Running;
    if(state.associations[opposite].present)state.associations[opposite].weight=0;
    return Animation::valid(association);
}

inline bool advance(const Metadata& metadata,int32_t driver,uint32_t frame,
    Driving::DrivingInput input,const Driving::ControlInput& controls,
    std::array<float,4>& axes,State& state,bool& knockOff) {
    knockOff=false;
    if(!valid(metadata)||driver<0||!Driving::valid(input))return false;
    for(float value:axes)if(!std::isfinite(value)||value< -1||value>1)return false;
    if(state.initialised&&state.driver==driver&&state.frameValid&&state.frame==frame)return true;
    auto result=state;
    if(!result.initialised||result.driver!=driver) {
        result={};result.driver=driver;result.initialised=true;
        if(!Animation::initialise(metadata.clips[0],100,false,nullptr,result.associations[0]))return false;
    }
    Driving::ControlState controlState{axes[0],axes[1]};
    if(input.player&&!Driving::advanceControls(controls,controlState))return false;
    input.controls=controlState;
    Driving::AssociationRequests requests;
    if(!Driving::associationRequests(input,drivingAssociations(result),requests))return false;
    if(requests.knockOff) {
        knockOff=true;
        return true;
    }
    if(requests.blendAnimation>=0&&!blend(metadata,requests.blendAnimation,requests.blendRate,result))return false;
    if(requests.fade210)result.associations[index(210)].blendDelta=requests.fadeDelta;
    if(requests.fade211)result.associations[index(211)].blendDelta=requests.fadeDelta;
    Driving::PoseState pose{axes[2],axes[3]};
    Driving::PoseOutput output;
    if(!Driving::advancePose(input,requests,drivingAssociations(result),pose,output)||
        !heldAxis(metadata,output.lateral,result)||
        (output.hasLongitudinal&&!heldAxis(metadata,output.longitudinal,result)))return false;
    float seconds=0;
    if(!Animation::animationSeconds(input.blendPredictionTime,seconds))return false;
    result.sample={};
    for(size_t item=0;item<Ids.size();++item) {
        auto& association=result.associations[item];
        Animation::Events events;
        if(!Animation::beforePose(seconds,association,events))return false;
        if(association.present&&association.weight>0) {
            result.sample.layers[result.sample.count++]={Ids[item],association.time,
                association.weight,association.flags};
        }
    }
    if(!result.sample.count)return false;
    result.sample.available=true;
    for(size_t item=0;item<Ids.size();++item) {
        Animation::Events events;
        if(!Animation::afterPose(metadata.clips[item],seconds,0,result.associations[item],events))return false;
    }
    result.frame=frame;result.frameValid=true;
    axes={controlState.longitudinal,controlState.lateral,pose.lateral,pose.longitudinal};
    state=result;
    return true;
}

}
