#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_play_test_camera.h"
#include "src/storyland/storyland_traffic_population.h"
#include "src/storyland/storyland_vehicle_population.h"

inline bool storylandGeneratorModelMatches(int32_t generatorModel,int32_t vehicleModel) {
    const int64_t resolved=generatorModel < -1 ? -int64_t(generatorModel) : generatorModel;
    return vehicleModel>=0 && resolved==vehicleModel;
}

inline int32_t storylandMissionCarGenerator(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& actors,const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::VehicleScriptState& car,bool headingKnown=true) {
    for(const auto& generator:world.carGenerators) {
        if(!headingKnown&&(!(std::fabs(generator.x-car.x)<=.05f)||
            !(std::fabs(generator.y-car.y)<=.05f)||!(std::fabs(generator.z-car.z)<=.05f)))continue;
        const int32_t generatorModel=generator.model < -1 && generator.model!=INT32_MIN?
            -generator.model:generator.model;
        if(!storylandGeneratorMatchesMission(generatorModel,{generator.x,generator.y,generator.z},
            generator.heading,car.model,{car.x,car.y,car.z},car.heading,headingKnown))continue;
        const auto end=runtime.vehicles.begin()+std::min(actors.size(),runtime.vehicles.size());
        const bool owned=std::any_of(runtime.vehicles.begin(),end,
            [&](const auto& vehicle){return vehicle.generator==generator.handle;});
        if(!owned)return generator.handle;
    }
    return -1;
}

inline bool storylandUnclaimedGeneratorVehicle(const StorylandPlayTestRuntime& runtime,
    const StorylandMissionEnvironment& world,size_t index) {
    if(index>=runtime.vehicles.size())return false;
    const auto& vehicle=runtime.vehicles[index];
    if(vehicle.scriptHandle>=0||vehicle.generator<0||vehicle.savedGarage>=0||vehicle.craneCarried||
        vehicle.coopControlled||vehicle.coopSeatReserved||vehicle.coopIdentity||vehicle.missionDriver||
        !vehicle.parked||vehicle.hasDriver||!(vehicle.health>0)||vehicle.wrecked||vehicle.burning||
        int(index)==runtime.occupiedVehicle||int(index)==runtime.transitionVehicle||
        int(index)==runtime.coopRemoteVehicle||
        int(index)==storylandRemoteVehicleSlot(runtime)||
        !(std::fabs(vehicle.speed)<=.1f)||!(std::fabs(vehicle.lateralSpeed)<=.1f))return false;
    return std::none_of(world.missionVehicles.begin(),world.missionVehicles.end(),
        [index](const auto& entry) {
            return !entry.second.deleted&&entry.second.spawnedSlot==int32_t(index);
        });
}

inline size_t storylandFindMissionGeneratorVehicle(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& actors,const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::VehicleScriptState& car,int32_t& generatorHandle,
    bool headingKnown=true) {
    generatorHandle=-1;
    for(size_t index=0;index<std::min(actors.size(),runtime.vehicles.size());++index) {
        const auto& vehicle=runtime.vehicles[index];
        const auto& actor=actors[index];
        if(!storylandUnclaimedGeneratorVehicle(runtime,world,index)||
            vehicle.nativeDefinition.modelId!=uint32_t(car.model))continue;
        const auto generator=std::find_if(world.carGenerators.begin(),world.carGenerators.end(),
            [&](const auto& item){return item.handle==vehicle.generator;});
        if(generator==world.carGenerators.end()||
            !storylandGeneratorModelMatches(generator->model,car.model)||
            !(std::fabs(generator->x-car.x)<=.05f)||!(std::fabs(generator->y-car.y)<=.05f)||
            !(std::fabs(generator->z-car.z)<=.05f)||
            (headingKnown&&!(std::fabs(std::remainder(generator->heading-car.heading,360.0f))<=1.0f))||
            !(std::hypot(actor.position.x-generator->x,actor.position.y-generator->y)<=.25f)||
            !(std::fabs(actor.position.z-generator->z)<=2.0f)||
            !(std::fabs(std::remainder(actor.yaw-generator->heading-90.0f,360.0f))<=5.0f))continue;
        generatorHandle=vehicle.generator;
        return index;
    }
    generatorHandle=storylandMissionCarGenerator(runtime,actors,world,car,headingKnown);
    return runtime.vehicles.size();
}

