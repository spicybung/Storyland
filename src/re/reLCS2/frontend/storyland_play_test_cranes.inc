// Native cranes own a physical vehicle frame while carrying it. Keep that
// ownership separate from script-authored freeze and damage settings.
static bool storylandReadCraneVehicle(int32_t handle,StorylandLcsCraneVehicle& result,
    std::string& error) {
    error.clear();
    auto& world=gPlayTestMissionEnvironment;
    const auto found=world.missionVehicles.find(handle);
    if(found==world.missionVehicles.end()||found->second.deleted)return false;
    const auto& scripted=found->second;
    if(scripted.spawnedSlot<0)return false;
    const size_t slot=size_t(scripted.spawnedSlot);
    if(slot>=gNativeVehicles.size()||slot>=gPlayTestRuntime.vehicles.size()||
        gPlayTestRuntime.vehicles[slot].scriptHandle!=handle)return false;
    const auto& vehicle=gPlayTestRuntime.vehicles[slot];
    const auto& actor=gNativeVehicles[slot];
    const auto& definition=vehicle.nativeDefinition;
    result={};result.handle=handle;result.model=int32_t(definition.modelId);
    result.automobile=definition.nativeType==0&&definition.domain==StorylandVehicleDomain::Road&&
        !definition.twoWheels();
    if(!world.vehicleAttachmentFrame||!world.vehicleAttachmentFrame(handle,result.matrix)||
        (result.automobile&&!definition.collision)) {
        error="Crane vehicle has no native body transform or collision bounds.";return false;
    }
    result.health=scripted.healthChangedByScript?scripted.health:vehicle.health;
    result.wrecked=result.health<=0||scripted.scriptStatus==5;
    result.roofHeight=definition.collision?definition.collision->maximum.z:0;
    result.hasDriver=vehicle.hasDriver||vehicle.missionDriver||int(slot)==gPlayTestRuntime.occupiedVehicle;
    if(scripted.driver>=0) {
        const auto driver=world.missionCharacters.find(scripted.driver);
        result.hasDriver=result.hasDriver||(driver!=world.missionCharacters.end()&&driver->second.alive);
    }
    const auto action=gPlayTestRuntime.vehicleAction;
    result.beingEntered=int(slot)==gPlayTestRuntime.transitionVehicle&&
        (action==StorylandVehicleAction::Enter||action==StorylandVehicleAction::Hijack);
    const float yaw=actor.yaw*.0174532925199433f;
    result.velocity={(std::cos(yaw)*vehicle.speed-std::sin(yaw)*vehicle.lateralSpeed)/50,
        (std::sin(yaw)*vehicle.speed+std::cos(yaw)*vehicle.lateralSpeed)/50,vehicle.verticalSpeed/50};
    return true;
}

static bool storylandTransformCrane(const StorylandLcsCrane& crane,
    const StorylandLcsCraneMatrix& matrix,std::string& error) {
    const auto& instances=gDtzArchive.nativeWorld().instances;
    const auto found=std::find_if(instances.begin(),instances.end(),[&](const auto& instance) {
        return ((uint64_t(instance.poolHeader)<<32)|instance.poolIndex)==crane.worldKey;
    });
    if(found==instances.end()||int32_t(found->modelId)!=crane.model||
        !storylandLcsCraneFiniteMatrix(matrix)) {
        error="Crane native building identity or transform is invalid.";return false;
    }
    const auto previous=found->matrix;
    struct RenderBinding {
        StorylandArchiveBrowser* browser=nullptr;
        std::vector<StorylandLcsDynamicBinding> placements;
        std::vector<StorylandLcsCraneMatrix> previous;
    };
    std::vector<RenderBinding> render;
    size_t count=0;
    const auto prepare=[&](StorylandArchiveBrowser& browser) {
        RenderBinding binding;binding.browser=&browser;
        if(!storylandFindLcsCraneRenderBindings(browser,gDtzArchive.unpackedBytes(),
            gDtzArchive.nativeWorld(),*found,crane.originalMatrix,binding.placements,error))return false;
        for(const auto& placement:binding.placements) {
            if(placement.placement>=browser.placements().size()) {
                error="Crane render placement is no longer present.";return false;
            }
            StorylandLcsCraneMatrix old{};
            std::copy_n(browser.placements()[placement.placement].matrix,16,old.begin());
            binding.previous.push_back(old);
        }
        count+=binding.placements.size();render.push_back(std::move(binding));return true;
    };
    if(!prepare(gArchiveBrowser))return false;
    for(auto& area:gAreaArchives)if(area.browser&&!prepare(*area.browser))return false;
    if(count==0&&crane.status!=StorylandLcsCrane::None) {
        error="The active crane's native render geometry is not loaded.";return false;
    }
    if(!gDtzArchive.setNativeInstanceTransform(crane.worldKey,matrix)) {
        error="Crane native collision transform was rejected.";return false;
    }
    for(auto& binding:render)if(!storylandApplyLcsDynamicTransform(*binding.browser,binding.placements,matrix)) {
        for(auto& rollback:render)for(size_t index=0;index<rollback.placements.size();++index)
            rollback.browser->setPlacementTransform(rollback.placements[index].placement,rollback.previous[index]);
        gDtzArchive.setNativeInstanceTransform(crane.worldKey,previous);
        error="Crane render transform was rejected.";return false;
    }
    gBuildingSwapsApplied=true;return true;
}

