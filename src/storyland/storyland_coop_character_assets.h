#pragma once

#include "src/storyland/storyland_play_test.h"
#include "src/graphics/storyland_native_graphics.h"

inline bool storylandLoadCoopCharacter(const StorylandDtzArchive& archive,
    const std::string& name, StorylandPlayTestAsset& result, std::string& error) {
    uint32_t model = UINT32_MAX;
    if (!archive.findModelIdByName(name, model)) {
        error = "Co-op character is missing from the game data: " + name;
        return false;
    }
    std::vector<StorylandPlayTestAsset> assets;
    if (!storylandLoadPlayTestModels(archive, {name}, assets, error) || assets.size() != 1) return false;
    auto& asset = assets.front();
    if (asset.model.modelKind() != StorylandModelKind::PedModel || asset.model.armatureBones().empty()) {
        error = "Co-op character has no pedestrian skeleton: " + name;
        return false;
    }
    const bool vcs = archive.gameVariant() == StorylandDtzGameVariant::Vcs;
    asset.pedStats = storylandDecodePedStats(archive.unpackedBytes(), model, vcs);
    const auto clothing = storylandDecodePedClothing(archive.unpackedBytes(), model, vcs);
    std::array<uint32_t,6> colors{};
    if (storylandTrySelectNativePedClothing(clothing, -1, 0, colors)) {
        for (auto& color : asset.materialColors) color = storylandNativePedMaterialColor(color, colors);
    }
    result = std::move(asset);
    return true;
}
