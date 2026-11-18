#include "src/storyland/storyland_manhunt_audio.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <set>

namespace {
bool fits(uint64_t offset, uint64_t size, uint64_t limit) {
    return offset <= limit && size <= limit - offset;
}
uint32_t word(const std::vector<uint8_t> &data, size_t offset) {
    return uint32_t(data[offset]) | uint32_t(data[offset + 1]) << 8 | uint32_t(data[offset + 2]) << 16 |
           uint32_t(data[offset + 3]) << 24;
}
uint16_t half(const std::vector<uint8_t> &data, size_t offset) {
    return uint16_t(data[offset]) | uint16_t(data[offset + 1]) << 8;
}
bool magic(const std::vector<uint8_t> &data, size_t offset, const char *signature) {
    return fits(offset, 4, data.size()) && std::memcmp(data.data() + offset, signature, 4) == 0;
}
} // namespace

bool storylandReadManhuntSounds(const std::vector<uint8_t> &bytes, std::vector<StorylandManhuntSound> &sounds,
                                std::string &error) {
    sounds.clear();
    error.clear();
    bool validEmptyBank = false;
    std::function<bool(size_t, size_t, const std::string &, unsigned)> parse;
    parse = [&](size_t base, size_t size, const std::string &name, unsigned depth) {
        if (depth > 8 || !fits(base, size, bytes.size()) || sounds.size() > 65536) {
            error = "Invalid Manhunt audio archive range or nesting.";
            return false;
        }
        const size_t limit = base + size;
        if (size >= 8 && magic(bytes, base, "AFS\0")) {
            const uint32_t count = word(bytes, base + 4);
            if (count > 65536 || !fits(base + 8, uint64_t(count) * 8, limit)) {
                error = "Truncated AFS entry table.";
                return false;
            }
            uint32_t names = 0;
            if (fits(base + 8 + size_t(count) * 8, 8, limit))
                names = word(bytes, base + 8 + size_t(count) * 8);
            for (uint32_t i = 0; i < count; ++i) {
                const uint32_t offset = word(bytes, base + 8 + i * 8),
                               length = word(bytes, base + 12 + i * 8);
                if (!length)
                    continue;
                if (!fits(offset, length, size)) {
                    error = "AFS entry extends beyond its parent archive.";
                    return false;
                }
                std::string child = name + "/" + std::to_string(i);
                if (names && fits(uint64_t(names) + uint64_t(i) * 48, 32, size)) {
                    const char *text =
                        reinterpret_cast<const char *>(bytes.data() + base + names + size_t(i) * 48);
                    size_t length = 0;
                    while (length < 32 && text[length])
                        ++length;
                    if (length)
                        child = name + "/" + std::string(text, length);
                }
                if (!parse(base + offset, length, child, depth + 1))
                    return false;
            }
            return true;
        }
        if (size >= 12 && (magic(bytes, base, "2AGs") || magic(bytes, base, "VAGs"))) {
            const bool version2 = bytes[base] == '2';
            const uint32_t length = word(bytes, base + 4), rate = half(bytes, base + 8),
                           count = bytes[base + 11];
            const size_t header = version2 ? 44 : 12;
            if (size < header || !count || count > 32 || rate < 4000 || rate > 192000 ||
                length != size - header || length % (64 * count)) {
                error = "Invalid VAS stream header.";
                return false;
            }
            std::set<unsigned> order;
            for (unsigned i = 0; i < count; ++i) {
                const unsigned lane = version2 ? bytes[base + 12 + i] : i;
                if (lane >= count || !order.insert(lane).second) {
                    error = "Invalid VAS stream ordering.";
                    return false;
                }
                sounds.push_back({name + (count > 1 ? "/stream_" + std::to_string(i) : ""),
                                  base + header + lane * 64, length / count, rate, count * 64, false});
            }
            return true;
        }
        if (size >= 20 && bytes[base] == 0x80 && bytes[base + 1] == 0) {
            auto bigHalf = [&](size_t offset) { return uint16_t(bytes[offset]) << 8 | bytes[offset + 1]; };
            auto bigWord = [&](size_t offset) {
                return uint32_t(bytes[offset]) << 24 | uint32_t(bytes[offset + 1]) << 16 |
                       uint32_t(bytes[offset + 2]) << 8 | bytes[offset + 3];
            };
            const uint32_t header = uint32_t(bigHalf(base + 2)) + 4;
            const uint32_t rate = bigWord(base + 8), samples = bigWord(base + 12);
            const uint16_t channels = bytes[base + 7];
            const bool ahx = bytes[base + 4] == 0x11 && bytes[base + 18] == 6;
            if (header < 20 || header > size ||
                (!ahx && (bytes[base + 4] != 3 || bytes[base + 5] != 18 || bytes[base + 6] != 4)) ||
                !channels || channels > 2 || rate < 4000 || rate > 192000 ||
                (!ahx && !fits(header, ((uint64_t(samples) + 31) / 32) * 18 * channels, size)) ||
                std::memcmp(bytes.data() + base + header - 6, "(c)CRI", 6) != 0) {
                error = "Unsupported or truncated ADX audio stream.";
                return false;
            }
            sounds.push_back({name, base, size, rate, 0, false, channels, samples, true, ahx});
            return true;
        }
        if (size >= 24 && word(bytes, base) == 3 && word(bytes, base + 4) >= 2 &&
            word(bytes, base + 4) <= 3) {
            const uint32_t block = word(bytes, base + 8), data = word(bytes, base + 16),
                           dataSize = word(bytes, base + 20);
            if (!fits(block, 0x3C, size) || !fits(data, dataSize, size) ||
                !magic(bytes, base + block, "SBlk") || word(bytes, base + block + 4) != 3) {
                error = "Unsupported or truncated Sony BNK header.";
                return false;
            }
            const size_t header = base + block;
            const uint32_t count = half(bytes, header + 0x18);
            const uint64_t grains = uint64_t(block) + word(bytes, header + 0x20),
                           waves = uint64_t(block) + word(bytes, header + 0x34);
            if (!fits(grains, uint64_t(count) * 8, size)) {
                error = "Truncated BNK grain table.";
                return false;
            }
            if (count == 0u && dataSize == 0u)
                validEmptyBank = true;
            for (uint32_t i = 0; i < count; ++i) {
                const uint32_t value = word(bytes, base + size_t(grains) + i * 8);
                if ((value >> 16) != 0x100)
                    continue;
                const uint64_t relative = waves + (value & 65535);
                if (!fits(relative, 24, size)) {
                    error = "Truncated BNK waveform.";
                    return false;
                }
                const size_t tone = base + size_t(relative);
                const uint32_t start = word(bytes, tone + 16);
                uint32_t length = word(bytes, tone + 20);
                const uint16_t flags = half(bytes, tone + 14);
                if (start >= dataSize) {
                    error = "Invalid BNK sample offset.";
                    return false;
                }
                if (!length) {
                    length = 16;
                    while (fits(uint64_t(start) + length, 16, dataSize)) {
                        const size_t packet = base + data + start + length;
                        bool silent = true;
                        for (size_t j = 0; j < 16; ++j)
                            silent = silent && bytes[packet + j] == 0;
                        if (silent)
                            break;
                        length += 16;
                        if ((bytes[packet + 1] & 7) == 7)
                            break;
                    }
                }
                if (!fits(start, length, dataSize)) {
                    error = "BNK sample extends beyond audio data.";
                    return false;
                }
                const unsigned center = bytes[tone + 2], note = center & 128 ? 256 - center : center;
                const double baseRate = center & 128 ? 48000.0 : 44100.0;
                const double rate = baseRate * std::pow(2.0, (60.0 - note + bytes[tone + 3] / 128.0) / 12.0);
                const uint32_t sampleRate =
                    uint32_t(std::clamp(std::round(rate / 10.0) * 10.0, 4000.0, 192000.0));
                sounds.push_back({name + "/sound_" + std::to_string(i), base + data + start, length,
                                  sampleRate, 0, (flags & 0x80) != 0});
            }
            return true;
        }
        return true;
    };
    if (!parse(0, bytes.size(), "audio", 0)) {
        sounds.clear();
        return false;
    }
    if (sounds.empty() && !validEmptyBank) {
        error = "No supported Manhunt PSP audio streams were found.";
        return false;
    }
    return true;
}

