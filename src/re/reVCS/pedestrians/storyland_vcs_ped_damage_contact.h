#pragma once
#include <array>
#include <cmath>
#include <cstdint>

inline bool storylandVcsPedDamageContactAllowed(bool collisionEnabled,
    bool residentSeated, bool drownsInWater, int32_t nativeWeapon) {
    if (nativeWeapon == 46 && !drownsInWater) return false;
    return collisionEnabled || residentSeated || nativeWeapon == 46;
}

inline bool storylandVcsVehicleExposesOccupants(int32_t nativeType, int32_t model) {
    return nativeType == 1 || nativeType == 2 || nativeType == 6 ||
        nativeType == 8 || nativeType == 9 || model == 170;
}

inline bool storylandVcsPedPrimaryBulletCollision(bool collisionEnabled,
    bool residentSeated, bool includeExposedOccupants, int32_t nativeType, int32_t model) {
    return collisionEnabled || (residentSeated && includeExposedOccupants &&
        storylandVcsVehicleExposesOccupants(nativeType, model));
}

inline bool storylandVcsPedSphereIntersection(const std::array<float,3>& start,
    const std::array<float,3>& end, const std::array<float,3>& center,
    float radius, float& nearest) {
    const float x = end[0] - start[0], y = end[1] - start[1], z = end[2] - start[2];
    const float hx = center[0] - start[0], hy = center[1] - start[1], hz = center[2] - start[2];
    const float projection = (hx * x + hy * y) + hz * z;
    const float lengthSquared = (x * x + y * y) + z * z;
    const float distanceSquared = (hx * hx + hy * hy) + hz * hz;
    const float discriminant = projection * projection - lengthSquared * (distanceSquared - radius * radius);
    if (!std::isfinite(discriminant) || discriminant < 0 || lengthSquared <= 0) return false;
    const float fraction = (projection - std::sqrt(discriminant)) / lengthSquared;
    if (!std::isfinite(fraction) || fraction < 0 || fraction > 1 || !(fraction < nearest)) return false;
    nearest = fraction;
    return true;
}

inline bool storylandVcsVehicleHeadIntersection(const std::array<float,3>& start,
    const std::array<float,3>& end, const std::array<float,3>& head, float& nearest) {
    const float x = end[0] - start[0], y = end[1] - start[1], z = end[2] - start[2];
    const float hx = head[0] - start[0], hy = head[1] - start[1], hz = (head[2] + .1f) - start[2];
    const float projection = (hx * x + hy * y) + hz * z;
    const float lengthSquared = (x * x + y * y) + z * z;
    const float distanceSquared = (hx * hx + hy * hy) + hz * hz;
    const float discriminant = projection * projection - lengthSquared * (distanceSquared - .2f * .2f);
    if (!std::isfinite(discriminant) || discriminant < 0 || lengthSquared <= 0) return false;
    const float fraction = (projection - std::sqrt(discriminant)) / lengthSquared;
    if (!std::isfinite(fraction) || fraction < 0 || fraction > 1 || !(fraction < nearest)) return false;
    nearest = fraction;
    return true;
}
