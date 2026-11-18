int32_t lcsWorldCreateCharacter(StorylandMissionEnvironment& world, int32_t model,
    const std::array<float, 3>& position, bool cop, bool swat = false) {
    if (model < 0 || world.missionCharacters.size() >= 128 || world.nextMissionCharacterHandle == INT32_MAX)
        return -1;
    int32_t handle = std::max(2, world.nextMissionCharacterHandle);
    while (handle < INT32_MAX && world.missionCharacters.count(handle)) ++handle;
    if (handle == INT32_MAX) return -1;
    StorylandMissionEnvironment::Character character;
    character.model = model; character.type = cop ? 6 : 4;
    character.x = position[0]; character.y = position[1]; character.z = position[2];
    character.creationSource = 1; character.persistent = false; character.canBeShotInVehicle = true;
    if (world.resolvePedStats && world.resolvePedStats(model, -1, character.pedStats))
        character.animationGroup = character.pedStats.animationGroup;
    if (cop) {
        const bool army = world.modelIdByName && model == world.modelIdByName("army");
        const bool fbi = world.modelIdByName && model == world.modelIdByName("fbi");
        const int32_t weapon = army || fbi ? 25 : swat ? 23 : 17;
        const auto definition = world.playerWeapons.find(weapon);
        StorylandMissionEnvironment::Character::WeaponState inventory;
        inventory.owned = true; inventory.ammunition = 1000;
        if (definition != world.playerWeapons.end()) {
            inventory.slot = definition->second.slot; inventory.model = definition->second.model;
        }
        character.weapons.emplace(weapon, inventory); character.currentWeapon = weapon;
        character.weaponsChanged = true; character.weaponSelectionChanged = true;
        character.armour = army || fbi ? 100.0f : swat ? 50.0f : 0.0f;
        character.accuracy = army ? 84 : fbi ? 76 : swat ? 68 : 60;
    }
    world.missionCharacters.emplace(handle, character); world.nextMissionCharacterHandle = handle + 1;
    world.requestedModels.insert(model);
    return handle;
}

int32_t lcsWorldCreateVehicle(StorylandMissionEnvironment& world, int32_t model,
    std::array<float, 3> position, float heading, bool persistent = false) {
    if (model < 0 || world.missionVehicles.size() >= 256 || world.nextMissionVehicleHandle <= 0 ||
        world.nextMissionVehicleHandle == INT32_MAX) return -1;
    int32_t handle = world.nextMissionVehicleHandle;
    while (handle < INT32_MAX && world.missionVehicles.count(handle)) ++handle;
    if (handle == INT32_MAX) return -1;
    if (world.groundHeightAt) {
        float height = position[2];
        if (world.groundHeightAt({position[0], position[1], 1000}, height)) position[2] = height;
    }
    StorylandMissionEnvironment::VehicleScriptState vehicle;
    vehicle.model = model; vehicle.x = position[0]; vehicle.y = position[1]; vehicle.z = position[2];
    vehicle.heading = heading; vehicle.headingChangedByScript = true; vehicle.movedByScript = true;
    vehicle.persistent = persistent; vehicle.ambientOrigin = !persistent;
    vehicle.scriptStatus = 4; vehicle.doorLock = 1;
    if (world.lcsExtendedWorld.vehicleComponentsPending && world.lcsExtendedWorld.selectVehicleComponents &&
        world.lcsExtendedWorld.selectVehicleComponents(model,
            world.lcsExtendedWorld.nextVehicleComponents, vehicle.components)) {
        world.lcsExtendedWorld.explicitVehicleComponents.insert(handle);
        world.lcsExtendedWorld.vehicleComponentsPending = false;
        world.lcsExtendedWorld.nextVehicleComponents = {-2, -2};
    }
    world.missionVehicles.emplace(handle, vehicle); world.nextMissionVehicleHandle = handle + 1;
    world.requestedModels.insert(model);
    return handle;
}

void lcsWorldClearTaxiShortcut(StorylandMissionEnvironment& world) {
    auto& shortcut = world.lcsExtendedWorld.taxiShortcut;
    const auto vehicle = world.missionVehicles.find(shortcut.vehicle);
    if (vehicle != world.missionVehicles.end()) vehicle->second.persistent = false;
    const auto driver = world.missionCharacters.find(shortcut.driver);
    if (driver != world.missionCharacters.end()) driver->second.persistent = false;
    world.radarBlips.erase(std::remove_if(world.radarBlips.begin(), world.radarBlips.end(),
        [&](const auto& blip) { return blip.handle == shortcut.blip; }), world.radarBlips.end());
    if (shortcut.state >= 3) world.control = shortcut.previousControl;
    if (world.playerVehicleEntryTarget == shortcut.vehicle) {
        world.playerVehicleEntryTarget = -1; world.playerVehicleEntryAsPassenger = false;
    }
    shortcut = {};
}

bool lcsWorldAdvanceTaxiShortcut(StorylandMissionEnvironment& world, std::string& error) {
    auto& shortcut = world.lcsExtendedWorld.taxiShortcut;
    if (!shortcut.state) return true;
    if (shortcut.state == 1) {
        if (!world.modelIdByName) { error = "Shortcut taxi requires native model metadata."; return false; }
        const int32_t model = world.modelIdByName("cabbie");
        if (model < 0) { error = "The native cabbie model is missing."; return false; }
        world.requestedModels.insert(model);
        if (!world.residentModels.count(model)) return true;
        if (world.missionVehicles.size() >= 256 || world.missionCharacters.size() >= 128) return true;
        if (world.clearAmbientArea) world.clearAmbientArea({shortcut.origin[0]-3, shortcut.origin[1]-3,
            shortcut.origin[2]-3, shortcut.origin[0]+3, shortcut.origin[1]+3, shortcut.origin[2]+3}, true);
        const int32_t vehicle = lcsWorldCreateVehicle(world, model, shortcut.origin, shortcut.originHeading, true);
        if (vehicle < 0) return true;
        const int32_t driver = lcsWorldCreateCharacter(world, 9, shortcut.origin, false);
        if (driver < 0) { world.missionVehicles.erase(vehicle); return true; }
        auto& car = world.missionVehicles.at(vehicle);
        car.driver = driver; car.autoPilotMission = 0; car.aiMission = 0; car.cruiseSpeed = 0;
        auto& ped = world.missionCharacters.at(driver);
        ped.vehicle = vehicle; ped.passengerSeat = -1; ped.persistent = true;
        shortcut.vehicle = vehicle; shortcut.driver = driver; shortcut.state = 2;
        StorylandMissionEnvironment::RadarBlip blip;
        blip.handle = storylandAllocateMissionBlipHandle(world, false);
        blip.entity = StorylandMissionEnvironment::BlipEntity::Vehicle; blip.entityHandle = vehicle;
        blip.display = 1; blip.color = 0; shortcut.blip = blip.handle;
        if (blip.handle >= 0) world.radarBlips.push_back(blip);
        return true;
    }
    const auto found = world.missionVehicles.find(shortcut.vehicle);
    const auto driver = world.missionCharacters.find(shortcut.driver);
    if (found == world.missionVehicles.end() || found->second.deleted || !found->second.alive ||
        found->second.health < 250 || found->second.wrecked || driver == world.missionCharacters.end() ||
        !driver->second.alive || driver->second.health <= 0 || driver->second.vehicle != shortcut.vehicle) {
        lcsWorldClearTaxiShortcut(world); return true;
    }
    auto& vehicle = found->second;
    if (shortcut.state == 2) {
        const float dx = vehicle.x - world.x, dy = vehicle.y - world.y, dz = vehicle.z - world.z;
        if (dx*dx + dy*dy + dz*dz > 14400) { lcsWorldClearTaxiShortcut(world); return true; }
        if (world.playerVehicleEntryTarget == shortcut.vehicle ||
            (world.inVehicle && world.occupiedMissionVehicle == shortcut.vehicle && world.playerPassengerSeat >= 0)) {
            shortcut.previousControl = world.control; world.control = false;
            world.playerVehicleEntryAsPassenger = true; shortcut.state = 3;
        }
    }
    if (shortcut.state == 3 && world.inVehicle && world.occupiedMissionVehicle == shortcut.vehicle &&
        world.playerPassengerSeat >= 0) {
        vehicle.temporaryAction = 8; vehicle.temporaryActionUntil = world.gameMilliseconds + 2500;
        world.fadeStartAlpha = world.fadeAlpha; world.fadeTargetAlpha = 1;
        world.fadeDuration = world.fadeTotal = 2500; world.fadeDirection = 0; world.fading = true;
        StorylandMissionEnvironment::LcsMessage message;
        message.key = "TAXI"; message.durationMilliseconds = 4500;
        storylandQueueLcsMessage(world, message, 2);
        shortcut.state = 4; shortcut.startedAt = world.gameMilliseconds + 3000;
    } else if (shortcut.state == 4 && world.gameMilliseconds > shortcut.startedAt) {
        if (world.requestCollision && !world.requestCollision(shortcut.destination, error)) return false;
        if (world.loadSceneCollision && !world.loadSceneCollision(shortcut.destination, error)) return false;
        if (world.loadScene && !world.loadScene(shortcut.destination, error)) return false;
        if (world.clearAmbientArea) world.clearAmbientArea({shortcut.destination[0]-3, shortcut.destination[1]-3,
            shortcut.destination[2]-3, shortcut.destination[0]+3, shortcut.destination[1]+3, shortcut.destination[2]+3}, true);
        auto point = shortcut.destination;
        if (world.groundHeightAt) world.groundHeightAt({point[0], point[1], 1000}, point[2]);
        vehicle.x = point[0]; vehicle.y = point[1]; vehicle.z = point[2];
        vehicle.heading = shortcut.destinationHeading; vehicle.headingChangedByScript = true;
        vehicle.movedByScript = true; vehicle.teleportedByScript = true;
        const float angle = vehicle.heading * .017453292519943295f;
        vehicle.velocity = {-std::sin(angle)*20, std::cos(angle)*20, 0};
        vehicle.velocityChangedByScript = true; vehicle.speed = 20;
        world.x = point[0]; world.y = point[1]; world.z = point[2]; world.playerWarped = true;
        world.fadeStartAlpha = world.fadeAlpha; world.fadeTargetAlpha = 0;
        world.fadeDuration = world.fadeTotal = 1000; world.fadeDirection = 1; world.fading = true;
        shortcut.state = 5; shortcut.startedAt = world.gameMilliseconds + 1500;
    } else if (shortcut.state == 5 && world.gameMilliseconds > shortcut.startedAt) {
        world.money = std::max(0, world.money - 9); world.playerExitVehicleRequested = true;
        shortcut.state = 6;
    } else if (shortcut.state == 6 && (!world.inVehicle || world.occupiedMissionVehicle != shortcut.vehicle)) {
        vehicle.aiMission = 1; vehicle.autoPilotMission = 1; vehicle.aiActive = true;
        vehicle.cruiseSpeed = 15; vehicle.autoPilotRoutePending = true;
        lcsWorldClearTaxiShortcut(world);
    }
    return true;
}

