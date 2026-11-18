#pragma once
#include <algorithm>
#include <cstdint>
#include <set>
#include <string>

inline uint32_t storylandCoopGuestStage(bool vcs, const std::set<std::string>& completed,
    const std::string& missionText, uint32_t previous = 0) {
    if (!vcs) return 0;
    uint32_t stage = std::min(previous, 2u);
    for (const auto& mission : completed) {
        if (mission.compare(0, 5, "LAN_B") == 0) stage = std::max(stage, 1u);
        if (mission == "LAN_B06" || mission.compare(0, 5, "LAN_C") == 0)
            stage = 2;
    }
    if (missionText == "LAN_B1" || missionText == "LAN_B01") stage = std::max(stage, 1u);
    if (missionText.compare(0, 5, "LAN_C") == 0) stage = 2;
    return stage;
}

inline const char* storylandCoopGuestModelName(bool vcs, uint32_t stage) {
    if (!vcs) return "gang01";
    if (stage >= 2) return "LanSm";
    if (stage == 1) return "lanst";
    return "Gang7A";
}

inline const char* storylandCoopGuestModelName(bool vcs, const std::set<std::string>& completed,
    const std::string& missionText) {
    return storylandCoopGuestModelName(vcs, storylandCoopGuestStage(vcs, completed, missionText));
}
