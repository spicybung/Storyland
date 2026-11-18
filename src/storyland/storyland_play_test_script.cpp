#include "src/storyland/storyland_play_test_script.h"
#include "src/storyland/storyland_time.h"
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test_weapons.h"
#include "src/storyland/storyland_car_generators.h"
#include "src/storyland/storyland_mission_vehicle_transform.h"
#include "src/storyland/storyland_native_body.h"
#include "src/storyland/storyland_script_object.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>
#include <sstream>
namespace {
std::string trim(const std::string& text) {
    const auto first=text.find_first_not_of(" \t\r\n");
    if(first==std::string::npos) return {};
    return text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
}
}
size_t storylandMissionVehicleSlot(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& vehicles,StorylandMissionEnvironment& environment,
    const StorylandNativePoint& player) {
    if(vehicles.size()<runtime.vehicles.size())return vehicles.size();
    size_t selected=runtime.vehicles.size();
    float farthest=-1;
    for(size_t slot=0;slot<std::min(vehicles.size(),runtime.vehicles.size());++slot) {
        const auto& vehicle=runtime.vehicles[slot];
        if(int(slot)==runtime.occupiedVehicle||int(slot)==runtime.transitionVehicle||
            int(slot)==runtime.coopRemoteVehicle||int(slot)==storylandRemoteVehicleSlot(runtime)||
            vehicle.savedGarage>=0||vehicle.coopControlled||vehicle.coopSeatReserved||
            vehicle.coopIdentity||vehicle.missionDriver)continue;
        if(std::any_of(environment.missionVehicles.begin(),environment.missionVehicles.end(),
            [slot](const auto& entry){return !entry.second.deleted&&entry.second.persistent&&
                entry.second.spawnedSlot==int32_t(slot);}))continue;
        if(vehicle.craneCarried||(vehicle.scriptHandle>=0&&
            std::any_of(environment.lcsCranes.cranes.begin(),environment.lcsCranes.cranes.end(),
                [&](const auto& crane){return crane.vehicle==vehicle.scriptHandle;})))continue;
        const auto mission=environment.missionVehicles.find(vehicle.scriptHandle);
        if(vehicle.scriptHandle>=0&&(mission==environment.missionVehicles.end()||
            (!mission->second.deleted&&(
                mission->second.persistent||mission->second.drivingToTarget||
                mission->second.aiActive||environment.introMissionProtected))))continue;
        bool missionPassenger=false;
        if(vehicle.scriptHandle>=0)for(const auto& character:environment.missionCharacters)
            if(character.second.persistent&&character.second.alive&&character.second.vehicle==vehicle.scriptHandle)missionPassenger=true;
        if(missionPassenger)continue;
        const auto& position=vehicles[slot].position;
        const float distance=std::hypot(position.x-player.x,position.y-player.y);
        if(distance>farthest) {farthest=distance;selected=slot;}
    }
    if(selected<runtime.vehicles.size()) {
        for(auto& entry:environment.missionVehicles)
            if(entry.second.spawnedSlot==int(selected)) {
                entry.second.spawnedSlot=-1;
                if(!entry.second.persistent) {
                    entry.second.deleted=true;entry.second.alive=false;
                    environment.vcsVehicleFlag28.erase(entry.first);
                }
            }
        for(auto& generator:environment.carGenerators)
            if(generator.spawnedSlot==int(selected))generator.spawnedSlot=-1;
    }
    return selected;
}

bool storylandJoinMissionCarWithRoad(const StorylandNativePaths& paths,StorylandNativeTrafficActor& actor) {
    // CPathFind::FindNodeClosestToCoorsFavourDirection, then the nearest
    // linked node, oriented along the vehicle's current heading.
    uint32_t closest=UINT32_MAX;float best=10000.0f;
    const float angle=actor.yaw*0.0174532925199433f,dirX=std::cos(angle),dirY=std::sin(angle);
    for(size_t index=0;index<paths.nodes.size();++index) {
        const auto& node=paths.nodes[index];
        if(!node.vehicle||node.water!=actor.water||node.disabled||node.links.empty())continue;
        const float x=node.position.x-actor.position.x,y=node.position.y-actor.position.y;
        float score=std::fabs(x)+std::fabs(y)+3*std::fabs(node.position.z-actor.position.z);
        if(score>=best)continue;
        const float length=std::hypot(x,y);
        score-=(length>.0001f?(x*dirX+y*dirY)/length-1.0f:-1.0f)*20.0f;
        if(score<best) {best=score;closest=uint32_t(index);}
    }
    if(closest==UINT32_MAX)return false;
    uint32_t previous=UINT32_MAX;best=std::numeric_limits<float>::infinity();
    const auto& node=paths.nodes[closest];
    for(const auto& link:node.links) {
        if(link.target>=paths.nodes.size())continue;
        const auto& target=paths.nodes[link.target];
        if(!target.vehicle||target.water!=actor.water||target.disabled)continue;
        const float distance=std::hypot(target.position.x-node.position.x,target.position.y-node.position.y);
        if(distance<best&&distance>.001f) {best=distance;previous=link.target;}
    }
    if(previous==UINT32_MAX)return false;
    const auto& before=paths.nodes[previous].position;
    if((node.position.x-before.x)*dirX+(node.position.y-before.y)*dirY<0)std::swap(previous,closest);
    actor.node=previous;actor.nextNode=closest;actor.previousNode=UINT32_MAX;
    actor.lane=0;actor.laneOffset=0;actor.crossingCommitted=false;
    const auto& from=paths.nodes[previous].position;const auto& to=paths.nodes[closest].position;
    const float x=to.x-from.x,y=to.y-from.y,z=to.z-from.z,length=std::hypot(x,y,z);
    actor.distance=std::clamp(((actor.position.x-from.x)*x+(actor.position.y-from.y)*y+
        (actor.position.z-from.z)*z)/length,0.0f,length);
    actor.groundOffset=actor.position.z-(from.z+z*actor.distance/length);
    return true;
}

