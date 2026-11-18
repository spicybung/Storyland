namespace {

bool failVcsJetSki(StorylandPlayTestRuntime& runtime,const std::string& reason) {
    runtime.vehiclePhysicsError="Native VCS JetSki: "+reason;
    return false;
}

struct VcsJetSkiRider {
    int32_t handle=-1;
    bool player=false,driver=false,alive=true;
    int32_t seat=-1;
    StorylandNativePoint position;
    float heading=0;
    StorylandVcsPedFallState fall;
};

bool prepareVcsJetSkiRider(StorylandPlayTestRuntime& runtime,
    const StorylandNativeTrafficActor& actor,const StorylandRuntimeVehicle& vehicle,
    int32_t handle,bool player,bool driver,int32_t seat,VcsJetSkiRider& output) {
    auto& world=*runtime.missionEnvironment;
    const auto resident=world.missionCharacters.find(handle);
    if(!player&&resident==world.missionCharacters.end())
        return failVcsJetSki(runtime,"the ejected rider has no resident character.");
    const bool alive=player?runtime.health>0:resident->second.health>0;
    if(!world.animationDuration||!world.animationAssociationFlags||!world.characterOriginOffset)
        return failVcsJetSki(runtime,"the original fall animation or pedestrian origin binding is unavailable.");
    uint16_t fallFlags=0,getUpFlags=0;
    if(!world.animationAssociationFlags(0,121,fallFlags)||(fallFlags&2u))
        return failVcsJetSki(runtime,"STD121 is unavailable or unexpectedly repeats.");
    const int32_t getUpAnimation=storylandVcsPedGetUpAnimation(fallFlags);
    if(!world.animationAssociationFlags(0,getUpAnimation,getUpFlags)||(getUpFlags&2u))
        return failVcsJetSki(runtime,"the original get-up association is unavailable or unexpectedly repeats.");
    const float duration=world.animationDuration(0,121);
    const float getUpDuration=world.animationDuration(0,getUpAnimation);
    if(!std::isfinite(duration)||duration<=0||!std::isfinite(getUpDuration)||getUpDuration<=0)
        return failVcsJetSki(runtime,"the original fall/get-up clips have invalid durations.");
    float originOffset=0;
    if(!world.characterOriginOffset(handle,originOffset)||!std::isfinite(originOffset))
        return failVcsJetSki(runtime,"the actual rider COL origin is unavailable.");
    const auto& definition=vehicle.nativeDefinition;
    const auto frame=storylandVehicleOccupantFrame(definition,
        {actor.position.x,actor.position.y,actor.position.z},actor.yaw,
        definition.previewMinimumZ,vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,
        vehicle.bodyPitch+vehicle.roadPitch,unsigned(seat+1),actor.water,
        vehicle.craneCarried?&vehicle.craneFrame:nullptr,storylandVcsJetSkiNativeAngularZ(vehicle));
    if(!std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);}))
        return failVcsJetSki(runtime,"the actual seated rider frame is unavailable.");
    const float angle=actor.yaw*radians,cosine=std::cos(angle),sine=std::sin(angle);
    storyland_vcs_jetski_knockoff::Input input;
    input.pedestrianMass=player?runtime.playerMass:StorylandNativePedMass;
    input.vehicleMass=vehicle.mass;
    input.nativeNormalImpulse=vehicle.vcsJetSkiContactImpulse;
    input.contactNormal=vehicle.vcsJetSkiContactNormal;
    input.nativeVehicleVelocity={
        (cosine*vehicle.speed-sine*vehicle.lateralSpeed)/StorylandNativePhysicsRate,
        (sine*vehicle.speed+cosine*vehicle.lateralSpeed)/StorylandNativePhysicsRate,
        vehicleVerticalVelocity(vehicle)/StorylandNativePhysicsRate};
    input.nativeRandom=float(world.nextNativeRandom())/2147483648.0f;
    storyland_vcs_jetski_knockoff::Output impulse;
    if(!storyland_vcs_jetski_knockoff::force(input,impulse))
        return failVcsJetSki(runtime,"the live argument47 collision force is invalid.");
    if(!world.objectWorld)
        return failVcsJetSki(runtime,"the native ejection building collision world is unavailable.");
    const auto vehicleFrame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
        {actor.position.x,actor.position.y,actor.position.z},actor.yaw,definition.previewMinimumZ,
        vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
        actor.water,vehicle.craneCarried?&vehicle.craneFrame:nullptr),definition.previewRoot);
    storyland_vcs_jetski_placement::Input placementInput;
    placementInput.origin={frame[12],frame[13],frame[14]};
    placementInput.vehicleOrigin={vehicleFrame[12],vehicleFrame[13],vehicleFrame[14]};
    placementInput.nativeVelocity=impulse.nativeVelocity;
    placementInput.nativeStep=world.nativeScriptTimeStep;
    storyland_vcs_jetski_placement::Output placement;
    if(!storyland_vcs_jetski_placement::evaluate(placementInput,
        [&](const std::array<float,3>& point,float radius,bool& hit) {
            return storylandNativeBuildingSphereContact(*world.objectWorld,
                {point[0],point[1],point[2]},radius,hit);
        },[&](const std::array<float,3>& point,float endZ,bool& hit) {
            const float fraction=storylandTraceNativeBuildings(*world.objectWorld,
                {point[0],point[1],point[2]},{point[0],point[1],endZ});
            if(!std::isfinite(fraction)||fraction<0||fraction>1)return false;
            hit=fraction<1;return true;
        },placement))return failVcsJetSki(runtime,"the original ejection placement query is invalid.");
    if(placement.nativeBlockedFlags)
        return failVcsJetSki(runtime,
            placement.kind==storyland_vcs_jetski_placement::Kind::CollisionRecovery?
                "blocked ejection requires the unported native27A2A8 vehicle-exit collision recovery.":
                "blocked ejection requires the unported native202878 airborne-association state.");
    VcsJetSkiRider rider;
    rider.handle=handle;rider.player=player;rider.driver=driver;rider.seat=seat;rider.alive=alive;
    rider.position={placement.origin[0],placement.origin[1],placement.origin[2]-originOffset};
    rider.heading=std::atan2(frame[5],frame[4])/radians;
    rider.fall.sourceVehicle=vehicle.scriptHandle;
    rider.fall.fallAnimation=121;rider.fall.getUpAnimation=getUpAnimation;
    rider.fall.fallDuration=duration;rider.fall.getUpDuration=getUpDuration;
    rider.fall.time=alive?std::min(impulse.initialAnimationTime,duration):0;
    for(size_t axis=0;axis<3;++axis)
        rider.fall.velocity[axis]=impulse.nativeVelocity[axis]*StorylandNativePhysicsRate;
    if(!alive)rider.fall.getUpAfter=UINT32_MAX;
    else if(player) {
        if(!storyland_vcs_jetski_knockoff::playerGetUpDeadline(world.gameMilliseconds,
            duration,rider.fall.getUpAfter))return failVcsJetSki(runtime,"the player fall deadline is invalid.");
    } else {
        const auto character=world.missionCharacters.find(handle);
        if(character==world.missionCharacters.end())
            return failVcsJetSki(runtime,"a detached rider has no resident mission character.");
        auto& ped=character->second;
        if(!ped.nativePedRandomSeedInitialized) {
            ped.nativePedRandomSeed=uint8_t(world.nextNativeRandom());
            ped.nativePedRandomSeedInitialized=true;
        }
        rider.fall.getUpAfter=storylandVcsPedGetUpDeadline(world.gameMilliseconds,
            duration,ped.nativePedRandomSeed,world.nativeFrameCounter);
    }
    if(!storylandValidVcsPedFall(rider.fall)||!std::isfinite(rider.position.z)||
        !std::isfinite(rider.heading))return failVcsJetSki(runtime,"the original rider fall state is invalid.");
    output=rider;
    return true;
}

