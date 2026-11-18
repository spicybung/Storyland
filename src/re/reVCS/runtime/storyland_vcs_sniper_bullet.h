#pragma once

#include "src/leeds/leeds_native_world.h"
#include <cstddef>
#include <cstdint>

enum class StorylandVcsBulletSource : uint8_t {
    None,
    Player,
    AmbientCharacter,
    MissionCharacter
};

struct StorylandVcsSniperBullet {
    bool active = false;
    StorylandNativePoint position{}, velocity{}, origin{};
    size_t weapon = SIZE_MAX;
    int32_t nativeWeapon = -1;
    float damage = 0;
    float expiresAtMilliseconds = 0;
    StorylandVcsBulletSource sourceKind = StorylandVcsBulletSource::None;
    int32_t sourceHandle = -1;
    uint32_t playerShotSerial = 0;
};
