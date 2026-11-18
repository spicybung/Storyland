#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct StorylandPedStats {
    bool valid=false;
    uint32_t modelId=0;
    int32_t statIndex=-1;
    int32_t pedType=-1;
    int32_t animationGroup=0;
    uint32_t vehicleClassMask = 0;
    std::string name;
    float fleeDistance=16.0f;
    float headingChangeRate=7.5f;
    float attackStrength=1.0f;
    float defendWeakness=1.0f;
    uint16_t flags=0;
    uint8_t fear=50,temper=50,lawfulness=50,view=50;
    template<class Archive> void saveState(Archive& archive) {
        archive(valid,modelId,statIndex,pedType,animationGroup,name,fleeDistance,
            headingChangeRate,attackStrength,defendWeakness,flags,fear,temper,lawfulness,view);
    }
};

inline bool storylandValidLcsPedStats(const StorylandPedStats& stats) {
    return stats.valid&&stats.modelId<=65535&&stats.statIndex>=0&&stats.statIndex<42&&
        stats.pedType>=0&&stats.pedType<=255&&stats.animationGroup>=0&&stats.animationGroup<200&&
        stats.name.size()<=24&&stats.name.compare(0,5,"STAT_")==0&&
        std::all_of(stats.name.begin(),stats.name.end(),[](unsigned char c){return c>=32&&c<=126;})&&
        std::isfinite(stats.fleeDistance)&&stats.fleeDistance>=0&&stats.fleeDistance<=200&&
        std::isfinite(stats.headingChangeRate)&&stats.headingChangeRate>=0&&stats.headingChangeRate<=100&&
        std::isfinite(stats.attackStrength)&&stats.attackStrength>=0&&stats.attackStrength<=100&&
        std::isfinite(stats.defendWeakness)&&stats.defendWeakness>=0&&stats.defendWeakness<=100;
}

struct StorylandPedClothing {
    bool valid = false;
    uint8_t regularCount = 0, scriptCount = 0;
    std::vector<std::array<uint32_t,6>> regular, scripted;
    uint8_t regularCursor = 0;
};

inline bool storylandTrySelectNativePedClothing(const StorylandPedClothing& clothing,
    int32_t scriptColorSet, int32_t colorSet, std::array<uint32_t,6>& colors) {
    if (!clothing.valid) return false;
    if (scriptColorSet >= 0) {
        if (size_t(scriptColorSet) >= clothing.scripted.size()) return false;
        colors = clothing.scripted[size_t(scriptColorSet)];
        return true;
    }
    if (clothing.regular.empty()) {
        if (clothing.regularCount != 0 || colorSet > 0) return false;
        colors.fill(UINT32_MAX);
        return true;
    }
    size_t selected;
    if (colorSet >= 0) {
        selected = size_t(colorSet);
        if (selected >= clothing.regular.size()) return false;
    } else {
        const unsigned next = unsigned(clothing.regularCursor) + 1u;
        selected = next >= clothing.regular.size() ? 0 : next;
    }
    colors = clothing.regular[selected];
    return true;
}

inline std::array<uint32_t,6> storylandSelectPedClothing(const StorylandPedClothing& clothing,
    int32_t scriptColorSet, int32_t colorSet, uint32_t variation,
    const std::vector<std::array<uint8_t,3>>&) {
    if (clothing.valid) {
        if (scriptColorSet >= 0 && size_t(scriptColorSet) < clothing.scripted.size())
            return clothing.scripted[size_t(scriptColorSet)];
        if (!clothing.regular.empty()) {
            const size_t selected = colorSet >= 0 ? size_t(colorSet) : size_t(variation);
            return clothing.regular[selected % clothing.regular.size()];
        }
    }
    std::array<uint32_t,6> colors{};
    colors.fill(UINT32_MAX);
    return colors;
}

inline StorylandPedClothing storylandDecodePedClothing(const std::vector<uint8_t>& data,
    uint32_t modelId, bool vcs) {
    StorylandPedClothing result;
    if (!vcs || modelId > 65535 || data.size() < 0x94) return result;
    const auto word = [&](size_t offset) -> uint32_t {
        if (offset > data.size() || data.size() - offset < 4) return 0;
        return uint32_t(data[offset]) | uint32_t(data[offset + 1]) << 8 |
            uint32_t(data[offset + 2]) << 16 | uint32_t(data[offset + 3]) << 24;
    };
    const uint32_t models = word(0x3C);
    if (modelId >= word(0x38) || !models ||
        uint64_t(models) + uint64_t(modelId) * 4 + 4 > data.size()) return result;
    const uint32_t model = word(size_t(models) + size_t(modelId) * 4);
    if (!model || uint64_t(model) + 0xC9 > data.size() || data[size_t(model) + 0x10] != 7) return result;
    result.regularCursor = data[size_t(model) + 0x46];
    result.regularCount = data[size_t(model) + 0x47];
    result.scriptCount = data[size_t(model) + 0x48];
    // The retail routines use a four-byte set stride and copy six palette
    // indices. Preserve the overlapping rows, including the final two bytes.
    if (result.regularCount > 16 || result.scriptCount > 4) return {};
    const uint32_t palette = word(0x90);
    const auto decode = [&](size_t table, uint8_t count,
        std::vector<std::array<uint32_t,6>>& sets) {
        for (size_t set = 0; set < count; ++set) {
            std::array<uint32_t,6> colors{};
            for (size_t slot = 0; slot < colors.size(); ++slot) {
                const uint8_t index = data[size_t(model) + table + set * 4 + slot];
                const size_t rgb = index >= 240 ? size_t(model) + 0x99 + size_t(index - 240) * 3 :
                    size_t(palette) + size_t(index) * 3;
                if ((!palette && index < 240) || rgb > data.size() || data.size() - rgb < 3) return false;
                colors[slot] = uint32_t(data[rgb]) | uint32_t(data[rgb + 1]) << 8 |
                    uint32_t(data[rgb + 2]) << 16 | 0xFF000000u;
            }
            sets.push_back(colors);
        }
        return true;
    };
    if (!decode(0x49, result.regularCount, result.regular) ||
        !decode(0x89, result.scriptCount, result.scripted)) return {};
    result.valid = true;
    return result;
}

