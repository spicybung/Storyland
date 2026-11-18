#pragma once

#include "src/storyland/storyland_native_audio.h"
#include "src/re/reVCS/audio/storyland_vcs_audio_random.h"

struct StorylandVehicleDoorAudio {
    const StorylandNativeSoundSample* sample=nullptr;
    uint8_t volume=0;
    uint32_t frequency=0;
    float pitch=1;
};

inline StorylandVehicleDoorAudio storylandResolveVehicleDoorAudio(
    const StorylandNativeAudioCatalog& catalog,const StorylandVehicleDefinition& vehicle,
    bool opening,bool psp,StorylandVcsAudioRandom& random) {
    StorylandVehicleDoorAudio sound;
    sound.sample=catalog.vehicleDoorSound(vehicle,opening,psp);
    if(!sound.sample)return sound;
    sound.volume=uint8_t(opening?117u+random.words[1]%10u:122u+random.words[2]%5u);
    const bool helicopter=vehicle.nativeType==(vehicle.vcsModelLayout?4:3);
    sound.frequency=helicopter?(opening?23459u:28062u):sound.sample->frequency;
    if(sound.frequency) {
        sound.frequency=uint32_t(int64_t(sound.frequency)+random.displacement(sound.frequency/32u));
        if(sound.sample->frequency)sound.pitch=float(sound.frequency)/float(sound.sample->frequency);
    }
    return sound;
}

inline float storylandVehicleDoorSoundGain(const StorylandVehicleDoorAudio& sound,float distance) {
    return float(storylandRetailSoundVolume(sound.volume,StorylandRetailDoorRange,distance))/127.0f;
}
