#pragma once

#include "src/re/reVCS/runtime/storyland_vcs_attachment.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include "src/storyland/storyland_weapon_attachment.h"

inline bool storylandPlayerPoseWorldFrame(bool vcs, const StorylandMissionEnvironment& world,
    const StorylandModelFile& model, const StorylandNativePoint& position, float heading,
    float height, std::array<float,16>& result) {
    if (!vcs || !world.activeVcsCharacterAttachment(0))
        return storylandStandingPedWorldFrame(model, position, heading, height, result);
    std::array<float,16> root{};
    if (!storylandVcsCharacterEntityFrame(world, 0, root)) return false;
    const auto orientation = storylandScriptObjectMatrix(
        0, 0, 0, 0, 0, 90 - storylandPedModelForwardAngle(model));
    result = storylandVehicleMatrixProduct(root, orientation);
    return std::all_of(result.begin(), result.end(), [](float value) { return std::isfinite(value); });
}
