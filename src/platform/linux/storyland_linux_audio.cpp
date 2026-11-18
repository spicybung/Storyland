#include "src/platform/linux/storyland_linux_audio.h"
#include "src/storyland/storyland_frontend_ambience.h"
#include "src/platform/linux/storyland_linux_session.h"
#include "src/storyland/storyland_radio_vb.h"
#include "src/storyland/storyland_radio_stations.h"
#include "src/storyland/storyland_native_audio.h"
#include "src/re/reVCS/audio/storyland_vcs_water_cannon_audio.h"
#include "src/re/reVCS/audio/storyland_vcs_police_heli_audio.h"
#include "src/storyland/storyland_character_speech_host.h"
#include "src/re/reLCS2/audio/storyland_lcs_police_radio.h"
#include "src/re/reLCS2/audio/storyland_lcs_script_speech_host.h"
#include "src/storyland/storyland_script_sound.h"
#include "src/storyland/storyland_ped_contact_audio.h"
#include "src/storyland/storyland_vehicle_door_audio.h"
#include "src/storyland/storyland_vehicle_radio.h"
#include "src/storyland/storyland_media.h"
#include "src/storyland/storyland_sdt.h"
#include "src/storyland/storyland_vag_bank.h"
#ifdef STORYLAND_HAS_COMPRESSED_AUDIO
#include "src/storyland/storyland_compressed_audio.h"
#endif
#include <SDL.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <condition_variable>
#include <fstream>
#include <set>
#include <mutex>
#include <thread>

namespace {
StorylandNativePoint audioListener(const StorylandLinuxSession& session) {
    if(session.vcs()) {
        if(session.camera.initialized)return session.camera.eye;
        if(session.environment.camera.active&&session.environment.camera.positionSet)
            return {session.environment.camera.x,session.environment.camera.y,session.environment.camera.z};
    }
    return session.playerPosition;
}

float audioDistance(const StorylandLinuxSession& session,const StorylandNativePoint& point) {
    const auto listener=audioListener(session);
    const float x=point.x-listener.x,y=point.y-listener.y,z=point.z-listener.z;
    return session.vcs()?storylandVcsAudioDistance(x,y,z):std::hypot(x,y,z);
}

std::string audioName(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return char(std::tolower(character));
    });
    return value;
}
std::string audioStreamKey(const std::string& value) {
    std::string key;
    for (unsigned char character : value)
        if (std::isalnum(character)) key.push_back(char(std::tolower(character)));
    return key;
}
SDL_AudioStream* createNativePcmConverter(SDL_AudioFormat sourceFormat,
    Uint8 sourceChannels,int sourceFrequency) {
    // SDL2 initializes shared conversion tables during the first stream creation.
    static std::mutex constructionMutex;
    std::lock_guard<std::mutex> lock(constructionMutex);
    return SDL_NewAudioStream(sourceFormat,sourceChannels,sourceFrequency,
        AUDIO_S16SYS,2,32000);
}
}

// The decoder workers feed bounded stereo PCM queues. One SDL device mixes
// radio, native cutscene voice and the eight real SCM mission audio slots.
struct StorylandLinuxAudio::Stream {
    static constexpr size_t queueCapacity = 16384;
    static constexpr size_t queueTarget = 8192;
    std::array<int16_t, queueCapacity> queue{};
    std::atomic<uint64_t> readPosition{0}, writePosition{0}, consumedSamples{0};
    std::thread worker;
    SDL_AudioDeviceID device = 0;
    std::atomic<bool> cancelled{false}, requested{false}, paused{false};
    std::atomic<bool> loaded{false}, started{false}, finished{false}, failed{false}, exhausted{false};
    std::atomic<uint32_t> durationMilliseconds{0};
    std::atomic<float> gain{1}, pitch{1};
    double samplePhase = 0;
    std::atomic<uint64_t> milliseconds{0}, baseMilliseconds{0};
    std::atomic<uint32_t> underruns{0}, peakQueuedBytes{0};
    std::condition_variable wake;
    std::mutex waitMutex;
    mutable std::mutex statusMutex;
    std::string diagnostic;

    ~Stream() { stop(); }
    bool playing() const {
        return started && requested && !paused && !cancelled && !finished && !failed;
    }
    void status(std::string text) {
        std::lock_guard<std::mutex> lock(statusMutex);
        diagnostic = std::move(text);
    }
    std::string status() const {
        std::lock_guard<std::mutex> lock(statusMutex);
        return diagnostic;
    }
    void stop() {
        cancelled = true;
        requested = false;
        wake.notify_all();
        if (worker.joinable()) worker.join();
    }
    void controls(bool play, bool pause, float volume) {
        requested = play;
        paused = pause;
        gain = std::clamp(volume, 0.0f, 1.0f);
        wake.notify_all();
    }
    void start(const std::filesystem::path& path, double offset = 0, bool loop = false, size_t bankSlot = 0,
        StorylandFrontendAmbience ambience=StorylandFrontendAmbience::None) {
        stop();
        // A callback already in progress must finish before queue cursors reset.
        if (device) SDL_LockAudioDevice(device);
        cancelled = false;
        loaded = false;
        started = false;
        finished = false;
        failed = false;
        exhausted = false;
        milliseconds = baseMilliseconds = consumedSamples = 0;
        readPosition = writePosition = 0;
        samplePhase = 0;
        pitch = 1;
        underruns = 0;
        peakQueuedBytes = 0;
        durationMilliseconds = 0;
        if (device) SDL_UnlockAudioDevice(device);
        status("Loading " + path.filename().string());
        try {
            worker = std::thread(&Stream::decode, this, path, offset, loop, bankSlot, ambience);
        } catch (const std::exception& exception) {
            failed = true;
            status("Could not start audio worker: " + std::string(exception.what()));
        }
    }
    void decode(std::filesystem::path path, double offset, bool loop, size_t bankSlot,
        StorylandFrontendAmbience ambience) {
        const auto fail = [&](const std::string& error) { failed = true; status(error); };
        try {
            StorylandRadioVb decoder;
            StorylandMediaClip clip;
            std::unique_ptr<SDL_AudioStream, decltype(&SDL_FreeAudioStream)> converter(nullptr, SDL_FreeAudioStream);
            std::unique_ptr<Uint8, decltype(&SDL_FreeWAV)> wave(nullptr, SDL_FreeWAV);
            const Uint8* source = nullptr;
            size_t sourceBytes = 0, sourcePosition = 0, sourceFrameBytes = 0;
            uint64_t convertedSamples = 0, targetSamples = 0;
            uint32_t sourceRate = 0;
            std::vector<Uint8> padding;
            bool padded = false;
            bool flushed = false;
            const auto extension = audioName(path.extension().string());
            const bool vb = extension == ".vb";
            std::string error;
            double duration = 0;
            if (vb) {
                if (!decoder.open(path, error)) { fail(error); return; }
                duration = decoder.duration();
            } else {
                SDL_AudioSpec sourceFormat{};
                if (extension == ".sdt") {
                    if (!storylandLoadSdtPcm(path, bankSlot, clip, error)) { fail(error); return; }
                } else if (extension == ".raw") {
                    if (!storylandLoadVagBankPcm(path, bankSlot, clip, error)) { fail(error); return; }
                } else if (extension == ".vag") {
                    std::ifstream input(path, std::ios::binary | std::ios::ate);
                    const auto length = input ? input.tellg() : std::streampos(-1);
                    if (length <= 0 || uint64_t(length) > 4u * 1024u * 1024u) {
                        fail("Mission VAG is empty, unreadable or exceeds the native playback limit."); return;
                    }
                    std::vector<uint8_t> bytes(static_cast<size_t>(length));
                    input.seekg(0);
                    if (!input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size())) ||
                        !storylandDecodeVagPcm(bytes, 0, bytes.size(), 0, clip, error)) {
                        fail(error.empty() ? "Could not read mission VAG." : error); return;
                    }
#ifdef STORYLAND_HAS_COMPRESSED_AUDIO
                } else if (extension == ".at3" || extension == ".aa3" || extension == ".oma" ||
                    extension == ".mp3" || extension == ".ogg" || extension == ".flac") {
                    if (!storylandDecodeCompressedAudio(path, clip, error,
                        [this] { return cancelled.load(); })) { if (!cancelled) fail(error); return; }
#endif
                } else if (extension == ".wav") {
                    std::error_code fileError;
                    const uint64_t length = std::filesystem::file_size(path, fileError);
                    if (fileError || !length || length > 16u * 1024u * 1024u) {
                        fail("Mission WAV is empty, unreadable or exceeds the playback limit."); return;
                    }
                    Uint8* bytes = nullptr;
                    Uint32 lengthBytes = 0;
                    if (!SDL_LoadWAV(path.string().c_str(), &sourceFormat, &bytes, &lengthBytes)) {
                        fail("Could not decode mission WAV: " + std::string(SDL_GetError())); return;
                    }
                    wave.reset(bytes);
                    source = bytes;
                    sourceBytes = lengthBytes;
                } else {
                    fail("Unsupported native mission audio format: " + extension); return;
                }
                if (!wave) {
                    sourceFormat.format = AUDIO_S16SYS;
                    sourceFormat.channels = Uint8(clip.channels);
                    sourceFormat.freq = int(clip.sampleRate);
                    source = reinterpret_cast<const Uint8*>(clip.pcm.data());
                    sourceBytes = clip.pcm.size() * sizeof(int16_t);
                }
                sourceFrameBytes = size_t(SDL_AUDIO_BITSIZE(sourceFormat.format) / 8) * sourceFormat.channels;
                if (!sourceFrameBytes || sourceFormat.freq <= 0 || !sourceBytes || sourceBytes % sourceFrameBytes) {
                    fail("Native mission clip has invalid PCM metadata."); return;
                }
                duration = double(sourceBytes / sourceFrameBytes) / sourceFormat.freq;
                sourceRate = uint32_t(sourceFormat.freq);
                padding.assign(sourceFrameBytes * 1024, sourceFormat.silence);
                converter.reset(createNativePcmConverter(sourceFormat.format,
                    sourceFormat.channels,sourceFormat.freq));
                if (!converter) { fail("Could not convert native PCM: " + std::string(SDL_GetError())); return; }
            }
            if (!(duration > 0) || !std::isfinite(duration)) { fail("Native audio has no playable samples."); return; }
            durationMilliseconds = uint32_t(std::min(std::ceil(duration * 1000), double(UINT32_MAX)));
            std::vector<int16_t> samples;
            size_t samplePosition = 0;
            bool eof = false;
            bool announced = false;
            bool ambienceTailSent = false;
            StorylandFrontendEcho echo(32000,2,ambience);
            const double startSeconds = std::fmod(std::max(offset, 0.0), duration);
            baseMilliseconds = uint64_t(startSeconds * 1000.0);
            milliseconds = baseMilliseconds.load();
            if (vb && offset > 0 && !decoder.seek(offset, samples, error,
                [this] { return cancelled.load(); })) { if (!cancelled) fail(error); return; }
            if (!vb) {
                sourcePosition = std::min(sourceBytes,
                    size_t(startSeconds / duration * double(sourceBytes / sourceFrameBytes)) * sourceFrameBytes);
                targetSamples = ((uint64_t((sourceBytes - sourcePosition) / sourceFrameBytes) * 32000u +
                    sourceRate / 2u) / sourceRate) * 2u;
            }
            const auto readSamples = [&]() {
                if (vb) {
                    if (!decoder.read(samples, eof, error)) return false;
                    if (eof && loop) { eof = false; return decoder.seek(0, samples, error); }
                    return true;
                }
                if (convertedSamples == targetSamples) {
                    if (!loop) { samples.clear(); eof = true; return true; }
                    SDL_AudioStreamClear(converter.get());
                    sourcePosition = 0;
                    convertedSamples = 0;
                    padded = flushed = false;
                    targetSamples = ((uint64_t(sourceBytes / sourceFrameBytes) * 32000u +
                        sourceRate / 2u) / sourceRate) * 2u;
                }
                // Feed only a small native chunk at a time. Converted PCM never
                // grows with clip length and remains owned by this refill worker.
                while (SDL_AudioStreamAvailable(converter.get()) == 0) {
                    if (sourcePosition < sourceBytes) {
                        const size_t bytes = std::min(sourceBytes - sourcePosition, sourceFrameBytes * 1024);
                        if (SDL_AudioStreamPut(converter.get(), source + sourcePosition, int(bytes)) < 0) {
                            error = SDL_GetError(); return false;
                        }
                        sourcePosition += bytes;
                    } else if (!padded) {
                        // Drain the resampler's final filter window, then keep
                        // exactly the rational source duration. SDL2 otherwise
                        // drops part of a short last input chunk at flush.
                        if (SDL_AudioStreamPut(converter.get(), padding.data(), int(padding.size())) < 0) {
                            error = SDL_GetError(); return false;
                        }
                        padded = true;
                    } else if (!flushed) {
                        if (SDL_AudioStreamFlush(converter.get()) < 0) { error = SDL_GetError(); return false; }
                        flushed = true;
                    } else { error = "Native PCM conversion ended before the source samples were drained."; return false; }
                }
                samples.resize(size_t(std::min<uint64_t>(2048, targetSamples - convertedSamples)));
                const int bytes = SDL_AudioStreamGet(converter.get(), samples.data(), int(samples.size() * sizeof(int16_t)));
                if (bytes < 0) { error = SDL_GetError(); return false; }
                samples.resize(size_t(bytes) / sizeof(int16_t));
                convertedSamples += samples.size();
                return true;
            };
            while (!cancelled && !failed) {
                uint64_t queued = writePosition.load() - readPosition.load(std::memory_order_acquire);
                while (!cancelled && !eof && queued < queueTarget) {
                    if (samplePosition == samples.size()) {
                        if (!readSamples()) { fail(error); break; }
                        samplePosition = 0;
                        if(eof&&ambience==StorylandFrontendAmbience::Menu&&!ambienceTailSent&&!loop) {
                            samples.assign(StorylandFrontendEcho::menuTailFrames(32000)*2,0);
                            ambienceTailSent=true;eof=false;
                        }
                        if (eof) { exhausted = true; break; }
                        echo.process(samples.data(),samples.size());
                    }
                    const size_t count = std::min({size_t(2048), samples.size() - samplePosition,
                        queueTarget - size_t(queued)});
                    const uint64_t write = writePosition.load(std::memory_order_relaxed);
                    for (size_t sample = 0; sample < count; ++sample)
                        queue[size_t(write + sample) % queueCapacity] = samples[samplePosition + sample];
                    samplePosition += count;
                    writePosition.store(write + count, std::memory_order_release);
                    queued = write + count - readPosition.load(std::memory_order_acquire);
                    const uint32_t bytes = uint32_t(queued * sizeof(int16_t));
                    if (bytes > peakQueuedBytes) peakQueuedBytes = bytes;
                    if (count) loaded = true;
                }
                if (started && !announced) {
                    status("Playing " + path.filename().string());
                    announced = true;
                }
                if (finished) { status("Finished " + path.filename().string()); break; }
                std::unique_lock<std::mutex> lock(waitMutex);
                wake.wait_for(lock, std::chrono::milliseconds(10));
            }
        } catch (const std::exception& exception) {
            fail("Native audio failed: " + std::string(exception.what()));
        }
    }
    // Single consumer: SDL's output thread, or the clocked silent consumer while
    // that device is paused. No allocation, I/O or waits occur in the callback.
    void mix(int16_t* destination, size_t count) {
        if (cancelled || !requested || paused || failed || finished) return;
        const uint64_t read = readPosition.load(std::memory_order_relaxed);
        const uint64_t write = writePosition.load(std::memory_order_acquire);
        const size_t available = size_t(write - read) / 2;
        const size_t requestedFrames = count / 2;
        const double rate = std::clamp(double(pitch.load()), 0.25, 4.0);
        const float volume = gain.load();
        double position = samplePhase;
        size_t mixedFrames = 0;
        while (mixedFrames < requestedFrames && size_t(position) < available) {
            const size_t frame = size_t(position);
            const size_t next = std::min(frame + 1, available - 1);
            const float blend = float(position - double(frame));
            for (size_t channel = 0; channel < 2; ++channel) {
                const float first = queue[size_t(read + frame * 2 + channel) % queueCapacity];
                const float second = queue[size_t(read + next * 2 + channel) % queueCapacity];
                const int value = destination[mixedFrames * 2 + channel] +
                    int(std::lround((first + (second - first) * blend) * volume));
                destination[mixedFrames * 2 + channel] = int16_t(std::clamp(value, -32768, 32767));
            }
            ++mixedFrames;
            position += rate;
        }
        const size_t consumedFrames = std::min(size_t(position), available);
        const size_t consumed = consumedFrames * 2;
        samplePhase = consumedFrames == available ? 0 : position - double(consumedFrames);
        if (consumed) {
            readPosition.store(read + consumed, std::memory_order_release);
            consumedSamples += consumed;
            milliseconds = baseMilliseconds.load() + consumedSamples.load() * 1000u / (32000u * 2u);
            started = true;
        }
        if (mixedFrames < requestedFrames && started && !exhausted) ++underruns;
        if (exhausted && read + consumed == write && started) finished = true;
    }
};

