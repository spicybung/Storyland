#pragma once

#include "src/audio/storyland_mission_voice_profiles.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

struct StorylandMissionVoicePcm {
    std::vector<std::int16_t> samples;
    std::uint32_t sampleRate = 0;
    std::string voiceId;
    std::string backendVersion;
    int wordsPerMinute = 0;
    int pitch = 0;
    int volume = 0;
    bool roughnessApplied = false;
};

struct StorylandMissionVoiceBackendInfo {
    bool available = false;
    std::string version;
    std::string libraryPath;
    std::string dataPath;
    std::string message;
};

class StorylandEspeakMissionVoiceBackend {
public:
    bool available(std::string& error);
    StorylandMissionVoiceBackendInfo status();
    bool synthesize(
        const StorylandPreparedMissionVoice& prepared,
        StorylandMissionVoicePcm& pcm,
        std::string& error,
        const std::atomic_bool* cancelled = nullptr);
};
