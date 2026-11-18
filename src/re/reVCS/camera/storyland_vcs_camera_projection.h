#pragma once
#include "src/storyland/storyland_play_test_camera.h"
#include <cmath>

namespace StorylandVcsCameraProjection {
constexpr float DrawHalfAngle = 0.00872664712369442f;
constexpr float CullHalfAngle = 0.00872638914734125f;
constexpr float DegreesToRadians = 0.017453292519943295f;

// This policy applies only to the original VCS PS2 ordinary on-foot camera.
// Manual/lock cameras and vehicles already have their own lens conversion;
// script cameras, PSP assets and the optional free-aim camera retain theirs.
inline bool ps2FollowPed(bool vcs,bool psp,bool scriptCamera,bool manualAim,
    bool aimLocked,bool freeAim,bool vehicleCamera) {
    return vcs&&!psp&&!scriptCamera&&!manualAim&&!aimLocked&&!freeAim&&!vehicleCamera;
}

inline bool verticalFieldOfView(float horizontal, float aspect, float& vertical) {
    vertical = 0;
    if (!std::isfinite(horizontal) || horizontal <= 0 || horizontal >= 180 ||
        !std::isfinite(aspect) || aspect <= 0) return false;
    const float horizontalTangent = std::tan(horizontal * DrawHalfAngle);
    const float verticalTangent = horizontalTangent / aspect;
    if (!std::isfinite(verticalTangent) || verticalTangent <= 0) return false;
    vertical = 2.0f * std::atan(verticalTangent) / DegreesToRadians;
    return std::isfinite(vertical) && vertical > 0 && vertical < 180;
}

inline bool sphereVisible(const StorylandStoriesCamera& camera, float horizontal,
    const StorylandNativePoint& point, float radius, float nearClip, float farClip,
    const StorylandNativePoint& up = {0,0,1}) {
    const auto finite = [](const StorylandNativePoint& value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    if (!camera.initialized || !finite(camera.eye) || !finite(camera.target) ||
        !finite(point) || !finite(up) || !std::isfinite(horizontal) || horizontal <= 0 ||
        horizontal >= 180 || !std::isfinite(radius) || radius < 0 ||
        !std::isfinite(nearClip) || !std::isfinite(farClip) || farClip <= nearClip) return false;
    StorylandNativePoint forward{camera.target.x-camera.eye.x,
        camera.target.y-camera.eye.y,camera.target.z-camera.eye.z};
    const float length = std::hypot(forward.x,forward.y,forward.z);
    if (length < .000001f) return false;
    forward = {forward.x/length,forward.y/length,forward.z/length};
    StorylandNativePoint right{forward.y*up.z-forward.z*up.y,
        forward.z*up.x-forward.x*up.z,forward.x*up.y-forward.y*up.x};
    const float rightLength = std::hypot(right.x,right.y,right.z);
    if (rightLength < .000001f) return false;
    right = {right.x/rightLength,right.y/rightLength,right.z/rightLength};
    const StorylandNativePoint vertical{right.y*forward.z-right.z*forward.y,
        right.z*forward.x-right.x*forward.z,right.x*forward.y-right.y*forward.x};
    const StorylandNativePoint relative{point.x-camera.eye.x,
        point.y-camera.eye.y,point.z-camera.eye.z};
    const float depth = relative.x*forward.x+relative.y*forward.y+relative.z*forward.z;
    if (depth+radius < nearClip || depth-radius > farClip) return false;
    const float side = relative.x*right.x+relative.y*right.y+relative.z*right.z;
    const float height = relative.x*vertical.x+relative.y*vertical.y+relative.z*vertical.z;
    const float horizontalAngle = horizontal * CullHalfAngle;
    const float verticalAngle = horizontalAngle * .7f;
    const float horizontalCosine = std::cos(horizontalAngle);
    const float horizontalSine = -std::sin(horizontalAngle);
    const float verticalCosine = std::cos(verticalAngle);
    const float verticalSine = -std::sin(verticalAngle);
    return !(radius < side*horizontalCosine+depth*horizontalSine) &&
        !(radius < side*(-horizontalCosine)+depth*horizontalSine) &&
        !(radius < depth*verticalSine+height*(-verticalCosine)) &&
        !(radius < depth*verticalSine+height*verticalCosine);
}
}
