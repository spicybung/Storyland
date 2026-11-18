#pragma once

#include "src/leeds/leeds_vehicle.h"
#include "src/storyland/storyland_vehicle_contact.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace StorylandBikeDynamics {
constexpr float Radians=.01745329251994329577f;
constexpr float NativeRate=50.0f;

inline float leanInput(float current,float target,float seconds) {
    if(!std::isfinite(current))current=0;
    if(!std::isfinite(target))target=0;
    if(!std::isfinite(seconds)||seconds<=0)return current;
    return std::clamp(current+(std::clamp(target,-1.0f,1.0f)-current)*
        (1-std::pow(.8f,seconds*NativeRate)),-1.0f,1.0f);
}

inline float centerOfMassShift(const StorylandVehicleDefinition& definition,float lean) {
    if(!definition.hasNativeBikeDynamics||!std::isfinite(lean))return 0;
    return (lean<0?definition.bikeLeanBackCenter:definition.bikeLeanForwardCenter)*
        std::clamp(lean,-1.0f,1.0f);
}

inline float steeringAngle(const StorylandVehicleDefinition& definition,float angle,
    float speed,float leanRadians,float tractionMultiplier,float roadAdhesion,
    unsigned rearSurfaceGroup,bool frontContact) {
    if(!definition.hasNativeBikeDynamics||!frontContact||speed<=.5f||
        !std::isfinite(speed)||!std::isfinite(angle)||definition.steeringLock<=0)return angle;
    const float velocity=speed/NativeRate;
    float grip=.004f*.25f*definition.traction*std::max(0.0f,tractionMultiplier)*
        roadAdhesion*4*definition.bikeSpeedSteer;
    if(rearSurfaceGroup==1||rearSurfaceGroup==3)grip*=definition.bikeSlipSteer;
    float factor=std::asin(std::clamp(grip/(velocity*velocity),0.0f,1.0f))/
        (definition.steeringLock*Radians);
    if(angle*leanRadians>0)factor*=2;
    return angle*std::min(factor,1.0f);
}

inline float angularRetention(float velocity,float retention,float damping,float seconds) {
    if(!std::isfinite(velocity)||!std::isfinite(seconds)||seconds<=0||velocity==0)return 1;
    const unsigned steps=std::max(1u,unsigned(std::ceil(seconds*120)));
    const float nativeStep=seconds*NativeRate/float(steps);
    float angular=velocity;
    for(unsigned step=0;step<steps;++step) {
        const float nativeVelocity=angular/NativeRate;
        angular*=std::pow(std::clamp(retention/(1+damping*nativeVelocity*nativeVelocity),0.0f,1.0f),
            nativeStep);
    }
    return angular/velocity;
}

inline float pitchRetention(const StorylandVehicleDefinition& definition,float velocity,
    float forwardZ,bool frontContact,bool rearContact,float seconds) {
    if(!std::isfinite(velocity)||!std::isfinite(seconds)||seconds<=0)return 1;
    float retention=.9995f;
    float damping=1;
    if(!frontContact) {
        damping=100;
        retention=rearContact&&forwardZ>0?
            retention-std::min(.25f*std::fabs(definition.bikeWheelieSine-forwardZ),.07f):.98f;
    } else if(!rearContact) {
        damping=100;
        if(forwardZ<0)retention*=std::min(.3f*std::fabs(definition.bikeStoppieSine-forwardZ),.1f)+.9f;
    }
    return angularRetention(velocity,retention,damping,seconds);
}

inline float rollRetention(float velocity,float seconds) {
    return angularRetention(velocity,.9f,1000,seconds);
}

inline StorylandNativePoint angularDampingDelta(const StorylandVehicleDefinition& definition,
    StorylandNativePoint angularVelocity,StorylandNativePoint forward,StorylandNativePoint right,
    bool frontContact,bool rearContact,float seconds) {
    using namespace StorylandVehicleContact;
    const float pitch=dot(angularVelocity,right);
    const float roll=dot(angularVelocity,forward);
    const float pitchChange=pitch*(pitchRetention(definition,pitch,forward.z,
        frontContact,rearContact,seconds)-1);
    const float rollChange=roll*(rollRetention(roll,seconds)-1);
    return add(scale(right,pitchChange),scale(forward,rollChange));
}

inline StorylandNativePoint averageContactNormal(
    const std::array<StorylandNativePoint,4>& normals,const std::array<float,4>& timers,
    StorylandNativePoint up) {
    using namespace StorylandVehicleContact;
    StorylandNativePoint result{};
    unsigned count=0;
    for(size_t index=0;index<timers.size();++index)if(timers[index]>0) {
        result=add(result,normals[index]);
        ++count;
    }
    if(count==0)return {0,0,1};
    result=scale(result,1.0f/float(count));
    return dot(result,up)<-.5f?scale(result,-1):result;
}

