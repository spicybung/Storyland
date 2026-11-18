#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

struct StorylandVcsBulletCameraFrame {
    std::array<float,3> position{}, right{}, forward{}, up{};
};

struct StorylandVcsBulletPass {
    uint16_t event=0;
    uint8_t volume=0;
};

inline bool storylandVcsBulletTraceAllowed(bool playerShooter,int cameraMode,float speedPerSecond) {
    if(!playerShooter)return true;
    switch(cameraMode) {
    case 7:case 8:case 34:case 39:case 40:case 42:case 45:case 46:
        return speedPerSecond>=2.5f;
    default:return true;
    }
}

inline bool storylandVcsBulletCameraFrame(const std::array<float,3>& eye,
    const std::array<float,3>& target,const std::array<float,3>& upHint,float roll,
    StorylandVcsBulletCameraFrame& result) {
    auto normalize=[](std::array<float,3>& value) {
        const float length=std::hypot(value[0],value[1],value[2]);
        if(!(length>.00001f)||!std::isfinite(length))return false;
        for(float& component:value)component/=length;
        return true;
    };
    result.position=eye;
    result.forward={target[0]-eye[0],target[1]-eye[1],target[2]-eye[2]};
    if(!normalize(result.forward))return false;
    const auto& f=result.forward;
    result.right={f[1]*upHint[2]-f[2]*upHint[1],f[2]*upHint[0]-f[0]*upHint[2],
        f[0]*upHint[1]-f[1]*upHint[0]};
    if(!normalize(result.right))return false;
    const auto& r=result.right;
    result.up={r[1]*f[2]-r[2]*f[1],r[2]*f[0]-r[0]*f[2],r[0]*f[1]-r[1]*f[0]};
    if(roll!=0&&std::isfinite(roll)) {
        const float angle=roll*.0174532925199433f,c=std::cos(angle),s=std::sin(angle);
        for(std::size_t axis=0;axis<3;++axis)result.up[axis]=result.up[axis]*c-result.right[axis]*s;
        result.right={f[1]*result.up[2]-f[2]*result.up[1],f[2]*result.up[0]-f[0]*result.up[2],
            f[0]*result.up[1]-f[1]*result.up[0]};
    }
    return true;
}

inline bool storylandVcsBulletPass(const StorylandVcsBulletCameraFrame& camera,
    const std::array<float,3>& start,const std::array<float,3>& end,
    StorylandVcsBulletPass& result) {
    result={};
    const auto local=[&](const std::array<float,3>& point,const std::array<float,3>& axis) {
        return (point[0]-camera.position[0])*axis[0]+(point[1]-camera.position[1])*axis[1]+
            (point[2]-camera.position[2])*axis[2];
    };
    const float startForward=local(start,camera.forward),endForward=local(end,camera.forward);
    if(!std::isfinite(startForward)||!std::isfinite(endForward)||!(startForward*endForward<0))return false;
    const float amount=std::fabs(startForward)/(std::fabs(startForward)+std::fabs(endForward));
    const float startRight=local(start,camera.right),endRight=local(end,camera.right);
    const float startUp=local(start,camera.up),endUp=local(end,camera.up);
    const float right=startRight+amount*(endRight-startRight),up=startUp+amount*(endUp-startUp);
    const float distance=std::sqrt(right*right+up*up);
    if(!std::isfinite(distance)||!(distance<2.0f))return false;
    result.event=right<0?186:185;
    result.volume=uint8_t((1.0f-distance*.5f)*127.0f);
    return true;
}
