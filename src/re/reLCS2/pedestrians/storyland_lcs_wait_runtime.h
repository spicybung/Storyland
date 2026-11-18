#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/re/reLCS2/pedestrians/storyland_lcs_wait_animation.h"
#include <algorithm>
#include <cmath>
#include <limits>

inline int32_t storylandLcsWaitClipMilliseconds(float seconds) {
    if(!std::isfinite(seconds)||seconds<=0)return 0;
    int32_t milliseconds=int32_t(std::clamp(double(seconds)*1000.0,1.0,double(INT32_MAX)));
    if(milliseconds<INT32_MAX&&float(milliseconds)/1000.0f<seconds)++milliseconds;
    return milliseconds;
}

inline int32_t storylandLcsDefaultSeatedWait(uint32_t& randomState) {
    // Use the VM's saved random stream for the native [25000,30000) default.
    randomState=randomState*1664525u+1013904223u;
    return 25000+int32_t((uint64_t(randomState>>16)*5000u)/65536u);
}

inline int32_t storylandLcsDefaultRoadLookWait(uint32_t& randomState) {
    randomState=randomState*1664525u+1013904223u;
    return 2000+int32_t((uint64_t(randomState>>16)*3000u)/65536u);
}

inline bool storylandAdvanceLcsWaitAnimations(StorylandMissionEnvironment& world,
    uint32_t& randomState,std::string& error) {
    for(auto waiting=world.lcsWaitAnimations.begin();waiting!=world.lcsWaitAnimations.end();) {
        auto& animation=waiting->second;
        const auto character=world.missionCharacters.find(waiting->first);
        const bool dead=waiting->first==1?world.health<=0:character==world.missionCharacters.end()||
            !character->second.alive||character->second.health<=0;
        const uint32_t elapsed=world.gameMilliseconds-animation.startedAt;
        bool atmFinished=false;
        if(!dead&&animation.state==25&&!animation.repeat) {
            const auto* definition=storylandLcsWaitAnimation(animation.state);
            const float seconds=world.lcsAnimationDuration?
                world.lcsAnimationDuration(definition->bank,definition->clip):0;
            const int32_t clipMilliseconds=storylandLcsWaitClipMilliseconds(seconds);
            if(!clipMilliseconds) {
                error="ATM wait completion requires the decoded ped/ATM clip.";
                return false;
            }
            atmFinished=elapsed>=uint32_t(clipMilliseconds);
        }
        const bool clipFinished=!animation.repeat&&(animation.state==21||animation.state==23||animation.state>=36);
        const bool timedOut=animation.durationMilliseconds>=0&&(animation.state==25?
            world.gameMilliseconds>animation.startedAt+uint32_t(animation.durationMilliseconds):
            clipFinished?elapsed>=uint32_t(animation.durationMilliseconds):
            elapsed>uint32_t(animation.durationMilliseconds));
        const bool expired=atmFinished||timedOut;
        if(!dead&&!expired) {++waiting;continue;}
        if(!dead&&(animation.state==21||animation.state==24)) {
            // Native FinishedWaitCB advances down→idle; the sitting timer
            // advances idle→up. Movement resumes only after up has finished.
            const int32_t nextState=animation.state==21?24:23;
            const auto* definition=storylandLcsWaitAnimation(nextState);
            const float seconds=world.lcsAnimationDuration?
                world.lcsAnimationDuration(definition->bank,definition->clip):0;
            const int32_t clipMilliseconds=storylandLcsWaitClipMilliseconds(seconds);
            if(!clipMilliseconds) {
                error="Seated wait transition requires decoded ped/"+std::string(definition->clip)+".";
                return false;
            }
            const int32_t duration=nextState==24?storylandLcsDefaultSeatedWait(randomState):clipMilliseconds;
            animation={nextState,world.gameMilliseconds,duration,world.nextPedAnimationSerial++};
            world.pedWaitStateMilliseconds[waiting->first]=duration;
            ++waiting;continue;
        }
        world.pedWaitStateMilliseconds.erase(waiting->first);
        waiting=world.lcsWaitAnimations.erase(waiting);
    }
    return true;
}
