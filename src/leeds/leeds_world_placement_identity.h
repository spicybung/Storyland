#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

inline std::array<uint32_t,17> storylandWorldPlacementKey(uint32_t ipl,uint32_t resource,
    uint32_t pass,uint32_t boundsXY,uint32_t boundsZR,const float* matrix,
    const std::array<float,3>& origin) {
    std::array<uint32_t,17> key{ipl,resource,pass,boundsXY,boundsZR};
    size_t cursor=5;
    for(size_t column=0;column<3;++column)for(size_t axis=0;axis<3;++axis) {
        const float value=matrix[column*4+axis]==0?0:matrix[column*4+axis];
        std::memcpy(&key[cursor++],&value,sizeof(value));
    }
    for(size_t axis=0;axis<3;++axis) {
        const float sum=matrix[12+axis]+origin[axis];
        const float value=sum==0?0:sum;
        std::memcpy(&key[cursor++],&value,sizeof(value));
    }
    return key;
}

using StorylandWorldPositionIntervals = std::array<std::array<double,2>,3>;

inline float storylandLcsSerializedSectorOriginY(uint32_t row) {
    return float(-2000.0+double(86.6f)*(double(row)+0.5));
}

inline StorylandWorldPositionIntervals storylandWorldFloat32PositionIntervals(
    const float* matrix, const std::array<float,3>& origin) {
    StorylandWorldPositionIntervals intervals{};
    for(size_t axis=0;axis<3;++axis) {
        const float value=matrix[12+axis];
        const float previous=std::nextafter(value,-std::numeric_limits<float>::infinity());
        const float next=std::nextafter(value,std::numeric_limits<float>::infinity());
        intervals[axis]={double(origin[axis])+(double(previous)+double(value))*0.5,
            double(origin[axis])+(double(value)+double(next))*0.5};
        uint32_t bits=0;
        std::memcpy(&bits,&value,sizeof(bits));
        if(bits&1u) {
            // Midpoints round to the neighbouring even significand.
            intervals[axis][0]=std::nextafter(intervals[axis][0],
                std::numeric_limits<double>::infinity());
            intervals[axis][1]=std::nextafter(intervals[axis][1],
                -std::numeric_limits<double>::infinity());
        }
    }
    return intervals;
}

inline bool storylandIntersectWorldPositionIntervals(StorylandWorldPositionIntervals& accumulated,
    const StorylandWorldPositionIntervals& candidate) {
    for(size_t axis=0;axis<3;++axis) {
        if(!(accumulated[axis][0]<=accumulated[axis][1]&&
            candidate[axis][0]<=candidate[axis][1]&&
            accumulated[axis][0]<=candidate[axis][1]&&
            candidate[axis][0]<=accumulated[axis][1]))return false;
    }
    for(size_t axis=0;axis<3;++axis) {
        if(candidate[axis][0]>accumulated[axis][0])accumulated[axis][0]=candidate[axis][0];
        if(candidate[axis][1]<accumulated[axis][1])accumulated[axis][1]=candidate[axis][1];
    }
    return true;
}
