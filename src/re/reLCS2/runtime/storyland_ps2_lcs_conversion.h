#pragma once
#include "src/leeds/leeds_ps2_save.h"
#include "src/storyland/storyland_mission_vm.h"
#include <cmath>
#include <limits>

namespace storyland_ps2_save {
struct LcsPlayer {
    int32_t money = 0;
    int32_t displayedMoney = 0;
    int32_t packages = 0;
    uint8_t maximumHealth = 0;
    uint8_t maximumArmour = 0;
    bool infiniteSprint = false;
    bool fastReload = false;
    bool fireProof = false;
    bool freeHospital = false;
    bool freePolice = false;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint32_t savedMilliseconds = 0;
};
struct LcsThread {
    StorylandMissionVm state;
    std::vector<uint32_t> nativeReturnValues;
    bool hasFunctionCalls = false;
    uint32_t localsBase = 0;
    bool skipWakeTime = false;
    bool missionFlag = false;
    bool active = false;
};
struct LcsConversion {
    LcsPlayer player;
    uint32_t mainSize = 0;
    uint32_t largestMissionSize = 0;
    uint16_t missionCount = 0;
    std::vector<int32_t> globals;
    std::vector<LcsThread> threads;
    std::vector<StorylandMissionEnvironment::Garage> garages;
    std::map<std::string, double> statistics;
    std::string lastMission;
};
inline uint16_t half(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2)
        throw std::runtime_error("Truncated native PS2 field.");
    return uint16_t(bytes[offset]) | uint16_t(bytes[offset + 1]) << 8;
}
inline float real(const std::vector<uint8_t>& bytes, size_t offset) {
    const uint32_t bits = word(bytes, offset);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    if (!std::isfinite(value)) throw std::runtime_error("Invalid native PS2 floating-point field.");
    return value;
}
inline bool flag(const std::vector<uint8_t>& bytes, size_t offset, bool wide = false) {
    if (offset >= bytes.size()) throw std::runtime_error("Truncated native PS2 flag.");
    const uint32_t value = wide ? word(bytes, offset) : bytes[offset];
    if (value > 1) throw std::runtime_error("Invalid native PS2 flag.");
    return value != 0;
}
struct Statistic { const char* name; size_t offset; size_t count; char type; };
inline constexpr Statistic lcsStatisticFields[] = {
        {"PeopleKilledByPlayer", 0, 1, 'i'},
        {"PeopleKilledByOthers", 4, 1, 'i'},
        {"CarsExploded", 8, 1, 'i'},
        {"BoatsExploded", 12, 1, 'i'},
        {"TyresPopped", 16, 1, 'i'},
        {"RoundsFiredByPlayer", 20, 1, 'i'},
        {"PedsKilledOfThisType", 24, 23, 'i'},
        {"HelisDestroyed", 116, 1, 'i'},
        {"ProgressMade", 120, 1, 'f'},
        {"TotalProgressInGame", 124, 1, 'f'},
        {"KgsOfExplosivesUsed", 128, 1, 'i'},
        {"BulletsThatHit", 132, 1, 'i'},
        {"CarsCrushed", 136, 1, 'i'},
        {"HeadsPopped", 140, 1, 'i'},
        {"WantedStarsAttained", 144, 1, 'i'},
        {"WantedStarsEvaded", 148, 1, 'i'},
        {"TimesArrested", 152, 1, 'i'},
        {"TimesDied", 156, 1, 'i'},
        {"DaysPassed", 160, 1, 'i'},
        {"SafeHouseVisits", 164, 1, 'i'},
        {"Sprayings", 168, 1, 'i'},
        {"MaximumJumpDistance", 172, 1, 'f'},
        {"MaximumJumpHeight", 176, 1, 'f'},
        {"MaximumJumpFlips", 180, 1, 'i'},
        {"MaximumJumpSpins", 184, 1, 'i'},
        {"BestStuntJump", 188, 1, 'i'},
        {"NumberOfUniqueJumpsFound", 192, 1, 'i'},
        {"TotalNumberOfUniqueJumps", 196, 1, 'i'},
        {"MissionsGiven", 200, 1, 'i'},
        {"PassengersDroppedOffWithTaxi", 204, 1, 'i'},
        {"MoneyMadeWithTaxi", 208, 1, 'i'},
        {"IndustrialPassed", 212, 1, 'b'},
        {"CommercialPassed", 216, 1, 'b'},
        {"SuburbanPassed", 220, 1, 'b'},
        {"PamphletMissionPassed", 224, 1, 'b'},
        {"NoMoreHurricanes", 228, 1, 'b'},
        {"DistanceTravelledOnFoot", 232, 1, 'f'},
        {"DistanceTravelledByCar", 236, 1, 'f'},
        {"DistanceTravelledByBike", 240, 1, 'f'},
        {"DistanceTravelledByBoat", 244, 1, 'f'},
        {"DistanceTravelledByPlane", 248, 1, 'f'},
        {"LivesSavedWithAmbulance", 252, 1, 'i'},
        {"CriminalsCaught", 256, 1, 'i'},
        {"FiresExtinguished", 260, 1, 'i'},
        {"HighestLevelVigilanteMission", 264, 1, 'i'},
        {"HighestLevelAmbulanceMission", 268, 1, 'i'},
        {"HighestLevelFireMission", 272, 1, 'i'},
        {"PhotosTaken", 276, 1, 'i'},
        {"NumberKillFrenziesPassed", 280, 1, 'i'},
        {"MaxSecondsOnCarnageLeft", 284, 1, 'i'},
        {"MaxKillsOnRcTriad", 288, 1, 'i'},
        {"TotalNumberKillFrenzies", 292, 1, 'i'},
        {"TotalNumberMissions", 296, 1, 'i'},
        {"TimesDrowned", 300, 1, 'i'},
        {"SeagullsKilled", 304, 1, 'i'},
        {"WeaponBudget", 308, 1, 'f'},
        {"LoanSharks", 312, 1, 'i'},
        {"MovieStunts", 316, 1, 'i'},
        {"PizzasDelivered", 320, 1, 'f'},
        {"NoodlesDelivered", 324, 1, 'f'},
        {"MoneyMadeFromTourist", 328, 1, 'f'},
        {"TouristsTakenToSpots", 332, 1, 'f'},
        {"GarbagePickups", 336, 1, 'i'},
        {"IceCreamSold", 340, 1, 'f'},
        {"TopShootingRangeScore", 344, 1, 'i'},
        {"ShootingRank", 348, 1, 'i'},
        {"TopScrapyardChallengeScore", 352, 1, 'f'},
        {"Top9mmMayhemScore", 356, 1, 'f'},
        {"TopScooterShooterScore", 360, 1, 'f'},
        {"TopWichitaWipeoutScore", 364, 1, 'f'},
        {"LongestWheelie", 368, 1, 'i'},
        {"LongestStoppie", 372, 1, 'i'},
        {"Longest2Wheel", 376, 1, 'i'},
        {"LongestWheelieDist", 380, 1, 'f'},
        {"LongestStoppieDist", 384, 1, 'f'},
        {"Longest2WheelDist", 388, 1, 'f'},
        {"LongestFacePlantDist", 392, 1, 'f'},
        {"AutoPaintingBudget", 396, 1, 'f'},
        {"PropertyDestroyed", 400, 1, 'i'},
        {"NumPropertyOwned", 404, 1, 'i'},
        {"UnlockedCostumes", 408, 1, 'h'},
        {"BloodRingKills", 410, 1, 'i'},
        {"BloodRingTime", 414, 1, 'i'},
        {"PropertyOwned", 418, 15, 'c'},
        {"HighestChaseValue", 433, 1, 'f'},
        {"FastestTimes", 437, 23, 'i'},
        {"HighestScores", 529, 5, 'i'},
        {"BestPositions", 549, 1, 'i'},
        {"KillsSinceLastCheckpoint", 553, 1, 'i'},
        {"TotalLegitimateKills", 557, 1, 'i'},
        {"LastMissionPassedName", 561, 8, 's'},
        {"CheatedCount", 569, 1, 'i'},
        {"CarsSold", 573, 1, 'i'},
        {"MoneyMadeWithCarSales", 577, 1, 'i'},
        {"BikesSold", 581, 1, 'i'},
        {"MoneyMadeWithBikeSales", 585, 1, 'i'},
        {"NumberOfExportedCars", 589, 1, 'i'},
        {"TotalNumberOfCarExport", 593, 1, 'i'},
        {"HighestLevelSlashTv", 597, 1, 'i'},
        {"MoneyMadeWithSlashTv", 601, 1, 'i'},
        {"TotalKillsOnSlashTv", 605, 1, 'i'},
        {"PackagesSmuggled", 609, 1, 'i'},
        {"SmugglersWasted", 613, 1, 'i'},
        {"FastestSmugglingTime", 617, 1, 'i'},
        {"MoneyMadeInCoach", 621, 1, 'i'},
        {"CashMadeCollectingTrash", 625, 1, 'i'},
        {"HitmenKilled", 629, 1, 'i'},
        {"HighestGuardianAngelJusticeDished", 633, 1, 'i'},
        {"GuardianAngelMissionsPassed", 637, 1, 'i'},
        {"GuardianAngelHighestLevelInd", 641, 1, 'i'},
        {"GuardianAngelHighestLevelCom", 645, 1, 'i'},
        {"GuardianAngelHighestLevelSub", 649, 1, 'i'},
        {"MostTimeLeftTrainRace", 653, 1, 'i'},
        {"BestTimeGoGoFaggio", 657, 1, 'i'},
        {"DirtBikeMostAir", 661, 1, 'i'},
        {"HighestTrainCashEarned", 665, 1, 'i'},
        {"FastestHeliRaceTime", 669, 1, 'i'},
        {"BestHeliRacePosition", 673, 1, 'i'},
        {"NumberOutfitChanges", 677, 1, 'i'},
        {"BestBanditLapTimes", 681, 3, 'i'},
        {"BestBanditPositions", 693, 3, 'i'},
        {"BestStreetRacePositions", 705, 6, 'i'},
        {"FastestStreetRaceLapTimes", 729, 6, 'i'},
        {"FastestStreetRaceTimes", 753, 6, 'i'},
        {"FastestDirtBikeLapTimes", 777, 10, 'i'},
        {"FastestDirtBikeTimes", 817, 10, 'i'},
        {"FavoriteRadioStationList", 857, 10, 'f'},
    };

