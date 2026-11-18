#pragma once

#include "src/re/reVCS/pedestrians/storyland_vcs_ped_motion.h"
#include <cmath>
#include <cstdint>

struct StorylandBulletImpact {
    int32_t weapon = -1;
    bool player = false;
    bool canKnockDown = true;
    float heading = 0.0f;
    float speed = 0.0f;
    float verticalSpeed = 0.0f;
    StorylandNativePoint sourceDirection{};
};

struct StorylandBulletReaction {
    int32_t animation = -1;
    float startedAt = 0.0f;
    float duration = 0.0f;
    float nextAllowedAt = 0.0f;
    StorylandVcsPedFallState fall;
    bool knockdown = false;
    bool dying = false;
    bool vehicleImpact = false, recoveryBlocked = false;
    uint64_t sourceVehicleIdentity = 0;
    uint8_t entitySeed = 0;
    bool entitySeedInitialized = false;

    bool active(float now) const {
        if (dying) return false;
        if (knockdown) return std::isfinite(now) && now >= startedAt &&
            ((animation >= (vehicleImpact ? 25 : 28) && animation <= 31) || animation == 132 || animation == 135);
        return animation >= 29 && animation <= 35 && std::isfinite(now) &&
            std::isfinite(startedAt) && std::isfinite(duration) && duration > 0.0f &&
            now >= startedAt && now - startedAt < duration;
    }

    float time(float now) const {
        return active(now) ? (knockdown ? fall.time : now - startedAt) : 0.0f;
    }

    float blendRate() const { return knockdown && fall.recovering ? 1000.0f : 8.0f; }
};

inline void storylandRetireBulletReaction(StorylandBulletReaction& reaction) {
    if(reaction.knockdown&&!reaction.fall.recovering&&(!reaction.fall.grounded||
        (reaction.vehicleImpact&&std::hypot(reaction.fall.velocity[0],reaction.fall.velocity[1])>.05f))) {
        reaction.dying=true;reaction.animation=-1;reaction.duration=0;
    } else reaction={};
}

template <typename Duration>
bool storylandStartNativeBulletReaction(StorylandBulletReaction& reaction, bool vcs,
    uint8_t direction, float now, Duration resolveDuration) {
    if (direction > 3 || !std::isfinite(now) || !std::isfinite(reaction.nextAllowedAt) ||
        now < reaction.nextAllowedAt || reaction.active(now)) return false;
    const int32_t animation = (vcs ? 32 : 29) + direction;
    const float duration = resolveDuration(0, animation);
    if (!std::isfinite(duration) || duration <= 0.0f) return false;
    reaction.animation = animation;
    reaction.vehicleImpact = false;
    reaction.recoveryBlocked = false;
    reaction.sourceVehicleIdentity = 0;
    reaction.dying = false;
    reaction.startedAt = now;
    reaction.duration = duration;
    return true;
}

inline float storylandPlayerBulletReactionInterval(bool vcs,int32_t weapon) {
    // SLUS_215.90, 0x2C6DBC..0x2C6DD8, player hit-association deadline.
    return vcs ? (weapon == 28 ? 2.5f : 1.0f) : 0.0f;
}

inline bool storylandVcsBulletKnockdownWeapon(int32_t weapon,bool player) {
    // SLUS_215.90, 0x2C6BD0..0x2C6C10.
    return (weapon >= 21 && weapon <= 23) ||
        (!player && (weapon == 20 || weapon == 34 || weapon == 37));
}
