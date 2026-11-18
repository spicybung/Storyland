#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/leeds/leeds_model.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include <algorithm>
#include <cmath>

// CPedModelInfo::m_pColNodeInfos and ConvertPedNode2BoneTag, LCS.
// These spheres follow decoded pose bones, including the authored limb offsets.
inline bool storylandLcsPedHitSpheres(const std::vector<StorylandModelBone>& bones,
    const std::vector<std::array<float,16>>& pose,const StorylandVehicleMatrix& root,
    std::vector<StorylandMissionEnvironment::CharacterHitSphere>& output,bool seated=false) {
    struct Node {uint32_t bone;uint8_t piece;float x,z,radius;};
    static constexpr Node nodes[]={{5,6,0,.05f,.15f},{3,0,0,.15f,.2f},{3,0,0,-.05f,.25f},
        {3,1,0,-.25f,.25f},{32,2,.03f,-.05f,.16f},{22,3,-.03f,-.05f,.16f},
        {42,4,0,.15f,.2f},{52,5,0,.15f,.2f},{43,4,0,.15f,.15f},{53,5,0,.15f,.15f}};
    output.clear();
    if(bones.size()!=pose.size()||!std::all_of(root.begin(),root.end(),[](float f){return std::isfinite(f);}))return false;
    const float scale=std::hypot(root[0],root[1],root[2]);
    if(!(scale>0))return false;
    for(const auto& node:nodes) {
        if(seated&&node.piece!=6)continue;
        const auto bone=std::find_if(bones.begin(),bones.end(),[&](const auto& value){return value.boneId==node.bone;});
        if(bone==bones.end())continue;
        const auto& frame=pose[size_t(bone-bones.begin())];
        StorylandMissionEnvironment::CharacterHitSphere sphere;
        sphere.center=storylandVehicleTransformPoint(root,{frame[12]+node.x,frame[13],frame[14]+(seated?.1f:node.z)});
        sphere.radius=(seated?.2f:node.radius)*scale;sphere.piece=node.piece;
        if(!std::all_of(sphere.center.begin(),sphere.center.end(),[](float f){return std::isfinite(f);}))continue;
        output.push_back(sphere);
    }
    return !output.empty();
}

inline StorylandVehicleMatrix storylandLcsPedGroundFrame(const std::vector<StorylandModelPoint>& points,
    const std::array<float,3>& position,float heading,float modelForward,float height=1.75f) {
    if(points.empty())return {};
    auto minimum=points.front(),maximum=minimum;
    for(const auto& point:points) {
        minimum.x=std::min(minimum.x,point.x);maximum.x=std::max(maximum.x,point.x);
        minimum.y=std::min(minimum.y,point.y);maximum.y=std::max(maximum.y,point.y);
        minimum.z=std::min(minimum.z,point.z);maximum.z=std::max(maximum.z,point.z);
    }
    const float scale=std::clamp(height/std::max(.001f,std::max({maximum.x-minimum.x,
        maximum.y-minimum.y,maximum.z-minimum.z})),.001f,10.0f);
    const float angle=(heading+90-modelForward)*.0174532925199433f,c=std::cos(angle),s=std::sin(angle);
    const float x=-(minimum.x+maximum.x)*.5f*scale,y=-(minimum.y+maximum.y)*.5f*scale;
    return {c*scale,s*scale,0,0,-s*scale,c*scale,0,0,0,0,scale,0,
        position[0]+c*x-s*y,position[1]+s*x+c*y,position[2]-minimum.z*scale,1};
}

inline bool storylandLcsPedCriticalHit(int32_t weapon,uint8_t piece,bool enabled,bool player,uint32_t random) {
    // Native InflictDamage never critically dismembers the player or torso.
    if(!enabled||player||piece<2||piece>6)return false;
    if(weapon>=26&&weapon<=29)return true; // M4, Ruger, sniper, laser-scope.
    if(weapon==17||(weapon>=19&&weapon<=25)||weapon==32||weapon==33||weapon==42)
        return (random&(weapon==19?7u:15u))==0;
    return false;
}
