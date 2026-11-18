#include "src/audio/storyland_mission_voice_preview.h"

#include <exception>
#include <utility>

StorylandMissionVoicePreview::StorylandMissionVoicePreview(
    StartPlayback start, PlaybackActive active, StopPlayback stop)
    : startPlayback(std::move(start)), playbackActive(std::move(active)),
      stopPlayback(std::move(stop)) {}

StorylandMissionVoicePreview::~StorylandMissionVoicePreview() {
    shutdown();
}

void StorylandMissionVoicePreview::shutdown() {
    stop();
    if (worker.joinable()) worker.join();
    backend.shutdown();
    generating = false;
    generatedPcm = {};
}

bool StorylandMissionVoicePreview::request(const StorylandPreparedMissionVoice& prepared) {
    update();
    if (busy()) return false;
    if (!startPlayback || !playbackActive || !stopPlayback) {
        failure = true;
        message = "Voice playback has no audio output connected.";
        return false;
    }
    if (!prepared.profile || !prepared.line || !prepared.emotion) {
        failure = true;
        message = "The voice request is incomplete.";
        return false;
    }
    const StorylandMissionVoiceRequest request{
        prepared.profile->id, prepared.line->id, prepared.text, prepared.emotion->id};
    StorylandPreparedMissionVoice verified;
    const auto check = storylandPrepareMissionVoice(request, verified);
    if (!check.accepted()) {
        failure = true;
        message = check.message;
        return false;
    }
    cancelled.store(false, std::memory_order_relaxed);
    generationFinished.store(false, std::memory_order_relaxed);
    generatedPcm = {};
    generationError.clear();
    generated = false;
    failure = false;
    message = "Synthesizing " + std::string(verified.profile->displayName) + "...";
    generating = true;
    try {
        worker = std::thread([this, verified] {
            try {
                generated = backend.synthesize(verified, generatedPcm, generationError, &cancelled);
            } catch (const std::exception& error) {
                generationError = "Speech synthesis failed: " + std::string(error.what());
                generated = false;
            } catch (...) {
                generationError = "Speech synthesis failed with an unknown error.";
                generated = false;
            }
            generationFinished.store(true, std::memory_order_release);
        });
    } catch (const std::exception& error) {
        generating = false;
        failure = true;
        message = "Could not start speech synthesis: " + std::string(error.what());
        return false;
    }
    return true;
}

void StorylandMissionVoicePreview::update() {
    if (generating && generationFinished.load(std::memory_order_acquire)) {
        if (worker.joinable()) worker.join();
        generating = false;
        if (cancelled.load(std::memory_order_relaxed)) {
            generatedPcm = {};
            return;
        }
        if (!generated) {
            failure = true;
            message = generationError.empty() ? "Speech synthesis produced no audio." : generationError;
            generatedPcm = {};
            return;
        }
        playingPcm = std::move(generatedPcm);
        std::string error;
        try {
            playing = startPlayback(playingPcm, error);
        } catch (const std::exception& exception) {
            error = exception.what();
            playing = false;
        } catch (...) {
            error = "Audio output failed with an unknown error.";
            playing = false;
        }
        failure = !playing;
        if (!playing) {
            stopPlayback();
            playingPcm = {};
            message = "Voice playback failed: " + (error.empty() ? std::string("audio output rejected PCM.") : error);
            return;
        }
        message = "Playing synthesized voice.";
    }
    if (playing && !playbackActive()) {
        stopPlayback();
        playing = false;
        playingPcm = {};
        message = "Voice playback finished.";
    }
}

void StorylandMissionVoicePreview::stop() {
    cancelled.store(true, std::memory_order_relaxed);
    if (stopPlayback) stopPlayback();
    playing = false;
    playingPcm = {};
    failure = false;
    message = "Voice stopped.";
}
