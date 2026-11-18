#pragma once

#include "src/re/reVCS/vehicles/storyland_vcs_helicopter.h"

namespace StorylandVcsHovercraft {
using Vector=StorylandVcsHelicopter::Vector;
using Frame=StorylandVcsHelicopter::Frame;
using Handling=StorylandVcsHelicopter::Handling;
using Controls=StorylandVcsHelicopter::Controls;
using State=StorylandVcsHelicopter::State;
using StorylandVcsHelicopter::add;
using StorylandVcsHelicopter::subtract;
using StorylandVcsHelicopter::multiply;
using StorylandVcsHelicopter::dot;
using StorylandVcsHelicopter::transform;
using StorylandVcsHelicopter::finite;
using StorylandVcsHelicopter::moveForce;
using StorylandVcsHelicopter::turnForce;

struct Pad {
    int16_t horizontal=0,vertical=0;
    uint8_t accelerator=0,brake=0;
    bool handbrake=false,lookLeft=false,lookRight=false;
};

struct ControlResult {
    Controls flight;
    float steering=0,brake=0,roadThrottle=0;
};

inline ControlResult controls(const Pad& pad,const Frame& frame,const Vector& velocity,
    float steeringDegrees,float nativeStep,const Vector& previous,bool hover=true) {
    ControlResult result;
    const float interpolation=nativeStep*.2f;
    const auto filtered=[&](float old,float target) {
        return std::clamp(old+(target-old)*interpolation,-1.0f,1.0f);
    };
    const float horizontal=float(pad.horizontal)*(1.0f/128.0f);
    result.flight.yaw=filtered(previous[0],hover?horizontal:
        (pad.lookLeft?-1.0f:float(pad.lookRight)));
    result.flight.pitch=filtered(previous[1],float(pad.vertical)*(1.0f/128.0f));
    result.flight.roll=filtered(previous[2],horizontal);
    const float forwardSpeed=dot(frame.forward,velocity);
    result.steering=((-result.flight.yaw*.01745329424738884f)*steeringDegrees)*
        std::max(1-forwardSpeed,.1f);
    if(hover&&pad.handbrake) {
        result.steering*=1.5f;
        result.flight.yaw*=1.5f;
    }
    result.flight.thrust=float(int(pad.accelerator)-int(pad.brake))*(1.0f/255.0f);
    if(forwardSpeed>0&&pad.brake)
        result.brake=float(pad.brake)*(forwardSpeed>.35f?(1.0f/510.0f):(1.0f/255.0f));
    else result.brake=std::sqrt(dot(velocity,velocity))<.1f&&pad.brake<10&&pad.accelerator<10?.5f:.01f;
    return result;
}

inline Vector cross(const Vector& first,const Vector& second) {
    return {first[1]*second[2]-first[2]*second[1],
        -first[0]*second[2]+first[2]*second[0],
        first[0]*second[1]-first[1]*second[0]};
}

inline float inputScale(float yaw,float pitch) {
    constexpr float quarterPi=.7853981852531433f;
    constexpr float halfPi=1.5707963705062866f;
    constexpr float threeQuartersPi=2.356194496154785f;
    constexpr float pi=3.1415927410125732f;
    float angle=(yaw==0&&pitch==0)?0:std::atan2(pitch,yaw);
    if(angle>-quarterPi&&angle<=quarterPi)return 1/std::cos(angle);
    if(angle>quarterPi&&angle<=threeQuartersPi)angle-=halfPi;
    else if(angle>threeQuartersPi)angle-=pi;
    else if(angle<=-threeQuartersPi)angle+=pi;
    else if(angle<-quarterPi)angle+=halfPi;
    else return 1;
    return 1/std::cos(angle);
}

// SLUS-21590 mode3, native model-info +27C bit40, initialized CPlane state.
inline bool flyingForces(const Handling& handling,const Frame& frame,
    const Vector& centreOfMass,float mass,float turnMass,const Vector& wind,
    bool missionCreated,float height,float collisionMinimumY,bool waterContact,
    const Controls& input,float nativeStep,State& state,uint32_t& stallIncrement,
    const Vector& secondaryVelocity={},const Vector& secondaryAngularVelocity={}) {
    stallIncrement=0;
    if(!std::isfinite(nativeStep)||nativeStep<0||!std::isfinite(height)||
        !std::isfinite(collisionMinimumY)||!std::isfinite(mass)||!std::isfinite(turnMass)||
        mass<=0||turnMass<=0||!finite(frame.right)||!finite(frame.forward)||!finite(frame.up)||
        !finite(centreOfMass)||!finite(wind)||!finite(state.velocity)||
        !finite(state.angularVelocity)||!finite(secondaryVelocity)||!finite(secondaryAngularVelocity)||
        !std::isfinite(input.yaw)||!std::isfinite(input.pitch)||
        !std::isfinite(input.roll)||!std::isfinite(input.thrust))return false;
    for(float value:handling)if(!std::isfinite(value))return false;
    if(handling[12]<=0||handling[13]<=0||handling[14]<=0)return false;
    if(nativeStep==0)return true;

    const auto airVelocity=missionCreated?state.velocity:
        subtract(state.velocity,multiply(wind,handling[22]));
    const auto centre=transform(frame,centreOfMass);
    const auto tail=multiply(frame.forward,collisionMinimumY);
    const float scale=inputScale(input.yaw,input.pitch);
    const float yaw=input.yaw*scale,pitch=input.pitch*(-scale);
    const float forwardSpeed=dot(airVelocity,frame.forward);
    const float forwardSquared=forwardSpeed*forwardSpeed;

    float thrust=input.thrust;
    if(thrust>0) {
        thrust*=handling[0];
        if(waterContact)thrust+=thrust;
        if(forwardSpeed>0&&handling[1]<1) {
            const float difference=handling[1]>=0?
                forwardSpeed-handling[1]:forwardSpeed+handling[1];
            const float factor=handling[1]>=0?.65f:3.0f;
            thrust*=std::max(1.0f-difference*(difference*factor),0.0f);
        }
    } else if(thrust<0&&dot(state.velocity,frame.forward)<.02f)
        thrust=handling[0]*std::min(thrust-forwardSpeed*7.76f,0.0f);
    else thrust=0;
    Vector force{};
    for(size_t axis=0;axis<3;++axis)
        force[axis]=nativeStep*(mass*(thrust*(frame.forward[axis]*.008f)));
    moveForce(state,force,mass);

    const float sideSpeed=-dot(airVelocity,frame.right);
    const float sideForce=(handling[4]*sideSpeed)*std::fabs(sideSpeed);
    for(size_t axis=0;axis<3;++axis)
        force[axis]=nativeStep*(mass*(frame.right[axis]*sideForce));
    moveForce(state,force,mass);

    const auto pointVelocity=[&]() {
        const auto angular=add(state.angularVelocity,secondaryAngularVelocity);
        return add(add(cross(angular,tail),state.velocity),secondaryVelocity);
    };
    const float sidePointSpeed=-dot(pointVelocity(),frame.right);
    const float yawSpeed=forwardSpeed<=0?std::min(thrust,forwardSpeed):std::max(thrust,forwardSpeed);
    const float yawForce=((handling[2]*yaw)*yawSpeed+
        (handling[3]*sidePointSpeed)*std::fabs(sidePointSpeed))*turnMass*nativeStep;
    turnForce(state,frame,centreOfMass,multiply(frame.right,yawForce),add(tail,centre),turnMass);

    const float rollForce=((handling[5]*input.roll)*forwardSpeed)*turnMass*nativeStep;
    turnForce(state,frame,centreOfMass,multiply(frame.right,rollForce),add(frame.up,centre),turnMass);
    Vector horizontalRight{frame.forward[1],-frame.forward[0],0};
    if(frame.up[2]<=0)horizontalRight=multiply(horizontalRight,-1);
    const float sign=frame.right[2]>0?-1.0f:1.0f;
    const float rollCorrection=(sign*(1-dot(frame.right,horizontalRight)))*
        (1-std::fabs(frame.forward[2]))*10.0f;
    const float correctionForce=((handling[6]*rollCorrection)*turnMass)*(nativeStep*.5f);
    turnForce(state,frame,centreOfMass,multiply(frame.right,correctionForce),add(frame.up,centre),turnMass);

    const float upPointSpeed=-dot(pointVelocity(),frame.up);
    const float pitchForce=((handling[7]*pitch)*forwardSpeed+
        (handling[8]*upPointSpeed)*std::fabs(upPointSpeed))*turnMass*nativeStep;
    turnForce(state,frame,centreOfMass,multiply(frame.up,pitchForce),add(tail,centre),turnMass);

    const float airMagnitude=std::max(std::sqrt(dot(airVelocity,airVelocity)),.01f);
    const float angleOfAttack=-std::asin(std::clamp(dot(airVelocity,frame.up)/airMagnitude,-1.0f,1.0f));
    if(angleOfAttack>.34906587f)stallIncrement=uint32_t(std::trunc(nativeStep*20.0f));
    const float baseLift=handling[9]*handling[21];
    float lift=(((baseLift+handling[10]*angleOfAttack)*forwardSquared)*mass)*nativeStep;
    const float gravityForce=nativeStep*(mass*.008f);
    if(lift>gravityForce&&height>80)
        lift=(std::max(1-(height-80)*.02f,0.0f)*(nativeStep*.008f))*mass;
    moveForce(state,multiply(frame.up,lift),mass);

    Vector local{dot(state.angularVelocity,frame.right),dot(state.angularVelocity,frame.forward),
        dot(state.angularVelocity,frame.up)};
    Vector resistance{};
    const float speedFactor=2*std::sqrt(dot(state.velocity,state.velocity));
    for(size_t axis=0;axis<3;++axis) {
        const float retention=std::pow(handling[12+axis],nativeStep);
        const float nonlinear=handling[16+axis];
        if(nonlinear>0) {
            const float ratio=retention/((nonlinear*local[axis])*local[axis]+1);
            resistance[axis]=local[axis]*std::pow(ratio,nativeStep)-local[axis];
        } else local[axis]*=std::pow(retention,speedFactor);
    }
    state.angularVelocity=transform(frame,local);
    resistance=multiply(resistance,speedFactor);
    const std::array<Vector,3> directions{multiply(frame.forward,-1),multiply(frame.up,-1),
        multiply(frame.right,-1)};
    const std::array<Vector,3> points{add(frame.up,centre),add(frame.right,centre),
        add(frame.forward,centre)};
    for(size_t axis=0;axis<3;++axis)if(resistance[axis]!=0) {
        force=multiply(multiply(directions[axis],resistance[axis]),turnMass);
        turnForce(state,frame,centreOfMass,force,points[axis],turnMass);
    }
    return finite(state.velocity)&&finite(state.angularVelocity);
}
}
