#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <cmath>

inline bool storylandDecodePedestrianGroups(const std::vector<uint8_t>& data,bool vcs,
    std::vector<std::array<uint32_t,16>>& result,std::string& error) {
    result.clear();error.clear();
    const auto word=[&](size_t offset)->uint32_t {
        return uint32_t(data[offset])|uint32_t(data[offset+1])<<8|
            uint32_t(data[offset+2])<<16|uint32_t(data[offset+3])<<24;
    };
    if(data.size()<(vcs?0xBCu:0xB8u)) {error="Truncated pedestrian group header.";return false;}
    const size_t count=vcs?71:37;
    const uint32_t table=word(vcs?0xB8:0xB4),models=word(0x3C),modelCount=word(0x38);
    if(!table||uint64_t(table)+count*16u*4u>data.size()||!models||models>=data.size()) {
        error="Invalid pedestrian group table.";return false;
    }
    std::vector<std::array<uint32_t,16>> groups(count);
    for(size_t group=0;group<groups.size();++group)for(size_t slot=0;slot<16;++slot) {
        const uint32_t id=word(size_t(table)+(group*16+slot)*4);
        if(id>=modelCount||uint64_t(models)+uint64_t(id)*4+4>data.size()) {
            error="Invalid pedestrian group model index.";return false;
        }
        const uint32_t model=word(size_t(models)+size_t(id)*4);
        if(!model||uint64_t(model)+0x38>data.size()||data[size_t(model)+0x10]!=7) {
            error="Pedestrian group references a non-pedestrian model.";return false;
        }
        groups[group][slot]=id;
    }
    result=std::move(groups);return true;
}

inline bool storylandDecodeLcsPedestrianGroups(const std::vector<uint8_t>& data,
    std::vector<std::array<uint32_t,16>>& result,std::string& error) {
    return storylandDecodePedestrianGroups(data,false,result,error);
}

struct StorylandLcsPopulation {
    bool available=false;
    bool vcs=false;
    StorylandNativeZonePopulation values;
    StorylandVcsZonePopulationRecord vcsValues;
};

inline int32_t storylandLcsPopulationShort(int64_t value) {
    const uint16_t narrowed=uint16_t(value);
    return narrowed<0x8000u?int32_t(narrowed):int32_t(narrowed)-0x10000;
}

inline StorylandLcsPopulation storylandLcsPopulationAt(const StorylandMissionEnvironment& world,
    const StorylandNativePoint& position,float hour) {
    StorylandLcsPopulation result;
    if(!world.zones||world.zones->vcs||!std::isfinite(hour))return result;
    const auto& zones=*world.zones;
    const auto* zone=zones.at(position,true);
    if(!zone||zone->populationDay>=zones.populations.size()||zone->populationNight>=zones.populations.size())return result;
    auto day=zones.populations[zone->populationDay],night=zones.populations[zone->populationNight];
    const auto apply=[&](StorylandNativeZonePopulation& values,int32_t daytime) {
        const auto key=std::make_pair(zone->name,daytime);
        const auto cars=world.zoneCarInformation.find(key),peds=world.zonePedInformation.find(key);
        if(cars!=world.zoneCarInformation.end())values.cars=cars->second.values;
        if(peds!=world.zonePedInformation.end())values.pedestrians=peds->second.values;
        const auto carDensity=world.zoneCarDensities.find(key),pedDensity=world.zonePedDensities.find(key);
        if(carDensity!=world.zoneCarDensities.end())values.cars[0]=carDensity->second;
        if(pedDensity!=world.zonePedDensities.end())values.pedestrians[0]=pedDensity->second;
        const auto civilians=world.zoneCivilianCarInformation.find(key);
        if(civilians!=world.zoneCivilianCarInformation.end()) {
            const auto& densities=civilians->second;
            for(size_t index=0;index<values.civilianCars.size();++index)
                values.civilianCars[index]=densities[index];
            for(size_t index=0;index<values.boats.size();++index)
                values.boats[index]=densities[values.civilianCars.size()+index];
        }
        const auto group=world.zonePedGroups.find(key);
        if(group!=world.zonePedGroups.end())values.pedestrianGroup=uint16_t(group->second);
    };
    apply(day,1);apply(night,0);
    // CTheZones chooses day at 08..18, night at 22..04, and changes the
    // population mix over the three intervening hours. The native clock
    // supplies whole hours to these weights.
    const int clock=(int(std::floor(std::fmod(hour,24.0f)))+24)%24;
    const float dayWeight=clock>=8&&clock<19?1.0f:clock>=22||clock<5?0.0f:
        clock>=19?1.0f-float(clock-19)/3.0f:float(clock-5)/3.0f;
    const float nightWeight=clock>=19&&clock<22?float(clock-19)/3.0f:1.0f-dayWeight;
    const auto blend=[&](const auto& a,const auto& b,auto& output) {
        for(size_t index=0;index<output.size();++index)
            output[index]=int32_t(std::clamp(double(float(a[index])*dayWeight+float(b[index])*nightWeight),
                double(INT32_MIN),double(INT32_MAX)));
    };
    blend(day.cars,night.cars,result.values.cars);
    blend(day.pedestrians,night.pedestrians,result.values.pedestrians);
    const auto blendThresholds=[&](const auto& a,const auto& b,auto& output) {
        int32_t dayThreshold=0,nightThreshold=0,previous=0;
        for(size_t index=0;index<output.size();++index) {
            dayThreshold=storylandLcsPopulationShort(int64_t(dayThreshold)+a[index]);
            nightThreshold=storylandLcsPopulationShort(int64_t(nightThreshold)+b[index]);
            const int32_t threshold=int32_t(float(dayThreshold)*dayWeight+float(nightThreshold)*nightWeight);
            output[index]=threshold-previous;previous=threshold;
        }
    };
    blendThresholds(day.civilianCars,night.civilianCars,result.values.civilianCars);
    blendThresholds(day.boats,night.boats,result.values.boats);
    result.values.pedestrianGroup=clock>=5&&clock<19?day.pedestrianGroup:night.pedestrianGroup;
    result.available=true;return result;
}

// Native 0..999 decisions choose police first, then the nine gangs, with the
// remainder reserved for civilians. A negative script ratio cannot underflow.
inline int storylandLcsPopulationKind(const std::array<int32_t,11>& values,uint32_t decision) {
    int64_t threshold=std::max(0,values[10]);
    const int32_t choice=int32_t(decision%1000u);
    if(choice<threshold)return 9;
    for(int gang=0;gang<9;++gang) {
        threshold+=std::max(0,values[size_t(gang)+1]);
        if(choice<threshold)return gang;
    }
    return -1;
}

template<size_t Count>
inline int storylandLcsPopulationClass(const std::array<int32_t,Count>& weights,uint32_t decision) {
    static_assert(Count>0,"A native vehicle population requires a fallback class.");
    const int32_t choice=int32_t(decision%1000u);
    int32_t threshold=0;
    for(size_t index=0;index+1<weights.size();++index) {
        threshold=storylandLcsPopulationShort(int64_t(threshold)+weights[index]);
        if(choice<threshold)return int(index);
    }
    return int(Count-1);
}
