#pragma once

#include <cstdint>

namespace StorylandVcsBlood {
// SLUS_215.90: corpse shadow branch 0x1F508C..0x1F5218.
constexpr float CorpseDelay=2.0f;
constexpr float CorpseGrowthSeconds=5.0f;
constexpr float CorpseRadius=.7500000596046448f;
constexpr float CorpseGrowthPerSecond=.1500000071246177f;
constexpr float CorpseRestSeconds=40.0f;

// CSkidmarks::RegisterOne 0x407C44..0x407E20 retains nine blood samples.
constexpr float SkidSampleSeconds=.101f;
constexpr unsigned SkidBloodSamples=9;
constexpr float SkidRestSeconds=10.0f;
constexpr float SkidFadeSeconds=10.0f;
constexpr float SkidRed=60.0f/255.0f,SkidGreen=15.0f/255.0f,SkidBlue=4.0f/255.0f;
constexpr float SkidAlpha=51.0f/255.0f;

inline bool groundSplat(uint32_t sequence,uint32_t random,float& radius,float& seconds) {
    // CParticle ground-blood path 0x1EA7C0..0x1EA890.
    switch(sequence&7u) {
    case 1:radius=.1f;seconds=float(2000u+(random&4095u))*.001f;return true;
    case 2:radius=.2f;seconds=float(8000u+(random&4095u))*.001f;return true;
    default:return false;
    }
}
}
