#pragma once

#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_script_object.h"
#include "src/re/reVCS/runtime/storyland_vcs_attachment_state.h"
#include <cstring>

inline bool storylandEquipVcsAttachmentWeapon(StorylandMissionEnvironment& world,
    int32_t handle, int32_t type, StorylandVcsCharacterAttachment& attachment,
    std::string& error) {
    const auto definition = world.playerWeapons.find(type);
    const auto character = world.missionCharacters.find(handle);
    if ((handle != 0 && character == world.missionCharacters.end()) ||
        definition == world.playerWeapons.end() || definition->second.slot < 0) {
        error = "VCS attachment weapon requires a native character and weapon definition.";
        return false;
    }
    const auto equip = [&](auto& inventory, int32_t& selected,
        bool& changed, bool& selectionChanged) {
        if (attachment.savedWeapon < 0) {
            attachment.savedWeapon = selected;
            const auto previous = inventory.find(selected);
            attachment.savedAmmunition = previous == inventory.end() ? 0 : previous->second.ammunition;
        }
        attachment.savedSlotWeapon = -1;
        attachment.savedSlotAmmunition = 0;
        int32_t previousAmmunition = 0;
        for (const auto& entry : inventory) {
            if (!entry.second.owned || entry.second.slot != definition->second.slot) continue;
            attachment.savedSlotWeapon = entry.first;
            attachment.savedSlotAmmunition = uint16_t(entry.second.ammunition);
            previousAmmunition = entry.second.ammunition;
            break;
        }
        int32_t ammunition = 30000;
        const bool sameType = attachment.savedSlotWeapon == type;
        const bool mergeSlot = definition->second.slot >= 4 && definition->second.slot <= 6;
        if (sameType || (attachment.savedSlotWeapon > 0 && mergeSlot)) {
            const uint32_t sum = uint32_t(previousAmmunition) + 30000u;
            std::memcpy(&ammunition, &sum, sizeof(ammunition));
            if (ammunition >= 10000 && ammunition < 20000) ammunition = 9999;
        }
        ammunition = std::min(ammunition, 99999);
        for (auto& entry : inventory) {
            if (entry.first == type || entry.second.slot != definition->second.slot) continue;
            entry.second.owned = false;
            entry.second.ammunition = 0;
        }
        auto& weapon = inventory[type];
        weapon.slot = definition->second.slot;
        weapon.model = definition->second.model;
        weapon.owned = true;
        weapon.ammunition = ammunition;
        selected = type;
        attachment.attachedWeapon = type;
        changed = true;
        selectionChanged = true;
    };
    if (handle == 0)
        equip(world.playerWeapons, world.currentWeapon,
            world.weaponInventoryChanged, world.weaponSelectionRequested);
    else
        equip(character->second.weapons, character->second.currentWeapon,
            character->second.weaponsChanged, character->second.weaponSelectionChanged);
    return true;
}

inline void storylandRestoreVcsAttachmentWeapon(StorylandMissionEnvironment& world,
    int32_t handle, const StorylandVcsCharacterAttachment& attachment) {
    if (attachment.savedWeapon < 0) return;
    const auto character = world.missionCharacters.find(handle);
    if (handle != 0 && character == world.missionCharacters.end()) return;
    const auto restore = [&](auto& inventory, int32_t& selected,
        bool& changed, bool& selectionChanged) {
        const auto temporary = inventory.find(attachment.attachedWeapon);
        const int32_t temporarySlot = temporary == inventory.end() ? -1 : temporary->second.slot;
        for (auto& entry : inventory) {
            if (entry.second.slot != temporarySlot) continue;
            entry.second.owned = false;
            entry.second.ammunition = 0;
        }
        const auto restoreWeapon = [&](int32_t type, int32_t ammunition) {
            if (type < 0) return;
            const auto definition = world.playerWeapons.find(type);
            if (definition == world.playerWeapons.end()) return;
            auto& weapon = inventory[type];
            weapon.slot = definition->second.slot;
            weapon.model = definition->second.model;
            weapon.owned = true;
            weapon.ammunition = ammunition;
        };
        restoreWeapon(attachment.savedSlotWeapon, attachment.savedSlotAmmunition);
        restoreWeapon(attachment.savedWeapon, attachment.savedAmmunition);
        selected = attachment.savedWeapon;
        changed = true;
        selectionChanged = true;
    };
    if (handle == 0)
        restore(world.playerWeapons, world.currentWeapon,
            world.weaponInventoryChanged, world.weaponSelectionRequested);
    else
        restore(character->second.weapons, character->second.currentWeapon,
            character->second.weaponsChanged, character->second.weaponSelectionChanged);
}

