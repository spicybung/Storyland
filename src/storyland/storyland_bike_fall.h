#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

struct StorylandBikeFallDecision {
    bool eject = false;
    bool right = false;
    float damage = 0;
    int32_t animation = -1;
    float inheritVelocity = 1;
    float sideForce = 0;
    float upForce = 0;
    float forwardForce = 0;
    bool nativeAngularFall = false;
    bool nativeWaterFall = false;
};

inline int32_t storylandBikeSideFallAnimation(bool vcs,bool right,bool fast) {
    if(right)return vcs?(fast?31:27):(fast?28:24);
    return vcs?(fast?29:26):(fast?26:23);
}

inline StorylandBikeFallDecision storylandBikeCollisionFall(bool vcs, uint32_t model,
    float nativeImpulsePerMass, bool playerDriven, bool missionVehicle,
    float normalForward, float normalRight, float normalUp, float worldUp,
    float speedSquared) {
    // SLUS-21423 0x155964–0x155DA8, SLUS-21590 0x33647C–0x336C70.
    StorylandBikeFallDecision result;
    if(!std::isfinite(nativeImpulsePerMass)||nativeImpulsePerMass<=0||
        !std::isfinite(normalForward)||!std::isfinite(normalRight)||
        !std::isfinite(normalUp)||!std::isfinite(worldUp)||!std::isfinite(speedSquared))return result;
    const float collisionSpeed=800.0f*nativeImpulsePerMass*
        (playerDriven?.65f:missionVehicle?.4f:1.0f);
    if(collisionSpeed<=10)return result;
    float frontWeight=.6f,upWeight=.05f;
    if(std::fabs(normalForward)>.85f&&worldUp>=.85f)
        frontWeight+=7.0f*worldUp*worldUp;
    const bool protectedModel=vcs?(model==205||model==231||model==232):
        (model==202||model==207||model==208||model==209||model==210);
    if(protectedModel) {
        frontWeight*=.65f;
        upWeight*=.75f;
    }
    const float score=collisionSpeed*(frontWeight*std::fabs(normalForward)+
        .45f*std::fabs(normalRight)+upWeight*std::max(0.0f,normalUp)-
        1.5f*std::min(0.0f,normalUp));
    if(score<=75)return result;
    result.eject=true;
    result.damage=.05f*score;
    const float directionAngle=std::atan2(normalRight,-normalForward)+.7853981633974483f;
    const int direction=int((directionAngle<0?directionAngle+6.283185307179586f:directionAngle)*
        .6366197723675814f)%4;
    if(direction==0) {
        result.animation=121;
        result.inheritVelocity=0;
        if(speedSquared<225) {
            result.upForce=5;
            result.forwardForce=-6;
        }
        return result;
    }
    const bool fast=speedSquared>225;
    result.right=direction==3;
    result.animation=storylandBikeSideFallAnimation(vcs,result.right,fast);
    result.inheritVelocity=fast?.3f:1.0f;
    result.sideForce=(fast?6.0f:8.0f)*(result.right?-1.0f:1.0f);
    result.upForce=fast||(vcs&&result.right)?5.0f:4.0f;
    return result;
}

inline StorylandBikeFallDecision storylandVcsBikeAngularFall(
    const std::array<float,3>& nativeAngularVelocity) {
    StorylandBikeFallDecision result;
    for (float value : nativeAngularVelocity)
        if (!std::isfinite(value)) return result;
    const float xy = nativeAngularVelocity[0] * nativeAngularVelocity[0] +
        nativeAngularVelocity[1] * nativeAngularVelocity[1];
    const float squared = xy + nativeAngularVelocity[2] * nativeAngularVelocity[2];
    // SLUS-21590 33E3FC..33E49C: cause47, direction0, both seated riders.
    if (!(squared > .3f * .3f)) return result;
    result.eject = true;
    result.animation = 30;
    result.nativeAngularFall = true;
    return result;
}

// LCS angular-only falls retain the existing provisional thresholds.
inline StorylandBikeFallDecision storylandBikeSpinFall(bool vcs,bool twoWheels,float speed,
    float yawRate,float rollRate,float pitchRate,float bankDegrees,bool supported,
    bool bodyContact,bool scriptedRotation=false) {
    StorylandBikeFallDecision result;
    if(!twoWheels||!std::isfinite(speed)||!std::isfinite(yawRate)||
        !std::isfinite(rollRate)||!std::isfinite(pitchRate)||!std::isfinite(bankDegrees))return result;
    if (vcs) return storylandVcsBikeAngularFall({rollRate / 50.0f,
        pitchRate / 50.0f, yawRate / 50.0f});
    const bool freeRotation=!supported||scriptedRotation;
    const bool yawSpin=std::fabs(yawRate)>=4.1887902048f;
    const bool rollSpin=std::fabs(rollRate)>=4&&(freeRotation||bodyContact||std::fabs(bankDegrees)>35);
    const bool pitchSpin=std::fabs(pitchRate)>=4&&freeRotation;
    const bool overturned=bodyContact&&std::fabs(bankDegrees)>=85;
    if((std::fabs(speed)<3&&supported)||!(yawSpin||rollSpin||pitchSpin||overturned))return result;
    result.eject=true;
    result.right=rollSpin?rollRate<0:overturned?bankDegrees<0:yawRate>0;
    result.animation=pitchSpin&&!rollSpin&&!yawSpin?121:
        storylandBikeSideFallAnimation(vcs,result.right,std::fabs(speed)>15);
    result.sideForce=result.animation==121?0:result.right?-4:4;
    result.upForce=4;
    return result;
}
