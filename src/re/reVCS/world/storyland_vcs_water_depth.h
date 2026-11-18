#pragma once

inline bool storylandVcsWaterDepthAllowsContact(float worldZ,float& waterHeight) {
    if(waterHeight-worldZ>50.0f) {
        waterHeight=0;
        return false;
    }
    return true;
}
