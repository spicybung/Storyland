#pragma once
#include "src/re/reLCS2/runtime/storyland_lcs_population.h"
#include "src/storyland/storyland_native_random.h"
#include <algorithm>
#include <cmath>

inline float storylandPopulationDayWeight(float hour) {
    if(!std::isfinite(hour))return 0;
    const int clock=(int(std::floor(std::fmod(hour,24.0f)))+24)%24;
    if(clock>=8&&clock<19)return 1;
    if(clock>=22||clock<5)return 0;
    return clock>=19?1-float(clock-19)/3:float(clock-5)/3;
}

inline StorylandLcsPopulation storylandVcsZonePopulation(const StorylandMissionEnvironment& world,
    const StorylandNativeZone& zone,float hour) {
    StorylandLcsPopulation result;
    if(!zone.populationLoaded||!std::isfinite(hour))return result;
    const auto periodRecord=[&](int period) {
        auto row=zone.populationRecords[size_t(period)];
        const auto key=std::make_pair(zone.name,period);
        const auto applyRatios=[](const auto& values,auto& thresholds,uint16_t& police) {
            police=uint16_t(values[10]);
            uint16_t cumulative=police;
            for(size_t gang=0;gang<thresholds.size();++gang) {
                cumulative=uint16_t(cumulative+uint16_t(values[gang+1]));
                thresholds[gang]=cumulative;
            }
        };
        const auto cars=world.zoneCarInformation.find(key);
        if(cars!=world.zoneCarInformation.end()) {
            row.carDensity=uint16_t(cars->second.values[0]);
            applyRatios(cars->second.values,row.gangCarThresholds,row.policeCarThreshold);
        }
        const auto peds=world.zonePedInformation.find(key);
        if(peds!=world.zonePedInformation.end()) {
            row.pedestrianDensity=uint16_t(peds->second.values[0]);
            applyRatios(peds->second.values,row.gangPedestrianThresholds,row.policePedestrianThreshold);
        }
        const auto classes=world.zoneCivilianCarInformation.find(key);
        if(classes!=world.zoneCivilianCarInformation.end()) {
            uint16_t cumulative=0;
            for(size_t index=0;index<row.vehicleClassThresholds.size();++index) {
                cumulative=uint16_t(cumulative+uint16_t(classes->second[index]));
                row.vehicleClassThresholds[index]=cumulative;
            }
        }
        const auto group=world.zonePedGroups.find(key);
        if(group!=world.zonePedGroups.end())row.pedestrianGroup=uint16_t(group->second);
        return row;
    };
    const auto day=periodRecord(1),night=periodRecord(0);
    const int clock=(int(std::floor(std::fmod(hour,24.0f)))+24)%24;
    const bool daytime=clock>=8&&clock<19;
    const bool nighttime=clock>=22||clock<5;
    auto& row=result.vcsValues;
    row=daytime?day:night;
    if(!daytime&&!nighttime) {
        const float dayWeight=storylandPopulationDayWeight(hour),nightWeight=1-dayWeight;
        row.carDensity=uint16_t(float(day.carDensity)*dayWeight+float(night.carDensity)*nightWeight);
        // 3684F8..368630 reads the night record for both products. Retain
        // that native transition behavior for every field except car density.
    }
    row.pedestrianGroup=clock>=5&&clock<19?day.pedestrianGroup:night.pedestrianGroup;
    const auto expand=[](const auto& thresholds,uint16_t police,auto& output) {
        output[10]=police;
        int32_t previous=police;
        for(size_t gang=0;gang<thresholds.size();++gang) {
            output[gang+1]=int32_t(thresholds[gang])-previous;
            previous=thresholds[gang];
        }
    };
    result.values.cars[0]=row.carDensity;
    result.values.pedestrians[0]=row.pedestrianDensity;
    expand(row.gangCarThresholds,row.policeCarThreshold,result.values.cars);
    expand(row.gangPedestrianThresholds,row.policePedestrianThreshold,result.values.pedestrians);
    int32_t previous=0;
    for(size_t index=0;index<result.values.civilianCars.size();++index) {
        result.values.civilianCars[index]=int32_t(row.vehicleClassThresholds[index])-previous;
        previous=row.vehicleClassThresholds[index];
    }
    result.values.pedestrianGroup=row.pedestrianGroup;
    result.available=true;result.vcs=true;
    return result;
}