struct StorylandLinuxAudio::Output {
    static constexpr size_t fixedStreamCount = 36;
    static constexpr size_t continuousStreamCount = 250;
    std::array<Stream*, fixedStreamCount + continuousStreamCount> streams{};
    SDL_AudioDeviceID device = 0;
    std::atomic<StorylandAudioMode> selectedMode{StorylandAudioMode::Ps2};
    StorylandAudioProcessor audioProcessor{32000};
    std::vector<int16_t> generatedSamples;
    uint32_t generatedRate = 0;
    double generatedPosition = 0;
    std::atomic_bool generatedActive{false};
    bool attempted = false;
    std::string error;
    Output(Stream& voiceStream, Stream& radioStream) {
        streams[0] = &radioStream;
        streams[1] = &voiceStream;
    }
    ~Output() { close(); }
    void close() {
        if (device) SDL_CloseAudioDevice(device);
        device = 0;
        for (auto* stream : streams) if (stream) stream->device = 0;
        attempted = false;
        error.clear();
        generatedActive.store(false, std::memory_order_release);
        generatedSamples.clear();
        generatedRate = 0;
        generatedPosition = 0;
        audioProcessor = StorylandAudioProcessor{32000};
    }
    static void callback(void* context, Uint8* bytes, int length) {
        auto& output = *static_cast<Output*>(context);
        std::fill(bytes, bytes + length, Uint8(0));
        const size_t samples = size_t(length) / sizeof(int16_t);
        auto* destination = reinterpret_cast<int16_t*>(bytes);
        for (auto* stream : output.streams) if (stream) stream->mix(destination, samples);
        output.mixGeneratedVoice(destination, samples / 2);
        output.audioProcessor.process(destination,samples/2,
            output.selectedMode.load(std::memory_order_relaxed));
    }
    void mixGeneratedVoice(int16_t* destination, size_t frames) {
        if (!generatedActive.load(std::memory_order_relaxed) || generatedSamples.empty()) return;
        const double step = double(generatedRate) / 32000.0;
        for (size_t frame = 0; frame < frames; ++frame) {
            const size_t position = size_t(generatedPosition);
            if (position >= generatedSamples.size()) {
                generatedActive.store(false, std::memory_order_release);
                break;
            }
            const size_t next = std::min(position + 1, generatedSamples.size() - 1);
            const double fraction = generatedPosition - double(position);
            const double sample = double(generatedSamples[position]) +
                (double(generatedSamples[next]) - double(generatedSamples[position])) * fraction;
            const int value = int(std::lround(sample));
            for (size_t channel = 0; channel < 2; ++channel) {
                const int mixed = int(destination[frame * 2 + channel]) + value;
                destination[frame * 2 + channel] = int16_t(std::clamp(mixed, -32768, 32767));
            }
            generatedPosition += step;
        }
    }
    size_t attachContinuous(Stream& stream) {
        if (device) SDL_LockAudioDevice(device);
        size_t slot = fixedStreamCount;
        while (slot < streams.size() && streams[slot]) ++slot;
        if (slot < streams.size()) {
            stream.device = device;
            streams[slot] = &stream;
        }
        if (device) SDL_UnlockAudioDevice(device);
        return slot;
    }
    void detachContinuous(size_t slot) {
        if (slot < fixedStreamCount || slot >= streams.size()) return;
        if (device) SDL_LockAudioDevice(device);
        if (streams[slot]) streams[slot]->device = 0;
        streams[slot] = nullptr;
        if (device) SDL_UnlockAudioDevice(device);
    }
    bool open() {
        if (attempted) return device != 0;
        attempted = true;
        if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            error = "Could not initialize SDL audio output: " + std::string(SDL_GetError());
            return false;
        }
        SDL_AudioSpec format{};
        format.freq = 32000;
        format.format = AUDIO_S16SYS;
        format.channels = 2;
        format.samples = 1024;
        format.callback = &Output::callback;
        format.userdata = this;
        device = SDL_OpenAudioDevice(nullptr, 0, &format, nullptr, 0);
        if (!device) {
            error = "Could not open SDL audio output: " + std::string(SDL_GetError());
            return false;
        }
        for (auto* stream : streams) if (stream) stream->device = device;
        SDL_PauseAudioDevice(device, 0);
        return true;
    }
};

struct StorylandLinuxAudio::VehicleVoice {
    std::array<std::unique_ptr<Stream>,6> layers;
    int vehicle = -1;
    int32_t scriptHandle = -1;
    size_t model = SIZE_MAX;
    bool playerVehicle = false;
    uint32_t accelerationGear = UINT32_MAX;
    float retryAt = 0;
    StorylandVcsBoboAudioState bobo;

    VehicleVoice() {
        for (auto& layer : layers) layer = std::make_unique<Stream>();
    }
    void stop() {
        for (auto& layer : layers) layer->stop();
        vehicle = -1;
        scriptHandle = -1;
        accelerationGear = UINT32_MAX;
        bobo={};
    }
};

struct StorylandLinuxAudio::ContinuousVoice {
    Stream stream;
    uint16_t id = 0;
    size_t outputSlot = SIZE_MAX;
    std::wstring bankPath;
    size_t sampleSlot = 0;
    int32_t vehicleHandle = -1;
};

struct StorylandLinuxAudio::CharacterVoice {
    Stream stream;
    StorylandCharacterSpeechState state;
    size_t outputSlot=0;
    int speaker=-1,sample=-1;
    uint32_t starts=0;
};

StorylandLinuxAudio::StorylandLinuxAudio()
    : voice(std::make_unique<Stream>()), radio(std::make_unique<Stream>()),
      scriptMusic(std::make_unique<Stream>()), missionTune(std::make_unique<Stream>()),
      frontendVoice(std::make_unique<Stream>()),
      policeRadio(std::make_unique<Stream>()),policeRadioQueue(std::make_unique<StorylandLcsPoliceRadioQueue>()),
      lcsScriptSpeech(std::make_unique<StorylandLcsScriptSpeechHost>()),
      output(std::make_unique<Output>(*voice, *radio)), catalog(std::make_shared<StorylandNativeAudioCatalog>()) {
    for (size_t index = 0; index < missionStreams.size(); ++index) {
        missionStreams[index] = std::make_unique<Stream>();
        output->streams[index + 2] = missionStreams[index].get();
    }
    size_t outputSlot = 2 + missionStreams.size();
    for (auto& voice : vehicleVoices) {
        voice = std::make_unique<VehicleVoice>();
        for (auto& layer : voice->layers) output->streams[outputSlot++] = layer.get();
    }
    for (auto& stream : effectStreams) {
        stream = std::make_unique<Stream>();
        output->streams[outputSlot++] = stream.get();
    }
    output->streams[outputSlot++] = scriptMusic.get();
    output->streams[outputSlot++] = missionTune.get();
    output->streams[outputSlot++] = policeRadio.get();
    output->streams[outputSlot++] = frontendVoice.get();
    missionSerials.fill(UINT64_MAX);
}
StorylandLinuxAudio::~StorylandLinuxAudio() {
    stop();
    output.reset();
}

