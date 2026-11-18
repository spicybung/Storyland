#pragma once
#include <algorithm>

template<class MissionSet, class EmpireSites>
inline bool storylandVcsTurfUnlocked(bool vcs, bool extended,
    const MissionSet& passedMissions, const EmpireSites& sites) {
    return vcs && extended &&
        passedMissions.count("LOU_A03") != 0 && passedMissions.count("LOU_A04") != 0 &&
        std::any_of(sites.begin(), sites.end(), [](const auto& site) { return site.owner == 6; });
}
