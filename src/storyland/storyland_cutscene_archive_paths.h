#pragma once
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

struct StorylandCutsceneArchivePaths {
    std::filesystem::path directory;
    std::filesystem::path image;

    bool valid() const { return !directory.empty() && !image.empty(); }
};

inline std::string storylandCutsceneFilenameLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return char(std::tolower(character));
    });
    return value;
}

inline StorylandCutsceneArchivePaths storylandFindCutsceneArchive(
    const std::vector<std::filesystem::path>& files, bool vcs) {
    const std::vector<std::string> archiveNames = vcs ?
        std::vector<std::string>{"mocapps2", "mocappsp", "mocap"} :
        std::vector<std::string>{"cuts"};

    for (const auto& name : archiveNames) {
        for (const auto& directory : files) {
            if (storylandCutsceneFilenameLower(directory.filename().string()) != name + ".dir") continue;
            for (const auto& image : files) {
                if (directory.parent_path() == image.parent_path() &&
                    storylandCutsceneFilenameLower(image.filename().string()) == name + ".img")
                    return {directory, image};
            }
        }
    }
    if (!vcs) {
        for (const auto& image : files)
            if (storylandCutsceneFilenameLower(image.filename().string()) == "cuts.img")
                return {{}, image};
    }
    return {};
}
