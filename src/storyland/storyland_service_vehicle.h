#pragma once

#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test_runtime.h"

// Host-owned route state. These vehicles do not acquire mission-script handles.
// The task is transient: saving restores the car without an active dispatch.
struct StorylandServiceVehicleAutopilot {
    StorylandMissionEnvironment::VehicleScriptState task;
    StorylandServiceRole role=StorylandServiceRole::Civilian;
    float arrivalRadius=3;
    bool arrived=false;
    int32_t incidentHandle=-1;
    uint64_t incidentVehicleIdentity=0;
    uint64_t incidentPedIdentity=0;
};

int storylandSpawnVcsServiceVehicle(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const StorylandNativePaths& paths,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,
    std::vector<StorylandNativeTrafficActor>& cars,const StorylandNativePoint& player,
    const StorylandNativePoint& destination,StorylandServiceRole role,
    uint32_t modelId,uint32_t driverModelId,float arrivalRadius);

bool storylandRetargetVcsServiceVehicle(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& cars,size_t slot,
    const StorylandNativePoint& destination);

void storylandCancelVcsServiceVehicle(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& cars,size_t slot);

void storylandDispatchVcsFireEngine(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const StorylandNativePaths& paths,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,
    std::vector<StorylandNativeTrafficActor>& cars,const StorylandNativePoint& player);

void storylandDispatchVcsAmbulance(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world,const StorylandNativePaths& paths,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,
    std::vector<StorylandNativeTrafficActor>& cars,const StorylandNativePoint& player);
