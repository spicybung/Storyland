int executeLcsRestoredCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (vm.vcs) return 0;
    size_t count = 0;
    switch (command) {
    case 0x029A: case 0x0552: case 0x0556: case 0x055F: case 0x0633: count = 1; break;
    case 0x05E1: count = 2; break;
    default: return 0;
    }
    std::array<Operand, 2> values{};
    for (size_t index = 0; index < count; ++index) {
        if (!readOperand(vm, values[index])) {
            error = "LCS restored native command has a truncated operand.";
            return -1;
        }
    }
    if (!vm.environment) {
        error = "LCS restored native command requires the live world.";
        return -1;
    }
    auto& world = *vm.environment;
    if (command == 0x0552) {
        // SLUS 0x189028 stores the complete int32 event selector.
        world.statistics["LcsTonightsEvent"] = values[0].bits;
        return 1;
    }
    if (command == 0x0556) {
        // The native setter normalizes every nonzero input to one byte 1.
        world.statistics["LcsPlayerHasMetDebbieHarry"] = values[0].bits != 0;
        return 1;
    }
    if (command == 0x055F) {
        // SLUS 0x189208 stores this value unmodified; the rubbish renderer
        // at 0x1F2528 gates its fourth native texture on nonzero.
        world.statistics["LcsRubbishFourthTexture"] = values[0].bits;
        return 1;
    }
    const int32_t handle = values[0].bits;
    if (command == 0x029A || command == 0x0633) {
        const auto found = world.missionVehicles.find(handle);
        if (found == world.missionVehicles.end() || found->second.deleted) {
            error = "LCS restored vehicle command references an unknown vehicle.";
            return -1;
        }
        auto& vehicle = found->second;
        if (command == 0x029A) {
            const uint16_t storedModel = uint16_t(vehicle.model);
            const int32_t model = storedModel >= 0x8000 ? int32_t(storedModel) - 0x10000 : storedModel;
            // 0x25E048 preserves a signed-short alias in addition to the three taxi models.
            updateScmCondition(vm, model == 151 || model == 166 || model == 181 || model == -967, inverted);
            return 1;
        }
        if (!world.vehicleHandlingClass) {
            error = "Helicopter rotor command requires native handling metadata.";
            return -1;
        }
        const int32_t type = world.vehicleHandlingClass(handle);
        if (type < 0) {
            error = "Helicopter rotor command could not resolve the vehicle's native class.";
            return -1;
        }
        if (type == 3) {
            vehicle.rotorSpeed = .22f;
            vehicle.rotorChangedByScript = true;
        }
        return 1;
    }
    // Native SetDie excludes the player from weapon pickups regardless of this bit.
    if (handle == 1) return 1;
    const auto character = world.missionCharacters.find(handle);
    if (character == world.missionCharacters.end()) {
        error = "LCS weapon-drop permission references an unknown character.";
        return -1;
    }
    // 0x2F10F8 writes CPed+0x1A0 bit32 as (argument == 0).
    // SetDie 0x1E94A0 skips the native weapon-pickup producer when that bit is set.
    character->second.behavior.dropsWeapons = values[1].bits != 0;
    return 1;
}

using LcsExtensionTask = StorylandLcsEntityExtensionState::Task;

int lcsExtensionHostObjective(int nativeObjective) {
    switch (nativeObjective) {
    case 0: return 0;
    case 1: return 1;
    case 3: case 10: return 0x00E0;
    case 4: return 0x00E1;
    case 6: return 2;
    case 8: return 0x00E4;
    case 9: return 0x00E5;
    case 11: return 0x00E6;
    case 12: return 0x00E7;
    case 15: return 0x00EF;
    case 16: return 0x00F1;
    case 17: return 0x00E9;
    case 18: return 0x00EA;
    case 21: return 0x00EB;
    case 22: return 0x00EC;
    case 24: return 0x00ED;
    case 25: return 0x0158;
    case 30: return 0x011B;
    case 35: return 0x00F0;
    case 41: return 0x00F2;
    case 46: return 0x00F4;
    case 32: return 0x0219;
    case 101: return 0x0359;
    case 102: return 0x0119;
    case 103: return 0x10000;
    case 104: return 0x04F8;
    default: return -1;
    }
}

int lcsExtensionScriptObjective(int nativeObjective) {
    switch (nativeObjective) {
    case 0: return 0;
    case 1: return 1;
    case 3: return 0x0198;
    case 4: return 0x0199;
    case 6: return 2;
    case 8: return 0x01CE;
    case 9: return 0x01D0;
    case 10: return 0x01D3;
    case 11: return 0x01D4;
    case 12: return 0x01D6;
    case 15: return 0x0310;
    case 16: return 0x03E7;
    case 17: return 0x01D9;
    case 18: return 0x01DA;
    case 21: return 0x01DD;
    case 22: return 0x01DE;
    case 24: return 0x0216;
    case 25: return 0x023E;
    case 30: return 0x01E6;
    case 35: return 0x037C;
    case 41: return 0x0470;
    case 46: return 0x04CB;
    case 32: return 0x036A;
    case 101: return 0x0585;
    case 102: return 0x01E3;
    case 103: return 0x009E;
    case 104: return 0x04F8;
    default: return -1;
    }
}

int32_t lcsExtensionRuntimeCharacter(int32_t handle) {
    return handle == 1 ? 0 : handle;
}

bool lcsExtensionCharacterExists(const StorylandMissionEnvironment& world, int32_t handle) {
    return handle == 1 || world.missionCharacters.count(handle) != 0;
}

void lcsExtensionAssignTask(StorylandMissionEnvironment& world, int32_t handle,
    const LcsExtensionTask& task) {
    auto& character = world.missionCharacters.at(handle);
    world.lcsEntityExtension.tasks[handle] = task;
    character.objective = lcsExtensionScriptObjective(task.objective);
    character.objectiveTarget = task.target;
    character.objectiveCompleted = false;
    character.walkingToTarget = false;
    character.followingScriptPath = task.objective == 103;
    character.targetX = task.destination[0];
    character.targetY = task.destination[1];
    character.objectiveHeight = task.destination[2];
    character.objectiveRadius = task.radius;
    character.targetRadius = task.radius;
    character.walkSpeed = task.speed;
    character.routeId = task.route;
    character.routeMode = task.routeMode;
    character.routeCursor = task.routeCursor;
    character.routeDirection = task.routeDirection;
    character.animationId = -1;
    character.attackStartedAt = 0;
    character.seekRoute.clear();
    character.seekRouteCursor = 0;
    character.seekNode = UINT32_MAX;
    world.characterWanderTasks.erase(handle);
    world.characterTargets.erase(handle);
    if (task.objective != 16 && task.objective != 17 && task.objective != 18)
        world.characterVehicleTasks.erase(handle);
}

bool lcsExtensionQueueVehicleTask(StorylandMissionEnvironment& world, int32_t handle,
    int32_t vehicleHandle, int32_t seat, bool exiting, bool hijacking, std::string& error) {
    const auto actor = world.missionCharacters.find(handle);
    const auto vehicle = world.missionVehicles.find(vehicleHandle);
    if (actor == world.missionCharacters.end() || vehicle == world.missionVehicles.end() ||
        vehicle->second.deleted) {
        error = "LCS vehicle objective references an unknown character or vehicle.";
        return false;
    }
    StorylandMissionEnvironment::CharacterVehicleTask transition;
    transition.vehicle = vehicleHandle;
    transition.seat = seat;
    transition.exiting = exiting;
    transition.hijacking = hijacking;
    if (!prepareLcsCharacterVehicleTask(world, handle, transition, error)) return false;
    world.characterVehicleTasks[handle] = std::move(transition);
    return true;
}

bool lcsExtensionGround(StorylandMissionEnvironment& world, float x, float y,
    float& z, bool always, std::string& error) {
    if (!always && z > -100.0f) return true;
    if (!world.groundHeightAt) {
        error = "LCS objective requires the loaded collision world.";
        return false;
    }
    float ground = 0;
    if (!world.groundHeightAt({x, y, 1000.0f}, ground)) ground = 20.0f;
    if (!std::isfinite(ground)) {
        error = "LCS objective ground height is non-finite.";
        return false;
    }
    z = ground;
    return true;
}

int32_t lcsExtensionNextCollective(StorylandMissionEnvironment& world) {
    auto& state = world.lcsEntityExtension;
    const int32_t result = state.nextCollective;
    state.nextCollective = result == INT32_MAX ? 0 : result + 1;
    return result;
}

bool lcsExtensionAddCollectiveMember(StorylandMissionEnvironment& world,
    int32_t collective, int32_t handle) {
    const auto ped = world.missionCharacters.find(handle);
    if (ped == world.missionCharacters.end() || !ped->second.alive || ped->second.health <= 0 ||
        ped->second.creationSource == 2 || ped->second.type == 6) return false;
    auto& rows = world.lcsEntityExtension.collectives;
    const auto free = std::find_if(rows.begin(), rows.end(), [](const auto& row) {
        return row.collective == -1;
    });
    if (free == rows.end()) return false;
    *free = {collective, handle};
    return true;
}

std::vector<int32_t> lcsExtensionCollectiveMembers(StorylandMissionEnvironment& world,
    int32_t collective) {
    std::vector<int32_t> members;
    for (auto& row : world.lcsEntityExtension.collectives) {
        if (row.collective != collective) continue;
        if (!world.missionCharacters.count(row.character)) {
            row = {};
            continue;
        }
        members.push_back(row.character);
    }
    return members;
}

bool lcsExtensionRoute(StorylandMissionEnvironment& world, int32_t route,
    int32_t mode, LcsExtensionTask& task, std::string& error) {
    task.route = int16_t(uint16_t(route));
    task.routeMode = int16_t(uint16_t(mode));
    const auto first = std::find_if(world.characterRoutePoints.begin(), world.characterRoutePoints.end(),
        [&](const auto& point) { return point.route == task.route; });
    if (task.route == -1 || task.routeMode < 1 || task.routeMode > 3 ||
        first == world.characterRoutePoints.end() || first + 1 == world.characterRoutePoints.end() ||
        (first + 1)->route != task.route) {
        error = "LCS follow-route requires two contiguous points and mode 1, 2 or 3.";
        return false;
    }
    task.objective = 30;
    task.destination = (first + 1)->position;
    return true;
}

int executeLcsExtensionObjectiveCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    int count = 0;
    switch (command) {
    case 0x0198: case 0x01E5: case 0x025C: case 0x025D: case 0x025E:
    case 0x0261: case 0x027E: case 0x027F: case 0x036A: case 0x037C:
    case 0x03B1: case 0x03E7: case 0x0464: case 0x054E: count = 1; break;
    case 0x01CE: case 0x01CF: case 0x01D0: case 0x01D1: case 0x01D3:
    case 0x01D4: case 0x01D5: case 0x01DD: case 0x01DE: case 0x01E3:
    case 0x01E4: case 0x0262: case 0x0263: case 0x0264: case 0x0265:
    case 0x0266: case 0x0267: case 0x0268: case 0x0269: case 0x026A:
    case 0x026B: case 0x026D: case 0x026E: case 0x0272: case 0x027D:
    case 0x031E: case 0x0470: case 0x04CB: count = 2; break;
    case 0x01E6: case 0x0277: case 0x0278: case 0x027B: case 0x0310: count = 3; break;
    case 0x0199: case 0x01E7: case 0x025F: case 0x027C: count = 4; break;
    case 0x019A: case 0x0260: case 0x0273: case 0x027A: count = 5; break;
    case 0x009E: count = 6; break;
    default: return 0;
    }
    std::array<Operand, 6> values{};
    for (int index = 0; index < count; ++index) {
        if (!readOperand(vm, values[size_t(index)])) {
            error = "LCS objective has a truncated or invalid operand.";
            return -1;
        }
    }
    if (!vm.environment) {
        error = "LCS objectives require the live world.";
        return -1;
    }
    auto& world = *vm.environment;
    const auto real = [&](size_t index) { return operandFloat(values[index]); };
    const auto condition = [&](bool value) { updateScmCondition(vm, value, inverted); return 1; };
    const auto store = [&](size_t index, int32_t value) {
        if (writeOperand(vm, values[index], value)) return 1;
        error = "LCS collective creation requires a writable output.";
        return -1;
    };
    const auto finite = [&](int first, int last) {
        for (int index = first; index < last; ++index) {
            if (!std::isfinite(real(size_t(index)))) {
                error = "LCS objective coordinates must be finite.";
                return false;
            }
        }
        return true;
    };

    if (command == 0x01E7 || command == 0x03B1) {
        const int16_t route = int16_t(uint16_t(values[0].bits));
        auto& points = world.characterRoutePoints;
        if (command == 0x03B1) {
            const auto first = std::find_if(points.begin(), points.end(),
                [route](const auto& point) { return point.route == route; });
            points.erase(first, points.end());
            return 1;
        }
        if (!finite(1, 4)) return -1;
        if (route == -1 || points.size() == 200) {
            error = "The native LCS route table is full or the route uses reserved ID -1.";
            return -1;
        }
        points.push_back({route, {real(1), real(2), real(3)}});
        return 1;
    }

    if (command == 0x027C || command == 0x027D) {
        const size_t output = size_t(count - 1);
        if (!values[output].reference) {
            error = "LCS collective handle output is not writable.";
            return -1;
        }
        std::vector<int32_t> candidates;
        const auto includeVehicle = [&](int32_t vehicle) {
            const auto car = world.missionVehicles.find(vehicle);
            if (car != world.missionVehicles.end() && car->second.driver < 0 && world.acquireVehicleDriver)
                world.acquireVehicleDriver(vehicle);
            for (const auto& entry : world.missionCharacters)
                if (entry.second.vehicle == vehicle && !entry.second.attached) candidates.push_back(entry.first);
        };
        if (command == 0x027D) {
            const auto vehicle = world.missionVehicles.find(values[0].bits);
            if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
                error = "LCS collective vehicle does not exist.";
                return -1;
            }
            includeVehicle(values[0].bits);
        } else {
            if (!finite(0, 3) || real(2) < 0) {
                error = "LCS collective recruitment requires a finite nonnegative radius.";
                return -1;
            }
            float ground = 0;
            if (!lcsExtensionGround(world, real(0), real(1), ground, true, error)) return -1;
            const float radius = real(2);
            const std::array<float, 6> bounds{real(0) - radius, real(1) - radius, ground - radius,
                real(0) + radius, real(1) + radius, ground + radius};
            const auto inside = [&](float x, float y, float z) {
                return std::hypot(x - real(0), y - real(1), z - ground) <= radius;
            };
            for (const auto& entry : world.missionCharacters) {
                const auto& ped = entry.second;
                if (ped.vehicle < 0 && inside(ped.x, ped.y, ped.z)) candidates.push_back(entry.first);
            }
            for (const auto& entry : world.missionVehicles)
                if (!entry.second.deleted && inside(entry.second.x, entry.second.y, entry.second.z))
                    includeVehicle(entry.first);
            if (world.ambientCharactersInArea && world.acquireAmbientCharacters) {
                for (const auto& ped : world.ambientCharactersInArea(bounds)) {
                    if (!ped.onFoot || ped.health <= 0 || ped.type == 6 ||
                        !inside(ped.position[0], ped.position[1], ped.position[2])) continue;
                    std::vector<int32_t> acquired;
                    if (!world.acquireAmbientCharacters({ped.slot}, false, acquired) || acquired.size() != 1) {
                        error = "LCS collective could not acquire a selected ambient character.";
                        return -1;
                    }
                    candidates.push_back(acquired.front());
                }
            }
        }
        const int32_t collective = lcsExtensionNextCollective(world);
        for (int32_t candidate : candidates) lcsExtensionAddCollectiveMember(world, collective, candidate);
        return store(output, collective);
    }

    const bool collective = command >= 0x025C && command <= 0x027F;
    const int32_t first = values[0].bits;
    std::vector<int32_t> actors;
    if (collective) {
        actors = lcsExtensionCollectiveMembers(world, first);
        if (command == 0x027E) {
            for (auto& row : world.lcsEntityExtension.collectives)
                if (row.collective == first) row = {};
            return 1;
        }
        if (command == 0x027F) {
            return condition(std::all_of(actors.begin(), actors.end(), [&](int32_t handle) {
                const auto& ped = world.missionCharacters.at(handle);
                return ped.vehicle >= 0 && !ped.attached;
            }));
        }
    } else if (command == 0x0464) {
        const auto vehicle = world.missionVehicles.find(first);
        if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
            error = "LCS leave-vehicle command references an unknown car.";
            return -1;
        }
        for (const auto& entry : world.missionCharacters)
            if (entry.second.vehicle == first && !entry.second.attached) actors.push_back(entry.first);
        if (world.inVehicle && world.occupiedMissionVehicle == first) {
            world.playerExitVehicleRequested = true;
            world.playerVehicleTransition = true;
        }
    } else if (command == 0x031E && first == 1) {
        world.lcsEntityExtension.runningOverrides[first] = values[1].bits != 0;
        return 1;
    } else if (command == 0x03E7 && first == 1) {
        world.lcsEntityExtension.tasks.erase(1);
        world.playerWalkTask.clear();
        world.playerNavigation.clearRoute();
        world.playerVehicleEntryTarget = -1;
        world.playerVehicleEntryAsPassenger = false;
        if (world.inVehicle) {
            world.playerExitVehicleRequested = true;
            world.playerVehicleTransition = true;
        }
        return 1;
    } else {
        if (!world.missionCharacters.count(first)) {
            error = "LCS objective requires a non-player character.";
            return -1;
        }
        actors.push_back(first);
    }

    if (command == 0x01E3 || command == 0x01E4 || command == 0x01E5) {
        int32_t leader = -1;
        if (command != 0x01E5) {
            if (command == 0x01E4 && values[1].bits != 0) {
                error = "LCS leader player index must be zero.";
                return -1;
            }
            leader = command == 0x01E4 ? 1 : values[1].bits;
            if (leader == first || !lcsExtensionCharacterExists(world, leader)) {
                error = "LCS leader must be another existing character.";
                return -1;
            }
        }
        auto& ped = world.missionCharacters.at(first);
        if (leader < 0) {
            storylandClearVcsLeader(world, first, ped);
            world.lcsEntityExtension.tasks.erase(first);
        } else {
            ped.leader = lcsExtensionRuntimeCharacter(leader);
            ped.leaderOutOfRange = false;
            ped.leaderUpdateAfter = 0;
            ped.leaderVehicleAfter = 0;
        }
        return 1;
    }
    if (command == 0x031E) {
        auto& ped = world.missionCharacters.at(first);
        world.lcsEntityExtension.runningOverrides[first] = values[1].bits != 0;
        ped.walkSpeed = values[1].bits ? 3.8f : 1.25f;
        const auto task = world.lcsEntityExtension.tasks.find(first);
        if (task != world.lcsEntityExtension.tasks.end()) {
            task->second.running = values[1].bits != 0;
            task->second.speed = ped.walkSpeed;
        }
        return 1;
    }
    if (command == 0x054E) {
        auto& ped = world.missionCharacters.at(first);
        if (ped.followingScriptPath) {
            ped.followingScriptPath = false;
            ped.walkingToTarget = false;
            ped.objective = 0;
            ped.seekRoute.clear();
            world.lcsEntityExtension.tasks.erase(first);
        }
        return 1;
    }

    LcsExtensionTask task;
    switch (command) {
    case 0x025C: task.objective = 0; break;
    case 0x025D: task.objective = 1; break;
    case 0x0198: case 0x025E: task.objective = 3; break;
    case 0x0199: case 0x019A: case 0x025F: case 0x0260: task.objective = 4; break;
    case 0x0261: task.objective = 6; break;
    case 0x01CE: case 0x01CF: case 0x0262: case 0x0263: task.objective = 8; break;
    case 0x01D0: case 0x01D1: case 0x0264: case 0x0265: task.objective = 9; break;
    case 0x01D3: case 0x0266: case 0x0267: task.objective = 10; break;
    case 0x01D4: case 0x01D5: case 0x0268: case 0x0269: task.objective = 11; break;
    case 0x026A: case 0x026B: task.objective = 12; break;
    case 0x0310: task.objective = 15; break;
    case 0x03E7: case 0x0464: task.objective = 16; break;
    case 0x026D: task.objective = 17; break;
    case 0x026E: task.objective = 18; break;
    case 0x01DD: task.objective = 21; break;
    case 0x01DE: case 0x0272: task.objective = 22; break;
    case 0x0273: case 0x0278: task.objective = 24; break;
    case 0x027A: case 0x027B: task.objective = 25; task.speed = 3.8f; break;
    case 0x01E6: case 0x0277:
        if (!lcsExtensionRoute(world, values[1].bits, values[2].bits, task, error)) return -1;
        break;
    case 0x037C: task.objective = 35; break;
    case 0x0470: task.objective = 41; break;
    case 0x04CB: task.objective = 46; break;
    case 0x036A: task.objective = 32; break;
    case 0x009E: task.objective = 103; break;
    default: error = "LCS objective has no native assignment."; return -1;
    }
    const bool targetPlayer = command == 0x01CF || command == 0x01D1 || command == 0x01D3 ||
        command == 0x01D5 || command == 0x0263 || command == 0x0265 || command == 0x0267 ||
        command == 0x0269 || command == 0x026B;
    const bool targetCharacter = task.objective == 8 || task.objective == 9 || task.objective == 10 ||
        task.objective == 11 || task.objective == 12 || task.objective == 15 || task.objective == 46;
    if (targetCharacter) {
        if (targetPlayer && values[1].bits != 0) {
            error = "LCS target player index must be zero.";
            return -1;
        }
        task.target = targetPlayer ? 1 : values[1].bits;
        if (!lcsExtensionCharacterExists(world, task.target)) {
            error = "LCS objective character target does not exist.";
            return -1;
        }
    }
    if (task.objective == 17 || task.objective == 18 || task.objective == 22 || task.objective == 41) {
        task.target = values[1].bits;
        const auto vehicle = world.missionVehicles.find(task.target);
        if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
            error = "LCS objective vehicle target does not exist.";
            return -1;
        }
    }
    if (task.objective == 21) {
        task.target = values[1].bits;
        if (std::none_of(world.objects.begin(), world.objects.end(), [&](const auto& object) {
            return object.handle == task.target;
        })) {
            error = "LCS destruction objective references an unknown object.";
            return -1;
        }
    }
    if (task.objective == 4 || task.objective == 24 || task.objective == 25 || task.objective == 103) {
        if (!finite(1, count - (task.objective == 103 ? 1 : 0))) return -1;
        const bool area = command == 0x019A || command == 0x0260 || command == 0x0273 || command == 0x027A;
        task.destination = {real(1), real(2), task.objective == 103 || command == 0x0199 ||
            command == 0x025F ? real(3) : 0};
        if (area) {
            task.destination[0] = real(1) * .5f + real(3) * .5f;
            task.destination[1] = real(2) * .5f + real(4) * .5f;
            task.radius = std::max(std::fabs(real(3) - real(1)), std::fabs(real(4) - real(2))) * .5f;
        }
        const float originalHeight = task.destination[2];
        if (!lcsExtensionGround(world, task.destination[0], task.destination[1], task.destination[2],
            area || task.objective == 24 || task.objective == 25, error)) return -1;
        if (command == 0x025F && originalHeight <= -100.0f) task.destination[2] += originalHeight;
        if (task.objective == 103) {
            task.radius = real(4);
            task.speed = values[5].bits == 1 ? 3.8f : 1.25f;
            if (task.radius < 0) { error = "LCS follow-path radius must be nonnegative."; return -1; }
        }
    }
    if (task.objective == 15 && (values[2].bits < 0 || values[2].bits > 8)) {
        error = "LCS formation index is outside the native 0..8 table.";
        return -1;
    }
    for (int32_t handle : actors) {
        auto& ped = world.missionCharacters.at(handle);
        if (task.target == handle && targetCharacter) continue;
        auto assignment = task;
        if (task.objective == 3) {
            assignment.target = -1;
            assignment.destination = {ped.x, ped.y, ped.z};
            assignment.actionStartedAt = world.gameMilliseconds;
            assignment.actionDuration = 10.0f;
        } else if (task.objective == 10) {
            assignment.actionStartedAt = world.gameMilliseconds;
            assignment.actionDuration = 6.0f;
        }
        if (task.objective == 16) assignment.target = ped.vehicle;
        if (task.objective == 15) ped.formation = values[2].bits;
        lcsExtensionAssignTask(world, handle, assignment);
        if (task.objective == 0) {
            world.lcsEntityExtension.tasks.erase(handle);
            ped.objectiveCompleted = false;
        }
    }
    return 1;
}

