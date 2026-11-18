#pragma once

#include "src/platform/linux/storyland_linux_session.h"
#include "src/storyland/storyland_multiplayer.h"
#include "src/storyland/storyland_coop_menu.h"
#include "imgui.h"

struct StorylandCoopPanel {
    bool open = false;
    char address[16]{};
    char code[33]{};
    char message[769]{};
    int port = 28731;

    void draw(StorylandLinuxSession& session, float scale = 1.f) {
        if (!open) return;
        auto& network = session.coopNetwork();
        const auto display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize({std::min(430.f * scale, display.x * .9f),
            std::min(410.f * scale, display.y * .85f)}, ImGuiCond_Appearing);
        if (ImGui::Begin("Co-op", &open)) {
            storylandCoopFriendlyFireMenu(network, session.environment.coopFriendlyFire);
            ImGui::TextWrapped("Open the same game on both devices and start Play Game. Share the host's IPv4 address, UDP port and invitation code.");
            if (!session.canPlayCoop()) ImGui::TextDisabled("Start Play Game to use co-op.");
            if (!network.active()) {
                ImGui::SetNextItemWidth(175.f * scale);
                ImGui::InputText("Host IPv4", address, sizeof(address), ImGuiInputTextFlags_CharsDecimal);
                ImGui::SetNextItemWidth(175.f * scale);
                ImGui::InputInt("UDP port", &port, 0);
                ImGui::SetNextItemWidth(240.f * scale);
                ImGui::InputText("Invitation code", code, sizeof(code), ImGuiInputTextFlags_CharsHexadecimal);
                port = std::clamp(port, 1024, 65535);
                ImGui::BeginDisabled(!session.canPlayCoop());
                const uint32_t game = session.vcs() ? 2u : 1u;
                if (ImGui::Button("Host", {80 * scale, 34 * scale})) network.host(uint16_t(port), game);
                ImGui::SameLine();
                if (ImGui::Button("Join", {80 * scale, 34 * scale}))
                    network.join(address, uint16_t(port), code, game);
                ImGui::EndDisabled();
            } else {
                if (!network.invitation.empty()) {
                    ImGui::Text("UDP port %u", unsigned(network.localPort()));
                    ImGui::TextWrapped("%s", network.invitation.c_str());
                    if (ImGui::Button("Copy invitation", {0, 34 * scale}))
                        ImGui::SetClipboardText(network.invitation.c_str());
                }
                if (ImGui::Button("Disconnect", {0, 34 * scale})) network.disconnect();
            }
            ImGui::TextWrapped("%s", session.coopStatus().c_str());
            if (network.connected()) {
                ImGui::Separator();
                ImGui::Text("Other player %.0f m away", std::hypot(network.remote.x - session.playerPosition.x,
                    network.remote.y - session.playerPosition.y));
                ImGui::BeginChild("Chat history", {0, 90 * scale}, ImGuiChildFlags_Borders);
                const auto& history = network.chat();
                const size_t first = history.size() > 12 ? history.size() - 12 : 0;
                for (size_t index = first; index < history.size(); ++index)
                    ImGui::TextWrapped("%s", history[index].text.c_str());
                ImGui::EndChild();
                ImGui::SetNextItemWidth(std::min(250.f * scale, ImGui::GetContentRegionAvail().x));
                const bool submitted = ImGui::InputText("Message", message, sizeof(message),
                    ImGuiInputTextFlags_EnterReturnsTrue);
                if ((ImGui::Button("Send", {0, 34 * scale}) || submitted) && network.sendChat(message))
                    message[0] = '\0';
            } else ImGui::TextWrapped("Use the same Wi-Fi or a shared VPN. Over the internet, the host needs a reachable UDP port.");
        }
        ImGui::End();
    }
};
