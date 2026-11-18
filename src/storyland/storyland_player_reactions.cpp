#include "src/storyland/storyland_player_reactions.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float Radians = 0.01745329251994329577f;

bool validContext(const StorylandPlayerReactionContext& context, bool airborneDive = false) {
    return context.health > 0 && context.onFoot && (context.grounded || airborneDive) && context.control &&
        !context.water && !context.scripted && std::isfinite(context.seconds) && context.seconds > 0;
}

bool sameTarget(const StorylandReactionTarget& first, const StorylandReactionTarget& second) {
    return first.kind == second.kind && first.handle == second.handle && first.model == second.model;
}

bool pairedAction(StorylandPlayerReaction action) {
    return action >= StorylandPlayerReaction::GrabStart;
}

bool repeatingAction(StorylandPlayerReaction action) {
    return action == StorylandPlayerReaction::GrabHold || action == StorylandPlayerReaction::GrabWalk ||
        action == StorylandPlayerReaction::RearGrabHold;
}

int32_t actionAnimation(StorylandPlayerReaction action, bool victim, bool vcs) {
    using Action = StorylandPlayerReaction;
    switch (action) {
    case Action::EvadeDive: return victim ? -1 : 146;
    case Action::CarGesture: return victim ? -1 : vcs ? 158 : 159;
    case Action::GrabStart: return victim ? 265 : 263;
    case Action::GrabHold: return victim ? 267 : 264;
    case Action::GrabWalk: return victim ? 302 : 301;
    case Action::GrabJab: return victim ? 288 : 287;
    case Action::GrabKnee: return victim ? 290 : 289;
    case Action::GrabShove: return victim ? 269 : 268;
    case Action::GrabRelease: return victim ? 296 : 295;
    case Action::RearGrabStart: return victim ? 309 : 314;
    case Action::RearGrabHold: return victim ? 309 : 308;
    case Action::RearGrabKnee: return victim ? 330 : 329;
    case Action::RearGrabRelease:
    case Action::RearGrabNeckBreakEnd: return victim ? 311 : 310;
    case Action::RearGrabNeckBreak: return victim ? 313 : 312;
    default: return -1;
    }
}

bool contactAction(StorylandPlayerReaction action) {
    using Action = StorylandPlayerReaction;
    return action == Action::GrabJab || action == Action::GrabKnee ||
        action == Action::RearGrabKnee || action == Action::RearGrabNeckBreak;
}

bool beginAction(StorylandPlayerReactionState& state, StorylandPlayerReaction action,
    const StorylandPlayerReactionBindings& bindings) {
    if (!bindings.duration) return false;
    if ((pairedAction(action) || action == StorylandPlayerReaction::EvadeDive) && !bindings.rootDelta)
        return false;
    const int32_t animation = actionAnimation(action, false, state.vcs);
    const int32_t group = pairedAction(action) ? 63 : 0;
    const float duration = bindings.duration(animation, group);
    if (!std::isfinite(duration) || duration <= 0) return false;
    float victimDuration = 0;
    if (pairedAction(action)) {
        victimDuration = bindings.duration(actionAnimation(action, true, state.vcs), group);
        if (!std::isfinite(victimDuration) || victimDuration <= 0) return false;
    }
    float contact = -1;
    if (contactAction(action)) {
        if (!bindings.contactTime) return false;
        const bool knee = action == StorylandPlayerReaction::GrabKnee ||
            action == StorylandPlayerReaction::RearGrabKnee;
        contact = bindings.contactTime(animation, group, knee ? 54u : 24u);
        if (!std::isfinite(contact) || contact < 0 || contact > duration) return false;
    }
    state.action = action;
    state.elapsed = 0;
    state.playerAnimationDuration = duration;
    state.duration = action == StorylandPlayerReaction::RearGrabNeckBreakEnd ?
        std::max(duration, victimDuration) : duration;
    state.victimDuration = victimDuration;
    state.contactAt = contact;
    state.contactApplied = false;
    state.scriptForcedDive = false;
    return true;
}

