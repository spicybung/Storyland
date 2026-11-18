#pragma once
#include "src/storyland/storyland_bike_fall.h"
#include "src/leeds/leeds_native_world.h"

struct StorylandPlayTestRuntime;
struct StorylandRuntimeVehicle;

unsigned storylandEjectVcsMissionBikeRiders(StorylandPlayTestRuntime& runtime,
    const StorylandNativeTrafficActor& actor, StorylandRuntimeVehicle& vehicle,
    const StorylandBikeFallDecision& decision, bool forced = false);

void storylandAdvanceVcsMissionPedFalls(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, float seconds);
