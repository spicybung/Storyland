#pragma once
#include "src/storyland/storyland_play_test_runtime.h"

inline StorylandServiceRole storylandVehicleServiceRole(const StorylandPlayTestRuntime& runtime,
    const StorylandRuntimeVehicle& vehicle) {
    if(runtime.crime.vcs&&vehicle.vcsViceUnit&&vehicle.nativeDefinition.modelId==204)
        return StorylandServiceRole::Vice;
    return vehicle.model<runtime.modelServiceRoles.size()?runtime.modelServiceRoles[vehicle.model]:vehicle.serviceRole;
}

inline int32_t storylandVcsLawVehicleDriverModel(uint32_t nativeModel) {
    switch (nativeModel) {
    case 191: return 119;
    case 193: return 10;
    case 207: return 8;
    case 218: case 246: return 11;
    case 234: return 9;
    default: return -1;
    }
}

inline size_t storylandVehicleDriverModel(const StorylandPlayTestRuntime& runtime,
    const StorylandRuntimeVehicle& vehicle,uint32_t variation=0) {
    const auto role=storylandVehicleServiceRole(runtime,vehicle);
    const auto& models=runtime.pedestrianModelStats;
    const auto& roles=runtime.pedestrianModelRoles;
    if(runtime.crime.vcs&&vehicle.vcsViceUnit&&vehicle.nativeDefinition.modelId==204&&
        vehicle.vcsViceCrewSequence<4) {
        const uint32_t nativeModel=storylandVcsViceCrewModel(vehicle.vcsViceCrewSequence);
        for(size_t index=0;index<models.size()&&index<roles.size();++index)
            if(models[index].valid&&models[index].pedType==6&&models[index].modelId==nativeModel&&
                roles[index]==StorylandServiceRole::Vice)return index;
        return SIZE_MAX;
    }
    // 2B3CA0 selects the cop subtype from the generated VCS police car;
    // 324D08 resolves it to the original fixed resident ped model. Scripted
    // drivers and non-police traffic retain their existing assignment.
    if(runtime.crime.vcs&&vehicle.scriptHandle<0&&vehicle.nativeDefinition.vcsModelLayout&&
        (vehicle.nativeDefinition.vehicleClass==13||vehicle.nativeDefinition.vehicleClass==24)) {
        const int32_t nativeModel=storylandVcsLawVehicleDriverModel(vehicle.nativeDefinition.modelId);
        if(nativeModel>=0)for(size_t index=0;index<models.size()&&index<roles.size();++index)
            if(models[index].valid&&models[index].pedType==6&&models[index].modelId==uint32_t(nativeModel))return index;
    }
    const int gang=runtime.crime.vcs?vehicle.nativeDefinition.vehicleClass-14:
        vehicle.nativeDefinition.vehicleClass-12;
    if(role==StorylandServiceRole::Civilian&&gang>=0&&gang<9&&runtime.missionEnvironment) {
        const auto& gangModels=runtime.missionEnvironment->gangs[size_t(gang)].pedModels;
        const auto matches=[&](size_t model) {
            return model<models.size()&&models[model].valid&&
                (int32_t(models[model].modelId)==gangModels[0]||int32_t(models[model].modelId)==gangModels[1]);
        };
        if(matches(vehicle.driverModel))return vehicle.driverModel;
        for(unsigned candidate=0;candidate<2;++candidate) {
            const int32_t nativeModel=gangModels[(variation+candidate)%2];
            for(size_t index=0;index<models.size();++index)
                if(nativeModel>0&&int32_t(models[index].modelId)==nativeModel&&matches(index))return index;
        }
        return SIZE_MAX;
    }
    if(vehicle.driverModel<models.size()&&vehicle.driverModel<roles.size()&&
        roles[vehicle.driverModel]==role&&models[vehicle.driverModel].valid&&
        (role!=StorylandServiceRole::Civilian||
            ((models[vehicle.driverModel].pedType==4||models[vehicle.driverModel].pedType==5)&&
             storylandCivilianPedestrianModelAllowed(runtime.lcsPedestrianGroups,models[vehicle.driverModel].modelId))))
        return vehicle.driverModel;
    return storylandChooseVehicleDriver(role,vehicle.nativeDefinition.vehicleClass,models,roles,variation,
        &runtime.lcsPedestrianGroups);
}

inline bool storylandAssignVehicleDriver(const StorylandPlayTestRuntime& runtime,
    StorylandRuntimeVehicle& vehicle,uint32_t variation) {
    vehicle.serviceRole=storylandVehicleServiceRole(runtime,vehicle);
    vehicle.driverModel=storylandVehicleDriverModel(runtime,vehicle,variation);
    return vehicle.driverModel!=SIZE_MAX;
}
