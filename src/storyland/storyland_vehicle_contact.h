#pragma once

#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <cmath>

namespace StorylandVehicleContact {
using Point = StorylandNativePoint;

inline Point add(Point a, Point b) {
    return {a.x+b.x,a.y+b.y,a.z+b.z};
}
inline Point subtract(Point a, Point b) {
    return {a.x-b.x,a.y-b.y,a.z-b.z};
}
inline Point scale(Point a, float factor) {
    return {a.x*factor,a.y*factor,a.z*factor};
}
inline float dot(Point a, Point b) {
    return a.x*b.x+a.y*b.y+a.z*b.z;
}
inline Point cross(Point a, Point b) {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
inline Point unit(Point a) {
    const float length=std::sqrt(dot(a,a));
    return length>1e-6f?scale(a,1/length):Point{};
}

inline Point angularVelocityFromEulerRates(Point rates,float headingRadians,float pitchRadians) {
    const float cosine=std::cos(headingRadians),sine=std::sin(headingRadians);
    const float pitchCosine=std::cos(pitchRadians),pitchSine=std::sin(pitchRadians);
    // Rates are pitch, roll, yaw for Rz(heading-pi/2) * Rx(pitch) * Ry(roll).
    return {sine*rates.x+cosine*pitchCosine*rates.y,
        -cosine*rates.x+sine*pitchCosine*rates.y,rates.z+pitchSine*rates.y};
}

inline Point eulerRatesFromAngularVelocity(Point angularVelocity,float headingRadians,
    float pitchRadians) {
    const float cosine=std::cos(headingRadians),sine=std::sin(headingRadians);
    const float pitchCosine=std::cos(pitchRadians),pitchSine=std::sin(pitchRadians);
    const float pitchRate=sine*angularVelocity.x-cosine*angularVelocity.y;
    const float horizontalRoll=cosine*angularVelocity.x+sine*angularVelocity.y;
    // Euler roll and yaw coincide at vertical pitch. The host's continuous
    // damped inverse is exact outside |cos(pitch)|<0.1 and bounds amplification
    // to 10 within that singular region, while preserving world-Z rotation.
    const float inverseCosine=pitchCosine/std::max(pitchCosine*pitchCosine,.01f);
    const float rollRate=horizontalRoll*inverseCosine;
    return {pitchRate,rollRate,angularVelocity.z-pitchSine*rollRate};
}

struct WheelBasis {
    Point forward, left;
    bool valid = false;
};

inline WheelBasis wheelBasis(Point vehicleForward, Point groundNormal, float steeringRadians) {
    WheelBasis result;
    if (!std::isfinite(steeringRadians)) return result;
    const Point normal = unit(groundNormal);
    if (dot(normal, normal) < 0.5f) return result;
    const Point forward = unit(subtract(vehicleForward,
        scale(normal, dot(vehicleForward, normal))));
    if (dot(forward, forward) < 0.5f) return result;
    const Point left = unit(cross(normal, forward));
    const float cosine = std::cos(steeringRadians);
    const float sine = std::sin(steeringRadians);
    // reLCS projects the chassis forward axis onto the contact plane, then
    // steers in that plane. Rotating around world Z first changes the
    // requested steering angle on slopes and banked roads.
    result.forward = add(scale(forward, cosine), scale(left, sine));
    result.left = subtract(scale(left, cosine), scale(forward, sine));
    result.valid = true;
    return result;
}

struct Body {
    Point velocity, angularVelocity, arm;
    float inverseMass=0, inverseTurnMass=0;
    // VCS PS2 constructors: automobile 0x133F34, motorcycle 0x335610.
    float elasticity=.05f;

    Point pointVelocity() const {
        return add(velocity,cross(angularVelocity,arm));
    }

    float inverseContactMass(Point direction) const {
        const auto lever=cross(arm,direction);
        return inverseMass+dot(lever,lever)*inverseTurnMass;
    }
};

struct Impact {
    float impulse=0;
    float closingSpeed=0;
};

inline Point contactAxisAwayFromSecond(Point axis, float projectedDisplacement) {
    // Keep the whole separating axis together; changing each component's sign
    // independently rotates normals for oblique vehicle orientations. The
    // native sphere contact convention points away from the second body.
    return projectedDisplacement < 0.0f ? axis : scale(axis, -1.0f);
}

inline Point orientedPairNormal(Point firstPosition, Point secondPosition, Point suppliedNormal) {
    const Point displacement = subtract(firstPosition, secondPosition);
    Point normal = unit(suppliedNormal);
    if (dot(normal, normal) < 0.5f) normal = unit(displacement);
    if (dot(normal, normal) < 0.5f) normal = {1.0f, 0.0f, 0.0f};
    if (dot(normal, displacement) < 0.0f) normal = scale(normal, -1.0f);
    return normal;
}

inline Impact impact(const Body& first, const Body& second, Point normal) {
    // Contact mass and mean elasticity: LCS 0x2C0974, VCS 0x34C734.
    // The static branch uses only the moving body's elasticity (LCS 0x2BF33C).
    const float closing=dot(subtract(second.pointVelocity(),first.pointVelocity()),normal);
    const float inverseMass=first.inverseContactMass(normal)+second.inverseContactMass(normal);
    if(!(closing>0&&inverseMass>0))return {};
    const float elasticity=second.inverseMass==0?first.elasticity:
        first.inverseMass==0?second.elasticity:(first.elasticity+second.elasticity)*.5f;
    return {(1+elasticity)*closing/inverseMass,closing};
}

enum class TyreState : uint8_t { Rolling, Spinning, Sliding, Locked };

struct TyreForce {
    float forward=0, lateral=0, slip=0;
    TyreState state=TyreState::Rolling;
};

inline TyreForce tyre(float forwardSpeed,float lateralSpeed,float thrust,float brake,
    float adhesion,float tractionLoss,unsigned wheelCount,TyreState previous) {
    // Common ProcessBikeWheel force calculation: LCS 0x3286BC, VCS 0x3E7750.
    // Speeds and velocity changes here are scaled from the native 50 Hz units.
    TyreForce result;
    if(!wheelCount||!(adhesion>0))return result;
    const bool driving=brake<=0&&thrust!=0;
    const bool wasSlipping=previous!=TyreState::Rolling;
    if(wasSlipping)adhesion*=tractionLoss;
    result.lateral=-lateralSpeed/float(wheelCount);
    if(driving) {
        result.forward=thrust;
        result.lateral=std::clamp(result.lateral,-adhesion,adhesion);
    } else {
        result.forward=-forwardSpeed/float(wheelCount);
        if(brake<=adhesion)result.forward=std::clamp(result.forward,-brake,brake);
        else if(std::fabs(forwardSpeed)>.005f*50)result.state=TyreState::Locked;
    }
    const float requested=std::hypot(result.forward,result.lateral);
    if(requested>adhesion) {
        if(result.state!=TyreState::Locked)
            result.state=driving&&forwardSpeed<.2f*50?TyreState::Spinning:TyreState::Sliding;
        const float force=adhesion*(wasSlipping?1.0f:tractionLoss)/requested;
        result.forward*=force;result.lateral*=force;
        result.slip=std::clamp(1-adhesion/requested,.05f,1.0f);
    }
    return result;
}

struct RetailTyreParameters {
    bool automobile=false;
    bool front=false;
    bool playerControlled=false;
    float throttle=0;
    int32_t modelId=-1;
};

inline TyreForce retailTyre(float forwardSpeed,float lateralSpeed,float thrust,float brake,
    float adhesion,float tractionLoss,unsigned wheelCount,TyreState previous,
    const RetailTyreParameters& parameters) {
    // VCS PS2 ProcessWheel 0x3E6C18 and ProcessBikeWheel 0x3E7618.
    TyreForce result;
    if(!wheelCount||!std::isfinite(forwardSpeed)||!std::isfinite(lateralSpeed)||
        !std::isfinite(thrust)||!std::isfinite(brake)||!std::isfinite(adhesion)||
        !std::isfinite(tractionLoss)||adhesion<0||brake<0||tractionLoss<0)return result;
    const bool driving=brake==0&&thrust!=0;
    const bool wasSlipping=previous!=TyreState::Rolling;
    const float spinGrip=1-.2f*std::fabs(parameters.throttle);
    if(wasSlipping) {
        adhesion*=tractionLoss;
        if(parameters.automobile&&parameters.playerControlled&&previous==TyreState::Spinning)
            adhesion*=spinGrip;
    }
    result.lateral=-lateralSpeed/float(wheelCount);
    if(driving) {
        result.forward=thrust;
        result.lateral=std::clamp(result.lateral,-adhesion,adhesion);
    } else {
        result.forward=-forwardSpeed/float(wheelCount);
        if(brake<=adhesion)result.forward=std::clamp(result.forward,-brake,brake);
        else if(std::fabs(forwardSpeed)>.005f*50)result.state=TyreState::Locked;
    }
    const float requestedSquared=result.forward*result.forward+result.lateral*result.lateral;
    if(requestedSquared>adhesion*adhesion) {
        if(result.state!=TyreState::Locked) {
            if(parameters.automobile) {
                const float spinThreshold=parameters.front&&forwardSpeed>.15f*50?.6f:.3f;
                result.state=driving&&std::fabs(result.forward)>spinThreshold*adhesion?
                    TyreState::Spinning:TyreState::Sliding;
            } else if(driving&&forwardSpeed<.2f*50) {
                if(parameters.modelId!=178&&parameters.modelId!=179)result.state=TyreState::Spinning;
            } else result.state=TyreState::Sliding;
        }
        float loss=wasSlipping?1.0f:tractionLoss;
        if(!wasSlipping&&parameters.automobile&&parameters.playerControlled&&
            result.state==TyreState::Spinning)loss*=spinGrip;
        const float requested=std::sqrt(requestedSquared);
        const float factor=adhesion/requested*loss;
        result.forward*=factor;
        result.lateral*=factor;
        result.slip=std::clamp(1-adhesion/requested,.05f,1.0f);
    }
    return result;
}

inline float retailTyreLoad(float springRatio,float suspensionForce,float axleBias) {
    // VCS PS2 CAutomobile::ProcessWheelPair, 0x1534E0-0x153528.
    return std::clamp((1-springRatio)*suspensionForce*4*axleBias,0.0f,2.0f);
}

inline bool retailHandbrakeActive(float forwardSpeed,bool bike,unsigned padMode=0) {
    // CAutomobile 0x152F18, CBike 0x33AE18. Alternate bike pad modes lock at rest.
    return (bike&&(padMode==2||padMode==3))||std::fabs(forwardSpeed)>.01f*50;
}

inline float retailCoastingBrake(float mass,bool bike) {
    if(!(mass>0)||!std::isfinite(mass))return 0;
    return bike?.54f/(mass+200):(mass<500?.09f:.9f)/mass;
}

inline Point retailTyreTurnDelta(const TyreForce& force,Point forward,Point left,
    float antiDive,bool braking,bool driving) {
    // VCS PS2 0x3E71B0-0x3E728C changes turn force without changing move force.
    if(!std::isfinite(antiDive)||antiDive<=0)antiDive=0;
    const float forwardFactor=braking?1-antiDive:driving?1-.5f*antiDive:1;
    return add(scale(forward,force.forward*forwardFactor),scale(left,force.lateral));
}

inline unsigned adhesionGroup(uint8_t surface) {
    // LCS 0x3B5690 and VCS 0x4B8DD0.
    constexpr uint8_t group[]={2,2,3,3,1,2,1,1,1,1,1,1,1,1,1,1,1,0,4,5,2,2,2,0,1,3,3,1,1,0,3,1,1,4,2};
    return surface<sizeof(group)?group[surface]:2;
}

inline float wetGrip(uint8_t surface,float wetRoads) {
    // The wet-road switch tables in LCS 0x3B5720 and VCS 0x4B8E60.
    constexpr int8_t tenths[]={-2,-2,-3,0,-2,-2,-3,-3,-2,-3,-3,-3,-3,-3,-3,-3,-3,-3,5,0,
        -2,-2,-2,-3,-3,-2,-3,-3,-3,-3,-2,-2,-3,5,-2};
    return 1+(surface<sizeof(tenths)?tenths[surface]*.1f*wetRoads:0);
}
}
