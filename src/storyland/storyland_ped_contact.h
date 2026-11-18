#pragma once
#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

struct StorylandPedContactState {
    int32_t animation = -1;
    float animationTime = 0, animationDuration = 0, animationMotionTime = 0;
    float cooldown = 0, avoidanceTime = 0, blockedSeconds = 0, waitCredit = 0;
    uint64_t obstacle = 0;
    uint32_t progressFrame = 0;
    StorylandNativePoint avoidance{}, contactNormal{}, velocity{};
    bool yielding = false, obstructed = false, progressFrameValid = false;

    bool active() const {
        return animation >= 0 && animationDuration > 0 && animationTime < animationDuration + .25f;
    }
    float weight(float time) const {
        if (animation < 0 || animationDuration <= 0) return 0;
        const float maximum = std::min(1.0f, animationDuration * 8.0f);
        return time <= animationDuration ? std::clamp(time * 8.0f, 0.0f, maximum) :
            std::max(0.0f, maximum - (time - animationDuration) * 4.0f);
    }
    float weight() const { return weight(animationTime); }
    void clear() { *this = {}; }
};

struct StorylandPedContactObstacle {
    uint64_t identity = 0;
    StorylandNativePoint position{}, velocity{};
    float radius = .31f, waitCredit = 0;
    bool movable = true;
    bool fastMover = false, missionCharacter = false;
};

struct StorylandPedContactGoal {
    StorylandNativePoint position{};
    float radius = 0;
};

struct StorylandPedContactPlan {
    StorylandNativePoint destination{}, normal{};
    uint64_t obstacle = 0;
    bool contact = false, yielding = false, incomingBump = false;
};

inline bool storylandFinitePedContactPoint(const StorylandNativePoint& point) {
    return std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z);
}

inline void storylandAdvancePedContact(StorylandPedContactState& state, float seconds) {
    if (!std::isfinite(seconds) || seconds <= 0) return;
    const float step = std::min(seconds, .1f);
    if (state.active()) {
        state.animationTime = std::min(state.animationDuration + .25f, state.animationTime + step);
    }
    state.cooldown = std::max(0.0f, state.cooldown - step);
    state.avoidanceTime = std::max(0.0f, state.avoidanceTime - step);
    if (state.avoidanceTime == 0) {
        state.obstacle = 0;
        state.yielding = false;
        state.avoidance = {};
    }
    state.obstructed = false;
}

inline void storylandPublishPedContact(std::vector<StorylandPedContactObstacle>& obstacles,
    uint64_t identity, const StorylandNativePoint& position, const StorylandPedContactState& state) {
    if(!storylandFinitePedContactPoint(position))return;
    for (auto& obstacle : obstacles) if (obstacle.identity == identity) {
        obstacle.position = position;
        obstacle.velocity = storylandFinitePedContactPoint(state.velocity)?state.velocity:StorylandNativePoint{};
        obstacle.waitCredit = std::isfinite(state.waitCredit)?state.waitCredit:0;
        return;
    }
}

inline uint64_t storylandPedContactHash(uint64_t value) {
    value ^= value >> 30;
    value *= UINT64_C(0xBF58476D1CE4E5B9);
    value ^= value >> 27;
    value *= UINT64_C(0x94D049BB133111EB);
    return value ^ (value >> 31);
}

inline bool storylandPedestrianHasPriority(uint64_t identity, float waitCredit,
    const StorylandPedContactObstacle& other) {
    if (!other.movable) return false;
    if (std::fabs(waitCredit - other.waitCredit) > .25f) return waitCredit > other.waitCredit;
    const uint64_t first = std::min(identity, other.identity);
    const uint64_t second = std::max(identity, other.identity);
    const bool firstWins = (storylandPedContactHash(first ^ storylandPedContactHash(second)) & 1) == 0;
    return (identity == first) == firstWins;
}

inline StorylandNativePoint storylandPedContactNormal(uint64_t identity,
    const StorylandNativePoint& position, const StorylandPedContactObstacle& other) {
    const float x = position.x - other.position.x, y = position.y - other.position.y;
    const float length = std::hypot(x, y);
    if (length > .00001f) return {x / length, y / length, 0};
    const uint64_t first = std::min(identity, other.identity);
    const uint64_t second = std::max(identity, other.identity);
    const uint64_t hash = storylandPedContactHash(first ^ storylandPedContactHash(second));
    const float angle = float(hash & 65535) * (6.283185307179586f / 65536.0f);
    const float sign = identity == first ? 1.0f : -1.0f;
    return {sign * std::cos(angle), sign * std::sin(angle), 0};
}

