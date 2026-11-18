#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

struct StorylandPedTypeRecord {
    uint32_t offset = 0;
    uint32_t threatsOffset = 0, avoidOffset = 0;
    uint32_t threats = 0, avoid = 0;
};

inline bool storylandPedTypeRecord(const std::vector<uint8_t>& data, bool vcs,
    uint32_t type, StorylandPedTypeRecord& result) {
    result = {};
    if (type >= 23 || data.size() < 0x90) return false;
    const auto word = [&](size_t offset) -> uint32_t {
        if (offset > data.size() || data.size() - offset < 4) return 0;
        return uint32_t(data[offset]) | uint32_t(data[offset + 1]) << 8 |
            uint32_t(data[offset + 2]) << 16 | uint32_t(data[offset + 3]) << 24;
    };
    const uint32_t table = word(vcs ? 0x88 : 0x8C);
    if (table < 0xC0 || uint64_t(table) + 23 * 4 > data.size()) return false;
    const uint32_t first = word(table), second = word(size_t(table) + 4);
    if (second <= first) return false;
    const uint32_t stride = second - first;
    if (stride != 12 && stride != 32) return false;
    const uint32_t record = word(size_t(table) + type * 4);
    if (record < 0xC0 || uint64_t(record) + stride > data.size() || word(record) != (1u << type)) return false;
    result.offset = record;
    result.threatsOffset = record + stride - 8;
    result.avoidOffset = record + stride - 4;
    result.threats = word(result.threatsOffset);
    result.avoid = word(result.avoidOffset);
    return true;
}
