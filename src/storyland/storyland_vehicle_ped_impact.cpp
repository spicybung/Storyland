#include "src/storyland/storyland_vehicle_ped_impact.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_play_test_weapons.h"
#include "src/storyland/storyland_native_body.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include "src/storyland/storyland_sprint.h"
#include <limits>

namespace {
using Point = StorylandNativePoint;
namespace Contact = StorylandVehicleContact;
constexpr float Radians = .01745329251994329577f;
constexpr float PedRadius = .30f;

bool finite(Point point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

Point interpolate(Point first, Point last, float fraction) {
    return Contact::add(first, Contact::scale(Contact::subtract(last, first), fraction));
}

struct VehicleSweep {
    size_t slot = 0;
    uint64_t identity = 0;
    Point first{}, last{}, velocity{}, angular{};
    float firstHeading = 0, turn = 0;
    float mass = 0, turnMass = 0;
    bool fixed = false;
};

struct PedContact {
    Point point{}, normal{}, velocity{}, vehiclePosition{};
    float fraction = 1, penetration = 0, closingSpeed = 0, impulse = 0;
    bool hit = false, current = false;
};

std::array<float,16> vehicleFrame(const StorylandRuntimeVehicle& vehicle,
    Point position, float heading) {
    const auto& definition = vehicle.nativeDefinition;
    return storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
        {position.x, position.y, position.z}, heading, definition.previewMinimumZ,
        vehicle.suspension, vehicle.bodyRoll - vehicle.roadRoll,
        vehicle.bodyPitch + vehicle.roadPitch, false), definition.previewRoot);
}

struct ShapeBounds {
    Point minimum{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity()};
    Point maximum{-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()};
    float horizontalRadius = 0;
};

ShapeBounds collisionBounds(const StorylandNativeCollisionModel& model,
    const std::array<float,16>& frame, Point origin) {
    ShapeBounds result;
    for (float x : {model.minimum.x, model.maximum.x})
        for (float y : {model.minimum.y, model.maximum.y})
            for (float z : {model.minimum.z, model.maximum.z}) {
                const auto corner = storylandVehicleTransformPoint(frame, {x, y, z});
                result.minimum.x = std::min(result.minimum.x, corner[0]);
                result.minimum.y = std::min(result.minimum.y, corner[1]);
                result.minimum.z = std::min(result.minimum.z, corner[2]);
                result.maximum.x = std::max(result.maximum.x, corner[0]);
                result.maximum.y = std::max(result.maximum.y, corner[1]);
                result.maximum.z = std::max(result.maximum.z, corner[2]);
                result.horizontalRadius = std::max(result.horizontalRadius,
                    std::hypot(corner[0] - origin.x, corner[1] - origin.y));
            }
    return result;
}

