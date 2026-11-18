#pragma once

#include "src/storyland/storyland_bullet_reaction.h"

using StorylandVcsBulletReaction = StorylandBulletReaction;

template <typename Duration>
bool storylandStartVcsBulletReaction(StorylandVcsBulletReaction& reaction,
    uint8_t direction, float now, Duration resolveDuration) {
    return storylandStartNativeBulletReaction(reaction, true, direction, now, resolveDuration);
}