int executeLcsExtensionPopulationCommand(StorylandMissionVm& vm, uint16_t command,
    std::string& error) {
    int count = 0;
    switch (command) {
    case 0x02E1: case 0x0594: count = 8; break;
    case 0x02E2: case 0x037B: count = 4; break;
    case 0x046E: count = 10; break;
    case 0x05D6: count = 5; break;
    case 0x0565: count = 2; break;
    case 0x0566: count = 3; break;
    default: return 0;
    }
    std::string zoneName;
    if (command == 0x02E2 && !readScmText(vm, zoneName)) {
        error = "LCS population zone label is truncated.";
        return -1;
    }
    std::array<Operand, 10> values{};
    for (int index = 0; index < count; ++index)
        if (!readOperand(vm, values[size_t(index)])) {
            error = "LCS population command has an invalid operand.";
            return -1;
        }
    if (!vm.environment) { error = "LCS population requires the live world."; return -1; }
    auto& world = *vm.environment;
    const size_t output = size_t(count - 1);
    if (!values[output].reference ||
        values[output].variable >= (values[output].global ? vm.globals.size() : vm.locals.size())) {
        error = "LCS population result is not writable.";
        return -1;
    }
    const auto store = [&](int32_t value) { return writeOperand(vm, values[output], value) ? 1 : -1; };
    const auto real = [&](size_t index) { return operandFloat(values[index]); };
    const auto finite = [&](int count) {
        for (int index = 0; index < count; ++index)
            if (!std::isfinite(real(size_t(index)))) return false;
        return true;
    };
    if (command == 0x037B || command == 0x0565 || command == 0x0566) {
        if (!world.randomCharacterModel || !world.resolvePedStats || world.missionCharacters.size() >= 128 ||
            world.nextMissionCharacterHandle < 2 || world.nextMissionCharacterHandle == INT32_MAX ||
            world.missionCharacters.count(world.nextMissionCharacterHandle)) {
            error = "LCS random creation requires native population metadata and a free character slot.";
            return -1;
        }
        StorylandMissionEnvironment::Character character;
        int32_t vehicleHandle = -1;
        if (command == 0x037B) {
            if (!finite(3)) { error = "LCS random character coordinates are non-finite."; return -1; }
            character.x = real(0); character.y = real(1); character.z = real(2);
            if (!lcsExtensionGround(world, character.x, character.y, character.z, false, error)) return -1;
        } else {
            vehicleHandle = values[0].bits;
            const auto vehicle = world.missionVehicles.find(vehicleHandle);
            if (vehicle == world.missionVehicles.end() || vehicle->second.deleted || !vehicle->second.alive) {
                error = "LCS random occupant references an unknown vehicle.";
                return -1;
            }
            auto& car = vehicle->second;
            if (command == 0x0565) {
                if (car.driver >= 0 || (world.inVehicle && world.occupiedMissionVehicle == vehicleHandle &&
                    world.playerPassengerSeat < 0)) return store(-1);
            } else {
                if (!world.vehiclePassengerCapacity) { error = "LCS passenger creation requires seat metadata."; return -1; }
                const int capacity = world.vehiclePassengerCapacity(vehicleHandle);
                int seat = values[1].bits < 0 ? 0 : int(uint8_t(values[1].bits));
                if (values[1].bits < 0)
                    while (seat < capacity && lcsPassengerSeatOccupied(world, -1, vehicleHandle, seat)) ++seat;
                if (seat >= capacity || lcsPassengerSeatOccupied(world, -1, vehicleHandle, seat)) return store(-1);
                character.passengerSeat = seat;
            }
            character.vehicle = vehicleHandle;
            character.x = car.x; character.y = car.y; character.z = car.z; character.heading = car.heading;
        }
        if (!world.randomCharacterModel(vehicleHandle, character.model, character.type) ||
            character.model < 0 || character.type < 0) return store(-1);
        if (!world.resolvePedStats(character.model, -1, character.pedStats) || !character.pedStats.valid) {
            error = "LCS random character has no native model personality.";
            return -1;
        }
        character.animationGroup = character.pedStats.animationGroup;
        character.pedStat = character.pedStats.statIndex;
        character.canBeShotInVehicle = true;
        character.heedThreats = false;
        if (command == 0x037B) character.z += 1.0f;
        character.creationSource = 2;
        character.speechProfile = world.nativePedSpeechProfile ? world.nativePedSpeechProfile(character.model) : -1;
        const uint16_t draw = uint16_t(world.nextNativeRandom()) % 25;
        character.walletMoney = draw == 23 ? 400 : draw;
        character.walletInitialized = true;
        const auto threats = world.pedTypeThreats.find(character.type);
        if (threats != world.pedTypeThreats.end()) character.fearFlags = threats->second;
        const int32_t handle = world.nextMissionCharacterHandle++;
        world.missionCharacters.emplace(handle, std::move(character));
        if (command == 0x0565) world.missionVehicles.at(vehicleHandle).driver = handle;
        if (vm.isMissionThread) world.missionCleanupCharacters.insert(handle);
        return store(handle);
    }

    const StorylandNativeZone* zone = nullptr;
    float playerZ = world.z;
    const auto playerCar = world.missionVehicles.find(world.occupiedMissionVehicle);
    if (world.inVehicle && playerCar != world.missionVehicles.end()) playerZ = playerCar->second.z;
    const float height = command == 0x046E ? 10.0f : 5.0f;
    std::array<float, 6> bounds{};
    if (command == 0x02E2) {
        if (!world.zones) { error = "LCS population zone query requires the loaded zone table."; return -1; }
        zone = world.zones->named(zoneName);
        if (!zone) return store(-1);
        bounds = {zone->minimum.x, zone->minimum.y, std::max(zone->minimum.z, playerZ - height),
            zone->maximum.x, zone->maximum.y, std::min(zone->maximum.z, playerZ + height)};
    } else {
        if (!finite(4)) { error = "LCS population area bounds are non-finite."; return -1; }
        bounds = {std::min(real(0), real(2)), std::min(real(1), real(3)), playerZ - height,
            std::max(real(0), real(2)), std::max(real(1), real(3)), playerZ + height};
    }
    const bool unchecked = command == 0x05D6;
    const size_t categoryOffset = command == 0x02E2 ? 0 : 4;
    const auto suitable = [&](int type, int model) {
        if (unchecked) return true;
        if (command == 0x046E) {
            if (type != 6) return false;
            if (model >= 1 && model <= 4) return values[size_t(model + 3)].bits != 0;
            return model >= 97 && model <= 104 && values[8].bits != 0;
        }
        return ((type == 4 || type == 5) && values[categoryOffset].bits != 0) ||
            (type >= 7 && type <= 15 && values[categoryOffset + 1].bits != 0) ||
            ((type == 18 || type == 20) && values[categoryOffset + 2].bits != 0);
    };
    const auto inside = [&](float x, float y, float z) {
        return x >= bounds[0] && x <= bounds[3] && y >= bounds[1] && y <= bounds[4] &&
            z >= bounds[2] && z <= bounds[5];
    };
    int32_t result = -1;
    for (auto entry = world.missionCharacters.rbegin(); entry != world.missionCharacters.rend(); ++entry) {
        const auto& ped = entry->second;
        if (ped.fadeOut || ped.waitState != 0 || ped.removeFromWorld ||
            !inside(ped.x, ped.y, ped.z) || !suitable(ped.type, ped.model)) continue;
        if (!unchecked && (entry->first == world.lastRandomCharacter || ped.creationSource != 1 ||
            !ped.alive || ped.health <= 0 || ped.leader >= 0 ||
            std::any_of(world.missionCharacters.begin(), world.missionCharacters.end(),
                [&](const auto& other) { return other.second.leader == entry->first; }) ||
            (world.characterInControl && !world.characterInControl(entry->first)))) continue;
        if (command == 0x0594 && (ped.vehicle >= 0 || ped.behavior.attemptedAttractor ||
            ped.behavior.iceCreamVehicle >= 0)) continue;
        result = entry->first;
        break;
    }
    if (result < 0) {
        const auto& enumerate = unchecked ? world.allAmbientCharactersInArea : world.ambientCharactersInArea;
        if (!enumerate || !world.acquireAmbientCharacters) {
            error = "LCS population query requires live ambient character enumeration.";
            return -1;
        }
        for (const auto& ped : enumerate(bounds)) {
            if ((!unchecked && ped.health <= 0) || (command == 0x0594 && !ped.onFoot) ||
                !suitable(ped.type, ped.model) || !inside(ped.position[0], ped.position[1], ped.position[2])) continue;
            std::vector<int32_t> acquired;
            if (!world.acquireAmbientCharacters({ped.slot}, true, acquired) || acquired.size() != 1) {
                error = "LCS could not acquire the selected ambient character.";
                return -1;
            }
            result = acquired.front();
            break;
        }
    }
    if (result >= 0) {
        auto& ped = world.missionCharacters.at(result);
        ped.creationSource = 2; ped.persistent = true; ped.heedThreats = false;
        world.lastRandomCharacter = result;
        if (vm.isMissionThread) world.missionCleanupCharacters.insert(result);
    }
    return store(result);
}

