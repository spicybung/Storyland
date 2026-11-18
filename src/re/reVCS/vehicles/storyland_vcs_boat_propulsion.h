#pragma once

#include "src/storyland/storyland_vehicle_occupants.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

inline bool storylandVcsBoatPropulsionAvailable(const StorylandVehicleDefinition& definition) {
    return definition.vcsModelLayout&&definition.hasNativeHandling&&
        definition.hasNativeBoatHandling&&definition.collision&&
        (definition.nativeType==1||definition.nativeType==2)&&definition.modelId!=278;
}

inline float storylandVcsBoatThrottle(uint8_t accelerator,uint8_t brake,bool handbrake,
    float& reverseInput) {
    // Original CBoat 19F774..19F89C reads the ordinary accelerator/brake pad fields.
    if(accelerator&&brake&&!handbrake) {reverseInput=0;return 0;}
    if(handbrake)reverseInput=0;
    else reverseInput=std::clamp(reverseInput+(float(brake)*(1.0f/255.0f)-reverseInput)*.1f,0.0f,1.0f);
    if(reverseInput<.05f) {reverseInput=0;return float(accelerator)*(1.0f/255.0f);}
    return -reverseInput*.3f;
}

inline std::array<float,3> storylandVcsBoatPropellerPoint(
    const StorylandVehicleDefinition& definition,const StorylandVehicleMatrix& frame) {
    if(!definition.collision)return {frame[12],frame[13],frame[14]};
    return storylandVehicleTransformPoint(frame,{0,
        definition.collision->minimum.y*definition.boatThrustY,
        definition.collision->minimum.z*definition.boatThrustZ});
}

inline std::array<float,3> storylandVcsBoatPropellerForce(
    const StorylandVehicleDefinition& definition,const StorylandVehicleMatrix& frame,
    const std::array<float,3>& nativeVelocity,float throttle,float steeringRadians,
    bool handbrake,float physicalMass,float nativeStep,
    bool propellerWaterAvailable,float propellerWaterHeight,
    bool centerWaterAvailable,float centerWaterHeight) {
    std::array<float,3> result{};
    if(!storylandVcsBoatPropulsionAvailable(definition)||!std::isfinite(throttle)||
        !std::isfinite(steeringRadians)||!std::isfinite(physicalMass)||physicalMass<=0||
        !std::isfinite(nativeStep)||nativeStep<=0||!propellerWaterAvailable||
        !std::isfinite(propellerWaterHeight)||
        !std::all_of(nativeVelocity.begin(),nativeVelocity.end(),[](float value){return std::isfinite(value);})||
        !std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);}))return result;
    if(!(std::fabs(throttle)>.05f)&&!(std::hypot(nativeVelocity[0],nativeVelocity[1])>.01f))return result;
    const auto propeller=storylandVcsBoatPropellerPoint(definition,frame);
    if(!(propeller[2]-.5f<propellerWaterHeight))return result;
    float depth=propellerWaterHeight-(propeller[2]-.5f);
    depth=depth>1?1:depth*depth;
    const float forward=(nativeVelocity[0]*frame[4]+nativeVelocity[1]*frame[5])+nativeVelocity[2]*frame[6];
    float speedSteering=forward*definition.tractionBias;
    if(definition.nativeType!=2&&handbrake)speedSteering*=.5f;
    speedSteering=std::clamp(1.0f-speedSteering,0.0f,1.0f);
    const float sine=-std::sin(-steeringRadians*speedSteering);
    const float cosine=std::cos(-steeringRadians*speedSteering);
    const float downward=-std::fabs(steeringRadians);
    float reverseBraking=1;
    const float speedSquared=(nativeVelocity[0]*nativeVelocity[0]+nativeVelocity[1]*nativeVelocity[1])+
        nativeVelocity[2]*nativeVelocity[2];
    if(throttle<0&&speedSquared>.001f&&forward>0)
        reverseBraking=definition.brakeDeceleration*2500.0f;
    // Original 1A1868..1A1924 supplies force, without a road transmission or speed clamp.
    for(size_t axis=0;axis<3;++axis) {
        const float direction=(frame[axis]*sine+frame[4+axis]*cosine)+frame[8+axis]*downward;
        float force=(reverseBraking*depth)*direction;
        force=std::fabs(throttle)*force;
        force*=40.0f;
        force=definition.engineAcceleration*force;
        result[axis]=physicalMass*force;
    }
    if(result[2]>.8f) {
        if(!centerWaterAvailable||!std::isfinite(centerWaterHeight)||!(frame[14]<centerWaterHeight)) {
            result[0]*=.5f;result[1]*=.5f;result[2]=0;
        }
    } else if(result[2]>.2f) {
        const float residual=1.2f-result[2];
        result[2]=residual*residual+.2f;
    }
    for(float& force:result)force=(throttle<0?-force:force)*nativeStep;
    return result;
}

inline std::array<float,3> storylandVcsBoatResistedVelocity(
    const StorylandVehicleDefinition& definition,const StorylandVehicleMatrix& frame,
    const std::array<float,3>& nativeVelocity,float submergedVolume,float physicalMass,
    float longitudinalLean,bool handbrake,float nativeStep) {
    if(!storylandVcsBoatPropulsionAvailable(definition)||!std::isfinite(submergedVolume)||submergedVolume<0||
        !std::isfinite(physicalMass)||physicalMass<=0||!std::isfinite(longitudinalLean)||
        !std::isfinite(nativeStep)||nativeStep<=0||
        !std::all_of(nativeVelocity.begin(),nativeVelocity.end(),[](float value){return std::isfinite(value);})||
        !std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);}))return nativeVelocity;
    // Original CBoat 1A3650 uses authored per-axis movement resistance and native velocity units.
    const float forward=(nativeVelocity[0]*frame[4]+nativeVelocity[1]*frame[5])+nativeVelocity[2]*frame[6];
    float resistance=definition.suspensionForce*(submergedVolume*.001f);
    resistance*=submergedVolume;
    resistance*=physicalMass;
    const float reciprocal=1.0f/std::fabs((forward*forward+.05f)*resistance+1.0f);
    float side=definition.boatMoveResistance[0];
    if(definition.nativeType==2) {
        side-=handbrake?-.2f:longitudinalLean*.2f;
        side=std::clamp(side,.001f,.99f);
    }
    side+=(1.0f-side)*(1.0f-frame[10]);
    side=std::clamp(side,.001f,.99f);
    float vertical=definition.boatMoveResistance[2];
    vertical+=(1.0f-vertical)*std::fabs(frame[6]);
    vertical=std::clamp(vertical,.001f,.99f);
    const float exponent=nativeStep*.5f;
    const std::array<float,3> factors{
        std::pow(reciprocal*side,exponent),
        std::pow(reciprocal*definition.boatMoveResistance[1],exponent),
        std::pow(reciprocal*vertical,exponent)};
    std::array<float,3> local{};
    for(size_t axis=0;axis<3;++axis) {
        const size_t column=axis*4;
        local[axis]=((nativeVelocity[0]*frame[column]+nativeVelocity[1]*frame[column+1])+
            nativeVelocity[2]*frame[column+2])*factors[axis];
    }
    std::array<float,3> result{};
    for(size_t axis=0;axis<3;++axis)
        result[axis]=(local[0]*frame[axis]+local[1]*frame[4+axis])+local[2]*frame[8+axis];
    result[2]*=factors[2];
    return result;
}
