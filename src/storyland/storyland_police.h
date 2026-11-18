#pragma once
#include "src/storyland/storyland_service_roles.h"
#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <cstdint>
#include <vector>

struct StorylandPoliceBudget { uint8_t cars=0,officers=0,roadblockDensity=0; };
inline StorylandPoliceBudget storylandPoliceBudget(int wanted,bool vcs=true) {
    constexpr StorylandPoliceBudget lcs[]{{0,0,0},{1,1,0},{2,3,0},{2,4,12},{2,6,18},{3,8,24},{3,10,30}};
    constexpr StorylandPoliceBudget vcsLevels[]{{0,0,0},{1,2,0},{2,3,0},{3,4,12},{3,6,18},{4,8,24},{4,10,30}};
    const int level=std::clamp(wanted,0,6);
    if(vcs)return vcsLevels[level];
    return lcs[level];
}
inline StorylandServiceRole storylandWantedPoliceRole(int wanted) {
    return wanted==4?StorylandServiceRole::Swat:wanted==5?StorylandServiceRole::Fbi:
        wanted==6?StorylandServiceRole::Army:StorylandServiceRole::Police;
}
// CCopPed constructors: LCS 0x352D08, VCS 0x324D08. The supplied reLCS
// PC source reduces these values; the PS2 games retain the table below.
inline uint8_t storylandPoliceWeaponAccuracy(StorylandServiceRole role) {
    switch(role) {
    case StorylandServiceRole::Swat:return 85;
    case StorylandServiceRole::Fbi:case StorylandServiceRole::Vice:return 95;
    case StorylandServiceRole::Army:return 100;
    default:return 75;
    }
}
struct StorylandPlayTestRuntime;
struct StorylandRuntimePed;
StorylandNativePoint storylandPoliceInstantHitEnd(StorylandPlayTestRuntime& runtime,
    const StorylandRuntimePed& officer,const StorylandNativePoint& origin,
    const StorylandNativePoint& target,float range,bool playerTarget,float spreadScale);
bool storylandPoliceOfficerInControl(const StorylandPlayTestRuntime& runtime,const StorylandRuntimePed& officer);
bool storylandPoliceOfficerCanArrest(const StorylandPlayTestRuntime& runtime,size_t officerIndex);
void storylandInitializePoliceLoadout(const StorylandPlayTestRuntime& runtime,StorylandRuntimePed& officer);
void storylandChoosePolicePursuitWeapon(const StorylandPlayTestRuntime& runtime,StorylandRuntimePed& officer);
bool storylandPolicePresence(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,const StorylandNativePoint& position,float radius);
void storylandUpdatePoliceCrime(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,const StorylandNativePoint& player,float seconds);
void storylandDispatchPolice(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    std::vector<StorylandNativeTrafficActor>& pedestrians,const StorylandNativePoint& player,float cameraYaw,float seconds);
