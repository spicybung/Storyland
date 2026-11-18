#pragma once
#include "src/storyland/storyland_vehicle_systems.h"
#include "src/re/reLCS2/world/storyland_lcs_garages.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_vehicle_occupants.h"

inline bool storylandPrepareVcsGarageDoors(const std::vector<uint8_t>& data,
    const StorylandNativeWorld& native,const StorylandMissionEnvironment::Garage& garage,
    StorylandMissionEnvironment::LcsGarageDoorState& state,std::string& error) {
    const auto models=storylandNativeGarageDoorModels(data);
    if(models.empty()||native.instances.empty()) {
        error="Garage doors require the original VCS model-index table and placements.";
        return false;
    }
    StorylandMissionEnvironment::LcsGarageDoorState decoded;
    for(const auto& instance:native.instances) {
        const auto& original=instance.originalMatrix;
        if(!models.count(int32_t(instance.originalModelId))||original[15]!=1||
            !storylandGarageContainsPoint(garage,{original[12],original[13],original[14]},1.5f))continue;
        const auto collision=native.collisionModels.find(instance.originalModelId);
        if(collision==native.collisionModels.end()) {
            error="Native garage door has no original collision bounds.";return false;
        }
        if(decoded.doors.empty())decoded.height=collision->second.maximum.z-collision->second.minimum.z-.1f;
        decoded.doors.push_back({uint64_t(instance.poolHeader)<<32|instance.poolIndex,
            instance.originalModelId,original});
    }
    if(garage.swingDoor)decoded.height=decoded.height*.5f-.1f;
    if(!std::isfinite(decoded.height)||decoded.height<=0||decoded.doors.size()>2) {
        error="Native garage door placement or travel is invalid.";return false;
    }
    decoded.state=garage.state;
    decoded.position=std::clamp(garage.doorPosition,0.0f,1.0f)*decoded.height;
    state=std::move(decoded);error.clear();return true;
}

inline std::array<float,4> storylandVcsGarageOuterBounds(const StorylandMissionEnvironment::Garage& garage) {
    const auto& b=garage.bounds;
    const float lastX=b[3]+b[5]-b[0],lastY=b[4]+b[6]-b[1];
    return {std::min({b[0],b[3],b[5],lastX}),std::min({b[1],b[4],b[6],lastY}),
        std::max({b[0],b[3],b[5],lastX}),std::max({b[1],b[4],b[6],lastY})};
}

inline float storylandVcsGarageDistanceSquared(const StorylandMissionEnvironment::Garage& garage,
    const StorylandNativePoint& point) {
    const auto b=storylandVcsGarageOuterBounds(garage);
    const float x=std::max({b[0]-point.x,point.x-b[2],0.0f});
    const float y=std::max({b[1]-point.y,point.y-b[3],0.0f});
    return x*x+y*y;
}

inline StorylandVehicleMatrix storylandVcsGarageVehicleMatrix(const StorylandNativeTrafficActor& actor,
    const StorylandRuntimeVehicle& vehicle) {
    const auto& definition=vehicle.nativeDefinition;
    return storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
        {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
        vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
        actor.water),definition.previewRoot);
}

inline std::pair<bool,bool> storylandVcsGarageVehicleSpheres(const StorylandMissionEnvironment::Garage& garage,
    const StorylandNativeTrafficActor& actor,const StorylandRuntimeVehicle& vehicle,float margin=0) {
    bool any=false,all=true;
    const auto& collision=vehicle.nativeDefinition.collision;
    if(!collision)return {false,false};
    const auto matrix=storylandVcsGarageVehicleMatrix(actor,vehicle);
    for(const auto& sphere:collision->spheres) {
        const auto center=storylandVehicleTransformPoint(matrix,
            {sphere.center.x,sphere.center.y,sphere.center.z});
        const bool inside=storylandGarageContainsPoint(garage,center,sphere.radius+margin);
        any=any||inside;all=all&&inside;
    }
    return {any,all};
}

