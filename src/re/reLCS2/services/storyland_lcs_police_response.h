#pragma once

#include <algorithm>
#include <cmath>

// LCS keeps a bounded pursuit roster, as indicated by the CopPursuit diagnostics
// in SLUS_214.23. These host-side sight and memory thresholds are intentionally
// isolated from the game's wanted/chaos tables: they are not asserted to be
// byte-for-byte constants from the stripped retail executable.
namespace storylandLcsPoliceResponse {

inline constexpr float OfficerSightDistance = 72.0f;
inline constexpr float OfficerHearingDistance = 25.0f;
inline constexpr float OfficerPursuitMemory = 18.0f;
inline constexpr float OfficerPursuitReleaseDistance = 155.0f;
inline constexpr float VehicleSightDistance = 95.0f;
inline constexpr float VehicleDispatchDistance = 38.0f;
inline constexpr float VehiclePursuitMemory = 21.0f;
inline constexpr float VehiclePursuitReleaseDistance = 185.0f;
inline constexpr float DirectVehicleInterceptDistance = 28.0f;

inline bool noticedOfficer(float distance, bool unobstructed, bool heardShot) {
    return std::isfinite(distance) && distance >= 0.0f &&
        ((distance <= OfficerSightDistance && unobstructed) ||
         (distance <= OfficerHearingDistance && heardShot));
}

inline bool continueOfficer(float distance, float elapsed, float lastContact) {
    return std::isfinite(distance) && std::isfinite(elapsed) && std::isfinite(lastContact) &&
        distance >= 0.0f && distance <= OfficerPursuitReleaseDistance && elapsed >= lastContact &&
        elapsed - lastContact <= OfficerPursuitMemory;
}

inline bool noticedVehicle(float distance, bool unobstructed) {
    return std::isfinite(distance) && distance >= 0.0f &&
        ((distance <= VehicleSightDistance && unobstructed) ||
         distance <= VehicleDispatchDistance);
}

inline bool continueVehicle(float distance, float elapsed, float lastContact) {
    return std::isfinite(distance) && std::isfinite(elapsed) && std::isfinite(lastContact) &&
        distance >= 0.0f && distance <= VehiclePursuitReleaseDistance && elapsed >= lastContact &&
        elapsed - lastContact <= VehiclePursuitMemory;
}

inline bool directIntercept(float distance, bool roadClear, int wanted, bool playerInVehicle,
    float authoredStraightLineDistance) {
    if (!std::isfinite(distance) || distance < 0.0f || !roadClear || wanted <= 0) return false;
    if (wanted == 1 && !playerInVehicle) return false;
    const float limit = authoredStraightLineDistance > 0.0f ?
        std::min(authoredStraightLineDistance, DirectVehicleInterceptDistance) :
        DirectVehicleInterceptDistance;
    return distance <= limit;
}

inline float pursuitCruise(int wanted) {
    if (wanted <= 0) return 0.0f;
    return std::clamp(8.5f + 1.1f * float(wanted), 8.5f, 14.0f);
}

}  // namespace storylandLcsPoliceResponse
