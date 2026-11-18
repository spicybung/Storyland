#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

struct StorylandScriptMusicEmitter {
    std::array<float,3> position{};
    int32_t radius=0,stream=-1;
    template<class Archive> void saveState(Archive& archive) {archive(position,radius,stream);}
};

inline int32_t storylandVcsMusicPositionFromMilliseconds(uint64_t milliseconds) {
    // 04FB converts the native stream byte position by 308/256. The host
    // provides a millisecond cursor, whose retail VB conversion is 7/256.
    const uint64_t maximum=uint64_t(std::numeric_limits<int32_t>::max());
    if(milliseconds>maximum*7u/308u)return std::numeric_limits<int32_t>::max();
    const uint64_t bytes=milliseconds*256u/7u;
    return int32_t(std::min(bytes*308u/256u,maximum));
}

inline const char* storylandVcsMusicStreamStem(int stream) {
    // VCS PS2 CDSTREAM.IRX, 112 path records at module address 0x6410.
    static constexpr const char* names[]={
        "flash","vrock","paradise","vcpr","vcfl","wave","fresh","espant",
        "emotion","city","water","sawmill","city","water","water","city",
        "city","city","city","city","city","amb_bikr","constrct","philcol",
        "mall_1","mall_2","mall_3","mall_4","mall_5","crowd","city","police",
        "taxi","news_1","news_2","news_3","news_4","news_5","news_6","news_7",
        "news_8","news_9","news_10","news_11","news_12","news_13","news_14","news_15",
        "news_16","news_17","news_18","news_19","news_20","theme","jera1","jera2",
        "jera3","phia1","phia2","phia3","phia4","mara1","mara2","mara2",
        "mara2","mara5","loua1","loua2","loua3","lanb1","lanb2","lanb4",
        "lanb5","lanb6","umbb1","umbb2","umbb3","umbb4","bryb1","bryb1",
        "bryb1","lanc1","lanc1","lanc3","lanc4","lanc5","lanc6","lanc6",
        "lanc8","lanc11a","lanc11b","renc1","renc2","renc3","renc4","renc5",
        "renc6","renc7","menc1","menc2","menc3","menc5","menc6","diac1",
        "diac2","diac3","diac4","diac4","diac5b","theme","miscom2","miscom2",
    };
    return stream>=0&&stream<int(sizeof(names)/sizeof(names[0]))?names[stream]:nullptr;
}

struct StorylandScriptMusicSelection {
    int stream=-1;
    float gain=0;
};

inline const char* storylandLcsMusicStreamStem(int stream) {
    // PS2 omits the port's MP3-player slot; native mission completion is 64.
    static constexpr const char* names[]={
        "head","double","kjah","rise","lips","mundo","msx","flash","lcj","lcfr",
        "city","water","sawmill","city","water","water","city","city","city",
        "city","city","city","city","police","taxi","news_a","news_b","news_c","news_d",
        "news_e","news_f","news_g","news_h","news_i","news_j","news_k","news_l","news_m","news_n",
        "biker","bonevoy","campain","cashchp","contban","crazy69","crazy69","deadly","donprob","drivnmr",
        "electon","final","final_2","homswet","hotwhel","kidnapp","landgrb","morgue","overdos","rufjust",
        "sayonar","sicilan","theofer","intro","finale","miscom2"
    };
    return stream>=0&&size_t(stream)<std::size(names)?names[stream]:nullptr;
}

inline StorylandScriptMusicSelection storylandLcsRiotMusic(uint8_t intensity,
    const std::array<float,3>& listener) {
    if(!intensity)return {};
    for(float coordinate:listener)if(!std::isfinite(coordinate))return {};
    const float x=listener[0]-300.7f,y=listener[1]+322.0f,z=listener[2]-12.0f;
    const float distance=std::sqrt(x*x+y*y+z*z);
    if(distance>=65)return {};
    const float height=std::clamp((50.0f-listener[2])/30.0f,0.0f,1.0f);
    const float distanceGain=distance<=20?1.0f:1.0f-(distance-20.0f)/45.0f;
    return {22,float(intensity)/127.0f*height*distanceGain};
}
inline StorylandScriptMusicSelection storylandSelectScriptMusic(
    const std::vector<StorylandScriptMusicEmitter>& emitters,const std::array<float,3>& listener) {
    for(float value:listener)if(!std::isfinite(value))return {};
    for(const auto& emitter:emitters) {
        const float dx=listener[0]-emitter.position[0],dy=listener[1]-emitter.position[1];
        const float dz=listener[2]-emitter.position[2];
        const float distance=std::sqrt(dx*dx+dy*dy+dz*dz);
        if(emitter.radius<=0||!(distance<float(emitter.radius)))continue;
        // Native 0x18C430 selects the first matching sphere; 0x18C5B8 attenuates to 40/127.
        return {emitter.stream,(40.0f/127.0f)*(1.0f-distance/float(emitter.radius))};
    }
    return {};
}
