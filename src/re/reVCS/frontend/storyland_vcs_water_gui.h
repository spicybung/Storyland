#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "imgui.h"

namespace StorylandVcsWaterGui {

enum class Phase { Revealing, Accelerating, Cruising, Slowing, Floating, Away };

struct AnimationState {
    Phase phase = Phase::Revealing;
    double age = 0.0;
    float phaseTime = 0.0f;
    float position = -140.0f;
    float speed = 0.0f;
    float cruiseSpeed = 155.0f;
    float width = 0.0f;
    float stopPosition = 0.0f;
    float stopDuration = 0.0f;
    float returnDelay = 0.0f;
    float reveal = 0.0f;
    int direction = 1;
    bool stopPlanned = false;
    bool initialized = false;
    std::uint32_t randomSeed = 0x61C3E520u;
};

inline AnimationState& GetState() {
    static AnimationState state;
    return state;
}

inline float Random01(AnimationState& state) {
    state.randomSeed = state.randomSeed * 1664525u + 1013904223u;
    return float(state.randomSeed >> 8) / 16777216.0f;
}

inline float WaveHeight(float x, double time) {
    return float(std::sin(x * 0.031 + time * 1.35) * 2.4 +
                 std::sin(x * 0.067 - time * 1.75) * 1.1 +
                 std::sin(x * 0.013 + time * 0.43) * 1.5);
}

inline void BeginPass(AnimationState& state) {
    state.cruiseSpeed = 140.0f + Random01(state) * 48.0f;
    state.stopPlanned = state.width > 260.0f && Random01(state) < 0.68f;
    state.stopPosition = state.width * (0.32f + Random01(state) * 0.36f);
    state.stopDuration = 1.8f + Random01(state) * 3.0f;
    state.phase = Phase::Accelerating;
    state.phaseTime = 0.0f;
}

inline void Advance(AnimationState& state, float deltaTime, float width) {
    if (!std::isfinite(width) || width <= 0.0f) return;
    if (!state.initialized) {
        state = AnimationState{};
        state.initialized = true;
        state.width = width;
    } else if (state.width != width) {
        const float progress = (state.position + 140.0f) / (state.width + 280.0f);
        state.position = progress * (width + 280.0f) - 140.0f;
        state.stopPosition *= width / state.width;
        state.width = width;
    }
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;

    float remaining = std::min(deltaTime, 0.25f);
    while (remaining > 0.0f) {
        const float step = std::min(remaining, 1.0f / 60.0f);
        remaining -= step;
        state.age += step;
        state.phaseTime += step;

        if (state.phase == Phase::Revealing) {
            const float fraction = std::min(state.phaseTime / 1.25f, 1.0f);
            state.reveal = fraction * fraction * (3.0f - 2.0f * fraction);
            if (fraction >= 1.0f) BeginPass(state);
            continue;
        }
        if (state.phase == Phase::Away) {
            if (state.phaseTime >= state.returnDelay) {
                state.direction = -state.direction;
                BeginPass(state);
            }
            continue;
        }
        if (state.phase == Phase::Floating) {
            if (state.phaseTime >= state.stopDuration) {
                state.phase = Phase::Accelerating;
                state.phaseTime = 0.0f;
            }
            continue;
        }

        const float distanceToStop = (state.stopPosition - state.position) * state.direction;
        const float brakingDistance = state.speed * state.speed / 240.0f;
        if (state.stopPlanned && distanceToStop <= brakingDistance + state.speed * step) {
            state.phase = Phase::Slowing;
            state.phaseTime = 0.0f;
            state.stopPlanned = false;
        }

        const float previousSpeed = state.speed;
        if (state.phase == Phase::Slowing) {
            state.speed = std::max(0.0f, state.speed - 120.0f * step);
        } else {
            state.speed = std::min(state.cruiseSpeed, state.speed + 175.0f * step);
            if (state.speed >= state.cruiseSpeed) state.phase = Phase::Cruising;
        }
        state.position += state.direction * (previousSpeed + state.speed) * 0.5f * step;

        if (state.phase == Phase::Slowing && state.speed <= 0.0f) {
            state.phase = Phase::Floating;
            state.phaseTime = 0.0f;
        }
        if ((state.direction > 0 && state.position > width + 140.0f) ||
            (state.direction < 0 && state.position < -140.0f)) {
            state.position = state.direction > 0 ? width + 140.0f : -140.0f;
            state.speed = 0.0f;
            state.returnDelay = 1.3f + Random01(state) * 2.5f;
            state.phase = Phase::Away;
            state.phaseTime = 0.0f;
        }
    }
}

inline void DrawWater(ImDrawList* drawList, const ImVec2& origin, float width,
                      float bottom, float scale, const AnimationState& state) {
    const float revealedWidth = width * state.reveal;
    const float top = bottom - 23.0f * scale;
    const float step = std::max(8.0f, width / 512.0f);
    for (float x = 0.0f; x < revealedWidth; x += step) {
        const float nextX = std::min(x + step, revealedWidth);
        const float firstY = top + WaveHeight(x, state.age) * scale;
        const float nextY = top + WaveHeight(nextX, state.age) * scale;
        const float firstX = origin.x + x * scale;
        const float secondX = origin.x + nextX * scale;
        drawList->AddQuadFilled(ImVec2(firstX, firstY), ImVec2(secondX, nextY),
            ImVec2(secondX, bottom), ImVec2(firstX, bottom), IM_COL32(11, 115, 180, 185));
        drawList->AddLine(ImVec2(firstX, firstY), ImVec2(secondX, nextY),
            IM_COL32(100, 225, 255, 240), 1.7f * scale);
        if (std::sin(x * 0.075 - state.age * 2.1) > 0.82) {
            drawList->AddLine(ImVec2(firstX, firstY + 7.0f * scale),
                ImVec2(secondX, nextY + 7.0f * scale), IM_COL32(161, 238, 255, 72), scale);
        }
    }
    drawList->AddRectFilled(ImVec2(origin.x, bottom),
        ImVec2(origin.x + revealedWidth * scale, bottom + 1.5f * scale),
        IM_COL32(238, 48, 145, 255));
}

inline void DrawRider(ImDrawList* drawList, ImTextureID riderTexture, float x,
                      float surfaceY, float scale, const AnimationState& state) {
    const float moving = std::min(state.speed / state.cruiseSpeed, 1.0f);
    const float bob = float(std::sin(state.age * 2.7)) * (2.0f - moving);
    const float accelerationTilt = state.phase == Phase::Accelerating ?
        -0.10f * (1.0f - moving) : 0.0f;
    const float tilt = float(std::sin(state.age * 2.0)) * (0.035f - moving * 0.02f) +
        accelerationTilt - moving * 0.035f;
    const float cosine = std::cos(tilt);
    const float sine = std::sin(tilt);
    const float face = float(state.direction);

    if (moving > 0.02f) {
        for (int index = 0; index < 7; ++index) {
            const float phase = float(std::fmod(state.age * 1.8 + index / 7.0, 1.0));
            const float distance = 30.0f + phase * 80.0f * moving;
            const float wakeY = surfaceY + (3.0f + phase * 5.0f) * scale;
            const int opacity = int((1.0f - phase) * moving * 155.0f);
            drawList->AddLine(ImVec2(x - face * distance * scale, wakeY),
                ImVec2(x - face * (distance + 10.0f) * scale, wakeY - 1.5f * scale),
                IM_COL32(221, 248, 255, opacity), 1.3f * scale);
            const float sprayX = x - face * (28.0f + phase * 33.0f) * scale;
            const float sprayY = surfaceY - std::sin(phase * 3.14159265f) * 9.0f * moving * scale;
            drawList->AddCircleFilled(ImVec2(sprayX, sprayY), (1.3f - phase * 0.6f) * scale,
                IM_COL32(213, 245, 255, opacity), 6);
        }
    }
    if (moving < 0.35f) {
        for (int index = 0; index < 2; ++index) {
            const float phase = float(std::fmod(state.age * 0.36 + index * 0.5, 1.0));
            const int opacity = int((1.0f - phase) * (1.0f - moving) * 105.0f);
            drawList->AddEllipse(ImVec2(x, surfaceY + 5.0f * scale),
                ImVec2((32.0f + phase * 24.0f) * scale, (3.0f + phase * 3.5f) * scale),
                IM_COL32(196, 239, 255, opacity), 0.0f, 24, scale);
        }
    }

    if (!riderTexture) return;

    // The transparent source includes wide margins. Select its opaque rider and hull.
    constexpr float imageSize = 1254.0f;
    const ImVec2 uvTopLeft(74.0f / imageSize, 188.0f / imageSize);
    const ImVec2 uvBottomRight(1176.0f / imageSize, 1073.0f / imageSize);
    const float spriteWidth = 83.0f * scale;
    const float spriteHeight = spriteWidth * (885.0f / 1102.0f);
    const float bottomY = surfaceY + (11.0f + bob) * scale;
    const auto corner = [&](float px, float py) {
        return ImVec2(x + (px * cosine - py * sine) * face,
                      bottomY + (px * sine + py * cosine));
    };
    const float halfWidth = spriteWidth * 0.5f;
    drawList->AddImageQuad(riderTexture,
        corner(-halfWidth, -spriteHeight), corner(halfWidth, -spriteHeight),
        corner(halfWidth, 0.0f), corner(-halfWidth, 0.0f),
        uvTopLeft, ImVec2(uvBottomRight.x, uvTopLeft.y), uvBottomRight,
        ImVec2(uvTopLeft.x, uvBottomRight.y));
}

inline void Render(bool vcsFormatSelected, bool enabled = true, float uiScale = 1.0f,
                   ImTextureID riderTexture = ImTextureID(0)) {
    AnimationState& state = GetState();
    if (!vcsFormatSelected || !enabled || !ImGui::GetCurrentContext()) {
        state = AnimationState{};
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiIO& io = ImGui::GetIO();
    if (viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f) return;
    const float scale = std::isfinite(uiScale) ? std::clamp(uiScale, 0.5f, 3.0f) : 1.0f;
    const float width = viewport->Size.x / scale;
    const float bottom = viewport->Pos.y + viewport->Size.y - 1.5f * scale;
    Advance(state, io.DeltaTime, width);

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    drawList->PushClipRect(viewport->Pos,
        ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), true);
    DrawWater(drawList, viewport->Pos, width, bottom, scale, state);
    if (state.phase != Phase::Revealing && state.phase != Phase::Away) {
        const float x = viewport->Pos.x + state.position * scale;
        const float surfaceY = bottom - 23.0f * scale + WaveHeight(state.position, state.age) * scale;
        DrawRider(drawList, riderTexture, x, surfaceY, scale, state);
    }
    drawList->PopClipRect();
}

}
