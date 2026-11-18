#pragma once

#include "src/leeds/leeds_native_world.h"
#include <array>
#include <cmath>
#include <cstdint>

// The PS2 police-radio manager owns a 36-entry name/sample table and a
// 60-element FIFO.  Its startup routine clears every eight-byte name.  No
// original producer for the mapping table has yet been established; leave it
// empty until a real source is available rather than fabricating speech.
struct StorylandVcsPoliceRadioState {
    struct ZoneMapping {
        std::array<uint8_t,8> name{};
        uint32_t sample=0;
    };
    std::array<ZoneMapping,36> mapping{};
    std::array<uint32_t,60> samples{};
    uint8_t count=0;
    uint8_t writeIndex=0;
    uint8_t readIndex=0;
    uint32_t missionLookupEntries=0;

    void clearQueue() {
        count=0;
        writeIndex=0;
        readIndex=0;
    }

    void enqueue(uint32_t sample) {
        if(count>=samples.size())return;
        samples[writeIndex]=sample;
        writeIndex=uint8_t((unsigned(writeIndex)+1u)%samples.size());
        ++count;
    }
};

// Producer at 0x27FAE8: zone and table lookups precede all queue mutations.
// Audio-manager event 65 is the original unused-sample placeholder, not a
// guessed radio voice. The end marker depends on M_LOOKUP's entry count.
inline void storylandVcsPoliceRadioReport(StorylandVcsPoliceRadioState& radio,
    const StorylandNativeZones& zones,const StorylandNativePoint& point,
    bool audioManagerActive,int32_t musicMode) {
    if(!audioManagerActive||musicMode==2||radio.count>50||!zones.vcs||
        !std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))return;
    const StorylandNativeZone* selected=nullptr;
    for(size_t index=0;index<zones.audio.size()&&index<36;++index) {
        const auto navigation=zones.audio[index];
        if(navigation>=zones.navigation.size())return;
        const auto& zone=zones.navigation[navigation];
        if(zone.contains(point)) {selected=&zone;break;}
    }
    if(!selected)return;
    std::array<uint8_t,8> zoneName{};
    for(size_t index=0;index<zoneName.size()&&index<selected->name.size();++index)
        zoneName[index]=uint8_t(selected->name[index]);
    const StorylandVcsPoliceRadioState::ZoneMapping* match=nullptr;
    for(const auto& candidate:radio.mapping)if(candidate.name==zoneName) {match=&candidate;break;}
    if(!match)return;
    for(unsigned index=0;index<4;++index)radio.enqueue(65);
    const float middleY=selected->minimum.y+(selected->maximum.y-selected->minimum.y)*.5f;
    const float quarterY=(selected->maximum.y-selected->minimum.y)*.25f;
    const float middleX=selected->minimum.x+(selected->maximum.x-selected->minimum.x)*.5f;
    const float quarterX=(selected->maximum.x-selected->minimum.x)*.25f;
    const bool northSouth=point.y>middleY+quarterY||point.y<middleY-quarterY;
    if(northSouth)radio.enqueue(65);
    if(point.x>middleX+quarterX||point.x<middleX-quarterX||!northSouth)radio.enqueue(65);
    radio.enqueue(match->sample);
    radio.enqueue(65);
    radio.enqueue(radio.missionLookupEntries+0x1752u);
}
