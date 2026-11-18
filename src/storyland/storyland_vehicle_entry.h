#pragma once
#include "src/storyland/storyland_vehicle_transition.h"
#include "src/leeds/leeds_vehicle.h"
#include <array>
#include <cmath>

struct StorylandPlayTestRuntime;
bool storylandBeginPlayTestVehicleEntry(StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& cars,const StorylandNativePoint& player,
    int32_t slot,int32_t passengerSeat=-1);

struct StorylandVehicleAccessState {
    bool vcs=true, twoWheels=false, boat=false, helicopter=false,hovercraft=false,jetSki=false;
    bool wrecked=false, carried=false, visible=true, driverProtected=false;
    int32_t doorLock=0;
    float upZ=1, rightZ=0, heightAboveGround=0;
    std::array<float,3> velocity{}, angularVelocity{};
};

inline bool storylandRetailVehicleLocksOpen(bool vcs, int32_t lock, bool player=true) {
    // LCS 0x32A510; VCS 0x3E9930. Lockout-player-only is active only in VCS.
    if(lock==2 || lock==4 || lock==5 || lock==9)return false;
    return !(vcs && player && lock==3);
}

inline float storylandVehicleNativeSpeedSquared(const std::array<float,3>& velocity) {
    constexpr float secondsPerNativeStep=1.0f/50.0f;
    float squared=0;
    for(float component:velocity) {
        if(!std::isfinite(component))return INFINITY;
        squared+=component*component*secondsPerNativeStep*secondsPerNativeStep;
    }
    return squared;
}

inline bool storylandRetailVehicleCanEnter(const StorylandVehicleAccessState& state) {
    // LCS 0x32A570; VCS 0x3E99D0. Inverted vehicles are distinct from vehicles
    // lying on their side. VCS also exempts helicopters from the tilt test.
    if(state.wrecked || state.carried || !state.visible || state.driverProtected ||
        !storylandRetailVehicleLocksOpen(state.vcs,state.doorLock))return false;
    if(!std::isfinite(state.upZ) ||
        (!state.twoWheels && !(state.vcs && state.helicopter) &&
         state.upZ>=-.1f && state.upZ<=.1f))return false;
    constexpr float limit=.2f*.2f;
    return storylandVehicleNativeSpeedSquared(state.velocity)<=std::nextafter(limit,INFINITY) &&
        storylandVehicleNativeSpeedSquared(state.angularVelocity)<=std::nextafter(limit,INFINITY);
}

inline bool storylandRetailVehicleCanExit(const StorylandVehicleAccessState& state, bool jumpExit=false) {
    // LCS 0x32A668..0x32A80C; VCS general branch 0x3E9BD4..0x3E9D64.
    if(state.carried || !std::isfinite(state.upZ) || state.doorLock==4)return false;
    if(state.vcs&&(state.helicopter||state.hovercraft)&&
        (!std::isfinite(state.heightAboveGround)||state.heightAboveGround>2.0f))return false;
    const bool onSide=state.upZ>=-.1f && state.upZ<=.1f;
    if(state.boat && ((state.vcs&&!state.jetSki) || (!state.vcs&&!onSide)))return true;
    const float speed=storylandVehicleNativeSpeedSquared(state.velocity);
    if(onSide ? speed>=.005f : !jumpExit && speed>.005f)return false;
    const float limit=state.vcs?.03f:.01f;
    for(float component:state.angularVelocity) {
        if(!std::isfinite(component))return false;
        const float native=std::fabs(component)/50.0f;
        if(onSide ? native>=limit : native>limit)return false;
    }
    return true;
}

inline bool storylandRetailVehicleCanJumpOut(const StorylandVehicleAccessState& state,
    bool bikePassenger=false) {
    if(state.carried || state.boat || state.doorLock==4 || !std::isfinite(state.upZ))return false;
    // CHeli::CanPedJumpOutCar, 2EB9D8: no road-speed or height window.
    if(state.vcs&&state.helicopter)return state.upZ>=.3f;
    if(state.vcs&&state.hovercraft) {
        // CPlane3F9C68, model-info hover flag: planar speed strictly above .3.
        if(!std::isfinite(state.velocity[0])||!std::isfinite(state.velocity[1]))return false;
        const float x=state.velocity[0]/50.0f,y=state.velocity[1]/50.0f;
        return std::sqrt(x*x+y*y)>.3f;
    }
    const float speed=storylandVehicleNativeSpeedSquared(state.velocity);
    if(state.twoWheels) {
        // LCS 0x32A810: a passenger prevents the rider's rear bail-out.
        // The VCS special vehicle flag preceding its common speed branch has
        // not been identified; use the common validated speed window there.
        if(bikePassenger)return false;
        return state.vcs ? state.upZ>=.3f && state.heightAboveGround<=15 && speed>=.1f && speed<=.5f :
            speed>=.07f;
    }
    // LCS 0x32A870; VCS 0x3E9D68 adds a 15m road-height rejection.
    return state.upZ>=.3f && (!state.vcs || state.heightAboveGround<=15) &&
        speed>=.1f && speed<=.5f;
}

inline StorylandVehicleEntryVariant storylandRetailBikeMountVariant(float rightZ, float upZ,
    float localForward, float localLeft, float moveRatio, bool rightHandSide) {
    if(!std::isfinite(rightZ) || !std::isfinite(upZ) || !std::isfinite(moveRatio))
        return StorylandVehicleEntryVariant::Normal;
    if(std::fabs(rightZ)>=.5f || upZ<=0) {
        const bool liftTowardPed=rightHandSide ? rightZ<0 : rightZ>0;
        return liftTowardPed ? StorylandVehicleEntryVariant::BikePickup :
            StorylandVehicleEntryVariant::BikePullup;
    }
    const float distance=std::hypot(localForward,localLeft);
    if(std::fabs(rightZ)<.1f && moveRatio>1.5f && distance>0 &&
        localForward/distance>std::cos(.52359877559829887308f))
        return StorylandVehicleEntryVariant::BikeFront;
    return StorylandVehicleEntryVariant::Normal;
}

inline unsigned storylandVehicleSeatDoor(unsigned nativeSeat, bool rightHandSide) {
    return (nativeSeat>=2 ? 2u : 0u)+(rightHandSide ? 1u : 0u);
}

inline std::array<float,3> storylandVehicleEntrySeatTarget(const StorylandVehicleDefinition& definition,
    StorylandVehicleEntryVariant variant, StorylandVehiclePhase phase, float phaseSeconds,
    bool doorMissing=false) {
    if(variant!=StorylandVehicleEntryVariant::PassengerSideDriver)return {};
    const auto driver=definition.seatPosition(0),passenger=definition.seatPosition(1);
    float weight=1;
    if(phase==StorylandVehiclePhase::CloseInside) {
        const float close=doorMissing?0:definition.sideTransitionTiming[1].seconds[unsigned(StorylandVehiclePhase::CloseInside)];
        weight=definition.shuffleSeconds>0?1-std::clamp((phaseSeconds-close)/definition.shuffleSeconds,0.0f,1.0f):0;
    } else if(phase!=StorylandVehiclePhase::GetIn)return {};
    return {(passenger[0]-driver[0])*weight,(passenger[1]-driver[1])*weight,
        (passenger[2]-driver[2])*weight};
}
