static bool storylandVcsHovercraftPlayerControls(const StorylandPlayTestRuntime& runtime,
    const StorylandRuntimeVehicle& vehicle, size_t slot) {
    if (int32_t(slot) != runtime.occupiedVehicle || runtime.playerVehicleSeat >= 0)
        return false;
    if (!runtime.missionEnvironment) return false;
    const auto& environment = *runtime.missionEnvironment;
    const auto scripted = environment.missionVehicles.find(vehicle.scriptHandle);
    return scripted == environment.missionVehicles.end() || scripted->second.scriptStatus != 3;
}

static bool storylandVcsHovercraftPhysical(const StorylandPlayTestRuntime& runtime,
    const StorylandRuntimeVehicle& vehicle, size_t slot) {
    if (!vehicle.nativeDefinition.vcsHovercraft() || vehicle.heliCrashing) return false;
    if (runtime.missionEnvironment && vehicle.scriptHandle >= 0 &&
        runtime.missionEnvironment->remoteVehicle == vehicle.scriptHandle) return false;
    if (!vehicle.aiActive || storylandVcsHovercraftPlayerControls(runtime,vehicle,slot)) return true;
    if (!runtime.missionEnvironment) return false;
    const auto& scripted = runtime.missionEnvironment->missionVehicles;
    const auto found = scripted.find(vehicle.scriptHandle);
    // Retained autopilot data does not own an abandoned physical body.
    return found != scripted.end() && found->second.scriptStatus == 4;
}

static bool storylandVcsHovercraftSprings(StorylandRuntimeVehicle& vehicle,
    const StorylandNativeTrafficActor& actor, const StorylandNativeWorld& world,
    const std::array<float,16>& matrix, float nativeStep,
    StorylandVcsHovercraft::State& state) {
    namespace H = StorylandVcsHovercraft;
    const auto& definition = vehicle.nativeDefinition;
    const auto& wheels = definition.physicalWheels();
    if (!definition.hasNativeSuspension || wheels.size() != 4) return false;
    const float seconds = nativeStep / StorylandNativePhysicsRate;
    const float mass = vehicle.mass;
    const float turnMass = definition.turnMass * vehicle.turnMassMultiplier * (vehicle.heavy ? 5.0f : 1.0f);
    if (!(mass > 0) || !(turnMass > 0)) return false;
    const H::Frame frame{{matrix[0],matrix[1],matrix[2]},
        {matrix[4],matrix[5],matrix[6]}, {matrix[8],matrix[9],matrix[10]}};
    const H::Vector centre = H::transform(frame, definition.centerOfMass);
    const H::Vector axis = H::multiply(frame.up, -1);
    std::array<H::Vector,4> points{}, arms{};
    std::array<float,4> speeds{}, springForces{};
    auto contactActor=actor;
    updateVehicleWheelContacts(vehicle, contactActor, world, 0);
    if (!vehicle.collisionEnabled) vehicle.wheelContacts = {};
    for (size_t index = 0; index < wheels.size(); ++index) {
        const auto& point = vehicle.wheelContactPoints[index];
        points[index] = {point.x-matrix[12], point.y-matrix[13], point.z-matrix[14]};
        arms[index] = H::subtract(points[index], centre);
        if (!vehicle.wheelContacts[index]) continue;
        const auto& wheel = wheels[index];
        const auto& normal = vehicle.wheelContactNormals[index];
        const auto direction = storylandVehicleSpringDirection(
            {axis[0],axis[1],axis[2]}, normal, false);
        const float impulse = storylandNativeSpringImpulse(mass, definition.suspensionForce,
            vehicle.wheelSpringRatios[index], wheel.front ? definition.suspensionBias :
            1-definition.suspensionBias, seconds);
        vehicle.wheelSpringImpulses[index] = impulse;
        springForces[index] = impulse / StorylandNativePhysicsRate;
        const H::Vector force{direction.x*springForces[index], direction.y*springForces[index],
            direction.z*springForces[index]};
        H::moveForce(state, force, mass);
        H::turnForce(state, frame, definition.centerOfMass, force, points[index], turnMass);
    }
    const auto pointSpeed = [&](size_t index) {
        return H::dot(H::add(state.velocity, H::cross(state.angularVelocity, arms[index])), axis);
    };
    for (size_t index = 0; index < wheels.size(); ++index) speeds[index] = pointSpeed(index);
    for (size_t index = 0; index < wheels.size(); ++index) {
        if (!vehicle.wheelContacts[index]) continue;
        const auto cross = H::cross(arms[index], axis);
        const float effectiveMass = 1 / (1/mass + H::dot(cross,cross)/turnMass);
        const float impulse = storylandNativeSpringDamping(definition.suspensionDamping,
            springForces[index]*StorylandNativePhysicsRate, speeds[index]*StorylandNativePhysicsRate,
            pointSpeed(index)*StorylandNativePhysicsRate, effectiveMass, seconds);
        vehicle.wheelDampingImpulses[index] = impulse;
        const auto force = H::multiply(axis, impulse/StorylandNativePhysicsRate);
        H::moveForce(state, force, mass);
        H::turnForce(state, frame, definition.centerOfMass, force, points[index], turnMass);
    }
    for (size_t index = 0; index < definition.wheels.size(); ++index) {
        const float target = storylandVehicleWheelTarget(wheels[index], vehicle.wheelSpringRatios[index]);
        vehicle.wheelPositions[index] = storylandVehicleWheelFollow(vehicle.wheelPositions[index], target, seconds);
    }
    return H::finite(state.velocity) && H::finite(state.angularVelocity);
}