bool StorylandLinuxAudio::isPlaying() const { return enabled && voice->playing(); }
bool StorylandLinuxAudio::radioIsPlaying() const { return enabled && radio->playing(); }
bool StorylandLinuxAudio::musicStreamIsPlaying(int32_t stream) const {
    if (missionTuneStream == stream && missionTune->playing()) return true;
    if (scriptMusicStream == stream && scriptMusic->playing()) return true;
    return radioChannel == stream && radio->playing();
}
uint64_t StorylandLinuxAudio::scriptMusicPositionMilliseconds() const {
    return scriptMusicStream>=0?scriptMusic->milliseconds.load():0;
}
bool StorylandLinuxAudio::vehicleIsPlaying() const {
    if (!enabled) return false;
    for (const auto& voice : vehicleVoices)
        for (const auto& layer : voice->layers) if (layer->playing()) return true;
    return false;
}
bool StorylandLinuxAudio::vehicleHornIsPlaying() const {
    if (!enabled) return false;
    for (const auto& voice : vehicleVoices) if (voice->layers[5]->playing()) return true;
    return false;
}
bool StorylandLinuxAudio::waterCannonSoundIsPlaying(size_t index) const {
    return enabled&&index<waterCannonVoices.size()&&waterCannonVoices[index]&&
        waterCannonVoices[index]->stream.playing();
}
float StorylandLinuxAudio::waterCannonSoundGain(size_t index) const {
    return index<waterCannonVoices.size()&&waterCannonVoices[index]?
        waterCannonVoices[index]->stream.gain.load():0.0f;
}
float StorylandLinuxAudio::waterCannonSoundPitch(size_t index) const {
    return index<waterCannonVoices.size()&&waterCannonVoices[index]?
        waterCannonVoices[index]->stream.pitch.load():0.0f;
}
bool StorylandLinuxAudio::policeHeliSoundIsPlaying(int32_t slot) const {
    const auto voice=policeHeliVoices.find(slot);
    return enabled&&voice!=policeHeliVoices.end()&&voice->second->stream.playing();
}
float StorylandLinuxAudio::policeHeliSoundGain(int32_t slot) const {
    const auto voice=policeHeliVoices.find(slot);
    return voice==policeHeliVoices.end()?0.0f:voice->second->stream.gain.load();
}
size_t StorylandLinuxAudio::continuousSoundCount() const { return continuousVoices.size(); }
bool StorylandLinuxAudio::continuousSoundIsPlaying(int32_t handle) const {
    const auto voice = continuousVoices.find(handle);
    return enabled && voice != continuousVoices.end() && voice->second->stream.playing();
}
uint64_t StorylandLinuxAudio::continuousSoundPlaybackMilliseconds(int32_t handle) const {
    const auto voice = continuousVoices.find(handle);
    return voice != continuousVoices.end() ? voice->second->stream.milliseconds.load() : 0;
}
float StorylandLinuxAudio::continuousSoundGain(int32_t handle) const {
    const auto voice = continuousVoices.find(handle);
    return voice != continuousVoices.end() ? voice->second->stream.gain.load() : 0;
}
bool StorylandLinuxAudio::characterSpeechIsPlaying(int speaker) const {
    for(const auto& entry:characterVoices)
        if(entry.second->speaker==speaker)return enabled&&entry.second->stream.playing();
    return false;
}
int StorylandLinuxAudio::characterSpeechSample(int speaker) const {
    for(const auto& entry:characterVoices)if(entry.second->speaker==speaker)return entry.second->sample;
    return -1;
}
uint32_t StorylandLinuxAudio::characterSpeechStartCount(int speaker) const {
    for(const auto& entry:characterVoices)if(entry.second->speaker==speaker)return entry.second->starts;
    return 0;
}
uint64_t StorylandLinuxAudio::playbackMilliseconds() const { return voice->milliseconds; }
uint64_t StorylandLinuxAudio::radioPlaybackMilliseconds() const { return radio->milliseconds; }
uint32_t StorylandLinuxAudio::underrunCount() const {
    uint32_t count = voice->underruns + radio->underruns;
    for (const auto& stream : missionStreams) count += stream->underruns;
    for (const auto& voice : continuousVoices) count += voice.second->stream.underruns;
    return count;
}
uint32_t StorylandLinuxAudio::maximumQueuedBytes() const {
    uint32_t bytes = std::max(voice->peakQueuedBytes.load(), radio->peakQueuedBytes.load());
    for (const auto& stream : missionStreams) bytes = std::max(bytes, stream->peakQueuedBytes.load());
    for (const auto& voice : continuousVoices)
        bytes = std::max(bytes, voice.second->stream.peakQueuedBytes.load());
    return bytes;
}

std::string StorylandLinuxAudio::status() const {
    std::string text = voice->status();
    const auto music = radio->status();
    if (!music.empty()) { if (!text.empty()) text += " | "; text += music; }
    const auto tuneStatus = missionTune->status();
    if (missionTuneStream >= 0 && !tuneStatus.empty()) {
        if (!text.empty()) text += " | ";
        text += tuneStatus;
    }
    const auto scriptStatus = scriptMusic->status();
    if (scriptMusicStream >= 0 && !scriptStatus.empty()) {
        if (!text.empty()) text += " | ";
        text += scriptStatus;
    }
    for (const auto& stream : missionStreams) if (stream->playing()) {
        if (!text.empty()) text += " | ";
        text += stream->status();
    }
    if (!missionDiagnostic.empty()) { if (!text.empty()) text += " | "; text += missionDiagnostic; }
    return text;
}

void StorylandLinuxAudio::stop() { stopPlayback(false); }

void StorylandLinuxAudio::releaseOutput() {
    stop();
    output->close();
}

void StorylandLinuxAudio::stopPlayback(bool keepFrontend) {
    stopGeneratedVoice();
    stopContinuousSounds();
    stopWaterCannonSounds();
    stopPoliceHeliSounds();
    stopCharacterSpeech();
    voice->stop();
    radio->stop();
    scriptMusic->stop();
    scriptMusicStream = -1;
    stopMissionTune();
    policeRadio->stop();policeRadioQueue->clear();policeRadioSampleActive=false;
    lcsScriptSpeech->clear();
    missionTuneSerial = UINT32_MAX;
    for (auto& stream : missionStreams) stream->stop();
    for (auto& voice : vehicleVoices) voice->stop();
    for (auto& stream : effectStreams) stream->stop();
    if(!keepFrontend)frontendVoice->stop();
    vehicleHornHeld = false;
    missionSerials.fill(UINT64_MAX);
    catalog->clear();
    catalogRoot.clear();
    missionDiagnostic.clear();
    silentSampleRemainder = 0;
    currentPath.clear();
    loadSeen = startSeen = clearSeen = radioClearSeen = radioStartSeen = UINT32_MAX;
    radioChannel = -1;
    radioVehicle = -1;
    radioScripted = false;
    radioBroadcast = {};
    radioRoot.clear();
    policeRadioPath.clear();
    radioStations.clear();
    sceneWasPlaying = false;
    cutsceneClock.reset();
}

void StorylandLinuxAudio::setMode(StorylandAudioMode value) {
    output->selectedMode.store(storylandAudioModeFromInt(int(value)),std::memory_order_relaxed);
}

StorylandAudioMode StorylandLinuxAudio::mode() const {
    return output->selectedMode.load(std::memory_order_relaxed);
}

bool StorylandLinuxAudio::playGeneratedVoice(
    const std::vector<int16_t>& samples, uint32_t sampleRate, std::string& error) {
    if (!enabled) {
        error = "Audio is disabled. Enable audio before previewing a voice.";
        return false;
    }
    if (samples.empty() || sampleRate < 8000 || sampleRate > 192000 ||
        samples.size() > size_t(sampleRate) * 120) {
        error = "Generated voice PCM has invalid dimensions or exceeds two minutes.";
        return false;
    }
    if (!output->open()) {
        error = output->error;
        return false;
    }
    std::vector<int16_t> ownedSamples(samples);
    SDL_LockAudioDevice(output->device);
    output->generatedSamples.swap(ownedSamples);
    output->generatedRate = sampleRate;
    output->generatedPosition = 0;
    output->generatedActive.store(true, std::memory_order_release);
    SDL_UnlockAudioDevice(output->device);
    error.clear();
    return true;
}

void StorylandLinuxAudio::stopGeneratedVoice() {
    if (!output) return;
    if (output->device) SDL_LockAudioDevice(output->device);
    output->generatedActive.store(false, std::memory_order_release);
    output->generatedSamples.clear();
    output->generatedPosition = 0;
    output->generatedRate = 0;
    if (output->device) SDL_UnlockAudioDevice(output->device);
}

bool StorylandLinuxAudio::generatedVoicePlaying() const {
    return output && output->generatedActive.load(std::memory_order_acquire);
}

void StorylandLinuxAudio::setEnabled(bool value) {
    if (enabled == value) return;
    enabled = value;
    if (!enabled) {
        stopGeneratedVoice();
        frontendVoice->stop();
    }
    // Gameplay cues retain their queues while muted; a transient menu click
    // is discarded so it cannot resume unexpectedly when audio is enabled.
    if (output->device) SDL_PauseAudioDevice(output->device, enabled ? 0 : 1);
}

std::filesystem::path StorylandLinuxAudio::findStream(const StorylandLinuxSession& session,
    const std::vector<std::string>& names, bool missionClip) const {
    std::vector<std::filesystem::path> roots;
    auto base = session.gameRoot();
    for (unsigned level = 0; level < 3 && !base.empty(); ++level) {
        for (const auto* directory : {"AUDIO", "audio", "Audio", "MUSIC", "music"}) {
            std::error_code error;
            const auto candidate = base / directory;
            if (std::filesystem::is_directory(candidate, error)) roots.push_back(candidate);
        }
        const auto parent = base.parent_path();
        if (parent == base) break;
        base = parent;
    }
    roots.push_back(session.gameRoot());
    for (size_t key = 0; key < names.size(); ++key) {
        if (names[key].empty() || (key && names[key] == names[0])) continue;
        std::filesystem::path normalizedMatch;
        for (const auto& root : roots) {
            std::error_code error;
            for (std::filesystem::recursive_directory_iterator entry(root,
                    std::filesystem::directory_options::skip_permission_denied |
                    std::filesystem::directory_options::follow_directory_symlink, error), end;
                 entry != end; entry.increment(error)) {
                if (error) { error.clear(); continue; }
                if (entry.depth() > 5) { entry.disable_recursion_pending(); continue; }
                if (!entry->is_regular_file(error)) continue;
                const auto& path = entry->path();
                const auto extension = audioName(path.extension().string());
                const bool compressed = extension == ".at3" || extension == ".aa3" || extension == ".oma" ||
                    extension == ".mp3" || extension == ".ogg" || extension == ".flac";
                if (extension != ".vb" && !compressed &&
                    (!missionClip || (extension != ".vag" && extension != ".wav"))) continue;
                const auto stem = audioName(path.stem().string());
                if (stem == names[key]) return path;
                if (normalizedMatch.empty() && audioStreamKey(stem) == audioStreamKey(names[key]))
                    normalizedMatch = path;
            }
        }
        if (!normalizedMatch.empty()) return normalizedMatch;
    }
    return {};
}

std::filesystem::path StorylandLinuxAudio::findVoice(const StorylandLinuxSession& session) const {
    return findStream(session, {audioName(session.environment.cutscene.name),
        audioName(session.cutscene.scene().audioStreamName())});
}

void StorylandLinuxAudio::loadCatalog(const StorylandLinuxSession& session) {
    if (catalogRoot == session.gameRoot()) return;
    stopContinuousSounds();
    stopWaterCannonSounds();
    stopPoliceHeliSounds();
    stopCharacterSpeech();
    policeRadio->stop();policeRadioQueue->clear();policeRadioSampleActive=false;
    catalog = std::make_shared<StorylandNativeAudioCatalog>();
    catalogRoot = session.gameRoot();
    std::set<std::filesystem::path> banks;
    std::vector<std::filesystem::path> roots;
    auto base = session.gameRoot();
    for (unsigned level = 0; level < 3 && !base.empty(); ++level) {
        for (const auto* directory : {"AUDIO", "Audio", "audio"}) {
            std::error_code error;
            const auto path = base / directory;
            if (std::filesystem::is_directory(path, error)) roots.push_back(path);
        }
        const auto parent = base.parent_path();
        if (parent == base) break;
        base = parent;
    }
    if (roots.empty()) roots.push_back(session.gameRoot());
    for (const auto& root : roots) {
        std::error_code error;
        for (std::filesystem::recursive_directory_iterator entry(root,
                std::filesystem::directory_options::skip_permission_denied |
                std::filesystem::directory_options::follow_directory_symlink, error), end;
             entry != end; entry.increment(error)) {
            if (error) { error.clear(); continue; }
            if (entry.depth() > 5) { entry.disable_recursion_pending(); continue; }
            if (!entry->is_regular_file(error)) continue;
            const auto& path = entry->path();
            const auto extension = audioName(path.extension().string());
            if (extension != ".sdt" && extension != ".raw") continue;
            const auto canonical = std::filesystem::weakly_canonical(path, error);
            if (error || !banks.insert(canonical).second) { error.clear(); continue; }
            if (extension == ".raw") {
                if (session.vcs()) {
                    std::string diagnostic;
                    catalog->addPspBank(path.wstring(), diagnostic);
                    if (!diagnostic.empty()) missionDiagnostic = diagnostic;
                }
                continue;
            }
            auto raw = path;
            raw.replace_extension(".RAW");
            if (!std::filesystem::is_regular_file(raw, error)) raw.replace_extension(".raw");
            const auto rawSize = std::filesystem::file_size(raw, error);
            if (error) { error.clear(); continue; }
            std::ifstream input(path, std::ios::binary | std::ios::ate);
            const auto length = input ? input.tellg() : std::streampos(-1);
            if (length <= 0 || uint64_t(length) > 2u * 1024u * 1024u) continue;
            std::vector<uint8_t> bytes(static_cast<size_t>(length));
            input.seekg(0);
            if (!input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size()))) continue;
            StorylandSdtIndex index;
            std::string diagnostic;
            if (storylandDecodeSdtIndex(bytes, rawSize, index, diagnostic))
                catalog->addBank(path.wstring(), index, session.vcs());
        }
    }
}