inline StorylandNativePoint uprightAngularDelta(StorylandNativePoint right,
    StorylandNativePoint forward,StorylandNativePoint averageNormal,bool balancedByRider,
    bool beingPickedUp,bool standing,float seconds) {
    using namespace StorylandVehicleContact;
    if((!balancedByRider&&!beingPickedUp&&!standing)||!std::isfinite(seconds)||seconds<=0)
        return {};
    const float side=std::clamp(dot(right,averageNormal),-1.0f,1.0f);
    const float gain=balancedByRider?.1f:.15f;
    // Original SLUS upright impulses are per call. Normalize their 50 Hz
    // rate for the host timestep; one 0.02-second step matches the original.
    return scale(forward,gain*side*NativeRate*NativeRate*seconds);
}

struct VisualLeanInput {
    float current=0;
    StorylandNativePoint tyreAcceleration{};
    StorylandNativePoint forward{0,1,0};
    StorylandNativePoint right{1,0,0};
    StorylandNativePoint averageNormal{0,0,1};
    float steeringRadians=0;
    float readyBlend=0;
    bool hasContact=false;
    bool balancedByRider=false;
    bool beingPickedUp=false;
    bool standing=false;
    bool frontTyreBurst=false;
};

inline float visualLean(const StorylandVehicleDefinition& definition,
    const VisualLeanInput& input,float seconds) {
    using namespace StorylandVehicleContact;
    if(!definition.hasNativeBikeHandling||!std::isfinite(seconds)||seconds<=0)
        return input.current;
    const float nativeStep=seconds*NativeRate;
    const float idleAngle=input.readyBlend*10*Radians;
    if(input.balancedByRider||input.beingPickedUp) {
        auto surfaceRight=cross(input.forward,input.averageNormal);
        const float lengthSquared=dot(surfaceRight,surfaceRight);
        surfaceRight=lengthSquared>0?scale(surfaceRight,1/std::sqrt(lengthSquared)):
            StorylandNativePoint{1,0,0};
        float lean=0;
        if(input.hasContact)
            lean=dot(input.tyreAcceleration,surfaceRight)*seconds/NativeRate;
        else if(definition.steeringLock>0)
            lean=-input.steeringRadians/(definition.steeringLock*Radians)*.004f*nativeStep;
        lean/=.008f*std::max(nativeStep,.5f);
        const float limit=std::sin(definition.bikeMaximumLeanRadians)*
            (input.frontTyreBurst?.6f:1.0f);
        lean=std::clamp(lean,-limit,limit);
        const float target=std::asin(std::clamp(lean,-1.0f,1.0f))-idleAngle;
        const float retention=std::pow(definition.bikeLeanRetention,nativeStep);
        return input.current*retention+target*(1-retention);
    }
    if(input.standing) {
        const float retention=std::pow(.97f,nativeStep);
        const float target=std::asin(std::clamp(input.right.z,-1.0f,1.0f))+15*Radians+idleAngle;
        return input.current*retention-target*(1-retention);
    }
    return input.current*std::pow(.95f,nativeStep);
}

inline float tyreLateralSpeed(StorylandNativePoint velocity,StorylandNativePoint lateral,
    bool front,bool rearContact) {
    // BIKE_WHEELSPEC_2 suppresses only the front tyre's sideways response.
    return front&&!rearContact?0.0f:StorylandVehicleContact::dot(velocity,lateral);
}

inline bool rearTyreFirst(const StorylandVehicleDefinition& definition) {
    // LCS stores its combined flags at +0xD0; VCS separates handling flags.
    return definition.vcsModelLayout?(definition.handlingFlags&0x20000000u)!=0:
        (definition.modelFlags&0x00200000u)!=0;
}

struct BalanceDelta {float pitch=0,yaw=0;};

inline BalanceDelta balance(const StorylandVehicleDefinition& definition,float forwardZ,
    float speed,float steeringRadians,float lean,float throttle,bool braking,bool handbrake,
    bool frontContact,bool rearContact,float seconds) {
    BalanceDelta result;
    if(!definition.hasNativeBikeDynamics||!std::isfinite(seconds)||seconds<=0||
        !std::isfinite(forwardZ)||!std::isfinite(speed)||!std::isfinite(lean)||
        !std::isfinite(throttle)||!std::isfinite(steeringRadians))return result;
    const float timeScale=seconds*NativeRate*NativeRate;
    const float speedScale=std::min(std::fabs(speed)/NativeRate,.1f);
    if(lean<0&&((!braking&&!handbrake)||(!frontContact&&!rearContact))) {
        // Original LCS 0x157C38 / VCS 0x337520 give the Sanchez its own pedal factor.
        const bool sanchez=definition.modelId==(definition.vcsModelLayout?205u:210u);
        const float pedal=std::clamp(throttle,-1.0f,1.0f);
        const float pedalFactor=sanchez?.7f*pedal+.3f:.5f*pedal+.5f;
        result.pitch=-lean*definition.bikeLeanBackForce*speedScale*
            pedalFactor*timeScale;
    } else if(lean>0&&!frontContact&&!rearContact)
        result.pitch=-lean*definition.bikeLeanForwardForce*speedScale*timeScale;
    if(!frontContact&&rearContact&&forwardZ>0) {
        float error=definition.bikeWheelieSine-forwardZ;
        if(error>.15f)error=std::max(.3f-error,0.0f);
        else if(error<-.08f)error=std::min(-.15f-error,0.0f);
        result.pitch+=.5f*definition.bikeWheelieStability*speedScale*error*timeScale;
        result.yaw=-.5f*steeringRadians*definition.bikeWheelieSteer*timeScale;
    } else if(frontContact&&!rearContact&&forwardZ<0) {
        float error=definition.bikeStoppieSine-forwardZ;
        if(error>.15f)error=std::max(.3f-error,0.0f);
        else if(error<-.15f)error=std::min(-.3f-error,0.0f);
        result.pitch+=.5f*definition.bikeStoppieStability*speedScale*error*timeScale;
        result.yaw=-.5f*std::min(std::fabs(speed)*.1f,1.0f)*steeringRadians*
            definition.bikeWheelieSteer*timeScale;
    }
    return result;
}

