#pragma once

#include <algorithm>
#include <cmath>

enum class StorylandPlayFrameMode { Fps30, Fps60, Fps60Hz120 };

inline double storylandSimulationFrameSeconds(StorylandPlayFrameMode mode) {
    return mode == StorylandPlayFrameMode::Fps30 ? 1.0 / 30.0 : 1.0 / 60.0;
}

inline double storylandPresentationFrameSeconds(StorylandPlayFrameMode mode) {
    return mode == StorylandPlayFrameMode::Fps60Hz120 ? 1.0 / 120.0 : storylandSimulationFrameSeconds(mode);
}

inline int storylandPresentationSwapInterval(StorylandPlayFrameMode mode, double refreshRate) {
    if (!std::isfinite(refreshRate) || refreshRate < 1) return 0;
    const double ratio = refreshRate * storylandPresentationFrameSeconds(mode);
    if (ratio < 1) return 1;
    const double interval = std::round(ratio);
    if (interval > 4 || std::fabs(ratio - interval) > .015) return 0;
    return int(interval);
}

inline double storylandNextPresentationTime(double previous, double completed,
    double period, bool synchronized) {
    return synchronized ? completed : std::max(previous + period, completed);
}

inline bool storylandSimulationFrameDue(double& deadline, double now, double period,
    double previous = 0) {
    if (!std::isfinite(now) || !std::isfinite(period) || period <= 0) return false;
    if (previous > 0 && now - previous < .01 - 1e-8) return false;
    if (deadline <= 0 || !std::isfinite(deadline)) {
        deadline = now + period;
        return true;
    }
    const double tolerance = period * .006;
    if (now + tolerance < deadline) return false;
    const double elapsed = std::max(0.0, now + tolerance - deadline);
    deadline += (std::floor(elapsed / period) + 1.0) * period;
    return true;
}

struct StorylandSimulationCadence {
    double phase = 0, elapsed = 0;

    void reset() { phase = elapsed = 0; }

    double advance(double seconds, StorylandPlayFrameMode mode) {
        if (!std::isfinite(seconds) || seconds <= 0) return 0;
        const double step = std::min(seconds, .1);
        phase += step;
        elapsed += step;
        const double period = storylandSimulationFrameSeconds(mode);
        if (phase + 1e-8 < period || elapsed + 1e-8 < .01) return 0;
        phase = std::max(0.0, phase - std::floor((phase + 1e-8) / period) * period);
        const double result = elapsed;
        elapsed = 0;
        return result;
    }
};
