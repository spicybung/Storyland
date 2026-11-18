#pragma once
#include "src/storyland/storyland_play_test.h"
#include "src/storyland/storyland_native_body.h"
#include <algorithm>
#include <cmath>

enum class StorylandDebugMovement { Walking, Flight, Noclip };

inline StorylandNativePoint storylandMoveDebugPlayer(const StorylandNativeWorld& world,
    StorylandNativePoint position, float cameraYaw, const StorylandPlayTestInput& input,
    StorylandDebugMovement mode, float speed, float seconds) {
    if(mode==StorylandDebugMovement::Walking || !std::isfinite(cameraYaw) ||
        !std::isfinite(speed) || !std::isfinite(seconds) || seconds<=0.0f) return position;
    const float forward=float(input.forward)-float(input.backward);
    const float sideways=float(input.left)-float(input.right);
    const float vertical=float(input.ascend)-float(input.descend);
    const float magnitude=std::sqrt(forward*forward+sideways*sideways+vertical*vertical);
    if(magnitude<=0.0f)return position;
    constexpr float radians=0.01745329251994329577f;
    const float yaw=cameraYaw*radians;
    const float multiplier=input.sprinting?3.0f:input.running?1.0f:0.25f;
    const float distance=std::clamp(speed,1.0f,60.0f)*multiplier*std::min(seconds,0.1f)/magnitude;
    const StorylandNativePoint desired{
        position.x+(std::cos(yaw)*forward-std::sin(yaw)*sideways)*distance,
        position.y+(std::sin(yaw)*forward+std::cos(yaw)*sideways)*distance,
        position.z+vertical*distance};
    if(mode==StorylandDebugMovement::Noclip)return desired;
    StorylandNativeBody body;
    if(!storylandNativeBodyClear(world,position,body))return position;
    return storylandMoveNativeBody(world,position,desired,body).position;
}
