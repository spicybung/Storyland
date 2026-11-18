#pragma once
#include "src/storyland/storyland_vehicle_transition.h"
#include <limits>

inline float storylandVehicleBailoutDetachTime(bool vcs, bool motorcycle, uint32_t model) {
    if (!motorcycle) return .07f;
    // SLUS_214.23 314E58..314EA4; SLUS_215.90 30344C..303498.
    const bool longerBackExit = vcs ? model == 229 || model == 231 || model == 232 :
        model == 202 || model == 206 || model == 207 || model == 208;
    return longerBackExit ? .55f : .35f;
}

inline bool storylandVehicleBailoutDetaches(bool vcs, bool motorcycle,
    uint32_t model, float elapsed) {
    return std::isfinite(elapsed) && elapsed > storylandVehicleBailoutDetachTime(vcs, motorcycle, model);
}

inline int32_t storylandVehicleExitGetUpAnimation(uint16_t flags) {
    // Both original SetGetUp functions inspect ASSOC_FRONTAL.
    return (flags & 0x1000u) ? 135 : 132;
}

inline uint32_t storylandVehiclePlayerFallDeadline(uint32_t now, float duration,
    float elapsed, bool rollout) {
    if (!std::isfinite(duration) || duration <= 0 || !std::isfinite(elapsed) || elapsed < 0)
        return UINT32_MAX;
    // LCS 1E2DE0..1E2EAC; VCS 2024B0..20258C.
    const float lengthMilliseconds = duration * 1000.0f;
    float deadline = float(now) + lengthMilliseconds;
    if (rollout) {
        const float elapsedMilliseconds = elapsed * 1000.0f;
        deadline -= elapsedMilliseconds;
        deadline += 100.0f;
    } else {
        deadline += 500.0f;
    }
    if (!std::isfinite(deadline) || deadline >= float(std::numeric_limits<int32_t>::max()))
        return uint32_t(std::numeric_limits<int32_t>::max());
    return deadline <= 0 ? 0u : uint32_t(int32_t(deadline));
}

inline bool storylandVehicleExitCanGetUp(const StorylandVehicleExitPose& pose,
    uint32_t now, bool grounded, bool alive) {
    return alive && grounded && pose.recovery == StorylandVehicleExitRecovery::Fallen &&
        pose.getUpAfter != UINT32_MAX && pose.getUpAfter < now;
}

inline float storylandVehicleExitPoseBlend(const StorylandVehicleExitPose& pose) {
    return pose.recovery == StorylandVehicleExitRecovery::GetUp ? 1000.0f : 8.0f;
}
