#include "src/storyland/storyland_mission_program.h"
#include "src/re/reLCS2/runtime/storyland_lcs_messages.h"
#include "src/re/reVCS/runtime/storyland_vcs_attachment.h"
#include <algorithm>
#include <cstdio>
#include <new>

namespace {
bool clearVcsMissionGlobals(const std::vector<uint8_t>& script,
    std::vector<int32_t>& globals, std::map<size_t,std::string>& strings, std::string& error) {
    if (script.size() < 16 || script[15] != 'm') return true;
    const auto word = [&](size_t offset) {
        return uint32_t(script[offset]) | uint32_t(script[offset + 1]) << 8 |
            uint32_t(script[offset + 2]) << 16 | uint32_t(script[offset + 3]) << 24;
    };
    const auto headerTarget = [&](size_t offset, size_t& target) {
        if (offset > script.size() || script.size() - offset < 7 ||
            script[offset] != 2 || script[offset + 1] != 0 || script[offset + 2] != 6) return false;
        const uint64_t address = uint64_t(word(offset + 3)) + 8;
        if (address > script.size() || address <= offset + 7) return false;
        target = size_t(address);
        return true;
    };
    size_t globalsEnd = 0, missionHeader = 0, mainEntry = 0;
    if (!headerTarget(8, globalsEnd) || !headerTarget(globalsEnd, missionHeader) ||
        !headerTarget(missionHeader, mainEntry) || mainEntry - missionHeader < 20) {
        error = "Retail VCS mission launch has an invalid SCM variable header.";
        return false;
    }
    const uint16_t first = uint16_t(script[missionHeader + 8]) |
        uint16_t(script[missionHeader + 9]) << 8;
    const uint16_t count = uint16_t(script[missionHeader + 10]) |
        uint16_t(script[missionHeader + 11]) << 8;
    const size_t end = size_t(first) + count;
    if (first > INT16_MAX || count > INT16_MAX || end > globals.size() ||
        globalsEnd < 16 || end > (globalsEnd - 16) / 4) {
        error = "Retail VCS mission variables extend outside the SCM global space.";
        return false;
    }
    // Native0289 at3B230C clears only this header-defined mission workspace.
    std::fill(globals.begin() + first, globals.begin() + end, 0);
    strings.erase(strings.lower_bound(first), strings.lower_bound(end));
    return true;
}

class MissionAllocationError : public std::exception {
    char text[256]{};
public:
    MissionAllocationError(const char* phase,const StorylandMissionVm& vm) noexcept {
        std::snprintf(text,sizeof(text),"Mission memory allocation failed during %s; script %.24s, PC 0x%zX, instructions %zu.",
            phase,vm.scriptName.c_str(),vm.pc,vm.instructions);
    }
    const char* what() const noexcept override {return text;}
};
}


bool StorylandMissionProgram::start(const std::vector<uint8_t>& scm,uint32_t mainOffset,
    const std::vector<uint32_t>& missionOffsets,StorylandMissionVm& mission,
    StorylandMissionEnvironment& environment,bool vcs) {
    stop();mission.stop();
    if(scm.empty()||mainOffset>=scm.size()||missionOffsets.empty()) {
        diagnostic="Retail program has no valid MAIN entry or mission table.";return false;
    }
    main.environment=&environment;mission.environment=&environment;
    environment.scriptTerminationRequests.clear();
    mission.vcs=vcs; // The idle foreground VM still owns the shared frame clock.
    main.startRetail(scm,mainOffset,vcs);main.isMissionThread=false;
    main.isInternalMission=false;
    background.reserve(96);
    offsets=missionOffsets;globals=main.globals;strings=main.globalStrings;
    running=true;diagnostic="Retail MAIN is initializing the world.";
    return true;
}

void StorylandMissionProgram::stop() {
    running=false;missionIndex=-1;main.stop();background.clear();
    main.isMissionThread=false;main.isInternalMission=false;
    offsets.clear();globals.clear();strings.clear();diagnostic.clear();backgroundErrors.clear();
}

bool StorylandMissionProgram::startCoopMission(int32_t index, StorylandMissionVm& mission) {
    if (!running || index < 0 || size_t(index) >= offsets.size() || !main.environment) return false;
    if (missionIndex == index && (mission.state == StorylandMissionState::Running ||
        mission.state == StorylandMissionState::Waiting)) return true;
    if (missionIndex >= 0) {
        if (mission.vcs) storylandCleanupVcsMissionActors(*main.environment);
        else mission.cleanupLcsMission();
    }
    mission.stop();
    StorylandMissionVm caller;
    StorylandMissionVm::ScriptLaunch request;
    request.internalMission = true; request.missionThread = true; request.target = index;
    caller.pendingLaunches.push_back(std::move(request));
    return launch(caller, mission);
}

