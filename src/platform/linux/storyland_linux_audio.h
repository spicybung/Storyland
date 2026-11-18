#pragma once
#include "src/storyland/storyland_audio_mode.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "src/platform/linux/storyland_linux_radio.h"
#include "src/platform/linux/storyland_cutscene_audio_clock.h"
#include "src/storyland/storyland_character_audio.h"
#include "src/re/reVCS/audio/storyland_vcs_audio_random.h"

class StorylandLinuxSession;
class StorylandNativeAudioCatalog;
struct StorylandLcsPoliceRadioQueue;
struct StorylandLcsScriptSpeechHost;

class StorylandLinuxAudio {
public:
    StorylandLinuxAudio();
    ~StorylandLinuxAudio();
    void synchronize(StorylandLinuxSession& session);
    bool playFrontendSound(const StorylandLinuxSession& session,uint16_t event);
    void serviceNativeVcsAudio(StorylandLinuxSession& session);
    void advanceSilently(float seconds);
    uint32_t advance(uint32_t wallMilliseconds, bool paused = false);
    void stop();
    void releaseOutput();
    void setEnabled(bool value);
    void setMode(StorylandAudioMode value);
    StorylandAudioMode mode() const;
    bool playGeneratedVoice(const std::vector<int16_t>& samples, uint32_t sampleRate, std::string& error);
    void stopGeneratedVoice();
    bool generatedVoicePlaying() const;
    void cycleRadio(StorylandLinuxSession& session, int direction);
    bool isEnabled() const { return enabled; }
    bool isPlaying() const;
    bool radioIsPlaying() const;
    bool musicStreamIsPlaying(int32_t stream) const;
    uint64_t scriptMusicPositionMilliseconds() const;
    bool playMissionTune(const StorylandLinuxSession& session, int32_t stream, bool looping, std::string& error);
    void stopMissionTune();
    bool vehicleIsPlaying() const;
    bool vehicleHornIsPlaying() const;
    bool waterCannonSoundIsPlaying(size_t index) const;
    float waterCannonSoundGain(size_t index) const;
    float waterCannonSoundPitch(size_t index) const;
    bool policeHeliSoundIsPlaying(int32_t slot) const;
    float policeHeliSoundGain(int32_t slot) const;
    size_t continuousSoundCount() const;
    bool continuousSoundIsPlaying(int32_t handle) const;
    uint64_t continuousSoundPlaybackMilliseconds(int32_t handle) const;
    float continuousSoundGain(int32_t handle) const;
    bool characterSpeechIsPlaying(int speaker) const;
    int characterSpeechSample(int speaker) const;
    uint32_t characterSpeechStartCount(int speaker) const;
    void setVehicleHornHeld(bool value) { vehicleHornHeld = value; }
    int32_t currentRadioChannel(bool vcs = true) const { return radioChannel >= 0 ? radioChannel : (vcs ? 9 : 10); }
    uint64_t playbackMilliseconds() const;
    uint64_t radioPlaybackMilliseconds() const;
    uint32_t underrunCount() const;
    uint32_t maximumQueuedBytes() const;
    std::string status() const;
    const std::string& missionError() const { return missionDiagnostic; }

private:
    struct Stream;
    struct Output;
    struct VehicleVoice;
    struct ContinuousVoice;
    struct CharacterVoice;
    std::unique_ptr<Stream> voice;
    std::unique_ptr<Stream> radio;
    std::unique_ptr<Stream> scriptMusic;
    std::unique_ptr<Stream> missionTune;
    std::unique_ptr<Stream> frontendVoice;
    std::unique_ptr<Stream> policeRadio;
    std::unique_ptr<StorylandLcsPoliceRadioQueue> policeRadioQueue;
    std::unique_ptr<StorylandLcsScriptSpeechHost> lcsScriptSpeech;
    bool policeRadioSampleActive=false;
    std::array<std::unique_ptr<Stream>,8> missionStreams;
    std::array<std::unique_ptr<VehicleVoice>,3> vehicleVoices;
    std::array<std::unique_ptr<Stream>,4> effectStreams;
    std::map<int32_t, std::unique_ptr<ContinuousVoice>> continuousVoices;
    std::array<std::unique_ptr<ContinuousVoice>,3> waterCannonVoices;
    std::map<int32_t,std::unique_ptr<ContinuousVoice>> policeHeliVoices;
    std::map<int64_t,std::unique_ptr<CharacterVoice>> characterVoices;
    StorylandCharacterSpeechGlobal characterSpeechGlobal;
    StorylandVcsAudioRandom vcsAudioRandom;
    StorylandVcsAudioRandom lcsAudioRandom;
    uint8_t lcsDeathSoundVariation=0;
    StorylandCharacterSpeechCamera characterSpeechCamera;
    std::unique_ptr<Output> output;
    std::shared_ptr<StorylandNativeAudioCatalog> catalog;
    std::array<uint64_t,8> missionSerials{};
    std::filesystem::path catalogRoot;
    std::string missionDiagnostic;
    double silentSampleRemainder = 0;
    void synchronizeMission(StorylandLinuxSession& session);
    void synchronizeScriptMusic(StorylandLinuxSession& session);
    void synchronizeMissionTune(StorylandLinuxSession& session);
    void synchronizeLcsPoliceRadio(StorylandLinuxSession& session);
    void synchronizeVehicles(StorylandLinuxSession& session);
    void synchronizeEffects(StorylandLinuxSession& session);
    void synchronizeContinuousSounds(StorylandLinuxSession& session);
    void stopContinuousSounds();
    void stopPlayback(bool keepFrontend);
    void synchronizeWaterCannonSounds(StorylandLinuxSession& session);
    void stopWaterCannonSounds();
    void synchronizePoliceHeliSounds(StorylandLinuxSession& session);
    void stopPoliceHeliSounds();
    bool queueCharacterSpeech(StorylandLinuxSession& session,int speaker,uint16_t event);
    void synchronizeCharacterSpeech(StorylandLinuxSession& session);
    void stopCharacterSpeech();
    void loadCatalog(const StorylandLinuxSession& session);
    void synchronizeRadio(StorylandLinuxSession& session);
    void startVehicleRadio(StorylandLinuxSession& session, int selection);
    void stopRadio(uint32_t gameMilliseconds);
    void discoverRadio(const StorylandLinuxSession& session);
    std::filesystem::path findVoice(const StorylandLinuxSession& session) const;
    std::filesystem::path findStream(const StorylandLinuxSession& session,
        const std::vector<std::string>& names, bool missionClip = false) const;

    bool enabled = true;
    bool vehicleHornHeld = false;
    bool sceneWasPlaying = false;
    uint32_t loadSeen = UINT32_MAX;
    uint32_t startSeen = UINT32_MAX;
    uint32_t clearSeen = UINT32_MAX;
    uint32_t radioClearSeen = UINT32_MAX;
    uint32_t radioStartSeen = UINT32_MAX;
    int32_t radioChannel = -1;
    int32_t scriptMusicStream = -1;
    int32_t missionTuneStream = -1;
    uint32_t missionTuneSerial = UINT32_MAX;
    int radioVehicle = -1;
    bool radioScripted = false;
    StorylandRadioBroadcast radioBroadcast;
    std::filesystem::path radioRoot;
    std::filesystem::path policeRadioPath;
    std::vector<StorylandLinuxRadioStation> radioStations;
    StorylandCutsceneAudioClock cutsceneClock;
    std::filesystem::path currentPath;
};
