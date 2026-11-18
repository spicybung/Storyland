#pragma once

#include "src/leeds/leeds_native_layout.h"
#include "src/storyland/storyland_play_test.h"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

inline bool storylandVcsPedAnimationDependency(const std::vector<uint8_t>& bytes,
    int32_t model, std::string& bank, std::string& error) {
    bank.clear();
    error.clear();
    if (model < 0) return true;
    if (bytes.size() < 0x84) {
        error = "Pedestrian animation dependency requires the native model registry.";
        return false;
    }
    const uint32_t count = storylandNativeLayoutWord(bytes, 0x38);
    const uint32_t models = storylandNativeLayoutWord(bytes, 0x3C);
    if (uint32_t(model) >= count || models < 32 ||
        uint64_t(models) + uint64_t(count) * 4 > bytes.size()) {
        error = "Pedestrian animation dependency references an invalid model.";
        return false;
    }
    const uint32_t info = storylandNativeLayoutWord(bytes, size_t(models) + size_t(model) * 4);
    if (info < 32 || uint64_t(info) + 0x11 > bytes.size()) {
        error = "Pedestrian animation dependency has no native model record.";
        return false;
    }
    if (bytes[size_t(info) + 0x10] != 7) return true;
    if (uint64_t(info) + 0x30 > bytes.size()) {
        error = "Pedestrian animation dependency record is truncated.";
        return false;
    }
    const int32_t index = int32_t(storylandNativeLayoutWord(bytes, size_t(info) + 0x2C));
    if (index == -1) return true;
    StorylandNativeAnimationLayout layout;
    if (index < 0 || !storylandNativeAnimationLayout(bytes, true, layout) ||
        uint32_t(index) >= layout.blockCount) {
        error = "Pedestrian animation dependency names an invalid native bank.";
        return false;
    }
    const size_t block = size_t(layout.blocks) + size_t(index) * layout.blockStride;
    for (size_t letter = 0; letter < 20 && bytes[block + letter]; ++letter) {
        const uint8_t value = bytes[block + letter];
        if (value < 32 || value > 126) {
            error = "Pedestrian animation dependency has an invalid bank name.";
            bank.clear();
            return false;
        }
        bank.push_back(char(value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value));
    }
    if (bank.empty()) {
        error = "Pedestrian animation dependency has an empty bank name.";
        return false;
    }
    return true;
}

inline bool storylandPreparePedModelAnimations(const StorylandDtzArchive& archive,
    const std::set<int32_t>& requested, const std::set<int32_t>& resident,
    StorylandPlayTestAnimation& player, StorylandPlayTestAnimation& pedestrian,
    std::string& error) {
    if (archive.gameVariant() != StorylandDtzGameVariant::Vcs) return true;
    for (int32_t model : requested) {
        if (model < 0 || resident.count(model)) continue;
        std::string bank;
        if (!storylandVcsPedAnimationDependency(archive.unpackedBytes(), model, bank, error))
            return false;
        if (bank.empty()) continue;
        // Native 3C7DF8 requests the model's declared animation block before
        // its first use. The model is published as resident only afterwards.
        if (!player.loadAnimationBank(archive, bank, error) ||
            !pedestrian.loadAnimationBank(archive, bank, error)) {
            error = "Pedestrian model " + std::to_string(model) + " animation bank " + bank + ": " + error;
            return false;
        }
    }
    return true;
}
