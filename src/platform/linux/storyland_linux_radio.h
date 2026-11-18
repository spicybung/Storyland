#pragma once

#include "src/storyland/storyland_radio_stations.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

struct StorylandLinuxRadioStation {
    StorylandRadioStationInfo info;
    std::filesystem::path path;
    int channel = -1;
};

inline std::vector<StorylandLinuxRadioStation> storylandLinuxDiscoverRadio(
    const std::filesystem::path& gameRoot, bool vcs, std::filesystem::path* policeStream = nullptr) {
    std::vector<StorylandLinuxRadioStation> stations;
    if (policeStream) policeStream->clear();
    std::vector<std::filesystem::path> roots;
    auto base = gameRoot;
    for (unsigned level = 0; level < 3 && !base.empty(); ++level) {
        for (const auto* directory : {"AUDIO", "Audio", "audio", "MUSIC", "music"}) {
            std::error_code error;
            const auto candidate = base / directory;
            if (std::filesystem::is_directory(candidate, error) &&
                std::find(roots.begin(), roots.end(), candidate) == roots.end()) roots.push_back(candidate);
        }
        const auto parent = base.parent_path();
        if (parent == base) break;
        base = parent;
    }
    for (const auto& root : roots) {
        std::error_code error;
        for (std::filesystem::recursive_directory_iterator entry(root,
                std::filesystem::directory_options::skip_permission_denied, error), end;
             entry != end; entry.increment(error)) {
            if (error) { error.clear(); continue; }
            if (entry.depth() > 5) { entry.disable_recursion_pending(); continue; }
            if (!entry->is_regular_file(error)) continue;
            auto extension = entry->path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
                return char(std::tolower(c));
            });
            const bool compressed = extension == ".at3" || extension == ".aa3" || extension == ".oma" ||
                extension == ".mp3" || extension == ".ogg" || extension == ".flac" || extension == ".wav";
            if (extension != ".vb" && !compressed) continue;
            if (storylandPoliceRadioStream(entry->path().stem().string())) {
                const auto bytes = entry->file_size(error);
                if (!error && bytes && policeStream && (policeStream->empty() || extension == ".vb"))
                    *policeStream = entry->path();
                continue;
            }
            auto info = storylandRadioStationInfo(entry->path().stem().string(), vcs);
            if (info.key.empty()) continue;
            const int channel = storylandRadioStationOrder(info.key, vcs);
            if (channel >= (vcs ? 9 : 10)) continue;
            const auto found = std::find_if(stations.begin(), stations.end(), [&](const auto& station) {
                return station.channel == channel;
            });
            if (found == stations.end()) stations.push_back({std::move(info), entry->path(), channel});
            else if (extension == ".vb") found->path = entry->path();
        }
    }
    std::sort(stations.begin(), stations.end(), [](const auto& a, const auto& b) {
        return a.channel < b.channel;
    });
    return stations;
}

inline int storylandLinuxRadioSelection(int current, int direction, size_t stationCount) {
    if (!stationCount) return 0;
    const int choices = int(stationCount) + 1;
    if (current < 0 || current >= choices) current = choices - 1;
    return direction < 0 ? (current + choices - 1) % choices : (current + 1) % choices;
}
