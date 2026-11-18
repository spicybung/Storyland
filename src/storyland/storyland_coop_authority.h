#pragma once
#include "src/storyland/storyland_play_test_weapons.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <limits>

class StorylandCoopAuthority {
public:
    struct Entity {
        uint64_t mission = 0, generation = 0;
        int32_t handle = -1, model = -1;
    };
    struct Player {
        bool connected = false, alive = true, arrested = false, policeIgnore = false;
        unsigned wanted = 0;
        StorylandNativePoint position{};
    };
    struct Hit {
        uint64_t sequence = 0;
        unsigned player = 0;
        Entity target;
        StorylandMissionCharacterDamage damage;
    };
    explicit StorylandCoopAuthority(StorylandPlayTestRuntime& runtime) : runtime(runtime) {}
    bool beginMission() {
        if (mission == std::numeric_limits<uint64_t>::max()) return false;
        ++mission; characters.clear(); received = {};
        return true;
    }
    Entity registerCharacter(int32_t handle) {
        if (!mission || !runtime.missionEnvironment || generation == std::numeric_limits<uint64_t>::max()) return {};
        const auto found = runtime.missionEnvironment->missionCharacters.find(handle);
        if (found == runtime.missionEnvironment->missionCharacters.end()) return {};
        Entity entity{mission, ++generation, handle, found->second.model};
        characters[handle] = entity;
        return entity;
    }
    void removeCharacter(int32_t handle) { characters.erase(handle); }
    uint64_t missionId() const { return mission; }
    const Entity* character(int32_t handle) const {
        const auto found = characters.find(handle);
        return found == characters.end() ? nullptr : &found->second;
    }
    bool setPlayer(unsigned index, const Player& value) {
        if (index >= players.size() || value.wanted > 6 || !finite(value.position)) return false;
        players[index] = value;
        return true;
    }
    int policeTarget(const StorylandNativePoint& officer, float range) const {
        return target(officer, range, 3, true);
    }
    // Only players named by the enemy's hostility mask may become its target.
    int enemyTarget(const StorylandNativePoint& enemy, float range, unsigned hostilePlayers) const {
        if (hostilePlayers > 3) return -1;
        return target(enemy, range, hostilePlayers, false);
    }
    bool applyHit(const Hit& hit, const std::function<bool(unsigned, const StorylandMissionCharacterDamage&)>& validateAction) {
        if (hit.player >= players.size() || !hit.sequence || !validateAction) return false;
        const auto& player = players[hit.player];
        if (!player.connected || !player.alive || player.arrested || hit.sequence <= received[hit.player]) return false;
        const auto found = characters.find(hit.target.handle);
        if (found == characters.end() || hit.target.mission != mission ||
            hit.target.generation != found->second.generation || hit.target.model != found->second.model ||
            hit.damage.handle != hit.target.handle || hit.damage.model != hit.target.model) return false;
        if (!std::isfinite(hit.damage.amount) || hit.damage.amount <= 0) return false;
        // Required host validation: ammunition, fire timing, range and collision trace.
        if (!validateAction(hit.player, hit.damage)) return false;
        // Guest kills must not award the host statistics, havoc or rampage progress.
        if (!storylandApplyMissionCharacterDamage(runtime, hit.damage, hit.player == 0)) return false;
        received[hit.player] = hit.sequence;
        return true;
    }
private:
    int target(const StorylandNativePoint& officer, float range, unsigned mask, bool police) const {
        if (!finite(officer) || !std::isfinite(range) || range < 0) return -1;
        int selected = -1;
        double nearest = double(range) * range;
        for (unsigned index = 0; index < players.size(); ++index) {
            const auto& player = players[index];
            if (!(mask & (1u << index)) || !player.connected || !player.alive || player.arrested) continue;
            if (police && (player.policeIgnore || !player.wanted)) continue;
            const double dx = double(player.position.x) - officer.x;
            const double dy = double(player.position.y) - officer.y;
            const double dz = double(player.position.z) - officer.z;
            const double distance = dx * dx + dy * dy + dz * dz;
            if (distance <= nearest && (selected < 0 || distance < nearest)) { selected = int(index); nearest = distance; }
        }
        return selected;
    }
    static bool finite(const StorylandNativePoint& point) {
        return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
    }
    StorylandPlayTestRuntime& runtime;
    uint64_t mission = 0, generation = 0;
    std::map<int32_t, Entity> characters;
    std::array<Player, 2> players{};
    std::array<uint64_t, 2> received{};
};
