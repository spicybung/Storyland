#pragma once
#include "src/storyland/storyland_frontend_ambience.h"

#include <cstddef>
#include <atomic>
#include "src/storyland/storyland_manhunt_audio.h"
#include <cstdint>
#include <string>
#include <map>
#include <memory>

class StorylandRadioStream;
#include <vector>

struct StorylandMediaClip {
    uint32_t sourceIndex=UINT32_MAX;
    int64_t loopStartFrame=-1, loopEndFrame=-1;
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
    bool prepareVideoPlayback(const std::wstring& filePath, std::string& errorMessage,
        const std::shared_ptr<std::atomic<bool>>& cancelled);
    bool loadRadioStream(const std::wstring& filePath, std::string& errorMessage, double startSeconds=0);
    std::string radioPlaybackError() const;
    bool loadSdtClip(const std::wstring& filePath, size_t clipIndex, std::string& errorMessage,
        uint64_t maximumBytes=4u*1024u*1024u);
    bool loadAudioTrackFromVideo(const std::wstring& filePath, std::string& errorMessage);
    void close();

    StorylandMediaKind kind() const;
    const std::wstring& sourcePath() const;
    const std::vector<StorylandMediaClip>& clips() const;
    int selectedClip() const;
    bool selectClip(size_t index, std::string& errorMessage);

    bool play(std::string& errorMessage);
    void setPlaybackGain(float gain);
    void setFrontendAmbience(StorylandFrontendAmbience effect);
    void setPlaybackLooping(bool looping);
    void setPlaybackPitch(float pitch);
    void setPlaybackPaused(bool paused);
    void stop();
    bool isPlaying() const;
    bool playbackPositionMilliseconds(uint32_t& position) const;

    bool tickVideo(std::string& errorMessage);
    bool stepVideoFrame(int direction, std::string& errorMessage);
    bool seekVideoFrame(uint64_t frameIndex, std::string& errorMessage);
    uint64_t videoFrameCountEstimate() const;
    bool replaceCurrentVideoFrame(const StorylandVideoFrame& replacement, std::string& errorMessage);
    bool currentVideoFrameIsOverridden() const;
    const StorylandVideoFrame& videoFrame() const;
    uint32_t videoWidth() const;
    uint32_t videoHeight() const;
    double videoDurationSeconds() const;
    double videoPositionSeconds() const;
    double videoFrameRate() const;
    uint64_t videoFrameIndex() const;
    bool videoDecoderReady() const;

    std::string summary() const;
    bool isManhuntAudio() const { return manhuntAudioLoaded; }

private:
    bool loadManhuntAudio(const std::wstring& filePath, std::string& errorMessage);
    std::shared_ptr<StorylandRadioStream> radioStream;
    std::shared_ptr<StorylandMediaFile> videoAudio;
    std::shared_ptr<std::atomic<bool>> videoPreparationCancelled;
    std::string videoAudioError;
    std::wstring sdtSourcePath;
    std::vector<uint8_t> manhuntAudioBytes;
    std::vector<StorylandManhuntSound> manhuntSounds;
    bool manhuntAudioLoaded = false;
    bool loadSdt(const std::wstring& filePath, std::string& errorMessage);
    bool loadRaw(const std::wstring& filePath, std::string& errorMessage);
    bool loadVb(const std::wstring& filePath, std::string& errorMessage);
    bool loadWav(const std::wstring& filePath, std::string& errorMessage);
    bool loadCompressedAudio(const std::wstring& filePath, std::string& errorMessage);
    bool loadVideo(const std::wstring& filePath, std::string& errorMessage);
    bool decodeVag(const std::vector<uint8_t>& bytes, size_t offset, size_t size,
                   uint32_t sampleRateHint, StorylandMediaClip& clip, std::string& errorMessage) const;
    bool startPcmOutput(const StorylandMediaClip& clip, std::string& errorMessage);
    void stopPcmOutput();
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
    uint64_t videoPlaybackStartTickMs = 0;
    double videoPlaybackStartPosition = 0.0;
    double decodedVideoPresentationStart = -1.0;
    double videoAudioLeadSeconds = 0.0;
    bool videoWaitingForPresentation = false;
    bool videoPlaying = false;
    bool videoReady = false;
    std::map<uint64_t, StorylandVideoFrame> videoFrameOverrides;
    std::wstring videoDecodePath;

    float playbackGain=1.0f, playbackPitch=1.0f;
    StorylandFrontendAmbience frontendAmbience=StorylandFrontendAmbience::None;
    bool playbackLooping=false;
    bool playbackPaused=false;
#ifdef _WIN32
    void* pcmOutput = nullptr;
    std::vector<uint8_t> pcmBytes;
    void* sourceReader = nullptr;
    void* gameVideoDecoder = nullptr;
#endif
};