inline void storylandRetireGarageVehicleReferences(StorylandPlayTestRuntime& runtime,size_t slot) {
    auto& vehicle=runtime.vehicles[slot];
    auto& world=*runtime.missionEnvironment;
    if(vehicle.scriptHandle>=0) {
        const int32_t handle=vehicle.scriptHandle;
        const auto script=world.missionVehicles.find(handle);
        if(script!=world.missionVehicles.end()) {
            script->second.deleted=true;script->second.alive=false;
            script->second.visible=false;script->second.collisionEnabled=false;
            script->second.spawnedSlot=-1;
        }
        world.missionCleanupVehicles.erase(handle);
        world.vcsVehicleFlag28.erase(handle);
        world.vcsGarageStorableMissionVehicles.erase(handle);
        world.releaseCharacterVehicleReferences(handle);
    }
    if(vehicle.generator>=0) {
        for(auto& generator:world.carGenerators)if(generator.handle==vehicle.generator)generator.spawnedSlot=-1;
        const auto generator=runtime.generatorStates.find(vehicle.generator);
        if(generator!=runtime.generatorStates.end()) {
            generator->second.spawned=false;generator->second.waitingForDeparture=true;
        }
    }
    vehicle.scriptHandle=-1;vehicle.generator=-1;
    vehicle.hasDriver=false;vehicle.missionDriver=false;vehicle.aiActive=false;
}

inline void storylandStoreVcsGarageVehicles(StorylandPlayTestRuntime& runtime,
    const StorylandMissionEnvironment::Garage& garage,std::vector<StorylandNativeTrafficActor>& actors) {
    auto& world=*runtime.missionEnvironment;
    const uint8_t owner = runtime.coopConnected ? runtime.coopPlayerIndex : 0;
    size_t stored=0;
    for(const auto& vehicle:runtime.vehicles)
        if(vehicle.savedGarage==garage.handle&&!vehicle.visible&&vehicle.garageOwner==owner)++stored;
    for(size_t index=std::min(actors.size(),runtime.vehicles.size());index-->0;) {
        auto& vehicle=runtime.vehicles[index];
        if(!vehicle.visible||int32_t(index)==runtime.occupiedVehicle||vehicle.coopControlled||
            (vehicle.garageOwner!=255&&vehicle.garageOwner!=owner)||
            !storylandGarageContainsPoint(garage,{actors[index].position.x,actors[index].position.y,
                actors[index].position.z}))continue;
        const auto script=world.missionVehicles.find(vehicle.scriptHandle);
        const bool garageStorable=world.vcsGarageStorableMissionVehicles.count(vehicle.scriptHandle)!=0;
        if(script!=world.missionVehicles.end()&&script->second.persistent&&!script->second.ambientOrigin&&!garageStorable)continue;
        if(script!=world.missionVehicles.end())vehicle.vcsGarageStorable=garageStorable;
        const bool keep=stored<size_t(std::clamp(garage.maximumCars,0,4))&&!vehicle.wrecked&&vehicle.health>0&&
            storylandVcsGarageVehicleSpheres(garage,actors[index],vehicle,1).second;
        storylandRetireGarageVehicleReferences(runtime,index);
        vehicle.savedGarage=keep?garage.handle:-1;
        vehicle.garageOwner=owner;
        vehicle.visible=false;vehicle.collisionEnabled=false;vehicle.positionFrozen=true;
        vehicle.speed=vehicle.lateralSpeed=vehicle.verticalSpeed=vehicle.yawVelocity=0;
        vehicle.burning=false;vehicle.engineFireSpent=true;
        for(auto& effect:runtime.effects)if(effect.vehicle==int32_t(index))effect.remaining=0;
        if(keep)++stored;else vehicle.health=0;
    }
}

inline void storylandRestoreVcsGarageVehicles(StorylandPlayTestRuntime& runtime,
    const StorylandMissionEnvironment::Garage& garage) {
    const uint8_t owner = runtime.coopConnected ? runtime.coopPlayerIndex : 0;
    for(auto& vehicle:runtime.vehicles) {
        if(vehicle.savedGarage!=garage.handle||vehicle.visible||vehicle.garageOwner!=owner)continue;
        StorylandRuntimeVehicle restored;
        storylandApplyVehicleDefinition(restored,vehicle.nativeDefinition);
        restored.model=vehicle.model;restored.primaryColor=vehicle.primaryColor;
        restored.garageOwner=vehicle.garageOwner;restored.coopIdentity=vehicle.coopIdentity;
        restored.vcsGarageStorable=vehicle.vcsGarageStorable;
        restored.secondaryColor=vehicle.secondaryColor;restored.colorSet=vehicle.colorSet;
        restored.scriptColorSet=vehicle.scriptColorSet;restored.radioStation=vehicle.radioStation;
        restored.damageProofs=vehicle.damageProofs;restored.tractionMultiplier=vehicle.tractionMultiplier;
        restored.parked=true;restored.hasDriver=false;restored.doorLock=1;
        restored.bodyHeight=vehicle.bodyHeight;restored.halfWidth=vehicle.halfWidth;
        restored.halfLength=vehicle.halfLength;restored.grounded=true;
        vehicle=std::move(restored);
    }
}

