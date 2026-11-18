#pragma once
#include "src/leeds/leeds_texture.h"
#include "src/storyland/storyland_weapon_types.h"
#include <algorithm>
#include <cmath>
#include <string>

// These sprites lie on a supporting surface; other particles are billboards
// and must retain their ordinary depth relationship to scenery and actors.
inline bool storylandParticleUsesSurfaceDepthOffset(const StorylandRuntimeParticle& particle) {
    if(particle.nativePresentation.enabled)return false;
    return particle.waterSurface || particle.kind == StorylandParticleKind::BloodPool ||
        (particle.nativePhysics && particle.nativeVcs &&
            (particle.nativeType == 76 || particle.nativeType == 77));
}

inline StorylandNativePoint storylandSwimmingSurfaceVertex(const StorylandRuntimeParticle& particle,
    float horizontal,float vertical) {
    const float cosine=std::cos(particle.waterHeading),sine=std::sin(particle.waterHeading);
    return {particle.position.x-sine*horizontal+cosine*vertical,
        particle.position.y+cosine*horizontal+sine*vertical,particle.position.z};
}

inline StorylandNativePoint storylandBloodPoolVertex(const StorylandRuntimeParticle& particle,
    float horizontal,float vertical) {
    auto normal=particle.bloodSurfaceNormal;
    const float normalLength=std::hypot(normal.x,normal.y,normal.z);
    if(!std::isfinite(normalLength)||normalLength<.0001f)normal={0,0,1};
    else {normal.x/=normalLength;normal.y/=normalLength;normal.z/=normalLength;}
    StorylandNativePoint tangent{normal.z,0,-normal.x};
    const float tangentLength=std::hypot(tangent.x,tangent.z);
    if(tangentLength<.0001f)tangent={1,0,0};
    else {tangent.x/=tangentLength;tangent.z/=tangentLength;}
    const StorylandNativePoint bitangent{normal.y*tangent.z-normal.z*tangent.y,
        normal.z*tangent.x-normal.x*tangent.z,normal.x*tangent.y-normal.y*tangent.x};
    const float cosine=std::cos(particle.bloodSurfaceRotation),sine=std::sin(particle.bloodSurfaceRotation);
    const float x=horizontal*cosine-vertical*sine,y=horizontal*sine+vertical*cosine;
    return {particle.position.x+tangent.x*x+bitangent.x*y,
        particle.position.y+tangent.y*x+bitangent.y*y,
        particle.position.z+tangent.z*x+bitangent.z*y};
}

inline bool storylandParticleSmokeTexture(const std::string& name) {
    return name=="smoke1"||name=="smoke2"||name=="smoke3"||
        name=="smoke4"||name=="smoke5"||name=="smokeii_3";
}

inline std::string storylandParticleTextureKey(const char* name,bool nativePixels=false) {
    if(!name)return {};
    return nativePixels&&storylandParticleSmokeTexture(name)?"native:"+std::string(name):std::string(name);
}

inline bool storylandPrepareSmokeTexture(const std::string& name, RgbaImage& image) {
    if(!storylandParticleSmokeTexture(name)||image.width<=0||image.height<=0||
        image.rgba.size()!=size_t(image.width)*size_t(image.height)*4)return false;
    for(size_t offset=3;offset<image.rgba.size();offset+=4)
        if(image.rgba[offset]!=255)return false;
    for(size_t offset=0;offset<image.rgba.size();offset+=4) {
        const unsigned coverage=std::max({image.rgba[offset],image.rgba[offset+1],image.rgba[offset+2]});
        for(size_t channel=0;channel<3;++channel)
            image.rgba[offset+channel]=coverage?uint8_t(unsigned(image.rgba[offset+channel])*255u/coverage):255;
        image.rgba[offset+3]=uint8_t(coverage);
    }
    return true;
}

