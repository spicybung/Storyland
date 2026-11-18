#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstdint>

inline bool storylandLcsHudItemVisible(int16_t flashing,int16_t item,uint32_t nativeFrameCounter) {
    return flashing!=item||(nativeFrameCounter&8u)!=0;
}

struct StorylandNavigationDirection {float x=0,y=-1,distance=0,height=0;};
inline bool storylandNavigationDirection(const std::array<float,3>& target,
    const std::array<float,3>& origin,float cameraYaw,StorylandNavigationDirection& result) {
    if(!std::isfinite(cameraYaw))return false;
    for(size_t i=0;i<3;++i)if(!std::isfinite(target[i])||!std::isfinite(origin[i]))return false;
    const double dx=double(target[0])-origin[0],dy=double(target[1])-origin[1],dz=double(target[2])-origin[2];
    const double flat=std::hypot(dx,dy),distance=std::hypot(flat,dz);
    const double angle=std::fmod(double(cameraYaw),360.0)*0.01745329251994329577;
    const double limit=std::numeric_limits<float>::max();
    result.distance=float(std::min(distance,limit));result.height=float(std::clamp(dz,-limit,limit));
    if(flat>0.0001) {
        result.x=float((std::sin(angle)*dx-std::cos(angle)*dy)/flat);
        result.y=float(-(std::cos(angle)*dx+std::sin(angle)*dy)/flat);
    } else {result.x=0;result.y=-1;}
    return true;
}

inline bool storylandMissionArrowPose(const std::array<float,3>& authoredPosition,
    float animationTime,std::array<float,3>& point,std::array<float,3>& direction) {
    if(!std::isfinite(animationTime))return false;
    for(float value:authoredPosition)if(!std::isfinite(value))return false;
    point=authoredPosition;
    point[2]+=1.8f+0.12f*std::sin(animationTime*2.8f);
    direction={point[0],point[1],point[2]-1.0f};
    return true;
}

