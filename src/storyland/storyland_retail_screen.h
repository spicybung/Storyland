#pragma once

#include <algorithm>
#include <cmath>

namespace StorylandRetailScreen {

constexpr int Automatic = 0;
constexpr int FourByThree = 1;
constexpr int SixteenByNine = 2;
constexpr double Ps2Aspect = 4.0 / 3.0;

// An automatic game viewport uses the console's output aspect ratio, not the
// current desktop window. Explicitly selected PC aspect ratios remain available.
inline int resolvedAspectMode(int requestedMode, bool playing, bool pspAssets) {
    if (requestedMode != Automatic || !playing) return requestedMode;
    return pspAssets ? SixteenByNine : FourByThree;
}

inline bool hasNativePs2WidescreenCrop(int requestedMode, bool playing, bool pspAssets) {
    return playing && !pspAssets && requestedMode > FourByThree;
}

// Stories PS2 widescreen retains the 4:3 scene's horizontal coverage, and
// crops the top and bottom. Preserve the 4:3 horizontal half-angle at wider
// output ratios; do not change the camera position or its look target.
inline double ps2VerticalFieldOfView(double verticalFourByThreeDegrees,
                                     double outputAspect,
                                     bool nativeWidescreenCrop) {
    if (!nativeWidescreenCrop || !std::isfinite(verticalFourByThreeDegrees) ||
        verticalFourByThreeDegrees <= 0.0 || verticalFourByThreeDegrees >= 179.0 ||
        !std::isfinite(outputAspect) || outputAspect <= Ps2Aspect)
        return verticalFourByThreeDegrees;

    constexpr double Pi = 3.1415926535897932384626433832795;
    const double verticalHalfAngle = verticalFourByThreeDegrees * (Pi / 360.0);
    const double nativeHorizontalHalfTangent = std::tan(verticalHalfAngle) * Ps2Aspect;
    return (360.0 / Pi) * std::atan(nativeHorizontalHalfTangent / outputAspect);
}

} // namespace StorylandRetailScreen
