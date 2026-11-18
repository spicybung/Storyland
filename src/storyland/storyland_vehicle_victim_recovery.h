#pragma once
#include "src/re/reVCS/services/storyland_vcs_police_loadout.h"

#include "src/storyland/storyland_vehicle_carjack_animation.h"
#include "src/storyland/storyland_service_population.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_ped_motion.h"
#include "src/re/reVCS/vehicles/storyland_vcs_carjack_recovery.h"
#include "src/storyland/storyland_weapon_attachment.h"

inline uint32_t storylandVcsCarjackRecoveryDeadline(uint32_t now,uint8_t entitySeed,
    uint32_t frameCounter) {
    // Original SetFall173 preserves the installed association and takes the no-association deadline.
    return now+2000u+(uint32_t(entitySeed)+frameCounter)%1000u;
}

inline bool storylandVcsCarjackFall(StorylandPlayTestAnimation& animation,bool right,
    uint32_t now,uint8_t seed,uint32_t frame,const StorylandNativeTrafficActor& actor,
    const StorylandRuntimeVehicle& vehicle,StorylandVcsPedFallState& fall) {
    fall.fallAnimation=right?120:121;
    uint16_t flags=0,getUpFlags=0;
    if(!animation.retailAnimationAssociationFlags(fall.fallAnimation,0,flags)||(flags&2u))return false;
    fall.getUpAnimation=storylandVcsPedGetUpAnimation(flags);
    if(!animation.retailAnimationAssociationFlags(fall.getUpAnimation,0,getUpFlags)||(getUpFlags&2u))return false;
    fall.fallDuration=animation.retailAnimationDuration(fall.fallAnimation,0);
    fall.getUpDuration=animation.retailAnimationDuration(fall.getUpAnimation,0);
    fall.getUpAfter=storylandVcsCarjackRecoveryDeadline(now,seed,frame);
    fall.sourceVehicle=vehicle.scriptHandle;
    constexpr float radians=.01745329251994329577f;
    const float cosine=std::cos(actor.yaw*radians),sine=std::sin(actor.yaw*radians);
    fall.velocity={cosine*vehicle.speed-sine*vehicle.lateralSpeed,
        sine*vehicle.speed+cosine*vehicle.lateralSpeed,vehicle.verticalSpeed};
    return storylandValidVcsPedFall(fall);
}

inline int32_t storylandPromoteCarjackDriver(StorylandPlayTestRuntime& runtime,size_t slot,
    const StorylandNativeTrafficActor& actor,const std::array<float,3>& position) {
    auto& world=*runtime.missionEnvironment;
    auto& vehicle=runtime.vehicles[slot];
    if(world.missionCharacters.size()>=128||world.nextMissionCharacterHandle<1||
        world.nextMissionCharacterHandle==INT32_MAX||
        world.missionCharacters.count(world.nextMissionCharacterHandle))return -1;
    const auto model=storylandVehicleDriverModel(runtime,vehicle,uint32_t(slot));
    if(model>=runtime.pedestrianModelStats.size()||!runtime.pedestrianModelStats[model].valid)return -1;
    if(vehicle.scriptHandle<0) {
        if(world.nextMissionVehicleHandle<1||world.nextMissionVehicleHandle==INT32_MAX||
            world.missionVehicles.count(world.nextMissionVehicleHandle))return -1;
        StorylandMissionEnvironment::VehicleScriptState state;
        state.model=int32_t(vehicle.nativeDefinition.modelId);state.spawnedSlot=int32_t(slot);
        state.x=actor.position.x;state.y=actor.position.y;state.z=actor.position.z;
        state.heading=actor.yaw-90;state.health=vehicle.health;state.alive=vehicle.health>0;
        state.speed=vehicle.speed;state.ambientOrigin=true;state.persistent=false;
        state.primaryColor=vehicle.primaryColor;state.secondaryColor=vehicle.secondaryColor;
        state.colorSet=vehicle.colorSet;state.scriptColorSet=vehicle.scriptColorSet;
        vehicle.scriptHandle=world.nextMissionVehicleHandle++;
        world.missionVehicles.emplace(vehicle.scriptHandle,state);
    }
    StorylandMissionEnvironment::Character character;
    character.pedStats=runtime.pedestrianModelStats[model];
    character.model=int32_t(character.pedStats.modelId);character.type=character.pedStats.pedType;
    character.pedStat=character.pedStats.statIndex;character.animationGroup=character.pedStats.animationGroup;
    if(runtime.crime.vcs)storylandInitializeVcsPoliceLoadout(character,runtime.weapons);
    character.x=position[0];character.y=position[1];character.z=position[2];
    character.heading=actor.yaw-90;character.vehicle=vehicle.scriptHandle;
    character.persistent=false;character.creationSource=1;
    character.nativePedRandomSeed=uint8_t(world.nextNativeRandom());
    character.nativePedRandomSeedInitialized=true;
    character.walletMoney=storylandInitialVcsWallet(world.nativeRandomState);character.walletInitialized=true;
    character.speechProfile=world.nativePedSpeechProfile?world.nativePedSpeechProfile(character.model):-1;
    const int32_t handle=world.nextMissionCharacterHandle++;
    world.missionCharacters.emplace(handle,std::move(character));
    return handle;
}

