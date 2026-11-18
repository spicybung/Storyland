#pragma once
#include <cstdint>
#include <algorithm>
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
    float sx = 1.0f;
    float sy = 1.0f;
    float sz = 1.0f;
    uint32_t offset = 0;
};

struct StorylandAnimTrack {
    uint32_t index = 0;
    uint32_t clipIndex = 0;
    uint32_t channelIndex = 0;
    uint32_t offset = 0;
    uint32_t keyDataOffset = 0;
    uint32_t keyStride = 0;
    uint32_t channelFlags = 0;
    uint32_t boneId = 0xFFFFFFFFu;
    uint32_t boneIndex = 0xFFFFFFFFu;
    uint32_t parentIndex = 0xFFFFFFFFu;
    uint32_t tag = 0;
    bool hasNativeBoneTag = false;
    std::string name;
    std::string source;
    std::vector<StorylandAnimKey> keys;
};

inline std::vector<StorylandAnimKey>::const_iterator storylandNextAnimKey(
    const StorylandAnimTrack& track, float seconds) {
    const auto& keys = track.keys;
    if (track.source == "ANPK IFP (file-order keys)")
        return std::find_if(keys.begin(), keys.end(),
            [seconds](const StorylandAnimKey& key) { return key.time >= seconds; });
    return std::upper_bound(keys.begin(), keys.end(), seconds,
        [](float time, const StorylandAnimKey& key) { return time < key.time; });
}

struct StorylandAnimClip {
    uint32_t index = 0;
    uint32_t entryOffset = 0;
    uint32_t channelTableOffset = 0;
    uint32_t channelCount = 0;
    uint32_t flags = 0;
    float duration = 0.0f;
    std::string name;
    std::string bankName;
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
    bool loadFromDtz(const std::vector<uint8_t>& bytes, bool vcs, std::string& errorMessage);
    bool loadFromMemory(const std::vector<uint8_t>& bytes, const std::wstring& displayPath, std::string& errorMessage);
    bool loadNativeBank(const std::vector<uint8_t>& bytes, const std::wstring& displayPath,
        bool vcs, std::string& errorMessage);
    bool loadLcsCutsceneIfp(const std::vector<uint8_t>& bytes, const std::wstring& displayPath, std::string& errorMessage);

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
    bool appendDecodedBank(const StorylandAnimFile& bank,const std::string& bankName,
        const std::vector<std::string>& allowedNames);
    void removeDecodedBank(const std::string& bankName);
    bool isIfp() const;
    bool isManhuntIfp() const { return manhuntIfp; }

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

    bool ifpFormat = false;
    bool manhuntIfp = false;
    uint16_t nativeBoneIdFlag = 0;
    std::vector<std::vector<StorylandAnimTrack>> ifpClipTracks;
    bool parseManhuntIfp(std::string& errorMessage);
    bool parseIfp(std::string& errorMessage, bool allowLcsCutsceneKeys = false);
    bool loadFromMemoryImpl(const std::vector<uint8_t>& bytes, const std::wstring& displayPath,
        std::string& errorMessage, bool allowLcsCutsceneKeys, uint16_t boneIdFlag = 0);
    void parse();
    void collectStrings();
    void collectHeaderFields();
    void scanLeedsAnimationClips();
    void rebuildActiveClipTracks();
    void buildStaticInspectionTracks();
};
