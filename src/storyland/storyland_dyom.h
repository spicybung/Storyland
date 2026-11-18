#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct StorylandMissionEnvironment;
class StorylandMissionVm;

enum class StorylandDyomEntityKind { Actor, Vehicle, Object, Pickup };
enum class StorylandDyomActorBehavior { Idle, AttackPlayer, FollowPlayer };
enum class StorylandDyomObjectiveKind {
    ReachPoint,
    ReachPointOnFoot,
    ReachPointInVehicle,
    EnterVehicle,
    DefeatActor,
    DestroyVehicle,
    CollectPickup,
    Wait,
    Survive
};

struct StorylandDyomPosition {
    float x = 0;
    float y = 0;
    float z = 0;
    float heading = 0;
};

struct StorylandDyomPlayer {
    StorylandDyomPosition position;
    int health = 100;
    int armour = 0;
    int weapon = 0;
    int ammunition = 0;
};

struct StorylandDyomEntity {
    std::string id;
    std::string name;
    StorylandDyomEntityKind kind = StorylandDyomEntityKind::Actor;
    int32_t model = 0;
    StorylandDyomPosition position;
    int health = 100;
    int weapon = 0;
    int ammunition = 0;
    int pedestrianType = 4;
    StorylandDyomActorBehavior behavior = StorylandDyomActorBehavior::Idle;
    int primaryColor = -1;
    int secondaryColor = -1;
    int pickupType = 3;
    bool mustSurvive = false;
};

struct StorylandDyomObjective {
    std::string id;
    std::string text;
    StorylandDyomObjectiveKind kind = StorylandDyomObjectiveKind::ReachPoint;
    StorylandDyomPosition position;
    float radius = 3;
    std::string target;
    float durationSeconds = 5;
    float timeLimitSeconds = 0;
};

struct StorylandDyomProject {
    uint32_t version = 1;
    std::string game = "VCS";
    std::string title = "Untitled mission";
    std::string author;
    std::string description;
    StorylandDyomPlayer player;
    int hour = 12;
    int minute = 0;
    int weather = 0;
    int reward = 0;
    float timeLimitSeconds = 0;
    std::string successText = "Mission passed";
    std::string failureText = "Mission failed";
    std::vector<StorylandDyomEntity> entities;
    std::vector<StorylandDyomObjective> objectives;
};

struct StorylandDyomIssue {
    std::string field;
    std::string message;
    bool warning = false;
};

struct StorylandDyomModelRequirement {
    int32_t model = 0;
    StorylandDyomEntityKind kind = StorylandDyomEntityKind::Actor;
    std::string entityId;
};

struct StorylandDyomEntityBinding {
    std::string id;
    StorylandDyomEntityKind kind = StorylandDyomEntityKind::Actor;
    unsigned local = 0;
};

struct StorylandDyomObjectiveRange {
    std::string id;
    std::string textKey;
    size_t begin = 0;
    size_t end = 0;
};

struct StorylandDyomCompiled {
    std::string title;
    std::vector<uint8_t> bytecode;
    std::vector<StorylandDyomModelRequirement> requiredModels;
    std::vector<int32_t> requiredWeapons;
    std::map<std::string, std::string> text;
    std::vector<StorylandDyomEntityBinding> entities;
    std::vector<StorylandDyomObjectiveRange> objectives;
    size_t successOffset = 0;
    size_t failureOffset = 0;
    size_t cleanupOffset = 0;
    static constexpr unsigned playerLocal = 0;
    static constexpr unsigned objectiveLocal = 90;
    static constexpr unsigned resultLocal = 91;

    int objectiveAt(size_t programCounter) const;
};

const char* storylandDyomEntityKindName(StorylandDyomEntityKind kind);
const char* storylandDyomActorBehaviorName(StorylandDyomActorBehavior behavior);
const char* storylandDyomObjectiveKindName(StorylandDyomObjectiveKind kind);
std::vector<StorylandDyomIssue> storylandValidateDyomProject(
    const StorylandDyomProject& project, bool requirePlayable = true);
bool storylandParseDyomProject(const std::string& json, StorylandDyomProject& project,
    std::string& error);
bool storylandSerializeDyomProject(const StorylandDyomProject& project, std::string& json,
    std::string& error);
bool storylandLoadDyomProject(const std::filesystem::path& path, StorylandDyomProject& project,
    std::string& error);
bool storylandSaveDyomProject(const std::filesystem::path& path, const StorylandDyomProject& project,
    std::string& error);
bool storylandCompileDyomVcs(const StorylandDyomProject& project, StorylandDyomCompiled& compiled,
    std::string& error);
bool storylandStartDyomVcs(const StorylandDyomCompiled& compiled, StorylandMissionVm& mission,
    StorylandMissionEnvironment& environment, std::string& error);
bool storylandAbortDyomVcs(const StorylandDyomCompiled& compiled, StorylandMissionVm& mission,
    std::string& error);
