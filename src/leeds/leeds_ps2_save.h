#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace storyland_ps2_save {
struct Slot {
    std::string directory;
    std::string name;
    bool vcs = false;
    std::vector<uint8_t> sourceBytes;
    std::array<std::vector<uint8_t>, 5> blocks;
};
inline uint32_t word(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4)
        throw std::runtime_error("Truncated PS2 save field.");
    return uint32_t(bytes[offset]) | uint32_t(bytes[offset + 1]) << 8 |
        uint32_t(bytes[offset + 2]) << 16 | uint32_t(bytes[offset + 3]) << 24;
}
inline Slot decode(const std::vector<uint8_t>& bytes, const std::string& directory,
    const std::string& name) {
    if (bytes.size() != 102400 && bytes.size() != 98304)
        throw std::runtime_error("Unsupported PS2 game save size.");
    uint32_t checksum = 0;
    for (size_t index = 0; index < bytes.size() - 4; ++index) checksum += bytes[index];
    if (checksum != word(bytes, bytes.size() - 4))
        throw std::runtime_error("PS2 game save checksum failed.");
    Slot slot;
    slot.sourceBytes = bytes;
    slot.directory = directory;
    slot.name = name;
    slot.vcs = bytes.size() == 98304;
    constexpr const char* tags[] = {"SIMP", "SRPT", "GRGE", "PLYR", "STAT"};
    size_t cursor = 0;
    for (size_t index = 0; index < slot.blocks.size(); ++index) {
        if (cursor > bytes.size() - 12 || std::memcmp(bytes.data() + cursor, tags[index], 4))
            throw std::runtime_error("Invalid PS2 save block order.");
        const uint32_t length = word(bytes, cursor + 4);
        cursor += 8;
        if (length > bytes.size() - 4 - cursor)
            throw std::runtime_error("Truncated PS2 save block.");
        slot.blocks[index].assign(bytes.begin() + cursor, bytes.begin() + cursor + length);
        cursor += (size_t(length) + 3) & ~size_t(3);
    }
    if (slot.blocks[0].size() != (slot.vcs ? 260u : 248u) ||
        slot.blocks[2].size() != (slot.vcs ? 9156u : 10692u) ||
        slot.blocks[3].size() != 400 ||
        slot.blocks[4].size() != (slot.vcs ? 958u : 897u))
        throw std::runtime_error("Unsupported PS2 save layout.");
    const auto& script = slot.blocks[1];
    if (script.size() < 12 || std::memcmp(script.data(), "SCR\0", 4) ||
        word(script, 4) != script.size() - 8 || word(script, 8) > script.size() - 12)
        throw std::runtime_error("Invalid PS2 script block.");
    return slot;
}
inline std::vector<uint8_t> encode(const Slot& slot) {
    const Slot original = decode(slot.sourceBytes, slot.directory, slot.name);
    if (slot.vcs != original.vcs)
        throw std::runtime_error("Cannot change the game of a PS2 save.");
    auto bytes = slot.sourceBytes;
    size_t cursor = 0;
    for (size_t index = 0; index < slot.blocks.size(); ++index) {
        if (slot.blocks[index].size() != original.blocks[index].size())
            throw std::runtime_error("Cannot resize a native PS2 save block.");
        cursor += 8;
        std::copy(slot.blocks[index].begin(), slot.blocks[index].end(), bytes.begin() + cursor);
        cursor += (slot.blocks[index].size() + 3) & ~size_t(3);
    }
    uint32_t checksum = 0;
    for (size_t index = 0; index < bytes.size() - 4; ++index) checksum += bytes[index];
    for (unsigned index = 0; index < 4; ++index)
        bytes[bytes.size() - 4 + index] = uint8_t(checksum >> (index * 8));
    decode(bytes, slot.directory, slot.name);
    return bytes;
}
inline Slot readSave(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    const auto length = input.tellg();
    if (!input || (length != 98304 && length != 102400))
        throw std::runtime_error("Unsupported PS2 game save size.");
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size())))
        throw std::runtime_error("Could not read the complete PS2 game save.");
    return decode(bytes, "", path.filename().u8string());
}
class MemoryCard {
    std::vector<uint8_t> bytes;
    uint32_t clusters = 0;
    uint32_t allocationStart = 0;
    uint32_t allocationEnd = 0;
    size_t pageStride = 0;
    std::set<uint32_t> directories;
    size_t entryBudget = 32768;

