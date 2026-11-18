#pragma once

#include <filesystem>
#include <string>

inline std::filesystem::path storylandGameUserDirectory(const std::filesystem::path& documents, bool vcs) {
    if (documents.empty()) return {};
    return documents / (vcs ? "GTA Vice City Stories User Files" : "GTA Liberty City Stories User Files");
}

inline std::filesystem::path storylandPreviousGameUserDirectory(const std::filesystem::path& documents, bool vcs) {
    if (documents.empty()) return {};
    return documents / (vcs ? "Grand Theft Auto Vice City Stories User Files" : "Grand Theft Auto Liberty City Stories User Files");
}

inline std::filesystem::path storylandGameSaveSlot(const std::filesystem::path& directory, bool vcs, int slot) {
    if (directory.empty() || slot < 1 || slot > 8) return {};
    return directory / (std::string(vcs ? "GTAVCSsf" : "GTALCSsf") + std::to_string(slot) + ".b");
}
