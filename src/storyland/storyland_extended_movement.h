#pragma once

#include "src/leeds/leeds_native_world.h"
#include <cstdint>

struct StorylandPlayTestRuntime;

enum class StorylandExtendedAction : uint8_t {
    None,
    ClimbJump,
    ClimbIdle,
    ClimbPull,
    ClimbStand,
    ClimbFinish,
    ClimbVault,
    ClimbVaultFinish,
    CrouchRollLeft,
    CrouchRollRight
};

struct StorylandExtendedMovement {
    bool crouched = false;
    bool crouchPressed = false;
    bool crouchHeld = false;
    bool leftHeld = false;
    bool rightHeld = false;
    bool justLanded = false;
    float crouchMoveAmount = 0.0f;
    float crouchMoveCommand = 0.0f;
    float crouchStanceBlend = 0.0f;
    float crouchMoveBlend = 0.0f;
    float crouchAnimationTime = 0.0f;
    float crouchMoveHeading = 0.0f;
    float crouchTravelDistance = 0.0f;
    int crouchMotionDirection = 0;
    StorylandExtendedAction action = StorylandExtendedAction::None;
    float elapsed = 0.0f;
    float fallingDistance = 0.0f;
    float highestFallZ = 0.0f;
    StorylandNativePoint climbStart{};
    StorylandNativePoint climbDestination{};
    StorylandNativePoint climbLedge{};
    StorylandNativePoint climbHang{};
    StorylandNativePoint climbEdge{};
    float climbHeading = 0.0f;
    bool climbPullRequested = false;
    bool climbDropRequested = false;
    bool climbAligning = false;
    bool climbVault = false;
    bool climbSideLip = false;
    bool sprintJumpBoost = false;

    bool busy() const {
        return action != StorylandExtendedAction::None;
    }
    bool climbing() const {
        return action >= StorylandExtendedAction::ClimbJump &&
            action <= StorylandExtendedAction::ClimbVaultFinish;
    }
    bool rolling() const {
        return action == StorylandExtendedAction::CrouchRollLeft ||
            action == StorylandExtendedAction::CrouchRollRight;
    }
    void cancel() {
        action = StorylandExtendedAction::None;
        elapsed = 0.0f;
        justLanded = false;
        fallingDistance = 0.0f;
        climbPullRequested = false;
        climbDropRequested = false;
        climbAligning = false;
        climbVault = false;
        climbSideLip = false;
        sprintJumpBoost = false;
    }
};

float storylandExtendedActionDuration(StorylandExtendedAction action);
const char* storylandExtendedActionClip(StorylandExtendedAction action);
bool storylandExtendedActionLoops(StorylandExtendedAction action);
bool storylandTryStartClimb(StorylandExtendedMovement& state, const StorylandNativeWorld& world,
    const StorylandNativePoint& position, float headingDegrees, bool airborne = false);
bool storylandStepExtendedMovement(StorylandExtendedMovement& state,
    StorylandPlayTestRuntime& runtime, const StorylandNativeWorld& world,
    StorylandNativePoint& position, float headingDegrees, float seconds);
void storylandCancelExtendedMovement(StorylandPlayTestRuntime& runtime, bool clearCrouch);
