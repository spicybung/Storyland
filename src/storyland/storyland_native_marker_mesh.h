#pragma once

#include "src/leeds/leeds_model.h"
#include "src/storyland/storyland_native_marker.h"
#include <algorithm>
#include <array>

struct StorylandNativeMarkerBasis {
    std::array<float,3> right{1,0,0};
    std::array<float,3> forward{0,1,0};
    std::array<float,3> up{0,0,1};
};

// Marker material zero is replaced by the runtime color, including its alpha.
template<class DrawVertex>
void storylandVisitNativeMarkerPart(const StorylandModelFile& model,
    const StorylandMissionEnvironment::Marker& marker,const std::array<float,3>& position,
    float scale,float angle,float opacity,const DrawVertex& drawVertex,
    float verticalScale=0,const StorylandNativeMarkerBasis& basis={}) {
    const auto& points=model.previewPoints();
    const auto& prelights=model.previewPrelights();
    const auto& texcoords=model.previewTexcoords();
    const auto materials=model.previewMaterialColors();
    const float cosine=std::cos(angle),sine=std::sin(angle);
    if(verticalScale<=0)verticalScale=scale;
    for(const auto& triangle:model.previewTriangles()) {
        if(triangle.a>=points.size()||triangle.b>=points.size()||triangle.c>=points.size())continue;
        const uint32_t material=triangle.materialIndex<materials.size()?
            materials[triangle.materialIndex]:0xFFFFFFFFu;
        for(uint32_t index:std::array<uint32_t,3>{triangle.a,triangle.b,triangle.c}) {
            const auto& point=points[index];
            std::array<float,4> color{};
            if(triangle.materialIndex==0)
                color={marker.red/255.0f,marker.green/255.0f,marker.blue/255.0f,opacity};
            else for(size_t channel=0;channel<4;++channel)
                color[channel]=float((material>>(channel*8))&255u)/255.0f;
            if(index<prelights.size()&&prelights[index].valid) {
                const auto& prelight=prelights[index];
                const float colorScale=prelight.encoding==StorylandPrelightEncoding::Ps2VifRgba8888?
                    1.0f/128.0f:1.0f/255.0f;
                const std::array<uint8_t,4> channels{prelight.red,prelight.green,prelight.blue,prelight.alpha};
                for(size_t channel=0;channel<4;++channel)
                    color[channel]*=std::min(1.0f,channels[channel]*colorScale);
            }
            std::array<float,3> oriented{};
            for(size_t axis=0;axis<3;++axis)
                oriented[axis]=point.x*basis.right[axis]+point.y*basis.forward[axis]+point.z*basis.up[axis];
            const std::array<float,3> transformed{
                position[0]+(oriented[0]*cosine-oriented[1]*sine)*scale,
                position[1]+(oriented[0]*sine+oriented[1]*cosine)*scale,
                position[2]+oriented[2]*verticalScale};
            const std::array<float,2> uv=index<texcoords.size()?
                std::array<float,2>{texcoords[index].u,texcoords[index].v}:std::array<float,2>{};
            drawVertex(transformed,color,uv,triangle.materialIndex);
        }
    }
}

inline float storylandNativeArrowScale(float size,float distance) {
    if(distance>=25.0f)return size;
    if(distance<=5.0f)return size*0.7f;
    return size-(25.0f-distance)*(0.3f*size)/20.0f;
}

inline float storylandNativeMarkerOpacity(uint8_t alpha,float radius,float distance) {
    float result=alpha;
    if(distance<=radius+1.0f)result*=0.3f;
    else if(distance<radius+12.0f)result*=1.0f-(radius+12.0f-distance)*0.7f/11.0f;
    return float(uint8_t(result))/255.0f;
}

template<class DrawVertex>
void storylandVisitNativeWorldArrow(const StorylandModelFile& model,
    const std::array<float,3>& authoredPosition,float size,const std::array<uint8_t,4>& color,
    uint32_t createdAt,uint32_t now,const std::array<float,3>& focus,const DrawVertex& drawVertex) {
    if(!std::isfinite(size)||size<=0)return;
    StorylandMissionEnvironment::Marker marker;
    marker.red=color[0];marker.green=color[1];marker.blue=color[2];
    const float distance=std::hypot(authoredPosition[0]-focus[0],authoredPosition[1]-focus[1]);
    const float standardSize=storylandNativeArrowScale(size,distance);
    const uint32_t elapsed=now-createdAt;
    const float pulse=std::sin(float(elapsed&1023u)*6.2831853071795864769f/1024.0f);
    auto position=authoredPosition;
    position[2]+=0.25f*standardSize*pulse;
    const float angle=float(elapsed%1440u)*0.25f*0.0174532925199433f;
    storylandVisitNativeMarkerPart(model,marker,position,standardSize*(1.0f-0.2f*pulse),
        angle,color[3]/255.0f,drawVertex);
}

