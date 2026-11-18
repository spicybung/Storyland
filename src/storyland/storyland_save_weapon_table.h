#pragma once
#include "src/storyland/storyland_save.h"

namespace storyland_save {
inline bool restoreWeaponTable(State& state,
    const std::vector<StorylandWeaponDefinition>& definitions,std::string& error) {
    if(state.weaponIds.size()==definitions.size()) {
        for(size_t index=0;index<state.weaponIds.size();++index) {
            if(state.weaponIds[index]!=definitions[index].nativeId) {
                error="Save weapon assignments do not match the loaded game.";
                return false;
            }
        }
        error.clear();
        return true;
    }
    if(!state.mission.vcs||state.loadedVersion!=53||state.weaponIds.size()!=39||
        definitions.size()!=40) {
        error="Save weapon table does not match the loaded game.";
        return false;
    }
    for(size_t index=0;index<definitions.size();++index) {
        if(definitions[index].nativeId!=int32_t(index)||
            (index<state.weaponIds.size()&&state.weaponIds[index]!=int32_t(index))) {
            error="Save weapon assignments do not match the loaded game.";
            return false;
        }
    }
    if(state.inventory.size()!=39||state.equipped>=39||
        (!state.confiscated.empty()&&state.confiscated.size()!=39)||
        std::any_of(state.dropped.begin(),state.dropped.end(),
            [](const auto& pickup){return pickup.weapon>=39;})) {
        error="Legacy VCS save weapon inventory is inconsistent.";
        return false;
    }
    auto identifiers=state.weaponIds;
    auto inventory=state.inventory;
    auto confiscated=state.confiscated;
    identifiers.push_back(39);
    inventory.emplace_back();
    if(!confiscated.empty())confiscated.emplace_back();
    state.weaponIds.swap(identifiers);
    state.inventory.swap(inventory);
    state.confiscated.swap(confiscated);
    error.clear();
    return true;
}
}
