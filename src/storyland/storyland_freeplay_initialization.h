#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_scm.h"
#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

// Execute only the two authored world-placement scripts in an isolated VM.
// MAIN and mission threads are never started by this helper.
inline bool storylandInitializeFreeplayScm(const StorylandScmFile& script,bool vcs,
    StorylandMissionEnvironment& world,std::string& error) {
    error.clear();
    if(!vcs) {
        error="The LCS freeplay initializer has not been verified against its native MAIN.SCM.";
        return false;
    }
    if(!world.pickups.empty() || !world.carGenerators.empty()) {
        error="Freeplay world placements are already initialized.";
        return false;
    }
    auto candidate=std::make_unique<StorylandMissionEnvironment>();
    candidate->area=world.area;
    auto bytes=std::make_shared<const std::vector<uint8_t>>(script.rawBytes());
    std::vector<int32_t> globals;
    std::map<size_t,std::string> strings;
    for(const char* required:{"INITPU","INITCG"}) {
        const auto row=std::find_if(script.missions().begin(),script.missions().end(),
            [required](const auto& entry) {
                auto name=entry.scriptName;
                for(char& character:name)character=char(std::toupper(static_cast<unsigned char>(character)));
                return name==required;
            });
        if(row==script.missions().end() || !row->hasBinaryRange || row->binarySize==0 ||
            uint64_t(row->binaryOffset)+row->binarySize>bytes->size()) {
            error=std::string("The native ")+required+" initializer was not found.";
            return false;
        }
        StorylandMissionVm initializer;
        initializer.environment=candidate.get();
        initializer.startRetailShared(bytes,row->binaryOffset,true,false);
        initializer.isMissionThread=false;
        if(!globals.empty()) {initializer.globals=globals;initializer.globalStrings=strings;}
        for(unsigned frame=0;frame<32 && initializer.state==StorylandMissionState::Running;++frame)
            initializer.tick(0,false);
        if(initializer.state!=StorylandMissionState::Finished) {
            error=std::string("The native ")+required+" initializer could not finish: "+initializer.diagnostic;
            return false;
        }
        if(initializer.instructions!=initializer.executionTrace.size()) {
            error=std::string("The native ")+required+" initializer exceeded the auditable instruction limit.";
            return false;
        }
        for(const auto& instruction:initializer.executionTrace) {
            const auto opcode=instruction.opcode & 0x7fff;
            const bool allowed=opcode==0x0238 || opcode==0x0023 || opcode==0x0004 ||
                opcode==0x013f || opcode==0x01f9 || opcode==0x00a9 || opcode==0x00aa;
            if(!allowed || instruction.offset<row->binaryOffset ||
                instruction.offset>=uint64_t(row->binaryOffset)+row->binarySize) {
                error=std::string("The native ")+required+" initializer has an unsupported instruction.";
                return false;
            }
        }
        globals=initializer.globals;
        strings=initializer.globalStrings;
    }
    world.pickups=std::move(candidate->pickups);
    world.nextPickupHandle=candidate->nextPickupHandle;
    world.carGenerators=std::move(candidate->carGenerators);
    world.nextCarGeneratorHandle=candidate->nextCarGeneratorHandle;
    return true;
}
