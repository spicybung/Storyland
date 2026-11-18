#pragma once

#include "src/graphics/storyland_graphics_quality.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

enum class StorylandAspectRatio : int {
    Auto = 0,
    Ratio4x3,
    Ratio16x9,
    Ratio16x10,
    Ratio21x9
};

inline constexpr const char* StorylandAspectRatioNames[] = {
    "Auto (native)", "4:3", "16:9", "16:10", "21:9"
};

inline double storylandAspectRatioValue(StorylandAspectRatio mode) {
    switch (mode) {
    case StorylandAspectRatio::Ratio4x3: return 4.0 / 3.0;
    case StorylandAspectRatio::Ratio16x9: return 16.0 / 9.0;
    case StorylandAspectRatio::Ratio16x10: return 16.0 / 10.0;
    case StorylandAspectRatio::Ratio21x9: return 21.0 / 9.0;
    default: return 0.0;
    }
}

struct StorylandViewRectangle {
    int x = 0, y = 0, width = 0, height = 0;

    double aspect() const { return height > 0 ? double(width) / height : 1.0; }
    int openGlY(int framebufferHeight) const { return framebufferHeight - y - height; }
    bool contains(float px, float py) const {
        return px >= x && py >= y && px < x + width && py < y + height;
    }
};

inline StorylandViewRectangle storylandFitView(int width, int height, StorylandAspectRatio mode) {
    StorylandViewRectangle view{0, 0, std::max(0, width), std::max(0, height)};
    if (width <= 0 || height <= 0) return view;
    const double ratio = storylandAspectRatioValue(mode);
    if (ratio <= 0) return view;
    if (double(width) / height > ratio)
        view.width = std::clamp(int(std::lround(height * ratio)), 1, width);
    else
        view.height = std::clamp(int(std::lround(width / ratio)), 1, height);
    view.x = (width - view.width) / 2;
    view.y = (height - view.height) / 2;
    return view;
}

struct StorylandDisplaySettings {
    StorylandAspectRatio aspectRatio = StorylandAspectRatio::Auto;
    StorylandGraphicsQualitySettings graphicsQuality;
    bool trails=true;

    bool load(const std::filesystem::path& directory, std::string& error) {
        aspectRatio = StorylandAspectRatio::Auto;
        graphicsQuality={};
        trails=true;
        const auto path = directory / "display.cfg";
        std::error_code filesystemError;
        if (!std::filesystem::exists(path, filesystemError)) {
            if (filesystemError) { error = filesystemError.message(); return false; }
            error.clear(); return true;
        }
        std::ifstream input(path);
        if (!input) { error = "Could not read display settings: " + path.string(); return false; }
        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            const auto separator = line.find(' ');
            if (separator == std::string::npos) continue;
            const auto key=line.substr(0,separator);
            const auto value = line.substr(separator + 1);
            if(key=="fxaa") {graphicsQuality.fxaa=value=="1";continue;}
            if(key=="anisotropic16x") {graphicsQuality.anisotropic16x=value=="1";continue;}
            if(key=="trails") {trails=value=="1";continue;}
            if(key!="aspect")continue;
            if (value == "auto") aspectRatio = StorylandAspectRatio::Auto;
            else if (value == "4:3") aspectRatio = StorylandAspectRatio::Ratio4x3;
            else if (value == "16:9") aspectRatio = StorylandAspectRatio::Ratio16x9;
            else if (value == "16:10") aspectRatio = StorylandAspectRatio::Ratio16x10;
            else if (value == "21:9") aspectRatio = StorylandAspectRatio::Ratio21x9;
        }
        if (input.bad()) { error = "Could not finish reading display settings."; return false; }
        error.clear(); return true;
    }

    bool save(const std::filesystem::path& directory, std::string& error) const {
        if (directory.empty()) { error = "The game user-files directory is unavailable."; return false; }
        std::error_code filesystemError;
        std::filesystem::create_directories(directory, filesystemError);
        if (filesystemError) { error = filesystemError.message(); return false; }
        const auto path = directory / "display.cfg";
        std::ofstream output(path, std::ios::trunc);
        if (!output) { error = "Could not write display settings: " + path.string(); return false; }
        const int index = int(aspectRatio);
        output << "version 2\naspect " << (index > 0 && index < 5 ? StorylandAspectRatioNames[index] : "auto") << '\n';
        output << "fxaa " << int(graphicsQuality.fxaa) << '\n';
        output << "anisotropic16x " << int(graphicsQuality.anisotropic16x) << '\n';
        output << "trails " << int(trails) << '\n';
        output.close();
        if (!output) { error = "Could not finish writing display settings."; return false; }
        error.clear(); return true;
    }
};
