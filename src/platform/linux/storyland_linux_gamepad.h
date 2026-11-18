#pragma once

#include "src/platform/windows/storyland_controller.h"
#include "src/storyland/storyland_play_test.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

inline int storylandStrongerGamepadAxis(int keyboard, int gamepad) {
    return std::abs(gamepad) > std::abs(keyboard) ? gamepad : keyboard;
}

inline void storylandMergeLinuxGamepad(const StorylandControllerState& controller,
    bool driving, StorylandPlayTestInput& input, std::array<int16_t, 20>& nativePad) {
    const int horizontal = storylandStrongerGamepadAxis(nativePad[0], controller.movementAxis(false));
    const int vertical = storylandStrongerGamepadAxis(nativePad[1], controller.movementAxis(true));
    nativePad[0] = int16_t(storylandStrongerGamepadAxis(nativePad[0], controller.pad[0]));
    nativePad[1] = int16_t(storylandStrongerGamepadAxis(nativePad[1], controller.pad[1]));
    nativePad[2] = int16_t(storylandStrongerGamepadAxis(nativePad[2], controller.pad[2]));
    nativePad[3] = int16_t(storylandStrongerGamepadAxis(nativePad[3], controller.pad[3]));
    for (size_t button = 4; button < nativePad.size(); ++button)
        nativePad[button] = std::max(nativePad[button], int16_t(controller.pad[button]));

    if (driving) {
        input.acceleratorPressure=std::max(input.forward?255:0,std::clamp(controller.pad[16],0,255));
        input.brakePressure=std::max(input.backward?255:0,std::clamp(controller.pad[14],0,255));
        input.forward = input.acceleratorPressure>0;
        input.backward = input.brakePressure>0;
        input.handbrake = input.handbrake || controller.down(6);
        const float lean=std::clamp(-float(controller.movementAxis(true))/128.0f,-1.0f,1.0f);
        if(std::fabs(lean)>std::fabs(input.bikeLean))input.bikeLean=lean;
    } else {
        input.forward = vertical < 0;
        input.backward = vertical > 0;
        input.sprinting = input.sprinting || controller.down(16);
    }
    input.left = horizontal < 0;
    input.right = horizontal > 0;
    if (controller.movementAxis(false) != 0 || (!driving && controller.movementAxis(true) != 0)) {
        input.analogMovement = true;
        input.analogHorizontal = std::clamp(float(horizontal) / 128.0f, -1.0f, 1.0f);
        input.analogForward = driving ? float(input.forward) - float(input.backward) :
            std::clamp(-float(vertical) / 128.0f, -1.0f, 1.0f);
    }
}