struct TyreAngularImpulses {
    StorylandNativePoint longitudinal,lateral;
};

inline TyreAngularImpulses tyreAngularImpulses(StorylandNativePoint contactPoint,
    StorylandNativePoint turnImpulse,StorylandNativePoint forward,StorylandNativePoint right,
    float inverseTurnMass,bool front,bool braking,bool reversing,
    StorylandNativePoint centerOfMass={}) {
    using namespace StorylandVehicleContact;
    TyreAngularImpulses result{};
    // ProcessBikeWheel projects the origin-relative contact first. Native
    // ApplyTurnForce then subtracts the rotated centre of mass from each point.
    const auto sideImpulse=scale(right,dot(turnImpulse,right));
    if(front||(!braking&&!reversing)) {
        const auto longitudinalPoint=subtract(contactPoint,
            scale(right,dot(contactPoint,right)));
        result.longitudinal=scale(cross(subtract(longitudinalPoint,centerOfMass),
            scale(subtract(turnImpulse,sideImpulse),2)),inverseTurnMass);
    }
    const auto lateralPoint=scale(forward,dot(contactPoint,forward));
    result.lateral=scale(cross(subtract(lateralPoint,centerOfMass),sideImpulse),inverseTurnMass);
    return result;
}

inline StorylandNativePoint tyreAngularDelta(StorylandNativePoint contactPoint,
    StorylandNativePoint turnImpulse,StorylandNativePoint forward,StorylandNativePoint right,
    float inverseTurnMass,bool front,bool braking,bool reversing,
    StorylandNativePoint centerOfMass={}) {
    const auto impulses=tyreAngularImpulses(contactPoint,turnImpulse,forward,right,
        inverseTurnMass,front,braking,reversing,centerOfMass);
    return StorylandVehicleContact::add(impulses.longitudinal,impulses.lateral);
}

inline StorylandNativePoint nativeAngularLimit(StorylandNativePoint velocity) {
    // Both original ApplyTurnForce functions limit each world component to
    // four radians per native step, after every individual torque application.
    constexpr float limit=4*NativeRate;
    return {std::clamp(velocity.x,-limit,limit),std::clamp(velocity.y,-limit,limit),
        std::clamp(velocity.z,-limit,limit)};
}

inline StorylandNativePoint tyreLinearVelocity(StorylandNativePoint velocity,
    StorylandNativePoint change) {
    if(change.x==0&&change.y==0&&change.z==0)return velocity;
    // Original ApplyMoveForce bounds linear world velocity independently
    // of ApplyTurnForce; it does not bound angular velocity before a torque.
    constexpr float limit=4*NativeRate;
    const auto result=StorylandVehicleContact::add(velocity,change);
    return {std::clamp(result.x,-limit,limit),std::clamp(result.y,-limit,limit),
        std::clamp(result.z,-limit,limit)};
}

inline StorylandNativePoint tyreAngularVelocity(StorylandNativePoint angularVelocity,
    StorylandNativePoint contactPoint,StorylandNativePoint turnImpulse,
    StorylandNativePoint forward,StorylandNativePoint right,float inverseTurnMass,
    bool front,bool braking,bool reversing,StorylandNativePoint centerOfMass={}) {
    using namespace StorylandVehicleContact;
    if(turnImpulse.x==0&&turnImpulse.y==0&&turnImpulse.z==0)return angularVelocity;
    const auto impulses=tyreAngularImpulses(contactPoint,turnImpulse,forward,right,
        inverseTurnMass,front,braking,reversing,centerOfMass);
    auto result=angularVelocity;
    if(front||(!braking&&!reversing))
        result=nativeAngularLimit(add(result,impulses.longitudinal));
    return nativeAngularLimit(add(result,impulses.lateral));
}
}