StorylandNativePoint localToWorld(const StorylandNativePoint& local, float heading) {
    const float angle = heading * Radians;
    return {local.x * std::cos(angle) + local.y * std::sin(angle),
        local.x * std::sin(angle) - local.y * std::cos(angle), local.z};
}

float pointSegmentDistanceSquared(float x, float y, float ax, float ay, float bx, float by) {
    const float dx = bx - ax, dy = by - ay;
    const float length = dx * dx + dy * dy;
    const float phase = length > 0 ? std::clamp(((x - ax) * dx + (y - ay) * dy) / length, 0.0f, 1.0f) : 0;
    const float rx = x - ax - phase * dx, ry = y - ay - phase * dy;
    return rx * rx + ry * ry;
}

float segmentRectangleDistanceSquared(float ax, float ay, float bx, float by, float length, float width) {
    const auto inside = [&](float x, float y) { return std::fabs(x) <= length && std::fabs(y) <= width; };
    if (inside(ax, ay) || inside(bx, by)) return 0;
    float enter = 0, leave = 1;
    const auto slab = [&](float from, float delta, float half) {
        if (std::fabs(delta) < std::numeric_limits<float>::epsilon()) return std::fabs(from) <= half;
        float first = (-half - from) / delta, last = (half - from) / delta;
        if (first > last) std::swap(first, last);
        enter = std::max(enter, first);
        leave = std::min(leave, last);
        return enter <= leave;
    };
    if (slab(ax, bx - ax, length) && slab(ay, by - ay, width) && leave >= 0 && enter <= 1) return 0;
    const auto pointRectangle = [&](float x, float y) {
        const float dx = std::max(std::fabs(x) - length, 0.0f), dy = std::max(std::fabs(y) - width, 0.0f);
        return dx * dx + dy * dy;
    };
    float distance = std::min(pointRectangle(ax, ay), pointRectangle(bx, by));
    for (float x : {-length, length}) for (float y : {-width, width})
        distance = std::min(distance, pointSegmentDistanceSquared(x, y, ax, ay, bx, by));
    return distance;
}

void appendRootDelta(StorylandNativePoint& sum, const StorylandPlayerReactionBindings& bindings,
    const StorylandPlayerReactionPose& pose, float from, float to, float heading, double repetitions = 1) {
    if (!bindings.rootDelta || to <= from) return;
    const auto local = bindings.rootDelta(pose.animation, pose.group, from, to);
    if (!std::isfinite(local.x) || !std::isfinite(local.y) || !std::isfinite(local.z)) return;
    const auto world = localToWorld(local, heading);
    const StorylandNativePoint next{float(double(sum.x) + double(world.x) * repetitions),
        float(double(sum.y) + double(world.y) * repetitions), float(double(sum.z) + double(world.z) * repetitions)};
    if (std::isfinite(next.x) && std::isfinite(next.y) && std::isfinite(next.z)) sum = next;
}

