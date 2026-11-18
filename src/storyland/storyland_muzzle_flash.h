#pragma once

#include "src/storyland/storyland_weapon_types.h"
#include "src/storyland/storyland_mission_vm.h"
#include <cmath>

template<class Runtime>
StorylandNativePoint storylandMuzzleFlashPosition(const StorylandRuntimeParticle& particle,
    const Runtime& runtime) {
    if(particle.kind!=StorylandParticleKind::MuzzleFlash)return particle.position;
    StorylandNativePoint point=particle.position;
    bool valid=false;
    const auto matches=[&](size_t weapon) {
        return weapon<runtime.weapons.size()&&runtime.weapons[weapon].nativeId==particle.muzzleWeapon;
    };
    switch(particle.muzzleOwner) {
    case StorylandMuzzleOwner::Player:
        if(runtime.health>0&&matches(runtime.equippedWeapon)) {
            point=runtime.playerMuzzle;valid=runtime.playerMuzzleValid;
        }
        break;
    case StorylandMuzzleOwner::AmbientPed:
        if(particle.muzzleHandle>=0&&size_t(particle.muzzleHandle)<runtime.pedestrians.size()) {
            const auto& ped=runtime.pedestrians[size_t(particle.muzzleHandle)];
            if(ped.health>0&&ped.speechIdentity==particle.muzzleIdentity&&matches(ped.weapon)) {
                point=ped.weaponMuzzle;valid=ped.weaponMuzzleValid;
            }
        }
        break;
    case StorylandMuzzleOwner::MissionPed: {
        if(!runtime.missionEnvironment)break;
        const auto character=runtime.missionEnvironment->missionCharacters.find(particle.muzzleHandle);
        if(character==runtime.missionEnvironment->missionCharacters.end()||
            !character->second.alive||character->second.health<=0||character->second.removeFromWorld||
            character->second.speechIdentity!=particle.muzzleIdentity)break;
        const auto found=runtime.missionCombat.find(particle.muzzleHandle);
        if(found!=runtime.missionCombat.end()&&matches(found->second.weapon)) {
            point=found->second.muzzle;valid=found->second.muzzleValid;
        }
        break;
    }
    default:break;
    }
    if(!valid||!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))
        return particle.position;
    // Keep a flash at its emission point if the owner teleports during its lifetime.
    if(std::hypot(point.x-particle.position.x,point.y-particle.position.y,
        point.z-particle.position.z)>3.0f)return particle.position;
    return point;
}
