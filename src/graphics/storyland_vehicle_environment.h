#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

struct StorylandVehicleEnvironmentState {
    bool viewerOpen = false;
    bool enabled = true;
    bool frozen = false;
    bool ready = false;
    int faceSize = 64;
    uint32_t texture = 0;
    std::array<float,3> position{};
    std::array<float,16> cameraView{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    uint64_t captures = 0;
    uint64_t lastCaptureTimeMs = 0;
    unsigned nextCaptureFace = 0;
    unsigned completedFaces = 0;
    size_t renderedTriangles = 0;
    std::string diagnostic;
};

inline bool storylandVehicleEnvironmentHasConsumers(int profile, bool viewerOpen,
    bool vehicles, bool weapons, bool reflectivePreview = false) {
    return viewerOpen || vehicles || (profile == 2 && weapons) || reflectivePreview;
}

struct StorylandVehicleReflectionMaterial {
    uint32_t texture = 0;
    int mode = 0;
    float strength = 0.0f;
    bool budokai = false;
};

inline StorylandVehicleReflectionMaterial storylandSelectVehicleReflectionMaterial(
    int profile, bool vehicle, bool weapon, bool charred, float coefficient,
    uint32_t authoredTexture, uint32_t ps2Texture, uint32_t pspTexture,
    const StorylandVehicleEnvironmentState* environment, bool preview = false) {
    StorylandVehicleReflectionMaterial result;
    result.budokai = profile == 2 && !charred && (vehicle || weapon);
    if (charred || !std::isfinite(coefficient) || coefficient <= 0.0f) return result;
    result.strength = coefficient;
    result.texture = authoredTexture;
    if (profile == 3 && pspTexture) result.texture = pspTexture;
    if ((profile == 0 || profile == 2) && !result.budokai)
        result.texture = preview && authoredTexture ? authoredTexture : ps2Texture;
    const bool sceneReflection = result.budokai ||
        (profile != 0 && profile != 2 && profile != 3 && vehicle);
    if (sceneReflection && !result.texture && environment && environment->enabled &&
        environment->ready && environment->texture) {
        result.texture = environment->texture;
        result.mode = 4;
    } else if (result.texture) result.mode = 2;
    return result;
}

// Capturing six sides of the world every frame is not required to keep vehicle
// reflections responsive. Gameplay captures have a fixed update interval;
// the editor and the live cube-map viewer continue updating every frame.
inline bool storylandVehicleEnvironmentCaptureDue(
    const StorylandVehicleEnvironmentState& state, uint64_t nowMs, bool playGame,
    bool progressive = false) {
    if (!state.enabled) return false;
    if (state.frozen && state.ready) return false;
    if (!playGame || state.viewerOpen) return true;

    // Each gameplay update renders one cubemap face, including the initial
    // acquisition. Never render all six world views on the first playable
    // frame after a mission or while the player is looking toward mainland.
    // A fresh probe requires six samples before its texture is marked ready.
    const uint64_t interval = progressive ?
        (state.ready ? 180u : 90u) : (state.ready ? 1250u : 250u);
    return state.lastCaptureTimeMs == 0 || nowMs < state.lastCaptureTimeMs ||
        nowMs - state.lastCaptureTimeMs >= interval;
}

// World-space cube-map reflections do not need regeneration merely because
// the camera rotates. Preserve a finished capture while the player remains in
// the same area, but keep moving vehicles and weather changes responsive.
// The scene probe updates one face per allowed frame while playing a game.
inline bool storylandVehicleEnvironmentProbeMoved(
    const StorylandVehicleEnvironmentState& state,
    const std::array<float,3>& cameraPosition, uint64_t nowMs,
    bool playGame, bool progressive) {
    if (!playGame || state.viewerOpen || !progressive ||
        !state.ready || state.nextCaptureFace != 0u) return true;
    float distanceSquared = 0.0f;
    for (size_t axis = 0; axis < cameraPosition.size(); ++axis) {
        const float delta = cameraPosition[axis] - state.position[axis];
        if (!std::isfinite(delta)) return true;
        distanceSquared += delta * delta;
    }
    if (!std::isfinite(distanceSquared) || distanceSquared >= 64.0f) return true;
    return nowMs < state.lastCaptureTimeMs ||
        nowMs - state.lastCaptureTimeMs >= 8000u;
}

inline bool storylandVehicleEnvironmentInsideProbe(
    const std::array<float,3>& eye, float x, float y, float z,
    float radius = 104.0f) {
    const float dx = x - eye[0];
    const float dy = y - eye[1];
    const float dz = z - eye[2];
    return std::isfinite(dx) && std::isfinite(dy) && std::isfinite(dz) &&
        dx * dx + dy * dy + dz * dz <= radius * radius;
}

inline constexpr std::array<std::array<float,3>,6> StorylandEnvironmentDirections{{
    {{1,0,0}}, {{-1,0,0}}, {{0,1,0}}, {{0,-1,0}}, {{0,0,1}}, {{0,0,-1}}
}};

inline constexpr std::array<std::array<float,3>,6> StorylandEnvironmentUp{{
    {{0,-1,0}}, {{0,-1,0}}, {{0,0,1}}, {{0,0,-1}}, {{0,-1,0}}, {{0,-1,0}}
}};

inline std::array<float,16> storylandEnvironmentFaceView(
    unsigned face, const std::array<float,3>& position) {
    face = std::min(face,5u);
    const auto& forward = StorylandEnvironmentDirections[face];
    const auto& up = StorylandEnvironmentUp[face];
    const std::array<float,3> right{
        forward[1]*up[2]-forward[2]*up[1],
        forward[2]*up[0]-forward[0]*up[2],
        forward[0]*up[1]-forward[1]*up[0]};
    return {right[0],up[0],-forward[0],0,
        right[1],up[1],-forward[1],0,
        right[2],up[2],-forward[2],0,
        -(right[0]*position[0]+right[1]*position[1]+right[2]*position[2]),
        -(up[0]*position[0]+up[1]*position[1]+up[2]*position[2]),
        forward[0]*position[0]+forward[1]*position[1]+forward[2]*position[2],1};
}

inline std::array<float,16> storylandEnvironmentProjection(float farClip) {
    constexpr float nearClip = 0.25f;
    farClip = std::max(farClip,nearClip+1.0f);
    return {1,0,0,0,0,1,0,0,0,0,-(farClip+nearClip)/(farClip-nearClip),-1,
        0,0,-2*farClip*nearClip/(farClip-nearClip),0};
}

inline std::array<std::array<float,3>,3> storylandEnvironmentEyeToWorld(const float* view) {
    std::array<std::array<float,3>,3> result{{{{1,0,0}},{{0,1,0}},{{0,0,1}}}};
    if (!view) return result;
    // A camera basis can contain viewport scaling. Invert it rather than
    // allowing an actor's local transform to rotate the shared environment.
    const float determinant = view[0]*(view[5]*view[10]-view[9]*view[6])-
        view[4]*(view[1]*view[10]-view[9]*view[2])+
        view[8]*(view[1]*view[6]-view[5]*view[2]);
    if (!std::isfinite(determinant) || std::fabs(determinant)<1.0e-12f) return result;
    const float reciprocal = 1.0f/determinant;
    result[0] = {(view[5]*view[10]-view[9]*view[6])*reciprocal,
        (view[9]*view[2]-view[1]*view[10])*reciprocal,
        (view[1]*view[6]-view[5]*view[2])*reciprocal};
    result[1] = {(view[8]*view[6]-view[4]*view[10])*reciprocal,
        (view[0]*view[10]-view[8]*view[2])*reciprocal,
        (view[4]*view[2]-view[0]*view[6])*reciprocal};
    result[2] = {(view[4]*view[9]-view[8]*view[5])*reciprocal,
        (view[8]*view[1]-view[0]*view[9])*reciprocal,
        (view[0]*view[5]-view[4]*view[1])*reciprocal};
    return result;
}

inline std::array<float,2> storylandEnvironmentAtlasUv(
    const std::array<float,3>& direction, int faceSize) {
    const float x=direction[0], y=direction[1], z=direction[2];
    const float ax=std::fabs(x), ay=std::fabs(y), az=std::fabs(z);
    unsigned face=0;
    float horizontal=0, vertical=0, major=0;
    if (ax>=ay && ax>=az) {
        face=x>=0?0:1; horizontal=x>=0?-z:z; vertical=-y; major=ax;
    } else if (ay>=az) {
        face=y>=0?2:3; horizontal=x; vertical=y>=0?z:-z; major=ay;
    } else {
        face=z>=0?4:5; horizontal=z>=0?x:-x; vertical=-y; major=az;
    }
    const float edge=0.5f/std::max(1,faceSize);
    const float u=std::clamp(horizontal/std::max(major,1.0e-12f)*.5f+.5f,edge,1-edge);
    const float v=std::clamp(vertical/std::max(major,1.0e-12f)*.5f+.5f,edge,1-edge);
    return {(float(face%3)+u)/3.0f,(float(face/3)+v)/2.0f};
}
