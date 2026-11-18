#pragma once
#include "src/leeds/leeds_native_world.h"
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

enum class StorylandLcsPoliceRadioSample : uint32_t {
    Central = 0x0060,
    East = 0x00A3,
    In = 0x00CC,
    North = 0x00E5,
    LastSeen = 0x00F6,
    MessageNoise = 0x00F8,
    Suspect = 0x00FA,
    South = 0x011B,
    West = 0x0136,
    End = 0x161E
};

inline std::vector<uint32_t> storylandLcsSuspectLastSeenSamples(
    const StorylandNativeZones& zones, const StorylandNativePoint& position) {
    if(zones.vcs)return {};
    const StorylandNativeZone* selected=nullptr;
    for(uint16_t index:zones.audio) {
        if(index>=zones.navigation.size())return {};
        if(zones.navigation[index].contains(position)) {
            selected=&zones.navigation[index];
            break;
        }
    }
    if(!selected)return {};
    struct ZoneSample {const char* name;uint32_t sample;};
    // SLUS_214.23 22C798 retains these fourteen names; other rows are empty.
    static constexpr std::array<ZoneSample,14> samples{{
        {"VICE_C",0x0F73},{"IND_ZON",0x0F74},{"COM_ZON",0x0F75},
        {"BEACH1",0x0F5B},{"BEACH2",0x0F77},{"BEACH3",0x0F76},
        {"GOLFC",0x0F55},{"STARI",0x0F69},{"DOCKS",0x0F72},
        {"HAVANA",0x0F57},{"HAITI",0x0F56},{"PORNI",0x0F61},
        {"DTOWN",0x0F4B},{"A_PORT",0x0F4C}
    }};
    uint32_t zoneSample=0;
    for(const auto& entry:samples)if(selected->name==entry.name) {
        zoneSample=entry.sample;
        break;
    }
    if(!zoneSample)return {};
    using Sample=StorylandLcsPoliceRadioSample;
    std::vector<uint32_t> result{uint32_t(Sample::MessageNoise),uint32_t(Sample::Suspect),
        uint32_t(Sample::LastSeen),uint32_t(Sample::In)};
    const float width=selected->maximum.x-selected->minimum.x;
    const float height=selected->maximum.y-selected->minimum.y;
    const float middleX=selected->minimum.x+width*.5f;
    const float middleY=selected->minimum.y+height*.5f;
    bool northSouth=false;
    if(position.y>middleY+height*.25f) {
        result.push_back(uint32_t(Sample::North));
        northSouth=true;
    } else if(position.y<middleY-height*.25f) {
        result.push_back(uint32_t(Sample::South));
        northSouth=true;
    }
    if(position.x>middleX+width*.25f)result.push_back(uint32_t(Sample::East));
    else if(position.x<middleX-width*.25f)result.push_back(uint32_t(Sample::West));
    else if(!northSouth)result.push_back(uint32_t(Sample::Central));
    result.push_back(zoneSample);
    result.push_back(uint32_t(Sample::MessageNoise));
    result.push_back(uint32_t(Sample::End));
    return result;
}

struct StorylandLcsPoliceRadioQueue {
    std::deque<uint32_t> samples;
    uint32_t waitFrames=0,lastFrame=UINT32_MAX;

    void clear() {
        samples.clear();waitFrames=0;lastFrame=UINT32_MAX;
    }

    bool enqueue(const StorylandNativeZones& zones,const StorylandNativePoint& position,
        bool audioEnabled,int32_t musicMode,const std::function<bool(uint32_t)>& available,
        std::string& error) {
        error.clear();
        if(!audioEnabled||musicMode==2||samples.size()>50)return true;
        const auto report=storylandLcsSuspectLastSeenSamples(zones,position);
        for(uint32_t sample:report) {
            if(sample==uint32_t(StorylandLcsPoliceRadioSample::End))continue;
            if(!available||!available(sample)) {
                error="LCS police-radio sample "+std::to_string(sample)+
                    " is unavailable from its native SDT bank.";
                return false;
            }
        }
        samples.insert(samples.end(),report.begin(),report.end());
        return true;
    }

    bool take(uint32_t nativeFrame,uint32_t& sample) {
        if(waitFrames) {
            if(nativeFrame!=lastFrame)--waitFrames;
            lastFrame=nativeFrame;
            return false;
        }
        if(samples.empty())return false;
        sample=samples.front();samples.pop_front();
        if(sample==uint32_t(StorylandLcsPoliceRadioSample::End)) {
            waitFrames=30;lastFrame=nativeFrame;
            return false;
        }
        return true;
    }
};
