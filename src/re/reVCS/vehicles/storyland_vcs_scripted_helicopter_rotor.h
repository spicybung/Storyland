#pragma once

namespace StorylandVcsScriptedHelicopterRotor {

inline float advance(float speed) {
    if (speed < 0.22f) speed += 0.001f;
    return speed;
}

inline bool flightReady(float speed) {
    return speed >= 0.15f;
}

}
