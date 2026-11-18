#pragma once

#include "src/re/reVCS/vehicles/storyland_vcs_hovercraft.h"

namespace StorylandVcsHovercraft {
struct WaterSample {
    bool available=false;
    float height=0;
};

struct WaterResult {
    bool waterAvailable=false,hasBuoyancy=false;
    float volume=0;
    Vector force{},point{};
};

struct WaterResponse {
    bool contact = false;
    bool drowning = false;
    bool damageDriver = false;
    float retention = 1;
    float driverDamage = 0;
};

// Original automobile138B68 consumer for a native hover-bit40 model.
inline bool waterResponse(const WaterResult& water, float mass, float nativeStep,
    float upZ, bool alternateGravity, int driverObjective, WaterResponse& result) {
    result = {};
    if (!std::isfinite(mass) || mass <= 0 || !std::isfinite(nativeStep) ||
        nativeStep < 0 || !std::isfinite(upZ) || !finite(water.force) ||
        !finite(water.point)) return false;
    if (!water.hasBuoyancy) return true;
    const float gravity = alternateGravity ? .0026666666381061077f : .00800000037997961f;
    const float denominator = mass * (std::max(nativeStep, .5f) * gravity);
    const float ratio = water.force[2] / denominator;
    result.retention = std::pow(1.0f - ratio * .05000000074505806f, nativeStep);
    if (!std::isfinite(result.retention)) return false;
    result.contact = true;
    result.drowning = upZ < -.5f;
    result.damageDriver = result.drowning && driverObjective >= 0 && driverObjective != 16;
    if (result.damageDriver) result.driverDamage = nativeStep;
    return true;
}

// Original generic CBuoyancy369450/369DB0/369950, initialized linear slices.
template<class SampleWater>
bool waterForces(const Vector& minimum,const Vector& maximum,const Frame& frame,
    const Vector& position,const Vector& nativeVelocity,float mass,float coefficient,
    float nativeStep,SampleWater sampleWater,WaterResult& result) {
    result={};
    if(!finite(minimum)||!finite(maximum)||!finite(frame.right)||!finite(frame.forward)||
        !finite(frame.up)||!finite(position)||!finite(nativeVelocity)||!std::isfinite(mass)||
        mass<=0||!std::isfinite(coefficient)||coefficient<0||!std::isfinite(nativeStep)||nativeStep<0)
        return false;
    for(size_t axis=0;axis<3;++axis)if(!(maximum[axis]>minimum[axis]))return false;
    const auto centre=sampleWater(position[0],position[1],position[2]);
    if(!centre.available)return true;
    if(!std::isfinite(centre.height))return false;
    result.waterAvailable=true;
    const Vector half{(maximum[0]-minimum[0])*.5f,(maximum[1]-minimum[1])*.5f,
        (maximum[2]-minimum[2])*.5f};
    const float largest=std::max({half[0],half[1],half[2]});
    const Vector ratio{half[0]/largest,half[1]/largest,half[2]/largest};
    Vector localPoint{};
    float count=1;
    unsigned xCount=0;
    for(float x=minimum[0];x<=maximum[0];x+=half[0]) {
        if(++xCount>4)return false;
        unsigned yCount=0;
        for(float y=minimum[1];y<=maximum[1];y+=half[1]) {
            if(++yCount>4)return false;
            const auto arm=transform(frame,{x,y,0});
            const auto water=sampleWater(position[0]+arm[0],position[1]+arm[1],position[2]);
            if(water.available&&!std::isfinite(water.height))return false;
            // The original per-slice query leaves its initialized zero on failure.
            float localZ=(water.available?water.height:0)-(position[2]+arm[2]);
            if(localZ<minimum[2])continue;
            localZ=std::min(localZ,maximum[2]);
            const float slice=std::fabs(localZ-minimum[2]);
            result.volume+=slice;
            const Vector scaled{x*ratio[0],y*ratio[1],(localZ+minimum[2])*(ratio[2]*.5f)};
            const float inverse=1/count,previous=1-inverse;
            for(size_t axis=0;axis<3;++axis)
                localPoint[axis]=previous*localPoint[axis]+slice*(inverse*scaled[axis]);
            result.hasBuoyancy=true;
            count+=1;
        }
    }
    result.volume/=(maximum[2]-minimum[2])*9;
    if(result.hasBuoyancy) {
        result.point=transform(frame,localPoint);
        result.force[2]=(result.volume*coefficient)*nativeStep;
        const float verticalMomentum=nativeVelocity[2]*mass;
        if(result.force[2]*4<verticalMomentum)
            result.force[2]=std::max(result.force[2]-verticalMomentum,0.0f);
    }
    return finite(result.point)&&finite(result.force)&&std::isfinite(result.volume);
}
}
