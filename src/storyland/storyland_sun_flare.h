#pragma once

#include "src/storyland/storyland_sky.h"
#include "src/storyland/storyland_script_corona.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

inline std::array<StorylandVcsSunRegistration,2> storylandVcsSunRegistrations(
    const std::array<float,3>& direction,const std::array<float,3>& camera,
    const std::array<uint8_t,3>& core,const std::array<uint8_t,3>& corona,
    float rawSunSize,float spriteSize,bool blocked,bool suppressed,bool sceneSuppressed,size_t& count) {
    std::array<StorylandVcsSunRegistration,2> result{};
    count=0;
    if(blocked||sceneSuppressed||direction[2]<=-.2f||!std::isfinite(rawSunSize)||
        !std::isfinite(spriteSize))return result;
    for(size_t axis=0;axis<3;++axis)
        if(!std::isfinite(direction[axis])||!std::isfinite(camera[axis]))return result;
    for(size_t index=0;index<2;++index) {
        if(index==1&&(direction[2]<=0||suppressed))break;
        auto& registration=result[count++];
        registration.id=unsigned(index+1);
        for(size_t axis=0;axis<3;++axis)
            registration.position[axis]=camera[axis]+direction[axis]*150.0f;
        registration.color=index?corona:core;
        registration.registrationSize=rawSunSize*(index?20.0f:10.64f);
        registration.size=registration.registrationSize>0?
            registration.registrationSize*spriteSize:-registration.registrationSize;
        registration.flare=uint8_t(index);
        registration.checkObstacles=index!=0;
    }
    return result;
}

struct StorylandVcsSunSprite {
    float x=0,y=0,depth=0,halfWidth=0,halfHeight=0,rotation=0;
    std::array<float,3> color{};
    unsigned texture=0;
    bool depthTest=false;
};

inline std::array<StorylandVcsSunSprite,9> storylandVcsSunSprites(
    const std::array<StorylandVcsSunRegistration,2>& registrations,size_t registrationCount,
    const std::array<uint8_t,2>& alpha,const float* view,const float* projection,size_t& count,float fogginess=0) {
    std::array<StorylandVcsSunSprite,9> result{};
    count=0;
    if(!view||!projection||std::fabs(projection[5])<1.0e-6f)return result;
    const float aspectScale=std::fabs(projection[0]/projection[5]);
    for(size_t index=0;index<std::min(registrationCount,size_t(2));++index) {
        const auto& registration=registrations[index];
        StorylandCoronaProjection projected;
        if(!alpha[index]||!storylandProjectCorona(registration.position,view,projection,projected,
            registration.range,registration.pullAway))continue;
        const float x=projection[0]*projected.cameraX/projected.depth-projection[8];
        const float y=projection[5]*projected.cameraY/projected.depth-projection[9];
        auto& sprite=result[count++];
        sprite.x=x;sprite.y=y;sprite.texture=registration.texture;
        sprite.depthTest=!registration.checkObstacles;
        float drawDepth=projected.biasedDepth;
        if(registration.id==1) {
            const float farDenominator=projection[10]+1.0f;
            if(std::fabs(farDenominator)>1.0e-8f)
                drawDepth=projection[14]/farDenominator*.95f-registration.pullAway;
        }
        sprite.depth=(-projection[10]*drawDepth+projection[14])/drawDepth;
        const float fogScale=storylandCoronaFogScale(projected.depth,fogginess);
        sprite.halfHeight=2*registration.size*fogScale/projected.depth;
        sprite.halfWidth=sprite.halfHeight*aspectScale;
        sprite.rotation=20.0f/projected.depth;
        sprite.color=storylandCoronaSpriteColor(storylandCoronaFogColor(registration.color,fogScale),
            alpha[index],drawDepth);
        if(!registration.flare)continue;
        size_t flareCount=0;
        const auto& flares=storylandCoronaFlares(registration.flare,flareCount);
        for(size_t ghostIndex=0;ghostIndex<flareCount&&count<result.size();++ghostIndex) {
            const auto& flare=flares[ghostIndex];
            auto& ghost=result[count++];
            ghost.x=x*flare.position;ghost.y=y*flare.position;
            ghost.depth=sprite.depth;ghost.texture=unsigned(flare.texture);
            ghost.halfHeight=8*flare.size/448.0f;
            ghost.halfWidth=ghost.halfHeight*aspectScale;
            ghost.color=storylandCoronaFlareColor(registration.color,alpha[index],flare);
        }
    }
    return result;
}

