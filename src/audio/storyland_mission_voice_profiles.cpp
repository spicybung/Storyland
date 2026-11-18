#include "src/audio/storyland_mission_voice_profiles.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <vector>
#include <utility>

namespace storylandMissionVoiceDetail {

#include "src/audio/storyland_mission_voice_data.inl"

struct Word {
    std::string lower;
    std::size_t begin;
    std::size_t end;
};

bool isLetter(unsigned char value) noexcept {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

bool isDigit(unsigned char value) noexcept {
    return value >= '0' && value <= '9';
}

char lowerLetter(unsigned char value) noexcept {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value + ('a' - 'A'));
    }
    return static_cast<char>(value);
}

std::string lowerText(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (char character : text) {
        const auto value = static_cast<unsigned char>(character);
        result.push_back(lowerLetter(value));
    }
    return result;
}

bool isAllowedWord(std::string_view word) noexcept {
    return word == "fuck" || word == "shit";
}

bool isBlockedWord(std::string_view word) noexcept {
    return std::binary_search(blockedWords.begin(), blockedWords.end(), word);
}

bool isAllowedCharacter(unsigned char value) noexcept {
    if (isLetter(value) || isDigit(value) || value == ' ') {
        return true;
    }
    constexpr std::string_view punctuation = ".,!?;:'\"()-";
    return value < 128 && punctuation.find(static_cast<char>(value)) != std::string_view::npos;
}

struct TextCharacter {
    std::uint32_t value;
    std::size_t begin;
    std::size_t end;
};

bool isRussianLetter(std::uint32_t value) noexcept {
    return (value >= 0x0410U && value <= 0x044FU) || value == 0x0401U || value == 0x0451U;
}

bool isLatinLetter(std::uint32_t value) noexcept {
    return value < 128U && isLetter(static_cast<unsigned char>(value));
}

bool isTextDigit(std::uint32_t value) noexcept {
    return value >= '0' && value <= '9';
}

bool decodeCharacter(std::string_view text, std::size_t begin, TextCharacter& character) noexcept {
    const auto first = static_cast<unsigned char>(text[begin]);
    std::size_t length = 0;
    std::uint32_t value = 0;
    std::uint32_t minimum = 0;
    if (first < 0x80U) {
        length = 1;
        value = first;
    } else if (first >= 0xC2U && first <= 0xDFU) {
        length = 2;
        value = first & 0x1FU;
        minimum = 0x80U;
    } else if (first >= 0xE0U && first <= 0xEFU) {
        length = 3;
        value = first & 0x0FU;
        minimum = 0x800U;
    } else if (first >= 0xF0U && first <= 0xF4U) {
        length = 4;
        value = first & 0x07U;
        minimum = 0x10000U;
    } else {
        return false;
    }
    if (text.size() - begin < length) {
        return false;
    }
    for (std::size_t index = 1; index < length; ++index) {
        const auto continuation = static_cast<unsigned char>(text[begin + index]);
        if ((continuation & 0xC0U) != 0x80U) {
            return false;
        }
        value = (value << 6U) | (continuation & 0x3FU);
    }
    if (value < minimum || value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU)) {
        return false;
    }
    character = {value, begin, begin + length};
    return true;
}

void appendLowerRussianCharacter(std::string& text, std::uint32_t value) {
    if (value < 128U) {
        text.push_back(lowerLetter(static_cast<unsigned char>(value)));
        return;
    }
    if (value == 0x0401U || value == 0x0451U) {
        value = 0x0435U;
    } else if (value >= 0x0410U && value <= 0x042FU) {
        value += 0x20U;
    }
    text.push_back(static_cast<char>(0xC0U | (value >> 6U)));
    text.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
}

bool isBlockedRussianWord(std::string_view word) noexcept {
    return std::binary_search(blockedRussianWords.begin(), blockedRussianWords.end(), word);
}