void lcsWorldAdvanceSwatRopes(StorylandMissionEnvironment& world) {
    const float timestep = world.nativeScriptTimeStep;
    if (!(timestep > 0)) return;
    for (auto& rope : world.lcsExtendedWorld.swatRopes) {
        if (!rope.active) continue;
        if (world.gameMilliseconds > rope.createdAt + 20000) {
            rope.velocities[0][2] -= .0015f * timestep;
            for (size_t axis = 0; axis < 3; ++axis) rope.points[0][axis] += rope.velocities[0][axis] * timestep;
        }
        const float damping = std::pow(.85f, timestep);
        for (size_t index = 1; index < rope.points.size(); ++index) {
            const auto previous = rope.points[index];
            for (size_t axis = 0; axis < 3; ++axis)
                rope.points[index][axis] += rope.velocities[index][axis] * damping * timestep;
            rope.points[index][2] -= .05f * timestep;
            std::array<float, 3> delta{}; float length = 0;
            for (size_t axis = 0; axis < 3; ++axis) {
                delta[axis] = rope.points[index][axis] - rope.points[index - 1][axis];
                length += delta[axis] * delta[axis];
            }
            length = std::sqrt(length);
            if (length > 0) for (size_t axis = 0; axis < 3; ++axis)
                rope.points[index][axis] = rope.points[index - 1][axis] + delta[axis] * (.625f / length);
            for (size_t axis = 0; axis < 3; ++axis)
                rope.velocities[index][axis] = (rope.points[index][axis] - previous[axis]) / timestep;
        }
        if (world.gameMilliseconds > rope.createdAt + 20000 && rope.points[0][2] < 0) rope.active = false;
        const auto found = world.missionCharacters.find(rope.character);
        if (found == world.missionCharacters.end() || rope.landed) continue;
        auto& ped = found->second;
        float ground = ped.z;
        const bool grounded = world.groundHeightAt && world.groundHeightAt({ped.x, ped.y, 1000}, ground);
        if (rope.active && (!grounded || ped.z - 2 >= ground)) {
            rope.progress += .003f * timestep;
            const float along = std::clamp(rope.progress, 0.0f, .999f) * 31;
            const size_t index = size_t(along); const float fraction = along - float(index);
            const auto point = [&](size_t axis) {
                return rope.points[index][axis] * (1-fraction) + rope.points[index+1][axis] * fraction;
            };
            ped.x = point(0); ped.y = point(1); ped.z = point(2);
            ped.heading = std::remainder(ped.heading + (float(rope.character & 31)*.003f-.05f) *
                timestep * 57.2957795131f, 360.0f);
        } else {
            rope.landed = true; ped.collisionEnabled = true; ped.disableVerticalMovement = false;
            ped.positionFrozen = false; ped.animationId = -1;
            if (grounded) ped.z = ground;
            if (world.wanted > 0 && !world.policeIgnorePlayer) {
                ped.objective = 11; ped.objectiveTarget = 1;
                world.characterTargets[rope.character] = {StorylandMissionEnvironment::TargetKind::Character, 0};
            }
        }
    }
}

bool lcsWorldSpawnAreaClear(const StorylandMissionEnvironment& world,
    const std::array<float, 3>& point, float radius) {
    return !world.areaOccupied || !world.areaOccupied({point[0]-radius, point[1]-radius, point[2]-radius,
        point[0]+radius, point[1]+radius, point[2]+radius}, 6);
}

void lcsWorldAdvanceRoadblocks(StorylandMissionEnvironment& world) {
    auto& blocks = world.lcsExtendedWorld.roadblocks;
    const size_t slot = world.nativeFrameCounter & 15u;
    if (slot >= blocks.size() || !blocks[slot].used || !world.modelIdByName) return;
    auto& block = blocks[slot];
    std::array<float, 3> middle{}, delta{}; float length = 0, playerDistance = 0;
    for (size_t axis = 0; axis < 3; ++axis) {
        middle[axis] = (block.start[axis] + block.end[axis]) * .5f;
        delta[axis] = block.end[axis] - block.start[axis]; length += delta[axis] * delta[axis];
        const float distance = middle[axis] - std::array<float, 3>{world.x,world.y,world.z}[axis];
        playerDistance += distance * distance;
    }
    if (playerDistance >= 10000) return;
    block.used = false; length = std::sqrt(length); if (!(length > 0)) return;
    for (float& coordinate : delta) coordinate /= length;
    const std::string name = world.wanted >= 6 ? "barracks" : world.wanted >= 5 ? "fbicar" :
        world.wanted >= 4 ? "enforcer" : "police";
    int32_t model = world.modelIdByName(name);
    if (model < 0 || !world.residentModels.count(model)) model = world.modelIdByName("police");
    float radius = 0;
    if (model < 0 || !world.nativeModelRadius || !world.nativeModelRadius(model, radius) || !(radius > 0)) return;
    const float spacing = radius * 2 + .25f;
    const int count = std::min(6, int(length / spacing));
    const int32_t copModel = world.modelIdByName(world.wanted >= 6 ? "army" : world.wanted >= 5 ?
        "fbi" : world.wanted >= 4 ? "swat" : "cop");
    for (int index = 0; index < count; ++index) {
        const float offset = spacing * float(index - count / 2);
        std::array<float, 3> point{};
        for (size_t axis = 0; axis < 3; ++axis) point[axis] = middle[axis] + offset * delta[axis];
        float heading = std::atan2(-delta[0], delta[1]) * 57.2957795131f;
        heading += (float(world.nextNativeRandom() & 255u) - 128) * .003f * 57.2957795131f;
        if (world.nextNativeRandom() & 1u)
            heading += ((float(world.nextNativeRandom() & 255u) - 128) * .003f + 3.1416f) * 57.2957795131f;
        if (!lcsWorldSpawnAreaClear(world, point, radius - .25f)) continue;
        const int32_t handle = lcsWorldCreateVehicle(world, model, point, heading);
        if (handle < 0) break;
        auto& car = world.missionVehicles.at(handle);
        car.renderAlpha = 0; car.siren = (world.nextNativeRandom() & 1u) != 0;
        car.engineOn = false; car.aiMission = 0; car.cruiseSpeed = 0;
        world.lcsExtendedWorld.fadingVehicles.insert(handle); block.vehicles.push_back(handle);
        for (int side : {-1, 1}) {
            auto foot = point;
            foot[0] += float(side)*delta[1]*2.5f; foot[1] -= float(side)*delta[0]*2.5f;
            if (world.groundHeightAt) world.groundHeightAt({foot[0],foot[1],1000}, foot[2]);
            const int32_t pedHandle = lcsWorldCreateCharacter(world, copModel, foot, true, world.wanted >= 4);
            if (pedHandle < 0) continue;
            auto& ped = world.missionCharacters.at(pedHandle);
            ped.objective = 11; ped.objectiveTarget = 1; ped.heading = std::atan2(foot[0]-world.x,world.y-foot[1])*57.2957795131f;
            world.characterTargets[pedHandle] = {StorylandMissionEnvironment::TargetKind::Character, 0};
            block.characters.push_back(pedHandle);
        }
    }
}

