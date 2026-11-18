#pragma once
#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <cstdint>
#include <string>
#include <vector>

enum class StorylandPlayerRestartCause : uint8_t { None, Death, Arrest, Critical };

struct StorylandPlayerRestartState {
    StorylandPlayerRestartCause cause = StorylandPlayerRestartCause::None;
    uint32_t startedAt = 0, elapsedMilliseconds = 0, fadeInStartedAt = 0;
    int wantedAtStart = 0;
    bool resurrected = false, fadeOutStarted = false, controlReleased = false;
    bool introMissionRestart = false;
    bool active() const { return cause != StorylandPlayerRestartCause::None; }
    bool canBegin() const { return !active() || controlReleased; }
};

struct StorylandPlayerRestartStep {
    bool fadeOutStarted = false, resurrectionRequested = false;
    bool controlReleased = false, finished = false;
};

struct StorylandPlayerRestartPresentation {
    float fadeAlpha = 0;
    std::array<uint8_t,3> fadeColor{0,0,0};
    bool showMessage = false;
    const char* messageKey = "";
};

struct StorylandPlayerRestartPolicy {
    int fee = 0;
    uint32_t minutesPassed = 0;
    bool consumeHospitalPass = false, consumeJailPass = false;
    bool clearWeapons = false, storeReturnableWeapons = false;
};

struct StorylandPlayerRestartPoint {
    StorylandNativePoint position{};
    float heading = 0;
    bool overridden = false;
};

enum class StorylandRestartTaxiPhase : uint8_t {
    None, AwaitModel, Waiting, Entering, FadingOut, Arriving, Exiting
};

struct StorylandRestartTaxiState {
    StorylandRestartTaxiPhase phase = StorylandRestartTaxiPhase::None;
    StorylandNativePoint start{},destination{};
    float startHeading = 0,destinationHeading = 0;
    int32_t vehicle = -1,driver = -1,blip = -1;
    uint32_t deadline = 0;
    bool holdsControl = false,previousPlayerControl = true,previousWorldControl = true;
    bool active() const { return phase != StorylandRestartTaxiPhase::None; }
    template<class Archive> void saveState(Archive& archive) {
        archive(phase,start.x,start.y,start.z,destination.x,destination.y,destination.z,
            startHeading,destinationHeading,vehicle,driver,blip,deadline,holdsControl,
            previousPlayerControl,previousWorldControl);
    }
};

inline bool storylandValidRestartTaxiState(const StorylandRestartTaxiState& state) {
    if(unsigned(state.phase) > unsigned(StorylandRestartTaxiPhase::Exiting) ||
        !std::isfinite(state.start.x) || !std::isfinite(state.start.y) || !std::isfinite(state.start.z) ||
        !std::isfinite(state.destination.x) || !std::isfinite(state.destination.y) || !std::isfinite(state.destination.z) ||
        !std::isfinite(state.startHeading) || !std::isfinite(state.destinationHeading) ||
        state.vehicle < -1 || state.driver < -1) return false;
    if(state.phase == StorylandRestartTaxiPhase::None || state.phase == StorylandRestartTaxiPhase::AwaitModel)
        return state.vehicle == -1 && state.driver == -1 && state.blip == -1 && !state.holdsControl;
    return state.vehicle > 0 && state.driver > 0 &&
        state.holdsControl == (state.phase != StorylandRestartTaxiPhase::Waiting);
}

inline bool storylandBeginPlayerRestart(StorylandPlayerRestartState& state,
    StorylandPlayerRestartCause cause,uint32_t now,int wanted) {
    if(!state.canBegin() || cause == StorylandPlayerRestartCause::None) return false;
    state = {};
    state.cause = cause;
    state.startedAt = now;
    state.wantedAtStart = std::clamp(wanted,0,6);
    return true;
}

inline StorylandPlayerRestartStep storylandStepPlayerRestart(
    StorylandPlayerRestartState& state,uint32_t now) {
    StorylandPlayerRestartStep result;
    if(!state.active()) return result;
    state.elapsedMilliseconds = uint32_t(now - state.startedAt);
    // VCS 0x24C534 / LCS 0x205728: strict elapsed > 0x800.
    if(!state.fadeOutStarted && state.elapsedMilliseconds > 2048) {
        state.fadeOutStarted = true;
        result.fadeOutStarted = true;
    }
    result.resurrectionRequested = !state.resurrected && state.elapsedMilliseconds >= 4096;
    // Both PS2 executables retain the restart state for another 0x200 ms.
    if(state.resurrected && !state.controlReleased && state.elapsedMilliseconds >= 4608) {
        state.controlReleased = true;
        state.fadeInStartedAt = now;
        result.controlReleased = true;
    }
    if(state.controlReleased && uint32_t(now - state.fadeInStartedAt) >= 3000) {
        state = {};
        result.finished = true;
    }
    return result;
}

