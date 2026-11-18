#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace storyland_ctw {
class PspTimecycle {
public:
    static constexpr size_t WeatherCount = 8, ChannelCount = 55, HourCount = 24;
    static constexpr size_t ByteCount = WeatherCount * ChannelCount * HourCount * 4;
    static constexpr uint32_t ResourceHash = 0x6252DA25;

    static bool angular(size_t channel) {
        return channel == 3 || channel == 4 || channel == 8 || channel == 9 || channel == 34 || channel == 35;
    }
    static bool colour(size_t channel) {
        return channel <= 2 || (channel >= 5 && channel <= 7) ||
            (channel >= 10 && channel <= 18) || (channel >= 20 && channel <= 22) ||
            (channel >= 26 && channel <= 31) || (channel >= 36 && channel <= 44) || channel >= 49;
    }
    bool load(const std::vector<uint8_t>& bytes, std::string& error) {
        loaded = false;
        if (bytes.size() != ByteCount) { error = "PSP timecycle.dat must contain 42,240 bytes."; return false; }
        for (size_t index = 0; index < source.size(); ++index) {
            const size_t offset = index * 4;
            const uint32_t word = uint32_t(bytes[offset]) | uint32_t(bytes[offset + 1]) << 8 |
                uint32_t(bytes[offset + 2]) << 16 | uint32_t(bytes[offset + 3]) << 24;
            std::memcpy(&source[index], &word, 4);
            if (source[index] != -4096 && (source[index] < 0 || source[index] > 255 * 4096)) {
                error = "PSP timecycle.dat contains a value outside the native byte-channel range.";
                return false;
            }
        }
        for (size_t weather = 0; weather < WeatherCount; ++weather) {
            for (size_t channel = 0; channel < ChannelCount; ++channel) {
                const size_t base = (weather * ChannelCount + channel) * HourCount;
                for (size_t hour = 0; hour < HourCount; ++hour) {
                    int32_t fixed = source[base + hour];
                    if (fixed == -4096) {
                        size_t before = 1, after = 1;
                        while (before <= HourCount && source[base + (hour + HourCount - before) % HourCount] == -4096) ++before;
                        while (after <= HourCount && source[base + (hour + after) % HourCount] == -4096) ++after;
                        if (before <= HourCount) {
                            const int32_t start = source[base + (hour + HourCount - before) % HourCount];
                            int32_t difference = source[base + (hour + after) % HourCount] - start;
                            if (angular(channel)) {
                                if (difference < -128 * 4096) difference += 256 * 4096;
                                if (difference > 128 * 4096) difference -= 256 * 4096;
                            }
                            fixed = start + (difference / int32_t(before + after)) * int32_t(before);
                        }
                    }
                    // Native Init truncates to bytes before applying the colour curve.
                    const int32_t integer = fixed >= 0 ? fixed / 4096 : -int32_t((int64_t(-fixed) + 4095) / 4096);
                    uint8_t value = uint8_t(integer);
                    if (colour(channel)) value = uint8_t(std::pow(float(value) / 255.0f, 0.6f) * 255.0f);
                    resolved[base + hour] = value;
                }
            }
        }
        loaded = true;
        error.clear();
        return true;
    }
    bool ready() const { return loaded; }
    int32_t raw(size_t weather, size_t channel, size_t hour) const {
        if (!loaded || weather >= WeatherCount || channel >= ChannelCount || hour >= HourCount) return -4096;
        return source[(weather * ChannelCount + channel) * HourCount + hour];
    }
    uint8_t value(size_t weather, size_t channel, size_t hour) const {
        if (!loaded || weather >= WeatherCount || channel >= ChannelCount || hour >= HourCount) return 0;
        return resolved[(weather * ChannelCount + channel) * HourCount + hour];
    }
    // Native InitInterpolators uses 12-bit hours and a 1/2048 fractional step.
    float sample(size_t weather, size_t channel, float hour) const {
        if (!loaded || weather >= WeatherCount || channel >= ChannelCount || !std::isfinite(hour)) return 0;
        hour = std::fmod(std::fmod(hour, 24.0f) + 24.0f, 24.0f);
        const size_t first = size_t(hour), next = (first + 1) % HourCount;
        float start = value(weather, channel, first);
        float difference = float(value(weather, channel, next)) - start;
        if (angular(channel)) {
            if (difference >= 128) difference -= 256;
            else if (difference <= -128) difference += 256;
        }
        return start + difference * (std::floor((hour - float(first)) * 2048.0f) / 2048.0f);
    }
    static const char* channelName(size_t channel) {
        switch (channel) {
        case 0: return "Light diffuse R"; case 1: return "Light diffuse G"; case 2: return "Light diffuse B";
        case 3: return "Light azimuth"; case 4: return "Light elevation";
        case 13: return "Light ambient R"; case 14: return "Light ambient G"; case 15: return "Light ambient B";
        case 16: return "Light specular R"; case 17: return "Light specular G"; case 18: return "Light specular B";
        case 20: return "Fog R"; case 21: return "Fog G"; case 22: return "Fog B";
        case 23: return "Fog start / far clip"; case 24: return "Fog end / far clip";
        case 29: return "Background R"; case 30: return "Background G"; case 31: return "Background B";
        case 34: return "Shadow azimuth"; case 35: return "Shadow elevation";
        default: return "Unmapped";
        }
    }
private:
    bool loaded = false;
    std::array<int32_t, WeatherCount * ChannelCount * HourCount> source{};
    std::array<uint8_t, WeatherCount * ChannelCount * HourCount> resolved{};
};
}
