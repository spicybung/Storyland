#pragma once

#include <cmath>
#include <cstdint>
#include <string>

struct StorylandForkliftState {
    uint16_t height = 0;
    int32_t carriedObject = -1;
    float heightOffset = 0;
    float visualOffset = 0;
    bool moving = false;

    template<class Archive> void saveState(Archive& archive) {
        archive(height, carriedObject, heightOffset, visualOffset);
    }
};

inline bool storylandValidForkliftState(const StorylandForkliftState& state) {
    return state.height <= 2500 && state.carriedObject >= -1 &&
        std::isfinite(state.heightOffset) && std::isfinite(state.visualOffset) &&
        state.visualOffset >= -.2f && state.visualOffset <= 1.301f;
}

inline bool storylandDefaultForkliftState(const StorylandForkliftState& state) {
    return state.height == 0 && state.carriedObject == -1 &&
        state.heightOffset == 0 && state.visualOffset == 0;
}

inline float storylandForkliftHeightOffset(uint16_t height) {
    return float(height) * .0006f - .2f;
}

template<class Point> inline Point storylandAnimateForklift(Point point,
    const std::string& part, const std::string& sourcePart, const StorylandForkliftState& state) {
    if (!sourcePart.empty() && part == sourcePart) point.z += state.visualOffset;
    return point;
}
