#pragma once

#include <cstdint>
#include <filesystem>

struct StorylandLinuxCtwOptions {
    std::filesystem::path archive;
    std::filesystem::path capture;
    std::filesystem::path report;
    int width = 1280;
    int height = 800;
    uint64_t frames = 0;
    double duration = 0.0;
    bool hideUi = false;
    bool topView = false;
    int focusResource = -1;
};

int storylandRunLinuxCtwViewer(const StorylandLinuxCtwOptions& options);

