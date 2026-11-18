#pragma once

#include "src/storyland/storyland_visibility.h"
#include "src/graphics/storyland_flow_deformation.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

enum class StorylandMeshEffect { Off, Bouncy, Jelly, Melt, CurlNoise, HarmonicCage };
enum class StorylandMeshCategory : size_t { Cars, Characters, World, Weapons, None };

struct StorylandMeshEffects {
    struct Setting {
        StorylandMeshEffect mode=StorylandMeshEffect::Off;
        float startedAt=0;
    };
    std::array<Setting,4> categories{};
    float strength=.65f;

    bool spatial(StorylandMeshCategory category) const {
        const size_t index=size_t(category);
        if(index>=categories.size()||strength<=0.0f)return false;
        const auto mode=categories[index].mode;
        return mode==StorylandMeshEffect::CurlNoise||mode==StorylandMeshEffect::HarmonicCage;
    }

    bool enabled(StorylandMeshCategory category) const {
        const size_t index=size_t(category);
        return index<categories.size()&&categories[index].mode!=StorylandMeshEffect::Off&&strength>0;
    }
};

template<class Points>
std::array<float,6> storylandMeshEffectBounds(const Points& points) {
    if(points.empty())return {};
    const auto& first=points.front();
    std::array<float,6> bounds{first.x,first.y,first.z,first.x,first.y,first.z};
    for(const auto& point:points) {
        bounds[0]=std::min(bounds[0],point.x);bounds[3]=std::max(bounds[3],point.x);
        bounds[1]=std::min(bounds[1],point.y);bounds[4]=std::max(bounds[4],point.y);
        bounds[2]=std::min(bounds[2],point.z);bounds[5]=std::max(bounds[5],point.z);
    }
    return bounds;
}

inline std::array<float,16> storylandMeshEffectTransform(const StorylandMeshEffects& effects,
    StorylandMeshCategory category,const std::array<float,6>& bounds,float seconds,size_t seed=0,
    const float* placement=nullptr) {
    std::array<float,16> matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    if(!effects.enabled(category)||!std::isfinite(seconds)||bounds[5]-bounds[2]<=.001f)return matrix;
    const auto& setting=effects.categories[size_t(category)];
    float strength=std::clamp(effects.strength,0.0f,1.0f);
    if(category==StorylandMeshCategory::World) {
        // Keep deformation local even when a single mesh spans a whole sector.
        const float scale=placement?std::max({std::hypot(placement[0],placement[1],placement[2]),
            std::hypot(placement[4],placement[5],placement[6]),std::hypot(placement[8],placement[9],placement[10])}):1;
        const float span=scale*std::max({bounds[3]-bounds[0],bounds[4]-bounds[1],bounds[5]-bounds[2]});
        strength*=3.0f/std::max(3.0f,span);
    }
    const float phase=seconds*7+float(seed%29)*.61f;
    float sx=1,sy=1,sz=1,shearX=0,shearY=0;
    if(setting.mode==StorylandMeshEffect::Bouncy) {
        sz=1+strength*(category==StorylandMeshCategory::World?.12f:.22f)*std::sin(phase);
        if(category!=StorylandMeshCategory::World)sx=sy=1/std::sqrt(sz);
    } else if(setting.mode==StorylandMeshEffect::Jelly) {
        sx=1+.11f*strength*std::sin(phase*.87f);
        sy=1+.11f*strength*std::cos(phase*1.08f);
        sz=1/(sx*sy);
        shearX=.32f*strength*std::sin(phase);
        shearY=.28f*strength*std::cos(phase*.81f);
    } else if(setting.mode==StorylandMeshEffect::Melt) {
        const float elapsed=seconds>=setting.startedAt?seconds-setting.startedAt:seconds;
        const float melted=strength*(1-std::exp(-std::max(0.0f,elapsed)*.8f));
        sz=1-.88f*melted;sx=1+.53f*melted;sy=1+.40f*melted;
        shearX=.19f*melted;shearY=-.12f*melted;
    }
    // Use the bottom of the mesh as the pivot so squash stays on the floor.
    const float x=(bounds[0]+bounds[3])*.5f,y=(bounds[1]+bounds[4])*.5f,z=bounds[2];
    matrix[0]=sx;matrix[5]=sy;matrix[8]=shearX;matrix[9]=shearY;matrix[10]=sz;
    matrix[12]=x*(1-sx)-z*shearX;
    matrix[13]=y*(1-sy)-z*shearY;
    matrix[14]=z*(1-sz);
    return matrix;
}

