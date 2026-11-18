#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Gang identity is assigned by native population selection, independent of
// the model ID. A script-owned actor is never silently assigned ambient gang AI.
inline int storylandGangIndexFromPedType(int32_t pedType) {
    return pedType>=7&&pedType<=15?pedType-7:-1;
}

inline bool storylandGangJoinsPolice(bool opcode0367Flag,bool policePursuing,
    bool fortBaxterSoldier,bool scriptedPed) {
    return opcode0367Flag&&policePursuing&&!fortBaxterSoldier&&!scriptedPed;
}

inline bool storylandGangRespondsToAttack(int witnessGang,int attackedGang,
    bool fortBaxterSoldier,bool scriptedPed,float distance) {
    // The 30 m hearing radius is a Storyland gameplay parameter, NOT a
    // claimed retail CPU constant. The gang identity is script-authoritative.
    return witnessGang>=0&&attackedGang>=0&&witnessGang==attackedGang&&
        !fortBaxterSoldier&&!scriptedPed&&std::isfinite(distance)&&distance<=30.0f;
}

inline float storylandGangAimDeviation(float baseline,int accuracy) {
    if(!std::isfinite(baseline))return 0.0f;
    if(accuracy<=0)return baseline;
    // Approximate mapping of the retail SCM's gang accuracy configuration.
    // Do not present it as a measured engine precision algorithm.
    return baseline*(1.0f-std::clamp(float(accuracy)/100.0f,0.0f,1.0f));
}

inline float storylandGangShotDelay(float interval,float multiplier) {
    if(!std::isfinite(interval)||interval<0.0f)return 0.0f;
    if(!std::isfinite(multiplier)||multiplier<=0.0f)return interval;
    return interval/std::clamp(multiplier,0.25f,2.55f);
}
