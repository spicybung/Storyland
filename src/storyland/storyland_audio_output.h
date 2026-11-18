#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xaudio2.h>
#include "src/storyland/storyland_audio_mixer.h"
#include <objbase.h>
#include "src/platform/windows/storyland_audio_session.h"
#include <deque>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <string>
#include <memory>
#include <mutex>

class StorylandAudioDevice {
public:
    ~StorylandAudioDevice() {
        if(master)master->DestroyVoice();
        if(engine)engine->Release();
        if(module)FreeLibrary(module);
    }
    static std::shared_ptr<StorylandAudioDevice> acquire(std::string& error) {
        static std::mutex mutex;
        static std::weak_ptr<StorylandAudioDevice> active;
        std::lock_guard<std::mutex> lock(mutex);
        if(auto device=active.lock())return device;
        // COM is per-thread; media inspection and streaming can enter here
        // outside the game-audio worker. Keep it initialized for that thread.
        struct Apartment {
            HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
            ~Apartment(){if(SUCCEEDED(result))CoUninitialize();}
        };
        thread_local Apartment apartment;
        if(FAILED(apartment.result)&&apartment.result!=RPC_E_CHANGED_MODE) {
            error="Windows audio COM initialization failed ("+std::to_string(uint32_t(apartment.result))+").";
            return {};
        }
        auto device=std::make_shared<StorylandAudioDevice>();
        device->module=LoadLibraryW(L"xaudio2_9.dll");
        if(!device->module)device->module=LoadLibraryW(L"xaudio2_8.dll");
        if(!device->module) {error="Windows XAudio2 could not be loaded.";return {};}
        using Create=HRESULT (WINAPI*)(IXAudio2**,UINT32,XAUDIO2_PROCESSOR);
        const auto create=reinterpret_cast<Create>(GetProcAddress(device->module,"XAudio2Create"));
        HRESULT result=create?create(&device->engine,0,XAUDIO2_DEFAULT_PROCESSOR):E_NOINTERFACE;
        if(SUCCEEDED(result))result=device->engine->CreateMasteringVoice(&device->master);
        if(SUCCEEDED(result))result=device->engine->StartEngine();
        if(FAILED(result)) {
            error="Windows audio device initialization failed ("+std::to_string(uint32_t(result))+").";
            return {};
        }
        active=device;return device;
    }
    IXAudio2* engine=nullptr;
private:
    HMODULE module=nullptr;
    IXAudio2MasteringVoice* master=nullptr;
};

class StorylandAudioOutput {
public:
    ~StorylandAudioOutput() { close(); }
    StorylandAudioOutput() = default;
    StorylandAudioOutput(const StorylandAudioOutput&) = delete;
    StorylandAudioOutput& operator=(const StorylandAudioOutput&) = delete;

    bool open(uint32_t sampleRate, uint16_t channels, std::string& error) {
        close();
        if(!sampleRate||(channels!=1&&channels!=2)) {
            error="Audio output requires mono or stereo PCM with a nonzero sample rate.";return false;
        }
        std::string mixerError;
        mixer=StorylandAudioMixer::acquire(mixerError);
        if(mixer) {
            mixedVoice=mixer->add(sampleRate,channels);
            rate=sampleRate;
            error.clear();return true;
        }
        std::string deviceError;
        device=StorylandAudioDevice::acquire(deviceError);
        WAVEFORMATEX format{};
        format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=channels;
        format.nSamplesPerSec=sampleRate;format.wBitsPerSample=16;
        format.nBlockAlign=WORD(channels*sizeof(int16_t));
        format.nAvgBytesPerSec=sampleRate*format.nBlockAlign;
        const HRESULT result=device?device->engine->CreateSourceVoice(&voice,&format,0,2.0f):E_FAIL;
        if(FAILED(result)) {
            if(device)deviceError="Windows XAudio2 source initialization failed ("+std::to_string(uint32_t(result))+").";
            device.reset();
            error="SDL audio output: "+mixerError+"; XAudio2: "+deviceError;
            return false;
        }
        rate=sampleRate;
        error.clear();return true;
    }
    const char* backendName() const {
        if(mixer)return mixer->backendName();
        return voice?"XAudio2":"none";
    }
    bool submit(const void* data, uint32_t bytes, bool loop=false) {
        if(!data||!bytes)return false;
        if(mixer) {
            std::lock_guard<std::mutex> lock(mixer->mutex);
            const uint32_t frameBytes=mixedVoice->channels*sizeof(int16_t);
            if(bytes%frameBytes)return false;
            mixedVoice->buffers.push_back({static_cast<const int16_t*>(data),bytes/frameBytes,loop});return true;
        }
        if(!voice)return false;
        XAUDIO2_BUFFER buffer{};
        buffer.AudioBytes=bytes;buffer.pAudioData=static_cast<const BYTE*>(data);
        buffer.LoopCount=loop?XAUDIO2_LOOP_INFINITE:0;
        return SUCCEEDED(voice->SubmitSourceBuffer(&buffer));
    }
    bool pause(bool value) {
        bool succeeded=false;
        if(mixer) {
            std::lock_guard<std::mutex> lock(mixer->mutex);
            mixedVoice->paused=value;succeeded=true;
        } else succeeded=voice&&SUCCEEDED(value?voice->Stop():voice->Start());
#ifdef _WIN32
        // A new Windows session can become visible only after playback starts.
        // Retry the bounded startup recovery without holding the callback lock.
        if(succeeded&&!value)storylandWindowsAudio::recoverOutputSessionOnce(backendName());
#endif
        return succeeded;
    }
    void gain(float value) {
        value=std::isfinite(value)?std::clamp(value,0.0f,1.0f):0.0f;
        if(voice)voice->SetVolume(value);
        if(mixer){std::lock_guard<std::mutex> lock(mixer->mutex);mixedVoice->gain=value;}
    }
    void pitch(float value) {
        value=std::isfinite(value)?std::clamp(value,0.5f,2.0f):1.0f;
        if(voice)voice->SetFrequencyRatio(value);
        if(mixer){std::lock_guard<std::mutex> lock(mixer->mutex);mixedVoice->pitch=value;}
    }
    uint32_t queued() const {
        if(mixer){std::lock_guard<std::mutex> lock(mixer->mutex);return uint32_t(mixedVoice->buffers.size());}
        XAUDIO2_VOICE_STATE state{};
        if(voice)voice->GetState(&state);
        return state.BuffersQueued;
    }
    uint32_t milliseconds() const {
        if(mixer) {
            std::lock_guard<std::mutex> lock(mixer->mutex);
            return uint32_t(std::min(mixedVoice->played*1000/rate,double(UINT32_MAX)));
        }
        XAUDIO2_VOICE_STATE state{};
        if(voice)voice->GetState(&state);
        return rate?uint32_t(std::min<uint64_t>(state.SamplesPlayed*1000/rate,UINT32_MAX)):0;
    }
    void close() {
        if(mixer){mixer->remove(mixedVoice);mixedVoice.reset();mixer.reset();}
        if(voice) {voice->DestroyVoice();voice=nullptr;}
        device.reset();
        rate=0;
    }
private:
    std::shared_ptr<StorylandAudioMixer> mixer;
    std::shared_ptr<StorylandAudioMixer::Voice> mixedVoice;
    std::shared_ptr<StorylandAudioDevice> device;
    IXAudio2SourceVoice* voice=nullptr;
    uint32_t rate=0;
};
#endif
