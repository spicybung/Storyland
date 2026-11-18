#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

class StorylandCutsceneAudioClock {
public:
    void reset(uint64_t position = 0) {
        timelineMilliseconds = position;
        stalledMilliseconds = 0;
        usingFallback = false;
    }

    uint32_t advance(uint32_t wallMilliseconds, bool enabled, bool hasSource,
        bool requested, bool paused, bool failed, bool started, bool finished,
        uint64_t playbackMilliseconds) {
        if (paused) return 0;
        if (!enabled || !hasSource || failed || !requested || usingFallback) {
            timelineMilliseconds += wallMilliseconds;
            return wallMilliseconds;
        }
        if (started && playbackMilliseconds > timelineMilliseconds) {
            const uint64_t difference = playbackMilliseconds - timelineMilliseconds;
            const uint32_t elapsed = uint32_t(std::min<uint64_t>(difference,
                std::numeric_limits<uint32_t>::max()));
            timelineMilliseconds += elapsed;
            stalledMilliseconds = 0;
            return elapsed;
        }
        if (finished) {
            timelineMilliseconds += wallMilliseconds;
            return wallMilliseconds;
        }
        stalledMilliseconds = std::min<uint64_t>(5000, stalledMilliseconds + wallMilliseconds);
        if (stalledMilliseconds >= 5000) {
            usingFallback = true;
            timelineMilliseconds += wallMilliseconds;
            return wallMilliseconds;
        }
        return 0;
    }

    bool fallbackActive() const { return usingFallback; }

private:
    uint64_t timelineMilliseconds = 0;
    uint64_t stalledMilliseconds = 0;
    bool usingFallback = false;
};