inline int storylandPopulationKind(const StorylandLcsPopulation& population,bool vehicles,uint32_t decision) {
    if(!population.vcs)return storylandLcsPopulationKind(
        vehicles?population.values.cars:population.values.pedestrians,decision);
    const auto& row=population.vcsValues;
    const uint32_t choice=storylandNativeRandomRangeFromValue(decision,1000);
    if(choice<(vehicles?row.policeCarThreshold:row.policePedestrianThreshold))return 9;
    const auto& thresholds=vehicles?row.gangCarThresholds:row.gangPedestrianThresholds;
    for(size_t gang=0;gang<thresholds.size();++gang)
        if(choice<thresholds[gang])return int(gang);
    return -1;
}

inline int storylandVcsPopulationClass(const StorylandLcsPopulation& population,bool boat,uint32_t decision) {
    const auto& thresholds=population.vcsValues.vehicleClassThresholds;
    const uint32_t roadTotal=thresholds[8];
    const uint32_t total=boat?thresholds[12]:roadTotal;
    const uint32_t start=boat?roadTotal:0;
    if(total<=start)return -1;
    const uint32_t choice=start+storylandNativeRandomRangeFromValue(decision,total-start);
    const size_t first=boat?9:0,last=boat?12:8;
    for(size_t index=first;index<last;++index)
        if(choice<thresholds[index])return int(index);
    return int(last);
}

inline StorylandLcsPopulation storylandNativePopulationAt(const StorylandMissionEnvironment& world,
    const StorylandNativePoint& point,float hour) {
    if(!world.zones)return {};
    if(!world.zones->vcs)return storylandLcsPopulationAt(world,point,hour);
    const auto* zone=world.zones->at(point,true);
    return zone?storylandVcsZonePopulation(world,*zone,hour):StorylandLcsPopulation{};
}

inline size_t storylandPopulationDensityTarget(bool available,int32_t density,float multiplier,size_t capacity) {
    if(!available||density<=0||!std::isfinite(multiplier)||multiplier<=0)return 0;
    return size_t(std::clamp(double(density)*double(multiplier),0.0,double(capacity)));
}

inline size_t storylandVcsPedestrianTarget(const StorylandLcsPopulation& population,
    float multiplier,float rain,size_t alreadyCounted,size_t capacity) {
    if(!population.available||!std::isfinite(multiplier)||multiplier<=0||!std::isfinite(rain))return 0;
    const float weather=1.0f-.8f*std::sqrt(std::clamp(rain,0.0f,1.0f));
    const float density=float(population.vcsValues.carDensity)+population.vcsValues.pedestrianDensity;
    const size_t total=size_t(std::clamp(density*multiplier*.4f*weather,0.0f,25.0f));
    return total>alreadyCounted?std::min(capacity,total-alreadyCounted):0;
}

inline size_t storylandVcsVehicleTarget(const StorylandLcsPopulation& population,
    float multiplier,size_t protectedCount,size_t capacity) {
    if(!population.available||!std::isfinite(multiplier)||multiplier<=0)return 0;
    if(protectedCount>=16)return 0;
    const double desired=double(population.vcsValues.carDensity)*double(multiplier)*double(.7f);
    return size_t(std::clamp(std::ceil(desired),0.0,double(std::min(capacity,size_t(16)-protectedCount))));
}

inline std::vector<uint32_t> storylandPopulationPedestrianModels(const StorylandMissionEnvironment& world,
    const StorylandLcsPopulation& population,const std::vector<std::array<uint32_t,16>>& groups) {
    std::vector<uint32_t> result;
    if(!population.available)return result;
    if(population.values.pedestrianGroup<groups.size()) {
        const auto& group=groups[population.values.pedestrianGroup];
        result.assign(group.begin(),group.end());
    }
    for(size_t gang=0;gang<world.gangs.size();++gang)
        if(population.values.cars[gang+1]>0||population.values.pedestrians[gang+1]>0)
            for(int32_t model:world.gangs[gang].pedModels)
                if(model>0&&std::find(result.begin(),result.end(),uint32_t(model))==result.end())
                    result.push_back(uint32_t(model));
    return result;
}

// Pick among the resident entries of a zone's authored pedestrian group,
// retaining duplicate entries as weights while avoiding visible identical
// characters when alternatives are already loaded. Never substitute a gang,
// police or soldier model for a civilian group.
inline size_t storylandSelectVariedPedestrianModel(const std::vector<size_t>& weightedModels,
    const std::vector<size_t>& nearbyModels,uint32_t decision) {
    if(weightedModels.empty())return SIZE_MAX;
    size_t fewest=SIZE_MAX;
    std::vector<size_t> choices;
    choices.reserve(weightedModels.size());
    for(size_t model:weightedModels) {
        const size_t present=size_t(std::count(nearbyModels.begin(),nearbyModels.end(),model));
        if(present<fewest) {fewest=present;choices.clear();}
        if(present==fewest)choices.push_back(model);
    }
    return choices.empty()?SIZE_MAX:choices[decision%choices.size()];
}
