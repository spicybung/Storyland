#pragma once

#include "src/storyland/storyland_native_speech.h"
#include "src/storyland/storyland_native_random.h"
#include "src/re/reVCS/audio/storyland_vcs_speech_audio.h"
#include "src/re/reLCS2/audio/storyland_lcs_script_speech.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

constexpr uint16_t StorylandNoCharacterSpeech=0xEF;
constexpr uint64_t StorylandPlayerSpeechIdentity=1;

inline uint64_t storylandNewCharacterSpeechIdentity() {
    static std::atomic<uint64_t> next{2};
    return next.fetch_add(1,std::memory_order_relaxed);
}

struct StorylandCharacterSpeechTiming {
    uint32_t nextDelay,nextRandom,repeatDelay,repeatRandom,sharedDelay;
};
inline constexpr std::array<StorylandCharacterSpeechTiming,58> StorylandVcsCharacterSpeechTiming={{
#include "src/re/reVCS/audio/storyland_vcs_speech_timing.inc"
}};

struct StorylandCharacterSpeechQueue {
    uint16_t pending=StorylandNoCharacterSpeech,lastEvent=StorylandNoCharacterSpeech;
    uint32_t lastStarted=0,nextAllowed=0;
};
struct StorylandLcsPlayerSpeechQueue {
    uint16_t pending=StorylandLcsNoSpeechEvent,lastEvent=StorylandLcsNoSpeechEvent;
    uint32_t lastStarted=0,nextAllowed=0;
};
struct StorylandCharacterSpeechGlobal {
    std::array<uint32_t,58> nextAllowed{};
    uint32_t random=1;
    uint32_t nextRandom() { return storylandNativeRandom(random); }
};
struct StorylandCharacterSpeechState : StorylandSpeechState {
    StorylandCharacterSpeechQueue queue;
    StorylandLcsPlayerSpeechQueue lcsQueue;
    int pendingAudioSample=-1;
    uint16_t pendingAudioEvent=StorylandNoCharacterSpeech;
    uint32_t pendingAudioFrequency=0;
};
struct StorylandCharacterSpeechService {
    bool suppressedAnimation=false,suppressedPedType=false,burning=false;
    bool fireSpeechDisabled=false,player=false;
};
struct StorylandCharacterSpeechCamera {
    std::array<float,3> previous{};
    float accumulatedDistance=0,averageDisplacement=0;
    unsigned updates=0;
    void update(float x,float y,float z) {
        const float dx=x-previous[0],dy=y-previous[1],dz=z-previous[2];
        accumulatedDistance+=std::sqrt(dx*dx+dy*dy+dz*dz);
        previous={x,y,z};
        if(++updates==4) {
            averageDisplacement=accumulatedDistance/float(updates);
            accumulatedDistance=0;updates=0;
        }
    }
};

inline bool storylandVcsCharacterSpeechCameraAllows(uint16_t event,float displacement) {
    if(event<0x66||event>0x9F)return false;
    if(displacement>1.65f)return (event>=0x67&&event<0x6A)||event==0x9E;
    if(displacement>1.25f)return event<0x6A||event==0x88||event==0x8D||event==0x9E;
    if(displacement>.9f) {
        switch(event) {
        case 0x66:case 0x67:case 0x68:case 0x69:case 0x6B:case 0x77:
        case 0x88:case 0x8D:case 0x8F:case 0x90:case 0x91:case 0x9E:return true;
        default:return false;
        }
    }
    return true;
}
inline uint32_t storylandCharacterSpeechJitter(uint32_t random,uint32_t bound) {
    return uint32_t(uint64_t(random&0x7FFFFFFFu)*bound>>31);
}
template<class Random>
bool storylandQueueVcsCharacterSpeech(StorylandCharacterSpeechQueue& queue,uint16_t event,
    uint32_t now,float pedZ,float cameraZ,float displacement,Random&& random) {
    if(!storylandVcsCharacterSpeechCameraAllows(event,displacement)||pedZ>cameraZ+3.0f||event>=queue.pending)
        return false;
    if(event==queue.lastEvent&&event!=0x66) {
        const auto& timing=StorylandVcsCharacterSpeechTiming[event-0x66];
        const uint32_t threshold=queue.lastStarted+timing.repeatDelay+
            storylandCharacterSpeechJitter(random(),timing.repeatRandom);
        if(now<threshold)return false;
    }
    queue.pending=event;return true;
}
template<class Random>
int storylandServiceVcsCharacterSpeech(StorylandCharacterSpeechQueue& queue,
    StorylandCharacterSpeechGlobal& global,uint32_t now,const StorylandCharacterSpeechService& context,Random&& random) {
    if(context.suppressedAnimation||context.suppressedPedType)return -1;
    if(context.burning&&!context.fireSpeechDisabled)queue.pending=0x6B;
    if(queue.pending==StorylandNoCharacterSpeech)return -1;
    if(queue.pending==0x66)queue.nextAllowed=now-1u;
    if(!(queue.nextAllowed<now))return -1;
    const uint16_t event=queue.pending;
    queue.pending=StorylandNoCharacterSpeech;
    if(event<0x66||event>0x9F)return -1;
    const size_t index=event-0x66;
    if(!(global.nextAllowed[index]<now))return -1;
    const auto& timing=StorylandVcsCharacterSpeechTiming[index];
    queue.lastStarted=now;
    queue.nextAllowed=now+timing.nextDelay+storylandCharacterSpeechJitter(random(),timing.nextRandom);
    if(event==0x7F&&context.player)queue.nextAllowed+=2000;
    global.nextAllowed[index]=now+timing.sharedDelay;
    queue.lastEvent=event;return event;
}
inline int storylandCharacterDamageSpeech(bool vcs,int32_t weapon,bool fatal) {
    if(fatal)return int(StorylandSpeechEvent::Death);
    return weapon==(vcs?47:44)?0x69:0;
}
inline bool storylandCharacterSpeechAnimationSuppressed(int32_t animation) {
    return animation==0x135||animation==0x139||animation==0x137;
}

