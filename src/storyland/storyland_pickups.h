#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_play_test_weapons.h"
#include "src/re/reLCS2/runtime/storyland_lcs_messages.h"
#include <cctype>
#include <cstring>
#include <limits>

// Resolve through the loaded game's model directory; LCS and VCS model IDs differ.
inline std::string storylandPickupModelName(std::string name) {
    const auto slash=name.find_last_of("/\\");if(slash!=std::string::npos)name.erase(0,slash+1);
    const auto dot=name.find('.');if(dot!=std::string::npos)name.resize(dot);
    for(char& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
    return name;
}

inline void storylandRemoveAdrenalinePickup(StorylandPlayTestRuntime& runtime) {
    if(!runtime.missionEnvironment||runtime.adrenaline.pickupHandle<0)return;
    for(auto& pickup:runtime.missionEnvironment->pickups) {
        if(pickup.handle!=runtime.adrenaline.pickupHandle)continue;
        pickup.removed=true;
        pickup.unavailable=true;
        pickup.respawnAt=0;
        break;
    }
}

inline bool storylandPlaceAdrenalinePickup(StorylandPlayTestRuntime& runtime,int32_t model,
    const StorylandNativePoint& position,std::string& error) {
    error.clear();
    auto* world=runtime.missionEnvironment;
    if(!world||!runtime.crime.vcs||!runtime.adrenaline.enabled||model<0||
        !std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z)) {
        error="Enable VCS adrenaline and load its native model before placing a pickup.";
        return false;
    }
    auto found=std::find_if(world->pickups.begin(),world->pickups.end(),
        [&](const auto& pickup){return pickup.handle==runtime.adrenaline.pickupHandle;});
    if(found==world->pickups.end()) {
        if(world->nextPickupHandle<=0||world->nextPickupHandle==INT32_MAX||world->pickups.size()>=4096) {
            error="The pickup pool is full.";
            return false;
        }
        StorylandMissionEnvironment::Pickup pickup;
        pickup.handle=world->nextPickupHandle++;
        world->pickups.push_back(std::move(pickup));
        found=world->pickups.end()-1;
        runtime.adrenaline.pickupHandle=found->handle;
    }
    // One optional host-owned slot; repeated placement moves that slot.
    const int32_t handle=found->handle;
    *found={};
    found->handle=handle;
    found->model=model;
    found->type=3;
    found->x=position.x;found->y=position.y;found->z=position.z;
    found->area=world->area;
    auto& events=world->collectedPickupHandles;
    events.erase(std::remove(events.begin(),events.end(),handle),events.end());
    return true;
}
inline bool storylandNativePickupWeaponId(const std::vector<uint8_t>& bytes,
    int32_t model, int32_t& result, bool vcs = true) {
    const auto word = [&](size_t offset) {
        uint32_t value = 0;
        if (offset <= bytes.size() && bytes.size() - offset >= sizeof(value)) {
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
        }
        return value;
    };
    if (model == -1) {
        result = 0;
        return true;
    }
    if (!vcs && bytes.size() >= 0x60) {
        const uint32_t specialModels = word(0x5C);
        if (specialModels && uint64_t(specialModels) + 0x122 <= bytes.size()) {
            for (const auto& mapping : {std::pair<uint32_t, int32_t>{0x11C, 38},
                    {0x120, 37}, {0x11A, 38}}) {
                const size_t offset = size_t(specialModels) + mapping.first;
                const int32_t special = uint16_t(bytes[offset]) | uint16_t(bytes[offset + 1]) << 8;
                if (model == special) {
                    result = mapping.second;
                    return true;
                }
            }
        }
    }
    if (bytes.size() < 0x40 || model < 0 || uint32_t(model) >= word(0x38)) {
        return false;
    }
    const uint32_t table = word(0x3C);
    if (!table || uint64_t(table) + (uint64_t(uint32_t(model)) + 1) * 4 > bytes.size()) {
        return false;
    }
    const uint32_t info = word(size_t(table) + size_t(model) * 4);
    const uint32_t weaponOffset = vcs ? 0x3C : 0x38;
    if (!info || uint64_t(info) + weaponOffset + 4 > bytes.size() || (vcs && bytes[info + 0x10] != 4)) {
        return false;
    }
    // VCS 3FB4A0 reads +3C; LCS 333868 reads +38 after its special-model cases.
    result = int32_t(word(size_t(info) + weaponOffset));
    return true;
}

