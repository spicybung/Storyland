#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct StorylandManhuntSound {
    std::string name;
    uint64_t offset = 0;
    uint64_t size = 0;
    uint32_t sampleRate = 0;
    uint32_t blockStride = 0;
    bool pcm16 = false;
    uint16_t channels = 1;
    uint32_t sampleCount = 0;
    bool adx = false;
    bool ahx = false;
};
bool storylandReadManhuntSounds(const std::vector<uint8_t> &bytes, std::vector<StorylandManhuntSound> &sounds,
                                std::string &error);
bool storylandDecodeManhuntSound(const std::vector<uint8_t> &bytes, const StorylandManhuntSound &sound,
                                 std::vector<int16_t> &pcm, std::string &error);

bool storylandBuildManhuntAhxMpeg(const std::vector<uint8_t> &bytes, const StorylandManhuntSound &sound,
                                  std::vector<uint8_t> &mpeg, std::string &error);
