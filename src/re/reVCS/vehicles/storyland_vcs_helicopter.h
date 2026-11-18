#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace StorylandVcsHelicopter {
using Vector = std::array<float,3>;
using Handling = std::array<float,24>;

struct Frame {
    Vector right{1,0,0};
    Vector forward{0,1,0};
    Vector up{0,0,1};
};

struct Controls {
    float yaw=0, pitch=0, roll=0, thrust=0;
};

struct Pad {
    int16_t horizontal=0, vertical=0;
    uint8_t accelerator=0, brake=0;
    bool lookLeft=false, lookRight=false, stabilise=false;
};

struct State {
    Vector velocity{};
    Vector angularVelocity{};
};

inline float dot(const Vector& first, const Vector& second) {
    return (first[0]*second[0]+first[1]*second[1])+first[2]*second[2];
}

inline Vector multiply(const Vector& value, float factor) {
    return {value[0]*factor,value[1]*factor,value[2]*factor};
}

inline Vector add(const Vector& first, const Vector& second) {
    return {first[0]+second[0],first[1]+second[1],first[2]+second[2]};
}

inline Vector subtract(const Vector& first, const Vector& second) {
    return {first[0]-second[0],first[1]-second[1],first[2]-second[2]};
}

inline Vector normalise(Vector value) {
    const float squared=dot(value,value);
    if(squared<=0) {
        value[0]=1;
        return value;
    }
    return multiply(value,1/std::sqrt(squared));
}

inline Vector transform(const Frame& frame, const Vector& value) {
    Vector result{};
    for(size_t axis=0;axis<3;++axis)
        result[axis]=(frame.right[axis]*value[0]+frame.forward[axis]*value[1])+
            frame.up[axis]*value[2];
    return result;
}

inline bool finite(const Vector& value) {
    return std::all_of(value.begin(),value.end(),[](float item){return std::isfinite(item);});
}

// SLUS-21590 CHeli::ProcessControlInputs, 002E7068..002E75FC.
inline Controls controls(const Pad& pad, const Frame& frame, const Vector& velocity,
    const Handling& handling, float health, bool alternate=false) {
    Controls result;
    result.thrust=float(int(pad.accelerator)-int(pad.brake))*(1.0f/255.0f);
    result.pitch=float(pad.vertical)*(1.0f/128.0f);
    if(alternate) {
        result.roll=pad.lookRight?-1.0f:float(pad.lookLeft);
        result.yaw=float(pad.horizontal)*(1.0f/128.0f);
    } else {
        result.roll=float(-int(pad.horizontal))*(1.0f/128.0f);
        result.yaw=pad.lookLeft?-1.0f:float(pad.lookRight);
    }
    if(pad.stabilise&&frame.up[2]>0) {
        const auto horizontalForward=normalise({-frame.right[1],frame.right[0],0});
        const auto horizontalRight=normalise({frame.forward[1],-frame.forward[0],0});
        result.pitch=std::clamp(dot(velocity,horizontalForward)*handling[8],-2.0f,2.0f);
        result.roll=std::clamp(dot(velocity,horizontalRight)*handling[6],-2.0f,2.0f);
    }
    if(health<250) {
        result.thrust=-.1f;
        result.yaw+=.5f;
    }
    return result;
}

// Rotor increments are per ProcessControl call, not multiplied by CTimer.
inline float occupiedRotor(float rotor, bool fasterModel=false) {
    return rotor<.22f?rotor+(fasterModel?.003f:.001f):rotor;
}

inline bool flightEnabled(float rotor, unsigned wheelContacts, float thrust,
    const Vector& velocity, bool rotorDisabled=false, bool holdingGroundRotor=false) {
    if(rotorDisabled||!(rotor>.15f))return false;
    if(wheelContacts<4&&!holdingGroundRotor)return true;
    return thrust>0||std::any_of(velocity.begin(),velocity.end(),
        [](float value){return std::fabs(value)>.02f;});
}

inline void moveForce(State& state, Vector force, float mass) {
    const float inverseMass=1/mass;
    for(size_t axis=0;axis<3;++axis)
        state.velocity[axis]=std::clamp(state.velocity[axis]+force[axis]*inverseMass,-4.0f,4.0f);
}

inline void turnForce(State& state, const Frame& frame, const Vector& centreOfMass,
    const Vector& force, const Vector& point, float turnMass) {
    const auto lever=subtract(point,transform(frame,centreOfMass));
    const Vector moment{lever[1]*force[2]-lever[2]*force[1],
        -lever[0]*force[2]+lever[2]*force[0],lever[0]*force[1]-lever[1]*force[0]};
    const float inverseMass=1/turnMass;
    for(size_t axis=0;axis<3;++axis)
        state.angularVelocity[axis]=std::clamp(state.angularVelocity[axis]+
            moment[axis]*inverseMass,-4.0f,4.0f);
}

