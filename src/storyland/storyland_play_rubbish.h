#pragma once

#include "src/leeds/leeds_native_world.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

// Retail-inspired ground litter: native textured paper/leaves and low-poly twigs, with wind
// hops, rotation and wakes from passing vehicles. The original Rubbish code
// in GTA III / Vice City is not embedded or required by this implementation.
// Native LCS/VCS texture names are resolved separately by each platform renderer.
enum class StorylandRubbishKind : uint8_t {
    NewspaperA,
    NewspaperB,
    LeafA,
    LeafB,
    Branch
};

struct StorylandRubbishVehicle {
    StorylandNativePoint position{};
    float direction=0.0f;
    float speed=0.0f;
    float halfWidth=0.85f,halfLength=1.7f;
};

struct StorylandRubbishSheet {
    StorylandRubbishKind kind=StorylandRubbishKind::NewspaperA;
    StorylandNativePoint origin{},destination{},position{};
    float width=0.35f,height=0.24f;
    float yaw=0.0f,roll=0.0f,spin=0.0f;
    float hopTime=0.0f,hopDuration=0.0f,hopHeight=0.0f;
    float cooldown=0.0f;
    float opacity=0.0f;
    bool flying=false;
};

class StorylandPlayRubbish {
public:
    using GroundHeight=std::function<bool(const StorylandNativePoint&,float&)>;
    static constexpr size_t MaximumSheets=48;

    bool enabled=true;
    void reset();
    void update(const StorylandNativePoint& focus,const StorylandNativePoint& wind,
                const std::vector<StorylandRubbishVehicle>& vehicles,float seconds,
                const GroundHeight& ground);
    const std::vector<StorylandRubbishSheet>& sheets() const {return litter;}
    size_t movingCount() const;

private:
    uint32_t randomState=0xA2F9B413u;
    float spawnAccumulator=0.0f;
    std::vector<StorylandRubbishSheet> litter;
    float random01();
    bool spawn(const StorylandNativePoint& focus,const GroundHeight& ground);
    void beginHop(StorylandRubbishSheet& sheet,float distanceX,float distanceY,
                  float height,float duration);
};

const char* storylandRubbishTextureName(StorylandRubbishKind kind);
