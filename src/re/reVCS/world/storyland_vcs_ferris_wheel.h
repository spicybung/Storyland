#pragma once
#include "src/storyland/storyland_script_object.h"
#include <cstring>

namespace StorylandVcsFerrisWheel {
constexpr float pi=3.14159265358979323846f;
constexpr float radius=23.0f;

inline bool configure(StorylandMissionEnvironment& world,const std::vector<uint8_t>& bytes,
    bool vcs,std::string& error) {
    auto& wheel=world.ferrisWheel;
    if(!vcs||wheel.configured)return true;
    const auto contains=[&](size_t at,size_t length) {
        return at<=bytes.size()&&length<=bytes.size()-at;
    };
    const auto word=[&](size_t at) {
        uint32_t value=0;std::memcpy(&value,bytes.data()+at,4);return value;
    };
    if(!contains(0x38,4)||!contains(0x5C,4)) {
        error="Truncated VCS Ferris wheel registry header.";return false;
    }
    const uint32_t registry=word(0x5C),modelCount=word(0x38);
    if(registry<0x20||!contains(registry,0x136)) {
        error="Invalid VCS Ferris wheel model registry.";return false;
    }
    const auto model=[&](size_t at) {
        return uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8;
    };
    const uint32_t wheelModel=model(registry+0x132),cabinModel=model(registry+0x134);
    if(wheelModel>=modelCount||cabinModel>=modelCount||!world.objectWorld) {
        error="VCS Ferris wheel requires its native models and building pool.";return false;
    }
    wheel.wheelModel=wheelModel;wheel.cabinModel=cabinModel;
    const StorylandNativeCollisionInstance* selected=nullptr;
    for(const auto& instance:world.objectWorld->instances) {
        if(instance.poolHeader!=0x24||instance.modelId!=wheelModel)continue;
        if(!selected||instance.poolIndex<selected->poolIndex)selected=&instance;
    }
    if(selected) {
        if(!std::all_of(selected->originalMatrix.begin(),selected->originalMatrix.end(),
            [](float value){return std::isfinite(value);})||selected->originalMatrix[15]!=1) {
            error="VCS Ferris wheel has an invalid native building matrix.";return false;
        }
        wheel.worldKey=(uint64_t(selected->poolHeader)<<32)|selected->poolIndex;
        wheel.originalMatrix=wheel.wheelMatrix=selected->originalMatrix;
        wheel.wheelFound=true;
    }
    wheel.configured=true;
    error.clear();return true;
}

inline void restart(StorylandVcsFerrisWheelState& wheel) {
    if(wheel.state!=1) {wheel.state=0;wheel.stopRequested=false;}
}

inline void removeCabins(StorylandMissionEnvironment& world) {
    auto& wheel=world.ferrisWheel;
    if(!wheel.cabinsResident)return;
    for(int32_t handle:wheel.cabins)if(handle>=0)storylandDeleteScriptObject(world,handle);
    wheel.cabins.fill(-1);wheel.swingAngle.fill(0);wheel.swingSpeed.fill(0);
    wheel.cabinsResident=false;
    if(wheel.modelRequested)world.requestedModels.erase(int32_t(wheel.cabinModel));
    wheel.modelRequested=false;
}

inline bool createCabins(StorylandMissionEnvironment& world,std::string& error) {
    auto& wheel=world.ferrisWheel;
    if(wheel.cabinsResident)return true;
    if(!wheel.modelRequested) {
        world.requestedModels.insert(int32_t(wheel.cabinModel));
        wheel.modelRequested=true;
    }
    if(!world.residentModels.count(int32_t(wheel.cabinModel)))return true;
    for(size_t index=0;index<wheel.cabins.size();++index) {
        wheel.swingSpeed[index]=wheel.swingAngle[index]=0;
        if(storylandPrepareObjectPoolIndices(world)>=475)continue;
        if(world.nextObjectHandle<=0||world.nextObjectHandle==INT32_MAX) {
            error="VCS Ferris wheel cannot allocate a live object handle.";return false;
        }
        StorylandMissionEnvironment::Object object;
        object.handle=world.nextObjectHandle;
        object.model=int32_t(wheel.cabinModel);object.creationSource=5;
        object.x=wheel.wheelMatrix[12];object.y=wheel.wheelMatrix[13];object.z=wheel.wheelMatrix[14];
        object.heading=180;object.area=0;
        if(!storylandPrepareScriptObject(world,object,false,error))return false;
        wheel.cabins[index]=object.handle;
        world.objects.push_back(object);++world.nextObjectHandle;
    }
    world.requestedModels.erase(int32_t(wheel.cabinModel));
    wheel.modelRequested=false;wheel.cabinsResident=true;
    return true;
}

inline int32_t cabinHandle(const StorylandMissionEnvironment& world,size_t index) {
    if(index>=world.ferrisWheel.cabins.size())return -1;
    const int32_t handle=world.ferrisWheel.cabins[index];
    return std::any_of(world.objects.begin(),world.objects.end(),[&](const auto& object) {
        return object.handle==handle;
    })?handle:-1;
}

inline float advanceMotion(StorylandVcsFerrisWheelState& wheel,float timeStep) {
    float swingForce=0;
    const float seconds=timeStep*.02f;
    if(wheel.state==0) {
        wheel.speed+=seconds*.01f;
        if(wheel.speed>=.05f) {wheel.speed=.05f;wheel.state=1;}
        swingForce=.001f;
    } else if(wheel.state==1) {
        wheel.speed=.05f;
        if(wheel.stopRequested) {
            float phase=wheel.angle;
            if(phase<pi/6.0f)phase+=pi/6.0f;
            wheel.remainingStopAngle=pi/6.0f-std::fmod(phase,pi/6.0f);
            if(wheel.remainingStopAngle<pi/24.0f)wheel.remainingStopAngle+=pi/6.0f;
            wheel.state=2;wheel.stopRequested=false;
        }
    } else if(wheel.state==2) {
        if(wheel.remainingStopAngle<pi/24.0f) {
            wheel.speed=std::sin(wheel.remainingStopAngle*(24.0f/pi))*.05f;
            swingForce=.001f;
        }
        if(wheel.speed<=.003f) {
            wheel.speed=0;wheel.remainingStopAngle=0;wheel.state=3;
        }
    } else wheel.speed=0;
    const float delta=wheel.speed*seconds;
    if(wheel.remainingStopAngle>0)wheel.remainingStopAngle-=delta;
    wheel.angle+=delta;
    if(wheel.angle>2.0f*pi)wheel.angle-=2.0f*pi;
    const float cosine=std::cos(delta),sine=std::sin(delta);
    const auto previous=wheel.wheelMatrix;
    for(size_t axis=0;axis<3;++axis) {
        wheel.wheelMatrix[4+axis]=previous[4+axis]*cosine+previous[8+axis]*sine;
        wheel.wheelMatrix[8+axis]=previous[8+axis]*cosine-previous[4+axis]*sine;
    }
    return swingForce;
}

inline bool advance(StorylandMissionEnvironment& world,std::string& error) {
    auto& wheel=world.ferrisWheel;
    if(!wheel.configured||!wheel.wheelFound)return true;
    if(!world.applyLcsGarageDoorTransform) {
        error="VCS Ferris wheel requires the live native building transform consumer.";return false;
    }
    std::array<float,3> camera{world.x,world.y,world.z},target{};
    if(world.activeCameraVectors)world.activeCameraVectors(camera,target);
    else if(world.camera.active)camera={world.camera.x,world.camera.y,world.camera.z};
    const float x=camera[0]-wheel.wheelMatrix[12],y=camera[1]-wheel.wheelMatrix[13],
        z=camera[2]-wheel.wheelMatrix[14];
    const float distanceSquared=x*x+y*y+z*z;
    if(distanceSquared>90000.0f)removeCabins(world);
    else if(distanceSquared<75625.0f&&!createCabins(world,error))return false;
    const float timeStep=std::max(0.0f,world.nativeScriptTimeStep);
    const float force=advanceMotion(wheel,timeStep);
    StorylandMissionEnvironment::LcsGarageDoor building;
    building.worldKey=wheel.worldKey;building.model=wheel.wheelModel;
    building.originalMatrix=wheel.originalMatrix;
    if(!world.applyLcsGarageDoorTransform(building,wheel.wheelMatrix,error))return false;
    if(!wheel.cabinsResident)return true;
    for(size_t index=0;index<wheel.cabins.size();++index) {
        const auto found=std::find_if(world.objects.begin(),world.objects.end(),[&](const auto& object) {
            return object.handle==wheel.cabins[index];
        });
        if(found==world.objects.end()) {wheel.cabins[index]=-1;continue;}
        auto& object=*found;
        wheel.swingSpeed[index]=(wheel.swingSpeed[index]+force-wheel.swingAngle[index]*.1f)*.95f;
        wheel.swingAngle[index]+=wheel.swingSpeed[index]*timeStep;
        const float phase=wheel.angle+float(index)*(pi/6.0f);
        const std::array<float,3> position{wheel.wheelMatrix[12],
            wheel.wheelMatrix[13]+radius*std::sin(phase),wheel.wheelMatrix[14]+radius*std::cos(phase)};
        const std::array<float,3> previous{object.x,object.y,object.z};
        const float denominator=std::max(timeStep,1.0f);
        for(size_t axis=0;axis<3;++axis)
            object.velocity[axis]=denominator>0?(position[axis]-previous[axis])*50.0f/denominator:0;
        object.x=position[0];object.y=position[1];object.z=position[2];
        object.rotationX=wheel.swingAngle[index]*180.0f/pi;object.rotationY=0;object.heading=180;
    }
    error.clear();return true;
}
}
