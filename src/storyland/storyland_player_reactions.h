#pragma once
#include "src/leeds/leeds_native_world.h"
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

enum class StorylandPlayerReaction : uint8_t {
    None, EvadeDive, CarGesture, GrabStart, GrabHold, GrabWalk,
    GrabJab, GrabKnee, GrabShove, GrabRelease, RearGrabStart,
    RearGrabHold, RearGrabKnee, RearGrabRelease, RearGrabNeckBreak, RearGrabNeckBreakEnd
};

enum class StorylandReactionTargetKind : uint8_t { None, Ambient, Mission };

struct StorylandReactionTarget {
    StorylandReactionTargetKind kind = StorylandReactionTargetKind::None;
    int32_t handle = -1;
    uint32_t model = 0;
    StorylandNativePoint position;
    float heading = 0, health = 0, armour = 0;
    bool eligible = false;
};

struct StorylandReactionVehicle {
    int32_t slot = -1;
    uint32_t model = 0;
    StorylandNativePoint position, velocity;
    float heading = 0, halfLength = 0, halfWidth = 0, height = 0;
    bool active = false;
};

struct StorylandPlayerReactionBindings {
    std::function<float(int32_t, int32_t)> duration;
    std::function<float(int32_t, int32_t, uint32_t)> contactTime;
    // Local forward, right and up displacement from the actual root track.
    std::function<StorylandNativePoint(int32_t, int32_t, float, float)> rootDelta;
    std::function<bool(int32_t, int32_t, uint16_t&)> associationFlags;
};

struct StorylandPlayerReactionContext {
    StorylandNativePoint position;
    float heading = 0, health = 0, movementSpeed = 0, meleeReach = 0;
    float seconds = 0;
    uint32_t now = 0, controlFrame = 0;
    bool vcs = true, onFoot = true, grounded = true, control = true;
    bool water = false, scripted = false, unarmed = false, meleeWeapon = false;
    bool firing = false, crouched = false, extendedMovement = false;
};

struct StorylandPlayerReactionInput {
    bool jump = false, interact = false, fire = false, target = false;
    bool forward = false, grab = false, cancel = false;
};

struct StorylandPlayerReactionPose {
    int32_t group = 0, animation = -1;
    float seconds = 0, blendRate = 8;
    bool repeat = false;
};

struct StorylandPlayerReactionState {
    StorylandPlayerReaction action = StorylandPlayerReaction::None;
    StorylandReactionTarget target;
    StorylandReactionVehicle evadingVehicle;
    uint32_t lastControlFrame = UINT32_MAX;
    uint32_t annoyanceSerial = 0, gestureExpiresAt = 0;
    uint8_t evadeFrames = 0, temper = 50;
    bool temperInitialized = false, gesturePending = false, vcs = true;
    bool jumpHeld = false, interactHeld = false, fireHeld = false, grabHeld = false;
    bool latchedGrip = false, snapAfterGrab = false;
    bool contactApplied = false, scriptForcedDive = false;
    float elapsed = 0, duration = 0, victimDuration = 0, contactAt = -1, heading = 0;
    float playerHealth = 0, gripDistance = 0, playerAnimationDuration = 0;

    bool active() const { return action != StorylandPlayerReaction::None; }
    bool scriptedDive() const { return action == StorylandPlayerReaction::EvadeDive && scriptForcedDive; }
    bool holding() const;
    bool rearGrip() const;
    void clearAction();
    void reset();
};

struct StorylandPlayerReactionResult {
    bool ownsMovement = false, consumeJump = false, consumeInteract = false;
    bool consumeFire = false, damageContact = false, fatalContact = false;
    bool releasedTarget = false;
    StorylandNativePoint displacement, victimDisplacement;
    StorylandPlayerReactionPose playerPose, victimPose;
};

uint8_t storylandAnnoyPlayerTemper(uint8_t temper, bool policeEvent);
void storylandNotifyPlayerVehicleAnnoyance(StorylandPlayerReactionState& state,
    const StorylandPlayerReactionContext& context, const StorylandReactionVehicle& vehicle,
    uint32_t serial, bool policeEvent = false);
bool storylandVehicleThreatensPlayer(const StorylandReactionVehicle& vehicle,
    const StorylandPlayerReactionContext& context);
bool storylandSweptVehicleTouchesPlayer(const StorylandReactionVehicle& previous,
    const StorylandReactionVehicle& current, const StorylandNativePoint& playerFrom,
    const StorylandNativePoint& playerTo, float playerRadius = .33f, float playerHeight = 1.8f);
void storylandArmPlayerEvasion(StorylandPlayerReactionState& state,
    const StorylandReactionVehicle& vehicle);
float storylandVcsForcedEvasionHeading(float velocityX, float velocityY,
    uint8_t direction, uint32_t random);
bool storylandBeginScriptedPlayerEvasiveDive(StorylandPlayerReactionState& state,
    float heading, const StorylandPlayerReactionBindings& bindings);
StorylandPlayerReactionPose storylandPlayerReactionPose(const StorylandPlayerReactionState& state,
    bool victim = false);
StorylandPlayerReactionResult storylandUpdatePlayerReaction(StorylandPlayerReactionState& state,
    const StorylandPlayerReactionContext& context, const StorylandPlayerReactionInput& input,
    const StorylandReactionTarget* selectedTarget,
    const std::vector<StorylandReactionVehicle>& vehicles,
    const StorylandPlayerReactionBindings& bindings);

struct StorylandPlayTestRuntime;
struct StorylandPlayTestInput;
void storylandUpdatePlayerCombatControls(StorylandPlayTestRuntime& runtime,
    const StorylandPlayTestInput& input, bool aiming);
int32_t storylandPlayerWeaponManualAimMode(const StorylandPlayTestRuntime& runtime,
    int32_t weapon);
bool storylandEqualizerUsesNormalAim(const StorylandPlayTestRuntime& runtime);
bool storylandPlayerUsesBareHands(const StorylandPlayTestRuntime& runtime);
StorylandPlayerReactionPose storylandHeldVictimPose(const StorylandPlayTestRuntime& runtime,
    StorylandReactionTargetKind kind, int32_t handle, uint32_t model,
    const StorylandPlayerReactionPose& terminalPose);
bool storylandRequestScriptedPlayerEvasiveDive(StorylandPlayTestRuntime& runtime,
    int32_t vehicle, std::string& error);
StorylandReactionVehicle storylandPlayerReactionVehicle(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& vehicles, size_t slot);
void storylandNotifyPlayerVehicleAnnoyance(StorylandPlayTestRuntime& runtime,
    StorylandPlayerReactionState& state, const std::vector<StorylandNativeTrafficActor>& vehicles,
    size_t slot, uint32_t serial, bool policeEvent = false);
bool storylandUpdatePlayerCombatReactions(StorylandPlayTestRuntime& runtime,
    StorylandPlayerReactionState& state, const StorylandPlayerReactionBindings& bindings,
    const StorylandNativeWorld& world, std::vector<StorylandNativeTrafficActor>& pedestrians,
    const std::vector<StorylandNativeTrafficActor>& vehicles, StorylandNativePoint& player,
    float& heading, bool& jump, bool& interact, bool targetHeld, bool& fire, bool& reload,
    bool forward, float seconds);
bool storylandPlayerReactionHoldsTarget(const StorylandPlayerReactionState& state,
    StorylandReactionTargetKind kind, int32_t handle, uint32_t model);
