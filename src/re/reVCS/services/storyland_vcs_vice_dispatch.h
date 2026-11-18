#pragma once

#include <cstdint>

// Original VCS police-vehicle selector at 0x432BE8 and vice crew creation
// at 0x42AC84. Selection is side-effect free: the shared sequence and clock
// only advance after the vehicle and both officers have been created.
struct StorylandVcsViceDispatchState {
    uint32_t lastCreatedAt = 0;
    uint32_t crewSequence = 0;
};

inline bool storylandVcsViceDispatchEligible(const StorylandVcsViceDispatchState& state,
    int wanted, uint32_t now, bool carLoaded, bool viceOneLoaded, bool viceTwoLoaded) {
    return wanted >= 3 && state.crewSequence < 4 &&
        state.lastCreatedAt + uint32_t(120000) < now &&
        carLoaded && viceOneLoaded && viceTwoLoaded;
}

inline uint32_t storylandVcsViceCrewModel(uint32_t sequence) {
    return sequence < 2 ? 117u : 118u;
}

inline void storylandCommitVcsViceDispatch(StorylandVcsViceDispatchState& state, uint32_t now) {
    state.crewSequence = (state.crewSequence + 1u) % 4u;
    state.lastCreatedAt = now;
}

// In the original close-pursuit branch at 0x429084, the driver and all
// passengers receive a leave-car objective when a law vehicle is within 14
// planar units of an on-foot player. Against an upright vehicle, its native
// speed must be below 0.05 units/step and the close-pursuit counter must be at
// least 101. Original 0x3E9878 bypasses that check if its up vector is <= -0.9.
inline bool storylandVcsViceExitGate(bool lawFlag,float planarDistance,bool playerInVehicle,
    bool targetBypassesStoppedCheck,float playerNativeSpeed,uint16_t stoppedCounter) {
    return lawFlag&&planarDistance<14.0f&&
        (!playerInVehicle||targetBypassesStoppedCheck||(playerNativeSpeed<.05f&&stoppedCounter>=101));
}
