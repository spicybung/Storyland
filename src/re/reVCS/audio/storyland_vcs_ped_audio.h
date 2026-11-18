#pragma once

#include "src/re/reVCS/audio/storyland_vcs_audio_random.h"
#include "src/storyland/storyland_vehicle_audio.h"
#include <cstdint>

struct StorylandVcsPedContactSound {
    int sample=-1;
    unsigned bank=0;
    uint32_t frequency=0;
    uint8_t volume=0;
    uint8_t counter=0;
    bool active=false;
};

inline unsigned storylandVcsPedSurfaceAudioBank(uint8_t surface) {
    // SLUS_215.90 0x1C02E4..0x1C047C and its table at0x4A89B0.
    switch(surface) {
    case 2:return 61;
    case 3:case 4:return 62;
    case 6:case 9:case 10:case 11:case 12:case 13:case 14:case 15:
    case 16:case 27:case 28:return 64;
    case 18:case 33:return 66;
    case 19:return 65;
    case 20:case 21:case 22:return 63;
    default:return 0;
    }
}

inline StorylandVcsPedContactSound storylandPrepareVcsPedContactSound(uint16_t event,
    uint8_t surface,bool player,unsigned loadedSurfaceBank,const StorylandVcsAudioRandom& random,
    bool inAir=false) {
    StorylandVcsPedContactSound sound;
    if(event<33||event>36||inAir)return sound;
    sound.active=true;
    sound.counter=uint8_t(event-32);
    sound.volume=uint8_t(80u+random.words[3]%15u);
    if(!player)sound.volume>>=1;
    sound.sample=157+int(random.words[2]%5u);
    const unsigned surfaceBank=storylandVcsPedSurfaceAudioBank(surface);
    if(surface==19)sound.volume=uint8_t(30u+random.words[3]%15u);
    if(surface==25) {
        sound.sample=121+int(random.words[2]%3u);
    } else if(surfaceBank&&surfaceBank==loadedSurfaceBank) {
        sound.bank=surfaceBank;
        switch(surfaceBank) {
        case 61:sound.sample=int(random.words[1]&3u);break;
        case 62:sound.sample=int(random.words[4]%5u);break;
        case 63:sound.sample=int(random.words[2]%5u);break;
        case 64:sound.sample=int(random.words[0]%5u);break;
        case 65:sound.sample=int(random.words[3]&3u);break;
        case 66:sound.sample=int(random.words[4]&3u);break;
        }
    }
    return sound;
}

inline void storylandFinishVcsPedContactSound(StorylandVcsPedContactSound& sound,
    uint8_t movementState,uint32_t sampleFrequency,StorylandVcsAudioRandom& random) {
    if(!sound.active||!sampleFrequency)return;
    // Native0x1C04A8..0x1C0598 shares the manager's pitch selector with speech,
    // frontend and vehicle one-shots before applying the ped movement scale.
    sound.frequency=uint32_t(int64_t(sampleFrequency)+random.displacement(sampleFrequency/17u));
    switch(movementState) {
    case 2:case 6:
        sound.frequency=uint32_t(uint64_t(sound.frequency)*9u/10u);
        sound.volume>>=2;
        break;
    case 4:
        sound.frequency=uint32_t(uint64_t(sound.frequency)*11u/10u);
        sound.volume>>=1;
        break;
    case 5:
        sound.frequency=uint32_t(uint64_t(sound.frequency)*12u/10u);
        break;
    }
}

inline float storylandVcsPedContactGain(const StorylandVcsPedContactSound& sound,float distance) {
    return sound.active?float(storylandRetailSoundVolume(sound.volume,20.0f,distance))/127.0f:0;
}
