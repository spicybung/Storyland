#pragma once
#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace StorylandVcsVehicleSteering {

inline constexpr float Pi = 3.14159265358979323846f;

inline float wrap(float angle) {
    if (angle >= -Pi && angle < Pi) return angle;
    float result = std::fmod(std::clamp(angle, -25.0f, 25.0f), 2.0f * Pi);
    if (result >= Pi) result -= 2.0f * Pi;
    if (result < -Pi) result += 2.0f * Pi;
    return result;
}

inline float bearing(float x, float y) {
    const float result = std::atan2(y, x);
    return result < 0.0f ? result + 2.0f * Pi : result;
}

struct Bearings {
    float clockwise = 0;
    float counterclockwise = 0;

    explicit Bearings(float destination) : clockwise(destination), counterclockwise(destination) {}

    void exclude(float bearing, float halfWidth) {
        if (std::fabs(wrap(bearing - clockwise)) < halfWidth)
            clockwise = wrap(bearing - halfWidth);
        if (std::fabs(wrap(bearing - counterclockwise)) < halfWidth)
            counterclockwise = wrap(bearing + halfWidth);
    }

    float select(float destination, float heading) const {
        const float reference = wrap(destination + 0.5f * wrap(heading - destination));
        const float first = std::fabs(wrap(clockwise - reference));
        const float second = std::fabs(wrap(counterclockwise - reference));
        if (first > Pi * 0.5f && second > Pi * 0.5f) return reference;
        if (std::fabs(first - second) < 0.08f) return counterclockwise;
        return first < second ? clockwise : counterclockwise;
    }
};

inline bool inSearchSquare(const StorylandNativePoint& origin,
    const StorylandNativePoint& candidate, float radius) {
    return std::fabs(candidate.x - origin.x) < radius &&
        std::fabs(candidate.y - origin.y) < radius &&
        std::fabs(candidate.z - origin.z) < 4.0f;
}

inline float searchRadius(float nativePlanarSpeed) {
    return 12.0f * std::min(2.0f, 1.0f + 2.5f * nativePlanarSpeed);
}

inline void avoidVehicle(Bearings& bearings, const StorylandNativePoint& origin,
    float heading, const StorylandNativePoint& nativeVelocity, float radius, float maximumX,
    const StorylandNativePoint& obstacle, const StorylandNativePoint& obstacleNativeVelocity,
    const StorylandNativeCollisionModel& collision) {
    const float dx = obstacle.x - origin.x, dy = obstacle.y - origin.y;
    const float distance = std::hypot(dx, dy);
    if (distance < 1.0f) return;
    const float closing = dx * (nativeVelocity.x - obstacleNativeVelocity.x) +
        dy * (nativeVelocity.y - obstacleNativeVelocity.y);
    if (110.0f * closing - collision.boundingRadius - radius < distance) return;
    const float direction = bearing(dx, dy);
    const float relative = direction - heading;
    const float projected = std::fabs(std::cos(relative) * collision.maximum.x) +
        std::fabs(std::sin(relative) * collision.maximum.y);
    bearings.exclude(direction, (projected + 1.2f * maximumX) / distance);
}

inline void avoidPedestrian(Bearings& bearings, const StorylandNativePoint& origin,
    float maximumX, const StorylandNativePoint& pedestrian) {
    const float dx = pedestrian.x - origin.x, dy = pedestrian.y - origin.y;
    const float distance = std::hypot(dx, dy);
    bearings.exclude(bearing(dx, dy), (1.2f * maximumX + 0.4f) / distance);
}

inline float cornerSpeedFactor(float originalTurn) {
    const float angle = std::fabs(wrap(originalTurn));
    if (angle <= 0.4f) return 1.0f;
    if (angle >= 1.2f) return 0.4f;
    return 1.0f - (angle - 0.4f) * 0.75f;
}

struct Controls {
    float steering = 0;
    float accelerator = 0;
    float brake = 0;
    bool handbrake = false;
};

struct ReverseState {
    uint32_t missionStartedAt = 0;
    uint32_t lastMovingAt = 0;
    uint32_t lastContactAt = 0;
    uint32_t deadline = 0;
    int32_t action = 0;
    int8_t drivingStyle = 0;
};

inline bool requestReverse(ReverseState& state, uint32_t now, float nativePlanarSpeedSquared,
    int32_t status, int8_t mission, int8_t cruise, uint8_t creationSource) {
    constexpr float threshold = .018f * .018f;
    if (nativePlanarSpeedSquared > threshold) {
        state.missionStartedAt = now;
        state.lastMovingAt = now;
    }
    if (status != 3 || state.action != 0 || mission == 0 || mission == 11 || mission == 6 ||
        cruise == 0 || (creationSource == 1 && mission == 1) ||
        (creationSource != 2 && (state.drivingStyle == 0 || state.drivingStyle == 4))) return false;
    if (uint32_t(now - state.lastContactAt) > 500) state.missionStartedAt = now;
    if (!(nativePlanarSpeedSquared < threshold) || uint32_t(now - state.missionStartedAt) <= 2000)
        return false;
    state.action = 2;
    state.deadline = now + (mission != 1 || creationSource == 2 ? 1500u : 700u);
    state.missionStartedAt = now;
    if (creationSource == 1) state.drivingStyle = std::max<int8_t>(state.drivingStyle, 2);
    return true;
}

