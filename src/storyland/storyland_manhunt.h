#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <zlib.h>

inline bool storylandUnpackManhunt(std::vector<uint8_t> &bytes, bool &compressed, std::string &error) {
    compressed = bytes.size() >= 4 && std::memcmp(bytes.data(), "Z2HM", 4) == 0;
    if (!compressed)
        return true;
    if (bytes.size() < 10) {
        error = "Truncated Manhunt compressed header.";
        return false;
    }
    const uint32_t size =
        uint32_t(bytes[4]) | uint32_t(bytes[5]) << 8 | uint32_t(bytes[6]) << 16 | uint32_t(bytes[7]) << 24;
    if (size < 32 || size > 512u * 1024u * 1024u) {
        error = "Invalid Manhunt decompressed size.";
        return false;
    }
    std::vector<uint8_t> unpacked(size);
    uLongf actual = size;
    if (uncompress(unpacked.data(), &actual, bytes.data() + 8, uLong(bytes.size() - 8)) != Z_OK ||
        actual != size) {
        error = "Manhunt compressed payload is corrupt or truncated.";
        return false;
    }
    bytes.swap(unpacked);
    return true;
}

inline bool storylandPackManhunt(const std::vector<uint8_t> &bytes, std::vector<uint8_t> &packed,
                                 std::string &error) {
    if (bytes.size() > UINT32_MAX) {
        error = "Manhunt resource is too large.";
        return false;
    }
    uLongf size = compressBound(uLong(bytes.size()));
    packed.resize(size_t(size) + 8);
    std::memcpy(packed.data(), "Z2HM", 4);
    for (unsigned i = 0; i < 4; ++i)
        packed[4 + i] = uint8_t(bytes.size() >> (i * 8));
    if (compress2(packed.data() + 8, &size, bytes.data(), uLong(bytes.size()), Z_BEST_COMPRESSION) != Z_OK) {
        error = "Could not compress Manhunt resource.";
        return false;
    }
    packed.resize(size_t(size) + 8);
    return true;
}
