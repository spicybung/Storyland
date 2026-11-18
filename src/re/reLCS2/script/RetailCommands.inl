int executeLcsRetailCollectiveQuery(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (vm.vcs || command < 0x0280 || command > 0x0285) return 0;
    std::array<Operand, 6> values{};
    for (auto& value : values) {
        if (!readOperand(vm, value)) {
            error = "LCS collective coordinate query has a truncated operand.";
            return -1;
        }
    }
    if (!vm.environment) {
        error = "LCS collective coordinate query requires the live world.";
        return -1;
    }
    const float x = operandFloat(values[1]);
    const float y = operandFloat(values[2]);
    const float extentX = operandFloat(values[3]);
    const float extentY = operandFloat(values[4]);
    if (!std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(extentX) || !std::isfinite(extentY)) {
        error = "LCS collective coordinate query requires finite coordinates and extents.";
        return -1;
    }
    auto& world = *vm.environment;
    const auto members = lcsExtensionCollectiveMembers(world, values[0].bits);
    const unsigned movement = (command - 0x0280) % 3;
    const bool stopped = command >= 0x0283;
    const float minimumX = roundEeFloat(double(x) - extentX);
    const float maximumX = roundEeFloat(double(x) + extentX);
    const float minimumY = roundEeFloat(double(y) - extentY);
    const float maximumY = roundEeFloat(double(y) + extentY);
    bool allInside = true;
    for (const int32_t handle : members) {
        LcsScriptEntityPosition position;
        if (!getLcsScriptEntityPosition(world, LcsScriptEntity::Character, handle, position, true))
            continue;
        if ((stopped && !position.stopped) ||
            (movement == 1 && position.inVehicle) ||
            (movement == 2 && !position.inVehicle) ||
            position.coordinates[0] < minimumX || position.coordinates[0] > maximumX ||
            position.coordinates[1] < minimumY || position.coordinates[1] > maximumY) {
            allInside = false;
            break;
        }
    }
    if (values[5].bits) {
        StorylandMissionEnvironment::LocateRegion region;
        region.center = {x, y, world.z};
        region.extent = {extentX, extentY, 0};
        region.threeDimensional = false;
        region.expiresAt = world.gameMilliseconds + 100;
        const auto existing = std::find_if(world.locateRegions.begin(), world.locateRegions.end(),
            [&](const auto& item) {
                return item.center == region.center && item.extent == region.extent &&
                    !item.threeDimensional;
            });
        if (existing != world.locateRegions.end()) existing->expiresAt = region.expiresAt;
        else if (world.locateRegions.size() < 32) world.locateRegions.push_back(region);
    }
    updateScmCondition(vm, allInside, inverted);
    return 1;
}

int executeLcsRetailCharacterCommand(StorylandMissionVm& vm, uint16_t command,
    std::string& error) {
    if (vm.vcs || (command != 0x0639 && command != 0x065A)) return 0;
    Operand handle, value;
    if (!readOperand(vm, handle) || (command == 0x0639 && !readOperand(vm, value))) {
        error = "LCS native character flag has a truncated operand.";
        return -1;
    }
    if (!vm.environment) {
        error = "LCS native character flag requires the live world.";
        return -1;
    }
    auto& world = *vm.environment;
    const auto character = world.missionCharacters.find(handle.bits);
    if (handle.bits != 1 && character == world.missionCharacters.end()) {
        error = "LCS native character flag references an unknown character.";
        return -1;
    }
    if (command == 0x065A) {
        world.characterFlags[handle.bits] |= 0x2000u;
    } else if (character != world.missionCharacters.end()) {
        // SLUS 0x2F35C4 stores the low argument bit, rather than a nonzero test.
        character->second.behavior.canAttackPlayerWithCops = (uint32_t(value.bits) & 1u) != 0;
    }
    return 1;
}

int executeLcsRetailWorldQuery(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (vm.vcs || command != 0x0627) return 0;
    if (!vm.environment) {
        error = "LCS shortcut-taxi query requires the live world.";
        return -1;
    }
    const auto& world = *vm.environment;
    const auto vehicle = world.missionVehicles.find(world.lcsExtendedWorld.taxiShortcut.vehicle);
    const bool exists = vehicle != world.missionVehicles.end() && !vehicle->second.deleted;
    updateScmCondition(vm, exists, inverted);
    return 1;
}
