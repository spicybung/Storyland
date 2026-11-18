#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

struct StorylandWaterCannonSample {
    std::array<float,3> position{};
    std::array<float,3> nativeVelocity{};
    bool active=false;

    template<class Archive> void saveState(Archive& archive) {
        archive(position,nativeVelocity,active);
    }
};

struct StorylandWaterCannon {
    int32_t ownerSlot=-1,ownerScriptHandle=-1;
    uint8_t current=0;
    uint32_t startedAt=0;
    std::array<StorylandWaterCannonSample,16> samples{};

    template<class Archive> void saveState(Archive& archive) {
        archive(ownerSlot,ownerScriptHandle,current,startedAt,samples);
    }
};