void storylandApplyMissionVehicleAutopilot(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& vehicles,StorylandMissionEnvironment& environment,
    const StorylandNativePaths& paths) {
    for(auto& entry:environment.missionVehicles) {
        auto& state=entry.second;const int32_t slot=state.spawnedSlot;
        if(state.deleted||slot<0||size_t(slot)>=vehicles.size()||size_t(slot)>=runtime.vehicles.size()||
            runtime.vehicles[size_t(slot)].scriptHandle!=entry.first)continue;
        auto& vehicle=runtime.vehicles[size_t(slot)];
        vehicle.autoPilotMission=state.autoPilotMission;
        vehicles[size_t(slot)].avoidLevelTransitions=state.avoidLevelTransitions;
        vehicle.engineOn=state.engineOn;
        vehicle.missionDriver=state.driver>=2&&environment.missionCharacters.count(state.driver)&&
            environment.missionCharacters.at(state.driver).alive;
        vehicle.boatAnchored=state.boatAnchored;vehicle.boatAnchorHeading=state.boatAnchorHeading;
        if(state.autoPilotMission==1&&state.autoPilotRoutePending&&
            storylandJoinMissionCarWithRoad(paths,vehicles[size_t(slot)]))state.autoPilotRoutePending=false;
    }
}

namespace {
void erasePlayTestVehicleSlot(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& vehicles,StorylandMissionEnvironment& environment,size_t slot) {
    const auto remap=[&](int32_t& index) {
        if(index==int32_t(slot))index=-1;
        else if(index>int32_t(slot)&&index!=INT32_MAX)--index;
    };
    const bool pendingOtherVehicle=environment.playerWarped&&environment.inVehicle&&
        environment.occupiedMissionVehicle>=0&&
        environment.occupiedMissionVehicle!=runtime.vehicles[slot].scriptHandle;
    if(runtime.occupiedVehicle==int32_t(slot)&&!pendingOtherVehicle) {
        environment.inVehicle=false;environment.occupiedMissionVehicle=-1;environment.playerWarped=true;
    }
    if(runtime.transitionVehicle==int32_t(slot))runtime.vehicleAction=StorylandVehicleAction::None;
    vehicles.erase(vehicles.begin()+slot);
    std::move(runtime.vehicles.begin()+slot+1,runtime.vehicles.end(),runtime.vehicles.begin()+slot);
    runtime.vehicles.back()={};
    remap(runtime.occupiedVehicle);remap(runtime.transitionVehicle);remap(runtime.coopRemoteVehicle);
    for(auto& contact:runtime.vehicles) {
        if(contact.pendingContactSlot==int32_t(slot))contact.pendingBodyImpact=false;
        remap(contact.lastCollidedSlot);remap(contact.pendingContactSlot);
    }
    for(auto& entry:environment.missionVehicles)remap(entry.second.spawnedSlot);
    for(auto& generator:environment.carGenerators)remap(generator.spawnedSlot);
    const auto remapEffect=[&](StorylandRuntimeEffect& effect) {
        if(effect.vehicle==int32_t(slot))effect.remaining=0;
        remap(effect.vehicle);
    };
    for(auto& effect:runtime.effects)remapEffect(effect);
    for(auto& effect:runtime.worldEffects)remapEffect(effect);
    runtime.soundEvents.erase(std::remove_if(runtime.soundEvents.begin(),runtime.soundEvents.end(),
        [&](const auto& event){return event.vehicle==int32_t(slot);}),runtime.soundEvents.end());
    for(auto& event:runtime.soundEvents)remap(event.vehicle);
    runtime.vehiclePositions.clear();runtime.vehicleWorldMatrices.clear();runtime.movementCars.clear();
}
}