inline LcsConversion convertLcs(const Slot& slot, bool decodeThreads = true) {
    if (slot.vcs) throw std::runtime_error("VCS requires its own native state conversion.");
    // Revalidate the complete container before translating any runtime fields.
    const Slot checked = decode(encode(slot), slot.directory, slot.name);
    LcsConversion result;
    const auto& simple = checked.blocks[0];
    const auto& player = checked.blocks[3];
    result.player.money = int32_t(word(player, 0));
    result.player.displayedMoney = int32_t(word(player, 15));
    result.player.packages = int32_t(word(player, 19));
    result.player.infiniteSprint = flag(player, 27, true);
    result.player.fastReload = flag(player, 31, true);
    result.player.fireProof = flag(player, 35, true);
    result.player.maximumHealth = player[39];
    result.player.maximumArmour = player[40];
    result.player.freePolice = flag(player, 41, true);
    result.player.freeHospital = flag(player, 45, true);
    result.player.hour = simple[28];
    result.player.minute = simple[29];
    result.player.savedMilliseconds = word(simple, 32);
    if (result.player.hour > 23 || result.player.minute > 59 || result.player.packages < 0)
        throw std::runtime_error("Invalid LCS player information.");

    const auto& script = checked.blocks[1];
    const uint32_t variableBytes = word(script, 8);
    if (variableBytes < 8 || variableBytes % 4 || variableBytes / 4 > 6656)
        throw std::runtime_error("Invalid LCS global variable space.");
    for (uint32_t index = 0; index < variableBytes; index += 4)
        result.globals.push_back(int32_t(word(script, 12 + index)));
    size_t threadStart = 0;
    uint32_t threadCount = 0;
    for (uint32_t count = 1; count <= 96; ++count) {
        const size_t length = size_t(count) * 540;
        if (length + 28 > script.size()) break;
        const size_t start = script.size() - length;
        if (start < 12 + variableBytes + 20 || word(script, start - 4) != count) continue;
        const uint32_t mainSize = word(script, start - 16);
        const uint32_t largest = word(script, start - 12);
        if (mainSize <= variableBytes || mainSize > 16 * 1024 * 1024 || !largest || largest > mainSize) continue;
        if (threadStart) throw std::runtime_error("Ambiguous LCS thread table.");
        threadStart = start;
        threadCount = count;
        result.mainSize = mainSize;
        result.largestMissionSize = largest;
        result.missionCount = half(script, start - 8);
    }
    if (!threadStart) throw std::runtime_error("Cannot locate the native LCS thread table.");
    for (uint32_t index = 0; decodeThreads && index < threadCount; ++index) {
        const size_t base = threadStart + size_t(index) * 540;
        LcsThread thread;
        auto& vm = thread.state;
        vm.vcs = false;
        vm.wholeScm = true;
        vm.isInternalMission = false;
        for (size_t character = 0; character < 8 && script[base + 16 + character]; ++character) {
            const uint8_t value = script[base + 16 + character];
            if (value < 32 || value > 126) throw std::runtime_error("Invalid LCS thread name.");
            vm.scriptName.push_back(char(value));
        }
        vm.pc = word(script, base + 24);
        if (vm.pc >= result.mainSize) throw std::runtime_error("LCS thread needs unsupported mission-address relocation.");
        const uint16_t depth = half(script, base + 92);
        if (depth > 16) throw std::runtime_error("Invalid native LCS return stack.");
        for (uint16_t stack = 0; stack < depth; ++stack) {
            const uint32_t encoded = word(script, base + 28 + stack * 4);
            const bool function = (encoded & (1u << 23)) != 0;
            const uint32_t address = function ? encoded & 0x003fffffu : encoded;
            if (address >= result.mainSize) throw std::runtime_error("Invalid native LCS return address.");
            thread.nativeReturnValues.push_back(encoded);
            thread.hasFunctionCalls = thread.hasFunctionCalls || function;
            if (!function) vm.returnStack.push_back(address);
        }
        for (size_t local = 0; local < 104; ++local)
            vm.locals[local] = int32_t(word(script, base + 96 + local * 4));
        vm.locals[104] = int32_t(word(script, base + 512));
        vm.locals[105] = int32_t(word(script, base + 516));
        thread.localsBase = word(script, base + 520);
        if (thread.localsBase >= 104) throw std::runtime_error("Invalid native LCS locals base.");
        thread.skipWakeTime = flag(script, base + 527);
        thread.missionFlag = flag(script, base + 537);
        thread.active = flag(script, base + 524);
        vm.condition = flag(script, base + 525);
        vm.conditionValid = true;
        vm.isMissionThread = flag(script, base + 526);
        const uint16_t condition = half(script, base + 532);
        if (condition > 8 && (condition < 21 || condition > 28))
            throw std::runtime_error("Unsupported native LCS condition state.");
        if (flag(script, base + 534)) throw std::runtime_error("LCS save contains an unfinished inverted instruction.");
        vm.conditionOr = condition >= 21;
        vm.conditionCount = vm.conditionOr ? condition - 20 : condition;
        vm.deathArrestEnabled = flag(script, base + 535);
        vm.deathArrestExecuted = flag(script, base + 536);
        const uint32_t wake = word(script, base + 528);
        const uint32_t difference = wake - result.player.savedMilliseconds;
        vm.delayMilliseconds = !thread.skipWakeTime && difference < 0x80000000u ? difference : 0;
        vm.state = vm.delayMilliseconds ? StorylandMissionState::Waiting : StorylandMissionState::Running;
        if (!thread.active) vm.state = StorylandMissionState::Idle;
        vm.globals = result.globals;
        vm.globals.resize(6656);
        // Encoded function returns also carry parameter counts and local-frame offsets.
        // They must not be resumed as ordinary GOSUB addresses.
        if (thread.hasFunctionCalls) {
            vm.returnStack.clear();
            vm.state = StorylandMissionState::Idle;
            vm.diagnostic = "Native function frames require MAIN.SCM conversion before execution.";
        }
        result.threads.push_back(std::move(thread));
    }

    const auto& garages = checked.blocks[2];
    const uint32_t garageCount = word(garages, 0);
    if (garageCount > 32) throw std::runtime_error("Invalid LCS garage count.");
    for (uint32_t index = 0; index < garageCount; ++index) {
        const size_t base = 2156 + size_t(index) * 256;
        StorylandMissionEnvironment::Garage garage;
        garage.handle = int32_t(index);
        garage.type = garages[base];
        garage.state = garages[base + 1];
        garage.maximumCars = garages[base + 2];
        garage.locked = flag(garages, base + 12, true);
        garage.resprayDone = flag(garages, base + 16, true);
        garage.swingDoor = flag(garages, base + 48, true);
        garage.cameraFollowsPlayer = flag(garages, base + 52, true);
        const float x = real(garages, base + 56), y = real(garages, base + 60);
        const float length1 = real(garages, base + 116), length2 = real(garages, base + 120);
        garage.bounds = {x, y, real(garages, base + 64),
            x + real(garages, base + 68) * length1, y + real(garages, base + 72) * length1,
            x + real(garages, base + 76) * length2, y + real(garages, base + 80) * length2,
            real(garages, base + 84)};
        garage.doorPosition = real(garages, base + 148);
        result.garages.push_back(garage);
    }
    const auto& statistics = checked.blocks[4];
    for (const auto& field : lcsStatisticFields) {
        if (field.type == 's') {
            for (size_t index = 0; index < field.count && statistics[field.offset + index]; ++index) {
                const uint8_t value = statistics[field.offset + index];
                if (value < 32 || value > 126) throw std::runtime_error("Invalid LCS mission label.");
                result.lastMission.push_back(char(value));
            }
            continue;
        }
        const size_t width = field.type == 'c' ? 1 : field.type == 'h' ? 2 : 4;
        for (size_t index = 0; index < field.count; ++index) {
            const size_t offset = field.offset + index * width;
            double value;
            if (field.type == 'f') value = real(statistics, offset);
            else if (field.type == 'b') value = flag(statistics, offset, true);
            else if (field.type == 'h') value = half(statistics, offset);
            else if (field.type == 'c') value = statistics[offset];
            else value = int32_t(word(statistics, offset));
            std::string key = field.name;
            if (field.count > 1) key += "[" + std::to_string(index) + "]";
            result.statistics.emplace(std::move(key), value);
        }
    }
    return result;
}
inline void putWord(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    if (offset > bytes.size() || bytes.size() - offset < 4)
        throw std::runtime_error("Native save write is out of bounds.");
    for (unsigned index = 0; index < 4; ++index) bytes[offset + index] = uint8_t(value >> (index * 8));
}
inline std::vector<uint8_t> encodeLcsPlayerAndStatistics(const Slot& original,
    const LcsPlayer& player, const std::map<std::string, double>& statistics,
    const std::string& lastMission) {
    if (original.vcs) throw std::runtime_error("VCS requires its own native state conversion.");
    if (player.hour > 23 || player.minute > 59 || player.packages < 0 || lastMission.size() > 8)
        throw std::runtime_error("Invalid LCS player or mission information.");
    Slot changed = decode(encode(original), original.directory, original.name);
    auto& data = changed.blocks[3];
    putWord(data, 0, uint32_t(player.money));
    putWord(data, 15, uint32_t(player.displayedMoney));
    putWord(data, 19, uint32_t(player.packages));
    putWord(data, 27, player.infiniteSprint);
    putWord(data, 31, player.fastReload);
    putWord(data, 35, player.fireProof);
    data[39] = player.maximumHealth;
    data[40] = player.maximumArmour;
    putWord(data, 41, player.freePolice);
    putWord(data, 45, player.freeHospital);
    auto& simple = changed.blocks[0];
    simple[28] = player.hour;
    simple[29] = player.minute;
    putWord(simple, 32, player.savedMilliseconds);
    auto& stat = changed.blocks[4];
    for (const auto& field : lcsStatisticFields) {
        if (field.type == 's') {
            const auto begin = stat.begin() + field.offset;
            const std::string previous(begin, std::find(begin, begin + field.count, uint8_t(0)));
            if (previous == lastMission) continue;
            std::fill_n(stat.begin() + field.offset, field.count, 0);
            for (size_t index = 0; index < lastMission.size(); ++index) {
                const unsigned char character = static_cast<unsigned char>(lastMission[index]);
                if (character < 32 || character > 126) throw std::runtime_error("Invalid LCS mission name.");
                stat[field.offset + index] = character;
            }
            continue;
        }
        const size_t width = field.type == 'c' ? 1 : field.type == 'h' ? 2 : 4;
        for (size_t index = 0; index < field.count; ++index) {
            std::string key = field.name;
            if (field.count > 1) key += "[" + std::to_string(index) + "]";
            const auto entry = statistics.find(key);
            if (entry == statistics.end() || !std::isfinite(entry->second))
                throw std::runtime_error("Missing or invalid LCS statistic: " + key);
            const double value = entry->second;
            const size_t offset = field.offset + index * width;
            if (field.type == 'f') {
                if (std::fabs(value) > std::numeric_limits<float>::max())
                    throw std::runtime_error("LCS statistic exceeds its native range.");
                const float scalar = float(value);
                uint32_t bits;
                std::memcpy(&bits, &scalar, sizeof(bits));
                putWord(stat, offset, bits);
            } else {
                const double minimum = field.type == 'i' ? double(INT32_MIN) : 0;
                const double maximum = field.type == 'i' ? double(INT32_MAX) :
                    field.type == 'h' ? 65535 : field.type == 'c' ? 255 : 1;
                if (value < minimum || value > maximum || std::trunc(value) != value)
                    throw std::runtime_error("LCS statistic exceeds its native range.");
                const uint32_t bits = field.type == 'i' ? uint32_t(int32_t(value)) : uint32_t(value);
                for (size_t byte = 0; byte < width; ++byte) stat[offset + byte] = uint8_t(bits >> (byte * 8));
            }
        }
    }
    return encode(changed);
}
}
