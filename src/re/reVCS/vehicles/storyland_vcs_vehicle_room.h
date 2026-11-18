#pragma once
#include <algorithm>
#include <array>
#include <cmath>

struct StorylandVcsVehicleRoomQuery {
    std::array<float,3> lineStart{},lineEnd{},sphereCenter{};
    float radius=.6f;
};

inline bool storylandVcsVehicleRoomSupport(float ceiling,float ground) {
    return std::isfinite(ceiling)&&std::isfinite(ground)&&(ceiling==0||ceiling>=ground);
}

inline bool storylandVcsVehicleRoomQuery(const std::array<float,16>& frame,
    const std::array<float,3>& seat,const std::array<float,3>& door,bool rightHandSide,
    StorylandVcsVehicleRoomQuery& result,const std::array<float,3>* additionalOffset=nullptr) {
    const auto finite=[](const auto& values) {
        return std::all_of(values.begin(),values.end(),[](float value){return std::isfinite(value);});
    };
    if(!finite(frame)||!finite(seat)||!finite(door)||(additionalOffset&&!finite(*additionalOffset)))return false;
    StorylandVcsVehicleRoomQuery query;
    const float seatX=rightHandSide?seat[0]:-seat[0];
    for(size_t axis=0;axis<3;++axis)
        query.lineStart[axis]=((frame[axis]*seatX+frame[4+axis]*seat[1])+
            frame[8+axis]*seat[2])+frame[12+axis];
    query.sphereCenter=door;
    if(additionalOffset) {
        auto offset=*additionalOffset;
        if(rightHandSide)offset[0]=-offset[0];
        for(size_t axis=0;axis<3;++axis)
            query.sphereCenter[axis]+=((frame[axis]*offset[0]+frame[4+axis]*offset[1])+
                frame[8+axis]*offset[2])+0.0f;
    }
    if(frame[10]<0) {
        query.lineStart[2]+=.5f;
        query.sphereCenter[2]+=.5f;
    }
    const std::array<float,3> delta{query.sphereCenter[0]-query.lineStart[0],
        query.sphereCenter[1]-query.lineStart[1],query.sphereCenter[2]-query.lineStart[2]};
    const float length=std::sqrt((delta[0]*delta[0]+delta[1]*delta[1])+delta[2]*delta[2]);
    if(!std::isfinite(length)||length<=0)return false;
    const float scale=(length+query.radius)/length;
    for(size_t axis=0;axis<3;++axis)
        query.lineEnd[axis]=query.lineStart[axis]+delta[axis]*scale;
    query.sphereCenter[2]+=.5f;
    if(!finite(query.lineEnd)||!finite(query.sphereCenter))return false;
    result=query;
    return true;
}