void storylandRemoveMissionVehicle(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& vehicles,StorylandMissionEnvironment& environment,int32_t handle) {
    const auto found=environment.missionVehicles.find(handle);
    if(found==environment.missionVehicles.end())return;
    auto& state=found->second;
    int32_t slot=state.spawnedSlot;
    state.deleted=true;state.alive=false;state.health=0;
    environment.vcsVehicleFlag28.erase(handle);
    environment.missionCleanupVehicles.erase(handle);
    if(slot<0||size_t(slot)>=vehicles.size()||size_t(slot)>=runtime.vehicles.size()||
        runtime.vehicles[size_t(slot)].scriptHandle!=handle) {
        slot=-1;
        for(size_t index=0;index<std::min(vehicles.size(),runtime.vehicles.size());++index)
            if(runtime.vehicles[index].scriptHandle==handle) {slot=int32_t(index);break;}
        state.spawnedSlot=slot;
        if(slot<0)return;
    }
    erasePlayTestVehicleSlot(runtime,vehicles,environment,size_t(slot));
}

void storylandClearMissionVehicleSpace(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& vehicles,StorylandMissionEnvironment& environment,
    const StorylandNativeWorld& world,int32_t handle) {
    const auto created=environment.missionVehicles.find(handle);
    if(created==environment.missionVehicles.end()||created->second.deleted)return;
    const auto definitionFor=[&](int32_t model)->const StorylandVehicleDefinition* {
        const auto found=std::find_if(runtime.modelDefinitions.begin(),runtime.modelDefinitions.end(),
            [&](const auto& definition){return definition.modelId==uint32_t(model);});
        return found==runtime.modelDefinitions.end()?nullptr:&*found;
    };
    const auto bodyFor=[&](const StorylandRuntimeVehicle& vehicle,
        const StorylandNativeTrafficActor& actor,
        const StorylandMissionEnvironment::VehicleScriptState* scripted) {
        StorylandNativeBody body;
        const auto& definition=vehicle.nativeDefinition;
        body.collision=definition.collision.get();
        if(!body.collision) {
            const auto collision=world.collisionModels.find(definition.modelId);
            if(collision!=world.collisionModels.end())body.collision=&collision->second;
        }
        if(scripted)body.collisionTransform=storylandMissionVehicleWorldMatrix(vehicle,actor,*scripted);
        else body.collisionTransform=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
            {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
            vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
            actor.water),definition.previewRoot);
        return body;
    };
    const auto& target=created->second;
    const auto* definition=definitionFor(target.model);
    if(!definition)return;
    StorylandRuntimeVehicle targetVehicle;
    targetVehicle.nativeDefinition=*definition;
    StorylandNativeTrafficActor targetActor;
    targetActor.position={target.x,target.y,target.z};targetActor.yaw=target.heading+90;
    targetActor.water=definition->domain==StorylandVehicleDomain::Water;
    StorylandNativeBody targetBody=bodyFor(targetVehicle,targetActor,&target);
    bool constructed=false;
    for(size_t slot=0;slot<std::min(vehicles.size(),runtime.vehicles.size());++slot) {
        if(runtime.vehicles[slot].scriptHandle!=handle)continue;
        targetBody=bodyFor(runtime.vehicles[slot],vehicles[slot],&target);
        constructed=true;break;
    }
    if(!targetBody.collision)return;
    int32_t adoptedGenerator=-1;
    const size_t adoptedSlot=constructed?runtime.vehicles.size():
        storylandFindMissionGeneratorVehicle(runtime,vehicles,environment,target,adoptedGenerator,false);
    // VCS CREATE_CAR (0x3D03B0) invokes 0x25A428 before adding the car.
    // Scan the live vehicle pool, including unclaimed generated cars, before
    // collision response can push an overlapping parked car out of this space.
    const auto protectedHandle=[&](int32_t previous) {
        if(previous==handle||previous==environment.remoteVehicle||
            previous==environment.remoteControl.vehicle||
            (environment.inVehicle&&previous==environment.occupiedMissionVehicle))return true;
        if(previous>=0&&(std::any_of(environment.lcsCranes.cranes.begin(),environment.lcsCranes.cranes.end(),
                [&](const auto& crane){return crane.vehicle==previous;})||
            std::any_of(environment.vehicleRopes.begin(),environment.vehicleRopes.end(),
                [&](const auto& rope){return rope.second.attachedVehicle==previous;})))return true;
        return previous>=0&&std::any_of(environment.missionCharacters.begin(),environment.missionCharacters.end(),
            [&](const auto& character){return character.second.persistent&&character.second.alive&&
                character.second.vehicle==previous;});
    };
    const auto removeScripted=[&](int32_t previous) {
        std::vector<int32_t> passengers;
        for(const auto& character:environment.missionCharacters)
            if(character.second.vehicle==previous)passengers.push_back(character.first);
        for(int32_t passenger:passengers)storylandDeleteMissionCharacter(environment,passenger);
        storylandRemoveMissionVehicle(runtime,vehicles,environment,previous);
    };
    for(size_t count=std::min(vehicles.size(),runtime.vehicles.size());count>0;--count) {
        const size_t slot=count-1;
        const auto& vehicle=runtime.vehicles[slot];
        const int32_t previous=vehicle.scriptHandle;
        const auto scripted=environment.missionVehicles.find(previous);
        // The constructor immediately replaces this eligible generator slot.
        // Keeping its allocation intact avoids counting finite generations twice.
        if(slot==adoptedSlot||int(slot)==runtime.occupiedVehicle||int(slot)==runtime.transitionVehicle||
            int(slot)==runtime.coopRemoteVehicle||
            int(slot)==storylandRemoteVehicleSlot(runtime)||vehicle.savedGarage>=0||
            vehicle.craneCarried||vehicle.coopControlled||vehicle.coopSeatReserved||vehicle.coopIdentity||
            vehicle.missionDriver||
            std::any_of(environment.missionVehicles.begin(),environment.missionVehicles.end(),
                [slot](const auto& entry){return !entry.second.deleted&&entry.second.persistent&&
                    entry.second.spawnedSlot==int32_t(slot);})||
            (previous>=0&&(protectedHandle(previous)||scripted==environment.missionVehicles.end()||
                scripted->second.persistent)))continue;
        const auto body=bodyFor(vehicle,vehicles[slot],
            scripted==environment.missionVehicles.end()?nullptr:&scripted->second);
        if(!body.collision||!storylandNativeVehicleBodiesOverlap({},targetBody,{},body))continue;
        if(scripted!=environment.missionVehicles.end())removeScripted(previous);
        else erasePlayTestVehicleSlot(runtime,vehicles,environment,slot);
    }
    std::vector<int32_t> remove;
    for(const auto& entry:environment.missionVehicles) {
        const auto& previous=entry.second;
        if(previous.deleted||previous.persistent||protectedHandle(entry.first))continue;
        const bool constructed=std::any_of(runtime.vehicles.begin(),
            runtime.vehicles.begin()+std::min(vehicles.size(),runtime.vehicles.size()),
            [&](const auto& vehicle){return vehicle.scriptHandle==entry.first;});
        if(constructed)continue;
        const auto* previousDefinition=definitionFor(previous.model);
        if(!previousDefinition)continue;
        StorylandRuntimeVehicle previousVehicle;
        previousVehicle.nativeDefinition=*previousDefinition;
        StorylandNativeTrafficActor previousActor;
        previousActor.position={previous.x,previous.y,previous.z};previousActor.yaw=previous.heading+90;
        previousActor.water=previousDefinition->domain==StorylandVehicleDomain::Water;
        const auto body=bodyFor(previousVehicle,previousActor,&previous);
        if(body.collision&&storylandNativeVehicleBodiesOverlap({},targetBody,{},body))remove.push_back(entry.first);
    }
    for(int32_t previous:remove)removeScripted(previous);
}