StorylandMissionVoiceCheck reject(
    StorylandMissionVoiceStatus status,
    std::string message,
    std::string matchedWord = {},
    std::size_t byteOffset = 0) {
    StorylandMissionVoiceCheck result;
    result.status = status;
    result.message = std::move(message);
    result.matchedWord = std::move(matchedWord);
    result.byteOffset = byteOffset;
    return result;
}

}

std::string_view storylandMissionVoicePolicyId() noexcept {
    return storylandMissionVoiceDetail::policyId;
}

std::size_t storylandMissionVoiceMaximumTextBytes() noexcept {
    return storylandMissionVoiceDetail::maximumTextBytes;
}

const char* storylandMissionVoiceStatusName(StorylandMissionVoiceStatus status) noexcept {
    switch (status) {
    case StorylandMissionVoiceStatus::Ready: return "ready";
    case StorylandMissionVoiceStatus::Dispatched: return "dispatched";
    case StorylandMissionVoiceStatus::EmptyText: return "empty_text";
    case StorylandMissionVoiceStatus::TextTooLong: return "text_too_long";
    case StorylandMissionVoiceStatus::UnsupportedText: return "unsupported_text";
    case StorylandMissionVoiceStatus::ForbiddenWord: return "forbidden_word";
    case StorylandMissionVoiceStatus::UnknownProfile: return "unknown_profile";
    case StorylandMissionVoiceStatus::UnknownLine: return "unknown_line";
    case StorylandMissionVoiceStatus::ProfileMismatch: return "profile_mismatch";
    case StorylandMissionVoiceStatus::TextMismatch: return "text_mismatch";
    case StorylandMissionVoiceStatus::UnknownEmotion: return "unknown_emotion";
    case StorylandMissionVoiceStatus::MissingBackend: return "missing_backend";
    case StorylandMissionVoiceStatus::BackendRejected: return "backend_rejected";
    case StorylandMissionVoiceStatus::BackendFailure: return "backend_failure";
    case StorylandMissionVoiceStatus::UnsupportedLanguage: return "unsupported_language";
    }
    return "unknown_status";
}

std::size_t storylandMissionVoiceProfileCount() noexcept {
    return storylandMissionVoiceDetail::profiles.size();
}

const StorylandMissionVoiceProfile* storylandMissionVoiceProfileAt(std::size_t index) noexcept {
    const auto& profiles = storylandMissionVoiceDetail::profiles;
    return index < profiles.size() ? &profiles[index] : nullptr;
}

const StorylandMissionVoiceProfile* storylandFindMissionVoiceProfile(std::string_view id) noexcept {
    for (const auto& profile : storylandMissionVoiceDetail::profiles) {
        if (profile.id == id) {
            return &profile;
        }
    }
    return nullptr;
}

std::size_t storylandMissionVoiceEmotionCount() noexcept {
    return storylandMissionVoiceDetail::emotions.size();
}

const StorylandMissionVoiceEmotion* storylandMissionVoiceEmotionAt(std::size_t index) noexcept {
    const auto& emotions = storylandMissionVoiceDetail::emotions;
    return index < emotions.size() ? &emotions[index] : nullptr;
}

const StorylandMissionVoiceEmotion* storylandFindMissionVoiceEmotion(std::string_view id) noexcept {
    for (const auto& emotion : storylandMissionVoiceDetail::emotions) {
        if (emotion.id == id) {
            return &emotion;
        }
    }
    return nullptr;
}

std::size_t storylandMissionVoiceLineCount() noexcept {
    return storylandMissionVoiceDetail::lines.size();
}

const StorylandMissionVoiceLine* storylandMissionVoiceLineAt(std::size_t index) noexcept {
    const auto& lines = storylandMissionVoiceDetail::lines;
    return index < lines.size() ? &lines[index] : nullptr;
}

const StorylandMissionVoiceLine* storylandFindMissionVoiceLine(std::string_view id) noexcept {
    for (const auto& line : storylandMissionVoiceDetail::lines) {
        if (line.id == id) {
            return &line;
        }
    }
    return nullptr;
}

