#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

template<class Points, class Scalar>
bool storylandActorOutsideClip(const Points& points, const Scalar* projection, const Scalar* modelView) {
    if(points.empty()||!projection||!modelView)return false;
    std::array<double,3> minimum{
        std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()};
    std::array<double,3> maximum{-minimum[0],-minimum[1],-minimum[2]};
    for(const auto& point:points) {
        const std::array<double,3> position{point.x,point.y,point.z};
        for(size_t axis=0;axis<3;++axis) {
            if(!std::isfinite(position[axis]))return false;
            minimum[axis]=std::min(minimum[axis],position[axis]);
            maximum[axis]=std::max(maximum[axis],position[axis]);
        }
    }
    std::array<double,16> clip{},magnitude{};
    for(size_t column=0;column<4;++column)for(size_t row=0;row<4;++row) {
        for(size_t term=0;term<4;++term) {
            const double product=double(projection[term*4+row])*double(modelView[column*4+term]);
            if(!std::isfinite(product))return false;
            clip[column*4+row]+=product;
            magnitude[column*4+row]+=std::fabs(product);
        }
    }
    const std::array<double,3> center{
        (minimum[0]+maximum[0])*.5,(minimum[1]+maximum[1])*.5,(minimum[2]+maximum[2])*.5};
    const std::array<double,3> extent{
        (maximum[0]-minimum[0])*.5,(maximum[1]-minimum[1])*.5,(maximum[2]-minimum[2])*.5};
    for(size_t axis=0;axis<3;++axis)for(double side:{-1.0,1.0}) {
        double distance=clip[15]+side*clip[12+axis];
        double radius=0;
        double rounding=magnitude[15]+magnitude[12+axis];
        for(size_t component=0;component<3;++component) {
            const double coefficient=clip[component*4+3]+side*clip[component*4+axis];
            distance+=coefficient*center[component];
            radius+=std::fabs(coefficient)*extent[component];
            rounding+=(magnitude[component*4+3]+magnitude[component*4+axis])*
                std::max(std::fabs(minimum[component]),std::fabs(maximum[component]));
        }
        const double padding=64*std::numeric_limits<float>::epsilon()*std::max(1.0,rounding)+1e-5;
        if(distance+radius < -padding)return true;
    }
    return false;
}
