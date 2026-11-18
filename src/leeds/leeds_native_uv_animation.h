#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

struct StorylandNativeTextureUvAnimation {
    uint8_t u = 0;
    uint8_t v = 0;

    static StorylandNativeTextureUvAnimation fromVcsRaster(uint32_t rasterProperties) {
        return {uint8_t((rasterProperties >> 24u) & 3u),
            uint8_t((rasterProperties >> 26u) & 3u)};
    }

    bool animated() const {
        return u != 3u && v != 3u &&
            ((u == 1u || u == 2u) || (v == 1u || v == 2u));
    }

    std::array<float, 2> offset(double seconds) const {
        if (!animated() || !std::isfinite(seconds) || seconds < 0.0) return {};
        // SLUS_215.90 0x23E7AC advances the two world phases by 0.05 at
        // 30 Hz. 0x2407B8 reads the per-axis mode from RslRaster +0x04.
        // Mode 3 selects a lighting effect and leaves the texture matrix static.
        const double phase = std::fmod(seconds, 2.0 / 3.0) * 1.5;
        const auto component = [phase](uint8_t mode) {
            if (mode == 1u) return float(phase);
            if (mode == 2u) return float(std::floor(phase * 2.0) * 0.25);
            return 0.0f;
        };
        return {component(u), component(v)};
    }
};

// Per-submission scratch. The source mesh and its authored UVs stay immutable.
// A static material binds its original stream without allocating or copying it.
struct StorylandNativeUvCoordinates {
    std::vector<std::array<float, 2>> coordinates;
    const void* data = nullptr;
    size_t stride = 0;

    void prepare(const float* source, size_t count, size_t sourceStride,
        const std::array<float, 2>& offset) {
        data = source;
        stride = sourceStride;
        if (!source || !count || sourceStride < sizeof(float) * 2u ||
            !std::isfinite(offset[0]) || !std::isfinite(offset[1]) ||
            (offset[0] == 0.0f && offset[1] == 0.0f)) return;
        coordinates.resize(count);
        const auto* bytes = reinterpret_cast<const uint8_t*>(source);
        for (size_t index = 0; index < count; ++index) {
            auto& uv = coordinates[index];
            std::memcpy(uv.data(), bytes + index * sourceStride, sizeof(uv));
            uv[0] += offset[0];
            uv[1] += offset[1];
        }
        data = coordinates.data();
        stride = sizeof(coordinates.front());
    }
};
