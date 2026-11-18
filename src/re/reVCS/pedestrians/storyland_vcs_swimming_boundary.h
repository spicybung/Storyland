#pragma once
#include <algorithm>
#include <array>
#include <cmath>

struct StorylandVcsSwimmingBoundary {
    float strength = 0;
    float waveStrength = 0;
    float velocity = 0;

    void setEnabled(bool enabled) {
        strength = enabled ? 0.0f : -1.0f;
    }

    float advance(float x, float y, bool inWater) {
        if (strength <= -1.0f) {
            strength = -1.0f;
            waveStrength = 0;
            velocity = 0;
            return 0;
        }
        // SLUS-21590 0x1AAA90, 0x1AAB38 and globals 0x48651C..0x48652C.
        constexpr std::array<std::array<float,2>,7> points{{
            {-500, -2000}, {-500, -1010}, {-108, -600}, {-108, -300},
            {-380, 380}, {-70, 1515}, {-70, 2000}
        }};
        float force = 0;
        if (!inWater) {
            strength = 0;
            velocity = 0;
        } else {
            for (size_t index = 0; index + 1 < points.size(); ++index) {
                const auto& begin = points[index];
                const auto& end = points[index + 1];
                if (y < begin[1] || y > end[1]) continue;
                const float fraction = (y - begin[1]) / (end[1] - begin[1]);
                const float boundary = begin[0] + fraction * (end[0] - begin[0]);
                if (x <= boundary - 40.0f) continue;
                strength = std::min(1.0f, (x - boundary + 40.0f) / 40.0f);
                force -= 8.0f * strength;
            }
        }
        waveStrength += (strength - waveStrength) * 0.1f;
        return force;
    }

    float waterWind(float weatherWind) const {
        return std::clamp(weatherWind + waveStrength, 0.0f, 1.0f);
    }

    template<class Archive> void saveState(Archive& archive) {
        archive(strength, waveStrength, velocity);
    }
};
