#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace StorylandVcsLoadingChapterDetail {

inline uint32_t word(const std::vector<uint8_t>& bytes, size_t offset) {
    return uint32_t(bytes[offset]) | uint32_t(bytes[offset + 1]) << 8 |
        uint32_t(bytes[offset + 2]) << 16 | uint32_t(bytes[offset + 3]) << 24;
}

class Reader {
public:
    Reader(const std::vector<uint8_t>& source, size_t begin, size_t limit)
        : bytes(source), position(begin), end(limit) {}

    bool opcode(uint16_t value) {
        if (position > end || end - position < 2 ||
            bytes[position] != uint8_t(value) || bytes[position + 1] != uint8_t(value >> 8))
            return false;
        position += 2;
        return true;
    }

    bool integer(int32_t expected) {
        if (position >= end) return false;
        const uint8_t type = bytes[position++];
        if (type == 1) return expected == 0;
        const size_t count = type == 7 ? 1 : type == 8 ? 2 : type == 6 ? 4 : 0;
        if (!count || count > end - position) return false;
        int32_t value = 0;
        if (count == 1) value = int8_t(bytes[position]);
        else if (count == 2)
            value = int16_t(uint16_t(bytes[position]) | uint16_t(bytes[position + 1]) << 8);
        else value = int32_t(word(bytes, position));
        position += count;
        return value == expected;
    }

    bool global(size_t& slot) {
        if (position > end || end - position < 2 ||
            bytes[position] < 0xCD || bytes[position] >= 0xE6) return false;
        slot = size_t(bytes[position] - 0xCD) * 256 + bytes[position + 1];
        position += 2;
        return true;
    }

    bool comparison(size_t slot, int32_t value) {
        size_t actual = 0;
        return opcode(0x001B) && global(actual) && actual == slot && integer(value);
    }

    bool assignment(size_t slot, int32_t value) {
        size_t actual = 0;
        return opcode(0x0004) && global(actual) && actual == slot && integer(value);
    }

    bool target(size_t& destination) {
        if (position > end || end - position < 5 || bytes[position] != 6) return false;
        const int32_t label = int32_t(word(bytes, position + 1));
        position += 5;
        if (label < 0 || uint64_t(label) + 8 >= end) return false;
        destination = size_t(label) + 8;
        return true;
    }

    bool branch(uint16_t command, size_t& destination) {
        return opcode(command) && target(destination);
    }

    bool call(uint8_t inputs, size_t& destination) {
        if (!opcode(0x037B) || position > end || end - position < 3 ||
            bytes[position] != inputs || bytes[position + 1] != 0 || bytes[position + 2] >= 96)
            return false;
        position += 3;
        return target(destination);
    }

    const std::vector<uint8_t>& bytes;
    size_t position;
    size_t end;
};

inline bool transition(Reader& reader, size_t chapterSlot, int32_t chapter,
    const std::array<size_t, 4>& flags, size_t& setup, size_t& exit) {
    size_t nextChapter = 0, skipSetup = 0;
    if (!reader.comparison(chapterSlot, chapter) || !reader.branch(0x0022, nextChapter) ||
        !reader.opcode(0x0078) || !reader.integer(4)) return false;
    for (size_t flag : flags)
        if (!reader.comparison(flag, 1)) return false;
    if (!reader.branch(0x0022, skipSetup) || !reader.call(1, setup) ||
        !reader.integer(chapter + 1) || !reader.opcode(0x0001) || !reader.integer(0) ||
        reader.position != skipSetup || !reader.branch(0x0002, exit)) return false;
    return reader.position == nextChapter;
}

inline bool dispatcher(const std::vector<uint8_t>& bytes, size_t begin, size_t end,
    size_t& chapterSlot, size_t& setup) {
    // STORYSC advances only when all of the chapter's strand flags are set.
    Reader probe(bytes, begin, end);
    if (!probe.opcode(0x001B) || !probe.global(chapterSlot)) return false;
    Reader reader(bytes, begin, end);
    size_t firstExit = 0, secondExit = 0, secondSetup = 0;
    if (!transition(reader, chapterSlot, 0, {0x15, 0x12, 0x13, 0x14}, setup, firstExit) ||
        !transition(reader, chapterSlot, 1, {0x16, 0x15, 0x17, 0x18}, secondSetup, secondExit) ||
        setup != secondSetup || firstExit != secondExit) return false;
    const auto branchToExit = [&]() {
        size_t destination = 0;
        return reader.branch(0x0022, destination) && destination == firstExit;
    };
    if (!reader.comparison(chapterSlot, 2) || !branchToExit() ||
        !reader.comparison(0x296, 0) || !branchToExit() ||
        !reader.opcode(0x0078) || !reader.integer(5)) return false;
    for (size_t flag : {0x1Bu, 0x16u, 0x1Au, 0x1Du, 0x19u})
        if (!reader.comparison(flag, 1)) return false;
    size_t credits = 0;
    return branchToExit() && reader.opcode(0x0001) && reader.integer(5000) &&
        reader.call(0, credits) && reader.assignment(0x296, 1) &&
        reader.assignment(0x104, 1) && reader.position == firstExit;
}

