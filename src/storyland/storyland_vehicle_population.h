#pragma once
#include "src/storyland/storyland_native_random.h"
#include "src/storyland/storyland_population.h"
#include "src/storyland/storyland_service_population.h"

struct StorylandPopulationVehicleChoice {
    size_t model=SIZE_MAX;
    int nativeClass=-1;
};

inline size_t storylandResidentPopulationVehicle(const StorylandPlayTestRuntime& runtime,uint32_t model) {
    for(size_t index=0;index<runtime.modelDefinitions.size();++index)
        if(runtime.modelDefinitions[index].modelId==model)return index;
    return SIZE_MAX;
}

inline bool storylandResidentPopulationPedestrian(const StorylandPlayTestRuntime& runtime,int32_t model) {
    return model>0&&std::any_of(runtime.pedestrianModelStats.begin(),runtime.pedestrianModelStats.end(),
        [model](const auto& stats){return stats.valid&&stats.modelId==uint32_t(model);});
}

inline uint32_t storylandVcsPopulationPoliceModel(const StorylandPlayTestRuntime& runtime,uint32_t& random,
    bool canCreateViceCrew=false) {
    const auto resident=[&](uint32_t model) {
        return storylandResidentPopulationVehicle(runtime,model)!=SIZE_MAX;
    };
    const auto pedestrian=[&](int32_t model) {
        return storylandResidentPopulationPedestrian(runtime,model);
    };
    if(canCreateViceCrew&&runtime.missionEnvironment&&
        storylandVcsViceDispatchEligible(runtime.missionEnvironment->vcsViceDispatch,runtime.wanted,
            runtime.missionEnvironment->gameMilliseconds,resident(204),pedestrian(117),pedestrian(118)))
        return 204;
    if(runtime.wanted==4&&resident(234)&&pedestrian(9))
        return storylandNativeRandomRange(random,3)==2?234:207;
    if(runtime.wanted==5&&resident(193)&&pedestrian(10))return 193;
    if(runtime.wanted>=6&&resident(246)&&resident(218)&&pedestrian(11))
        return storylandNativeRandomRange(random,2)?246:218;
    if(runtime.wanted<2&&resident(191)&&pedestrian(119))
        return storylandNativeRandomRange(random,3)==2?191:207;
    return 207;
}

inline StorylandPopulationVehicleChoice storylandChooseVcsPopulationVehicle(
    const StorylandPlayTestRuntime& runtime,const StorylandLcsPopulation& population,bool boat,
    uint32_t& random,const StorylandNativeWorld* world=nullptr,bool canCreateViceCrew=false) {
    const auto& models=boat?runtime.waterModels:runtime.roadModels;
    if(!population.available||!population.vcs||!runtime.missionEnvironment)return {};
    const int kind=boat?-1:storylandPopulationKind(population,true,storylandNativeRandom(random));
    for(unsigned attempt=0;attempt<10;++attempt) {
        StorylandPopulationVehicleChoice choice;
        if(kind==9) {
            const uint32_t model=storylandVcsPopulationPoliceModel(runtime,random,canCreateViceCrew);
            choice.model=storylandResidentPopulationVehicle(runtime,model);
            choice.nativeClass=model==191?24:13;
        } else if(kind>=0) {
            const auto& gang=runtime.missionEnvironment->gangs[size_t(kind)];
            if(!storylandResidentPopulationPedestrian(runtime,gang.pedModels[0])||
                !storylandResidentPopulationPedestrian(runtime,gang.pedModels[1]))continue;
            if(gang.carModel>0)choice.model=storylandResidentPopulationVehicle(runtime,uint32_t(gang.carModel));
            choice.nativeClass=14+kind;
        } else {
            choice.nativeClass=storylandVcsPopulationClass(population,boat,storylandNativeRandom(random));
            if(choice.nativeClass<0)return {};
            uint64_t total=0;
            for(size_t model:models)if(model<runtime.modelDefinitions.size()) {
                const auto& definition=runtime.modelDefinitions[model];
                if(definition.vehicleClass==choice.nativeClass)total+=definition.frequency;
            }
            if(!total)continue;
            const uint64_t value=storylandNativeRandomRange(random,uint32_t(total));
            uint64_t cumulative=0;
            for(size_t model:models)if(model<runtime.modelDefinitions.size()) {
                const auto& definition=runtime.modelDefinitions[model];
                if(definition.vehicleClass!=choice.nativeClass||!definition.frequency)continue;
                cumulative+=definition.frequency;
                // 432AD0 selects the first cumulative frequency >= decision.
                if(value<=cumulative) {choice.model=model;break;}
            }
        }
        if(choice.model==SIZE_MAX||std::find(models.begin(),models.end(),choice.model)==models.end())continue;
        if(world) {
            const auto collision=world->collisionModels.find(runtime.modelDefinitions[choice.model].modelId);
            if(collision!=world->collisionModels.end()&&collision->second.boundingRadius>20)return {};
        }
        return choice;
    }
    return {};
}