// This is the ordinary mode-6 branch, with the constructor-cleared +268 bit8.
// Velocities and CTimer step are native units; gravity/integration/collision are
// performed by the caller, just as they are outside CVehicle::FlyingControl.
inline bool flyingForces(const Handling& handling, const Frame& frame,
    const Vector& centreOfMass, float mass, float turnMass, const Vector& wind,
    bool missionCreated, float height, const Controls& input, float nativeStep,
    State& state) {
    if(!std::isfinite(nativeStep)||nativeStep<0||!std::isfinite(height)||
        !std::isfinite(mass)||!std::isfinite(turnMass)||mass<=0||turnMass<=0||
        !finite(frame.right)||!finite(frame.forward)||!finite(frame.up)||
        !finite(centreOfMass)||!finite(wind)||!finite(state.velocity)||
        !finite(state.angularVelocity)||!std::isfinite(input.yaw)||
        !std::isfinite(input.pitch)||!std::isfinite(input.roll)||!std::isfinite(input.thrust))return false;
    for(float value:handling)if(!std::isfinite(value))return false;
    if(handling[11]<=0||handling[12]<=0||handling[13]<=0||handling[14]<=0||
        handling[9]<0)return false;
    if(nativeStep==0)return true;

    const auto airVelocity=missionCreated?state.velocity:
        subtract(state.velocity,multiply(wind,handling[22]));
    const auto centre=transform(frame,centreOfMass);
    state.velocity=multiply(state.velocity,std::pow(handling[11],nativeStep));
    float upSpeed=dot(airVelocity,frame.up);
    if(upSpeed>0)upSpeed+=upSpeed;
    const float worldLift=.5f-state.velocity[2];
    moveForce(state,{0,0,(worldLift*.008f*mass)*nativeStep},mass);
    float lift=handling[0]*input.thrust+.45f;
    lift-=handling[1]*upSpeed;
    if(height>80)lift*=10.0f/(height-80.0f+10.0f);
    Vector force{};
    for(size_t axis=0;axis<3;++axis)
        force[axis]=((lift*(frame.up[axis]*.008f))*mass)*nativeStep;
    moveForce(state,force,mass);

    const auto windUp=normalise(add({0,0,1},multiply(wind,handling[22])));
    const float rightTilt=frame.up[2]>0?
        -std::clamp(dot(frame.right,windUp),-handling[9],handling[9]):
        (frame.right[2]<0?handling[9]:-handling[9]);
    const float forwardTilt=frame.up[2]>0?
        -std::clamp(dot(frame.forward,windUp),-handling[9],handling[9]):
        (frame.forward[2]<0?handling[9]:-handling[9]);
    for(size_t axis=0;axis<3;++axis)
        force[axis]=((handling[10]*rightTilt)*turnMass)*nativeStep*frame.up[axis];
    turnForce(state,frame,centreOfMass,force,add(frame.right,centre),turnMass);
    for(size_t axis=0;axis<3;++axis)
        force[axis]=((handling[10]*forwardTilt)*turnMass)*nativeStep*frame.up[axis];
    turnForce(state,frame,centreOfMass,force,add(frame.forward,centre),turnMass);

    for(size_t axis=0;axis<3;++axis)
        force[axis]=((handling[7]*(input.pitch*frame.up[axis]))*turnMass)*nativeStep;
    turnForce(state,frame,centreOfMass,force,add(frame.forward,centre),turnMass);
    for(size_t axis=0;axis<3;++axis)
        force[axis]=((handling[5]*(input.roll*frame.up[axis]))*turnMass)*nativeStep;
    turnForce(state,frame,centreOfMass,force,add(frame.right,centre),turnMass);

    const float sideSpeed=-dot(airVelocity,frame.right);
    const float sideAcceleration=(handling[4]*sideSpeed)*std::fabs(sideSpeed);
    for(size_t axis=0;axis<3;++axis)
        force[axis]=(mass*(frame.right[axis]*sideAcceleration))*nativeStep;
    moveForce(state,force,mass);
    const float yawAcceleration=handling[2]*input.yaw+
        (handling[3]*sideSpeed)*std::fabs(sideSpeed);
    for(size_t axis=0;axis<3;++axis)
        force[axis]=frame.right[axis]*(yawAcceleration*turnMass*nativeStep);
    turnForce(state,frame,centreOfMass,force,add(multiply(frame.forward,-1),centre),turnMass);
    for(size_t axis=0;axis<3;++axis)
        force[axis]=((handling[2]*(input.yaw*frame.forward[axis]))*turnMass)*nativeStep;
    turnForce(state,frame,centreOfMass,force,add(frame.right,centre),turnMass);

    Vector local{dot(state.angularVelocity,frame.right),dot(state.angularVelocity,frame.forward),
        dot(state.angularVelocity,frame.up)};
    Vector resistance{};
    const float speedFactor=1+std::sqrt(dot(state.velocity,state.velocity));
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
