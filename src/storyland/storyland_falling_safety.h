#pragma once
#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

inline bool storylandNativeEntityHasFallen(bool vcs,float z) {
    return z<(vcs?-250.0f:-100.0f);
}
inline bool storylandNativeFallenScanDue(uint32_t frame,bool vehicle) {
    return (frame&7u)==(vehicle?5u:1u);
}
inline float storylandFallenPathFloat(double value) {
    float rounded=float(value);
    if((value>0&&double(rounded)>value)||(value<0&&double(rounded)<value))
        rounded=std::nextafter(rounded,0.0f);
    return std::fabs(rounded)<std::numeric_limits<float>::min()?std::copysign(0.0f,rounded):rounded;
}
inline uint32_t storylandFindNativeFallenNode(const StorylandNativePaths& paths,
    const StorylandNativePoint& point,bool vehicle,const std::set<uint32_t>* marked=nullptr) {
    if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))return UINT32_MAX;
    const int side=paths.vcs?50:100;
    const auto& grid=paths.vcs?paths.vcsSearchGrid:paths.lcsSearchGrid;
    if(grid.size()!=size_t(side*side))return UINT32_MAX;
    const auto cell=[&](float coordinate,float offset,float bias) {
        const float sum=storylandFallenPathFloat(double(coordinate)+offset);
        const float scaled=storylandFallenPathFloat(double(sum)*(paths.vcs?double(0.0125f):double(0.025f)));
        return int(std::clamp(storylandFallenPathFloat(double(scaled)+bias),0.0f,float(side-1)));
    };
    for(float radius:{100.0f,999999.875f}) {
        float best=storylandFallenPathFloat(double(radius)*radius);
        uint32_t result=UINT32_MAX;
        const int minX=cell(point.x,-radius,paths.vcs?30.0f:50.0f);
        const int maxX=cell(point.x,radius,paths.vcs?30.0f:50.0f);
        const int minY=cell(point.y,-radius,paths.vcs?25.0f:50.0f);
        const int maxY=cell(point.y,radius,paths.vcs?25.0f:50.0f);
        for(int y=minY;y<=maxY;++y)for(int x=minX;x<=maxX;++x)
            for(uint16_t index:grid[size_t(x)*side+size_t(y)][vehicle?0:1]) {
                if(index>=paths.nodes.size())continue;
                const auto& node=paths.nodes[index];
                if(node.vehicle!=vehicle||(node.flags&0x100)||
                    (paths.vcs&&(node.disabled||(node.flags&0x220)||(marked&&marked->count(index)))))continue;
                const float dx=std::fabs(storylandFallenPathFloat(double(node.position.x)-point.x));
                const float dy=std::fabs(storylandFallenPathFloat(double(node.position.y)-point.y));
                const float dz=std::fabs(storylandFallenPathFloat(double(node.position.z)-point.z));
                const float distance=storylandFallenPathFloat(double(storylandFallenPathFloat(double(dx)+dy))+
                    storylandFallenPathFloat(double(dz)*3.0));
                if(distance<best) {
                    best=distance;result=index;
                    if(distance<0.010000000707805157f)return result;
                }
            }
        if(result!=UINT32_MAX)return result;
    }
    return UINT32_MAX;
}
