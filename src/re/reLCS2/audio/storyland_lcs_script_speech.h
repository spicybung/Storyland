#pragma once

#include <cstdint>
#include <utility>

inline constexpr uint16_t StorylandLcsScriptSpeechEvent = 0x93;
inline constexpr uint16_t StorylandLcsNoSpeechEvent = 0xD1;
inline constexpr uint32_t StorylandLcsNoSpeechSample = 0x161E;

struct StorylandLcsScriptSpeechSpeaker {
    uint16_t queuedSound = StorylandLcsNoSpeechEvent;
    uint16_t lastQueuedSound = StorylandLcsNoSpeechEvent;
    uint32_t soundStart = 0;
    uint32_t lastSoundStart = 0;
};

struct StorylandLcsScriptSpeechGlobal {
    uint32_t event147ReadyAt = 0;
};

enum class StorylandLcsScriptSpeechResult {
    Waiting,
    GlobalCooldown,
    NativeSilent
};

inline bool storylandLcsScriptSpeechSample(uint16_t event, uint32_t& sample) {
    if (event != StorylandLcsScriptSpeechEvent) return false;
    // SLUS_214.23: all 130 model routes and all special-character routes
    // in GetPedCommentSfx (0x1A88D8) return NO_SAMPLE for event 147.
    sample = StorylandLcsNoSpeechSample;
    return true;
}

template <typename Random>
inline uint32_t storylandLcsScriptSpeechRandomDelay(Random&& random) {
    return uint32_t((uint64_t(random() & 0x7FFFFFFFu) * 1000u) >> 31);
}

template <typename Random>
inline bool storylandQueueLcsScriptSpeech(
    StorylandLcsScriptSpeechSpeaker& speaker,
    uint16_t event,
    uint32_t now,
    float pedZ,
    float cameraZ,
    float cameraAverageSpeed,
    Random&& random) {
    if (event != StorylandLcsScriptSpeechEvent) return false;
    // CPed::Say, 0x266FF0: these two camera gates exclude event 147.
    if (cameraZ + 3.0f < pedZ || cameraAverageSpeed > 0.9f) return false;
    if (event >= speaker.queuedSound) return false;
    if (event == speaker.lastQueuedSound) {
        const uint32_t nextRepeat = speaker.lastSoundStart + 3000u
            + storylandLcsScriptSpeechRandomDelay(std::forward<Random>(random));
        if (now < nextRepeat) return false;
    }
    speaker.queuedSound = event;
    return true;
}

template <typename Random>
inline StorylandLcsScriptSpeechResult storylandServiceLcsScriptSpeech(
    StorylandLcsScriptSpeechSpeaker& speaker,
    StorylandLcsScriptSpeechGlobal& shared,
    uint32_t now,
    bool headRemoved,
    Random&& random) {
    if (headRemoved || speaker.queuedSound != StorylandLcsScriptSpeechEvent
        || now <= speaker.soundStart) {
        return StorylandLcsScriptSpeechResult::Waiting;
    }
    // ServiceTalking, 0x266EB0: row 44 of the native 24-byte table
    // is {2000,1000,3000,1000,3000,0}. Its last word is shared state.
    if (now <= shared.event147ReadyAt) {
        speaker.queuedSound = StorylandLcsNoSpeechEvent;
        return StorylandLcsScriptSpeechResult::GlobalCooldown;
    }
    speaker.lastSoundStart = now;
    speaker.soundStart = now + 2000u
        + storylandLcsScriptSpeechRandomDelay(std::forward<Random>(random));
    shared.event147ReadyAt = now + 3000u;
    speaker.lastQueuedSound = speaker.queuedSound;
    speaker.queuedSound = StorylandLcsNoSpeechEvent;
    return StorylandLcsScriptSpeechResult::NativeSilent;
}