void StorylandLinuxAudio::synchronizeLcsPoliceRadio(StorylandLinuxSession& session) {
    auto& world=session.environment;
    world.lcsExtendedWorld.policeRadioMessage=[this,&session](const auto& position,std::string& error) {
        return policeRadioQueue->enqueue(session.archive.nativeWorld().zones,
            {position[0],position[1],position[2]},session.environment.audioSubsystemEnabled,
            session.environment.scriptAudioMode,[this](uint32_t sample) {
                return catalog->lcsPoliceSample(sample)!=nullptr;
            },error);
    };
    if(session.vcs()||!world.audioSubsystemEnabled) {
        policeRadio->stop();policeRadioQueue->clear();policeRadioSampleActive=false;
        return;
    }
    const bool paused=session.paused||session.runtime.paused||session.runtime.pauseMenu||
        world.scriptAudioMode==2;
    policeRadio->controls(policeRadioSampleActive,paused,session.runtime.sounds?100.0f/127.0f:0.0f);
    if(policeRadioSampleActive) {
        if(!policeRadio->finished&&!policeRadio->failed)return;
        if(policeRadio->failed)missionDiagnostic=policeRadio->status();
        policeRadio->stop();policeRadioSampleActive=false;
    }
    if(paused)return;
    uint32_t sampleId=0;
    if(!policeRadioQueue->take(world.nativeFrameCounter,sampleId))return;
    const auto* sample=catalog->lcsPoliceSample(sampleId);
    if(!sample) {
        missionDiagnostic="The queued LCS police-radio sample is no longer available.";
        policeRadioQueue->clear();return;
    }
    policeRadio->start(sample->bankPath,0,false,sample->slot);
    const uint32_t frequency=sampleId==uint32_t(StorylandLcsPoliceRadioSample::MessageNoise)?
        10025u+world.nextNativeRandom()%2000u:22050u;
    policeRadio->pitch=sample->frequency?float(frequency)/float(sample->frequency):1.0f;
    policeRadioSampleActive=true;
    policeRadio->controls(true,false,session.runtime.sounds?100.0f/127.0f:0.0f);
}

void StorylandLinuxAudio::synchronizeMission(StorylandLinuxSession& session) {
    auto& slots = session.environment.audioSlots;
    for (size_t index = 0; index < slots.size(); ++index) {
        auto& slot = slots[index];
        auto& stream = *missionStreams[index];
        if (missionSerials[index] != slot.serial) {
            stream.stop();
            missionSerials[index] = slot.serial;
            if (!slot.name.empty()) {
                loadCatalog(session);
                const auto* sample = catalog->missionSample(slot.name);
                if (sample) stream.start(sample->bankPath, 0, false, sample->slot);
                else {
                    const auto path = findStream(session, {audioName(slot.name)}, true);
                    if (!path.empty()) stream.start(path);
                    else {
                        stream.loaded = false;
                        stream.finished = false;
                        stream.started = false;
                        stream.durationMilliseconds = 0;
                        stream.failed = true;
                        stream.status("Native mission audio source was not found: " + slot.name);
                    }
                }
            }
        }
        if (slot.name.empty()) {
            slot.vcsBackendLoading=false;
            if(slot.vcsReleasePending) {
                session.environment.finishVcsMissionAudioRelease(index);
                missionSerials[index]=slot.serial;
            }
            continue;
        }
        slot.loaded = stream.loaded && !stream.failed;
        slot.finished = stream.finished;
        slot.playbackStarted = stream.started;
        slot.vcsBackendLoading = !stream.loaded && !stream.failed;
        slot.durationMilliseconds = stream.durationMilliseconds;
        if(slot.vcsReleasePending&&!slot.vcsBackendLoading) {
            stream.stop();
            session.environment.finishVcsMissionAudioRelease(index);
            missionSerials[index]=slot.serial;
            continue;
        }
        if(slot.vcsReleasePending) {
            stream.controls(false,session.paused||session.runtime.paused||session.runtime.pauseMenu,0);
            continue;
        }
        if(slot.finished&&slot.playbackStarted&&session.environment.vcsMissionAudioAutoRelease(index)) {
            stream.stop();
            session.environment.finishVcsMissionAudioRelease(index);
            missionSerials[index]=slot.serial;
            continue;
        }
        const float distanceGain = slot.positioned ? std::clamp(1.0f -
            audioDistance(session,{slot.x,slot.y,slot.z}) / 100.0f, 0.0f, 1.0f) : 1.0f;
        stream.controls(slot.play, session.paused || session.runtime.paused || session.runtime.pauseMenu,
            session.runtime.sounds ? distanceGain : 0.0f);
        if (stream.failed && missionDiagnostic.empty())
            missionDiagnostic = "Mission audio '" + slot.name + "': " + stream.status();
    }
}

bool StorylandLinuxAudio::playFrontendSound(const StorylandLinuxSession& session,uint16_t event) {
    if(!enabled||!session.loaded()||!session.runtime.sounds||
        (session.active()&&(!session.environment.audioSubsystemEnabled||
            !session.environment.frontendAudioEnabled)))return false;
    loadCatalog(session);
    const auto definition=storylandMissionEventSound(session.vcs(),event,true);
    if(definition.nativeSilent)return false;
    const StorylandNativeSoundSample* sample=nullptr;
    if(!session.vcs()) {
        // The original LCS frontend bank labels these three sounds FE_*;
        // prefer those over similarly named gameplay sound aliases.
        const char* key=event==196?"sfxfeback":event==197?"sfxfehighlight":
            event==198?"sfxfeselect":nullptr;
        if(key)sample=catalog->namedVehicleSample(key);
    }
    if(!sample&&definition.sampleName)sample=catalog->namedVehicleSample(definition.sampleName);
    if(!sample&&definition.sample>=0) {
        sample=catalog->vcsSample(definition.bank,size_t(definition.sample),session.archive.isPsp());
        if(!sample)sample=catalog->vcsSample(definition.bank,size_t(definition.sample));
    }
    if(!sample||!output->open())return false;
    frontendVoice->start(sample->bankPath,0,false,sample->slot,StorylandFrontendAmbience::Menu);
    if(definition.frequency>0&&sample->frequency>0)
        frontendVoice->pitch=definition.frequency/float(sample->frequency);
    frontendVoice->controls(true,false,definition.volume);
    return true;
}

bool StorylandLinuxAudio::playMissionTune(const StorylandLinuxSession& session, int32_t stream,
    bool looping, std::string& error) {
    const char* stem = session.vcs()?storylandVcsMusicStreamStem(stream):storylandLcsMusicStreamStem(stream);
    if (!stem) {
        error = "Mission completion stream is outside the native table.";
        return false;
    }
    const auto path = findStream(session, {stem});
    if (path.empty()) {
        error = "Mission completion stream unavailable: " + std::string(stem);
        return false;
    }
    missionTune->start(path,0,looping,0,std::string(stem)=="miscom2"?
        StorylandFrontendAmbience::MissionPassed:StorylandFrontendAmbience::None);
    missionTuneStream = stream;
    error.clear();
    return true;
}

void StorylandLinuxAudio::stopMissionTune() {
    missionTune->stop();
    missionTuneStream = -1;
}

void StorylandLinuxAudio::synchronizeMissionTune(StorylandLinuxSession& session) {
    auto& world = session.environment;
    if(!session.vcs()&&world.lcsExtendedWorld.musicPlaying&&world.missionPassedTune<0) {
        world.missionPassedTune=world.lcsExtendedWorld.musicTrack;
        world.missionTuneLoop=world.lcsExtendedWorld.musicLooped;
        ++world.missionPassedTuneSerial;
    }
    if (world.missionPassedTune < 0) {
        if (missionTuneStream >= 0) stopMissionTune();
        missionTuneSerial = world.missionPassedTuneSerial;
        return;
    }
    if (missionTuneStream < 0 && missionTuneSerial == world.missionPassedTuneSerial) return;
    if (missionTuneStream != world.missionPassedTune ||
        missionTuneSerial != world.missionPassedTuneSerial) {
        // The VM starts the stream immediately; saved games need to restore it.
        if (missionTuneStream != world.missionPassedTune) {
            std::string error;
            if (!playMissionTune(session, world.missionPassedTune, world.missionTuneLoop, error)) {
                missionTune->status(error);
                missionTuneSerial = world.missionPassedTuneSerial;
                return;
            }
        }
        missionTuneSerial = world.missionPassedTuneSerial;
    }
    const bool interrupted = session.paused || session.runtime.paused || session.runtime.pauseMenu;
    const float fade = world.musicDoesFade ? 1.0f - std::clamp(world.fadeAlpha, 0.0f, 1.0f) : 1.0f;
    missionTune->controls(true, interrupted, session.runtime.sounds ? fade : 0.0f);
    if (missionTune->finished) {
        stopMissionTune();
        world.missionPassedTune = -1;
        world.missionTuneLoop = false;
        if(!session.vcs()) {
            world.lcsExtendedWorld.musicPlaying=false;
            if(world.scriptAudioMode==0) {
                world.scriptAudioMode=1;
                world.radioEnabled=true;
                world.radioChanged=true;
            }
        }
    }
}

void StorylandLinuxAudio::synchronizeScriptMusic(StorylandLinuxSession& session) {
    const auto& world = session.environment;
    const auto selection = session.vcs() ?
        storylandSelectScriptMusic(world.musicEmitters,
            {session.playerPosition.x, session.playerPosition.y, session.playerPosition.z}) :
        storylandLcsRiotMusic(world.lcsExtendedWorld.riotIntensity,
            {session.camera.eye.x,session.camera.eye.y,session.camera.eye.z});
    if (selection.stream != scriptMusicStream) {
        scriptMusic->stop();
        scriptMusic->status({});
        scriptMusicStream = selection.stream;
        if (const char* stem = session.vcs()?storylandVcsMusicStreamStem(selection.stream):
            storylandLcsMusicStreamStem(selection.stream)) {
            const auto path = findStream(session, {stem});
            if (path.empty()) scriptMusic->status("Script music stream unavailable: " + std::string(stem));
            else {
                const int32_t position=session.vcs()&&selection.stream>=0&&
                    size_t(selection.stream)<world.scriptMusicStartPositions.size()?
                    world.scriptMusicStartPositions[size_t(selection.stream)]:0;
                scriptMusic->start(path,storylandRadioScriptPositionSeconds(position,session.vcs()),true);
            }
        }
    }
    const bool interrupted = session.paused || session.runtime.paused || session.runtime.pauseMenu;
    const float fade = world.musicDoesFade ? 1.0f - std::clamp(world.fadeAlpha, 0.0f, 1.0f) : 1.0f;
    const bool audible = session.runtime.sounds && !world.cutscene.playing && missionTuneStream < 0;
    scriptMusic->controls(selection.stream >= 0, interrupted, audible ? selection.gain * fade : 0.0f);
}

