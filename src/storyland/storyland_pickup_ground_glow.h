#pragma once
#include "src/storyland/storyland_pickup_visual.h"
#include "src/leeds/leeds_native_world.h"
#include <set>

struct StorylandPickupGlowVertex {
    std::array<float,3> position{};
    float u=0,v=0;
};

inline bool storylandPickupGlowBoundsOverlap(const StorylandNativeCollisionModel& model,
    const std::array<float,16>& matrix,const StorylandPickupGroundGlow& glow,
    float radiusX,float radiusY) {
    const std::array<float,3> lower{model.minimum.x,model.minimum.y,model.minimum.z};
    const std::array<float,3> upper{model.maximum.x,model.maximum.y,model.maximum.z};
    bool nonzero=false;
    for(size_t axis=0;axis<3;++axis) {
        if(!std::isfinite(lower[axis])||!std::isfinite(upper[axis])||lower[axis]>upper[axis])return true;
        nonzero=nonzero||lower[axis]!=upper[axis];
    }
    if(!nonzero)return true;
    std::array<float,3> worldMinimum{},worldMaximum{};
    for(unsigned corner=0;corner<8;++corner) {
        std::array<float,3> point{};
        for(size_t axis=0;axis<3;++axis) {
            point[axis]=matrix[axis]*(corner&1?upper[0]:lower[0])+
                matrix[4+axis]*(corner&2?upper[1]:lower[1])+
                matrix[8+axis]*(corner&4?upper[2]:lower[2])+matrix[12+axis];
            if(!std::isfinite(point[axis]))return true;
            if(!corner)worldMinimum[axis]=worldMaximum[axis]=point[axis];
            else {
                worldMinimum[axis]=std::min(worldMinimum[axis],point[axis]);
                worldMaximum[axis]=std::max(worldMaximum[axis],point[axis]);
            }
        }
    }
    constexpr float tolerance=.0001f;
    return worldMaximum[0]+tolerance>=glow.position[0]-radiusX&&
        worldMinimum[0]-tolerance<=glow.position[0]+radiusX&&
        worldMaximum[1]+tolerance>=glow.position[1]-radiusY&&
        worldMinimum[1]-tolerance<=glow.position[1]+radiusY&&
        worldMaximum[2]+tolerance>=glow.position[2]-glow.downDistance&&
        worldMinimum[2]-tolerance<=glow.position[2]+glow.upDistance;
}

template<class Emit>
void storylandVisitPickupGroundGlow(const StorylandNativeWorld& world,
    const StorylandPickupGroundGlow& glow,const std::array<float,3>& camera,Emit emit) {
    if(!glow.visible)return;
    const float dx=camera[0]-glow.position[0],dy=camera[1]-glow.position[1];
    if(dx*dx+dy*dy>glow.range*glow.range)return;
    const float determinant=glow.frontX*glow.sideY-glow.frontY*glow.sideX;
    if(!std::isfinite(determinant)||determinant==0)return;
    const float radiusX=std::abs(glow.frontX)+std::abs(glow.sideX);
    const float radiusY=std::abs(glow.frontY)+std::abs(glow.sideY);
    std::set<uint32_t> candidates;
    if(world.collisionIndexCount==world.instances.size()&&!world.collisionCells.empty()) {
        const int west=int(std::floor((glow.position[0]-radiusX)/StorylandCollisionCellSize));
        const int east=int(std::floor((glow.position[0]+radiusX)/StorylandCollisionCellSize));
        const int south=int(std::floor((glow.position[1]-radiusY)/StorylandCollisionCellSize));
        const int north=int(std::floor((glow.position[1]+radiusY)/StorylandCollisionCellSize));
        for(int x=west;x<=east;++x)for(int y=south;y<=north;++y) {
            const auto found=world.collisionCells.find({x,y});
            if(found!=world.collisionCells.end())candidates.insert(found->second.begin(),found->second.end());
        }
        candidates.insert(world.collisionLargeInstances.begin(),world.collisionLargeInstances.end());
    } else {
        for(size_t index=0;index<world.instances.size();++index)candidates.insert(uint32_t(index));
    }
    for(uint32_t index:candidates) {
        if(index>=world.instances.size())continue;
        const auto& instance=world.instances[index];
        if(!instance.nativeUsesCollision)continue;
        const auto found=world.collisionModels.find(instance.modelId);
        if(found==world.collisionModels.end()||!world.collisionModelEnabled(found->second))continue;
        const auto& collision=found->second;
        const auto& matrix=instance.matrix;
        if(!storylandPickupGlowBoundsOverlap(collision,matrix,glow,radiusX,radiusY))continue;
        for(const auto& triangle:collision.triangles) {
            std::vector<StorylandPickupGlowVertex> polygon;
            for(uint32_t vertexIndex:triangle.vertices) {
                if(vertexIndex>=collision.vertices.size()) {polygon.clear();break;}
                const auto& local=collision.vertices[vertexIndex];
                StorylandPickupGlowVertex vertex;
                for(size_t axis=0;axis<3;++axis)vertex.position[axis]=matrix[axis]*local.x+
                    matrix[4+axis]*local.y+matrix[8+axis]*local.z+matrix[12+axis];
                const float x=vertex.position[0]-glow.position[0];
                const float y=vertex.position[1]-glow.position[1];
                vertex.u=(x*glow.sideY-y*glow.sideX)/determinant;
                vertex.v=(y*glow.frontX-x*glow.frontY)/determinant;
                polygon.push_back(vertex);
            }
            if(polygon.size()!=3)continue;
            const auto& a=polygon[0].position;
            const auto& b=polygon[1].position;
            const auto& c=polygon[2].position;
            if((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])<=0)continue;
            for(unsigned plane=0;plane<6&&!polygon.empty();++plane) {
                const auto distance=[&](const StorylandPickupGlowVertex& vertex) {
                    if(plane==0)return vertex.u+1;
                    if(plane==1)return 1-vertex.u;
                    if(plane==2)return vertex.v+1;
                    if(plane==3)return 1-vertex.v;
                    if(plane==4)return vertex.position[2]-(glow.position[2]-glow.downDistance);
                    return glow.position[2]+glow.upDistance-vertex.position[2];
                };
                std::vector<StorylandPickupGlowVertex> clipped;
                auto previous=polygon.back();
                float previousDistance=distance(previous);
                for(const auto& current:polygon) {
                    const float currentDistance=distance(current);
                    if((previousDistance>=0)!=(currentDistance>=0)) {
                        const float fraction=previousDistance/(previousDistance-currentDistance);
                        StorylandPickupGlowVertex intersection;
                        for(size_t axis=0;axis<3;++axis)intersection.position[axis]=previous.position[axis]+
                            (current.position[axis]-previous.position[axis])*fraction;
                        intersection.u=previous.u+(current.u-previous.u)*fraction;
                        intersection.v=previous.v+(current.v-previous.v)*fraction;
                        clipped.push_back(intersection);
                    }
                    if(currentDistance>=0)clipped.push_back(current);
                    previous=current;
                    previousDistance=currentDistance;
                }
                polygon=std::move(clipped);
            }
            for(size_t vertex=1;vertex+1<polygon.size();++vertex)
                for(size_t corner:{size_t(0),vertex,vertex+1}) {
                    auto output=polygon[corner];
                    output.u=output.u*.5f+.5f;
                    output.v=output.v*.5f+.5f;
                    emit(output);
                }
        }
    }
}