inline void storylandBindMissionCarGenerator(StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& actors,StorylandMissionEnvironment& world,
    int32_t generatorHandle,size_t slot) {
    if(generatorHandle<0)return;
    for(auto& generator:world.carGenerators) {
        if(generator.handle!=generatorHandle)continue;
        const auto end=runtime.vehicles.begin()+std::min(actors.size(),runtime.vehicles.size());
        const bool alreadyGenerated=std::any_of(runtime.vehicles.begin(),end,
            [&](const auto& live){return live.generator==generator.handle;});
        if(!alreadyGenerated&&generator.generations>0)--generator.generations;
        generator.spawnedSlot=int32_t(slot);
        runtime.generatorStates[generator.handle].spawned=true;
        break;
    }
}

inline bool storylandCarGeneratorReady(StorylandPlayTestRuntime::GeneratorState& state,
    uint32_t now,int32_t minimumDelay,int32_t maximumDelay,float distanceSquared,uint32_t& random) {
    if(state.spawned) {
        state.spawned=false;state.waitingForDeparture=true;
        const uint32_t minimum=uint32_t(std::clamp(minimumDelay,0,3600000));
        const uint32_t maximum=uint32_t(std::clamp(maximumDelay,int32_t(minimum),3600000));
        random=random*1664525u+1013904223u;
        state.nextSpawnAt=now+minimum+random%(maximum-minimum+1);
    }
    if(state.waitingForDeparture) {
        if(distanceSquared<=140.0f*140.0f)return false;
        state.waitingForDeparture=false;
    }
    return !state.nextSpawnAt||int32_t(now-state.nextSpawnAt)>=0;
}

inline bool storylandVcsCarGeneratorReady(StorylandPlayTestRuntime::GeneratorState& state,
    uint32_t now,float distanceSquared,float cameraGenerationDistance,bool forceSpawn) {
    if (state.nextSpawnAt && now < state.nextSpawnAt) return false;
    if (!std::isfinite(distanceSquared) || !std::isfinite(cameraGenerationDistance) ||
        cameraGenerationDistance <= 0.0f) return false;
    const float maximumDistance = 110.0f * cameraGenerationDistance;
    if (distanceSquared >= maximumDistance * maximumDistance) {
        state.waitingForDeparture = false;
        return false;
    }
    if (state.waitingForDeparture) return false;
    if (!forceSpawn && distanceSquared < 20.0f * 20.0f) return false;
    return true;
}

inline bool storylandLcsCarGeneratorReady(StorylandPlayTestRuntime::GeneratorState& state,
    uint32_t now,float distanceSquared,float cameraGenerationDistance,bool forceSpawn) {
    if(state.nextSpawnAt && now<state.nextSpawnAt)return false;
    if(!std::isfinite(distanceSquared)||!std::isfinite(cameraGenerationDistance)||
        cameraGenerationDistance<=0.0f)return false;
    const float maximumDistance=140.0f*cameraGenerationDistance;
    if(distanceSquared>=maximumDistance*maximumDistance) {
        state.waitingForDeparture=false;
        return false;
    }
    if(state.waitingForDeparture)return false;
    const float minimumDistance=std::max(0.0f,maximumDistance-20.0f);
    if(!forceSpawn&&distanceSquared<minimumDistance*minimumDistance)return false;
    return true;
}

