#pragma once
#include "src/re/reVCS/gangs/storyland_vcs_turf_unlock.h"
#include "src/re/reVCS/gangs/storyland_empire.h"
#include <cmath>

inline bool storylandExtendedGangRivals(bool enabled, bool firstCuban, bool firstHaitian,
    bool secondCuban, bool secondHaitian) {
    return enabled && ((firstCuban && secondHaitian) || (firstHaitian && secondCuban));
}

template<class Game>
inline float storylandExtendedEmpireFireRate(bool vcs, bool extended, const Game& game,
    int pedestrianType, float x, float y, float playerX, float playerY) {
    if (!storylandVcsTurfUnlocked(vcs, extended, game.passedMissions, game.empireSites)) return 1.0f;
    const int gang=pedestrianType-7;
    if (gang!=0 && gang!=5) return 1.0f;
    for (size_t site=0;site<game.empireSites.size();++site) {
        if (game.empireSites[site].owner!=gang) continue;
        const auto& position=StorylandVcsEmpireSites[site];
        if (std::hypot(x-position[0],y-position[1])<=45.0f &&
            std::hypot(playerX-position[0],playerY-position[1])<=65.0f) return 1.12f;
    }
    return 1.0f;
}
