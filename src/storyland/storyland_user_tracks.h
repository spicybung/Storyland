#pragma once
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

inline std::vector<std::filesystem::path> storylandFindUserTracks(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> tracks;
    if (directory.empty()) return tracks;
    for (const auto& folder : {directory, directory / "User Tracks"}) {
        std::error_code error;
        std::filesystem::directory_iterator item(folder, std::filesystem::directory_options::skip_permission_denied, error), end;
        for (size_t visited = 0; !error && item != end && visited < 4096; item.increment(error), ++visited) {
            std::error_code entryError;
            if (!item->is_regular_file(entryError) || entryError) continue;
            auto extension = item->path().extension().u8string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (extension != ".mp3" && extension != ".ogg" && extension != ".wav" && extension != ".flac" &&
                extension != ".m4a" && extension != ".aac" && extension != ".opus") continue;
            if (item->file_size(entryError) == 0 || entryError) continue;
            tracks.push_back(item->path());
        }
    }
    std::sort(tracks.begin(), tracks.end());
    return tracks;
}
