#pragma once

#include <chrono>
#include <cstdint>
#include <string>

struct StorylandFrameTraceStage {
    double milliseconds = 0.0;
    std::uint32_t calls = 0;
};

struct StorylandFrameTraceStages {
    bool graphicsAvailable = false;
    std::string backend, glVendor, glRenderer, glVersion, shaderMode;
    int consoleProfile = -1, renderMode = -1;
    bool aeroEnabled = false;
    double frameBudgetMilliseconds = 0.0;
    int viewportWidth = 0, viewportHeight = 0;
    std::uint64_t worldInstances = 0, worldTriangles = 0, worldTexturedTriangles = 0;
    std::uint64_t worldDrawCommands = 0;
    StorylandFrameTraceStage worldDraw;
    StorylandFrameTraceStage actors;
    StorylandFrameTraceStage actorShadows;
    StorylandFrameTraceStage scriptShadows;
    StorylandFrameTraceStage scriptCoronas;
    StorylandFrameTraceStage environmentComposition;
    StorylandFrameTraceStage nativeProps;
    StorylandFrameTraceStage nativeOwnerBuild;
    StorylandFrameTraceStage nativeSelection;
    StorylandFrameTraceStage nativeStreamIndex;
    StorylandFrameTraceStage nativeRepresentation;
    StorylandFrameTraceStage nativeModelResolve;
    StorylandFrameTraceStage nativeCompiledEnsure;
    StorylandFrameTraceStage nativeActorDraw;
    StorylandFrameTraceStage nativeCompiledDraw;
    std::uint64_t nativeCandidates = 0, nativeModels = 0;
    std::uint64_t nativeRepresentationChecks = 0, nativeRepresented = 0;
    std::uint64_t nativeMissingSkips = 0, nativeAssetHits = 0, nativeAssetAttempts = 0;
    std::uint64_t nativeNameFailures = 0, nativeLoadAttempts = 0, nativeLoadFailures = 0;
    std::uint64_t nativeEnsureAttempts = 0, nativeEnsureKnown = 0, nativeEnsureFailures = 0;
    std::uint64_t nativeActorDrawn = 0, nativeCompiledQueued = 0;
    StorylandFrameTraceStage present;
    StorylandFrameTraceStage traceIo;
};

template<class Clock = std::chrono::steady_clock>
class StorylandFrameTraceScope {
public:
    StorylandFrameTraceScope(bool enabled, StorylandFrameTraceStage& stage)
        : enabled_(enabled), stage_(stage) {
        if (enabled_) {
            ++stage_.calls;
            start_ = Clock::now();
        }
    }

    ~StorylandFrameTraceScope() {
        if (enabled_)
            stage_.milliseconds += std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
    }

    StorylandFrameTraceScope(const StorylandFrameTraceScope&) = delete;
    StorylandFrameTraceScope& operator=(const StorylandFrameTraceScope&) = delete;

private:
    bool enabled_;
    StorylandFrameTraceStage& stage_;
    typename Clock::time_point start_{};
};