inline int32_t storylandVcsRandomGeneratorModel(const StorylandPlayTestRuntime& runtime,
    const StorylandMissionEnvironment& world,const StorylandNativePoint& player,uint32_t& random) {
    const auto population=storylandNativePopulationAt(world,player,runtime.clockHour);
    if(!population.available || !population.vcs)return -1;
    const int vehicleClass=storylandVcsPopulationClass(population,false,storylandNativeRandom(random));
    if(vehicleClass<0)return -1;
    uint64_t total=0;
    for(size_t index:runtime.roadModels)if(index<runtime.modelDefinitions.size()) {
        const auto& definition=runtime.modelDefinitions[index];
        if(definition.vehicleClass==vehicleClass)total+=definition.frequency;
    }
    if(total==0 || total>UINT32_MAX)return -1;
    const uint32_t selected=storylandNativeRandomRange(random,uint32_t(total));
    uint64_t cumulative=0;
    for(size_t index:runtime.roadModels)if(index<runtime.modelDefinitions.size()) {
        const auto& definition=runtime.modelDefinitions[index];
        if(definition.vehicleClass!=vehicleClass || !definition.frequency)continue;
        cumulative+=definition.frequency;
        if(selected<=cumulative && definition.modelId<=uint32_t(INT32_MAX))
            return int32_t(definition.modelId);
    }
    return -1;
}

inline int32_t storylandLcsRandomGeneratorModel(const StorylandPlayTestRuntime& runtime,
    const StorylandMissionEnvironment& world,const StorylandNativePoint& player,uint32_t& random) {
    const auto population=storylandNativePopulationAt(world,player,runtime.clockHour);
    if(!population.available||population.vcs)return -1;
    const int vehicleClass=storylandLcsPopulationClass(population.values.civilianCars,
        storylandNativeRandomRange(random,1000));
    uint64_t total=0;
    for(size_t index:runtime.roadModels)if(index<runtime.modelDefinitions.size()) {
        const auto& definition=runtime.modelDefinitions[index];
        if(definition.vehicleClass==vehicleClass)total+=definition.frequency;
    }
    if(total==0||total>UINT32_MAX)return -1;
    const uint32_t selected=storylandNativeRandomRange(random,uint32_t(total));
    uint64_t cumulative=0;
    for(size_t index:runtime.roadModels)if(index<runtime.modelDefinitions.size()) {
        const auto& definition=runtime.modelDefinitions[index];
        if(definition.vehicleClass!=vehicleClass||!definition.frequency)continue;
        cumulative+=definition.frequency;
        if(selected<=cumulative&&definition.modelId<=uint32_t(INT32_MAX))
            return int32_t(definition.modelId);
    }
    return -1;
}

inline void storylandApplyVcsGeneratorColors(StorylandRuntimeVehicle& vehicle,
    int32_t regularSet,int32_t scriptedSet) {
    // Native 0x3D9A58 selects one model colour set for both paint channels.
    // The regular set takes precedence over the scripted set.
    const bool scripted=regularSet<0;
    const int32_t selected=scripted?scriptedSet:regularSet;
    if(selected<0)return;
    const auto& sets=scripted?vehicle.nativeDefinition.scriptColors:vehicle.nativeDefinition.colors;
    if(size_t(selected)>=sets.size())return;
    const auto& colors=sets[size_t(selected)];
    vehicle.primaryColor=colors[0];
    vehicle.secondaryColor=colors[1];
    vehicle.colorSet=scripted?-1:selected;
    vehicle.scriptColorSet=scripted?selected:-1;
}

