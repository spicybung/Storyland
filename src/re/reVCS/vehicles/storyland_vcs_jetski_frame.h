#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

using StorylandVcsJetSkiMatrix=std::array<float,16>;

inline StorylandVcsJetSkiMatrix storylandVcsJetSkiIdentity() {
    return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
}

inline StorylandVcsJetSkiMatrix storylandVcsJetSkiMatrixProduct(
    const StorylandVcsJetSkiMatrix& left,const StorylandVcsJetSkiMatrix& right) {
    StorylandVcsJetSkiMatrix result{};
    for(size_t column=0;column<4;++column)
        for(size_t row=0;row<4;++row)
            result[column*4+row]=((left[row]*right[column*4]+
                left[4+row]*right[column*4+1])+left[8+row]*right[column*4+2])+
                left[12+row]*right[column*4+3];
    return result;
}

inline bool storylandVcsJetSkiInverse(const StorylandVcsJetSkiMatrix& input,
    StorylandVcsJetSkiMatrix& result) {
    if(!std::all_of(input.begin(),input.end(),[](float value){return std::isfinite(value);})||
        input[3]!=0||input[7]!=0||input[11]!=0||input[15]!=1)return false;
    const float a=input[0],b=input[4],c=input[8];
    const float d=input[1],e=input[5],f=input[9];
    const float g=input[2],h=input[6],i=input[10];
    const float determinant=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(!std::isfinite(determinant)||std::fabs(determinant)<.000001f)return false;
    auto inverse=storylandVcsJetSkiIdentity();
    inverse[0]=(e*i-f*h)/determinant;inverse[4]=(c*h-b*i)/determinant;
    inverse[8]=(b*f-c*e)/determinant;inverse[1]=(f*g-d*i)/determinant;
    inverse[5]=(a*i-c*g)/determinant;inverse[9]=(c*d-a*f)/determinant;
    inverse[2]=(d*h-e*g)/determinant;inverse[6]=(b*g-a*h)/determinant;
    inverse[10]=(a*e-b*d)/determinant;
    for(size_t row=0;row<3;++row)
        inverse[12+row]=-((inverse[row]*input[12]+inverse[4+row]*input[13])+
            inverse[8+row]*input[14]);
    if(!std::all_of(inverse.begin(),inverse.end(),[](float value){return std::isfinite(value);}))
        return false;
    result=inverse;return true;
}

struct StorylandVcsJetSkiChassis {
    bool available=false;
    StorylandVcsJetSkiMatrix local=storylandVcsJetSkiIdentity();
    StorylandVcsJetSkiMatrix parent=storylandVcsJetSkiIdentity();
    StorylandVcsJetSkiMatrix inverseBind=storylandVcsJetSkiIdentity();
    std::vector<uint8_t> pointMask;
};

template<class Point>
inline Point storylandVcsJetSkiTransformPoint(const StorylandVcsJetSkiMatrix& matrix,
    const Point& source) {
    Point point=source;
    point.x=(matrix[0]*source.x+matrix[4]*source.y)+matrix[8]*source.z+matrix[12];
    point.y=(matrix[1]*source.x+matrix[5]*source.y)+matrix[9]*source.z+matrix[13];
    point.z=(matrix[2]*source.x+matrix[6]*source.y)+matrix[10]*source.z+matrix[14];
    if(source.hasNormal) {
        point.nx=(matrix[0]*source.nx+matrix[4]*source.ny)+matrix[8]*source.nz;
        point.ny=(matrix[1]*source.nx+matrix[5]*source.ny)+matrix[9]*source.nz;
        point.nz=(matrix[2]*source.nx+matrix[6]*source.ny)+matrix[10]*source.nz;
    }
    if(source.hasPs2Normal) {
        point.ps2Nx=(matrix[0]*source.ps2Nx+matrix[4]*source.ps2Ny)+matrix[8]*source.ps2Nz;
        point.ps2Ny=(matrix[1]*source.ps2Nx+matrix[5]*source.ps2Ny)+matrix[9]*source.ps2Nz;
        point.ps2Nz=(matrix[2]*source.ps2Nx+matrix[6]*source.ps2Ny)+matrix[10]*source.ps2Nz;
    }
    return point;
}

inline bool storylandVcsJetSkiChassisFrame(const StorylandVcsJetSkiChassis& chassis,
    float nativeAngularZ,StorylandVcsJetSkiMatrix& result) {
    if(!chassis.available||!std::isfinite(nativeAngularZ))return false;
    const float angle=std::clamp(-5.5f*nativeAngularZ,-1.023f,1.023f);
    const float cosine=std::cos(angle),sine=std::sin(angle);
    result={cosine,0,-sine,0,0,1,0,0,sine,0,cosine,0,
        chassis.local[12],chassis.local[13],chassis.local[14],1};
    return true;
}

inline bool storylandVcsJetSkiChassisDelta(const StorylandVcsJetSkiChassis& chassis,
    float nativeAngularZ,StorylandVcsJetSkiMatrix& result) {
    StorylandVcsJetSkiMatrix frame{};
    if(!storylandVcsJetSkiChassisFrame(chassis,nativeAngularZ,frame))return false;
    result=storylandVcsJetSkiMatrixProduct(
        storylandVcsJetSkiMatrixProduct(chassis.parent,frame),chassis.inverseBind);
    return std::all_of(result.begin(),result.end(),[](float value){return std::isfinite(value);});
}

template<class Point>
inline bool storylandVcsJetSkiChassisPose(const StorylandVcsJetSkiChassis& chassis,
    float nativeAngularZ,const std::vector<Point>& source,std::vector<Point>& result) {
    StorylandVcsJetSkiMatrix delta{};
    if(source.empty()||source.size()!=chassis.pointMask.size()||
        !storylandVcsJetSkiChassisDelta(chassis,nativeAngularZ,delta))return false;
    result=source;
    for(size_t index=0;index<result.size();++index)if(chassis.pointMask[index]) {
        result[index]=storylandVcsJetSkiTransformPoint(delta,source[index]);
        if(!std::isfinite(result[index].x)||!std::isfinite(result[index].y)||
            !std::isfinite(result[index].z))return false;
    }
    return true;
}

inline bool storylandVcsJetSkiAxesValid(const std::array<float,4>& axes) {
    return std::all_of(axes.begin(),axes.end(),[](float value) {
        return std::isfinite(value)&&value>=-1&&value<=1;
    });
}
