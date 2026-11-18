#pragma once

#include "src/leeds/leeds_native_world.h"

struct StorylandCorpseBloodState {
    StorylandNativePoint previousPosition{};
    float age=0,stationaryTime=0;
    bool observed=false,poolEmitted=false,suppressPool=false;
};

struct StorylandBloodyTyreState {
    StorylandNativePoint previousContact{};
    float distanceRemaining=0;
    float sampleClock=0;
    unsigned bloodSamplesRemaining=0,markSequence=0;
    bool contactValid=false;
};

struct StorylandTyreBloodContact {
    StorylandNativePoint position{};
    float remaining=.6f;
    int area=0;
};
