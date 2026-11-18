#pragma once

#include <algorithm>
#include <cmath>

inline double storylandVideoAudioLeadSeconds(double videoStart,double audioStart) {
    if(!std::isfinite(videoStart)||!std::isfinite(audioStart)||videoStart<0||audioStart<0)return 0;
    return std::max(0.0,videoStart-audioStart);
}

inline bool storylandVideoPresentationDue(double audioPosition,double audioLead) {
    return std::isfinite(audioPosition)&&audioPosition>=std::max(0.0,audioLead);
}
