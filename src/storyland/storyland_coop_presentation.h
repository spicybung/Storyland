#pragma once

#include "src/storyland/storyland_multiplayer.h"
#include "src/storyland/storyland_play_test_camera.h"
#include "src/storyland/storyland_play_test_hud.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <functional>
#include <string>

struct StorylandCoopSpeech {
    std::string text;
    uint64_t ticket = 0;
    double startedAt = 0;
    double expiresAt = 0;
};

class StorylandCoopPresentation {
public:
    void update(const StorylandMultiplayer& network, double now) {
        const auto& history = network.chat();
        if (!network.connected() || !std::isfinite(now) || now < lastTime) {
            speech = {};
            lastTicket = history.empty() ? 0 : history.back().ticket;
            lastTime = std::isfinite(now) ? now : 0;
            return;
        }
        lastTime = now;
        for (const auto& message : history) {
            auto& bubble = speech[message.local ? 0 : 1];
            if (message.delivery == StorylandMultiplayer::Delivery::Failed) {
                if (bubble.ticket == message.ticket) bubble = {};
                continue;
            }
            if (message.ticket <= lastTicket) continue;
            bubble.text = message.text;
            bubble.ticket = message.ticket;
            bubble.startedAt = now;
            bubble.expiresAt = now + std::clamp(5.0 + message.text.size() * .035, 6.0, 14.0);
        }
        if (!history.empty()) lastTicket = history.back().ticket;
    }

    const StorylandCoopSpeech& message(bool local) const { return speech[local ? 0 : 1]; }

    float opacity(bool local, double now) const {
        const auto& bubble = message(local);
        if (bubble.text.empty() || !std::isfinite(now) || now < bubble.startedAt) return 0;
        return float(std::clamp(bubble.expiresAt - now, 0.0, 1.0));
    }

private:
    std::array<StorylandCoopSpeech, 2> speech{};
    uint64_t lastTicket = 0;
    double lastTime = 0;
};

inline std::string storylandCoopOverheadText(const std::string& text) {
    size_t position = 0;
    unsigned characters = 0;
    while (position < text.size() && characters < 180) {
        ++position;
        while (position < text.size() && (uint8_t(text[position]) & 0xC0) == 0x80) ++position;
        ++characters;
    }
    return position == text.size() ? text : text.substr(0, position) + "...";
}

inline bool storylandCoopHeadScreen(const StorylandStoriesCamera& camera,
    const StorylandNativePoint& head, ImVec2 viewport, bool inverted, ImVec2& point) {
    if (viewport.x <= 0 || viewport.y <= 0) return false;
    const float dx = camera.target.x - camera.eye.x;
    const float dy = camera.target.y - camera.eye.y;
    const float dz = camera.target.z - camera.eye.z;
    const float depth = (head.x - camera.eye.x) * dx +
        (head.y - camera.eye.y) * dy + (head.z - camera.eye.z) * dz;
    if (!std::isfinite(depth) || depth <= .01f) return false;
    std::array<float, 2> projected{};
    if (!storylandCameraScreenCoordinates(camera, head, viewport.x / viewport.y, projected)) return false;
    point = {projected[0] * viewport.x / 480.f, projected[1] * viewport.y / 272.f};
    if (inverted) point.x = viewport.x - point.x;
    return point.x >= 0 && point.x <= viewport.x && point.y >= 0 && point.y <= viewport.y;
}

inline void storylandDrawCoopMeter(ImDrawList* draw, const StorylandRetailHudAssets* assets,
    ImVec2 position, ImVec2 size, float value, float maximum, bool armour, bool vcs, int alpha,
    const std::function<ImTextureID(const std::string&)>& textureFor = {}) {
    const auto texture = [&](const char* name) {
        return textureFor ? textureFor(name) : storylandRetailSpriteTexture(assets, name);
    };
    const float fraction = std::isfinite(value) && std::isfinite(maximum) && maximum > 0 ?
        std::clamp(value / maximum, 0.f, 1.f) : 0;
    const ImVec2 end(position.x + size.x, position.y + size.y);
    const ImU32 color = armour ? IM_COL32(150, 180, 240, alpha) :
        vcs ? IM_COL32(244, 112, 196, alpha) : IM_COL32(235, 235, 235, alpha);
    draw->AddRectFilled(position, end, IM_COL32(5, 5, 9, alpha * 210 / 255), 1);
    if (fraction > 0) {
        draw->PushClipRect(position, {position.x + size.x * fraction, end.y}, true);
        const auto fill = texture(armour ? "bar_inside2dark" : "bar_inside1dark");
        if (fill) draw->AddImage(fill, position, end, {.25f, 0}, {1, 1}, color);
        else draw->AddRectFilled({position.x + 1, position.y + 1}, {end.x - 1, end.y - 1}, color);
        draw->PopClipRect();
    }
    const auto outline = texture("bar_outline");
    if (outline) draw->AddImage(outline, position, end, {.25f, 0}, {1, 1}, IM_COL32(255, 255, 255, alpha));
    else draw->AddRect(position, end, IM_COL32(230, 220, 230, alpha), 1);
}