namespace {
template<class Contains>
void clearMissionEntities(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& vehicles,std::vector<StorylandNativeTrafficActor>& pedestrians,
    StorylandMissionEnvironment& environment,const Contains& inside,bool cars) {
    const auto remap=[](int& index,size_t removed,int missing=-1) {
        if(index==int(removed))index=missing;
        else if(index>int(removed)&&index!=INT32_MAX)--index;
    };
    if(cars) {
        for(size_t count=std::min(vehicles.size(),runtime.vehicles.size());count>0;--count) {
            const size_t slot=count-1;
            const auto& vehicle=runtime.vehicles[slot];
            const auto scripted=environment.missionVehicles.find(vehicle.scriptHandle);
            const bool missionOwned=vehicle.scriptHandle>=0&&
                (scripted==environment.missionVehicles.end()||scripted->second.persistent);
            const bool craneOwned=vehicle.craneCarried||(vehicle.scriptHandle>=0&&
                std::any_of(environment.lcsCranes.cranes.begin(),environment.lcsCranes.cranes.end(),
                    [&](const auto& crane){return crane.vehicle==vehicle.scriptHandle;}));
            const bool missionPassenger=vehicle.scriptHandle>=0&&
                std::any_of(environment.missionCharacters.begin(),environment.missionCharacters.end(),
                    [&](const auto& entry){return entry.second.persistent&&entry.second.alive&&
                        entry.second.vehicle==vehicle.scriptHandle;});
            if(int(slot)==runtime.occupiedVehicle||int(slot)==runtime.transitionVehicle||
                int(slot)==storylandRemoteVehicleSlot(runtime)||missionOwned||missionPassenger||
                craneOwned||vehicle.savedGarage>=0||!inside(vehicles[slot].position))continue;
            if(scripted!=environment.missionVehicles.end()) {
                scripted->second.deleted=true;scripted->second.alive=false;
                environment.vcsVehicleFlag28.erase(vehicle.scriptHandle);
                environment.missionCleanupVehicles.erase(vehicle.scriptHandle);
                std::vector<int32_t> passengers;
                for(const auto& entry:environment.missionCharacters)
                    if(entry.second.vehicle==vehicle.scriptHandle)passengers.push_back(entry.first);
                for(int32_t handle:passengers)storylandDeleteMissionCharacter(environment,handle);
            }
            erasePlayTestVehicleSlot(runtime,vehicles,environment,slot);
        }
    } else {
        std::set<int32_t> clearedCharacters;
        for(auto character=environment.missionCharacters.begin();character!=environment.missionCharacters.end();) {
            const auto& ped=character->second;
            if(!ped.persistent&&inside({ped.x,ped.y,ped.z})) {
                const int32_t handle=character->first;
                ++character;
                storylandDeleteMissionCharacter(environment,handle);
                clearedCharacters.insert(handle);
            } else ++character;
        }
        for(size_t count=std::min(pedestrians.size(),runtime.pedestrians.size());count>0;--count) {
            const size_t slot=count-1;
            const int32_t handle=runtime.pedestrians[slot].scriptHandle;
            const auto scripted=environment.missionCharacters.find(handle);
            if(scripted!=environment.missionCharacters.end()&&scripted->second.persistent)continue;
            if(!clearedCharacters.count(handle)&&!inside(pedestrians[slot].position))continue;
            pedestrians.erase(pedestrians.begin()+slot);
            std::move(runtime.pedestrians.begin()+slot+1,runtime.pedestrians.end(),runtime.pedestrians.begin()+slot);
            runtime.pedestrians.back()={};
            remap(runtime.targetedPed,slot);remap(runtime.arrestOfficer,slot);
            remap(runtime.playerMelee.target,slot);
            for(auto& ped:runtime.pedestrians)remap(ped.melee.target,slot);
            for(auto& entry:runtime.missionCombat)remap(entry.second.melee.target,slot);
            if(runtime.targetIndicator.ambient==int32_t(slot))runtime.targetIndicator.reset();
            else remap(runtime.targetIndicator.ambient,slot);
            const auto remapOwner=[&](int& owner) {
                if(owner>=0&&size_t(owner)<runtime.pedestrians.size())remap(owner,slot,INT32_MAX);
            };
            for(auto& projectile:runtime.projectiles)remapOwner(projectile.owner);
            for(auto& effect:runtime.effects)remapOwner(effect.owner);
            for(auto& effect:runtime.worldEffects)remapOwner(effect.owner);
            runtime.soundEvents.erase(std::remove_if(runtime.soundEvents.begin(),runtime.soundEvents.end(),
                [&](const auto& event){return event.speaker==int32_t(slot);}),runtime.soundEvents.end());
            for(auto& event:runtime.soundEvents) {
                if(event.speaker>=0&&size_t(event.speaker)<runtime.pedestrians.size())remap(event.speaker,slot);
            }
        }
        runtime.movementPedestrians.clear();
        if(runtime.targetedPed<0)runtime.targetVisible=false;
        if(runtime.arrestOfficer<0)runtime.arrestProgress=0;
    }
}
}

