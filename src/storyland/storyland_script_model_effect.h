#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

struct StorylandModelScriptEffect {
    uint8_t type=0,subtype=0;
    int8_t label=0;
    std::array<float,3> position{};
    float heading=0;
};

inline bool storylandDecodeVcsModelScriptEffects(const std::vector<uint8_t>& bytes,int32_t model,
    std::vector<StorylandModelScriptEffect>& effects,std::string& error) {
    effects.clear();error.clear();
    const auto range=[&](uint64_t offset,uint64_t length) {
        return offset<=bytes.size()&&length<=bytes.size()-offset;
    };
    const auto word=[&](size_t offset) {
        return uint32_t(bytes[offset])|(uint32_t(bytes[offset+1])<<8)|
            (uint32_t(bytes[offset+2])<<16)|(uint32_t(bytes[offset+3])<<24);
    };
    const auto real=[&](size_t offset) {
        const uint32_t bits=word(offset);float value;std::memcpy(&value,&bits,4);return value;
    };
    if(!range(0x94,4)||model<0||uint32_t(model)>=word(0x38)) {
        error="Script effects require a valid native model.";return false;
    }
    const uint32_t modelTable=word(0x3C);
    if(!range(uint64_t(modelTable)+uint64_t(model)*4,4)) {
        error="Native script-effect model table is truncated.";return false;
    }
    const uint32_t info=word(size_t(modelTable)+size_t(model)*4);
    if(!info||!range(info,0x22)) {error="Native script-effect model info is absent.";return false;}
    const int16_t group=int16_t(uint16_t(bytes[info+0x20])|(uint16_t(bytes[info+0x21])<<8));
    if(group<0)return true;
    // ResourceImage begins after the 0x20-byte header. Native 332458 passes
    // body+74 to 1B5150; modelInfo+20 selects its 8-byte group descriptor.
    const uint32_t registry=word(0x94);
    if(!registry||!range(registry,4)||uint32_t(group)>=word(registry)||
        !range(uint64_t(registry)+4+uint64_t(group)*8,8)) {
        error="Native script-effect registry/group is invalid.";return false;
    }
    const size_t descriptor=size_t(registry)+4+size_t(group)*8;
    const uint32_t count=word(descriptor),rows=word(descriptor+4);
    if(count>INT32_MAX||!range(rows,uint64_t(count)*0x30)) {
        error="Native script-effect rows are truncated.";return false;
    }
    std::vector<StorylandModelScriptEffect> decoded;decoded.reserve(count);
    for(uint32_t index=0;index<count;++index) {
        const size_t row=size_t(rows)+size_t(index)*0x30;
        StorylandModelScriptEffect effect;
        effect.type=bytes[row];effect.subtype=bytes[row+1];effect.label=int8_t(bytes[row+2]);
        for(size_t axis=0;axis<3;++axis)effect.position[axis]=real(row+0x10+axis*4);
        effect.heading=real(row+0x20);
        if(!std::isfinite(effect.position[0])||!std::isfinite(effect.position[1])||
            !std::isfinite(effect.position[2])||!std::isfinite(effect.heading)) {
            error="Native script-effect position/heading is non-finite.";return false;
        }
        decoded.push_back(effect);
    }
    effects=std::move(decoded);return true;
}

inline bool storylandScriptEffectMatches(const StorylandModelScriptEffect& effect,int32_t type,
    int32_t subtype=-1,int32_t label=-1) {
    const auto lower=[](int32_t value) {
        return value>='A'&&value<='Z'?value+('a'-'A'):value;
    };
    return int32_t(effect.type)==type&&(subtype<0||int32_t(effect.subtype)==subtype)&&
        (label<0||lower(effect.label)==lower(label));
}

inline std::array<float,4> storylandScriptEffectWorldTransform(const StorylandModelScriptEffect& effect,
    const std::array<float,16>& matrix) {
    std::array<float,4> result{};
    for(size_t axis=0;axis<3;++axis)result[axis]=matrix[12+axis]+
        matrix[axis]*effect.position[0]+matrix[4+axis]*effect.position[1]+matrix[8+axis]*effect.position[2];
    const float x=-matrix[4],y=matrix[5];
    const float heading=x==0&&y==0?0:std::atan2(x,y);
    result[3]=(heading-effect.heading)*57.2957763671875f;
    return result;
}