void StorylandLinuxAudio::synchronizeVehicles(StorylandLinuxSession& session) {
    auto& runtime = session.runtime;
    if (!enabled || !runtime.sounds) {
        for (auto& voice : vehicleVoices) if (voice->vehicle >= 0) voice->stop();
        return;
    }
    loadCatalog(session);
    synchronizeLcsPoliceRadio(session);
    std::vector<std::pair<float, int>> candidates;
    const size_t count = std::min(session.vehicles.size(), runtime.vehicles.size());
    const int remoteVehicle=session.vcs()?storylandVcsRemoteVehicleSlot(runtime):-1;
    for (size_t index = 0; index < count; ++index) {
        const auto& vehicle = runtime.vehicles[index];
        if (vehicle.model >= runtime.modelDefinitions.size() ||
            runtime.modelDefinitions[vehicle.model].bicycle() || vehicle.health <= 0 ||
            (!vehicle.engineOn && !vehicle.siren)) continue;
        const bool occupied = int(index) == runtime.occupiedVehicle;
        const bool remote=int(index)==remoteVehicle;
        if (!vehicle.hasDriver && !vehicle.missionDriver && !vehicle.siren && !occupied &&
            !remote && vehicle.autoPilotMission != 14) continue;
        const auto& point = session.vehicles[index].position;
        const float distance = audioDistance(session,point);
        const bool retained = std::any_of(vehicleVoices.begin(), vehicleVoices.end(),
            [&](const auto& voice) { return voice->vehicle == int(index) && voice->model == vehicle.model; });
        const float range = vehicle.siren ? (session.vcs()?75.0f:110.0f) :
            std::max(StorylandRetailSkidRange, StorylandRetailEngineRange);
        if (occupied || distance < range + (retained ? 5.0f : 0.0f))
            candidates.emplace_back(occupied ? -1000.0f : distance - (retained ? 8.0f : 0.0f), int(index));
    }
    std::sort(candidates.begin(), candidates.end());
    if (candidates.size() > vehicleVoices.size()) candidates.resize(vehicleVoices.size());
    for (auto& voice : vehicleVoices) {
        const bool keep = std::any_of(candidates.begin(), candidates.end(),
            [&](const auto& candidate) { return candidate.second == voice->vehicle; });
        if (!keep && voice->vehicle >= 0) voice->stop();
    }
    const bool paused = session.paused || runtime.paused || runtime.pauseMenu;
    for (const auto& candidate : candidates) {
        auto entry = std::find_if(vehicleVoices.begin(), vehicleVoices.end(),
            [&](const auto& voice) { return voice->vehicle == candidate.second; });
        if (entry == vehicleVoices.end())
            entry = std::find_if(vehicleVoices.begin(), vehicleVoices.end(),
                [](const auto& voice) { return voice->vehicle < 0; });
        if (entry == vehicleVoices.end()) continue;
        auto& voice = **entry;
        const auto& vehicle = runtime.vehicles[size_t(candidate.second)];
        const auto& actor = session.vehicles[size_t(candidate.second)];
        const auto& definition = runtime.modelDefinitions[vehicle.model];
        const bool occupied = candidate.second == runtime.occupiedVehicle;
        if (voice.vehicle >= 0 && (voice.model != vehicle.model || voice.playerVehicle != occupied ||
            voice.scriptHandle != vehicle.scriptHandle)) voice.stop();
        if (voice.vehicle < 0) {
            voice.vehicle = candidate.second;
            voice.scriptHandle = vehicle.scriptHandle;
            voice.model = vehicle.model;
            voice.playerVehicle = occupied;
            voice.retryAt = 0;
        }
        const auto startLayer = [&](size_t layer, const StorylandNativeSoundSample* sample, bool loop) {
            if (!sample) return;
            auto& stream = *voice.layers[layer];
            stream.start(sample->bankPath, 0, loop, sample->slot);
        };
        const bool bobo=definition.vcsModelLayout&&definition.hasNativeAudio&&definition.modelId==208&&!occupied;
        if (!bobo&&runtime.elapsed >= voice.retryAt &&
            (voice.layers[0]->finished || voice.layers[0]->failed || !voice.layers[0]->worker.joinable() ||
             voice.layers[1]->finished || voice.layers[1]->failed || !voice.layers[1]->worker.joinable())) {
            voice.retryAt = runtime.elapsed + 1.0f;
            const auto pair = catalog->enginePair(definition, actor.water, occupied);
            for (size_t layer = 0; layer < 2; ++layer) {
                auto& stream = *voice.layers[layer];
                if (pair[layer] && (!stream.worker.joinable() || stream.finished || stream.failed))
                    startLayer(layer, pair[layer], true);
            }
        }
        const float rev = vehicle.nativeHandling ? std::clamp(vehicle.transmission.rpm +
            std::fabs(vehicle.throttle) * .12f, 0.0f, 1.0f) : std::clamp(
            std::fabs(vehicle.speed) / std::max(1.0f, vehicle.handlingTopSpeed) +
            std::fabs(vehicle.throttle) * .35f, 0.0f, 1.0f);
        const float distance = occupied ? 0.0f : audioDistance(session,actor.position);
        const float gain = .32f * storylandRetailSoundVolume(127, StorylandRetailEngineRange, distance) / 127.0f;
        const float effectGain = .32f * std::clamp(1 - distance / 75.0f, 0.0f, 1.0f);
        const bool idleAvailable = voice.layers[0]->worker.joinable() && !voice.layers[0]->failed;
        const bool revAvailable = voice.layers[1]->worker.joinable() && !voice.layers[1]->failed;
        if(bobo) {
            if(voice.layers[0]->worker.joinable())voice.layers[0]->stop();
            const bool wheelsOnGround=vehicle.contactsValid?
                std::any_of(vehicle.wheelContacts.begin(),vehicle.wheelContacts.end(),[](bool value){return value;}):
                vehicle.grounded;
            auto state=voice.bobo;
            const auto sound=storylandVcsBoboMotorSound(state,vehicle.speed/50.0f,
                definition.maximumTransmissionVelocity,vehicle.throttle,wheelsOnGround,
                uint32_t(candidate.second),distance);
            if(!paused)voice.bobo=state;
            auto& motor=*voice.layers[1];
            if(!sound.volume) {
                if(motor.worker.joinable())motor.stop();
            } else {
                const auto* sample=catalog->vcsSample(0,74);
                if(sample&&(!motor.worker.joinable()||motor.finished||motor.failed))startLayer(1,sample,true);
                if(sample&&sample->frequency)motor.pitch=float(sound.frequency)/float(sample->frequency);
                motor.controls(true,paused,float(sound.volume)/127.0f);
            }
        } else {
            voice.layers[0]->controls(idleAvailable, paused, gain * (revAvailable ? std::sqrt(1 - rev) : 1));
            voice.layers[0]->pitch = .9f + rev * .3f;
            voice.layers[1]->controls(revAvailable, paused, gain * (idleAvailable ? std::sqrt(rev) : 1));
            voice.layers[1]->pitch = .75f + rev * .8f;
        }
        const bool vcsAudio = definition.vcsModelLayout && definition.hasNativeAudio;
        const float slip = actor.water || !vehicle.skidRoadContact ? 0.0f : vehicle.tyreSlip;
        auto& skid = *voice.layers[2];
        const auto skidVolume = storylandRetailSkidVolume(slip, distance);
        if (!skidVolume) { if (skid.worker.joinable()) skid.stop(); }
        else {
            const auto* sample = vcsAudio ? catalog->vcsSample(0, 260) : catalog->vehicleNoise(false);
            if (sample && (!skid.worker.joinable() || skid.finished || skid.failed))
                startLayer(2, sample, true);
            if (sample && sample->frequency) skid.pitch = storylandRetailSkidFrequency(slip) / sample->frequency;
            skid.controls(true, paused, skidVolume / 127.0f);
        }
        auto& siren = *voice.layers[3];
        if (!vehicle.siren) { if (siren.worker.joinable()) siren.stop(); }
        else {
            const auto* sample = catalog->vehicleSirenSample(definition, session.archive.isPsp());
            if (sample && (!siren.worker.joinable() || siren.finished || siren.failed)) {
                startLayer(3, sample, true);
                const auto frequency=vcsAudio?definition.sirenFrequency:
                    storylandLcsVehicleSirenAudio(definition.modelId).frequency;
                if (sample->frequency&&frequency) siren.pitch = float(frequency) / float(sample->frequency);
            }
            const float sirenGain=vcsAudio?std::min(1.0f,effectGain*2):
                storylandRetailSoundVolume(80,110.0f,distance)/127.0f;
            siren.controls(true, paused, sirenGain);
        }
        auto& acceleration = *voice.layers[4];
        const bool accelerating = vcsAudio && occupied && definition.audioBank >= 4 && definition.audioBank <= 27 &&
            !actor.water && vehicle.throttle > .15f && vehicle.speed > .4f && !vehicle.handbrake && vehicle.grounded;
        if (!accelerating) {
            if (acceleration.worker.joinable()) acceleration.stop();
            voice.accelerationGear = UINT32_MAX;
        } else if (voice.accelerationGear != vehicle.transmission.gear) {
            if (acceleration.worker.joinable()) acceleration.stop();
            voice.accelerationGear = vehicle.transmission.gear;
            startLayer(4, catalog->vcsSample(definition.audioBank, 0), false);
        }
        acceleration.controls(accelerating, paused, gain);
        acceleration.pitch = .9f + rev * .3f;
        if (accelerating && acceleration.started && !acceleration.finished && !acceleration.failed)
            voice.layers[1]->gain = 0;
        auto& horn = *voice.layers[5];
        const bool hornActive = occupied && runtime.playerControl && vehicleHornHeld;
        if (!hornActive) {
            if (horn.worker.joinable()) horn.stop();
        } else {
            const auto* profile = !session.vcs() ? storylandLcsVehicleAudio(definition.modelId) : nullptr;
            const auto* sample = vcsAudio ? catalog->vcsVehicleSample(definition.hornSample, session.archive.isPsp()) :
                profile ? catalog->namedVehicleSample(profile->horn) : catalog->vehicleNoise(true);
            if (sample && (!horn.worker.joinable() || horn.finished || horn.failed)) {
                startLayer(5, sample, true);
                if (sample->frequency) {
                    if (vcsAudio) horn.pitch = float(definition.hornFrequency) / float(sample->frequency);
                    else if (profile) horn.pitch = float(profile->hornFrequency) / float(sample->frequency);
                }
            }
            horn.controls(true, paused, std::min(1.0f, effectGain * 2));
        }
    }
}

void StorylandLinuxAudio::stopContinuousSounds() {
    for (const auto& voice : continuousVoices)
        output->detachContinuous(voice.second->outputSlot);
    continuousVoices.clear();
}

void StorylandLinuxAudio::synchronizeContinuousSounds(StorylandLinuxSession& session) {
    const auto& sounds = session.environment.continuousSounds;
    for (auto voice = continuousVoices.begin(); voice != continuousVoices.end();) {
        const auto sound = sounds.find(voice->first);
        if (sound == sounds.end() || sound->second.id != voice->second->id) {
            output->detachContinuous(voice->second->outputSlot);
            voice = continuousVoices.erase(voice);
        } else {
            ++voice;
        }
    }
    const bool paused = session.paused || session.runtime.paused || session.runtime.pauseMenu;
    for (const auto& entry : sounds) {
        const auto& sound = entry.second;
        const auto definition = session.vcs()?storylandVcsContinuousSound(sound.id):storylandLcsContinuousSound(sound.id);
        if(definition.nativeSilent)continue;
        const auto* sample = definition.sampleName?catalog->namedVehicleSample(definition.sampleName):
            definition.sample>=0?catalog->vcsSample(definition.bank,size_t(definition.sample)):nullptr;
        if (!sample) {
            missionDiagnostic = "Continuous sound has no verified native sample binding: " +
                std::to_string(sound.id);
            continue;
        }
        auto voice = continuousVoices.find(entry.first);
        if (voice == continuousVoices.end()) {
            auto created = std::make_unique<ContinuousVoice>();
            created->id = sound.id;
            created->outputSlot = output->attachContinuous(created->stream);
            if (created->outputSlot >= output->streams.size()) {
                missionDiagnostic = "Continuous audio entity pool is full.";
                continue;
            }
            created->stream.start(sample->bankPath, 0, true, sample->slot);
            if (definition.frequency > 0 && sample->frequency > 0)
                created->stream.pitch = definition.frequency / float(sample->frequency);
            voice = continuousVoices.emplace(entry.first, std::move(created)).first;
        }
        const float distance = audioDistance(session,{sound.position[0],sound.position[1],sound.position[2]});
        auto& stream = voice->second->stream;
        stream.controls(true, paused, session.runtime.sounds ? storylandScriptSoundGain(definition, distance) : 0);
        if (stream.failed)
            missionDiagnostic = "Continuous sound " + std::to_string(sound.id) + ": " + stream.status();
    }
}

void StorylandLinuxAudio::stopWaterCannonSounds() {
    for(auto& voice:waterCannonVoices) {
        if(!voice)continue;
        voice->stream.stop();
        output->detachContinuous(voice->outputSlot);
        voice.reset();
    }
}