struct StorylandSunFlareState {
    bool visible=false;
    float x=0.0f,y=0.0f;
    float intensity=0.0f;
    std::array<float,3> color{};
};

// The Stories sun is a world-space directional light. A translation in the
// view matrix must not change where the sun appears on the screen.
inline StorylandSunFlareState storylandSunFlareAt(const StorylandSkyState& sky,
    const float* view,const float* projection) {
    StorylandSunFlareState result;
    if(!view||!projection||sky.sunDirectionZ<=0.01f)return result;
    const float direction[3]={sky.sunDirectionX,sky.sunDirectionY,sky.sunDirectionZ};
    for(float v:direction)if(!std::isfinite(v))return result;
    float eye[3]={};
    for(int row=0;row<3;++row)
        for(int column=0;column<3;++column)
            eye[row]+=view[column*4+row]*direction[column];
    const float clipX=projection[0]*eye[0]+projection[4]*eye[1]+projection[8]*eye[2];
    const float clipY=projection[1]*eye[0]+projection[5]*eye[1]+projection[9]*eye[2];
    const float clipW=projection[3]*eye[0]+projection[7]*eye[1]+projection[11]*eye[2];
    if(!std::isfinite(clipW)||clipW<=0.00001f)return result;
    const float x=clipX/clipW,y=clipY/clipW;
    if(!std::isfinite(x)||!std::isfinite(y)||std::fabs(x)>1.15f||std::fabs(y)>1.15f)
        return result;
    const float weather=std::clamp(1.0f-0.75f*sky.cloudOpacity-0.20f*sky.rainIntensity,0.0f,1.0f);
    const float horizon=std::clamp((sky.sunDirectionZ-0.01f)*4.0f,0.0f,1.0f);
    const float strength=std::clamp(sky.sunCorona.a*sky.sunCore.a*sky.spriteBrightness*weather*horizon,0.0f,1.0f);
    if(strength<=0.0001f)return result;
    result.visible=true;result.x=x;result.y=y;result.intensity=strength;
    result.color={sky.sunCorona.r,sky.sunCorona.g,sky.sunCorona.b};
    return result;
}

struct StorylandSunFlareGhost {
    float x=0,y=0,size=0,alpha=0;
    std::array<float,3> color{};
    int texture=0;
};

inline std::array<StorylandSunFlareGhost,9> storylandSunFlareGhosts(
    const StorylandSkyState& sky,const StorylandSunFlareState& state,size_t& count) {
    std::array<StorylandSunFlareGhost,9> result{};
    count=0;
    if(!state.visible)return result;
    size_t nativeCount=0;
    const auto& retail=storylandCoronaFlares(1,nativeCount,false);
    for(size_t i=0;i<nativeCount;++i) {
        const auto& flare=retail[i];
        StorylandSunFlareGhost& ghost=result[count++];
        ghost.x=state.x*flare.position;
        ghost.y=state.y*flare.position;
        ghost.size=std::clamp(flare.size*0.0035f*std::max(0.2f,sky.spriteSize),0.009f,0.09f);
        ghost.alpha=std::clamp(state.intensity*0.65f,0.0f,0.65f);
        ghost.color={std::clamp(state.color[0]*float(flare.color[0])/255.0f,0.0f,1.0f),
                     std::clamp(state.color[1]*float(flare.color[1])/255.0f,0.0f,1.0f),
                     std::clamp(state.color[2]*float(flare.color[2])/255.0f,0.0f,1.0f)};
        ghost.texture=flare.texture;
    }
    return result;
}
