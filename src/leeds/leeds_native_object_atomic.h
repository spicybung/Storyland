#pragma once
#include "src/leeds/leeds_dtz.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

inline bool storylandNativeObjectAtomicIndex(const std::vector<uint8_t>& bytes,
    StorylandDtzGameVariant game, int32_t model, bool renderDamaged,
    size_t& atomic, std::string& error) {
    atomic = 0;
    if (game != StorylandDtzGameVariant::Lcs && game != StorylandDtzGameVariant::Vcs) {
        error = "Script object atomic selection requires an identified LCS or VCS model layout.";
        return false;
    }
    const size_t countOffset = game == StorylandDtzGameVariant::Vcs ? 0x38 : 0x34;
    const size_t flagsOffset = game == StorylandDtzGameVariant::Vcs ? 0x3A : 0x36;
    const auto word = [&](size_t offset) {
        uint32_t value = 0;
        if (offset <= bytes.size() && bytes.size() - offset >= sizeof(value))
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    };
    if (bytes.size() < 0x40 || model < 0 || uint32_t(model) >= word(0x38)) {
        error = "Script object model is outside the native model-info table.";
        return false;
    }
    const uint32_t table = word(0x3C);
    const uint64_t entry = uint64_t(table) + uint64_t(uint32_t(model)) * 4;
    if (!table || entry + 4 > bytes.size()) {
        error = "Script object model-info pointer is outside GAME.DTZ.";
        return false;
    }
    const uint32_t info = word(size_t(entry));
    if (!info || uint64_t(info) + flagsOffset + sizeof(uint16_t) > bytes.size() ||
        (bytes[size_t(info) + 0x10] != 1 && bytes[size_t(info) + 0x10] != 3)) {
        error = "Script object does not have native simple-model atomic metadata.";
        return false;
    }
    const uint8_t count = bytes[size_t(info) + countOffset];
    uint16_t flags = 0;
    std::memcpy(&flags, bytes.data() + info + flagsOffset, sizeof(flags));
    atomic = renderDamaged || (flags & 8u) ? size_t(flags & 3u) : 0;
    if (!count || count > 3 || atomic >= count) {
        error = "Script object selects an atomic outside its authored simple model.";
        return false;
    }
    error.clear();
    return true;
}

inline bool storylandVcsObjectAtomicIndex(const std::vector<uint8_t>& bytes,int32_t model,
    bool renderDamaged,size_t& atomic,std::string& error) {
    return storylandNativeObjectAtomicIndex(bytes,StorylandDtzGameVariant::Vcs,
        model,renderDamaged,atomic,error);
}
