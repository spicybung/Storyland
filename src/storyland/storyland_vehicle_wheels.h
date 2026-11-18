#pragma once
#include "src/leeds/leeds_model.h"
#include "src/leeds/leeds_vehicle.h"
#include <cmath>
#include <algorithm>
#include <array>
#include <string>

inline StorylandModelPoint storylandAnimateVehicleWheel(const StorylandModelPoint& point,
    const StorylandModelPoint& pivot,float rollingAngle,float steeringAngle) {
    const float x=point.x-pivot.x,y=point.y-pivot.y,z=point.z-pivot.z;
    const float cosine=std::cos(-rollingAngle),sine=std::sin(-rollingAngle);
    const float rolledY=y*cosine-z*sine,rolledZ=y*sine+z*cosine;
    const float steering=steeringAngle*0.0174532925199433f;
    const float steerCosine=std::cos(steering),steerSine=std::sin(steering);
    StorylandModelPoint result=point;
    result.x=pivot.x+x*steerCosine-rolledY*steerSine;
    result.y=pivot.y+x*steerSine+rolledY*steerCosine;
    result.z=pivot.z+rolledZ;
    const float normalY=point.ny*cosine-point.nz*sine;
    result.nx=point.nx*steerCosine-normalY*steerSine;
    result.ny=point.nx*steerSine+normalY*steerCosine;
    result.nz=point.ny*sine+point.nz*cosine;
    const float ps2NormalY=point.ps2Ny*cosine-point.ps2Nz*sine;
    result.ps2Nx=point.ps2Nx*steerCosine-ps2NormalY*steerSine;
    result.ps2Ny=point.ps2Nx*steerSine+ps2NormalY*steerCosine;
    result.ps2Nz=point.ps2Ny*sine+point.ps2Nz*cosine;
    return result;
}

inline float storylandDecodedWheelRadius(const std::vector<StorylandModelPoint>& points,
    const std::vector<StorylandModelTriangle>& triangles) {
    float radius=0;
    for(const auto& triangle:triangles) {
        if(!triangle.vehicleWheel)continue;
        for(uint32_t index:{triangle.a,triangle.b,triangle.c}) {
            if(index>=points.size())continue;
            const float distance=std::hypot(points[index].y-triangle.vehiclePivot.y,points[index].z-triangle.vehiclePivot.z);
            if(std::isfinite(distance)&&distance>=0.15f&&distance<=1.5f)radius=std::max(radius,distance);
        }
    }
    return radius;
}

inline void storylandSetupVehicleTyreGeometry(StorylandVehicleDefinition& definition,
    const StorylandModelFile& model,bool sharedWheel=false) {
    const auto setup=[&](StorylandVehicleWheelDefinition& wheel) {
        auto& geometry=wheel.tyreContactGeometry;
        geometry.clear();wheel.tyreUsesWheelFrame=sharedWheel;
        const bool mirrored=sharedWheel&&wheel.previewPivot[0]<definition.previewRoot[12];
        const float scale=wheel.front?definition.wheelScale:definition.rearWheelScale;
        for(const auto& triangle:model.previewTriangles()) {
            if(!sharedWheel&&(!triangle.vehicleWheel||
                std::fabs(triangle.vehiclePivot.x-wheel.previewPivot[0])+
                std::fabs(triangle.vehiclePivot.y-wheel.previewPivot[1])+
                std::fabs(triangle.vehiclePivot.z-wheel.previewPivot[2])>=.0001f))continue;
            for(uint32_t index:{triangle.a,triangle.b,triangle.c}) {
                if(index>=model.previewPoints().size())continue;
                const auto& point=model.previewPoints()[index];
                std::array<float,3> local{point.x,point.y,point.z};
                if(sharedWheel) {
                    for(float& axis:local)axis*=scale;
                    if(mirrored) {local[0]=-local[0];local[1]=-local[1];}
                } else {
                    for(size_t axis=0;axis<3;++axis)local[axis]-=wheel.previewPivot[axis];
                }
                if(std::all_of(local.begin(),local.end(),[](float value){return std::isfinite(value);}))
                    geometry.push_back(local);
            }
        }
        std::sort(geometry.begin(),geometry.end());
        geometry.erase(std::unique(geometry.begin(),geometry.end()),geometry.end());
    };
    for(auto& wheel:definition.wheels)setup(wheel);
    for(auto& wheel:definition.auxiliaryWheels)setup(wheel);
}

