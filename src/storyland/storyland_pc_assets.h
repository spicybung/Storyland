#pragma once

#include "src/leeds/leeds_texture.h"
#include <filesystem>
#include <string>
#include <vector>

enum class StorylandPcTexturePack { Hud, Frontend, Fonts, Particles, Buttons };

struct StorylandPcTexture {
    std::string group;
    std::string name;
    RgbaImage image;
};

// Available textures are returned even when a missing or invalid file is reported.
bool storylandReadPcTextures(StorylandPcTexturePack pack,
    std::vector<StorylandPcTexture>& textures, std::string& diagnostic,
    const std::filesystem::path& artworkDirectory = {});
