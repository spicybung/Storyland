#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <tuple>

enum class StorylandSpeechEvent : uint16_t {
    Death=0x66, Pain=0x6A, Burning=0x6B, Arrested=0x6C,
    Shoot=0x73, DrawWeapon=0x74, Threatened=0x76, Panic=0x77,
    Jacking=0x78, Jacked=0x7A, Chased=0x86, Attack=0x89,
    Dodge=0x8D, Crash=0x8F, Blocked=0x91, Bump=0x9B, Chat=0x9C
};
struct StorylandSpeechRange {
    uint16_t model,event,variant,first,count;
};
inline constexpr StorylandSpeechRange StorylandVcsSpeechRanges[]={
#include "src/re/reVCS/audio/storyland_vcs_speech_data.inc"
};
inline const StorylandSpeechRange* storylandVcsSpeechRange(uint16_t model,uint16_t event,unsigned variant=0) {
    const auto find=[&](unsigned selected)->const StorylandSpeechRange* {
        const auto key=std::make_tuple(model,event,uint16_t(selected));
        const auto first=std::begin(StorylandVcsSpeechRanges),last=std::end(StorylandVcsSpeechRanges);
        const auto found=std::lower_bound(first,last,key,[](const auto& row,const auto& value) {
            return std::tie(row.model,row.event,row.variant)<value;
        });
        return found!=last&&std::tie(found->model,found->event,found->variant)==key?found:nullptr;
    };
    if(const auto* result=find(variant))return result;
    return variant?find(0):nullptr;
}
// 0507 custom-profile dispatch at 1C8558. First samples are relative to bank 2.
// Profile 1 is silent; profiles 2..29 include their native generic-event fallback.
inline constexpr StorylandSpeechRange StorylandVcsSpeechProfileRanges[]={
    {2,102,0,2223,28},
    {2,106,0,2251,51},
    {2,107,0,2302,21},
    {2,119,0,709,3},
    {2,137,0,706,3},
    {2,138,0,2251,51},
    {2,143,0,703,3},
    {2,159,0,712,3},
    {3,102,0,2223,28},
    {3,106,0,2251,51},
    {3,107,0,2302,21},
    {3,119,0,795,3},
    {3,137,0,792,3},
    {3,138,0,2251,51},
    {3,143,0,789,3},
    {3,159,0,798,3},
    {4,102,0,2223,28},
    {4,106,0,2251,51},
    {4,107,0,2302,21},
    {4,119,0,1047,3},
    {4,137,0,1044,3},
    {4,138,0,2251,51},
    {4,143,0,1041,3},
    {4,159,0,1050,3},
    {5,102,0,2223,28},
    {5,106,0,2251,51},
    {5,107,0,2302,21},
    {5,119,0,1059,3},
    {5,137,0,1056,3},
    {5,138,0,2251,51},
    {5,143,0,1053,3},
    {5,159,0,1062,3},
    {6,102,0,2223,28},
    {6,106,0,2251,51},
    {6,107,0,2302,21},
    {6,119,0,1149,3},
    {6,137,0,1146,3},
    {6,138,0,2251,51},
    {6,143,0,1143,3},
    {6,159,0,1152,3},
    {7,102,0,2223,28},
    {7,106,0,2251,51},
    {7,107,0,2302,21},
    {7,119,0,1161,3},
    {7,137,0,1158,3},
    {7,138,0,2251,51},
    {7,143,0,1155,3},
    {7,159,0,1164,3},
    {8,102,0,2223,28},
    {8,106,0,2251,51},
    {8,107,0,2302,21},
    {8,119,0,1173,3},
    {8,137,0,1170,3},
    {8,138,0,2251,51},
    {8,143,0,1167,3},
    {8,159,0,1176,3},
    {9,102,0,2223,28},
    {9,106,0,2251,51},
    {9,107,0,2302,21},
    {9,115,0,1201,5},
    {9,119,0,2323,23},
    {9,138,0,2251,51},
    {9,140,0,1206,2},
    {10,102,0,2223,28},
    {10,106,0,2251,51},
    {10,107,0,2302,21},
    {10,119,0,1405,3},
    {10,137,0,1402,3},
    {10,138,0,2251,51},
    {10,143,0,1399,3},
    {10,159,0,1408,3},
    {11,102,0,2223,28},
    {11,106,0,2251,51},
    {11,107,0,2302,21},
    {11,119,0,1417,3},
    {11,137,0,1414,3},
    {11,138,0,2251,51},
    {11,143,0,1411,3},
    {11,159,0,1420,3},
    {12,102,0,2223,28},
    {12,106,0,2251,51},
    {12,107,0,2302,21},
    {12,115,0,1442,6},
    {12,119,0,1437,3},
    {12,137,0,1434,3},
    {12,138,0,2251,51},
    {12,154,0,1440,2},
    {13,102,0,2223,28},
    {13,106,0,2251,51},
    {13,107,0,2302,21},
    {13,119,0,1772,3},
    {13,137,0,1769,3},
    {13,138,0,2251,51},
    {13,143,0,1766,3},
    {13,159,0,1775,3},
    {14,102,0,2223,28},
    {14,106,0,2251,51},
    {14,107,0,2302,21},
    {14,119,0,2033,3},
    {14,137,0,2030,3},
    {14,138,0,2251,51},
    {14,143,0,2027,3},
    {14,159,0,2036,3},
    {15,102,0,2223,28},
    {15,106,0,2251,51},
    {15,107,0,2302,21},
    {15,119,0,2088,3},
    {15,137,0,2085,3},
    {15,138,0,2251,51},
    {15,143,0,2082,3},
    {15,159,0,2091,3},
    {16,102,0,2223,28},
    {16,106,0,2251,51},
    {16,107,0,2302,21},
    {16,119,0,2100,3},
    {16,137,0,2097,3},
    {16,138,0,2251,51},
    {16,143,0,2094,3},
    {16,159,0,2103,3},
    {17,102,0,2223,28},
    {17,106,0,2251,51},
    {17,107,0,2302,21},
    {17,119,0,2515,3},
    {17,137,0,2512,3},
    {17,138,0,2251,51},
    {17,143,0,2509,3},
    {17,159,0,2518,3},
    {18,102,0,2223,28},
    {18,106,0,2251,51},
    {18,107,0,2302,21},
    {18,119,0,2559,2},
    {18,137,0,2556,3},
    {18,138,0,2251,51},
    {18,143,0,2553,3},
    {18,159,0,2561,3},
    {19,102,0,1208,16},
    {19,106,0,1224,34},
    {19,107,0,1258,11},
    {19,119,0,1269,13},
    {19,127,0,1282,11},
    {19,137,0,2614,3},
    {19,138,0,1224,34},
    {20,102,0,1208,16},
    {20,106,0,1224,34},
    {20,107,0,1258,11},
    {20,119,0,1269,13},
    {20,127,0,1282,11},
    {20,137,0,2617,3},
    {20,138,0,1224,34},
    {21,102,0,1208,16},
    {21,106,0,1224,34},
    {21,107,0,1258,11},
    {21,119,0,1269,13},
    {21,127,0,1282,11},
    {21,137,0,2620,3},
    {21,138,0,1224,34},
    {22,102,0,1208,16},
    {22,106,0,1224,34},
    {22,107,0,1258,11},
    {22,119,0,1269,13},
    {22,127,0,1282,11},
    {22,137,0,2623,5},
    {22,138,0,1224,34},
    {23,102,0,2223,28},
    {23,106,0,2251,51},
    {23,107,0,2302,21},
    {23,119,0,2780,3},
    {23,137,0,2777,3},
    {23,138,0,2251,51},
    {23,143,0,2774,3},
    {23,159,0,2783,3},
    {24,102,0,2223,28},
    {24,106,0,2251,51},
    {24,107,0,2302,21},
    {24,119,0,2792,3},
    {24,137,0,2789,3},
    {24,138,0,2251,51},
    {24,143,0,2786,3},
    {24,159,0,2795,3},
    {25,102,0,2223,28},
    {25,106,0,2251,51},
    {25,107,0,2302,21},
    {25,119,0,2848,3},
    {25,137,0,2845,3},
    {25,138,0,2251,51},
    {25,143,0,2842,3},
    {25,159,0,2851,3},
    {26,102,0,2223,28},
    {26,106,0,2251,51},
    {26,107,0,2302,21},
    {26,119,0,2860,3},
    {26,137,0,2857,3},
    {26,138,0,2251,51},
    {26,143,0,2854,3},
    {26,159,0,2863,3},
    {27,102,0,2223,28},
    {27,106,0,2251,51},
    {27,107,0,2302,21},
    {27,119,0,4646,3},
    {27,137,0,4643,3},
    {27,138,0,2251,51},
    {27,143,0,4640,3},
    {27,159,0,4649,3},
    {28,102,0,2223,28},
    {28,106,0,2251,51},
    {28,107,0,2302,21},
    {28,119,0,5520,3},
    {28,137,0,5517,3},
    {28,138,0,2251,51},
    {28,143,0,5514,3},
    {28,159,0,5523,3},
    {29,102,0,1208,16},
    {29,106,0,1224,34},
    {29,107,0,1258,11},
    {29,119,0,5508,3},
    {29,127,0,1282,11},
    {29,137,0,5505,3},
    {29,138,0,1224,34},
    {29,143,0,5502,3},
    {29,159,0,5511,3},
};
inline const StorylandSpeechRange* storylandVcsSpeechProfileRange(int32_t profile,uint16_t event) {
    if(profile<2||profile>29)return nullptr;
    const auto key=std::make_pair(uint16_t(profile),event);
    const auto first=std::begin(StorylandVcsSpeechProfileRanges),last=std::end(StorylandVcsSpeechProfileRanges);
    const auto found=std::lower_bound(first,last,key,[](const auto& row,const auto& value) {
        return std::make_pair(row.model,row.event)<value;
    });
    return found!=last&&std::make_pair(found->model,found->event)==key?found:nullptr;
}
struct StorylandSpeechState {
    uint32_t random=0x6D2B79F5u;
    int last=-1;
    float nextAllowed=0;
    std::array<uint8_t,8> playerCounters{};
    bool playerGate(uint16_t event) {
        unsigned index=8,period=1;
        if(event==0x89) {index=0;period=4;}
        else if(event==0x73) {index=1;period=8;}
        else if(event==0x93) {index=2;period=4;}
        else if(event==0x78) {index=3;period=4;}
        else if(event==0x7A) {index=4;period=2;}
        else if(event>=0x8F&&event<=0x91) {index=5;period=4;}
        else if(event==0x87) {index=6;period=4;}
        if(index==8)return true;
        const bool allowed=playerCounters[index]==0;
        playerCounters[index]=uint8_t((playerCounters[index]+1)%period);
        return allowed;
    }
    int choose(const StorylandSpeechRange& range,uint32_t randomValue) {
        if(!range.count)return -1;
        int sample=range.first+int(randomValue%range.count);
        if(sample==last)sample=range.first+(sample-range.first+1)%range.count;
        last=sample;return sample;
    }
    int choose(const StorylandSpeechRange& range) {
        random^=random<<13;random^=random>>17;random^=random<<5;
        return choose(range,random);
    }
};