bool lcsExtensionCanSee(StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::Character& source, const std::array<float, 3>& target) {
    const float dx = target[0] - source.x, dy = target[1] - source.y;
    const float heading = source.heading * 0.0174532925199433f;
    if (dx * -std::sin(heading) + dy * std::cos(heading) < 0 || dx * dx + dy * dy >= 1600.0f)
        return false;
    return !world.scriptLineOfSightHit({source.x, source.y, source.z + 1},
        {target[0], target[1], target[2] + 1}, 1);
}

int executeLcsExtensionCharacterCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    int count = 0;
    switch (command) {
    case 0x0326: case 0x0327: case 0x0354: case 0x04F8: case 0x0512:
    case 0x0576: case 0x0589: case 0x058B: case 0x0590: case 0x059C: count = 1; break;
    case 0x0128: case 0x0324: case 0x0325: case 0x0369: case 0x03F6:
    case 0x03F7: case 0x0421: case 0x0438: case 0x0472: case 0x0485:
    case 0x04E7: case 0x0568: case 0x0597: case 0x0598: case 0x05F6: count = 2; break;
    case 0x03FE: case 0x04F0: case 0x0585: count = 3; break;
    case 0x0515: count = 5; break;
    case 0x02DA: count = 6; break;
    default: return 0;
    }
    std::array<Operand, 6> values{};
    for (int index = 0; index < count; ++index)
        if (!readOperand(vm, values[size_t(index)])) {
            error = "LCS character command has an invalid operand.";
            return -1;
        }
    if (!vm.environment) { error = "LCS character command requires the live world."; return -1; }
    auto& world = *vm.environment;
    const auto condition = [&](bool result) { updateScmCondition(vm, result, inverted); return 1; };
    const auto writable = [&](size_t index) {
        return values[index].reference &&
            values[index].variable < (values[index].global ? vm.globals.size() : vm.locals.size());
    };
    const auto store = [&](size_t index, int32_t result) {
        if (writable(index) && writeOperand(vm, values[index], result)) return 1;
        error = "LCS character query result is not writable.";
        return -1;
    };
    const auto real = [&](size_t index) { return operandFloat(values[index]); };
    const auto finite = [&](int first, int last) {
        for (int index = first; index < last; ++index) if (!std::isfinite(real(size_t(index)))) return false;
        return true;
    };
    if (command == 0x03F6 || command == 0x03F7) {
        if (values[0].bits < 0 || values[0].bits >= 23) {
            error = "LCS threat policy has an invalid native pedestrian type.";
            return -1;
        }
        auto& threats = world.pedTypeThreats[values[0].bits];
        if (command == 0x03F6) threats |= uint32_t(values[1].bits);
        else threats &= ~uint32_t(values[1].bits);
        return 1;
    }
    if (command == 0x0597) {
        if (values[0].bits < 0 || size_t(values[0].bits) >= world.gangs.size()) {
            error = "LCS gang policy index is outside the native table.";
            return -1;
        }
        world.gangs[size_t(values[0].bits)].attackWithPolice = values[1].bits != 0;
        return 1;
    }
    if (command == 0x04E7) {
        world.playerSpeechEnabled = values[1].bits == 0;
        return 1;
    }
    if (command == 0x02DA) {
        if (values[0].bits != 0 || !finite(1, 5) || !world.characterShooting) {
            error = "LCS player shooting-area query requires player zero and live shooting state.";
            return -1;
        }
        const float minX = std::min(real(1), real(3)), maxX = std::max(real(1), real(3));
        const float minY = std::min(real(2), real(4)), maxY = std::max(real(2), real(4));
        if (values[5].bits && world.locateRegions.size() < 128) {
            StorylandMissionEnvironment::LocateRegion region;
            region.center = {(minX + maxX) * .5f, (minY + maxY) * .5f, world.z};
            region.extent = {(maxX - minX) * .5f, (maxY - minY) * .5f, 0};
            region.expiresAt = world.gameMilliseconds + 100;
            world.locateRegions.push_back(region);
        }
        return condition(world.characterShooting(0) && world.x >= minX && world.x <= maxX &&
            world.y >= minY && world.y <= maxY);
    }
    if (command == 0x0568) {
        if (values[0].bits != 0) { error = "LCS drive-by weapon command requires player zero."; return -1; }
        if (!world.inVehicle) return 1;
        auto weapon = std::find_if(world.playerWeapons.begin(), world.playerWeapons.end(),
            [](const auto& entry) { return entry.second.slot == 5 && entry.second.owned; });
        if (weapon == world.playerWeapons.end()) {
            weapon = world.playerWeapons.find(23);
            if (weapon == world.playerWeapons.end() || weapon->second.slot != 5 || weapon->second.model < 0) {
                error = "LCS drive-by weapon requires the native weapon 23 metadata.";
                return -1;
            }
            weapon->second.owned = true;
            weapon->second.ammunition = std::max(0, values[1].bits);
            world.currentWeapon = 23;
            world.weaponSelectionRequested = true;
        } else if (weapon->second.ammunition < values[1].bits) {
            weapon->second.ammunition = values[1].bits;
        }
        world.weaponInventoryChanged = true;
        return 1;
    }
    if (command == 0x0327) {
        if (values[0].bits != 0 || !world.explodeCharacterHead || !world.explodeCharacterHead(0)) {
            error = "LCS explode-player-head requires the live native damage consumer.";
            return -1;
        }
        return 1;
    }
    const int32_t handle = values[0].bits;
    const bool player = handle == 1;
    const auto found = world.missionCharacters.find(handle);
    auto* character = found != world.missionCharacters.end() ? &found->second : nullptr;
    if (!player && !character) {
        error = "LCS character command references an unknown character.";
        return -1;
    }
    if (command == 0x0326) {
        if (!world.explodeCharacterHead || !world.explodeCharacterHead(lcsExtensionRuntimeCharacter(handle))) {
            error = "LCS explode-character-head requires the live native damage consumer.";
            return -1;
        }
        return 1;
    }
    if (command == 0x0421) {
        if (values[1].bits != 15) return 1;
        if (!world.characterSpeech) { error = "LCS script speech requires native audio."; return -1; }
        world.characterSpeech(lcsExtensionRuntimeCharacter(handle), 0x93);
        return 1;
    }
    if (command == 0x0472) {
        const int32_t leader = lcsExtensionRuntimeCharacter(handle);
        const int32_t followers = int32_t(std::count_if(world.missionCharacters.begin(), world.missionCharacters.end(),
            [leader](const auto& entry) { return entry.second.leader == leader; }));
        return store(1, followers);
    }
    if (command == 0x04F0 || command == 0x059C) {
        auto& crouch = player ? world.playerCrouch : character->crouch;
        if (command == 0x059C) {
            const auto animation = world.scriptedCharacterAnimations.find(lcsExtensionRuntimeCharacter(handle));
            const auto request = world.pedAnimationRequests.find(lcsExtensionRuntimeCharacter(handle));
            return condition((crouch.poseAnimation == 153 && crouch.poseFade.present) ||
                (animation != world.scriptedCharacterAnimations.end() && animation->second.animation == 153) ||
                (character && character->animationId == 153) ||
                (request != world.pedAnimationRequests.end() && request->second.animationId == 153));
        }
        if (!values[1].bits) {
            storylandClearCrouch(crouch, world.gameMilliseconds);
            crouch.requested = false;
            return 1;
        }
        if (values[2].bits == -1 && crouch.poseAnimation == 153 && crouch.poseFade.present && crouch.poseFade.rate >= 0) {
            crouch.requested = true;
            return 1;
        }
        const bool extending = crouch.active && values[2].bits != -1;
        if (!extending) {
            const float duration = world.animationDuration ? world.animationDuration(0, 155) : 0;
            if (!std::isfinite(duration) || duration <= 0 || duration >= 60) {
                error = "LCS crouch requires native animation association 155.";
                return -1;
            }
        }
        crouch.requested = true;
        const uint32_t deadline = values[2].bits == -1 ? UINT32_MAX : world.gameMilliseconds + uint32_t(values[2].bits);
        if (extending) crouch.deadline = std::max(crouch.deadline, deadline);
        else {
            crouch.active = true; crouch.deadline = deadline; crouch.poseAnimation = 155;
            crouch.startedAt = world.gameMilliseconds;
            crouch.poseFade = {0, 4.0f, world.gameMilliseconds, true};
        }
        return 1;
    }
    if (!character) { error = "This LCS behavior requires a non-player character."; return -1; }
    auto& behavior = character->behavior;
    if (command == 0x0438) { character->isChrisCriminal = values[1].bits != 0; return 1; }
    if (command == 0x0598) { behavior.frightenedWhenJacked = values[1].bits != 0; return 1; }
    if (command == 0x0512) return condition(behavior.leavingVehicleToDie);
    if (command == 0x0576) return condition(character->waitState == 12);
    if (command == 0x058B) return condition(behavior.attemptedAttractor);
    if (command == 0x0590) return condition(behavior.boughtIceCream);
    if (command == 0x0324 || command == 0x0325) {
        if (command == 0x0325 && values[1].bits != 0) {
            error = "LCS player group query requires player zero.";
            return -1;
        }
        const int32_t leader = command == 0x0325 ? 0 : lcsExtensionRuntimeCharacter(values[1].bits);
        return condition(character->leader == leader && !character->leaderOutOfRange);
    }
    if (command == 0x0128 || command == 0x0369 || command == 0x0485) {
        if (!world.scriptLineOfSightHit) { error = "LCS sight requires native collision."; return -1; }
        if (command == 0x0485) {
            struct Neighbour { float distance; int32_t type; float health; std::array<float, 3> position; };
            std::vector<Neighbour> neighbours;
            const auto add = [&](int32_t type, float health, const std::array<float, 3>& point) {
                const float distance = std::hypot(point[0] - character->x, point[1] - character->y, point[2] - character->z);
                if (distance < 40) neighbours.push_back({distance, type, health, point});
            };
            for (const auto& entry : world.missionCharacters)
                if (entry.first != handle)
                    add(entry.second.type, entry.second.health, {entry.second.x, entry.second.y, entry.second.z});
            if (world.allAmbientCharactersInArea)
                for (const auto& ped : world.allAmbientCharactersInArea({character->x - 40, character->y - 40,
                    character->z - 40, character->x + 40, character->y + 40, character->z + 40}))
                    add(ped.type, ped.health, ped.position);
            std::stable_sort(neighbours.begin(), neighbours.end(),
                [](const auto& left, const auto& right) { return left.distance < right.distance; });
            const size_t limit = std::min(size_t(10), neighbours.size());
            for (size_t index = 0; index < limit; ++index)
                if (neighbours[index].health <= 0 && neighbours[index].type == values[1].bits &&
                    lcsExtensionCanSee(world, *character, neighbours[index].position)) return condition(true);
            return condition(false);
        }
        if (command == 0x0128 && values[1].bits != 0) { error = "LCS sight player index must be zero."; return -1; }
        const int32_t target = command == 0x0128 ? 1 : values[1].bits;
        float x = 0, y = 0, z = 0, heading = 0;
        if (!vcsCharacterPosition(world, lcsExtensionRuntimeCharacter(target), x, y, z, heading)) {
            error = "LCS sight target does not exist.";
            return -1;
        }
        return condition(lcsExtensionCanSee(world, *character, {x, y, z}));
    }
    if (command == 0x0354) {
        if (character->creationSource != 2) return 1;
        world.missionCleanupCharacters.erase(handle);
        if (character->vehicle >= 0) storylandDeleteMissionCharacter(world, handle);
        else {
            character->creationSource = 1; character->persistent = false;
            character->removeFromWorld = true;
        }
        return 1;
    }
    if (command == 0x03FE) {
        const auto other = world.missionCharacters.find(values[1].bits);
        if (other == world.missionCharacters.end() || other->first == handle) {
            error = "LCS conversation requires two distinct non-player characters.";
            return -1;
        }
        const auto converse = [&](StorylandMissionEnvironment::Character& ped, int32_t partner) {
            ped.behavior.conversationPartner = partner;
            ped.behavior.conversationDeadline = world.gameMilliseconds + uint32_t(values[2].bits);
            ped.behavior.nextConversationSpeech = world.gameMilliseconds + 3000;
            ped.lookAtCharacter = partner; ped.walkingToTarget = false;
            ped.animationGroup = 0; ped.animationId = 14; ped.animationStartedAt = world.gameMilliseconds;
            ped.animationLoop = true;
        };
        converse(*character, other->first);
        converse(other->second, handle);
        return 1;
    }
    if (command == 0x04F8) {
        const auto car = world.missionVehicles.find(character->vehicle);
        if (car == world.missionVehicles.end() || car->second.deleted || character->passengerSeat != 0) return 1;
        if (car->second.driver >= 0 && car->second.driver != handle) {
            const auto driver = world.missionCharacters.find(car->second.driver);
            if (driver == world.missionCharacters.end() || driver->second.objective != 0x03E7) return 1;
        }
        StorylandMissionEnvironment::CharacterVehicleTask geometry;
        geometry.vehicle = car->first;
        geometry.seat = character->passengerSeat;
        geometry.exiting = true;
        if (!world.prepareCharacterVehicleTask || !world.prepareCharacterVehicleTask(handle, geometry, error)) {
            if (error.empty()) error = "LCS passenger slide requires native low-vehicle metadata.";
            return -1;
        }
        const int animation = geometry.low ? 96 : 95;
        const float duration = world.animationDuration ? world.animationDuration(0, animation) : 0;
        if (!std::isfinite(duration) || duration <= 0 || duration >= 60) {
            error = "LCS passenger slide requires native association 95 or 96.";
            return -1;
        }
        LcsExtensionTask task;
        task.objective = 104; task.target = car->first;
        task.actionStartedAt = world.gameMilliseconds; task.actionDuration = duration;
        lcsExtensionAssignTask(world, handle, task);
        character->animationGroup = 0; character->animationId = animation;
        character->animationStartedAt = world.gameMilliseconds; character->animationLoop = false;
        return 1;
    }
    if (command == 0x0515) {
        if (!finite(1, 5) || real(4) < 0 || !world.pedestrianLineClear || !world.groundHeightAt) {
            error = "LCS wander-path query requires finite geometry and native collision.";
            return -1;
        }
        const float dx = real(1) - character->x, dy = real(2) - character->y, dz = real(3) - character->z;
        const float length = std::hypot(dx, dy, dz);
        const float lowZ = std::min(character->z, real(3));
        if (std::fabs(dz) > real(4) || !world.pedestrianLineClear({character->x, character->y, lowZ},
            {real(1), real(2), lowZ})) return condition(false);
        const int samples = int(std::min(4.0f, std::ceil(length)));
        float previous = 0;
        if (!world.groundHeightAt({character->x, character->y, character->z}, previous) || previous < character->z - 5)
            return condition(false);
        previous += .5f;
        for (int index = 1; index < samples; ++index) {
            const float fraction = float(index) / std::max(length, .0001f);
            const float x = character->x + dx * fraction, y = character->y + dy * fraction;
            float height = 0, water = 0;
            if (!world.groundHeightAt({x, y, previous}, height) || std::fabs(height - previous) > 1 ||
                (world.waterHeightAt && world.waterHeightAt({x, y, height}, water) && height < water))
                return condition(false);
            previous = height + .5f;
        }
        return condition(true);
    }
    if (command == 0x0589) {
        if (behavior.iceCreamVehicle >= 0) {
            behavior.iceCreamVehicle = -1; behavior.iceCreamSlot = -1; behavior.iceCreamStartedAt = 0;
            character->objective = 0; character->walkingToTarget = false;
            world.lcsEntityExtension.tasks.erase(handle);
        }
        return 1;
    }
    if (command == 0x0585) {
        if (!writable(2)) { error = "LCS ice-cream assignment result is not writable."; return -1; }
        const auto car = world.missionVehicles.find(values[1].bits);
        if (car == world.missionVehicles.end() || car->second.deleted) { error = "LCS attractor vehicle does not exist."; return -1; }
        if (character->objective || behavior.attemptedAttractor || behavior.iceCreamVehicle >= 0 ||
            !character->alive || character->vehicle >= 0 || character->attached) return store(2, 0);
        std::array<float, 16> matrix{};
        if (!world.vehicleWorldMatrix || !world.vehicleWorldMatrix(car->first, matrix)) {
            error = "LCS attractor requires the live vehicle transform.";
            return -1;
        }
        const float dx = character->x - matrix[12], dy = character->y - matrix[13], dz = character->z - matrix[14];
        const int slot = (dx * matrix[0] + dy * matrix[1] + dz * matrix[2] > 0 ? 1 : 0) |
            (dx * matrix[4] + dy * matrix[5] + dz * matrix[6] > 0 ? 0 : 2);
        std::array<float, 3> position{};
        if (!storylandIceCreamPosition(world, car->first, slot, position) ||
            std::hypot(character->x - position[0], character->y - position[1], character->z - position[2]) >= 20)
            return store(2, 0);
        for (const auto& entry : world.missionCharacters)
            if (entry.second.behavior.iceCreamVehicle == car->first && entry.second.behavior.iceCreamSlot == slot)
                return store(2, 0);
        if (world.pedestrianLineClear && !world.pedestrianLineClear({character->x, character->y, character->z + 1},
            {position[0], position[1], position[2] + 1})) return store(2, 0);
        behavior.iceCreamVehicle = car->first; behavior.iceCreamSlot = slot;
        behavior.iceCreamStartedAt = 0; behavior.iceCreamDeadline = world.gameMilliseconds + 30000;
        LcsExtensionTask task; task.objective = 101; task.destination = position; task.radius = .2f;
        lcsExtensionAssignTask(world, handle, task);
        return store(2, 1);
    }
    if (command == 0x05F6) {
        const auto car = world.missionVehicles.find(values[1].bits);
        if (car == world.missionVehicles.end() || car->second.deleted) { error = "LCS evasive vehicle does not exist."; return -1; }
        if (!character->alive || character->vehicle >= 0 || character->attached ||
            (world.characterInControl && !world.characterInControl(handle))) return 1;
        const int animation = character->pedStats.flags & 0x10 ? 157 : 146;
        const float duration = world.animationDuration ? world.animationDuration(0, animation) : 0;
        if (!std::isfinite(duration) || duration <= 0 || duration >= 60) {
            error = "LCS evasive dive requires native association 146 or 157.";
            return -1;
        }
        auto velocity = car->second.velocity;
        if (world.vehicleWorldVelocity && !world.vehicleWorldVelocity(car->first, velocity)) {
            error = "LCS evasive vehicle has no live velocity.";
            return -1;
        }
        const float angle = std::atan2(-velocity[0], velocity[1]);
        const float side = world.nextNativeRandom() & 1 ? 1.0f : -1.0f;
        behavior.diveVelocity = animation == 157 ? std::array<float, 2>{} :
            std::array<float, 2>{side * std::cos(angle) * 3, side * std::sin(angle) * 3};
        behavior.diveDeadline = world.gameMilliseconds + uint32_t(duration * 1000);
        character->heading = angle * 57.2957795131f + side * 90;
        character->animationGroup = 0; character->animationId = animation;
        character->animationStartedAt = world.gameMilliseconds; character->animationLoop = false;
        character->followingScriptPath = false; character->walkingToTarget = false;
        return 1;
    }
    return 0;
}

int executeLcsExtensionGarageCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    int count = 0;
    switch (command) {
    case 0x0221: case 0x0222: case 0x032E: count = 1; break;
    case 0x0220: case 0x02FF: case 0x03D9: count = 2; break;
    default: return 0;
    }
    std::array<Operand, 2> values{};
    for (int index = 0; index < count; ++index)
        if (!readOperand(vm, values[size_t(index)])) { error = "LCS garage command has invalid operands."; return -1; }
    if (!vm.environment) { error = "LCS garage command requires the live world."; return -1; }
    auto& world = *vm.environment;
    if (command == 0x0222) { world.freeBombs = values[0].bits != 0; return 1; }
    const int32_t handle = int16_t(uint16_t(values[0].bits));
    const auto garage = std::find_if(world.garages.begin(), world.garages.end(),
        [handle](const auto& item) { return item.handle == handle; });
    if (garage == world.garages.end()) { error = "LCS garage handle does not exist."; return -1; }
    if (command == 0x0220) {
        if (values[1].bits < 0) { garage->targetVehicle = -1; return 1; }
        const auto vehicle = world.missionVehicles.find(values[1].bits);
        if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
            error = "LCS mission garage target vehicle does not exist.";
            return -1;
        }
        garage->targetVehicle = values[1].bits;
        if (garage->state == 5) garage->state = 0;
        return 1;
    }
    if (command == 0x02FF) {
        garage->type = uint8_t(values[1].bits);
        garage->state = 0;
        const auto door = world.lcsGarageDoors.find(handle);
        if (door != world.lcsGarageDoors.end()) door->second.state = 0;
        return 1;
    }
    bool result = false;
    if (command == 0x0221) result = garage->state == 5;
    else if (command == 0x032E) {
        result = garage->resprayDone;
        garage->resprayDone = false;
    } else {
        const int group = garage->type == 8 ? 0 : garage->type == 9 ? 1 : garage->type == 10 ? 2 :
            garage->type == 22 ? 3 : -1;
        const unsigned bit = (uint32_t(values[1].bits) - 1u) & 31u;
        result = group >= 0 && (world.importGarageMasks[size_t(group)] & (uint32_t(1) << bit)) != 0;
    }
    updateScmCondition(vm, result, inverted);
    return 1;
}
int executeLcsCollectiveAreaCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (vm.vcs || command < 0x028F || command > 0x0294) return 0;
    std::array<Operand,6> values{};
    for (auto& value : values) {
        if (!readOperand(vm, value)) {
            error = "LCS collective area query has a truncated or invalid operand.";
            return -1;
        }
    }
    if (!vm.environment) {
        error = "LCS collective area query requires the live world.";
        return -1;
    }
    auto& world = *vm.environment;
    std::array<float,2> minimum{}, maximum{};
    for (size_t axis = 0; axis < 2; ++axis) {
        const float first = operandFloat(values[axis+1]);
        const float second = operandFloat(values[axis+3]);
        if (!std::isfinite(first) || !std::isfinite(second)) {
            error = "LCS collective area bounds must be finite.";
            return -1;
        }
        minimum[axis] = std::min(first,second);
        maximum[axis] = std::max(first,second);
    }
    const unsigned movement = unsigned(command - 0x028F) % 3;
    const bool stopped = command >= 0x0292;
    bool matches = true;
    for (auto& member : world.lcsEntityExtension.collectives) {
        if (member.collective != values[0].bits) continue;
        LcsScriptEntityPosition position;
        if (!getLcsScriptEntityPosition(world,LcsScriptEntity::Character,member.character,position)) {
            member = {};
            continue;
        }
        if (stopped && !position.stopped) { matches = false; break; }
        for (size_t axis = 0; axis < 2; ++axis) {
            if (!(position.coordinates[axis] >= minimum[axis] &&
                  position.coordinates[axis] <= maximum[axis])) {
                matches = false;
                break;
            }
        }
        if (movement == 1 && position.inVehicle) matches = false;
        if (movement == 2 && !position.inVehicle) matches = false;
        if (!matches) break;
    }
    if (values[5].bits) {
        StorylandMissionEnvironment::LocateRegion region;
        for (size_t axis = 0; axis < 2; ++axis) {
            region.center[axis] = (minimum[axis] + maximum[axis]) * .5f;
            region.extent[axis] = (maximum[axis] - minimum[axis]) * .5f;
        }
        region.center[2] = 20.0f;
        if (world.groundHeightAt) {
            float ground = 0;
            if (world.groundHeightAt({region.center[0],region.center[1],1000},ground) && std::isfinite(ground))
                region.center[2] = ground;
        }
        region.threeDimensional = false;
        region.expiresAt = world.gameMilliseconds + 100;
        const auto existing = std::find_if(world.locateRegions.begin(),world.locateRegions.end(),
            [&](const auto& item) { return item.center == region.center && item.extent == region.extent &&
                item.threeDimensional == region.threeDimensional; });
        if (existing != world.locateRegions.end()) existing->expiresAt = region.expiresAt;
        else if (world.locateRegions.size() < 32) world.locateRegions.push_back(region);
    }
    updateScmCondition(vm,matches,inverted);
    return 1;
}

int executeLcsCollectivePlayerCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (vm.vcs || command < 0x028C || command > 0x028E) return 0;
    std::array<Operand,5> values{};
    for (auto& value : values) {
        if (!readOperand(vm, value)) {
            error = "LCS collective player query has a truncated or invalid operand.";
            return -1;
        }
    }
    if (!vm.environment) {
        error = "LCS collective player query requires the live world.";
        return -1;
    }
    auto& world = *vm.environment;
    LcsScriptEntityPosition player;
    if (!getLcsScriptEntityPosition(world,LcsScriptEntity::Player,values[1].bits,player)) {
        error = "LCS collective player query requires player index zero.";
        return -1;
    }
    const float radiusX = operandFloat(values[2]);
    const float radiusY = operandFloat(values[3]);
    if (!std::isfinite(radiusX) || !std::isfinite(radiusY) ||
        !std::isfinite(player.coordinates[0]) || !std::isfinite(player.coordinates[1])) {
        error = "LCS collective player query requires finite coordinates and extents.";
        return -1;
    }
    bool matches = true;
    for (auto& member : world.lcsEntityExtension.collectives) {
        if (member.collective != values[0].bits) continue;
        LcsScriptEntityPosition position;
        if (!getLcsScriptEntityPosition(world,LcsScriptEntity::Character,member.character,position)) {
            member = {};
            continue;
        }
        if (!(position.coordinates[0] >= player.coordinates[0] - radiusX &&
              position.coordinates[0] <= player.coordinates[0] + radiusX &&
              position.coordinates[1] >= player.coordinates[1] - radiusY &&
              position.coordinates[1] <= player.coordinates[1] + radiusY) ||
            (command == 0x028D && position.inVehicle) ||
            (command == 0x028E && !position.inVehicle)) {
            matches = false;
            break;
        }
    }
    if (values[4].bits) {
        StorylandMissionEnvironment::LocateRegion region;
        region.center = player.coordinates;
        region.extent = {radiusX,radiusY,0};
        region.expiresAt = world.gameMilliseconds + 100;
        const auto existing = std::find_if(world.locateRegions.begin(),world.locateRegions.end(),
            [&](const auto& item) { return item.center == region.center && item.extent == region.extent &&
                !item.threeDimensional; });
        if (existing != world.locateRegions.end()) existing->expiresAt = region.expiresAt;
        else if (world.locateRegions.size() < 32) world.locateRegions.push_back(region);
    }
    updateScmCondition(vm,matches,inverted);
    return 1;
}

int executeLcsEntityExtensionCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (vm.vcs) return 0;
    if (command == 0x0295) {
        Operand collective, output;
        if (!readOperand(vm, collective) || !readOperand(vm, output) || !output.reference) {
            error = "LCS collective count requires a collective and writable output.";
            return -1;
        }
        if (!vm.environment) {
            error = "LCS collective count requires the live world.";
            return -1;
        }
        // The native command prunes missing pool handles before counting.
        const auto members = lcsExtensionCollectiveMembers(*vm.environment, collective.bits);
        if (!writeOperand(vm, output, int32_t(members.size()))) {
            error = "LCS collective count output is not writable.";
            return -1;
        }
        return 1;
    }
    int result = executeLcsCollectivePlayerCommand(vm, command, inverted, error);
    if (result) return result;
    result = executeLcsCollectiveAreaCommand(vm, command, inverted, error);
    if (result) return result;
    result = executeLcsExtensionObjectiveCommand(vm, command, inverted, error);
    if (result) return result;
    result = executeLcsExtensionPopulationCommand(vm, command, error);
    if (result) return result;
    result = executeLcsExtensionCharacterCommand(vm, command, inverted, error);
    if (result) return result;
    return executeLcsExtensionGarageCommand(vm, command, inverted, error);
}

void lcsExtensionFinishTask(StorylandMissionEnvironment& world, int32_t handle,
    StorylandMissionEnvironment::Character& character, bool completed) {
    character.objective = 0; character.objectiveTarget = -1;
    character.objectiveCompleted = completed; character.walkingToTarget = false;
    character.followingScriptPath = false; character.attackStartedAt = 0;
    character.seekRoute.clear(); character.seekRouteCursor = 0;
    world.characterTargets.erase(handle);
    world.lcsEntityExtension.tasks.erase(handle);
}

int32_t lcsExtensionNearestCar(StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::Character& character) {
    float nearest = 30.0f * float(world.lcsEnterCarRangeMultiplier);
    int32_t result = -1;
    for (const auto& entry : world.missionVehicles) {
        const auto& car = entry.second;
        if (!car.alive || car.deleted || car.persistent || std::fabs(car.speed) > .1f ||
            (car.doorLock != 0 && car.doorLock != 1)) continue;
        const float distance = std::hypot(car.x - character.x, car.y - character.y, car.z - character.z);
        if (distance < nearest) { result = entry.first; nearest = distance; }
    }
    if (world.findVehicleToSteal && nearest > 0) {
        const int32_t ambient = world.findVehicleToSteal({character.x, character.y, character.z}, nearest);
        if (ambient >= 0) result = ambient;
    }
    return result;
}

void lcsExtensionAdvanceLeader(StorylandMissionEnvironment& world, int32_t handle,
    StorylandMissionEnvironment::Character& character) {
    if (character.leader < 0) return;
    float x = 0, y = 0, z = 0, heading = 0;
    if (!vcsCharacterPosition(world, character.leader, x, y, z, heading)) {
        storylandClearVcsLeader(world, handle, character);
        world.lcsEntityExtension.tasks.erase(handle);
        return;
    }
    const bool player = character.leader == 0;
    const auto leader = world.missionCharacters.find(character.leader);
    const int32_t vehicle = player ? (world.inVehicle ? world.occupiedMissionVehicle : -1) : leader->second.vehicle;
    const bool leaderExiting = player ? world.playerExitVehicleRequested :
        world.characterVehicleTasks.count(leader->first) && world.characterVehicleTasks.at(leader->first).exiting;
    const float distance = std::hypot(x - character.x, y - character.y, z - character.z);
    if (character.leaderUpdateAfter >= world.gameMilliseconds && distance <= 15) return;
    const bool previouslyOutOfRange = character.leaderOutOfRange;
    character.leaderOutOfRange = distance > 30;
    if (character.leaderOutOfRange && previouslyOutOfRange && character.creationSource == 2 &&
        character.vehicle < 0 && !character.attached) {
        lcsExtensionFinishTask(world, handle, character, false);
        return;
    }
    if (world.characterVehicleTasks.count(handle) || character.attached) return;
    LcsExtensionTask task;
    if (character.vehicle >= 0) {
        if (vehicle >= 0 && !leaderExiting) return;
        if (character.objective == 0x03E7) return;
        task.objective = 16; task.target = character.vehicle;
        character.leaderVehicleAfter = world.gameMilliseconds + 250u +
            uint32_t((uint64_t(world.nextNativeRandom()) * 1250u) >> 31);
    } else if (vehicle >= 0 && !leaderExiting) {
        if (!world.vehiclePassengerCapacity || world.vehiclePassengerCapacity(vehicle) <= 0) return;
        task.objective = 17; task.target = vehicle; task.speed = 3.8f;
    } else {
        const bool attacking = (world.characterShooting && world.characterShooting(handle)) ||
            (world.characterFighting && world.characterFighting(handle));
        if (attacking && !(player && world.playerCrouch.movementState >= 2 && distance > 15)) return;
        task.objective = 102; task.target = player ? 1 : character.leader;
    }
    if (character.objective != lcsExtensionScriptObjective(task.objective) || character.objectiveTarget != task.target)
        lcsExtensionAssignTask(world, handle, task);
}