bool contactAt(const StorylandRuntimeVehicle& vehicle, const VehicleSweep& sweep,
    Point pedestrian, float height, float fraction, PedContact& result) {
    const Point position = interpolate(sweep.first, sweep.last, fraction);
    const float heading = sweep.firstHeading + sweep.turn * fraction;
    const float angle = heading * Radians, cosine = std::cos(angle), sine = std::sin(angle);
    const auto* model = vehicle.nativeDefinition.collision.get();
    std::array<float,16> frame{};
    if (model) {
        frame = vehicleFrame(vehicle, position, heading);
        const auto bounds = collisionBounds(*model, frame, position);
        if (!finite(bounds.minimum) || !finite(bounds.maximum) ||
            pedestrian.x + PedRadius < bounds.minimum.x || pedestrian.x - PedRadius > bounds.maximum.x ||
            pedestrian.y + PedRadius < bounds.minimum.y || pedestrian.y - PedRadius > bounds.maximum.y ||
            pedestrian.z >= bounds.maximum.z - .02f || pedestrian.z + height <= bounds.minimum.z) return false;
    } else if (pedestrian.z >= position.z + vehicle.bodyHeight - .02f ||
        pedestrian.z + height <= position.z + .05f) return false;
    const float dx = pedestrian.x - position.x, dy = pedestrian.y - position.y;
    const float x = dx * cosine + dy * sine, y = dy * cosine - dx * sine;
    const float nearestX = std::clamp(x, -vehicle.halfLength, vehicle.halfLength);
    const float nearestY = std::clamp(y, -vehicle.halfWidth, vehicle.halfWidth);
    float nx = x - nearestX, ny = y - nearestY;
    const float distance = std::hypot(nx, ny);
    if (!model && distance > PedRadius) return false;
    StorylandNativeContact nativeContact;
    if (model) {
        bool collision = false;
        const unsigned spheres = height > .6f ? 4u : 1u;
        for (unsigned sphere = 0; sphere < spheres; ++sphere) {
            const float z = pedestrian.z + std::min(height * .5f, PedRadius) +
                (spheres > 1 ? float(sphere) * (height - 2 * PedRadius) / float(spheres - 1) : 0);
            bool hit = false;
            StorylandNativeContact primitive;
            if (!storylandNativeSphereModelContact({pedestrian.x, pedestrian.y, z},
                std::min(PedRadius, height * .5f), *model,
                frame, hit, &primitive)) return false;
            if (hit && primitive.valid && (!nativeContact.valid || primitive.penetration > nativeContact.penetration))
                nativeContact = primitive;
            collision = collision || hit;
        }
        if (!collision) return false;
    }
    float penetration = std::max(0.0f, PedRadius - distance);
    if (distance > .00001f) {
        nx /= distance;
        ny /= distance;
    } else if (vehicle.halfLength - std::fabs(x) < vehicle.halfWidth - std::fabs(y)) {
        nx = std::copysign(1.0f, x == 0 ? 1 : x);
        ny = 0;
        penetration = vehicle.halfLength + PedRadius - std::fabs(x);
    } else {
        nx = 0;
        ny = std::copysign(1.0f, y == 0 ? 1 : y);
        penetration = vehicle.halfWidth + PedRadius - std::fabs(y);
    }
    result.hit = true;
    result.fraction = fraction;
    result.penetration = penetration;
    result.vehiclePosition = position;
    result.normal = {cosine * nx - sine * ny, sine * nx + cosine * ny, 0};
    const float verticalPenetration = pedestrian.z + height - position.z - .05f;
    if (verticalPenetration < penetration && sweep.velocity.z < 0) {
        result.normal = {0, 0, -1};
        result.penetration = verticalPenetration;
    }
    result.point = {position.x + cosine * nearestX - sine * nearestY,
        position.y + sine * nearestX + cosine * nearestY,
        std::clamp(pedestrian.z + height * .5f, position.z + .05f,
            position.z + vehicle.bodyHeight)};
    if (nativeContact.valid) {
        result.normal = nativeContact.normal;
        result.point = nativeContact.point;
        result.penetration = nativeContact.penetration;
    }
    const Point arm = Contact::subtract(result.point, position);
    result.velocity = Contact::add(sweep.velocity, Contact::cross(sweep.angular, arm));
    return true;
}

PedContact sweepContact(const StorylandRuntimeVehicle& vehicle, const VehicleSweep& sweep,
    Point previous, Point current, Point pedVelocity, float height, float pedMass, bool player) {
    PedContact result;
    const float length = vehicle.halfLength + PedRadius;
    const float width = vehicle.halfWidth + PedRadius;
    float radius = std::hypot(length, width);
    float minimumZ = .05f, maximumZ = vehicle.bodyHeight;
    if (vehicle.nativeDefinition.collision) {
        const auto bounds = collisionBounds(*vehicle.nativeDefinition.collision,
            vehicleFrame(vehicle, {}, 0), {});
        if (!finite(bounds.minimum) || !finite(bounds.maximum) ||
            !std::isfinite(bounds.horizontalRadius)) return result;
        radius = bounds.horizontalRadius + PedRadius;
        minimumZ = bounds.minimum.z;
        maximumZ = bounds.maximum.z;
    }
    const Point from = Contact::subtract(previous, sweep.first);
    const Point to = Contact::subtract(current, sweep.last);
    const Point delta = Contact::subtract(to, from);
    const float denominator = delta.x * delta.x + delta.y * delta.y;
    const float closest = denominator > .000001f ?
        std::clamp(-(from.x * delta.x + from.y * delta.y) / denominator, 0.0f, 1.0f) : 0;
    if (std::hypot(from.x + delta.x * closest, from.y + delta.y * closest) > radius ||
        std::min(from.z, to.z) >= maximumZ ||
        std::max(from.z, to.z) + height <= minimumZ) return result;
    const float travel = std::hypot(delta.x, delta.y, delta.z) +
        std::fabs(sweep.turn * Radians) * radius;
    if (!std::isfinite(travel) || travel > 76.8f) return result;
    const unsigned steps = std::max(1u, unsigned(std::ceil(travel / .075f)));
    if (steps > 1024) return result;
    for (unsigned sample = 0; sample <= steps; ++sample) {
        const float fraction = float(sample) / float(steps);
        if (!contactAt(vehicle, sweep, interpolate(previous, current, fraction),
            height, fraction, result)) continue;
        if (sample) {
            float first = float(sample - 1) / float(steps), last = fraction;
            for (unsigned iteration = 0; iteration < 10; ++iteration) {
                const float middle = (first + last) * .5f;
                PedContact refined;
                if (contactAt(vehicle, sweep, interpolate(previous, current, middle),
                    height, middle, refined)) { last = middle; result = refined; }
                else first = middle;
            }
        }
        break;
    }
    if (!result.hit) return result;
    PedContact last;
    result.current = contactAt(vehicle, sweep, current, height, 1, last);
    const Point relative = Contact::subtract(result.velocity, pedVelocity);
    result.closingSpeed = std::max(0.0f, Contact::dot(relative, result.normal));
    const auto& definition = vehicle.nativeDefinition;
    const auto frame = vehicleFrame(vehicle, result.vehiclePosition,
        sweep.firstHeading + sweep.turn * result.fraction);
    const auto center = storylandVehicleTransformPoint(frame, definition.centerOfMass);
    const Point arm{result.point.x - center[0], result.point.y - center[1], result.point.z - center[2]};
    const Point lever = Contact::cross(arm, result.normal);
    result.impulse = storylandNativePedVehicleImpulse(result.closingSpeed, pedMass,
        sweep.mass, sweep.turnMass, Contact::dot(lever, lever), sweep.fixed,
        player && (!vehicle.hasDriver || vehicle.wrecked));
    return result;
}