inline float storylandVehicleTyreExtent(const StorylandVehicleWheelDefinition& wheel,
    const std::array<float,16>& bodyFrame,const std::array<float,3>& normal,
    float rollingAngle=0,float steeringAngle=0,bool burst=false) {
    std::array<float,3> local{};
    for(size_t axis=0;axis<3;++axis)
        local[axis]=bodyFrame[axis*4]*normal[0]+bodyFrame[axis*4+1]*normal[1]+
            bodyFrame[axis*4+2]*normal[2];
    if(wheel.tyreUsesWheelFrame) {
        const auto bodyNormal=local;
        for(size_t axis=0;axis<3;++axis)
            local[axis]=wheel.previewFrame[axis*4]*bodyNormal[0]+
                wheel.previewFrame[axis*4+1]*bodyNormal[1]+wheel.previewFrame[axis*4+2]*bodyNormal[2];
    } else if(burst) {
        local[1]*=.7f;local[2]*=.7f;
    }
    const float steering=steeringAngle*.01745329251994329577f;
    const float steerCosine=std::cos(steering),steerSine=std::sin(steering);
    const float steeredX=steerCosine*local[0]+steerSine*local[1];
    const float steeredY=-steerSine*local[0]+steerCosine*local[1];
    const float rollCosine=std::cos(rollingAngle),rollSine=std::sin(rollingAngle);
    local={steeredX,rollCosine*steeredY-rollSine*local[2],
        rollSine*steeredY+rollCosine*local[2]};
    if(wheel.tyreUsesWheelFrame&&burst) {local[1]*=.7f;local[2]*=.7f;}
    if(wheel.tyreContactGeometry.empty())return wheel.radius*std::hypot(local[1],local[2]);
    float extent=0;
    for(const auto& point:wheel.tyreContactGeometry)
        extent=std::max(extent,-local[0]*point[0]-local[1]*point[1]-local[2]*point[2]);
    return extent;
}

// Native automobile door hinges: left -72 degrees, right +72 degrees.
inline StorylandModelPoint storylandAnimateVehicleDoor(const StorylandModelPoint& point,
    const StorylandModelPoint& pivot,const std::string& part,const std::array<float,4>& ratios) {
    int door=-1;
    if(part.find("door_lf")!=std::string::npos)door=0;
    else if(part.find("door_rf")!=std::string::npos)door=1;
    else if(part.find("door_lr")!=std::string::npos)door=2;
    else if(part.find("door_rr")!=std::string::npos)door=3;
    if(door<0)return point;
    const float angle=(door%2?1.0f:-1.0f)*1.2566370614359173f*std::clamp(ratios[size_t(door)],0.0f,1.0f);
    const float x=point.x-pivot.x,y=point.y-pivot.y,c=std::cos(angle),s=std::sin(angle);
    StorylandModelPoint result=point;
    result.x=pivot.x+x*c-y*s;
    result.y=pivot.y+x*s+y*c;
    result.nx=point.nx*c-point.ny*s;
    result.ny=point.nx*s+point.ny*c;
    result.ps2Nx=point.ps2Nx*c-point.ps2Ny*s;
    result.ps2Ny=point.ps2Nx*s+point.ps2Ny*c;
    return result;
}

inline int storylandVehicleDoorPart(const std::string& part) {
    if (part.find("bonnet") != std::string::npos) return 0;
    if (part.find("boot") != std::string::npos) return 1;
    if (part.find("door_lf") != std::string::npos) return 2;
    if (part.find("door_rf") != std::string::npos) return 3;
    if (part.find("door_lr") != std::string::npos) return 4;
    if (part.find("door_rr") != std::string::npos) return 5;
    return -1;
}