struct StorylandRadarVertex {float x=0,y=0,u=0,v=0;};
using StorylandMarkerTriangle=std::array<std::array<float,3>,3>;
inline std::vector<StorylandMarkerTriangle> storylandCheckpointMesh(const std::array<float,3>& position,
    const std::array<float,3>& destination,float radius,uint8_t type) {
    std::vector<StorylandMarkerTriangle> triangles;
    if(!std::isfinite(radius)||radius<=0||radius>10000)return triangles;
    std::array<float,3> forward{},right{},up{};
    for(size_t axis=0;axis<3;++axis) {
        if(!std::isfinite(position[axis])||!std::isfinite(destination[axis]))return triangles;
        forward[axis]=destination[axis]-position[axis];
    }
    const float length=std::hypot(forward[0],forward[1],forward[2]);
    if(!std::isfinite(length))return triangles;
    if(length>0)for(auto& value:forward)value/=length;else forward={0,1,0};
    const float horizontal=std::hypot(forward[0],forward[1]);
    right=horizontal>.0001f?std::array<float,3>{forward[1]/horizontal,-forward[0]/horizontal,0}:
        std::array<float,3>{1,0,0};
    up={right[1]*forward[2]-right[2]*forward[1],right[2]*forward[0]-right[0]*forward[2],
        right[0]*forward[1]-right[1]*forward[0]};
    const auto point=[&](float x,float y,float z) {
        std::array<float,3> result{};
        for(size_t axis=0;axis<3;++axis)result[axis]=position[axis]+radius*
            (right[axis]*x+forward[axis]*y+up[axis]*z);
        return result;
    };
    if(type==0) {
        constexpr unsigned latitudeSegments=12,longitudeSegments=24;
        const auto spherePoint=[&](float latitude,float longitude) {
            const float height=std::sin(latitude);
            const float horizontal=std::cos(latitude);
            return std::array<float,3>{position[0]+radius*horizontal*std::cos(longitude),
                position[1]+radius*horizontal*std::sin(longitude),position[2]+radius*height};
        };
        for(unsigned latitude=0;latitude<latitudeSegments;++latitude) {
            const float lower=-1.57079632679f+float(latitude)*3.14159265359f/latitudeSegments;
            const float upper=-1.57079632679f+float(latitude+1)*3.14159265359f/latitudeSegments;
            for(unsigned longitude=0;longitude<longitudeSegments;++longitude) {
                const float first=float(longitude)*6.28318530718f/longitudeSegments;
                const float second=float(longitude+1)*6.28318530718f/longitudeSegments;
                const auto a=spherePoint(lower,first),b=spherePoint(upper,first);
                const auto c=spherePoint(upper,second),d=spherePoint(lower,second);
                triangles.push_back({a,b,c});
                triangles.push_back({a,c,d});
            }
        }
    } else if(type==3) {
        // A solid directional marker; the native placement, target, size and
        // colour are retained while the host supplies the marker mesh.
        const std::array<std::array<float,2>,7> outline{{{-.14f,-.65f},{.14f,-.65f},{.14f,.1f},
            {.45f,.1f},{0,.7f},{-.45f,.1f},{-.14f,.1f}}};
        for(size_t index=0;index<outline.size();++index) {
            const auto& a=outline[index];const auto& b=outline[(index+1)%outline.size()];
            const auto a0=point(a[0],a[1],-.08f),a1=point(a[0],a[1],.08f);
            const auto b0=point(b[0],b[1],-.08f),b1=point(b[0],b[1],.08f);
            triangles.push_back({point(0,0,.08f),a1,b1});
            triangles.push_back({point(0,0,-.08f),b0,a0});
            triangles.push_back({a0,b0,b1});triangles.push_back({a0,b1,a1});
        }
    } else if(type==8) {
        constexpr unsigned segments=40;
        for(unsigned index=0;index<segments;++index) {
            const float a=float(index)*6.28318530718f/segments,b=float(index+1)*6.28318530718f/segments;
            const auto a0=point(std::cos(a)*.88f,0,std::sin(a)*.88f),a1=point(std::cos(a),0,std::sin(a));
            const auto b0=point(std::cos(b)*.88f,0,std::sin(b)*.88f),b1=point(std::cos(b),0,std::sin(b));
            triangles.push_back({a0,a1,b1});triangles.push_back({a0,b1,b0});
        }
    }
    return triangles;
}
inline std::vector<StorylandRadarVertex> storylandClipRadarTile(const std::array<StorylandRadarVertex,4>& corners,
    float centerX,float centerY,float radius,unsigned sides=48) {
    std::vector<StorylandRadarVertex> polygon(corners.begin(),corners.end());
    sides=std::max(3u,sides);
    const float limit=radius*std::cos(3.14159265358979323846f/sides);
    for(unsigned side=0;side<sides&&!polygon.empty();++side) {
        const float angle=(float(side)+0.5f)*6.28318530718f/sides;
        const float nx=std::cos(angle),ny=std::sin(angle);
        const auto distance=[&](const StorylandRadarVertex& vertex) {
            return (vertex.x-centerX)*nx+(vertex.y-centerY)*ny-limit;
        };
        std::vector<StorylandRadarVertex> clipped;
        auto previous=polygon.back();float previousDistance=distance(previous);
        for(const auto& vertex:polygon) {
            const float currentDistance=distance(vertex);
            if((previousDistance<=0)!=(currentDistance<=0)) {
                const float fraction=previousDistance/(previousDistance-currentDistance);
                clipped.push_back({previous.x+(vertex.x-previous.x)*fraction,previous.y+(vertex.y-previous.y)*fraction,
                    previous.u+(vertex.u-previous.u)*fraction,previous.v+(vertex.v-previous.v)*fraction});
            }
            if(currentDistance<=0)clipped.push_back(vertex);
            previous=vertex;previousDistance=currentDistance;
        }
        polygon=std::move(clipped);
    }
    return polygon;
}
