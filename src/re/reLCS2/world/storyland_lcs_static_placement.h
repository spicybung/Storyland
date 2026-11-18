#pragma once
#include "src/re/reLCS2/world/storyland_lcs_dynamic_placement.h"
#include <set>

// Retail LCS static meshes use the collision-sphere center as their local
// origin and a power-of-two scale enclosing each collision-box axis. This
// normalization is independently recoverable from CBaseModelInfo/CColModel;
// a native pivot or a nearby compiled bounding sphere is not an identity.
inline bool storylandDecodeLcsStaticNormalization(const std::vector<uint8_t>& bytes,
    uint32_t model,std::array<float,3>& scale,std::array<float,3>& offset) {
    const auto range=[&](uint64_t at,uint64_t length) {
        return at<=bytes.size()&&length<=bytes.size()-at;
    };
    const auto word=[&](size_t at) {
        return uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8|
            uint32_t(bytes[at+2])<<16|uint32_t(bytes[at+3])<<24;
    };
    const auto real=[&](size_t at) {
        const uint32_t bits=word(at);float value;std::memcpy(&value,&bits,4);return value;
    };
    if(!range(0,0x40)||model>=word(0x38))return false;
    const uint64_t table=word(0x3C),row=table+uint64_t(model)*4;
    if(table<0x20||!range(row,4))return false;
    const uint32_t info=word(size_t(row));
    if(info<0x20||!range(info,0x18)||(bytes[size_t(info)+0x10]!=1&&bytes[size_t(info)+0x10]!=3))return false;
    const uint32_t collision=word(size_t(info)+0x14);
    if(collision<0x20||!range(collision,0x2C))return false;
    std::array<float,3> decodedScale{},decodedOffset{};
    for(size_t axis=0;axis<3;++axis) {
        const float center=real(size_t(collision)+axis*4);
        const float minimum=real(size_t(collision)+0x10+axis*4);
        const float maximum=real(size_t(collision)+0x20+axis*4);
        if(!std::isfinite(center)||!std::isfinite(minimum)||!std::isfinite(maximum)||
            minimum>maximum||std::fabs(center)>1000000)return false;
        const float extent=std::max(std::fabs(minimum-center),std::fabs(maximum-center));
        if(!(extent>0)||extent>1000000)return false;
        decodedOffset[axis]=center;
        decodedScale[axis]=std::exp2(std::ceil(std::log2(extent)));
        if(!std::isfinite(decodedScale[axis]))return false;
    }
    scale=decodedScale;offset=decodedOffset;return true;
}

inline bool storylandLcsStaticAffineEqual(const float* matrix,
    const std::array<float,16>& expected,const std::array<float,3>& scale) {
    for(size_t axis=0;axis<3;++axis)for(size_t component=0;component<4;++component) {
        const size_t index=axis*4+component;
        // Compare the complete basis. The tolerance covers native float
        // matrix optimization, not a position-radius search.
        if(!std::isfinite(matrix[index])||!std::isfinite(expected[index])||
            std::fabs(matrix[index]-expected[index])>scale[axis]*0.00001f)return false;
    }
    for(size_t component=12;component<16;++component)
        if(!std::isfinite(matrix[component])||!std::isfinite(expected[component])||
            std::fabs(matrix[component]-expected[component])>0.001f)return false;
    return true;
}

inline bool storylandFindLcsStaticPlacements(const StorylandArchiveBrowser& browser,
    const std::vector<uint8_t>& bytes,const StorylandNativeWorld& world,
    const StorylandNativeCollisionInstance& instance,std::vector<size_t>& result,std::string& error) {
    result.clear();error.clear();
    // The archive must have a validated LCS world layout. VCS has its own
    // native identities; an undecoded archive cannot report a visual bind.
    if(browser.nativeWorldMasterOffset()==UINT32_MAX)return true;
    std::array<float,3> scale{},offset{};
    if(!storylandDecodeLcsStaticNormalization(bytes,instance.originalModelId,scale,offset)) {
        error="LCS static model has no validated native normalization.";return false;
    }
    const uint64_t key=(uint64_t(instance.poolHeader)<<32)|instance.poolIndex;
    const auto saved=world.originalScriptInstanceMatrices.find(key);
    const auto& original=saved==world.originalScriptInstanceMatrices.end()?instance.matrix:saved->second;
    const auto expected=storylandLcsNormalizedPlacement(original,scale,offset);
    const auto current=storylandLcsNormalizedPlacement(instance.matrix,scale,offset);
    std::vector<size_t> matches;std::set<uint32_t> identities;
    for(size_t index=0;index<browser.placements().size();++index) {
        const auto& placement=browser.placements()[index];
        if(placement.imgOffset==UINT64_MAX)continue; // Exact model-ID dynamic binding owns these.
        if(!storylandLcsStaticAffineEqual(placement.matrix,expected,scale)&&
            !storylandLcsStaticAffineEqual(placement.matrix,current,scale))continue;
        matches.push_back(index);identities.insert(placement.iplId);
    }
    if(matches.empty())return true;
    if(identities.size()!=1) {
        error="Ambiguous LCS static render identities share the native normalization.";return false;
    }
    if(instance.modelId!=instance.originalModelId) {
        error="A replaced LCS static model has no verified render-variant identity.";return false;
    }
    // The inverse must be unique too: coincident LODs, timed replacements,
    // and native models sharing collision bounds must not hide one another.
    for(const auto& other:world.instances) {
        const uint64_t otherKey=(uint64_t(other.poolHeader)<<32)|other.poolIndex;
        if(otherKey==key)continue;
        std::array<float,3> otherScale{},otherOffset{};
        if(!storylandDecodeLcsStaticNormalization(bytes,other.originalModelId,otherScale,otherOffset))continue;
        const auto previous=world.originalScriptInstanceMatrices.find(otherKey);
        const auto& matrix=previous==world.originalScriptInstanceMatrices.end()?other.matrix:previous->second;
        const auto candidate=storylandLcsNormalizedPlacement(matrix,otherScale,otherOffset);
        const auto currentCandidate=storylandLcsNormalizedPlacement(other.matrix,otherScale,otherOffset);
        for(size_t index:matches)if(storylandLcsStaticAffineEqual(browser.placements()[index].matrix,candidate,otherScale)||
            storylandLcsStaticAffineEqual(browser.placements()[index].matrix,currentCandidate,otherScale)) {
            error="Ambiguous native entities share an LCS static render identity.";return false;
        }
    }
    for(size_t index:matches) {
        const auto& placement=browser.placements()[index];
        const bool decoded=std::any_of(browser.worldMeshes().begin(),browser.worldMeshes().end(),[&](const auto& mesh) {
            return mesh.sectorIndex==placement.sectorIndex&&mesh.resourceIndex==placement.resourceIndex&&
                (mesh.emptyGeometry||(!mesh.vertices.empty()&&!mesh.triangles.empty()));
        });
        if(!decoded) {error="LCS static render identity has no decoded geometry.";return false;}
    }
    result=std::move(matches);return true;
}

inline bool storylandApplyLcsStaticVisibility(StorylandArchiveBrowser& browser,
    const std::vector<size_t>& bindings,bool visible) {
    for(size_t index:bindings)if(!browser.setPlacementVisibility(index,visible))return false;
    return true;
}
