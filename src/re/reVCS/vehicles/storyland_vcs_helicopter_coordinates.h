#pragma once

#include "src/storyland/storyland_vehicle_occupants.h"
#include <algorithm>
#include <array>
#include <cmath>

inline bool storylandVcsHelicopterOriginOffset(const StorylandVehicleDefinition& definition,
    std::array<float,3>& offset) {
    if(!definition.vcsModelLayout||definition.nativeType!=4||
        definition.hasNativeSuspension||!definition.collision)return false;
    const auto frame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
        {0,0,0},90,definition.previewMinimumZ,0,0,0),definition.previewRoot);
    if(!std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);}))return false;
    for(size_t column=0;column<3;++column)for(size_t row=0;row<3;++row)
        if(std::fabs(frame[column*4+row]-(column==row?1.0f:0.0f))>.0001f)return false;
    offset={frame[12],frame[13],frame[14]};
    return true;
}

inline bool storylandVcsHelicopterTargetOrigin(const StorylandVehicleDefinition& definition,
    const std::array<float,3>& actorPosition,float heading,float suspension,float roll,float pitch,
    bool nativeDestination,std::array<float,3>& origin) {
    if(!std::all_of(actorPosition.begin(),actorPosition.end(),
        [](float value){return std::isfinite(value);}))return false;
    if(!nativeDestination) {
        origin=actorPosition;
        return true;
    }
    if(!definition.vcsModelLayout||definition.nativeType!=4||!definition.collision)return false;
    const auto frame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
        actorPosition,heading,definition.previewMinimumZ,suspension,roll,pitch),definition.previewRoot);
    if(!std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);}))return false;
    origin={frame[12],frame[13],frame[14]};
    return true;
}