void storylandClearMissionArea(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& vehicles,std::vector<StorylandNativeTrafficActor>& pedestrians,
    StorylandMissionEnvironment& environment,const std::array<float,6>& bounds,bool cars) {
    const auto inside=[&](const StorylandNativePoint& point) {
        return point.x>=bounds[0]&&point.x<=bounds[3]&&point.y>=bounds[1]&&point.y<=bounds[4]&&
            point.z>=bounds[2]&&point.z<=bounds[5];
    };
    clearMissionEntities(runtime,vehicles,pedestrians,environment,inside,cars);
}

void storylandClearMissionRadius(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& vehicles,std::vector<StorylandNativeTrafficActor>& pedestrians,
    StorylandMissionEnvironment& environment,const StorylandNativePoint& center,float radius) {
    if(!std::isfinite(center.x)||!std::isfinite(center.y)||!std::isfinite(center.z)||
        !std::isfinite(radius)||radius<0)return;
    // Retail 1984A8 clears entity pools by horizontal distance; Z is not tested.
    const float squaredRadius=radius*radius;
    const auto inside=[&](const StorylandNativePoint& point) {
        const float x=point.x-center.x,y=point.y-center.y;
        return x*x+y*y<squaredRadius;
    };
    clearMissionEntities(runtime,vehicles,pedestrians,environment,inside,true);
    clearMissionEntities(runtime,vehicles,pedestrians,environment,inside,false);
}