static bool storylandUpdateCraneVehicle(int32_t handle,const StorylandLcsCraneMatrix& matrix,
    bool carried,bool crusher,std::string& error) {
    StorylandLcsCraneVehicle current;
    if(!storylandReadCraneVehicle(handle,current,error)) {
        if(error.empty())error="Crane vehicle disappeared before its transform was applied.";
        return false;
    }
    if(!storylandLcsCraneFiniteMatrix(matrix)) {error="Crane vehicle transform is invalid.";return false;}
    auto& scripted=gPlayTestMissionEnvironment.missionVehicles.at(handle);
    const size_t slot=size_t(scripted.spawnedSlot);
    auto& vehicle=gPlayTestRuntime.vehicles[slot];auto& actor=gNativeVehicles[slot];
    const auto& definition=vehicle.nativeDefinition;
    StorylandVehiclePose pose;
    if(!storylandVehiclePoseFromNativeFrame(definition,matrix,vehicle.suspension,pose)) {
        error="Crane vehicle frame cannot be restored to native physics.";return false;
    }
    actor.yaw=pose.heading;vehicle.bodyRoll=pose.roll;vehicle.bodyPitch=pose.pitch;
    vehicle.roadRoll=vehicle.roadPitch=0;
    actor.position={pose.position[0],pose.position[1],pose.position[2]};
    vehicle.craneFrame=matrix;vehicle.craneCarried=carried;
    if(slot<gPlayTestRuntime.movementCars.size()) {
        auto& obstacle=gPlayTestRuntime.movementCars[slot];
        obstacle.position=actor.position;obstacle.yaw=actor.yaw;obstacle.collisionEnabled=!carried;
    }
    if(slot<gPlayTestRuntime.vehiclePositions.size())gPlayTestRuntime.vehiclePositions[slot]=actor.position;
    vehicle.speed=vehicle.lateralSpeed=vehicle.verticalSpeed=vehicle.yawVelocity=0;
    if(carried) {vehicle.headlightsOn=false;vehicle.grounded=false;}
    if(carried&&crusher) {vehicle.damageProofs|=8;scripted.damageProofs|=8;}
    scripted.x=actor.position.x;scripted.y=actor.position.y;scripted.z=actor.position.z;
    scripted.heading=actor.yaw-90;scripted.speed=0;scripted.movedByScript=true;
    return true;
}

static void bindStorylandPlayTestCranes() {
    auto& world=gPlayTestMissionEnvironment;
    if(gPlayTestMissionVm.vcs)return;
    world.craneCameraPosition=[] {
        return std::array<float,3>{gPlayTestCamera.eye.x,gPlayTestCamera.eye.y,gPlayTestCamera.eye.z};
    };
    auto& binding=world.lcsCranes.bindings;
    binding.discover=[](std::vector<StorylandLcsCrane>& cranes,std::string& error) {
        return storylandDiscoverLcsCranes(gDtzArchive.unpackedBytes(),gDtzArchive.nativeWorld(),cranes,error);
    };
    binding.transform=storylandTransformCrane;
    binding.readVehicle=storylandReadCraneVehicle;
    binding.updateVehicle=storylandUpdateCraneVehicle;
    binding.findVehicles=[](const std::array<float,4>& bounds,
        std::vector<StorylandLcsCraneVehicle>& vehicles,std::string& error) {
        auto& environment=gPlayTestMissionEnvironment;
        if(!environment.findScriptVehiclesInArea) {error="Crane vehicle pool scan is unavailable.";return false;}
        vehicles.clear();
        const auto handles=environment.findScriptVehiclesInArea(bounds,-1,StorylandMissionEnvironment::VehicleAreaRoad);
        for(int32_t handle:handles) {
            StorylandLcsCraneVehicle vehicle;std::string vehicleError;
            if(storylandReadCraneVehicle(handle,vehicle,vehicleError))vehicles.push_back(vehicle);
            else if(!vehicleError.empty()) {error=vehicleError;return false;}
        }
        return true;
    };
    binding.removeVehicle=[](int32_t handle,std::string& error) {
        StorylandLcsCraneVehicle vehicle;
        if(!storylandReadCraneVehicle(handle,vehicle,error)) {
            if(error.empty())error="Military crane vehicle is no longer present.";
            return false;
        }
        auto& scripted=gPlayTestMissionEnvironment.missionVehicles.at(handle);
        gPlayTestRuntime.vehicles[size_t(scripted.spawnedSlot)].craneCarried=false;
        scripted.deleted=true;scripted.alive=false;return true;
    };
    binding.groundHeight=[](const std::array<float,3>& point,float& height) {
        const auto& query=gPlayTestMissionEnvironment.groundHeightAt;
        return query&&query({point[0],point[1],1000},height);
    };
    binding.message=[](const std::string& key,int32_t number) {
        auto& environment=gPlayTestMissionEnvironment;
        StorylandMissionEnvironment::LcsMessage message;
        message.key=key;message.durationMilliseconds=4000;
        if(number>=0)message.numbers.push_back(number);
        storylandQueueLcsMessage(environment,std::move(message),0,StorylandLcsMessagePriority::Now);
    };
    binding.reward=[](int32_t amount) {
        auto& money=gPlayTestMissionEnvironment.money;
        money=int32_t(uint32_t(money)+uint32_t(amount));
    };
}