void StorylandLinuxAudio::synchronizeWaterCannonSounds(StorylandLinuxSession& session) {
    if(!session.vcs()) {stopWaterCannonSounds();return;}
    const auto* sample=storylandVcsWaterCannonSample(*catalog,session.archive.isPsp());
    const bool paused=session.paused||session.runtime.paused||session.runtime.pauseMenu;
    for(size_t index=0;index<waterCannonVoices.size();++index) {
        const auto& cannon=session.runtime.waterCannons[index];
        const auto& position=cannon.samples[cannon.current].position;
        const float gain=cannon.ownerSlot<0||!sample||!sample->frequency||!session.runtime.sounds?0.0f:
            storylandVcsWaterCannonGain(audioDistance(session,{position[0],position[1],position[2]}));
        auto& voice=waterCannonVoices[index];
        if(voice&&sample&&(voice->bankPath!=sample->bankPath||voice->sampleSlot!=sample->slot)) {
            voice->stream.stop();output->detachContinuous(voice->outputSlot);voice.reset();
        }
        if(gain<=0) {
            if(voice) {voice->stream.stop();output->detachContinuous(voice->outputSlot);voice.reset();}
            continue;
        }
        if(!voice) {
            voice=std::make_unique<ContinuousVoice>();
            voice->outputSlot=output->attachContinuous(voice->stream);
            if(voice->outputSlot>=output->streams.size()) {voice.reset();continue;}
            voice->bankPath=sample->bankPath;voice->sampleSlot=sample->slot;
            voice->stream.start(sample->bankPath,0,true,sample->slot);
            voice->stream.pitch=storylandVcsWaterCannonPitch(sample->frequency);
        }
        voice->stream.controls(true,paused,gain);
    }
}

void StorylandLinuxAudio::stopPoliceHeliSounds() {
    for(auto& entry:policeHeliVoices) {
        entry.second->stream.stop();
        output->detachContinuous(entry.second->outputSlot);
    }
    policeHeliVoices.clear();
}

void StorylandLinuxAudio::synchronizePoliceHeliSounds(StorylandLinuxSession& session) {
    if(!session.vcs()) {stopPoliceHeliSounds();return;}
    const auto* sample=storylandVcsPoliceHeliDistantSample(*catalog,session.archive.isPsp());
    const bool paused=session.paused||session.runtime.paused||session.runtime.pauseMenu;
    std::set<int32_t> live;
    for(size_t slot=0;slot<session.vehicles.size()&&slot<session.runtime.vehicles.size();++slot) {
        const auto& vehicle=session.runtime.vehicles[slot];
        if(!sample||!sample->frequency||!session.runtime.sounds||!vehicle.nativeDefinition.vcsModelLayout||
            vehicle.nativeDefinition.modelId!=261||vehicle.nativeDefinition.nativeType!=4)continue;
        const float gain=storylandVcsPoliceHeliDistantGain(vehicle.rotorSpeed,
            audioDistance(session,session.vehicles[slot].position));
        if(gain<=0)continue;
        const int32_t key=int32_t(slot);
        auto found=policeHeliVoices.find(key);
        if(found!=policeHeliVoices.end()&&(found->second->vehicleHandle!=vehicle.scriptHandle||
            found->second->bankPath!=sample->bankPath||found->second->sampleSlot!=sample->slot)) {
            found->second->stream.stop();output->detachContinuous(found->second->outputSlot);
            policeHeliVoices.erase(found);found=policeHeliVoices.end();
        }
        if(found==policeHeliVoices.end()) {
            auto voice=std::make_unique<ContinuousVoice>();
            voice->vehicleHandle=vehicle.scriptHandle;voice->bankPath=sample->bankPath;voice->sampleSlot=sample->slot;
            voice->outputSlot=output->attachContinuous(voice->stream);
            if(voice->outputSlot>=output->streams.size())continue;
            voice->stream.start(sample->bankPath,0,true,sample->slot);
            found=policeHeliVoices.emplace(key,std::move(voice)).first;
        }
        found->second->stream.controls(true,paused,gain);
        live.insert(key);
    }
    for(auto it=policeHeliVoices.begin();it!=policeHeliVoices.end();) {
        if(live.count(it->first)) {++it;continue;}
        it->second->stream.stop();output->detachContinuous(it->second->outputSlot);
        it=policeHeliVoices.erase(it);
    }
}

void StorylandLinuxAudio::stopCharacterSpeech() {
    for(const auto& entry:characterVoices)output->detachContinuous(entry.second->outputSlot);
    characterVoices.clear();characterSpeechGlobal={};characterSpeechCamera={};
}

bool StorylandLinuxAudio::queueCharacterSpeech(StorylandLinuxSession& session,int speaker,uint16_t event) {
    if((session.vcs()?(event<0x66||event>0x9F):!storylandLcsPlayerSpeechTiming(event))||
        session.paused||session.runtime.paused||
        session.runtime.pauseMenu||session.environment.cutscene.playing)return false;
    const auto actor=storylandResolveCharacterSpeechActor(session.runtime,session.environment,
        session.pedestrians,session.playerPosition,speaker);
    if(!actor.valid||(!session.vcs()&&!actor.player))return false;
    auto found=characterVoices.find(actor.key);
    if(found==characterVoices.end()) {
        auto created=std::make_unique<CharacterVoice>();created->speaker=speaker;
        created->outputSlot=output->attachContinuous(created->stream);
        if(created->outputSlot>=output->streams.size()) {
            missionDiagnostic="Native character speech output pool is full.";return false;
        }
        found=characterVoices.emplace(actor.key,std::move(created)).first;
    }
    const auto camera=session.camera.initialized?session.camera.eye:audioListener(session);
    if(session.vcs())return storylandQueueVcsCharacterSpeech(found->second->state.queue,event,
            session.environment.gameMilliseconds,actor.position.z,camera.z,
            characterSpeechCamera.averageDisplacement,[&]{return characterSpeechGlobal.nextRandom();});
    return storylandQueueLcsPlayerSpeech(found->second->state.lcsQueue,event,
            session.environment.gameMilliseconds,actor.position.z,camera.z,
            characterSpeechCamera.averageDisplacement,[&]{return characterSpeechGlobal.nextRandom();});
}

void StorylandLinuxAudio::synchronizeCharacterSpeech(StorylandLinuxSession& session) {
    static StorylandVcsSpeechAudio nativeSpeechAudio;
    static StorylandLcsSpeechAudio nativeLcsSpeechAudio;
    const bool paused=session.paused||session.runtime.paused||session.runtime.pauseMenu;
    const bool cutscene=session.environment.cutscene.playing;
    const auto registerFire=[&](int speaker) {
        const auto actor=storylandResolveCharacterSpeechActor(session.runtime,session.environment,
            session.pedestrians,session.playerPosition,speaker);
        if(!actor.valid||(!session.vcs()&&!actor.player)||characterVoices.count(actor.key)||!storylandCharacterHasActiveFire(
            session.runtime,session.environment,session.archive.nativeWorld(),actor))return;
        auto created=std::make_unique<CharacterVoice>();created->speaker=speaker;
        created->outputSlot=output->attachContinuous(created->stream);
        if(created->outputSlot<output->streams.size())characterVoices.emplace(actor.key,std::move(created));
    };
    if(!paused&&!cutscene) {
        registerFire(-1);
        for(size_t index=0;index<session.runtime.pedestrians.size();++index)registerFire(int(index));
        for(const auto& entry:session.environment.missionCharacters)
            registerFire(int(session.runtime.pedestrians.size())+entry.first);
    }
    std::vector<int64_t> serviceOrder;
    for(const auto& entry:characterVoices)serviceOrder.push_back(entry.first);
    const auto speakerForKey=[&](int64_t key) {
        return key<0?-1:storylandFindCharacterSpeechSpeaker(session.runtime,session.environment,uint64_t(key));
    };
    bool servicedNativeComment=false;
    std::sort(serviceOrder.begin(),serviceOrder.end(),[&](int64_t a,int64_t b){return speakerForKey(a)<speakerForKey(b);});
    for(int64_t key:serviceOrder) {
        auto entry=characterVoices.find(key);
        const int speaker=speakerForKey(key);
        if(speaker==-2) {
            output->detachContinuous(entry->second->outputSlot);characterVoices.erase(entry);continue;
        }
        const auto actor=storylandResolveCharacterSpeechActor(session.runtime,session.environment,
            session.pedestrians,session.playerPosition,speaker);
        if(!actor.valid||actor.key!=key) {
            output->detachContinuous(entry->second->outputSlot);characterVoices.erase(entry);continue;
        }
        auto& current=*entry->second;current.speaker=speaker;
        const float gain=actor.player?1.0f:std::clamp(1.0f-audioDistance(session,actor.position)/50.0f,0.0f,1.0f);
        current.stream.controls(!cutscene&&current.state.pendingAudioSample<0,paused,gain);
        if(cutscene) {
            current.stream.stop();current.state.queue.pending=StorylandNoCharacterSpeech;
            current.state.lcsQueue.pending=StorylandLcsNoSpeechEvent;
            current.state.pendingAudioSample=-1;
        } else if(!paused) {
            auto& pending=current.state;
            if(pending.pendingAudioSample>=0) {
                if(current.stream.failed||current.stream.cancelled)pending.pendingAudioSample=-1;
                else if(current.stream.loaded&&!session.environment.vcsAudioServiceSuppressed) {
                    if(servicedNativeComment)continue;
                    servicedNativeComment=true;
                    float pitch=1;
                    const bool accepted=session.vcs()?nativeSpeechAudio.admit(pending.pendingAudioSample,
                        pending.pendingAudioEvent,pending.pendingAudioFrequency,true,false,vcsAudioRandom,pitch):
                        nativeLcsSpeechAudio.admit(pending.pendingAudioSample,pending.pendingAudioEvent,
                            pending.pendingAudioFrequency,true,false,lcsAudioRandom,pitch);
                    pending.pendingAudioSample=-1;
                    if(accepted) {
                        current.stream.pitch=pitch;
                        current.stream.controls(true,false,gain);
                        ++current.starts;
                    } else current.stream.stop();
                }
                continue;
            }
            StorylandCharacterSpeechService context;
            context.player=actor.player;context.suppressedAnimation=actor.suppressedAnimation;
            context.burning=storylandCharacterHasActiveFire(session.runtime,session.environment,session.archive.nativeWorld(),actor);
            const int admitted=session.vcs()?
                storylandServiceVcsCharacterSpeech(current.state.queue,characterSpeechGlobal,
                    session.environment.gameMilliseconds,context,[&]{return characterSpeechGlobal.nextRandom();}):
                storylandServiceLcsPlayerSpeech(current.state.lcsQueue,characterSpeechGlobal,
                    session.environment.gameMilliseconds,context,[&]{return characterSpeechGlobal.nextRandom();});
            if(admitted>=0&&actor.enabled) {
                const auto* range=session.vcs()?storylandCharacterSpeechRange(actor,uint16_t(admitted)):
                    storylandLcsPlayerSpeechRange(uint16_t(admitted));
                if(range&&range->count&&(!session.vcs()||!actor.player||current.state.playerGate(uint16_t(admitted)))) {
                    const int selected=current.state.choose(*range,characterSpeechGlobal.nextRandom());
                    const auto* sample=selected<0?nullptr:session.vcs()?
                        catalog->vcsSample(2,size_t(selected),session.archive.isPsp()):catalog->lcsSample(uint32_t(selected));
                    const bool blocked=session.vcs()?nativeSpeechAudio.blocked(selected,uint16_t(admitted)):
                        nativeLcsSpeechAudio.blocked(selected,uint16_t(admitted));
                    if(sample&&sample->frequency&&gain>=.02f&&!blocked) {
                        current.stream.start(sample->bankPath,0,false,sample->slot);
                        current.stream.controls(false,false,gain);current.sample=selected;
                        current.state.pendingAudioSample=selected;
                        current.state.pendingAudioEvent=uint16_t(admitted);
                        current.state.pendingAudioFrequency=sample->frequency;
                    }
                }
            }
        }
        if(current.stream.failed)missionDiagnostic="Native character speech: "+current.stream.status();
    }
}