template<class Point>
Point storylandMeshEffectVertex(const StorylandMeshEffects& effects,StorylandMeshCategory category,
    const std::array<float,6>& bounds,float seconds,size_t seed,Point point,const float* placement=nullptr) {
    if(!effects.spatial(category)||!std::isfinite(seconds))return point;
    const float width=bounds[3]-bounds[0],depth=bounds[4]-bounds[1],height=bounds[5]-bounds[2];
    if(width<.00001f||depth<.00001f||height<.00001f)return point;
    const auto& setting=effects.categories[size_t(category)];
    float weight=std::clamp(effects.strength,0.0f,1.0f);
    if(category==StorylandMeshCategory::World) {
        float scale=1.0f;
        if(placement)scale=std::max({std::hypot(placement[0],placement[1],placement[2]),
            std::hypot(placement[4],placement[5],placement[6]),std::hypot(placement[8],placement[9],placement[10])});
        weight*=std::min(1.0f,8.0f/(scale*std::max({width,depth,height})));
    }
    const float u=std::clamp((point.x-bounds[0])/width,0.0f,1.0f);
    const float v=std::clamp((point.y-bounds[1])/depth,0.0f,1.0f);
    const float w=std::clamp((point.z-bounds[2])/height,0.0f,1.0f);
    const float phase=float(seed%1009)*.081f;
    if(setting.mode==StorylandMeshEffect::CurlNoise) {
        const float x=(u-.5f)*4.0f,y=(v-.5f)*4.0f,z=w*3.0f;
        auto a=storylandCurlNoise(x,y,z,seconds,phase);
        const auto b=storylandCurlNoise(x*2.1f,y*2.1f,z*2.1f,seconds*1.3f,phase+1.31f);
        const float fade=.25f+.75f*std::sin(w*1.570796327f);
        const float magnitude=weight*std::min({width,depth,height,4.0f})*.30f*fade;
        point.x+=magnitude*(a.x+b.x*.32f);
        point.y+=magnitude*(a.y+b.y*.32f);
        point.z+=magnitude*(a.z+b.z*.32f)*.65f;
    } else if(setting.mode==StorylandMeshEffect::HarmonicCage) {
        // Trilinear eight-corner cage. Individual corners follow two harmonics.
        // The bottom corners are anchored vertically to preserve ground contact.
        const float magnitude=weight*std::min({width,depth,height,5.0f})*.18f;
        for(int iz=0;iz<2;++iz)for(int iy=0;iy<2;++iy)for(int ix=0;ix<2;++ix) {
            const float blend=(ix?u:1.0f-u)*(iy?v:1.0f-v)*(iz?w:1.0f-w);
            const float corner=float(ix+2*iy+4*iz);
            const float t=seconds*2.1f+phase+corner*1.71f;
            const float h=std::sin(t)+.34f*std::sin(t*1.97f+corner);
            point.x+=magnitude*blend*h;
            point.y+=magnitude*blend*(std::cos(t*.83f)+.27f*std::sin(t*2.24f));
            point.z+=magnitude*blend*(iz?std::sin(t*1.31f)+.25f*std::cos(t*2.49f):0.0f);
        }
    }
    return point;
}

inline std::array<float,6> storylandMeshEffectBounds(const StorylandMeshBounds& bounds) {
    return {float(bounds.minimum[0]),float(bounds.minimum[1]),float(bounds.minimum[2]),
        float(bounds.maximum[0]),float(bounds.maximum[1]),float(bounds.maximum[2])};
}
