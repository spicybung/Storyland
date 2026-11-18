#pragma once
#include <cmath>
#include <cstdint>
#include <limits>

inline uint32_t storylandRadioSourcePosition(double startSeconds, uint32_t elapsedMilliseconds,
    double durationSeconds, bool looping) {
    if(!std::isfinite(startSeconds) || startSeconds<0) startSeconds=0;
    double position=startSeconds*1000.0+double(elapsedMilliseconds);
    if(looping && std::isfinite(durationSeconds) && durationSeconds>0)
        position=std::fmod(position,durationSeconds*1000.0);
    const double maximum=double(std::numeric_limits<uint32_t>::max());
    if(!std::isfinite(position) || position>=maximum) return std::numeric_limits<uint32_t>::max();
    return uint32_t(position);
}
