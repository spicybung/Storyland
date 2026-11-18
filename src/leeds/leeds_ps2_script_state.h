#pragma once
#include "src/leeds/leeds_ps2_save.h"
#include <map>

namespace storyland_ps2_save {
struct PersistentScripts {
    bool vcs = false;
    uint32_t variableBytes = 0;
    uint32_t mainSize = 0;
    uint32_t largestMissionSize = 0;
    std::map<size_t, int32_t> globals;
};

inline PersistentScripts readPersistentScripts(const Slot& slot,
    const std::vector<uint8_t>& scm) {
    const Slot checked = decode(encode(slot), slot.directory, slot.name);
    const auto& script = checked.blocks[1];
    if (scm.size() < 32 || scm[8] != 2 || scm[9] != 0 || scm[10] != 6)
        throw std::runtime_error("Load the matching MAIN.SCM before loading this PS2 save.");
    const uint32_t variableBytes = word(scm, 11);
    if (variableBytes < 8 || variableBytes % 4 || variableBytes > 6656 * 4 ||
        word(script, 8) != variableBytes || variableBytes > script.size() - 12 ||
        !std::equal(scm.begin() + 8, scm.begin() + 16, script.begin() + 12))
        throw std::runtime_error("PS2 save and MAIN.SCM variable spaces do not match.");
    const size_t table = size_t(variableBytes) + 16;
    const uint32_t count = word(scm, table);
    if (count > variableBytes / 4 || table + 4 > scm.size() ||
        size_t(count) > (scm.size() - table - 4) / 2)
        throw std::runtime_error("Invalid MAIN.SCM persistent-variable table.");
    const size_t metadata = size_t(variableBytes) + 12;
    const uint32_t metadataSize = word(script, metadata);
    if (metadataSize < 20 || metadataSize > script.size() - metadata ||
        script.size() - metadata - metadataSize < 16)
        throw std::runtime_error("Invalid PS2 script metadata.");
    // The metadata length includes the two program-size words at its end.
    size_t sizes = metadata + metadataSize - 8;
    if (!checked.vcs) {
        sizes = 0;
        for (uint32_t threads = 1; threads <= 96; ++threads) {
            const size_t length = size_t(threads) * 540;
            if (length + 16 > script.size()) break;
            const size_t candidate = script.size() - length - 16;
            if (candidate < metadata || word(script, candidate + 12) != threads ||
                word(script, candidate) != word(scm, 0) ||
                word(script, candidate + 4) != word(scm, 4)) continue;
            if (sizes) throw std::runtime_error("Ambiguous LCS script metadata.");
            sizes = candidate;
        }
        if (!sizes) throw std::runtime_error("Cannot locate the LCS script program sizes.");
    } else {
        const uint32_t threads = word(script, sizes + 12);
        if (threads > 96 || size_t(threads) * 536 != script.size() - sizes - 16)
            throw std::runtime_error("Invalid VCS script thread table.");
    }
    PersistentScripts result;
    result.vcs = checked.vcs;
    result.variableBytes = variableBytes;
    result.mainSize = word(script, sizes);
    result.largestMissionSize = word(script, sizes + 4);
    if (result.mainSize != word(scm, 0) || result.largestMissionSize != word(scm, 4) ||
        result.mainSize > scm.size() || result.mainSize <= variableBytes)
        throw std::runtime_error("PS2 save and MAIN.SCM program sizes do not match.");
    for (uint32_t index = 0; index < count; ++index) {
        const size_t entry = table + 4 + size_t(index) * 2;
        const uint32_t offset = uint32_t(scm[entry]) | uint32_t(scm[entry + 1]) << 8;
        if (offset < 8 || offset % 4 || offset > variableBytes - 4)
            throw std::runtime_error("Invalid persistent-variable address in MAIN.SCM.");
        if (!result.globals.emplace(offset / 4, int32_t(word(script, 12 + offset))).second)
            throw std::runtime_error("Duplicate persistent-variable address in MAIN.SCM.");
    }
    return result;
}

inline Slot writePersistentScripts(const Slot& original, const std::vector<uint8_t>& scm,
    const std::vector<int32_t>& runtimeGlobals) {
    const auto layout = readPersistentScripts(original, scm);
    Slot result = original;
    for (const auto& entry : layout.globals) {
        if (entry.first >= runtimeGlobals.size())
            throw std::runtime_error("Runtime globals are incomplete; PS2 save was not changed.");
        const uint32_t bits = uint32_t(runtimeGlobals[entry.first]);
        const size_t offset = 12 + entry.first * 4;
        for (unsigned byte = 0; byte < 4; ++byte)
            result.blocks[1][offset + byte] = uint8_t(bits >> (byte * 8));
    }
    // Retail VCS LoadAllScripts (US 0x25A348) restores only persistent globals
    // and zeros the others. It does not resume the serialized thread pool.
    // Keep that pool and unconverted subsystems intact instead of serializing
    // Storyland handles or function frames into native engine structures.
    return result;
}
}
