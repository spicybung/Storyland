#pragma once

#include <array>
#include <cstdint>

inline bool storylandVcsAudioEventAdmitted(bool managerEnabled,bool serviceSuppressed) {
    return managerEnabled&&!serviceSuppressed;
}

// The original audio manager owns five shared random words. The selector and
// polarity are process globals (SLUS 0x487FD4 and 0x487FD0). Original game
// save/load treatment of those globals is not yet verified.
struct StorylandVcsAudioRandom {
    std::array<uint32_t, 5> words{};
    uint32_t selector=0;
    uint32_t previousFrontendRadioSample=236;
    bool positive=true;
    bool initialized=false;

    template<class Draw> void refresh(Draw draw) {
        for(auto& word:words)word=draw();
    }

    void initialize() {
        if(initialized)return;
        // The audio constructor runs before the game's timer-based reseed.
        // Its initial words must not advance the later gameplay RNG stream.
        uint32_t seed=180371;
        for(auto& word:words) {
            seed=uint32_t((uint64_t(seed)*16807u)%2147483647u);
            word=seed;
        }
        initialized=true;
    }

    template<class Draw> void service(Draw draw,bool suppressed) {
        if(suppressed)return;
        initialize();
        refresh(draw);
    }

    int32_t displacement(uint32_t magnitude) {
        if(!magnitude)return 0;
        const uint32_t index=(magnitude+selector)%5;
        const uint32_t offset=words[index]%magnitude;
        selector+=offset;
        if(offset&1u)positive=!positive;
        return positive?int32_t(offset):-int32_t(offset);
    }
};