void storylandAdvanceLcsEntityExtension(StorylandMissionEnvironment& world, uint32_t elapsedMilliseconds) {
    auto& extension = world.lcsEntityExtension;
    for (auto& row : extension.collectives)
        if (row.collective >= 0 && !world.missionCharacters.count(row.character)) row = {};
    for (auto entry = extension.tasks.begin(); entry != extension.tasks.end();) {
        if (entry->first == 1 && entry->second.objective == 13) {
            if ((!world.playerWalkTask.active && !world.playerWalkTask.completed) ||
                !world.missionCharacters.count(entry->second.target) || world.health <= 0) {
                world.playerWalkTask.clear();
                world.playerNavigation.clearRoute();
                entry = extension.tasks.erase(entry);
            } else ++entry;
        } else if (!world.missionCharacters.count(entry->first)) entry = extension.tasks.erase(entry);
        else ++entry;
    }
    for (auto entry = extension.runningOverrides.begin(); entry != extension.runningOverrides.end();)
        if (entry->first != 1 && !world.missionCharacters.count(entry->first))
            entry = extension.runningOverrides.erase(entry);
        else ++entry;
    std::vector<int32_t> removed;
    for (const auto& entry : world.missionCharacters)
        if (!entry.second.networkControlled && entry.second.removeFromWorld) removed.push_back(entry.first);
    for (int32_t handle : removed) storylandDeleteMissionCharacter(world, handle);
    const float seconds = float(std::min(elapsedMilliseconds, 100u)) * .001f;
    const int playerWeapon = world.characterCrouchWeaponAnimation ? world.characterCrouchWeaponAnimation(0) : 155;
    storylandAdvanceCrouch(world.playerCrouch, world.gameMilliseconds, world.health > 0, playerWeapon);
    for (auto& entry : world.missionCharacters) {
        const int32_t handle = entry.first;
        auto& character = entry.second;
        if (character.networkControlled) continue;
        const int weaponAnimation = world.characterCrouchWeaponAnimation ? world.characterCrouchWeaponAnimation(handle) : 155;
        storylandAdvanceCrouch(character.crouch, world.gameMilliseconds,
            character.alive && character.health > 0, weaponAnimation);
        if (!character.alive || (character.health <= 0 && !character.behavior.leavingVehicleToDie)) continue;
        if (world.characterReactionOwned && world.characterReactionOwned(handle)) continue;
        if (character.behavior.leavingVehicleToDie && !world.characterVehicleTasks.count(handle)) {
            const auto car = world.missionVehicles.find(character.vehicle);
            if (character.vehicle < 0 || car == world.missionVehicles.end() || car->second.deleted) {
                character.behavior.leavingVehicleToDie = false;
                character.health = 0; character.alive = false;
                lcsExtensionFinishTask(world, handle, character, false);
                continue;
            }
            LcsExtensionTask task; task.objective = 16; task.target = character.vehicle;
            lcsExtensionAssignTask(world, handle, task);
        }
        if (character.health > 0) lcsExtensionAdvanceLeader(world, handle, character);
        auto found = extension.tasks.find(handle);
        if (found != extension.tasks.end() && !found->second.waitingForExit &&
            character.objective != lcsExtensionScriptObjective(found->second.objective) &&
            !world.characterVehicleTasks.count(handle)) {
            extension.tasks.erase(found);
            found = extension.tasks.end();
        }
        if (character.behavior.diveDeadline && int32_t(world.gameMilliseconds - character.behavior.diveDeadline) >= 0 &&
            character.animationId == 157) character.animationId = -1;
        if (character.health > 0) {
            const int savedObjective = character.objective, savedTarget = character.objectiveTarget;
            character.objective = found == extension.tasks.end() ? 0 : lcsExtensionHostObjective(found->second.objective);
            if (found != extension.tasks.end() && (found->second.objective == 8 || found->second.objective == 9 ||
                found->second.objective == 10 || found->second.objective == 11 || found->second.objective == 12 ||
                found->second.objective == 15 || found->second.objective == 46 || found->second.objective == 102))
                character.objectiveTarget = lcsExtensionRuntimeCharacter(savedTarget);
            const int hostBefore = character.objective;
            const bool held = storylandAdvanceCharacterBehavior(world, handle, character, seconds);
            const int hostAfter = character.objective, targetAfter = character.objectiveTarget;
            character.objective = savedObjective; character.objectiveTarget = savedTarget;
            if (found != extension.tasks.end() && hostAfter != hostBefore) {
                if (hostAfter == 0) {
                    lcsExtensionFinishTask(world, handle, character, character.objectiveCompleted);
                    continue;
                }
                if (hostAfter == 0x00E9 && found->second.objective == 32) {
                    LcsExtensionTask task; task.objective = 17; task.target = targetAfter;
                    lcsExtensionAssignTask(world, handle, task);
                    found = extension.tasks.find(handle);
                }
            }
            if (held) continue;
        }
        if (found == extension.tasks.end()) {
            const auto running = extension.runningOverrides.find(handle);
            if (running != extension.runningOverrides.end() && running->second && character.walkingToTarget)
                character.walkSpeed = std::max(character.walkSpeed, 3.8f);
            continue;
        }
        auto& task = found->second;
        if (world.characterVehicleTasks.count(handle)) continue;
        if (task.waitingForExit) {
            task.waitingForExit = false;
            character.objective = lcsExtensionScriptObjective(task.objective);
            character.objectiveTarget = task.target; character.objectiveCompleted = false;
        }
        if (task.objective == 104) {
            const auto car = world.missionVehicles.find(task.target);
            if (car == world.missionVehicles.end() || car->second.deleted || character.vehicle != task.target) {
                lcsExtensionFinishTask(world, handle, character, false); continue;
            }
            if (float(world.gameMilliseconds - task.actionStartedAt) * .001f < task.actionDuration) continue;
            if (car->second.driver >= 0 && car->second.driver != handle) continue;
            car->second.driver = handle; car->second.scriptStatus = 3;
            character.passengerSeat = -1; character.animationId = -1;
            lcsExtensionFinishTask(world, handle, character, true);
            continue;
        }
        if (task.objective == 35) {
            if (character.vehicle >= 0) { lcsExtensionFinishTask(world, handle, character, true); continue; }
            if (task.target < 0) task.target = lcsExtensionNearestCar(world, character);
            if (task.target < 0) { lcsExtensionFinishTask(world, handle, character, false); continue; }
        }
        if (task.objective == 16 || task.objective == 17 || task.objective == 18 || task.objective == 35) {
            const bool exiting = task.objective == 16;
            if ((exiting && character.vehicle < 0) || (!exiting && character.vehicle == task.target)) {
                lcsExtensionFinishTask(world, handle, character, true); continue;
            }
            if (exiting && character.leaderVehicleAfter &&
                int32_t(world.gameMilliseconds - character.leaderVehicleAfter) <= 0) continue;
            const int32_t vehicleHandle = exiting ? character.vehicle : task.target;
            const auto car = world.missionVehicles.find(vehicleHandle);
            if (car == world.missionVehicles.end() || car->second.deleted || !car->second.alive) {
                if (character.behavior.leavingVehicleToDie) {
                    character.behavior.leavingVehicleToDie = false; character.health = 0; character.alive = false;
                }
                lcsExtensionFinishTask(world, handle, character, false); continue;
            }
            int seat = exiting ? character.passengerSeat : -1;
            if (task.objective == 17) {
                const int capacity = world.vehiclePassengerCapacity ? world.vehiclePassengerCapacity(vehicleHandle) : 0;
                const bool frontSeatOnly = world.lcsExtendedWorld.frontSeatOnlyCharacters.count(handle) != 0;
                seat = 0;
                while (seat < capacity && ((!character.allowFrontPassenger && seat == 0) ||
                    (frontSeatOnly && seat != 0) || lcsPassengerSeatOccupied(world, handle, vehicleHandle, seat) ||
                    (world.inVehicle && world.occupiedMissionVehicle == vehicleHandle && world.playerPassengerSeat == seat))) ++seat;
                if (seat >= capacity) { character.walkingToTarget = false; continue; }
            }
            if (!exiting && seat < 0 && car->second.driver < 0 && world.acquireVehicleDriver)
                world.acquireVehicleDriver(vehicleHandle);
            const bool hijacking = !exiting && seat < 0 &&
                (car->second.driver >= 0 || (world.inVehicle && world.occupiedMissionVehicle == vehicleHandle &&
                    world.playerPassengerSeat < 0));
            std::string error;
            if (!lcsExtensionQueueVehicleTask(world, handle, vehicleHandle, seat, exiting, hijacking, error) &&
                character.behavior.leavingVehicleToDie) {
                character.behavior.leavingVehicleToDie = false; character.health = 0; character.alive = false;
                lcsExtensionFinishTask(world, handle, character, false);
            }
            continue;
        }
        if (character.vehicle >= 0 && task.objective != 6 && task.objective != 9 && task.objective != 32) {
            std::string error;
            if (lcsExtensionQueueVehicleTask(world, handle, character.vehicle, character.passengerSeat, true, false, error))
                task.waitingForExit = true;
            continue;
        }
        if (task.objective == 9) {
            float x = 0, y = 0, z = 0, heading = 0;
            if (!vcsCharacterPosition(world, lcsExtensionRuntimeCharacter(task.target), x, y, z, heading)) {
                lcsExtensionFinishTask(world, handle, character, true); continue;
            }
            const float distance = std::hypot(x - character.x, y - character.y, z - character.z);
            if (character.vehicle >= 0) {
                const auto car = world.missionVehicles.find(character.vehicle);
                if (car != world.missionVehicles.end() && car->second.driver == handle && distance > 10) {
                    car->second.targetX = x; car->second.targetY = y; car->second.targetZ = z;
                    car->second.drivingToTarget = true; car->second.cruiseSpeed = 12;
                } else {
                    if (car != world.missionVehicles.end()) { car->second.drivingToTarget = false; car->second.speed = 0; }
                    std::string error;
                    if (lcsExtensionQueueVehicleTask(world, handle, character.vehicle, character.passengerSeat, true, false, error))
                        task.waitingForExit = true;
                }
                continue;
            }
            if (distance > 30 && int32_t(world.gameMilliseconds - task.actionStartedAt) >= 5000) {
                task.actionStartedAt = world.gameMilliseconds;
                const int32_t car = lcsExtensionNearestCar(world, character);
                if (car >= 0) {
                    std::string error;
                    if (lcsExtensionQueueVehicleTask(world, handle, car, -1, false,
                        world.missionVehicles.at(car).driver >= 0, error)) {
                        task.waitingForExit = true;
                        character.objective = 0x01DA; character.objectiveTarget = car;
                        continue;
                    }
                }
            }
        }
        if (task.objective == 3 || task.objective == 10 || task.objective == 11 || task.objective == 41) {
            float x = task.destination[0], y = task.destination[1], z = task.destination[2], heading = 0;
            bool targetExists = true;
            if (task.objective == 41) {
                const auto car = world.missionVehicles.find(task.target);
                targetExists = car != world.missionVehicles.end() && !car->second.deleted;
                if (targetExists) { x = car->second.x; y = car->second.y; z = car->second.z; }
            } else if (task.objective != 3)
                targetExists = vcsCharacterPosition(world, lcsExtensionRuntimeCharacter(task.target), x, y, z, heading);
            if (!targetExists) { lcsExtensionFinishTask(world, handle, character, false); continue; }
            float dx = character.x - x, dy = character.y - y;
            const float distance = std::hypot(dx, dy);
            if (task.actionDuration > 0 && float(world.gameMilliseconds - task.actionStartedAt) * .001f > task.actionDuration) {
                if (task.objective == 3 || distance >= 30) {
                    lcsExtensionFinishTask(world, handle, character, true); continue;
                }
                task.actionStartedAt = world.gameMilliseconds; task.actionDuration = 5;
            }
            if (distance < .001f) {
                const float angle = character.heading * .0174532925199433f;
                dx = -std::sin(angle); dy = std::cos(angle);
            }
            const float scale = 10.0f / std::max(.001f, std::hypot(dx, dy));
            character.targetX = character.x + dx * scale; character.targetY = character.y + dy * scale;
            character.objectiveHeight = character.z; character.walkSpeed = 3.8f;
            character.walkingToTarget = !character.stayInSamePlace;
        } else if (task.objective == 15) {
            float x = 0, y = 0, z = 0, heading = 0;
            if (!vcsCharacterPosition(world, lcsExtensionRuntimeCharacter(task.target), x, y, z, heading)) {
                lcsExtensionFinishTask(world, handle, character, false); continue;
            }
            static constexpr std::array<std::array<int, 2>, 9> formation{{
                {0, 0}, {0, -1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}, {-1, 0}, {1, 0}, {0, 1}}};
            const float spacing = 1.75f * (1.0f + float(world.nextNativeRandom()) / 2147483648.0f * .25f);
            const auto offset = formation[size_t(std::clamp(character.formation, 0, 8))];
            const float angle = heading * .0174532925199433f;
            character.targetX = x + spacing * (float(offset[0]) * std::cos(angle) - float(offset[1]) * std::sin(angle));
            character.targetY = y + spacing * (float(offset[0]) * std::sin(angle) + float(offset[1]) * std::cos(angle));
            character.objectiveHeight = z;
            const float distance = std::hypot(character.targetX - character.x, character.targetY - character.y);
            character.walkingToTarget = distance > 1 && !character.stayInSamePlace;
            character.walkSpeed = distance > 3 ? 3.8f : 1.25f;
        } else if (task.objective != 32 && task.objective != 101) {
            const int scriptObjective = character.objective, scriptTarget = character.objectiveTarget;
            character.objective = task.objective == 103 ? 0 : lcsExtensionHostObjective(task.objective);
            if (task.objective == 103) character.followingScriptPath = true;
            if (task.objective == 4 || task.objective == 24 || task.objective == 25 || task.objective == 103) {
                character.targetX = task.destination[0]; character.targetY = task.destination[1];
                character.objectiveHeight = task.destination[2];
            }
            if (task.objective == 8 || task.objective == 9 || task.objective == 12 || task.objective == 46 || task.objective == 102)
                character.objectiveTarget = lcsExtensionRuntimeCharacter(task.target);
            storylandAdvanceScriptCharacterObjective(world, handle, character);
            const bool finished = character.objective == 0 && (task.objective != 103 || !character.followingScriptPath);
            character.objective = scriptObjective; character.objectiveTarget = scriptTarget;
            task.routeCursor = character.routeCursor; task.routeDirection = character.routeDirection;
            if (finished) { lcsExtensionFinishTask(world, handle, character, character.objectiveCompleted); continue; }
        }
        const auto running = extension.runningOverrides.find(handle);
        if (running != extension.runningOverrides.end() && running->second && character.walkingToTarget)
            character.walkSpeed = std::max(character.walkSpeed, 3.8f);
        const int movement = character.walkingToTarget ? (character.walkSpeed > 4 ? 5 : character.walkSpeed > 2 ? 4 : 2) : 1;
        character.movementState = uint8_t(movement);
        storylandObserveCrouchMovement(character.crouch, world.gameMilliseconds, movement, weaponAnimation);
        if (character.walkingToTarget && character.collisionEnabled && character.vehicle < 0 && !character.attached) {
            float x = character.targetX, y = character.targetY;
            const int scriptObjective = character.objective;
            character.objective = task.objective == 103 ? 0 : lcsExtensionHostObjective(task.objective);
            if (!storylandSeekScriptPedNode(world, handle, character, x, y) ||
                !storylandSeekLocalPedAvoidance(world, character, x, y)) {
                character.walkingToTarget = false;
                storylandWaitForScriptPedObstruction(world, handle, character);
            } else { character.targetX = x; character.targetY = y; }
            character.objective = scriptObjective;
        }
    }
}

