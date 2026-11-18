#pragma once

#include "src/re/reVCS/runtime/storyland_vcs_projectile.h"
#include <limits>

inline int storylandVcsProjectileOwner(const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::ScriptProjectile& projectile, size_t pedestrianCount) {
    auto source = projectile.source;
    if (source.kind == StorylandMissionEnvironment::DamageSourceKind::Vehicle)
        source = storylandVcsProjectileExplosionSource(world, projectile);
    if (source.kind == StorylandMissionEnvironment::DamageSourceKind::Character) {
        if (source.handle == 0) return -1;
        if (source.handle >= 0 && pedestrianCount < size_t(std::numeric_limits<int>::max()) &&
            size_t(source.handle) < size_t(std::numeric_limits<int>::max()) - pedestrianCount)
            return int(pedestrianCount) + source.handle;
    }
    return std::numeric_limits<int>::max();
}

template <typename VehicleContainer>
inline int storylandVcsProjectileSourceVehicle(const StorylandMissionEnvironment& world,
    int32_t scriptSlot, const VehicleContainer& vehicles) {
    if (scriptSlot < 0 || size_t(scriptSlot) >= world.scriptProjectiles.size()) return -1;
    const auto& projectile = world.scriptProjectiles[size_t(scriptSlot)];
    if (projectile.source.kind != StorylandMissionEnvironment::DamageSourceKind::Vehicle ||
        projectile.source.handle < 0) return -1;
    for (size_t index = 0; index < vehicles.size(); ++index)
        if (vehicles[index].scriptHandle == projectile.source.handle) return int(index);
    return -1;
}
