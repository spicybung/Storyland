#pragma once
#include "src/storyland/storyland_actor_shadow.h"
#include <array>
#include <vector>
#include <limits>

namespace StorylandShadowProjection {
using Vertex=StorylandActorShadowVertex;
using Point=StorylandNativePoint;

inline Point transform(const std::array<float,16>& matrix,const Point& point) {
    return {matrix[0]*point.x+matrix[4]*point.y+matrix[8]*point.z+matrix[12],
        matrix[1]*point.x+matrix[5]*point.y+matrix[9]*point.z+matrix[13],
        matrix[2]*point.x+matrix[6]*point.y+matrix[10]*point.z+matrix[14]};
}

struct LocalBounds {
    std::array<double,3> minimum{};
    std::array<double,3> maximum{};
    bool valid = false;

    bool excludes(const Point& a, const Point& b, const Point& c) const {
        if (!valid) return false;
        return (a.x < minimum[0] && b.x < minimum[0] && c.x < minimum[0]) ||
            (a.x > maximum[0] && b.x > maximum[0] && c.x > maximum[0]) ||
            (a.y < minimum[1] && b.y < minimum[1] && c.y < minimum[1]) ||
            (a.y > maximum[1] && b.y > maximum[1] && c.y > maximum[1]) ||
            (a.z < minimum[2] && b.z < minimum[2] && c.z < minimum[2]) ||
            (a.z > maximum[2] && b.z > maximum[2] && c.z > maximum[2]);
    }
};

inline LocalBounds localBounds(const std::array<float,16>& matrix,
    const Point& minimum, const Point& maximum,
    const Point& modelMinimum, const Point& modelMaximum) {
    LocalBounds bounds;
    const std::array<double,3> x{matrix[0],matrix[1],matrix[2]};
    const std::array<double,3> y{matrix[4],matrix[5],matrix[6]};
    const std::array<double,3> z{matrix[8],matrix[9],matrix[10]};
    const std::array<std::array<double,3>,3> inverse{{
        {y[1]*z[2]-y[2]*z[1],y[2]*z[0]-y[0]*z[2],y[0]*z[1]-y[1]*z[0]},
        {z[1]*x[2]-z[2]*x[1],z[2]*x[0]-z[0]*x[2],z[0]*x[1]-z[1]*x[0]},
        {x[1]*y[2]-x[2]*y[1],x[2]*y[0]-x[0]*y[2],x[0]*y[1]-x[1]*y[0]}
    }};
    const double determinant=x[0]*inverse[0][0]+x[1]*inverse[0][1]+x[2]*inverse[0][2];
    if (!std::isfinite(determinant) || std::fabs(determinant)<1e-10) return bounds;
    const std::array<double,3> center{
        (double(minimum.x)+maximum.x)*.5-matrix[12],
        (double(minimum.y)+maximum.y)*.5-matrix[13],
        (double(minimum.z)+maximum.z)*.5-matrix[14]};
    const std::array<double,3> extent{
        (double(maximum.x)-minimum.x)*.5,
        (double(maximum.y)-minimum.y)*.5,
        (double(maximum.z)-minimum.z)*.5};
    const std::array<double,3> modelMagnitude{
        std::max(std::fabs(double(modelMinimum.x)),std::fabs(double(modelMaximum.x))),
        std::max(std::fabs(double(modelMinimum.y)),std::fabs(double(modelMaximum.y))),
        std::max(std::fabs(double(modelMinimum.z)),std::fabs(double(modelMaximum.z)))};
    std::array<double,3> padding{};
    for (size_t axis=0;axis<3;++axis) {
        const double scale=std::fabs(double(matrix[axis]))*modelMagnitude[0]+
            std::fabs(double(matrix[4+axis]))*modelMagnitude[1]+
            std::fabs(double(matrix[8+axis]))*modelMagnitude[2]+
            std::fabs(double(matrix[12+axis]));
        padding[axis]=16*std::numeric_limits<float>::epsilon()*std::max(1.0,scale)+1e-5;
    }
    for (size_t axis=0;axis<3;++axis) {
        double localCenter=0, radius=0;
        for (size_t component=0;component<3;++component) {
            const double coefficient=inverse[axis][component]/determinant;
            localCenter+=coefficient*center[component];
            radius+=std::fabs(coefficient)*(extent[component]+padding[component]);
        }
        bounds.minimum[axis]=localCenter-radius;
        bounds.maximum[axis]=localCenter+radius;
        if (!std::isfinite(bounds.minimum[axis]) || !std::isfinite(bounds.maximum[axis]) ||
            bounds.minimum[axis]>bounds.maximum[axis]) return LocalBounds{};
    }
    bounds.valid=true;
    return bounds;
}

inline Point normal(const Point& a,const Point& b,const Point& c) {
    return {(b.y-a.y)*(c.z-a.z)-(b.z-a.z)*(c.y-a.y),
        (b.z-a.z)*(c.x-a.x)-(b.x-a.x)*(c.z-a.z),
        (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)};
}

inline float clipDistance(const Vertex& vertex,unsigned plane,float minimumZ,float maximumZ) {
    switch(plane) {
    case 0:return vertex.uv[0];
    case 1:return 1-vertex.uv[0];
    case 2:return vertex.uv[1];
    case 3:return 1-vertex.uv[1];
    case 4:return vertex.position[2]-minimumZ;
    default:return maximumZ-vertex.position[2];
    }
}

template<class Draw>
void triangle(const StorylandActorShadow& shadow,const std::array<float,2>& front,
    const std::array<float,2>& side,const Point& a,const Point& b,const Point& c,const Draw& draw) {
    const auto n=normal(a,b,c);
    const float magnitude=std::hypot(n.x,n.y,n.z);
    if(!std::isfinite(magnitude)||magnitude<1e-8f||std::fabs(n.z)<magnitude*.1f)return;
    const float determinant=side[0]*front[1]-side[1]*front[0];
    std::array<Vertex,12> polygon{},clipped{};
    const std::array<Point,3> points{a,b,c};
    for(size_t index=0;index<points.size();++index) {
        const auto& point=points[index];
        if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))return;
        const float x=point.x-shadow.position[0],y=point.y-shadow.position[1];
        const float sideways=(x*front[1]-y*front[0])/determinant;
        const float forward=(side[0]*y-side[1]*x)/determinant;
        polygon[index]={{point.x,point.y,point.z},{(sideways+1)*.5f,(1-forward)*.5f}};
    }
    size_t count=3;
    const float minimumZ=shadow.position[2]-shadow.maximumDrop;
    const float maximumZ=shadow.position[2]+shadow.projectionHeight;
    for(unsigned plane=0;plane<6&&count>=3;++plane) {
        size_t nextCount=0;
        Vertex previous=polygon[count-1];
        float previousDistance=clipDistance(previous,plane,minimumZ,maximumZ);
        for(size_t index=0;index<count;++index) {
            const auto& current=polygon[index];
            const float currentDistance=clipDistance(current,plane,minimumZ,maximumZ);
            const bool previousInside=previousDistance>=0,currentInside=currentDistance>=0;
            if(previousInside!=currentInside) {
                const float fraction=previousDistance/(previousDistance-currentDistance);
                auto& intersection=clipped[nextCount++];
                for(size_t axis=0;axis<3;++axis)
                    intersection.position[axis]=previous.position[axis]+
                        (current.position[axis]-previous.position[axis])*fraction;
                for(size_t axis=0;axis<2;++axis)
                    intersection.uv[axis]=previous.uv[axis]+(current.uv[axis]-previous.uv[axis])*fraction;
            }
            if(currentInside)clipped[nextCount++]=current;
            previous=current;previousDistance=currentDistance;
        }
        count=nextCount;polygon=clipped;
    }
    for(size_t index=0;index<count;++index) {
        polygon[index].position[2]+=.025f;
        for(float& coordinate:polygon[index].uv)coordinate=std::clamp(coordinate,0.0f,1.0f);
    }
    for(size_t index=1;index+1<count;++index) {
        const auto& p=polygon[0].position;const auto& q=polygon[index].position;
        const auto& r=polygon[index+1].position;
        if(std::fabs((q[0]-p[0])*(r[1]-p[1])-(q[1]-p[1])*(r[0]-p[0]))<1e-8f)continue;
        draw(std::array<Vertex,3>{polygon[0],polygon[index],polygon[index+1]});
    }
}
}

