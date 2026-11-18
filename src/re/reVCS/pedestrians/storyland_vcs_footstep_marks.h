#pragma once

#include <cmath>
#include <cstdint>

struct StorylandVcsFootstepMarkState {
    uint8_t mode = 0;
    uint32_t remaining = 0;
};

inline void storylandSetVcsFootstepMarks(StorylandVcsFootstepMarkState& state,
    int32_t mode, int32_t remaining) {
    // PS2 41D508 writes CPed+1D8 bits12..13 and the full CPed+6E8 word.
    state.mode = uint8_t(uint32_t(mode) & 3u);
    state.remaining = uint32_t(remaining);
}

inline bool storylandVcsFootstepStateUpdates(int32_t nativePedState) {
    return nativePedState != 56 && nativePedState != 61 && nativePedState != 63 &&
        nativePedState != 46 && nativePedState != 45 && nativePedState != 48;
}

inline void storylandAdvanceVcsFootstepMarkCounter(StorylandVcsFootstepMarkState& state) {
    // 1FA9F0 decrements once per eligible ped update, without timestep scaling.
    if (state.mode && state.remaining && state.remaining < 300) {
        --state.remaining;
        if (!state.remaining) state.mode = 0;
    }
}

inline void storylandConsumeVcsFootstepMarks(StorylandVcsFootstepMarkState& state,
    uint32_t amount) {
    if (!state.mode) return;
    if (state.remaining > amount) state.remaining -= amount;
    else state = {};
}

inline unsigned storylandVcsFootstepCrossing(float currentTime, float timeStep,
    float duration) {
    if (!std::isfinite(currentTime) || !std::isfinite(timeStep) ||
        !std::isfinite(duration) || timeStep <= 0 || duration <= 0) return 0;
    // 1FAD7C and 1FB044 test the actual association time and its latest step.
    constexpr float first = 0.066666670143604278564453125f;
    const float second = duration * .5f + first;
    const float previous = currentTime - timeStep;
    if (first <= currentTime && previous < first) return 1;
    if (second <= currentTime && previous < second) return 2;
    return 0;
}

inline unsigned storylandObserveVcsFootstepMarks(StorylandVcsFootstepMarkState& state,
    int32_t pedStat, float currentTime, float timeStep, float duration) {
    if (!state.mode || pedStat == 39) return 0;
    const unsigned foot = storylandVcsFootstepCrossing(currentTime, timeStep, duration);
    if (foot) storylandConsumeVcsFootstepMarks(state, 20);
    return foot;
}
