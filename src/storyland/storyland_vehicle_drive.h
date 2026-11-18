#pragma once
#include "src/leeds/leeds_vehicle.h"
#include "src/storyland/storyland_retail_transmission.h"
#include <cmath>

// Native Stories physical velocities are world units per 1/50-second step.
inline constexpr float StorylandVehicleStepsPerSecond=50.0f;
struct StorylandVehiclePedals {
    float throttle=0,brake=0;
    bool burnout=false;
};

inline StorylandVehiclePedals storylandRetailVehiclePedals(float velocity,
    uint8_t accelerator,uint8_t brake,bool reducedReverse=false) {
    // VCS CAutomobile 0x14CC30 and CBike 0x3444D4.
    StorylandVehiclePedals result;
    if(!std::isfinite(velocity))return result;
    constexpr float pedalScale=1.0f/255.0f;
    float command=float(int(accelerator)-int(brake))*pedalScale;
    if(reducedReverse&&command<0)command*=.3f;
    const float nativeSpeed=velocity/StorylandVehicleStepsPerSecond;
    if(std::fabs(nativeSpeed)<.01f) {
        if(accelerator>150&&brake>150) {
            result.throttle=float(accelerator)*pedalScale;
            result.brake=float(brake)*pedalScale;
            result.burnout=true;
        } else result.throttle=command;
    } else if((nativeSpeed>=0&&command>=0)||(nativeSpeed<0&&command<0)) {
        result.throttle=command;
    } else result.brake=std::fabs(command);
    return result;
}

inline float storylandRetailVehicleBrakeDelta(const StorylandVehicleDefinition& definition,
    float brake,float seconds) {
    // CAutomobile 0x13DF50 and CBike 0x339E9C supply a native velocity change.
    if(!std::isfinite(brake)||!std::isfinite(seconds)||seconds<=0||
        !std::isfinite(definition.brakeDeceleration)||definition.brakeDeceleration<0)return 0;
    return std::clamp(brake,0.0f,1.0f)*definition.brakeDeceleration*
        StorylandVehicleStepsPerSecond*StorylandVehicleStepsPerSecond*seconds;
}

inline float storylandVehicleSteeringInputFromAngle(float angle,float steeringLock) {
    if(!std::isfinite(angle)||!std::isfinite(steeringLock)||steeringLock<=0)return 0;
    return std::copysign(std::sqrt(std::min(std::fabs(angle)/steeringLock,1.0f)),angle);
}

inline float storylandRetailVehicleSteeringAngle(float& input,float target,
    float steeringLock,float seconds) {
    // VCS CAutomobile 0x14CBB8/0x14CDE8 and CBike 0x344438/0x34463C.
    if(!std::isfinite(input))input=0;
    if(!std::isfinite(target))target=0;
    if(!std::isfinite(steeringLock)||steeringLock<=0)return 0;
    if(std::isfinite(seconds)&&seconds>0) {
        const float nativeStep=seconds*StorylandVehicleStepsPerSecond;
        input+=(std::clamp(target,-1.0f,1.0f)-input)*(.2f*nativeStep);
    }
    input=std::clamp(input,-1.0f,1.0f);
    return input*std::fabs(input)*steeringLock;
}

inline float storylandVehicleSteeringAngle(float& input, float target,
    float steeringLock, float seconds) {
    if (!std::isfinite(input)) input = 0.0f;
    if (!std::isfinite(target)) target = 0.0f;
    if (!std::isfinite(steeringLock) || steeringLock <= 0.0f) return 0.0f;
    if (std::isfinite(seconds) && seconds > 0.0f) {
        // reLCS ProcessControlInputs retains 80% of the stick each native
        // tick. Compose that response across the host frame duration.
        const float retention = std::pow(0.8f, seconds * StorylandVehicleStepsPerSecond);
        input = std::clamp(target, -1.0f, 1.0f) +
            (input - std::clamp(target, -1.0f, 1.0f)) * retention;
    }
    input = std::clamp(input, -1.0f, 1.0f);
    return input * std::fabs(input) * steeringLock;
}

