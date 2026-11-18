#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include "src/leeds/leeds_ctw_timecycle.h"

struct StorylandCtwColour { float r, g, b; };
struct StorylandCtwEnvironment {
    StorylandCtwColour skyTop, skyBottom, ambient, sun, fog;
    StorylandCtwColour specular{1, 1, 1};
    bool nativePsp = false;
    float fogStartFraction = 0, fogEndFraction = 1;
    float sunX = 0, sunY = 0, sunZ = 1;
    float rain = 0, clouds = 0, fogAmount = 0, lightning = 0;
    float wetness = 0, wind = 0, daylight = 1;
};

inline StorylandCtwColour storylandCtwBlend(StorylandCtwColour a, StorylandCtwColour b, float weight) {
    return {a.r + (b.r - a.r) * weight, a.g + (b.g - a.g) * weight, a.b + (b.b - a.b) * weight};
}

inline StorylandCtwEnvironment storylandCtwEnvironment(float hour, int weather, float seconds) {
    if (!std::isfinite(hour)) hour = 12;
    if (!std::isfinite(seconds)) seconds = 0;
    hour = std::fmod(std::fmod(hour, 24.0f) + 24.0f, 24.0f);
    weather = std::clamp(weather, 0, 4);
    const float angle = (hour - 6.0f) * 6.28318530718f / 24.0f;
    const float daylight = std::clamp(std::sin(angle) * 2.0f + 0.15f, 0.0f, 1.0f);
    const float twilight = std::clamp(1.0f - std::fabs(std::sin(angle)) * 5.0f, 0.0f, 1.0f);
    StorylandCtwEnvironment state;
    state.daylight = daylight;
    state.sunX = std::cos(angle) * 0.8f;
    state.sunY = 0.35f;
    state.sunZ = std::sin(angle);
    state.clouds = weather == 0 ? 0.12f : weather == 1 ? 0.65f : 0.9f;
    state.rain = weather == 2 ? 0.65f : weather == 4 ? 1.0f : 0.0f;
    state.fogAmount = weather == 3 ? 0.92f : 0.12f + state.rain * 0.48f;
    state.wetness = state.rain;
    state.wind = weather == 4 ? 1.0f : weather == 2 ? 0.6f : 0.2f;
    state.skyTop = storylandCtwBlend({0.025f, 0.035f, 0.085f}, {0.18f, 0.40f, 0.64f}, daylight);
    state.skyBottom = storylandCtwBlend({0.075f, 0.085f, 0.15f}, {0.65f, 0.75f, 0.81f}, daylight);
    state.skyBottom = storylandCtwBlend(state.skyBottom, {0.72f, 0.36f, 0.24f}, twilight * 0.6f);
    const auto grey = storylandCtwBlend({0.055f, 0.065f, 0.095f}, {0.39f, 0.43f, 0.47f}, daylight);
    state.skyTop = storylandCtwBlend(state.skyTop, grey, state.clouds * 0.7f);
    state.skyBottom = storylandCtwBlend(state.skyBottom, grey, state.clouds * 0.7f);
    state.ambient = storylandCtwBlend({0.22f, 0.25f, 0.34f}, {0.66f, 0.66f, 0.65f}, daylight);
    state.sun = storylandCtwBlend({0.08f, 0.10f, 0.16f}, {0.43f, 0.40f, 0.34f}, daylight);
    const float attenuation = 1.0f - state.clouds * 0.65f;
    state.sun = {state.sun.r * attenuation, state.sun.g * attenuation, state.sun.b * attenuation};
    if (weather == 4) {
        const float phase = std::fmod(std::max(0.0f, seconds), 11.0f);
        state.lightning = phase < 0.12f || (phase > 0.22f && phase < 0.28f) ? 0.55f : 0.0f;
    }
    state.skyTop = storylandCtwBlend(state.skyTop, {0.85f, 0.88f, 1.0f}, state.lightning);
    state.skyBottom = storylandCtwBlend(state.skyBottom, {0.85f, 0.88f, 1.0f}, state.lightning);
    state.fog = state.skyBottom;
    return state;
}


