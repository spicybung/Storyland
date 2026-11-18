#pragma once
#include <cstdint>

// SLUS_215.90: 0x3132B0, 0x313318 and 0x313450.
// SLUS_214.23: 0x23C668, 0x23C6D0 and 0x23C808.
// Both games mask their game-millisecond clock and halve it.
enum class StorylandTrafficLight : uint8_t { Green=0, Amber=1, Red=2, Off=3 };
inline StorylandTrafficLight storylandRetailTrafficLight(unsigned group,uint32_t milliseconds,
    float wind=0,bool forcedGreen=false,bool vcs=true) {
    if(group!=1&&group!=2)return StorylandTrafficLight::Green;
    if(wind>1.1f||forcedGreen)return StorylandTrafficLight::Green;
    const uint32_t phase=(milliseconds&0x7FFFu)>>1;
    if(group==1)return phase<5000?StorylandTrafficLight::Green:
        phase<6000?StorylandTrafficLight::Amber:StorylandTrafficLight::Red;
    return phase<6000?StorylandTrafficLight::Red:phase<11000?StorylandTrafficLight::Green:
        phase<12000?StorylandTrafficLight::Amber:StorylandTrafficLight::Red;
}
inline StorylandTrafficLight storylandRetailPedestrianLight(uint32_t milliseconds,bool vcs=true) {
    const uint32_t phase=(milliseconds&0x7FFFu)>>1;
    return phase<12000?StorylandTrafficLight::Red:
        phase<15384?StorylandTrafficLight::Green:StorylandTrafficLight::Amber;
}
inline StorylandTrafficLight storylandRetailTrafficLightDisplay(unsigned group,uint32_t milliseconds,
    float wind=0,bool forcedGreen=false,bool vcs=true) {
    if(wind>1.1f)return milliseconds&0x400u?StorylandTrafficLight::Off:StorylandTrafficLight::Amber;
    return storylandRetailTrafficLight(group,milliseconds,wind,forcedGreen,vcs);
}
