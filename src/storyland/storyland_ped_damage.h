#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

struct StorylandPedDamageResult {
    float health = 0, armour = 0;
    float applied = 0, absorbed = 0;
    bool accepted = false, fatal = false;
};

struct StorylandDeathAssociation {
    int32_t animation = -1;
    float blendRate = 4, speed = 1;
};

inline int32_t storylandNativeDrowningWeapon(bool vcs) { return vcs ? 46 : 43; }
inline int32_t storylandNativeFallWeapon(bool vcs) { return vcs ? 47 : 44; }
inline int32_t storylandNativeExplosionWeapon(bool vcs) { return vcs ? 44 : 41; }
inline int32_t storylandNativeRunoverWeapon(bool vcs) { return vcs ? 43 : 40; }
inline int32_t storylandNativeRammedWeapon(bool vcs) { return vcs ? 42 : 39; }
inline bool storylandNativeDeathAnimation(bool vcs,int32_t animation) {
    return (animation>=(vcs?16:13)&&animation<=(vcs?31:28))||
        animation==(vcs?40:37)||animation==(vcs?42:39)||animation==152||
        (vcs&&animation==121);
}

inline bool storylandNativeDamageBypassesArmour(bool vcs, int32_t weapon) {
    return weapon == storylandNativeDrowningWeapon(vcs) || weapon == storylandNativeFallWeapon(vcs);
}

inline uint8_t storylandNativePedDamageProof(bool vcs,int32_t weapon,uint8_t fallback=0) {
    if(weapon<0)return fallback;
    if(weapon<=11)return 16;
    if(vcs) {
        if(weapon==19||(weapon>=21&&weapon<=31)||weapon==34||weapon==35||weapon==45)return 1;
        if(weapon==33)return 2;
        if(weapon==12||weapon==15||weapon==17||weapon==18||weapon==32||weapon==44)return 4;
        if(weapon==42||weapon==43||weapon==47)return 8;
    } else {
        if(weapon==17||(weapon>=19&&weapon<=29)||weapon==32||weapon==33||weapon==42)return 1;
        if(weapon==31)return 2;
        if(weapon==12||weapon==15||weapon==30||weapon==41)return 4;
        if(weapon==39||weapon==40||weapon==44)return 8;
    }
    return 0;
}

inline StorylandPedDamageResult storylandApplyNativePedDamage(float health, float armour,
    float amount, bool player, float defendWeakness, bool bypassArmour, bool forceFatal = false) {
    StorylandPedDamageResult result{health, armour};
    if (!std::isfinite(health) || !std::isfinite(armour) || !std::isfinite(amount) ||
        !std::isfinite(defendWeakness) || health <= 0 || armour < 0 || amount <= 0 || defendWeakness < 0)
        return result;
    result.applied = amount * (player ? .33f : defendWeakness);
    if (!std::isfinite(result.applied)) return result;
    result.accepted = true;
    result.absorbed = bypassArmour ? 0.0f : std::min(armour, result.applied);
    result.armour -= result.absorbed;
    result.health -= result.applied - result.absorbed;
    result.fatal = forceFatal || result.health <= 1.0f;
    if (result.fatal) result.health = 0;
    return result;
}

inline bool storylandNativePedCriticalRoll(bool vcs, int32_t weapon, bool enabled,
    bool player, uint32_t random) {
    if (!enabled || player) return false;
    if (vcs) {
        if (weapon >= 28 && weapon <= 31) return true;
        if (weapon == 19 || (weapon >= 21 && weapon <= 27) || weapon == 35 || weapon == 45)
            return (random & (weapon == 21 ? 7u : 15u)) == 0;
    } else {
        if (weapon >= 26 && weapon <= 29) return true;
        if (weapon == 17 || (weapon >= 19 && weapon <= 25) || weapon == 33 || weapon == 42)
            return (random & (weapon == 19 ? 7u : 15u)) == 0;
    }
    return false;
}

inline bool storylandNativePedCriticalNeedsRandom(bool vcs, int32_t weapon) {
    return vcs ? weapon == 19 || (weapon >= 21 && weapon <= 27) || weapon == 35 || weapon == 45 :
        weapon == 17 || (weapon >= 19 && weapon <= 25) || weapon == 33 || weapon == 42;
}

inline StorylandDeathAssociation storylandNativeDeathAssociation(bool vcs, uint8_t kind,
    int32_t weapon, uint8_t piece = 0, uint8_t direction = 0, bool critical = false) {
    StorylandDeathAssociation result;
    result.animation = vcs ? 16 : 13;
    if (weapon == storylandNativeDrowningWeapon(vcs)) {
        result.animation = 152;
        return result;
    }
    if (critical && piece > 0 && piece <= 6) {
        result.animation = (vcs ? 20 : 17) + (piece == 6 ? 0 : piece);
        return result;
    }
    if (kind == 16 || kind == 4 || kind == 8 || weapon == (vcs ? 21 : 19) ||
        weapon == storylandNativeFallWeapon(vcs))
        result.animation = (vcs ? 28 : 25) + std::min<uint8_t>(direction, 3);
    return result;
}

inline uint8_t storylandNativeHitDirection(float heading, float sourceX, float sourceY) {
    if (!std::isfinite(heading) || !std::isfinite(sourceX) || !std::isfinite(sourceY) ||
        std::hypot(sourceX, sourceY) < .00001f) return 0;
    const float angle = std::remainder(std::atan2(sourceY, sourceX) * 57.29577951308232f - heading, 360.0f);
    if (angle >= -45 && angle < 45) return 0;
    if (angle >= 45 && angle < 135) return 1;
    if (angle >= 135 || angle < -135) return 2;
    return 3;
}
