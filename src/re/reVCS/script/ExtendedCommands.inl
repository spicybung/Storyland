int executeVcsRestoredQueryCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (!vm.vcs || (command != 0x040B && command != 0x046E && command != 0x04F9 && command != 0x0502)) return 0;
    if (!vm.environment) {
        error = "Restored native query requires the live world.";
        return -1;
    }
    Operand handle;
    if (!readOperand(vm, handle)) {
        error = "Restored native query has a truncated handle.";
        return -1;
    }
    auto& world = *vm.environment;
    if (command == 0x040B) {
        Operand frozen;
        if (!readOperand(vm, frozen)) {
            error = "Follower freeze has a truncated flag.";
            return -1;
        }
        if (handle.bits != 0) {
            if (!world.missionCharacters.count(handle.bits)) {
                error = "Follower freeze references an unknown character.";
                return -1;
            }
            return 1;
        }
        // 2D57E0 applies the same physical freeze as02F8 to the player's
        // linked followers and native objective15 (00EF follow-formation).
        for (auto& entry : world.missionCharacters) {
            auto& character = entry.second;
            if (character.removeFromWorld) continue;
            if (character.vehicle >= 0) {
                const auto vehicle = world.missionVehicles.find(character.vehicle);
                if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) continue;
            }
            if (character.leader == 0 ||
                (character.objective == 0x00EF && character.objectiveTarget == 0))
                character.positionFrozen = frozen.bits != 0;
        }
        return 1;
    }
    if (command == 0x046E) {
        const auto vehicle = world.missionVehicles.find(handle.bits);
        if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
            error = "Photograph query references an unknown vehicle.";
            return -1;
        }
        updateScmCondition(vm, world.photographedVehicles.erase(handle.bits) != 0, inverted);
        return 1;
    }
    const bool exists = handle.bits == 0 || world.missionCharacters.count(handle.bits) != 0;
    if (command == 0x04F9) {
        if (!exists) {
            updateScmCondition(vm, false, inverted);
            return 1;
        }
        updateScmCondition(vm, world.characterReceivingPairedAction &&
            world.characterReceivingPairedAction(handle.bits), inverted);
        return 1;
    }
    if (!exists) {
        error = "Gang-protection reset references an unknown character.";
        return -1;
    }
    // 41DE90 clears CPed+79C without changing neighbouring flags or personality.
    world.characterFlags[handle.bits] = 0;
    return 1;
}

int executeVcsGarageOpcode(StorylandMissionVm& vm, uint16_t command, bool, std::string& error) {
    if (!vm.vcs || command != 0x040A) return 0;
    if (!vm.environment) {
        error = "Garage vehicle flag requires the live world.";
        return -1;
    }
    Operand handle, enabled;
    if (!readOperand(vm, handle) || !readOperand(vm, enabled)) {
        error = "Garage vehicle flag has truncated operands.";
        return -1;
    }
    auto& world = *vm.environment;
    const auto vehicle = world.missionVehicles.find(handle.bits);
    if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
        error = "Garage vehicle flag references an unknown vehicle.";
        return -1;
    }
    if (enabled.bits != 0) world.vcsGarageStorableMissionVehicles.insert(handle.bits);
    else world.vcsGarageStorableMissionVehicles.erase(handle.bits);
    return 1;
}

int executeVcsCameraFovOpcode(StorylandMissionVm& vm, uint16_t command, bool, std::string& error) {
    if (!vm.vcs || command != 0x0545) return 0;
    if (!vm.environment) {
        error = "VCS camera field of view requires the live world.";
        return -1;
    }
    Operand value;
    if (!readOperand(vm, value)) {
        error = "VCS camera field of view has a truncated operand.";
        return -1;
    }
    vm.environment->camera.vcsFieldOfViewOverride = operandFloat(value);
    return 1;
}

