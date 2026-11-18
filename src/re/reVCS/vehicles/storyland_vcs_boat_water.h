#pragma once

#include "src/storyland/storyland_vehicle_occupants.h"
#include <algorithm>
#include <array>
#include <cmath>

struct StorylandBoatWaterSample {
    bool available=false;
    float height=0;
    std::array<float,3> normal{0,0,1};
};

struct StorylandVcsBoatWaterContact {
    bool waterAvailable=false,hasBuoyancy=false,strongContact=false;
    std::array<float,3> force{};
    float forceSquared=0,thresholdSquared=0;
    float submergedVolume=0;
    bool inverted=false;
    bool drowningDriver=false;
};

template<class SampleWater>
StorylandVcsBoatWaterContact storylandEvaluateVcsBoatWater(
    const StorylandVehicleDefinition& definition,const StorylandVehicleMatrix& frame,
    const std::array<float,3>& nativeLinearVelocity,
    const std::array<float,3>& nativeAngularVelocity,float physicalMass,float nativeStep,SampleWater sampleWater) {
    StorylandVcsBoatWaterContact result;
    const auto& hull=definition.boatBuoyancy;
    if(!definition.vcsModelLayout||!hull.valid||
        (definition.nativeType!=1&&definition.nativeType!=2)||
        !std::isfinite(nativeStep)||nativeStep<=0||
        !std::isfinite(physicalMass)||physicalMass<=0||
        !std::isfinite(definition.suspensionDamping)||definition.suspensionDamping<0||
        !std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);})||
        !std::all_of(nativeLinearVelocity.begin(),nativeLinearVelocity.end(),
            [](float value){return std::isfinite(value);})||
        !std::all_of(nativeAngularVelocity.begin(),nativeAngularVelocity.end(),
            [](float value){return std::isfinite(value);}))return result;
    const auto center=sampleWater(frame[12],frame[13]);
    if(!center.available||!std::isfinite(center.height))return result;
    result.waterAvailable=true;
    const float height=hull.maximum[2]-hull.minimum[2];
    if(!(height>0)||!std::isfinite(height))return result;
    const float normalization=1.0f/(9.0f*height);
    const float halfX=(hull.maximum[0]-hull.minimum[0])*.5f;
    const float halfY=(hull.maximum[1]-hull.minimum[1])*.5f;
    for(size_t x=0;x<3;++x)for(size_t y=0;y<3;++y) {
        const float localX=hull.minimum[0]+float(x)*halfX;
        const float localY=hull.minimum[1]+float(y)*halfY;
        const std::array<float,3> arm{
            frame[0]*localX+frame[4]*localY,
            frame[1]*localX+frame[5]*localY,
            frame[2]*localX+frame[6]*localY};
        const auto water=sampleWater(frame[12]+arm[0],frame[13]+arm[1]);
        if(!water.available||!std::isfinite(water.height)||
            !std::all_of(water.normal.begin(),water.normal.end(),
                [](float value){return std::isfinite(value);}))continue;
        const float localWaterZ=water.height-(frame[14]+arm[2]);
        if(localWaterZ<hull.minimum[2])continue;
        const float slice=std::fabs(std::min(localWaterZ,hull.maximum[2])-hull.minimum[2])-
            (1.0f-hull.sliceWeights[x+y*3]);
        if(slice<0)continue;
        result.hasBuoyancy=true;
        result.submergedVolume+=slice;
        const std::array<float,3> normal{
            water.normal[0]/3.0f,water.normal[1]/3.0f,(water.normal[2]+2.0f)/3.0f};
        const std::array<float,3> velocity{
            nativeLinearVelocity[0]+nativeAngularVelocity[1]*arm[2]-nativeAngularVelocity[2]*arm[1],
            nativeLinearVelocity[1]+nativeAngularVelocity[2]*arm[0]-nativeAngularVelocity[0]*arm[2],
            nativeLinearVelocity[2]+nativeAngularVelocity[0]*arm[1]-nativeAngularVelocity[1]*arm[0]};
        const float normalSpeed=velocity[0]*normal[0]+velocity[1]*normal[1]+velocity[2]*normal[2];
        const float damping=std::max(0.0f,1.0f-definition.suspensionDamping*normalSpeed);
        // Native 369EE8 uses linear slices in the initialized VCS buoyancy state.
        const float amplitude=normalization*slice*hull.coefficient*nativeStep*damping;
        for(size_t axis=0;axis<3;++axis)result.force[axis]+=normal[axis]*amplitude;
    }
    result.submergedVolume*=normalization;
    result.forceSquared=result.force[0]*result.force[0]+result.force[1]*result.force[1]+
        result.force[2]*result.force[2];
    // Native 1A06F0 loads float bits 0x3A51B718.
    constexpr float contactCoefficient=0x1.a36e30p-11f;
    const float threshold=physicalMass*(nativeStep*contactCoefficient);
    result.thresholdSquared=threshold*threshold;
    // CBoat::ProcessControl 1A0730 and 1A07D4 both use strict comparisons.
    result.strongContact=result.hasBuoyancy&&std::isfinite(result.forceSquared)&&
        result.forceSquared>result.thresholdSquared;
    result.inverted=result.strongContact&&frame[10]<-.6f;
    result.drowningDriver=result.inverted&&definition.nativeType!=2;
    return result;
}
