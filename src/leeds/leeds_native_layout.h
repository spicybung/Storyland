#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

struct StorylandVcsPspStreamingLayout {
    uint32_t table = 0;
    uint32_t count = 0;
    uint32_t textures = 0;
    uint32_t collisions = 0;
    uint32_t animations = 0;
};

inline uint32_t storylandNativeLayoutWord(const std::vector<uint8_t>& bytes, size_t offset) {
    return uint32_t(bytes[offset]) | uint32_t(bytes[offset + 1]) << 8 |
        uint32_t(bytes[offset + 2]) << 16 | uint32_t(bytes[offset + 3]) << 24;
}

inline bool storylandVcsPspStreamingLayout(const std::vector<uint8_t>& bytes,
    StorylandVcsPspStreamingLayout& result) {
    result = {};
    if (bytes.size() < 0x80) return false;
    const uint32_t manager = storylandNativeLayoutWord(bytes, 0x7C);
    if (manager < 0xE0 || uint64_t(manager) + 0xAC > bytes.size()) return false;
    const uint32_t table = storylandNativeLayoutWord(bytes, size_t(manager) + 0x98);
    const uint32_t count = storylandNativeLayoutWord(bytes, size_t(manager) + 0x14);
    const uint32_t textures = storylandNativeLayoutWord(bytes, size_t(manager) + 8);
    const uint32_t collisions = storylandNativeLayoutWord(bytes, size_t(manager) + 12);
    const uint32_t animations = storylandNativeLayoutWord(bytes, size_t(manager) + 16);
    if (table < 0xE0 || count < 256 || count > 100000 ||
        textures == 0 || textures > collisions || collisions > animations || animations > count ||
        uint64_t(table) + uint64_t(count) * 20 > bytes.size()) return false;
    size_t populated = 0;
    for (uint32_t index = 0; index < count; ++index) {
        const size_t row = size_t(table) + size_t(index) * 20;
        const uint32_t start = storylandNativeLayoutWord(bytes, row + 12);
        const uint32_t sectors = storylandNativeLayoutWord(bytes, row + 16);
        if (storylandNativeLayoutWord(bytes, row) != 0) return false;
        if ((start == 0 || start == UINT32_MAX) && sectors == 0) continue;
        if (start >= 0x400000 || sectors == 0 || sectors > 0x20000) return false;
        ++populated;
    }
    if (populated < 32) return false;
    result.table = table;
    result.count = count;
    result.textures = textures;
    result.collisions = collisions;
    result.animations = animations;
    return true;
}

struct StorylandNativeAnimationLayout {
    uint32_t trees = 0;
    uint32_t blocks = 0;
    uint32_t treeCount = 0;
    uint32_t blockCount = 0;
    uint32_t treeStride = 0;
    uint32_t blockStride = 0;
    uint32_t firstTreeOffset = 0;
    uint32_t treeCountOffset = 0;
};

inline bool storylandNativeAnimationLayout(const std::vector<uint8_t>& bytes, bool vcs,
    StorylandNativeAnimationLayout& result) {
    result = {};
    if (bytes.size() < 0x84u) return false;
    const uint32_t manager = storylandNativeLayoutWord(bytes, 0x80u);
    const size_t prefix = vcs ? 200u * 0x34u + 990u * 0x38u + 12u : 0u;
    if (manager < 32u || uint64_t(manager) + prefix + 20u > bytes.size()) return false;
    const size_t base = size_t(manager) + prefix;
    StorylandVcsPspStreamingLayout streaming;
    const bool psp = vcs && storylandVcsPspStreamingLayout(bytes, streaming);
    result.trees = storylandNativeLayoutWord(bytes, base);
    result.blocks = storylandNativeLayoutWord(bytes, base + 4u);
    result.treeCount = storylandNativeLayoutWord(bytes, base + 8u);
    result.blockCount = storylandNativeLayoutWord(bytes, base + 12u);
    result.treeStride = vcs ? 40u : 36u;
    result.blockStride = vcs ? (psp ? 44u : 48u) : 40u;
    result.firstTreeOffset = psp ? 24u : 28u;
    result.treeCountOffset = psp ? 28u : 32u;
    return result.trees >= 32u && result.blocks >= 32u &&
        result.treeCount <= 50000u && result.blockCount <= 4096u &&
        uint64_t(result.trees) + uint64_t(result.treeCount) * result.treeStride <= bytes.size() &&
        uint64_t(result.blocks) + uint64_t(result.blockCount) * result.blockStride <= bytes.size();
}
