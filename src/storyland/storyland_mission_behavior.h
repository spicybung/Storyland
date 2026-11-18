#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include <algorithm>
#include <cmath>

inline bool storylandMissionPedGangDamageAllowed(bool vcs, uint16_t mask, int32_t attackerType) {
    // LCS 0x1EAD78 recognizes types 7..15; 0x31E6B8 tests the victim's mask.
    if (attackerType < 7 || attackerType >= (vcs ? 23 : 16)) return true;
    return (mask & (uint16_t(1) << (attackerType - 7))) != 0;
}

inline bool storylandMissionPedCanAttackPlayer(const StorylandMissionEnvironment& world,
    int32_t handle, bool meleeWeapon) {
    const auto actor = world.missionCharacters.find(handle);
    if (actor == world.missionCharacters.end()) return false;
    const auto& character = actor->second;
    // Native 100D60/101F40 applies the player's ignore flag to active kill objectives too.
    if (world.everyoneIgnoresPlayer) return false;
    // Native 219A78 allows cops, melee attacks and the explicit mission flag.
    if (character.type == 6 || meleeWeapon || character.creationSource != 2 ||
        character.behavior.canAttackPlayerWithCops) return true;
    for (const auto& entry : world.missionCharacters) {
        const auto& other = entry.second;
        if (entry.first == handle || !other.alive || other.health <= 0 ||
            other.currentWeapon < 19 || other.currentWeapon >= 36 ||
            std::hypot(world.x - other.x, world.y - other.y, world.z - other.z) >= 30)
            continue;
        const auto target = world.characterTargets.find(entry.first);
        if (target != world.characterTargets.end() &&
            target->second.kind == StorylandMissionEnvironment::TargetKind::Character &&
            target->second.handle == 0) return false;
    }
    return true;
}

inline bool storylandLcsYieldPlayerAttackToPolice(StorylandMissionEnvironment& world,
    int32_t handle, bool policePursuing) {
    const auto actor = world.missionCharacters.find(handle);
    if (!policePursuing || actor == world.missionCharacters.end()) return false;
    auto& character = actor->second;
    // SLUS 0x31CDFC checks player target, cop type, mission ownership and bit34.
    if (character.type == 6 || character.creationSource == 2 ||
        character.behavior.canAttackPlayerWithCops) return false;
    StorylandLcsEntityExtensionState::Task flee;
    flee.objective = 3;
    flee.destination = {character.x, character.y, character.z};
    flee.actionStartedAt = world.gameMilliseconds;
    flee.actionDuration = 10.0f;
    world.lcsEntityExtension.tasks[handle] = flee;
    character.objective = 0x0198;
    character.objectiveTarget = -1;
    character.objectiveCompleted = false;
    character.walkingToTarget = false;
    character.followingScriptPath = false;
    character.targetX = character.x;
    character.targetY = character.y;
    character.objectiveHeight = character.z;
    character.objectiveRadius = flee.radius;
    character.targetRadius = flee.radius;
    character.walkSpeed = flee.speed;
    character.animationId = -1;
    character.attackStartedAt = 0;
    character.seekRoute.clear();
    character.seekRouteCursor = 0;
    character.seekNode = UINT32_MAX;
    world.characterWanderTasks.erase(handle);
    world.characterTargets.erase(handle);
    world.characterVehicleTasks.erase(handle);
    return true;
}

inline bool storylandMissionPedCanDuckAtThreat(const StorylandMissionEnvironment::Character& character,
    bool armedThreat) {
    // Native 1009F8 fixes the no-duck flag for mission cops/gangs unless 0543 permits normal handling.
    const bool restrictedType = character.type == 6 || (character.type >= 7 && character.type < 16);
    return !(armedThreat && character.creationSource == 2 && restrictedType &&
        !character.behavior.allowDuckAgainstArmedTargets);
}

inline float storylandMissionPedInstantHitDamage(const StorylandMissionEnvironment::Character& character,
    int32_t weapon, float damage) {
    // 2C6B84 selects the same saturated fourfold branch used for uncontrolled/crouched victims.
    if (character.behavior.useUncontrolledWeaponDamage && weapon >= 19 && weapon < 36)
        return std::min(255.0f, damage * 4.0f);
    return damage;
}
