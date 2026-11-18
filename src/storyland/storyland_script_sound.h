#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>
#include "src/re/reVCS/audio/storyland_vcs_audio_random.h"
#include "src/re/reVCS/audio/storyland_vcs_suspension_audio.h"

struct StorylandScriptSoundDefinition {
    int sample=-1;
    unsigned bank=0;
    float range=40.0f;
    float volume=70.0f/127.0f;
    float frequency=0;
    bool positioned=true;
    bool nativeSilent=false;
    // LCS VAG names are independent of the VCS bank/slot numbering.
    const char* sampleName=nullptr;
};

inline bool storylandVcsFrontendUsesAudioRandom(uint16_t id) {
    return id==185||id==186||id==195;
}

inline StorylandScriptSoundDefinition storylandVcsPrepareSharedFrontendSound(
    uint16_t id,StorylandVcsAudioRandom& random,float frontendParameter=0) {
    StorylandScriptSoundDefinition sound;
    sound.positioned=false;
    if(id==185||id==186) {
        sound.sample=28+int(random.words[0]&1u);
        // 1C5A3C converts the queued parameter to an integer and stores its low byte.
        // Valid near-miss producers supply a finite value between 0 and 127.
        const float parameter=std::isfinite(frontendParameter)?std::clamp(frontendParameter,0.0f,255.0f):0.0f;
        sound.volume=float(uint8_t(parameter))/127.0f;
    } else if(id==195) {
        uint32_t sample=random.previousFrontendRadioSample+((random.words[1]&1u)?1u:2u);
        if(sample>=239)sample-=3;
        random.previousFrontendRadioSample=sample;
        sound.sample=int(sample);
        sound.volume=103.0f/127.0f;
    } else sound.nativeSilent=true;
    return sound;
}

inline void storylandVcsFinishSharedFrontendSound(StorylandScriptSoundDefinition& sound,
    uint16_t id,StorylandVcsAudioRandom& random,uint32_t sampleFrequency) {
    if(!storylandVcsFrontendUsesAudioRandom(id))return;
    const uint32_t range=id==195?1500u:sampleFrequency>>5;
    sound.frequency=float(int64_t(sampleFrequency)+random.displacement(range));
}

inline StorylandScriptSoundDefinition storylandVcs039aVehicleSound(StorylandVcsAudioRandom& random) {
    StorylandScriptSoundDefinition sound;
    sound.bank=0;sound.sample=255;sound.range=35.0f;
    sound.volume=float(55u+random.words[1]%15u)/127.0f;
    sound.frequency=float(12000+random.displacement(1500));
    return sound;
}

inline StorylandScriptSoundDefinition storylandLcsVehicleRockingSound() {
    // SLUS_214.23 1A4594 dispatches vehicle event 23 to SFX0:274
    // (SFX_SHAG_SUSPENS). Audio-manager pitch/volume variation is not
    // available in the LCS host, so use the sample rate and midpoint volume.
    StorylandScriptSoundDefinition sound;
    sound.sampleName="sfxshagsuspens";
    sound.range=35.0f;
    sound.volume=62.0f/127.0f;
    sound.frequency=12000.0f;
    return sound;
}

inline bool storylandVcsScriptSoundUsesAudioRandom(uint16_t id) {
    const uint8_t nativeId=uint8_t(id);
    return (nativeId>=36&&nativeId<=39)||(nativeId>=81&&nativeId<=85);
}

inline void storylandVcsPrepareScriptAudioRandom(StorylandScriptSoundDefinition& sound,
    uint16_t id,const StorylandVcsAudioRandom& random) {
    const uint8_t nativeId=uint8_t(id);
    if(nativeId==81||nativeId==82) sound.volume=float(80u+random.words[2]%20u)/127.0f;
    else if(nativeId==83) {
        sound.sample=95+int(random.words[3]&3u);
        sound.volume=float(70u+random.words[2]%30u)/127.0f;
    } else if(nativeId==84||nativeId==85)
        sound.volume=float(60u+random.words[2]%30u)/127.0f;
}

inline void storylandVcsApplyScriptAudioRandom(StorylandScriptSoundDefinition& sound,
    uint16_t id,StorylandVcsAudioRandom& random,uint32_t sampleFrequency) {
    // The original 1C4048/1C40C8/1C4134/1C42B0 branches call 2A7560
    // before their distance gate. Event 36 deliberately makes two calls.
    switch(uint8_t(id)) {
    case 36: {
        const int32_t first=random.displacement(300);
        const int32_t second=random.displacement(300);
        sound.frequency=float(int64_t(sampleFrequency)+first+second);
        break;
    }
    case 37:sound.frequency=float(39243+random.displacement(300));break;
    case 38:sound.frequency=float(16000+random.displacement(300));break;
    case 39:sound.frequency=float(22050+random.displacement(300));break;
    case 81:case 82:sound.frequency=float(18600+random.displacement(1500));break;
    case 83:case 84:case 85:
        sound.frequency=float(int64_t(sampleFrequency)+random.displacement(sampleFrequency>>4));break;
    }
}

