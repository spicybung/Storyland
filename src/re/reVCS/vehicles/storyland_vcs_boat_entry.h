#pragma once
#include "src/storyland/storyland_vehicle_occupants.h"

inline bool storylandVcsOrdinaryBoat(const StorylandVehicleDefinition& definition) {
    return definition.vcsModelLayout&&definition.nativeType==1&&
        definition.domain==StorylandVehicleDomain::Water;
}
inline bool storylandVcsBoatEntryAvailable(const StorylandVehicleDefinition& definition) {
    return storylandVcsOrdinaryBoat(definition)&&definition.modelId!=250&&definition.modelId!=278&&
        definition.hasNativeHandling&&definition.hasNativeDummies&&definition.collision;
}
inline std::array<float,3> storylandVcsBoatSeekPosition(const StorylandVehicleMatrix& frame,
    const std::array<float,3>& seat) {
    // SeekBoat30A7D0 clears dummy0.x before applying the physical matrix.
    return storylandVehicleTransformPoint(frame,{0,seat[1],seat[2]});
}
inline bool storylandVcsBoatSeekReached(float x,float y,float goalX,float goalY) {
    return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(goalX)&&std::isfinite(goalY)&&
        std::hypot(x-goalX,y-goalY)<.5f;
}
inline std::array<float,3> storylandVcsBoatExitOrigin(const StorylandVehicleMatrix& frame,
    const std::array<float,3>& seatOrigin,float collisionMinimumZ) {
    // SetExitBoat302430: overturned hull support, otherwise the current seat origin.
    auto result=frame[10]<=-.9f?storylandVehicleTransformPoint(frame,{0,0,collisionMinimumZ}):seatOrigin;
    result[2]+=1;
    return result;
}