void StorylandLinuxAudio::synchronizeEffects(StorylandLinuxSession& session) {
    auto events = std::move(session.runtime.soundEvents);
    session.runtime.soundEvents.clear();
    auto scriptEvents=std::move(session.environment.scriptSounds);
    session.environment.scriptSounds.clear();
    if(session.vcs()&&session.environment.vcsAudioServiceSuppressed) {
        for(const auto& event:scriptEvents)if(event.vcsVehicle039a||event.vcsVehicleAudioEvent>=0||
            (event.frontend&&storylandVcsFrontendUsesAudioRandom(event.id))||
            (!event.frontend&&storylandVcsScriptSoundUsesAudioRandom(event.id)))
            session.environment.scriptSounds.push_back(event);
        scriptEvents.erase(std::remove_if(scriptEvents.begin(),scriptEvents.end(),
            [](const auto& event) {return event.vcsVehicle039a||event.vcsVehicleAudioEvent>=0||
                (event.frontend&&storylandVcsFrontendUsesAudioRandom(event.id))||
                (!event.frontend&&storylandVcsScriptSoundUsesAudioRandom(event.id));}),scriptEvents.end());
    }
    const bool paused = session.paused || session.runtime.paused || session.runtime.pauseMenu;
    if(!session.vcs()&&!paused) {
        for(const auto& event:events)
            if(event.nativeSpeech==StorylandLcsScriptSpeechEvent)
                lcsScriptSpeech->queue(session.environment,session.runtime,session.pedestrians,
                    event.speaker,uint16_t(event.nativeSpeech),session.camera.eye);
        lcsScriptSpeech->service(session.environment,session.runtime,session.pedestrians,session.camera.eye);
    }
    for (auto& stream : effectStreams) {
        stream->paused = paused;
        if ((!enabled || !session.runtime.sounds) && stream->worker.joinable()) stream->stop();
    }
    if(!enabled||!session.runtime.sounds){
        if(session.vcs()&&storylandVcsAudioEventAdmitted(session.environment.audioSubsystemEnabled,
            session.environment.vcsAudioServiceSuppressed))
            for(const auto& event:events)
                if(storylandIsPedContactSound(event))
                    storylandResolveVcsPedContactAudio(*catalog,event,session.runtime.groundSurface,
                        session.archive.isPsp(),vcsAudioRandom);
        if(session.vcs()&&storylandVcsAudioEventAdmitted(session.environment.audioSubsystemEnabled,
            session.environment.vcsAudioServiceSuppressed))
            for(const auto& event:scriptEvents) {
                if(event.vcsVehicle039a)storylandVcs039aVehicleSound(vcsAudioRandom);
                else if(event.vcsVehicleAudioEvent>=0)
                    storylandVcsSuspensionScriptSound(uint16_t(event.vcsVehicleAudioEvent),vcsAudioRandom);
                else if(event.frontend&&session.environment.frontendAudioEnabled&&
                    storylandVcsFrontendUsesAudioRandom(event.id)) {
                    auto definition=storylandVcsPrepareSharedFrontendSound(event.id,vcsAudioRandom,event.frontendParameter);
                    const auto* sample=catalog->vcsSample(definition.bank,size_t(definition.sample));
                    storylandVcsFinishSharedFrontendSound(definition,event.id,vcsAudioRandom,
                        sample?sample->frequency:0);
                }
                else if(!event.frontend&&storylandVcsScriptSoundUsesAudioRandom(event.id)) {
                    auto definition=storylandVcsScriptSound(event.id);
                    storylandVcsPrepareScriptAudioRandom(definition,event.id,vcsAudioRandom);
                    const auto* sample=definition.sample>=0?catalog->vcsSample(definition.bank,size_t(definition.sample)):nullptr;
                    storylandVcsApplyScriptAudioRandom(definition,event.id,vcsAudioRandom,sample?sample->frequency:0);
                }
            }
        stopCharacterSpeech();return;
    }
    loadCatalog(session);
    storylandEnsureCharacterSpeechIdentities(session.runtime,session.environment);
    if(!session.vcs()&&!paused)
        lcsAudioRandom.refresh([&] {return characterSpeechGlobal.nextRandom();});
    if(!paused&&!session.environment.cutscene.playing) {
        const auto camera=session.camera.initialized?session.camera.eye:audioListener(session);
        characterSpeechCamera.update(camera.x,camera.y,camera.z);
    }
    for (const auto& event : events) {
        if(!session.vcs()&&event.nativeSpeech==StorylandLcsScriptSpeechEvent)continue;
        const int speaker=event.speakerIdentity?storylandFindCharacterSpeechSpeaker(
            session.runtime,session.environment,event.speakerIdentity):event.speaker;
        if(event.nativeSpeech>=0&&(session.vcs()||speaker<0)) {
            if(speaker!=-2)queueCharacterSpeech(session,speaker,uint16_t(event.nativeSpeech));
            continue;
        }
        const bool nativePlayerDeath=storylandIsPlayerDeathSound(event,session.vcs());
        if(session.vcs()&&event.type==StorylandPlayTestSound::Damage&&!nativePlayerDeath)continue;
        if(session.vcs()&&event.type==StorylandPlayTestSound::MeleeSwing)
            queueCharacterSpeech(session,speaker,uint16_t(StorylandSpeechEvent::Attack));
        if(session.vcs()&&event.type==StorylandPlayTestSound::WeaponShot)
            queueCharacterSpeech(session,speaker,uint16_t(StorylandSpeechEvent::Shoot));
        if(!nativePlayerDeath&&(event.nativeSpeech>=0||event.type==StorylandPlayTestSound::Damage)) {
            const auto& world=session.environment;
            bool speechEnabled=world.playerSpeechEnabled;
            if(event.speaker>=0) {
                speechEnabled=true;
                if(size_t(event.speaker)>=session.runtime.pedestrians.size()) {
                    const int32_t handle=event.speaker-int32_t(session.runtime.pedestrians.size());
                    const auto character=world.missionCharacters.find(handle);
                    if(character!=world.missionCharacters.end())speechEnabled=character->second.speechEnabled;
                }
            }
            if(!speechEnabled)continue;
        }
        const bool nativePedContact=session.vcs()&&storylandIsPedContactSound(event);
        if(nativePedContact&&!storylandVcsAudioEventAdmitted(session.environment.audioSubsystemEnabled,
            session.environment.vcsAudioServiceSuppressed))continue;
        const auto pedContact=nativePedContact?storylandResolveVcsPedContactAudio(*catalog,event,
            session.runtime.groundSurface,session.archive.isPsp(),vcsAudioRandom):StorylandPedContactAudio{};
        const StorylandNativeSoundSample* sample=pedContact.sample;
        if(nativePlayerDeath&&(!session.environment.audioSubsystemEnabled||
            (session.vcs()&&session.environment.vcsAudioServiceSuppressed)))continue;
        const auto deathSound=nativePlayerDeath?storylandResolvePlayerDeathAudio(*catalog,
            session.vcs(),session.archive.isPsp(),session.vcs()?vcsAudioRandom:lcsAudioRandom,
            lcsDeathSoundVariation):StorylandPlayerDeathAudio{};
        if(nativePlayerDeath) {
            sample=deathSound.sample;
            if(!sample)continue;
        }
        if (event.type == StorylandPlayTestSound::VehicleAlarm && event.vehicle >= 0 &&
            size_t(event.vehicle) < session.runtime.vehicles.size()) {
            const auto& definition = session.runtime.vehicles[size_t(event.vehicle)].nativeDefinition;
            sample = definition.hasNativeAudio ? catalog->vcsVehicleSample(definition.hornSample, session.archive.isPsp()) : catalog->vehicleNoise(true);
        }
        StorylandVehicleDoorAudio doorSound;
        if ((event.type==StorylandPlayTestSound::VehicleDoorOpen||
             event.type==StorylandPlayTestSound::VehicleDoorClose)&&event.vehicle>=0&&
            size_t(event.vehicle)<session.runtime.vehicles.size()) {
            const auto& definition=session.runtime.vehicles[size_t(event.vehicle)].nativeDefinition;
            doorSound=storylandResolveVehicleDoorAudio(*catalog,definition,
                event.type==StorylandPlayTestSound::VehicleDoorOpen,session.archive.isPsp(),
                session.vcs()?vcsAudioRandom:lcsAudioRandom);
            sample=doorSound.sample;
        }
        if (event.type==StorylandPlayTestSound::VehicleStarter&&event.vehicle>=0&&
            size_t(event.vehicle)<session.runtime.vehicles.size()) {
            const auto& definition=session.runtime.vehicles[size_t(event.vehicle)].nativeDefinition;
            sample=catalog->vehicleStartSound(definition);
        }
        if (!sample&&!nativePedContact) {
            std::string weapon;
            if (event.weapon >= 0 && size_t(event.weapon) < session.runtime.weapons.size())
                weapon = session.runtime.weapons[size_t(event.weapon)].name;
            const auto matches = catalog->event(event.type, weapon);
            if (!matches.empty()) sample = matches[size_t(session.runtime.elapsed * 1000) % matches.size()];
        }
        if (!sample) continue;
        float gain = std::clamp(event.volume, 0.0f, 1.0f);
        if(nativePlayerDeath)gain*=storylandPlayerDeathSoundGain(deathSound,audioDistance(session,event.position));
        else if(nativePedContact)gain*=storylandVcsPedContactGain(pedContact.native,audioDistance(session,event.position));
        else if(doorSound.sample)gain*=storylandVehicleDoorSoundGain(doorSound,audioDistance(session,event.position));
        else if (event.positioned) gain *= std::clamp(1.0f-audioDistance(session,event.position)/75.0f,0.0f,1.0f);
        if(gain<=0)continue;
        auto stream=std::find_if(effectStreams.begin(),effectStreams.end(),[](const auto& item) {
            return !item->worker.joinable()||item->finished||item->failed;
        });
        if(stream==effectStreams.end())continue;
        (*stream)->start(sample->bankPath, 0, false, sample->slot);
        if(nativePlayerDeath)(*stream)->pitch=deathSound.pitch;
        else if(nativePedContact)(*stream)->pitch=pedContact.pitch;
        else if(doorSound.sample)(*stream)->pitch=doorSound.pitch;
        (*stream)->controls(true, paused, gain);
    }
    synchronizeCharacterSpeech(session);
    if(!session.environment.audioSubsystemEnabled)return;
    for(const auto& event:scriptEvents) {
        if(event.frontend&&!session.environment.frontendAudioEnabled)continue;
        const bool sharedFrontend=session.vcs()&&event.frontend&&
            storylandVcsFrontendUsesAudioRandom(event.id);
        auto definition=sharedFrontend?storylandVcsPrepareSharedFrontendSound(event.id,vcsAudioRandom,event.frontendParameter):
            event.vcsVehicleAudioEvent==StorylandMissionEnvironment::ScriptSound::LcsVehicleRocking?storylandLcsVehicleRockingSound():
            event.vcsVehicle039a?storylandVcs039aVehicleSound(vcsAudioRandom):
            event.vcsVehicleAudioEvent>=0?storylandVcsSuspensionScriptSound(uint16_t(event.vcsVehicleAudioEvent),vcsAudioRandom):
            storylandMissionEventSound(session.vcs(),event.id,event.frontend,
                uint32_t(session.environment.gameMilliseconds),session.runtime.groundSurface);
        if(sharedFrontend) {
            const auto* source=catalog->vcsSample(definition.bank,size_t(definition.sample));
            storylandVcsFinishSharedFrontendSound(definition,event.id,vcsAudioRandom,source?source->frequency:0);
        }
        if(session.vcs()&&!event.frontend&&!event.vcsVehicle039a&&event.vcsVehicleAudioEvent<0&&
            storylandVcsScriptSoundUsesAudioRandom(event.id)) {
            storylandVcsPrepareScriptAudioRandom(definition,event.id,vcsAudioRandom);
            const auto* source=definition.sample>=0?catalog->vcsSample(definition.bank,size_t(definition.sample)):nullptr;
            storylandVcsApplyScriptAudioRandom(definition,event.id,vcsAudioRandom,source?source->frequency:0);
        }
        if(definition.nativeSilent)continue;
        const StorylandNativeSoundSample* sample=definition.sampleName?
            catalog->namedVehicleSample(definition.sampleName):
            definition.sample>=0?catalog->vcsSample(definition.bank,size_t(definition.sample)):nullptr;
        if(!sample)continue;
        auto stream=std::find_if(effectStreams.begin(),effectStreams.end(),[](const auto& entry) {
            return !entry->worker.joinable()||entry->finished||entry->failed;
        });
        // Native processing still advances later event state when all output voices are occupied.
        if(stream==effectStreams.end())continue;
        const float distance=audioDistance(session,{event.position[0],event.position[1],event.position[2]});
        const float gain=storylandScriptSoundGain(definition,distance);
        if(gain<=0)continue;
        (*stream)->start(sample->bankPath,0,false,sample->slot,
            event.frontend&&(event.id==196||event.id==197||event.id==198)?
                StorylandFrontendAmbience::Menu:StorylandFrontendAmbience::None);
        if(definition.frequency>0&&sample->frequency>0)
            (*stream)->pitch=definition.frequency/float(sample->frequency);
        else (*stream)->pitch=1.0f;
        (*stream)->controls(true,paused&&!event.frontend,gain);
    }
}

