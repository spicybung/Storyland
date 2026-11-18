#pragma once

#include <algorithm>
#include <cstdint>

inline bool storylandNativeWeaponUsesTwoHands(bool vcs, int32_t weapon) {
    // CWeapon::IsType2Handed: VCS 0x2CEA98, LCS 0x2AF190.
    if (vcs) {
        switch (weapon) {
        case 21: case 22: case 23: case 28: case 29: case 30:
        case 31: case 33: case 34: case 35: case 37:
            return true;
        default:
            return false;
        }
    }
    switch (weapon) {
    case 19: case 20: case 21: case 26: case 27: case 28:
    case 29: case 31: case 32: case 33: case 35:
        return true;
    default:
        return false;
    }
}

inline bool storylandWeaponSupportsAimedLocomotion(bool vcs,int32_t weapon,int32_t fireType,uint32_t nativeFlags) {
    const bool torsoAim=(nativeFlags&0x40u)!=0&&(nativeFlags&0x80u)==0;
    return fireType>=1&&fireType<=3&&
        (storylandNativeWeaponUsesTwoHands(vcs,weapon)||weapon==(vcs?32:30)||torsoAim);
}

inline int32_t storylandNativePlayerWeaponLocomotionGroup(bool vcs, int32_t weapon) {
    // CPlayerPed::SetRealMoveAnim: VCS 0x237890, LCS 0x34C590.
    if (vcs) {
        switch (weapon) {
        case 32:
            return 31;
        case 19: case 20: case 24: case 25: case 26: case 27:
        case 3: case 10: case 6: case 9:
            return 32;
        case 11: case 33: case 35:
            return 36;
        default:
            return storylandNativeWeaponUsesTwoHands(true, weapon) ? 33 : 30;
        }
    }
    switch (weapon) {
    case 30:
        return 32;
    case 17: case 18: case 22: case 23: case 24: case 25:
    case 3: case 10: case 6: case 9:
        return 33;
    case 11: case 31: case 33:
        return 37;
    default:
        return storylandNativeWeaponUsesTwoHands(false, weapon) ? 34 : 31;
    }
}

inline int32_t storylandNativePedWeaponLocomotionGroup(bool vcs, int32_t weapon,
    int32_t authoredGroup) {
    // VCS CPed's animation remapper changes movement associations 2..7 to
    // group 33 for exactly the seven bits in 0xC30E00000 (0x215748..0x21579C).
    // LCS has no equivalent remapper: retain its authored pedestrian group.
    if (vcs) {
        switch (weapon) {
        case 21: case 22: case 23: case 28: case 29: case 34: case 35:
            return 33;
        default:
            break;
        }
    }
    return std::max(0, authoredGroup);
}
