#pragma once
#include "src/storyland/storyland_play_test_runtime.h"
#include <string>
enum class StorylandRuntimeCommandStatus { Unrecognized, Executed, Error };
struct StorylandRuntimeCommandResult {
    std::string output;
    bool timeChanged = false, teleported = false, refocus = false;
    int weather = -1;
};
StorylandRuntimeCommandStatus storylandExecutePlayTestCommand(const std::string& line,
    StorylandPlayTestRuntime& runtime,StorylandNativePoint& player,float& heading,
    StorylandRuntimeCommandResult& result);

struct StorylandMissionEnvironment;
size_t storylandMissionVehicleSlot(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& vehicles,StorylandMissionEnvironment& environment,
    const StorylandNativePoint& player);
void storylandRemoveMissionVehicle(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& vehicles,StorylandMissionEnvironment& environment,int32_t handle);
void storylandClearMissionVehicleSpace(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& vehicles,StorylandMissionEnvironment& environment,
    const StorylandNativeWorld& world,int32_t handle);
bool storylandJoinMissionCarWithRoad(const StorylandNativePaths& paths,StorylandNativeTrafficActor& actor);
void storylandApplyMissionVehicleAutopilot(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& vehicles,StorylandMissionEnvironment& environment,
    const StorylandNativePaths& paths);
void storylandClearMissionArea(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& vehicles,
    std::vector<StorylandNativeTrafficActor>& pedestrians,
    StorylandMissionEnvironment& environment,const std::array<float,6>& bounds,bool cars);
void storylandClearMissionRadius(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& vehicles,
    std::vector<StorylandNativeTrafficActor>& pedestrians,
    StorylandMissionEnvironment& environment,const StorylandNativePoint& center,float radius);
void storylandCaptureMissionWeapons(const StorylandPlayTestRuntime& runtime,StorylandMissionEnvironment& environment);
bool storylandApplyMissionWeapons(StorylandPlayTestRuntime& runtime,StorylandMissionEnvironment& environment,
    std::string& error,bool vcs=true);

bool storylandCaptureMissionCrouches(const StorylandPlayTestRuntime& runtime,StorylandMissionEnvironment& captured);

void storylandCleanupVcsMissionActors(StorylandMissionEnvironment& world);
