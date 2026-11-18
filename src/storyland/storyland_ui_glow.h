#pragma once

#include "imgui.h"
#include <array>
#include <algorithm>

// A restrained host-side UI treatment, not a recovered retail post-effect.
// It only emits additional geometry; it does not affect layout or input.
inline ImU32 storylandUiGlowColor(ImU32 color,unsigned alpha) {
    const unsigned sourceAlpha=(color&IM_COL32_A_MASK)>>IM_COL32_A_SHIFT;
    const unsigned combined=std::min(255u,alpha)*sourceAlpha/255u;
    return (color&~ImU32(IM_COL32_A_MASK))|(ImU32(combined)<<IM_COL32_A_SHIFT);
}

template<class Draw>
inline void storylandDrawUiGlow(ImVec2 position,ImVec2 radius,ImU32 color,Draw draw) {
    const ImU32 tint=storylandUiGlowColor(color,22);
    if((tint&IM_COL32_A_MASK)==0)return;
    const std::array<ImVec2,4> offsets{
        ImVec2(-radius.x,0),ImVec2(radius.x,0),ImVec2(0,-radius.y),ImVec2(0,radius.y)};
    for(const auto& offset:offsets)
        draw(ImVec2(position.x+offset.x,position.y+offset.y),tint);
}
