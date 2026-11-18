#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

enum class StorylandAudioMode : int {
    PcEnhanced = 0,
    Ps2 = 1,
    Psp = 2
};

inline StorylandAudioMode storylandAudioModeFromInt(int value) {
    switch (value) {
        case 0: return StorylandAudioMode::PcEnhanced;
        case 2: return StorylandAudioMode::Psp;
        default: return StorylandAudioMode::Ps2;
    }
}

inline const char* storylandAudioModeName(StorylandAudioMode mode) {
    switch (mode) {
        case StorylandAudioMode::PcEnhanced: return "PC (Enhanced)";
        case StorylandAudioMode::Ps2: return "PS2";
        case StorylandAudioMode::Psp: return "PSP";
    }
    return "PS2";
}

// These are output profiles. They never change original audio assets, decoded
// playback duration, script timing, sound identifiers, or the underlying samples.
// Native PSP source banks are used when available; this profile cannot create
// missing retail PSP recordings from PS2 samples.
class StorylandAudioProcessor {
public:
    explicit StorylandAudioProcessor(unsigned sampleRate)
        : dcAlpha(coefficient(18.0f, sampleRate)),
          detailAlpha(coefficient(3800.0f, sampleRate)),
          pspAlpha(coefficient(9000.0f, sampleRate)) {}

    void process(float* stereo, std::size_t frames, StorylandAudioMode mode) {
        if (stereo == nullptr || frames == 0 || mode == StorylandAudioMode::Ps2) {
            resetForMode(mode);
            return;
        }
        resetForMode(mode);
        for (std::size_t index = 0; index < frames; ++index) {
            float left = stereo[index * 2];
            float right = stereo[index * 2 + 1];
            processFrame(left, right, mode);
            stereo[index * 2] = std::clamp(left, -1.0f, 1.0f);
            stereo[index * 2 + 1] = std::clamp(right, -1.0f, 1.0f);
        }
    }

    void process(std::int16_t* stereo, std::size_t frames, StorylandAudioMode mode) {
        if (stereo == nullptr || frames == 0 || mode == StorylandAudioMode::Ps2) {
            resetForMode(mode);
            return;
        }
        resetForMode(mode);
        for (std::size_t index = 0; index < frames; ++index) {
            float left = float(stereo[index * 2]) / 32768.0f;
            float right = float(stereo[index * 2 + 1]) / 32768.0f;
            processFrame(left, right, mode);
            stereo[index * 2] = toPcm16(left);
            stereo[index * 2 + 1] = toPcm16(right);
        }
    }

private:
    struct ChannelState {
        float dc = 0;
        float detail = 0;
        float handheld = 0;
    };

    ChannelState channels[2];
    StorylandAudioMode activeMode = StorylandAudioMode::Ps2;
    float dcAlpha;
    float detailAlpha;
    float pspAlpha;

    static float coefficient(float frequency, unsigned sampleRate) {
        const float rate = float(std::max(sampleRate, 8000u));
        return 1.0f - std::exp(-6.28318530717958647692f * frequency / rate);
    }

    static std::int16_t toPcm16(float value) {
        const float scaled = std::clamp(value, -1.0f, 1.0f) * 32768.0f;
        return std::int16_t(std::clamp(std::lround(scaled), -32768l, 32767l));
    }

    void resetForMode(StorylandAudioMode mode) {
        if (activeMode == mode) return;
        activeMode = mode;
        channels[0] = {};
        channels[1] = {};
    }

    void processFrame(float& left, float& right, StorylandAudioMode mode) {
        if (mode == StorylandAudioMode::PcEnhanced) {
            float samples[2] = {left, right};
            for (unsigned channel = 0; channel < 2; ++channel) {
                auto& state = channels[channel];
                state.dc += dcAlpha * (samples[channel] - state.dc);
                const float cleaned = samples[channel] - state.dc * 0.75f;
                state.detail += detailAlpha * (cleaned - state.detail);
                samples[channel] = cleaned + 0.08f * (cleaned - state.detail);
            }
            left = samples[0];
            right = samples[1];
        } else if (mode == StorylandAudioMode::Psp) {
            const float center = (left + right) * 0.5f;
            const float mixed[2] = {
                left * 0.45f + center * 0.55f,
                right * 0.45f + center * 0.55f
            };
            for (unsigned channel = 0; channel < 2; ++channel) {
                auto& state = channels[channel];
                state.handheld += pspAlpha * (mixed[channel] - state.handheld);
            }
            left = channels[0].handheld;
            right = channels[1].handheld;
        }
    }
};