inline StorylandPedStats storylandDecodePedStats(const std::vector<uint8_t>& data,
    uint32_t modelId,bool vcs,int32_t overrideStatIndex=-1) {
    StorylandPedStats result;
    result.modelId=modelId;
    if(modelId>65535||data.size()<0x94)return result;
    const auto word=[&](size_t offset)->uint32_t {
        if(offset>data.size()||data.size()-offset<4)return 0;
        return uint32_t(data[offset])|(uint32_t(data[offset+1])<<8)|
            (uint32_t(data[offset+2])<<16)|(uint32_t(data[offset+3])<<24);
    };
    const uint32_t models=word(0x3C);
    if(!models||uint64_t(models)+uint64_t(modelId)*4+4>data.size())return result;
    const uint32_t model=word(size_t(models)+size_t(modelId)*4);
    if(!model||uint64_t(model)+(vcs?0x40u:0x38u)>data.size()||data[size_t(model)+0x10]!=7)return result;
    if(overrideStatIndex<-1||overrideStatIndex>=(vcs?128:42))return result;
    const uint32_t statIndex=overrideStatIndex>=0?uint32_t(overrideStatIndex):
        word(size_t(model)+(vcs?0x38:0x34));
    const uint32_t pedType=word(size_t(model)+(vcs?0x34:0x30));
    if(statIndex>=128||pedType>255)return result;
    // Both retail games use 0x34-byte records. LCS keeps the name before its
    // numeric fields and owns a distinct header pointer and model-info layout.
    const uint32_t table=word(vcs?0x8C:0x90);
    if(!table||uint64_t(table)+uint64_t(statIndex)*4+4>data.size())return result;
    const uint32_t record=word(size_t(table)+size_t(statIndex)*4);
    if(!record||uint64_t(record)+0x34>data.size())return result;
    if(!vcs&&overrideStatIndex>=0&&word(record)!=statIndex)return result;
    const size_t nameOffset=vcs?0x1A:4,nameLength=vcs?26:24;
    for(size_t index=0;index<nameLength&&data[size_t(record)+nameOffset+index];++index) {
        const unsigned char c=data[size_t(record)+nameOffset+index];
        if(c<32||c>126)return result;
        result.name.push_back(char(c));
    }
    if(result.name.compare(0,5,"STAT_")!=0)return result;
    const auto real=[&](size_t offset) {
        const uint32_t bits=word(size_t(record)+offset);
        float value=0;
        std::memcpy(&value,&bits,sizeof(value));
        return value;
    };
    result.fleeDistance=real(vcs?4:0x1C);
    result.headingChangeRate=real(vcs?8:0x20);
    result.attackStrength=real(vcs?12:0x28);
    result.defendWeakness=real(vcs?16:0x2C);
    if(!std::isfinite(result.fleeDistance)||result.fleeDistance<0||result.fleeDistance>200||
       !std::isfinite(result.headingChangeRate)||result.headingChangeRate<0||result.headingChangeRate>100||
       !std::isfinite(result.attackStrength)||result.attackStrength<0||result.attackStrength>100||
       !std::isfinite(result.defendWeakness)||result.defendWeakness<0||result.defendWeakness>100)return {};
    result.statIndex=int32_t(statIndex);
    result.pedType=int32_t(pedType);
    if(vcs)result.vehicleClassMask=word(size_t(model)+0x3C);
    const uint32_t animationGroup=word(size_t(model)+(vcs?0x30:0x2C));
    if(animationGroup<200)result.animationGroup=int32_t(animationGroup);
    result.flags=vcs?data[size_t(record)+0x14]:uint16_t(word(size_t(record)+0x30));
    result.fear=data[size_t(record)+(vcs?0x16:0x24)];
    result.temper=data[size_t(record)+(vcs?0x17:0x25)];
    result.lawfulness=data[size_t(record)+(vcs?0x18:0x26)];
    result.view=data[size_t(record)+(vcs?0x19:0x27)];
    result.valid=true;
    return result;
}
