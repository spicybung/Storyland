#pragma once
#include "src/re/reVCS/pedestrians/storyland_vcs_animation_root.h"
#include <algorithm>

struct StorylandVcsHelicopterDoorSelection {
    uint32_t group=0,association=71;
    bool omitSeatOffset=false;
};

inline StorylandVcsHelicopterDoorSelection storylandVcsHelicopterDoorSelection(
    uint32_t modelFlags,bool rear) {
    if(rear&&(modelFlags&1u))return {1,175,true};
    if(modelFlags&4u)return {0,72,false};
    if((modelFlags&0x4000u)&&!(modelFlags&0x200u))return {0,78,false};
    return {};
}

inline bool storylandVcsHelicopterDoorPosition(const std::array<float,16>& frame,
    const std::array<float,3>& root,const std::array<float,3>& dummy,
    float seatOffset,bool rightHandSide,std::array<float,3>& result) {
    const auto finite=[](const auto& values) {
        return std::all_of(values.begin(),values.end(),[](float value){return std::isfinite(value);});
    };
    if(!finite(frame)||!finite(root)||!finite(dummy)||!std::isfinite(seatOffset))return false;
    auto seat=dummy;
    auto offset=root;
    seat[0]+=seatOffset;
    if(rightHandSide)offset[0]=-offset[0];
    else seat[0]=-seat[0];
    const std::array<float,3> local{seat[0]-offset[0],seat[1]-offset[1],seat[2]-offset[2]};
    std::array<float,3> point{};
    for(size_t axis=0;axis<point.size();++axis)
        point[axis]=((frame[axis]*local[0]+frame[4+axis]*local[1])+frame[8+axis]*local[2])+frame[12+axis];
    if(!finite(point))return false;
    result=point;
    return true;
}

inline bool storylandVcsHelicopterSeekReached(float x,float y,float targetX,float targetY) {
    const float dx=targetX-x,dy=targetY-y;
    return std::isfinite(dx)&&std::isfinite(dy)&&std::sqrt(dx*dx+dy*dy)<.5f;
}

inline bool storylandVcsHelicopterPreferLeft(float x,float y,
    const std::array<float,3>& left,const std::array<float,3>& right) {
    const float leftX=left[0]-x,leftY=left[1]-y,rightX=right[0]-x,rightY=right[1]-y;
    return leftX*leftX+leftY*leftY<rightX*rightX+rightY*rightY;
}

inline int storylandVcsNearestPassengerSeat(float x,float y,
    const std::array<std::array<float,3>,3>& doors,uint8_t available) {
    int selected=-1;
    float nearest=0;
    for(size_t seat=0;seat<doors.size();++seat) {
        if(!(available&(1u<<seat)))continue;
        const float dx=doors[seat][0]-x,dy=doors[seat][1]-y;
        const float squared=dx*dx+dy*dy;
        if(!std::isfinite(squared))continue;
        if(selected<0||squared<nearest) {selected=int(seat);nearest=squared;}
    }
    return selected;
}
