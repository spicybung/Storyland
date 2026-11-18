#pragma once

#include "src/re/reVCS/audio/storyland_vcs_audio_random.h"

#include <cstdint>

struct StorylandVcsSuspensionSound {
    bool admitted=false;
    int32_t sample=-1;
    int32_t frequency=0;
    int32_t volume=0;
    int32_t priority=0;
    float range=0;
};

inline StorylandVcsSuspensionSound storylandVcsSuspensionSound(
    uint16_t event, StorylandVcsAudioRandom& random) {
    if(event!=20&&event!=21)return {};
    const int32_t baseFrequency=event==20?15600:13118;
    StorylandVcsSuspensionSound sound;
    sound.admitted=true;
    sound.sample=270;
    sound.frequency=baseFrequency+random.displacement(uint32_t(baseFrequency)>>3);
    sound.volume=55+int32_t(random.words[0]%15);
    sound.priority=51;
    sound.range=35;
    return sound;
}
