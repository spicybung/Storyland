#pragma once

#include "src/storyland/storyland_mission_program.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

inline const char* storylandScriptStateName(StorylandMissionState state) {
    switch(state) {
    case StorylandMissionState::Idle: return "idle";
    case StorylandMissionState::Running: return "running";
    case StorylandMissionState::Waiting: return "waiting";
    case StorylandMissionState::Finished: return "finished";
    case StorylandMissionState::Unsupported: return "unsupported";
    case StorylandMissionState::Fault: return "fault";
    }
    return "unknown";
}

inline std::string storylandScriptFailureKey(const StorylandMissionVm& mission,
    const StorylandMissionProgram& program) {
    std::ostringstream out;
    if(mission.state==StorylandMissionState::Fault||mission.state==StorylandMissionState::Unsupported)
        out << int(mission.state) << ':' << mission.scriptName << ':' << mission.pc << ':' << mission.diagnostic;
    for(const auto& issue:program.threadErrors())out << '\n' << issue;
    return out.str();
}

inline std::string storylandScriptReport(const StorylandMissionVm& mission,
    const StorylandMissionProgram& program, const StorylandMissionEnvironment& world) {
    std::ostringstream out;
    out << "Storyland script diagnostic\nGame: " << (mission.vcs?"Vice City Stories":"Liberty City Stories")
        << "\nGame time (ms): " << world.gameMilliseconds
        << "\nMission: " << program.activeMission() << "\nProgram: " << program.status() << "\n";
    for(const auto& issue:program.threadErrors())out << "Background error: " << issue << '\n';
    size_t count=0;
    program.visitThreads(mission,[&](const StorylandMissionVm& thread) {
        if(thread.state==StorylandMissionState::Idle && thread.executionTrace.empty())return;
        if(count>=32)return;
        out << "\nThread " << ++count << ": " << (thread.scriptName.empty()?"(unnamed)":thread.scriptName)
            << "\nState: " << storylandScriptStateName(thread.state)
            << "\nPC: 0x" << std::hex << thread.pc << "\nFile offset: 0x" << thread.fileOffset
            << std::dec << "\nInstructions: " << thread.instructions
            << "\nWait milliseconds: " << thread.delayMilliseconds
            << "\nVCS absolute wait: " << thread.vcsAbsoluteWait << " wake: " << thread.vcsWakeTime
            << "\nCondition: " << thread.condition << " valid: " << thread.conditionValid
            << "\nDiagnostic: " << thread.diagnostic << "\nRecent opcodes:\n";
        size_t recent=0;
        const size_t skip=thread.executionTrace.size()>64?thread.executionTrace.size()-64:0;
        for(const auto& instruction:thread.executionTrace) {
            if(recent++<skip)continue;
            out << "  0x" << std::hex << instruction.offset << "  " << std::setfill('0')
                << std::setw(4) << instruction.opcode << std::setfill(' ') << std::dec << '\n';
        }
    });
    return out.str();
}

inline bool storylandWriteScriptReport(const std::filesystem::path& directory,
    const std::string& text, std::filesystem::path& written, std::string& error) {
    if(directory.empty()) {error="No game user files directory is available.";return false;}
    std::error_code code;
    std::filesystem::create_directories(directory,code);
    if(code) {error="Could not create the game user files directory: "+code.message();return false;}
    const auto logs=directory/"Storyland Script Logs";
    std::filesystem::create_directories(logs,code);
    if(code) {error="Could not create the script logs directory: "+code.message();return false;}
    for(unsigned index=1;index<=99999;++index) {
        const auto folder=logs/("Log "+std::to_string(index));
        if(!std::filesystem::create_directory(folder,code)) {
            if(code) {error="Could not reserve a script log: "+code.message();return false;}
            continue;
        }
        const auto path=folder/"Storyland Script Log.txt";
        std::ofstream file(path,std::ios::out|std::ios::binary);
        if(!file) {error="Could not create the script log.";std::filesystem::remove(folder,code);return false;}
        file << text;
        file.flush();
        if(!file) {error="Could not finish writing the script log.";return false;}
        written=path;error.clear();return true;
    }
    error="Could not find an unused script log filename.";return false;
}