inline bool headerTarget(const std::vector<uint8_t>& bytes, size_t offset, size_t& target) {
    Reader reader(bytes, offset, bytes.size());
    return reader.branch(0x0002, target) && target > reader.position;
}

inline std::optional<size_t> chapterGlobal(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 16 || bytes[15] != 'm') return std::nullopt;
    const uint64_t mainEnd64 = uint64_t(word(bytes, 0)) + 8;
    if (mainEnd64 > bytes.size() || mainEnd64 > 16u * 1024u * 1024u) return std::nullopt;
    const size_t mainEnd = size_t(mainEnd64);
    size_t globalsEnd = 0, missionHeader = 0, mainBegin = 0;
    if (!headerTarget(bytes, 8, globalsEnd) || !headerTarget(bytes, globalsEnd, missionHeader) ||
        !headerTarget(bytes, missionHeader, mainBegin) || mainBegin >= mainEnd || globalsEnd < 16)
        return std::nullopt;
    constexpr std::array<uint8_t, 8> mainName{0x38, 0x02, 0x0A, 'M', 'A', 'I', 'N', 0};
    if (mainEnd - mainBegin < mainName.size() ||
        !std::equal(mainName.begin(), mainName.end(), bytes.begin() + mainBegin))
        return std::nullopt;

    constexpr std::array<uint8_t, 11> storyName{
        0x38, 0x02, 0x0A, 'S', 'T', 'O', 'R', 'Y', 'S', 'C', 0};
    const auto end = bytes.begin() + mainEnd;
    const auto story = std::search(bytes.begin() + mainBegin, end, storyName.begin(), storyName.end());
    if (story == end) return std::nullopt;
    const size_t storyBegin = size_t(story - bytes.begin()) + storyName.size();
    const size_t storyEnd = storyBegin + std::min(size_t(1024), mainEnd - storyBegin);
    std::optional<size_t> identified;
    for (size_t offset = storyBegin; offset + 10 <= storyEnd; ++offset) {
        if (bytes[offset] != 0x7B || bytes[offset + 1] != 0x03) continue;
        Reader caller(bytes, offset, mainEnd);
        size_t step = 0, slot = 0, setup = 0;
        if (!caller.call(0, step) || step < mainBegin ||
            !dispatcher(bytes, step, mainEnd, slot, setup) || setup < mainBegin ||
            slot >= (globalsEnd - 16) / 4) continue;
        const uint8_t globalPage = uint8_t(0xCD + slot / 256);
        const uint8_t globalIndex = uint8_t(slot % 256);
        const std::array<uint8_t, 5> initialize{0x04, 0x00, globalPage, globalIndex, 0x01};
        const std::array<uint8_t, 5> writeChapter{0x35, 0x00, globalPage, globalIndex, 0x0D};
        const auto setupEnd = bytes.begin() + setup + std::min(size_t(4096), mainEnd - setup);
        if (std::search(bytes.begin() + mainBegin, story, initialize.begin(), initialize.end()) == story ||
            std::search(bytes.begin() + setup, setupEnd, writeChapter.begin(), writeChapter.end()) == setupEnd)
            continue;
        if (identified && *identified != slot) return std::nullopt;
        identified = slot;
    }
    return identified;
}

}

inline std::optional<unsigned> storylandReadVcsLoadingChapter(
    const std::vector<uint8_t>& fullScm, const std::vector<int32_t>& globals) {
    // Retail stores chapters as 0..2; loading artwork uses 1..3.
    const auto slot = StorylandVcsLoadingChapterDetail::chapterGlobal(fullScm);
    if (!slot || *slot >= globals.size()) return std::nullopt;
    const int32_t value = globals[*slot];
    if (value < 0 || value > 2) return std::nullopt;
    return unsigned(value + 1);
}
