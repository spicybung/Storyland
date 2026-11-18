#pragma once
#include "src/leeds/leeds_archive.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>

struct StorylandLcsDynamicModel {
    uint32_t model=0;
    int32_t resource=-1,transparentResource=-1;
    std::array<float,3> scale{},offset{};
    std::array<float,4> bounds{};
    uint16_t flags=0;
};

// Both PS2 Stories games store 48-byte sDynamic master-WRLD records. Their
// normalization belongs to the compiled mesh, including the native pivot.
// VCS uses master+0x2E8/0x2EC; LCS uses +0x328/0x32C (wide +0x4A8/0x4B0).
inline bool storylandDecodeLcsDynamicModels(const StorylandArchiveBrowser& browser,
    std::vector<StorylandLcsDynamicModel>& result,std::string& error,const uint32_t* selectedModel=nullptr) {
    result.clear();
    const auto& bytes=browser.lvzBytes();
    const uint32_t master=browser.nativeWorldMasterOffset(true);
    if(master==UINT32_MAX)return true;
    const bool wide=browser.nativeWorldUses64BitPointers();
    const bool vcs=browser.nativeWorldMasterOffset()==UINT32_MAX;
    const auto word=[&](size_t at) {
        return uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8|
            uint32_t(bytes[at+2])<<16|uint32_t(bytes[at+3])<<24;
    };
    if(uint64_t(master)+(wide?0x4B8u:vcs?0x2F0u:0x330u)>bytes.size()) {
        error="Truncated LCS dynamic-model header.";return false;
    }
    const uint32_t count=word(size_t(master)+(wide?0x4A8u:vcs?0x2E8u:0x328u));
    const size_t pointerAt=size_t(master)+(wide?0x4B0u:vcs?0x2ECu:0x32Cu);
    const uint32_t pointer=word(pointerAt);
    if(wide&&word(pointerAt+4u)!=0u) {error="Invalid wide LCS dynamic-model pointer.";return false;}
    if(count==0)return true;
    const uint64_t table=uint64_t(master)+pointer;
    if(count>4096||pointer<0x20||table+uint64_t(count)*48u>bytes.size()) {
        error="Invalid LCS dynamic-model table.";return false;
    }
    const auto half=[&](size_t at) {return uint16_t(bytes[at])|uint16_t(bytes[at+1])<<8;};
    const auto real=[&](size_t at) {
        const uint32_t bits=word(at);float value;std::memcpy(&value,&bits,sizeof(value));return value;
    };
    std::vector<StorylandLcsDynamicModel> models;
    if(!selectedModel)models.reserve(count);
    for(uint32_t index=0;index<count;++index) {
        const size_t row=size_t(table)+size_t(index)*48;
        StorylandLcsDynamicModel model;
        model.model=half(row+12);model.resource=int16_t(half(row+14));
        model.flags=uint16_t(half(row+28));
        model.transparentResource=int16_t(half(row+30));
        for(size_t axis=0;axis<3;++axis) {
            model.scale[axis]=real(row+axis*4);model.offset[axis]=real(row+16+axis*4);
            if(!std::isfinite(model.scale[axis])||model.scale[axis]<0||!std::isfinite(model.offset[axis])) {
                error="Nonfinite LCS dynamic-model normalization.";return false;
            }
        }
        for(size_t component=0;component<4;++component) {
            model.bounds[component]=real(row+32+component*4);
            if(!std::isfinite(model.bounds[component])) {
                error="Nonfinite LCS dynamic-model bounds.";return false;
            }
        }
        if(model.bounds[3]<0) {error="Negative LCS dynamic-model bound radius.";return false;}
        if(!selectedModel||model.model==*selectedModel)models.push_back(model);
    }
    // Several native models have multiple compiled variants. Put the lowest
    // variant byte first so model-level instantiation selects the base mesh.
    std::stable_sort(models.begin(),models.end(),[](const auto& a,const auto& b) {
        return a.model<b.model||(a.model==b.model&&uint8_t(a.flags)<uint8_t(b.flags));
    });
    result=std::move(models);return true;
}

struct StorylandLcsDynamicBinding {
    size_t placement=0;
    std::array<float,3> scale{},offset{};
};

// The compiled vertices use their own scale/offset inside the native object frame.
inline std::array<float,16> storylandLcsNormalizedPlacement(const std::array<float,16>& native,
    const std::array<float,3>& scale,const std::array<float,3>& offset) {
    std::array<float,16> result{};
    for(size_t axis=0;axis<3;++axis)for(size_t row=0;row<3;++row)
        result[axis*4+row]=native[axis*4+row]*scale[axis];
    for(size_t row=0;row<3;++row)
        result[12+row]=native[12+row]+native[row]*offset[0]+native[4+row]*offset[1]+native[8+row]*offset[2];
    result[15]=1;return result;
}

inline bool storylandFindLcsDynamicPlacements(const StorylandArchiveBrowser& browser,uint32_t model,
    const std::array<float,16>& originalNative,std::vector<StorylandLcsDynamicBinding>& result,
    std::string& error,const std::array<float,16>* currentNative=nullptr) {
    result.clear();std::vector<StorylandLcsDynamicModel> models;
    if(!storylandDecodeLcsDynamicModels(browser,models,error,&model))return false;
    std::map<size_t,StorylandLcsDynamicBinding> matches;
    const auto equal=[](const float* a,const std::array<float,16>& b) {
        for(size_t component=0;component<16;++component)
            if(!std::isfinite(a[component])||!std::isfinite(b[component])||std::fabs(a[component]-b[component])>0.01f)return false;
        return true;
    };
    for(const auto& definition:models)if(definition.model==model) {
        const auto expected=storylandLcsNormalizedPlacement(originalNative,definition.scale,definition.offset);
        const auto current=currentNative?storylandLcsNormalizedPlacement(*currentNative,definition.scale,definition.offset):expected;
        for(size_t index=0;index<browser.placements().size();++index) {
            const auto& placement=browser.placements()[index];
            if((definition.resource<=0||placement.resourceIndex!=uint32_t(definition.resource))&&
                (definition.transparentResource<=0||placement.resourceIndex!=uint32_t(definition.transparentResource)))continue;
            if(!equal(placement.matrix,expected)&&!equal(placement.matrix,current))continue;
            const StorylandLcsDynamicBinding binding{index,definition.scale,definition.offset};
            const auto found=matches.find(index);
            if(found!=matches.end()&&(found->second.scale!=binding.scale||found->second.offset!=binding.offset)) {
                error="Ambiguous LCS native-to-render normalization.";return false;
            }
            matches[index]=binding;
        }
    }
    for(const auto& match:matches)result.push_back(match.second);
    return true;
}

inline bool storylandApplyLcsDynamicTransform(StorylandArchiveBrowser& browser,
    const std::vector<StorylandLcsDynamicBinding>& bindings,const std::array<float,16>& native) {
    for(const auto& binding:bindings)
        if(!browser.setPlacementTransform(binding.placement,
            storylandLcsNormalizedPlacement(native,binding.scale,binding.offset)))return false;
    return true;
}

inline bool storylandApplyLcsDynamicVisibility(StorylandArchiveBrowser& browser,
    const std::vector<StorylandLcsDynamicBinding>& bindings,bool visible) {
    for(const auto& binding:bindings)if(!browser.setPlacementVisibility(binding.placement,visible))return false;
    return true;
}