template<class Draw>
void storylandVisitNativeActorShadow(const StorylandActorShadow& shadow,
    const StorylandNativeWorld& world,const Draw& draw) {
    namespace Projection=StorylandShadowProjection;
    std::array<float,2> front,side;
    if(!storylandActorShadowAxes(shadow,front,side))return;
    const float halfX=std::fabs(front[0])+std::fabs(side[0]);
    const float halfY=std::fabs(front[1])+std::fabs(side[1]);
    const StorylandNativePoint minimum{shadow.position[0]-halfX,shadow.position[1]-halfY,
        shadow.position[2]-shadow.maximumDrop};
    const StorylandNativePoint maximum{shadow.position[0]+halfX,shadow.position[1]+halfY,
        shadow.position[2]+shadow.projectionHeight};
    if(std::fabs(minimum.x)>1e8f||std::fabs(minimum.y)>1e8f||
        std::fabs(maximum.x)>1e8f||std::fabs(maximum.y)>1e8f)return;
    bool indexed=world.collisionIndexOwner==world.instances.data()&&
        world.collisionIndexCount==world.instances.size()&&
        world.collisionIndexModels==world.collisionModels.size();
    std::vector<uint32_t> candidates;
    if(indexed) {
        const int32_t firstX=int32_t(std::floor(minimum.x/StorylandCollisionCellSize));
        const int32_t lastX=int32_t(std::floor(maximum.x/StorylandCollisionCellSize));
        const int32_t firstY=int32_t(std::floor(minimum.y/StorylandCollisionCellSize));
        const int32_t lastY=int32_t(std::floor(maximum.y/StorylandCollisionCellSize));
        if(int64_t(lastX-firstX+1)*int64_t(lastY-firstY+1)>4096)indexed=false;
        else {
            candidates=world.collisionLargeInstances;
            for(int32_t y=firstY;y<=lastY;++y)for(int32_t x=firstX;x<=lastX;++x) {
                const auto cell=world.collisionCells.find({x,y});
                if(cell!=world.collisionCells.end())
                    candidates.insert(candidates.end(),cell->second.begin(),cell->second.end());
            }
            std::sort(candidates.begin(),candidates.end());
            candidates.erase(std::unique(candidates.begin(),candidates.end()),candidates.end());
        }
    }
    const size_t count=indexed?candidates.size():world.instances.size();
    for(size_t index=0;index<count;++index) {
        const size_t slot=indexed?size_t(candidates[index]):index;
        if(slot>=world.instances.size())continue;
        const auto& instance=world.instances[slot];
        if(!instance.nativeUsesCollision||!(instance.nativeSectorLists&0x63u)||instance.poolHeader==0x2Cu)continue;
        if(world.scriptEnvironment&&world.scriptEnvironment->lcsExtendedWorld.deletedNativeObjects.count(
            (uint64_t(instance.poolHeader)<<32)|instance.poolIndex))continue;
        const auto found=world.collisionModels.find(instance.modelId);
        if(found==world.collisionModels.end()||!world.collisionModelEnabled(found->second))continue;
        const auto& model=found->second;const auto& matrix=instance.matrix;
        const StorylandNativePoint center{(model.minimum.x+model.maximum.x)*.5f,
            (model.minimum.y+model.maximum.y)*.5f,(model.minimum.z+model.maximum.z)*.5f};
        const StorylandNativePoint extent{(model.maximum.x-model.minimum.x)*.5f,
            (model.maximum.y-model.minimum.y)*.5f,(model.maximum.z-model.minimum.z)*.5f};
        const auto worldCenter=Projection::transform(matrix,center);
        const StorylandNativePoint worldExtent{
            std::fabs(matrix[0])*extent.x+std::fabs(matrix[4])*extent.y+std::fabs(matrix[8])*extent.z,
            std::fabs(matrix[1])*extent.x+std::fabs(matrix[5])*extent.y+std::fabs(matrix[9])*extent.z,
            std::fabs(matrix[2])*extent.x+std::fabs(matrix[6])*extent.y+std::fabs(matrix[10])*extent.z};
        if(worldCenter.x+worldExtent.x<minimum.x||worldCenter.x-worldExtent.x>maximum.x||
            worldCenter.y+worldExtent.y<minimum.y||worldCenter.y-worldExtent.y>maximum.y||
            worldCenter.z+worldExtent.z<minimum.z||worldCenter.z-worldExtent.z>maximum.z)continue;
        const auto local=Projection::localBounds(matrix,minimum,maximum,model.minimum,model.maximum);
        for(const auto& triangle:model.triangles) {
            const auto& vertices=triangle.vertices;
            if(vertices[0]>=model.vertices.size()||vertices[1]>=model.vertices.size()||
                vertices[2]>=model.vertices.size())continue;
            if(local.excludes(model.vertices[vertices[0]],model.vertices[vertices[1]],
                model.vertices[vertices[2]]))continue;
            Projection::triangle(shadow,front,side,
                Projection::transform(matrix,model.vertices[vertices[0]]),
                Projection::transform(matrix,model.vertices[vertices[1]]),
                Projection::transform(matrix,model.vertices[vertices[2]]),draw);
        }
        for(const auto& box:model.boxes) {
            std::array<StorylandNativePoint,8> vertices;
            for(size_t vertex=0;vertex<vertices.size();++vertex)
                vertices[vertex]=Projection::transform(matrix,
                    {vertex&1?box.maximum.x:box.minimum.x,vertex&2?box.maximum.y:box.minimum.y,
                     vertex&4?box.maximum.z:box.minimum.z});
            const auto boxCenter=Projection::transform(matrix,
                {(box.minimum.x+box.maximum.x)*.5f,(box.minimum.y+box.maximum.y)*.5f,
                 (box.minimum.z+box.maximum.z)*.5f});
            constexpr std::array<std::array<size_t,4>,6> faces{{
                {0,2,3,1},{4,5,7,6},{0,1,5,4},{2,6,7,3},{0,4,6,2},{1,3,7,5}}};
            for(const auto& face:faces) {
                const auto& a=vertices[face[0]];const auto& b=vertices[face[1]];
                const auto& c=vertices[face[2]];const auto& d=vertices[face[3]];
                auto normal=Projection::normal(a,b,c);
                if(normal.x*(a.x-boxCenter.x)+normal.y*(a.y-boxCenter.y)+normal.z*(a.z-boxCenter.z)<0)
                    normal={-normal.x,-normal.y,-normal.z};
                if(normal.z<=std::hypot(normal.x,normal.y,normal.z)*.1f)continue;
                Projection::triangle(shadow,front,side,a,b,c,draw);
                Projection::triangle(shadow,front,side,a,c,d,draw);
            }
        }
    }
}