void lcsWorldAdvanceSetPieces(StorylandMissionEnvironment& world) {
    if (!world.modelIdByName || !world.groundHeightAt) return;
    const size_t slice = world.nativeFrameCounter & 7u;
    const size_t first = world.setPieces.size()*slice/8, last = world.setPieces.size()*(slice+1)/8;
    for (size_t index = first; index < last; ++index) {
        const auto& piece = world.setPieces[index]; const auto& point = piece.coordinates;
        const uint32_t previous = world.lcsExtendedWorld.setPieceLastTriggered[index];
        if ((previous != 0 && world.gameMilliseconds <= previous + 20000) ||
            world.x < point[0] || world.x > point[2] || world.y < point[1] || world.y > point[3]) continue;
        const bool pedestrian = piece.type == 4 || piece.type == 5;
        const bool alley = piece.type == 1;
        if (piece.type < 1 || piece.type > 7 || world.wanted < (pedestrian || alley ? 1 : 2) ||
            world.inVehicle == (pedestrian || alley)) continue;
        if (!pedestrian && !alley) {
            std::array<float, 3> velocity{};
            if (!world.vehicleWorldVelocity || !world.vehicleWorldVelocity(world.occupiedMissionVehicle, velocity) ||
                velocity[0]*(world.x-point[4]) + velocity[1]*(world.y-point[5]) >= 0) continue;
        }
        const int count = piece.type == 1 || piece.type == 5 || piece.type >= 6 ? 2 : 1;
        std::vector<int32_t> generatedCars, generatedPeds;
        bool success = true;
        for (int part = 0; part < count; ++part) {
            const size_t offset = 4 + size_t(part)*4;
            std::array<float, 3> spawn{point[offset],point[offset+1],1000};
            world.groundHeightAt(spawn, spawn[2]);
            const std::array<float,3> target{point[offset+2],point[offset+3],spawn[2]};
            const int32_t model = world.modelIdByName(pedestrian ? "cop" : "police");
            float radius = pedestrian ? .5f : 1.5f;
            if (!pedestrian && world.nativeModelRadius) world.nativeModelRadius(model, radius);
            if (!lcsWorldSpawnAreaClear(world, spawn, radius)) { success = false; break; }
            if (pedestrian) {
                const int32_t handle = lcsWorldCreateCharacter(world, model, spawn, true);
                if (handle < 0) { success = false; break; }
                auto& ped = world.missionCharacters.at(handle);
                ped.objective = 21; ped.objectiveCompleted = false; ped.walkingToTarget = true;
                ped.targetX = target[0]; ped.targetY = target[1]; ped.walkSpeed = 1.25f;
                generatedPeds.push_back(handle);
            } else {
                const float heading = std::atan2(spawn[0]-target[0],target[1]-spawn[1])*57.2957795131f;
                const int32_t handle = lcsWorldCreateVehicle(world, model, spawn, heading);
                if (handle < 0) { success = false; break; }
                auto& car = world.missionVehicles.at(handle);
                car.scriptStatus = 3; car.aiActive = true; car.autoPilotMission = -1;
                car.aiMission = alley ? 24 : (piece.type == 3 || piece.type == 7) ? 16 : 26;
                car.cruiseSpeed = (alley ? 4 : 16) * (5.0f/6.0f);
                car.drivingStyle = alley ? 1 : (piece.type == 3 || piece.type == 7) ? 2 : 3;
                car.targetX = target[0]; car.targetY = target[1]; car.targetZ = 0;
                car.drivingToTarget = alley; car.aiTargetVehicle = world.occupiedMissionVehicle;
                if (!alley) {
                    car.temporaryAction = 8; car.temporaryActionUntil = world.gameMilliseconds + 100;
                    const float angle = heading*.017453292519943295f;
                    car.velocity = {-std::sin(angle)*(100.0f/3.0f),std::cos(angle)*(100.0f/3.0f),0};
                    car.velocityChangedByScript = true;
                }
                generatedCars.push_back(handle);
                for (int seat : {-1, 0}) {
                    const int32_t cop = lcsWorldCreateCharacter(world, world.modelIdByName("cop"), spawn, true);
                    if (cop < 0) continue;
                    auto& ped = world.missionCharacters.at(cop); ped.vehicle = handle; ped.passengerSeat = seat;
                    if (seat == -1) car.driver = cop;
                    generatedPeds.push_back(cop);
                }
            }
        }
        if (success) world.lcsExtendedWorld.setPieceLastTriggered[index] = world.gameMilliseconds;
        else {
            for (int32_t car : generatedCars) world.missionVehicles.at(car).deleted = true;
            for (int32_t ped : generatedPeds) world.missionCharacters.erase(ped);
        }
    }
}

bool storylandAdvanceLcsExtendedWorld(StorylandMissionEnvironment& world, std::string& error) {
    auto& extension = world.lcsExtendedWorld;
    if (!extension.scriptPause) {
        for (auto entry = extension.fadingVehicles.begin(); entry != extension.fadingVehicles.end();) {
            const auto found = world.missionVehicles.find(*entry);
            if (found == world.missionVehicles.end() || found->second.deleted) {
                entry = extension.fadingVehicles.erase(entry); continue;
            }
            auto& vehicle = found->second;
            vehicle.renderAlpha = uint8_t(vehicle.fadeOut ? std::max(0, int(vehicle.renderAlpha)-8) :
                std::min(255, int(vehicle.renderAlpha)+16));
            if (vehicle.renderAlpha == 255 || vehicle.renderAlpha == 0) entry = extension.fadingVehicles.erase(entry);
            else ++entry;
        }
        lcsWorldAdvanceSwatRopes(world);
        lcsWorldAdvanceRoadblocks(world);
        lcsWorldAdvanceSetPieces(world);
        if (!lcsWorldAdvanceTaxiShortcut(world, error)) return false;
    }
    for (auto entry = extension.cutsceneAttachments.begin(); entry != extension.cutsceneAttachments.end();) {
        const auto object = std::find_if(world.objects.begin(), world.objects.end(),
            [&](const auto& candidate) { return candidate.handle == entry->first; });
        if (std::any_of(world.cutscene.actors.begin(),world.cutscene.actors.end(),
            [&](const auto& actor) { return actor.handle == entry->first; })) { ++entry; continue; }
        if (object == world.objects.end()) { entry = extension.cutsceneAttachments.erase(entry); continue; }
        const auto& attachment = entry->second; std::array<float, 16> matrix{};
        const bool resolved = attachment.vehicle ? world.vehicleWorldMatrix &&
            world.vehicleWorldMatrix(attachment.parent, matrix) : extension.objectAttachmentMatrix &&
            extension.objectAttachmentMatrix(attachment.parent, attachment.bone, attachment.component, matrix);
        if (resolved && !storylandApplyScriptObjectAttachment(*object, matrix, {}, {})) {
            error = "An attached cutscene object received an invalid parent transform."; return false;
        }
        ++entry;
    }
    return true;
}

int executeLcsExtendedWorldSpawnCommand(StorylandMissionVm& vm, uint16_t command, std::string& error) {
    if (vm.vcs) return 0;
    const int count = command == 0x04C5 ? 6 : command == 0x04C6 ? 0 : command == 0x04FD ? 13 :
        command == 0x0508 ? 3 : -1;
    if (count < 0) return 0;
    if (!vm.environment) { error = "LCS world creation requires the live world."; return -1; }
    auto& world = *vm.environment; std::array<Operand, 13> values{};
    for (int index = 0; index < count; ++index) {
        if (!readOperand(vm, values[size_t(index)])) { error = "LCS world creation has truncated operands."; return -1; }
        if ((index > 0 || command != 0x04FD) && !std::isfinite(operandFloat(values[size_t(index)]))) {
            error = "LCS world creation requires finite coordinates."; return -1;
        }
    }
    const auto real = [&](size_t index) { return operandFloat(values[index]); };
    if (command == 0x04C6) {
        for (auto& block : world.lcsExtendedWorld.roadblocks) block.used = false;
        return 1;
    }
    if (command == 0x04C5) {
        auto& blocks = world.lcsExtendedWorld.roadblocks;
        auto free = std::find_if(blocks.begin(), blocks.end(), [](const auto& block) { return !block.used; });
        if (free == blocks.end()) {
            if (blocks.size() >= 16) return 1;
            blocks.emplace_back(); free = blocks.end()-1;
        }
        *free = {}; free->used = true;
        free->start = {real(0),real(1),real(2)}; free->end = {real(3),real(4),real(5)};
        return 1;
    }
    if (command == 0x04FD) {
        if (world.setPieces.size() >= 96) return 1;
        StorylandMissionEnvironment::SetPiece piece; piece.type = uint8_t(values[0].bits);
        for (size_t index = 0; index < 12; ++index) piece.coordinates[index] = real(index+1);
        piece.coordinates[0] = std::min(real(1),real(3)); piece.coordinates[2] = std::max(real(1),real(3));
        piece.coordinates[1] = std::min(real(2),real(4)); piece.coordinates[3] = std::max(real(2),real(4));
        world.setPieces.push_back(piece); return 1;
    }
    if (!world.modelIdByName) { error = "SWAT rope requires native model metadata."; return -1; }
    const int32_t swat = world.modelIdByName("swat");
    if (swat < 0 || !world.residentModels.count(swat)) return 1;
    auto& ropes = world.lcsExtendedWorld.swatRopes;
    auto free = std::find_if(ropes.begin(), ropes.end(), [](const auto& rope) { return !rope.active; });
    if (free == ropes.end()) {
        if (ropes.size() >= 8) return 1;
        ropes.emplace_back(); free = ropes.end()-1;
    }
    const std::array<float, 3> anchor{real(0),real(1),real(2)};
    const int32_t handle = lcsWorldCreateCharacter(world, swat, anchor, true, true);
    if (handle < 0) return 1;
    *free = {}; free->active = true; free->character = handle; free->createdAt = world.gameMilliseconds;
    free->points[0] = anchor;
    for (size_t index = 1; index < free->points.size(); ++index) {
        free->points[index] = free->points[index-1];
        free->points[index][2] += index & 1u ? -.625f : .625f;
    }
    auto& ped = world.missionCharacters.at(handle);
    ped.collisionEnabled = false; ped.disableVerticalMovement = true; ped.positionFrozen = true;
    ped.animationGroup = 0; ped.animationId = 168; ped.animationBlend = 4;
    ped.animationStartedAt = world.gameMilliseconds; ped.animationLoop = true;
    return 1;
}

int executeLcsVerifiedPlatformCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (vm.vcs) return 0;
    switch (command) {
    case 0x0456: // 2692D0: branch to success, no operands.
    case 0x05A8: case 0x05A9: case 0x05AA: // 24DDB4, no debug operands in this executable.
    case 0x0622: case 0x0626: // 2F0E2C, no output is written or consumed.
    case 0x065B: // 2732A8, no operands.
        return 1;
    case 0x05A1: // 24F07C: native PS2 condition is false.
    case 0x0624: // 2F2D78: native PS2 condition is false.
    case 0x0625: // 2F2DE4: native PS2 condition is false.
        updateScmCondition(vm, false, inverted);
        return 1;
    case 0x05A2: { // 24F0E8 collects four parameters before returning success.
        std::array<Operand, 4> values{};
        for (auto& value : values) {
            if (!readOperand(vm, value)) {
                error = "LCS native platform command has truncated operands.";
                return -1;
            }
        }
        return 1;
    }
    default:
        return 0;
    }
}

int executeLcsExtendedPresentationCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (vm.vcs) return 0;
    const int platform = executeLcsVerifiedPlatformCommand(vm, command, inverted, error);
    if (platform) return platform;
    int count = -1;
    switch (command) {
    case 0x0162: case 0x0193: case 0x034F: case 0x0350: case 0x0351: case 0x05D4:
        count = 1; break;
    case 0x0531: case 0x0584: case 0x0654: count = 2; break;
    case 0x025A: count = 4; break;
    case 0x0192: count = 5; break;
    case 0x03B3: count = 6; break;
    case 0x0501: count = 7; break;
    case 0x043C: count = 8; break;
    case 0x04DA: count = 9; break;
    case 0x03A2: count = 12; break;
    case 0x01FE: case 0x036C: case 0x056E: case 0x0610: count = 0; break;
    default: return 0;
    }
    if (!vm.environment) {
        error = "LCS world command requires the live world.";
        return -1;
    }
    auto& world = *vm.environment;
    if (command == 0x0610) {
        updateScmCondition(vm, world.lcsExtendedWorld.scriptPause != 0 &&
            world.lcsExtendedWorld.scriptInputPause != 0, inverted);
        return 1;
    }
    if (command == 0x056E) {
        std::string bank;
        if (!readScmText(vm, bank) || bank.empty()) {
            error = "LOAD_UNCOMPRESSED_ANIM requires its eight-byte animation name.";
            return -1;
        }
        if (world.lcsExtendedWorld.uncompressedCutsceneAnimations.size() >= 7) {
            error = "The native uncompressed cutscene animation list is full."; return -1;
        }
        // StorylandAnimKey always holds expanded floating-point keyframes.
        // Keep the native load policy so its requested clip survives save/load.
        world.lcsExtendedWorld.uncompressedCutsceneAnimations.push_back(bank);
        return 1;
    }
    if (command == 0x01FE || command == 0x036C) {
        std::string label;
        std::array<Operand, 8> values{};
        if (!readScmText(vm, label)) {
            error = "START_KILL_FRENZY requires its eight-byte message key.";
            return -1;
        }
        for (auto& value : values) {
            if (!readOperand(vm, value)) {
                error = "START_KILL_FRENZY has truncated operands.";
                return -1;
            }
        }
        if (!world.startKillFrenzy) {
            error = "START_KILL_FRENZY requires the live weapons and kill counter.";
            return -1;
        }
        const std::array<int32_t, 4> models{
            values[3].bits, values[4].bits, values[5].bits, values[6].bits};
        return world.startKillFrenzy(label, values[0].bits, values[1].bits, values[2].bits,
            models, values[7].bits != 0, command == 0x036C, error) ? 1 : -1;
    }
    std::array<Operand, 12> values{};
    for (int index = 0; index < count; ++index) {
        if (!readOperand(vm, values[size_t(index)])) {
            error = "LCS world command has truncated operands.";
            return -1;
        }
    }
    const auto real = [&](size_t index) { return operandFloat(values[index]); };
    const auto position = [&](size_t index) {
        return std::array<float, 3>{real(index), real(index + 1), real(index + 2)};
    };
    const auto finite = [&](size_t begin, size_t end) {
        for (size_t index = begin; index < end; ++index) {
            if (!std::isfinite(real(index))) {
                error = "LCS world coordinates and sizes must be finite.";
                return false;
            }
        }
        return true;
    };
    const auto writable = [&](size_t index) {
        const auto& value = values[index];
        if (value.reference && value.variable < (value.global ? vm.globals.size() : vm.locals.size())) return true;
        error = "LCS world query requires a writable output.";
        return false;
    };
    const auto storeFloat = [&](size_t index, float value) {
        int32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return writeOperand(vm, values[index], bits);
    };
    if (command == 0x0162) {
        if (!finite(0, 1) || real(0) < 0) {
            error = "SET_TIME_SCALE requires a finite nonnegative scale.";
            return -1;
        }
        world.nativeTimeScale = real(0);
        return 1;
    }
    if (command == 0x05D4) {
        world.lcsExtendedWorld.scriptPause = uint8_t(values[0].bits);
        world.lcsExtendedWorld.scriptInputPause = uint8_t(values[0].bits);
        return 1;
    }
    if (command == 0x0193) {
        world.continuousSounds.erase(values[0].bits);
        return 1;
    }
    if (command == 0x0192) {
        if (!finite(0, 3) || !writable(4)) return -1;
        StorylandMissionEnvironment::ContinuousSound sound;
        sound.id = uint16_t(values[3].bits);
        sound.position = position(0);
        if (!world.continuousSoundAvailable || !world.continuousSoundAvailable(sound.id)) {
            error = "LCS continuous sound has no native sample binding: " + std::to_string(sound.id);
            return -1;
        }
        if (world.continuousSounds.size() >= 250 || world.nextContinuousSoundHandle <= 0 ||
            world.nextContinuousSoundHandle == INT32_MAX) {
            error = "LCS continuous sound pool is full.";
            return -1;
        }
        const int32_t handle = world.nextContinuousSoundHandle;
        if (!writeOperand(vm, values[4], handle)) return -1;
        world.continuousSounds.emplace(handle, sound);
        ++world.nextContinuousSoundHandle;
        return 1;
    }
    if (command >= 0x034F && command <= 0x0351) {
        static constexpr const char* keys[] = {"IndustrialPassed", "CommercialPassed", "SuburbanPassed"};
        world.statistics[keys[command - 0x034F]] = values[0].bits != 0 ? 1 : 0;
        return 1;
    }
    if (command == 0x025A) {
        if (!finite(0, 4)) return -1;
        auto point = position(0);
        if (point[2] <= -100.0f) {
            if (!world.groundHeightAt || !world.groundHeightAt(point, point[2]) || !std::isfinite(point[2])) {
                error = "Critical mission restart requires the native ground height.";
                return -1;
            }
        }
        world.nextRestartOverride = {point[0], point[1], point[2], real(3)};
        world.nextRestartOverrideActive = true;
        world.criticalMissionRestartRequested = true;
        return 1;
    }
    if (command == 0x0531) {
        if (values[0].bits != 0) {
            error = "SET_DRUNK_CAM requires player zero.";
            return -1;
        }
        world.drunkCameraIntensity = uint8_t(values[1].bits);
        world.drunkVisualIntensity = 0;
        return 1;
    }
    if (command == 0x0654) {
        if (!finite(0, 2)) return -1;
        world.weaponAimPadParameters = {real(0), real(1)};
        return 1;
    }
    if (command == 0x0584) {
        if (values[0].bits != 0 || !writable(1)) {
            error = "Coach passenger query requires player zero and a writable result.";
            return -1;
        }
        writeOperand(vm, values[1], int32_t(world.lcsExtendedWorld.busFaresCollected));
        world.lcsExtendedWorld.busFaresCollected = 0;
        return 1;
    }
    if (command == 0x0501) {
        if (values[0].bits != 0) {
            error = "GET_WHEELIE_STATS requires player zero.";
            return -1;
        }
        for (size_t index = 1; index < 7; ++index) if (!writable(index)) return -1;
        for (size_t index = 0; index < 3; ++index) {
            writeOperand(vm, values[index * 2 + 1], world.playerVehicleStunt.milliseconds[index]);
            storeFloat(index * 2 + 2, world.playerVehicleStunt.distances[index]);
        }
        world.playerVehicleStunt.milliseconds = {};
        world.playerVehicleStunt.distances = {};
        return 1;
    }
    if (command == 0x03B3) {
        if (!finite(0, 6)) return -1;
        auto& emitters = world.scriptParticleEmitters;
        emitters.erase(std::remove_if(emitters.begin(), emitters.end(), [&](const auto& emitter) {
            for (size_t axis = 0; axis < 3; ++axis) {
                if (emitter.particle.position[axis] < std::min(real(axis), real(axis + 3)) ||
                    emitter.particle.position[axis] > std::max(real(axis), real(axis + 3))) return false;
            }
            return true;
        }), emitters.end());
        return 1;
    }
    if (command == 0x04DA) {
        if (!finite(0, 4) || real(3) < 0) return -1;
        StorylandMissionEnvironment::ScriptCorona corona;
        corona.position = position(0);
        if (corona.position[2] <= -100.0f && (!world.groundHeightAt ||
            !world.groundHeightAt(corona.position, corona.position[2]))) {
            error = "Weapon-shop corona requires native ground height.";
            return -1;
        }
        corona.size = -real(3);
        corona.type = uint8_t(values[4].bits);
        corona.flareType = uint8_t(values[5].bits);
        corona.color = {uint8_t(values[6].bits), uint8_t(values[7].bits), uint8_t(values[8].bits)};
        world.frameScriptCoronas[{reinterpret_cast<uintptr_t>(&vm), int32_t(vm.pc)}] = corona;
        return 1;
    }
    if (!finite(1, 8)) return -1;
    StorylandMissionEnvironment::ScriptParticle particle;
    particle.type = command == 0x03A2 ? int32_t(uint16_t(values[0].bits)) : values[0].bits;
    particle.position = position(1);
    particle.velocity = position(4);
    particle.size = real(7);
    if (command == 0x03A2) {
        if (particle.position[2] <= -100.0f && (!world.groundHeightAt ||
            !world.groundHeightAt(particle.position, particle.position[2]))) {
            error = "Moving particle ground sentinel requires native ground height.";
            return -1;
        }
        particle.size = std::max(0.0f, particle.size);
        if (!world.scriptEmitterAvailable || !world.scriptEmitterAvailable(uint16_t(particle.type)) ||
            !particle.emitterParametersSupported()) {
            error = "LCS moving particle requires a supported native particle emitter.";
            return -1;
        }
        if (world.scriptParticleEmitters.size() >= 100) return 1;
        if (particle.type == 17) particle.color = {uint8_t(values[8].bits),
            uint8_t(values[9].bits), uint8_t(values[10].bits), 255};
        const uint32_t duration = particle.type == 6 ? 5000u : uint32_t(values[11].bits);
        world.scriptParticleEmitters.push_back({particle, world.gameMilliseconds + duration, duration != 0});
        return 1;
    }
    if (!world.createScriptParticles) {
        error = "LCS single particle requires the live particle renderer.";
        return -1;
    }
    return world.createScriptParticles({particle}, error) ? 1 : -1;
}

int executeLcsExtendedVehicleCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (vm.vcs) return 0;
    size_t count = 0;
    switch (command) {
    case 0x014E: case 0x0388: case 0x0455: case 0x04AD: case 0x04AF:
    case 0x04F5: case 0x04F6: case 0x0546: case 0x0569: case 0x0577:
    case 0x05A0: case 0x05C8: case 0x05F7: case 0x0611: case 0x0665:
        count = 1; break;
    case 0x017D: case 0x0331: case 0x0339: case 0x0498: case 0x0545:
    case 0x0599: count = 2; break;
    case 0x047C: case 0x050B: count = 3; break;
    case 0x04A7: case 0x04D7: count = 5; break;
    default: return 0;
    }
    if (!vm.environment) {
        error = "LCS vehicle command requires the live world.";
        return -1;
    }
    auto& world = *vm.environment;
    auto& extension = world.lcsExtendedWorld;
    std::array<Operand, 5> values{};
    for (size_t index = 0; index < count; ++index) {
        if (!readOperand(vm, values[index])) {
            error = "LCS vehicle command has truncated operands.";
            return -1;
        }
    }
    const int32_t handle = values[0].bits;
    const auto condition = [&](bool value) {
        updateScmCondition(vm, value, inverted);
        return 1;
    };
    if (command == 0x014E) return condition(handle == extension.lastCrushedVehicle);
    if (command == 0x0577) {
        world.taxiBoost = handle != 0;
        return 1;
    }
    if (command == 0x05C8) {
        const float height = operandFloat(values[0]);
        if (!std::isfinite(height)) {
            error = "RC helicopter height limit must be finite.";
            return -1;
        }
        world.rcHeliHeightLimit = height;
        return 1;
    }
    if (command == 0x050B) {
        // Retail18772C compares the model but never branches on the result.
        // The two signed bytes are the next constructor's global extras.
        extension.nextVehicleComponents = {int8_t(uint8_t(values[1].bits)), int8_t(uint8_t(values[2].bits))};
        extension.vehicleComponentsPending = true;
        return 1;
    }
    if (command == 0x0665) {
        if (handle == 0) return 1;
        if (handle < 0 || !world.setPlayerClipAmmunition || !world.setPlayerClipAmmunition(handle)) {
            error = "The active player weapon cannot accept that clip ammunition.";
            return -1;
        }
        return 1;
    }
    if (command == 0x0545) {
        if (handle != 0) {
            error = "Player auto aim requires player zero.";
            return -1;
        }
        extension.autoAim = values[1].bits != 0;
        if (!extension.autoAim) world.playerTarget = {};
        return 1;
    }
    if (command == 0x04AD || command == 0x04AF) {
        if (handle != 0) {
            error = "Vehicle class query requires player zero.";
            return -1;
        }
        if (!world.inVehicle || world.occupiedMissionVehicle < 0) return condition(false);
        if (!world.vehicleHandlingClass) {
            error = "Vehicle class query requires native handling data.";
            return -1;
        }
        return condition(world.vehicleHandlingClass(world.occupiedMissionVehicle) ==
            (command == 0x04AD ? 4 : 3));
    }
    if (command == 0x04F5 || command == 0x04F6) {
        bool waiting = false;
        if (!extension.collisionWaiting || !extension.collisionWaiting(
            command == 0x04F5 ? 1 : 2, handle, waiting)) {
            error = "Collision readiness requires the live entity streaming state.";
            return -1;
        }
        return condition(waiting);
    }
    if (command == 0x017D) {
        if (handle != 0) {
            error = "Player-object collision query requires player zero.";
            return -1;
        }
        const auto object = std::find_if(world.objects.begin(), world.objects.end(),
            [&](const auto& item) { return item.handle == values[1].bits; });
        if (object == world.objects.end()) {
            error = "Player-object collision query references an unknown object.";
            return -1;
        }
        if (world.inVehicle) {
            if (!extension.playerTouchesObject) {
                error = "Vehicle-object contact requires the live collision world."; return -1;
            }
            return condition(extension.playerTouchesObject(object->handle));
        }
        if (!world.objectTouchingCharacter) {
            error = "Player-object contact requires the live collision world.";
            return -1;
        }
        return condition(world.objectTouchingCharacter(*object, 0));
    }
    const auto found = world.missionVehicles.find(handle);
    if (found == world.missionVehicles.end() || found->second.deleted) {
        error = "LCS vehicle command references an unknown vehicle.";
        return -1;
    }
    auto& vehicle = found->second;
    if (command == 0x0388) return condition(vehicle.siren);
    if (command == 0x0339) {
        if (values[1].bits) extension.bigWheelVehicles.insert(handle);
        else extension.bigWheelVehicles.erase(handle);
        return 1;
    }
    if (command == 0x047C) {
        vehicle.temporaryAction = uint8_t(values[1].bits);
        vehicle.temporaryActionUntil = world.gameMilliseconds + uint32_t(values[2].bits);
        return 1;
    }
    if (command == 0x0498) {
        vehicle.contactExplosion = values[1].bits != 0;
        return 1;
    }
    if (command == 0x0546) {
        if (!world.fireHunterGun || !world.fireHunterGun(handle, error)) {
            if (error.empty()) error = "Hunter gun requires its live projectile consumer.";
            return -1;
        }
        return 1;
    }
    if (command == 0x0569) {
        if (!world.vehicleHandlingClass || world.vehicleHandlingClass(handle) != 3) {
            error = "Crashing helicopter command requires a helicopter.";
            return -1;
        }
        vehicle.heliCrashing = true;
        return 1;
    }
    if (command == 0x0599) {
        vehicle.renderAlpha = uint8_t(values[1].bits);
        extension.fadingVehicles.insert(handle);
        return 1;
    }
    if (command == 0x05A0) {
        if (!world.vehicleNativeType) {
            error = "Disarming a car bomb requires the vehicle's native class.";
            return -1;
        }
        if (world.vehicleNativeType(handle) == 0 && vehicle.bombType != 0) {
            vehicle.bombType = 0;
            vehicle.bombOwner = -1;
            vehicle.bombCountdownActive = false;
            vehicle.bombDeadline = 0;
        }
        return 1;
    }
    if (command == 0x05F7) {
        vehicle.health = 0;
        vehicle.healthChangedByScript = true;
        vehicle.alive = false;
        vehicle.wrecked = true;
        vehicle.scriptStatus = 5;
        vehicle.engineOn = false;
        vehicle.siren = false;
        vehicle.bombType = 0;
        vehicle.bombOwner = -1;
        vehicle.bombDeadline = 0;
        vehicle.bombCountdownActive = false;
        vehicle.alarmMilliseconds = 0;
        vehicle.alarmChangedByScript = true;
        vehicle.doorDamage.fill(3);
        vehicle.panelDamage.fill(3);
        vehicle.lightDamage.fill(1);
        vehicle.visiblyDamaged = true;
        vehicle.doorsChangedByScript = true;
        vehicle.extinguishRequested = true;
        for (auto& fire : world.scriptFires) {
            if (fire.second.attachedKind == 2 && fire.second.attachedHandle == handle)
                fire.second.active = false;
        }
        return 1;
    }
    if (command == 0x0611) {
        // NativeProcessOpenDoor is called at the end of rear entry animations
        //174 and170: open both rear doors to their animation-end ratios.
        vehicle.doorOpenRatios[4] = 1;
        vehicle.doorOpenRatios[5] = 1;
        vehicle.doorsChangedByScript = true;
        return 1;
    }
    if (command == 0x0331) {
        const auto target = world.missionVehicles.find(values[1].bits);
        if (target == world.missionVehicles.end() || target->second.deleted || target->first == handle) {
            error = "Vehicle ramming requires a distinct live target.";
            return -1;
        }
        vehicle.aiTargetVehicle = target->first;
        vehicle.aiMission = 15;
        vehicle.aiActive = true;
        vehicle.aiMissionStartedAt = world.gameMilliseconds;
        vehicle.cruiseSpeed = std::max(vehicle.cruiseSpeed, 5.0f);
        return 1;
    }
    if (command == 0x0455) {
        vehicle.targetX = world.x;
        vehicle.targetY = world.y;
        vehicle.targetZ = world.z;
        vehicle.drivingToTarget = true;
        vehicle.aiActive = true;
        vehicle.aiMission = 4;
        vehicle.aiMissionStartedAt = world.gameMilliseconds;
        vehicle.autoPilotRoutePending = true;
        return 1;
    }
    const bool plane = command == 0x04D7;
    if (!world.vehicleHandlingClass || world.vehicleHandlingClass(handle) != (plane ? 5 : 3) ||
        !world.vehicleFlightAvailable || !world.vehicleFlightAvailable(handle)) {
        error = "Aircraft navigation requires its native flying handling profile.";
        return -1;
    }
    for (size_t index = 1; index <= 3; ++index) {
        if (!std::isfinite(operandFloat(values[index]))) {
            error = "Aircraft destination must be finite.";
            return -1;
        }
    }
    vehicle.targetX = operandFloat(values[1]);
    vehicle.targetY = operandFloat(values[2]);
    vehicle.targetZ = operandFloat(values[3]);
    vehicle.cruiseSpeed = float(uint8_t(values[4].bits)) * (5.0f / 6.0f);
    vehicle.aiMission = plane ? 22 : 20;
    vehicle.aiActive = true;
    vehicle.scriptStatus = 3;
    vehicle.drivingToTarget = false;
    vehicle.aiMissionStartedAt = world.gameMilliseconds;
    return 1;
}