StorylandPlayerReactionResult advanceAction(StorylandPlayerReactionState& state,
    const StorylandPlayerReactionContext& context, const StorylandPlayerReactionBindings& bindings) {
    StorylandPlayerReactionResult result;
    if (!state.active()) return result;
    result.ownsMovement = state.action != StorylandPlayerReaction::CarGesture;
    result.consumeJump = result.ownsMovement;
    result.consumeInteract = pairedAction(state.action);
    result.consumeFire = pairedAction(state.action) || state.action == StorylandPlayerReaction::EvadeDive;
    const float previous = state.elapsed;
    const float step = std::min(context.seconds, 0.1f);
    const auto pose = storylandPlayerReactionPose(state);
    const auto victimPose = storylandPlayerReactionPose(state, true);
    const auto appendMovement = [&](float from, float to, double repetitions = 1) {
        const bool finishingSnap = state.action == StorylandPlayerReaction::RearGrabNeckBreakEnd;
        appendRootDelta(result.displacement, bindings, pose,
            finishingSnap ? std::min(from, state.playerAnimationDuration) : from,
            finishingSnap ? std::min(to, state.playerAnimationDuration) : to, state.heading, repetitions);
        if (victimPose.animation >= 0 && state.duration > 0) {
            appendRootDelta(result.victimDisplacement, bindings, victimPose,
                finishingSnap ? std::min(from, state.victimDuration) : from / state.duration * state.victimDuration,
                finishingSnap ? std::min(to, state.victimDuration) : to / state.duration * state.victimDuration,
                state.heading + (state.rearGrip() ? 0 : 180), repetitions);
        }
    };
    const float end = previous + step;
    if (repeatingAction(state.action)) {
        if (end < state.duration) {
            appendMovement(previous, end);
            state.elapsed = end;
        } else {
            appendMovement(previous, state.duration);
            const double remaining = std::max(0.0, double(step) - double(state.duration - previous));
            const double cycles = std::floor(remaining / double(state.duration));
            if (cycles > 0) appendMovement(0, state.duration, cycles);
            state.elapsed = float(std::fmod(remaining, double(state.duration)));
            appendMovement(0, state.elapsed);
        }
    } else {
        state.elapsed = std::min(end, state.duration);
        appendMovement(previous, state.elapsed);
    }
    if (!state.contactApplied && state.contactAt >= 0 && state.elapsed >= state.contactAt) {
        state.contactApplied = true;
        result.damageContact = true;
        result.fatalContact = state.action == StorylandPlayerReaction::RearGrabNeckBreak;
    }
    result.playerPose = storylandPlayerReactionPose(state);
    result.victimPose = storylandPlayerReactionPose(state, true);
    if (state.elapsed >= state.duration && !repeatingAction(state.action)) {
        using Action = StorylandPlayerReaction;
        if (state.action == Action::GrabStart || state.action == Action::GrabJab ||
            state.action == Action::GrabKnee) {
            if (!beginAction(state, Action::GrabHold, bindings)) state.clearAction();
        } else if (state.action == Action::RearGrabStart || state.action == Action::RearGrabKnee) {
            const auto next = state.action == Action::RearGrabStart && state.snapAfterGrab ?
                Action::RearGrabNeckBreak : Action::RearGrabHold;
            state.snapAfterGrab = false;
            if (!beginAction(state, next, bindings)) state.clearAction();
        } else if (state.action == Action::RearGrabNeckBreak) {
            if (!beginAction(state, Action::RearGrabNeckBreakEnd, bindings)) {
                result.releasedTarget = true;
                state.clearAction();
            }
        } else {
            result.releasedTarget = pairedAction(state.action);
            state.clearAction();
        }
    }
    return result;
}
}

bool StorylandPlayerReactionState::holding() const { return pairedAction(action); }

bool StorylandPlayerReactionState::rearGrip() const {
    return action >= StorylandPlayerReaction::RearGrabStart;
}

void StorylandPlayerReactionState::clearAction() {
    action = StorylandPlayerReaction::None;
    target = {};
    elapsed = duration = victimDuration = playerAnimationDuration = 0;
    contactAt = -1;
    contactApplied = false;
    scriptForcedDive = false;
    latchedGrip = false;
    snapAfterGrab = false;
    gripDistance = 0;
}

void StorylandPlayerReactionState::reset() { *this = {}; }

float storylandVcsForcedEvasionHeading(float velocityX, float velocityY,
    uint8_t direction, uint32_t random) {
    constexpr float pi = 0x1.921fb6p+1f;
    constexpr float halfPi = 0x1.921fb6p+0f;
    float angle = std::atan2(-velocityX, velocityY);
    switch (direction & 3u) {
    case 0: angle += float(random & 1u) * pi - halfPi; break;
    case 1: angle -= halfPi; break;
    case 2: angle += halfPi; break;
    default: break;
    }
    if (angle < -pi) angle += 2 * pi;
    else if (angle >= pi) angle -= 2 * pi;
    return angle / Radians + 90;
}

bool storylandBeginScriptedPlayerEvasiveDive(StorylandPlayerReactionState& state,
    float heading, const StorylandPlayerReactionBindings& bindings) {
    if (!std::isfinite(heading)) return false;
    if (!beginAction(state, StorylandPlayerReaction::EvadeDive, bindings)) return false;
    state.heading = heading;
    state.scriptForcedDive = true;
    state.evadeFrames = 0;
    state.evadingVehicle = {};
    state.gesturePending = false;
    state.target = {};
    return true;
}