float animationDuration(const StorylandPlayTestRuntime& runtime, int animation) {
    if (runtime.missionEnvironment && runtime.missionEnvironment->animationDuration) {
        const float duration = runtime.missionEnvironment->animationDuration(0, animation);
        if (std::isfinite(duration) && duration > 0) return duration;
    }
    if (runtime.reactionBindings.duration) {
        const float duration = runtime.reactionBindings.duration(animation, 0);
        if (std::isfinite(duration) && duration > 0) return duration;
    }
    return 0;
}

void beginFall(StorylandPlayTestRuntime& runtime, StorylandBulletReaction& reaction,
    uint8_t direction, Point velocity, bool player, bool fatal) {
    if (reaction.knockdown) {
        if (fatal) reaction.dying = true;
        return;
    }
    StorylandVcsPedFallState fall;
    fall.fallAnimation = (runtime.crime.vcs ? 28 : 25) + direction;
    uint16_t flags = 0;
    if (runtime.missionEnvironment && runtime.missionEnvironment->animationAssociationFlags)
        runtime.missionEnvironment->animationAssociationFlags(0, fall.fallAnimation, flags);
    else if (runtime.reactionBindings.associationFlags)
        runtime.reactionBindings.associationFlags(fall.fallAnimation, 0, flags);
    fall.getUpAnimation = storylandVcsPedGetUpAnimation(flags);
    fall.fallDuration = animationDuration(runtime, fall.fallAnimation);
    fall.getUpDuration = animationDuration(runtime, fall.getUpAnimation);
    if (!reaction.entitySeedInitialized && !player) {
        reaction.entitySeed = runtime.missionEnvironment ?
            uint8_t(runtime.missionEnvironment->nextNativeRandom()) : 0;
        reaction.entitySeedInitialized = true;
    }
    const auto* environment = runtime.missionEnvironment;
    const uint32_t now = environment ? environment->gameMilliseconds : runtime.trafficClockMilliseconds;
    fall.getUpAfter = storylandVcsPedGetUpDeadline(now, fall.fallDuration,
        player ? 0 : reaction.entitySeed, player || !environment ? 0 : environment->nativeFrameCounter,
        fall.fallDuration > 0 ? (player ? 500 : 1000) : 2000);
    fall.velocity = {velocity.x, velocity.y, velocity.z};
    reaction.fall = fall;
    reaction.knockdown = true;
    reaction.vehicleImpact = true;
    reaction.dying = fatal;
    reaction.recoveryBlocked = false;
    reaction.animation = fatal || fall.fallDuration <= 0 ? -1 : fall.fallAnimation;
    reaction.duration = fall.fallDuration;
    reaction.startedAt = runtime.elapsed;
}

