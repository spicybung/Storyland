#pragma once

#include <array>
#include <cmath>

struct StorylandLcsStoppedPedState {
    int movementState=0;
    bool airborne=false,landing=false,standing=false;
    std::array<float,2> animationMoveDelta{};
    bool runStop=false,jumpLaunch=false,jumpGlide=false;
};

inline bool storylandLcsPedStopped(const StorylandLcsStoppedPedState& state) {
    return (state.movementState==0||state.movementState==1)&&
        !state.airborne&&!state.landing&&state.standing&&
        state.animationMoveDelta[0]==0.0f&&state.animationMoveDelta[1]==0.0f;
}

inline bool storylandLcsPlayerStopped(const StorylandLcsStoppedPedState& state) {
    return !state.runStop&&!state.jumpLaunch&&!state.jumpGlide&&storylandLcsPedStopped(state);
}

inline bool storylandLcsVehicleStopped(float distanceTravelled,float nativeTimeStep) {
    return std::isfinite(distanceTravelled)&&distanceTravelled>=0&&
        std::isfinite(nativeTimeStep)&&nativeTimeStep>=0&&
        0.01f*nativeTimeStep>=distanceTravelled;
}

struct StorylandScriptVehicleTravel {
    std::array<float,3> frameOrigin{};
    float distance=0;
    bool active=false,measured=false;

    void begin(const std::array<float,3>& position) {
        frameOrigin=position;
        distance=0;
        active=std::isfinite(position[0])&&std::isfinite(position[1])&&std::isfinite(position[2]);
        measured=false;
    }

    void finish(const std::array<float,3>& position) {
        if(!active)return;
        distance=std::hypot(position[0]-frameOrigin[0],position[1]-frameOrigin[1],
            position[2]-frameOrigin[2]);
        measured=std::isfinite(distance);
    }
};