uint8_t storylandAnnoyPlayerTemper(uint8_t temper, bool policeEvent) {
    // VCS CPlayerPed::AnnoyPlayerPed, 0x237FF8..0x238034.
    if (temper < 52) return uint8_t(temper + 1);
    if (policeEvent) return temper < 55 ? uint8_t(temper + 1) : uint8_t(46);
    return temper;
}

void storylandNotifyPlayerVehicleAnnoyance(StorylandPlayerReactionState& state,
    const StorylandPlayerReactionContext& context, const StorylandReactionVehicle& vehicle,
    uint32_t serial, bool policeEvent) {
    if (!validContext(context) || !vehicle.active || serial == 0 || serial == state.annoyanceSerial ||
        context.movementSpeed > 0.001f || !context.meleeWeapon || context.firing) return;
    state.annoyanceSerial = serial;
    state.temper = storylandAnnoyPlayerTemper(state.temper, policeEvent);
    // Vehicle look reactions at 0x1F112C; collision look timer at 0x1F6348.
    if (state.temper < 50 && vehicle.model != 207) return;
    state.gesturePending = true;
    state.gestureExpiresAt = context.now + 1300;
}

bool storylandVehicleThreatensPlayer(const StorylandReactionVehicle& vehicle,
    const StorylandPlayerReactionContext& context) {
    if (!vehicle.active || !validContext(context) || vehicle.halfLength <= 0 || vehicle.halfWidth <= 0)
        return false;
    const float dx = context.position.x - vehicle.position.x;
    const float dy = context.position.y - vehicle.position.y;
    const float speedSquared = vehicle.velocity.x * vehicle.velocity.x + vehicle.velocity.y * vehicle.velocity.y;
    if (!std::isfinite(speedSquared) || speedSquared <= 0 ||
        dx * vehicle.velocity.x + dy * vehicle.velocity.y <= 0) return false;
    if (context.position.z > vehicle.position.z + vehicle.height + 0.33f ||
        context.position.z + 1.8f < vehicle.position.z) return false;
    const float angle = vehicle.heading * Radians;
    const float localX = dx * std::cos(angle) + dy * std::sin(angle);
    const float localY = -dx * std::sin(angle) + dy * std::cos(angle);
    const float futureX = localX - (vehicle.velocity.x * std::cos(angle) +
        vehicle.velocity.y * std::sin(angle)) * std::min(context.seconds, 0.1f);
    // Native vehicle evasion volume expands its broad-phase bounds by 2m
    // (0x2CFF1C). Actual collision still uses the engine's body sweep.
    return std::min(std::fabs(localX), std::fabs(futureX)) < vehicle.halfLength + 2.0f &&
        std::fabs(localY) < vehicle.halfWidth + 0.33f;
}

