#pragma once
#include <cstdint>

inline bool storylandVehicleExitMayCrossSeats(bool vcs,bool saddle,bool privileged,
    bool oppositeOccupied,bool oppositeReserved) {
    if(saddle)return true;
    if(oppositeOccupied)return privileged;
    return vcs||!oppositeReserved;
}

inline unsigned storylandVehicleExitOppositeSeat(unsigned nativeSeat,bool rightHandSide) {
    return (nativeSeat>=2?2u:0u)+(rightHandSide?0u:1u);
}

inline bool storylandVehicleExitMayReposition(bool player,uint8_t creationSource) {
    return player||creationSource==2;
}