StorylandMissionVoiceCheck storylandInspectMissionVoiceText(std::string_view text) {
    using namespace storylandMissionVoiceDetail;

    if (text.empty()) {
        return reject(StorylandMissionVoiceStatus::EmptyText, "Dialogue is empty.");
    }
    if (text.size() > maximumTextBytes) {
        return reject(StorylandMissionVoiceStatus::TextTooLong, "Dialogue exceeds the text limit.");
    }

    for (std::size_t index = 0; index < text.size(); ++index) {
        const auto value = static_cast<unsigned char>(text[index]);
        if (!isAllowedCharacter(value)) {
            return reject(
                StorylandMissionVoiceStatus::UnsupportedText,
                "Only plain English ASCII text and basic punctuation are accepted.",
                {}, index);
        }
        if ((value == '!' || value == '?') && index > 0 && index + 1 < text.size()) {
            const auto previous = static_cast<unsigned char>(text[index - 1]);
            const auto next = static_cast<unsigned char>(text[index + 1]);
            if ((isLetter(previous) || isDigit(previous)) && (isLetter(next) || isDigit(next))) {
                return reject(
                    StorylandMissionVoiceStatus::UnsupportedText,
                    "Punctuation cannot replace a letter inside a word.",
                    {}, index);
            }
        }
    }

    std::vector<Word> words;
    for (std::size_t index = 0; index < text.size();) {
        const auto value = static_cast<unsigned char>(text[index]);
        if (!isLetter(value) && !isDigit(value)) {
            ++index;
            continue;
        }

        const std::size_t begin = index;
        bool hasLetters = false;
        bool hasDigits = false;
        while (index < text.size()) {
            const auto current = static_cast<unsigned char>(text[index]);
            if (!isLetter(current) && !isDigit(current)) {
                break;
            }
            hasLetters = hasLetters || isLetter(current);
            hasDigits = hasDigits || isDigit(current);
            ++index;
        }

        const std::string lower = lowerText(text.substr(begin, index - begin));
        if (hasLetters && hasDigits) {
            return reject(
                StorylandMissionVoiceStatus::UnsupportedText,
                "Mixed letter-and-number words require explicit plain-text spelling.",
                lower, begin);
        }
        if (isBlockedWord(lower) && !isAllowedWord(lower)) {
            return reject(
                StorylandMissionVoiceStatus::ForbiddenWord,
                "This word is not permitted in synthesized dialogue.",
                lower, begin);
        }
        words.push_back({lower, begin, index});
    }

    if (words.empty()) {
        return reject(StorylandMissionVoiceStatus::EmptyText, "Dialogue contains no spoken words or numbers.");
    }

    for (std::size_t start = 0; start < words.size(); ++start) {
        std::string joined = words[start].lower;
        const std::size_t end = std::min(words.size(), start + std::size_t{12});
        for (std::size_t index = start + 1; index < end; ++index) {
            joined += words[index].lower;
            if (joined.size() > 40) {
                break;
            }
            const auto span = text.substr(words[start].begin, words[index].end - words[start].begin);
            if (joined == "hell" && lowerText(span) == "he'll") {
                continue;
            }
            if (isBlockedWord(joined)) {
                return reject(
                    StorylandMissionVoiceStatus::ForbiddenWord,
                    "Separated spelling of a forbidden word is not permitted.",
                    joined, words[start].begin);
            }
            if (isAllowedWord(joined)) {
                return reject(
                    StorylandMissionVoiceStatus::UnsupportedText,
                    "The two permitted swear words must use their exact whole-word spelling.",
                    joined, words[start].begin);
            }
        }
    }

    return {};
}