template<class RemovedSlot>
void storylandEraseGeneratedVehicleSlot(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& environment,std::vector<StorylandNativeTrafficActor>& actors,
    size_t removed,RemovedSlot onRemovedSlot) {
    const auto remap=[&](int32_t& index) {
        if(index==int32_t(removed))index=-1;
        else if(index>int32_t(removed)&&index!=INT32_MAX)--index;
    };
    actors.erase(actors.begin()+removed);
    std::move(runtime.vehicles.begin()+removed+1,runtime.vehicles.end(),
        runtime.vehicles.begin()+removed);
    runtime.vehicles.back()={};
    remap(runtime.occupiedVehicle);remap(runtime.transitionVehicle);remap(runtime.coopRemoteVehicle);
    for(auto& mission:environment.missionVehicles)remap(mission.second.spawnedSlot);
    for(auto& source:environment.carGenerators)remap(source.spawnedSlot);
    for(auto& vehicle:runtime.vehicles) {
        remap(vehicle.lastCollidedSlot);
        if(vehicle.pendingContactSlot==int32_t(removed))vehicle.pendingBodyImpact=false;
        remap(vehicle.pendingContactSlot);
    }
    const auto remapEffects=[&](auto& effects) {
        for(auto& effect:effects) {
            if(effect.vehicle==int32_t(removed))effect.remaining=0;
            remap(effect.vehicle);
        }
    };
    remapEffects(runtime.effects);remapEffects(runtime.worldEffects);
    for(auto& event:runtime.soundEvents)remap(event.vehicle);
    onRemovedSlot(removed);
    runtime.vehiclePositions.clear();runtime.vehicleWorldMatrices.clear();runtime.movementCars.clear();
}

inline size_t storylandVcsParkedGeneratorCount(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& actors) {
    size_t count=0;
    for(size_t slot=0;slot<std::min(actors.size(),runtime.vehicles.size());++slot) {
        const auto& vehicle=runtime.vehicles[slot];
        if(vehicle.generator>=0&&vehicle.scriptHandle<0)++count;
    }
    return count;
}

template<class RemovedSlot>
void storylandRetireDistantGeneratorVehicles(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& environment,std::vector<StorylandNativeTrafficActor>& actors,
    const StorylandNativePoint& playerPosition,float fieldOfView,RemovedSlot onRemovedSlot) {
    if(runtime.paused||runtime.pauseMenu||runtime.visibleArea!=0||!environment.sphereOnScreen)return;
    const float generationDistance=storylandVcsCameraGenerationDistance(fieldOfView);
    if(generationDistance<=0)return;
    bool downward=false;
    std::array<float,3> eye{},target{};
    if(environment.activeCameraVectors&&environment.activeCameraVectors(eye,target)) {
        const float x=target[0]-eye[0],y=target[1]-eye[1],z=target[2]-eye[2];
        const float length=std::hypot(x,y,z);
        downward=std::isfinite(length)&&length>0&&z/length<-.9f;
    }
    // Original 433378 admits source-3 cars; 433508/433574 applies this strict distance.
    for(size_t count=std::min(actors.size(),runtime.vehicles.size());count>0;--count) {
        const size_t slot=count-1;
        const auto& vehicle=runtime.vehicles[slot];
        if(vehicle.generator<0||vehicle.scriptHandle>=0||vehicle.savedGarage>=0||vehicle.craneCarried||
            vehicle.coopControlled||vehicle.coopSeatReserved||vehicle.coopIdentity||
            !vehicle.parked||vehicle.hasDriver||vehicle.missionDriver||
            int(slot)==runtime.occupiedVehicle||int(slot)==runtime.transitionVehicle)continue;
        if(std::any_of(environment.missionVehicles.begin(),environment.missionVehicles.end(),
            [slot](const auto& entry){return !entry.second.deleted&&entry.second.spawnedSlot==int32_t(slot);}))continue;
        const auto& actor=actors[slot];
        const float distance=std::hypot(actor.position.x-playerPosition.x,actor.position.y-playerPosition.y);
        const float limit=storylandVcsTrafficRemovalDistance(generationDistance,false,downward,actor.water);
        if(!std::isfinite(distance)||!(distance>limit)||environment.sphereOnScreen(
            {actor.position.x,actor.position.y,actor.position.z+1},2.0f))continue;
        const auto state=runtime.generatorStates.find(vehicle.generator);
        if(state!=runtime.generatorStates.end())state->second.spawned=false;
        storylandEraseGeneratedVehicleSlot(runtime,environment,actors,slot,onRemovedSlot);
    }
}

