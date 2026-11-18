#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "src/leeds/leeds_native_world.h"

// Physical steering for AI road vehicles. Angles are expressed in degrees.
namespace StorylandVehicleRoadFollow {

constexpr float DegreesToRadians = 0.01745329251994329577f;
constexpr float RadiansToDegrees = 57.295779513082320876f;
constexpr float HalfPi = 1.57079632679489661923f;

inline float pursuitError(float headingError) {
    // Pure pursuit normally uses sin(error). Behind the vehicle this tends
    // back to zero, so a waypoint directly behind produces no steering at
    // all. Keep the closest left/right turn until it comes alongside again.
    return std::clamp(headingError, -HalfPi, HalfPi);
}

inline bool lookAhead(const StorylandNativePaths& paths,
    const StorylandNativeTrafficActor& actor, float distance,
    const StorylandNativeZones* zones, bool avoidLevelTransitions,
    bool fastLane, bool ignoreDisabledRoads, StorylandNativePoint& target) {
    if (actor.node >= paths.nodes.size() || actor.nextNode >= paths.nodes.size() ||
        actor.node == actor.nextNode || !std::isfinite(distance)) return false;
    // Sample the existing lane and junction routing on a copy. The road sample
    // is a steering target; it must never replace the physical car position.
    auto route = actor;
    if (fastLane && route.vehicle && !route.water) route.lane = 0;
    if (!storylandNativeTrafficRoutePosition(paths, route, route.position, zones,
        avoidLevelTransitions, ignoreDisabledRoads)) return false;
    float remaining = std::clamp(distance, 0.0f, 32.0f);
    while (remaining > 0.0f) {
        const float step = std::min(remaining, 2.0f);
        storylandAdvanceNativeTraffic(paths, route, step / 8.0f, zones,
            avoidLevelTransitions, fastLane, ignoreDisabledRoads);
        remaining -= step;
        if (route.node == route.nextNode) break;
    }
    target = route.position;
    return std::isfinite(target.x) && std::isfinite(target.y) && std::isfinite(target.z);
}

template <typename Point>
float cornerSpeedLimit(float desiredSpeed, float yaw, const Point& position,
                       const Point& target, float grip, float wheelbase) {
    const float dx = target.x-position.x;
    const float dy = target.y-position.y;
    const float distance = std::max(0.1f, std::hypot(dx,dy));
    const float desiredYaw = std::atan2(dy,dx)*RadiansToDegrees;
    const float delta = std::remainder(desiredYaw-yaw,360.0f)*DegreesToRadians;
    const float curvature = 2.0f*std::abs(std::sin(pursuitError(delta)))/distance;
    const float cornerLimit = std::sqrt(std::max(0.1f,grip)*9.81f /
                                        std::max(curvature,0.0001f));
    return std::min(std::max(0.0f,desiredSpeed),
                    std::max(2.0f,cornerLimit));
}

template <typename Actor>
void integrate(Actor& actor, float& steeringAngle, float& yawVelocity,
               float speed, float lateralSpeed, float wheelbase,
               float steerLock, float dt, const decltype(actor.position)& target,
               bool twoWheels) {
    if (dt <= 0.0f) return;
    const float dx = target.x-actor.position.x;
    const float dy = target.y-actor.position.y;
    const float distance = std::max(0.5f,std::hypot(dx,dy));
    const float targetYaw = std::atan2(dy,dx);
    const float yaw = actor.yaw*DegreesToRadians;
    const float error = std::remainder(targetYaw-yaw,6.2831853071795864769f);
    const float curvature = 2.0f*std::sin(pursuitError(error))/distance;
    const float requestedSteer = std::atan(curvature*std::max(0.5f,wheelbase)) *
                                 RadiansToDegrees;
    const float steerLimit = std::max(1.0f,steerLock);
    const float steerResponse = (twoWheels ? 150.0f : 100.0f)*dt;
    const float requested = std::clamp(requestedSteer,-steerLimit,steerLimit);
    steeringAngle += std::clamp(requested-steeringAngle,-steerResponse,steerResponse);
    const float desiredYawVelocity = speed * std::tan(steeringAngle * DegreesToRadians) /
        std::max(0.5f, wheelbase) * RadiansToDegrees;
    const float yawResponse = (twoWheels ? 135.0f : 110.0f) * dt;
    yawVelocity += std::clamp(desiredYawVelocity - yawVelocity, -yawResponse, yawResponse);
    yawVelocity = std::clamp(yawVelocity, -110.0f, 110.0f);
    const float middleYaw = yaw + yawVelocity * DegreesToRadians * dt * 0.5f;
    actor.position.x += (std::cos(middleYaw)*speed-std::sin(middleYaw)*lateralSpeed)*dt;
    actor.position.y += (std::sin(middleYaw)*speed+std::cos(middleYaw)*lateralSpeed)*dt;
    actor.yaw = std::remainder(actor.yaw + yawVelocity * dt, 360.0f);
}

inline void updateProgress(const StorylandNativePaths& paths,
    StorylandNativeTrafficActor& actor, const StorylandNativeZones* zones,
    bool avoidLevelTransitions, bool fastLane, bool ignoreDisabledRoads) {
    // Progress is in world units, like storylandAdvanceNativeTraffic, not a
    // normalized fraction. Advance the route only after the car passes the
    // end of its current segment and preserve its physical pose throughout.
    for (unsigned transition = 0; transition < 4; ++transition) {
        if (actor.node >= paths.nodes.size() || actor.nextNode >= paths.nodes.size() ||
            actor.node == actor.nextNode) return;
        const auto& from = paths.nodes[actor.node].position;
        const auto& to = paths.nodes[actor.nextNode].position;
        const float dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
        const float planarSquared = dx * dx + dy * dy;
        const float length = std::sqrt(planarSquared + dz * dz);
        if (planarSquared < .0001f || length < .001f) return;
        const float fraction = ((actor.position.x - from.x) * dx +
            (actor.position.y - from.y) * dy) / planarSquared;
        actor.distance = std::clamp(fraction, 0.0f, 1.0f) * length;
        if (actor.blockedSeconds > 0.0f) return;
        const float distanceToJunction = std::hypot(actor.position.x - to.x,
            actor.position.y - to.y);
        if (distanceToJunction > (fraction >= 1.0f ? 30.0f : 7.0f)) return;
        if (fraction < 1.0f && (1.0f - fraction) * std::sqrt(planarSquared) > 3.0f)
            return;
        const auto position = actor.position;
        const float yaw = actor.yaw;
        auto progress = actor;
        progress.distance = std::max(0.0f, length - .01f);
        storylandAdvanceNativeTraffic(paths, progress, .02f / 8.0f, zones,
            avoidLevelTransitions, fastLane, ignoreDisabledRoads);
        if (progress.node == actor.node && progress.nextNode == actor.nextNode) return;
        if (fraction < 1.0f) {
            if (progress.nextNode >= paths.nodes.size() || progress.node == progress.nextNode)
                return;
            const auto& following = paths.nodes[progress.nextNode].position;
            const float nextX = following.x - to.x;
            const float nextY = following.y - to.y;
            const float nextLength = std::hypot(nextX, nextY);
            if (nextLength < .001f) return;
            const float travelled = ((position.x - to.x) * nextX +
                (position.y - to.y) * nextY) / nextLength;
            const float heading = actor.yaw * DegreesToRadians;
            const float direction = (std::cos(heading) * nextX +
                std::sin(heading) * nextY) / nextLength;
            // A smooth corner may enter the outgoing road before the vehicle
            // centre crosses the old segment's end plane. Advance only after
            // it has moved into that road, while it is still near the junction.
            if (travelled < .75f || direction <= 0.0f) return;
        }
        actor = std::move(progress);
        actor.position = position;
        actor.yaw = yaw;
    }
}

} // namespace StorylandVehicleRoadFollow
