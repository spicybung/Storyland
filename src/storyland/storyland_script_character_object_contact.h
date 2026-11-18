#pragma once

#include "src/storyland/storyland_script_object.h"

template <typename CollisionModels>
inline bool storylandScriptCharacterTouchesObject(
    const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::Object& object,
    int32_t characterHandle,
    const CollisionModels& models) {
    StorylandNativePoint point{world.x, world.y, world.z};
    float heading = world.heading;
    if (characterHandle != 0) {
        const auto character = world.missionCharacters.find(characterHandle);
        if (character == world.missionCharacters.end()) return false;
        point = {character->second.x, character->second.y, character->second.z};
        heading = character->second.heading;
    }
    const auto bounds = models.find(uint32_t(object.model));
    if (bounds == models.end() || !object.collisionEnabled) return false;
    return storylandScriptBoxesOverlap(
        {bounds->second.minimum, bounds->second.maximum,
            {object.x, object.y, object.z}, object.heading, object.rotationX, object.rotationY},
        {{-.35f, -.35f, 0}, {.35f, .35f, 1.8f}, point, heading});
}
