#pragma once

#include <cstdint>

struct StorylandPcMovementKeys {
    bool forward = false;
    bool backward = false;
    float bikeLean = 0;
    int16_t nativeVertical = 0;
};

inline StorylandPcMovementKeys storylandPcMovementKeys(bool driving, bool motorcycle,
    bool forward, bool backward, bool up, bool down,
    bool leanForward = false, bool leanBackward = false) {
    StorylandPcMovementKeys result;
    motorcycle = driving && motorcycle;
    result.forward = forward || (driving && !motorcycle && up);
    result.backward = backward || (driving && !motorcycle && down);
    if (motorcycle) {
        result.bikeLean = float(up || leanForward) - float(down || leanBackward);
        result.nativeVertical = int16_t(-128 * result.bikeLean);
    } else {
        result.nativeVertical = int16_t(128 * (int(result.backward) - int(result.forward)));
    }
    return result;
}

struct StorylandPcKeyPress {
    bool held = false;

    bool update(bool down, bool enabled) {
        const bool pressed = enabled && down && !held;
        held = down;
        return pressed;
    }
};