inline bool storylandAdvanceVcsSafehouseGarages(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& actors,float seconds,std::string& error) {
    if(!runtime.missionEnvironment)return true;
    if(runtime.paused||runtime.pauseMenu||!std::isfinite(seconds)||seconds<=0)return true;
    auto& world=*runtime.missionEnvironment;
    const uint8_t owner = runtime.coopConnected ? runtime.coopPlayerIndex : 0;
    for(auto& vehicle:runtime.vehicles)if(vehicle.visible&&vehicle.savedGarage>=0&&vehicle.garageOwner==owner)vehicle.savedGarage=-1;
    const float step=std::min(seconds,.1f)*50;
    for(auto& garage:world.garages) {
        const bool hideout=runtime.crime.vcs?storylandVcsHideoutGarage(garage.type):
            ((garage.type>=16&&garage.type<=18)||(garage.type>=24&&garage.type<=32));
        if(!hideout||garage.locked)continue;
        if(!world.prepareLcsGarageDoors) {
            error="VCS safehouse garages require their native door placement binding.";return false;
        }
        StorylandMissionEnvironment::LcsGarageDoorState doors;
        if(!world.prepareLcsGarageDoors(garage,doors,error))return false;
        if(!runtime.crime.vcs) {
            doors.state=garage.state;
            doors.position=garage.doorPosition*doors.height;
        }
        const float distance=storylandVcsGarageDistanceSquared(garage,{world.x,world.y,world.z});
        const bool inVehicle=runtime.occupiedVehicle>=0;
        unsigned inside=0,stored=0;bool straddling=false,playerIntersects=false;
        for(size_t index=0;index<actors.size()&&index<runtime.vehicles.size();++index) {
            const auto& vehicle=runtime.vehicles[index];
            if(vehicle.savedGarage==garage.handle&&!vehicle.visible&&vehicle.garageOwner==owner)++stored;
            if(!vehicle.visible||vehicle.coopControlled||
                (vehicle.garageOwner!=255&&vehicle.garageOwner!=owner))continue;
            const auto spheres=storylandVcsGarageVehicleSpheres(garage,actors[index],vehicle);
            straddling=straddling||(spheres.first&&!spheres.second);
            if(int32_t(index)==runtime.occupiedVehicle)playerIntersects=spheres.first;
            else if(storylandGarageContainsPoint(garage,{actors[index].position.x,actors[index].position.y,
                actors[index].position.z}))++inside;
        }
        if(!inVehicle) {
            const auto bounds=storylandVcsGarageOuterBounds(garage);
            playerIntersects=world.x>bounds[0]&&world.x<bounds[2]&&world.y>bounds[1]&&world.y<bounds[3];
            if(!playerIntersects&&world.characterHitSpheres) {
                std::vector<StorylandMissionEnvironment::CharacterHitSphere> spheres;
                if(world.characterHitSpheres(runtime.crime.vcs?0:1,spheres))for(const auto& sphere:spheres)
                    playerIntersects=playerIntersects||storylandGarageContainsPoint(garage,sphere.center,sphere.radius);
            }
        }
        const unsigned capacity=unsigned(std::clamp(garage.maximumCars,0,4));
        if(garage.state==0) {
            if((distance<7.84f||(distance<100&&inVehicle))&&(!inVehicle||stored<capacity)) {
                storylandRestoreVcsGarageVehicles(runtime,garage);
                garage.state=3;
            }
        } else if(garage.state==1) {
            if((((distance>225||(distance>10.2400007f&&!inVehicle))&&!straddling)||
                (inVehicle&&inside>=capacity))||distance>4900)garage.state=2;
        } else if(garage.state==2) {
            const float travel=doors.position-step*(garage.swingDoor?.034f:.068f);
            garage.doorPosition=std::max(0.0f,travel)/doors.height;
            if(playerIntersects)garage.state=3;
            else if(garage.doorPosition==0) {
                garage.state=0;
                storylandStoreVcsGarageVehicles(runtime,garage,actors);
            }
        } else if(garage.state==3) {
            const float travel=doors.position+step*(garage.swingDoor?.0255f:.0595f);
            garage.doorPosition=std::min(doors.height,travel)/doors.height;
            if(garage.doorPosition>=1)garage.state=1;
        }
        if(!runtime.crime.vcs) {
            doors.state=garage.state;doors.position=garage.doorPosition*doors.height;
            world.lcsGarageDoors[garage.handle]=doors;
            if(!storylandApplyLcsGarageDoors(world,garage,doors,error))return false;
        }
    }
    error.clear();return true;
}