inline StorylandScriptSoundDefinition storylandVcsSuspensionScriptSound(
    uint16_t event,StorylandVcsAudioRandom& random) {
    if(event==55) {
        // VCS PS2 vehicle audio dispatch 1BC6A4: POLMAV selects SFX0:196,
        // original 32000 Hz +/- the shared 1/16 sample-frequency selector.
        StorylandScriptSoundDefinition sound;
        sound.bank=0;sound.sample=196;sound.range=120.0f;sound.volume=1.0f;
        sound.frequency=float(32000+random.displacement(2000));
        return sound;
    }
    const auto native=storylandVcsSuspensionSound(event,random);
    StorylandScriptSoundDefinition sound;
    sound.nativeSilent=!native.admitted;
    sound.bank=0;sound.sample=native.sample;sound.range=native.range;
    sound.frequency=float(native.frequency);sound.volume=float(native.volume)/127.0f;
    return sound;
}

inline StorylandScriptSoundDefinition storylandVcsFrontendSound(uint16_t id) {
    StorylandScriptSoundDefinition sound;
    sound.positioned=false;sound.volume=1;
    // ProcessFrontend (0x1C55C0), switch table 0x4A9250. These
    // events use a single native sample; bank 3 starts at global sample 5819.
    switch(id) {
    case 59:sound.sample=134;break;
    case 60:sound.sample=135;break;
    case 101:sound.sample=230;break;
    case 168:sound.sample=276;break;
    case 177:sound.sample=235;sound.frequency=28509;break;
    case 178:sound.sample=235;sound.frequency=32000;break;
    case 194:sound.sample=6;break;
    case 196:sound.bank=3;sound.sample=7;break;
    case 197:sound.bank=3;sound.sample=3;break;
    case 198:sound.bank=3;sound.sample=0;break;
    case 206:sound.sample=12;break;
    default:sound.nativeSilent=true;break;
    }
    return sound;
}

inline StorylandScriptSoundDefinition storylandLcsFrontendSound(uint16_t id) {
    StorylandScriptSoundDefinition sound;
    sound.positioned=false;
    sound.volume=1.0f;
    switch(id) {
    case 196: sound.sampleName="sfxmenuback"; break;
    case 197: sound.sampleName="sfxmenustart"; break;
    case 198: sound.sampleName="sfxmenuselect"; break;
    default: sound.nativeSilent=true; break;
    }
    return sound;
}

