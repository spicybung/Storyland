#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <map>
#include <string>

inline void storylandRecordLcsPedTypeKill(std::map<std::string, double>& statistics,
    int32_t type, bool classifiedAsCriminal) {
    const int32_t category = classifiedAsCriminal ? 18 : type;
    if (category < 0 || category >= 23) return;
    auto& statistic = statistics["PedsKilledOfThisType[" + std::to_string(category) + "]"];
    const double stored = statistic;
    const int32_t previous = stored >= INT32_MIN && stored <= INT32_MAX ? int32_t(stored) : 0;
    const uint32_t bits = uint32_t(previous) + 1u;
    statistic = bits <= uint32_t(INT32_MAX) ? double(bits) : double(int64_t(bits) - 0x100000000LL);
}

struct StorylandLcsEntityExtensionState {
    struct CollectiveMember {
        int32_t collective = -1;
        int32_t character = -1;
        template<class Archive> void saveState(Archive& archive) {
            archive(collective, character);
        }
    };
    std::array<CollectiveMember, 32> collectives{};
    int32_t nextCollective = 0;

    struct Task {
        int32_t objective = 0;
        int32_t target = -1;
        std::array<float, 3> destination{};
        float radius = 0.5f;
        float speed = 1.25f;
        int16_t route = -1;
        int16_t routeMode = 0;
        int16_t routeCursor = 1;
        int16_t routeDirection = 1;
        bool running = false;
        bool waitingForExit = false;
        uint32_t actionStartedAt = 0;
        float actionDuration = 0;
        template<class Archive> void saveState(Archive& archive) {
            archive(objective, target, destination, radius, speed, route, routeMode,
                routeCursor, routeDirection, running, waitingForExit, actionStartedAt, actionDuration);
        }
    };
    std::map<int32_t, Task> tasks;
    std::map<int32_t, bool> runningOverrides;

    bool valid() const {
        if (nextCollective < 0 || tasks.size() > 128 || runningOverrides.size() > 128) return false;
        for (const auto& entry : runningOverrides) if (entry.first < 1) return false;
        for (const auto& member : collectives) {
            if (member.collective < -1 || member.character < -1 ||
                (member.collective >= 0 && member.character < 2)) return false;
        }
        for (const auto& entry : tasks) {
            const auto& task = entry.second;
            if (entry.first < 1 || task.target < -1 || !std::isfinite(task.radius) ||
                task.radius < 0 || !std::isfinite(task.speed) || task.speed < 0 ||
                !std::isfinite(task.actionDuration) || task.actionDuration < 0 || task.actionDuration >= 60 ||
                task.routeMode < 0 || task.routeMode > 3 ||
                (task.routeDirection != 1 && task.routeDirection != -1)) return false;
            for (float coordinate : task.destination) if (!std::isfinite(coordinate)) return false;
            if (entry.first == 1 && (task.objective != 13 || task.target < 2 ||
                (task.radius != 2.0f && task.radius != 3.0f) || task.speed != 1.25f ||
                task.route != -1 || task.routeMode != 0 || task.routeCursor != 1 ||
                task.routeDirection != 1 || task.running || task.waitingForExit ||
                task.actionStartedAt != 0 || task.actionDuration != 0)) return false;
            if (entry.first != 1 && task.objective == 13) return false;
            switch (task.objective) {
            case 0: case 1: case 3: case 4: case 6: case 8: case 9: case 10: case 11:
            case 12: case 13: case 15: case 16: case 17: case 18: case 21: case 22: case 24:
            case 25: case 30: case 35: case 41: case 46: case 32: case 101: case 102:
            case 103: case 104: break;
            default: return false;
            }
        }
        return true;
    }

    template<class Archive> void saveState(Archive& archive) {
        archive(collectives, nextCollective, tasks, runningOverrides);
    }
};
