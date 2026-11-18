#pragma once
#include "src/storyland/storyland_vehicle_occupants.h"
#include "src/storyland/storyland_vehicle_transition.h"
#include <algorithm>
#include <cmath>

inline constexpr int32_t StorylandVcsHelicopterBailoutGroup=89;
inline constexpr int32_t StorylandVcsHelicopterBailoutAnimation=412;
inline constexpr int32_t StorylandVcsHelicopterBailoutCompletionBone=3;

inline bool storylandVcsHelicopter(const StorylandVehicleDefinition& definition) {
    return definition.vcsModelLayout&&definition.nativeType==4&&
        definition.domain==StorylandVehicleDomain::Air;
}

inline bool storylandVcsHelicopterBailout(const StorylandVehicleDefinition& definition,
    StorylandVehicleEntryVariant variant) {
    return storylandVcsHelicopter(definition)&&variant==StorylandVehicleEntryVariant::MovingExit;
}

inline bool storylandVcsHelicopterBailoutFrame(const StorylandVehicleDefinition& definition,
    const StorylandVehicleMatrix& vehicle,StorylandVehicleMatrix& result) {
    if(!storylandVcsHelicopter(definition)||!definition.hasNativeDummies||
        !std::all_of(vehicle.begin(),vehicle.end(),[](float value){return std::isfinite(value);}))return false;
    const auto seat=definition.seatPosition(0);
    if(!std::all_of(seat.begin(),seat.end(),[](float value){return std::isfinite(value);}))return false;
    const auto origin=storylandVehicleTransformPoint(vehicle,seat);
    // 2FE854..2FE9F8: full vehicle translation, yaw-only ped heading.
    const float heading=vehicle[4]==0&&vehicle[5]==0?0:std::atan2(-vehicle[4],vehicle[5]);
    const float cosine=std::cos(heading),sine=std::sin(heading);
    result={cosine,sine,0,0,-sine,cosine,0,0,0,0,1,0,origin[0],origin[1],origin[2],1};
    return std::all_of(result.begin(),result.end(),[](float value){return std::isfinite(value);});
}

inline bool storylandVcsHelicopterBailoutFrame(const StorylandVehicleDefinition& definition,
    const std::array<float,3>& position,float heading,float suspension,float roll,float pitch,
    StorylandVehicleMatrix& result) {
    const auto vehicle=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
        position,heading,definition.previewMinimumZ,suspension,roll,pitch),definition.previewRoot);
    return storylandVcsHelicopterBailoutFrame(definition,vehicle,result);
}

inline bool storylandVcsHelicopterBailoutGroundAvailable(float first,float second) {
    // 406EF8 uses -100 for a missed ray and accepts the higher of two hits.
    const float height=std::max(first,second);
    return std::isfinite(height)&&(height>-99.0f||height<-200.0f);
}
