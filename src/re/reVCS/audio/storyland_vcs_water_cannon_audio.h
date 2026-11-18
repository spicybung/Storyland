#pragma once

#include "src/storyland/storyland_native_audio.h"
#include "src/storyland/storyland_script_sound.h"

// VCS PS2 0x1C3B70 services the three water-cannon slots. The hose voice
// uses SFX0 sample 179, 15591 Hz, volume 100 and a sixty-unit range.
inline const StorylandNativeSoundSample* storylandVcsWaterCannonSample(
    const StorylandNativeAudioCatalog& catalog,bool psp) {
    if(!psp) {
        const auto* sample=catalog.vcsSample(0,179,false);
        return sample&&sample->key=="hose"?sample:nullptr;
    }
    const StorylandNativeSoundSample* selected=nullptr;
    for(const auto& sample:catalog.samples) {
        if(sample.nativeVcsBank!=0||!sample.psp||sample.key!="hose")continue;
        if(selected)return nullptr;
        selected=&sample;
    }
    return selected;
}

inline float storylandVcsWaterCannonGain(float distance) {
    StorylandScriptSoundDefinition sound;
    sound.positioned=true;sound.range=60.0f;sound.volume=100.0f/127.0f;
    return storylandScriptSoundGain(sound,distance);
}

inline float storylandVcsWaterCannonPitch(uint32_t sampleFrequency) {
    return sampleFrequency?15591.0f/float(sampleFrequency):1.0f;
}
