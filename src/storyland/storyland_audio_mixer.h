#pragma once
#include "src/storyland/storyland_audio_mode.h"
#include <SDL.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

// Shared PCM output. Per-voice gain is mixed in software: changing radio
// volume must never change the Windows endpoint volume or mute dialogue.
class StorylandAudioMixer {
public:
    struct Buffer {const int16_t* data=nullptr;uint32_t frames=0;bool loop=false;};
    struct Voice {
        uint32_t rate=0;uint16_t channels=0;
        double cursor=0,played=0;
        float gain=1,pitch=1;
        bool paused=true;
        std::deque<Buffer> buffers;
    };
    std::mutex mutex;
    StorylandAudioProcessor audioProcessor{48000};
    ~StorylandAudioMixer() {
        if(output)SDL_CloseAudioDevice(output);
        if(initialized)SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    static std::shared_ptr<StorylandAudioMixer> acquire(std::string& error) {
        std::lock_guard<std::mutex> guard(controlMutex());
        if(auto device=activeDevice().lock()) {
            if(device->output&&SDL_GetAudioDeviceStatus(device->output)==SDL_AUDIO_PLAYING) {
                error.clear();return device;
            }
            if(device->openOutput(preferredDevice(),error))return device;
            return {};
        }
        auto device=std::make_shared<StorylandAudioMixer>();
        if(SDL_InitSubSystem(SDL_INIT_AUDIO)!=0){error=SDL_GetError();return {};}
        device->initialized=true;
        if(!device->openOutput(preferredDevice(),error))return {};
        activeDevice()=device;error.clear();return device;
    }
    static StorylandAudioMode currentAudioMode() {
        return preferredAudioMode().load(std::memory_order_relaxed);
    }
    static void selectAudioMode(StorylandAudioMode mode) {
        preferredAudioMode().store(storylandAudioModeFromInt(int(mode)), std::memory_order_relaxed);
    }
    static std::shared_ptr<StorylandAudioMixer> current() {
        std::lock_guard<std::mutex> guard(controlMutex());
        return activeDevice().lock();
    }
    static std::vector<std::string> outputDevices() {
        std::lock_guard<std::mutex> guard(controlMutex());
        std::vector<std::string> names;
        if(!SDL_WasInit(SDL_INIT_AUDIO))return names;
        const int count=SDL_GetNumAudioDevices(0);
        for(int index=0;index<count;++index) {
            const char* name=SDL_GetAudioDeviceName(index,0);
            if(name&&*name)names.emplace_back(name);
        }
        return names;
    }
    static std::string preferredOutputDevice() {
        std::lock_guard<std::mutex> guard(controlMutex());
        return preferredDevice();
    }
    static bool selectOutputDevice(const std::string& name,std::string& error) {
        std::lock_guard<std::mutex> guard(controlMutex());
        auto device=activeDevice().lock();
        if(!device) {preferredDevice()=name;error.clear();return true;}
        const std::string previous=preferredDevice();
        if(device->output&&name==device->openedDevice&&
            SDL_GetAudioDeviceStatus(device->output)==SDL_AUDIO_PLAYING) {
            preferredDevice()=name;error.clear();return true;
        }
        if(device->openOutput(name,error)) {
            preferredDevice()=name;return true;
        }
        const std::string selectionFailure=error;
        std::string restoreFailure;
        if(device->openOutput(previous,restoreFailure))
            error=selectionFailure+" Previous audio output restored.";
        else error=selectionFailure+" Previous audio output could not be restored: "+restoreFailure;
        return false;
    }
    std::string describe() const {
        std::lock_guard<std::mutex> guard(controlMutex());
        std::ostringstream description;
        description<<"Device: "<<(openedDevice.empty()?"System default":openedDevice)
            <<"; driver: "<<backendName()<<"; callbacks: "<<callbackCount.load(std::memory_order_acquire)
            <<std::fixed<<std::setprecision(6)<<"; PCM peak: "<<lastPeak.load()
            <<"; PCM RMS: "<<lastRms.load();
        if(!output||SDL_GetAudioDeviceStatus(output)!=SDL_AUDIO_PLAYING)description<<"; output stopped";
        return description.str();
    }
    std::string statusText() const {return describe();}
    const char* backendName() const {
        const char* driver=SDL_GetCurrentAudioDriver();
        return driver?driver:"SDL audio";
    }
    std::shared_ptr<Voice> add(uint32_t rate,uint16_t channels) {
        auto voice=std::make_shared<Voice>();voice->rate=rate;voice->channels=channels;
        std::lock_guard<std::mutex> guard(mutex);voices.push_back(voice);return voice;
    }
    void remove(const std::shared_ptr<Voice>& voice) {
        std::lock_guard<std::mutex> guard(mutex);
        voices.erase(std::remove(voices.begin(),voices.end(),voice),voices.end());
    }
    // Also used by deterministic audio checks without opening a physical device.
    void mix(float* destination,size_t frames) {
        std::fill(destination,destination+frames*2,0.0f);
        std::lock_guard<std::mutex> guard(mutex);
        for(auto& voice:voices) {
            if(voice->paused)continue;
            const double step=double(voice->rate)*voice->pitch/48000.0;
            for(size_t frame=0;frame<frames&&!voice->buffers.empty();++frame) {
                auto& buffer=voice->buffers.front();
                const size_t sample=std::min(size_t(voice->cursor),size_t(buffer.frames-1));
                const size_t next=std::min(sample+1,size_t(buffer.frames-1));
                const float fraction=float(voice->cursor-sample);
                for(unsigned channel=0;channel<2;++channel) {
                    const unsigned source=voice->channels==1?0:channel;
                    const float a=buffer.data[sample*voice->channels+source];
                    const float b=buffer.data[next*voice->channels+source];
                    destination[frame*2+channel]+=(a+(b-a)*fraction)*voice->gain/32768.0f;
                }
                voice->cursor+=step;voice->played+=step;
                while(!voice->buffers.empty()&&voice->cursor>=voice->buffers.front().frames) {
                    const auto count=voice->buffers.front().frames;
                    if(voice->buffers.front().loop){voice->cursor=std::fmod(voice->cursor,double(count));break;}
                    voice->cursor-=count;voice->buffers.pop_front();
                }
                if(voice->buffers.empty())voice->cursor=0;
            }
        }
        for(size_t sample=0;sample<frames*2;++sample)destination[sample]=std::clamp(destination[sample],-1.f,1.f);
        audioProcessor.process(destination,frames,currentAudioMode());
    }
private:
    static std::atomic<StorylandAudioMode>& preferredAudioMode() {
        static std::atomic<StorylandAudioMode> value{StorylandAudioMode::Ps2};
        return value;
    }
    static std::mutex& controlMutex() {static std::mutex control;return control;}
    static std::weak_ptr<StorylandAudioMixer>& activeDevice() {
        static std::weak_ptr<StorylandAudioMixer> device;return device;
    }
    static std::string& preferredDevice() {static std::string name;return name;}
    bool openOutput(const std::string& name,std::string& error) {
        // Closing waits for the old callback. Do not hold the voice mutex here.
        if(output) {SDL_CloseAudioDevice(output);output=0;}
#ifdef _WIN32
        const char* driver=SDL_GetCurrentAudioDriver();
        if(!driver||std::strcmp(driver,"dummy")==0||std::strcmp(driver,"disk")==0||
            std::strcmp(driver,"null")==0) {
            error="SDL selected an inaudible audio driver.";return false;
        }
#endif
        const uint64_t previousCallbacks=callbackCount.load(std::memory_order_acquire);
        SDL_AudioSpec desired{};
        desired.freq=48000;desired.format=AUDIO_F32SYS;desired.channels=2;desired.samples=512;
        desired.callback=callback;desired.userdata=this;
        output=SDL_OpenAudioDevice(name.empty()?nullptr:name.c_str(),0,&desired,nullptr,0);
        if(!output){error="Could not open "+(name.empty()?std::string("the default audio output"):name)+": "+SDL_GetError();return false;}
        SDL_PauseAudioDevice(output,0);
        const Uint32 started=SDL_GetTicks();
        while(callbackCount.load(std::memory_order_acquire)==previousCallbacks&&Uint32(SDL_GetTicks()-started)<300u)
            SDL_Delay(1);
        if(callbackCount.load(std::memory_order_acquire)==previousCallbacks) {
            error="The SDL audio device opened but its output callback did not start.";
            SDL_CloseAudioDevice(output);output=0;return false;
        }
        openedDevice=name;error.clear();return true;
    }
    static void callback(void* context,Uint8* bytes,int length) {
        auto& device=*static_cast<StorylandAudioMixer*>(context);
        const size_t frames=size_t(length)/(2*sizeof(float));
        float* samples=reinterpret_cast<float*>(bytes);
        device.mix(samples,frames);
        float peak=0;double energy=0;
        for(size_t index=0;index<frames*2;++index) {
            peak=std::max(peak,std::fabs(samples[index]));energy+=double(samples[index])*samples[index];
        }
        device.lastPeak=peak;
        device.lastRms=frames?float(std::sqrt(energy/double(frames*2))):0;
        device.callbackCount.fetch_add(1,std::memory_order_release);
    }
    std::atomic<uint64_t> callbackCount{0};
    std::atomic<float> lastPeak{0},lastRms{0};
    std::string openedDevice;
    SDL_AudioDeviceID output=0;
    bool initialized=false;
    std::vector<std::shared_ptr<Voice>> voices;
};