void StorylandLinuxAudio::stopRadio(uint32_t gameMilliseconds) {
    if (radio->started || radio->finished)
        radioBroadcast.remember(radioChannel, radio->milliseconds, gameMilliseconds);
    radio->stop();
    radioChannel = -1;
}

void StorylandLinuxAudio::discoverRadio(const StorylandLinuxSession& session) {
    if (radioRoot == session.gameRoot()) return;
    radioRoot = session.gameRoot();
    radioStations = storylandLinuxDiscoverRadio(radioRoot, session.vcs(), &policeRadioPath);
}

void StorylandLinuxAudio::startVehicleRadio(StorylandLinuxSession& session, int selection) {
    const int vehicle = session.runtime.occupiedVehicle;
    if (vehicle < 0 || size_t(vehicle) >= session.runtime.vehicles.size()) return;
    auto& runtime = session.runtime;
    auto& car = runtime.vehicles[size_t(vehicle)];
    if (storylandVehicleUsesPoliceRadio(runtime, car)) {
        radioVehicle = vehicle;
        stopRadio(session.environment.gameMilliseconds);
        radioScripted = false;
        radioChannel = storylandPoliceRadioChannel(session.vcs());
        runtime.radioPopupTime = 0;
        runtime.radioStationStem = "police";
        runtime.radioStationLabel = "Police Radio";
        if (policeRadioPath.empty()) {
            radio->status("Police radio stream was not found in AUDIO/MUSIC.");
            return;
        }
        radio->start(policeRadioPath, radioBroadcast.position(radioChannel, session.vcs(),
            session.environment.gameMilliseconds, policeRadioPath), true);
        return;
    }
    selection = std::clamp(selection, 0, int(radioStations.size()));
    car.radioStation = selection;
    radioVehicle = vehicle;
    stopRadio(session.environment.gameMilliseconds);
    runtime.radioPopupTime = 3.0f;
    if (size_t(selection) >= radioStations.size()) {
        runtime.radioStationStem.clear();
        runtime.radioStationLabel = "Radio Off";
        radio->status("Radio off.");
        return;
    }
    const auto& station = radioStations[size_t(selection)];
    radioChannel = station.channel;
    runtime.radioStationStem = station.info.key;
    runtime.radioStationLabel = station.info.label;
    radio->start(station.path, radioBroadcast.position(station.channel, session.vcs(),
        session.environment.gameMilliseconds, station.path), true);
}

void StorylandLinuxAudio::cycleRadio(StorylandLinuxSession& session, int direction) {
    const int vehicle = session.runtime.occupiedVehicle;
    if (radioScripted || storylandOccupiedVehicleUsesPoliceRadio(session.runtime) ||
        vehicle < 0 || size_t(vehicle) >= session.runtime.vehicles.size() ||
        session.environment.cutscene.playing || session.paused || session.runtime.paused ||
        session.runtime.pauseMenu) return;
    discoverRadio(session);
    const int current = session.runtime.vehicles[size_t(vehicle)].radioStation;
    const int selection = storylandLinuxRadioSelection(current, direction, radioStations.size());
    session.runtime.soundEvents.emplace_back(StorylandPlayTestSound::RadioSwitch);
    startVehicleRadio(session, selection);
}

void StorylandLinuxAudio::synchronizeRadio(StorylandLinuxSession& session) {
    auto& world = session.environment;
    const bool police = storylandOccupiedVehicleUsesPoliceRadio(session.runtime);
    if (police) {
        world.radioChannelChanged = false;
        if (radioScripted) {
            stopRadio(world.gameMilliseconds);
            radioScripted = false;
            radioVehicle = -1;
        }
    }
    if (world.radioChannelChanged) {
        world.radioChannelChanged = false;
        const unsigned channel = uint8_t(world.radioChannel);
        if (const char* key = storylandRadioChannelKey(int(channel), session.vcs())) {
            stopRadio(session.environment.gameMilliseconds);
            radioScripted = false;
            radioVehicle = -1;
            discoverRadio(session);
            const auto station = std::find_if(radioStations.begin(), radioStations.end(),
                [&](const auto& value) { return value.channel == int(channel); });
            const auto path = station != radioStations.end() ? station->path : findStream(session, {key});
            if (path.empty()) radio->status("Native radio stream was not found: " + std::string(key));
            else {
                radioChannel = int32_t(channel);
                radioScripted = true;
                radioClearSeen = world.cutscene.clearRequest;
                radioStartSeen = world.cutscene.startRequest;
                const double position = world.radioPosition < 0 ?
                    radioBroadcast.position(int(channel), session.vcs(), world.gameMilliseconds, path) :
                    storylandRadioScriptPositionSeconds(world.radioPosition, session.vcs());
                radio->start(path, position, true);
                session.runtime.radioStationStem = key;
                session.runtime.radioStationLabel = storylandRadioStationInfo(key, session.vcs()).label;
            }
        } else if (channel == unsigned(session.vcs() ? 9 : 10)) {
            stopRadio(session.environment.gameMilliseconds);
            radioScripted = false;
            session.runtime.radioStationStem.clear();
            session.runtime.radioStationLabel = "Radio Off";
            const int vehicle = session.runtime.occupiedVehicle;
            if (vehicle >= 0 && size_t(vehicle) < session.runtime.vehicles.size()) {
                discoverRadio(session);
                session.runtime.vehicles[size_t(vehicle)].radioStation = int(radioStations.size());
                radioVehicle = vehicle;
            }
            radio->status("Radio off.");
        } else if (channel != 255) radio->status("Unsupported native radio channel: " + std::to_string(channel));
    }
    if (radioScripted && (world.cutscene.clearRequest != radioClearSeen ||
        world.cutscene.startRequest != radioStartSeen)) {
        stopRadio(session.environment.gameMilliseconds);
        radioScripted = false;
        radioVehicle = -1;
        radio->status({});
    }
    const int vehicle = session.runtime.occupiedVehicle;
    const bool driving = vehicle >= 0 && size_t(vehicle) < session.runtime.vehicles.size() &&
        !world.cutscene.playing && !world.camera.cutsceneOwned && !world.widescreen;
    if (!radioScripted && !driving && radioVehicle >= 0) {
        stopRadio(session.environment.gameMilliseconds);
        radioVehicle = -1;
        session.runtime.radioPopupTime = 0;
    }
    const bool policeChannel = radioChannel == storylandPoliceRadioChannel(session.vcs());
    if (!radioScripted && driving && (radioVehicle != vehicle || police != policeChannel)) {
        discoverRadio(session);
        int selection = session.runtime.vehicles[size_t(vehicle)].radioStation;
        if (selection < 0) selection = radioStations.empty() ? 0 : vehicle % int(radioStations.size());
        startVehicleRadio(session, selection);
    }
    const bool interrupted = session.paused || session.runtime.paused || session.runtime.pauseMenu;
    bool speech = voice->playing();
    for (size_t index = 0; index < missionStreams.size(); ++index)
        speech = speech || (world.audioSlots[index].playFlag && missionStreams[index]->playing());
    const float fade = world.musicDoesFade ? 1.0f - std::clamp(world.fadeAlpha, 0.0f, 1.0f) : 1.0f;
    const float volume = session.runtime.sounds && session.runtime.radioEnabled &&
        world.frontendAudioEnabled && world.radioEnabled ?
        fade * (speech ? 20.0f / 127.0f : 1.0f) : 0;
    radio->controls(radioChannel >= 0, interrupted, volume);
}

void StorylandLinuxAudio::serviceNativeVcsAudio(StorylandLinuxSession& session) {
    if(!session.active()||!session.vcs())return;
    auto& world=session.environment;
    vcsAudioRandom.service([&world] {return world.nextNativeRandom();},world.vcsAudioServiceSuppressed);
}

void StorylandLinuxAudio::synchronize(StorylandLinuxSession& session) {
    if(session.active()&&session.vcs()) {
        vcsAudioRandom.initialize();
    }
    if (!session.active() || !session.environment.audioSubsystemEnabled) {
        if (voice->worker.joinable() || radio->worker.joinable() || scriptMusic->worker.joinable() || missionTune->worker.joinable() ||
            !currentPath.empty()) stopPlayback(!session.active());
        missionDiagnostic.clear();
        return;
    }
    missionDiagnostic.clear();
    loadCatalog(session);
    const std::weak_ptr<StorylandNativeAudioCatalog> nativeCatalog = catalog;
    session.environment.continuousSoundAvailable = [nativeCatalog, vcs = session.vcs()](uint16_t id) {
        const auto available = nativeCatalog.lock();
        if (!available) return false;
        const auto definition = vcs?storylandVcsContinuousSound(id):storylandLcsContinuousSound(id);
        if(!vcs)return definition.nativeSilent||
            (definition.sampleName&&available->namedVehicleSample(definition.sampleName));
        return !definition.nativeSilent && definition.sample >= 0 &&
            available->vcsSample(definition.bank, size_t(definition.sample));
    };
    if (enabled && !output->open()) {
        voice->failed = true;
        voice->status(output->error);
        missionDiagnostic = output->error;
        return;
    }
    const auto& scene = session.environment.cutscene;
    if (scene.clearRequest != clearSeen) {
        voice->stop();
        currentPath.clear();
        clearSeen = scene.clearRequest;
        loadSeen = UINT32_MAX;
        startSeen = scene.startRequest;
        sceneWasPlaying = false;
        cutsceneClock.reset();
    }
    if (scene.loaded && scene.loadRequest != loadSeen) {
        loadSeen = scene.loadRequest;
        currentPath = findVoice(session);
        if (!currentPath.empty()) {
            voice->controls(false, session.paused || session.runtime.paused || session.runtime.pauseMenu,
                session.runtime.sounds ? 1.0f : 0.0f);
            voice->start(currentPath, scene.elapsedMilliseconds / 1000.0);
            cutsceneClock.reset(scene.elapsedMilliseconds);
        }
        else {
            voice->stop();
            voice->failed = true;
            voice->status("Native cutscene audio was not found for " + scene.name + ".");
        }
    }
    if (scene.startRequest != startSeen) {
        startSeen = scene.startRequest;
        if (!currentPath.empty() && (voice->started || voice->finished || voice->failed)) voice->start(currentPath);
        cutsceneClock.reset();
    }
    voice->controls(scene.playing && !cutsceneClock.fallbackActive(),
        session.paused || session.runtime.paused || session.runtime.pauseMenu,
        session.runtime.sounds ? 1.0f : 0.0f);
    if (sceneWasPlaying && !scene.playing) voice->stop();
    sceneWasPlaying = scene.playing;
    if (scene.loaded && voice->failed) missionDiagnostic = voice->status();
    synchronizeMission(session);
    synchronizeMissionTune(session);
    synchronizeScriptMusic(session);
    synchronizeRadio(session);
    synchronizeVehicles(session);
    synchronizeWaterCannonSounds(session);
    synchronizePoliceHeliSounds(session);
    synchronizeContinuousSounds(session);
    synchronizeEffects(session);
}

void StorylandLinuxAudio::advanceSilently(float seconds) {
    if (enabled || !std::isfinite(seconds) || seconds <= 0) return;
    silentSampleRemainder += double(seconds) * 32000.0;
    size_t frames = size_t(silentSampleRemainder);
    silentSampleRemainder -= double(frames);
    std::array<int16_t,2048> discarded{};
    while (frames) {
        const size_t count = std::min(frames, discarded.size() / 2);
        discarded.fill(0);
        for (auto* stream : output->streams) if (stream) stream->mix(discarded.data(), count * 2);
        frames -= count;
    }
}

uint32_t StorylandLinuxAudio::advance(uint32_t wallMilliseconds, bool paused) {
    const uint32_t elapsed = cutsceneClock.advance(wallMilliseconds, enabled,
        !currentPath.empty(), voice->requested, paused || (voice->requested && voice->paused), voice->failed,
        voice->started, voice->finished, voice->milliseconds);
    if (cutsceneClock.fallbackActive()) {
        // A late decoder or a stalled device must not resume old dialogue
        // after the scene has switched to its independent clock.
        voice->controls(false, voice->paused, 0.0f);
    }
    return elapsed;
}