inline float storylandPickupEeFloat(double value) {
    const double maximum = std::numeric_limits<float>::max();
    value = std::clamp(value, -maximum, maximum);
    float result = float(value);
    if ((value > 0 && double(result) > value) || (value < 0 && double(result) < value))
        result = std::nextafter(result, 0.0f);
    if (std::fpclassify(result) == FP_SUBNORMAL) return std::copysign(0.0f, result);
    return result;
}

inline bool storylandPickupWithinSphere(const std::array<float,3>& point, float radius,
    float x, float y, float z) {
    const float dx = storylandPickupEeFloat(double(x) - point[0]);
    const float dy = storylandPickupEeFloat(double(y) - point[1]);
    const float dz = storylandPickupEeFloat(double(z) - point[2]);
    const float xx = storylandPickupEeFloat(double(dx) * dx);
    const float yy = storylandPickupEeFloat(double(dy) * dy);
    const float zz = storylandPickupEeFloat(double(dz) * dz);
    const float sum = storylandPickupEeFloat(double(storylandPickupEeFloat(double(xx) + yy)) + zz);
    return storylandPickupEeFloat(std::sqrt(double(sum))) < radius;
}

inline bool storylandAnyPickupInSphere(const StorylandMissionEnvironment& environment,
    const StorylandPlayTestRuntime& runtime, const std::array<float,3>& point, float radius) {
    for (const auto& pickup : environment.pickups) {
        if (!pickup.removed && pickup.type != 0 &&
            storylandPickupWithinSphere(point, radius, pickup.x, pickup.y, pickup.z)) return true;
    }
    for (const auto& pickup : runtime.pickups) {
        if (pickup.ammunition > 0 && storylandPickupWithinSphere(point, radius,
                pickup.position.x, pickup.position.y, pickup.position.z)) return true;
    }
    return false;
}

template<class Resident>
bool storylandFindPickupWeapon(StorylandMissionEnvironment& environment,
    const StorylandPlayTestRuntime& runtime, const std::vector<uint8_t>& game,
    const std::array<float,3>& point, float radius, Resident resident, int32_t& result) {
    result = -1;
    const bool vcs = runtime.crime.vcs;
    const auto within = [&](float x, float y, float z) {
        return storylandPickupWithinSphere(point, radius, x, y, z);
    };
    for (auto& pickup : environment.pickups) {
        if (pickup.removed || pickup.type == 0) continue;
        if (!vcs) {
            // Retail 25016C assigns model4 in a branch delay slot. The current
            // object keeps its model until the native pickup is recreated.
            pickup.regenerationModel = 4;
        }
        if ((vcs && pickup.type != 4) || pickup.unavailable ||
            pickup.area != runtime.visibleArea || !resident(pickup.model) ||
            !within(pickup.x, pickup.y, pickup.z)) {
            continue;
        }
        return storylandNativePickupWeaponId(game, pickup.model, result, vcs);
    }
    // Host drops and script pickups occupy separate containers. Their stable
    // creation order is used within each; native interleaved slot reuse is not
    // reconstructed. Random street pickups are a different native type.
    if (runtime.pickupsEnabled || !vcs) {
        for (const auto& pickup : runtime.pickups) {
            if ((vcs && pickup.randomlyGenerated) || pickup.respawn > 0 || pickup.ammunition <= 0 ||
                pickup.weapon >= runtime.weapons.size() ||
                !within(pickup.position.x, pickup.position.y, pickup.position.z)) {
                continue;
            }
            const int32_t model = runtime.weapons[pickup.weapon].modelId;
            if (resident(model)) {
                return storylandNativePickupWeaponId(game, model, result, vcs);
            }
        }
    }
    return true;
}