inline void storylandDrawCoopOverhead(ImDrawList* draw, const StorylandRetailHudAssets* assets,
    const StorylandStoriesCamera& camera, const StorylandNativeWorld& world,
    const StorylandMultiplayer& network, const StorylandCoopPresentation& presentation,
    const StorylandPlayTestRuntime& runtime, const StorylandNativePoint& localPosition,
    double now, bool invertedMap = false,
    const std::function<ImTextureID(const std::string&)>& textureFor = {}) {
    if (!draw || !network.connected() || !runtime.hud || runtime.pauseMenu || runtime.paused) return;
    const auto* environment = runtime.missionEnvironment;
    if (environment && (!environment->wantsHud || environment->widescreen || environment->cutscene.playing)) return;
    const auto viewport = ImGui::GetIO().DisplaySize;
    const float scale = std::clamp(viewport.y / 720.f, .7f, 1.75f);
    const auto remote = network.display(now);
    for (unsigned index = 0; index < 2; ++index) {
        const bool local = index == 0;
        if (local ? environment && !environment->playerVisible : !remote.visible()) continue;
        if (!local && environment && remote.area != environment->area) continue;
        StorylandNativePoint head = local ? localPosition : StorylandNativePoint{remote.x, remote.y, remote.z};
        if (local && runtime.playerTorsoValid) {
            head = runtime.playerTorso;
            head.z += .8f;
        } else if (!local && (remote.actionFlags & 32)) {
            head = {remote.torsoX, remote.torsoY, remote.torsoZ + .8f};
        } else head.z += 2.05f;
        const float distance = std::hypot(head.x - camera.eye.x, head.y - camera.eye.y, head.z - camera.eye.z);
        if (!std::isfinite(distance) || distance > 65.f) continue;
        ImVec2 point;
        if (!storylandCoopHeadScreen(camera, head, viewport, invertedMap, point)) continue;
        if (!local && storylandTraceNativeSegment(world, camera.eye, head) < .985f) continue;
        const int alpha = int(255.f * std::clamp((65.f - distance) / 12.f, 0.f, 1.f));
        const float width = 65.f * scale;
        const float height = 7.f * scale;
        const float health = local ? runtime.health : remote.health;
        const float armour = local ? runtime.armour : remote.armour;
        const float maximumHealth = local ? runtime.maximumHealth() : remote.maximumHealth;
        const float maximumArmour = local ? runtime.maximumArmour() : remote.maximumArmour;
        const float left = std::clamp(point.x - width * .5f, 1.f, std::max(1.f, viewport.x - width - 1));
        const float top = std::clamp(point.y, 1.f, std::max(1.f, viewport.y - height * 2 - 3 * scale));
        storylandDrawCoopMeter(draw, assets, {left, top}, {width, height}, health, maximumHealth,
            false, runtime.crime.vcs, alpha, textureFor);
        if (armour > 0) storylandDrawCoopMeter(draw, assets, {left, top + height + 2 * scale},
            {width, height}, armour, maximumArmour, true, runtime.crime.vcs, alpha, textureFor);
        const float speechAlpha = presentation.opacity(local, now);
        if (speechAlpha <= 0) continue;
        const std::string text = storylandCoopOverheadText(presentation.message(local).text);
        const float fontSize = 16.f * scale;
        const float wrap = std::min(260.f * scale, viewport.x - 16.f);
        const auto extent = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, wrap, text.c_str());
        const float textX = std::clamp(point.x - extent.x * .5f, 4.f, std::max(4.f, viewport.x - extent.x - 4));
        const float textY = std::max(2.f, top - extent.y - 6 * scale);
        const int textAlpha = int(alpha * speechAlpha);
        draw->AddText(ImGui::GetFont(), fontSize, {textX + scale, textY + scale},
            IM_COL32(0, 0, 0, textAlpha), text.c_str(), nullptr, wrap);
        draw->AddText(ImGui::GetFont(), fontSize, {textX, textY},
            local ? IM_COL32(210, 235, 255, textAlpha) : IM_COL32(255, 245, 210, textAlpha),
            text.c_str(), nullptr, wrap);
    }
}
