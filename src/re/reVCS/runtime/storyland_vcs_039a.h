#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

inline bool storylandVcs039aTurnVelocity(const std::array<float,16>& matrix,
    const std::array<float,3>& center,const std::array<float,3>& character,
    float characterMass,float turnMass,uint32_t random,std::array<float,3>& angular) {
    for(float value:matrix)if(!std::isfinite(value))return false;
    for(float value:center)if(!std::isfinite(value))return false;
    for(float value:character)if(!std::isfinite(value))return false;
    for(float value:angular)if(!std::isfinite(value))return false;
    if(!std::isfinite(characterMass)||characterMass<=0||!std::isfinite(turnMass)||turnMass<=0)return false;
    const float sample=float(random)*0x1p-31f;
    const float vertical=(sample*(-0.400000035762786865234375f)-0.8f)*characterMass;
    const float centerX=matrix[0]*center[0]+matrix[4]*center[1]+matrix[8]*center[2];
    const float centerY=matrix[1]*center[0]+matrix[5]*center[1]+matrix[9]*center[2];
    const float armX=(character[0]-matrix[12])-centerX;
    const float armY=(character[1]-matrix[13])-centerY;
    const float inverse=1.0f/turnMass;
    angular[0]=std::clamp(angular[0]+(armY*vertical)*inverse,-4.0f,4.0f);
    angular[1]=std::clamp(angular[1]+(-armX*vertical)*inverse,-4.0f,4.0f);
    angular[2]=std::clamp(angular[2],-4.0f,4.0f);
    return true;
}