bool StorylandMissionProgram::restartMission(int32_t index, StorylandMissionVm& mission) {
    if (!running || index < 0 || size_t(index) >= offsets.size() || !main.environment) return false;
    if (mission.vcs) storylandCleanupVcsMissionActors(*main.environment);
    else mission.cleanupLcsMission();
    const auto discardDuplicate = [index](StorylandMissionVm& thread) {
        auto& requests = thread.pendingLaunches;
        requests.erase(std::remove_if(requests.begin(), requests.end(), [index](const auto& request) {
            return request.internalMission && request.target == index;
        }), requests.end());
    };
    discardDuplicate(main);
    for (auto& thread : background) discardDuplicate(thread);
    mission.stop();
    missionIndex = -1;
    return startCoopMission(index, mission);
}

void StorylandMissionProgram::publish(const StorylandMissionVm& vm) {
    globals=vm.globals;strings=vm.globalStrings;
}

void StorylandMissionProgram::processScriptTerminations(StorylandMissionVm& mission) {
    if(!main.environment)return;
    auto requests=std::move(main.environment->scriptTerminationRequests);
    main.environment->scriptTerminationRequests.clear();
    for(const auto& name:requests) {
        const auto terminate=[&](StorylandMissionVm& vm) {
            if(vm.state!=StorylandMissionState::Running&&vm.state!=StorylandMissionState::Waiting)return;
            std::string normalized=vm.scriptName;
            for(char& character:normalized)if(character>='A'&&character<='Z')character+=char('a'-'A');
            if(normalized!=name)return;
            vm.state=StorylandMissionState::Finished;
            vm.pendingLaunches.clear();vm.delayMilliseconds=0;vm.vcsAbsoluteWait=false;
            if(vm.isMissionThread) {
                vm.missionCleanupRequested=true;
                if(vm.vcs&&vm.environment)storylandCleanupVcsMissionActors(*vm.environment);
            }
            vm.diagnostic="SCM thread terminated by script name.";
        };
        terminate(main);terminate(mission);
        for(auto& vm:background)terminate(vm);
    }
}

void StorylandMissionProgram::run(StorylandMissionVm& vm,uint32_t elapsedMilliseconds,bool advanceEnvironment) {
    if(vm.globals.size()!=globals.size())return;
    vm.globals.swap(globals);vm.globalStrings.swap(strings);
    try {vm.tick(elapsedMilliseconds,advanceEnvironment);}
    catch(...) {vm.globals.swap(globals);vm.globalStrings.swap(strings);throw;}
    vm.globals.swap(globals);vm.globalStrings.swap(strings);
}

bool StorylandMissionProgram::launch(StorylandMissionVm& caller,StorylandMissionVm& mission) {
    while(!caller.pendingLaunches.empty()) {
        if(caller.pendingLaunches.front().internalMission&&
            (mission.state==StorylandMissionState::Running||mission.state==StorylandMissionState::Waiting))return true;
        auto request=std::move(caller.pendingLaunches.front());caller.pendingLaunches.pop_front();
        uint32_t offset=0;
        if(request.internalMission) {
            if(request.target<0||size_t(request.target)>=offsets.size()) {
                diagnostic="Retail mission launch index is outside MAIN.SCM's mission table.";return false;
            }
            offset=offsets[size_t(request.target)];
        } else {
            const int64_t address=request.target<0?int64_t(caller.missionBase)-request.target:int64_t(request.target)+8;
            if(address<0||address>=int64_t(main.scriptBytes().size())) {
                diagnostic="Retail thread launch label is outside MAIN.SCM.";return false;
            }
            offset=uint32_t(address);
        }
        if(!request.internalMission&&background.size()>=96) {
            diagnostic="Native background script pool is full.";return false;
        }
        if(request.arguments.size()>128) {diagnostic="Script launch has too many arguments.";return false;}
        StorylandMissionVm child;
        child.environment=main.environment;
        child.startRetailShared(main.sharedRetailScript,offset,main.vcs,false);
        child.isMissionThread=request.missionThread;
        child.isInternalMission=request.internalMission;
        child.missionSlot=request.internalMission?request.target:-1;
        if(!request.internalMission)child.missionBase=caller.missionBase;
        if (request.internalMission && main.vcs &&
            !clearVcsMissionGlobals(main.scriptBytes(), globals, strings, diagnostic)) return false;
        child.globals=globals;child.globalStrings=strings;
        for(size_t index=0;index<request.arguments.size();++index)child.locals[index]=request.arguments[index];
        child.localStrings=std::move(request.strings);
        if(request.internalMission) {
            for(unsigned pass=0;pass<64&&child.state==StorylandMissionState::Running;++pass)child.tick(0,false);
            publish(child);
            mission=std::move(child);missionIndex=request.target;
            diagnostic="Retail mission "+std::to_string(missionIndex)+" launched by MAIN.";
            if(mission.state==StorylandMissionState::Unsupported||mission.state==StorylandMissionState::Fault) {
                diagnostic="Startup/mission slot "+std::to_string(missionIndex)+" ("+mission.scriptName+"): "+mission.diagnostic;
                return false;
            }
            if(mission.state==StorylandMissionState::Finished) {
                missionIndex=-1;
                diagnostic=mission.diagnostic;
            }
        } else {
            if(background.size()>=96) {diagnostic="Native background script pool is full.";return false;}
            background.push_back(std::move(child));
        }
    }
    return true;
}

