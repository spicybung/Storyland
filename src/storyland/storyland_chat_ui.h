#pragma once
#include <imgui.h>
#include <algorithm>
#include <map>
#include "src/storyland/storyland_multiplayer.h"
#include "src/storyland/storyland_chat_translation.h"

class StorylandChatUi {
public:
    bool open = false;
    ImFont* font = nullptr;
    void show() { open = true; focusInput = true; ImGui::GetIO().ClearInputCharacters(); }
    void close(int key = 0) { open = false; suppressedKey = key; }
    bool blocksGameInput() {
        if (suppressedKey && !(GetAsyncKeyState(suppressedKey) & 0x8000)) suppressedKey = 0;
        return open || suppressedKey != 0 || (ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput);
    }
    void settings() {
        translationControls();
        if (ImGui::CollapsingHeader("Custom translation service")) {
            bool changed = ImGui::InputText("LibreTranslate URL", endpoint, sizeof(endpoint));
            changed |= ImGui::InputText("API key", apiKey, sizeof(apiKey), ImGuiInputTextFlags_Password);
            if (changed) { translator.cancelQueued(); translated.clear(); }
            ImGui::TextWrapped("Leave the URL empty to use the built-in online translator. A custom URL must accept LibreTranslate requests. The key is kept only for this session.");
        }
        if (ImGui::Button("Retry translations")) { translator.cancelQueued(); translated.clear(); }
    }
    void draw(StorylandMultiplayer& network) {
        bool translationArrived = false;
        for (auto& result : translator.collect()) {
            const auto found = translated.find(result.ticket);
            if (found != translated.end()) {
                found->second = std::move(result);
                translationArrived = true;
            }
        }
        const auto& messages = network.chat();
        if (!messages.empty()) {
            const uint64_t oldest = messages.front().ticket * 2;
            while (!translated.empty() && translated.begin()->first < oldest) translated.erase(translated.begin());
        } else translated.clear();
        if (enabled) {
            for (const auto& message : messages) {
                if (message.local) continue;
                const uint64_t ticket = message.ticket * 2 + uint64_t(language);
                if (translated.count(ticket)) continue;
                if (translator.submit(ticket, message.text, language == 0 ? "en" : "ru", endpoint, apiKey))
                    translated.emplace(ticket, StorylandChatTranslation::Result{ticket, {}, "Translating..."});
                else break;
            }
        }
        if (!network.connected() && !open) return;
        if (font) ImGui::PushFont(font);
        const auto display = ImGui::GetIO().DisplaySize;
        const float width = std::min(480.0f, std::max(220.0f, display.x - 32));
        const float height = open ? 330.0f : 165.0f;
        ImGui::SetNextWindowPos(ImVec2(16, std::max(48.0f, display.y - height - 30)), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(open ? 0.94f : 0.55f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        if (!open) flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing;
        if (ImGui::Begin("Chat", nullptr, flags)) {
            if (open) translationControls();
            ImGui::BeginChild("Messages", ImVec2(0, open ? -72.0f : -24.0f), false);
            const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 12;
            for (const auto& message : messages) {
                ImGui::PushID(int(message.ticket));
                ImGui::TextColored(message.local ? ImVec4(0.5f, 0.85f, 1, 1) : ImVec4(1, 0.8f, 0.4f, 1),
                    "%s", message.local ? "You" : network.isHost() ? "Guest" : "Host");
                ImGui::SameLine();
                ImGui::PushTextWrapPos(); ImGui::TextUnformatted(message.text.c_str()); ImGui::PopTextWrapPos();
                if (message.local && message.delivery != StorylandMultiplayer::Delivery::Delivered)
                    ImGui::TextDisabled("%s", message.delivery == StorylandMultiplayer::Delivery::Pending ? "Sending..." : "Not delivered");
                const auto found = translated.find(message.ticket * 2 + uint64_t(language));
                if (enabled && !message.local && found != translated.end()) {
                    if (!found->second.text.empty() && found->second.text != message.text) {
                        ImGui::PushTextWrapPos();
                        ImGui::TextColored(ImVec4(0.65f, 1, 0.65f, 1), "%s", found->second.text.c_str());
                        ImGui::PopTextWrapPos();
                    } else if (!found->second.error.empty()) ImGui::TextWrapped("%s", found->second.error.c_str());
                }
                ImGui::PopID();
            }
            const uint64_t latest = messages.empty() ? 0 : messages.back().ticket;
            if ((!open || atBottom) && (latest != lastShown || translationArrived)) ImGui::SetScrollHereY(1.0f);
            lastShown = latest;
            ImGui::EndChild();
            if (open) {
                if (focusInput) { ImGui::GetIO().ClearInputCharacters(); ImGui::SetKeyboardFocusHere(); focusInput = false; }
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##Message", input, sizeof(input), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (network.sendChat(input)) { input[0] = 0; error.clear(); close(VK_RETURN); }
                    else { error = "Not sent: connect first, wait for pending messages, and use at most 768 UTF-8 bytes."; focusInput = true; }
                }
                if (!error.empty()) ImGui::TextWrapped("%s", error.c_str());
                else ImGui::TextDisabled("Enter: send | Escape: close");
            } else ImGui::TextDisabled("T: chat");
        }
        ImGui::End();
        if (font) ImGui::PopFont();
    }
private:
    void translationControls() {
        bool changed = ImGui::Checkbox("Translate chat", &enabled);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(125.0f);
        changed |= ImGui::Combo("Read in", &language, "English\0Russian\0");
        if (changed) { translator.cancelQueued(); translated.clear(); }
        if (enabled) ImGui::TextWrapped(endpoint[0]
            ? "Incoming messages are sent to your translator; translations appear below the originals."
            : "Online translation by MyMemory: incoming messages are sent to the service. Daily limits apply.");
    }
    StorylandChatTranslation translator;
    bool enabled = true, focusInput = false;
    int language = 0;
    int suppressedKey = 0;
    char endpoint[512]{}, apiKey[256]{}, input[769]{};
    std::string error;
    uint64_t lastShown = 0;
    std::map<uint64_t, StorylandChatTranslation::Result> translated;
};