std::array<float, 3> lcsPathNodePosition(const StorylandNativePaths& paths, uint32_t index,
    bool offsetFromRoad) {
    if (index >= paths.nodes.size()) return {};
    const auto& node = paths.nodes[index];
    std::array<float, 3> result{node.position.x, node.position.y, node.position.z};
    if (!offsetFromRoad || !node.width || node.links.empty() ||
        node.links.front().target >= paths.nodes.size()) return result;
    const auto& target = paths.nodes[node.links.front().target].position;
    float dx = target.x - node.position.x, dy = target.y - node.position.y;
    const float distance = std::hypot(dx, dy);
    if (distance > 0) { dx /= distance; dy /= distance; }
    else { dx = 1; dy = 0; }
    if (dx < 0) { dx = -dx; dy = -dy; }
    const float offset = 2.5f + float(node.width) * 0.0625f;
    result[0] -= dy * offset;
    result[1] += dx * offset;
    return result;
}

int executeLcsExtendedPathQuery(StorylandMissionVm& vm, uint16_t command, std::string& error) {
    if (vm.vcs || (command != 0x02C5 && command != 0x04D8 && command != 0x04D9 && command != 0x04BE)) return 0;
    if (!vm.environment || !vm.environment->pedestrianPaths ||
        vm.environment->pedestrianPaths->lcsSearchGrid.size() != 10000) {
        error = "LCS path query requires the native 100 by 100 search grid.";
        return -1;
    }
    const bool straight = command == 0x04BE;
    const bool nth = command == 0x04D8 || command == 0x04D9;
    const bool pedestrian = command == 0x02C5 || command == 0x04D9;
    const size_t inputs = straight ? 5 : nth ? 4 : 3;
    const size_t count = inputs + (straight ? 7 : 3);
    std::array<Operand, 12> values{};
    for (size_t index = 0; index < count; ++index) {
        if (!readOperand(vm, values[index])) { error = "LCS path query has truncated operands."; return -1; }
        if (index >= inputs && (!values[index].reference || values[index].variable >=
            (values[index].global ? vm.globals.size() : vm.locals.size()))) {
            error = "LCS path query requires writable outputs."; return -1;
        }
    }
    for (size_t index = 0; index < (straight ? 5u : 3u); ++index)
        if (!std::isfinite(operandFloat(values[index]))) {
            error = "LCS path query requires finite coordinates and ranges."; return -1;
        }
    auto& world = *vm.environment;
    const auto& paths = *world.pedestrianPaths;
    const auto flags = storylandNativeRoadNodeFlags(paths, world.roadChanges);
    std::array<float, 3> point{operandFloat(values[0]), operandFloat(values[1]), operandFloat(values[2])};
    if (!straight && point[2] <= -100) {
        if (!world.groundHeightAt) { error = "LCS path ground sentinel requires native collision."; return -1; }
        float height = 20;
        if (world.groundHeightAt({point[0], point[1], 1000}, height) && std::isfinite(height)) point[2] = height;
        else point[2] = 20;
    }
    const auto cell = [](float coordinate, float radius) {
        const float sum = roundEeFloat(double(coordinate) + radius);
        const float scaled = roundEeFloat(double(sum) * 0.02500000037252903);
        return int(std::clamp(roundEeFloat(double(scaled) + 50), 0.0f, 99.0f));
    };
    const auto distance = [&](const StorylandNativePoint& node) {
        const float dx = std::fabs(roundEeFloat(double(node.x) - point[0]));
        const float dy = std::fabs(roundEeFloat(double(node.y) - point[1]));
        const float dz = std::fabs(roundEeFloat(double(node.z) - point[2]));
        return roundEeFloat(double(roundEeFloat(double(dx) + dy)) + roundEeFloat(double(dz) * 3));
    };
    const auto visit = [&](float radius, auto&& visitor) {
        const int minX = cell(point[0], -radius), maxX = cell(point[0], radius);
        const int minY = cell(point[1], -radius), maxY = cell(point[1], radius);
        for (int y = minY; y <= maxY; ++y) for (int x = minX; x <= maxX; ++x)
            for (uint16_t index : paths.lcsSearchGrid[size_t(x) * 100 + size_t(y)][pedestrian ? 1 : 0])
                if (index < paths.nodes.size() && !visitor(uint32_t(index))) return;
    };
    std::array<float, 7> result{};
    if (straight) {
        const float minimumLength = operandFloat(values[3]), radius = operandFloat(values[4]);
        if (radius < 0) { error = "Straight road search radius must be nonnegative."; return -1; }
        uint32_t selected = UINT32_MAX, linked = UINT32_MAX;
        float best = 100000000.0f;
        visit(radius, [&](uint32_t index) {
            const auto& node = paths.nodes[index];
            if (flags[index] & 0x160u) return true;
            const float proximity = distance(node.position);
            if (proximity >= best) return true;
            for (const auto& edge : node.links) {
                if (edge.target >= paths.nodes.size() || (flags[edge.target] & 0x60u)) continue;
                const auto& next = paths.nodes[edge.target].position;
                if (std::hypot(node.position.x - next.x, node.position.y - next.y,
                    node.position.z - next.z) > minimumLength) {
                    selected = index; linked = edge.target; best = proximity;
                }
            }
            return true;
        });
        if (selected != UINT32_MAX && best < radius * radius) {
            const auto first = lcsPathNodePosition(paths, selected, true);
            const auto second = lcsPathNodePosition(paths, linked, true);
            std::copy(first.begin(), first.end(), result.begin());
            std::copy(second.begin(), second.end(), result.begin() + 3);
            const auto& a = paths.nodes[selected].position;
            const auto& b = paths.nodes[linked].position;
            result[6] = std::atan2(b.x - a.x, a.y - b.y) * 57.2957763671875f;
        }
    } else {
        auto& marked = world.pathSearchMarkedNodes[pedestrian ? 1 : 0];
        if (nth) marked.clear();
        const int32_t skip = nth ? std::max(0, int32_t(uint32_t(values[3].bits) - 1u)) : 0;
        if (size_t(skip) > paths.nodes.size()) {
            if (pedestrian) { error = "LCS pedestrian path rank exceeds its native node pool."; return -1; }
        } else {
            uint32_t selected = UINT32_MAX;
            for (int32_t rank = 0; rank <= skip; ++rank) {
                selected = UINT32_MAX;
                for (unsigned pass = 0; pass < 2 && selected == UINT32_MAX; ++pass) {
                    const float radius = pass ? (nth ? 999999.875f : 800.0f) : 100.0f;
                    float best = roundEeFloat(double(radius) * radius);
                    visit(radius, [&](uint32_t index) {
                        if ((flags[index] & (nth ? 0x160u : 0x120u)) || (nth && marked.count(index))) return true;
                        const float proximity = distance(paths.nodes[index].position);
                        if (proximity < best) { selected = index; best = proximity; }
                        return proximity >= 0.010000000707805157f;
                    });
                }
                if (selected == UINT32_MAX) break;
                if (rank < skip) marked.insert(selected);
            }
            if (selected == UINT32_MAX && pedestrian) {
                error = "LCS pedestrian path query found no native node."; return -1;
            }
            const auto position = lcsPathNodePosition(paths, selected, !pedestrian);
            std::copy(position.begin(), position.end(), result.begin());
        }
    }
    for (size_t index = inputs; index < count; ++index) {
        int32_t bits = 0;
        std::memcpy(&bits, &result[index - inputs], sizeof(bits));
        if (!writeOperand(vm, values[index], bits)) return -1;
    }
    return 1;
}

