#pragma once
#include "src/storyland/storyland_mission_vm.h"

class StorylandMissionProgram {
public:
    StorylandMissionProgram() {
        main.isMissionThread=false;
        main.isInternalMission=false;
    }
    bool start(const std::vector<uint8_t>& scm,uint32_t mainOffset,
        const std::vector<uint32_t>& missionOffsets,StorylandMissionVm& mission,
        StorylandMissionEnvironment& environment,bool vcs=true);
    bool seedPersistentGlobals(const std::map<size_t,int32_t>& saved,StorylandMissionVm& mission) {
        if(!running)return false;
        for(const auto& entry:saved)if(entry.first>=globals.size())return false;
        for(const auto& entry:saved)globals[entry.first]=entry.second;
        main.globals=globals;mission.globals=globals;return true;
    }
    const std::vector<int32_t>& persistentSaveGlobals() const {return globals;}
    void stop();
    void failMission(StorylandMissionVm& mission,const std::string& reason);
    bool startCoopMission(int32_t index, StorylandMissionVm& mission);
    bool restartMission(int32_t index, StorylandMissionVm& mission);
    bool applyCoopOutcome(StorylandMissionVm& mission, StorylandMissionOutcome outcome,
        StorylandMissionFailure failure, const std::string& key);
    void tick(uint32_t elapsedMilliseconds,StorylandMissionVm& mission);
    bool active() const {return running;}
    bool consumeMissionCleanupRequests(StorylandMissionVm& mission) {
        bool requested=false;
        const auto consume=[&](StorylandMissionVm& thread) {
            requested=requested||thread.missionCleanupRequested;
            thread.missionCleanupRequested=false;
        };
        consume(mission);consume(main);
        for(auto& thread:background)consume(thread);
        return requested;
    }
    int activeMission() const {return missionIndex;}
    const std::string& status() const {return diagnostic;}
    const std::set<std::string>& threadErrors() const {return backgroundErrors;}
    template<class Visitor>
    void visitThreads(const StorylandMissionVm& foreground, Visitor&& visitor) const {
        visitor(foreground);
        visitor(main);
        for (const auto& thread : background) {
            visitor(thread);
        }
    }
    std::string activeMissionName(const StorylandMissionVm& foreground) const {
        const auto eligible = [](const StorylandMissionVm& thread) {
            return thread.isMissionThread && (thread.state == StorylandMissionState::Running ||
                thread.state == StorylandMissionState::Waiting);
        };
        if (eligible(foreground)) {
            return foreground.scriptName;
        }
        for (auto thread = background.rbegin(); thread != background.rend(); ++thread) {
            if (eligible(*thread)) {
                return thread->scriptName;
            }
        }
        return eligible(main) ? main.scriptName : std::string{};
    }
    std::vector<std::array<uint32_t,2>> vcsWaitStates() const {
        std::vector<std::array<uint32_t,2>> states{{uint32_t(main.vcsAbsoluteWait),main.vcsWakeTime}};
        for(const auto& thread:background)
            states.push_back({uint32_t(thread.vcsAbsoluteWait),thread.vcsWakeTime});
        return states;
    }
    bool restoreVcsWaitStates(const std::vector<std::array<uint32_t,2>>& states) {
        if(states.size()!=background.size()+1)return false;
        const auto valid=[](const StorylandMissionVm& thread,const std::array<uint32_t,2>& state) {
            return state[0]<=1&&(!state[0]||(thread.vcs&&thread.state==StorylandMissionState::Waiting));
        };
        if(!valid(main,states.front()))return false;
        for(size_t index=0;index<background.size();++index)
            if(!valid(background[index],states[index+1]))return false;
        main.vcsAbsoluteWait=states.front()[0]!=0;
        main.vcsWakeTime=states.front()[1];
        for(size_t index=0;index<background.size();++index) {
            background[index].vcsAbsoluteWait=states[index+1][0]!=0;
            background[index].vcsWakeTime=states[index+1][1];
        }
        return true;
    }
    std::vector<uint8_t> messageWaitStates() const {
        std::vector<uint8_t> states{uint8_t(main.messageWaitSkippable)};
        for (const auto& thread : background) {
            states.push_back(thread.messageWaitSkippable);
        }
        return states;
    }
    bool restoreMessageWaitStates(const std::vector<uint8_t>& states) {
        if (states.size() != background.size() + 1) {
            return false;
        }
        for (uint8_t value : states) {
            if (value > 1) {
                return false;
            }
        }
        main.messageWaitSkippable = states.front();
        for (size_t index = 0; index < background.size(); ++index) {
            background[index].messageWaitSkippable = states[index + 1];
        }
        return true;
    }
    template<class A> void saveState(A& a) {
        a(running,missionIndex,main,background,offsets,globals,strings,diagnostic,backgroundErrors);
    }
    std::vector<std::array<bool,2>> deathArrestStates() const {
        std::vector<std::array<bool,2>> states{{main.deathArrestEnabled,main.deathArrestExecuted}};
        for(const auto& thread:background)states.push_back({thread.deathArrestEnabled,thread.deathArrestExecuted});
        return states;
    }
    std::vector<StorylandMissionVm::OutcomeState> outcomeStates() const {
        std::vector<StorylandMissionVm::OutcomeState> states{main.outcomeState()};
        for(const auto& thread:background)states.push_back(thread.outcomeState());
        return states;
    }
    bool restoreOutcomeStates(const std::vector<StorylandMissionVm::OutcomeState>& states) {
        if(states.size()!=background.size()+1)return false;
        if(!main.restoreOutcomeState(states.front())||main.isInternalMission)return false;
        for(size_t index=0;index<background.size();++index) {
            if(!background[index].restoreOutcomeState(states[index+1])||
                background[index].isInternalMission)return false;
        }
        return true;
    }
    void initializeLegacyOutcomes(StorylandMissionVm& mission) {
        main.isInternalMission=false;
        for(auto& thread:background)thread.isInternalMission=false;
        mission.isInternalMission=mission.isMissionThread;
        mission.missionSlot=missionIndex;
    }
    std::vector<StorylandMissionVm::OutcomeState> resultStates() const {
        std::vector<StorylandMissionVm::OutcomeState> result;
        result.reserve(background.size()+1);
        result.push_back(main.outcomeState());
        for(const auto& thread:background)result.push_back(thread.outcomeState());
        return result;
    }
    bool restoreResultStates(const std::vector<StorylandMissionVm::OutcomeState>& states) {
        if(states.size()!=background.size()+1)return false;
        StorylandMissionVm validator;
        for(const auto& state:states)if(!validator.restoreOutcomeState(state))return false;
        main.restoreOutcomeState(states.front());
        for(size_t index=0;index<background.size();++index)background[index].restoreOutcomeState(states[index+1]);
        return true;
    }
    bool restoreDeathArrestStates(const std::vector<std::array<bool,2>>& states) {
        if(states.size()!=background.size()+1)return false;
        main.deathArrestEnabled=states[0][0];main.deathArrestExecuted=states[0][1];
        for(size_t index=0;index<background.size();++index) {
            background[index].deathArrestEnabled=states[index+1][0];
            background[index].deathArrestExecuted=states[index+1][1];
        }
        return true;
    }
    bool restoreBindings(const StorylandMissionVm& source,StorylandMissionEnvironment& environment) {
        const auto bind=[&](StorylandMissionVm& vm) {
            vm.sharedRetailScript=source.sharedRetailScript;vm.script=source.script;
            vm.opcodeDefinitions=source.opcodeDefinitions;vm.provisionalOpcodes=source.provisionalOpcodes;
            vm.environment=&environment;
            if(!vm.validSavedExecution(vm.scriptBytes().size()))return false;
            return true;
        };
        if(background.size()>96||!bind(main))return false;
        for(auto& vm:background)if(!bind(vm))return false;
        return missionIndex>=-1&&(missionIndex<0||size_t(missionIndex)<offsets.size());
    }
private:
    bool running=false;
    int missionIndex=-1;
    StorylandMissionVm main;
    std::vector<StorylandMissionVm> background;
    std::vector<uint32_t> offsets;
    std::vector<int32_t> globals;
    std::map<size_t,std::string> strings;
    std::string diagnostic;
    std::set<std::string> backgroundErrors;
    void publish(const StorylandMissionVm& vm);
    void processScriptTerminations(StorylandMissionVm& mission);
    void run(StorylandMissionVm& vm,uint32_t elapsedMilliseconds,bool advanceEnvironment);
    bool launch(StorylandMissionVm& caller,StorylandMissionVm& mission);
};
