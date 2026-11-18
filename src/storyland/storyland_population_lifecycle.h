#pragma once
#include "src/storyland/storyland_play_test_runtime.h"
#include <algorithm>
#include <cmath>

inline bool storylandPopulationCorpseFadeDue(float deathSeconds,bool killFrenzy) {
    return std::isfinite(deathSeconds)&&deathSeconds>(killFrenzy?15.0f:30.0f);
}

inline uint8_t storylandPopulationRenderAlpha(uint8_t alpha,bool fadeOut) {
    return uint8_t(fadeOut?std::max(0,int(alpha)-8):std::min(255,int(alpha)+16));
}

inline void storylandAdvancePopulationPedestrian(StorylandRuntimePed& pedestrian,bool killFrenzy) {
    if(pedestrian.health<=0&&storylandPopulationCorpseFadeDue(pedestrian.deathTime,killFrenzy))
        pedestrian.populationFadeOut=true;
    pedestrian.renderAlpha=storylandPopulationRenderAlpha(pedestrian.renderAlpha,pedestrian.populationFadeOut);
}
