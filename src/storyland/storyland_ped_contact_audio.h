#pragma once

#include "src/storyland/storyland_native_audio.h"
#include "src/re/reVCS/audio/storyland_vcs_ped_audio.h"
#include <tuple>

inline bool storylandIsPedContactSound(const StorylandPlayTestSoundEvent& event) {
    return event.nativeSpeech<0&&(event.type==StorylandPlayTestSound::Footstep||
        event.type==StorylandPlayTestSound::Land);
}

inline const StorylandNativeSoundSample* storylandVcsPedContactSample(
    const StorylandNativeAudioCatalog& catalog,unsigned bank,size_t slot,bool psp) {
    if(!psp)return catalog.vcsSample(bank,slot,false);
    // PSP bank zero has a different layout. Match its embedded VAG family,
    // then select a variation within that family without using PS2 slot IDs.
    const char* family=nullptr;
    size_t variation=slot;
    switch(bank) {
    case 0:
        if(slot>=157&&slot<=161) {family="footstepcon";variation=slot-157;}
        else if(slot>=121&&slot<=123) {family="colveg";variation=slot-121;}
        break;
    case 61:family="footstepgras";break;
    case 62:family="footstepgrav";break;
    case 63:family="footstepwoo";break;
    case 64:family="footstepmet";break;
    case 65:family="footstepwat";break;
    case 66:family="footstepsan";break;
    }
    if(!family)return nullptr;
    const std::string prefix=family,sfxPrefix="sfx"+prefix;
    std::vector<const StorylandNativeSoundSample*> matches;
    for(const auto& sample:catalog.samples) {
        if(!sample.psp)continue;
        if(sample.key.compare(0,prefix.size(),prefix)==0||
            sample.key.compare(0,sfxPrefix.size(),sfxPrefix)==0)matches.push_back(&sample);
    }
    std::sort(matches.begin(),matches.end(),[](const auto* first,const auto* second) {
        return std::tie(first->bankPath,first->slot)<std::tie(second->bankPath,second->slot);
    });
    return matches.empty()?nullptr:matches[variation%matches.size()];
}

struct StorylandPedContactAudio {
    StorylandVcsPedContactSound native;
    const StorylandNativeSoundSample* sample=nullptr;
    float pitch=1;
};

inline StorylandPedContactAudio storylandResolveVcsPedContactAudio(
    const StorylandNativeAudioCatalog& catalog,const StorylandPlayTestSoundEvent& event,
    uint8_t playerSurface,bool psp,StorylandVcsAudioRandom& random) {
    StorylandPedContactAudio result;
    if(!storylandIsPedContactSound(event))return result;
    const uint16_t nativeEvent=event.nativePedAudioEvent>=33&&event.nativePedAudioEvent<=36?
        uint16_t(event.nativePedAudioEvent):event.type==StorylandPlayTestSound::Land?35:33;
    unsigned loadedBank=storylandVcsPedSurfaceAudioBank(playerSurface);
    if(loadedBank&&!storylandVcsPedContactSample(catalog,loadedBank,0,psp))loadedBank=0;
    result.native=storylandPrepareVcsPedContactSound(nativeEvent,event.surface,event.speaker<0,
        loadedBank,random);
    result.sample=storylandVcsPedContactSample(catalog,result.native.bank,
        size_t(result.native.sample),psp);
    if(result.sample) {
        storylandFinishVcsPedContactSound(result.native,event.nativePedMoveState,
            result.sample->frequency,random);
        if(result.sample->frequency)
            result.pitch=float(result.native.frequency)/float(result.sample->frequency);
    }
    return result;
}

struct StorylandPlayerDeathAudio {
    const StorylandNativeSoundSample* sample=nullptr;
    int nativeSample=-1;
    uint32_t frequency=0;
    uint8_t volume=0;
    float pitch=1;
};

inline bool storylandIsPlayerDeathSound(const StorylandPlayTestSoundEvent& event,bool vcs) {
    return event.speaker<0&&event.nativeSpeech<0&&event.nativePedAudioEvent==(vcs?205:204);
}

inline StorylandPlayerDeathAudio storylandResolvePlayerDeathAudio(
    const StorylandNativeAudioCatalog& catalog,bool vcs,bool psp,
    StorylandVcsAudioRandom& random,uint8_t& lcsVariation) {
    StorylandPlayerDeathAudio result;
    result.nativeSample=vcs?225:237+(lcsVariation&1u);
    if(!vcs)lcsVariation=uint8_t((lcsVariation+1u)&1u);
    result.frequency=uint32_t(16000+random.displacement(6000));
    result.volume=uint8_t(55u+random.words[4]%20u);
    if(!psp)result.sample=vcs?catalog.vcsSample(0,size_t(result.nativeSample),false):
        catalog.lcsSample(uint32_t(result.nativeSample));
    if(result.sample&&result.sample->frequency)
        result.pitch=float(result.frequency)/float(result.sample->frequency);
    return result;
}

inline float storylandPlayerDeathSoundGain(const StorylandPlayerDeathAudio& sound,float distance) {
    return float(storylandRetailSoundVolume(sound.volume,40.0f,distance))/127.0f;
}
