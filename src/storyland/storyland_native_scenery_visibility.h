#pragma once

#include "src/leeds/leeds_archive.h"
#include "src/leeds/leeds_dtz.h"
#include "src/re/reLCS2/world/storyland_lcs_object_visibility.h"
#include "src/re/reLCS2/world/storyland_lcs_static_placement.h"
#include <set>

inline bool storylandFindNativeObjectRenderPlacements(const StorylandArchiveBrowser& browser,
    const StorylandDtzArchive& archive,const StorylandNativeCollisionInstance& instance,
    std::vector<size_t>& result,std::string& error) {
    result.clear();
    error.clear();
    if(browser.hasNativeScenery()) {
        const auto& placements=browser.placements();
        const auto& meshes=browser.worldMeshes();
        for(size_t index=0;index<placements.size();++index) {
            const auto& placement=placements[index];
            if(placement.nativeInstanceIndex!=UINT32_MAX&&
                placement.nativePoolHeader==instance.poolHeader&&placement.nativePoolIndex==instance.poolIndex&&
                placement.resourceIndex<meshes.size()&&
                meshes[placement.resourceIndex].nativeModelInfoId==int32_t(instance.modelId))
                result.push_back(index);
        }
        return true;
    }
    const auto& world=archive.nativeWorld();
    const uint64_t owner=(uint64_t(instance.poolHeader)<<32)|instance.poolIndex;
    const auto saved=world.originalScriptInstanceMatrices.find(owner);
    const auto& original=saved==world.originalScriptInstanceMatrices.end()?instance.matrix:saved->second;
    std::vector<StorylandLcsDynamicBinding> bindings;
    if(!storylandFindLcsDynamicPlacements(browser,instance.modelId,original,
        bindings,error,&instance.matrix))return false;
    if(!bindings.empty()) {
        for(const auto& binding:bindings)result.push_back(binding.placement);
        return true;
    }
    return storylandFindLcsStaticPlacements(browser,archive.unpackedBytes(),world,instance,result,error);
}

inline bool storylandApplyNativeObjectVisibility(StorylandArchiveBrowser& browser,
    const StorylandDtzArchive& archive,const std::set<uint64_t>& invisible,std::string& error) {
    error.clear();
    std::vector<size_t> hidden;
    if(browser.hasNativeScenery()) {
        const auto& placements=browser.placements();
        for(size_t index=0;index<placements.size();++index) {
            const auto& placement=placements[index];
            if(placement.nativeInstanceIndex==UINT32_MAX)continue;
            const uint64_t owner=(uint64_t(placement.nativePoolHeader)<<32)|placement.nativePoolIndex;
            if(invisible.count(owner))hidden.push_back(index);
        }
    } else {
        const auto& world=archive.nativeWorld();
        for(const auto& instance:world.instances) {
            const uint64_t owner=(uint64_t(instance.poolHeader)<<32)|instance.poolIndex;
            if(!invisible.count(owner))continue;
            std::vector<size_t> bindings;
            if(!storylandFindNativeObjectRenderPlacements(browser,archive,instance,bindings,error))return false;
            hidden.insert(hidden.end(),bindings.begin(),bindings.end());
        }
    }
    browser.clearScriptPlacementVisibility();
    for(size_t index:hidden) {
        if(!browser.setPlacementVisibility(index,false)) {
            error="Native object visibility references an unavailable render placement.";
            return false;
        }
    }
    return true;
}

inline bool storylandSetClosestNativeObjectVisibility(StorylandMissionEnvironment& world,
    const StorylandDtzArchive& archive,const std::vector<StorylandArchiveBrowser*>& browsers,
    const std::array<float,3>& point,float radius,int32_t requestedModel,bool visible,std::string& error) {
    error.clear();
    if(!std::isfinite(radius)||radius<0||
        !std::all_of(point.begin(),point.end(),[](float value) {return std::isfinite(value);})) {
        error="Closest-object visibility received an invalid search range.";
        return false;
    }
    const int32_t model=requestedModel<0&&world.resolveObjectModel?
        world.resolveObjectModel(requestedModel):requestedModel;
    if(model<0) {
        error="Closest-object visibility has an unresolved native model.";
        return false;
    }
    const bool vcs=archive.gameVariant()==StorylandDtzGameVariant::Vcs;
    StorylandLcsObjectSelection selected;
    if(!vcs)selected=storylandFindLcsClosestObject(world,archive.nativeWorld(),point,radius,model,requestedModel);
    else {
        float nearest=2*radius;
        const auto candidate=[&](float x,float y,float z,int32_t handle,uint64_t owner) {
            if(std::hypot(x-point[0],y-point[1])>=radius)return;
            const float distance=std::hypot(x-point[0],y-point[1],z-point[2]);
            if(distance<nearest) {nearest=distance;selected={handle,owner};}
        };
        for(const auto& object:world.objects)if(object.model==model||object.model==requestedModel)
            candidate(object.x,object.y,object.z,object.handle,UINT64_MAX);
        for(const auto& instance:archive.nativeWorld().instances)if(instance.modelId==uint32_t(model))
            candidate(instance.matrix[12],instance.matrix[13],instance.matrix[14],-1,
                (uint64_t(instance.poolHeader)<<32)|instance.poolIndex);
    }
    if(!selected.found())return true;
    if(selected.scriptHandle>=0) {
        if(vcs) {
            const auto object=std::find_if(world.objects.begin(),world.objects.end(),
                [&](const auto& item) {return item.handle==selected.scriptHandle;});
            if(object!=world.objects.end())object->damage.visible=visible;
        } else if(visible)world.invisibleScriptObjects.erase(selected.scriptHandle);
        else world.invisibleScriptObjects.insert(selected.scriptHandle);
        return true;
    }
    const auto& instances=archive.nativeWorld().instances;
    const auto instance=std::find_if(instances.begin(),instances.end(),[&](const auto& item) {
        return ((uint64_t(item.poolHeader)<<32)|item.poolIndex)==selected.nativeKey;
    });
    if(instance==instances.end()) {
        error="Native visibility target disappeared.";
        return false;
    }
    if(vcs) {
        bool bound=false;
        for(const auto* browser:browsers)if(browser) {
            std::vector<size_t> bindings;
            if(!storylandFindNativeObjectRenderPlacements(*browser,archive,*instance,bindings,error))return false;
            bound=bound||!bindings.empty();
        }
        if(!bound) {
            error="Native object "+std::to_string(model)+" has no decoded render binding in the loaded area.";
            return false;
        }
    }
    if(visible)world.invisibleNativeObjects.erase(selected.nativeKey);
    else world.invisibleNativeObjects.insert(selected.nativeKey);
    world.nativeObjectVisibilityChanged=true;
    return true;
}