inline void storylandReleaseVcsCharacterAttachmentState(StorylandMissionEnvironment& world,
    int32_t handle) {
    const auto character = world.missionCharacters.find(handle);
    const auto attachment = world.vcsCharacterAttachments.find(handle);
    if (attachment != world.vcsCharacterAttachments.end()) {
        storylandRestoreVcsAttachmentWeapon(world, handle, attachment->second);
        world.vcsCharacterAttachments.erase(attachment);
    }
    world.vcsCharacterObjectParents.erase(handle);
    world.mountedGunRequests.erase(std::remove_if(world.mountedGunRequests.begin(),
        world.mountedGunRequests.end(), [handle](const auto& request) {
            return request.character == handle;
        }), world.mountedGunRequests.end());
    if (handle == 0) {
        world.playerAttachedVehicle = -1;
        world.playerAttachment = {};
        world.playerAttachedPitchLimits = {};
        world.playerCollisionEnabled = true;
        world.manualAim.initialized = false;
        return;
    }
    if (character == world.missionCharacters.end()) return;
    character->second.vehicle = -1;
    character->second.passengerSeat = -1;
    character->second.attached = false;
    character->second.attachment = {};
    if (character->second.alive && character->second.health > 0) {
        character->second.animationId = -1;
        character->second.animationGroup = 0;
        world.scriptedCharacterAnimations.erase(handle);
    }
}

inline void storylandDetachVcsCharacter(StorylandMissionEnvironment& world, int32_t handle) {
    const auto objectParent = world.vcsCharacterObjectParents.find(handle);
    const auto character = world.missionCharacters.find(handle);
    const bool dead = handle == 0 ? world.health <= 0 :
        character != world.missionCharacters.end() && character->second.health <= 0;
    if (objectParent != world.vcsCharacterObjectParents.end() && dead) {
        objectParent->second = -1;
        if (character != world.missionCharacters.end()) character->second.attached = false;
        return;
    }
    storylandReleaseVcsCharacterAttachmentState(world, handle);
}

inline bool storylandVcsCharacterEntityFrame(const StorylandMissionEnvironment& world,
    int32_t handle, std::array<float,16>& result, float* limitedHeading = nullptr) {
    const bool player = handle == 0;
    const auto found = world.missionCharacters.find(handle);
    if (!player && found == world.missionCharacters.end()) return false;
    const auto attachment = world.vcsCharacterAttachments.find(handle);
    const auto objectParent = world.vcsCharacterObjectParents.find(handle);
    const bool invalidatedObject = objectParent != world.vcsCharacterObjectParents.end() &&
        objectParent->second < 0;
    if (attachment != world.vcsCharacterAttachments.end() && !invalidatedObject) {
        if (!player && !found->second.attached) return false;
        std::array<float,16> parentFrame{};
        if (objectParent != world.vcsCharacterObjectParents.end()) {
            const auto object = std::find_if(world.objects.begin(), world.objects.end(),
                [&](const auto& candidate) { return candidate.handle == objectParent->second; });
            if (object == world.objects.end()) return false;
            parentFrame = storylandScriptObjectMatrix(object->x, object->y, object->z,
                object->rotationX, object->rotationY, object->heading);
        } else {
            const int32_t vehicle = player ? world.playerAttachedVehicle : found->second.vehicle;
            const auto car = world.missionVehicles.find(vehicle);
            if (car == world.missionVehicles.end() || car->second.deleted ||
                !world.vehicleAttachmentFrame || !world.vehicleAttachmentFrame(vehicle, parentFrame))
                return false;
        }
        return storylandVcsAttachedCharacterFrame(parentFrame,
            player ? world.playerAttachment : found->second.attachment, attachment->second,
            !player && found->second.mountedGunInitialized,
            player ? world.heading : found->second.heading, result, limitedHeading);
    }
    if (world.activeVcsCharacterAttachment(handle) || (!player && found->second.attached)) return false;
    const float heading = player ? world.heading : found->second.heading;
    const float x = player ? world.x : found->second.x;
    const float y = player ? world.y : found->second.y;
    const float z = player ? world.z : found->second.z;
    float originOffset = 0;
    if (!world.characterOriginOffset || !world.characterOriginOffset(handle, originOffset) ||
        !std::isfinite(originOffset) || !std::isfinite(heading) ||
        !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
    const float angle = heading * 0.0174532942473888397216796875f;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    result = {cosine, sine, 0, 0, -sine, cosine, 0, 0, 0, 0, 1, 0,
        x, y, z + originOffset, 1};
    if (limitedHeading) *limitedHeading = heading;
    return true;
}

inline bool storylandPositionVcsAttachedCharacter(StorylandMissionEnvironment& world,
    int32_t handle) {
    const bool player = handle == 0;
    const auto found = world.missionCharacters.find(handle);
    const auto attachment = world.vcsCharacterAttachments.find(handle);
    if (attachment == world.vcsCharacterAttachments.end() ||
        (!player && found == world.missionCharacters.end())) return false;
    std::array<float,16> frame{};
    float originOffset = 0;
    float limitedHeading = 0;
    if (!storylandVcsCharacterEntityFrame(world, handle, frame, &limitedHeading) ||
        !world.characterOriginOffset || !world.characterOriginOffset(handle, originOffset) ||
        !std::isfinite(originOffset)) return false;
    float& x = player ? world.x : found->second.x;
    float& y = player ? world.y : found->second.y;
    float& z = player ? world.z : found->second.z;
    float& heading = player ? world.heading : found->second.heading;
    x = frame[12];
    y = frame[13];
    z = frame[14] - originOffset;
    if (player || !found->second.mountedGunInitialized) heading = limitedHeading;
    return true;
}