template<class DrawVertex>
void storylandVisitNativeDoorArrow(const StorylandModelFile& model,
    const std::array<float,3>& point,uint32_t now,const std::array<float,3>& focus,
    const std::array<uint8_t,4>& color,const DrawVertex& drawVertex) {
    const std::array<float,3> position{point[0],point[1],point[2]+1.8f};
    storylandVisitNativeWorldArrow(model,position,0.8f,color,0,now,focus,drawVertex);
}

template<class DrawVertex>
void storylandVisitNativeEntryMarker(const StorylandModelFile& model,
    const StorylandMissionEnvironment::Marker& marker,uint32_t now,const DrawVertex& drawVertex) {
    storylandVisitNativeMarkerPart(model,marker,storylandNativeMarkerPosition(marker,now),
        marker.radius,0.0f,228.0f/255.0f,drawVertex);
}

template<class GroundHeight,class DrawVertex>
void storylandVisitNativeWorldCylinderSet(const StorylandModelFile& model,uint8_t type,
    const std::array<float,3>& authoredPosition,float size,const std::array<uint8_t,4>& color,
    uint32_t createdAt,uint32_t now,const std::array<float,3>& focus,
    const GroundHeight& groundHeight,const DrawVertex& drawVertex,float pulseFraction=0.1f) {
    if(!std::isfinite(size)||size<=0)return;
    StorylandMissionEnvironment::Marker marker;
    marker.red=color[0];marker.green=color[1];marker.blue=color[2];
    const bool lcs=type==5u;
    const unsigned layers=lcs?3u:2u;
    const std::array<float,3> radii{1.0f,0.93f,0.86f};
    const std::array<float,3> rotations=lcs?std::array<float,3>{1.0f,2.0f,-1.0f}:
        std::array<float,3>{1.0f,-1.0f,0.0f};
    const float distance=std::hypot(authoredPosition[0]-focus[0],authoredPosition[1]-focus[1]);
    const float pulse=1.0f-pulseFraction*storylandNativeMarkerPulse(type,createdAt,now);
    const uint32_t elapsed=now-createdAt;
    const float angle=float(elapsed%7200u)*0.05f*0.0174532925199433f;
    float ground=authoredPosition[2];
    const bool grounded=distance<100.0f&&groundHeight&&
        groundHeight({authoredPosition[0],authoredPosition[1],authoredPosition[2]+1.0f},ground)&&
        std::isfinite(ground)&&ground!=0.0f;
    for(unsigned layer=0;layer<layers;++layer) {
        const float radius=size*radii[layer];
        const float opacity=storylandNativeMarkerOpacity(color[3],radius,distance);
        const std::array<float,3> position{authoredPosition[0],authoredPosition[1],
            grounded?ground-0.05f*radius:authoredPosition[2]};
        storylandVisitNativeMarkerPart(model,marker,position,radius*pulse,
            angle*rotations[layer],opacity,drawVertex);
    }
}

template<class DrawVertex>
void storylandVisitNativePersistentMarker(const StorylandModelFile& model,
    const StorylandMissionEnvironment::Marker& marker,const StorylandMissionEnvironment& world,
    const std::array<float,3>& focus,const DrawVertex& drawVertex) {
    if(marker.type==7u) {
        storylandVisitNativeEntryMarker(model,marker,world.gameMilliseconds,drawVertex);
        return;
    }
    storylandVisitNativeWorldCylinderSet(model,marker.type,{marker.x,marker.y,marker.z},marker.radius,
        {marker.red,marker.green,marker.blue,uint8_t(marker.type==5u?159u:228u)},
        marker.createdAt,world.gameMilliseconds,focus,world.groundHeightAt,drawVertex);
}

