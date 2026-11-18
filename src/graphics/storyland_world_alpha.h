#pragma once

enum class StorylandWorldAlphaComparison { Greater, GreaterEqual, Less };

struct StorylandWorldAlphaPass {
    StorylandWorldAlphaComparison comparison = StorylandWorldAlphaComparison::GreaterEqual;
    float reference = 8.0f / 128.0f;
    bool writeDepth = true;
    bool additive = false;
};

inline bool storylandVcsPs2CompiledWorld(int consoleProfile, bool nativeScenery,
    bool nativeShaderActive = true) {
    return consoleProfile == 2 && !nativeScenery && nativeShaderActive;
}

inline unsigned storylandWorldAlphaPassCount(int consoleProfile, unsigned phase,
    bool nativeScenery, bool nativeShaderActive = true) {
    return !nativeScenery && phase == 2 &&
        ((consoleProfile == 2 && nativeShaderActive) || consoleProfile == 3)
        ? 2u : 1u;
}

inline StorylandWorldAlphaPass storylandWorldAlphaPass(int consoleProfile, unsigned phase,
    unsigned pass) {
    StorylandWorldAlphaPass state;
    state.writeDepth = phase == 0 || phase == 2;
    state.additive = phase == 3;
    if (phase == 2) {
        state.writeDepth = pass == 0;
        state.comparison = pass == 0
            ? (consoleProfile == 3 ? StorylandWorldAlphaComparison::Greater
                                   : StorylandWorldAlphaComparison::GreaterEqual)
            : StorylandWorldAlphaComparison::Less;
        // VCS GS AFAIL=FB_ONLY: weak alpha still blends, but does not write Z.
        // GS alpha 128 is opaque; the native pass reference is 120, not 240/255.
        state.reference = consoleProfile == 3 ? 240.0f / 255.0f : 120.0f / 128.0f;
    } else if (consoleProfile == 3) {
        state.reference = 8.0f / 255.0f;
    }
    return state;
}
