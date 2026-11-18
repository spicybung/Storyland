#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

struct StorylandLocomotionFootfalls {
    float phase = 0.0f;
    unsigned crossings = 0;
    unsigned firstHalfStep = 0;
    // Positions along the distance travelled for each foot strike are
    // generated from the previous phase, not from frame time.
    float firstDistance = 0.0f;
    float spacing = 0.0f;
};

inline StorylandLocomotionFootfalls storylandAdvanceLocomotionFootfalls(
    float previousPhase, float travelled, float stride) {
    StorylandLocomotionFootfalls result;
    if (!std::isfinite(previousPhase) || !std::isfinite(travelled) ||
        !std::isfinite(stride) || stride <= 0.00001f || travelled < 0.0f) return result;
    previousPhase -= std::floor(previousPhase);
    result.phase = previousPhase;
    if (travelled <= 0.000001f) return result;
    const double halfSteps = double(travelled) * 2.0 / double(stride);
    const double initialStep = double(previousPhase) * 2.0;
    const double absoluteStep = initialStep + halfSteps;
    if (!std::isfinite(absoluteStep) || absoluteStep > 1000000.0) return result;
    result.phase = float(absoluteStep * 0.5 - std::floor(absoluteStep * 0.5));
    const int start = int(std::floor(initialStep));
    const int finish = int(std::floor(absoluteStep));
    if (finish <= start) return result;
    result.crossings = unsigned(std::min(4, finish - start));
    result.firstHalfStep = unsigned(start + 1);
    result.spacing = stride * 0.5f;
    result.firstDistance = float(double(start + 1) - initialStep) * result.spacing;
    return result;
}

inline float storylandLocomotionTurnRate(float metersPerSecond, float maximumTurnRate = 420.0f) {
    if (!std::isfinite(metersPerSecond) || metersPerSecond <= 0.0f) return maximumTurnRate;
    // More steering is available at a standstill. Fast sprint turns retain
    // a direction of travel instead of spinning 420 degrees per second.
    return std::clamp(maximumTurnRate - 28.0f * metersPerSecond,
        maximumTurnRate * 0.52f, maximumTurnRate);
}

inline float storylandLocomotionTurnTowards(float heading, float desiredHeading,
    float metersPerSecond, float seconds) {
    if (!std::isfinite(heading) || !std::isfinite(desiredHeading) ||
        !std::isfinite(seconds) || seconds <= 0.0f) return heading;
    const float delta = std::remainder(desiredHeading - heading, 360.0f);
    const float limitedSeconds = std::clamp(seconds, 0.0f, 0.10f);
    return heading + std::clamp(delta,
        -storylandLocomotionTurnRate(metersPerSecond)*limitedSeconds,
         storylandLocomotionTurnRate(metersPerSecond)*limitedSeconds);
}

inline bool storylandLocomotionActuallyMoving(float travelled, float seconds,
    float minimumSpeed = 0.08f) {
    return std::isfinite(travelled) && std::isfinite(seconds) && seconds > 0.0f &&
        travelled > std::max(0.001f, minimumSpeed * seconds);
}
