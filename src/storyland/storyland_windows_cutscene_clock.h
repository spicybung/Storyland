#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

// A scene clock follows the voice cursor while it advances. A decoder or
// audio device that never advances cannot hold the mission script forever.
class StorylandWindowsCutsceneClock {
public:
    uint32_t advance(uint32_t wall, bool hasTicket, bool paused, bool failed,
        bool cancelled, bool started, bool finished, bool positionValid,
        uint32_t position) {
        if (paused) return 0;
        if (!hasTicket || failed || cancelled || fallback) {
            timeline += wall;
            return wall;
        }
        if (started && positionValid && position > timeline) {
            const auto elapsed = uint32_t(std::min<uint64_t>(
                uint64_t(position) - timeline, std::numeric_limits<uint32_t>::max()));
            timeline += elapsed;
            stalled = 0;
            return elapsed;
        }
        if (finished) {
            timeline += wall;
            return wall;
        }
        stalled = std::min<uint64_t>(5000, stalled + wall);
        if (stalled >= 5000) {
            fallback = true;
            timeline += wall;
            return wall;
        }
        return 0;
    }
    bool fallbackActive() const { return fallback; }
private:
    uint64_t timeline = 0;
    uint64_t stalled = 0;
    bool fallback = false;
};
