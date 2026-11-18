#pragma once

#include "src/audio/storyland_mission_voice_backend.h"
#include <memory>

namespace storylandNeuralVoiceDetail {
class Worker;
}

class StorylandNeuralMissionVoiceBackend {
public:
    StorylandNeuralMissionVoiceBackend();
    bool available(std::string& error);
    bool synthesize(
        const StorylandPreparedMissionVoice& prepared,
        StorylandMissionVoicePcm& pcm,
        std::string& error,
        const std::atomic_bool* cancelled = nullptr);
    void shutdown();

private:
    std::shared_ptr<storylandNeuralVoiceDetail::Worker> runtime;
};

class StorylandMissionVoiceSynthesisBackend {
public:
    std::string name() const;
    bool available(std::string& error);
    bool synthesize(
        const StorylandPreparedMissionVoice& prepared,
        StorylandMissionVoicePcm& pcm,
        std::string& error,
        const std::atomic_bool* cancelled = nullptr);
    void shutdown();

private:
    StorylandNeuralMissionVoiceBackend neural;
    StorylandEspeakMissionVoiceBackend formant;
};
