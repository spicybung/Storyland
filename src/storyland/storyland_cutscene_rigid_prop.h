#pragma once
#include "src/leeds/leeds_anim.h"
#include "src/leeds/leeds_model.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

inline std::string storylandCutsceneTrackName(std::string value) {
    const auto slash=value.find_last_of("/\\");
    if(slash!=std::string::npos)value.erase(0,slash+1);
    const auto dot=value.find_last_of('.');
    if(dot!=std::string::npos)value.erase(dot);
    for(char& c:value)c=char(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

inline const StorylandAnimTrack* storylandFindRigidPropTrack(
        const std::vector<StorylandAnimTrack>& tracks,const std::string& modelName) {
    const auto basename=storylandCutsceneTrackName(modelName);
    const StorylandAnimTrack* exact=nullptr;
    const StorylandAnimTrack* root=nullptr;
    const StorylandAnimTrack* only=nullptr;
    size_t count=0;
    for(const auto& track:tracks) {
        if(track.keys.empty())continue;
        ++count;
        only=&track;
        const auto candidate=storylandCutsceneTrackName(track.name);
        if(candidate==basename || (basename=="cssuitcase"&&candidate=="suitcase")) {
            if(!exact)exact=&track;
        }
        if(candidate=="root"||candidate=="root0"||candidate=="origin"||
            (track.boneId==0u&&track.parentIndex==UINT32_MAX)) {
            if(!root)root=&track;
        }
    }
    if(exact)return exact;
    if(root)return root;
    return count==1?only:nullptr;
}

inline std::array<float,4> storylandNormalizeCutsceneQuaternion(const StorylandAnimKey& key) {
    const float len=std::sqrt(key.qx*key.qx+key.qy*key.qy+key.qz*key.qz+key.qw*key.qw);
    if(!std::isfinite(len)||len<0.000001f)return {0,0,0,1};
    return {key.qx/len,key.qy/len,key.qz/len,key.qw/len};
}

inline StorylandAnimKey storylandSampleRigidTrack(const StorylandAnimTrack& track,float seconds) {
    const auto& keys=track.keys;
    if(keys.empty())return {};
    if(seconds<=keys.front().time)return keys.front();
    if(track.source != "ANPK IFP (file-order keys)" && seconds>=keys.back().time)return keys.back();
    auto next=storylandNextAnimKey(track,seconds);
    if(next==keys.end())return keys.back();
    const auto& a=*(next-1);
    const auto& b=*next;
    const float delta=b.time-a.time;
    const float t=delta>1.e-6f?std::clamp((seconds-a.time)/delta,0.f,1.f):0.f;
    const auto mix=[&](float x,float y){return x+(y-x)*t;};
    StorylandAnimKey result;
    result.tx=mix(a.tx,b.tx);result.ty=mix(a.ty,b.ty);result.tz=mix(a.tz,b.tz);
    result.sx=mix(a.sx,b.sx);result.sy=mix(a.sy,b.sy);result.sz=mix(a.sz,b.sz);
    auto qa=storylandNormalizeCutsceneQuaternion(a);
    auto qb=storylandNormalizeCutsceneQuaternion(b);
    float dot=0;
    for(int i=0;i<4;++i)dot+=qa[i]*qb[i];
    if(dot<0){for(float& q:qb)q=-q;dot=-dot;}
    float left=1-t,right=t;
    if(dot<0.9995f){
        const float angle=std::acos(std::clamp(dot,-1.f,1.f));
        const float sine=std::sin(angle);
        left=std::sin((1-t)*angle)/sine;right=std::sin(t*angle)/sine;
    }
    result.qx=qa[0]*left+qb[0]*right;
    result.qy=qa[1]*left+qb[1]*right;
    result.qz=qa[2]*left+qb[2]*right;
    result.qw=qa[3]*left+qb[3]*right;
    const auto normalized=storylandNormalizeCutsceneQuaternion(result);
    result.qx=normalized[0];result.qy=normalized[1];
    result.qz=normalized[2];result.qw=normalized[3];
    return result;
}

inline std::array<float,3> storylandRotateCutsceneVertex(
        std::array<float,3> value,const std::array<float,4>& rotation) {
    const float x=rotation[0],y=rotation[1],z=rotation[2],w=rotation[3];
    const std::array<float,3> qv{x,y,z};
    const std::array<float,3> cross{
        qv[1]*value[2]-qv[2]*value[1],
        qv[2]*value[0]-qv[0]*value[2],
        qv[0]*value[1]-qv[1]*value[0]};
    const std::array<float,3> secondary{
        qv[1]*cross[2]-qv[2]*cross[1],
        qv[2]*cross[0]-qv[0]*cross[2],
        qv[0]*cross[1]-qv[1]*cross[0]};
    for(int i=0;i<3;++i)value[i]+=2.0f*(cross[i]*w+secondary[i]);
    return value;
}

inline bool storylandSampleAuthoredRigidCutsceneProp(
        const StorylandAnimTrack& track,float seconds,
        const std::vector<StorylandModelPoint>& source,
        std::vector<StorylandModelPoint>& result,bool rootMotion) {
    if(track.keys.empty()||source.empty()||!std::isfinite(seconds))return false;
    const auto& initial=track.keys.front();
    const auto current=storylandSampleRigidTrack(track,seconds);
    const auto q0=storylandNormalizeCutsceneQuaternion(initial);
    const auto qt=storylandNormalizeCutsceneQuaternion(current);
    // R(t) * inverse(R(0)), so the prop retains its authored bind pose.
    const std::array<float,4> conjugate{-q0[0],-q0[1],-q0[2],q0[3]};
    const std::array<float,4> relative{
        qt[3]*conjugate[0]+qt[0]*conjugate[3]+qt[1]*conjugate[2]-qt[2]*conjugate[1],
        qt[3]*conjugate[1]-qt[0]*conjugate[2]+qt[1]*conjugate[3]+qt[2]*conjugate[0],
        qt[3]*conjugate[2]+qt[0]*conjugate[1]-qt[1]*conjugate[0]+qt[2]*conjugate[3],
        qt[3]*conjugate[3]-qt[0]*conjugate[0]-qt[1]*conjugate[1]-qt[2]*conjugate[2]};
    const bool rotation=(track.channelFlags&1u)!=0u||track.channelFlags==0;
    const bool translation=(track.channelFlags&2u)!=0u||track.channelFlags==0;
    const bool scale=(track.channelFlags&4u)!=0u||track.channelFlags==0;
    const float initialScale[3]={initial.sx,initial.sy,initial.sz};
    const float currentScale[3]={current.sx,current.sy,current.sz};
    const float initialPosition[3]={initial.tx,initial.ty,initial.tz};
    const float currentPosition[3]={current.tx,current.ty,current.tz};
    result.clear();result.reserve(source.size());
    for(const auto& point:source) {
        std::array<float,3> location{point.x,point.y,point.z};
        if(scale)for(int axis=0;axis<3;++axis) {
            if(!std::isfinite(currentScale[axis])||!std::isfinite(initialScale[axis])||
                std::abs(initialScale[axis])<0.000001f)return false;
            location[axis]*=currentScale[axis]/initialScale[axis];
        }
        if(rotation)location=storylandRotateCutsceneVertex(location,relative);
        if(translation)for(int axis=0;axis<3;++axis) {
            const float delta=currentPosition[axis]-initialPosition[axis];
            // CUT actors already receive the scene origin in the draw path.
            // Only an explicitly requested root-motion track contributes an
            // initial offset; otherwise preserve the model's starting pose.
            const float base=rootMotion&&std::abs(initialPosition[axis])<300.0f?
                initialPosition[axis]:0.0f;
            location[axis]+=delta+base;
        }
        if(!std::isfinite(location[0])||!std::isfinite(location[1])||!std::isfinite(location[2]))return false;
        StorylandModelPoint posed=point;
        posed.x=location[0];posed.y=location[1];posed.z=location[2];
        if(posed.hasNormal&&rotation){
            const auto normal=storylandRotateCutsceneVertex({posed.nx,posed.ny,posed.nz},relative);
            posed.nx=normal[0];posed.ny=normal[1];posed.nz=normal[2];
        }
        if(posed.hasPs2Normal&&rotation){
            const auto normal=storylandRotateCutsceneVertex({posed.ps2Nx,posed.ps2Ny,posed.ps2Nz},relative);
            posed.ps2Nx=normal[0];posed.ps2Ny=normal[1];posed.ps2Nz=normal[2];
        }
        result.push_back(posed);
    }
    return true;
}
