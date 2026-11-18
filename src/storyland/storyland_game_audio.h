#pragma once
#include "src/storyland/storyland_media.h"
#include <atomic>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <objbase.h>
#endif

struct StorylandGameAudioTicket {
    std::atomic<bool> loaded{false}, finished{false}, failed{false};
    std::atomic<bool> requested{false}, cancelled{false};
    std::atomic<bool> started{false}, paused{false};
    std::string diagnostic;
    std::atomic<float> gain{1.0f},pitch{1.0f};
    std::atomic<uint32_t> durationMilliseconds{0};
    std::atomic<uint32_t> playbackMilliseconds{0};
    std::atomic<bool> playbackPositionValid{false};
    bool looping=false;
    StorylandFrontendAmbience frontendAmbience=StorylandFrontendAmbience::None;
};

class StorylandGameAudio {
    struct Request {
        std::wstring path;
        size_t sample=0;
        bool bank=false;
        std::shared_ptr<StorylandGameAudioTicket> ticket;
    };
    struct Voice {
        std::unique_ptr<StorylandMediaFile> player;
        std::shared_ptr<StorylandGameAudioTicket> ticket;
        bool started=false;
    };
    std::mutex mutex;
    std::condition_variable wake,decodeWake;
    std::deque<Request> requests;
    std::deque<Voice> readyVoices;
    std::vector<Voice> voices;
    bool stopping=false;
    std::thread worker,decoder;

    void decode() {
#ifdef _WIN32
        const HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
#endif
        for(;;) {
            Request request;
            {
                std::unique_lock<std::mutex> lock(mutex);
                decodeWake.wait(lock,[&]{return stopping||!requests.empty();});
                if(stopping)break;
                request=std::move(requests.front());requests.pop_front();
            }
            if(request.ticket->cancelled) {
                request.ticket->finished=true;
                continue;
            }
            std::unique_ptr<StorylandMediaFile> player;
            try {
                player=std::make_unique<StorylandMediaFile>();
                std::string error;
                const bool loaded=request.bank?
                    player->loadSdtClip(request.path,request.sample,error,64u*1024u*1024u):
                    player->loadFromFile(request.path,error);
                if(loaded&&!request.ticket->cancelled) {
                    player->setFrontendAmbience(request.ticket->frontendAmbience);
                    if(!player->clips().empty())request.ticket->durationMilliseconds=
                        uint32_t(std::max(0.0,player->clips().front().durationSeconds)*1000.0);
                    std::unique_lock<std::mutex> lock(mutex);
                    decodeWake.wait(lock,[&]{return stopping||readyVoices.size()<32;});
                    if(stopping||request.ticket->cancelled) {
                        request.ticket->cancelled=true;request.ticket->finished=true;
                    } else {
                        readyVoices.push_back({std::move(player),request.ticket,false});
                        request.ticket->loaded=true;
                    }
                    lock.unlock();wake.notify_one();
                } else {
                    request.ticket->diagnostic=error;
                    request.ticket->failed=!request.ticket->cancelled;
                    request.ticket->finished=true;
                }
            } catch(const std::exception& exception) {
                request.ticket->diagnostic=exception.what();
                request.ticket->failed=!request.ticket->cancelled;request.ticket->finished=true;
            } catch(...) {
                request.ticket->diagnostic="Audio decoding failed.";
                request.ticket->failed=!request.ticket->cancelled;request.ticket->finished=true;
            }
        }
#ifdef _WIN32
        if(SUCCEEDED(initialized))CoUninitialize();
#endif
    }

