#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

struct StorylandVcsCharacterAttachment {
    uint16_t type = 0;
    float rotationLimit = 0;
    int32_t savedWeapon = -1;
    int32_t savedAmmunition = 0;
    int32_t savedSlotWeapon = -1;
    uint16_t savedSlotAmmunition = 0;
    int32_t attachedWeapon = 0;

    template<class Archive> void saveState(Archive& archive) {
        archive(type, rotationLimit, savedWeapon, savedAmmunition,
            savedSlotWeapon, savedSlotAmmunition, attachedWeapon);
    }
};

inline float storylandVcsAttachmentTypeAngle(uint16_t type) {
    constexpr float halfTurn = 3.1415927410125732421875f;
    if (type == 1) return halfTurn * .5f;
    if (type == 2) return halfTurn;
    if (type == 3) return -halfTurn * .5f;
    return 0;
}

inline bool storylandVcsAttachedCharacterFrame(const std::array<float,16>& vehicle,
    const std::array<float,3>& offset, const StorylandVcsCharacterAttachment& attachment,
    bool mountedGunInitialized, float heading, std::array<float,16>& result,
    float* limitedHeading = nullptr) {
    if (!std::isfinite(heading) || !std::isfinite(attachment.rotationLimit) ||
        attachment.rotationLimit < 0 ||
        !std::all_of(vehicle.begin(), vehicle.end(), [](float value) { return std::isfinite(value); }) ||
        !std::all_of(offset.begin(), offset.end(), [](float value) { return std::isfinite(value); }))
        return false;
    constexpr float radians = 0.0174532942473888397216796875f;
    constexpr float fullTurn = 6.283185482025146484375f;
    const float objectAngle = std::atan2(-vehicle[4], vehicle[5]);
    const float typeAngle = storylandVcsAttachmentTypeAngle(attachment.type);
    float localAngle = attachment.type <= 3 ? typeAngle : heading * radians;
    float finalHeading = heading;
    if (!mountedGunInitialized) {
        const float desired = std::remainder(objectAngle + typeAngle, fullTurn);
        const float difference = std::remainder(heading * radians - desired, fullTurn);
        const float limited = std::remainder(desired + std::clamp(difference,
            -attachment.rotationLimit, attachment.rotationLimit), fullTurn);
        localAngle = limited - objectAngle;
        finalHeading = limited / radians;
    }
    const float cosine = std::cos(localAngle);
    const float sine = std::sin(localAngle);
    result = vehicle;
    for (size_t axis = 0; axis < 3; ++axis) {
        result[axis] = vehicle[axis] * cosine + vehicle[4 + axis] * sine;
        result[4 + axis] = -vehicle[axis] * sine + vehicle[4 + axis] * cosine;
        result[12 + axis] = vehicle[12 + axis] + vehicle[axis] * offset[0] +
            vehicle[4 + axis] * offset[1] + vehicle[8 + axis] * offset[2];
    }
    if (limitedHeading) *limitedHeading = finalHeading;
    return std::all_of(result.begin(), result.end(), [](float value) { return std::isfinite(value); });
}