inline StorylandPlayerRestartPresentation storylandPlayerRestartPresentation(
    const StorylandPlayerRestartState& state) {
    StorylandPlayerRestartPresentation result;
    if(!state.active()) return result;
    const bool death = state.cause == StorylandPlayerRestartCause::Death;
    result.fadeColor = death ? std::array<uint8_t,3>{200,200,200} :
        std::array<uint8_t,3>{0,0,0};
    result.messageKey = death ? "DEAD" :
        state.cause == StorylandPlayerRestartCause::Arrest ? "BUSTED" : "";
    const uint32_t messageDuration = death ? 4000 : 5000;
    result.showMessage = !state.resurrected && state.elapsedMilliseconds < messageDuration &&
        state.cause != StorylandPlayerRestartCause::Critical;
    if(state.controlReleased) {
        const uint32_t releaseElapsed = uint32_t(state.fadeInStartedAt - state.startedAt);
        const uint32_t fadeElapsed = state.elapsedMilliseconds >= releaseElapsed ?
            state.elapsedMilliseconds - releaseElapsed : 0;
        result.fadeAlpha = 1.0f - std::clamp(float(fadeElapsed) / 3000.0f,0.0f,1.0f);
    } else if(state.elapsedMilliseconds > 2048) {
        result.fadeAlpha = std::clamp(float(state.elapsedMilliseconds - 2048) / 2000.0f,0.0f,1.0f);
    }
    return result;
}

inline StorylandPlayerRestartPolicy storylandPlayerRestartPolicy(
    StorylandPlayerRestartCause cause,int wanted,bool hospitalPass,bool jailPass,bool vcs) {
    StorylandPlayerRestartPolicy result;
    if(cause == StorylandPlayerRestartCause::None || cause == StorylandPlayerRestartCause::Critical)
        return result;
    result.minutesPassed = 720;
    if(cause == StorylandPlayerRestartCause::Death) {
        result.consumeHospitalPass = hospitalPass;
        result.fee = hospitalPass ? 0 : 100;
        result.clearWeapons = !hospitalPass;
        result.storeReturnableWeapons = vcs && !hospitalPass;
    } else {
        constexpr int fees[]{100,100,200,400,600,900,1500};
        result.consumeJailPass = jailPass;
        result.fee = jailPass ? 0 : fees[std::clamp(wanted,0,6)];
        // VCS 0x24CBB4 and LCS 0x205C5C clear after resurrection even on a free release.
        result.clearWeapons = true;
        result.storeReturnableWeapons = vcs && !jailPass;
    }
    return result;
}

inline uint32_t storylandAdvancePickupRestartTimer(uint32_t deadline,uint32_t passed,uint32_t now) {
    if(deadline == 0) return 0;
    const uint32_t adjusted = deadline > passed ? deadline - passed : 0;
    // Native zero is an expired timer, whereas the host uses zero for no timer.
    if(adjusted) return adjusted;
    return now ? now : UINT32_MAX;
}

inline bool storylandNormalizeRestartHeight(
    const std::function<bool(const std::array<float,3>&,float&)>& groundHeightAt,
    float x,float y,float& z,bool vcs,std::string& error) {
    error.clear();
    if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
        error = "Native restart coordinates must be finite.";
        return false;
    }
    const bool sentinel = vcs ? (z == -100.0f || z <= -250.0f) : z <= -100.0f;
    if(!sentinel) return true;
    if(!groundHeightAt) {
        error = "Native restart sentinel height requires loaded ground collision.";
        return false;
    }
    float ground = 0;
    if(groundHeightAt({x,y,1000.0f},ground)) {
        if(!std::isfinite(ground)) {
            error = "Native restart ground height is not finite.";
            return false;
        }
        z = ground;
    } else z = 20.0f;
    return true;
}

struct StorylandPlayTestRuntime;
struct StorylandMissionEnvironment;

bool storylandBeginPlayerRestart(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,StorylandPlayerRestartState& state,
    StorylandPlayerRestartCause cause);
bool storylandChoosePlayerRestart(StorylandMissionEnvironment& world,
    const StorylandNativePoint& origin,StorylandPlayerRestartCause cause,bool vcs,
    StorylandPlayerRestartPoint& point,std::string& error);
bool storylandLoadPlayerRestartDefinitions(StorylandMissionEnvironment& world,
    const std::vector<uint8_t>& scm,uint32_t mainOffset,uint32_t mainEnd,bool vcs,
    std::string& error);
bool storylandApplyPlayerRestart(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,std::vector<StorylandNativeTrafficActor>& vehicles,
    StorylandPlayerRestartState& state,const StorylandNativePoint& origin,bool vcs,
    bool advanceClock,StorylandPlayerRestartPoint& point,std::string& error);
bool storylandPrepareIntroMissionRestart(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,const StorylandNativePoint& origin,bool vcs,
    std::string& error);
void storylandReleasePlayerRestartControl(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world);
void storylandApplyPlayerRestartPresentation(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,const StorylandPlayerRestartState& state);
bool storylandSetUpAfterDeathRestartTaxi(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,StorylandRestartTaxiState& state,
    const StorylandNativePoint& player,std::string& error);
bool storylandUpdateRestartTaxi(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,std::vector<StorylandNativeTrafficActor>& vehicles,
    StorylandRestartTaxiState& state,StorylandNativePoint& player,float& heading,
    std::string& error);
void storylandClearRestartTaxi(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,StorylandRestartTaxiState& state);
