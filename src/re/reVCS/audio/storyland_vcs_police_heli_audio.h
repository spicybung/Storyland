#pragma once

#include "src/storyland/storyland_native_audio.h"
#include "src/storyland/storyland_script_sound.h"

// Original VCS 0x1B66BC..0x1B68A0: distant generic-helicopter rotor layer.
inline const StorylandNativeSoundSample* storylandVcsPoliceHeliDistantSample(
    const StorylandNativeAudioCatalog& catalog,bool psp) {
    if(!psp) {
        const auto* sample=catalog.vcsSample(0,178,false);
        return sample&&sample->key=="sfxheli1wav"?sample:nullptr;
    }
    const StorylandNativeSoundSample* selected=nullptr;
    for(const auto& sample:catalog.samples) {
        if(sample.nativeVcsBank!=0||!sample.psp||sample.key!="sfxheli1wav")continue;
        if(selected)return nullptr;
        selected=&sample;
    }
    return selected;
}

inline float storylandVcsPoliceHeliDistantGain(float rotorSpeed,float distance) {
    if(!std::isfinite(rotorSpeed)||!std::isfinite(distance)||distance<25.0f)return 0;
    const float rotor=std::clamp(rotorSpeed*4.5454545f,0.0f,1.0f);
    const float nativeVolume=distance<40.0f?(distance-25.0f)*rotor*5.0000005f:rotor*75.0f;
    StorylandScriptSoundDefinition sound;
    sound.positioned=true;sound.range=250.0f;
    sound.volume=float(uint8_t(std::clamp(nativeVolume,0.0f,127.0f)))/127.0f;
    return storylandScriptSoundGain(sound,distance);
}
