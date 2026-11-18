static void appendDtzBehaviourData(const std::vector<uint8_t>& data, bool vcs,
    std::vector<StorylandDtzDataBlock>& blocks, std::vector<StorylandDtzDataField>& fields) {
    const auto valid = [&](uint32_t offset, size_t length) {
        return offset >= 0xC0 && uint64_t(offset) + length <= data.size();
    };
    const auto block = [&](const char* name, uint32_t offset, uint32_t rows) {
        const uint32_t end = rows && uint64_t(offset) + uint64_t(rows)*4u <= data.size()
            ? offset + rows*4u : offset;
        const std::string note = "Leeds-engine native records in GAME.DTZ. "
            "Each row points to its original handling or pedestrian-stat structure; "
            "edit the values here and rebuild GAME.DTZ to save them.";
        return appendDtzDataBlock(blocks, name, "native-behaviour", 0, offset,
            end, rows ? 4u : 0u, rows, true, note);
    };
    const auto field = [&](size_t group, uint32_t row, const std::string& label, uint32_t offset,
        const char* name, const char* type, double minimum, double maximum, bool editable = true) {
        const uint32_t size = std::string(type) == "float" || std::string(type) == "u32" ? 4 :
            std::string(type) == "u16" ? 2 : 1;
        if (!valid(offset, size)) return;
        std::string value = std::string(type) == "float" ? f32ValueText(data, offset) :
            std::string(type) == "char" ? std::string(1, char(data[offset])) :
            size == 4 ? std::to_string(readU32(data, offset)) :
            size == 2 ? std::to_string(readU16(data, offset)) : std::to_string(data[offset]);
        addDtzDataField(fields, group, blocks[group], row, offset, 0, size, label, name, type, value, editable, "");
        fields.back().hasLimits = true;
        fields.back().minimum = minimum;
        fields.back().maximum = maximum;
    };
    const uint32_t statsTable = readU32(data, vcs ? 0x8C : 0x90);
    uint32_t statsCount = 0;
    for (; statsCount < (vcs ? 128u : 42u); ++statsCount) {
        if (!valid(statsTable, size_t(statsCount + 1) * 4)) break;
        const uint32_t record = readU32(data, statsTable + statsCount * 4);
        if (!valid(record, 0x34) || !hasAsciiNameAt(data, record + (vcs ? 0x1A : 4), vcs ? 26 : 24, "STAT_")) break;
    }
    if (statsCount) {
        const size_t group = block("Ped stats", statsTable, statsCount);
        for (uint32_t row = 0; row < statsCount; ++row) {
            const uint32_t record = readU32(data, statsTable + row * 4);
            const auto label = readFixedAscii(data, record + (vcs ? 0x1A : 4), vcs ? 26 : 24);
            field(group, row, label, record + (vcs ? 4 : 0x1C), "Flee distance", "float", 0, 200);
            field(group, row, label, record + (vcs ? 8 : 0x20), "Heading change rate", "float", 0, 100);
            field(group, row, label, record + (vcs ? 12 : 0x28), "Attack strength", "float", 0, 100);
            field(group, row, label, record + (vcs ? 16 : 0x2C), "Defend weakness", "float", 0, 100);
            field(group, row, label, record + (vcs ? 0x14 : 0x30), "Flags", vcs ? "u8" : "u16", 0, vcs ? 255 : 65535);
            const char* names[] = {"Fear", "Temper", "Lawfulness", "View"};
            for (uint32_t i = 0; i < 4; ++i)
                field(group, row, label, record + (vcs ? 0x16 : 0x24) + i, names[i], "u8", 0, 100);
        }
    }
    const uint32_t types = readU32(data, vcs ? 0x88 : 0x8C);
    if (valid(types, 23 * 4)) {
        const uint32_t first = readU32(data, types), second = readU32(data, types + 4);
        const uint32_t stride = second > first ? second - first : 0;
        if (stride == 12 || stride == 32) {
            const size_t group = block("Ped types", types, 23);
            for (uint32_t row = 0; row < 23; ++row) {
                const uint32_t record = readU32(data, types + row * 4);
                if (!valid(record, stride) || readU32(data, record) != (1u << row)) continue;
                const auto label = "Type " + std::to_string(row);
                field(group, row, label, record, "Type flag", "u32", 0, UINT32_MAX, false);
                field(group, row, label, record + stride - 8, "Threat mask", "u32", 0, UINT32_MAX);
                field(group, row, label, record + stride - 4, "Avoid mask", "u32", 0, UINT32_MAX);
            }
        }
    }
    const uint32_t count = readU32(data, 0x38), models = readU32(data, 0x3C);
    if (count > 65536 || !valid(models, size_t(count) * 4)) return;
    const size_t pedestrians = block("Pedestrians", models, 0);
    const size_t vehicles = block("Vehicle handling", models, 0);
    for (uint32_t id = 0; id < count; ++id) {
        const uint32_t info = readU32(data, models + id * 4);
        if (!valid(info, vcs ? 0x44 : 0x46)) continue;
        std::string label = "Model " + std::to_string(id);
        const std::string nativeName=readFixedAscii(data,info+0x2Eu,20u);
        if(!nativeName.empty() && nativeName.find('.')==std::string::npos)
            label += " (" + nativeName + ")";
        if (data[info + 0x10] == 7) {
            ++blocks[pedestrians].rowCount;
            field(pedestrians, id, label, info + (vcs ? 0x34 : 0x30), "Ped type", "u32", 0, 22);
            if (statsCount) field(pedestrians, id, label, info + (vcs ? 0x38 : 0x34), "Stat profile", "u32", 0, statsCount - 1);
            field(pedestrians, id, label, info + (vcs ? 0x30 : 0x2C), "Animation group", "u32", 0, 199);
            if (vcs) field(pedestrians, id, label, info + 0x3C, "Vehicle class mask", "u32", 0, UINT32_MAX);
        } else if (data[info + 0x10] == 6) {
            const auto vehicle = storylandDecodeVehicleDefinition(data, id, vcs);
            // Only car/road handling belongs in the car editor. Boats,
            // aircraft and two-wheeled vehicles use additional native types.
            if (!vehicle.hasNativeHandling ||
                vehicle.domain != StorylandVehicleDomain::Road || vehicle.twoWheels()) continue;
            ++blocks[vehicles].rowCount;
            const uint32_t h = vehicle.handlingPointer;
            if(!vcs)label += " / handling " + std::to_string(vehicle.handlingId);
            struct Setting { const char* name; uint32_t vcsOffset, lcsOffset; double minimum, maximum; };
            const Setting settings[] = {
                {"Mass", 0xBC, 0, 100, 100000}, {"Turn mass", 0xC0, 4, 0.01, 10000000},
                {"Traction", 0xC4, 8, -5, 5}, {"Drag", 0x98, 0x18, 0, 100},
                {"Centre of mass X", 0xA0, 0x20, -99, 99}, {"Centre of mass Y", 0xA4, 0x24, -99, 99},
                {"Centre of mass Z", 0xA8, 0x28, -99, 99},
                {"Engine acceleration", 0x48, 0x84, 0, 10}, {"Engine inertia", 0x4C, 0x88, 0.01, 1000},
                {"Maximum velocity", 0x50, 0x8C, 0.001, 1000}, {"AI maximum velocity", 0x54, 0x90, 0, 1000},
                {"Reverse velocity", 0x58, 0x94, -1000, 1000},
                {"Brake deceleration", 0x60, 0x9C, 0, 1}, {"Brake bias", 0x64, 0xA0, 0, 1},
                {"Steering lock", 0x68, 0xA8, 1, 90}, {"Traction loss", 0x6C, 0xAC, 0, 10},
                {"Traction bias", 0x70, 0xB0, 0, 1}, {"Suspension force", 0x74, 0xB8, 0.001, 99},
                {"Suspension damping", 0x78, 0xBC, 0, 100}, {"Suspension upper limit", 0x7C, 0xC0, -100, 100},
                {"Suspension lower limit", 0x80, 0xC4, -100, 100}, {"Suspension bias", 0x84, 0xC8, 0, 1},
                {"Suspension anti-dive", 0x88, 0xCC, 0, 100}, {"Seat offset", 0x8C, 0xD4, -100, 100},
                {"Collision damage", 0xC8, 0x0C, 0, 100}
            };
            for (const auto& setting : settings)
                field(vehicles, id, label, h + (vcs ? setting.vcsOffset : setting.lcsOffset), setting.name,
                    "float", setting.minimum, setting.maximum);
            if (vcs) field(vehicles, id, label, h + 0xB8, "Authored maximum velocity", "float", 0.001, 1000);
            field(vehicles, id, label, h + (vcs ? 0x5C : 0x80), "Drive type", "char", 0, 255);
            field(vehicles, id, label, h + (vcs ? 0x5D : 0x81), "Engine type", "char", 0, 255);
            field(vehicles, id, label, h + (vcs ? 0x5E : 0x82), "Gears", "u8", 1, 5);
            field(vehicles, id, label, h + 0xD0, "Model flags", "u32", 0, UINT32_MAX);
            if (vcs) field(vehicles, id, label, h + 0xCC, "Handling flags", "u32", 0, UINT32_MAX);
            for (uint32_t gear = 0; gear < 5; ++gear) {
                const char* names[] = {" maximum", " upshift", " downshift"};
                for (uint32_t value = 0; value < 3; ++value) {
                    const auto name = "Gear " + std::to_string(gear + 1) + names[value];
                    field(vehicles, id, label, h + (vcs ? 0 : 0x38) + gear * 12 + value * 4, name.c_str(), "float", -1000, 1000);
                }
            }
        }
    }
}
