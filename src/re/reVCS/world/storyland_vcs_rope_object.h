#pragma once

#include "src/storyland/storyland_script_object.h"

#include <algorithm>
#include <array>
#include <cmath>

struct StorylandVcsRopeObjectContact {
    std::array<float,3> objectTop{};
    std::array<float,3> hookBottom{};
    std::array<float,3> acquiredHookPosition{};
    float objectMaximumZ = 0;
    float hookMinimumZ = 0;
};

inline bool storylandVcsRopeCurrentObject(const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::Object& object,
    StorylandMissionEnvironment::Object& current) {
    current = object;
    std::array<float,16> parent{};
    if (object.attachedCharacter >= 0) {
        if (!world.characterBoneMatrix ||
            !world.characterBoneMatrix(object.attachedCharacter, object.attachedBone, parent)) return false;
        return storylandApplyScriptObjectAttachment(current, parent,
            object.attachmentOffset, object.attachmentRotation);
    }
    if (object.attachedVehicle >= 0) {
        const auto vehicle = world.missionVehicles.find(object.attachedVehicle);
        if (vehicle == world.missionVehicles.end() || vehicle->second.deleted ||
            !world.vehicleWorldMatrix || !world.vehicleWorldMatrix(object.attachedVehicle, parent)) return false;
        return storylandApplyScriptObjectAttachment(current, parent,
            object.attachmentOffset, object.attachmentRotation);
    }
    return true;
}

inline bool storylandVcsRopeObjectContact(
    const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::Object& object,
    const StorylandMissionEnvironment::Object& hook,
    const std::array<float,3>& ropeEnd,
    StorylandVcsRopeObjectContact& contact) {
    if (!world.objectWorld || object.model < 0 || hook.model < 0) return false;
    const auto objectCollision = world.objectWorld->collisionModels.find(uint32_t(object.model));
    const auto hookCollision = world.objectWorld->collisionModels.find(uint32_t(hook.model));
    if (objectCollision == world.objectWorld->collisionModels.end() ||
        hookCollision == world.objectWorld->collisionModels.end()) return false;
    const float maximum = objectCollision->second.maximum.z;
    const float minimum = hookCollision->second.minimum.z;
    if (!std::isfinite(maximum) || !std::isfinite(minimum)) return false;
    const auto objectMatrix = storylandScriptObjectMatrix(object.x, object.y, object.z,
        object.rotationX, object.rotationY, object.heading);
    const auto hookMatrix = storylandScriptObjectMatrix(hook.x, hook.y, hook.z,
        hook.rotationX, hook.rotationY, hook.heading);
    const auto finite = [](float value) { return std::isfinite(value); };
    if (!std::all_of(objectMatrix.begin(), objectMatrix.end(), finite) ||
        !std::all_of(hookMatrix.begin(), hookMatrix.end(), finite) ||
        !std::all_of(ropeEnd.begin(), ropeEnd.end(), finite)) return false;
    StorylandVcsRopeObjectContact candidate;
    candidate.objectMaximumZ = maximum;
    candidate.hookMinimumZ = minimum;
    for (size_t axis = 0; axis < 3; ++axis) {
        candidate.objectTop[axis] = objectMatrix[12 + axis] + objectMatrix[8 + axis] * maximum;
        candidate.hookBottom[axis] = ropeEnd[axis] + hookMatrix[8 + axis] * minimum;
        candidate.acquiredHookPosition[axis] = objectMatrix[12 + axis] +
            objectMatrix[8 + axis] * (maximum - minimum);
    }
    if (!std::all_of(candidate.objectTop.begin(), candidate.objectTop.end(), finite) ||
        !std::all_of(candidate.hookBottom.begin(), candidate.hookBottom.end(), finite) ||
        !std::all_of(candidate.acquiredHookPosition.begin(), candidate.acquiredHookPosition.end(), finite)) return false;
    contact = candidate;
    return true;
}

