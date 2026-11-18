#pragma once

#include "src/audio/storyland_mission_voice_neural_backend.h"

#include <atomic>
#include <functional>
#include <string>
#include <thread>

class StorylandMissionVoicePreview {
public:
    using StartPlayback = std::function<bool(const StorylandMissionVoicePcm&, std::string&)>;
    using PlaybackActive = std::function<bool()>;
    using StopPlayback = std::function<void()>;

    StorylandMissionVoicePreview(StartPlayback start, PlaybackActive active, StopPlayback stop);
    ~StorylandMissionVoicePreview();
    StorylandMissionVoicePreview(const StorylandMissionVoicePreview&) = delete;
    StorylandMissionVoicePreview& operator=(const StorylandMissionVoicePreview&) = delete;

    bool request(const StorylandPreparedMissionVoice& prepared);
    void update();
    void stop();
    void shutdown();
    bool busy() const { return generating || playing; }
    bool failed() const { return failure; }
    std::string backendName() const { return backend.name(); }
    const std::string& status() const { return message; }

private:
    StorylandMissionVoiceSynthesisBackend backend;
    StartPlayback startPlayback;
    PlaybackActive playbackActive;
    StopPlayback stopPlayback;
    std::thread worker;
    std::atomic_bool cancelled{false};
    std::atomic_bool generationFinished{false};
    StorylandMissionVoicePcm generatedPcm;
    StorylandMissionVoicePcm playingPcm;
    std::string generationError;
    std::string message = "Select a dialogue line to preview its voice.";
    bool generated = false;
    bool generating = false;
    bool playing = false;
    bool failure = false;
};