bool ejectVcsJetSkiRiders(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& actors,size_t slot,
    StorylandNativePoint& player,float& heading) {
    auto& world=*runtime.missionEnvironment;
    auto& vehicle=runtime.vehicles[slot];const auto& actor=actors[slot];
    const auto scripted=world.missionVehicles.find(vehicle.scriptHandle);
    const bool playerHere=runtime.occupiedVehicle==int32_t(slot)&&
        runtime.vehicleAction==StorylandVehicleAction::None;
    std::array<VcsJetSkiRider,2> riders;
    size_t count=0;
    for(int32_t seat=-1;seat<=0;++seat) {
        int32_t handle=-1;
        const bool isPlayer=playerHere&&runtime.playerVehicleSeat==seat;
        if(isPlayer)handle=0;
        else if(scripted!=world.missionVehicles.end()&&!scripted->second.deleted) {
            if(seat<0)handle=scripted->second.driver;
            else for(const auto& entry:world.missionCharacters) {
                const auto& ped=entry.second;
                if(ped.vehicle==vehicle.scriptHandle&&ped.passengerSeat==0&&!ped.removeFromWorld&&
                    !ped.attached&&!ped.networkControlled) {handle=entry.first;break;}
            }
        }
        if(handle<0) {
            if(seat<0&&vehicle.hasDriver)
                return failVcsJetSki(runtime,"an unrepresented ambient driver cannot enter the native fall task.");
            continue;
        }
        if(!isPlayer) {
            const auto character=world.missionCharacters.find(handle);
            if(character==world.missionCharacters.end()||character->second.vehicle!=vehicle.scriptHandle||
                character->second.passengerSeat!=seat||character->second.attached||
                character->second.removeFromWorld||character->second.networkControlled)
                return failVcsJetSki(runtime,"the original driver/passenger reference is not a live seated actor.");
            const auto transition=world.characterVehicleTasks.find(handle);
            if(transition!=world.characterVehicleTasks.end()&&transition->second.animating)
                return failVcsJetSki(runtime,"a rider is still in its animated entry/exit task.");
        }
        if(!prepareVcsJetSkiRider(runtime,actor,vehicle,handle,isPlayer,seat<0,seat,riders[count]))return false;
        ++count;
    }
    for(size_t index=0;index<count;++index) {
        const auto& rider=riders[index];
        if(rider.player) {
            scriptPlayerLeavesVehicle(runtime,vehicle);
            player=rider.position;heading=rider.heading;
            runtime.playerJetSkiFall=rider.fall;runtime.playerJetSkiFallActive=true;
            if(!rider.alive) {
                runtime.playerDeath={rider.fall.fallAnimation,4,1};
                runtime.playerDeathTime=0;
            }
            runtime.vehicleExitPose={};
            auto& pose=runtime.vehicleExitPose;
            pose.active=true;pose.group=0;pose.animation=rider.fall.fallAnimation;
            pose.sourceVehicle=int32_t(slot);pose.elapsed=rider.fall.time;
            pose.duration=rider.fall.fallDuration;pose.velocity=rider.fall.velocity;
            runtime.occupiedVehicle=-1;runtime.playerVehicleSeat=-1;
            runtime.vehicleAction=StorylandVehicleAction::None;runtime.transitionVehicle=-1;
            runtime.vehicleTransitionPrepared=false;
            runtime.vehicleEntryVariant=StorylandVehicleEntryVariant::Normal;
            runtime.grounded=false;runtime.verticalSpeed=rider.fall.velocity[2];runtime.airborneTime=0;
            runtime.jumpLaunching=false;runtime.jumpFromLaunch=false;runtime.jumpHorizontalValid=false;
            runtime.playerPosition=player;runtime.playerReaction.clearAction();
            runtime.playerMelee={};storylandClearPlayTestTarget(runtime);
            storylandClearCrouch(world.playerCrouch,world.gameMilliseconds);
            world.inVehicle=false;world.occupiedMissionVehicle=-1;world.occupiedVehicleModel=-1;
            world.playerPassengerSeat=-1;world.playerVehicleTransition=false;
            world.playerExitVehicleRequested=false;
            world.x=player.x;world.y=player.y;world.z=player.z;world.heading=heading-90;
        } else {
            auto& ped=world.missionCharacters.at(rider.handle);
            const auto objective=storylandVcsEjectionObjective(false,false,ped.leader>=0,
                ped.dontReenterAfterEjection,rider.driver);
            ped.x=rider.position.x;ped.y=rider.position.y;ped.z=rider.position.z;
            ped.heading=rider.heading-90;ped.vehicle=-1;ped.passengerSeat=-1;
            ped.animationId=-1;ped.walkingToTarget=false;ped.movementSpeed=0;
            if(!rider.alive) {
                ped.deathRecorded=true;ped.deathStartedAt=world.gameMilliseconds;
                ped.animationStartedAt=world.gameMilliseconds;
                ped.animationId=rider.fall.fallAnimation;ped.animationGroup=0;
                ped.animationBlend=4;ped.animationLoop=false;
            }
            ped.seekRoute.clear();ped.seekRouteCursor=0;
            world.characterVehicleTasks.erase(rider.handle);
            world.scriptedCharacterAnimations.erase(rider.handle);
            world.characterWanderTasks.erase(rider.handle);world.characterTargets.erase(rider.handle);
            storylandClearCrouch(ped.crouch,world.gameMilliseconds);
            if(objective!=StorylandVcsEjectionObjective::Preserve) {
                ped.objective=objective==StorylandVcsEjectionObjective::EnterDriver?0x00EA:
                    objective==StorylandVcsEjectionObjective::EnterPassenger?0x00E9:0;
                ped.objectiveTarget=objective==StorylandVcsEjectionObjective::None?-1:vehicle.scriptHandle;
                ped.objectiveCompleted=false;
                if(objective==StorylandVcsEjectionObjective::EnterPassenger)ped.passengerSeat=rider.seat;
            }
            auto& combat=runtime.missionCombat[rider.handle];
            combat.contact.clear();combat.melee={};combat.crouchPose={};
            combat.verticalSpeed=rider.fall.velocity[2];combat.grounded=false;
            combat.gravityPositionValid=false;combat.airborneTime=combat.landingTime=0;
            world.vcsPedFalls[rider.handle]=rider.fall;
        }
        if(rider.driver) {
            vehicle.hasDriver=false;vehicle.missionDriver=false;vehicle.parked=false;
            if(scripted!=world.missionVehicles.end()) {
                scripted->second.driver=-1;scripted->second.scriptStatus=4;
            }
        }
    }
    vehicle.vcsJetSkiPose={};
    refreshMovementCars(runtime,actors);
    return true;
}