inline StorylandScriptSoundDefinition storylandVcsScriptSound(uint16_t id,uint32_t variation=0,int surface=-1) {
    StorylandScriptSoundDefinition sound;
    // Native 0x366500 routes these IDs to the frontend audio entity.
    switch(id) {
    case 1:case 7:case 8:case 9:sound.sample=222;break;
    case 10:sound.sample=173;break;
    case 13:case 14:sound.sample=289;break;
    case 16:sound.sample=6;break;
    case 70:sound.bank=3;sound.sample=3;break;
    case 71:sound.sample=12;break;
    case 34:sound.sample=230;break;
    default:break;
    }
    if(sound.sample>=0) {sound.positioned=false;sound.volume=1;return sound;}
    // Native one-shot table at 0x4A8F60; bank zero contains 292 samples.
    switch(uint8_t(id)) {
    case 0:sound.sample=4;sound.range=80;sound.volume=90.0f/127;sound.frequency=14000;break;
    case 3:sound.sample=105;sound.volume=60.0f/127;sound.frequency=10600;break;
    case 5:sound.sample=100;sound.range=80;sound.volume=60.0f/127;sound.frequency=22000;break;
    case 12:case 72:case 73:case 74:
        sound.sample=33+int(variation%3);sound.range=35;sound.volume=float(90+variation%20)/127;break;
    case 17:sound.sample=261;sound.range=120;sound.volume=float(115+variation%10)/127;sound.frequency=25472;break;
    case 18:sound.sample=23;sound.frequency=20159;break;
    case 35:sound.bank=35;sound.sample=0;sound.range=1000;sound.volume=100.0f/127;break;
    case 36:sound.sample=228;sound.range=30;sound.volume=75.0f/127;break;
    case 37:sound.sample=2;sound.frequency=39243;sound.range=30;sound.volume=75.0f/127;break;
    case 38:sound.sample=195;sound.range=50;sound.volume=1;break;
    case 39:sound.sample=7;sound.range=50;sound.volume=75.0f/127;break;
    case 40:sound.sample=24;sound.range=50;sound.frequency=16000;sound.volume=75.0f/127;break;
    case 42:sound.sample=170;sound.range=50;sound.volume=1;break;
    case 52:case 53:case 54:sound.sample=65;sound.frequency=12000;sound.volume=1;break;
    case 55:sound.sample=131;break;
    case 56:sound.sample=97;break;
    case 57:sound.sample=104;break;
    case 58:sound.sample=105;break;
    case 59:case 62:sound.sample=121;break;
    case 60:case 63:sound.sample=122;break;
    case 61:sound.sample=123;break;
    case 64:case 65:sound.sample=91;sound.frequency=20159;break;
    case 66:sound.sample=65;sound.frequency=20159;break;
    case 68:sound.sample=163;sound.range=70;sound.volume=100.0f/127;sound.frequency=20159;break;
    case 69:sound.sample=164;sound.range=40;sound.volume=50.0f/127;sound.frequency=20159;break;
    case 76:sound.sample=224;sound.range=80;sound.volume=80.0f/127;break;
    case 77:sound.sample=170;sound.range=60;break;
    case 78:sound.sample=170;sound.range=60;sound.volume=60.0f/127;break;
    case 79:sound.sample=165;sound.range=60;break;
    case 80:sound.sample=166;sound.range=55;sound.volume=80.0f/127;sound.frequency=19000;break;
    case 81:case 82:sound.sample=uint8_t(id)==81?291:37;sound.range=60;sound.volume=float(80+variation%20)/127;sound.frequency=18600;break;
    case 83:sound.sample=95+int(variation%4);sound.range=60;sound.volume=float(70+variation%30)/127;break;
    case 84:case 85:sound.sample=uint8_t(id)==84?280:65;sound.range=60;sound.volume=float(60+variation%30)/127;break;
    case 86:
        if(surface==19) {sound.nativeSilent=true;break;}
        if(surface==2||surface==3||surface==4||surface==8||surface==17||surface==18||
            surface==23||surface==25||surface==33)
            return storylandVcsScriptSound(87,variation,surface);
        sound.sample=31;sound.range=10;sound.volume=float(10+variation%7)/127;sound.frequency=30000;break;
    case 87:sound.sample=32;sound.range=10;sound.volume=float(10+variation%7)/127;sound.frequency=10600;break;
    case 19:sound.bank=2;sound.sample=2251+int(variation%51);sound.volume=1;break;
    case 20:sound.bank=2;sound.sample=1224+int(variation%34);sound.volume=1;break;
    default:sound.nativeSilent=true;break;
    }
    return sound;
}

inline StorylandScriptSoundDefinition storylandLcsScriptSound(uint16_t id,uint32_t variation=0,int surface=-1) {
    StorylandScriptSoundDefinition sound;
    // reLCS Script2.cpp ADD_ONE_OFF_SOUND routes these IDs to frontend audio.
    // The centre recording is used for the current mono playback path. Denied
    // purchases use the pitched left recording: the centre remains unpitched
    // in the original three-recording frontend mix.
    switch(id) {
    case 1:case 7:case 8:case 9:sound.sampleName="sfxpartmissioncompletecentre";break;
    case 10:sound.sampleName="sfxgocentre";break;
    case 13:sound.sampleName="sfxweaponcentre";break;
    case 14:sound.sampleName="sfxweaponleft";sound.frequency=5382;break;
    case 16:sound.sampleName="sfxarmbomb";break;
    default:break;
    }
    if(sound.sampleName) {sound.positioned=false;sound.volume=1;return sound;}

    // AudioLogic.cpp ProcessOneShotScriptObject, using its LCS sound enum.
    // Unknown audible routes (speech and the newer frontend IDs 70/71) stay
    // unresolved so the runtime reports them instead of silently dropping them.
    switch(uint8_t(id)) {
    case 3:sound.sampleName="sfxcolgate";sound.volume=60.0f/127;sound.frequency=10600;break;
    case 5:sound.sampleName="sfxcolcarpanel2";sound.range=80;sound.volume=60.0f/127;sound.frequency=22000;break;
    case 12:case 52:case 53:case 54: {
        static constexpr const char* samples[]={"sfxbulletwall1","sfxbulletwall2","sfxbulletwall3"};
        sound.sampleName=samples[variation%3];sound.range=50;sound.volume=float(90+variation%20)/127;break;
    }
    case 18:sound.sampleName="sfxbombbeep";sound.frequency=20159;break;
    case 35:sound.sampleName="sfxseaplanelow";sound.range=1000;sound.volume=100.0f/127;sound.positioned=false;break;
    case 57:case 58:case 59:
        sound.sampleName=uint8_t(id)==59?"sfxglasscrack":"sfxglasssmash";
        sound.range=60;sound.volume=(uint8_t(id)==58?60.0f:70.0f)/127;break;
    case 60: {
        static constexpr const char* samples[]={"sfxglassshard1","sfxglassshard2","sfxglassshard3","sfxglassshard4"};
        sound.sampleName=samples[variation%4];sound.range=55;sound.volume=25.0f/127;sound.frequency=19000;break;
    }
    case 61:case 62:
        sound.sampleName=uint8_t(id)==61?"sfxwoodenboxsmash":"sfxcardboardboxsmash";
        sound.range=60;sound.volume=float(80+variation%20)/127;sound.frequency=18600;break;
    case 63: {
        static constexpr const char* samples[]={"sfxcolcar1","sfxcolcar2","sfxcolcar3","sfxcolcar4","sfxcolcar5"};
        sound.sampleName=samples[variation%5];sound.range=60;sound.volume=float(70+variation%30)/127;break;
    }
    case 64:case 65:
        sound.sampleName=uint8_t(id)==64?"sfxtyrebump":"sfxhitball";
        sound.range=60;sound.volume=float(60+variation%30)/127;break;
    case 66: {
        if(surface==19) {sound.nativeSilent=true;break;}
        const bool soft=surface==2||surface==3||surface==4||surface==8||surface==17||
            surface==18||surface==23||surface==25||surface==33;
        sound.sampleName=soft?"sfxbulletshellhitground2":"sfxbulletshellhitground1";
        sound.frequency=soft?10600:30000;sound.range=20;sound.volume=float(30+variation%20)/127;break;
    }
    case 67:
        sound.sampleName="sfxbulletshellhitground2";sound.frequency=11000;
        sound.range=20;sound.volume=float(30+variation%20)/127;break;
    case 19:case 20:case 70:case 71:break;
    default:
        // Looping ambience and unused legacy IDs have no one-shot branch.
        if(uint8_t(id)<=69)sound.nativeSilent=true;
        break;
    }
    return sound;
}

