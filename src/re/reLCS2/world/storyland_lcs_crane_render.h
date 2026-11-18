#pragma once
#include "src/re/reLCS2/world/storyland_lcs_static_placement.h"

// Native crane buildings can be compiled into either static sector resources
// or sDynamic resources. Both need their verified mesh normalization retained
// when the controller rotates the native building frame.
inline bool storylandFindLcsCraneRenderBindings(const StorylandArchiveBrowser& browser,
    const std::vector<uint8_t>& data,const StorylandNativeWorld& world,
    const StorylandNativeCollisionInstance& instance,const std::array<float,16>& original,
    std::vector<StorylandLcsDynamicBinding>& result,std::string& error) {
    result.clear();error.clear();
    if(browser.hasNativeScenery()) {
        const auto& placements=browser.placements();
        const auto& meshes=browser.worldMeshes();
        for(size_t index=0;index<placements.size();++index) {
            const auto& placement=placements[index];
            if(placement.nativeInstanceIndex==UINT32_MAX||placement.nativePoolHeader!=instance.poolHeader||
                placement.nativePoolIndex!=instance.poolIndex||placement.resourceIndex>=meshes.size())continue;
            const auto& mesh=meshes[placement.resourceIndex];
            if(mesh.nativeModelInfoId!=int32_t(instance.modelId))continue;
            if(mesh.vertices.empty()||mesh.triangles.empty()) {
                if(mesh.emptyGeometry)continue;
                error="Native IMG crane placement has no decoded geometry.";return false;
            }
            result.push_back({index,{1,1,1},{0,0,0}});
        }
        return true;
    }
    std::vector<StorylandLcsDynamicBinding> dynamic;
    if(!storylandFindLcsDynamicPlacements(browser,instance.modelId,original,dynamic,error,&instance.matrix))return false;
    std::map<size_t,StorylandLcsDynamicBinding> candidates;
    for(const auto& binding:dynamic)candidates.emplace(binding.placement,binding);
    std::array<float,3> scale{},offset{};
    if(storylandDecodeLcsStaticNormalization(data,instance.originalModelId,scale,offset)) {
        std::vector<size_t> statics;
        if(!storylandFindLcsStaticPlacements(browser,data,world,instance,statics,error))return false;
        for(size_t index:statics) {
            const StorylandLcsDynamicBinding binding{index,scale,offset};
            const auto previous=candidates.find(index);
            if(previous!=candidates.end()&&(previous->second.scale!=scale||previous->second.offset!=offset)) {
                error="Crane render identity has conflicting native normalizations.";return false;
            }
            candidates[index]=binding;
        }
    }
    std::vector<StorylandLcsDynamicBinding> verified;
    for(const auto& item:candidates) {
        const auto& binding=item.second;
        if(binding.placement>=browser.placements().size()) {
            error="Crane render placement is no longer available.";return false;
        }
        const auto& placement=browser.placements()[binding.placement];
        bool decoded=false,renderable=false;
        for(const auto& mesh:browser.worldMeshes())
            if(mesh.sectorIndex==placement.sectorIndex&&mesh.resourceIndex==placement.resourceIndex) {
                decoded=decoded||mesh.emptyGeometry||(!mesh.vertices.empty()&&!mesh.triangles.empty());
                renderable=renderable||(!mesh.vertices.empty()&&!mesh.triangles.empty());
            }
        if(!decoded) {error="Crane render identity has no decoded geometry.";return false;}
        if(renderable)verified.push_back(binding);
    }
    result=std::move(verified);return true;
}
