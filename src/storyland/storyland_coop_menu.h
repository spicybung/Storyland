#pragma once

#include "src/storyland/storyland_multiplayer.h"
#include "imgui.h"

inline void storylandCoopFriendlyFireMenu(StorylandMultiplayer& network, bool& savedFriendlyFire) {
    if (!ImGui::BeginMenu("Friendly Fire")) return;
    const bool guest = network.active() && !network.isHost();
    ImGui::BeginDisabled(guest);
    if (ImGui::MenuItem("On", nullptr, network.friendlyFire()) && network.setFriendlyFire(true))
        savedFriendlyFire = true;
    if (ImGui::MenuItem("Off", nullptr, !network.friendlyFire()) && network.setFriendlyFire(false))
        savedFriendlyFire = false;
    ImGui::EndDisabled();
    if (guest) ImGui::TextDisabled("The host sets friendly fire.");
    ImGui::EndMenu();
}
