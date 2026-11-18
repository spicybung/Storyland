#pragma once
#include "src/storyland/storyland_character_audio.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_mission_vm.h"

inline void storylandEnsureCharacterSpeechIdentities(StorylandPlayTestRuntime& runtime,StorylandMissionEnvironment& world) {
    for(auto& ped:runtime.pedestrians)
        if(!ped.speechIdentity)ped.speechIdentity=storylandNewCharacterSpeechIdentity();
    for(auto& entry:world.missionCharacters)
        if(!entry.second.speechIdentity)entry.second.speechIdentity=storylandNewCharacterSpeechIdentity();
}
inline void storylandStampCharacterSoundEvent(StorylandPlayTestRuntime& runtime,StorylandPlayTestSoundEvent& event) {
    if(event.speaker<0)event.speakerIdentity=StorylandPlayerSpeechIdentity;
    else if(size_t(event.speaker)<runtime.pedestrians.size()) {
        auto& ped=runtime.pedestrians[size_t(event.speaker)];
        if(!ped.speechIdentity)ped.speechIdentity=storylandNewCharacterSpeechIdentity();
        event.speakerIdentity=ped.speechIdentity;
    } else if(runtime.missionEnvironment) {
        const auto found=runtime.missionEnvironment->missionCharacters.find(event.speaker-int(runtime.pedestrians.size()));
        if(found!=runtime.missionEnvironment->missionCharacters.end()) {
            auto& ped=found->second;
            if(!ped.speechIdentity)ped.speechIdentity=storylandNewCharacterSpeechIdentity();
            event.speakerIdentity=ped.speechIdentity;
        }
    }
}
inline void storylandRequestWeaponHitSpeech(StorylandPlayTestRuntime& runtime,int speaker,
    const StorylandNativePoint& position,int32_t weapon) {
    if(!runtime.crime.vcs&&speaker>=0)return;
    StorylandPlayTestSoundEvent impact(StorylandPlayTestSound::Damage);
    impact.speaker=speaker;impact.position=position;impact.positioned=true;
    impact.weapon=weapon;impact.nativeSpeech=runtime.crime.vcs?int(StorylandSpeechEvent::Pain):0x6B;
    storylandStampCharacterSoundEvent(runtime,impact);
    runtime.soundEvents.push_back(impact);
}

inline void storylandQueuePlayerDeathSounds(StorylandPlayTestRuntime& runtime) {
    const bool vcs=runtime.crime.vcs;
    if(runtime.occupiedVehicle>=0&&size_t(runtime.occupiedVehicle)<runtime.vehicles.size()&&
        !runtime.playerWater.drowning&&!runtime.vehicles[size_t(runtime.occupiedVehicle)].water.engineFlooded) {
        StorylandPlayTestSoundEvent crunch(StorylandPlayTestSound::Damage);
        crunch.nativePedAudioEvent=vcs?205:204;
        crunch.position=runtime.playerPosition;crunch.positioned=true;crunch.fatal=true;
        storylandStampCharacterSoundEvent(runtime,crunch);
        runtime.soundEvents.push_back(crunch);
    }
    StorylandPlayTestSoundEvent speech(StorylandPlayTestSound::Damage);
    speech.nativeSpeech=vcs?0x67:0x68;
    speech.fatal=true;
    storylandStampCharacterSoundEvent(runtime,speech);
    runtime.soundEvents.push_back(speech);
    uint16_t flags=0;
    const bool suppressed=vcs&&runtime.reactionBindings.associationFlags&&
        runtime.reactionBindings.associationFlags(runtime.playerDeath.animation,0,flags)&&(flags&0x800u);
    if(!suppressed) {
        speech.nativeSpeech=vcs?0x66:0x67;
        runtime.soundEvents.push_back(speech);
    }
}

inline int storylandLcsPlayerDamageSpeech(int32_t weapon,uint8_t kind) {
    if(weapon==storylandNativeDrowningWeapon(false))return -1;
    if(weapon==storylandNativeFallWeapon(false))return 0x6A;
    if(weapon==15||weapon==31)return 0x6C;
    switch(storylandNativePedDamageProof(false,weapon,kind)) {
    case 1:return 0x6B;
    case 2:return 0x6C;
    case 16:return 0x8B;
    default:return 0x68;
    }
}

inline void storylandQueuePedLandingSound(StorylandPlayTestRuntime& runtime,int speaker,
    const StorylandNativePoint& position,uint8_t surface,int nativeEvent=35,
    bool requestSpeech=false) {
    StorylandPlayTestSoundEvent landing(StorylandPlayTestSound::Land);
    landing.speaker=speaker;
    landing.position=position;
    landing.positioned=true;
    landing.surface=surface;
    if(runtime.crime.vcs) {
        landing.nativePedAudioEvent=nativeEvent==36?36:35;
        if(speaker<0)landing.nativePedMoveState=uint8_t(runtime.playerSprint.movementState);
        else if(runtime.missionEnvironment&&size_t(speaker)>=runtime.pedestrians.size()) {
            const auto character=runtime.missionEnvironment->missionCharacters.find(
                speaker-int(runtime.pedestrians.size()));
            if(character!=runtime.missionEnvironment->missionCharacters.end())
                landing.nativePedMoveState=character->second.movementState;
        }
    }
    storylandStampCharacterSoundEvent(runtime,landing);
    runtime.soundEvents.push_back(landing);
    if(requestSpeech&&(runtime.crime.vcs||speaker<0)) {
        StorylandPlayTestSoundEvent speech=landing;
        speech.nativeSpeech=runtime.crime.vcs?0x69:0x6A;
        speech.nativePedAudioEvent=-1;
        runtime.soundEvents.push_back(speech);
    }
}

inline void storylandQueuePedFootstepSound(StorylandPlayTestRuntime& runtime,int speaker,
    const StorylandNativePoint& position,uint8_t surface,unsigned foot,uint8_t movementState) {
    if(foot<1||foot>2)return;
    StorylandPlayTestSoundEvent step(StorylandPlayTestSound::Footstep);
    step.speaker=speaker;
    step.position=position;
    step.positioned=true;
    step.surface=surface;
    if(runtime.crime.vcs)step.nativePedAudioEvent=32+int(foot);
    step.nativePedMoveState=movementState;
    storylandStampCharacterSoundEvent(runtime,step);
    runtime.soundEvents.push_back(step);
}

inline bool storylandPlayerLocomotionSoundAllowed(const StorylandPlayTestRuntime& runtime) {
    return runtime.health>0&&runtime.grounded&&!runtime.jumpLaunching&&runtime.landingTime<=0&&
        !runtime.playerWater.active&&runtime.debugMovement==StorylandDebugMovement::Walking&&
        runtime.occupiedVehicle<0&&runtime.vehicleAction==StorylandPlayTestRuntime::VehicleAction::None;
}

inline unsigned storylandQueuePlayerLocomotionSound(StorylandPlayTestRuntime& runtime,
    const StorylandNativePoint& position,float phase,float phaseAdvance,float duration) {
    if(!runtime.crime.vcs||!storylandPlayerLocomotionSoundAllowed(runtime)||
        runtime.playerStats.statIndex==39)return 0;
    const unsigned foot=storylandVcsFootstepCrossing(phase*duration,phaseAdvance*duration,duration);
    storylandQueuePedFootstepSound(runtime,-1,position,runtime.groundSurface,foot,
        uint8_t(runtime.playerSprint.movementState));
    return foot;
}
