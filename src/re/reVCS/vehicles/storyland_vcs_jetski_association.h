#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace storyland_vcs_jetski_association {

enum Flag : uint32_t {
    Running = 0x1,
    Repeat = 0x2,
    DeleteWhenFaded = 0x4,
    FadeWhenFinished = 0x8,
    Partial = 0x10,
    Synchronised = 0x20,
    Independent = 0x100000
};

struct TimerInput {
    float rawNativeStep = 0;
    bool pauseA = false;
    bool pauseB = false;
    bool minimumDisabled = false;
    int32_t mode = 0;
    bool fixedStep = false;
};

struct TimerOutput {
    float unclippedNativeStep = 0;
    float physicsNativeStep = 0;
};

struct Clip {
    float duration = 0;
    uint32_t templateFlags = 0;
};

struct State {
    bool present = false;
    float weight = 1;
    float blendDelta = 0;
    float time = 0;
    float playbackSpeed = 1;
    float timeAdvance = 0;
    uint32_t flags = 0;
    uint32_t callbackType = 0;
};

struct Events {
    bool removed = false;
    bool finishCallback = false;
    bool fadeCallback = false;
};

struct SynchronisationSource {
    float time = 0;
    float duration = 0;
};

inline bool valid(const Clip& clip) {
    return std::isfinite(clip.duration) && clip.duration > 0;
}

inline bool valid(const State& state) {
    return !state.present || (std::isfinite(state.weight) &&
        std::isfinite(state.blendDelta) && std::isfinite(state.time) &&
        std::isfinite(state.playbackSpeed) && std::isfinite(state.timeAdvance));
}

inline bool publishTimer(const TimerInput& input, TimerOutput& output) {
    if (!std::isfinite(input.rawNativeStep) || input.rawNativeStep < 0) return false;
    TimerOutput result;
    result.unclippedNativeStep = input.rawNativeStep;
    if (result.unclippedNativeStep < .5f &&
        !input.pauseA && !input.pauseB && !input.minimumDisabled)
        result.unclippedNativeStep = .5f;
    result.physicsNativeStep = result.unclippedNativeStep;
    if (input.mode != 2)
        result.physicsNativeStep = std::min(result.physicsNativeStep, 3.333333492279052734375f);
    if (input.fixedStep) result.physicsNativeStep = 1;
    output = result;
    return true;
}

inline bool animationSeconds(float unclippedNativeStep, float& output) {
    if (!std::isfinite(unclippedNativeStep) || unclippedNativeStep < 0) return false;
    const float seconds = unclippedNativeStep * .02f;
    if (!std::isfinite(seconds)) return false;
    output = seconds;
    return true;
}

inline bool initialise(const Clip& clip, float requestedRate, bool otherFading,
    const SynchronisationSource* source, State& output) {
    if (!valid(clip) || !std::isfinite(requestedRate) || requestedRate < 0) return false;
    State result;
    result.present = true;
    result.flags = clip.templateFlags | Running;
    if ((clip.templateFlags & Synchronised) && source) {
        if (!std::isfinite(source->time) || source->time < 0 ||
            !std::isfinite(source->duration) || source->duration <= 0) return false;
        const float fraction = source->time / source->duration;
        result.time = fraction * clip.duration;
        if (!std::isfinite(result.time)) return false;
        if (result.flags & Repeat) {
            unsigned reductions = 0;
            while (result.time >= clip.duration) {
                if (++reductions > 4096) return false;
                result.time -= clip.duration;
            }
        } else if (result.time >= clip.duration) {
            result.time = clip.duration;
        }
    }
    if (otherFading || (clip.templateFlags & Partial)) {
        result.weight = 0;
        result.blendDelta = requestedRate;
    }
    output = result;
    return true;
}