inline bool storylandParticleBillboardAxes(const float* view,float* right,float* up) {
    float rightLength=0.0f;
    for(int axis=0;axis<3;++axis) {
        right[axis]=view[axis*4];
        up[axis]=view[axis*4+1];
        rightLength+=right[axis]*right[axis];
    }
    if(!std::isfinite(rightLength)||rightLength<1e-12f)return false;
    rightLength=std::sqrt(rightLength);
    float projection=0.0f;
    for(int axis=0;axis<3;++axis) {
        right[axis]/=rightLength;
        projection+=right[axis]*up[axis];
    }
    float upLength=0.0f;
    for(int axis=0;axis<3;++axis) {
        up[axis]-=right[axis]*projection;
        upLength+=up[axis]*up[axis];
    }
    if(!std::isfinite(upLength)||upLength<1e-12f)return false;
    upLength=std::sqrt(upLength);
    for(int axis=0;axis<3;++axis)up[axis]/=upLength;
    return true;
}

enum class StorylandNativeParticleBlend { Additive, AlphaAdditive, Alpha };

inline StorylandNativeParticleBlend storylandNativeParticleBlend(const StorylandRuntimeParticle& particle) {
    if(particle.nativePresentation.flags&0x04u)return StorylandNativeParticleBlend::Alpha;
    if(particle.nativePresentation.flags&0x80u)return StorylandNativeParticleBlend::AlphaAdditive;
    return StorylandNativeParticleBlend::Additive;
}

inline std::array<uint8_t,4> storylandNativeParticleColor(const StorylandRuntimeParticle& particle) {
    std::array<uint8_t,4> color;
    for(size_t channel=0;channel<3;++channel)
        color[channel]=uint8_t((unsigned(particle.nativeColor[channel])*particle.nativePresentation.intensity)>>8);
    color[3]=particle.nativePresentation.alpha;
    return color;
}

struct StorylandNativeParticleQuad {
    std::array<StorylandNativePoint,4> vertices{};
    std::array<std::array<float,2>,4> uv{{{0,0},{0,1},{1,1},{1,0}}};
};

