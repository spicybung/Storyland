#pragma once

#include "src/storyland/storyland_multiplayer.h"
#include "imgui.h"
#include <SDL.h>
#include <algorithm>
#include <string>

class StorylandPortableChat {
public:
    bool open = false;

    void show() {
        open = true;
        focusInput = true;
        ImGui::GetIO().ClearInputCharacters();
    }

    void close(SDL_Scancode key = SDL_SCANCODE_UNKNOWN) {
        open = false;
        suppressedKey = key;
        ImGui::SetWindowFocus(nullptr);
    }

    bool blocksGameInput() {
        if (suppressedKey != SDL_SCANCODE_UNKNOWN && !SDL_GetKeyboardState(nullptr)[suppressedKey])
            suppressedKey = SDL_SCANCODE_UNKNOWN;
        return open || suppressedKey != SDL_SCANCODE_UNKNOWN;
    }

    void draw(StorylandMultiplayer& network, float scale = 1.f) {
        if (!network.connected()) {
            if (open) close();
            return;
        }
        if (!open) return;
        const auto display = ImGui::GetIO().DisplaySize;
        const float width = std::min(460.f * scale, display.x - 24.f);
        const float height = std::min(290.f * scale, display.y - 72.f);
        ImGui::SetNextWindowPos({12.f, std::max(40.f, display.y - height - 20.f)}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({width, height}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(.9f);
        if (ImGui::Begin("Co-op chat", nullptr, ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
            ImGui::BeginChild("Messages", {0, -65.f * scale}, false);
            const auto& history = network.chat();
            for (const auto& message : history) {
                ImGui::TextColored(message.local ? ImVec4(.65f, .85f, 1.f, 1.f) : ImVec4(1.f, .9f, .65f, 1.f),
                    "%s", message.local ? "You" : network.isHost() ? "Guest" : "Host");
                ImGui::SameLine();
                ImGui::PushTextWrapPos();
                ImGui::TextUnformatted(message.text.c_str());
                ImGui::PopTextWrapPos();
                if (message.delivery == StorylandMultiplayer::Delivery::Pending) ImGui::TextDisabled("Sending...");
                if (message.delivery == StorylandMultiplayer::Delivery::Failed) ImGui::TextDisabled("Not delivered");
            }
            if (!history.empty() && history.back().ticket != lastShown) {
                lastShown = history.back().ticket;
                ImGui::SetScrollHereY(1.f);
            }
            ImGui::EndChild();
            if (focusInput) {
                ImGui::GetIO().ClearInputCharacters();
                ImGui::SetKeyboardFocusHere();
                focusInput = false;
            }
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##Message", message, sizeof(message), ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (network.sendChat(message)) {
                    message[0] = 0;
                    error.clear();
                    close(SDL_SCANCODE_RETURN);
                } else {
                    error = "Message not sent. Check the connection or shorten the message.";
                    focusInput = true;
                }
            }
            if (error.empty()) ImGui::TextDisabled("Enter: send | Escape: close");
            else ImGui::TextWrapped("%s", error.c_str());
        }
        ImGui::End();
    }

private:
    bool focusInput = false;
    SDL_Scancode suppressedKey = SDL_SCANCODE_UNKNOWN;
    uint64_t lastShown = 0;
    char message[769]{};
    std::string error;
};