inline bool storylandLcsPickupActionPressed(const StorylandMissionEnvironment& environment) {
    if (!environment.control) return false;
    const size_t button = environment.controllerMode == 3 ? 15 :
        environment.controllerMode == 4 ? 17 : 4;
    if (environment.controllerMode == 2 || environment.controllerMode > 4) return false;
    if (environment.padState[button] && !environment.previousPadState[button]) return true;
    for (size_t frame = 0; frame < 6; ++frame) {
        if (environment.padMovementSamples[frame][button] &&
            !environment.padMovementSamples[frame + 1][button]) return true;
    }
    return false;
}

inline std::string storylandLcsPropertyLabel(std::string key, uint16_t controllerMode) {
    if (key.size() >= 3 && key[key.size() - 2] == '_') {
        if (controllerMode <= 1) key.back() = 'L';
        else if (controllerMode == 3) key.back() = 'T';
        else if (controllerMode == 4) key.back() = 'C';
    }
    return key;
}

inline void storylandShowLcsPickupHelp(StorylandMissionEnvironment& environment,
    const std::string& key, const std::vector<int32_t>& numbers = {}) {
    const bool newHelp = environment.helpMessage != key || !environment.helpMessageMilliseconds;
    environment.helpMessage = key;
    environment.helpMessageNumbers = numbers;
    environment.helpMessagePersistent = false;
    environment.helpMessageMilliseconds = 7000;
    if (newHelp && environment.audioSubsystemEnabled && environment.frontendAudioEnabled &&
        environment.scriptSounds.size() < 4096) {
        environment.scriptSounds.push_back({197, {environment.x, environment.y, environment.z}, true});
    }
}

inline void storylandAccrueLcsPickupRevenue(StorylandMissionEnvironment::Pickup& pickup,
    StorylandLcsPickupDetails& details, const StorylandMissionEnvironment& environment) {
    const uint32_t elapsed = environment.gameMilliseconds - details.updatedAt;
    details.updatedAt = environment.gameMilliseconds;
    const float dx = storylandPickupEeFloat(double(environment.x) - pickup.x);
    const float dy = storylandPickupEeFloat(double(environment.y) - pickup.y);
    const float dz = storylandPickupEeFloat(double(environment.z) - pickup.z);
    const float xy = storylandPickupEeFloat(double(storylandPickupEeFloat(double(dx) * dx)) +
        storylandPickupEeFloat(double(dy) * dy));
    const float distance = storylandPickupEeFloat(std::sqrt(double(storylandPickupEeFloat(
        double(xy) + storylandPickupEeFloat(double(dz) * dz)))));
    if (distance > 10.0f) {
        const uint32_t product = elapsed * uint32_t(details.revenueRate);
        const uint32_t rateBits = 0x353A69DC;
        float rate = 0;
        std::memcpy(&rate, &rateBits, sizeof(rate));
        const float amount = storylandPickupEeFloat(double(storylandPickupEeFloat(double(product))) * rate);
        pickup.revenue = storylandPickupEeFloat(double(pickup.revenue) + amount);
    }
    const float maximum = storylandPickupEeFloat(double(uint32_t(pickup.ammunition)));
    pickup.revenue = std::min(pickup.revenue, maximum);
}

inline void storylandRememberPickupCollection(StorylandMissionEnvironment& environment,
    StorylandMissionEnvironment::Pickup& pickup) {
    auto& events = environment.collectedPickupHandles;
    if (events.size() >= 20) {
        const int32_t discarded = events.front();
        events.pop_front();
        for (auto& previous : environment.pickups) {
            if (previous.handle == discarded) previous.collected = false;
        }
    }
    events.push_back(pickup.handle);
    pickup.collected = true;
}