inline bool storylandBeginVcsCarjackedPlayerGetUp(StorylandPlayTestRuntime& runtime,size_t slot,
    bool right,const std::array<float,3>& door,StorylandNativePoint& player) {
    if(!runtime.crime.vcs||!runtime.missionEnvironment||slot>=runtime.vehicles.size()||
        runtime.occupiedVehicle!=int32_t(slot)||runtime.playerVehicleSeat>=0||runtime.health<=0)return false;
    const auto& vehicle=runtime.vehicles[slot];auto& world=*runtime.missionEnvironment;
    if(vehicle.nativeDefinition.nativeType!=0||!world.animationDuration||!world.animationAssociationFlags)return false;
    StorylandVcsPedFallState recovery;
    recovery.sourceVehicle=vehicle.scriptHandle;
    recovery.fallAnimation=int32_t(storylandVehicleAssociation("jacked",vehicle.nativeDefinition.lowVehicle(),right,true));
    recovery.fallDuration=world.animationDuration(0,recovery.fallAnimation);
    recovery.getUpAnimation=132;recovery.getUpDuration=world.animationDuration(0,132);
    recovery.getUpAfter=world.gameMilliseconds;
    float elapsed=recovery.fallDuration;
    for(const auto& entry:world.characterVehicleTasks) {
        const auto& task=entry.second;
        if(task.vehicle!=vehicle.scriptHandle||!task.hijacking||!task.animating)continue;
        elapsed=std::max(0.0f,float(world.gameMilliseconds-task.startedAt)*.001f-
            task.phaseSeconds[0]-task.phaseSeconds[1]-task.phaseSeconds[2]);break;
    }
    recovery.recovering=elapsed>=recovery.fallDuration;
    recovery.time=recovery.recovering?0:elapsed;
    uint16_t flags=0;
    if(!world.animationAssociationFlags(0,132,flags)||(flags&2u)||!storylandValidVcsPedFall(recovery))return false;
    runtime.playerJetSkiFall=recovery;runtime.playerJetSkiFallActive=true;
    auto& pose=runtime.vehicleExitPose;pose={};pose.active=true;
    pose.animation=recovery.animation();pose.elapsed=recovery.time;pose.group=0;
    pose.sourceVehicle=int32_t(slot);pose.duration=recovery.duration();
    player={door[0],door[1],door[2]};runtime.playerPosition=player;
    runtime.occupiedVehicle=-1;runtime.playerVehicleSeat=-1;
    runtime.grounded=false;runtime.verticalSpeed=0;runtime.jumpHorizontalValid=false;
    world.x=player.x;world.y=player.y;world.z=player.z;
    world.inVehicle=false;world.occupiedMissionVehicle=-1;world.playerPassengerSeat=-1;
    world.playerVehicleTransition=false;world.playerExitVehicleRequested=false;world.playerWarped=false;
    return true;
}

