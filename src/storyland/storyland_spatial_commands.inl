int executeSpatialCommand(StorylandMissionVm& vm, uint16_t command, bool inverted, std::string& error) {
    const auto definition = vm.opcodeDefinitions.find(command);
    if (definition == vm.opcodeDefinitions.end()) return 0;
    std::istringstream words(definition->second.second);
    std::string name;
    words >> name;
    const bool locate = name.rfind("locate_", 0) == 0;
    const bool area = name.rfind("is_", 0) == 0 && name.find("_in_area_") != std::string::npos;
    if (!locate && !area) return 0;
    const bool three = name.size() >= 3 && name.compare(name.size() - 3, 3, "_3d") == 0;
    const bool two = name.size() >= 3 && name.compare(name.size() - 3, 3, "_2d") == 0;
    if (!three && !two) return 0;
    std::string subject = name.substr(locate ? 7 : 3);
    if (subject.rfind("stopped_", 0) == 0) subject.erase(0, 8);
    subject = subject.substr(0, subject.find('_'));
    if (subject != "player" && subject != "char" && subject != "car" && subject != "object") return 0;
    if (!vm.vcs && command == 0x00FA) subject = "player";
    std::string target;
    if (locate) {
        const std::string prefix = name.substr(0, name.size() - 3);
        for (const char* kind : {"char", "car", "object"}) {
            const std::string suffix = std::string("_") + kind;
            if (prefix.size() >= suffix.size() && prefix.compare(prefix.size() - suffix.size(), suffix.size(), suffix) == 0 &&
                prefix != "locate_" + std::string(kind) && prefix != "locate_stopped_" + std::string(kind)) target = kind;
        }
    }
    const size_t dimensions = three ? 3 : 2;
    const size_t expected = target.empty() ? dimensions * 2 + 2 : dimensions + 3;
    if (definition->second.first != int(expected)) return 0;
    if (!vm.environment) { error = "Spatial query requires a live mission environment."; return -1; }
    std::array<Operand, 8> values{};
    for (size_t index = 0; index < expected; ++index) {
        if (!readOperand(vm, values[index])) { error = "Spatial query has a truncated operand."; return -1; }
    }
    auto& world = *vm.environment;
    struct EntityPosition {
        std::array<float, 3> point{};
        float speed = 0;
        bool inVehicle = false;
    };
    const auto vehiclePosition = [&](int32_t handle, EntityPosition& result) {
        const auto car = world.missionVehicles.find(handle);
        if (car == world.missionVehicles.end() || car->second.deleted) return false;
        result.point = {car->second.x, car->second.y, car->second.z};
        result.speed = car->second.speed;
        result.inVehicle = true;
        return true;
    };
    const auto resolve = [&](const std::string& kind, int32_t handle, EntityPosition& result) {
        if (kind == "player" || (kind == "char" && handle == (vm.vcs ? 0 : 1))) {
            if (kind == "player" && handle != 0) return false;
            result.point = {world.x, world.y, world.z};
            result.speed = world.speed;
            result.inVehicle = world.inVehicle;
            return true;
        }
        if (kind == "car") return vehiclePosition(handle, result);
        if (kind == "char") {
            const auto character = world.missionCharacters.find(handle);
            if (character == world.missionCharacters.end()) return false;
            const auto& ped = character->second;
            if (ped.vehicle >= 0) return vehiclePosition(ped.vehicle, result);
            result.point = {ped.x, ped.y, ped.z};
            result.speed = ped.movementSpeed;
            return true;
        }
        for (const auto& object : world.objects) {
            if (object.handle != handle) continue;
            result.point = {object.x, object.y, object.z};
            result.speed = std::hypot(object.velocity[0], object.velocity[1], object.velocity[2]);
            return true;
        }
        return false;
    };
    EntityPosition source, destination;
    if (!resolve(subject, values[0].bits, source) ||
        (!target.empty() && !resolve(target, values[1].bits, destination))) {
        error = "Spatial query references an unknown entity.";
        return -1;
    }
    if(vm.vcs&&subject=="car"&&target.empty()&&
        !storylandVcsVehiclePosition(world,values[0].bits,source.point,error))return -1;
    const auto nativeOrigin=[&](const std::string& kind,int32_t handle,EntityPosition& position) {
        if(!vm.vcs||!three||position.inVehicle||(kind!="char"&&kind!="player")||
            !world.characterOriginOffset)return true;
        float offset=0;
        if(!world.characterOriginOffset(handle,offset)||!std::isfinite(offset))return false;
        position.point[2]+=offset;
        return true;
    };
    if(!nativeOrigin(subject,values[0].bits,source)||
        (!target.empty()&&!nativeOrigin(target,values[1].bits,destination))) {
        error="Character locate requires its native model collision origin.";
        return -1;
    }
    bool matches = true;
    double normalizedDistance = 0;
    const bool sphere = values[expected - 1].bits != 0;
    for (size_t axis = 0; axis < dimensions; ++axis) {
        const float point = source.point[axis];
        const float first = target.empty() ? operandFloat(values[axis + 1]) : destination.point[axis];
        const float second = operandFloat(values[axis + (target.empty() ? dimensions + 1 : 2)]);
        if (!std::isfinite(point) || !std::isfinite(first) || !std::isfinite(second)) {
            error = "Spatial query contains a non-finite coordinate.";
            return -1;
        }
        const double center = locate ? double(first) : (double(first) + second) * 0.5;
        const double radius = locate ? std::fabs(double(second)) : std::fabs(double(first) - second) * 0.5;
        const double difference = double(point) - center;
        matches = matches && std::fabs(difference) <= radius;
        if (sphere && radius > 0) normalizedDistance += (difference / radius) * (difference / radius);
    }
    if (sphere) matches = matches && normalizedDistance <= 1.0;
    if (subject == "char" || subject == "player") {
        if (name.find("_on_foot_") != std::string::npos) matches = matches && !source.inVehicle;
        if (name.find("_in_car_") != std::string::npos) matches = matches && source.inVehicle;
    }
    if (name.find("stopped_") != std::string::npos) {
        if (!std::isfinite(source.speed)) { error = "Spatial query has a non-finite speed."; return -1; }
        matches = matches && std::fabs(source.speed) < 0.01f;
    }
    // The last operand on a native LOCATE command is not just a query
    // modifier: when enabled, SCM also places the mission's destination
    // marker.  Keep this driven by the active script instruction rather
    // than creating a permanent entity when the condition becomes true.
    if (sphere && locate && (subject == "player" ||
        (subject == "char" && values[0].bits == (vm.vcs ? 0 : 1)))) {
        StorylandMissionEnvironment::ScriptCheckpoint marker;
        const auto destinationCoordinates = [&]() -> std::array<float,3> {
            if (!target.empty()) return destination.point;
            std::array<float,3> point{operandFloat(values[1]), operandFloat(values[2]), world.z};
            if (three) {
                point[2] = operandFloat(values[3]);
            } else if (world.groundHeightAt) {
                float height = 0;
                if (world.groundHeightAt({point[0], point[1], 1000}, height) && std::isfinite(height))
                    point[2] = height + 0.6f;
            }
            return point;
        }();
        const size_t radiusOffset = target.empty() ? dimensions + 1 : 2;
        const float radiusX = std::fabs(operandFloat(values[radiusOffset]));
        const float radiusY = std::fabs(operandFloat(values[radiusOffset + 1]));
        if (std::all_of(destinationCoordinates.begin(), destinationCoordinates.end(),
                [](float value) { return std::isfinite(value); }) &&
            std::isfinite(radiusX) && std::isfinite(radiusY) &&
            radiusX > 0 && radiusY > 0 && radiusX < 1000 && radiusY < 1000) {
            marker.position = destinationCoordinates;
            marker.direction = {destinationCoordinates[0], destinationCoordinates[1] + 1, destinationCoordinates[2]};
            marker.radius = std::max(radiusX, radiusY);
            marker.area = world.area;
            marker.color = {255, 219, 43, 120};
            marker.type = 0;
            const auto key = std::make_pair(reinterpret_cast<uintptr_t>(&vm),
                int32_t(std::min(vm.pc, size_t(INT32_MAX))));
            if (world.frameScriptCheckpoints.size() < 32 || world.frameScriptCheckpoints.count(key))
                world.frameScriptCheckpoints[key] = marker;
        }
    }
    if (inverted) matches = !matches;
    if (vm.conditionCount) {
        vm.condition = vm.conditionOr ? vm.condition || matches : vm.condition && matches;
        --vm.conditionCount;
    } else vm.condition = matches;
    vm.conditionValid = true;
    return 1;
}