static bool storylandAdvanceVcsHovercraft(StorylandPlayTestRuntime& runtime,
    StorylandRuntimeVehicle& vehicle, StorylandNativeTrafficActor& actor,
    const StorylandNativeWorld& world, size_t slot, float seconds, std::string& error) {
    namespace H = StorylandVcsHovercraft;
    const auto& definition = vehicle.nativeDefinition;
    const auto fail = [&](const char* reason) {
        error = "Native VCS hovercraft " + std::to_string(definition.modelId) + ": " + reason;
        return false;
    };
    if (!runtime.missionEnvironment || !definition.vcsHovercraft() ||
        !definition.hasNativeHandling || !definition.hasNativeFlyingHandling ||
        !definition.hasNativeBuoyancyCoefficient || !definition.collision ||
        !definition.hasNativeSuspension || definition.physicalWheels().size() != 4)
        return fail("original handling, COL, suspension or mission state is unavailable");
    auto& environment = *runtime.missionEnvironment;
    const float nativeStep = environment.nativeScriptTimeStep;
    if (!std::isfinite(nativeStep) || nativeStep < 0 || !std::isfinite(seconds) || seconds < 0 ||
        !H::finite(vehicle.vcsPlaneControlAxes) || std::fabs(vehicle.vcsPlaneControlAxes[0]) > 1.5f ||
        std::fabs(vehicle.vcsPlaneControlAxes[1]) > 1 || std::fabs(vehicle.vcsPlaneControlAxes[2]) > 1)
        return fail("invalid physical time step or retained control axes");
    if (nativeStep == 0 || vehicle.positionFrozen) return true;
    const float physicalSeconds = nativeStep / StorylandNativePhysicsRate;
    const float turnMass = definition.turnMass * vehicle.turnMassMultiplier * (vehicle.heavy ? 5.0f : 1.0f);
    const auto previous = actor;
    const float previousSpeed = vehicle.speed;
    const auto matrix = storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
        {actor.position.x,actor.position.y,actor.position.z}, actor.yaw, definition.previewMinimumZ,
        vehicle.suspension, vehicle.bodyRoll-vehicle.roadRoll, vehicle.bodyPitch+vehicle.roadPitch),
        definition.previewRoot);
    if (!std::all_of(matrix.begin(), matrix.end(), [](float value) { return std::isfinite(value); }))
        return fail("nonfinite native entity frame");
    const H::Frame frame{{matrix[0],matrix[1],matrix[2]},
        {matrix[4],matrix[5],matrix[6]}, {matrix[8],matrix[9],matrix[10]}};
    const float c = std::cos(actor.yaw*radians), s = std::sin(actor.yaw*radians);
    const auto angular = vehicleAngularVelocity(vehicle, actor.yaw);
    H::State state{{(c*vehicle.speed-s*vehicle.lateralSpeed)/StorylandNativePhysicsRate,
        (s*vehicle.speed+c*vehicle.lateralSpeed)/StorylandNativePhysicsRate,
        vehicleVerticalVelocity(vehicle)/StorylandNativePhysicsRate},
        {angular.x/StorylandNativePhysicsRate,angular.y/StorylandNativePhysicsRate,
            angular.z/StorylandNativePhysicsRate}};
    const bool playerControls = storylandVcsHovercraftPlayerControls(runtime, vehicle, slot);
    H::ControlResult input;
    input.flight.yaw = vehicle.vcsPlaneControlAxes[0];
    input.flight.pitch = vehicle.vcsPlaneControlAxes[1];
    input.flight.roll = vehicle.vcsPlaneControlAxes[2];
    if (playerControls) {
        H::Pad pad;
        if (runtime.playerControl && runtime.health > 0) {
            pad.horizontal = int16_t(storylandVcsPadState(environment,2));
            pad.vertical = int16_t(storylandVcsPadState(environment,3));
            pad.accelerator = uint8_t(std::clamp(storylandVcsPadState(environment,22),0,255));
            pad.brake = uint8_t(std::clamp(storylandVcsPadState(environment,1),0,255));
            pad.handbrake = storylandVcsPadState(environment,16) != 0;
        }
        input = H::controls(pad, frame, state.velocity, definition.steeringLock,
            nativeStep, vehicle.vcsPlaneControlAxes);
        vehicle.vcsPlaneControlAxes = {input.flight.yaw,input.flight.pitch,input.flight.roll};
    }
    state.velocity[2] -= runtime.gravity * physicalSeconds / StorylandNativePhysicsRate;
    H::WaterResult water;
    const auto sampleWater = [&](float x, float y, float referenceZ) {
        H::WaterSample sample;
        sample.available = storylandNativeWaterHeight(world,x,y,sample.height) &&
            storylandVcsWaterDepthAllowsContact(referenceZ,sample.height);
        return sample;
    };
    const auto& collision = *definition.collision;
    if (!H::waterForces({collision.minimum.x,collision.minimum.y,collision.minimum.z},
        {collision.maximum.x,collision.maximum.y,collision.maximum.z}, frame,
        {matrix[12],matrix[13],matrix[14]}, state.velocity, vehicle.mass,
        definition.nativeBuoyancyCoefficient, nativeStep, sampleWater, water))
        return fail("invalid native buoyancy samples or force");
    int driverObjective = -1;
    int32_t driverHandle = -1;
    if(int32_t(slot)==runtime.occupiedVehicle&&runtime.playerVehicleSeat<0)
        driverObjective=runtime.vehicleAction==StorylandVehicleAction::Exit||
            environment.playerExitVehicleRequested?16:0;
    const auto scripted = environment.missionVehicles.find(vehicle.scriptHandle);
    if (driverObjective < 0 && scripted != environment.missionVehicles.end()) {
        const auto driver = environment.missionCharacters.find(scripted->second.driver);
        if (driver != environment.missionCharacters.end() && driver->second.alive &&
            !driver->second.attached && driver->second.vehicle == vehicle.scriptHandle) {
            driverObjective = environment.vcsCharacterExitingVehicle(driver->first)?16:0;
            driverHandle = driver->first;
        }
    }
    H::WaterResponse response;
    // The host exposes normal gravity, not native CPhysical alternate-gravity
    // bit32. Keep the original normal denominator even when host gravity varies.
    if (!H::waterResponse(water, vehicle.mass, nativeStep, frame.up[2], false, driverObjective, response))
        return fail("invalid native water damping or driver response");
    state.velocity = H::multiply(state.velocity, response.retention);
    state.angularVelocity = H::multiply(state.angularVelocity, response.retention);
    if (water.hasBuoyancy) {
        H::moveForce(state, water.force, vehicle.mass);
        H::turnForce(state, frame, definition.centerOfMass, water.force, water.point, turnMass);
    }
    vehicle.water.buoyant = response.contact;
    vehicle.water.engineFlooded = response.drowning;
    vehicle.water.immersion = std::clamp(water.volume,0.0f,1.0f);
    if(int32_t(slot)==runtime.occupiedVehicle)
        storylandStepWaterCharacter(runtime.playerWater,0,0,false,true,false,false,physicalSeconds,true);
    if (response.damageDriver) {
        if (driverHandle < 0) runtime.inflictDamage(response.driverDamage,0,46,false);
        else {
            const auto& driver = environment.missionCharacters.at(driverHandle);
            const StorylandMissionCharacterDamage damage{
                driverHandle,driver.model,response.driverDamage,false,0,46};
            storylandApplyMissionCharacterDamage(runtime,damage,false);
        }
    }
    if (!storylandVcsHovercraftSprings(vehicle,actor,world,matrix,nativeStep,state))
        return fail("invalid native suspension response");

    const auto publishVelocity = [&]() {
        vehicle.speed = (c*state.velocity[0]+s*state.velocity[1])*StorylandNativePhysicsRate;
        vehicle.lateralSpeed = (c*state.velocity[1]-s*state.velocity[0])*StorylandNativePhysicsRate;
        vehicle.verticalSpeed = state.velocity[2]*StorylandNativePhysicsRate;
        vehicle.suspensionVelocity = vehicle.verticalSpeed;
        for (size_t axis = 0; axis < 3; ++axis)
            vehicle.angularVelocity[axis] = state.angularVelocity[axis]*StorylandNativePhysicsRate;
        vehicle.springRollVelocity = 0;
        vehicle.springPitchVelocity = 0;
        vehicle.yawVelocity = 0;
    };
    publishVelocity();
    vehicle.grounded = std::any_of(vehicle.wheelContacts.begin(),vehicle.wheelContacts.end(),
        [](bool contact) { return contact; });
    vehicle.steeringAngle = input.steering/radians;
    vehicle.handbrake = false;
    vehicle.braking = input.brake > .01f;
    vehicle.throttle = input.roadThrottle;
    const float brake = storylandRetailVehicleBrakeDelta(definition,input.brake,physicalSeconds);
    const float rain = std::clamp(environment.weatherState.rain,0.0f,1.0f);
    applyVehicleTyreForces(vehicle,actor,0,brake,rain,physicalSeconds,false,playerControls);
    const auto tyreAngular = vehicleAngularVelocity(vehicle,actor.yaw);
    state.velocity = {(c*vehicle.speed-s*vehicle.lateralSpeed)/StorylandNativePhysicsRate,
        (s*vehicle.speed+c*vehicle.lateralSpeed)/StorylandNativePhysicsRate,
        vehicleVerticalVelocity(vehicle)/StorylandNativePhysicsRate};
    state.angularVelocity = {tyreAngular.x/StorylandNativePhysicsRate,
        tyreAngular.y/StorylandNativePhysicsRate,tyreAngular.z/StorylandNativePhysicsRate};
    const bool missionCreated = scripted != environment.missionVehicles.end() && scripted->second.persistent;
    const bool nativeFlightStatus = playerControls || (scripted != environment.missionVehicles.end() &&
        (scripted->second.scriptStatus == 0 || scripted->second.scriptStatus == 3 ||
            scripted->second.scriptStatus == 12));
    uint32_t stallIncrement = 0;
    const bool powered = nativeFlightStatus && vehicle.engineOn && vehicle.health > 0 &&
        !vehicle.wrecked && !response.drowning;
    if (!H::flyingForces(definition.flyingHandling,frame,definition.centerOfMass,
        vehicle.mass,turnMass,{runtime.windVelocity.x,runtime.windVelocity.y,runtime.windVelocity.z},
        missionCreated,matrix[14],collision.minimum.y,response.contact,input.flight,
        powered ? nativeStep : 0,state,stallIncrement))
        return fail("invalid native mode-3 force result");
    publishVelocity();
    storylandStepVehicleScriptPhysics(vehicle,actor,physicalSeconds);
    actor.position.x += state.velocity[0]*nativeStep;
    actor.position.y += state.velocity[1]*nativeStep;
    actor.position.z += state.velocity[2]*nativeStep;
    if (vehicle.collisionEnabled) {
        const auto movement = storylandMoveNativeBody(world,previous.position,actor.position,
            vehicleBody(vehicle,actor.yaw));
        resolveVehicleBodyMovement(vehicle,actor,previous.position,movement);
        if (movement.contact.valid && movement.contact.normal.z > .35f &&
            vehicleVerticalVelocity(vehicle) <= 0) vehicle.grounded = true;
    }
    vehicle.verticalSpeed = vehicleVerticalVelocity(vehicle);
    vehicle.suspensionVelocity = vehicle.verticalSpeed;
    vehicle.longitudinalAcceleration = (vehicle.speed-previousSpeed)/physicalSeconds;
    updateVehicleWheelContacts(vehicle,actor,world,0);
    return true;
}