StorylandRuntimeCommandStatus storylandExecutePlayTestCommand(const std::string& source,
    StorylandPlayTestRuntime& runtime,StorylandNativePoint& player,float& heading,
    StorylandRuntimeCommandResult& result) {
    result={};std::string line=trim(source);
    if(!line.empty()&&line.back()==';') line=trim(line.substr(0,line.size()-1));
    const size_t opening=line.find('(');
    if(opening==std::string::npos||line.empty()||line.back()!=')') return StorylandRuntimeCommandStatus::Unrecognized;
    const std::string name=trim(line.substr(0,opening));
    const std::vector<std::string> supported={"storyland.player.health","storyland.player.armour","storyland.player.damage",
        "storyland.player.money","storyland.player.wanted","storyland.player.position","storyland.player.teleport",
        "storyland.player.heading","storyland.time","storyland.time_scale","storyland.weather","storyland.pause",
        "storyland.hud","storyland.sounds","storyland.ped.health","storyland.vehicle.health","storyland.camera.refocus"};
    if(std::find(supported.begin(),supported.end(),name)==supported.end()) return StorylandRuntimeCommandStatus::Unrecognized;
    const auto fail=[&](const std::string& message) {result.output=message;return StorylandRuntimeCommandStatus::Error;};
    std::vector<double> values;
    std::string arguments=trim(line.substr(opening+1,line.size()-opening-2));
    while(!arguments.empty()) {
        const auto comma=arguments.find(',');
        const std::string argument=trim(arguments.substr(0,comma));
        double value=0;
        if(argument=="true"||argument=="True") value=1;
        else if(argument=="false"||argument=="False") value=0;
        else {char* end=nullptr;value=std::strtod(argument.c_str(),&end);
            if(end==argument.c_str()||*end||!std::isfinite(value)) return fail("Expected finite numeric arguments.");}
        values.push_back(value);
        if(comma==std::string::npos) break;
        arguments=trim(arguments.substr(comma+1));
        if(arguments.empty()) return fail("Missing argument after comma.");
        if(values.size()>3) return fail("Too many arguments.");
    }
    if(name=="storyland.camera.refocus") {
        if(!values.empty()) return fail("camera.refocus() takes no arguments.");
        result.refocus=true;
    } else if(name=="storyland.player.position"||name=="storyland.player.teleport") {
        if(values.size()!=3) return fail("player.position(x, y, z) requires three numbers.");
        for(double value:values) if(std::fabs(value)>1000000) return fail("Position is outside the supported world range.");
        if(runtime.occupiedVehicle>=0 && size_t(runtime.occupiedVehicle)<runtime.vehicles.size())
            runtime.vehicles[size_t(runtime.occupiedVehicle)].parked=true;
        runtime.occupiedVehicle=-1;
        player={float(values[0]),float(values[1]),float(values[2])};runtime.verticalSpeed=0;runtime.grounded=true;result.teleported=true;
    } else if(name=="storyland.ped.health"||name=="storyland.vehicle.health") {
        const bool ped=name=="storyland.ped.health";
        if(values.size()!=2||values[0]<0||values[0]>=15||std::floor(values[0])!=values[0]||
            values[1]<0||values[1]>(ped?100:1000)) return fail("Expected index 0..14 and a valid health value.");
        if(ped) runtime.pedestrians[size_t(values[0])].health=float(values[1]);
        else runtime.vehicles[size_t(values[0])].health=float(values[1]);
    } else {
        if(values.size()!=1) return fail("This command requires one argument.");
        const double value=values[0];
        if(name=="storyland.player.health"||name=="storyland.player.armour") {
            if(value<0||value>100) return fail("Health and armour must be between 0 and 100.");
            if(name=="storyland.player.health") runtime.health=float(value); else runtime.armour=float(value);
        } else if(name=="storyland.player.damage") {
            if(value<0||value>1000000) return fail("Damage must be between 0 and 1000000.");runtime.damage(float(value));
        } else if(name=="storyland.player.money") {
            if(std::floor(value)!=value||value<std::numeric_limits<int>::min()||value>std::numeric_limits<int>::max()) return fail("Money must be a 32-bit integer.");
            runtime.money=int(value);
        } else if(name=="storyland.player.wanted") {
            if(std::floor(value)!=value||value<0||value>6) return fail("Wanted level must be 0..6.");runtime.wanted=int(value);
        } else if(name=="storyland.player.heading") {
            if(std::fabs(value)>1000000) return fail("Heading is outside the supported range.");heading=float(std::remainder(value,360));
        } else if(name=="storyland.time") {
            if(value<0||value>=24) return fail("Time must be in the range 0..<24 hours.");
            storylandSetPlayTestClock(runtime,float(value));result.timeChanged=true;
        } else if(name=="storyland.time_scale") {
            if(value<0||value>1440) return fail("Time scale must be 0..1440 game minutes per second.");runtime.gameMinutesPerSecond=float(value);
        } else if(name=="storyland.weather") {
            if(std::floor(value)!=value||value<0||value>7) return fail("Native weather index must be 0..7.");result.weather=int(value);
        } else {
            if(value!=0&&value!=1) return fail("Expected True or False.");
            if(name=="storyland.pause") runtime.paused=value!=0;
            else if(name=="storyland.hud") runtime.hud=value!=0;
            else if(name=="storyland.sounds") runtime.sounds=value!=0;
        }
    }
    result.output=line;return StorylandRuntimeCommandStatus::Executed;
}


