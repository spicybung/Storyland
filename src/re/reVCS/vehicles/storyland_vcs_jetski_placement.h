#pragma once
#include <array>
#include <cstddef>
#include <cmath>

namespace storyland_vcs_jetski_placement {

using Point=std::array<float,3>;
enum class Kind { Raised, Retained, UnderVehicle, CollisionRecovery };
struct Input {
    Point origin{},vehicleOrigin{},nativeVelocity{};
    float nativeStep=0;
};
struct Output {
    Point origin{};
    Kind kind=Kind::Retained;
    bool nativeBlockedFlags=false;
};

inline bool finite(const Point& point) {
    return std::isfinite(point[0])&&std::isfinite(point[1])&&std::isfinite(point[2]);
}

template<class SphereQuery,class VerticalQuery>
bool evaluate(const Input& input,SphereQuery&& sphere,VerticalQuery&& vertical,Output& output) {
    if(!finite(input.origin)||!finite(input.vehicleOrigin)||!finite(input.nativeVelocity)||
        !std::isfinite(input.nativeStep)||input.nativeStep<0)return false;
    Point predicted{};
    for(size_t axis=0;axis<3;++axis)
        predicted[axis]=input.origin[axis]+input.nativeVelocity[axis]*input.nativeStep;
    if(!finite(predicted))return false;
    const Point points[]{
        {input.origin[0],input.origin[1],input.origin[2]+.5f},
        {input.origin[0],input.origin[1],input.origin[2]+.8f},
        {predicted[0],predicted[1],predicted[2]+.5f},
        {predicted[0],predicted[1],predicted[2]+.8f}};
    bool blocked=false;
    for(const auto& point:points) {
        if(!sphere(point,.4f,blocked))return false;
        if(blocked)break;
    }
    Output result;
    result.origin=input.origin;
    if(blocked) {
        bool support=false;
        if(!vertical(input.origin,input.origin[2]-2.0f,support))return false;
        result.nativeBlockedFlags=true;
        if(support)result.kind=Kind::CollisionRecovery;
        else {
            result.kind=Kind::UnderVehicle;
            result.origin={input.vehicleOrigin[0],input.vehicleOrigin[1],input.vehicleOrigin[2]-2.0f};
        }
    } else {
        if(!sphere({input.origin[0],input.origin[1],input.origin[2]+1.3f},.6f,blocked))return false;
        if(!blocked) {result.kind=Kind::Raised;result.origin[2]+=.5f;}
    }
    output=result;
    return finite(output.origin);
}

}