inline bool storylandSampleVcsCarjackedContinuation(StorylandPlayTestAnimation& animation,
    const StorylandModelFile& model,const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& actors,const StorylandVcsPedFallState& state,
    const StorylandNativePoint& position,float heading,float now,StorylandAnimationBlendState& blend,
    std::vector<StorylandModelPoint>& points,std::string& error,bool skinGeometry=true) {
    if(!storylandVcsCarjackedContinuation(state)||!runtime.missionEnvironment)return false;
    const auto source=runtime.missionEnvironment->missionVehicles.find(state.sourceVehicle);
    if(source==runtime.missionEnvironment->missionVehicles.end()||source->second.deleted||source->second.spawnedSlot<0)return false;
    const auto slot=size_t(source->second.spawnedSlot);
    if(slot>=runtime.vehicles.size()||slot>=actors.size())return false;
    const auto& vehicle=runtime.vehicles[slot];const auto& actor=actors[slot];
    if(vehicle.scriptHandle!=state.sourceVehicle)return false;
    const auto& definition=vehicle.nativeDefinition;
    StorylandVehicleCarjackSample sample;
    sample.active=true;sample.rightHandSide=state.fallAnimation<=60;
    sample.elapsed=state.time;sample.fraction=state.time/state.fallDuration;
    sample.door={position.x,position.y,position.z};
    const auto seat=storylandVehicleOccupantFrame(definition,
        {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
        vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
        0,actor.water,vehicle.craneCarried?&vehicle.craneFrame:nullptr);
    std::array<float,3> anchor{};
    if(!storylandSampleVehicleCarjackVictim(animation,model,definition,sample,seat,now,
        points,anchor,error,&blend.boneFrames,&blend.poseTransition,skinGeometry))return false;
    StorylandVehicleMatrix standing;
    if(!storylandStandingPedWorldFrame(model,position,heading,1.8f,standing))return false;
    const float squaredScale=standing[0]*standing[0]+standing[1]*standing[1]+standing[2]*standing[2];
    if(!std::isfinite(squaredScale)||squaredScale<=0)return false;
    StorylandVehicleMatrix inverse{};inverse[15]=1;
    for(size_t row=0;row<3;++row)for(size_t column=0;column<3;++column)
        inverse[column*4+row]=standing[row*4+column]/squaredScale;
    for(size_t row=0;row<3;++row)
        inverse[12+row]=-(inverse[row]*standing[12]+inverse[4+row]*standing[13]+inverse[8+row]*standing[14]);
    const auto orientation=storylandScriptObjectMatrix(0,0,0,0,0,90-storylandPedModelForwardAngle(model));
    const auto relative=storylandVehicleMatrixProduct(inverse,storylandVehicleMatrixProduct(seat,orientation));
    const float yaw=(90-storylandPedModelForwardAngle(model))*.01745329251994329577f;
    const std::array<float,3> localAnchor{std::cos(yaw)*anchor[0]+std::sin(yaw)*anchor[1],
        -std::sin(yaw)*anchor[0]+std::cos(yaw)*anchor[1],anchor[2]};
    const auto normal=[&](float& x,float& y,float& z) {
        const float a=relative[0]*x+relative[4]*y+relative[8]*z;
        const float b=relative[1]*x+relative[5]*y+relative[9]*z;
        const float c=relative[2]*x+relative[6]*y+relative[10]*z;
        const float length=std::hypot(a,b,c);
        if(length>0) {x=a/length;y=b/length;z=c/length;}
    };
    for(auto& point:points) {
        const auto transformed=storylandVehicleTransformPoint(relative,
            {point.x+localAnchor[0],point.y+localAnchor[1],point.z+localAnchor[2]});
        point.x=transformed[0];point.y=transformed[1];point.z=transformed[2];
        normal(point.nx,point.ny,point.nz);
        if(point.hasPs2Normal)normal(point.ps2Nx,point.ps2Ny,point.ps2Nz);
    }
    for(auto& frame:blend.boneFrames)frame=storylandVehicleMatrixProduct(relative,frame);
    const auto& bones=model.armatureBones();
    if(blend.poseTransition.previousLocal.size()==bones.size())
        for(size_t bone=0;bone<bones.size();++bone)if(bones[bone].parentIndex==UINT32_MAX)
            blend.poseTransition.previousLocal[bone]=blend.boneFrames[bone];
    return true;
}

inline unsigned storylandAdvanceVcsCarjackVictims(StorylandPlayTestAnimation& animation,
    StorylandPlayTestRuntime& runtime,const std::vector<StorylandNativeTrafficActor>& actors,
    StorylandNativePoint& player) {
    if(!runtime.crime.vcs||!runtime.missionEnvironment||runtime.paused||runtime.pauseMenu)return 0;
    auto& world=*runtime.missionEnvironment;
    if(world.cutscene.playing)return 0;
    unsigned completed=0;
    for(size_t slot=0;slot<runtime.vehicles.size()&&slot<actors.size();++slot) {
        auto& vehicle=runtime.vehicles[slot];const auto& actor=actors[slot];
        if(vehicle.coopControlled||!vehicle.nativeDefinition.motorcycle())continue;
        const auto sample=storylandVehicleCarjackSample(runtime,slot);
        if(!sample.active||!sample.motorcycleVictim)continue;
        const auto options=animation.vehicleAnimationOptions(vehicle.nativeDefinition);
        const float hitDuration=animation.vehicleActionDuration("bike_victim_hit",options,true);
        if(!std::isfinite(hitDuration)||hitDuration<=0||sample.elapsed<hitDuration)continue;
        auto car=world.missionVehicles.find(vehicle.scriptHandle);
        const bool playerVictim=runtime.occupiedVehicle==int32_t(slot)&&runtime.playerVehicleSeat<0&&
            world.inVehicle&&world.playerPassengerSeat<0;
        int32_t victim=car==world.missionVehicles.end()?-1:car->second.driver;
        auto character=world.missionCharacters.find(victim);
        const bool ambient=character==world.missionCharacters.end()&&!playerVictim&&
            vehicle.hasDriver&&!vehicle.missionDriver;
        if(!playerVictim&&!ambient&&character==world.missionCharacters.end())continue;
        uint8_t seed=uint8_t(runtime.combatRandom);
        if(character!=world.missionCharacters.end()) {
            if(!character->second.nativePedRandomSeedInitialized) {
                character->second.nativePedRandomSeed=uint8_t(world.nextNativeRandom());
                character->second.nativePedRandomSeedInitialized=true;
            }
            seed=character->second.nativePedRandomSeed;
        }
        StorylandVcsPedFallState fall;
        if(!storylandVcsCarjackFall(animation,sample.rightHandSide,world.gameMilliseconds,
            seed,world.nativeFrameCounter,actor,vehicle,fall))continue;
        if(ambient) {
            victim=storylandPromoteCarjackDriver(runtime,slot,actor,sample.door);
            character=world.missionCharacters.find(victim);
            if(character==world.missionCharacters.end())continue;
            fall.getUpAfter=storylandVcsCarjackRecoveryDeadline(world.gameMilliseconds,
                character->second.nativePedRandomSeed,world.nativeFrameCounter);
            fall.sourceVehicle=vehicle.scriptHandle;
            car=world.missionVehicles.find(vehicle.scriptHandle);
        }
        if(playerVictim) {
            runtime.playerJetSkiFall=fall;runtime.playerJetSkiFallActive=true;
            auto& pose=runtime.vehicleExitPose;
            pose={};pose.active=true;pose.animation=fall.animation();pose.group=0;
            pose.sourceVehicle=int32_t(slot);pose.duration=fall.duration();pose.velocity=fall.velocity;
            player={sample.door[0],sample.door[1],sample.door[2]};runtime.playerPosition=player;
            runtime.occupiedVehicle=-1;runtime.playerVehicleSeat=-1;
            runtime.grounded=false;runtime.verticalSpeed=fall.velocity[2];runtime.jumpHorizontalValid=false;
            world.x=player.x;world.y=player.y;world.z=player.z;
            world.inVehicle=false;world.occupiedMissionVehicle=-1;world.playerPassengerSeat=-1;
            world.playerVehicleTransition=false;world.playerExitVehicleRequested=false;
            world.playerWarped=false;
        } else {
            auto& ped=character->second;
            ped.vehicle=-1;ped.passengerSeat=-1;ped.passengerSideCarjack=false;
            ped.attached=false;ped.attachment={};
            ped.x=sample.door[0];ped.y=sample.door[1];ped.z=sample.door[2];
            ped.heading=actor.yaw-90;ped.animationId=-1;ped.walkingToTarget=false;ped.movementSpeed=0;
            ped.seekRoute.clear();ped.seekRouteCursor=0;
            ped.objective=ped.behavior.frightenedWhenJacked?0x00F2:0x00E4;
            ped.objectiveTarget=ped.behavior.frightenedWhenJacked?vehicle.scriptHandle:0;
            ped.objectiveCompleted=false;
            world.characterVehicleTasks.erase(victim);world.scriptedCharacterAnimations.erase(victim);
            world.characterWanderTasks.erase(victim);world.characterTargets.erase(victim);
            storylandClearCrouch(ped.crouch,world.gameMilliseconds);
            auto& combat=runtime.missionCombat[victim];
            combat.contact.clear();combat.melee={};combat.crouchPose={};
            combat.verticalSpeed=fall.velocity[2];combat.grounded=false;
            combat.gravityPositionValid=false;combat.airborneTime=combat.landingTime=0;
            world.vcsPedFalls[victim]=fall;
        }
        if(car!=world.missionVehicles.end())car->second.driver=-1;
        vehicle.hasDriver=false;vehicle.missionDriver=false;vehicle.displacedDriverTime=0;
        for(auto& entry:world.characterVehicleTasks)
            if(entry.second.vehicle==vehicle.scriptHandle&&entry.second.hijacking&&entry.second.animating)
                entry.second.driverRemoved=true;
        if(runtime.vehicleAction==StorylandVehicleAction::Hijack&&runtime.transitionVehicle==int32_t(slot))
            runtime.crime.report(StorylandCrime::VehicleTheft,player,runtime.elapsed,false,
                0x80000000u|uint32_t(slot));
        ++completed;
    }
    return completed;
}