template<class ModelName>
void storylandUpdateMissionPickups(StorylandMissionEnvironment& environment,
    StorylandPlayTestRuntime& runtime, ModelName modelName, bool vcs = false) {
    for (auto& pickup : environment.pickups) {
        if (pickup.removed || pickup.type == 0) continue;
        const uint32_t now = environment.gameMilliseconds;
        auto details = environment.lcsPickupDetails.find(pickup.handle);
        if (!vcs && pickup.type == 16 && details != environment.lcsPickupDetails.end()) {
            storylandAccrueLcsPickupRevenue(pickup, details->second, environment);
        }
        if (pickup.expiresAt && (vcs ? int32_t(now - pickup.expiresAt) >= 0 :
                int32_t(now - pickup.expiresAt) > 0)) {
            pickup.unavailable = true;
            if (!vcs) pickup.removed = true;
            continue;
        }
        const float dx = pickup.x - environment.x;
        const float dy = pickup.y - environment.y;
        const float dz = pickup.z - environment.z;
        const float planarDistance = dx * dx + dy * dy;
        if (pickup.unavailable) {
            const bool elapsed = pickup.respawnAt && (vcs ? int32_t(now - pickup.respawnAt) >= 0 :
                int32_t(now - pickup.respawnAt) > 0);
            if (!elapsed || planarDistance <= 100.0f) continue;
            pickup.unavailable = false;
            pickup.respawnAt = 0;
            if (!vcs && pickup.regenerationModel >= 0) {
                pickup.model = pickup.regenerationModel;
                pickup.regenerationModel = -1;
            }
        }
        const bool touching = planarDistance < (vcs ? 1.69f : 1.8f) &&
            (vcs ? std::fabs(dz) <= 2.0f : std::fabs(dz) < 2.0f);
        if ((!vcs && pickup.area != environment.area) || runtime.health <= 0 ||
            !environment.control || environment.cutscene.playing || !touching) {
            if (!vcs && details != environment.lcsPickupDetails.end()) details->second.helpShown = false;
            continue;
        }
        const std::string name = storylandPickupModelName(pickup.modelNameOverride.empty() ?
            modelName(pickup.model) : pickup.modelNameOverride);
        const bool bribe = name == "bribe";
        if (runtime.occupiedVehicle >= 0 && !bribe) continue;
        if (!vcs && pickup.type == 16) {
            if (std::isfinite(pickup.revenue) && pickup.revenue >= 10.0f) {
                const float money = storylandPickupEeFloat(double(runtime.money));
                const float total = storylandPickupEeFloat(double(money) + pickup.revenue);
                runtime.money = int32_t(std::clamp(std::trunc(double(total)),
                    double(INT32_MIN), double(INT32_MAX)));
                pickup.revenue = 0;
                StorylandPlayTestSoundEvent played(StorylandPlayTestSound::WeaponPickup);
                played.position = {pickup.x, pickup.y, pickup.z};
                played.positioned = false;
                runtime.soundEvents.push_back(played);
            }
            continue;
        }
        const bool property = !vcs && (pickup.type == 17 || pickup.type == 18);
        const bool hiddenPackage = !vcs && pickup.type == 6;
        if (pickup.type != 2 && pickup.type != 3 && pickup.type != 4 && pickup.type != 5 &&
            pickup.type != 8 && pickup.type != (vcs ? 11 : 15) && !hiddenPackage && !property) continue;
        bool taken = false;
        if (property) {
            if (details == environment.lcsPickupDetails.end()) continue;
            auto& metadata = details->second;
            if (pickup.type == 17) {
                if (!metadata.helpShown) storylandShowLcsPickupHelp(environment, metadata.textKey);
                metadata.helpShown = true;
                continue;
            }
            const std::string key = storylandLcsPropertyLabel(metadata.textKey, environment.controllerMode);
            if (environment.helpMessage.empty() || !environment.helpMessageMilliseconds)
                storylandShowLcsPickupHelp(environment, key, {pickup.ammunition});
            if (!storylandLcsPickupActionPressed(environment)) continue;
            if (environment.lcsMissionFlagSet && environment.lcsMissionFlagSet()) {
                storylandShowLcsPickupHelp(environment, "PROP_2");
                continue;
            }
            if (runtime.money < pickup.ammunition) {
                storylandShowLcsPickupHelp(environment, "PROP_1");
                continue;
            }
            runtime.money -= pickup.ammunition;
            environment.helpMessage.clear();
            environment.helpMessageNumbers.clear();
            environment.helpMessagePersistent = false;
            environment.helpMessageMilliseconds = 0;
            taken = true;
        } else if (hiddenPackage) {
            const auto statistic = [&](const char* key) {
                const auto found = environment.statistics.find(key);
                return found != environment.statistics.end() && std::isfinite(found->second) ?
                    int32_t(std::clamp(found->second, 0.0, double(INT32_MAX))) : 0;
            };
            const int32_t collected = int32_t(std::min<int64_t>(INT32_MAX,
                int64_t(statistic("CollectedPackages")) + 1));
            const int32_t total = statistic("TotalPackages");
            environment.statistics["CollectedPackages"] = collected;
            runtime.statistics["CollectedPackages"] = collected;
            const bool complete = total > 0 && collected == total;
            runtime.money = int(std::min<int64_t>(INT32_MAX,
                int64_t(runtime.money) + (complete ? 100100 : 100)));
            StorylandMissionEnvironment::LcsMessage notice;
            notice.key = complete ? "CO_ALL" : "CO_ONE";
            notice.missionText = environment.missionText;
            notice.numbers = complete ? std::vector<int32_t>{} : std::vector<int32_t>{collected, total};
            notice.durationMilliseconds = 5000;
            storylandQueueLcsMessage(environment, std::move(notice));
            if (environment.audioSubsystemEnabled && environment.scriptSounds.size() < 4096)
                environment.scriptSounds.push_back({1, {pickup.x, pickup.y, pickup.z}});
            taken = true;
        } else if (name == "health" && (vcs ? runtime.health < runtime.maximumHealth() :
                runtime.health <= runtime.maximumHealth() - 0.2f)) {
            runtime.health = runtime.maximumHealth();
            taken = true;
        } else if (name == "bodyarmour" && (vcs ? runtime.armour < runtime.maximumArmour() :
                runtime.armour <= runtime.maximumArmour() - 0.2f)) {
            runtime.armour = runtime.maximumArmour();
            taken = true;
        } else if (bribe && runtime.wanted > 0) {
            --runtime.wanted;
            runtime.crime.synchronize(runtime.wanted);
            taken = true;
        } else if (name == "adrenaline") {
            std::string error;
            taken = storylandActivateAdrenaline(runtime, vcs, error);
        } else if (name == "pickupsave" || name == "info" ||
            (!vcs && (name == "bonus" || name == "clothesp"))) {
            taken = true;
        } else if (!vcs && name == "killfrenzy" && !runtime.killFrenzy.active() &&
            (!environment.lcsMissionFlagSet || !environment.lcsMissionFlagSet())) {
            taken = true;
        } else if (pickup.type == 8) {
            runtime.money = int(std::clamp<int64_t>(int64_t(runtime.money) + pickup.ammunition, 0, INT32_MAX));
            taken = true;
        } else {
            bool weaponPickup = false;
            for (size_t index = 1; index < runtime.weapons.size(); ++index) {
                const auto& weapon = runtime.weapons[index];
                if (weapon.modelId != pickup.model && (name.empty() ||
                    storylandPickupModelName(modelName(weapon.modelId)) != name)) continue;
                weaponPickup = true;
                if (!vcs && runtime.killFrenzy.active()) break;
                const int32_t ammunition = pickup.ammunition != 0 ? pickup.ammunition : vcs ?
                    std::max(1, weapon.clipSize * 3) :
                    storylandLcsPickupDefaultAmmo(weapon.nativeId, pickup.type);
                taken = storylandGiveWeapon(runtime, index, ammunition);
                break;
            }
            // Native1DF144 collects a type3 prop whose model maps to weapon0
            // without granting inventory. Full health, armour and bribes retain
            // their eligibility checks from the native pickup update.
            if (vcs && pickup.type == 3 && !weaponPickup && !name.empty() &&
                name != "health" && name != "bodyarmour" && name != "bribe") {
                taken = true;
            }
        }
        if (!taken) continue;
        storylandRememberPickupCollection(environment, pickup);
        pickup.unavailable = true;
        if (pickup.type == 2) pickup.respawnAt = now + 30000u;
        else if (pickup.type == (vcs ? 11 : 15))
            pickup.respawnAt = now + (bribe ? 300000u : vcs ? 720000u : 360000u);
        if (!hiddenPackage) {
            StorylandPlayTestSoundEvent played(StorylandPlayTestSound::WeaponPickup);
            played.position = {pickup.x, pickup.y, pickup.z};
            played.positioned = false;
            runtime.soundEvents.push_back(played);
        }
    }
    environment.health = runtime.health;
    environment.armour = runtime.armour;
    environment.money = runtime.money;
    environment.wanted = runtime.wanted;
}
