#pragma once
#include "src/storyland/storyland_play_test_camera.h"
#include "src/re/reVCS/world/storyland_vcs_water_depth.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

enum class StorylandVcsEjectionObjective : int32_t {
    Preserve = -1,
    None = 0,
    EnterPassenger = 17,
    EnterDriver = 18
};

inline bool storylandVcsEjectionFlag(int32_t value) {
    return (uint32_t(value) & 1u) != 0;
}

inline StorylandVcsEjectionObjective storylandVcsEjectionObjective(
    bool player, bool forced, bool hasLeader, bool dontReenter, bool driver) {
    // SLUS-21590 3F0AB0..3F0B14, CPed+1D8 bit35.
    if (player || forced || hasLeader) return StorylandVcsEjectionObjective::Preserve;
    if (dontReenter) return StorylandVcsEjectionObjective::None;
    return driver ? StorylandVcsEjectionObjective::EnterDriver :
        StorylandVcsEjectionObjective::EnterPassenger;
}

struct StorylandVcsPedFallState {
    int32_t sourceVehicle = -1;
    int32_t fallAnimation = -1;
    int32_t getUpAnimation = 132;
    float time = 0;
    float fallDuration = 0;
    float getUpDuration = 0;
    uint32_t getUpAfter = 0;
    std::array<float,3> velocity{};
    bool grounded = false;
    bool recovering = false;

    int32_t animation() const { return recovering ? getUpAnimation : fallAnimation; }
    float duration() const { return recovering ? getUpDuration : fallDuration; }

    template<class Archive> void saveState(Archive& archive) {
        archive(sourceVehicle, fallAnimation, getUpAnimation, time, fallDuration,
            getUpDuration, getUpAfter, velocity, grounded, recovering);
    }
};

inline bool storylandVcsBikeFallAnimation(int32_t animation) {
    return animation == 26 || animation == 27 || animation == 29 || animation == 30 ||
        animation == 31 || animation == 120 || animation == 121 || animation == 139;
}

inline int32_t storylandVcsPedGetUpAnimation(uint16_t associationFlags) {
    // SetGetUp203820..203864 checks the current association's frontal flag.
    return (associationFlags & 0x1000u) != 0 ? 135 : 132;
}

inline uint32_t storylandVcsPedGetUpDeadline(uint32_t now, float duration,
    uint8_t entitySeed, uint32_t frameCounter, uint32_t downMilliseconds = 1000) {
    // SetFall202598..20266C uses EE single-precision arithmetic and CVT.W.S.
    const auto truncateFloat = [](double value) {
        float rounded = float(value);
        if (std::isfinite(rounded) && std::fabs(double(rounded)) > std::fabs(value))
            rounded = std::nextafter(rounded, 0.0f);
        return rounded;
    };
    const float nativeNow = truncateFloat(double(now));
    const float milliseconds = truncateFloat(double(duration) * 1000.0);
    float deadline = truncateFloat(double(nativeNow) + milliseconds);
    deadline = truncateFloat(double(deadline) + double(downMilliseconds));
    deadline = truncateFloat(double(deadline) + float((uint32_t(entitySeed) + frameCounter) % 1000u));
    if (!std::isfinite(deadline) || deadline >= 2147483648.0f) return uint32_t(INT32_MAX);
    if (deadline <= -2147483648.0f) return uint32_t(INT32_MIN);
    return uint32_t(int32_t(deadline));
}

inline bool storylandVcsPedCanGetUp(const StorylandVcsPedFallState& state,
    uint32_t now, bool alive) {
    // Fall202808..202868 requires support and a strict unsigned deadline.
    return alive && state.grounded && !state.recovering &&
        state.getUpAfter != UINT32_MAX && state.getUpAfter < now;
}

inline bool storylandValidVcsPedFall(const StorylandVcsPedFallState& state) {
    const bool carjackedRecovery=state.fallAnimation>=59&&state.fallAnimation<=62;
    return state.sourceVehicle >= -1 && (storylandVcsBikeFallAnimation(state.fallAnimation)||carjackedRecovery) &&
        (state.getUpAnimation == 132 || state.getUpAnimation == 135) &&
        std::isfinite(state.time) && state.time >= 0 && state.time <= state.duration() &&
        std::isfinite(state.fallDuration) && state.fallDuration > 0 &&
        std::isfinite(state.getUpDuration) && state.getUpDuration > 0 &&
        std::all_of(state.velocity.begin(), state.velocity.end(),
            [](float value) { return std::isfinite(value); });
}

struct StorylandVcsPedSupportProbe {
    bool tested = false;
    bool supported = true;
    bool water = false;
    StorylandNativePoint forwardPoint;
    StorylandNativePoint top;
    StorylandNativePoint bottom;
};

template<class WaterHeight, class BuildingsHit>
inline StorylandVcsPedSupportProbe storylandVcsPedForwardSupportProbe(
    bool enabled, bool alive, bool seated, const StorylandNativePoint& position,
    const StorylandNativePoint& destination, const StorylandNativePoint& forward,
    WaterHeight waterHeight, BuildingsHit buildingsHit) {
    StorylandVcsPedSupportProbe result;
    if (!enabled || !alive || seated) return result;
    const float dx = destination.x - position.x;
    const float dy = destination.y - position.y;
    const float dz = destination.z - position.z;
    const float length = std::hypot(dx, dy, dz);
    if (!(length > 0) || !std::isfinite(length)) return result;
    const float alignment = (dx * forward.x + dy * forward.y + dz * forward.z) / length;
    // Native 205130..205180 uses float bits3F7089A0 and a strict comparison.
    if (!(alignment > 0.9395999908447265625f)) return result;
    result.tested = true;
    result.forwardPoint = {position.x + forward.x, position.y + forward.y,
        position.z + forward.z};
    float water = 0;
    result.water = waterHeight(result.forwardPoint, water);
    if (result.water) {
        result.top = {result.forwardPoint.x, result.forwardPoint.y, water};
        result.bottom = result.forwardPoint;
    } else {
        // 40D750 returns0.3;3993C8 initializes7094E0 to0.3+1.5.
        result.top = {result.forwardPoint.x, result.forwardPoint.y,
            result.forwardPoint.z + 0.3f};
        result.bottom = {result.forwardPoint.x, result.forwardPoint.y,
            result.forwardPoint.z - 1.5f};
    }
    result.supported = buildingsHit(result.top, result.bottom);
    return result;
}

inline StorylandVcsPedSupportProbe storylandVcsPedForwardSupportProbe(
    const StorylandNativeWorld& world, bool enabled, bool alive, bool seated,
    const StorylandNativePoint& position, const StorylandNativePoint& destination,
    float heading, float originOffset) {
    constexpr float radians = 0.01745329251994329577f;
    const float angle = heading * radians;
    const StorylandNativePoint forward{-std::sin(angle), std::cos(angle), 0};
    const StorylandNativePoint nativePosition{position.x, position.y, position.z + originOffset};
    const StorylandNativePoint nativeDestination{destination.x, destination.y, destination.z + originOffset};
    return storylandVcsPedForwardSupportProbe(enabled, alive, seated, nativePosition, nativeDestination,
        forward,
        [&](const StorylandNativePoint& point, float& height) {
            return storylandNativeWaterHeight(world, point.x, point.y, height) &&
                storylandVcsWaterDepthAllowsContact(point.z, height);
        },
        [&](const StorylandNativePoint& top, const StorylandNativePoint& bottom) {
            return storylandTraceNativeBuildings(world, top, bottom) < 1.0f;
        });
}