void storylandCaptureMissionWeapons(const StorylandPlayTestRuntime& runtime,StorylandMissionEnvironment& environment) {
    environment.playerWeapons.clear();
    for(size_t index=0;index<runtime.weapons.size();++index) {
        const auto& definition=runtime.weapons[index];
        const int32_t nativeId=definition.nativeId<0&&index==0?0:definition.nativeId;
        if(nativeId<0)continue;
        StorylandMissionEnvironment::Weapon weapon;
        weapon.slot=definition.slot;weapon.model=definition.modelId;
        if(index<runtime.inventory.size()) {
            weapon.ammunition=runtime.inventory[index].ammunition;
            weapon.owned=runtime.inventory[index].owned||weapon.ammunition>0;
        }
        if(nativeId==0)weapon.owned=true;
        environment.playerWeapons.emplace(nativeId,weapon);
    }
    environment.currentWeapon=runtime.equippedWeapon<runtime.weapons.size()?
        std::max(0,runtime.weapons[runtime.equippedWeapon].nativeId):0;
}

bool storylandApplyMissionWeapons(StorylandPlayTestRuntime& runtime,StorylandMissionEnvironment& environment,
    std::string& error,bool vcs) {
    error.clear();
    const auto findWeapon=[&](int32_t nativeId) {
        for(size_t index=0;index<runtime.weapons.size();++index)
            if(runtime.weapons[index].nativeId==nativeId||
                (nativeId==0&&index==0))return index;
        return SIZE_MAX;
    };
    for(const auto& entry:environment.playerWeapons)
        if(environment.weaponInventoryChanged&&entry.second.owned&&findWeapon(entry.first)==SIZE_MAX) {
            error="SCM requested native weapon "+std::to_string(entry.first)+
                " whose definition/model is not available in the loaded game.";
            return false;
        }
    size_t selected=runtime.equippedWeapon;
    if(environment.weaponSelectionRequested) {
        selected=findWeapon(environment.currentWeapon);
        if(selected==SIZE_MAX) {error="SCM selected a weapon absent from the loaded game.";return false;}
    }
    const auto validateCharacterWeapons=[&](const auto& inventory) {
        for(const auto& weapon:inventory)if(weapon.second.owned&&findWeapon(weapon.first)==SIZE_MAX) {
            error="Mission character has a weapon absent from the loaded native definitions.";
            return false;
        }
        return true;
    };
    for(const auto& entry:environment.missionCharacters) {
        const auto& character=entry.second;
        int32_t current=character.currentWeapon;
        if(vcs) {
            if(!validateCharacterWeapons(character.weapons))return false;
        } else {
            const auto inventory=environment.characterWeapons.find(entry.first);
            if(inventory!=environment.characterWeapons.end()&&!validateCharacterWeapons(inventory->second))return false;
            const auto weapon=environment.characterCurrentWeapons.find(entry.first);
            if(weapon!=environment.characterCurrentWeapons.end())current=weapon->second;
        }
        if(findWeapon(current)==SIZE_MAX) {
            error="Mission character selected an unavailable native weapon.";return false;
        }
    }
    for(auto& entry:environment.missionCharacters) {
        auto& character=entry.second;
        if(!vcs) {
            const auto current=environment.characterCurrentWeapons.find(entry.first);
            if(current!=environment.characterCurrentWeapons.end())character.currentWeapon=current->second;
        }
        auto& combat=runtime.missionCombat[entry.first];
        const size_t selectedWeapon=findWeapon(character.currentWeapon);
        int ammunition=0;
        if(vcs) {
            const auto weapon=character.weapons.find(character.currentWeapon);
            if(weapon!=character.weapons.end())ammunition=weapon->second.ammunition;
        } else {
            const auto inventory=environment.characterWeapons.find(entry.first);
            if(inventory!=environment.characterWeapons.end()) {
                const auto weapon=inventory->second.find(character.currentWeapon);
                if(weapon!=inventory->second.end())ammunition=weapon->second.ammunition;
            }
        }
        if(combat.nativeWeapon!=character.currentWeapon||character.weaponSelectionChanged) {
            combat=StorylandMissionCombatState{};
            combat.nativeWeapon=character.currentWeapon;combat.weapon=selectedWeapon;
            combat.magazine=std::min(runtime.weapons[selectedWeapon].clipSize,ammunition);
        } else if(character.weaponsChanged) {
            combat.magazine=std::min({std::max(combat.magazine,0),ammunition,runtime.weapons[selectedWeapon].clipSize});
            if(combat.magazine==0&&combat.reloading<=0)combat.magazine=std::min(runtime.weapons[selectedWeapon].clipSize,ammunition);
        }
        character.weaponsChanged=false;character.weaponSelectionChanged=false;
    }
    for(auto combat=runtime.missionCombat.begin();combat!=runtime.missionCombat.end();) {
        if(!environment.missionCharacters.count(combat->first))combat=runtime.missionCombat.erase(combat);
        else ++combat;
    }
    runtime.inventory.resize(runtime.weapons.size());
    if(environment.clearPlayerWeaponsRequested) {
        for(auto& weapon:runtime.inventory)weapon=StorylandWeaponInventory{};
        selected=0;runtime.playerMelee=StorylandMeleeAttack{};
    }
    if(environment.weaponInventoryChanged)for(const auto& entry:environment.playerWeapons) {
        const size_t index=findWeapon(entry.first);
        if(index==SIZE_MAX)continue;
        auto& destination=runtime.inventory[index];
        if(!entry.second.owned) {destination=StorylandWeaponInventory{};continue;}
        const bool newlyOwned=!destination.owned&&destination.ammunition<=0;
        destination.owned=true;
        destination.ammunition=vcs?std::min(entry.second.ammunition,9999):std::max(0,entry.second.ammunition);
        const int capacity=std::max(0,runtime.weapons[index].clipSize);
        destination.magazine=std::min({destination.magazine,destination.ammunition,capacity});
        if(newlyOwned)destination.magazine=std::min(capacity,destination.ammunition);
    }
    if(selected!=runtime.equippedWeapon)runtime.playerMelee=StorylandMeleeAttack{};
    runtime.equippedWeapon=selected;
    environment.clearPlayerWeaponsRequested=false;
    environment.weaponInventoryChanged=false;environment.weaponSelectionRequested=false;
    return true;
}