inline StorylandCtwEnvironment storylandCtwBlendEnvironment(
    const StorylandCtwEnvironment& first, const StorylandCtwEnvironment& second, float weight) {
    weight = std::clamp(weight, 0.0f, 1.0f);
    StorylandCtwEnvironment result;
    result.skyTop = storylandCtwBlend(first.skyTop, second.skyTop, weight);
    result.skyBottom = storylandCtwBlend(first.skyBottom, second.skyBottom, weight);
    result.ambient = storylandCtwBlend(first.ambient, second.ambient, weight);
    result.sun = storylandCtwBlend(first.sun, second.sun, weight);
    result.fog = storylandCtwBlend(first.fog, second.fog, weight);
    const auto blend = [weight](float a, float b) { return a + (b - a) * weight; };
    result.sunX = blend(first.sunX, second.sunX);
    result.sunY = blend(first.sunY, second.sunY);
    result.sunZ = blend(first.sunZ, second.sunZ);
    result.rain = blend(first.rain, second.rain);
    result.clouds = blend(first.clouds, second.clouds);
    result.fogAmount = blend(first.fogAmount, second.fogAmount);
    result.lightning = blend(first.lightning, second.lightning);
    result.wetness = blend(first.wetness, second.wetness);
    result.wind = blend(first.wind, second.wind);
    result.daylight = blend(first.daylight, second.daylight);
    return result;
}

class StorylandCtwWeatherController {
public:
    void reset() { initialized = false; }

    StorylandCtwEnvironment update(float hour, int weather, double seconds, bool immediate) {
        if (!std::isfinite(seconds)) seconds = initialized ? previousSeconds : 0;
        const float elapsed = initialized ? float(std::clamp(seconds - previousSeconds, 0.0, 0.25)) : 0;
        previousSeconds = seconds;
        const auto target = storylandCtwEnvironment(hour, weather, float(std::fmod(std::max(0.0, seconds), 3600.0)));
        if (!initialized || immediate) {
            weights.fill(0);
            weights[size_t(std::clamp(weather, 0, 4))] = 1;
            wetness = target.rain;
            initialized = true;
        }
        const float blend = 1.0f - std::exp(-elapsed / 3.0f);
        for (size_t index = 0; index < weights.size(); ++index)
            weights[index] += ((int(index) == std::clamp(weather, 0, 4) ? 1.0f : 0.0f) - weights[index]) * blend;
        StorylandCtwEnvironment result = storylandCtwEnvironment(hour, 0, float(std::fmod(std::max(0.0, seconds), 3600.0)));
        float total = weights[0];
        for (size_t index = 1; index < weights.size(); ++index) {
            const float next = total + weights[index];
            if (next > 0) result = storylandCtwBlendEnvironment(result,
                storylandCtwEnvironment(hour, int(index), float(std::fmod(std::max(0.0, seconds), 3600.0))), weights[index] / next);
            total = next;
        }
        const float dryingTime = result.rain > wetness ? 5.0f : 45.0f;
        wetness += (result.rain - wetness) * (1.0f - std::exp(-elapsed / dryingTime));
        result.wetness = wetness;
        return result;
    }

private:
    bool initialized = false;
    double previousSeconds = 0;
    float wetness = 0;
    std::array<float, 5> weights{};
};

// Verified PSP renderer bindings: 0x089f02ec writes GE diffuse/ambient/specular;
// 0x089efef4 derives the directional light; 0x089efb50 clears the background;
// 0x088d2310 sets GE fog. Indices here are FILE channels, not runtime slots:
// native fog slots 19..21 map to file 20..22, and slots 26/27 to file 23/24.
// Unmapped sky/cloud/rain channels are deliberately not assigned guessed meanings.
inline StorylandCtwEnvironment storylandCtwNativeEnvironment(
    const storyland_ctw::PspTimecycle& cycle, size_t profile, float hour) {
    StorylandCtwEnvironment state{};
    if (!cycle.ready() || profile >= cycle.WeatherCount) return state;
    state.nativePsp = true;
    const auto rgb = [&](size_t channel) {
        return StorylandCtwColour{cycle.sample(profile, channel, hour) / 255.0f,
            cycle.sample(profile, channel + 1, hour) / 255.0f,
            cycle.sample(profile, channel + 2, hour) / 255.0f};
    };
    state.sun = rgb(0); state.ambient = rgb(13); state.specular = rgb(16);
    state.fog = rgb(20); state.skyTop = state.skyBottom = rgb(29);
    const float azimuth = cycle.sample(profile, 3, hour) * (6.28318530718f / 256.0f);
    const float elevation = (cycle.sample(profile, 4, hour) - 64.0f) * (6.28318530718f / 256.0f);
    state.sunX = -std::sin(azimuth) * std::cos(elevation);
    state.sunY = -std::cos(azimuth) * std::cos(elevation);
    state.sunZ = -std::sin(elevation);
    const float start = cycle.sample(profile, 23, hour) / 255.0f;
    const float end = cycle.sample(profile, 24, hour) / 255.0f;
    state.fogAmount = start != end ? 1.0f : 0.0f;
    state.fogStartFraction = std::min(start, 0.98f);
    state.fogEndFraction = std::min(std::max(start, end), 1.0f);
    return state;
}
