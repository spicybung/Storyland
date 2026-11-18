#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

// A restrained presentation effect. Original PS2 menu/music reverb settings
// have not been recovered, so the delay and wet levels are not retail claims.
enum class StorylandFrontendAmbience : uint8_t { None, Menu, MissionPassed };

class StorylandFrontendEcho {
public:
    StorylandFrontendEcho(uint32_t sampleRate,uint16_t channels,StorylandFrontendAmbience effect)
        : channelCount(channels),enabled(effect!=StorylandFrontendAmbience::None &&
            sampleRate>=8000&&sampleRate<=192000&&(channels==1||channels==2)) {
        if(enabled) {
            shortDelay.assign(size_t(std::max(1u,sampleRate*37u/1000u))*channels,0);
            longDelay.assign(size_t(std::max(1u,sampleRate*79u/1000u))*channels,0);
        }
    }

    void process(int16_t* samples,size_t count) {
        if(!enabled||!samples||count%channelCount)return;
        for(size_t i=0;i<count;++i) {
            const int32_t input=samples[i];
            // Keep the original onset and use only quiet, non-recursive taps.
            const int32_t wet=int32_t(shortDelay[shortPosition])*3/32+
                int32_t(longDelay[longPosition])/32;
            shortDelay[shortPosition]=int16_t(input);
            longDelay[longPosition]=int16_t(input);
            shortPosition=(shortPosition+1)%shortDelay.size();
            longPosition=(longPosition+1)%longDelay.size();
            samples[i]=int16_t(std::clamp(input+wet,-32768,32767));
        }
    }

    static size_t menuTailFrames(uint32_t sampleRate) {
        return size_t(sampleRate)*79u/1000u;
    }

private:
    uint16_t channelCount=0;
    bool enabled=false;
    std::vector<int16_t> shortDelay,longDelay;
    size_t shortPosition=0,longPosition=0;
};