void StorylandMissionProgram::tick(uint32_t elapsedMilliseconds,StorylandMissionVm& mission) {
    if(!running)return;
    const char* phase="foreground script";
    const StorylandMissionVm* activeVm=&mission;
    try {
    if(mission.globals.empty()) {mission.globals=globals;mission.globalStrings=strings;}
    run(mission,elapsedMilliseconds,true);
    if(mission.state==StorylandMissionState::Unsupported||mission.state==StorylandMissionState::Fault) {
        diagnostic="Mission "+mission.scriptName+": "+mission.diagnostic;
        running=false;
        mission.globals=globals;mission.globalStrings=strings;
        return;
    }
    if(mission.environment)elapsedMilliseconds=mission.environment->nativeFrameMilliseconds;
    processScriptTerminations(mission);
    phase="foreground launch";
    if(!launch(mission,mission)) {running=false;return;}
    if(mission.state==StorylandMissionState::Finished&&missionIndex>=0) {
        missionIndex=-1;
        diagnostic=mission.diagnostic;
    }
    processScriptTerminations(mission);
    phase="MAIN script";activeVm=&main;
    if(main.pendingLaunches.empty())run(main,elapsedMilliseconds,false);
    processScriptTerminations(mission);
    phase="MAIN launch";
    if(!launch(main,mission)) {running=false;return;}
    processScriptTerminations(mission);
    const size_t count=background.size();
    for(size_t index=0;index<count;++index) {
        phase="background script";activeVm=&background[index];
        run(background[index],elapsedMilliseconds,false);
        processScriptTerminations(mission);
    }
    phase="background launch";
    for(size_t index=0;index<count;++index) {
        if(background[index].pendingLaunches.empty())continue;
        activeVm=&background[index];
        StorylandMissionVm caller=std::move(background[index]);
        const bool valid=launch(caller,mission);
        background[index]=std::move(caller);
        processScriptTerminations(mission);
        if(!valid) {running=false;return;}
    }
    phase="finished thread cleanup";activeVm=&main;
    for(const auto& thread:background)
        if(thread.state==StorylandMissionState::Finished&&thread.missionCleanupRequested)
            main.missionCleanupRequested=true;
    background.erase(std::remove_if(background.begin(),background.end(),[](const auto& vm) {
        return vm.state==StorylandMissionState::Finished;
    }),background.end());
    if(main.state==StorylandMissionState::Unsupported||main.state==StorylandMissionState::Fault) {
        diagnostic="MAIN: "+main.diagnostic;running=false;
        mission.state=main.state;mission.diagnostic=diagnostic;
    }
    for(const auto& vm:background)if(vm.state==StorylandMissionState::Unsupported||vm.state==StorylandMissionState::Fault)
        backgroundErrors.insert("Background "+vm.scriptName+": "+vm.diagnostic);
    phase="foreground globals";activeVm=&mission;
    mission.globals=globals;mission.globalStrings=strings;
    } catch(const std::bad_alloc&) {throw MissionAllocationError(phase,*activeVm);}
}

