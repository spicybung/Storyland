#pragma once
#include "src/storyland/storyland_character_sound_events.h"
#include "src/storyland/storyland_play_test_camera.h"
#include <algorithm>
#include <vector>

struct StorylandCharacterSpeechActor {
    int64_t key=-1;
    uint64_t identity=0;
    int speaker=-1;
    int32_t handle=0;
    uint16_t model=0;
    unsigned variant=0;
    int32_t profile=-1;
    StorylandNativePoint position{};
    bool valid=false,enabled=false,alive=false,player=false,mission=false,suppressedAnimation=false;
};
inline int storylandFindCharacterSpeechSpeaker(const StorylandPlayTestRuntime& runtime,
    const StorylandMissionEnvironment& world,uint64_t identity) {
    if(identity==StorylandPlayerSpeechIdentity)return -1;
    if(!identity)return -2;
    for(const auto& entry:world.missionCharacters)
        if(entry.second.speechIdentity==identity)return int(runtime.pedestrians.size())+entry.first;
    for(size_t index=0;index<runtime.pedestrians.size();++index)
        if(runtime.pedestrians[index].scriptHandle<0&&runtime.pedestrians[index].speechIdentity==identity)return int(index);
    return -2;
}
inline StorylandCharacterSpeechActor storylandResolveCharacterSpeechActor(
    const StorylandPlayTestRuntime& runtime,const StorylandMissionEnvironment& world,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,const StorylandNativePoint& playerPosition,
    int speaker,uint64_t expectedIdentity=0) {
    StorylandCharacterSpeechActor result;
    if(expectedIdentity)speaker=storylandFindCharacterSpeechSpeaker(runtime,world,expectedIdentity);
    if(speaker < -1)return result;
    result.speaker=speaker;result.player=speaker<0;
    if(result.player) {
        result.valid=true;result.identity=StorylandPlayerSpeechIdentity;
        result.handle=runtime.crime.vcs?0:1;
        result.enabled=world.playerSpeechEnabled;result.alive=runtime.health>0;
        result.position=playerPosition;
        result.variant=world.playerSpeechMoodMilliseconds?unsigned(world.playerSpeechMood):
            runtime.wanted>=4?2u:runtime.wanted>=2?1u:0u;
    } else if(size_t(speaker)<runtime.pedestrians.size()) {
        if(size_t(speaker)>=pedestrians.size())return result;
        const auto& ped=runtime.pedestrians[size_t(speaker)];
        if(ped.scriptHandle>=0)return result;
        result.valid=ped.stats.modelId!=0;result.identity=ped.speechIdentity;
        result.enabled=true;result.alive=ped.health>0;result.model=uint16_t(ped.stats.modelId);
        result.variant=unsigned(result.identity%5);result.position=pedestrians[size_t(speaker)].position;
        result.key=int64_t(result.identity);result.handle=ped.scriptHandle;
        result.suppressedAnimation=storylandCharacterSpeechAnimationSuppressed(ped.ranged.animationId)||
            storylandCharacterSpeechAnimationSuppressed(ped.threatAnimation);
    } else {
        result.handle=speaker-int(runtime.pedestrians.size());
        const auto found=world.missionCharacters.find(result.handle);
        if(found==world.missionCharacters.end()||found->second.model<0)return result;
        const auto& ped=found->second;
        result.valid=true;result.identity=ped.speechIdentity;result.enabled=ped.speechEnabled;
        result.alive=ped.alive&&ped.health>0;result.mission=true;result.model=uint16_t(ped.model);
        result.position={ped.x,ped.y,ped.z};result.key=int64_t(result.identity);
        if(ped.creationSource==2&&ped.type!=0)result.profile=ped.speechProfile;
        result.suppressedAnimation=ped.animationGroup==0&&storylandCharacterSpeechAnimationSuppressed(ped.animationId);
    }
    if(result.player||result.mission) {
        const auto scripted=world.scriptedCharacterAnimations.find(result.handle);
        if(scripted!=world.scriptedCharacterAnimations.end()&&scripted->second.group==0)
            result.suppressedAnimation=result.suppressedAnimation||
                storylandCharacterSpeechAnimationSuppressed(scripted->second.animation);
    }
    return result;
}
inline const StorylandSpeechRange* storylandCharacterSpeechRange(
    const StorylandCharacterSpeechActor& actor,uint16_t event) {
    if(!actor.valid||actor.profile < -1||actor.profile>29)return nullptr;
    if(!actor.player&&actor.profile>0)return storylandVcsSpeechProfileRange(actor.profile,event);
    return storylandVcsSpeechRange(actor.model,event,actor.variant);
}
inline bool storylandCharacterHasActiveFire(const StorylandPlayTestRuntime& runtime,
    const StorylandMissionEnvironment& world,const StorylandNativeWorld& nativeWorld,
    const StorylandCharacterSpeechActor& actor) {
    if(!actor.valid)return false;
    if(actor.player||actor.mission)for(const auto& entry:world.scriptFires) {
        const auto& fire=entry.second;
        if(fire.active&&fire.attachedKind==1&&fire.attachedHandle==actor.handle)return true;
    }
    if(!actor.alive)return false;
    for(const auto& effect:runtime.effects) {
        if(effect.kind!=StorylandParticleKind::Flame||effect.damage<=0||effect.remaining<=0)continue;
        const float dx=actor.position.x-effect.position.x,dy=actor.position.y-effect.position.y,dz=actor.position.z-effect.position.z;
        if(dx*dx+dy*dy+dz*dz>effect.radius*effect.radius)continue;
        auto origin=effect.position;origin.z+=.25f;
        auto chest=actor.position;chest.z+=1.1f;
        if(storylandTraceNativeSegment(nativeWorld,origin,chest)>=.995f)return true;
    }
    return false;
}
inline bool storylandCharacterSpeechIsReaction(uint16_t event) { return event>=0x66&&event<=0x6B; }
