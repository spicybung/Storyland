#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

struct StorylandLcsStuckCarCheck {
    int32_t vehicle=-1;
    std::array<float,3> position{-5000.0f,-5000.0f,-5000.0f};
    uint32_t lastCheck=UINT32_MAX;
    float radius=0;
    uint32_t interval=0;
    bool stuck=false;

    template<class Archive> void saveState(Archive& archive) {
        archive(vehicle,position,lastCheck,radius,interval,stuck);
    }
};

template<class World>
bool storylandAddLcsStuckCarCheck(World& world,int32_t handle,float radius,
    uint32_t interval,std::string& error) {
    error.clear();
    const auto found=world.missionVehicles.find(handle);
    if(handle<0||found==world.missionVehicles.end()||found->second.deleted) {
        // Native registration ignores a handle whose pool entry has gone.
        return true;
    }
    const auto& vehicle=found->second;
    if(!std::isfinite(radius)||!std::isfinite(vehicle.x)||
        !std::isfinite(vehicle.y)||!std::isfinite(vehicle.z)) {
        error="ADD_STUCK_CAR_CHECK requires a finite radius and vehicle position.";
        return false;
    }
    const auto slot=std::find_if(world.lcsStuckCarChecks.begin(),world.lcsStuckCarChecks.end(),
        [](const auto& check){return check.vehicle<0;});
    if(slot==world.lcsStuckCarChecks.end())return true;
    slot->vehicle=handle;
    slot->position={vehicle.x,vehicle.y,vehicle.z};
    slot->lastCheck=world.gameMilliseconds;
    slot->radius=radius;
    slot->interval=interval;
    slot->stuck=false;
    return true;
}

template<class World>
void storylandRemoveLcsStuckCarCheck(World& world,int32_t handle) {
    for(auto& check:world.lcsStuckCarChecks)
        if(check.vehicle==handle)check=StorylandLcsStuckCarCheck{};
}

template<class World>
bool storylandIsLcsCarStuck(const World& world,int32_t handle) {
    for(const auto& check:world.lcsStuckCarChecks)
        if(check.vehicle==handle)return check.stuck;
    return false;
}

template<class World>
void storylandAdvanceLcsStuckCarChecks(World& world) {
    const uint32_t now=world.gameMilliseconds;
    for(auto& check:world.lcsStuckCarChecks) {
        if(check.vehicle<0||now<=uint32_t(check.lastCheck+check.interval))continue;
        const auto found=world.missionVehicles.find(check.vehicle);
        if(found==world.missionVehicles.end()||found->second.deleted) {
            check=StorylandLcsStuckCarCheck{};
            continue;
        }
        const auto& vehicle=found->second;
        const float x=vehicle.x-check.position[0];
        const float y=vehicle.y-check.position[1];
        const float z=vehicle.z-check.position[2];
        check.stuck=std::sqrt(x*x+y*y+z*z)<check.radius;
        check.position={vehicle.x,vehicle.y,vehicle.z};
        check.lastCheck=now;
    }
}