void StorylandMissionProgram::failMission(StorylandMissionVm& mission,const std::string& reason) {
    if(!running)return;
    const bool arrested=(mission.environment&&mission.environment->playerArrested)||
        reason.find("Busted")!=std::string::npos;
    const auto failure=arrested?
        StorylandMissionFailure::Arrest:StorylandMissionFailure::Death;
    const auto unwind=[&](StorylandMissionVm& thread) {
        if(!thread.isMissionThread||(thread.state!=StorylandMissionState::Running&&
            thread.state!=StorylandMissionState::Waiting))return;
        thread.globals=globals;
        thread.globalStrings=strings;
        if(thread.handleDeathArrest(failure)) {
            publish(thread);
            diagnostic=thread.diagnostic;
        } else if(thread.state==StorylandMissionState::Unsupported||
            thread.state==StorylandMissionState::Fault) {
            diagnostic=thread.diagnostic;
        }
    };
    unwind(mission);
    if(mission.state==StorylandMissionState::Unsupported||mission.state==StorylandMissionState::Fault)return;
    // Label-launched missions share this check with the foreground. A successful
    // unwind clears the shared mission flag before the next thread inspects it.
    for(auto& thread:background)unwind(thread);
    mission.globals=globals;
    mission.globalStrings=strings;
}

bool StorylandMissionProgram::applyCoopOutcome(StorylandMissionVm& mission,
    StorylandMissionOutcome outcome, StorylandMissionFailure failure, const std::string& key) {
    if (!mission.environment || (outcome != StorylandMissionOutcome::Passed &&
        outcome != StorylandMissionOutcome::Failed) || key.size() > 64 || unsigned(failure) > 3) return false;
    auto& world = *mission.environment;
    failure = outcome == StorylandMissionOutcome::Passed ? StorylandMissionFailure::None :
        failure == StorylandMissionFailure::None ? StorylandMissionFailure::Script : failure;
    const bool alreadyPassed = world.missionResult.outcome == StorylandMissionOutcome::Passed &&
        world.missionResult.key == key;
    if (outcome == StorylandMissionOutcome::Passed && !alreadyPassed && !key.empty() &&
        !world.passedMissions.count(key)) mission.registerMissionPassed(key, false);
    if (mission.missionResultSequence != world.missionResult.sequence || mission.missionResultKey != key)
        mission.missionResultSequence = 0;
    mission.recordMissionOutcome(outcome, failure, key,
        outcome == StorylandMissionOutcome::Passed ? "Mission passed." : "Mission failed.");
    const auto finish = [&](StorylandMissionVm& thread) {
        if (!thread.isMissionThread) return;
        thread.missionOutcome = outcome; thread.missionFailure = failure;
        thread.missionResultKey = key; thread.missionResultSequence = world.missionResult.sequence;
        thread.state = StorylandMissionState::Finished;
        thread.delayMilliseconds = 0; thread.vcsAbsoluteWait=false; thread.pendingLaunches.clear();
        thread.missionCleanupRequested = true;
    };
    finish(mission);
    for (auto& thread : background) finish(thread);
    const int32_t flag = world.declaredMissionFlag;
    const auto clearFlag = [flag](auto& values) {
        if (flag >= 0 && size_t(flag) < values.size()) values[size_t(flag)] = 0;
    };
    clearFlag(globals); clearFlag(main.globals); clearFlag(mission.globals);
    for (auto& thread : background) clearFlag(thread.globals);
    if (mission.vcs) {
        storylandCleanupVcsMissionActors(world);
        storylandReleaseVcsCharacterAttachmentState(world, 0);
    }
    else mission.cleanupLcsMission();
    world.control = true; world.cameraControlEnabled = true;
    world.camera.active = false; world.camera.cutsceneOwned = false; world.camera.restoreRequested = true;
    world.widescreen = false; world.timeFrozen = false; world.playerPositionFrozen = false;
    world.playerAttachedVehicle = -1; world.applyPlayerCarBrakes = false; world.playerVisible = true;
    world.playerCollisionEnabled = true;
    world.fadeAlpha = 0; world.fading = false; world.failCurrentMission = 0;
    world.messageKey = outcome == StorylandMissionOutcome::Passed ? "M_PASS" : "M_FAIL";
    world.messageStyle = 1; world.messageMilliseconds = 5000;
    world.messageNumbers.clear(); world.messageStringKey.clear();
    if (!mission.vcs) {
        StorylandMissionEnvironment::LcsMessage message;
        message.key = world.messageKey; message.durationMilliseconds = 5000;
        storylandQueueLcsMessage(world, std::move(message), 1);
    }
    missionIndex = -1;
    diagnostic = outcome == StorylandMissionOutcome::Passed ? "Mission passed." : "Mission failed.";
    return true;
}