void applyRecoil(StorylandRuntimeVehicle& vehicle, const VehicleSweep& sweep,
    const PedContact& contact, float nativeRecoil) {
    if (sweep.fixed || vehicle.coopControlled) return;
    const float force = contact.impulse / 10.0f +
        nativeRecoil * std::min(sweep.mass / 1400.0f, 1.0f);
    const float angle = (sweep.firstHeading + sweep.turn) * Radians;
    const Point currentVelocity{std::cos(angle) * vehicle.speed - std::sin(angle) * vehicle.lateralSpeed,
        std::sin(angle) * vehicle.speed + std::cos(angle) * vehicle.lateralSpeed, vehicle.verticalSpeed};
    const float available = std::max(0.0f, Contact::dot(currentVelocity, contact.normal));
    const float change = std::min({contact.closingSpeed, available, force * 50.0f / sweep.mass});
    vehicle.speed -= (contact.normal.x * std::cos(angle) + contact.normal.y * std::sin(angle)) * change;
    vehicle.lateralSpeed -= (-contact.normal.x * std::sin(angle) + contact.normal.y * std::cos(angle)) * change;
    if (vehicle.nativeDefinition.vcsModelLayout && vehicle.nativeDefinition.twoWheels() &&
        force > vehicle.vcsJetSkiContactImpulse) {
        vehicle.vcsJetSkiContactImpulse = force;
        vehicle.vcsJetSkiContactNormal = {-contact.normal.x, -contact.normal.y, -contact.normal.z};
    }
    vehicle.bodyContact = true;
}

enum class VictimKind { Player, Ambient, Mission };

struct Victim {
    VictimKind kind;
    int32_t index;
    Point position{}, velocity{};
    float heading = 0, health = 0, mass = 70;
    StorylandPedVehicleContactState* contact = nullptr;
    StorylandBulletReaction* reaction = nullptr;
};

bool damageVictim(StorylandPlayTestRuntime& runtime, const Victim& victim,
    size_t vehicleIndex, float amount, int32_t weapon, uint8_t direction, Point source,
    uint8_t piece = 0) {
    const auto& vehicle = runtime.vehicles[vehicleIndex];
    const bool playerAttack = int32_t(vehicleIndex) == runtime.occupiedVehicle && runtime.playerVehicleSeat < 0;
    if (victim.kind == VictimKind::Player)
        return runtime.inflictDamage(amount, 8, weapon, true, piece, direction, false, &source);
    if (victim.kind == VictimKind::Ambient) {
        auto& ped = runtime.pedestrians[size_t(victim.index)];
        const float before = ped.health + ped.armour;
        storylandDamagePlayTestPedWithVehicle(runtime, size_t(victim.index), amount,
            victim.position, playerAttack, weapon, direction, source, piece);
        return ped.health + ped.armour < before;
    }
    auto& environment = *runtime.missionEnvironment;
    auto& character = environment.missionCharacters.at(victim.index);
    const float before = character.health + character.armour;
    StorylandMissionCharacterDamage request;
    request.handle = victim.index;
    request.model = character.model;
    request.amount = amount;
    request.playerAttack = playerAttack;
    request.kind = 8;
    request.weapon = weapon;
    request.sourceKind = uint8_t(StorylandMissionEnvironment::DamageSourceKind::Vehicle);
    request.sourceHandle = vehicle.scriptHandle;
    request.attackTime = runtime.elapsed;
    request.hitDirection = direction;
    request.hitPiece = piece;
    if (!runtime.routeMissionCharacterDamage || !runtime.routeMissionCharacterDamage(request))
        storylandApplyMissionCharacterDamage(runtime, request);
    const bool accepted = character.health + character.armour < before;
    if (accepted && playerAttack)
        runtime.crime.report(StorylandCrime::Runover, victim.position, runtime.elapsed,
            character.type == 6, uint32_t(victim.index));
    return accepted;
}

float victimHealth(const StorylandPlayTestRuntime& runtime, const Victim& victim) {
    if (victim.kind == VictimKind::Player) return runtime.health;
    if (victim.kind == VictimKind::Ambient) return runtime.pedestrians[size_t(victim.index)].health;
    return runtime.missionEnvironment->missionCharacters.at(victim.index).health;
}

