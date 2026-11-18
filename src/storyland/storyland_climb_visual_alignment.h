#pragma once

#include "src/storyland/storyland_extended_movement.h"
#include "src/storyland/storyland_weapon_attachment.h"
#include <algorithm>
#include <array>
#include <cmath>

// Keep the physics capsule outside the wall while reducing the visual gap
// between the authored hanging pose and the ledge. The correction is bounded
// because the original SA clip and Leeds player skeleton have different binds.
inline bool storylandAlignClimbVisual(const StorylandModelFile& model,
    const StorylandExtendedMovement& climb,const StorylandNativePoint& position,float heading,
    std::vector<StorylandModelPoint>& points,std::vector<std::array<float,16>>& frames) {
    float weight=0;
    if(climb.action==StorylandExtendedAction::ClimbIdle)weight=1;
    else if(climb.action==StorylandExtendedAction::ClimbJump)
        weight=std::clamp((climb.elapsed-.20f)/.35f,0.0f,1.0f);
    else if(climb.action==StorylandExtendedAction::ClimbPull)
        weight=std::clamp(1.0f-climb.elapsed/.22f,0.0f,1.0f);
    if(weight<=0)return true;
    std::array<float,16> frame{};
    StorylandNativePoint right{},left{};
    if(!storylandStandingPedWorldFrame(model,position,heading,1.8f,frame)||
        !storylandPoseBoneWorldPoint(model,frames,24,frame,right)||
        !storylandPoseBoneWorldPoint(model,frames,34,frame,left))return false;
    const float radians=heading*.0174532925199433f;
    const float x=std::cos(radians),y=std::sin(radians);
    const float midX=(right.x+left.x)*.5f,midY=(right.y+left.y)*.5f,midZ=(right.z+left.z)*.5f;
    const float correction=std::clamp((climb.climbLedge.x-midX)*x+(climb.climbLedge.y-midY)*y,-.12f,.12f)*weight;
    const float height=std::clamp(climb.climbLedge.z-midZ,-.05f,.05f)*weight;
    const float scaleSquared=frame[0]*frame[0]+frame[1]*frame[1]+frame[2]*frame[2];
    if(!std::isfinite(correction)||!std::isfinite(height)||scaleSquared<1e-10f)return false;
    const std::array<float,3> worldDelta{x*correction,y*correction,height};
    const std::array<float,3> local{
        (frame[0]*worldDelta[0]+frame[1]*worldDelta[1]+frame[2]*worldDelta[2])/scaleSquared,
        (frame[4]*worldDelta[0]+frame[5]*worldDelta[1]+frame[6]*worldDelta[2])/scaleSquared,
        (frame[8]*worldDelta[0]+frame[9]*worldDelta[1]+frame[10]*worldDelta[2])/scaleSquared};
    for(auto& point:points) {point.x+=local[0];point.y+=local[1];point.z+=local[2];}
    for(auto& bone:frames) {bone[12]+=local[0];bone[13]+=local[1];bone[14]+=local[2];}
    return true;
}