int executeVcsMeleeImpactOpcode(StorylandMissionVm& vm, uint16_t command, bool, std::string& error) {
    if (!vm.vcs || command != 0x0508) return 0;
    if (!vm.environment) {
        error = "VCS melee impact flag requires the live world.";
        return -1;
    }
    Operand handle, enabled;
    if (!readOperand(vm, handle) || !readOperand(vm, enabled)) {
        error = "VCS melee impact flag has truncated operands.";
        return -1;
    }
    auto& world = *vm.environment;
    if (handle.bits == 0) world.playerVcsScriptFlags.ignoreMeleeImpacts = enabled.bits != 0;
    else {
        const auto character = world.missionCharacters.find(handle.bits);
        if (character == world.missionCharacters.end()) {
            error = "VCS melee impact flag references an unknown character.";
            return -1;
        }
        character->second.vcsScriptFlags.ignoreMeleeImpacts = enabled.bits != 0;
    }
    return 1;
}

int executeVcs039aOpcode(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    if(!vm.vcs||command!=0x039A)return 0;
    if(!vm.environment) {error="Vehicle rocking requires the live world.";return -1;}
    Operand vehicle,character;
    if(!readOperand(vm,vehicle)||!readOperand(vm,character)) {
        error="Vehicle rocking has truncated operands.";return -1;
    }
    auto& world=*vm.environment;
    const auto car=world.missionVehicles.find(vehicle.bits);
    if(car==world.missionVehicles.end()||car->second.deleted||
        (character.bits!=0&&!world.missionCharacters.count(character.bits))) {
        error="Vehicle rocking references an unknown entity.";return -1;
    }
    if(!world.vcsCharacterVehicleTurnForce) {
        error="Vehicle rocking requires the live vehicle physics binding.";return -1;
    }
    if(!world.vcsCharacterVehicleTurnForce(vehicle.bits,character.bits,world.nextNativeRandom())) {
        error="Vehicle rocking could not resolve the physical state.";return -1;
    }
    if(world.audioSubsystemEnabled&&world.scriptSounds.size()<4096) {
        std::array<float,16> matrix{};
        std::array<float,3> position{car->second.x,car->second.y,car->second.z};
        if(world.vehicleWorldMatrix&&world.vehicleWorldMatrix(vehicle.bits,matrix))
            position={matrix[12],matrix[13],matrix[14]};
        world.scriptSounds.push_back({0x17,position,false,true});
    }
    return 1;
}

int executeVcsFollowerAnimationOpcode(StorylandMissionVm& vm, uint16_t command, bool,
    std::string& error) {
    if (!vm.vcs || command != 0x03EF) return 0;
    if (!vm.environment) {
        error = "VCS follower animation flag requires the live world.";
        return -1;
    }
    Operand handle;
    if (!readOperand(vm, handle)) {
        error = "VCS follower animation flag has a truncated operand.";
        return -1;
    }
    auto& world = *vm.environment;
    if (handle.bits == 0) world.playerVcsScriptFlags.disablePlayerLeaderAnimationGroup = true;
    else {
        const auto character = world.missionCharacters.find(handle.bits);
        if (character == world.missionCharacters.end()) {
            error = "VCS follower animation flag references an unknown character.";
            return -1;
        }
        character->second.vcsScriptFlags.disablePlayerLeaderAnimationGroup = true;
    }
    return 1;
}

int executeVcsVehicleSeaMineOpcode(StorylandMissionVm& vm, uint16_t command, bool, std::string& error) {
    if(!vm.vcs||command!=0x043A)return 0;
    if(!vm.environment) {
        error="VCS vehicle sea-mine flag requires the live world.";
        return -1;
    }
    Operand handle,enabled;
    if(!readOperand(vm,handle)||!readOperand(vm,enabled)) {
        error="VCS vehicle sea-mine flag has truncated operands.";
        return -1;
    }
    auto& world=*vm.environment;
    const auto vehicle=world.missionVehicles.find(handle.bits);
    if(vehicle==world.missionVehicles.end()||vehicle->second.deleted) {
        error="VCS vehicle sea-mine flag references an unknown vehicle.";
        return -1;
    }
    vehicle->second.vcsSeaMineLauncher=enabled.bits!=0;
    return 1;
}