inline bool storylandNativeParticleQuad(const StorylandRuntimeParticle& particle,
    const float* view,const float* projection,float nativeFieldOfView,StorylandNativeParticleQuad& quad) {
    const auto& presentation=particle.nativePresentation;
    if(!presentation.enabled||!presentation.alpha||!std::isfinite(particle.size)||particle.size<=0||
        !std::isfinite(nativeFieldOfView)||nativeFieldOfView<=0||nativeFieldOfView>=180||
        !std::isfinite(projection[0])||!std::isfinite(projection[5])||
        std::fabs(projection[0])<.000001f||projection[5]<=0)return false;
    float right[3],up[3];
    if(!storylandParticleBillboardAxes(view,right,up))return false;
    const auto project=[&](const StorylandNativePoint& point,std::array<float,3>& screen) {
        const float x=view[0]*point.x+view[4]*point.y+view[8]*point.z+view[12];
        const float y=view[1]*point.x+view[5]*point.y+view[9]*point.z+view[13];
        const float z=view[2]*point.x+view[6]*point.y+view[10]*point.z+view[14];
        const float w=projection[3]*x+projection[7]*y+projection[11]*z+projection[15];
        if(!std::isfinite(w)||w<=.000001f)return false;
        const float clipZ=projection[2]*x+projection[6]*y+projection[10]*z+projection[14];
        if(clipZ<-w||clipZ>w)return false;
        screen={(projection[0]*x+projection[4]*y+projection[8]*z+projection[12])/w*320.0f,
            -(projection[1]*x+projection[5]*y+projection[9]*z+projection[13])/w*224.0f,-z};
        return std::isfinite(screen[0])&&std::isfinite(screen[1])&&screen[2]>.000001f;
    };
    std::array<float,3> screen;
    if(!project(particle.position,screen))return false;
    const float aspect=std::fabs(projection[5]/projection[0]);
    float width=640.0f/(aspect*screen[2]),height=448.0f/screen[2];
    if(presentation.flags&0x4000u) {
        const StorylandNativePoint eye{
            -(view[0]*view[12]+view[1]*view[13]+view[2]*view[14]),
            -(view[4]*view[12]+view[5]*view[13]+view[6]*view[14]),
            -(view[8]*view[12]+view[9]*view[13]+view[10]*view[14])};
        const float dx=particle.position.x-eye.x,dy=particle.position.y-eye.y,dz=particle.position.z-eye.z;
        const float length=std::hypot(dx,dy,dz);
        const float horizontal=length>.000001f?std::hypot(dx,dy)/length:0;
        if(horizontal>.001f)width*=std::max(0.0f,std::cos(horizontal*1.5707963267948966f));
    }
    if(presentation.textureStretch[0]!=0||presentation.textureStretch[1]!=0) {
        uint32_t seed=uint32_t(particle.nativeType)*0x9E3779B9u+
            uint32_t(std::max(0.0f,particle.lifetime-particle.remaining)*50.0f);
        for(float coordinate:{particle.position.x,particle.position.y,particle.position.z}) {
            uint32_t bits=0;std::memcpy(&bits,&coordinate,sizeof(bits));seed^=bits+0x9E3779B9u+(seed<<6)+(seed>>2);
        }
        const auto random=[&]() {
            seed^=seed>>16;seed*=0x7FEB352Du;seed^=seed>>15;seed*=0x846CA68Bu;seed^=seed>>16;
            return .1f+.9f*float(seed&0xFFFFFFu)/16777215.0f;
        };
        width+=random()*presentation.textureStretch[0];
        height+=random()*presentation.textureStretch[1];
    }
    float rotation=float(presentation.rotation)*.017453292519943295f;
    float halfWidth=particle.size*width,halfHeight=particle.size*height;
    if(presentation.rotation==0&&(presentation.flags&0x10u)) {
        const StorylandNativePoint previous{particle.position.x-particle.velocity.x*.02f,
            particle.position.y-particle.velocity.y*.02f,particle.position.z-particle.velocity.z*.02f};
        std::array<float,3> previousScreen;
        if(project(previous,previousScreen)) {
            const float dx=screen[0]-previousScreen[0],dy=screen[1]-previousScreen[1];
            const float length=std::hypot(dx,dy);
            if(length>.000001f) {
                rotation=std::atan2(dx,dy);
                halfHeight+=length*presentation.trailLength;
            }
        }
    } else if(presentation.rotation==0&&(presentation.flags&0x100u))
        halfHeight=(particle.size+std::fabs(particle.velocity.z*.2f)*presentation.trailLength)*height;
    const float cameraScale=particle.nativeVcs?
        .7002065181732178f/std::tan(nativeFieldOfView*.008726646259971648f):1.0f;
    halfWidth*=cameraScale;halfHeight*=cameraScale;
    if(!std::isfinite(halfWidth)||!std::isfinite(halfHeight)||halfWidth<=0||halfHeight<=0)return false;
    const float cosine=std::cos(rotation),sine=std::sin(rotation);
    const float horizontalScale=screen[2]/(320.0f*projection[0]);
    const float verticalScale=-screen[2]/(224.0f*projection[5]);
    const std::array<std::array<float,2>,4> corners{{{-1,-1},{-1,1},{1,1},{1,-1}}};
    for(size_t index=0;index<corners.size();++index) {
        const float x=corners[index][0]*halfWidth,y=corners[index][1]*halfHeight;
        const float horizontal=(cosine*x+sine*y)*horizontalScale;
        const float vertical=(-sine*x+cosine*y)*verticalScale;
        quad.vertices[index]={particle.position.x+right[0]*horizontal+up[0]*vertical,
            particle.position.y+right[1]*horizontal+up[1]*vertical,
            particle.position.z+right[2]*horizontal+up[2]*vertical};
    }
    return true;
}
