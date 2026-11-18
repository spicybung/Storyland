#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <vector>

enum class StorylandNativePrimitive {
    Unsupported,
    Triangles,
    TriangleStrip,
    TriangleFan
};

template <typename EmitTriangle>
void storylandVisitNativeTriangles(const std::vector<uint32_t>& indices,
    StorylandNativePrimitive primitive, EmitTriangle emit,
    uint32_t restartIndex = std::numeric_limits<uint32_t>::max(),
    const std::vector<bool>* adc = nullptr) {
    if (primitive == StorylandNativePrimitive::Unsupported) return;
    std::array<uint32_t, 3> vertices{};
    size_t count = 0;
    for (size_t index = 0; index < indices.size(); ++index) {
        const uint32_t vertex = indices[index];
        if (vertex == restartIndex) {
            count = 0;
            continue;
        }
        const bool suppress = adc && index < adc->size() && (*adc)[index];
        if (primitive == StorylandNativePrimitive::Triangles) {
            vertices[count % 3] = vertex;
            ++count;
            if (count % 3 == 0 && !suppress) emit(vertices[0], vertices[1], vertices[2]);
        } else if (primitive == StorylandNativePrimitive::TriangleFan) {
            if (count == 0) vertices[0] = vertex;
            else if (count == 1) vertices[1] = vertex;
            else {
                if (!suppress) emit(vertices[0], vertices[1], vertex);
                vertices[1] = vertex;
            }
            ++count;
        } else {
            if (count < 2) vertices[count] = vertex;
            else {
                if (!suppress) {
                    if ((count & 1u) == 0) emit(vertices[0], vertices[1], vertex);
                    else emit(vertices[1], vertices[0], vertex);
                }
                vertices[0] = vertices[1];
                vertices[1] = vertex;
            }
            ++count;
        }
    }
}

struct StorylandDmaVifStream {
    std::vector<uint8_t> bytes;
    std::vector<size_t> sourceOffsets;
};

inline uint32_t storylandNativeU32(const std::vector<uint8_t>& data, size_t offset) {
    return uint32_t(data[offset]) | (uint32_t(data[offset + 1]) << 8) |
        (uint32_t(data[offset + 2]) << 16) | (uint32_t(data[offset + 3]) << 24);
}

inline bool storylandDecodeDmaVifStream(const std::vector<uint8_t>& data,
    size_t start, size_t end, StorylandDmaVifStream& stream, std::string& error,
    bool transferTagData = false) {
    stream = {};
    error.clear();
    end = std::min(end, data.size());
    std::array<size_t, 2> returnAddresses{};
    size_t depth = 0;
    size_t cursor = start;
    std::set<std::array<size_t, 4>> visited;
    const auto fail = [&](const char* reason) {
        error = reason;
        stream = {};
        return false;
    };
    const auto fits = [&](size_t offset, size_t length) {
        return offset <= end && length <= end - offset;
    };
    const auto append = [&](size_t offset, size_t length) {
        stream.bytes.insert(stream.bytes.end(), data.begin() + offset, data.begin() + offset + length);
        for (size_t index = 0; index < length; ++index) stream.sourceOffsets.push_back(offset + index);
    };
    while (true) {
        if ((cursor & 15u) != 0 || !fits(cursor, 16)) return fail("DMA tag outside the native block or not aligned");
        if (!visited.insert({cursor, depth, returnAddresses[0], returnAddresses[1]}).second)
            return fail("DMA chain cycle");
        if (visited.size() > 65536 || stream.bytes.size() > 16u * 1024u * 1024u)
            return fail("DMA chain exceeds decoding limit");
        const uint32_t tag = storylandNativeU32(data, cursor);
        const uint32_t address = storylandNativeU32(data, cursor + 4);
        const uint32_t id = (tag >> 28) & 7u;
        const size_t length = size_t(tag & 0xffffu) * 16u;
        const bool reference = id == 0 || id == 3 || id == 4;
        const bool usesAddress = reference || id == 2 || id == 5;
        if (usesAddress && (address & 0x80000000u)) return fail("DMA scratchpad address is not a file offset");
        const size_t payload = reference ? size_t(address) : cursor + 16u;
        if (!fits(payload, length)) return fail("DMA payload exceeds native block");
        if (transferTagData) append(cursor + 8u, 8u);
        append(payload, length);
        const size_t afterPayload = cursor + 16u + (reference ? 0u : length);
        if (id == 0 || id == 7) return true;
        if (id == 2) cursor = address;
        else if (id == 5) {
            if (depth == returnAddresses.size()) return fail("DMA CALL stack overflow");
            returnAddresses[depth++] = afterPayload;
            cursor = address;
        } else if (id == 6) {
            if (depth == 0) return true;
            cursor = returnAddresses[--depth];
            returnAddresses[depth] = 0;
        } else cursor = afterPayload;
    }
}

struct StorylandVifCommand {
    size_t offset = 0;
    size_t payload = 0;
    size_t end = 0;
    uint32_t word = 0;
    uint16_t count = 0;
    uint8_t command = 0;
};

inline bool storylandDecodeVifCommands(const std::vector<uint8_t>& data,
    std::vector<StorylandVifCommand>& commands, std::string& error) {
    commands.clear();
    error.clear();
    size_t cycleLength = 1;
    size_t writeLength = 1;
    size_t cursor = 0;
    while (cursor < data.size()) {
        if (data.size() - cursor < 4) {
            error = "Truncated VIF command";
            return false;
        }
        StorylandVifCommand entry;
        entry.offset = cursor;
        entry.word = storylandNativeU32(data, cursor);
        entry.command = uint8_t((entry.word >> 24) & 0x7fu);
        entry.count = uint16_t((entry.word >> 16) & 0xffu);
        if (entry.count == 0) entry.count = 256;
        entry.payload = cursor + 4;
        const uint16_t immediate = uint16_t(entry.word);
        size_t payloadBytes = 0;
        if ((entry.command & 0x60u) == 0x60u) {
            const size_t components = ((entry.command >> 2) & 3u) + 1u;
            const size_t format = entry.command & 3u;
            if (format == 3 && components != 4) {
                error = "Reserved VIF UNPACK format";
                return false;
            }
            size_t sourceCount = entry.count;
            if (writeLength > cycleLength)
                sourceCount = (sourceCount / writeLength) * cycleLength +
                    std::min(sourceCount % writeLength, cycleLength);
            const size_t bitsPerVertex = format == 3 ? 16u : components * (32u >> format);
            payloadBytes = (sourceCount * bitsPerVertex + 7u) / 8u;
        } else {
            switch (entry.command) {
            case 0x01:
                cycleLength = immediate & 0xffu;
                writeLength = immediate >> 8;
                if (!cycleLength) cycleLength = 256;
                if (!writeLength) writeLength = 256;
                break;
            case 0x20: payloadBytes = 4; break;
            case 0x30:
            case 0x31: payloadBytes = 16; break;
            case 0x4a: payloadBytes = size_t(entry.count) * 8u; break;
            case 0x50:
            case 0x51: payloadBytes = size_t(immediate ? immediate : 65536u) * 16u; break;
            case 0x00: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
            case 0x10: case 0x11: case 0x13: case 0x14: case 0x15: case 0x17:
                break;
            default:
                error = "Unknown VIF command";
                return false;
            }
        }
        payloadBytes = (payloadBytes + 3u) & ~size_t(3u);
        if (payloadBytes > data.size() - entry.payload) {
            error = "Truncated VIF payload";
            return false;
        }
        entry.end = entry.payload + payloadBytes;
        commands.push_back(entry);
        cursor = entry.end;
    }
    return true;
}