inline float storylandPedContactGoalStopTime(const StorylandNativePoint& position,
    const StorylandNativePoint& velocity, float step, float horizon,
    const StorylandPedContactGoal* goal) {
    if (!goal || !storylandFinitePedContactPoint(goal->position) ||
        !std::isfinite(goal->radius) || goal->radius <= 0) return horizon;
    const float speedSquared = velocity.x * velocity.x + velocity.y * velocity.y;
    if (speedSquared <= .0001f) return horizon;
    const float x = position.x - goal->position.x;
    const float y = position.y - goal->position.y;
    const float z = position.z - goal->position.z;
    const float outside = x * x + y * y + z * z - goal->radius * goal->radius;
    if (!std::isfinite(outside)) return horizon;
    if (outside <= 0) return std::min(step, horizon);
    const float approach = x * velocity.x + y * velocity.y;
    if (approach >= 0) return horizon;
    const float discriminant = approach * approach - speedSquared * outside;
    if (!std::isfinite(discriminant) || discriminant < 0) return horizon;
    const float time = (-approach - std::sqrt(discriminant)) / speedSquared;
    if (!std::isfinite(time) || time < 0 || time >= horizon) return horizon;
    // The current requested step still goes through the real body sweep. Only
    // later prediction stops at the seek region that the objective will reach.
    return std::clamp(time, std::min(step, horizon), horizon);
}

