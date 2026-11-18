#pragma once
#include "src/leeds/leeds_native_world.h"
#include <cmath>
#include <cstring>

inline bool storylandReadNativeModelBaseOffset(const std::vector<uint8_t>& bytes,
    const StorylandNativeWorld& world,int32_t model,float& offset) {
    const auto word=[&](size_t position) {
        uint32_t value=0;
        if(position<=bytes.size()&&bytes.size()-position>=4)
            std::memcpy(&value,bytes.data()+position,4);
        return value;
    };
    if(model<0||uint32_t(model)>=word(0x38))return false;
    const uint32_t table=word(0x3C);
    if(table<0x20||uint64_t(table)+(uint64_t(model)+1)*4>bytes.size())return false;
    const uint32_t info=word(size_t(table)+size_t(model)*4);
    if(info<0x20||uint64_t(info)+0x18>bytes.size())return false;
    const uint32_t collision=word(size_t(info)+0x14);
    if(collision>=0x20&&uint64_t(collision)+0x1C<=bytes.size()) {
        float minimum=0;
        std::memcpy(&minimum,bytes.data()+collision+0x18,4);
        offset=-minimum;
        return std::isfinite(offset);
    }
    const auto found=world.collisionModels.find(uint32_t(model));
    if(found==world.collisionModels.end())return false;
    offset=-found->second.minimum.z;
    return std::isfinite(offset);
}

inline bool storylandReadNativeModelSphere(const std::vector<uint8_t>& bytes,
    const StorylandNativeWorld& world,int32_t model,StorylandNativePoint& center,float& radius) {
    const auto word=[&](size_t offset) {
        uint32_t value=0;
        if(offset<=bytes.size()&&bytes.size()-offset>=4)std::memcpy(&value,bytes.data()+offset,4);
        return value;
    };
    if(model<0||uint32_t(model)>=word(0x38))return false;
    const uint32_t table=word(0x3C),info=word(size_t(table)+size_t(model)*4);
    const uint32_t collision=info?word(size_t(info)+0x14):0;
    if(collision&&uint64_t(collision)+16<=bytes.size()) {
        std::memcpy(&center.x,bytes.data()+collision,4);
        std::memcpy(&center.y,bytes.data()+collision+4,4);
        std::memcpy(&center.z,bytes.data()+collision+8,4);
        std::memcpy(&radius,bytes.data()+collision+12,4);
        return std::isfinite(center.x)&&std::isfinite(center.y)&&std::isfinite(center.z)&&
            std::isfinite(radius)&&radius>=0&&radius<100000;
    }
    const auto found=world.collisionModels.find(uint32_t(model));
    if(found==world.collisionModels.end())return false;
    center=found->second.boundingCenter;radius=found->second.boundingRadius;
    return true;
}