bool storylandSweptVehicleTouchesPlayer(const StorylandReactionVehicle& previous,
    const StorylandReactionVehicle& current, const StorylandNativePoint& playerFrom,
    const StorylandNativePoint& playerTo, float playerRadius, float playerHeight) {
    if (!previous.active || !current.active || previous.slot != current.slot || previous.model != current.model ||
        playerRadius <= 0 || playerHeight <= 0 || previous.halfLength <= 0 || previous.halfWidth <= 0 ||
        current.halfLength <= 0 || current.halfWidth <= 0 || previous.height <= 0 || current.height <= 0)
        return false;
    const StorylandNativePoint from{playerFrom.x - previous.position.x,
        playerFrom.y - previous.position.y, playerFrom.z - previous.position.z};
    const StorylandNativePoint to{playerTo.x - current.position.x,
        playerTo.y - current.position.y, playerTo.z - current.position.z};
    const float angle = previous.heading * Radians;
    const float turn = std::remainder(current.heading - previous.heading, 360.0f) * Radians;
    const float radius = std::max(std::hypot(previous.halfLength, previous.halfWidth),
        std::hypot(current.halfLength, current.halfWidth));
    const float translation = std::hypot(to.x - from.x, to.y - from.y);
    const float dimensions = std::hypot(current.halfLength - previous.halfLength,
        current.halfWidth - previous.halfWidth);
    if (!std::isfinite(angle) || !std::isfinite(turn) || !std::isfinite(translation) ||
        !std::isfinite(from.z) || !std::isfinite(to.z) || !std::isfinite(radius) || !std::isfinite(dimensions))
        return false;
    const auto interpolate = [](float first, float second, float phase) { return first + (second - first) * phase; };
    const auto contact = [&](float phase) {
        const float z = interpolate(from.z, to.z, phase);
        const float height = interpolate(previous.height, current.height, phase);
        if (z > height || z + playerHeight < 0) return false;
        const float facing = angle + turn * phase, c = std::cos(facing), s = std::sin(facing);
        const float x = interpolate(from.x, to.x, phase), y = interpolate(from.y, to.y, phase);
        const float dx = std::max(std::fabs(x * c + y * s) -
            interpolate(previous.halfLength, current.halfLength, phase), 0.0f);
        const float dy = std::max(std::fabs(-x * s + y * c) -
            interpolate(previous.halfWidth, current.halfWidth, phase), 0.0f);
        return dx * dx + dy * dy <= playerRadius * playerRadius;
    };
    if (contact(0) || contact(1)) return true;
    const auto search = [&](const auto& self, float first, float last, unsigned depth) -> bool {
        const float zFirst = interpolate(from.z, to.z, first), zLast = interpolate(from.z, to.z, last);
        const float maximumHeight = std::max(interpolate(previous.height, current.height, first),
            interpolate(previous.height, current.height, last));
        if (std::min(zFirst, zLast) > maximumHeight || std::max(zFirst, zLast) + playerHeight < 0) return false;
        const float middle = (first + last) * .5f;
        if (contact(middle)) return true;
        const float facing = angle + turn * middle, c = std::cos(facing), s = std::sin(facing);
        const float ax = interpolate(from.x, to.x, first), ay = interpolate(from.y, to.y, first);
        const float bx = interpolate(from.x, to.x, last), by = interpolate(from.y, to.y, last);
        const float length = interpolate(previous.halfLength, current.halfLength, middle);
        const float width = interpolate(previous.halfWidth, current.halfWidth, middle);
        if (turn == 0 && dimensions == 0 && zFirst >= 0 && zLast >= 0 &&
            zFirst <= std::min(previous.height, current.height) && zLast <= std::min(previous.height, current.height)) {
            return segmentRectangleDistanceSquared(ax * c + ay * s, -ax * s + ay * c,
                bx * c + by * s, -bx * s + by * c, length, width) <= playerRadius * playerRadius;
        }
        const float x = interpolate(from.x, to.x, middle), y = interpolate(from.y, to.y, middle);
        const float dx = std::max(std::fabs(x * c + y * s) - length, 0.0f);
        const float dy = std::max(std::fabs(-x * s + y * c) - width, 0.0f);
        const float possibleTravel = (translation + std::fabs(turn) * radius + dimensions) * (last - first) * .5f;
        if (std::hypot(dx, dy) > playerRadius + possibleTravel || depth == 20) return false;
        return self(self, first, middle, depth + 1) || self(self, middle, last, depth + 1);
    };
    return search(search, 0, 1, 0);
}

void storylandArmPlayerEvasion(StorylandPlayerReactionState& state,
    const StorylandReactionVehicle& vehicle) {
    if (!vehicle.active || vehicle.slot < 0 || state.active()) return;
    state.evadingVehicle = vehicle;
    state.evadeFrames = 5;
}

StorylandPlayerReactionPose storylandPlayerReactionPose(const StorylandPlayerReactionState& state,
    bool victim) {
    StorylandPlayerReactionPose pose;
    pose.animation = actionAnimation(state.action, victim, state.vcs);
    if (pose.animation < 0) return pose;
    pose.group = pairedAction(state.action) ? 63 : 0;
    pose.seconds = victim && state.duration > 0 ? state.elapsed / state.duration * state.victimDuration : state.elapsed;
    if (state.action == StorylandPlayerReaction::RearGrabNeckBreakEnd)
        pose.seconds = std::min(state.elapsed, victim ? state.victimDuration : state.playerAnimationDuration);
    pose.repeat = repeatingAction(state.action);
    pose.blendRate = state.action == StorylandPlayerReaction::CarGesture ? 4.0f : 8.0f;
    return pose;
}