StorylandMissionVoiceCheck storylandInspectMissionVoiceText(
    std::string_view text,
    std::string_view language) {
    using namespace storylandMissionVoiceDetail;
    if (language == "en") {
        return storylandInspectMissionVoiceText(text);
    }
    if (language != "ru") {
        return reject(StorylandMissionVoiceStatus::UnsupportedLanguage, "Only English and Russian catalogue lines are supported.");
    }
    if (text.empty()) {
        return reject(StorylandMissionVoiceStatus::EmptyText, "Dialogue is empty.");
    }
    if (text.size() > maximumTextBytes) {
        return reject(StorylandMissionVoiceStatus::TextTooLong, "Dialogue exceeds the text limit.");
    }

    std::vector<TextCharacter> characters;
    for (std::size_t index = 0; index < text.size();) {
        TextCharacter character{};
        if (!decodeCharacter(text, index, character)) {
            return reject(StorylandMissionVoiceStatus::UnsupportedText, "Dialogue contains invalid UTF-8.", {}, index);
        }
        if (!isRussianLetter(character.value) &&
            !(character.value < 128U && isAllowedCharacter(static_cast<unsigned char>(character.value)))) {
            return reject(StorylandMissionVoiceStatus::UnsupportedText,
                "Russian dialogue accepts Russian letters, basic punctuation, spaces, and digits.", {}, index);
        }
        characters.push_back(character);
        index = character.end;
    }
    for (std::size_t index = 1; index + 1 < characters.size(); ++index) {
        const auto value = characters[index].value;
        if (value != '!' && value != '?') {
            continue;
        }
        const auto previous = characters[index - 1].value;
        const auto next = characters[index + 1].value;
        const bool previousWord = isRussianLetter(previous) || isLatinLetter(previous) || isTextDigit(previous);
        const bool nextWord = isRussianLetter(next) || isLatinLetter(next) || isTextDigit(next);
        if (previousWord && nextWord) {
            return reject(StorylandMissionVoiceStatus::UnsupportedText,
                "Punctuation cannot replace a letter inside a word.", {}, characters[index].begin);
        }
    }

    std::vector<Word> words;
    for (std::size_t index = 0; index < characters.size();) {
        const auto value = characters[index].value;
        if (!isRussianLetter(value) && !isLatinLetter(value) && !isTextDigit(value)) {
            ++index;
            continue;
        }
        const std::size_t begin = characters[index].begin;
        std::size_t end = begin;
        bool hasRussian = false;
        bool hasLatin = false;
        bool hasDigits = false;
        std::string lower;
        while (index < characters.size()) {
            const auto current = characters[index].value;
            if (!isRussianLetter(current) && !isLatinLetter(current) && !isTextDigit(current)) {
                break;
            }
            hasRussian = hasRussian || isRussianLetter(current);
            hasLatin = hasLatin || isLatinLetter(current);
            hasDigits = hasDigits || isTextDigit(current);
            appendLowerRussianCharacter(lower, current);
            end = characters[index].end;
            ++index;
        }
        if (hasRussian && hasLatin) {
            return reject(StorylandMissionVoiceStatus::UnsupportedText,
                "A word cannot mix Latin and Cyrillic letters.", lower, begin);
        }
        if ((hasRussian || hasLatin) && hasDigits) {
            return reject(StorylandMissionVoiceStatus::UnsupportedText,
                "Mixed letter-and-number words require explicit plain-text spelling.", lower, begin);
        }
        if ((isBlockedWord(lower) && !isAllowedWord(lower)) || isBlockedRussianWord(lower)) {
            return reject(StorylandMissionVoiceStatus::ForbiddenWord,
                "This word is not permitted in synthesized dialogue.", lower, begin);
        }
        if (hasLatin && !isAllowedWord(lower)) {
            return reject(StorylandMissionVoiceStatus::UnsupportedText,
                "Use Cyrillic spelling in Russian dialogue; only the two exact English profanity exceptions may use Latin letters.",
                lower, begin);
        }
        words.push_back({lower, begin, end});
    }
    if (words.empty()) {
        return reject(StorylandMissionVoiceStatus::EmptyText, "Dialogue contains no spoken words or numbers.");
    }
    for (std::size_t start = 0; start < words.size(); ++start) {
        std::string joined = words[start].lower;
        const std::size_t end = std::min(words.size(), start + std::size_t{12});
        for (std::size_t index = start + 1; index < end; ++index) {
            joined += words[index].lower;
            if (joined.size() > 80) {
                break;
            }
            if (isBlockedWord(joined) || isBlockedRussianWord(joined)) {
                return reject(StorylandMissionVoiceStatus::ForbiddenWord,
                    "Separated spelling of a forbidden word is not permitted.", joined, words[start].begin);
            }
            if (isAllowedWord(joined)) {
                return reject(StorylandMissionVoiceStatus::UnsupportedText,
                    "The two permitted swear words must use their exact whole-word spelling.", joined, words[start].begin);
            }
        }
    }
    return {};
}