bool updateVcsJetSkiPlayerFall(StorylandPlayTestRuntime& runtime,const StorylandNativeWorld& world,
    StorylandNativePoint& player,float heading,float seconds) {
    if(!runtime.playerJetSkiFallActive)return false;
    auto* environment=runtime.missionEnvironment;
    if(!environment||!world.paths.vcs||!runtime.vehicleExitPose.active||
        environment->cutscene.playing||environment->playerWarped||
        environment->activeVcsCharacterAttachment(0)||runtime.occupiedVehicle>=0) {
        runtime.playerJetSkiFall={};runtime.playerJetSkiFallActive=false;
        runtime.vehicleExitPose={};return false;
    }
    if(!std::isfinite(seconds)||seconds<=0)return true;
    auto& fall=runtime.playerJetSkiFall;
    if(!storylandValidVcsPedFall(fall)) {
        failVcsJetSki(runtime,"the active player fall state is invalid.");return true;
    }
    const float step=std::min(seconds,.1f);
    if(fall.fallAnimation>=59&&fall.fallAnimation<=62&&runtime.health<=0) {
        runtime.playerJetSkiFall={};runtime.playerJetSkiFallActive=false;
        runtime.vehicleExitPose={};runtime.movementSpeed=0;return true;
    }
    if(fall.fallAnimation>=59&&fall.fallAnimation<=62&&!fall.recovering) {
        fall.time=std::min(fall.fallDuration,fall.time+step);
        if(fall.time>=fall.fallDuration) {fall.recovering=true;fall.time=0;fall.velocity={};}
        auto& pose=runtime.vehicleExitPose;
        pose.animation=fall.animation();pose.elapsed=fall.time;pose.duration=fall.duration();
        runtime.movementSpeed=0;runtime.playerPosition=player;
        return true;
    }
    float surface=0;
    const bool waterContact=deepCharacterWater(world,player,surface);
    if(runtime.health>0&&!fall.recovering&&
        storyland_vcs_jetski_knockoff::canBeginGetUp(environment->gameMilliseconds,
            fall.getUpAfter,fall.grounded,waterContact)) {
        fall.recovering=true;fall.time=0;fall.velocity={};
    }
    const float previousTime=fall.time;
    fall.time=std::min(fall.duration(),fall.time+step);
    const StorylandNativePoint before=player;
    StorylandNativePoint target{player.x+fall.velocity[0]*step,player.y+fall.velocity[1]*step,player.z};
    if(fall.recovering) {
        if(!runtime.reactionBindings.rootDelta) {
            failVcsJetSki(runtime,"the actual get-up root displacement is unavailable.");return true;
        }
        const auto root=runtime.reactionBindings.rootDelta(fall.animation(),0,previousTime,fall.time);
        if(!std::isfinite(root.x)||!std::isfinite(root.y)||!std::isfinite(root.z)) {
            failVcsJetSki(runtime,"the actual get-up root displacement is invalid.");return true;
        }
        const float angle=heading*radians;
        target.x+=std::cos(angle)*root.x+std::sin(angle)*root.y;
        target.y+=std::sin(angle)*root.x-std::cos(angle)*root.y;
    }
    bool landed=false;
    float floor=0;
    if(waterContact) {
        const auto water=storylandStepWaterCharacter(runtime.playerWater,player.z,surface,true,
            runtime.playerCanSwim,false,false,step,true,std::hypot(fall.velocity[0],fall.velocity[1]),0,
            !runtime.missionEnvironment||runtime.missionEnvironment->playerDrownsInWater,
            runtime.missionEnvironment&&runtime.missionEnvironment->playerInfiniteSwim);
        target.z=water.height;fall.velocity[2]=runtime.playerWater.verticalSpeed;
        runtime.inflictDamage(water.damage,0,storylandNativeDrowningWeapon(true),false);
        if(water.entered)runtime.soundEvents.emplace_back(StorylandPlayTestSound::WaterSplash);
    } else {
        storylandStepWaterCharacter(runtime.playerWater,player.z,0,false,true,false,false,step,true);
        const auto vertical=storylandPedVerticalStep(fall.velocity[2],runtime.gravity,step);
        target.z+=vertical.displacement;fall.velocity[2]=vertical.velocity;
        const bool hasFloor=vehicleGroundHeight(world,target.x,target.y,std::max(before.z,target.z),floor);
        landed=hasFloor&&fall.velocity[2]<=0&&target.z<=floor+.005f;
        if(landed)target.z=floor;
    }
    const auto moved=storylandMovePlayTestCharacter(world,runtime,before,target,-1,false,false,
        false,runtime.vehicleExitPose.sourceVehicle);
    if(moved.clear)player=moved.position;
    fall.grounded=landed&&std::fabs(player.z-floor)<.05f;
    if(fall.grounded)fall.velocity[2]=0;
    if(moved.blocked&&target.z>before.z)fall.velocity[2]=std::min(0.0f,fall.velocity[2]);
    const float travelled=std::hypot(player.x-before.x,player.y-before.y);
    if(moved.blocked&&travelled<.15f*std::hypot(target.x-before.x,target.y-before.y))
        fall.velocity[0]=fall.velocity[1]=0;
    runtime.grounded=fall.grounded;runtime.verticalSpeed=fall.velocity[2];
    runtime.movementSpeed=travelled/step;runtime.playerPosition=player;
    if(fall.grounded||waterContact)runtime.airborneTime=0;
    else runtime.airborneTime+=step;
    auto& pose=runtime.vehicleExitPose;
    pose.animation=fall.animation();pose.elapsed=fall.time;pose.duration=fall.duration();pose.velocity=fall.velocity;
    if((fall.recovering&&fall.time>=fall.getUpDuration)||
        (runtime.health<=0&&(fall.grounded||waterContact)&&fall.time>=fall.fallDuration)) {
        runtime.playerJetSkiFallActive=false;runtime.playerJetSkiFall={};pose={};
        runtime.movementSpeed=0;runtime.jumpHorizontalValid=false;
    }
    return true;
}

}

