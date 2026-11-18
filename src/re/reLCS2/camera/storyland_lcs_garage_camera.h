#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <type_traits>
#include <vector>

using StorylandLcsGarageCameraPoint=std::array<float,3>;
struct StorylandLcsGarageCameraSphere {
    StorylandLcsGarageCameraPoint center{};
    float radius=0;
};
enum class StorylandLcsGarageCameraMode { FollowPed, MouseFollowPed, Vehicle, Other };
struct StorylandLcsGarageCameraCache {
    bool positionSet=false;
    StorylandLcsGarageCameraPoint eye{};
};
struct StorylandLcsGarageCameraView {
    StorylandLcsGarageCameraPoint eye{},target{};
    int32_t garage=-1;
};
using StorylandLcsGarageCameraGroundHeight=
    std::function<bool(const StorylandLcsGarageCameraPoint&,float&)>;

struct StorylandLcsGarageCameraGeometry {
    std::array<float,2> corner{},first{},second{};
    float firstLength=0,secondLength=0;
    float minimumX=0,maximumX=0,minimumY=0,maximumY=0,minimumZ=0,maximumZ=0;
};

inline bool storylandLcsGarageCameraFinite(const StorylandLcsGarageCameraPoint& point) {
    return std::all_of(point.begin(),point.end(),[](float value){return std::isfinite(value);});
}

template<class Garage>
bool storylandLcsGarageCameraGeometryFor(const Garage& garage,StorylandLcsGarageCameraGeometry& geometry) {
    const auto& b=garage.bounds;
    if(!std::all_of(b.begin(),b.end(),[](float value){return std::isfinite(value);})||b[2]>b[7])return false;
    StorylandLcsGarageCameraGeometry result;
    result.corner={b[0],b[1]};result.first={b[3]-b[0],b[4]-b[1]};
    result.second={b[5]-b[0],b[6]-b[1]};
    result.firstLength=std::hypot(result.first[0],result.first[1]);
    result.secondLength=std::hypot(result.second[0],result.second[1]);
    const float fourthX=b[3]+b[5]-b[0],fourthY=b[4]+b[6]-b[1];
    if(!std::isfinite(result.firstLength)||!std::isfinite(result.secondLength)||
        result.firstLength<=0||result.secondLength<=0||!std::isfinite(fourthX)||!std::isfinite(fourthY))return false;
    for(size_t axis=0;axis<2;++axis) {
        result.first[axis]/=result.firstLength;result.second[axis]/=result.secondLength;
    }
    // CGarages::AddOne includes the fourth corner in its minima, but its
    // native maxima use the three authored corners. Keep the same broad test.
    result.minimumX=std::min({b[0],b[3],b[5],fourthX});result.maximumX=std::max({b[0],b[3],b[5]});
    result.minimumY=std::min({b[1],b[4],b[6],fourthY});result.maximumY=std::max({b[1],b[4],b[6]});
    result.minimumZ=b[2];result.maximumZ=b[7];
    geometry=result;return true;
}

inline bool storylandLcsGarageCameraPointInside(const StorylandLcsGarageCameraGeometry& garage,
    const StorylandLcsGarageCameraPoint& point,float margin) {
    if(!storylandLcsGarageCameraFinite(point)||!std::isfinite(margin)||
        point[2]<garage.minimumZ-margin||point[2]>garage.maximumZ+margin)return false;
    const float x=point[0]-garage.corner[0],y=point[1]-garage.corner[1];
    const float first=x*garage.first[0]+y*garage.first[1];
    const float second=x*garage.second[0]+y*garage.second[1];
    return first>=-margin&&first<=garage.firstLength+margin&&
        second>=-margin&&second<=garage.secondLength+margin;
}

template<class Garage>
bool storylandLcsGarageCameraPointInside(const Garage& garage,
    const StorylandLcsGarageCameraPoint& point,float margin) {
    StorylandLcsGarageCameraGeometry geometry;
    return storylandLcsGarageCameraGeometryFor(garage,geometry)&&
        storylandLcsGarageCameraPointInside(geometry,point,margin);
}

template<class Garage>
bool storylandLcsGarageCameraEntityInside(const Garage& garage,
    const StorylandLcsGarageCameraPoint& origin,const std::vector<StorylandLcsGarageCameraSphere>& spheres,
    float margin) {
    StorylandLcsGarageCameraGeometry geometry;
    if(!storylandLcsGarageCameraGeometryFor(garage,geometry)||!storylandLcsGarageCameraFinite(origin)||
        !std::isfinite(margin)||origin[0]<geometry.minimumX-margin||origin[0]>geometry.maximumX+margin||
        origin[1]<geometry.minimumY-margin||origin[1]>geometry.maximumY+margin||
        origin[2]<geometry.minimumZ-margin||origin[2]>geometry.maximumZ+margin)return false;
    // Native IsEntityEntirelyInside3D checks every transformed collision
    // sphere against the oriented garage, reducing the margin by its radius.
    for(const auto& sphere:spheres) {
        if(!std::isfinite(sphere.radius)||sphere.radius<0||!storylandLcsGarageCameraFinite(sphere.center))return false;
        if(garage.type==12) {
            // The native crate garage uses sphere overlap in XY instead.
            // Its upper Y comparison uses SupX; sphere Z is not tested.
            if(sphere.center[0]+sphere.radius<geometry.minimumX-margin||
                sphere.center[0]-sphere.radius>geometry.maximumX+margin||
                sphere.center[1]+sphere.radius<geometry.minimumY-margin||
                sphere.center[1]-sphere.radius>geometry.maximumX+margin)return false;
        } else if(!storylandLcsGarageCameraPointInside(geometry,sphere.center,margin-sphere.radius))return false;
    }
    return true;
}

