#pragma once

#include "src/re/reVCS/camera/storyland_vcs_intoxication.h"

namespace StorylandSprint {

struct State {
    int movementState = 1;
    float walkStartTime = -1.0f;
    float walkStartDuration = 0.0f;
    float tiredTime = -1.0f;
    float tiredBlend = 0.0f;
    bool association = false;
    float blendDelta = 0.0f;
    float tiredBlendDelta = 0.0f;

    bool walkStartRunning() const {
        return walkStartTime >= 0.0f && walkStartTime < walkStartDuration;
    }
    int pose(bool vcs) const {
        if (walkStartRunning()) return vcs ? 7 : 4;
        return tiredBlend > 0.0f ? (vcs ? 12 : 9) : -1;
    }
    float poseTime() const {
        return walkStartRunning() ? walkStartTime : tiredTime;
    }
    float poseWeight() const {
        return walkStartRunning() ? 1.0f : tiredBlend;
    }
};

inline void drain(float& energy, float& maximum, float& training,
    float nativeStep, bool neverTired, bool adrenaline, float minimumEnergy = -100.0f) {
    if (energy > minimumEnergy && !neverTired && !adrenaline) {
        const float amount = StorylandVcsIntoxication::truncatedFloat(double(nativeStep) * .5);
        energy = StorylandVcsIntoxication::truncatedFloat(double(energy) - amount);
        training = StorylandVcsIntoxication::truncatedFloat(double(training) + amount);
    }
    if (training >= 500.0f) {
        training = 0.0f;
        if (maximum < 2000.0f)
            maximum = StorylandVcsIntoxication::truncatedFloat(double(maximum) + 10);
    }
}

inline void recover(float& energy, float maximum, float nativeStep,
    float amount, uint8_t intoxication) {
    amount = StorylandVcsIntoxication::recoveryAmount(intoxication, amount);
    if (energy < maximum) {
        const float scaled = StorylandVcsIntoxication::truncatedFloat(double(amount) * nativeStep);
        const float change = StorylandVcsIntoxication::truncatedFloat(double(scaled) * .5);
        energy = StorylandVcsIntoxication::truncatedFloat(double(energy) + change);
    }
}

inline int moveState(int requested, float energy, bool sprintAssociation,
    bool sprintFading, bool walkStartRunning, float minimumEnergy = -100.0f) {
    if (requested != 5) return requested;
    if (energy < 0 && (energy <= minimumEnergy || !sprintAssociation || sprintFading)) return 1;
    return walkStartRunning ? 1 : 5;
}

inline int finishMoveState(int current, bool sprintAssociation,
    bool walkStartRunning, float moveRatio) {
    if (current == 5 || sprintAssociation || walkStartRunning) return current;
    return moveRatio >= 1.0f ? 4 : moveRatio > 0.0f ? 2 : 1;
}

inline float advanceBlend(State& state, float weight, float seconds) {
    if (!state.association) return 0;
    if (seconds < 0) return weight;
    const float amount = StorylandVcsIntoxication::truncatedFloat(double(state.blendDelta) * seconds);
    const float result = StorylandVcsIntoxication::truncatedFloat(double(weight) + amount);
    if (result <= 0 && state.blendDelta < 0) {
        state.association = false;
        state.blendDelta = 0;
    } else if (result > 1 && state.blendDelta > 0) state.blendDelta = 0;
    return std::clamp(result, 0.0f, 1.0f);
}

inline bool controlBlend(State& state, float& weight, float moveRatio) {
    if (!state.association) {
        if (state.movementState != 5) return false;
        state.association = true;
        state.blendDelta = 2;
        weight = 0;
        return true;
    }
    if (state.movementState == 5 && moveRatio >= .4f) {
        if (state.blendDelta < 0) state.blendDelta = 2;
        return true;
    }
    if (weight <= 0) {
        state.blendDelta = -1000;
        return false;
    }
    if (state.blendDelta < 0 && weight < .8f) {
        if (moveRatio < 1) state.blendDelta = -8;
        return false;
    }
    if (moveRatio >= .4f && state.blendDelta >= 0) state.blendDelta = -1;
    return false;
}

inline void advanceTired(State& state, float seconds) {
    if (state.tiredTime < 0 || seconds < 0) return;
    state.tiredTime += seconds;
    const float amount = StorylandVcsIntoxication::truncatedFloat(double(state.tiredBlendDelta) * seconds);
    const float weight = StorylandVcsIntoxication::truncatedFloat(double(state.tiredBlend) + amount);
    if (weight <= 0 && state.tiredBlendDelta < 0) {
        state.tiredTime = -1;
        state.tiredBlendDelta = 0;
    } else if (weight > 1 && state.tiredBlendDelta > 0) state.tiredBlendDelta = 0;
    state.tiredBlend = std::clamp(weight, 0.0f, 1.0f);
}

inline void controlTired(State& state, bool target, float rate) {
    if (target) {
        if (state.tiredTime < 0) {
            state.tiredTime = 0;
            state.tiredBlend = 0;
        }
        const float remaining = StorylandVcsIntoxication::truncatedFloat(1.0 - double(state.tiredBlend));
        state.tiredBlendDelta = StorylandVcsIntoxication::truncatedFloat(double(remaining) * rate);
    } else if (state.tiredTime >= 0 && state.tiredBlendDelta >= 0) {
        state.tiredBlendDelta = StorylandVcsIntoxication::truncatedFloat(-double(state.tiredBlend) * rate);
    }
}

}
