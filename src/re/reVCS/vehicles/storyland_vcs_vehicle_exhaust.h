#pragma once

#include "src/storyland/storyland_vehicle_occupants.h"
#include <array>
#include <cmath>
#include <cstddef>

struct StorylandVcsExhaustPositions {
    bool nativePlacement=false;
    size_t count=0;
    std::array<std::array<float,3>,2> positions{};
};

inline StorylandVcsExhaustPositions storylandVcsExhaustPositions(
    const StorylandVehicleDefinition& definition,const StorylandVehicleMatrix& nativeFrame,
    bool twin) {
    StorylandVcsExhaustPositions result;
    if(!definition.vcsModelLayout||!definition.hasNativeDummies||
        (definition.nativeType!=0&&definition.nativeType!=6))return result;
    result.nativePlacement=true;
    auto local=definition.dummyPositions[6];
    for(float value:nativeFrame)if(!std::isfinite(value))return result;
    for(float value:local)if(!std::isfinite(value))return result;
    if(local[0]==0.0f&&local[1]==0.0f&&local[2]==0.0f)return result;
    result.positions[result.count++]=storylandVehicleTransformPoint(nativeFrame,local);
    if(twin) {
        local[0]=-local[0];
        result.positions[result.count++]=storylandVehicleTransformPoint(nativeFrame,local);
    }
    return result;
}
