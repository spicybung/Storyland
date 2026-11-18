#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_script_object.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

// Native WeaponInfo only merges ammunition when replacing a shotgun, SMG, or rifle.
inline bool storylandLcsWeaponSlotMergesAmmo(int32_t slot) {
    return slot==4||slot==5||slot==6;
}

// Negative vehicle handles below -1 represent LCS object parents. This preserves
// attachment references in the existing serialized character vehicle field.
inline bool storylandLcsAttachmentParentFrame(const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::Character& character,std::array<float,16>& frame) {
    if(character.vehicle >= 0)
        return world.vehicleAttachmentFrame && world.vehicleAttachmentFrame(character.vehicle,frame);
    if(character.vehicle == -1)return false;
    const int64_t handle=-int64_t(character.vehicle)-1;
    const auto object=std::find_if(world.objects.begin(),world.objects.end(),
        [&](const auto& candidate){return candidate.handle==handle;});
    if(object==world.objects.end())return false;
    frame=storylandScriptObjectMatrix(object->x,object->y,object->z,
        object->rotationX,object->rotationY,object->heading);
    return std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);});
}

inline bool storylandLcsAttachedCharacterFrame(const std::array<float,16>& vehicle,
    const std::array<float,3>& offset,float heading,std::array<float,16>& result) {
    if(!std::isfinite(heading)||!std::all_of(vehicle.begin(),vehicle.end(),[](float v){return std::isfinite(v);})||
        !std::all_of(offset.begin(),offset.end(),[](float v){return std::isfinite(v);}))return false;
    const float objectAngle=std::atan2(-vehicle[4],vehicle[5]);
    const float angle=heading*0.0174532925199433f-objectAngle;
    const float cosine=std::cos(angle),sine=std::sin(angle);
    result=vehicle;
    for(size_t row=0;row<3;++row) {
        result[row]=cosine*vehicle[row]+sine*vehicle[4+row];
        result[4+row]=-sine*vehicle[row]+cosine*vehicle[4+row];
        result[12+row]=vehicle[12+row]+vehicle[row]*offset[0]+vehicle[4+row]*offset[1]+vehicle[8+row]*offset[2];
    }
    return std::all_of(result.begin(),result.end(),[](float v){return std::isfinite(v);});
}

inline bool storylandEquipAttachmentWeapon(StorylandMissionEnvironment& world,int32_t handle,
    int32_t type,StorylandMissionEnvironment::CharacterAttachment& attachment,std::string& error) {
    if(handle!=1&&!world.missionCharacters.count(handle)) {
        error="Attaching a weapon requires an existing character.";return false;
    }
    const auto definition=world.playerWeapons.find(type);
    if(type<0||type>=37||(type!=0&&(definition==world.playerWeapons.end()||definition->second.slot<0))) {
        error="The attached character weapon has no native weapon definition.";return false;
    }
    auto& inventory=handle==1?world.playerWeapons:world.characterWeapons[handle];
    auto& selected=handle==1?world.currentWeapon:world.characterCurrentWeapons[handle];
    if(attachment.savedWeapon<0) {
        attachment.savedWeapon=selected;
        const auto previous=inventory.find(selected);
        attachment.savedAmmo=previous==inventory.end()?0:previous->second.ammunition;
    }
    StorylandMissionEnvironment::Weapon replacement;
    if(definition!=world.playerWeapons.end())replacement=definition->second;
    else replacement.slot=0;
    const auto current=inventory.find(type);
    const bool alreadyOwned=current!=inventory.end()&&current->second.owned;
    int64_t ammunition=30000;
    if(alreadyOwned)ammunition=std::min<int64_t>(99999,int64_t(current->second.ammunition)+ammunition);
    for(auto& entry:inventory)if(entry.first!=type&&entry.second.owned&&entry.second.slot==replacement.slot) {
        if(!alreadyOwned&&storylandLcsWeaponSlotMergesAmmo(replacement.slot))ammunition+=entry.second.ammunition;
        entry.second.owned=false;entry.second.ammunition=0;
    }
    replacement.owned=true;
    replacement.ammunition=int32_t(std::min<int64_t>(std::numeric_limits<int32_t>::max(),ammunition));
    inventory[type]=replacement;selected=type;
    if(handle==1) {world.weaponInventoryChanged=true;world.weaponSelectionRequested=true;}
    return true;
}

inline void storylandRestoreAttachmentWeapon(StorylandMissionEnvironment& world,int32_t handle,
    StorylandMissionEnvironment::CharacterAttachment& attachment) {
    if(attachment.savedWeapon<0)return;
    auto& inventory=handle==1?world.playerWeapons:world.characterWeapons[handle];
    auto& selected=handle==1?world.currentWeapon:world.characterCurrentWeapons[handle];
    const auto current=inventory.find(selected);
    if(current!=inventory.end())current->second.ammunition=0;
    const auto definition=world.playerWeapons.find(attachment.savedWeapon);
    auto& restored=inventory[attachment.savedWeapon];
    if(definition!=world.playerWeapons.end()) {
        restored.slot=definition->second.slot;restored.model=definition->second.model;
    } else if(attachment.savedWeapon==0)restored.slot=0;
    // Native SetCurrentWeapon resolves the stored type to a slot. Restore the recorded
    // type as well, so a same-slot replacement cannot retain the attachment-only weapon.
    for(auto& entry:inventory)if(entry.first!=attachment.savedWeapon&&entry.second.slot==restored.slot) {
        entry.second.owned=false;entry.second.ammunition=0;
    }
    restored.owned=true;restored.ammunition=attachment.savedAmmo;
    selected=attachment.savedWeapon;attachment.savedWeapon=-1;attachment.savedAmmo=0;
    if(handle==1) {world.weaponInventoryChanged=true;world.weaponSelectionRequested=true;}
}
