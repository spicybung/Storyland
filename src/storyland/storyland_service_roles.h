#pragma once
#include "src/leeds/leeds_ped_stats.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class StorylandServiceRole : uint8_t {
    Civilian, Police, Swat, Fbi, Army, Paramedic, Firefighter, MotorcyclePolice, Vice, Dea
};

inline std::string storylandServiceModelStem(std::string name) {
    const auto slash=name.find_last_of("/\\");
    if(slash!=std::string::npos)name.erase(0,slash+1);
    const auto dot=name.find_last_of('.');
    if(dot!=std::string::npos)name.resize(dot);
    std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
    return name;
}

inline bool storylandLawEnforcementRole(StorylandServiceRole role) {
    return role==StorylandServiceRole::Police||role==StorylandServiceRole::Swat||
        role==StorylandServiceRole::Fbi||role==StorylandServiceRole::Army||
        role==StorylandServiceRole::MotorcyclePolice||role==StorylandServiceRole::Vice||role==StorylandServiceRole::Dea;
}

inline bool storylandEmergencySirenRole(StorylandServiceRole role) {
    return (storylandLawEnforcementRole(role)&&role!=StorylandServiceRole::Army)||
        role==StorylandServiceRole::Paramedic||role==StorylandServiceRole::Firefighter;
}

inline StorylandServiceRole storylandNativeVehicleSirenRole(uint32_t model,bool vcs) {
    if(vcs) {
        switch(model) {
        case 191:return StorylandServiceRole::MotorcyclePolice;
        case 193:return StorylandServiceRole::Fbi;
        case 194:return StorylandServiceRole::Firefighter;
        case 207:return StorylandServiceRole::Police;
        case 227:return StorylandServiceRole::Paramedic;
        case 234:return StorylandServiceRole::Swat;
        default:return StorylandServiceRole::Civilian;
        }
    }
    switch(model) {
    case 138:return StorylandServiceRole::Firefighter;
    case 147:return StorylandServiceRole::Paramedic;
    case 148:return StorylandServiceRole::Fbi;
    case 157:return StorylandServiceRole::Police;
    case 158:return StorylandServiceRole::Swat;
    default:return StorylandServiceRole::Civilian;
    }
}

inline StorylandServiceRole storylandServicePedestrianRole(const std::string& model,bool vcs) {
    const auto name=storylandServiceModelStem(model);
    if(name=="cop"||name=="cop2"||name=="cop3"||name=="cop4")return StorylandServiceRole::Police;
    if(name=="bcop")return StorylandServiceRole::MotorcyclePolice;
    if(name=="vice1"||name=="vice2")return StorylandServiceRole::Vice;
    if(name=="dea1"||name=="dea2")return StorylandServiceRole::Dea;
    if(name=="swat")return StorylandServiceRole::Swat;
    if(name=="fbi")return StorylandServiceRole::Fbi;
    if(name=="army"||name=="soldier")return StorylandServiceRole::Army;
    if(name=="medic"||name=="paramedic")return StorylandServiceRole::Paramedic;
    if(name=="fireman"||name=="firefighter")return StorylandServiceRole::Firefighter;
    return StorylandServiceRole::Civilian;
}

inline StorylandServiceRole storylandServicePedestrianRole(const std::string& model,bool vcs,
    const StorylandPedStats& stats) {
    const auto namedRole=storylandServicePedestrianRole(model,vcs);
    if(namedRole!=StorylandServiceRole::Civilian||!vcs||!stats.valid)return namedRole;
    if(stats.pedType==16)return StorylandServiceRole::Paramedic;
    if(stats.pedType==17)return StorylandServiceRole::Firefighter;
    return StorylandServiceRole::Civilian;
}

inline StorylandServiceRole storylandServiceVehicleRole(const std::string& model) {
    const auto name=storylandServiceModelStem(model);
    if(name=="police"||name=="policem"||name=="polmav"||name=="predator"||name=="polcar")return StorylandServiceRole::Police;
    if(name=="electrap"||name=="polbike"||name=="policeb")return StorylandServiceRole::MotorcyclePolice;
    if(name=="enforcer"||name=="enforce")return StorylandServiceRole::Swat;
    if(name=="fbicar"||name=="fbi"||name=="fbiranch"||name=="fbiwash"||name=="fbiran")return StorylandServiceRole::Fbi;
    if(name=="barracks"||name=="barrackb"||name=="rhino"||name=="hunter")return StorylandServiceRole::Army;
    if(name=="ambulan"||name=="ambulance")return StorylandServiceRole::Paramedic;
    if(name=="firetruk"||name=="firetruck")return StorylandServiceRole::Firefighter;
    return StorylandServiceRole::Civilian;
}

inline bool storylandCivilianPedestrianModelAllowed(
    const std::vector<std::array<uint32_t,16>>& groups,uint32_t model) {
    if(groups.empty())return true;
    for(const auto& group:groups)
        if(std::find(group.begin(),group.end(),model)!=group.end())return true;
    return false;
}

inline size_t storylandChooseVehicleDriver(StorylandServiceRole vehicleRole,int vehicleClass,
    const std::vector<StorylandPedStats>& models,const std::vector<StorylandServiceRole>& roles,uint32_t variation,
    const std::vector<std::array<uint32_t,16>>* groups=nullptr) {
    std::vector<size_t> candidates;
    for(size_t index=0;index<models.size()&&index<roles.size();++index) {
        if(roles[index]!=vehicleRole||!models[index].valid||models[index].pedType==0)continue;
        if(vehicleRole==StorylandServiceRole::Civilian&&models[index].pedType!=4&&models[index].pedType!=5)continue;
        if(vehicleRole==StorylandServiceRole::Civilian&&groups&&
            !storylandCivilianPedestrianModelAllowed(*groups,models[index].modelId))continue;
        if(vehicleRole==StorylandServiceRole::Civilian&&models[index].vehicleClassMask&&
            vehicleClass>=0&&vehicleClass<32&&!(models[index].vehicleClassMask&(uint32_t(1)<<vehicleClass)))continue;
        candidates.push_back(index);
    }
    return candidates.empty()?SIZE_MAX:candidates[variation%candidates.size()];
}
