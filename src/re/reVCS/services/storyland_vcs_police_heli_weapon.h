#pragma once

#include <cstdint>
#include <array>
#include <algorithm>
#include <cmath>

// VCS PS2 2E6A14 selects the acquisition wait from the player's wanted level.
// The player-target branch uses these strict, unsigned millisecond tests.
inline uint32_t storylandVcsPoliceHeliAcquisitionDelay(int wanted) {
    switch(wanted) {
    case 3:return 10000;
    case 4:return 5000;
    case 5:return 3500;
    case 6:return 2000;
    default:return 999999;
    }
}

inline bool storylandVcsPoliceHeliCanShoot(uint32_t now,uint32_t acquiredAt,
    uint32_t nextShot,int wanted) {
    return acquiredAt+storylandVcsPoliceHeliAcquisitionDelay(wanted)<now&&nextShot<now;
}

// A freshly allocated or invalid render transform can contain finite zeros.
// Only use it when it has a real forward/up basis and matches the live actor.
inline bool storylandVcsPoliceHeliMuzzleFrameValid(const std::array<float,16>& frame,
    const std::array<float,3>& actor) {
    if(!std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);})||
        !std::all_of(actor.begin(),actor.end(),[](float value){return std::isfinite(value);}))return false;
    const float forward=std::hypot(frame[4],frame[5],frame[6]);
    const float up=std::hypot(frame[8],frame[9],frame[10]);
    return forward>.25f&&up>.25f&&std::hypot(frame[12]-actor[0],frame[13]-actor[1],frame[14]-actor[2])<5.0f;
}