inline bool storylandVcsRopeTouchesObject(const StorylandVcsRopeObjectContact& contact) {
    const float x = contact.objectTop[0] - contact.hookBottom[0];
    const float y = contact.objectTop[1] - contact.hookBottom[1];
    const float z = contact.objectTop[2] - contact.hookBottom[2];
    return std::sqrt((x * x + y * y) + z * z) < 1.5f;
}

inline bool storylandAcquireVcsRopeObject(StorylandMissionEnvironment& world,
    StorylandMissionEnvironment::VehicleRope& rope) {
    if (rope.hookObject < 0 || rope.attachedObject >= 0 || rope.attachedVehicle >= 0) return false;
    const auto hook = std::find_if(world.objects.begin(), world.objects.end(),
        [&](const auto& item) { return item.handle == rope.hookObject; });
    if (hook == world.objects.end()) return false;
    storylandPrepareObjectPoolIndices(world);
    StorylandMissionEnvironment::Object* selected = nullptr;
    StorylandMissionEnvironment::Object selectedPose;
    StorylandVcsRopeObjectContact selectedContact;
    for (auto& object : world.objects) {
        if (!object.ropeLiftable || object.handle == rope.hookObject ||
            (selected && object.nativePoolIndex <= selected->nativePoolIndex)) continue;
        if (std::any_of(world.vehicleRopes.begin(), world.vehicleRopes.end(),
            [&](const auto& other) {
                return other.first != rope.owner && other.second.attachedObject == object.handle;
            })) continue;
        StorylandMissionEnvironment::Object current;
        StorylandVcsRopeObjectContact contact;
        if (!storylandVcsRopeCurrentObject(world, object, current) ||
            !storylandVcsRopeObjectContact(world, current, *hook, rope.end, contact) ||
            !storylandVcsRopeTouchesObject(contact)) continue;
        selected = &object;
        selectedPose = std::move(current);
        selectedContact = contact;
    }
    if (!selected) return false;
    rope.attachedObject = selected->handle;
    rope.attachedVehicle = -1;
    rope.end = selectedContact.acquiredHookPosition;
    selected->x = selectedPose.x;
    selected->y = selectedPose.y;
    selected->z = selectedPose.z;
    selected->rotationX = selectedPose.rotationX;
    selected->rotationY = selectedPose.rotationY;
    selected->heading = selectedPose.heading;
    // Native object acquisition calls 0x132318, clearing both attachment references.
    selected->attachedVehicle = -1;
    selected->attachedCharacter = -1;
    selected->dynamic = false;
    return true;
}

inline bool storylandMoveVcsRopeObject(StorylandMissionEnvironment& world,
    StorylandMissionEnvironment::VehicleRope& rope) {
    if (rope.hookObject < 0 || rope.attachedObject < 0 || rope.attachedVehicle >= 0) return false;
    const auto object = std::find_if(world.objects.begin(), world.objects.end(),
        [&](const auto& item) { return item.handle == rope.attachedObject; });
    const auto hook = std::find_if(world.objects.begin(), world.objects.end(),
        [&](const auto& item) { return item.handle == rope.hookObject; });
    if (object == world.objects.end() || hook == world.objects.end()) return false;
    StorylandVcsRopeObjectContact contact;
    if (!storylandVcsRopeObjectContact(world, *object, *hook, rope.end, contact)) return false;
    const auto matrix = storylandScriptObjectMatrix(0, 0, 0,
        object->rotationX, object->rotationY, object->heading);
    const float separation = contact.objectMaximumZ - contact.hookMinimumZ;
    std::array<float,3> position{};
    for (size_t axis = 0; axis < 3; ++axis) {
        position[axis] = rope.end[axis] - matrix[8 + axis] * separation;
        if (!std::isfinite(position[axis]) || !std::isfinite(rope.velocity[axis])) return false;
    }
    object->x = position[0];
    object->y = position[1];
    object->z = position[2];
    object->velocity = rope.velocity;
    object->dynamic = false;
    hook->rotationX = object->rotationX;
    hook->rotationY = object->rotationY;
    hook->heading = object->heading;
    hook->x = rope.end[0];
    hook->y = rope.end[1];
    hook->z = rope.end[2];
    hook->velocity = rope.velocity;
    hook->dynamic = false;
    return true;
}
