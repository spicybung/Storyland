static bool storylandGarageCameraPlayerCollision(const std::vector<uint8_t>& data,
    StorylandNativeCollisionModel& collision,std::string& error) {
    error.clear();
    const auto valid=[&](uint64_t offset,uint64_t count) {
        return offset<=data.size()&&count<=data.size()-offset;
    };
    const auto word=[&](size_t offset) {
        return uint32_t(data[offset])|uint32_t(data[offset+1])<<8|
            uint32_t(data[offset+2])<<16|uint32_t(data[offset+3])<<24;
    };
    if(!valid(0,0x40)||word(0x38)==0) {
        error="Garage camera requires the native LCS player collision model.";return false;
    }
    // LCS's player model owns the shared native pedestrian collision body.
    const uint32_t table=word(0x3C);
    if(table==0||!valid(table,4)) {error="Garage camera player model table is truncated.";return false;}
    const uint32_t model=word(table);
    if(model==0||!valid(model,0x18)||data[size_t(model)+0x10]!=7) {
        error="Garage camera player model identity is invalid.";return false;
    }
    if(!storylandDecodeNativeCollisionModel(data,word(size_t(model)+0x14),
        uint32_t(data.size()),false,collision,error))return false;
    if(collision.spheres.empty()) {error="Garage camera player collision spheres are unavailable.";return false;}
    return true;
}

static bool applyStorylandPlayTestGarageCamera(bool remoteCamera,std::string& error) {
    error.clear();
    auto& world=gPlayTestMissionEnvironment;
    auto& cache=world.lcsGarageCamera;
    if(gPlayTestMissionVm.vcs||remoteCamera||world.camera.active||gPlayTestRuntime.health<=0||
        gPlayTestRuntime.debugMovement!=StorylandDebugMovement::Walking) {
        cache={};return true;
    }
    if(world.garages.empty()) {cache={};return true;}
    StorylandNativeCollisionModel playerCollision;
    if(!storylandGarageCameraPlayerCollision(gDtzArchive.unpackedBytes(),playerCollision,error))return false;
    const float angle=(gPlayTestPlayerHeading-90)*.0174532925199433f;
    const float cosine=std::cos(angle),sine=std::sin(angle);
    StorylandVehicleMatrix playerFrame{cosine,sine,0,0,-sine,cosine,0,0,0,0,1,0,
        gPlayerWorldX,gPlayerWorldY,gPlayerWorldZ-playerCollision.minimum.z,1};
    StorylandLcsGarageCameraPoint target{playerFrame[12],playerFrame[13],playerFrame[14]};
    int32_t vehicleModel=-1,vehicleType=-1;
    auto mode=StorylandLcsGarageCameraMode::FollowPed;
    if(gPlayTestRuntime.occupiedVehicle>=0) {
        const size_t slot=size_t(gPlayTestRuntime.occupiedVehicle);
        if(slot>=gNativeVehicles.size()||slot>=gPlayTestRuntime.vehicles.size()) {
            error="Garage camera occupied vehicle has no physical pool slot.";return false;
        }
        const auto& actor=gNativeVehicles[slot];const auto& vehicle=gPlayTestRuntime.vehicles[slot];
        const auto& definition=vehicle.nativeDefinition;
        const std::array<float,3> position{actor.position.x,actor.position.y,actor.position.z};
        const auto mesh=storylandVehicleMeshFrame(definition,position,actor.yaw,definition.previewMinimumZ,
            vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
            actor.water,vehicle.craneCarried?&vehicle.craneFrame:nullptr);
        const auto body=storylandVehicleMatrixProduct(mesh,definition.previewRoot);
        target={body[12],body[13],body[14]};
        playerFrame=storylandVehicleOccupantFrame(definition,position,actor.yaw,definition.previewMinimumZ,
            vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,0,
            actor.water,vehicle.craneCarried?&vehicle.craneFrame:nullptr);
        vehicleModel=int32_t(definition.modelId);vehicleType=definition.nativeType;
        mode=StorylandLcsGarageCameraMode::Vehicle;
    }
    const StorylandLcsGarageCameraPoint playerOrigin{playerFrame[12],playerFrame[13],playerFrame[14]};
    std::vector<StorylandLcsGarageCameraSphere> spheres;
    spheres.reserve(playerCollision.spheres.size());
    for(const auto& sphere:playerCollision.spheres)
        spheres.push_back({storylandVehicleTransformPoint(playerFrame,
            {sphere.center.x,sphere.center.y,sphere.center.z}),sphere.radius});

    // Camera preparation only reads door identities. Opening and closing stay
    // owned by the existing garage controller and authored script commands.
    auto doors=world.lcsGarageDoors;
    for(const auto& garage:world.garages) {
        if(garage.type<=0||garage.type==13||world.lcsGarageFollowPlayer.count(garage.handle)||
            doors.count(garage.handle))continue;
        StorylandLcsGarageCameraGeometry geometry;
        if(!storylandLcsGarageCameraGeometryFor(garage,geometry))continue;
        const bool iceCreamVan=vehicleModel==154&&
            target[0]>=geometry.minimumX-.5f&&target[0]<=geometry.maximumX+.5f&&
            target[1]>=geometry.minimumY-.5f&&target[1]<=geometry.maximumY+.5f;
        if(!iceCreamVan&&!storylandLcsGarageCameraEntityInside(garage,playerOrigin,spheres,.25f))continue;
        StorylandMissionEnvironment::LcsGarageDoorState state;
        if(!storylandPrepareLcsGarageDoors(gDtzArchive.unpackedBytes(),gDtzArchive.nativeWorld(),garage,state,error))
            return false;
        doors.emplace(garage.handle,std::move(state));
    }
    struct CameraWorld {
        const std::vector<StorylandMissionEnvironment::Garage>& garages;
        const std::set<int32_t>& lcsGarageFollowPlayer;
        const std::map<int32_t,StorylandMissionEnvironment::LcsGarageDoorState>& lcsGarageDoors;
    } cameraWorld{world.garages,world.lcsGarageFollowPlayer,doors};
    const auto view=storylandResolveLcsGarageCamera(cameraWorld,cache,playerOrigin,spheres,target,
        vehicleModel,vehicleType,mode,world.groundHeightAt);
    if(view) {
        gPlayTestCamera.vehicleCameraActive=false;
        gPlayTestCamera.eye={view->eye[0],view->eye[1],view->eye[2]};
        gPlayTestCamera.stringEye=gPlayTestCamera.eye;
        gPlayTestCamera.target={view->target[0],view->target[1],view->target[2]};
        gPlayTestCamera.aimLocked=false;
        const float x=view->target[0]-view->eye[0],y=view->target[1]-view->eye[1];
        const float horizontal=std::hypot(x,y);
        if(horizontal>.0001f)gPlayTestCamera.yaw=std::atan2(y,x)*57.295779513f;
        gPlayTestCamera.pitch=std::atan2(view->target[2]-view->eye[2],horizontal)*57.295779513f;
    }
    return true;
}
