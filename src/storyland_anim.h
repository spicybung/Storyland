#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct StorylandAnimField {
    std::string group;
    std::string name;
    uint32_t offset = 0;
    uint32_t value = 0;
    std::string note;
};

struct StorylandAnimKey {
    float time = 0.0f;
    float tx = 0.0f;
    float ty = 0.0f;
    float tz = 0.0f;
    float qx = 0.0f;
    float qy = 0.0f;
    float qz = 0.0f;
    float qw = 1.0f;
    uint32_t offset = 0;
};

struct StorylandAnimTrack {
    uint32_t index = 0;
    uint32_t clipIndex = 0;
    uint32_t channelIndex = 0;
    uint32_t offset = 0;
    uint32_t keyDataOffset = 0;
    uint32_t keyStride = 0;
    uint32_t boneId = 0xFFFFFFFFu;
    uint32_t boneIndex = 0xFFFFFFFFu;
    uint32_t parentIndex = 0xFFFFFFFFu;
    uint32_t tag = 0;
    std::string name;
    std::string source;
    std::vector<StorylandAnimKey> keys;
};

struct StorylandAnimClip {
    uint32_t index = 0;
    uint32_t entryOffset = 0;
    uint32_t channelTableOffset = 0;
    uint32_t channelCount = 0;
    uint32_t flags = 0;
    float duration = 0.0f;
    std::string name;
};

struct StorylandAnimPoseBone {
    uint32_t index = 0;
    uint32_t parentIndex = 0xFFFFFFFFu;
    uint32_t boneId = 0xFFFFFFFFu;
    std::string name;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool visible = true;
};

class StorylandAnimFile {
public:
    bool loadFromFile(const std::wstring& filePath, std::string& errorMessage);

    const std::wstring& sourcePath() const;
    const std::vector<uint8_t>& rawBytes() const;
    const std::vector<StorylandAnimField>& fields() const;
    const std::vector<StorylandAnimTrack>& tracks() const;
    const std::vector<StorylandAnimClip>& clips() const;
    const std::vector<std::string>& stringHints() const;

    float durationSeconds() const;
    float framesPerSecond() const;
    uint32_t frameCount() const;
    uint32_t activeClipIndex() const;
    bool setActiveClipIndex(uint32_t clipIndex);
    bool hasKeyframeCandidates() const;
    bool hasDecodedMotion() const;

    std::vector<StorylandAnimPoseBone> poseAt(float seconds) const;
    std::string summaryLine() const;

private:
    std::wstring path;
    std::vector<uint8_t> data;
    std::vector<StorylandAnimField> fieldRows;
    std::vector<StorylandAnimTrack> trackRows;
    std::vector<StorylandAnimClip> clipRows;
    std::vector<std::string> strings;
    float duration = 1.0f;
    float fps = 30.0f;
    uint32_t frames = 30;
    uint32_t activeClip = 0;
    bool hasCandidates = false;

    void parse();
    void collectStrings();
    void collectHeaderFields();
    void scanLeedsAnimationClips();
    void rebuildActiveClipTracks();
    void buildStaticInspectionTracks();
};
