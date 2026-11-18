#pragma once

#include "src/storyland/storyland_vehicle_effects.h"
#include "src/re/reVCS/runtime/storyland_vcs_blood.h"

template<class Emit>
void storylandVisitRoadSkid(const StorylandRoadSkid& skid,int area,Emit emit,bool textured=false) {
    if(skid.area!=area||skid.remaining<=0||!std::isfinite(skid.width)||skid.width<=0)return;
    const float dx=skid.end.x-skid.start.x,dy=skid.end.y-skid.start.y,dz=skid.end.z-skid.start.z;
    StorylandNativePoint side{dy*skid.normal.z-dz*skid.normal.y,
        dz*skid.normal.x-dx*skid.normal.z,dx*skid.normal.y-dy*skid.normal.x};
    const float length=std::hypot(side.x,side.y,side.z);
    if(!std::isfinite(length)||length<.0001f)return;
    const float scale=skid.width*.5f/length;
    side.x*=scale;side.y*=scale;side.z*=scale;
    const std::array<float,4> color{skid.blood?StorylandVcsBlood::SkidRed:.025f,
        skid.blood?StorylandVcsBlood::SkidGreen:.025f,skid.blood?StorylandVcsBlood::SkidBlue:.025f,
        (skid.blood?StorylandVcsBlood::SkidAlpha:.6f)*std::clamp(skid.opacity,0.0f,1.0f)*
            std::clamp(skid.remaining/std::max(.001f,skid.fadeSeconds),0.0f,1.0f)};
    const auto point=[&](const StorylandNativePoint& center,float amount) {
        return StorylandNativePoint{center.x+side.x*amount,center.y+side.y*amount,center.z+side.z*amount};
    };
    const auto strip=[&](float left,float right) {
        emit(point(skid.start,left),color,std::array<float,2>{(left+1)*.5f,skid.textureStart});
        emit(point(skid.end,left),color,std::array<float,2>{(left+1)*.5f,skid.textureEnd});
        emit(point(skid.end,right),color,std::array<float,2>{(right+1)*.5f,skid.textureEnd});
        emit(point(skid.start,right),color,std::array<float,2>{(right+1)*.5f,skid.textureStart});
    };
    if(skid.blood&&!textured) {
        strip(-1.0f,-.45f);strip(-.3f,.3f);strip(.45f,1.0f);
    } else strip(-1.0f,1.0f);
}
