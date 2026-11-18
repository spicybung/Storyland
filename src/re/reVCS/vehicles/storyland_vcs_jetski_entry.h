#pragma once
#include "src/storyland/storyland_motorcycle_door.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_animation_root.h"
#include "src/re/reVCS/vehicles/storyland_vcs_vehicle_room.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

constexpr uint32_t StorylandVcsJetSkiAnimationGroup=3;

inline bool storylandReadVcsJetSkiEntryRoots(const std::vector<uint8_t>& bytes,
    std::array<std::array<float,3>,2>& result) {
    std::array<std::array<float,3>,2> roots{};
    if(!storylandReadVcsAnimationRoot(bytes,StorylandVcsJetSkiAnimationGroup,194,roots[0])||
        !storylandReadVcsAnimationRoot(bytes,StorylandVcsJetSkiAnimationGroup,208,roots[1]))return false;
    result=roots;
    return true;
}

inline uint32_t storylandVcsJetSkiAssociation(std::string_view action,bool right,
    bool passenger,bool strongWaterContact,bool highAlign) {
    if(action=="align")return (right?85u:65u)+(highAlign?1u:0u);
    if(action=="enter")return right?209u:208u;
    if(action=="exit")return strongWaterContact?(right?213u:212u):(right?199u:198u);
    if(action=="sit")return passenger?204u:187u;
    return UINT32_MAX;
}

inline bool storylandVcsJetSkiDoorPosition(const std::array<float,16>& frame,
    const std::array<float,3>& root,const std::array<float,3>& seat,
    const std::array<float,3>& collisionMaximum,bool rightHandSide,
    std::array<float,3>& result) {
    return storylandMotorcycleDoorPosition(frame,root,seat,collisionMaximum,rightHandSide,result);
}

inline bool storylandVcsJetSkiHighAlign(float doorEntityZ,float pedestrianEntityZ) {
    return std::isfinite(doorEntityZ)&&std::isfinite(pedestrianEntityZ)&&
        doorEntityZ-pedestrianEntityZ>4.4f;
}

using StorylandVcsJetSkiRoomQuery=StorylandVcsVehicleRoomQuery;

inline bool storylandVcsJetSkiSeekReached(float x,float y,float doorX,float doorY) {
    const float dx=doorX-x,dy=doorY-y;
    return std::isfinite(dx)&&std::isfinite(dy)&&std::sqrt(dx*dx+dy*dy)<.5f;
}

inline bool storylandVcsJetSkiRoomSupport(float ceiling,float ground) {
    return storylandVcsVehicleRoomSupport(ceiling,ground);
}

inline bool storylandVcsJetSkiRoomQuery(const std::array<float,16>& frame,
    const std::array<float,3>& seat,const std::array<float,3>& door,bool rightHandSide,
    StorylandVcsJetSkiRoomQuery& result) {
    return storylandVcsVehicleRoomQuery(frame,seat,door,rightHandSide,result);
}