inline StorylandPedContactPlan storylandPlanPedContact(uint64_t identity,
    StorylandPedContactState& state, const StorylandNativePoint& position,
    const StorylandNativePoint& desired, const std::vector<StorylandPedContactObstacle>& obstacles,
    float seconds, float radius = .31f, const StorylandPedContactGoal* goal = nullptr,
    bool missionCharacter = false) {
    StorylandPedContactPlan result;
    result.destination = desired;
    if (!std::isfinite(seconds) || seconds <= 0||!storylandFinitePedContactPoint(position)||
        !storylandFinitePedContactPoint(desired))return result;
    const float step = std::min(seconds, .1f);
    const float dx = desired.x - position.x, dy = desired.y - position.y;
    const float travel = std::hypot(dx, dy);
    const float speed = travel / step;
    if (!std::isfinite(speed) || speed > 20) return result;
    const StorylandNativePoint velocity{dx / step, dy / step, 0};
    constexpr float horizon = .65f;
    const float stopTime = storylandPedContactGoalStopTime(position, velocity, step, horizon, goal);
    const StorylandPedContactObstacle* selected = nullptr;
    float earliest = std::numeric_limits<float>::infinity();
    for (const auto& other : obstacles) {
        if(other.identity==identity||!storylandFinitePedContactPoint(other.position)||
            !storylandFinitePedContactPoint(other.velocity)||!std::isfinite(other.radius)||
            other.radius<=0||std::fabs(position.z-other.position.z)>1.4f)continue;
        const float x = other.position.x - position.x, y = other.position.y - position.y;
        const float vx = other.velocity.x - velocity.x, vy = other.velocity.y - velocity.y;
        const float relativeSpeed = vx * vx + vy * vy;
        const float approach = x * vx + y * vy;
        const float clearance = radius + other.radius + .08f;
        const float currentDistance = std::hypot(x, y);
        float time = relativeSpeed > .0001f ? std::clamp(-approach / relativeSpeed, 0.0f, stopTime) : 0;
        float predictedDistance = std::hypot(x + vx * time, y + vy * time);
        if (stopTime < horizon) {
            // Other actors keep moving after this actor reaches its seek goal.
            // Check that interval too, so stopping never hides an incoming actor.
            const float stoppedX = x + vx * stopTime;
            const float stoppedY = y + vy * stopTime;
            const float otherSpeed = other.velocity.x * other.velocity.x +
                other.velocity.y * other.velocity.y;
            const float stoppedApproach = stoppedX * other.velocity.x + stoppedY * other.velocity.y;
            const float after = otherSpeed > .0001f ?
                std::clamp(-stoppedApproach / otherSpeed, 0.0f, horizon - stopTime) : 0;
            const float stoppedDistance = std::hypot(stoppedX + other.velocity.x * after,
                stoppedY + other.velocity.y * after);
            if (stoppedDistance < predictedDistance) {
                predictedDistance = stoppedDistance;
                time = stopTime + after;
            }
        }
        if (currentDistance >= clearance &&
            ((stopTime >= horizon && approach >= 0) || predictedDistance >= clearance)) continue;
        if (currentDistance > clearance + (speed + std::hypot(other.velocity.x, other.velocity.y)) * .65f) continue;
        const float score = time + currentDistance * .02f;
        if (score >= earliest) continue;
        earliest = score;
        selected = &other;
    }
    if (!selected) {
        if (state.avoidanceTime > 0 && travel > .00001f) {
            const float weight = std::min(.45f, state.avoidanceTime);
            const float x = dx / travel + state.avoidance.x * weight;
            const float y = dy / travel + state.avoidance.y * weight;
            const float length = std::hypot(x, y);
            result.destination.x = position.x + x / length * travel;
            result.destination.y = position.y + y / length * travel;
        }
        return result;
    }
    const auto& other = *selected;
    const auto normal = storylandPedContactNormal(identity, position, other);
    const float distance = std::hypot(position.x - other.position.x, position.y - other.position.y);
    const float closing = (other.velocity.x - velocity.x) * normal.x +
        (other.velocity.y - velocity.y) * normal.y;
    result.obstacle = other.identity;
    result.normal = normal;
    result.contact = distance < radius + other.radius + .035f && closing > .15f;
    result.incomingBump = other.fastMover && !(missionCharacter && other.missionCharacter);
    const bool priority = storylandPedestrianHasPriority(identity, state.waitCredit, other);
    result.yielding = !priority;
    state.obstructed = true;
    state.contactNormal = normal;
    if (travel <= .00001f) return result;
    const float forwardX = dx / travel, forwardY = dy / travel;
    if (state.obstacle != other.identity || state.avoidanceTime <= 0) {
        const float rightX = forwardY, rightY = -forwardX;
        const float lateral = (other.position.x - position.x) * rightX +
            (other.position.y - position.y) * rightY;
        const float opposing = forwardX * other.velocity.x + forwardY * other.velocity.y;
        const float side = opposing < -.1f || std::fabs(lateral) < .05f ? 1.0f : lateral > 0 ? -1.0f : 1.0f;
        state.avoidance = {rightX * side, rightY * side, 0};
        state.obstacle = other.identity;
        state.yielding = !priority;
    }
    state.avoidanceTime = .8f;
    const float forward = priority ? .65f : .25f;
    float x = forwardX * forward + state.avoidance.x;
    float y = forwardY * forward + state.avoidance.y;
    const float length = std::hypot(x, y);
    x /= length;
    y /= length;
    result.destination.x = position.x + x * travel;
    result.destination.y = position.y + y * travel;
    return result;
}

inline StorylandNativePoint storylandClipPedContactMotion(uint64_t identity,
    const StorylandNativePoint& from, StorylandNativePoint desired,
    const std::vector<StorylandPedContactObstacle>& obstacles, float radius = .31f) {
    for (unsigned iteration = 0; iteration < 3; ++iteration) {
        bool adjusted = false;
        for (const auto& other : obstacles) {
            if (other.identity == identity || std::fabs(from.z - other.position.z) > 1.4f) continue;
            const float combined = radius + other.radius + .002f;
            const float x = from.x - other.position.x, y = from.y - other.position.y;
            const float dx = desired.x - from.x, dy = desired.y - from.y;
            const float lengthSquared = dx * dx + dy * dy;
            const float c = x * x + y * y - combined * combined;
            if (lengthSquared < .00000001f) continue;
            if (c < 0) {
                const auto normal = storylandPedContactNormal(identity, from, other);
                const float into = dx * normal.x + dy * normal.y;
                if (into < 0) {
                    desired.x -= into * normal.x;
                    desired.y -= into * normal.y;
                    adjusted = true;
                }
                continue;
            }
            const float b = x * dx + y * dy;
            if (b >= 0) continue;
            const float discriminant = b * b - lengthSquared * c;
            if (discriminant <= 0) continue;
            const float fraction = (-b - std::sqrt(discriminant)) / lengthSquared;
            if (fraction < 0 || fraction >= 1) continue;
            const float hitX = from.x + dx * fraction, hitY = from.y + dy * fraction;
            const float normalX = (hitX - other.position.x) / combined;
            const float normalY = (hitY - other.position.y) / combined;
            const float remainingX = dx * (1 - fraction), remainingY = dy * (1 - fraction);
            const float into = std::min(0.0f, remainingX * normalX + remainingY * normalY);
            desired.x = hitX + remainingX - into * normalX;
            desired.y = hitY + remainingY - into * normalY;
            adjusted = true;
        }
        if (!adjusted) break;
    }
    return desired;
}

