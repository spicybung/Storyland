#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/leeds/leeds_native_world.h"
#include "src/re/reVCS/camera/storyland_vcs_remote_camera.h"
#include <algorithm>
#include <cmath>

namespace StorylandVcsVehicleCamera {
using Point=StorylandNativePoint;
constexpr float pi=3.14159265358979323846f;

inline Point add(Point a,Point b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Point subtract(Point a,Point b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Point multiply(Point a,float scale) {return {a.x*scale,a.y*scale,a.z*scale};}
inline float dot(Point a,Point b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Point cross(Point a,Point b) {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
inline bool finite(Point value) {
    return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);
}
inline bool normalize(Point& value) {
    const float length=std::sqrt(dot(value,value));
    if(!std::isfinite(length)||length<1e-7f)return false;
    value=multiply(value,1.0f/length);return true;
}
inline Point transform(const std::array<float,16>& frame,Point point) {
    return {frame[12]+frame[0]*point.x+frame[4]*point.y+frame[8]*point.z,
        frame[13]+frame[1]*point.x+frame[5]*point.y+frame[9]*point.z,
        frame[14]+frame[2]*point.x+frame[6]*point.y+frame[10]*point.z};
}
inline float angleDifference(float target,float current) {
    return std::remainder(target-current,2.0f*pi);
}

inline void bufferAngle(float target,float timeStep,float speedFactor,float acceleration,
    float& angle,float& speed) {
    const float desired=angleDifference(target,angle)*speedFactor;
    speed+=(desired-speed)*acceleration*timeStep;
    if((desired<0&&speed<desired)||(desired>0&&speed>desired))speed=desired;
    angle+=speed*std::min(timeStep,10.0f);
}

inline bool validate(const StorylandMissionEnvironment& world,int32_t handle,int32_t mode,
    std::string& error) {
    const auto vehicle=world.missionVehicles.find(handle);
    if(vehicle==world.missionVehicles.end()||vehicle->second.deleted)return false;
    std::array<float,16> frame{};
    if(!world.vehicleWorldMatrix||!world.vehicleWorldMatrix(handle,frame)||
        !std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);})) {
        error="Vehicle camera requires the live vehicle matrix.";return false;
    }
    if(mode==14) {
        if(!world.objectWorld||!world.vehicleNativeType||!world.vehicleHandlingClass||
            !world.objectWorld->collisionModels.count(uint32_t(vehicle->second.model))) {
            error="Wheel camera requires native collision bounds and vehicle classification.";return false;
        }
        const int type=world.vehicleNativeType(handle),handling=world.vehicleHandlingClass(handle);
        if(type<0||handling<0) {error="Wheel camera could not resolve the native vehicle type.";return false;}
        if((type==1||type==2)&&handling!=3&&handling!=5&&vehicle->second.driver>=0) {
            std::array<float,16> head{};
            if(!world.characterBoneMatrix||!world.characterBoneMatrix(vehicle->second.driver,5,head)) {
                error="Boat camera requires its driver's native head bone.";return false;
            }
        }
    } else if(mode==16) {
        std::array<float,3> seat{};
        if(!world.vehicleNativeType||world.vehicleNativeType(handle)!=0||
            !world.nativeVehicleCameraSeat||!world.nativeVehicleCameraSeat(vehicle->second.model,seat)) {
            error="Remote automobile camera requires its original native seat dummy.";return false;
        }
        if(!std::isfinite(world.vcsRemoteCameraUpsideDownOffset)||
            world.vcsRemoteCameraUpsideDownOffset<0||world.vcsRemoteCameraUpsideDownOffset>.53f) {
            error="Remote automobile camera has invalid native height state.";return false;
        }
    }
    return true;
}

inline bool update(StorylandMissionEnvironment& world,std::string& error) {
    const auto& focus=world.cameraFocus;
    const auto vehicle=world.missionVehicles.find(focus.handle);
    if(focus.kind!=3||vehicle==world.missionVehicles.end()||vehicle->second.deleted||
        (focus.mode!=3&&focus.mode!=14&&focus.mode!=16)) {
        error="Vehicle camera has no live native target.";return false;
    }
    std::array<float,16> frame{};
    if(!world.vehicleWorldMatrix||!world.vehicleWorldMatrix(focus.handle,frame)||
        !std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);})) {
        error="Vehicle camera requires the live vehicle matrix.";return false;
    }
    const auto& car=vehicle->second;
    auto& camera=world.camera;
    if(camera.vehicleUpdated&&camera.vehicleUpdatedAt==world.gameMilliseconds)return true;
    const Point position{frame[12],frame[13],frame[14]};
    const Point vehicleRight{frame[0],frame[1],frame[2]};
    const Point vehicleForward{frame[4],frame[5],frame[6]};
    const Point vehicleUp{frame[8],frame[9],frame[10]};
    const float timeStep=std::max(0.0f,world.nativeScriptTimeStep);
    Point eye{camera.x,camera.y,camera.z},target=position,up{0,0,1};
    if(focus.mode==3) {
        target.z+=.6f;
        Point delta=subtract(eye,target);
        const float distance=std::max(.002f,std::hypot(delta.x,delta.y));
        const float radius=std::clamp(distance,8.5f,9.95f);
        if(distance!=radius) {
            eye.x=target.x+delta.x*radius/distance;
            eye.y=target.y+delta.y*radius/distance;
        }
        eye.z=target.z+9.95f*std::sin(25.0f*pi/180.0f);
        float beta=std::atan2(target.y-eye.y,target.x-eye.x);
        float heading=std::atan2(vehicleForward.y,vehicleForward.x);
        std::array<float,3> velocity=car.velocity;
        if(world.vehicleWorldVelocity&&!world.vehicleWorldVelocity(focus.handle,velocity)) {
            error="Vehicle chase camera could not resolve its velocity.";return false;
        }
        const Point movement{velocity[0],velocity[1],velocity[2]};
        const bool movingForward=dot(vehicleForward,movement)>1.0f;
        float speedFactor=.15f,acceleration=.007f;
        if(car.model==-995||car.model==275) {
            speedFactor=.1f;acceleration=.003f;
            if(std::hypot(movement.x,movement.y)>3.14f)
                heading=std::atan2(movement.y,movement.x);
        }
        if(std::fabs(angleDifference(heading,beta))>20.0f*pi/180.0f&&
            movingForward&&!camera.transitioning)camera.vehicleAligning=true;
        if(camera.vehicleAligning||world.cameraBehindPlayerRequested||world.cameraFrontPlayerRequested) {
            bufferAngle(heading,timeStep,speedFactor,acceleration,beta,camera.vehicleBetaSpeed);
            if(world.cameraBehindPlayerRequested)beta=heading;
            if(world.cameraFrontPlayerRequested)beta=heading+pi;
            const float horizontal=std::hypot(eye.x-target.x,eye.y-target.y);
            eye.x=target.x-horizontal*std::cos(beta);
            eye.y=target.y-horizontal*std::sin(beta);
            if(std::fabs(angleDifference(heading,beta))<2.0f*pi/180.0f)camera.vehicleAligning=false;
            world.cameraBehindPlayerRequested=false;world.cameraFrontPlayerRequested=false;
        }
        if(world.objectSegmentCollision) {
            const float hit=world.objectSegmentCollision({target.x,target.y,target.z},{eye.x,eye.y,eye.z});
            if(std::isfinite(hit)&&hit>=0&&hit<1)eye=add(target,multiply(subtract(eye,target),hit));
        }
    } else if(focus.mode==16) {
        std::array<float,3> seat{};
        if(!world.vehicleNativeType||world.vehicleNativeType(focus.handle)!=0||
            !world.nativeVehicleCameraSeat||!world.nativeVehicleCameraSeat(car.model,seat)) {
            error="Remote automobile camera requires its original native seat dummy.";return false;
        }
        StorylandVcsRemoteCameraFrame native;
        float offset=world.vcsRemoteCameraUpsideDownOffset;
        if(offset<0||offset>.53f||!storylandVcsAutomobileRemoteCamera(frame,seat,false,offset,native)) {
            error="Remote automobile camera could not resolve its original native transform.";return false;
        }
        eye={native.eye[0],native.eye[1],native.eye[2]};
        target={native.eye[0]+native.forward[0],native.eye[1]+native.forward[1],
            native.eye[2]+native.forward[2]};
        up={native.up[0],native.up[1],native.up[2]};
        world.vcsRemoteCameraUpsideDownOffset=offset;
    } else {
        if(!world.objectWorld||!world.vehicleNativeType||!world.vehicleHandlingClass) {
            error="Wheel camera requires native collision bounds and vehicle classification.";return false;
        }
        const auto collision=world.objectWorld->collisionModels.find(uint32_t(car.model));
        if(collision==world.objectWorld->collisionModels.end()) {
            error="Wheel camera could not resolve the vehicle collision bounds.";return false;
        }
        const int32_t type=world.vehicleNativeType(focus.handle);
        const int32_t handling=world.vehicleHandlingClass(focus.handle);
        if(type<0||handling<0) {
            error="Wheel camera could not resolve the native vehicle type.";return false;
        }
        const float minimumX=collision->second.minimum.x;
        Point local{minimumX-.33f,-2.3f,.3f};
        Point forward=vehicleForward,right=vehicleRight;
        eye=transform(frame,local);
        if(handling==3||handling==5) {
            local.x=-1.55f;eye=transform(frame,local);up=vehicleUp;
        } else if(type==1||type==2) {
            right=cross(forward,{0,0,1});
            if(!normalize(right)) {error="Boat camera has a degenerate vehicle basis.";return false;}
            up=cross(right,forward);
            if(!normalize(up)) {error="Boat camera has a degenerate vertical axis.";return false;}
            if(car.driver>=0) {
                std::array<float,16> head{};
                if(!world.characterBoneMatrix||!world.characterBoneMatrix(car.driver,5,head)) {
                    error="Boat camera requires its driver's native head bone.";return false;
                }
                std::array<float,3> velocity=car.velocity;
                if(world.vehicleWorldVelocity&&!world.vehicleWorldVelocity(focus.handle,velocity)) {
                    error="Boat camera could not resolve its velocity.";return false;
                }
                eye={head[12]+velocity[0]*timeStep/50.0f,
                    head[13]+velocity[1]*timeStep/50.0f,head[14]+velocity[2]*timeStep/50.0f};
                eye=add(add(eye,multiply(right,-.5f)),multiply(forward,-.8f));eye.z+=.3f;
                if(car.model==219) {
                    eye=add(add(eye,multiply(right,.2f)),multiply(forward,-.2f));eye.z-=.3f;
                }
            } else eye.z+=.6f;
        } else if(type==6) {
            forward=cross({0,0,1},right);
            if(!normalize(forward)) {error="Motorcycle camera has a degenerate forward axis.";return false;}
            up=cross(right,forward);
            if(!normalize(up)) {error="Motorcycle camera has a degenerate vertical axis.";return false;}
            eye=add(add(add(position,multiply(right,minimumX-.2f)),multiply(forward,-2.3f)),multiply(up,.3f));
        } else {
            right=cross(forward,{0,0,1});
            if(!normalize(right)) {error="Wheel camera has a degenerate vehicle basis.";return false;}
            up=cross(right,forward);
            if(!normalize(up)) {error="Wheel camera has a degenerate vertical axis.";return false;}
        }
        const float angle=.4f*std::cos(float(world.gameMilliseconds&0x1FFFFu)*(pi/65536.0f));
        up=add(multiply(up,std::cos(angle)),multiply(right,std::sin(angle)));
        target=add(eye,forward);
    }
    if(!finite(eye)||!finite(target)||!finite(up)) {
        error="Vehicle camera produced an invalid native transform.";return false;
    }
    camera.x=eye.x;camera.y=eye.y;camera.z=eye.z;
    camera.targetX=target.x;camera.targetY=target.y;camera.targetZ=target.z;
    camera.upX=up.x;camera.upY=up.y;camera.upZ=up.z-1.0f;
    camera.positionSet=true;camera.targetSet=true;camera.fieldOfView=focus.mode==16?60:70;camera.roll=0;
    camera.vehicleUpdatedAt=world.gameMilliseconds;camera.vehicleUpdated=true;
    error.clear();return true;
}
}
