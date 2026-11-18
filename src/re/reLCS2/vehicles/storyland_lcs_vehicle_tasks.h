#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include "src/storyland/storyland_vehicle_entry.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_animation_root.h"
#include <limits>
#include <type_traits>

inline bool storylandReadVcsVehicleEntryRoot(const std::vector<uint8_t>& bytes,
    int32_t group,std::array<float,3>& root) {
    return group>=0&&storylandReadVcsAnimationRoot(bytes,uint32_t(group),194,root);
}

// Bind script objectives to the same decoded seats, clips and collision bodies
// used by the player vehicle transitions. A blocked door leaves the task pending.
template<class Duration>
bool storylandPrepareLcsVehicleTask(StorylandMissionEnvironment& environment,
    StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& cars,int32_t characterHandle,
    StorylandMissionEnvironment::CharacterVehicleTask& task,Duration duration,std::string& error,
    const std::function<bool(const StorylandVehicleDefinition&,std::array<float,3>&)>& entryRoot={}) {
    const auto character=environment.missionCharacters.find(characterHandle);
    const auto scripted=environment.missionVehicles.find(task.vehicle);
    if(character==environment.missionCharacters.end()||scripted==environment.missionVehicles.end()) {
        error="Vehicle task references an absent pedestrian or vehicle.";return false;
    }
    const auto model=std::find_if(runtime.modelDefinitions.begin(),runtime.modelDefinitions.end(),
        [&](const auto& item){return item.modelId==uint32_t(scripted->second.model);});
    const bool quad=model!=runtime.modelDefinitions.end()&&model->vcsModelLayout&&model->nativeType==9;
    const bool bike=model!=runtime.modelDefinitions.end()&&model->motorcycle();
    const bool vcsBike=bike&&model->vcsModelLayout;
    const bool saddle=quad||bike;
    const bool helicopter=model!=runtime.modelDefinitions.end()&&model->vcsModelLayout&&
        model->nativeType==4&&model->domain==StorylandVehicleDomain::Air;
    if(model==runtime.modelDefinitions.end()||!model->hasNativeDummies||
        (model->nativeType!=0&&!saddle&&!helicopter)||(!saddle&&(model->handlingFlags&0x60000u))||
        (quad&&task.seat>=0)||(helicopter&&task.exiting)) {
        error="Pedestrian entry/exit requires decoded automobile seats, motorcycle seats, or a supported VCS vehicle.";return false;
    }
    std::array<float,3> bikeRoot{};
    if(bike&&model->hasNativeMotorcycleEntryRoot)bikeRoot=model->nativeMotorcycleEntryRoot;
    else if(bike&&(!vcsBike||!entryRoot||!entryRoot(*model,bikeRoot))) {
        error="Motorcycle entry/exit requires its original animation-group door offset.";return false;
    }
    const size_t modelIndex=size_t(model-runtime.modelDefinitions.begin());
    if(modelIndex>=runtime.modelExtents.size()) {error="Vehicle collision extents are unavailable.";return false;}
    const auto& definition=*model;
    const auto& state=scripted->second;
    const bool nativeAutomobile=definition.nativeType==0&&definition.hasNativeDoorRoots&&
        (definition.vcsModelLayout||task.exiting);
    const int32_t slot=state.spawnedSlot;
    const StorylandRuntimeVehicle* vehicle=slot>=0&&size_t(slot)<runtime.vehicles.size()&&
        runtime.vehicles[size_t(slot)].scriptHandle==task.vehicle?&runtime.vehicles[size_t(slot)]:nullptr;
    const bool water=slot>=0&&size_t(slot)<cars.size()&&cars[size_t(slot)].water;
    task.low=definition.lowVehicle();task.boat=water;
    task.rightHandSide=task.seat>=0&&task.seat%2==0;
    task.hijacking=!task.exiting&&task.seat<0&&
        ((state.driver>=0&&state.driver!=characterHandle)||
            (environment.inVehicle&&environment.occupiedMissionVehicle==task.vehicle&&environment.playerPassengerSeat<0)||
            (!quad&&vehicle&&vehicle->hasDriver));
    task.vehicleOrigin={state.x,state.y,state.z};task.vehicleHeading=state.heading;
    const unsigned occupant=task.seat<0?0u:unsigned(task.seat+1);
    const auto frame=storylandVehicleOccupantFrame(definition,task.vehicleOrigin,state.heading+90,
        definition.previewMinimumZ,vehicle?vehicle->suspension:0,
        vehicle?vehicle->bodyRoll-vehicle->roadRoll:0,vehicle?vehicle->bodyPitch+vehicle->roadPitch:0,occupant,water);
    task.seatPosition={frame[12],frame[13],frame[14]};
    StorylandVehicleMatrix entityFrame{};
    float originOffset=0;
    if(nativeAutomobile||bike) {
        if(!vehicle||!definition.collision||
            !environment.characterOriginOffset||
            !environment.characterOriginOffset(characterHandle,originOffset)||!std::isfinite(originOffset)) {
            error="Native vehicle transition requires original door roots, collision and the character origin.";return false;
        }
        if(nativeAutomobile&&(task.seat>2||(task.seat>=1&&definition.numberOfDoors!=4)||
            (definition.vcsModelLayout&&task.seat==0&&(definition.modelFlags&0x4000u)))) {
            error="The original automobile passenger selector does not admit this seat.";return false;
        }
        entityFrame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
            task.vehicleOrigin,state.heading+90,definition.previewMinimumZ,vehicle->suspension,
            vehicle->bodyRoll-vehicle->roadRoll,vehicle->bodyPitch+vehicle->roadPitch,water),definition.previewRoot);
    }
    const float halfWidth=vehicle?vehicle->halfWidth:runtime.modelExtents[modelIndex][0];
    const float halfLength=vehicle?vehicle->halfLength:runtime.modelExtents[modelIndex][1];
    const float radians=(state.heading+90)*0.0174532925199433f;
    const float cosine=std::cos(radians),sine=std::sin(radians);
    const auto point=[&](float along,float left) {
        return StorylandNativePoint{state.x+cosine*along-sine*left,
            state.y+sine*along+cosine*left,state.z};
    };
    const auto ground=[&](StorylandNativePoint& target) {
        float height=0;
        if(environment.groundHeightAt&&environment.groundHeightAt({target.x,target.y,state.z+1.5f},height)&&
            std::fabs(height-state.z)<1.5f) {target.z=height;return true;}
        return world.collisionModels.empty()||water;
    };
    runtime.movementCars.clear();
    for(size_t i=0;i<cars.size()&&i<runtime.vehicles.size();++i)
        runtime.movementCars.push_back({cars[i].position,cars[i].yaw,runtime.vehicles[i].halfLength,
            runtime.vehicles[i].halfWidth,runtime.vehicles[i].bodyHeight,!runtime.vehicles[i].craneCarried});
    const auto clear=[&](const StorylandNativePoint& from,const StorylandNativePoint& to,bool dynamic) {
        const auto moved=dynamic?storylandMovePlayTestCharacter(world,runtime,from,to,-1,true):
            storylandMoveNativeCharacter(world,from,to);
        return moved.clear&&!moved.blocked&&std::hypot(moved.position.x-to.x,moved.position.y-to.y,
            moved.position.z-to.z)<0.04f;
    };
    task.route.clear();task.routeIndex=0;
    const float along=definition.seatPosition(occupant)[1]-.45f;
    const StorylandNativePoint start{character->second.x,character->second.y,character->second.z};
    if(saddle&&!task.exiting)task.rightHandSide=-(start.x-state.x)*sine+(start.y-state.y)*cosine<0;
    const bool preferredRight=task.rightHandSide;
    bool doorFound=false;
    bool repositioned=false;
    StorylandNativePoint originalExitDoor{};
    const bool privileged=storylandVehicleExitMayReposition(false,uint8_t(character->second.creationSource));
    if(task.exiting&&slot>=0&&storylandVehicleExitDoorReserved(runtime,size_t(slot),occupant,
        preferredRight,characterHandle)) {
        error="The selected exit door is reserved by another pedestrian.";return false;
    }
    const bool mayCross=slot<0||storylandVehicleExitMayCrossSeats(definition.vcsModelLayout,saddle,privileged,
        storylandVehicleExitSeatOccupied(runtime,size_t(slot),
            storylandVehicleExitOppositeSeat(occupant,preferredRight),characterHandle),
        storylandVehicleExitDoorReserved(runtime,size_t(slot),occupant,!preferredRight,characterHandle));
    for(unsigned choice=0;choice<(task.exiting||saddle?2u:1u);++choice) {
        const bool right=choice?!preferredRight:preferredRight;
        if(task.exiting&&choice&&!mayCross)continue;
        if(!task.exiting&&runtime.transitionVehicle==slot&&runtime.vehicleTransitionPrepared&&
            (runtime.vehicleAction==StorylandVehicleAction::Enter||runtime.vehicleAction==StorylandVehicleAction::Hijack)&&
            storylandVehicleSeatDoor(occupant,right)==storylandVehicleSeatDoor(
                unsigned(runtime.playerVehicleSeat+1),runtime.vehicleExitRight))continue;
        auto door=point(along,(right?-1.0f:1.0f)*(halfWidth+.5f));
        if(task.exiting&&!choice)originalExitDoor=door;
        if(bike) {
            std::array<float,3> position{};
            const auto& maximum=definition.collision->maximum;
            if(!storylandMotorcycleDoorPosition(entityFrame,bikeRoot,definition.seatPosition(occupant),
                {maximum.x,maximum.y,maximum.z},right,position))continue;
            door={position[0],position[1],position[2]-originOffset};
            if(task.exiting&&!choice)originalExitDoor=door;
        }
        if(nativeAutomobile) {
            const bool rear=occupant>=2;
            const bool omitSeatOffset=definition.vanVehicle()&&rear;
            std::array<float,3> anchor{};
            StorylandVcsVehicleRoomQuery query;
            const auto& seatDummy=definition.dummyPositions[definition.vcsModelLayout?
                (rear?5u:4u):(rear?3u:2u)];
            if(!storylandVcsHelicopterDoorPosition(entityFrame,definition.nativeDoorRoots[rear?1u:0u],
                seatDummy,omitSeatOffset?0:definition.seatOffset,right,anchor))continue;
            door={anchor[0],anchor[1],anchor[2]-originOffset};
            if(task.exiting&&!choice)originalExitDoor=door;
            if(!storylandVcsVehicleRoomQuery(entityFrame,seatDummy,anchor,right,query)||
                !storylandAutomobileRoomClear(runtime,world,cars,size_t(slot),query))continue;
        } else if(!ground(door)||!storylandNativeBodyClear(world,door,storylandNativeCharacterBody()))continue;
        if(task.exiting&&slot>=0&&
            !storylandVehicleExitDestinationClear(runtime,world,cars,size_t(slot),door))continue;
        auto approach=door;
        if(!nativeAutomobile&&!task.exiting) {
            const auto recovered=storylandMovePlayTestCharacter(world,runtime,door,door,-1,true);
            const float approachLimit=bike?.3f:.04f;
            const float approachShift=bike?std::hypot(recovered.position.x-door.x,
                recovered.position.y-door.y,recovered.position.z-door.z):
                std::hypot(recovered.position.x-door.x,recovered.position.y-door.y);
            if(!recovered.clear||(bike&&recovered.blocked)||approachShift>approachLimit)continue;
            if(bike)approach=recovered.position;
        }
        task.door={door.x,door.y,door.z};task.rightHandSide=right;doorFound=true;
        if(task.exiting) {task.route.push_back(task.door);break;}
        float best=std::numeric_limits<float>::infinity();
        const auto consider=[&](std::vector<StorylandNativePoint> route) {
            float length=0;auto previous=start;
            for(size_t index=0;index<route.size();++index) {
                auto& target=route[index];
                if(nativeAutomobile&&index+1==route.size()) {
                    const float dx=previous.x-door.x,dy=previous.y-door.y;
                    const float separation=std::hypot(dx,dy);
                    if(storylandVcsHelicopterSeekReached(previous.x,previous.y,door.x,door.y))target=previous;
                    else if(std::isfinite(separation)&&separation>0) {
                        const float radius=std::nextafter(.5f,0.0f);
                        target={door.x+dx*(radius/separation),door.y+dy*(radius/separation),previous.z};
                        for(unsigned attempt=0;attempt<4&&
                            !storylandVcsHelicopterSeekReached(target.x,target.y,door.x,door.y);++attempt) {
                            target.x=std::nextafter(target.x,door.x);target.y=std::nextafter(target.y,door.y);
                        }
                    } else return;
                    if(!storylandVcsHelicopterSeekReached(target.x,target.y,door.x,door.y))return;
                }
                if(!ground(target)||!clear(previous,target,true))return;
                length+=std::hypot(target.x-previous.x,target.y-previous.y,target.z-previous.z);previous=target;
            }
            if(length>=best)return;
            best=length;task.route.clear();
            for(const auto& target:route)task.route.push_back({target.x,target.y,target.z});
        };
        consider({approach});
        if(world.paths.vcs&&task.route.size()==1)break;
        const float dx=start.x-state.x,dy=start.y-state.y;
        const float startSide=(-dx*sine+dy*cosine)>=0?1.0f:-1.0f;
        const float doorSide=right?-1.0f:1.0f;
        for(float end:{1.0f,-1.0f}) {
            const auto nearCorner=point(end*(halfLength+.7f),startSide*(halfWidth+.7f));
            const auto farCorner=point(end*(halfLength+.7f),doorSide*(halfWidth+.7f));
            if(world.paths.vcs)consider({farCorner,approach});
            consider({nearCorner,farCorner,approach});
            if(world.paths.vcs&&nativeAutomobile&&task.route.empty()) {
                const StorylandNativePoint staging{door.x-sine*doorSide,door.y+cosine*doorSide,door.z};
                consider({farCorner,staging,approach});
                consider({nearCorner,farCorner,staging,approach});
            }
        }
        if(saddle&&task.route.empty())continue;
        break;
    }
    if(!doorFound&&task.exiting&&privileged&&mayCross&&slot>=0) {
        StorylandNativePoint position;
        if(storylandFindVehicleExitPosition(runtime,world,cars,size_t(slot),preferredRight,originalExitDoor,position)) {
            task.door={position.x,position.y,position.z};task.rightHandSide=preferredRight;
            task.route.push_back(task.door);doorFound=true;repositioned=true;
        }
    }
    if(!doorFound) {
        if(task.exiting) {error="No collision-clear native vehicle exit is available.";return false;}
        if(nativeAutomobile) {error="The original automobile room query rejects the selected door.";return false;}
        task.door=task.seatPosition;
    }
    task.phaseSeconds={};
    if(repositioned)return true;
    using Phase=StorylandVehiclePhase;
    if(saddle) {
        const float seconds=task.exiting?(task.rightHandSide?definition.rightExitSeconds:definition.exitSeconds):
            (task.rightHandSide?definition.rightEntrySeconds:definition.entrySeconds);
        if(!std::isfinite(seconds)||seconds<=0) {
            error="Rider transition requires its decoded native side clip.";return false;
        }
        if(task.hijacking) {
            const float pullOut=definition.sideTransitionTiming[task.rightHandSide?1u:0u].seconds[unsigned(Phase::PullOut)];
            if(!bike||!std::isfinite(pullOut)||pullOut<=0) {
                error="Occupied rider entry requires its original motorcycle hijack association.";return false;
            }
            task.phaseSeconds[unsigned(Phase::PullOut)]=pullOut;
        }
        task.phaseSeconds[unsigned(task.exiting?Phase::GetOut:Phase::GetIn)]=seconds;
        return true;
    }
    if(helicopter) {
        if(task.hijacking||!definition.retailTransitions) {
            error="Helicopter entry requires a vacant seat and original decoded associations.";return false;
        }
        task.phaseSeconds=definition.sideTransitionTiming[task.rightHandSide?1u:0u].seconds;
        task.phaseSeconds[unsigned(Phase::GetIn)]=task.rightHandSide?
            definition.rightEntrySeconds:definition.entrySeconds;
        if(task.phaseSeconds[unsigned(Phase::Align)]!=0||
            task.phaseSeconds[unsigned(Phase::GetIn)]<=0||
            !std::all_of(task.phaseSeconds.begin(),task.phaseSeconds.end(),
                [](float seconds){return std::isfinite(seconds)&&seconds>=0;})) {
            error="Helicopter entry requires native type4 timing and its immediate alignment callback.";return false;
        }
        return true;
    }
    for(const auto phase:{Phase::Align,Phase::Open,Phase::PullOut,Phase::GetIn,
        Phase::CloseInside,Phase::GetOut,Phase::CloseOutside}) {
        if(task.exiting!=(phase==Phase::GetOut||phase==Phase::CloseOutside))continue;
        if(phase==Phase::PullOut&&!task.hijacking)continue;
        if(definition.busVehicle()&&
            (phase==Phase::CloseInside||phase==Phase::CloseOutside))continue;
        const float seconds=[&]() {
            if constexpr(std::is_invocable_r_v<float,Duration,const std::string&,
                const StorylandVehicleDefinition&,bool,int32_t>)
                return duration(storylandVehiclePhaseAction(phase),definition,task.rightHandSide,task.seat);
            else return duration(storylandVehiclePhaseAction(phase),task.low,task.rightHandSide);
        }();
        if(!std::isfinite(seconds)||seconds<=0) {
            error=std::string("Native pedestrian vehicle clip is unavailable: ")+storylandVehiclePhaseAction(phase);return false;
        }
        task.phaseSeconds[unsigned(phase)]=seconds;
    }
    return true;
}

inline float storylandLcsVehicleDoorRatio(float previous,StorylandVehiclePhase phase,float seconds) {
    using Phase=StorylandVehiclePhase;
    float begin=0,end=0;bool opening=true;
    if(phase==Phase::Open) {begin=.41f;end=.89f;}
    else if(phase==Phase::CloseInside) {begin=.2f;end=.63f;opening=false;}
    else if(phase==Phase::GetOut) {begin=.06f;end=.43f;}
    else if(phase==Phase::CloseOutside) {begin=.35f;end=.5f;opening=false;}
    else if(phase==Phase::Done)return 0;
    else return previous;
    if(seconds<=begin)return previous;
    if(seconds>=end)return opening?1.0f:0.0f;
    const float fraction=(seconds-begin)/(end-begin);
    return opening?std::max(previous,fraction):std::min(previous,1-fraction);
}