// The same ownership, placement and slot-remapping rules serve both platforms.
template<class EnsureVehicle,class RemovedSlot>
void storylandUpdateCarGenerators(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& environment,const StorylandNativeWorld& nativeWorld,
    std::vector<StorylandNativeTrafficActor>& actors,const StorylandNativePoint& playerPosition,
    bool vcs,float fieldOfView,EnsureVehicle ensureVehicleModel,RemovedSlot onRemovedSlot) {
    // Retail SCM car generators become parked world vehicles when
    // their sectors are in range of the player. Reuse only unused
    // parked generator cars; retain protected and occupied cars.
    if(vcs)storylandRetireDistantGeneratorVehicles(runtime,environment,actors,playerPosition,
        fieldOfView,onRemovedSlot);
    runtime.carGeneratorPhase=uint8_t((runtime.carGeneratorPhase+1u)&3u);
    for(size_t generatorIndex=runtime.carGeneratorPhase;
        generatorIndex<environment.carGenerators.size();generatorIndex+=4) {
        auto& generator=environment.carGenerators[generatorIndex];
        const auto ownsGeneratorSlot=[&](size_t slot) {
            if(slot>=actors.size()||slot>=runtime.vehicles.size()||
                runtime.vehicles[slot].generator!=generator.handle)return false;
            const auto mission=environment.missionVehicles.find(runtime.vehicles[slot].scriptHandle);
            return mission==environment.missionVehicles.end()||!mission->second.deleted;
        };
        // Older saves can contain both the generated parked car and
        // its mission replacement. Retain the script car and remove
        // only an unclaimed duplicate at this authored placement.
        if(generator.spawnedSlot>=0&&ownsGeneratorSlot(size_t(generator.spawnedSlot))) {
            const size_t removed=size_t(generator.spawnedSlot);
            const auto& parked=runtime.vehicles[removed];
            const auto& position=actors[removed].position;
            if(storylandUnclaimedGeneratorVehicle(runtime,environment,removed)&&
                parked.nativeDefinition.modelId<=uint32_t(INT32_MAX)&&
                storylandGeneratorModelMatches(generator.model,int32_t(parked.nativeDefinition.modelId))&&
                std::hypot(position.x-generator.x,position.y-generator.y)<=.25f&&
                std::fabs(position.z-generator.z)<=2.0f&&
                std::fabs(std::remainder(actors[removed].yaw-generator.heading-90.0f,360.0f))<=5.0f) {
                for(const auto& entry:environment.missionVehicles) {
                    const auto& scripted=entry.second;const int32_t kept=scripted.spawnedSlot;
                    if(scripted.deleted||!scripted.alive||!storylandGeneratorModelMatches(generator.model,scripted.model)||kept<0||
                        size_t(kept)>=actors.size()||size_t(kept)>=runtime.vehicles.size()||
                        runtime.vehicles[size_t(kept)].scriptHandle!=entry.first||
                        runtime.vehicles[size_t(kept)].health<=0||
                        runtime.vehicles[size_t(kept)].generator>=0||
                        std::fabs(scripted.x-generator.x)>.05f||std::fabs(scripted.y-generator.y)>.05f||
                        std::fabs(scripted.z-generator.z)>2.0f||
                        std::fabs(std::remainder(scripted.heading-generator.heading,360.0f))>1.0f)continue;
                    const auto& live=actors[size_t(kept)];
                    if(std::hypot(live.position.x-generator.x,live.position.y-generator.y)>.25f||
                        std::fabs(live.position.z-generator.z)>2.0f||
                        std::fabs(std::remainder(live.yaw-generator.heading-90.0f,360.0f))>5.0f)continue;
                    runtime.vehicles[size_t(kept)].generator=generator.handle;
                    generator.spawnedSlot=kept;
                    storylandEraseGeneratedVehicleSlot(runtime,environment,actors,removed,onRemovedSlot);
                    break;
                }
            }
        }
        auto& generatorState=runtime.generatorStates[generator.handle];
        const bool hasOwnedVehicle=generator.spawnedSlot>=0&&ownsGeneratorSlot(size_t(generator.spawnedSlot));
        if(hasOwnedVehicle && generator.spawnedSlot==runtime.occupiedVehicle &&
            runtime.playerVehicleSeat<0) {
            auto& vehicle=runtime.vehicles[size_t(generator.spawnedSlot)];
            vehicle.generator=-1;
            generator.spawnedSlot=-1;
            if(generator.model<0)generator.model=-1;
            generatorState.spawned=false;
            generatorState.waitingForDeparture=true;
            generatorState.nextSpawnAt=(generatorState.nextSpawnAt ? generatorState.nextSpawnAt :
                environment.gameMilliseconds)+60000u;
        } else if(hasOwnedVehicle) {
            generatorState.spawned=true;
        }
        if((!vcs&&environment.carGeneratorsDisabled)||generator.generations==0||
            (generator.spawnedSlot>=0&&ownsGeneratorSlot(size_t(generator.spawnedSlot))))continue;
        const float dx=generator.x-playerPosition.x,dy=generator.y-playerPosition.y;
        const float distanceSquared=dx*dx+dy*dy;
        generatorState.spawned=false;
        const float scale=storylandVcsCameraGenerationDistance(fieldOfView);
        const bool ready=vcs?storylandVcsCarGeneratorReady(generatorState,environment.gameMilliseconds,
            distanceSquared,scale,generator.forceSpawn!=0):
            storylandLcsCarGeneratorReady(generatorState,environment.gameMilliseconds,
                distanceSquared,scale,generator.forceSpawn!=0);
        if(!ready)continue;
        generator.spawnedSlot=-1;
        for(size_t slot=0;slot<actors.size();++slot)
            if(ownsGeneratorSlot(slot)) {generator.spawnedSlot=int32_t(slot);break;}
        if(generator.spawnedSlot>=0)continue;
        // Original 3D9480 refuses parked creation at ten, before requesting its model.
        if(vcs&&storylandVcsParkedGeneratorCount(runtime,actors)>=10)continue;
        int32_t requestedModel=generator.model;
        if(requestedModel<0) {
            const int64_t previous= -int64_t(requestedModel);
            const bool previousResident=requestedModel < -1 && previous<=INT32_MAX &&
                std::any_of(runtime.modelDefinitions.begin(),runtime.modelDefinitions.end(),
                    [previous](const auto& item){return item.modelId==uint32_t(previous);});
            requestedModel=previousResident?int32_t(previous):vcs?
                storylandVcsRandomGeneratorModel(runtime,environment,playerPosition,environment.nativeRandomState):
                storylandLcsRandomGeneratorModel(runtime,environment,playerPosition,environment.nativeRandomState);
            if(requestedModel<=1)continue;
            generator.model=-requestedModel;
        }
        if(requestedModel<0 || !ensureVehicleModel(uint32_t(requestedModel)))continue;
        const auto definition=std::find_if(runtime.modelDefinitions.begin(),
            runtime.modelDefinitions.end(),[&](const auto& item) {
                return item.modelId==uint32_t(requestedModel)&&
                    (item.domain==StorylandVehicleDomain::Road ||
                     item.domain==StorylandVehicleDomain::Water);
            });
        if(definition==runtime.modelDefinitions.end())continue;
        const size_t modelIndex=size_t(definition-runtime.modelDefinitions.begin());
        const float halfWidth=modelIndex<runtime.modelExtents.size()?
            runtime.modelExtents[modelIndex][0]:0.8f;
        const float halfLength=modelIndex<runtime.modelExtents.size()?
            runtime.modelExtents[modelIndex][1]:1.7f;
        const float placementAngle=(generator.heading+90.0f)*.0174532925199433f;
        const float placementX=std::cos(placementAngle),placementY=std::sin(placementAngle);
        const auto overlapsPlacement=[&](float x,float y,float z,float yaw,float width,float length) {
            if(std::fabs(z-generator.z)>=2.0f)return false;
            const float angle=yaw*.0174532925199433f,forwardX=std::cos(angle),forwardY=std::sin(angle);
            const std::array<std::array<float,2>,4> axes{{
                {{placementX,placementY}},{{-placementY,placementX}},
                {{forwardX,forwardY}},{{-forwardY,forwardX}}}};
            for(const auto& axis:axes) {
                const float placementRadius=halfLength*std::fabs(placementX*axis[0]+placementY*axis[1])+
                    halfWidth*std::fabs(-placementY*axis[0]+placementX*axis[1]);
                const float otherRadius=length*std::fabs(forwardX*axis[0]+forwardY*axis[1])+
                    width*std::fabs(-forwardY*axis[0]+forwardX*axis[1]);
                if(std::fabs((x-generator.x)*axis[0]+(y-generator.y)*axis[1])>=
                    placementRadius+otherRadius+0.05f)return false;
            }
            return true;
        };
        bool occupied=runtime.occupiedVehicle<0&&overlapsPlacement(
            playerPosition.x,playerPosition.y,playerPosition.z,0,.35f,.35f);
        for(size_t index=0;index<std::min(actors.size(),runtime.vehicles.size());++index) {
            const auto& actor=actors[index];const auto& vehicle=runtime.vehicles[index];
            const auto mission=environment.missionVehicles.find(vehicle.scriptHandle);
            if(vehicle.craneCarried||(mission!=environment.missionVehicles.end()&&mission->second.deleted))continue;
            if(overlapsPlacement(actor.position.x,actor.position.y,actor.position.z,actor.yaw,
                vehicle.halfWidth,vehicle.halfLength)) {occupied=true;break;}
        }
        // A CREATE_CAR in this VM tick has not been bound yet. Reserve
        // its footprint before allowing the parked generator to spawn.
        if(!occupied)for(const auto& entry:environment.missionVehicles) {
            const auto& scripted=entry.second;
            if(scripted.deleted||!scripted.alive)continue;
            float width=0.8f,length=1.7f;
            const auto model=std::find_if(runtime.modelDefinitions.begin(),
                runtime.modelDefinitions.end(),[&](const auto& item) {
                    return item.modelId==uint32_t(scripted.model);
                });
            if(model!=runtime.modelDefinitions.end()) {
                const size_t index=size_t(model-runtime.modelDefinitions.begin());
                if(index<runtime.modelExtents.size()) {
                    width=runtime.modelExtents[index][0];length=runtime.modelExtents[index][1];
                }
            }
            if(overlapsPlacement(scripted.x,scripted.y,scripted.z,scripted.heading+90.0f,width,length)) {
                occupied=true;break;
            }
        }
        if(occupied)continue;
        size_t slot=actors.size();
        if(slot>=runtime.vehicles.size()) {
            for(size_t index=0;index<actors.size();++index) {
                const auto& actor=actors[index];
                const auto& vehicle=runtime.vehicles[index];
                if(int(index)==runtime.occupiedVehicle||
                    int(index)==runtime.transitionVehicle||
                    int(index)==storylandRemoteVehicleSlot(runtime)||
                    vehicle.scriptHandle>=0||vehicle.savedGarage>=0||vehicle.craneCarried||
                    !vehicle.parked||vehicle.generator<0)continue;
                const float separation=std::hypot(actor.position.x-playerPosition.x,
                    actor.position.y-playerPosition.y);
                if(separation>175.0f) {slot=index;break;}
            }
        }
        if(slot>=runtime.vehicles.size())break;
        StorylandNativeTrafficActor actor;
        actor.vehicle=true;
        actor.water=definition->domain==StorylandVehicleDomain::Water;
        actor.position={generator.x,generator.y,generator.z};
        actor.yaw=generator.heading+90.0f;
        StorylandRuntimeVehicle vehicle;
        vehicle.model=modelIndex;
        vehicle.hasDriver=false;
        vehicle.parked=true;
        vehicle.generator=generator.handle;
        if(vcs) {
            const auto security=storylandVcsParkedVehicleSecurity(environment.nativeRandomState,
                generator.alarm,generator.doorLock);
            vehicle.doorLock=security.doorLock;vehicle.alarmMilliseconds=security.alarmMilliseconds;
        } else {
            runtime.populationRandom=runtime.populationRandom*1664525u+1013904223u;
            vehicle.doorLock=int(runtime.populationRandom%100)<std::clamp(generator.doorLock,0,100)?2:1;
            vehicle.alarmMilliseconds=int((runtime.populationRandom>>8)%100)<std::clamp(generator.alarm,0,100)?65535:0;
        }
        vehicle.engineOn=false;
        if(!vcs) {
            vehicle.primaryColor=generator.color1;
            vehicle.secondaryColor=generator.color2;
        }
        storylandApplyVehicleDefinition(vehicle,*definition);
        vehicle.bikeStandingOverride=definition->twoWheels();
        if(vcs)storylandApplyVcsGeneratorColors(vehicle,generator.color1,generator.color2);
        if(modelIndex<runtime.modelExtents.size()) {
            vehicle.halfWidth=runtime.modelExtents[modelIndex][0];
            vehicle.halfLength=runtime.modelExtents[modelIndex][1];
            vehicle.bodyHeight=runtime.modelExtents[modelIndex][2];
        }
        if(!vehicle.nativeDefinition.hasNativeWheelDimensions&&modelIndex<runtime.modelWheelRadii.size())
            vehicle.wheelRadius=std::max(0.12f,runtime.modelWheelRadii[modelIndex]);
        // Defer obstructed or unsupported generators until the native
        // collision is ready and the complete vehicle footprint is free.
        if(actor.water) {
            const auto collision=nativeWorld.collisionModels.find(definition->modelId);
            if(collision==nativeWorld.collisionModels.end() ||
                !std::isfinite(collision->second.minimum.z))continue;
            float waterHeight=0;
            if(!storylandNativeWaterHeight(nativeWorld,generator.x,generator.y,waterHeight))continue;
            float baseHeight=generator.z;
            if((vcs&&(baseHeight==-100.0f||baseHeight<=-250.0f))||
                (!vcs&&baseHeight<=-100.0f)) {
                const StorylandNativePoint top{generator.x,generator.y,1000.0f};
                const StorylandNativePoint bottom{generator.x,generator.y,-1000.0f};
                const float fraction=storylandTraceNativeMapGround(nativeWorld,top,bottom);
                baseHeight=fraction<1.0f?top.z+(bottom.z-top.z)*fraction:20.0f;
            }
            actor.position.z=baseHeight-collision->second.minimum.z;
            if(!std::isfinite(actor.position.z))continue;
        } else if(!storylandPlaceParkedRoadVehicle(vehicle,actor,nativeWorld))continue;
        if(slot<actors.size()) {
            for(auto& old:environment.carGenerators)
                if(old.spawnedSlot==int(slot))old.spawnedSlot=-1;
        }
        if(slot==actors.size())actors.push_back(actor);
        else actors[slot]=actor;
        for (auto& contact : runtime.vehicles) {
            if (contact.lastCollidedSlot == int32_t(slot)) contact.lastCollidedSlot = -1;
            if (contact.pendingContactSlot == int32_t(slot)) {
                contact.pendingContactSlot = -1;
                contact.pendingBodyImpact = false;
            }
        }
        runtime.vehicles[slot]=vehicle;
        generator.spawnedSlot=int(slot);
        generatorState.spawned=true;
        generatorState.nextSpawnAt=environment.gameMilliseconds+4u;
        if(generator.generations>0)--generator.generations;
    }
}
