#pragma once
#include <array>
#include <cstdint>

struct StorylandVcsFerrisWheelState {
    bool configured=false,wheelFound=false,cabinsResident=false,modelRequested=false;
    uint32_t wheelModel=UINT32_MAX,cabinModel=UINT32_MAX;
    uint64_t worldKey=0;
    std::array<float,16> originalMatrix{},wheelMatrix{};
    float speed=0,angle=0,remainingStopAngle=0;
    uint8_t state=0;
    bool stopRequested=false;
    std::array<int32_t,12> cabins{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
    std::array<float,12> swingSpeed{},swingAngle{};

    template<class Archive> void saveState(Archive& archive) {
        archive(configured,wheelFound,cabinsResident,modelRequested,wheelModel,cabinModel,
            worldKey,originalMatrix,wheelMatrix,speed,angle,remainingStopAngle,state,
            stopRequested,cabins,swingSpeed,swingAngle);
    }
};
