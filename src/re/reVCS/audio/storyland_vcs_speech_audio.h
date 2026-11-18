#pragma once
#include "src/re/reVCS/audio/storyland_vcs_audio_random.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

// SLUS 1C3050: ten most recently admitted comment samples share one ring.
struct StorylandVcsSpeechAudio {
    std::array<int32_t,10> recent{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
    std::size_t cursor=0;
    bool blocked(int32_t sample,uint16_t event) const {
        const bool bypass=event>=0x67&&event<=0x69;
        return !bypass&&std::find(recent.begin(),recent.end(),sample)!=recent.end();
    }
    bool admit(int32_t sample,uint16_t event,uint32_t frequency,bool ready,bool suppressed,
        StorylandVcsAudioRandom& random,float& pitch) {
        if(sample<0||!frequency||!ready||suppressed||blocked(sample,event))return false;
        const int32_t adjusted=int32_t(frequency)+random.displacement(750);
        pitch=float(adjusted)/float(frequency);
        if(!(event>=0x67&&event<=0x69)) {
            recent[cursor]=sample;cursor=(cursor+1)%recent.size();
        }
        return true;
    }
};

// SLUS_214.23 0x1B11A8 uses the same ten-sample ring and +/-750 Hz
// displacement. Its bypass events are 0x68..0x6A, one above VCS.
struct StorylandLcsSpeechAudio : StorylandVcsSpeechAudio {
    bool blocked(int32_t sample,uint16_t event) const {
        return StorylandVcsSpeechAudio::blocked(sample,uint16_t(event-1));
    }
    bool admit(int32_t sample,uint16_t event,uint32_t frequency,bool ready,bool suppressed,
        StorylandVcsAudioRandom& random,float& pitch) {
        return StorylandVcsSpeechAudio::admit(sample,uint16_t(event-1),frequency,ready,suppressed,random,pitch);
    }
};
