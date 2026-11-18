#pragma once

#include "src/storyland/storyland_character_audio.h"
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/re/reVCS/services/storyland_vcs_police_response.h"
#include <algorithm>
#include <cmath>

inline void storylandVcsPoliceRememberAssailant(StorylandPlayTestRuntime& runtime,
    size_t victimIndex, int attackerIndex, bool playerAttack, bool damageApplied) {
    if (!runtime.crime.vcs || victimIndex >= runtime.pedestrians.size() || !damageApplied) return;
    auto& officer = runtime.pedestrians[victimIndex];
    if (!officer.isPolice(true) || officer.scriptHandle >= 0) return;
    if (playerAttack) {
        officer.policeAssailantIdentity = 0;
        officer.policeAssailantMissionHandle = -1;
        officer.policeAssailantUntil = 0;
        return;
    }
    if (officer.health <= 0 || attackerIndex < 0 || size_t(attackerIndex) >= runtime.pedestrians.size() ||
        size_t(attackerIndex) == victimIndex || !std::isfinite(runtime.elapsed)) return;
    auto& attacker = runtime.pedestrians[size_t(attackerIndex)];
    if (attacker.scriptHandle >= 0 || attacker.health <= 0 || attacker.isPolice(true)) return;
    if (!attacker.speechIdentity) attacker.speechIdentity = storylandNewCharacterSpeechIdentity();
    officer.policeAssailantIdentity = attacker.speechIdentity;
    officer.policeAssailantMissionHandle = -1;
    officer.policeAssailantUntil = runtime.elapsed + storylandVcsPoliceResponse::OfficerPursuitMemory;
    officer.clearFlee();
    officer.state = StorylandPedState::Combat;
}

inline void storylandVcsPoliceRememberMissionAssailant(StorylandPlayTestRuntime& runtime,
    size_t victimIndex, int32_t attackerHandle, bool damageApplied) {
    if (!runtime.crime.vcs || !runtime.missionEnvironment || !damageApplied ||
        victimIndex >= runtime.pedestrians.size() || attackerHandle <= 0 ||
        !std::isfinite(runtime.elapsed)) return;
    auto& officer = runtime.pedestrians[victimIndex];
    if (officer.scriptHandle >= 0 || !officer.isPolice(true) || officer.health <= 0) return;
    const auto attacker = runtime.missionEnvironment->missionCharacters.find(attackerHandle);
    if (attacker == runtime.missionEnvironment->missionCharacters.end() ||
        !attacker->second.alive || attacker->second.health <= 0 || attacker->second.type == 6) return;
    if (!attacker->second.speechIdentity)
        attacker->second.speechIdentity = storylandNewCharacterSpeechIdentity();
    officer.policeAssailantIdentity = attacker->second.speechIdentity;
    officer.policeAssailantMissionHandle = attackerHandle;
    officer.policeAssailantUntil = runtime.elapsed + storylandVcsPoliceResponse::OfficerPursuitMemory;
    officer.clearFlee();
    officer.state = StorylandPedState::Combat;
}

inline int storylandVcsPoliceFindAssailant(StorylandPlayTestRuntime& runtime,
    size_t victimIndex, size_t actorCount) {
    if (!runtime.crime.vcs || victimIndex >= runtime.pedestrians.size()) return -1;
    auto& officer = runtime.pedestrians[victimIndex];
    if (!officer.policeAssailantIdentity) return -1;
    if (officer.policeAssailantMissionHandle >= 0) return -1;
    if (officer.scriptHandle < 0 && officer.isPolice(true) && officer.health > 0 &&
        std::isfinite(runtime.elapsed) && runtime.elapsed <= officer.policeAssailantUntil) {
        const size_t limit = std::min(actorCount, runtime.pedestrians.size());
        for (size_t index = 0; index < limit; ++index) {
            const auto& target = runtime.pedestrians[index];
            if (index != victimIndex && target.speechIdentity == officer.policeAssailantIdentity &&
                target.scriptHandle < 0 && target.health > 0 && target.renderAlpha &&
                !target.water.active && !target.zombie.active && !target.isPolice(true)) return int(index);
        }
    }
    officer.policeAssailantIdentity = 0;
    officer.policeAssailantMissionHandle = -1;
    officer.policeAssailantUntil = 0;
    return -1;
}

inline int32_t storylandVcsPoliceFindMissionAssailant(StorylandPlayTestRuntime& runtime,
    size_t victimIndex) {
    if (!runtime.crime.vcs || victimIndex >= runtime.pedestrians.size()) return -1;
    auto& officer = runtime.pedestrians[victimIndex];
    if (!officer.policeAssailantIdentity || officer.policeAssailantMissionHandle < 0) return -1;
    if (officer.scriptHandle < 0 && officer.isPolice(true) && officer.health > 0 &&
        std::isfinite(runtime.elapsed) && runtime.elapsed <= officer.policeAssailantUntil &&
        runtime.missionEnvironment) {
        const auto target = runtime.missionEnvironment->missionCharacters.find(officer.policeAssailantMissionHandle);
        if (target != runtime.missionEnvironment->missionCharacters.end() && target->second.alive &&
            target->second.health > 0 && target->second.visible && !target->second.removeFromWorld &&
            target->second.type != 6 && target->second.speechIdentity == officer.policeAssailantIdentity)
            return target->first;
    }
    officer.policeAssailantIdentity = 0;
    officer.policeAssailantMissionHandle = -1;
    officer.policeAssailantUntil = 0;
    return -1;
}

inline bool storylandVcsPoliceAssailantAimTarget(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& actors, size_t officerIndex,
    StorylandNativePoint& position) {
    if (!runtime.crime.vcs || officerIndex >= runtime.pedestrians.size()) return false;
    const auto& officer=runtime.pedestrians[officerIndex];
    if (!officer.policeAssailantIdentity || !officer.isPolice(true) || officer.scriptHandle>=0 ||
        officer.health<=0 || !std::isfinite(runtime.elapsed) ||
        runtime.elapsed>officer.policeAssailantUntil) return false;
    if (officer.policeAssailantMissionHandle>=0) {
        if (!runtime.missionEnvironment) return false;
        const auto target=runtime.missionEnvironment->missionCharacters.find(officer.policeAssailantMissionHandle);
        if (target==runtime.missionEnvironment->missionCharacters.end() || !target->second.alive ||
            target->second.health<=0 || !target->second.visible || target->second.removeFromWorld ||
            target->second.type==6 || target->second.speechIdentity!=officer.policeAssailantIdentity) return false;
        const auto pose=runtime.missionCombat.find(target->first);
        position=pose!=runtime.missionCombat.end()&&pose->second.anchorsValid ? pose->second.torso :
            StorylandNativePoint{target->second.x,target->second.y,target->second.z+1.1f};
    } else {
        const size_t count=std::min(actors.size(),runtime.pedestrians.size());
        bool found=false;
        for (size_t index=0;index<count;++index) {
            const auto& target=runtime.pedestrians[index];
            if (index==officerIndex || target.speechIdentity!=officer.policeAssailantIdentity ||
                target.scriptHandle>=0 || target.health<=0 || !target.renderAlpha ||
                target.water.active || target.zombie.active || target.isPolice(true)) continue;
            position=target.torsoValid?target.torso:StorylandNativePoint{
                actors[index].position.x,actors[index].position.y,actors[index].position.z+1.1f};
            found=true;
            break;
        }
        if (!found) return false;
    }
    return std::isfinite(position.x)&&std::isfinite(position.y)&&std::isfinite(position.z);
}