inline float storylandRetailAutomobileSteeringLimit(float angle,float velocity,
    float lateralSpeed,float steeringLock,float nativeTraction,float roadAdhesion,
    bool contact,bool handbrake) {
    // CAutomobile 0x13E238..0x13E418; native side speed points right.
    constexpr float nativeRadians=.01745329424738884f;
    if(!contact||handbrake||!std::isfinite(angle)||!std::isfinite(velocity)||
        !std::isfinite(lateralSpeed)||!std::isfinite(steeringLock)||steeringLock<=0||
        !std::isfinite(nativeTraction)||nativeTraction<0||
        !std::isfinite(roadAdhesion)||roadAdhesion<0)return angle;
    const float speed=velocity/StorylandVehicleStepsPerSecond;
    if(speed<=.01f)return angle;
    const float side=-lateralSpeed/StorylandVehicleStepsPerSecond;
    if((angle<0&&side>.05f)||(angle>0&&side<-.05f))return angle;
    float limit=nativeTraction*4;
    limit=4*limit;
    limit*=roadAdhesion;
    limit/=speed*speed;
    const float factor=std::asin(std::clamp(limit,-1.0f,1.0f))/(steeringLock*nativeRadians);
    return angle*std::min(factor,1.0f);
}

struct StorylandVehicleTransmissionState {
    unsigned gear=1;
    float rpm=0,shiftCooldown=0;
    float engineSpeed=0,inertiaFactor=1;
};
inline float storylandVehicleTransmissionDrive(const StorylandVehicleDefinition& definition,
    StorylandVehicleTransmissionState& state,float velocity,float throttle,float seconds,
    bool drivenWheelsOnGround=true,uint8_t handlingBoost=0) {
    if(!std::isfinite(seconds)||seconds<=0||!std::isfinite(velocity)||!std::isfinite(throttle)||
        definition.numberOfGears<1||definition.numberOfGears>5)return 0;
    state.gear=std::min<unsigned>(state.gear,definition.numberOfGears);
    state.shiftCooldown=0;
    const bool useInertia=!definition.twoWheels();
    const float nativeSpeed=velocity/StorylandVehicleStepsPerSecond;
    const float delta=storylandStepRetailTransmission(definition,state,nativeSpeed,throttle,
        seconds*StorylandVehicleStepsPerSecond,drivenWheelsOnGround,useInertia,handlingBoost);
    const float engineSpeed=useInertia?state.engineSpeed:
        storylandRetailTransmissionGearFraction(definition,state.gear,nativeSpeed);
    state.rpm=std::clamp(engineSpeed,0.0f,1.0f);
    return delta*StorylandVehicleStepsPerSecond/seconds;
}

// Native constructor 0x133ED4..0x133F08 and CPhysical air resistance
// 0x348628..0x348788. Inputs/outputs here use metres per second.
inline float storylandRetailVehicleAirResistance(float drag,float speed,float seconds) {
    if(!std::isfinite(drag)||drag<0||!std::isfinite(speed)||!std::isfinite(seconds)||seconds<=0)return 1;
    const float resistance=drag>.01f?drag*.0005f:drag;
    const float base=resistance>.1f?resistance:
        1-resistance*std::fabs(speed)/StorylandVehicleStepsPerSecond;
    return std::pow(std::clamp(base,0.0f,1.0f),seconds*StorylandVehicleStepsPerSecond);
}

inline float storylandVehicleWheelbase(const StorylandVehicleDefinition& definition, float fallback) {
    std::array<float, 2> front{}, rear{};
    unsigned frontCount = 0, rearCount = 0;
    for (const auto& wheel : definition.wheels) {
        if (!std::isfinite(wheel.previewPivot[0]) || !std::isfinite(wheel.previewPivot[1])) continue;
        auto& axle = wheel.front ? front : rear;
        axle[0] += wheel.previewPivot[0];
        axle[1] += wheel.previewPivot[1];
        if (wheel.front) ++frontCount;
        else ++rearCount;
    }
    if (frontCount && rearCount) {
        const float length = std::hypot(front[0] / frontCount - rear[0] / rearCount,
            front[1] / frontCount - rear[1] / rearCount);
        if (std::isfinite(length) && length > 0.1f) return length;
    }
    return std::max(1.0f, fallback);
}

inline unsigned storylandVehicleDrivenWheelCount(const StorylandVehicleDefinition& definition) {
    const bool frontDrive=definition.driveType=='F'||definition.driveType=='4';
    const bool rearDrive=definition.driveType!='F';
    const auto& wheels=definition.physicalWheels();
    unsigned count=0;
    for(const auto& wheel:wheels)
        if(wheel.front?frontDrive:rearDrive)++count;
    if(count)return count;
    return definition.twoWheels()?1u:definition.driveType=='4'?4u:2u;
}

