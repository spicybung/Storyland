#pragma once
#include "src/storyland/storyland_actor_shadow.h"
#include <algorithm>
#include <cmath>

inline float storylandRetailPedShadowStrength(float distance, float strength) {
    if(!std::isfinite(distance)||distance<0||distance>=13.0f||!std::isfinite(strength)||strength<=0)return 0;
    const float fade=distance<6.5f?1.0f:1.0f-(distance-6.5f)*0.1538461595773697f;
    return float(int(std::clamp(strength,0.0f,255.0f)*fade));
}

struct StorylandPedShadowBasis {
    std::array<float,2> front{0,.5f},side{.5f,0},displacement{};
};

inline StorylandPedShadowBasis storylandPedShadowBasis(const std::array<float,3>& sunlight) {
    StorylandPedShadowBasis result;
    const float horizontal=std::hypot(sunlight[0],sunlight[1]);
    if(!std::isfinite(horizontal)||horizontal<1e-6f)return result;
    const float halfUnit=.5f/horizontal;
    result.front={-sunlight[0]*(horizontal+1)*halfUnit,-sunlight[1]*(horizontal+1)*halfUnit};
    result.side={-sunlight[1]*halfUnit,sunlight[0]*halfUnit};
    result.displacement={-sunlight[0]*.5f,-sunlight[1]*.5f};
    return result;
}

inline void storylandSetPedShadowBasis(StorylandActorShadow& shadow,
    const StorylandPedShadowBasis& basis) {
    shadow.position[0]+=basis.displacement[0];shadow.position[1]+=basis.displacement[1];
    shadow.position[2]+=1.0f;
    shadow.front=basis.front;shadow.side=basis.side;shadow.nativeAxes=true;
    shadow.halfWidth=std::hypot(basis.side[0],basis.side[1]);
    shadow.halfLength=std::hypot(basis.front[0],basis.front[1]);
    shadow.maximumDrop=4.0f;
}

// Visit each visible, on-foot gameplay actor once. The native sprite raster is
// shared by the protagonists, ambient pedestrians and mission characters.
template<class Draw>
void storylandVisitPedActorShadows(const StorylandPlayTestRuntime& runtime,
    const StorylandMissionEnvironment& world,
    const std::vector<StorylandNativeTrafficActor>& actors,
    const StorylandNativePoint& player,float playerHeading,
    const StorylandNativePoint& eye,float shadowStrength,const Draw& draw,
    const std::array<float,3>& sunlight={0,-1,0},bool ambientPedestriansVisible=true) {
    if(!std::isfinite(shadowStrength)||shadowStrength<=0)return;
    const auto basis=storylandPedShadowBasis(sunlight);
    const auto visit=[&](const StorylandNativePoint& position,float heading,uint8_t alpha) {
        if(!alpha||!std::isfinite(heading)||!std::isfinite(position.x)||
            !std::isfinite(position.y)||!std::isfinite(position.z)||
            !std::isfinite(eye.x)||!std::isfinite(eye.y))return;
        const float distance=std::hypot(position.x-eye.x,position.y-eye.y);
        const float intensity=storylandRetailPedShadowStrength(distance,shadowStrength);
        if(intensity<=0)return;
        if(world.sphereOnScreen&&!world.sphereOnScreen({position.x,position.y,position.z+1.0f},3.0f))return;
        StorylandActorShadow shadow;
        shadow.position={position.x,position.y,position.z};
        shadow.heading=heading;shadow.halfWidth=.6f;shadow.halfLength=.6f;
        shadow.opacity=std::clamp(intensity/255.0f,0.0f,1.0f)*alpha/255.0f;
        shadow.texture="shad_ped";
        storylandSetPedShadowBasis(shadow,basis);
        draw(shadow);
    };
    if(world.playerVisible&&runtime.occupiedVehicle<0&&!runtime.playerWater.active)
        visit(player,playerHeading,world.playerRenderAlpha);
    if(ambientPedestriansVisible) {
        const size_t count=std::min(actors.size(),runtime.pedestrians.size());
        for(size_t index=0;index<count;++index) {
            const auto& ped=runtime.pedestrians[index];
            if(ped.scriptHandle>=0||ped.serviceVehicleIdentity||ped.water.active)continue;
            visit(actors[index].position,actors[index].yaw,ped.renderAlpha);
        }
    }
    for(const auto& item:world.missionCharacters) {
        // The player is rendered separately and is not an extra pedestrian.
        if(item.first==(runtime.crime.vcs?0:1))continue;
        const auto& ped=item.second;
        if(!ped.visible||ped.removeFromWorld||ped.vehicle>=0||ped.attached)continue;
        visit({ped.x,ped.y,ped.z},ped.heading,ped.renderAlpha);
    }
}
