#pragma once

#include "src/storyland/storyland_blood_state.h"
#include "src/storyland/storyland_weapon_types.h"

struct StorylandPlayTestRuntime;
struct StorylandRuntimeVehicle;

bool storylandBloodDamageAllowed(bool vcs,uint8_t kind,int32_t weapon);
void storylandMarkBloodRunover(StorylandPlayTestRuntime& runtime,
    const StorylandNativePoint& position,int32_t weapon);
void storylandEmitBloodImpact(StorylandPlayTestRuntime& runtime,
    const StorylandNativePoint& impact,float damage,
    const StorylandNativePoint* source=nullptr,bool heavy=false);
void storylandUpdateCorpseBlood(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const std::vector<StorylandNativeTrafficActor>& actors,
    const StorylandNativePoint& player,float seconds);
void storylandUpdateBloodParticle(StorylandPlayTestRuntime& runtime,StorylandRuntimeParticle& particle,
    const StorylandNativeWorld& world,const StorylandNativePoint& previous,int area);
void storylandUpdateBloodPool(StorylandRuntimeParticle& particle,float seconds);
void storylandUpdateBloodyTyres(StorylandPlayTestRuntime& runtime,
    const StorylandNativeTrafficActor& actor,StorylandRuntimeVehicle& vehicle,
    const StorylandNativeWorld* world,float seconds);