inline StorylandScriptSoundDefinition storylandVcsContinuousSound(uint16_t id) {
    StorylandScriptSoundDefinition sound;
    // ProcessScriptObject's continuous table at 0x4A9140 is distinct from
    // the one-shot table. Dispatch reads the stored sound ID's low byte.
    switch (uint8_t(id)) {
    case 0:
        sound.sample = 4;
        sound.range = 80;
        sound.volume = 90.0f / 127;
        break;
    case 2:
    case 4:
        sound.sample = 162;
        sound.range = 80;
        sound.volume = 1;
        break;
    case 11:
        sound.sample = 272;
        sound.range = 40;
        sound.volume = 60.0f / 127;
        break;
    case 41:
        sound.sample = 68;
        sound.range = 40;
        sound.volume = 90.0f / 127;
        break;
    case 43:
        sound.sample = 132;
        sound.range = 40;
        sound.volume = 90.0f / 127;
        break;
    case 51:
        sound.sample = 20;
        sound.range = 80;
        sound.volume = 30.0f / 127;
        sound.frequency = 20812;
        break;
    case 67:
        sound.sample = 77;
        sound.positioned = false;
        sound.volume = 35.0f / 127;
        sound.frequency = 28159;
        break;
    default:
        sound.nativeSilent = true;
        break;
    }
    return sound;
}

inline StorylandScriptSoundDefinition storylandMissionEventSound(bool vcs,uint16_t id,
    bool frontend,uint32_t variation=0,int surface=-1) {
    if(frontend)return vcs?storylandVcsFrontendSound(id):storylandLcsFrontendSound(id);
    return vcs?storylandVcsScriptSound(id,variation,surface):
        storylandLcsScriptSound(id,variation,surface);
}

inline StorylandScriptSoundDefinition storylandLcsContinuousSound(uint16_t id) {
    StorylandScriptSoundDefinition sound;
    // SLUS_214.23: 1B2A98, switch table 3B28A0. The snoring and building
    // branches return at 1B2EA4 without submitting a voice on PS2.
    switch(uint8_t(id)) {
    case 0:
        sound.sampleName="sfxalarm";sound.range=80;sound.volume=90.0f/127;break;
    case 2:case 4:
        sound.sampleName="sfxgaragedoorloop";sound.range=80;sound.volume=1;break;
    case 11:
        sound.sampleName="sfxtankturret";sound.range=40;sound.volume=60.0f/127;break;
    case 51:
        sound.sampleName="sfxboatwaterloop";sound.range=80;
        sound.volume=30.0f/127;sound.frequency=20812;break;
    default:sound.nativeSilent=true;break;
    }
    return sound;
}

inline float storylandScriptSoundGain(const StorylandScriptSoundDefinition& sound,float distance) {
    if(!sound.positioned)return sound.volume;
    if(!std::isfinite(distance)||sound.range<=0)return 0;
    const float fraction=std::clamp((sound.range-distance)/(sound.range*0.8f),0.0f,1.0f);
    return std::floor(sound.volume*127.0f*fraction*fraction)/127.0f;
}
