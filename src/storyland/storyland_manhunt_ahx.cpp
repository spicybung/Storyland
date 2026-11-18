#include "src/storyland/storyland_manhunt_audio.h"
#include <algorithm>
#include <array>
#include <cstring>

bool storylandBuildManhuntAhxMpeg(const std::vector<uint8_t> &bytes, const StorylandManhuntSound &sound,
                                  std::vector<uint8_t> &mpeg, std::string &error) {
    mpeg.clear();
    if (sound.offset > bytes.size() || sound.size > bytes.size() - sound.offset || sound.size < 36) {
        error = "AHX stream range is invalid.";
        return false;
    }
    const size_t base = size_t(sound.offset), end = base + size_t(sound.size);
    size_t cursor = base + (size_t(bytes[base + 2]) << 8 | bytes[base + 3]) + 4;
    if (bytes[base + 19] != 0) {
        error = "Encrypted AHX streams are not supported.";
        return false;
    }
    constexpr size_t paddedFrame = 1044;
    static constexpr unsigned allocationBits[30] = {4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2,
                                                    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
    static constexpr unsigned offsets[5][16] = {
        {0}, {0}, {0, 1, 3, 4}, {0, 1, 3, 4, 5, 6, 7, 8}, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}};
    static constexpr int quantizerBits[17] = {-5, -7, 3, -10, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const size_t frames = (uint64_t(sound.sampleCount) + 1151) / 1152;
    if (frames > 64u * 1024u * 1024u / paddedFrame) {
        error = "AHX stream exceeds the decode limit.";
        return false;
    }
    for (size_t frame = 0; frame < frames; ++frame) {
        if (cursor > end || end - cursor < 4 ||
            std::memcmp(bytes.data() + cursor, "\xFF\xF5\xE0\xC0", 4) != 0) {
            error = "AHX MPEG frame header is invalid.";
            return false;
        }
        size_t bit = 32;
        bool valid = true;
        auto read = [&](unsigned count) {
            unsigned value = 0;
            for (unsigned i = 0; i < count; ++i) {
                if (bit >= (end - cursor) * 8 || bit >= paddedFrame * 8) {
                    valid = false;
                    return 0u;
                }
                value = (value << 1) | ((bytes[cursor + bit / 8] >> (7 - bit % 8)) & 1);
                ++bit;
            }
            return value;
        };
        std::array<unsigned, 30> allocation{}, scfsi{};
        for (size_t band = 0; band < 30; ++band)
            allocation[band] = read(allocationBits[band]);
        for (size_t band = 0; band < 30; ++band)
            if (allocation[band])
                scfsi[band] = read(2);
        for (size_t band = 0; band < 30; ++band)
            if (allocation[band]) {
                const unsigned factors = scfsi[band] == 0 ? 3 : scfsi[band] == 2 ? 1 : 2;
                read(factors * 6);
            }
        for (unsigned granule = 0; granule < 12; ++granule)
            for (size_t band = 0; band < 30; ++band)
                if (allocation[band]) {
                    const int bits = quantizerBits[offsets[allocationBits[band]][allocation[band] - 1]];
                    read(bits < 0 ? unsigned(-bits) : unsigned(bits * 3));
                }
        const size_t length = (bit + 7) / 8;
        if (!valid || length > paddedFrame || length > end - cursor) {
            error = "AHX MPEG frame is truncated.";
            return false;
        }
        const size_t output = mpeg.size();
        mpeg.resize(output + paddedFrame, 0);
        std::copy_n(bytes.data() + cursor, length, mpeg.data() + output);
        cursor += length;
    }
    error.clear();
    return true;
}