// LCS PS2 collective-to-character location commands (0x2FF7A0).
int executeLcsCollectiveCharacterQuery(StorylandMissionVm& vm,uint16_t command,
    bool inverted,std::string& error) {
    if(vm.vcs||command<0x0286||command>0x028B)return 0;
    const bool vehicleTarget=command>=0x0289;
    constexpr size_t count=5;
    std::array<Operand,6> values{};
    for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="LCS collective location query has truncated operands.";return -1;
    }
    if(!vm.environment) {error="LCS collective location query requires a live world.";return -1;}
    auto& world=*vm.environment;
    LcsScriptEntityPosition target;
    if(!getLcsScriptEntityPosition(world,vehicleTarget?LcsScriptEntity::Vehicle:LcsScriptEntity::Character,values[1].bits,target)) {
        error="LCS collective location query references an unknown target character.";return -1;
    }
    const std::array<float,3> extent{operandFloat(values[2]),operandFloat(values[3]),
        0.0f};
    if(!std::all_of(extent.begin(),extent.end(),[](float value){return std::isfinite(value);})||
        !std::all_of(target.coordinates.begin(),target.coordinates.end(),[](float value){return std::isfinite(value);})) {
        error="LCS collective location query requires finite coordinates.";return -1;
    }
    const auto members=lcsExtensionCollectiveMembers(world,values[0].bits);
    const unsigned mode=(command-0x0286)%3;
    bool allInside=true;
    for(int32_t member:members) {
        LcsScriptEntityPosition position;
        if(!getLcsScriptEntityPosition(world,LcsScriptEntity::Character,member,position))continue;
        const bool near=position.coordinates[0]>=target.coordinates[0]-extent[0] &&
            position.coordinates[0]<=target.coordinates[0]+extent[0] &&
            position.coordinates[1]>=target.coordinates[1]-extent[1] &&
            position.coordinates[1]<=target.coordinates[1]+extent[1];
        if(!near||(mode==1&&position.inVehicle)||(mode==2&&!position.inVehicle)) {
            allInside=false;break;
        }
    }
    if(values[count-1].bits) {
        StorylandMissionEnvironment::LocateRegion region;
        region.center=target.coordinates;region.extent=extent;region.threeDimensional=false;
        region.expiresAt=world.gameMilliseconds+100;
        const auto existing=std::find_if(world.locateRegions.begin(),world.locateRegions.end(),
            [&](const auto& item){return item.center==region.center&&item.extent==region.extent&&
                item.threeDimensional==region.threeDimensional;});
        if(existing!=world.locateRegions.end())existing->expiresAt=region.expiresAt;
        else if(world.locateRegions.size()<32)world.locateRegions.push_back(region);
    }
    updateScmCondition(vm,allInside,inverted);return 1;
}

int executeLcsBonusPickupCommand(StorylandMissionVm& vm, uint16_t command,
    std::string& error) {
    if (vm.vcs) return 0;
    const bool revenue = command == 0x04AB;
    const bool locked = command == 0x051C;
    const bool forSale = command == 0x051D;
    const bool clothes = command == 0x0560;
    const bool propertyOwned = command == 0x0547;
    const bool costume = command == 0x065D;
    if (!revenue && !locked && !forSale && !clothes && !propertyOwned && !costume) return 0;
    if (!vm.environment) {
        error = "LCS bonus commands require the live pickup and reward state.";
        return -1;
    }
    auto& world = *vm.environment;
    if (propertyOwned || costume) {
        Operand value;
        if (!readOperand(vm, value)) {
            error = "LCS reward command has a truncated index.";
            return -1;
        }
        if (costume) {
            const uint8_t index = uint8_t(value.bits);
            if (index >= 16) return 1;
            uint16_t previous = 0;
            const auto existing = world.statistics.find("UnlockedCostumes");
            const auto legacy = world.statistics.find("CostumeUnlockMask");
            const auto found = existing != world.statistics.end() ? existing : legacy;
            if (found != world.statistics.end()) {
                if (!std::isfinite(found->second) || found->second < 0 || found->second > UINT16_MAX) {
                    error = "Unlocked costume state is outside the native sixteen-bit mask.";
                    return -1;
                }
                previous = uint16_t(found->second);
            }
            world.statistics["UnlockedCostumes"] = uint16_t(previous | uint16_t(1u << index));
            world.statistics.erase("CostumeUnlockMask");
            return 1;
        }
        if (value.bits < 0 || value.bits >= 15) {
            error = "LCS property ownership index is outside the native fifteen-property table.";
            return -1;
        }
        const std::string key = "PropertyOwned[" + std::to_string(value.bits) + "]";
        const auto found = world.statistics.find(key);
        if (found != world.statistics.end() && found->second != 0) return 1;
        const auto count = world.statistics.find("NumPropertyOwned");
        const double previous = count == world.statistics.end() ? 0 : count->second;
        if (!std::isfinite(previous) || previous < 0 || previous > INT32_MAX - 1) {
            error = "The owned-property statistic is outside its native integer range.";
            return -1;
        }
        world.statistics[key] = 1;
        world.statistics["NumPropertyOwned"] = int32_t(previous) + 1;
        return 1;
    }

    std::array<Operand,5> values{};
    const size_t inputs = revenue ? 5 : locked ? 3 : 4;
    for (size_t index = 0; index < inputs; ++index) {
        if (!readOperand(vm, values[index])) {
            error = "LCS bonus pickup creation has a truncated operand.";
            return -1;
        }
    }
    StorylandLcsPickupDetails details;
    if ((locked || forSale) && !readScmText(vm, details.textKey)) {
        error = "LCS property pickup requires its original eight-character GXT label.";
        return -1;
    }
    Operand output;
    if (!readOperand(vm, output) || !output.reference) {
        error = "LCS bonus pickup creation requires a writable pickup handle.";
        return -1;
    }
    const float x = operandFloat(values[0]);
    const float y = operandFloat(values[1]);
    float z = operandFloat(values[2]);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
        error = "LCS bonus pickup coordinates must be finite.";
        return -1;
    }
    if (z <= -100.0f) {
        float ground = 0;
        if (!world.groundHeightAt || !world.groundHeightAt({x,y,z}, ground) || !std::isfinite(ground)) {
            error = "LCS bonus pickup ground sentinel requires native ground collision.";
            return -1;
        }
        z = roundEeFloat(double(ground) + 0.5);
    }
    if ((revenue || forSale) && values[3].bits < 0) {
        error = "LCS protection revenue and property prices cannot be negative.";
        return -1;
    }
    const std::string modelName = revenue ? "bigdollar" : locked ? "property_locked" :
        forSale ? "property_fsale" : "clothesp";
    const int32_t model = world.modelIdByName ? world.modelIdByName(modelName) : -1;
    if (model < 0) {
        error = "The original LCS pickup model is unavailable: " + modelName;
        return -1;
    }
    if (world.nextPickupHandle <= 0 || world.nextPickupHandle == INT32_MAX) {
        error = "LCS pickup handle space is exhausted.";
        return -1;
    }
    const size_t normalSlots = std::min(size_t(320), world.pickups.size());
    size_t slot = normalSlots;
    for (size_t index = 0; index < normalSlots; ++index) {
        const auto& pickup = world.pickups[index];
        if (pickup.removed || pickup.type == 0 || (pickup.unavailable && !pickup.respawnAt)) {
            slot = index;
            break;
        }
    }
    if (slot == normalSlots && normalSlots == 320) {
        for (const int32_t disposable : {8, 4}) {
            for (size_t index = 0; index < normalSlots; ++index) {
                if (world.pickups[index].type == disposable ||
                    (disposable == 4 && world.pickups[index].type == 5)) {
                    slot = index;
                    break;
                }
            }
            if (slot < normalSlots) break;
        }
        if (slot == normalSlots) {
            if (!writeOperand(vm, output, -1)) {
                error = "LCS pickup creation could not write the exhausted-pool result.";
                return -1;
            }
            return 1;
        }
    }
    StorylandMissionEnvironment::Pickup pickup;
    pickup.handle = world.nextPickupHandle;
    pickup.model = model;
    pickup.type = revenue ? 16 : locked ? 17 : forSale ? 18 : 2;
    pickup.x = x;
    pickup.y = y;
    pickup.z = z;
    pickup.area = world.area;
    pickup.ammunition = locked ? 0 : values[3].bits;
    details.revenueRate = revenue ? uint16_t(values[4].bits) : 0;
    details.updatedAt = world.gameMilliseconds;
    details.missionText = world.missionText;
    if (!details.valid()) {
        error = "LCS pickup metadata is outside the native text bounds.";
        return -1;
    }
    ++world.nextPickupHandle;
    const int32_t handle = pickup.handle;
    if (slot == world.pickups.size()) world.pickups.push_back(std::move(pickup));
    else {
        world.lcsPickupDetails.erase(world.pickups[slot].handle);
        world.pickups[slot] = std::move(pickup);
    }
    if (!clothes) world.lcsPickupDetails.emplace(handle, std::move(details));
    world.requestedModels.insert(model);
    if (!writeOperand(vm, output, handle)) {
        error = "LCS bonus pickup creation could not write its handle.";
        return -1;
    }
    return 1;
}
