#pragma once
#include <array>
#include <cstdint>
struct StorylandRoadChange {
    enum class Mode : uint8_t { Availability=0,LevelTransitions=1,AngledAvailability=2 };
    std::array<float,6> bounds{};
    bool vehicle=false,enabled=false;
    Mode mode=Mode::Availability; // LC17 extension; keep the legacy record unchanged.
    float width=0; // LC17 oriented road area width; bounds hold its directed baseline.
    bool angled=false;
};
