#pragma once

#include "src/storyland/storyland_mission_vm.h"
#include "src/re/reVCS/camera/storyland_vcs_intoxication.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

inline void storylandPublishMissionPad(StorylandMissionEnvironment& world,
    const std::array<int16_t,20>& input) {
    world.previousPadState = world.padState;
    world.padState = input;
    for (size_t index = world.padMovementSamples.size() - 1; index > 0; --index)
        world.padMovementSamples[index] = world.padMovementSamples[index - 1];
    world.padMovementSamples[0] = input;
    for (size_t index = 0; index < world.analogueSticks.size(); ++index)
        world.analogueSticks[index] = input[index];
    const bool crossEdge = input[16] != 0 && world.previousPadState[16] == 0;
    world.confirmJustPressed = crossEdge;
    world.messageSkipPressed = crossEdge;
    world.creditsSkipPressed = crossEdge;
}

namespace StorylandVcsPad {
enum Button : size_t {
    LeftX, LeftY, RightX, RightY, L1, L2, R1, R2, Up, Down, Left, Right,
    Start, Select, Square, Triangle, Cross, Circle, L3, R3
};

inline int16_t strongestAxis(int16_t stick, int16_t directional) {
    return std::abs(int(stick)) > std::abs(int(directional)) ? stick : directional;
}

inline int16_t scaledAxis(int16_t value, float scale) {
    const double exact = double(value) * double(scale);
    float rounded = float(exact);
    if (std::isfinite(rounded) && std::abs(double(rounded)) > std::abs(exact))
        rounded = std::nextafter(rounded, 0.0f);
    // EE CVT.W.S saturates before the native signed-halfword narrowing.
    const int32_t converted = !std::isfinite(rounded) ?
        (std::signbit(rounded) ? INT32_MIN : INT32_MAX) :
        rounded >= 2147483648.0f ? INT32_MAX :
        rounded <= -2147483648.0f ? INT32_MIN : int32_t(rounded);
    const uint32_t bits = uint32_t(converted) & 0xFFFFu;
    return int16_t(bits < 0x8000u ? int32_t(bits) : int32_t(bits) - 0x10000);
}

inline int16_t directionalAxis(const std::array<int16_t,20>& input, bool vertical) {
    const int positive = input[vertical ? Down : Right];
    const int negative = input[vertical ? Up : Left];
    return int16_t((positive - negative) / 2);
}

inline int16_t movementAxis(const StorylandMissionEnvironment& world, bool vertical,
    bool delayed) {
    if (world.controllerMode > 3) return 0;
    const size_t delay = std::min(size_t(world.drunkInputDelay),
        world.padMovementSamples.size() - 1);
    const auto& input = delayed && delay ? world.padMovementSamples[delay] : world.padState;
    const int16_t stick = input[vertical ? LeftY : LeftX];
    if (world.controllerMode == 1 || world.controllerMode == 3) return stick;
    return strongestAxis(stick, directionalAxis(input, vertical));
}

inline int32_t vehicleClass(const StorylandMissionEnvironment& world) {
    if (!world.inVehicle) return -1;
    if (world.activeVehicleCameraClass) return world.activeVehicleCameraClass();
    if (world.vehicleHandlingClass && world.occupiedMissionVehicle >= 0)
        return world.vehicleHandlingClass(world.occupiedMissionVehicle);
    return -1;
}

inline int32_t vehicleModel(const StorylandMissionEnvironment& world) {
    if (!world.inVehicle) return -1;
    const auto found = world.missionVehicles.find(world.occupiedMissionVehicle);
    return found != world.missionVehicles.end() ? found->second.model : world.occupiedVehicleModel;
}

inline bool menuAxisEdge(StorylandMissionEnvironment& world, int32_t action) {
    // Native globals 0x487A80..86 are sampled independently by each query.
    const size_t direction = action == 40 ? 0 : action == 41 ? 1 : action == 38 ? 2 : 3;
    const int16_t sample = world.padState[action >= 40 ? LeftY : LeftX];
    const int16_t previous = world.padDirectionSamples[direction];
    world.padDirectionSamples[direction] = sample;
    if (action == 38 || action == 40) return sample < -15 && previous >= -5;
    if (action == 39) return sample >= 26 && previous < 21;
    return sample >= 16 && previous < 6;
}
}

