#pragma once
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

struct StorylandActorShadow {
    std::array<float,3> position{};
    float heading=0, halfWidth=0, halfLength=0, opacity=0;
    const char* texture=nullptr;
    std::array<float,2> front{},side{};
    bool nativeAxes=false;
    float maximumDrop=8.0f,projectionHeight=.1f;
};
struct StorylandActorShadowVertex {
    std::array<float,3> position{};
    std::array<float,2> uv{};
};

inline bool storylandActorShadowAxes(const StorylandActorShadow& shadow,
    std::array<float,2>& front,std::array<float,2>& side) {
    if(!shadow.texture||!std::isfinite(shadow.opacity)||shadow.opacity<=0||
        !std::isfinite(shadow.maximumDrop)||shadow.maximumDrop<=0||
        !std::isfinite(shadow.projectionHeight)||shadow.projectionHeight<0||
        !std::all_of(shadow.position.begin(),shadow.position.end(),
            [](float value){return std::isfinite(value);}))return false;
    if(shadow.nativeAxes) {front=shadow.front;side=shadow.side;}
    else {
        if(!std::isfinite(shadow.heading)||!std::isfinite(shadow.halfWidth)||
            !std::isfinite(shadow.halfLength)||shadow.halfWidth<=0||shadow.halfLength<=0)return false;
        const float angle=shadow.heading*.0174532925199433f;
        const float c=std::cos(angle),s=std::sin(angle);
        front={s*shadow.halfLength,-c*shadow.halfLength};
        side={c*shadow.halfWidth,s*shadow.halfWidth};
    }
    return std::isfinite(front[0])&&std::isfinite(front[1])&&
        std::isfinite(side[0])&&std::isfinite(side[1])&&
        std::fabs(side[0]*front[1]-side[1]*front[0])>1e-8f;
}

inline float storylandRoadVehicleShadowStrength(float distance,float strength) {
    if(!std::isfinite(distance)||distance<0||distance>=18.0f||!std::isfinite(strength)||strength<=0)return 0;
    const float fade=distance<13.5f?1.0f:1.0f-(distance-13.5f)/4.5f;
    return float(int(std::clamp(strength,0.0f,255.0f)*fade));
}

template<class Ground,class Draw>
void storylandVisitActorShadow(const StorylandActorShadow& shadow,const Ground& ground,const Draw& draw) {
    std::array<float,2> front,side;
    if(!ground||!storylandActorShadowAxes(shadow,front,side))return;
    constexpr int divisions=2;
    std::array<StorylandActorShadowVertex,9> vertices{};
    std::array<bool,9> valid{};
    for(int y=0;y<=divisions;++y)for(int x=0;x<=divisions;++x) {
        const float u=float(x)/divisions,v=float(y)/divisions;
        const size_t index=size_t(y*(divisions+1)+x);
        auto& vertex=vertices[index];
        vertex.position={shadow.position[0]+side[0]*(u*2-1)+front[0]*(1-v*2),
            shadow.position[1]+side[1]*(u*2-1)+front[1]*(1-v*2),
            shadow.position[2]+shadow.projectionHeight};
        float height=0;
        valid[index]=ground(vertex.position,height)&&std::isfinite(height)&&
            height>=shadow.position[2]-shadow.maximumDrop&&
            height<=shadow.position[2]+shadow.projectionHeight;
        vertex.position[2]=height+.025f;vertex.uv={u,v};
    }
    for(int y=0;y<divisions;++y)for(int x=0;x<divisions;++x) {
        const std::array<size_t,4> index{size_t(y*3+x),size_t(y*3+x+1),size_t((y+1)*3+x+1),size_t((y+1)*3+x)};
        if(valid[index[0]]&&valid[index[1]]&&valid[index[2]]&&valid[index[3]])
            draw(std::array<StorylandActorShadowVertex,4>{vertices[index[0]],vertices[index[1]],vertices[index[2]],vertices[index[3]]});
    }
}

template<class Draw>
void storylandVisitVehicleActorShadows(const StorylandPlayTestRuntime& runtime,
    const StorylandMissionEnvironment& world,const std::vector<StorylandNativeTrafficActor>& actors,
    size_t assetCount,const StorylandNativePoint& eye,float shadowStrength,const Draw& draw,
    bool vehiclesVisible=true) {
    if(!vehiclesVisible||!std::isfinite(shadowStrength)||shadowStrength<=0)return;
    const size_t count=std::min(runtime.vehicles.size(),actors.size());
    for(size_t i=0;i<count;++i) {
        const auto& vehicle=runtime.vehicles[i];const auto& actor=actors[i];
        if(!vehicle.visible||!vehicle.clumpVisible||!vehicle.renderAlpha||actor.water||vehicle.model>=assetCount)continue;
        if(vehicle.scriptHandle>=0) {
            const auto script=world.missionVehicles.find(vehicle.scriptHandle);
            if(script!=world.missionVehicles.end()&&script->second.deleted)continue;
        } else if(world.carDensityMultiplier<=0&&vehicle.generator<0&&vehicle.savedGarage<0&&
            !vehicle.coopControlled&&int(i)!=runtime.occupiedVehicle)continue;
        const float distance=std::hypot(actor.position.x-eye.x,actor.position.y-eye.y);
        const bool road=vehicle.nativeDefinition.domain==StorylandVehicleDomain::Road||vehicle.nativeDefinition.twoWheels();
        if(!std::isfinite(distance)||(!road&&distance>80.0f))continue;
        const float intensity=road?storylandRoadVehicleShadowStrength(distance,shadowStrength):shadowStrength;
        if(intensity<=0)continue;
        const auto& definition=vehicle.nativeDefinition;
        const auto* collision=definition.collision.get();
        const float width=collision?(collision->maximum.x-collision->minimum.x)*.5f:vehicle.halfWidth;
        const float length=collision?(collision->maximum.y-collision->minimum.y)*.5f:vehicle.halfLength;
        if(!std::isfinite(width)||!std::isfinite(length)||width<=0||length<=0)continue;
        StorylandActorShadow shadow;
        const auto matrix=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
            {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
            vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
            actor.water),definition.previewRoot);
        const float longitudinalCenter=collision?(collision->minimum.y+collision->maximum.y)*.5f:0;
        shadow.position={matrix[12]+matrix[4]*longitudinalCenter,
            matrix[13]+matrix[5]*longitudinalCenter,matrix[14]};
        shadow.heading=actor.yaw-90.0f;shadow.halfWidth=width;shadow.halfLength=length;
        shadow.front={matrix[4]*length,matrix[5]*length};
        const float upright=matrix[10]<0?-1.0f:1.0f;
        shadow.side={matrix[0]*width*upright,matrix[1]*width*upright};
        shadow.nativeAxes=true;shadow.maximumDrop=4.5f;
        shadow.opacity=std::clamp(intensity/255.0f,0.0f,1.0f)*vehicle.renderAlpha/255.0f;
        shadow.texture=vehicle.nativeDefinition.twoWheels()?"shad_bike":"shad_car";
        draw(shadow);
    }
}