inline Controls reverseControls(Controls original, float nativeForwardSpeed) {
    original.steering = -original.steering;
    original.accelerator = nativeForwardSpeed > .04f ? 0.0f : -.5f;
    original.brake = nativeForwardSpeed > .04f ? .5f : 0.0f;
    original.handbrake = false;
    return original;
}

inline void expireReverse(ReverseState& state, uint32_t now, uint8_t creationSource) {
    if (state.action != 2 || !(state.deadline < now)) return;
    state.action = 0;
    if (creationSource == 2 && state.drivingStyle == 0) state.drivingStyle = 2;
}

struct RoadLink {
    StorylandNativePoint position;
    StorylandNativePoint direction;
    uint16_t index = UINT16_MAX;
};

inline bool roadLink(const StorylandNativePaths& paths, uint32_t from, uint32_t to,
    uint8_t lane, RoadLink& result) {
    if (from >= paths.nodes.size() || to >= paths.nodes.size() || from == to) return false;
    const auto& source = paths.nodes[from];
    const auto& destination = paths.nodes[to];
    const StorylandNativePathLink* connection = nullptr;
    for (const auto& link : source.links)
        if (link.target == to) { connection = &link; break; }
    if (!connection || !connection->nativeLanes || !connection->lanes) return false;
    const float x = destination.position.x - source.position.x;
    const float y = destination.position.y - source.position.y;
    const float length = std::hypot(x, y);
    if (length == 0.0f) return false;
    // 266CF0 returns signed hundredths; 26B5A8 centers one-way lanes.
    result.direction = {std::trunc(x * 100.0f / length) * .01f,
        std::trunc(y * 100.0f / length) * .01f, 0};
    lane = std::min(lane, uint8_t(connection->lanes - 1));
    const float offset = connection->oppositeLanes ?
        (float(lane) + .5f) * 5.0f + connection->dividerWidth :
        (float(lane) + .5f - float(connection->lanes) * .5f) * 5.0f;
    result.position = {(source.position.x + destination.position.x) * .5f + result.direction.y * offset,
        (source.position.y + destination.position.y) * .5f - result.direction.x * offset,
        (source.position.z + destination.position.z) * .5f};
    result.index = connection->carLink;
    return true;
}

inline bool passedRoadLink(const StorylandNativePoint& position,
    const RoadLink& current, const RoadLink& next) {
    const float x = position.x - current.position.x;
    const float y = position.y - current.position.y;
    const float separation = std::hypot(x, y);
    const float nextX = next.position.x - current.position.x;
    const float nextY = next.position.y - current.position.y;
    const float projection = x * nextX + y * nextY;
    const float denominator = separation * std::hypot(nextX, nextY);
    return separation < 5.0f || (projection > 0.0f && separation < 8.0f) ||
        (denominator > 0.0f && projection / denominator > .7f) ||
        (current.index != UINT16_MAX && current.index == next.index);
}

inline StorylandNativePoint roadTarget(const StorylandNativePoint& position, const RoadLink& link) {
    const float separation = std::hypot(position.x - link.position.x, position.y - link.position.y);
    const float anticipation = separation > 40.0f ? 0.0f : separation * .4f;
    return {link.position.x - link.direction.x * anticipation,
        link.position.y - link.direction.y * anticipation, link.position.z};
}

inline Controls roadControls(float heading, float originalBearing, float avoidedBearing,
    float nativeSpeed, float nativeForwardSpeed, int8_t nativeCruise, uint32_t model,
    float linkDistance, float linkTurn, float trafficSpeedFactor) {
    Controls result;
    float steeringLimit = model == 0xEA ? .7f : .5f;
    if (nativeSpeed > .45f) steeringLimit = .2f;
    result.steering = std::clamp(wrap(avoidedBearing - heading), -steeringLimit, steeringLimit);
    const float angle = std::fabs(wrap(linkTurn));
    float nextCorner = angle <= .1f ? 1.0f : angle >= 1.2f ? .4f :
        1.0f - (angle - .1f) * (.6f / 1.1f);
    if (linkDistance > 40.0f || nativeCruise < 12) nextCorner = 1.0f;
    else nextCorner = 1.0f - (1.0f - nextCorner) * (1.0f - .025f * linkDistance);
    const float desired = float(nativeCruise) * std::min({cornerSpeedFactor(originalBearing - heading),
        nextCorner, trafficSpeedFactor});
    const float current = nativeForwardSpeed * 60.0f;
    const float error = desired - current;
    if (desired < .05f && error < .03f) result.brake = 1.0f;
    else if (error > 0.0f) result.accelerator = std::min(error * (current < 2.0f ? .25f : .125f), 1.0f);
    else result.brake = std::min(-error * .05f, .5f);
    return result;
}

inline Controls directControls(float heading, float originalBearing, float avoidedBearing,
    float nativeSpeed, int8_t nativeCruise, uint32_t model) {
    Controls result;
    const float turn = wrap(avoidedBearing - heading);
    const float steeringLimit = model == 0xEA ? 0.7f : 0.5f;
    result.steering = std::clamp(turn, -steeringLimit, steeringLimit);
    result.handbrake = nativeSpeed > 0.3f && std::fabs(turn) > 0.7f;
    const float current = nativeSpeed * 60.0f;
    const float error = float(nativeCruise) * cornerSpeedFactor(originalBearing - heading) - current;
    if (error > 0.0f)
        result.accelerator = current < 25.0f ? std::min(error * 0.1f, 1.0f) : 1.0f;
    else
        result.brake = std::min(-error * 0.05f, 0.5f);
    return result;
}

}
