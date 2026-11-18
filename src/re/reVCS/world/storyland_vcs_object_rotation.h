#pragma once

#include <algorithm>
#include <array>
#include <cmath>

inline bool storylandVcsAdvanceObjectFrame(const std::array<float,16>& matrix,
    const std::array<float,3>& angularVelocity,float nativeStep,std::array<float,16>& result) {
    if(!std::isfinite(nativeStep)||nativeStep<0||
        !std::all_of(matrix.begin(),matrix.end(),[](float value){return std::isfinite(value);})||
        !std::all_of(angularVelocity.begin(),angularVelocity.end(),[](float value){return std::isfinite(value);}))
        return false;
    if(nativeStep==0||std::all_of(angularVelocity.begin(),angularVelocity.end(),[](float value){return value==0;})) {
        result=matrix;
        return true;
    }
    using Vector=std::array<float,3>;
    const auto cross=[](const Vector& first,const Vector& second) {
        return Vector{first[1]*second[2]-first[2]*second[1],
            first[2]*second[0]-first[0]*second[2],first[0]*second[1]-first[1]*second[0]};
    };
    const auto normalize=[](Vector& value) {
        const float square=value[0]*value[0]+value[1]*value[1]+value[2]*value[2];
        if(!std::isfinite(square)||square<=0)return false;
        const float inverse=1/std::sqrt(square);
        for(float& component:value)component*=inverse;
        return true;
    };
    const Vector rotation{angularVelocity[0]*nativeStep,angularVelocity[1]*nativeStep,
        angularVelocity[2]*nativeStep};
    std::array<Vector,3> axes{{{matrix[0],matrix[1],matrix[2]},
        {matrix[4],matrix[5],matrix[6]},{matrix[8],matrix[9],matrix[10]}}};
    // CPhysical::ApplyTurnSpeed34AAB0 and ProcessCollision3496F0..349870.
    for(auto& axis:axes) {
        const auto delta=cross(rotation,axis);
        for(size_t row=0;row<3;++row)axis[row]+=delta[row];
    }
    axes[2]=cross(axes[0],axes[1]);
    if(!normalize(axes[2]))return false;
    axes[0]=cross(axes[1],axes[2]);
    if(!normalize(axes[0]))return false;
    axes[1]=cross(axes[2],axes[0]);
    auto candidate=matrix;
    for(size_t column=0;column<3;++column)
        for(size_t row=0;row<3;++row)candidate[column*4+row]=axes[column][row];
    if(!std::all_of(candidate.begin(),candidate.end(),[](float value){return std::isfinite(value);}))
        return false;
    result=candidate;
    return true;
}
