#pragma once

#include <array>
#include <chrono>
#include <cstddef>

struct StorylandRenderPerformance {
    enum Stage : size_t {
        Prepare, Selection, Lights, Reflections, ShadowMap, World, Water,
        Actors, ProjectedShadows, CoronasMarkers, Hud, Particles, PostEffects,
        ScreenEffects, Count
    };
    inline static constexpr std::array<const char*, Count> names{{
        "prepare", "selection", "lights", "reflections", "shadow_map", "world",
        "water", "actors", "projected_shadows", "coronas_markers", "hud",
        "particles", "post_effects", "screen_effects"
    }};
    bool enabled = false;
    std::array<double, Count> milliseconds{};
    size_t placements = 0;
    size_t actors = 0;
};

class StorylandRenderPerformanceFrame {
public:
    explicit StorylandRenderPerformanceFrame(StorylandRenderPerformance& output)
        : report(output), enabled(output.enabled) {
        if (enabled) {
            report.milliseconds.fill(0);
            report.placements = report.actors = 0;
            previous = Clock::now();
        }
    }
    ~StorylandRenderPerformanceFrame() { next(stage); }
    void next(StorylandRenderPerformance::Stage nextStage) {
        if (!enabled) return;
        const auto now = Clock::now();
        report.milliseconds[stage] +=
            std::chrono::duration<double, std::milli>(now - previous).count();
        previous = now;
        stage = nextStage;
    }

private:
    using Clock = std::chrono::steady_clock;
    StorylandRenderPerformance& report;
    bool enabled;
    StorylandRenderPerformance::Stage stage = StorylandRenderPerformance::Prepare;
    Clock::time_point previous{};
};