bool storylandDecodeManhuntSound(const std::vector<uint8_t> &bytes, const StorylandManhuntSound &sound,
                                 std::vector<int16_t> &pcm, std::string &error) {
    pcm.clear();
    error.clear();
    if (sound.ahx) {
        error = "AHX audio requires the MPEG audio decoder.";
        return false;
    }
    if (sound.adx) {
        if (!fits(sound.offset, sound.size, bytes.size()) || sound.size < 20 ||
            sound.sampleCount > 64u * 1024u * 1024u / sound.channels) {
            error = "ADX sample exceeds the decode limit.";
            return false;
        }
        const size_t base = size_t(sound.offset);
        const size_t header = (size_t(bytes[base + 2]) << 8 | bytes[base + 3]) + 4;
        const unsigned cutoff = unsigned(bytes[base + 16]) << 8 | bytes[base + 17];
        const double a = std::sqrt(2.0) - std::cos(2.0 * 3.14159265358979323846 * cutoff / sound.sampleRate);
        const double b = std::sqrt(2.0) - 1.0;
        const double c = (a - std::sqrt((a + b) * (a - b))) / b;
        const int coefficient1 = int(std::lrint(float(c * 8192.0))),
                  coefficient2 = int(std::lrint(float(-c * c * 4096.0)));
        int history1[2] = {0, 0}, history2[2] = {0, 0};
        pcm.resize(size_t(sound.sampleCount) * sound.channels);
        for (uint32_t block = 0; uint64_t(block) * 32 < sound.sampleCount; ++block) {
            for (unsigned channel = 0; channel < sound.channels; ++channel) {
                const size_t offset = base + header + (size_t(block) * sound.channels + channel) * 18;
                if (!fits(offset, 18, base + sound.size)) {
                    error = "Truncated ADX block.";
                    pcm.clear();
                    return false;
                }
                const int scale = (int(bytes[offset]) << 8 | bytes[offset + 1]);
                for (unsigned i = 0; i < 32 && uint64_t(block) * 32 + i < sound.sampleCount; ++i) {
                    int nibble = (bytes[offset + 2 + i / 2] >> ((i & 1) ? 0 : 4)) & 15;
                    if (nibble >= 8)
                        nibble -= 16;
                    int sample =
                        nibble * scale +
                        ((coefficient1 * history1[channel] + coefficient2 * history2[channel]) >> 12);
                    sample = std::clamp(sample, -32768, 32767);
                    history2[channel] = history1[channel];
                    history1[channel] = sample;
                    pcm[(size_t(block) * 32 + i) * sound.channels + channel] = int16_t(sample);
                }
            }
        }
        return true;
    }
    if (sound.pcm16) {
        if (!fits(sound.offset, sound.size, bytes.size()) || sound.size % 2 ||
            sound.size / 2 > 64u * 1024u * 1024u) {
            error = "Invalid BNK PCM sample range.";
            return false;
        }
        pcm.resize(size_t(sound.size / 2));
        for (size_t i = 0; i < pcm.size(); ++i)
            pcm[i] = int16_t(half(bytes, size_t(sound.offset) + i * 2));
        return true;
    }
    if (sound.size % 16 || sound.size / 16 > 64u * 1024u * 1024u / 28u) {
        error = "Manhunt ADPCM sample exceeds the decode limit.";
        return false;
    }
    static constexpr int coefficients[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};
    pcm.reserve(size_t(sound.size / 16 * 28));
    int previous = 0, previous2 = 0;
    for (uint64_t position = 0; position < sound.size; position += 16) {
        const uint64_t offset =
            sound.offset +
            (sound.blockStride ? (position / 64) * sound.blockStride + position % 64 : position);
        if (!fits(offset, 16, bytes.size())) {
            error = "Truncated Manhunt ADPCM block.";
            pcm.clear();
            return false;
        }
        const uint8_t *packet = bytes.data() + size_t(offset);
        const unsigned filter = packet[0] >> 4, shift = packet[0] & 15;
        if (filter > 4 || shift > 12) {
            error = "Invalid Manhunt ADPCM predictor.";
            pcm.clear();
            return false;
        }
        for (unsigned i = 0; i < 28; ++i) {
            int sample = (packet[2 + i / 2] >> ((i & 1) * 4)) & 15;
            if (sample >= 8)
                sample -= 16;
            sample = (sample * 4096) >> shift;
            sample += (previous * coefficients[filter][0] + previous2 * coefficients[filter][1] + 32) >> 6;
            sample = std::clamp(sample, -32768, 32767);
            pcm.push_back(int16_t(sample));
            previous2 = previous;
            previous = sample;
        }
        if ((packet[1] & 7) == 7)
            break;
    }
    return !pcm.empty();
}