inline bool reblend(const Clip& clip, float requestedRate, State& state) {
    if (!valid(clip) || !valid(state) || !state.present ||
        !std::isfinite(requestedRate) || requestedRate < 0) return false;
    State result = state;
    const float remainingWeight = 1.0f - result.weight;
    result.blendDelta = remainingWeight * requestedRate;
    if (!(result.flags & Running) && result.time == clip.duration) {
        result.flags |= Running;
        result.time = 0;
    }
    if (!valid(result)) return false;
    state = result;
    return true;
}

inline bool fadeOther(uint32_t selectedTemplateFlags, float requestedRate,
    State& other, bool& fading) {
    if (!valid(other) || !std::isfinite(requestedRate) || requestedRate < 0) return false;
    fading = false;
    if (!other.present || (selectedTemplateFlags & Independent) ||
        (other.flags & Independent) ||
        bool(selectedTemplateFlags & Partial) != bool(other.flags & Partial)) return true;
    State result = other;
    if (result.weight > 0) {
        const float nextDelta = -requestedRate * result.weight;
        if (nextDelta < result.blendDelta || !(selectedTemplateFlags & Partial))
            result.blendDelta = nextDelta;
    } else {
        result.blendDelta = -1;
    }
    result.flags |= DeleteWhenFaded;
    if (!valid(result)) return false;
    other = result;
    fading = true;
    return true;
}

inline bool updateWeight(float seconds, State& state, Events& events) {
    if (!std::isfinite(seconds) || !valid(state)) return false;
    State result = state;
    Events next;
    if (result.present && seconds >= 0) {
        const float change = result.blendDelta * seconds;
        if (!std::isfinite(change)) return false;
        result.weight += change;
        if (!std::isfinite(result.weight)) return false;
        if (result.weight <= 0 && result.blendDelta < 0) {
            result.weight = 0;
            result.blendDelta = 0;
            if (result.flags & DeleteWhenFaded) {
                next.fadeCallback = result.callbackType == 1 || result.callbackType == 2;
                next.removed = true;
                result.present = false;
            }
        }
        if (result.present && result.weight > 1) {
            result.weight = 1;
            if (result.blendDelta > 0) result.blendDelta = 0;
        }
    }
    if (!valid(result)) return false;
    state = result;
    events = next;
    return true;
}

inline bool updateTime(const Clip& clip, float seconds, float synchronisedRate,
    State& state, Events& events) {
    if (!valid(clip) || !valid(state) || !std::isfinite(seconds) ||
        !std::isfinite(synchronisedRate)) return false;
    State result = state;
    Events next;
    if (result.present && (result.flags & Running)) {
        if (result.time >= clip.duration) {
            result.flags &= ~Running;
        } else {
            const float speed = result.flags & Synchronised ?
                clip.duration * synchronisedRate : result.playbackSpeed;
            if (!std::isfinite(speed)) return false;
            result.timeAdvance = speed * seconds;
            if (!std::isfinite(result.timeAdvance)) return false;
            result.time += result.timeAdvance;
            if (!std::isfinite(result.time)) return false;
            if (result.time >= clip.duration) {
                if (result.flags & Repeat) {
                    result.time -= clip.duration;
                } else {
                    result.time = clip.duration;
                    if (result.flags & FadeWhenFinished) {
                        result.flags |= DeleteWhenFaded;
                        result.blendDelta = -4;
                    }
                    if (result.callbackType == 1) {
                        result.callbackType = 0;
                        next.finishCallback = true;
                    }
                }
            }
        }
    }
    if (!valid(result)) return false;
    state = result;
    events = next;
    return true;
}

inline bool beforePose(float seconds, State& state, Events& events) {
    if (!std::isfinite(seconds) || !valid(state)) return false;
    if (seconds == 0) {
        events = {};
        return true;
    }
    return updateWeight(seconds, state, events);
}

inline bool afterPose(const Clip& clip, float seconds, float synchronisedRate,
    State& state, Events& events) {
    if (!valid(clip) || !std::isfinite(seconds) || !std::isfinite(synchronisedRate) ||
        !valid(state)) return false;
    if (seconds <= 0) {
        events = {};
        return true;
    }
    return updateTime(clip, seconds, synchronisedRate, state, events);
}

}
