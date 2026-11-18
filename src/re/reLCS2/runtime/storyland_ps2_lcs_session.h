#pragma once
#include "src/re/reLCS2/runtime/storyland_ps2_lcs_conversion.h"
#include "src/leeds/leeds_ps2_script_state.h"
#include "src/storyland/storyland_save.h"
#include "src/storyland/storyland_scm.h"

namespace storyland_ps2_save {
inline int lcsHideout(int type) {
    if (type >= 16 && type <= 18) return type - 16;
    if (type >= 24 && type <= 32) return type - 21;
    return -1;
}
inline void prepareLcsSession(const Slot& slot, const StorylandScmFile& scm,
    const std::vector<int32_t>& weaponIds, storyland_save::State& state) {
    if (slot.vcs || scm.header().dialect != StorylandScmDialect::Lcs)
        throw std::runtime_error("This save requires the matching LCS game data.");
    const auto persistent = readPersistentScripts(slot, scm.rawBytes());
    const auto converted = convertLcs(slot, false);
    if (scm.header().missionCount != converted.missionCount ||
        scm.header().mainScriptSize != uint64_t(converted.mainSize) + 8 ||
        scm.header().largestMissionScriptSize != converted.largestMissionSize)
        throw std::runtime_error("The saved LCS mission table does not match MAIN.SCM.");
    if (weaponIds.empty() || !converted.player.maximumHealth)
        throw std::runtime_error("The player or weapon data cannot be restored.");
    std::vector<uint32_t> offsets;
    for (const auto& mission : scm.missions()) offsets.push_back(mission.binaryOffset);
    if (!state.program.start(scm.rawBytes(), scm.header().mainScriptOffset,
        offsets, state.mission, state.environment, false))
        throw std::runtime_error(state.program.status());
    state.mission.startRetail(scm.rawBytes(), scm.header().mainScriptOffset, false);
    state.mission.state = StorylandMissionState::Idle;
    state.mission.isMissionThread = false;
    state.mission.isInternalMission = false;
    if (!state.program.seedPersistentGlobals(persistent.globals, state.mission))
        throw std::runtime_error("Saved script variables do not fit this game.");
    auto& world = state.environment;
    world.garages = converted.garages;
    world.statistics = converted.statistics;
    world.statistics["CollectedPackages"] = converted.player.packages;
    world.statistics["TotalPackages"] = word(slot.blocks[3], 23);
    world.lastMissionPassedName = converted.lastMission;
    if (!converted.lastMission.empty()) world.passedMissions.insert(converted.lastMission);
    const auto stat = [&](const char* name) {
        const auto found = converted.statistics.find(name);
        return found == converted.statistics.end() ? 0 :
            int32_t(std::clamp(found->second, 0.0, double(INT32_MAX)));
    };
    world.progressMade = stat("ProgressMade");
    world.progressTotal = stat("TotalProgressInGame");
    world.missionsGiven = stat("MissionsGiven");
    world.totalMissions = stat("TotalNumberMissions");
    world.uniqueJumpsFound = stat("NumberOfUniqueJumpsFound");
    world.totalUniqueJumps = stat("TotalNumberOfUniqueJumps");
    world.money = converted.player.money;
    world.hour = converted.player.hour + converted.player.minute / 60.0f;
    world.playerHealthLimit = converted.player.maximumHealth;
    world.playerArmourLimit = converted.player.maximumArmour;
    world.playerMaximumHealth = world.health = converted.player.maximumHealth;
    world.playerNeverTired = converted.player.infiniteSprint;
    world.playerFastReload = converted.player.fastReload;
    world.playerFireproofReward = converted.player.fireProof;
    world.getOutOfJailFree = converted.player.freePolice;
    world.freeHealthCare = converted.player.freeHospital;
    world.gameMilliseconds = converted.player.savedMilliseconds;
    world.area = int32_t(word(slot.blocks[0], 12));
    world.x = real(slot.blocks[0], 72);
    world.y = real(slot.blocks[0], 76);
    world.z = real(slot.blocks[0], 80);
    world.weatherState.previous = int16_t(half(slot.blocks[0], 56));
    world.weatherState.current = int16_t(half(slot.blocks[0], 58));
    world.weatherState.interpolation = real(slot.blocks[0], 68);
    world.forcedWeather = int16_t(half(slot.blocks[0], 60));
    if (world.weatherState.previous < 0 || world.weatherState.current < 0 ||
        world.weatherState.interpolation < 0 || world.weatherState.interpolation > 1)
        throw std::runtime_error("Invalid native LCS weather state.");
    world.weatherRestoreRequested = true;
    world.extraColor = flag(slot.blocks[0], 96, true) ? int32_t(word(slot.blocks[0], 92)) : -1;
    state.elapsed = float(double(world.gameMilliseconds) / 1000.0);
    state.weaponIds = weaponIds;
    state.inventory.resize(weaponIds.size());
    state.inventory[0].owned = true;
    const auto& garages = slot.blocks[2];
    world.freeBombs = flag(garages, 4, true);
    world.freeResprays = flag(garages, 8, true);
    for (size_t i = 0; i < 4; ++i) world.importGarageMasks[i] = word(garages, 24 + i * 4);
    std::map<int, int32_t> hideouts;
    for (const auto& garage : world.garages) {
        world.importedGarageHandles.insert(garage.handle);
        world.nextGarageHandle = std::max(world.nextGarageHandle, garage.handle + 1);
        const int hideout = lcsHideout(garage.type);
        if (hideout >= 0) hideouts.emplace(hideout, garage.handle);
    }
    for (size_t i = 0; i < 48; ++i) {
        const size_t offset = 44 + i * 44;
        const int32_t model = int32_t(word(garages, offset));
        if (!model) continue;
        if (model < 0 || word(garages, offset + 32) || garages[offset + 39] ||
            garages[offset + 40] || garages[offset + 41] || real(garages, offset + 28) != 1.0f)
            throw std::runtime_error("This LCS garage contains unsupported stored-car settings.");
        const auto garage = hideouts.find(int(i % 12));
        if (garage == hideouts.end()) throw std::runtime_error("Stored car has no matching hideout.");
        storyland_save::Vehicle vehicle;
        vehicle.model = uint32_t(model);
        vehicle.garage = garage->second;
        vehicle.actor.vehicle = true;
        vehicle.actor.position = {real(garages, offset + 4), real(garages, offset + 8), real(garages, offset + 12)};
        const float x = real(garages, offset + 16), y = real(garages, offset + 20), z = real(garages, offset + 24);
        vehicle.actor.yaw = std::atan2(-x, y) * 57.2957795131f + 90.0f;
        vehicle.bodyPitch = std::atan2(z, std::hypot(x, y)) * 57.2957795131f;
        vehicle.primaryColor = int8_t(garages[offset + 36]);
        vehicle.secondaryColor = int8_t(garages[offset + 37]);
        vehicle.radioStation = int8_t(garages[offset + 38]);
        state.vehicles.push_back(vehicle);
    }
}

inline std::vector<uint8_t> encodeLcsSession(const Slot& original,
    const StorylandScmFile& scm, const storyland_save::State& state) {
    if (state.mission.vcs || original.vcs) throw std::runtime_error("A native LCS session cannot change game format.");
    const auto converted = convertLcs(original, false);
    auto player = converted.player;
    const auto& world = state.environment;
    if (!std::isfinite(world.hour) || world.hour < 0 || world.hour >= 24)
        throw std::runtime_error("Invalid saved game clock.");
    player.money = player.displayedMoney = world.money;
    player.hour = uint8_t(world.hour);
    player.minute = uint8_t((world.hour - player.hour) * 60.0f + 0.0001f);
    player.minute = std::min<uint8_t>(player.minute, 59);
    player.savedMilliseconds = world.gameMilliseconds;
    player.maximumHealth = world.playerHealthLimit;
    player.maximumArmour = world.playerArmourLimit;
    player.infiniteSprint = world.playerNeverTired;
    player.fastReload = world.playerFastReload;
    player.fireProof = world.playerFireproofReward;
    player.freePolice = world.getOutOfJailFree;
    player.freeHospital = world.freeHealthCare;
    const auto packages = world.statistics.find("CollectedPackages");
    if (packages == world.statistics.end() || !std::isfinite(packages->second) ||
        packages->second < 0 || packages->second > INT32_MAX || std::floor(packages->second) != packages->second)
        throw std::runtime_error("Invalid hidden-package count.");
    player.packages = int32_t(packages->second);
    auto result = decode(encodeLcsPlayerAndStatistics(original, player, world.statistics,
        world.lastMissionPassedName), original.directory, original.name);
    result = writePersistentScripts(result, scm.rawBytes(), state.program.persistentSaveGlobals());
    auto& simple = result.blocks[0];
    if (world.weatherState.previous < 0 || world.weatherState.current < 0 ||
        world.forcedWeather < -1 || world.forcedWeather > INT16_MAX ||
        !std::isfinite(world.weatherState.interpolation) || world.weatherState.interpolation < 0 ||
        world.weatherState.interpolation > 1)
        throw std::runtime_error("Invalid LCS weather state.");
    const auto putHalf = [&](size_t offset, int16_t value) {
        simple[offset] = uint8_t(value); simple[offset + 1] = uint8_t(uint16_t(value) >> 8);
    };
    putHalf(56, world.weatherState.previous); putHalf(58, world.weatherState.current);
    putHalf(60, int16_t(world.forcedWeather));
    uint32_t blend; std::memcpy(&blend, &world.weatherState.interpolation, 4); putWord(simple, 68, blend);
    putWord(simple, 12, uint32_t(world.area));
    const std::array<float, 3> position{world.x, world.y, world.z};
    for (size_t i = 0; i < position.size(); ++i) {
        if (!std::isfinite(position[i])) throw std::runtime_error("Invalid saved streaming position.");
        uint32_t bits; std::memcpy(&bits, &position[i], 4); putWord(simple, 72 + i * 4, bits);
    }
    putWord(simple, 92, uint32_t(world.extraColor));
    putWord(simple, 96, world.extraColor >= 0);
    auto& data = result.blocks[2];
    if (world.garages.size() != converted.garages.size())
        throw std::runtime_error("The native garage table changed size; the save was not written.");
    const auto putReal = [&](size_t offset, float value) {
        if (!std::isfinite(value)) throw std::runtime_error("Invalid garage coordinate.");
        uint32_t bits; std::memcpy(&bits, &value, 4); putWord(data, offset, bits);
    };
    putWord(data, 4, world.freeBombs);
    putWord(data, 8, world.freeResprays);
    for (size_t i = 0; i < 4; ++i) putWord(data, 24 + i * 4, world.importGarageMasks[i]);
    std::map<int32_t, int> hideouts;
    std::set<int32_t> handles;
    for (const auto& garage : world.garages) {
        if (garage.handle < 0 || size_t(garage.handle) >= converted.garages.size() ||
            !handles.insert(garage.handle).second || garage.type < 0 || garage.type > 255 ||
            garage.maximumCars < 0 || garage.maximumCars > 4)
            throw std::runtime_error("Unsupported native garage record.");
        const auto& previous = converted.garages[size_t(garage.handle)];
        if (garage.bounds != previous.bounds)
            throw std::runtime_error("A native garage changed shape; the save was not written.");
        const size_t offset = 2156 + size_t(garage.handle) * 256;
        data[offset] = uint8_t(garage.type);
        data[offset + 1] = garage.state;
        data[offset + 2] = uint8_t(garage.maximumCars);
        putWord(data, offset + 12, garage.locked);
        putWord(data, offset + 16, garage.resprayDone);
        putWord(data, offset + 48, garage.swingDoor);
        putWord(data, offset + 52, garage.cameraFollowsPlayer);
        putReal(offset + 148, garage.doorPosition);
        const int hideout = lcsHideout(garage.type);
        if (hideout >= 0) hideouts.emplace(garage.handle, hideout);
    }
    std::fill(data.begin() + 44, data.begin() + 2156, 0);
    std::array<size_t, 12> counts{};
    for (const auto& vehicle : state.vehicles) {
        const auto garage = hideouts.find(vehicle.garage);
        if (garage == hideouts.end()) continue;
        if (counts[size_t(garage->second)] >= 4 || vehicle.damageProofs ||
            vehicle.tractionMultiplier != 1.0f || !vehicle.model)
            throw std::runtime_error("A stored car cannot be represented safely in this native save.");
        const size_t index = size_t(garage->second) + 12 * counts[size_t(garage->second)]++;
        const size_t offset = 44 + index * 44;
        putWord(data, offset, vehicle.model);
        putReal(offset + 4, vehicle.actor.position.x);
        putReal(offset + 8, vehicle.actor.position.y);
        putReal(offset + 12, vehicle.actor.position.z);
        const float yaw = (vehicle.actor.yaw - 90.0f) * 0.01745329252f;
        const float pitch = vehicle.bodyPitch * 0.01745329252f;
        putReal(offset + 16, -std::sin(yaw) * std::cos(pitch));
        putReal(offset + 20, std::cos(yaw) * std::cos(pitch));
        putReal(offset + 24, std::sin(pitch));
        putReal(offset + 28, 1.0f);
        data[offset + 36] = uint8_t(vehicle.primaryColor);
        data[offset + 37] = uint8_t(vehicle.secondaryColor);
        data[offset + 38] = uint8_t(vehicle.radioStation);
    }
    return encode(result);
}
}