inline StorylandNativeMarkerBasis storylandNativeDirectionalMarkerBasis(
    const std::array<float,3>& position,const std::array<float,3>& destination,uint8_t type) {
    StorylandNativeMarkerBasis basis;
    std::array<float,3> direction{destination[0]-position[0],destination[1]-position[1],
        type==8u?destination[2]-position[2]:0.0f};
    if(type!=8u&&direction[0]*direction[0]+direction[1]*direction[1]<0.0001f)direction[0]=1.0f;
    float length=std::sqrt(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
    if(length<=0)direction={1,0,0};
    else for(auto& component:direction)component/=length;
    std::array<float,3> up{0,0,1};
    if(type==8u&&direction[0]==0&&direction[1]==0&&direction[2]==1)up={1,0,0};
    basis.right={up[1]*direction[2]-up[2]*direction[1],
        up[2]*direction[0]-up[0]*direction[2],up[0]*direction[1]-up[1]*direction[0]};
    length=std::sqrt(basis.right[0]*basis.right[0]+basis.right[1]*basis.right[1]+basis.right[2]*basis.right[2]);
    if(length<=0)basis.right={1,0,0};
    else for(auto& component:basis.right)component/=length;
    basis.up={direction[1]*basis.right[2]-direction[2]*basis.right[1],
        direction[2]*basis.right[0]-direction[0]*basis.right[2],
        direction[0]*basis.right[1]-direction[1]*basis.right[0]};
    if(type==8u)for(auto& component:basis.up)component=-component;
    basis.forward=direction;
    if(type==3u) {
        constexpr float cosine=0.9396926207859084f,sine=-0.3420201433256687f;
        for(size_t axis=0;axis<3;++axis) {
            basis.forward[axis]=direction[axis]*cosine+basis.up[axis]*sine;
            basis.up[axis]=basis.up[axis]*cosine-direction[axis]*sine;
        }
    }
    return basis;
}

inline bool storylandNativePointerArrow(const std::array<float,3>& target,
    const std::array<float,3>& eye,const std::array<float,3>& forward,int area,
    StorylandMissionEnvironment::ScriptCheckpoint& marker) {
    for(size_t axis=0;axis<3;++axis)
        if(!std::isfinite(target[axis])||!std::isfinite(eye[axis])||!std::isfinite(forward[axis]))return false;
    marker.position={eye[0]+3.0f*forward[0],eye[1]+3.0f*forward[1],eye[2]+3.0f*forward[2]+0.8f};
    marker.direction=target;
    marker.radius=0.8f;
    marker.type=3;
    marker.area=area;
    marker.color={237,130,180,200};
    return true;
}

template<class DrawVertex>
void storylandVisitNativeFrameMarker(const StorylandModelFile& model,
    const StorylandMissionEnvironment::ScriptCheckpoint& checkpoint,
    const StorylandMissionEnvironment& world,const std::array<float,3>& focus,
    const DrawVertex& drawVertex) {
    StorylandMissionEnvironment::Marker marker;
    marker.x=checkpoint.position[0];
    marker.y=checkpoint.position[1];
    marker.z=checkpoint.position[2];
    marker.radius=checkpoint.radius;
    marker.type=checkpoint.type;
    marker.area=checkpoint.area;
    marker.red=checkpoint.color[0];
    marker.green=checkpoint.color[1];
    marker.blue=checkpoint.color[2];
    if(checkpoint.type==3u||checkpoint.type==8u) {
        const float distance=std::hypot(marker.x-focus[0],marker.y-focus[1]);
        const float scale=checkpoint.type==3u?storylandNativeArrowScale(marker.radius,distance):marker.radius;
        const auto basis=storylandNativeDirectionalMarkerBasis(checkpoint.position,checkpoint.direction,checkpoint.type);
        storylandVisitNativeMarkerPart(model,marker,checkpoint.position,scale,0,
            checkpoint.color[3]/255.0f,drawVertex,0,basis);
    } else if(checkpoint.nativeDrawCheckpoint) {
        const float distance=std::hypot(marker.x-focus[0],marker.y-focus[1]);
        float ground=marker.z;
        const bool grounded=distance<100.0f&&world.groundHeightAt&&
            world.groundHeightAt({marker.x,marker.y,marker.z+1.0f},ground)&&
            std::isfinite(ground)&&ground!=0.0f;
        const std::array<float,3> position{marker.x,marker.y,grounded?ground-0.05f*marker.radius:marker.z};
        const float angle=float(world.gameMilliseconds%7200u)*0.05f*0.0174532925199433f;
        storylandVisitNativeMarkerPart(model,marker,position,marker.radius,angle,
            storylandNativeMarkerOpacity(checkpoint.color[3],marker.radius,distance),drawVertex,100.0f);
    } else storylandVisitNativePersistentMarker(model,marker,world,focus,drawVertex);
}
