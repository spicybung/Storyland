int executeExtendedMissionCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    static constexpr std::array<uint16_t,14> vcsRoutes{
        0x0164, 0x01C5, 0x0319, 0x0357, 0x0364, 0x0399, 0x0442, 0x048A,
        0x04B6, 0x04B7, 0x04C2, 0x0542, 0x0543, 0x0557};
    static constexpr std::array<uint16_t,44> lcsRoutes{
        0x0386, 0x0387, 0x03E4, 0x03EF, 0x0416, 0x04C2, 0x04C3, 0x04C4,
        0x04D5, 0x04D6, 0x04DB, 0x04DD, 0x04DE, 0x04DF, 0x04E4, 0x04E5,
        0x04E6, 0x04EC, 0x0503, 0x0506, 0x050D, 0x0510, 0x051F, 0x0520,
        0x0521, 0x0522, 0x052B, 0x0544, 0x0567, 0x056B, 0x056D, 0x0570,
        0x0578, 0x0579, 0x058C, 0x058D, 0x0593, 0x0607, 0x0608, 0x0609,
        0x060A, 0x060B, 0x060E, 0x060F};
    const bool supported = vm.vcs ?
        std::find(vcsRoutes.begin(), vcsRoutes.end(), command) != vcsRoutes.end() :
        std::find(lcsRoutes.begin(), lcsRoutes.end(), command) != lcsRoutes.end();
    if (!supported) return 0;
    if (!vm.environment) {
        error = "Extended mission command requires the live world.";
        return -1;
    }
    auto& world = *vm.environment;
    const auto definition = vm.opcodeDefinitions.find(command);
    if (definition == vm.opcodeDefinitions.end() || definition->second.first < 0 ||
        definition->second.first > 6) {
        error = "Extended mission command has an invalid native operand count.";
        return -1;
    }
    std::array<Operand,6> values{};
    const size_t count = size_t(definition->second.first);
    for (size_t index = 0; index < count; ++index) {
        if (!readOperand(vm, values[index])) {
            error = "Extended mission command has a truncated typed operand.";
            return -1;
        }
    }
    const auto real = [&](size_t index) { return operandFloat(values[index]); };
    const auto condition = [&](bool matches) {
        updateScmCondition(vm, matches, inverted);
        return 1;
    };
    const auto storeInteger = [&](size_t index, int32_t value) {
        if (writeOperand(vm, values[index], value)) return 1;
        error = "Extended mission command requires a writable result.";
        return -1;
    };
    const auto finiteVector = [&]() {
        for (size_t index = 1; index < 4; ++index) {
            if (!std::isfinite(real(index))) {
                error = "Mission transform/velocity vector must be finite.";
                return false;
            }
        }
        return true;
    };
    if ((vm.vcs && command == 0x0364) || (!vm.vcs && command == 0x0593)) {
        for (size_t index = 0; index < 4; ++index) {
            if (!std::isfinite(real(index))) {
                error = "Mission taxi restart destination must be finite.";
                return -1;
            }
        }
        world.restartMissionTaxiDestination = {real(0), real(1), real(2), real(3)};
        world.restartMissionTaxiDestinationSet = true;
        return 1;
    }
    if ((vm.vcs && command == 0x0319) || (!vm.vcs && command == 0x0506)) {
        if (values[0].bits != 0) {
            error = "Drive-by permission requires player zero.";
            return -1;
        }
        world.playerCanDoDriveBy = values[1].bits != 0;
        return 1;
    }
    if (vm.vcs) {
        if (command == 0x0442 && values[0].bits == 0) {
            world.playerSeatedDriveByEnabled = (values[1].bits & 1) != 0;
            return 1;
        }
        if ((command == 0x04B6 || command == 0x04B7) && values[0].bits == 0) {
            auto& navigation = world.playerNavigation;
            if (command == 0x04B6) navigation.alternateObstructionHandling = values[1].bits != 0;
            else navigation.localAvoidanceSolver = values[1].bits != 0;
            return 1;
        }
        if (command == 0x0399 || command == 0x0442 || command == 0x04B6 || command == 0x04B7 ||
            command == 0x04C2 || command == 0x0542 || command == 0x0543 || command == 0x0557) {
            const auto found = world.missionCharacters.find(values[0].bits);
            if (found == world.missionCharacters.end()) {
                // Native 0442 explicitly checks GetAt's null result.
                if (command == 0x0442) return 1;
                error = "Mission character behavior references an unknown character.";
                return -1;
            }
            auto& character = found->second;
            if (command == 0x0399) character.behavior.onlyFrontPassenger = values[1].bits != 0;
            if (command == 0x0442) character.behavior.canDoDriveBy = (values[1].bits & 1) != 0;
            if (command == 0x04B6) character.behavior.alternateObstructionHandling = values[1].bits != 0;
            if (command == 0x04B7) character.behavior.localAvoidanceSolver = values[1].bits != 0;
            if (command == 0x04C2) character.behavior.useUncontrolledWeaponDamage = (values[1].bits & 1) != 0;
            if (command == 0x0542) character.behavior.canAttackPlayerWithCops = values[1].bits != 0;
            if (command == 0x0543) character.behavior.allowDuckAgainstArmedTargets = values[1].bits != 0;
            if (command == 0x0557) {
                character.behavior.seekNodesForMission = values[1].bits != 0;
                character.seekRoute.clear();
                character.seekRouteCursor = 0;
                character.seekNode = UINT32_MAX;
                character.seekWaitUntil = 0;
            }
            return 1;
        }
        if (command == 0x0164) {
            const auto phone = world.payphones.find(values[0].bits);
            if (phone == world.payphones.end()) {
                error = "TURN_PHONE_OFF references an unknown payphone.";
                return -1;
            }
            phone->second.enabled = false;
            return 1;
        }
        if (command == 0x01C5) {
            if (values[0].bits != 0) {
                error = "IS_PLAYER_IN_TAXI requires player zero.";
                return -1;
            }
            if (!world.inVehicle || world.playerVehicleTransition) return condition(false);
            if (!world.vehicleIsTaxi) {
                error = "Taxi query requires native vehicle model classification.";
                return -1;
            }
            return condition(world.vehicleIsTaxi(world.occupiedMissionVehicle));
        }
        if (command == 0x0357) {
            world.playerInStadium = values[0].bits != 0;
            return 1;
        }
        world.empireHudRequested = false;
        return 1;
    }
    if (command == 0x03E4) {
        world.forcedRandomPedType = values[0].bits;
        return 1;
    }
    if (command == 0x03EF) {
        world.generateCarsAroundCamera = values[0].bits != 0;
        return 1;
    }
    if (command == 0x04DB) {
        world.remoteContactDetonation = values[0].bits != 0;
        return 1;
    }
    if (command == 0x04C3 || command == 0x04C4) {
        if (values[0].bits != 0) {
            error = "Havoc statistics require player zero.";
            return -1;
        }
        if (command == 0x04C4) return storeInteger(1, int32_t(world.playerHavoc));
        world.playerHavoc = 0;
        return 1;
    }
    const int32_t handle = values[0].bits;
    const bool damageQuery = command >= 0x051F && command <= 0x0522;
    if (damageQuery) {
        const bool victimCharacter = command <= 0x0520;
        const bool attackerCharacter = command == 0x051F || command == 0x0521;
        StorylandMissionEnvironment::DamageSource damage;
        if (victimCharacter) {
            if (handle == 1) damage = world.playerDamageSource;
            else {
                const auto found = world.missionCharacters.find(handle);
                if (found == world.missionCharacters.end()) return condition(false);
                damage = found->second.lastDamageSource;
            }
        } else {
            const auto found = world.missionVehicles.find(handle);
            if (found == world.missionVehicles.end() || found->second.deleted) return condition(false);
            damage = found->second.lastDamageSource;
        }
        using Kind = StorylandMissionEnvironment::DamageSourceKind;
        if (!attackerCharacter)
            return condition(damage.kind == Kind::Vehicle && damage.handle == values[1].bits);
        if (damage.kind == Kind::Character && damage.handle == values[1].bits) return condition(true);
        int32_t attackerVehicle = -1;
        if (values[1].bits == 1) {
            if (world.inVehicle) attackerVehicle = world.occupiedMissionVehicle;
        } else {
            const auto attacker = world.missionCharacters.find(values[1].bits);
            if (attacker != world.missionCharacters.end() && !attacker->second.attached)
                attackerVehicle = attacker->second.vehicle;
        }
        return condition(attackerVehicle >= 0 && damage.kind == Kind::Vehicle && damage.handle == attackerVehicle);
    }
    const bool characterCommand = command == 0x0416 || command == 0x04DD ||
        command == 0x052B || command == 0x0567 || command == 0x056D ||
        command == 0x0570 || command == 0x0578 || command == 0x058D;
    if (characterCommand) {
        const auto found = world.missionCharacters.find(handle);
        if (found == world.missionCharacters.end()) {
            error = "Extended character command references an unknown non-player actor.";
            return -1;
        }
        auto& character = found->second;
        const bool enabled = values[1].bits != 0;
        switch (command) {
        case 0x0416:
            character.seekNodes = enabled;
            character.seekNode = UINT32_MAX;
            character.seekRoute.clear();
            character.seekRouteCursor = 0;
            character.seekWaitUntil = 0;
            break;
        case 0x04DD: character.drownsInWater = enabled; break;
        case 0x052B: character.stayInCarWhenJacked = enabled; break;
        case 0x0567: character.ignoreThreatsBehindObjects = enabled; break;
        case 0x056D: character.neverTargeted = enabled; break;
        case 0x0570: character.crouchWhenThreatened = enabled; break;
        case 0x0578:
            character.stayInSamePlace = enabled;
            character.stopShooting = enabled;
            character.attackStartedAt = 0;
            world.characterTargets.erase(handle);
            break;
        case 0x058D:
            if (!world.entityCollisionStreaming || !world.entityCollisionStreaming(1, handle, enabled)) {
                error = "Character collision residency could not be updated.";
                return -1;
            }
            character.loadCollision = enabled;
            break;
        }
        return 1;
    }
    const bool objectCommand = command == 0x0386 || command == 0x0387 ||
        command == 0x04DE || command == 0x04DF || command == 0x04EC ||
        command == 0x056B || command == 0x0607 || command == 0x0608 || command == 0x060B;
    if (objectCommand) {
        const auto found = std::find_if(world.objects.begin(), world.objects.end(),
            [handle](const auto& object) { return object.handle == handle; });
        if (found == world.objects.end()) {
            error = "Extended object command references an unknown object.";
            return -1;
        }
        auto& object = *found;
        if (command == 0x0386) {
            if (!finiteVector()) return -1;
            const std::array<float,3> velocity{real(1), real(2), real(3)};
            object.velocity = velocity;
            object.dynamic = true;
        } else if (command == 0x0387) object.collisionEnabled = values[1].bits != 0;
        else if (command == 0x04DE) object.recordsCollisions = values[1].bits != 0;
        else if (command == 0x04DF) return condition(object.collided);
        else if (command == 0x04EC) {
            if (!world.objectInWater) {
                error = "Object water query requires native object bounds and water regions.";
                return -1;
            }
            return condition(world.objectInWater(object));
        } else if (command == 0x056B) object.area = uint8_t(values[1].bits);
        else if (command == 0x0607)
            return condition(storylandObjectWeaponMatches(object.damage.lastWeapon, values[1].bits));
        else if (command == 0x0608) object.damage.lastWeapon = -1;
        else {
            storylandHydrateScriptObject(world, object);
            object.damage.proofs = 0;
            for (size_t index = 0; index < 5; ++index)
                if (values[index + 1].bits) object.damage.proofs |= uint8_t(1u << index);
        }
        return 1;
    }
    const auto found = world.missionVehicles.find(handle);
    if (found == world.missionVehicles.end() || found->second.deleted) {
        error = "Extended vehicle command references an unknown vehicle.";
        return -1;
    }
    auto& vehicle = found->second;
    const bool enabled = values[1].bits != 0;
    if (command == 0x04D5 || command == 0x04D6 || command == 0x04E4) {
        if (!world.vehicleNativeType || world.vehicleNativeType(handle) != 3) {
            error = "Helicopter control requires the native LCS helicopter subclass.";
            return -1;
        }
    }
    if (command == 0x04D5) {
        if (!std::isfinite(real(1))) {
            error = "Helicopter orientation must be finite.";
            return -1;
        }
        vehicle.heliOrientation = real(1);
    } else if (command == 0x04D6) vehicle.heliOrientation = -1;
    else if (command == 0x04E4) vehicle.heliStabiliser = enabled;
    else if (command == 0x04C2) vehicle.convoy = enabled;
    else if (command == 0x04E5) vehicle.straightLineDistance = uint8_t(values[1].bits);
    else if (command == 0x0544) vehicle.canBurstTyres = enabled;
    else if (command == 0x0579) {
        vehicle.positionFrozen = enabled;
        vehicle.loadCollision = !enabled;
    } else if (command == 0x058C) {
        if (!world.entityCollisionStreaming || !world.entityCollisionStreaming(2, handle, enabled)) {
            error = "Vehicle collision residency could not be updated.";
            return -1;
        }
        vehicle.loadCollision = enabled;
    } else if (command == 0x0503) {
        if (!world.vehicleNativeType) {
            error = "Tyre command requires native vehicle subclass metadata.";
            return -1;
        }
        const int type = world.vehicleNativeType(handle);
        if (type != 0 && type != 5) {
            error = "Tyre command requires an automobile or motorcycle.";
            return -1;
        }
        int wheel = uint8_t(values[1].bits);
        if (type == 5 && (wheel == 2 || wheel == 3)) wheel -= 2;
        if (type == 0 && wheel >= 13 && wheel <= 16) {
            static constexpr std::array<int,4> wheels{0, 2, 1, 3};
            wheel = wheels[size_t(wheel - 13)];
        }
        if (wheel < 0 || wheel >= (type == 5 ? 2 : 4)) {
            error = "Tyre index is outside the native wheel array.";
            return -1;
        }
        if (vehicle.canBurstTyres) vehicle.tyreStatus[size_t(wheel)] = 1;
    } else if (command == 0x04E6 || command == 0x0510 || command == 0x050D) {
        const int type = world.vehicleNativeType ? world.vehicleNativeType(handle) : -1;
        if (type != 0 && !(command == 0x050D && type == 3)) {
            error = "Door command requires a compatible native LCS body.";
            return -1;
        }
        if (command == 0x04E6) {
            if (vehicle.doorDamage[1] < 2) {
                vehicle.doorOpenRatios[1] = 1;
                vehicle.doorsChangedByScript = true;
            }
        } else if (command == 0x0510) {
            if (vehicle.doorDamage[1] < 2) vehicle.doorDamage[1] = 2;
            vehicle.bootAngularVelocity = -2;
            vehicle.doorsChangedByScript = true;
        } else {
            for (size_t door = 1; door < vehicle.doorOpenRatios.size(); ++door) {
                vehicle.doorOpenRatios[door] = 0;
                if (door >= 2 && vehicle.doorDamage[door] == 2) vehicle.doorDamage[door] = 0;
                else if (door >= 2 && vehicle.doorDamage[door] == 4) vehicle.doorDamage[door] = 1;
            }
            vehicle.bootAngularVelocity = 0;
            vehicle.doorsChangedByScript = true;
        }
    } else if (command == 0x0609 || command == 0x060A || command == 0x060F) {
        if (!finiteVector()) return -1;
        std::array<float,3> vector{real(1), real(2), real(3)};
        if (command == 0x0609) {
            vehicle.angularVelocity = vector;
            vehicle.angularVelocityChangedByScript = true;
        } else if (command == 0x060F) {
            vehicle.rotation = vector;
            vehicle.heading = vector[2];
            vehicle.rotationChangedByScript = vehicle.headingChangedByScript = true;
        } else {
            // Native 2F1FA0 stores input * 0.02; the host integrates per second.
            vehicle.velocity = vector;
            vehicle.velocityChangedByScript = true;
            const float heading = vehicle.heading * .0174532925199433f;
            vehicle.speed = -std::sin(heading) * vector[0] + std::cos(heading) * vector[1];
        }
    } else if (command == 0x060E) {
        for (size_t index = 1; index < 4; ++index) {
            if (!values[index].reference) {
                error = "Vehicle rotation requires three writable outputs.";
                return -1;
            }
        }
        std::array<float,9> basis{};
        if (!world.vehicleBasis || !world.vehicleBasis(handle, basis)) {
            error = "Vehicle rotation requires its live native basis.";
            return -1;
        }
        const auto degrees = [](float radians) {
            const float value = radians * 57.2957795131f;
            return value < 0 ? value + 360 : value;
        };
        const std::array<float,3> angles{
            degrees(std::atan2(-basis[2], basis[8])),
            degrees(std::asin(std::clamp(basis[5], -1.0f, 1.0f))),
            degrees(std::atan2(-basis[3], basis[4]))};
        for (size_t index = 0; index < angles.size(); ++index) {
            int32_t bits;
            std::memcpy(&bits, &angles[index], sizeof(bits));
            if (storeInteger(index + 1, bits) < 0) return -1;
        }
    }
    return 1;
}