inline const StorylandCharacterSpeechTiming* storylandLcsPlayerSpeechTiming(uint16_t event) {
    static constexpr StorylandCharacterSpeechTiming ordinary{500,800,500,2,1000};
    static constexpr StorylandCharacterSpeechTiming death{500,800,500,2,3000};
    static constexpr StorylandCharacterSpeechTiming bullet{100,2,100,2,1000};
    static constexpr StorylandCharacterSpeechTiming burning{1500,500,1500,1000,3000};
    static constexpr StorylandCharacterSpeechTiming defend{200,200,200,200,1000};
    switch(event) {
    case 0x67:return &death;
    case 0x68:case 0x69:case 0x6A:return &ordinary;
    case 0x6B:return &bullet;
    case 0x6C:return &burning;
    case 0x8B:return &defend;
    default:return nullptr;
    }
}

inline const StorylandSpeechRange* storylandLcsPlayerSpeechRange(uint16_t event) {
    // GetPlayerTalkSfx 0x1A9238 uses absolute sample IDs, including NO_SAMPLE for death.
    static constexpr StorylandSpeechRange ranges[]={
        {0,0x68,0,4797,16}, {0,0x69,0,4813,15}, {0,0x6A,0,4785,12},
        {0,0x6B,0,4797,16}, {0,0x6C,0,4828,7}, {0,0x8B,0,4813,15}
    };
    for(const auto& range:ranges)if(range.event==event)return &range;
    return nullptr;
}

inline bool storylandLcsPlayerSpeechCameraAllows(uint16_t event,float displacement) {
    if(!storylandLcsPlayerSpeechTiming(event))return false;
    if(displacement>1.65f)return event>=0x68&&event<=0x6A;
    if(displacement>1.25f)return event>=0x67&&event<=0x6A;
    if(displacement>.9f)return event<=0x6A||event==0x6C;
    return true;
}

template<class Random>
bool storylandQueueLcsPlayerSpeech(StorylandLcsPlayerSpeechQueue& queue,uint16_t event,
    uint32_t now,float pedZ,float cameraZ,float displacement,Random&& random) {
    if(!storylandLcsPlayerSpeechCameraAllows(event,displacement)||pedZ>cameraZ+3.0f||event>=queue.pending)
        return false;
    if(event==queue.lastEvent&&event!=0x67) {
        const auto& timing=*storylandLcsPlayerSpeechTiming(event);
        const uint32_t threshold=queue.lastStarted+timing.repeatDelay+
            storylandCharacterSpeechJitter(random(),timing.repeatRandom);
        if(now<threshold)return false;
    }
    queue.pending=event;
    return true;
}

template<class Random>
int storylandServiceLcsPlayerSpeech(StorylandLcsPlayerSpeechQueue& queue,
    StorylandCharacterSpeechGlobal& global,uint32_t now,const StorylandCharacterSpeechService& context,
    Random&& random) {
    if(context.suppressedPedType)return -1;
    if(context.burning&&!context.fireSpeechDisabled)queue.pending=0x6C;
    if(queue.pending==StorylandLcsNoSpeechEvent)return -1;
    if(queue.pending==0x67)queue.nextAllowed=now-1u;
    if(!(queue.nextAllowed<now))return -1;
    const uint16_t event=queue.pending;
    queue.pending=StorylandLcsNoSpeechEvent;
    const auto* timing=storylandLcsPlayerSpeechTiming(event);
    if(!timing)return -1;
    const size_t index=event-0x67;
    if(!(global.nextAllowed[index]<now))return -1;
    queue.lastStarted=now;
    queue.nextAllowed=now+timing->nextDelay+storylandCharacterSpeechJitter(random(),timing->nextRandom);
    global.nextAllowed[index]=now+timing->sharedDelay;
    queue.lastEvent=event;
    return event;
}