inline bool storylandVehiclePartVisible(const std::string& part, bool damagedPart,
    bool hasDamagedVariant, bool visiblyDamaged, const std::array<uint8_t,6>& doors,
    const std::array<uint8_t,7>* panels=nullptr) {
    const int door = storylandVehicleDoorPart(part);
    bool damaged = visiblyDamaged;
    if (door >= 0) {
        const uint8_t status = doors[size_t(door)];
        if (status == 3) return false;
        damaged = status == 1 || status == 4;
    } else if (panels) {
        int panel = -1;
        if (part.find("wing_lf") != std::string::npos) panel = 0;
        else if (part.find("wing_rf") != std::string::npos) panel = 1;
        else if (part.find("wing_lr") != std::string::npos) panel = 2;
        else if (part.find("wing_rr") != std::string::npos) panel = 3;
        else if (part.find("windscreen") != std::string::npos) panel = 4;
        else if (part.find("bump_front") != std::string::npos || part.find("bump_f") != std::string::npos) panel = 5;
        else if (part.find("bump_rear") != std::string::npos || part.find("bump_r") != std::string::npos) panel = 6;
        if (panel >= 0) {
            const uint8_t status = (*panels)[size_t(panel)];
            if (status == 3) return false;
            damaged = status != 0;
        }
    }
    if (damagedPart) return damaged;
    return !damaged || !hasDamagedVariant;
}

inline StorylandModelPoint storylandAnimateAircraftRotor(const StorylandModelPoint& point,
    const StorylandModelPoint& pivot, const std::string& part, float angle) {
    if (part.find("rotor") == std::string::npos && part.find("prop") == std::string::npos) return point;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float x = point.x - pivot.x;
    const float y = point.y - pivot.y;
    const float z = point.z - pivot.z;
    StorylandModelPoint result=point;
    if (part.find("rearrotor") != std::string::npos || part.find("tail_rotor") != std::string::npos ||
        part.find("rotor2") != std::string::npos) {
        result.y=pivot.y+y*cosine-z*sine;
        result.z=pivot.z+y*sine+z*cosine;
        result.ny=point.ny*cosine-point.nz*sine;
        result.nz=point.ny*sine+point.nz*cosine;
        result.ps2Ny=point.ps2Ny*cosine-point.ps2Nz*sine;
        result.ps2Nz=point.ps2Ny*sine+point.ps2Nz*cosine;
    } else if (part.find("prop") != std::string::npos) {
        result.x=pivot.x+x*cosine+z*sine;
        result.z=pivot.z-x*sine+z*cosine;
        result.nx=point.nx*cosine+point.nz*sine;
        result.nz=-point.nx*sine+point.nz*cosine;
        result.ps2Nx=point.ps2Nx*cosine+point.ps2Nz*sine;
        result.ps2Nz=-point.ps2Nx*sine+point.ps2Nz*cosine;
    } else {
        result.x=pivot.x+x*cosine-y*sine;
        result.y=pivot.y+x*sine+y*cosine;
        result.nx=point.nx*cosine-point.ny*sine;
        result.ny=point.nx*sine+point.ny*cosine;
        result.ps2Nx=point.ps2Nx*cosine-point.ps2Ny*sine;
        result.ps2Ny=point.ps2Nx*sine+point.ps2Ny*cosine;
    }
    return result;
}

inline StorylandModelPoint storylandAnimateVehiclePanel(const StorylandModelPoint& point,
    const StorylandModelPoint& pivot, const std::string& part,
    const std::array<float,4>& doors, float bonnet, float boot) {
    const int panel = storylandVehicleDoorPart(part);
    if (panel != 0 && panel != 1) {
        return storylandAnimateVehicleDoor(point, pivot, part, doors);
    }
    const float opening = panel == 0 ? bonnet : -boot;
    const float angle = std::clamp(opening, -1.0f, 1.0f) * 1.2566370614359173f;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float y = point.y - pivot.y;
    const float z = point.z - pivot.z;
    StorylandModelPoint result=point;
    result.y=pivot.y+y*cosine-z*sine;
    result.z=pivot.z+y*sine+z*cosine;
    result.ny=point.ny*cosine-point.nz*sine;
    result.nz=point.ny*sine+point.nz*cosine;
    result.ps2Ny=point.ps2Ny*cosine-point.ps2Nz*sine;
    result.ps2Nz=point.ps2Ny*sine+point.ps2Nz*cosine;
    return result;
}

inline size_t storylandVehicleTyreIndex(bool twoWheels, bool front, bool left) {
    if (twoWheels) return front ? 0 : 1;
    return left ? (front ? 0 : 1) : (front ? 2 : 3);
}

inline StorylandModelPoint storylandAnimateBurstTyre(const StorylandModelPoint& point,
    const StorylandModelPoint& pivot, uint8_t status) {
    if (status == 0) return point;
    StorylandModelPoint result=point;
    result.y=pivot.y+(point.y-pivot.y)*0.7f;
    result.z=pivot.z+(point.z-pivot.z)*0.7f;
    return result;
}
