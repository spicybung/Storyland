#pragma once
#include "src/storyland/storyland_play_rubbish.h"
#include <array>
#include <cmath>

// A small low-poly branch is represented by narrow wooden strips, not by a
// substituted native vegetation model or unrelated texture from GAME.DTZ.
struct StorylandRubbishBranchVertex {
    float x=0,y=0,z=0;
    float r=0.24f,g=0.16f,b=0.085f;
};

inline std::array<StorylandRubbishBranchVertex,18> storylandRubbishBranchMesh(
    const StorylandRubbishSheet& sheet) {
    std::array<StorylandRubbishBranchVertex,18> result{};
    unsigned position=0;
    const auto limb=[&](float startX,float startY,float endX,float endY,float width,
                        float red,float green,float blue) {
        const float deltaX=endX-startX,deltaY=endY-startY;
        const float length=std::hypot(deltaX,deltaY);
        const float perpX=length>1e-6f?-deltaY/length*width:0.0f;
        const float perpY=length>1e-6f?deltaX/length*width:width;
        const auto vertex=[&](float x,float y,float z) {
            result[position++]={x,y,z,red,green,blue};
        };
        vertex(startX-perpX,startY-perpY,.002f);
        vertex(startX+perpX,startY+perpY,.002f);
        vertex(endX+perpX*.60f,endY+perpY*.60f,.008f);
        vertex(startX-perpX,startY-perpY,.002f);
        vertex(endX+perpX*.60f,endY+perpY*.60f,.008f);
        vertex(endX-perpX*.60f,endY-perpY*.60f,.008f);
    };
    const float length=sheet.width;
    limb(-length,0.0f,length,0.0f,.027f,.25f,.17f,.087f);
    limb(-length*.26f,0.0f,length*.10f,sheet.height*.95f,.014f,.30f,.20f,.10f);
    limb(length*.25f,0.0f,length*.54f,-sheet.height*.78f,.011f,.20f,.13f,.072f);
    return result;
}