void stopVictim(StorylandPlayTestRuntime& runtime, const Victim& victim) {
    if (victim.kind != VictimKind::Player) {
        const auto kind = victim.kind == VictimKind::Ambient ?
            StorylandReactionTargetKind::Ambient : StorylandReactionTargetKind::Mission;
        if (runtime.playerReaction.target.kind == kind &&
            runtime.playerReaction.target.handle == victim.index) runtime.playerReaction.clearAction();
    }
    if (victim.kind == VictimKind::Player) {
        runtime.playerMelee = {};
        runtime.playerReaction.clearAction();
        runtime.playerCrouchPose = {};
        runtime.playerFall = {};
        runtime.jumpLaunching = false;
        runtime.landingTime = 0;
        runtime.grounded = false;
        runtime.verticalSpeed = victim.reaction->fall.velocity[2];
        runtime.movementSpeed = runtime.playerMoveBlend = 0;
        runtime.playerStopVariant = -1;
        runtime.playerStopTime = 0;
        storylandResetPlayerSprint(runtime);
        storylandClearPlayTestTarget(runtime);
    } else if (victim.kind == VictimKind::Ambient) {
        auto& ped = runtime.pedestrians[size_t(victim.index)];
        ped.melee = {};
        ped.ranged = {};
        ped.contact.clear();
        ped.clearGunThreat();
        ped.combatSpeed = ped.fireFlash = 0;
    } else {
        auto& combat = runtime.missionCombat[victim.index];
        auto& character = runtime.missionEnvironment->missionCharacters.at(victim.index);
        combat.melee = {};
        combat.contact.clear();
        combat.crouchPose = {};
        combat.fall = {};
        combat.landingTime = 0;
        combat.grounded = false;
        combat.verticalSpeed = combat.bulletReaction.fall.velocity[2];
        character.movementSpeed = 0;
        if (character.health > 0) {
            character.animationId = -1;
            character.waitState = 0;
            runtime.missionEnvironment->pedAnimationRequests.erase(victim.index);
            runtime.missionEnvironment->scriptedCharacterAnimations.erase(victim.index);
        }
    }
}

void moveVictim(StorylandPlayTestRuntime& runtime, const StorylandNativeWorld& world,
    Victim& victim, Point displacement, int32_t ignoredVehicle) {
    if (!finite(displacement)) return;
    if (victim.kind == VictimKind::Player) {
        victim.contact->displacement = Contact::add(victim.contact->displacement, displacement);
        return;
    }
    const Point desired = Contact::add(victim.position, displacement);
    const auto movement = storylandMovePlayTestCharacter(world, runtime, victim.position, desired,
        victim.kind == VictimKind::Ambient ? victim.index : INT32_MAX, false, false, false, ignoredVehicle);
    if (movement.clear) victim.position = movement.position;
}