template<class World>
std::optional<StorylandLcsGarageCameraView> storylandResolveLcsGarageCamera(const World& world,
    StorylandLcsGarageCameraCache& cache,const StorylandLcsGarageCameraPoint& playerOrigin,
    const std::vector<StorylandLcsGarageCameraSphere>& playerSpheres,
    const StorylandLcsGarageCameraPoint& target,int32_t vehicleModel,int32_t vehicleType,
    StorylandLcsGarageCameraMode mode,const StorylandLcsGarageCameraGroundHeight& groundHeight) {
    if(!storylandLcsGarageCameraFinite(target)||!storylandLcsGarageCameraFinite(playerOrigin)) {
        cache={};return std::nullopt;
    }
    const bool inZone=std::any_of(world.garages.begin(),world.garages.end(),[&](const auto& garage) {
        return garage.type>0&&storylandLcsGarageCameraPointInside(garage,target,.5f);
    });
    if(!inZone||mode==StorylandLcsGarageCameraMode::MouseFollowPed) {
        cache={};return std::nullopt;
    }
    const bool pedestrian=mode==StorylandLcsGarageCameraMode::FollowPed&&vehicleType<0;
    const bool vehicle=mode==StorylandLcsGarageCameraMode::Vehicle&&(vehicleType==0||vehicleType==5);
    if(!pedestrian&&!vehicle)return std::nullopt;
    if(cache.positionSet&&!storylandLcsGarageCameraFinite(cache.eye))cache={};

    using Garage=typename std::decay_t<decltype(world.garages)>::value_type;
    const Garage* selected=nullptr;
    StorylandLcsGarageCameraGeometry selectedGeometry;
    for(const auto& garage:world.garages) {
        if(garage.type<=0||garage.type==13||world.lcsGarageFollowPlayer.count(garage.handle))continue;
        const auto motion=world.lcsGarageDoors.find(garage.handle);
        if(motion!=world.lcsGarageDoors.end()&&(motion->second.state<0||motion->second.state>5))continue;
        StorylandLcsGarageCameraGeometry geometry;
        if(!storylandLcsGarageCameraGeometryFor(garage,geometry))continue;
        const bool iceCreamVan=vehicle&&vehicleModel==154&&
            target[0]>=geometry.minimumX-.5f&&target[0]<=geometry.maximumX+.5f&&
            target[1]>=geometry.minimumY-.5f&&target[1]<=geometry.maximumY+.5f;
        if(iceCreamVan||storylandLcsGarageCameraEntityInside(garage,playerOrigin,playerSpheres,.25f)) {
            // Garage processing assigns the final eligible garage in pool order.
            selected=&garage;selectedGeometry=geometry;
        }
    }
    if(!selected)return std::nullopt;
    if(!cache.positionSet) {
        auto door=target;
        const auto motion=world.lcsGarageDoors.find(selected->handle);
        if(motion!=world.lcsGarageDoors.end()&&!motion->second.doors.empty()) {
            const auto& matrix=motion->second.doors.front().originalMatrix;
            door={matrix[12],matrix[13],0};
        }
        if(!storylandLcsGarageCameraFinite(door))return std::nullopt;
        float x=door[0]-(selectedGeometry.minimumX+selectedGeometry.maximumX)*.5f;
        float y=door[1]-(selectedGeometry.minimumY+selectedGeometry.maximumY)*.5f;
        const float length=std::hypot(x,y);
        if(!std::isfinite(length))return std::nullopt;
        if(length>0) {x/=length;y/=length;}else {x=1;y=0;} // CVector::Normalise's zero-vector direction.
        float ground=target[2]-.2f;
        float foundGround=0;
        if(groundHeight&&groundHeight(target,foundGround)&&std::isfinite(foundGround))ground=foundGround;
        const StorylandLcsGarageCameraPoint eye{door[0]+13*x,door[1]+13*y,ground+3.1f};
        if(!storylandLcsGarageCameraFinite(eye))return std::nullopt;
        cache.eye=eye;cache.positionSet=true;
    }
    return StorylandLcsGarageCameraView{cache.eye,target,selected->handle};
}
