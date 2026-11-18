#pragma once
#include <algorithm>
#include <cmath>

// Host response ranges for the VCS PS2 pursuit and dispatch path. The wanted
// thresholds and vehicle/pedestrian identities are decoded separately from the
// retail data. These physical perception ranges are NOT claimed as ELF constants.
namespace storylandVcsPoliceResponse {

inline constexpr float OfficerSightDistance = 72.0f;
inline constexpr float OfficerHearingDistance = 24.0f;
inline constexpr float OfficerPursuitMemory = 16.0f;
inline constexpr float OfficerReleaseDistance = 145.0f;
inline constexpr float VehicleSightDistance = 100.0f;
inline constexpr float VehicleDispatchDistance = 35.0f;
inline constexpr float VehiclePursuitMemory = 18.0f;
inline constexpr float VehicleReleaseDistance = 190.0f;
inline constexpr float DirectInterceptDistance = 30.0f;

inline bool noticedOfficer(float distance, bool visible, bool audible) {
    return std::isfinite(distance) && distance >= 0.0f &&
        ((visible && distance <= OfficerSightDistance) ||
         (audible && distance <= OfficerHearingDistance));
}

inline bool continueOfficer(float distance, float now, float lastContact) {
    return std::isfinite(distance) && std::isfinite(now) && std::isfinite(lastContact) &&
        distance >= 0.0f && distance <= OfficerReleaseDistance && now >= lastContact &&
        now - lastContact <= OfficerPursuitMemory;
}

inline bool noticedVehicle(float distance, bool visible) {
    return std::isfinite(distance) && distance >= 0.0f &&
        ((visible && distance <= VehicleSightDistance) ||
         distance <= VehicleDispatchDistance);
}

inline bool continueVehicle(float distance, float now, float lastContact) {
    return std::isfinite(distance) && std::isfinite(now) && std::isfinite(lastContact) &&
        distance >= 0.0f && distance <= VehicleReleaseDistance && now >= lastContact &&
        now - lastContact <= VehiclePursuitMemory;
}

inline bool directIntercept(float distance, bool lineClear, int wanted,
    bool playerDriving, float authoredDistance) {
    if(!std::isfinite(distance) || distance < 0.0f || !lineClear || wanted <= 0) return false;
    if(wanted == 1 && !playerDriving) return false;
    const float maximum = authoredDistance > 0.0f ?
        std::min(authoredDistance, DirectInterceptDistance) : DirectInterceptDistance;
    return distance <= maximum;
}

inline float pursuitCruise(int wanted) {
    if(wanted <= 0) return 0.0f;
    return std::clamp(8.7f + float(wanted) * 1.15f, 8.7f, 15.0f);
}

} // namespace storylandVcsPoliceResponse