bool storylandPlayerReactionHoldsTarget(const StorylandPlayerReactionState& state,
    StorylandReactionTargetKind kind, int32_t handle, uint32_t model) {
    return state.holding() && state.target.kind == kind && state.target.handle == handle &&
        state.target.model == model;
}

StorylandPlayerReactionResult storylandUpdatePlayerReaction(StorylandPlayerReactionState& state,
    const StorylandPlayerReactionContext& context, const StorylandPlayerReactionInput& input,
    const StorylandReactionTarget* selectedTarget, const std::vector<StorylandReactionVehicle>& vehicles,
    const StorylandPlayerReactionBindings& bindings) {
    const bool jumpPressed = input.jump && !state.jumpHeld;
    const bool interactPressed = input.interact && !state.interactHeld;
    const bool firePressed = input.fire && !state.fireHeld;
    const bool grabPressed = input.grab && !state.grabHeld;
    state.grabHeld = input.grab;
    state.jumpHeld = input.jump;
    state.interactHeld = input.interact;
    state.fireHeld = input.fire;
    state.vcs = context.vcs;
    const bool validScriptedDive = state.scriptedDive() && context.health > 0 && context.onFoot &&
        std::isfinite(context.seconds) && context.seconds > 0;
    if (!validScriptedDive && !validContext(context, state.action == StorylandPlayerReaction::EvadeDive)) {
        const bool released = state.holding();
        state.clearAction();
        state.evadeFrames = 0;
        state.gesturePending = false;
        StorylandPlayerReactionResult result;
        result.releasedTarget = released;
        return result;
    }
    if (context.controlFrame != state.lastControlFrame) {
        state.lastControlFrame = context.controlFrame;
        if (state.evadeFrames > 0) --state.evadeFrames;
        if (state.evadeFrames == 0) state.evadingVehicle = {};
    }
    if (state.evadeFrames > 0) {
        const auto found = std::find_if(vehicles.begin(), vehicles.end(), [&](const auto& vehicle) {
            return vehicle.active && vehicle.slot == state.evadingVehicle.slot &&
                vehicle.model == state.evadingVehicle.model;
        });
        if (found == vehicles.end()) {
            state.evadeFrames = 0;
            state.evadingVehicle = {};
        } else state.evadingVehicle = *found;
    }
    if (state.active() && state.playerHealth > context.health) {
        state.clearAction();
        state.evadeFrames = 0;
    }
    state.playerHealth = context.health;
    if (state.holding()) {
        const bool finishingNeckBreak = selectedTarget && sameTarget(state.target, *selectedTarget) &&
            ((state.action == StorylandPlayerReaction::RearGrabNeckBreak && state.contactApplied) ||
                state.action == StorylandPlayerReaction::RearGrabNeckBreakEnd);
        if (!selectedTarget || (!finishingNeckBreak && (!selectedTarget->eligible || selectedTarget->health <= 0)) ||
            !sameTarget(state.target, *selectedTarget) || !context.unarmed) {
            state.clearAction();
            StorylandPlayerReactionResult result;
            result.releasedTarget = true;
            return result;
        }
        state.target = *selectedTarget;
        const bool releasing = state.action == StorylandPlayerReaction::GrabRelease ||
            state.action == StorylandPlayerReaction::RearGrabRelease ||
            state.action == StorylandPlayerReaction::RearGrabNeckBreakEnd;
        if ((input.cancel || (!input.target && !state.latchedGrip)) && !releasing &&
            state.action != StorylandPlayerReaction::RearGrabNeckBreak) {
            if (!beginAction(state, state.rearGrip() ? StorylandPlayerReaction::RearGrabRelease :
                StorylandPlayerReaction::GrabRelease, bindings)) state.clearAction();
        } else if (repeatingAction(state.action)) {
            const bool rear = state.rearGrip();
            if (interactPressed || grabPressed) beginAction(state, rear ? StorylandPlayerReaction::RearGrabNeckBreak :
                StorylandPlayerReaction::GrabShove, bindings);
            else if (jumpPressed) beginAction(state, rear ? StorylandPlayerReaction::RearGrabKnee :
                StorylandPlayerReaction::GrabKnee, bindings);
            else if (firePressed) beginAction(state, rear ? StorylandPlayerReaction::RearGrabKnee :
                StorylandPlayerReaction::GrabJab, bindings);
            else if (!rear && input.forward && state.action != StorylandPlayerReaction::GrabWalk)
                beginAction(state, StorylandPlayerReaction::GrabWalk, bindings);
            else if (!rear && !input.forward && state.action == StorylandPlayerReaction::GrabWalk)
                beginAction(state, StorylandPlayerReaction::GrabHold, bindings);
        }
        return advanceAction(state, context, bindings);
    }
    if (state.action == StorylandPlayerReaction::CarGesture &&
        (context.movementSpeed > 0.001f || input.forward || !context.meleeWeapon ||
            input.target || input.fire || jumpPressed || interactPressed))
        state.clearAction();
    if (state.active()) return advanceAction(state, context, bindings);
    const StorylandReactionVehicle* closest = nullptr;
    float nearest = std::numeric_limits<float>::infinity();
    for (const auto& vehicle : vehicles) {
        if (!storylandVehicleThreatensPlayer(vehicle, context)) continue;
        const float distance = std::hypot(vehicle.position.x - context.position.x,
            vehicle.position.y - context.position.y);
        if (distance < nearest) { closest = &vehicle; nearest = distance; }
    }
    if (closest) storylandArmPlayerEvasion(state, *closest);
    if (jumpPressed && state.evadeFrames > 0 && state.evadingVehicle.active) {
        const auto& vehicle = state.evadingVehicle;
        float sideX = -vehicle.velocity.y, sideY = vehicle.velocity.x;
        const float away = sideX * (context.position.x - vehicle.position.x) +
            sideY * (context.position.y - vehicle.position.y);
        if (away < 0) { sideX = -sideX; sideY = -sideY; }
        state.heading = std::atan2(sideY, sideX) / Radians;
        if (beginAction(state, StorylandPlayerReaction::EvadeDive, bindings)) {
            state.evadeFrames = 0;
            state.evadingVehicle = {};
            state.gesturePending = false;
            return advanceAction(state, context, bindings);
        }
    }
    if (context.vcs && context.unarmed && !input.cancel &&
        (grabPressed || (input.target && interactPressed)) &&
        selectedTarget && selectedTarget->eligible && selectedTarget->health > 0) {
        const float dx = selectedTarget->position.x - context.position.x;
        const float dy = selectedTarget->position.y - context.position.y;
        const float distance = std::hypot(dx, dy);
        const float bearing = std::atan2(dy, dx) / Radians;
        if (distance <= context.meleeReach && std::fabs(selectedTarget->position.z - context.position.z) < 1 &&
            std::fabs(std::remainder(bearing - context.heading, 360.0f)) <= 60) {
            const bool rear = std::fabs(std::remainder(selectedTarget->heading - bearing, 360.0f)) < 60;
            state.latchedGrip = grabPressed;
            state.snapAfterGrab = grabPressed && rear && !(context.extendedMovement && context.crouched);
            state.heading = bearing;
            state.target = *selectedTarget;
            state.gripDistance = distance;
            if (beginAction(state, rear ? StorylandPlayerReaction::RearGrabStart :
                StorylandPlayerReaction::GrabStart, bindings)) return advanceAction(state, context, bindings);
            state.clearAction();
        }
    }
    if (state.gesturePending && int32_t(state.gestureExpiresAt - context.now) <= 0)
        state.gesturePending = false;
    if (state.gesturePending && !context.meleeWeapon) state.gesturePending = false;
    if (state.gesturePending && !input.target && !input.fire && !input.forward && !context.firing &&
        context.movementSpeed <= 0.001f) {
        state.heading = context.heading;
        if (beginAction(state, StorylandPlayerReaction::CarGesture, bindings)) {
            state.gesturePending = false;
            return advanceAction(state, context, bindings);
        }
    }
    return {};
}