inline int32_t storylandVcsPadState(StorylandMissionEnvironment& world, int32_t action) {
    using namespace StorylandVcsPad;
    const auto& input = world.padState;
    const auto& previous = world.previousPadState;
    const auto held = [&](Button button) { return input[button] != 0; };
    const auto edge = [&](Button button) { return held(button) && previous[button] == 0; };
    const auto recent = [&](Button button) { return held(button) || previous[button] != 0; };
    const uint16_t mode = world.controllerMode;
    const bool validMode = mode <= 3;
    const bool controls = world.control;
    const auto cameraMode = [&]() {
        return world.activeCameraMode ? world.activeCameraMode() : world.camera.mode;
    };
    const auto aircraftLook = [&]() {
        const int32_t type = vehicleClass(world);
        return type == 3 || (type == 5 && vehicleModel(world) != 198);
    };
    const auto hornButton = [&]() { return mode == 1 ? L1 : mode == 2 ? R1 : L3; };
    const auto targetButton = [&]() { return mode == 3 ? L1 : R1; };

    // VCS PS2 0x3A7C68 dispatches this complete 45-entry table at 0x4BBA10.
    switch (action) {
    case 0:
        return validMode && held(mode == 3 ? Square : R3);
    case 1:
    case 17:
        return controls ? input[Square] : 0;
    case 2: {
        const int16_t drift = StorylandVcsIntoxication::advanceSteeringDrift(
            world.drunkSteeringDrift, world.drunkVisualIntensity, movementAxis(world, false, false));
        if (!controls || !validMode) return 0;
        if (mode >= 2 && held(L1)) {
            const int32_t type = vehicleClass(world);
            if (type != 3 && !(type == 5 && vehicleModel(world) != 198)) return 0;
        }
        return StorylandVcsIntoxication::steeringSample(movementAxis(world, false, true), drift);
    }
    case 3:
        if (!controls || !validMode) return 0;
        if (mode >= 2) {
            const int32_t type = vehicleClass(world);
            if (type == 3 || type == 5) return input[LeftY];
            if (held(L1)) return 0;
        }
        return movementAxis(world, true, false);
    case 4:
        if (!controls || !validMode) return 0;
        return mode == 3 ? directionalAxis(input, false) : input[RightX];
    case 5:
        return controls && validMode && held(Circle) ? input[LeftY] : 0;
    case 6:
    case 7:
        return controls && validMode ? movementAxis(world, action == 7, true) : 0;
    case 8:
    case 9: {
        if (!controls || cameraMode() == 16) return 0;
        const Button selected = action == 8 ? L2 : R2;
        const Button opposite = action == 8 ? R2 : L2;
        if (aircraftLook()) return held(selected) && !held(opposite);
        if (!validMode) return 0;
        if (mode < 2)
            return held(selected) && previous[selected] != 0 && !recent(opposite);
        return held(L1) && (action == 8 ? input[LeftX] < -10 : input[LeftX] > 10);
    }
    case 10:
        if (!controls || !validMode || cameraMode() == 16) return 0;
        if (mode < 2) return recent(L2) && recent(R2);
        if (aircraftLook()) return recent(L1) && recent(R1);
        return held(L1) && std::abs(int(input[LeftX])) < 30 && input[LeftY] > 10;
    case 11:
        return controls && world.area == 0 && held(R3);
    case 12:
        return controls && validMode && held(hornButton());
    case 13:
        return controls && validMode && edge(hornButton());
    case 14:
        return controls && held(Circle);
    case 15:
        return controls && edge(Circle);
    case 16:
        if (!controls || !validMode) return 0;
        if (mode >= 2 && vehicleModel(world) == 275) return held(Cross) && held(Square);
        return input[R1];
    case 18:
        return controls && held(Triangle);
    case 19:
        return controls && edge(Triangle);
    case 20:
        return controls && validMode ? input[Circle] : 0;
    case 21:
        return controls && validMode && edge(Circle);
    case 22:
        return controls ? input[Cross] : 0;
    case 23:
        return validMode && edge(mode == 1 ? Up : Select);
    case 24:
        return controls && validMode && edge(L2);
    case 25:
        return controls && validMode && edge(R2);
    case 26:
        return controls && validMode && held(targetButton());
    case 27:
        return controls && validMode && edge(targetButton());
    case 28:
        return controls && validMode && edge(Square);
    case 29:
        return controls && validMode && held(Cross) &&
            std::abs(int(movementAxis(world, false, true))) +
            std::abs(int(movementAxis(world, true, true))) > 64;
    case 30:
        return controls && held(R1) && edge(L2);
    case 31:
        if (!controls) return 0;
        if (world.area == 0 && held(R3) &&
            ((world.characterShooting && world.characterShooting(0)) ||
             (world.characterFighting && world.characterFighting(0)))) return 0;
        return held(L1);
    case 32:
        return controls && validMode && held(Square);
    case 33:
        return controls && validMode && held(mode == 2 ? Square : Cross);
    case 34: {
        int16_t stick = input[LeftX];
        int16_t directional = directionalAxis(input, false);
        const int32_t camera = cameraMode();
        if (camera == 11 || camera == 45) {
            const float scale = world.weaponAimPadParameters[0];
            stick = scaledAxis(stick, scale);
            directional = scaledAxis(directional, scale);
        }
        return strongestAxis(stick, directional);
    }
    case 35: {
        const int32_t axis = strongestAxis(input[LeftY], directionalAxis(input, true));
        return world.padLookInverted ? -axis : axis;
    }
    case 36:
        return controls ? input[RightX] : 0;
    case 37:
        return controls ? (world.padLookInverted ? -int32_t(input[RightY]) : input[RightY]) : 0;
    case 38:
    case 39:
    case 40:
    case 41: {
        const Button direction = action == 38 ? Left : action == 39 ? Right : action == 40 ? Up : Down;
        return menuAxisEdge(world, action) || edge(direction);
    }
    case 42:
        return held(Cross);
    case 43:
        return held(Triangle);
    case 44: {
        const bool pressed = edge(Cross);
        // 0x3A80E8 clears the same camera music-fade latch as 02A0(true).
        if (pressed) world.musicDoesFade = true;
        return pressed;
    }
    default:
        // The native out-of-range branch stores the collected action unchanged.
        return action;
    }
}
