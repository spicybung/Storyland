#pragma once

#include "src/storyland/storyland_water.h"
#include "src/storyland/storyland_weapon_attachment.h"

inline void storylandCaptureSwimmingLimbs(StorylandCharacterWaterState& water,
    const StorylandModelFile& model,const std::vector<std::array<float,16>>& frames,
    const std::array<float,16>& worldFrame) {
    auto& state=water.particles;
    state.limbSampleMask=0;
    if(!water.active||water.diving||water.underwater)return;
    // SLUS-21590 0x1F4078 uses CPed nodes 5,6,9,10. Native 0x3A8270
    // maps those nodes to these hand/foot skin identifiers.
    constexpr std::array<uint32_t,4> bones{34,24,43,53};
    for(unsigned index=0;index<bones.size();++index) {
        StorylandNativePoint point;
        if(!storylandPoseBoneWorldPoint(model,frames,bones[index],worldFrame,point))continue;
        state.limbPositions[index]={point.x,point.y,point.z};
        state.limbSampleMask|=uint8_t(1u<<index);
    }
}
