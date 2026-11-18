#pragma once
#include <cstdint>
#include <string>
#include <vector>
struct RgbaImage;
struct StorylandGimLevel {
    uint32_t offset = 0;
    uint32_t size = 0;
    uint32_t pitch = 0;
    uint32_t storedHeight = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};
struct StorylandGimImage {
    std::string name;
    uint32_t headerOffset = 0;
    uint16_t format = 0;
    bool swizzled = false;
    uint32_t paletteOffset = 0;
    uint32_t paletteCount = 0;
    uint16_t paletteFormat = 0;
    std::vector<StorylandGimLevel> levels;
};
bool storylandIsGim(const std::vector<uint8_t>& bytes);
bool storylandParseGim(const std::vector<uint8_t>& bytes, std::vector<StorylandGimImage>& images, std::string& error);
bool storylandDecodeGim(const std::vector<uint8_t>& bytes, const StorylandGimImage& image, uint32_t level, RgbaImage& output, std::string& error);
bool storylandReplaceGim(std::vector<uint8_t>& bytes, const StorylandGimImage& image, const RgbaImage& replacement, const std::vector<uint8_t>& palette, std::string& error);
