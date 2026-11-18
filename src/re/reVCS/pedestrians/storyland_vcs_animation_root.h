#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include "src/leeds/leeds_native_layout.h"

inline bool storylandReadVcsAnimationRoot(const std::vector<uint8_t>& bytes,
    uint32_t group,uint32_t association,std::array<float,3>& result) {
    if(bytes.size()<0x88)return false;
    const auto word=[&](size_t at) {
        return uint32_t(bytes[at])|(uint32_t(bytes[at+1])<<8)|
            (uint32_t(bytes[at+2])<<16)|(uint32_t(bytes[at+3])<<24);
    };
    const size_t table=word(0x84);
    if(table<32||table>bytes.size()||bytes.size()-table<0x10)return false;
    const uint32_t count=word(table);
    if(count==0||count>1024||count>(bytes.size()-table-0x10)/0x20)return false;
    for(uint32_t index=0;index<count;++index) {
        const size_t row=table+0x10+size_t(index)*0x20;
        if(word(row)!=group||word(row+4)!=association)continue;
        std::array<float,3> root{};
        for(size_t axis=0;axis<root.size();++axis) {
            const uint32_t bits=word(row+0x10+axis*4);
            std::memcpy(&root[axis],&bits,sizeof(bits));
            if(!std::isfinite(root[axis])||std::fabs(root[axis])>100)return false;
        }
        result=root;
        return true;
    }
    return false;
}

inline bool storylandReadVcsModelEntryRoot(const std::vector<uint8_t>& bytes,
    uint32_t modelInfo,std::array<float,3>& result) {
    if(modelInfo<32||uint64_t(modelInfo)+0x270>bytes.size())return false;
    StorylandNativeAnimationLayout layout;
    if(!storylandNativeAnimationLayout(bytes,true,layout))return false;
    const auto word=[&](size_t at) {
        if(at>bytes.size()||bytes.size()-at<4)return uint32_t(0);
        return uint32_t(bytes[at])|(uint32_t(bytes[at+1])<<8)|
            (uint32_t(bytes[at+2])<<16)|(uint32_t(bytes[at+3])<<24);
    };
    const uint32_t block=word(size_t(modelInfo)+0x26C);
    if(block>=layout.blockCount)return false;
    const size_t name=size_t(layout.blocks)+size_t(block)*layout.blockStride;
    const size_t manager=word(0x80);
    const size_t counts=manager+200u*0x34u+990u*0x38u;
    if(name>bytes.size()||bytes.size()-name<20||manager<32||
        counts>bytes.size()||bytes.size()-counts<12)return false;
    const uint32_t groups=word(counts);
    if(groups==0||groups>200)return false;
    for(uint32_t group=0;group<groups;++group) {
        const size_t bank=manager+size_t(group)*0x34+24;
        bool matches=true;
        for(size_t index=0;index<20;++index) {
            if(bytes[name+index]!=bytes[bank+index]) {matches=false;break;}
            if(bytes[name+index]==0)break;
        }
        if(matches)return storylandReadVcsAnimationRoot(bytes,group,194,result);
    }
    return false;
}
