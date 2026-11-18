#pragma once
#include "src/leeds/leeds_animation_groups.h"
#include "src/storyland/storyland_mission_vm.h"
#include <set>

inline bool storylandVcsScriptStreamingBacklogged(const std::vector<uint8_t>& bytes,
    const StorylandMissionEnvironment& world,bool ioBusy,bool& backlogged,std::string& error) {
    error.clear();backlogged=ioBusy;
    if(ioBusy)return true;
    std::set<int32_t> pendingModels;
    for(const auto* requests:{&world.requestedModels,&world.empireRequestedModels})
        for(int32_t model:*requests)if(model>=0&&!world.residentModels.count(model))pendingModels.insert(model);
    if(pendingModels.size()>=6) {backlogged=true;return true;}
    std::set<std::string> pendingBanks;
    const auto requestBank=[&](std::string bank) {
        bank=StorylandAnimationGroups::key(std::move(bank));
        if(bank.empty()||!world.animationBankLoaded) {
            error="Streaming backlog requires a valid native animation-bank residency binding.";return false;
        }
        if(!world.animationBankLoaded(bank))pendingBanks.insert(std::move(bank));
        return true;
    };
    for(const auto& bank:world.requestedAnimationBanks)if(!requestBank(bank))return false;
    StorylandAnimationGroups groups;bool groupsLoaded=false;
    for(int32_t group:world.requestedAnimationGroups) {
        if(world.animationGroupLoaded&&world.animationGroupLoaded(group))continue;
        if(!groupsLoaded) {
            if(!groups.load(bytes,true)) {
                error="Streaming backlog requires the native animation group-to-bank registry.";return false;
            }
            groupsLoaded=true;
        }
        const auto found=groups.groups.find(uint32_t(group));
        if(group<0||found==groups.groups.end()) {
            error="Streaming backlog contains an invalid native animation-group request.";return false;
        }
        if(!requestBank(found->second.bank))return false;
    }
    // Native 12C1E0 returns true for active I/O or at least six pending
    // resources. Groups sharing one animation block are one resource.
    backlogged=pendingModels.size()+pendingBanks.size()>=6;return true;
}
