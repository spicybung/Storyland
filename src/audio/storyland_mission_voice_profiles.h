#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

enum class StorylandMissionVoiceStatus {
    Ready,
    Dispatched,
    EmptyText,
    TextTooLong,
    UnsupportedText,
    ForbiddenWord,
    UnknownProfile,
    UnknownLine,
    ProfileMismatch,
    TextMismatch,
    UnknownEmotion,
    MissingBackend,
    BackendRejected,
    BackendFailure,
    UnsupportedLanguage
};

struct StorylandMissionVoiceCheck {
    StorylandMissionVoiceStatus status = StorylandMissionVoiceStatus::Ready;
    std::string message;
    std::string matchedWord;
    std::size_t byteOffset = 0;

    bool accepted() const noexcept {
        return status == StorylandMissionVoiceStatus::Ready ||
               status == StorylandMissionVoiceStatus::Dispatched;
    }
};

struct StorylandMissionVoiceProfile {
    std::string_view id;
    std::string_view displayName;
    std::string_view timbre;
    std::string_view accentDirection;
    std::string_view performanceDirection;
    float pitchOffsetSemitones;
    float speakingRateMultiplier;
    float energy;
    float roughness;
};

struct StorylandMissionVoiceEmotion {
    std::string_view id;
    float rateMultiplier;
    float pitchOffsetSemitones;
    float energyOffset;
};

struct StorylandMissionVoiceLine {
    std::string_view id;
    std::string_view profileId;
    std::string_view emotion;
    std::string_view text;
    std::string_view language = "en";
};

struct StorylandMissionVoiceRequest {
    std::string_view profileId;
    std::string_view lineId;
    std::string_view text;
    std::string_view emotion;
};

struct StorylandPreparedMissionVoice {
    const StorylandMissionVoiceProfile* profile = nullptr;
    const StorylandMissionVoiceLine* line = nullptr;
    const StorylandMissionVoiceEmotion* emotion = nullptr;
    std::string_view text;
    float pitchOffsetSemitones = 0.0f;
    float speakingRateMultiplier = 1.0f;
    float energy = 0.0f;
    float roughness = 0.0f;
};

using StorylandMissionVoiceBackend =
    std::function<bool(const StorylandPreparedMissionVoice&)>;

std::string_view storylandMissionVoicePolicyId() noexcept;
std::size_t storylandMissionVoiceMaximumTextBytes() noexcept;
const char* storylandMissionVoiceStatusName(StorylandMissionVoiceStatus status) noexcept;

std::size_t storylandMissionVoiceProfileCount() noexcept;
const StorylandMissionVoiceProfile* storylandMissionVoiceProfileAt(std::size_t index) noexcept;
const StorylandMissionVoiceProfile* storylandFindMissionVoiceProfile(std::string_view id) noexcept;

std::size_t storylandMissionVoiceEmotionCount() noexcept;
const StorylandMissionVoiceEmotion* storylandMissionVoiceEmotionAt(std::size_t index) noexcept;
const StorylandMissionVoiceEmotion* storylandFindMissionVoiceEmotion(std::string_view id) noexcept;

std::size_t storylandMissionVoiceLineCount() noexcept;
const StorylandMissionVoiceLine* storylandMissionVoiceLineAt(std::size_t index) noexcept;
const StorylandMissionVoiceLine* storylandFindMissionVoiceLine(std::string_view id) noexcept;

StorylandMissionVoiceCheck storylandInspectMissionVoiceText(std::string_view text);
StorylandMissionVoiceCheck storylandInspectMissionVoiceText(
    std::string_view text,
    std::string_view language);

StorylandMissionVoiceCheck storylandPrepareMissionVoice(
    const StorylandMissionVoiceRequest& request,
    StorylandPreparedMissionVoice& prepared);

StorylandMissionVoiceCheck storylandDispatchMissionVoice(
    const StorylandMissionVoiceRequest& request,
    const StorylandMissionVoiceBackend& backend);
