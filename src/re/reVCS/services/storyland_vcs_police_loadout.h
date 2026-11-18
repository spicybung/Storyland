#pragma once

#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_weapon_types.h"
#include <algorithm>
#include <vector>

// VCS CCopPed constructor 324D08 configures these exact resident models.
// Normal and motorcycle police keep the base constructor's active slot;
// tactical, federal, army and vice officers select their issued firearm.
inline void storylandInitializeVcsPoliceLoadout(StorylandMissionEnvironment::Character& character,
    const std::vector<StorylandWeaponDefinition>& definitions) {
    if (character.type != 6) return;
    int32_t firearm = -1;
    bool hasBaton = false;
    bool selectFirearm = false;
    switch (character.model) {
    case 8: case 119:
        firearm = 19;
        hasBaton = true;
        character.accuracy = 75;
        character.armour = 0;
        break;
    case 9:
        firearm = 25;
        selectFirearm = true;
        character.accuracy = 85;
        character.armour = 50;
        break;
    case 10:
        firearm = 27;
        selectFirearm = true;
        character.accuracy = 95;
        character.armour = 100;
        break;
    case 11:
        firearm = 27;
        selectFirearm = true;
        character.accuracy = 100;
        character.armour = 100;
        break;
    case 117: case 118:
        firearm = 25;
        selectFirearm = true;
        character.accuracy = 95;
        character.armour = 100;
        break;
    default:
        return;
    }
    const auto give = [&](int32_t nativeWeapon) {
        const auto found = std::find_if(definitions.begin(), definitions.end(),
            [nativeWeapon](const auto& value) { return value.nativeId == nativeWeapon; });
        if (found == definitions.end()) return false;
        auto& weapon = character.weapons[nativeWeapon];
        weapon.slot = found->slot;
        weapon.model = found->modelId;
        weapon.ammunition = 1000;
        weapon.owned = true;
        character.weaponsChanged = true;
        return true;
    };
    if (hasBaton) give(4);
    if (give(firearm) && selectFirearm) {
        character.currentWeapon = firearm;
        character.weaponSelectionChanged = true;
    }
}