    void run() {
#ifdef _WIN32
        const HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
#endif
        for(;;) {
            {
                std::unique_lock<std::mutex> lock(mutex);
                if(voices.empty())wake.wait(lock,[&]{return stopping||!readyVoices.empty();});
                else wake.wait_for(lock,std::chrono::milliseconds(10),[&]{return stopping||!readyVoices.empty();});
                if(stopping)break;
                while(!readyVoices.empty()) {
                    voices.push_back(std::move(readyVoices.front()));readyVoices.pop_front();
                }
            }
            decodeWake.notify_one();
            // File reads and decoding run separately. They must not hold up pitch,
            // gain, pause or cancellation updates on voices already playing.
            for(auto iterator=voices.begin();iterator!=voices.end();) {
                auto& voice=*iterator;
                bool remove=voice.ticket->cancelled;
                if(!remove&&voice.ticket->requested&&!voice.ticket->paused&&!voice.started) {
                    std::string error;
                    voice.player->setPlaybackLooping(voice.ticket->looping);
                    voice.player->setPlaybackPitch(voice.ticket->pitch);
                    voice.player->setPlaybackGain(voice.ticket->gain);
                    voice.started=voice.player->play(error);
                    voice.ticket->started = voice.started;
                    if (!voice.started) {
                        voice.ticket->diagnostic = error;
                        voice.ticket->failed = true;
                        remove = true;
                    }
                }
                if(voice.started && !remove) {
                    voice.player->setPlaybackPaused(voice.ticket->paused);
                    voice.player->setPlaybackPitch(voice.ticket->pitch);
                    voice.player->setPlaybackGain(voice.ticket->gain);
                    uint32_t position=0;
                    if(voice.player->playbackPositionMilliseconds(position)) {
                        voice.ticket->playbackMilliseconds=std::max(voice.ticket->playbackMilliseconds.load(),position);
                        voice.ticket->playbackPositionValid=true;
                    }
                    if(!voice.player->isPlaying()) {
                        const auto error=voice.player->radioPlaybackError();
                        if(!error.empty()) {voice.ticket->diagnostic=error;voice.ticket->failed=true;}
                        remove=true;
                    }
                }
                if(remove) {voice.ticket->finished=true;iterator=voices.erase(iterator);}
                else ++iterator;
            }
        }
        for(auto& voice:voices) {voice.ticket->cancelled=true;voice.ticket->finished=true;}
        voices.clear();
#ifdef _WIN32
        if(SUCCEEDED(initialized))CoUninitialize();
#endif
    }
public:
    StorylandGameAudio() {
        worker=std::thread([this]{run();});
        try {decoder=std::thread([this]{decode();});}
        catch(...) {
            {std::lock_guard<std::mutex> lock(mutex);stopping=true;}
            wake.notify_all();worker.join();throw;
        }
    }
    ~StorylandGameAudio() {
        {std::lock_guard<std::mutex> lock(mutex);stopping=true;}
        wake.notify_all();decodeWake.notify_all();
        if(decoder.joinable())decoder.join();
        if(worker.joinable())worker.join();
        for(auto& request:requests) {request.ticket->cancelled=true;request.ticket->finished=true;}
        for(auto& voice:readyVoices) {voice.ticket->cancelled=true;voice.ticket->finished=true;}
    }
    std::shared_ptr<StorylandGameAudioTicket> submit(const std::wstring& path,size_t sample,
        bool bank,float gain,bool playImmediately,bool looping=false,bool priority=false,
        StorylandFrontendAmbience ambience=StorylandFrontendAmbience::None) {
        auto ticket=std::make_shared<StorylandGameAudioTicket>();
        ticket->gain=gain;ticket->requested=playImmediately;ticket->looping=looping;
        ticket->frontendAmbience=ambience;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for(auto it=requests.begin();it!=requests.end();) {
                if(it->ticket->cancelled) {it->ticket->finished=true;it=requests.erase(it);}
                else ++it;
            }
            if (stopping || requests.size() >= (priority?33u:32u)) {
                ticket->diagnostic = stopping ? "Audio worker is stopping." : "Audio request queue is full.";
                ticket->failed = true;
                ticket->finished = true;
                return ticket;
            }
            if(priority)requests.push_front({path,sample,bank,ticket});
            else requests.push_back({path,sample,bank,ticket});
        }
        decodeWake.notify_one();return ticket;
    }
};