inline unsigned storylandVehicleDrivenTyreCount(const StorylandVehicleDefinition& definition) {
    if(!definition.twoWheels())return storylandVehicleDrivenWheelCount(definition);
    const bool frontDrive=definition.driveType=='F'||definition.driveType=='4';
    const bool rearDrive=definition.driveType!='F';
    unsigned count=0;
    for(const auto& wheel:definition.wheels)
        if(wheel.front?frontDrive:rearDrive)++count;
    return count;
}

inline float storylandVehicleDriveThrustPerWheel(const StorylandVehicleDefinition& definition,
    float thrust,unsigned substeps) {
    if(!substeps||!std::isfinite(thrust))return 0.0f;
    // Motorcycles have four suspension rays but only two physical tyre patches.
    // Each tyre force runs once, using the most relevant of its two contact rays.
    const unsigned driven=storylandVehicleDrivenTyreCount(definition);
    return driven?thrust/(float(driven)*float(substeps)):0.0f;
}

inline float storylandVehicleNativeTyreThrust(float transmissionDelta, unsigned substeps) {
    // HandlingMgr divides engine acceleration by the driven wheel count
    // before writing the native row. ProcessCarWheelPair passes that value
    // to each driven tyre, without dividing it for a second time.
    if (!substeps || !std::isfinite(transmissionDelta)) return 0.0f;
    return transmissionDelta / float(substeps);
}

inline float storylandVehicleDriveGrip(const StorylandVehicleDefinition& definition,
    const std::array<bool, 4>& contacts, bool grounded, bool contactsValid) {
    const auto& wheels=definition.physicalWheels();
    const bool nativeContacts=definition.vcsModelLayout&&
        (definition.nativeType==0||definition.motorcycle())&&contactsValid&&
        definition.hasNativeSuspension&&wheels.size()==contacts.size();
    if(!grounded&&!nativeContacts)return 0.0f;
    if(!contactsValid||!definition.hasNativeSuspension||wheels.size()!=contacts.size())
        return 1.0f;
    const bool frontDrive=definition.driveType=='F'||definition.driveType=='4';
    const bool rearDrive=definition.driveType!='F';
    unsigned drivenContacts=0;
    for(size_t index=0;index<contacts.size();++index) {
        const bool driven=wheels[index].front?frontDrive:rearDrive;
        if(driven&&contacts[index])++drivenContacts;
    }
    return float(drivenContacts)/float(storylandVehicleDrivenWheelCount(definition));
}

inline float storylandVehicleBrakeGrip(const StorylandVehicleDefinition& definition,
    const std::array<bool, 4>& contacts, bool grounded, bool contactsValid, bool handbrake) {
    if (!grounded) return 0.0f;
    if (!contactsValid || !definition.hasNativeSuspension || definition.physicalWheels().size() != contacts.size()) return 1.0f;
    const float frontBias = handbrake ? 0.0f : std::clamp(definition.brakeBias, 0.0f, 1.0f);
    unsigned frontCount = 0, rearCount = 0, frontContacts = 0, rearContacts = 0;
    for (size_t index = 0; index < contacts.size(); ++index) {
        if (definition.physicalWheels()[index].front) {
            ++frontCount;
            if (contacts[index]) ++frontContacts;
        } else {
            ++rearCount;
            if (contacts[index]) ++rearContacts;
        }
    }
    return (frontCount ? frontBias * float(frontContacts) / float(frontCount) : 0.0f) +
        (rearCount ? (1.0f - frontBias) * float(rearContacts) / float(rearCount) : 0.0f);
}

inline float storylandAdvanceWheelRotation(float angle, float speed, float radius, float seconds) {
    return std::remainder(angle + speed * seconds / std::max(0.12f, radius), 6.283185307179586f);
}

inline float storylandVehicleContactGrip(const StorylandVehicleDefinition& definition,
    const std::array<bool, 4>& contacts, bool grounded, bool contactsValid) {
    if (!grounded) return 0.0f;
    if (!contactsValid || !definition.hasNativeSuspension || definition.physicalWheels().size() != contacts.size()) return 1.0f;
    return float(std::count(contacts.begin(), contacts.end(), true)) / float(contacts.size());
}