inline StorylandNativePoint storylandSeparatePedContact(uint64_t identity,
    StorylandNativePoint position, const std::vector<StorylandPedContactObstacle>& obstacles,
    float maximumCorrection, float radius = .31f) {
    if (!std::isfinite(maximumCorrection) || maximumCorrection <= 0) return position;
    const auto original = position;
    for (unsigned iteration = 0; iteration < 4; ++iteration) {
        bool corrected = false;
        for (const auto& other : obstacles) {
            if (other.identity == identity || std::fabs(position.z - other.position.z) > 1.4f) continue;
            const float depth = radius + other.radius + .002f -
                std::hypot(position.x - other.position.x, position.y - other.position.y);
            if (depth <= .0001f) continue;
            const auto normal = storylandPedContactNormal(identity, position, other);
            const float correction = std::min(depth * (other.movable ? .5f : 1.0f), maximumCorrection);
            position.x += normal.x * correction;
            position.y += normal.y * correction;
            const float x = position.x - original.x, y = position.y - original.y;
            const float distance = std::hypot(x, y);
            if (distance >= maximumCorrection) {
                position.x = original.x + x * maximumCorrection / distance;
                position.y = original.y + y * maximumCorrection / distance;
                return position;
            }
            corrected = true;
        }
        if (!corrected) break;
    }
    return position;
}

inline int32_t storylandPedContactAnimation(bool vcs, uint8_t recipientMoveState,
    float recipientHeading, const StorylandNativePoint& initiatorToRecipient) {
    if (!std::isfinite(recipientHeading) || !storylandFinitePedContactPoint(initiatorToRecipient))
        return -1;
    // VCS1D9968/LCS1C3C04 select the recipient's side and movement state.
    const float angle = recipientHeading * .01745329251994329577f;
    const float forwardX = std::cos(angle), forwardY = std::sin(angle);
    const float forward = initiatorToRecipient.x * forwardX + initiatorToRecipient.y * forwardY;
    const float right = initiatorToRecipient.x * forwardY - initiatorToRecipient.y * forwardX;
    const int direction = forward < 0 ? (right > 0 ? 1 : 3) : 2;
    const bool running = recipientMoveState == 4 || recipientMoveState == 5;
    return (vcs ? (running ? 32 : 36) : (running ? 29 : 33)) + direction;
}

inline bool storylandPedContactOwnsRoot(bool vcs, int32_t animation) {
    return animation >= (vcs ? 36 : 33) && animation <= (vcs ? 39 : 36);
}

inline uint16_t storylandPedContactAssociationFlags(bool vcs, int32_t animation) {
    const int32_t first = vcs ? 32 : 29;
    if (animation >= first && animation < first + 4) return 0x418;
    if (animation == first + 4 || animation == first + 6) return 0x58;
    if (animation == first + 5) return vcs ? 0x8018 : 0x18;
    if (animation == first + 7) return 0x18;
    return 0;
}

inline void storylandRecordPedContactProgress(StorylandPedContactState& state,
    float requested, float travelled, float seconds) {
    if (!std::isfinite(seconds) || seconds <= 0) return;
    const float step = std::min(seconds, .1f);
    if (requested > .001f && travelled < requested * .25f && state.obstructed) {
        state.blockedSeconds += step;
        state.waitCredit = std::min(2.0f, state.waitCredit + step);
    } else {
        state.blockedSeconds = 0;
        state.waitCredit = std::max(0.0f, state.waitCredit - step * .5f);
    }
}

inline void storylandRecordPedContactFrame(StorylandPedContactState& state,
    float requested,float travelled,float seconds,uint32_t frame) {
    if(state.progressFrameValid&&state.progressFrame==frame)return;
    storylandRecordPedContactProgress(state,requested,travelled,seconds);
    state.progressFrame=frame;
    state.progressFrameValid=true;
}