int executeLcsExtendedCharacterQuery(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (vm.vcs) return 0;
    if (command != 0x05C7 && command != 0x05CD && command != 0x05E2 && command != 0x0612 && command != 0x0678) return 0;
    if (!vm.environment) { error = "LCS character policy requires the live world."; return -1; }
    std::array<Operand, 2> values{};
    const size_t count = command == 0x05E2 ? 1 : 2;
    for (size_t index = 0; index < count; ++index)
        if (!readOperand(vm, values[index])) { error = "LCS character policy has truncated operands."; return -1; }
    auto& world = *vm.environment;
    const int32_t handle = values[0].bits;
    if (command == 0x0678) {
        if (handle != 0) { error = "Freezing player followers requires player zero."; return -1; }
        for (auto& entry : world.missionCharacters) {
            auto& character = entry.second;
            if (character.leader == 0 || (character.objective == 15 && character.objectiveTarget == 1))
                character.positionFrozen = values[1].bits != 0;
        }
        return 1;
    }
    const auto found = world.missionCharacters.find(handle);
    if (command == 0x0612) {
        if (!values[1].reference) { error = "GET_CHAR_THREAT_CHAR requires a writable output."; return -1; }
        int32_t threat = 0;
        if (found != world.missionCharacters.end() || handle == 1) {
            const auto target = world.characterTargets.find(handle == 1 ? 0 : handle);
            if (target != world.characterTargets.end() && target->second.kind == StorylandMissionEnvironment::TargetKind::Character)
                threat = target->second.handle == 0 ? 1 : target->second.handle;
        }
        if (!writeOperand(vm, values[1], threat)) { error = "Invalid threat-character output."; return -1; }
        return 1;
    }
    if (handle != 1 && found == world.missionCharacters.end()) {
        error = "LCS character policy references an unknown character."; return -1;
    }
    if (command == 0x05E2) {
        updateScmCondition(vm, handle == 1 ? world.playerCrouch.active : found->second.crouch.active, inverted);
        return 1;
    }
    if (command == 0x05C7) {
        if(handle == 1)world.playerAllowFrontPassenger = values[1].bits == 0;
        else found->second.allowFrontPassenger = values[1].bits == 0;
    }
    else if (values[1].bits) world.lcsExtendedWorld.frontSeatOnlyCharacters.insert(handle);
    else world.lcsExtendedWorld.frontSeatOnlyCharacters.erase(handle);
    return 1;
}

int executeLcsExtendedSceneCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (vm.vcs) return 0;
    size_t count = 0;
    switch (command) {
    case 0x0444: case 0x0445: case 0x0527: case 0x055C: case 0x0580: break;
    case 0x050C: case 0x0557: case 0x0581: case 0x0582: case 0x0583:
    case 0x059B: case 0x0614: case 0x0634: count = 1; break;
    case 0x052A: case 0x0550: count = 2; break;
    case 0x03AF: case 0x0528: case 0x0529: case 0x059D: count = 3; break;
    case 0x0211: case 0x056A: case 0x0661: count = 4; break;
    case 0x058F: case 0x061B: count = 6; break;
    case 0x055B: count = 8; break;
    default: return 0;
    }
    if (!vm.environment) { error = "LCS scene command requires the live world."; return -1; }
    auto& world = *vm.environment;
    auto& extension = world.lcsExtendedWorld;
    std::array<Operand, 8> values{};
    for (size_t index = 0; index < count; ++index)
        if (!readOperand(vm, values[index])) { error = "LCS scene command has truncated operands."; return -1; }
    const auto real = [&](size_t index) { return operandFloat(values[index]); };
    const auto position = [&](size_t index) { return std::array<float, 3>{real(index), real(index + 1), real(index + 2)}; };
    const auto finite = [&](size_t first, size_t last) {
        for (size_t index = first; index < last; ++index) if (!std::isfinite(real(index))) {
            error = "LCS scene coordinates and sizes must be finite."; return false;
        }
        return true;
    };
    const auto condition = [&](bool result) { updateScmCondition(vm, result, inverted); return 1; };
    if (command == 0x050C) { extension.liftCamera = values[0].bits != 0; return 1; }
    if (command == 0x0527) { extension.cutsceneShadows = false; return 1; }
    if (command == 0x0557) { extension.riotIntensity = uint8_t(values[0].bits); return 1; }
    if (command == 0x0581) { extension.hurricanesAllowed = values[0].bits != 0; return 1; }
    if (command == 0x0583) { extension.playerInStadium = values[0].bits != 0; return 1; }
    if (command == 0x0634) { extension.crusherRewardMultiplier = uint8_t(values[0].bits); return 1; }
    if (command == 0x0580) {
        if (!extension.wantedStarsFlashing) { error = "Wanted-star query requires live wanted suspension state."; return -1; }
        return condition(extension.wantedStarsFlashing());
    }
    if (command == 0x0444 || command == 0x0445 || command == 0x0582) {
        const bool playing = command != 0x0445;
        const uint8_t track = command == 0x0582 ? uint8_t(uint8_t(values[0].bits) + 0x19) : uint8_t(0x3F);
        if (!extension.playMusic || !extension.playMusic(track, playing, command == 0x0444, error)) {
            if (error.empty()) error = "LCS music command requires its native streamed track.";
            return -1;
        }
        extension.musicTrack = track; extension.musicPlaying = playing;
        extension.musicLooped = command == 0x0444; ++extension.musicSerial;
        return 1;
    }
    if (command == 0x03AF) {
        if (!finite(0, 3)) return -1;
        if (!extension.policeRadioMessage || !extension.policeRadioMessage(position(0), error)) {
            if (error.empty()) error = "Police radio requires its native location and speech consumer.";
            return -1;
        }
        extension.policeRadioPosition = position(0); ++extension.policeRadioSerial;
        return 1;
    }
    if (command == 0x0211 || command == 0x056A) {
        if (!finite(0, 3)) return -1;
        auto point = position(0);
        if (point[2] <= -100) {
            if (!world.groundHeightAt) { error = "Explosion ground sentinel requires native collision."; return -1; }
            float height = 20;
            if (!world.groundHeightAt(point, height) || !std::isfinite(height)) height = 20;
            point[2] = height;
        }
        const bool suppressed = extension.suppressExplosionSound;
        extension.suppressExplosionSound = command == 0x056A;
        const bool result = storylandAddScriptExplosion(world, values[3].bits, point, error);
        extension.suppressExplosionSound = suppressed;
        return result ? 1 : -1;
    }
    if (command == 0x0528) {
        if (!finite(0, 3)) return -1;
        bool shattered = false;
        if (!extension.nearestGlassShattered || !extension.nearestGlassShattered(position(0), shattered)) {
            error = "Glass query requires the native object and dummy pools."; return -1;
        }
        return condition(shattered);
    }
    if (command == 0x055B) {
        if (!finite(0, 8)) return -1;
        lcsWorldClearTaxiShortcut(world);
        auto& shortcut = extension.taxiShortcut;
        shortcut.origin = position(0); shortcut.originHeading = real(3);
        shortcut.destination = position(4); shortcut.destinationHeading = real(7);
        shortcut.state = 1; shortcut.startedAt = world.gameMilliseconds;
        return 1;
    }
    if (command == 0x055C) {
        lcsWorldClearTaxiShortcut(world);
        return 1;
    }
    if (command == 0x059B) {
        if (values[0].bits != 0) { error = "Shortcut taxi query requires player zero."; return -1; }
        return condition(world.inVehicle && extension.taxiShortcut.vehicle >= 0 &&
            world.occupiedMissionVehicle == extension.taxiShortcut.vehicle);
    }
    if (command == 0x061B) {
        if (!finite(0, 6)) return -1;
        const std::array<float, 6> bounds{real(0), real(1), real(2), real(3), real(4), real(5)};
        if (!extension.clearNativeObjects || !extension.clearNativeObjects(bounds, error)) {
            if (error.empty()) error = "Clearing objects requires the live native object and dummy pools.";
            return -1;
        }
        std::vector<int32_t> removed;
        for (const auto& object : world.objects)
            if (object.creationSource != 2 && !object.persistent &&
                object.x >= bounds[0] && object.x <= bounds[3] && object.y >= bounds[1] &&
                object.y <= bounds[4] && object.z >= bounds[2] && object.z <= bounds[5]) removed.push_back(object.handle);
        for (int32_t handle : removed) storylandDeleteScriptObject(world, handle);
        return 1;
    }
    if (command == 0x058F) {
        if (!finite(0, 6) || !world.createScriptParticles) {
            error = "Big-gun flash requires finite endpoints and the live particle renderer."; return -1;
        }
        const auto start = position(0), end = position(3);
        float dx = end[0] - start[0], dy = end[1] - start[1];
        const float length = std::hypot(dx, dy);
        if (length > 0) { dx /= length; dy /= length; } else { dx = 1; dy = 0; }
        std::vector<StorylandMissionEnvironment::ScriptParticle> particles;
        const auto flash = [&](float forward, float side, float vertical, float size) {
            StorylandMissionEnvironment::ScriptParticle particle;
            particle.type = 16; particle.size = size;
            particle.position = {start[0] + dx * forward + dy * side,
                start[1] + dy * forward - dx * side, start[2] + vertical};
            particles.push_back(particle);
        };
        flash(0, 0, 0, .08f); flash(.06f, 0, 0, .06f); flash(.12f, 0, 0, .06f);
        for (float sign : {-1.0f, 1.0f}) {
            flash(-.1f, 0, sign * .04f, .04f);
            flash(-.1f, 0, sign * .08f, .02f);
            flash(-.1f, 0, sign * .11f, .02f);
            flash(-.1f, sign * .06f, 0, .04f);
            flash(-.1f, sign * .10f, 0, .03f);
            flash(-.1f, sign * .13f, 0, .02f);
        }
        StorylandMissionEnvironment::ScriptParticle smoke;
        smoke.type = 18; smoke.position = start;
        const float speed = .05f + .20f * float(world.nextNativeRandom() & 0xFFFFu) / 65535.0f;
        smoke.velocity = {dx * speed, dy * speed, 0}; particles.push_back(smoke);
        return world.createScriptParticles(particles, error) ? 1 : -1;
    }
    const auto cutsceneActor = std::find_if(world.cutscene.actors.begin(), world.cutscene.actors.end(),
        [&](const auto& actor) { return actor.handle == values[0].bits; });
    if (cutsceneActor != world.cutscene.actors.end() &&
        (command == 0x0529 || command == 0x052A || command == 0x0550)) {
        StorylandLcsExtendedWorld::CutsceneAttachment attachment;
        attachment.object = values[0].bits; attachment.parent = values[1].bits;
        attachment.vehicle = command == 0x0550;
        if (command == 0x0529) attachment.bone = values[2].bits;
        if (command == 0x052A && !readScmText(vm, attachment.component)) {
            error = "Cutscene frame attachment requires its eight-byte frame name."; return -1;
        }
        const bool parentExists = attachment.vehicle ? world.missionVehicles.count(attachment.parent) != 0 :
            std::any_of(world.cutscene.actors.begin(),world.cutscene.actors.end(),
                [&](const auto& actor) { return actor.handle == attachment.parent; });
        if (!parentExists || (!attachment.vehicle && attachment.parent == attachment.object)) {
            error = "Cutscene attachment references an invalid native parent."; return -1;
        }
        extension.cutsceneAttachments[attachment.object] = std::move(attachment);
        return 1;
    }
    const auto foundObject = std::find_if(world.objects.begin(), world.objects.end(),
        [&](const auto& value) { return value.handle == values[0].bits; });
    StorylandMissionEnvironment::Object cutsceneObject;
    auto* object = foundObject != world.objects.end() ? &*foundObject : nullptr;
    if (command == 0x059D && cutsceneActor != world.cutscene.actors.end()) {
        std::array<float,16> matrix{};
        if (!extension.objectAttachmentMatrix ||
            !extension.objectAttachmentMatrix(cutsceneActor->handle,-1,{},matrix)) {
            error = "Cutscene helicopter dust requires the sampled native actor matrix."; return -1;
        }
        cutsceneObject.x=matrix[12];cutsceneObject.y=matrix[13];cutsceneObject.z=matrix[14];
        object=&cutsceneObject;
    }
    if (!object) { error = "LCS scene command references an unknown object."; return -1; }
    if (command == 0x0614) { storylandHydrateScriptObject(world, *object); object->damage.renderDamaged = true; return 1; }
    if (command == 0x0661) {
        if (!finite(1, 4)) return -1;
        object->rotationX = real(2); object->rotationY = real(1); object->heading = real(3);
        return 1;
    }
    if (command == 0x059D) {
        if (!finite(1, 2) || !world.groundHeightAt || !world.createScriptParticles) {
            error = "Cutscene helicopter dust requires ground collision and particle rendering."; return -1;
        }
        float ground = 0, water = 0;
        const std::array<float, 3> origin{object->x, object->y, object->z};
        world.groundHeightAt(origin, ground);
        const bool onWater = world.waterHeightAt && world.waterHeightAt(origin, water) && water > ground;
        if (onWater) ground = water;
        const float radius = (origin[2] - ground - 1.0f - real(1)) * .3f + real(1);
        const int count = std::clamp(values[2].bits, 0, 8);
        std::vector<StorylandMissionEnvironment::ScriptParticle> particles;
        const int first = int(world.nativeFrameCounter & 7u) * 4;
        for (int index = 0; index < count; ++index) {
            const float angle = float(first + index) * .19634954084936207f;
            StorylandMissionEnvironment::ScriptParticle particle;
            const float dx = radius * std::cos(angle), dy = radius * std::sin(angle);
            particle.position = {origin[0] + dx, origin[1] + dy, ground};
            particle.velocity = {dx * .01f, dy * .01f, .01f};
            if (onWater) {
                particle.type = (count - index) & 1 ? 31 : 44;
                particle.size = radius * (particle.type == 31 ? .25f : .3f);
                particle.color = {255, 255, 255, 32};
            } else {
                if (origin[2] - ground >= 20) continue;
                particle.type = 53; particle.color = {10, 10, 10, 32};
            }
            particles.push_back(particle);
        }
        return particles.empty() || world.createScriptParticles(particles, error) ? 1 : -1;
    }
    StorylandLcsExtendedWorld::CutsceneAttachment attachment;
    attachment.object = object->handle; attachment.parent = values[1].bits;
    attachment.vehicle = command == 0x0550;
    if (command == 0x0529) attachment.bone = values[2].bits;
    if (command == 0x052A && !readScmText(vm, attachment.component)) {
        error = "Cutscene component attachment requires an eight-byte frame name."; return -1;
    }
    std::array<float, 16> matrix{};
    const bool resolved = attachment.vehicle ?
        world.vehicleWorldMatrix && world.vehicleWorldMatrix(attachment.parent, matrix) :
        extension.objectAttachmentMatrix && extension.objectAttachmentMatrix(attachment.parent,
            attachment.bone, attachment.component, matrix);
    if (!resolved || !storylandApplyScriptObjectAttachment(*object, matrix, {}, {})) {
        error = "Cutscene attachment has no native parent bone, frame or vehicle matrix."; return -1;
    }
    extension.cutsceneAttachments[object->handle] = std::move(attachment);
    object->dynamic = false; object->gravity = false;
    return 1;
}