void processVictim(StorylandPlayTestRuntime& runtime, const StorylandNativeWorld& world,
    Victim& victim, const std::vector<VehicleSweep>& sweeps, float seconds) {
    auto& state = *victim.contact;
    auto& reaction = *victim.reaction;
    if (!finite(victim.position) || !finite(victim.velocity)) { state = {}; return; }
    Point previous = state.previousPositionValid && finite(state.previousPosition) ?
        state.previousPosition : victim.position;
    const float travelled = std::hypot(victim.position.x - previous.x,
        victim.position.y - previous.y, victim.position.z - previous.z);
    if (travelled > std::max(2.0f, (std::hypot(victim.velocity.x, victim.velocity.y) + 5) * seconds)) {
        previous = victim.position;
        state.touchingVehicles.clear();
        state.pushedAlong = false;
    }
    reaction.recoveryBlocked = false;
    std::vector<uint64_t> touching;
    bool pushed = false;
    bool crushed = false;
    for (const auto& sweep : sweeps) {
        auto& vehicle = runtime.vehicles[sweep.slot];
        const bool down = reaction.knockdown && !reaction.fall.recovering;
        const float height = down ? .5f : 1.7f;
        const auto contact = sweepContact(vehicle, sweep, previous, victim.position,
            victim.velocity, height, victim.mass, victim.kind == VictimKind::Player);
        if (!contact.hit) continue;
        if (contact.current) touching.push_back(sweep.identity);
        const bool repeated = std::find(state.touchingVehicles.begin(), state.touchingVehicles.end(),
            sweep.identity) != state.touchingVehicles.end();
        if (down && contact.current && !vehicle.nativeDefinition.twoWheels()) {
            reaction.recoveryBlocked = true;
            const uint32_t now = runtime.missionEnvironment ? runtime.missionEnvironment->gameMilliseconds :
                runtime.trafficClockMilliseconds;
            const bool controlsDisabled = runtime.missionEnvironment && !runtime.missionEnvironment->control;
            if (!crushed && reaction.fall.grounded && reaction.fall.getUpAfter < now &&
                (victim.kind == VictimKind::Player || controlsDisabled)) {
                damageVictim(runtime, victim, sweep.slot,
                    victim.kind == VictimKind::Player ? seconds * 50.0f : 1000.0f,
                    storylandNativeRunoverWeapon(runtime.crime.vcs), 0, contact.vehiclePosition);
                crushed = true;
                if (victimHealth(runtime, victim) <= 0) reaction.dying = true;
            }
        }
        if (contact.closingSpeed <= .001f || contact.impulse <= 0) continue;
        pushed = true;
        const float approach = Contact::dot(contact.velocity, contact.normal);
        const auto policy = storylandNativeVehiclePedImpact(victim.kind == VictimKind::Player,
            down, contact.impulse, approach, std::hypot(contact.velocity.x, contact.velocity.y, contact.velocity.z),
            contact.normal.z, state.pushedAlong);
        uint8_t direction = storylandNativeHitDirection(victim.heading,
            -contact.velocity.x, -contact.velocity.y);
        bool accepted = false;
        if (!repeated && policy.damage > 0) {
            int32_t weapon = storylandNativeRammedWeapon(runtime.crime.vcs);
            uint8_t piece = 0;
            Point velocity = Contact::add(victim.velocity,
                Contact::scale(contact.normal, contact.impulse * 50.0f / victim.mass));
            if (policy.carriedVelocity > 0)
                velocity = Contact::scale(contact.velocity, policy.carriedVelocity);
            velocity.z = std::max(0.0f, victim.velocity.z);
            if (policy.lethal) {
                const auto nextRandom = [&]() {
                    if (runtime.missionEnvironment) return runtime.missionEnvironment->nextNativeRandom();
                    runtime.combatRandom = runtime.combatRandom * 1664525u + 1013904223u;
                    return runtime.combatRandom;
                };
                const uint32_t random = nextRandom();
                const float angle = (sweep.firstHeading + sweep.turn * contact.fraction) * Radians;
                const Point offset = Contact::subtract(interpolate(previous, victim.position,
                    contact.fraction), contact.vehiclePosition);
                const float along = offset.x * std::cos(angle) + offset.y * std::sin(angle);
                const float across = -offset.x * std::sin(angle) + offset.y * std::cos(angle);
                const float forwardSpeed = contact.velocity.x * std::cos(angle) + contact.velocity.y * std::sin(angle);
                bool runover = false;
                if (forwardSpeed >= 0) {
                    if (std::fabs(across) > vehicle.halfWidth * .99f) {
                        piece = across <= 0 ? 2 : 3;
                        runover = std::fabs(along) < vehicle.halfLength * .85f;
                    } else if ((along <= .1f || (random & 3u) <= 1u) && (random & 3u) != 0) {
                        piece = 6;
                        runover = true;
                    } else {
                        piece = 1;
                        const float roof = contact.vehiclePosition.z + vehicle.bodyHeight;
                        float originOffset = 1.04f;
                        if (runtime.missionEnvironment && runtime.missionEnvironment->characterOriginOffset) {
                            float decoded = 0;
                            const int32_t handle = victim.kind == VictimKind::Player ?
                                (runtime.crime.vcs ? 0 : 1) : victim.kind == VictimKind::Mission ? victim.index : -1;
                            if (handle >= 0 && runtime.missionEnvironment->characterOriginOffset(handle, decoded) &&
                                std::isfinite(decoded)) originOffset = decoded;
                        }
                        const float clearance = roof - victim.position.z - originOffset;
                        const float upward = (1.5f + float(nextRandom() & 255u) * .002f) *
                            clearance * std::hypot(contact.velocity.x, contact.velocity.y) /
                            std::max(.1f, vehicle.halfLength);
                        const Point travelDirection = Contact::unit(contact.velocity);
                        velocity = Contact::scale(travelDirection, upward * .2f);
                        velocity.z += upward * ((direction == 1 || direction == 3) ? 1.5f : 1.0f);
                        direction = uint8_t((direction + 2) & 3u);
                    }
                }
                if (runover) {
                    weapon = storylandNativeRunoverWeapon(runtime.crime.vcs);
                    velocity = Contact::scale(contact.velocity, .9f);
                    velocity.z = 0;
                    if (direction == 1 || direction == 3) direction = 2;
                }
            }
            accepted = damageVictim(runtime, victim, sweep.slot, policy.damage,
                weapon, direction, contact.vehiclePosition, piece);
            if (accepted || !policy.lethal) {
                beginFall(runtime, reaction, direction, velocity,
                    victim.kind == VictimKind::Player, victimHealth(runtime, victim) <= 0);
                reaction.fall.sourceVehicle = int32_t(sweep.slot);
                reaction.sourceVehicleIdentity = sweep.identity;
                stopVictim(runtime, victim);
            }
        }
        if (!repeated) applyRecoil(vehicle, sweep, contact, accepted ? policy.recoilForce : 0);
        if (!down && !reaction.knockdown && contact.current) {
            PedContact endpoint;
            if (contactAt(vehicle, sweep, victim.position, height, 1, endpoint))
                moveVictim(runtime, world, victim,
                    Contact::scale(endpoint.normal, endpoint.penetration + .005f), int32_t(sweep.slot));
        }
    }
    state.touchingVehicles = std::move(touching);
    state.pushedAlong = pushed;
    state.previousPosition = victim.position;
    state.previousPositionValid = true;
}
}

