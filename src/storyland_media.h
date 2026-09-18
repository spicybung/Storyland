#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct StorylandMediaClip {
    std::string name;
    uint64_t sourceOffset = 0;
    uint64_t sourceSize = 0;
    uint32_t sampleRate = 0;
    uint16_t channels = 1;
    double durationSeconds = 0.0;
    std::vector<int16_t> pcm;
};

struct StorylandVideoFrame {
    uint32_t width = 0;
    uint32_t height = 0;
    int64_t timestamp100ns = 0;
    std::vector<uint8_t> bgra;
};

enum class StorylandMediaKind {
    None,
    Audio,
    AudioArchive,
    Video
};

class StorylandMediaFile {
public:
    StorylandMediaFile();
    ~StorylandMediaFile();

    StorylandMediaFile(const StorylandMediaFile&) = delete;
    StorylandMediaFile& operator=(const StorylandMediaFile&) = delete;

    bool loadFromFile(const std::wstring& filePath, std::string& errorMessage);
    void close();

    StorylandMediaKind kind() const;
    const std::wstring& sourcePath() const;
    const std::vector<StorylandMediaClip>& clips() const;
    int selectedClip() const;
    bool selectClip(size_t index, std::string& errorMessage);

    bool play(std::string& errorMessage);
    void stop();
    bool isPlaying() const;

    bool tickVideo(std::string& errorMessage);
    const StorylandVideoFrame& videoFrame() const;
    uint32_t videoWidth() const;
    uint32_t videoHeight() const;
    double videoDurationSeconds() const;
    double videoPositionSeconds() const;
    bool videoDecoderReady() const;

    std::string summary() const;

private:
    bool loadSdt(const std::wstring& filePath, std::string& errorMessage);
    bool loadRaw(const std::wstring& filePath, std::string& errorMessage);
    bool loadWav(const std::wstring& filePath, std::string& errorMessage);
    bool loadVideo(const std::wstring& filePath, std::string& errorMessage);
    bool decodeVag(const std::vector<uint8_t>& bytes, size_t offset, size_t size,
                   uint32_t sampleRateHint, StorylandMediaClip& clip, std::string& errorMessage) const;
    bool startWaveOut(const StorylandMediaClip& clip, std::string& errorMessage);
    void stopWaveOut();
    void closeVideoDecoder();

    std::wstring path;
    StorylandMediaKind mediaKind = StorylandMediaKind::None;
    std::vector<StorylandMediaClip> audioClips;
    int selectedAudioClip = -1;
    bool audioPlaying = false;

    StorylandVideoFrame frame;
    uint32_t decodedVideoWidth = 0;
    uint32_t decodedVideoHeight = 0;
    int32_t decodedVideoStride = 0;
    double decodedVideoDuration = 0.0;
    double decodedVideoPosition = 0.0;
    double decodedVideoFrameRate = 30.0;
    uint64_t nextVideoDecodeTickMs = 0;
    bool videoPlaying = false;
    bool videoReady = false;
    std::wstring videoDecodePath;

#ifdef _WIN32
    void* waveOutHandle = nullptr;
    void* waveHeader = nullptr;
    std::vector<uint8_t> waveBytes;
    void* sourceReader = nullptr;
#endif
};
