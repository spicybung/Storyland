#pragma once

#include <cstdint>

struct StorylandVcsCharacterScriptFlags {
    uint8_t evasiveDirection = 0;
    bool acceptsPairedActions = true;
    bool ignoreMeleeImpacts = false;
    bool disablePlayerLeaderAnimationGroup = false;
};