bool storylandCaptureMissionCrouches(const StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& captured) {
    if(runtime.missionEnvironment==&captured)return false;
    if(!runtime.crime.vcs||!runtime.missionEnvironment)return true;
    std::vector<StorylandCrouchState*> materialize;
    const auto eligible=[&](int32_t handle,const StorylandCrouchWeaponPose& phase,
        const StorylandCrouchState& crouch,int32_t selected,bool alive,bool onFoot) {
        if(!alive||!onFoot||!crouch.active||!phase.active()||phase.nativeWeapon!=selected)return false;
        const auto definition=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
            [selected](const auto& weapon){return weapon.nativeId==selected;});
        if(definition==runtime.weapons.end())return false;
        return phase.animation==215?(definition->nativeFlags&0x20000u)!=0:
            crouch.requested&&(definition->nativeFlags&0x8000u)!=0;
    };
    const int32_t playerWeapon=runtime.equippedWeapon<runtime.weapons.size()?
        runtime.weapons[runtime.equippedWeapon].nativeId:-1;
    if(eligible(0,runtime.playerCrouchPose,captured.playerCrouch,playerWeapon,
        runtime.health>0&&captured.health>0,!captured.inVehicle&&!captured.activeVcsCharacterAttachment(0)))
        materialize.push_back(&captured.playerCrouch);
    for(auto& entry:captured.missionCharacters) {
        const auto combat=runtime.missionCombat.find(entry.first);
        if(combat==runtime.missionCombat.end())continue;
        auto& character=entry.second;
        // Automatic AI cover is transient. Do not serialize it as a native
        // script-requested crouch, which would outlive its combat owner.
        if(combat->second.extendedAutoCrouch&&character.crouch.requested&&
            character.crouch.deadline==combat->second.extendedAutoCrouchDeadline) {
            character.crouch={};
            continue;
        }
        if(combat->second.nativeWeapon!=character.currentWeapon)continue;
        if(eligible(entry.first,combat->second.crouchPose,character.crouch,character.currentWeapon,
            character.alive&&character.health>0,character.vehicle<0&&!character.attached))
            materialize.push_back(&character.crouch);
    }
    if(materialize.empty())return true;
    const float duration=captured.animationDuration?captured.animationDuration(0,155):0;
    if(!std::isfinite(duration)||duration<=0||duration>=60)return false;
    for(auto* crouch:materialize) {
        crouch->poseAnimation=155;
        crouch->poseFade={1,0,captured.gameMilliseconds,true};
        crouch->startedAt=captured.gameMilliseconds-uint32_t(std::ceil(duration*1000));
    }
    return true;
}