StorylandMissionVoiceCheck storylandPrepareMissionVoice(
    const StorylandMissionVoiceRequest& request,
    StorylandPreparedMissionVoice& prepared) {
    using namespace storylandMissionVoiceDetail;
    prepared = {};

    const auto* profile = storylandFindMissionVoiceProfile(request.profileId);
    if (profile == nullptr) {
        return reject(StorylandMissionVoiceStatus::UnknownProfile, "Unknown character voice profile.");
    }
    const auto* line = storylandFindMissionVoiceLine(request.lineId);
    if (line == nullptr) {
        return reject(StorylandMissionVoiceStatus::UnknownLine, "The line is not in the compiled dialogue catalogue.");
    }
    if (line->profileId != profile->id) {
        return reject(StorylandMissionVoiceStatus::ProfileMismatch, "The line belongs to a different character.");
    }
    if (!request.text.empty() && request.text != line->text) {
        return reject(StorylandMissionVoiceStatus::TextMismatch, "Runtime text cannot replace catalogue dialogue.");
    }

    const std::string_view emotionId = request.emotion.empty() ? line->emotion : request.emotion;
    const auto* emotion = storylandFindMissionVoiceEmotion(emotionId);
    if (emotion == nullptr) {
        return reject(StorylandMissionVoiceStatus::UnknownEmotion, "Unknown voice emotion.");
    }

    const auto check = storylandInspectMissionVoiceText(line->text, line->language);
    if (!check.accepted()) {
        return check;
    }

    prepared.profile = profile;
    prepared.line = line;
    prepared.emotion = emotion;
    prepared.text = line->text;
    prepared.pitchOffsetSemitones = std::clamp(
        profile->pitchOffsetSemitones + emotion->pitchOffsetSemitones, -4.0f, 4.0f);
    prepared.speakingRateMultiplier = std::clamp(
        profile->speakingRateMultiplier * emotion->rateMultiplier, 0.75f, 1.30f);
    prepared.energy = std::clamp(profile->energy + emotion->energyOffset, 0.05f, 1.0f);
    prepared.roughness = std::clamp(profile->roughness, 0.0f, 1.0f);
    return {};
}

StorylandMissionVoiceCheck storylandDispatchMissionVoice(
    const StorylandMissionVoiceRequest& request,
    const StorylandMissionVoiceBackend& backend) {
    using namespace storylandMissionVoiceDetail;
    StorylandPreparedMissionVoice prepared;
    const auto check = storylandPrepareMissionVoice(request, prepared);
    if (!check.accepted()) {
        return check;
    }
    if (!backend) {
        return reject(StorylandMissionVoiceStatus::MissingBackend, "No speech synthesis backend is connected.");
    }

    try {
        if (!backend(prepared)) {
            return reject(StorylandMissionVoiceStatus::BackendRejected, "The speech synthesis backend rejected the request.");
        }
    } catch (const std::exception&) {
        return reject(StorylandMissionVoiceStatus::BackendFailure, "The speech synthesis backend failed.");
    } catch (...) {
        return reject(StorylandMissionVoiceStatus::BackendFailure, "The speech synthesis backend failed unexpectedly.");
    }

    StorylandMissionVoiceCheck result;
    result.status = StorylandMissionVoiceStatus::Dispatched;
    return result;
}
