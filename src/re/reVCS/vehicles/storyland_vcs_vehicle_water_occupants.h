#pragma once
#include <array>
#include <algorithm>
#include <cmath>

struct StorylandVcsVehicleWaterOccupants {
    bool drowning=false;
    bool releaseRiders=false;
};

inline StorylandVcsVehicleWaterOccupants storylandVcsVehicleWaterOccupants(
    bool bike,float forceRatio,const std::array<float,4>& wheelFractions) {
    StorylandVcsVehicleWaterOccupants result;
    if(!std::isfinite(forceRatio))return result;
    const bool unsupported=std::any_of(wheelFractions.begin(),wheelFractions.end(),
        [](float fraction){return fraction==1.0f;});
    result.drowning=forceRatio>(bike?.8f:1.0f)||
        (forceRatio>(bike?.4f:.6f)&&unsupported);
    result.releaseRiders=bike&&result.drowning&&std::all_of(wheelFractions.begin(),wheelFractions.end(),
        [](float fraction){return fraction==1.0f;});
    return result;
}