void storylandUpdateVehiclePedestrianImpacts(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, std::vector<StorylandNativeTrafficActor>& pedestrians,
    std::vector<StorylandNativeTrafficActor>& vehicles,
    const std::vector<StorylandNativeTrafficActor>& vehicleStarts, const Point& player, float seconds) {
    if (runtime.paused || runtime.pauseMenu || !std::isfinite(seconds) || seconds <= 0) return;
    const float step = std::min(seconds, .1f);
    const bool cinematic = runtime.missionEnvironment && runtime.missionEnvironment->cutscene.playing;
    std::vector<VehicleSweep> sweeps;
    for (size_t index = 0; index < vehicles.size() && index < runtime.vehicles.size(); ++index) {
        auto& vehicle = runtime.vehicles[index];
        const auto& actor = vehicles[index];
        auto& previous = vehicle.pedestrianSweep;
        const auto& definition = vehicle.nativeDefinition;
        const bool identityChanged = previous.model != definition.modelId;
        if (!previous.identity || identityChanged) previous.identity = runtime.nextVehiclePedContactIdentity++;
        if (!runtime.nextVehiclePedContactIdentity) ++runtime.nextVehiclePedContactIdentity;
        VehicleSweep sweep;
        sweep.slot = index;
        sweep.identity = previous.identity;
        const bool continuous = previous.initialized && !identityChanged &&
            finite(previous.position) && std::isfinite(previous.heading);
        sweep.first = continuous ? previous.position :
            index < vehicleStarts.size() ? vehicleStarts[index].position : actor.position;
        sweep.firstHeading = continuous ? previous.heading :
            index < vehicleStarts.size() ? vehicleStarts[index].yaw : actor.yaw;
        sweep.last = actor.position;
        sweep.turn = std::remainder(actor.yaw - sweep.firstHeading, 360.0f);
        previous.position = actor.position;
        previous.heading = actor.yaw;
        previous.model = definition.modelId;
        previous.initialized = !cinematic && vehicle.collisionEnabled && !vehicle.craneCarried;
        if (!previous.initialized || actor.water || definition.domain != StorylandVehicleDomain::Road ||
            !finite(actor.position) || !std::isfinite(actor.yaw) ||
            !(vehicle.halfLength > 0) || !(vehicle.halfWidth > 0) || !(vehicle.bodyHeight > 0)) continue;
        const float angle = actor.yaw * Radians;
        sweep.velocity = {std::cos(angle) * vehicle.speed - std::sin(angle) * vehicle.lateralSpeed,
            std::sin(angle) * vehicle.speed + std::cos(angle) * vehicle.lateralSpeed,
            definition.hasNativeSuspension && vehicle.grounded ? vehicle.suspensionVelocity : vehicle.verticalSpeed};
        sweep.angular = Contact::angularVelocityFromEulerRates(
            {vehicle.springPitchVelocity, vehicle.springRollVelocity, vehicle.yawVelocity * Radians},
            angle, (vehicle.bodyPitch + vehicle.roadPitch) * Radians);
        sweep.angular = Contact::add(sweep.angular,
            {vehicle.angularVelocity[0], vehicle.angularVelocity[1], vehicle.angularVelocity[2]});
        sweep.mass = std::max(1.0f, vehicle.mass);
        sweep.turnMass = definition.turnMass > 0 ?
            definition.turnMass * vehicle.turnMassMultiplier * (vehicle.heavy ? 5.0f : 1.0f) :
            sweep.mass * (vehicle.halfLength * vehicle.halfLength + vehicle.halfWidth * vehicle.halfWidth) / 3;
        sweep.fixed = vehicle.positionFrozen;
        if (sweep.fixed) { sweep.velocity = {}; sweep.angular = {}; }
        const float travel = std::hypot(sweep.last.x - sweep.first.x,
            sweep.last.y - sweep.first.y, sweep.last.z - sweep.first.z);
        const float speed = std::hypot(sweep.velocity.x, sweep.velocity.y, sweep.velocity.z);
        const float ratedSpeed = std::isfinite(vehicle.handlingTopSpeed) ?
            std::max(0.0f, vehicle.handlingTopSpeed) : 0;
        const float travelLimit = std::max(2.0f, std::max(speed, ratedSpeed) * step * 2 + .5f);
        if (!finite(sweep.velocity) || !finite(sweep.angular) ||
            !std::isfinite(sweep.turnMass) || sweep.turnMass <= 0 ||
            travel > travelLimit || std::fabs(sweep.turn) > 90) continue;
        if (travel > .001f) {
            const Point measured = Contact::scale(Contact::subtract(sweep.last, sweep.first), 1 / step);
            if (Contact::dot(measured, sweep.velocity) >= 0 &&
                Contact::dot(measured, measured) > Contact::dot(sweep.velocity, sweep.velocity))
                sweep.velocity = measured;
        }
        sweeps.push_back(sweep);
    }
    auto* environment = runtime.missionEnvironment;
    const bool playerEligible = runtime.health > 0 && runtime.occupiedVehicle < 0 &&
        runtime.vehicleAction == StorylandVehicleAction::None && runtime.entryTransition < 0 &&
        !runtime.playerWater.active && !runtime.playerJetSkiFallActive &&
        !runtime.vehicleExitPose.active && runtime.debugMovement == StorylandDebugMovement::Walking &&
        !cinematic && (!environment || (environment->playerCollisionEnabled &&
            !environment->playerPositionFrozen && !environment->playerWarped &&
            !environment->activeVcsCharacterAttachment(runtime.crime.vcs ? 0 : 1) &&
            environment->playerAttachedVehicle < 0));
    if (playerEligible) {
        Victim victim{VictimKind::Player, -1, player, runtime.pedestrianContactPlayerVelocity,
            environment ? environment->heading + 90 : 0, runtime.health,
            std::max(1.0f, runtime.playerMass), &runtime.playerVehicleContact, &runtime.playerBulletReaction};
        if (victim.reaction->knockdown) victim.velocity = {victim.reaction->fall.velocity[0],
            victim.reaction->fall.velocity[1], victim.reaction->fall.velocity[2]};
        else victim.velocity.z = runtime.verticalSpeed;
        processVictim(runtime, world, victim, sweeps, step);
    } else runtime.playerVehicleContact = {};
    for (size_t index = 0; index < pedestrians.size() && index < runtime.pedestrians.size(); ++index) {
        auto& ped = runtime.pedestrians[index];
        auto& actor = pedestrians[index];
        if (cinematic || ped.health <= 0 || ped.serviceVehicleIdentity || ped.water.active ||
            (ped.scriptHandle >= 0 && environment && environment->missionCharacters.count(ped.scriptHandle))) {
            ped.vehicleContact = {};
            continue;
        }
        Victim victim{VictimKind::Ambient, int32_t(index), actor.position, ped.contact.velocity,
            actor.yaw, ped.health, 70, &ped.vehicleContact, &ped.bulletReaction};
        if (victim.reaction->knockdown) victim.velocity = {victim.reaction->fall.velocity[0],
            victim.reaction->fall.velocity[1], victim.reaction->fall.velocity[2]};
        processVictim(runtime, world, victim, sweeps, step);
        actor.position = victim.position;
    }
    if (environment) for (auto& entry : environment->missionCharacters) {
        auto& character = entry.second;
        auto& combat = runtime.missionCombat[entry.first];
        if (cinematic || !character.alive || character.health <= 0 || character.removeFromWorld ||
            character.vehicle >= 0 || character.attached || character.positionFrozen ||
            !character.collisionEnabled || !character.loadCollision || character.networkControlled ||
            combat.water.active || environment->vcsPedFalls.count(entry.first) ||
            environment->characterVehicleTasks.count(entry.first)) {
            combat.vehicleContact = {};
            continue;
        }
        Victim victim{VictimKind::Mission, entry.first, {character.x, character.y, character.z},
            combat.contact.velocity, character.heading + 90, character.health, 70,
            &combat.vehicleContact, &combat.bulletReaction};
        if (victim.reaction->knockdown) victim.velocity = {victim.reaction->fall.velocity[0],
            victim.reaction->fall.velocity[1], victim.reaction->fall.velocity[2]};
        else victim.velocity.z = combat.verticalSpeed;
        processVictim(runtime, world, victim, sweeps, step);
        character.x = victim.position.x;
        character.y = victim.position.y;
        character.z = victim.position.z;
    }
}

void storylandApplyPlayerVehicleDisplacement(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, Point& player) {
    const Point displacement = runtime.playerVehicleContact.displacement;
    runtime.playerVehicleContact.displacement = {};
    if (runtime.missionEnvironment && (runtime.missionEnvironment->playerWarped ||
        runtime.missionEnvironment->playerPositionFrozen || runtime.missionEnvironment->cutscene.playing)) {
        runtime.playerVehicleContact = {};
        return;
    }
    if (!finite(displacement) || Contact::dot(displacement, displacement) <= 0) return;
    const auto movement = storylandMovePlayTestCharacter(world, runtime, player,
        Contact::add(player, displacement), -1, false, false, false);
    if (movement.clear) player = movement.position;
    runtime.playerPosition = player;
}