    std::vector<uint8_t> cluster(uint32_t index) const {
        if (index >= clusters) throw std::runtime_error("PS2 card cluster is out of range.");
        const size_t offset = size_t(index) * pageStride * 2;
        std::vector<uint8_t> result(1024);
        std::copy_n(bytes.begin() + offset, 512, result.begin());
        std::copy_n(bytes.begin() + offset + pageStride, 512, result.begin() + 512);
        return result;
    }
    uint32_t next(uint32_t index) const {
        const size_t indirectIndex = index / 256 / 256;
        if (indirectIndex >= 32) throw std::runtime_error("PS2 FAT index is out of range.");
        const auto indirect = cluster(word(bytes, 80 + indirectIndex * 4));
        const auto fat = cluster(word(indirect, (index / 256 % 256) * 4));
        return word(fat, (index % 256) * 4);
    }
    std::vector<uint8_t> read(uint32_t index, size_t length) const {
        if (length > size_t(allocationEnd) * 1024)
            throw std::runtime_error("PS2 file exceeds card capacity.");
        std::vector<uint8_t> result;
        result.reserve(length);
        std::set<uint32_t> visited;
        while (result.size() < length) {
            if (index >= allocationEnd || !visited.insert(index).second)
                throw std::runtime_error("Invalid or cyclic PS2 cluster chain.");
            const auto data = cluster(allocationStart + index);
            const size_t count = std::min(data.size(), length - result.size());
            result.insert(result.end(), data.begin(), data.begin() + count);
            const uint32_t link = next(index);
            if (!(link & 0x80000000u)) throw std::runtime_error("PS2 file uses a free cluster.");
            if (result.size() == length) {
                if (link != 0xffffffffu) throw std::runtime_error("PS2 file chain exceeds its declared size.");
                break;
            }
            if (link == 0xffffffffu) throw std::runtime_error("Truncated PS2 cluster chain.");
            index = link & 0x7fffffffu;
        }
        return result;
    }
    void directory(uint32_t first, uint32_t count, const std::string& parent,
        unsigned depth, std::vector<Slot>& slots) {
        if (depth > 8 || count < 2 || count > entryBudget || !directories.insert(first).second)
            throw std::runtime_error("Invalid PS2 directory tree.");
        entryBudget -= count;
        const auto data = read(first, size_t(count) * 512);
        for (size_t offset = 0; offset < data.size(); offset += 512) {
            const uint32_t mode = uint32_t(data[offset]) | uint32_t(data[offset + 1]) << 8;
            if (!(mode & 0x8000)) continue;
            const auto begin = data.begin() + offset + 64;
            const auto end = std::find(begin, begin + 32, uint8_t(0));
            if (end == begin + 32) throw std::runtime_error("Invalid PS2 filename.");
            const std::string name(begin, end);
            if (name == "." || name == "..") continue;
            if (name.empty()) throw std::runtime_error("Empty PS2 filename.");
            const uint32_t size = word(data, offset + 4);
            const uint32_t child = word(data, offset + 16);
            if (mode & 0x20) {
                directory(child, size, parent.empty() ? name : parent + "/" + name, depth + 1, slots);
            } else if ((mode & 0x10) && (size == 102400 || size == 98304) &&
                name.front() >= '1' && name.front() <= '8') {
                const auto payload = read(child, size);
                if (payload.size() >= 4 && !std::memcmp(payload.data(), "SIMP", 4))
                    slots.push_back(decode(payload, parent, name));
            }
        }
    }
public:
    std::vector<Slot> open(const std::filesystem::path& path) {
        directories.clear();
        entryBudget = 32768;
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        const auto length = input.tellg();
        if (!input || length < 512 || length > 128 * 1024 * 1024)
            throw std::runtime_error("Cannot read PS2 memory card, or its size is unsupported.");
        bytes.resize(size_t(length));
        input.seekg(0);
        if (!input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size())))
            throw std::runtime_error("Could not read the complete PS2 memory card.");
        if (std::all_of(bytes.begin(), bytes.end(), [](uint8_t value) { return value == 255; }))
            throw std::runtime_error("This PS2 memory card is blank and unformatted.");
        if (std::memcmp(bytes.data(), "Sony PS2 Memory Card Format ", 27) ||
            word(bytes, 40) != 0x00020200)
            throw std::runtime_error("Unsupported PS2 memory card format.");
        clusters = word(bytes, 48);
        allocationStart = word(bytes, 52);
        allocationEnd = word(bytes, 56);
        if (!clusters || clusters > 131072 || allocationStart >= clusters ||
            !allocationEnd || allocationEnd > clusters - allocationStart)
            throw std::runtime_error("Invalid PS2 memory card geometry.");
        if (bytes.size() == size_t(clusters) * 1024) pageStride = 512;
        else if (bytes.size() == size_t(clusters) * 1056) pageStride = 528;
        else throw std::runtime_error("Truncated or unsupported PS2 memory card image.");
        const uint32_t root = word(bytes, 60);
        if (root >= allocationEnd) throw std::runtime_error("Invalid PS2 root directory.");
        const auto first = cluster(allocationStart + root);
        std::vector<Slot> slots;
        directory(root, word(first, 4), "", 0, slots);
        return slots;
    }
};
}