void storylandUpdateVcsJetSkiOccupants(StorylandPlayTestRuntime& runtime,
    std::vector<StorylandNativeTrafficActor>& actors,StorylandNativePoint& player,float& heading,
    float seconds) {
    if(!runtime.crime.vcs||!runtime.vehiclePhysicsError.empty()||runtime.paused||runtime.pauseMenu||
        !std::isfinite(seconds)||seconds<=0)return;
    auto* environment=runtime.missionEnvironment;
    if(environment&&environment->cutscene.playing)return;
    for(size_t slot=0;slot<actors.size()&&slot<runtime.vehicles.size();++slot) {
        auto& vehicle=runtime.vehicles[slot];const auto& actor=actors[slot];
        if(!vehicle.nativeDefinition.vcsJetSki())continue;
        if(vehicle.coopControlled)continue;
        const bool seatedPlayerDriver=runtime.occupiedVehicle==int32_t(slot)&&runtime.playerVehicleSeat<0;
        const bool playerDriver=seatedPlayerDriver&&
            runtime.vehicleAction==StorylandVehicleAction::None;
        int32_t driver=-1;
        bool actualDriver=seatedPlayerDriver;
        if(playerDriver)driver=0;
        else if(environment&&vehicle.scriptHandle>=0) {
            const auto scripted=environment->missionVehicles.find(vehicle.scriptHandle);
            if(scripted!=environment->missionVehicles.end()&&!scripted->second.deleted) {
                const auto ped=environment->missionCharacters.find(scripted->second.driver);
                if(ped!=environment->missionCharacters.end()&&ped->second.vehicle==vehicle.scriptHandle&&
                    ped->second.passengerSeat<0&&!ped->second.removeFromWorld&&!ped->second.attached) {
                    actualDriver=true;
                    const auto task=environment->characterVehicleTasks.find(ped->first);
                    if(task==environment->characterVehicleTasks.end()||!task->second.animating)driver=ped->first;
                }
            }
        } else if(vehicle.hasDriver) {driver=INT32_MAX;actualDriver=true;}
        const bool forcedDriverBrakes=seatedPlayerDriver&&environment&&
            (!environment->control||environment->applyPlayerCarBrakes||environment->playerCarBrakes);
        if(!actualDriver||forcedDriverBrakes)vehicle.vcsJetSkiBrake=1;
        if(forcedDriverBrakes) {
            vehicle.throttle=0;
            vehicle.handbrake=true;
        }
        if(driver<0) {vehicle.vcsJetSkiPose={};continue;}
        if(!environment) {failVcsJetSki(runtime,"the native clock and input environment is unavailable.");return;}
        const auto frame=storylandVehicleMatrixProduct(storylandVehicleMeshFrame(vehicle.nativeDefinition,
            {actor.position.x,actor.position.y,actor.position.z},actor.yaw,
            vehicle.nativeDefinition.previewMinimumZ,vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,
            vehicle.bodyPitch+vehicle.roadPitch,actor.water,
            vehicle.craneCarried?&vehicle.craneFrame:nullptr),vehicle.nativeDefinition.previewRoot);
        if(!std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);})) {
            failVcsJetSki(runtime,"the current physical vehicle frame is invalid.");return;
        }
        if(vehicle.water.buoyant&&frame[10]<-.6f) {
            failVcsJetSki(runtime,
                "inverted-water ejection42 requires the unported native202878 airborne-association state.");
            return;
        }
        const float angle=actor.yaw*radians,cosine=std::cos(angle),sine=std::sin(angle);
        const std::array<float,3> velocity{cosine*vehicle.speed-sine*vehicle.lateralSpeed,
            sine*vehicle.speed+cosine*vehicle.lateralSpeed,vehicleVerticalVelocity(vehicle)};
        const auto angular=vehicleAngularVelocity(vehicle,actor.yaw);
        storyland_vcs_jetski_driving::DrivingInput input;
        input.player=playerDriver;
        input.nativeForwardSpeed=((velocity[0]*frame[4]+velocity[1]*frame[5])+
            velocity[2]*frame[6])/StorylandNativePhysicsRate;
        input.nativeAngularSpeedSquared=(angular.x*angular.x+angular.y*angular.y+angular.z*angular.z)/
            (StorylandNativePhysicsRate*StorylandNativePhysicsRate);
        input.upZ=frame[10];input.throttle=vehicle.throttle;
        input.brake=vehicle.vcsJetSkiBrake;
        input.maximumForwardSpeed=vehicle.nativeDefinition.maximumAiVelocity;
        input.reverseSpeedLimit=vehicle.nativeDefinition.reverseVelocity;
        input.nativeStep=environment->nativeScriptTimeStep;
        input.blendPredictionTime=environment->nativeAnimationTimeStep;
        storyland_vcs_jetski_driving::ControlInput controls;
        if(playerDriver) {
            controls.horizontal=int16_t(storylandVcsPadState(*environment,2));
            controls.vertical=int16_t(storylandVcsPadState(*environment,3));
        }
        controls.nativeStep=input.nativeStep;
        bool knockOff=false;
        if(!storyland_vcs_jetski_pose::advance(vehicle.nativeDefinition.vcsJetSkiAnimations,
            driver,environment->nativeFrameCounter,input,controls,vehicle.vcsJetSkiAxes,
            vehicle.vcsJetSkiPose,knockOff)) {
            failVcsJetSki(runtime,"the decoded association/control update failed.");return;
        }
        if(knockOff&&!ejectVcsJetSkiRiders(runtime,actors,slot,player,heading))return;
    }
}
