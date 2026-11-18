#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// PS2 VCS 0x1C8960 and LCS 0x1B5320. Native velocity is per 50-Hz step;
// the returned drive delta already contains the native time step.
template<class Definition>
inline float storylandRetailTransmissionGearFraction(const Definition& definition,
    unsigned gear,float velocity) {
    const auto& current=definition.gears[gear];
    if(gear<2) {
        const float spread=definition.maximumTransmissionVelocity/
            float(definition.numberOfGears)*.3333f;
        const float numerator=gear==0?spread-velocity:velocity+spread;
        const float denominator=gear==0?spread-current.downshift:current.upshift+spread;
        return denominator!=0?numerator/denominator:0;
    }
    const float denominator=current.upshift-current.downshift;
    return denominator!=0?(velocity-current.downshift)/denominator:0;
}

template<class Definition,class State>
inline float storylandStepRetailTransmission(const Definition& definition,State& state,
    float velocity,float throttle,float nativeStep,bool drivenWheelsOnGround,
    bool useInertia,uint8_t handlingBoost=0) {
    if(!std::isfinite(velocity)||!std::isfinite(throttle)||!std::isfinite(nativeStep)||nativeStep<=0||
        definition.numberOfGears<1||definition.numberOfGears>5||
        !std::isfinite(definition.maximumTransmissionVelocity)||
        !std::isfinite(definition.reverseVelocity)||!std::isfinite(definition.engineAcceleration)||
        !std::isfinite(definition.engineInertia)||state.gear>definition.numberOfGears)return 0;
    if(velocity<definition.reverseVelocity||velocity>definition.maximumTransmissionVelocity)return 0;

    bool changedGear=false;
    for(unsigned attempt=0;attempt<6;++attempt) {
        const auto& current=definition.gears[state.gear];
        if(velocity>current.upshift&&(state.gear!=0||throttle>0)) {
            if(state.gear==definition.numberOfGears)return 0;
            ++state.gear;
        } else if(velocity<current.downshift&&state.gear!=0&&
            (state.gear!=1||throttle<0)) {
            --state.gear;
        } else break;
        changedGear=true;
    }

    // Native recursion disables the inertia pointers and clears this boost.
    if(changedGear)handlingBoost=0;
    float multiplier=1;
    if(definition.numberOfGears>1) {
        if(state.gear==0)multiplier=4.5f;
        else {
            const float ratio=1-float(state.gear-1)/float(definition.numberOfGears-1);
            const float boost=(definition.transmissionFlags&1)?5.0f:
                (definition.transmissionFlags&2)?4.0f:3.0f;
            multiplier=1+ratio*(ratio*boost);
        }
    }
    const float speedMultiplier=handlingBoost==1?1.2f:1.0f;
    const float powerMultiplier=handlingBoost==2?2.0f:1.0f;
    float acceleration=definition.engineAcceleration*powerMultiplier;
    acceleration*=multiplier;
    acceleration*=speedMultiplier;
    acceleration*=.4f;
    acceleration*=throttle;
    acceleration*=nativeStep;

    if(useInertia&&!changedGear) {
        if(!drivenWheelsOnGround) {
            if(definition.engineInertia>0)
                state.engineSpeed=std::min(1.0f,state.engineSpeed+
                    .1f*std::fabs(throttle)*nativeStep/definition.engineInertia);
            state.inertiaFactor=.1f;
        } else {
            const float fraction=storylandRetailTransmissionGearFraction(definition,state.gear,velocity);
            float change=fraction-state.engineSpeed;
            if(handlingBoost==1)change*=.75f;
            else if(handlingBoost==2)change*=.5f;
            float factor=1-change*definition.engineInertia;
            factor=std::min(factor,1.0f);
            factor=std::max(factor,.1f);
            factor=.85f*state.inertiaFactor+(1-.85f)*factor;
            acceleration*=factor;
            state.inertiaFactor=factor;
            state.engineSpeed=fraction;
        }
    }

    const float maximum=definition.gears[state.gear].maximum*speedMultiplier;
    float overspeed=0;
    if(maximum<0&&velocity<maximum)overspeed=maximum-velocity;
    else if(maximum>0&&velocity>maximum)overspeed=velocity-maximum;
    if(overspeed>0)acceleration*=1-std::min(overspeed/.05f,1.0f);
    return std::isfinite(acceleration)?acceleration:0;
}
