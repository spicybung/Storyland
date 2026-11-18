#include "src/storyland/storyland_dyom.h"
#include "src/storyland/storyland_mission_vm.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_character_handles.h"
#include "src/storyland/storyland_atomic_io.h"
#include "vendors/nlohmann/json.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {
using Json = nlohmann::json;
constexpr size_t maximumDocumentBytes = 4 * 1024 * 1024;
constexpr size_t maximumEntities = 64;
constexpr size_t maximumActors = 24;
constexpr size_t maximumVehicles = 16;
constexpr size_t maximumObjectives = 256;
constexpr unsigned blipLocal = 80;
constexpr unsigned sphereLocal = 81;
constexpr unsigned savedWeatherLocal = 92;
constexpr unsigned timerA = 104;
constexpr unsigned timerB = 105;

bool validIdentifier(const std::string& value) {
    if (value.empty() || value.size() > 48) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
            (character >= '0' && character <= '9') || character == '_' || character == '-';
    });
}

bool hasEmbeddedControl(const std::string& text) {
    return std::any_of(text.begin(), text.end(), [](unsigned char character) {
        return (character < 32 && character != '\n' && character != '\t' && character != '\r') || character == 127;
    });
}

std::string issueText(const std::vector<StorylandDyomIssue>& issues) {
    std::ostringstream output;
    for (const auto& issue : issues) {
        if (issue.warning) continue;
        if (output.tellp() > 0) output << '\n';
        output << issue.field << ": " << issue.message;
    }
    return output.str();
}

void checkKeys(const Json& value, const std::string& field, std::initializer_list<const char*> keys) {
    if (!value.is_object()) throw std::runtime_error(field + " must be an object.");
    for (auto it = value.begin(); it != value.end(); ++it) {
        const bool known = std::any_of(keys.begin(), keys.end(), [&](const char* key) { return it.key() == key; });
        if (!known) throw std::runtime_error(field + "." + it.key() + " is not a version 1 field.");
    }
}

const Json& required(const Json& value, const char* key, std::string_view field) {
    const auto found = value.find(key);
    if (found == value.end()) throw std::runtime_error(std::string(field) + "." + key + " is required.");
    return *found;
}

std::string stringValue(const Json& value, const std::string& field) {
    if (!value.is_string()) throw std::runtime_error(field + " must be a string.");
    return value.get<std::string>();
}

int integerValue(const Json& value, const std::string& field) {
    if (!value.is_number_integer()) throw std::runtime_error(field + " must be an integer.");
    if (value.is_number_unsigned()) {
        const auto integer = value.get<uint64_t>();
        if (integer > uint64_t(std::numeric_limits<int>::max())) throw std::runtime_error(field + " is too large.");
        return int(integer);
    }
    const auto integer = value.get<int64_t>();
    if (integer < std::numeric_limits<int>::min() || integer > std::numeric_limits<int>::max())
        throw std::runtime_error(field + " is outside the 32-bit integer range.");
    return int(integer);
}

float realValue(const Json& value, const std::string& field) {
    if (!value.is_number()) throw std::runtime_error(field + " must be a finite number.");
    const double real = value.get<double>();
    if (!std::isfinite(real) || std::abs(real) > double(std::numeric_limits<float>::max()))
        throw std::runtime_error(field + " must be a finite 32-bit number.");
    return float(real);
}

bool boolValue(const Json& value, const std::string& field) {
    if (!value.is_boolean()) throw std::runtime_error(field + " must be true or false.");
    return value.get<bool>();
}

template<typename T, typename Reader>
void optionalValue(const Json& value, const char* key, const std::string& field, T& destination, Reader reader) {
    const auto found = value.find(key);
    if (found != value.end()) destination = reader(*found, field + "." + key);
}

template<typename T>
T enumValue(const Json& value, const std::string& field, std::initializer_list<std::pair<const char*, T>> names) {
    const auto name = stringValue(value, field);
    for (const auto& entry : names) if (name == entry.first) return entry.second;
    throw std::runtime_error(field + " has an unknown value: " + name);
}

StorylandDyomPosition parsePosition(const Json& value, const std::string& field) {
    checkKeys(value, field, {"x", "y", "z", "heading"});
    StorylandDyomPosition position;
    position.x = realValue(required(value, "x", field), field + ".x");
    position.y = realValue(required(value, "y", field), field + ".y");
    position.z = realValue(required(value, "z", field), field + ".z");
    optionalValue(value, "heading", field, position.heading, realValue);
    return position;
}

Json positionJson(const StorylandDyomPosition& position) {
    return {{"x", position.x}, {"y", position.y}, {"z", position.z}, {"heading", position.heading}};
}

int32_t milliseconds(float seconds) {
    return int32_t(std::ceil(double(seconds) * 1000.0));
}

struct NativeWriter {
    struct Fixup { size_t offset; size_t label; };
    std::vector<uint8_t> bytes;
    std::vector<size_t> labels;
    std::vector<Fixup> fixups;

    size_t label() {
        labels.push_back(std::numeric_limits<size_t>::max());
        return labels.size() - 1;
    }
    void mark(size_t labelId) { labels.at(labelId) = bytes.size(); }
    void word(uint16_t value) {
        bytes.push_back(uint8_t(value));
        bytes.push_back(uint8_t(value >> 8));
    }
    void bits(uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(uint8_t(value >> shift));
    }
    void integer(int32_t value) { bytes.push_back(6); bits(uint32_t(value)); }
    void real(float value) {
        uint32_t representation = 0;
        std::memcpy(&representation, &value, sizeof(value));
        bytes.push_back(9);
        bits(representation);
    }
    void local(unsigned slot) {
        if (slot == timerA) bytes.push_back(11);
        else if (slot == timerB) bytes.push_back(12);
        else if (slot < 96) bytes.push_back(uint8_t(13 + slot));
        else throw std::runtime_error("Compiler requested an unsupported VCS local slot.");
    }
    void text(const std::string& value) {
        bytes.push_back(10);
        bytes.insert(bytes.end(), value.begin(), value.end());
        bytes.push_back(0);
    }
    void position(const StorylandDyomPosition& position) { real(position.x); real(position.y); real(position.z); }
    void set(unsigned slot, int32_t value) { word(0x0004); local(slot); integer(value); }
    void jump(uint16_t opcode, size_t labelId) {
        word(opcode);
        bytes.push_back(6);
        fixups.push_back({bytes.size(), labelId});
        bits(0);
    }
    void wait() { word(0x0001); integer(0); }
    void finish() {
        for (const auto& fixup : fixups) {
            const size_t destination = labels.at(fixup.label);
            if (destination == 0 || destination >= bytes.size() || destination > size_t(INT32_MAX))
                throw std::runtime_error("Compiler produced an invalid native mission label.");
            const uint32_t encoded = uint32_t(-int32_t(destination));
            for (unsigned index = 0; index < 4; ++index) bytes.at(fixup.offset + index) = uint8_t(encoded >> (index * 8));
        }
    }
};

void releaseMarker(NativeWriter& code, uint16_t opcode, unsigned slot) {
    const size_t skip = code.label();
    code.word(0x0015); code.local(slot); code.integer(0);
    code.jump(0x0022, skip);
    code.word(opcode); code.local(slot);
    code.set(slot, -1);
    code.mark(skip);
}

void print(NativeWriter& code, const std::string& key, bool big) {
    code.word(big ? 0x0055 : 0x0057);
    code.text(key);
    code.integer(big ? 4900 : 86400000);
    code.integer(big ? 1 : 0);
}

bool pointObjective(StorylandDyomObjectiveKind kind) {
    return kind == StorylandDyomObjectiveKind::ReachPoint || kind == StorylandDyomObjectiveKind::ReachPointOnFoot ||
        kind == StorylandDyomObjectiveKind::ReachPointInVehicle;
}

bool timedObjective(StorylandDyomObjectiveKind kind) {
    return kind == StorylandDyomObjectiveKind::Wait || kind == StorylandDyomObjectiveKind::Survive;
}

bool targetedObjective(StorylandDyomObjectiveKind kind) {
    return kind == StorylandDyomObjectiveKind::EnterVehicle || kind == StorylandDyomObjectiveKind::DefeatActor ||
        kind == StorylandDyomObjectiveKind::DestroyVehicle || kind == StorylandDyomObjectiveKind::CollectPickup;
}

bool projectPath(const std::filesystem::path& path, std::string& error) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
        return character >= 'A' && character <= 'Z' ? char(character - 'A' + 'a') : char(character);
    });
    if (extension != ".dyom" && extension != ".dyomvcs") {
        error = "DYOM projects must use the .dyom or legacy .dyomvcs extension.";
        return false;
    }
    return true;
}
}

const char* storylandDyomEntityKindName(StorylandDyomEntityKind kind) {
    switch (kind) {
    case StorylandDyomEntityKind::Actor: return "actor";
    case StorylandDyomEntityKind::Vehicle: return "vehicle";
    case StorylandDyomEntityKind::Object: return "object";
    case StorylandDyomEntityKind::Pickup: return "pickup";
    }
    return "unknown";
}

const char* storylandDyomActorBehaviorName(StorylandDyomActorBehavior behavior) {
    switch (behavior) {
    case StorylandDyomActorBehavior::Idle: return "idle";
    case StorylandDyomActorBehavior::AttackPlayer: return "attack_player";
    case StorylandDyomActorBehavior::FollowPlayer: return "follow_player";
    }
    return "unknown";
}

const char* storylandDyomObjectiveKindName(StorylandDyomObjectiveKind kind) {
    switch (kind) {
    case StorylandDyomObjectiveKind::ReachPoint: return "reach_point";
    case StorylandDyomObjectiveKind::ReachPointOnFoot: return "reach_point_on_foot";
    case StorylandDyomObjectiveKind::ReachPointInVehicle: return "reach_point_in_vehicle";
    case StorylandDyomObjectiveKind::EnterVehicle: return "enter_vehicle";
    case StorylandDyomObjectiveKind::DefeatActor: return "defeat_actor";
    case StorylandDyomObjectiveKind::DestroyVehicle: return "destroy_vehicle";
    case StorylandDyomObjectiveKind::CollectPickup: return "collect_pickup";
    case StorylandDyomObjectiveKind::Wait: return "wait";
    case StorylandDyomObjectiveKind::Survive: return "survive";
    }
    return "unknown";
}

int StorylandDyomCompiled::objectiveAt(size_t programCounter) const {
    for (size_t index = 0; index < objectives.size(); ++index)
        if (programCounter >= objectives[index].begin && programCounter < objectives[index].end) return int(index);
    return -1;
}

std::vector<StorylandDyomIssue> storylandValidateDyomProject(const StorylandDyomProject& project, bool requirePlayable) {
    std::vector<StorylandDyomIssue> issues;
    const auto issue = [&](const std::string& field, const std::string& message, bool incomplete = false) {
        issues.push_back({field, message, incomplete && !requirePlayable});
    };
    const auto integer = [&](int value, int low, int high, const std::string& field) {
        if (value < low || value > high) issue(field, "Expected " + std::to_string(low) + ".." + std::to_string(high) + ".");
    };
    const auto real = [&](float value, float low, float high, const std::string& field) {
        if (!std::isfinite(value) || value < low || value > high)
            issue(field, "Expected a finite number in " + std::to_string(low) + ".." + std::to_string(high) + ".");
    };
    const auto text = [&](const std::string& value, size_t limit, const std::string& field) {
        if (value.size() > limit) issue(field, "Text exceeds " + std::to_string(limit) + " UTF-8 bytes.");
        if (hasEmbeddedControl(value)) issue(field, "Text contains an unsupported control character.");
    };
    const auto position = [&](const StorylandDyomPosition& value, const std::string& field) {
        real(value.x, -50000, 50000, field + ".x");
        real(value.y, -50000, 50000, field + ".y");
        real(value.z, -10000, 10000, field + ".z");
        real(value.heading, -360000, 360000, field + ".heading");
    };
    if (project.version != 1) issue("version", "Only project schema version 1 is supported.");
    if (project.game != "VCS") issue("game", "DYOM supports VCS only.");
    text(project.title, 128, "title");
    if (project.title.empty()) issue("title", "A mission title is required to play.", true);
    text(project.author, 128, "author");
    text(project.description, 4096, "description");
    text(project.successText, 2048, "successText");
    text(project.failureText, 2048, "failureText");
    position(project.player.position, "player.position");
    integer(project.player.health, 1, 1000, "player.health");
    integer(project.player.armour, 0, 100, "player.armour");
    integer(project.player.weapon, 0, 255, "player.weapon");
    integer(project.player.ammunition, 0, 9999, "player.ammunition");
    integer(project.hour, 0, 23, "hour");
    integer(project.minute, 0, 59, "minute");
    integer(project.weather, 0, 7, "weather");
    integer(project.reward, 0, 1000000, "reward");
    real(project.timeLimitSeconds, 0, 86400, "timeLimitSeconds");
    if (project.entities.size() > maximumEntities) issue("entities", "At most 64 entities can be authored in one mission.");
    if (project.objectives.size() > maximumObjectives) issue("objectives", "At most 256 objectives can be authored in one mission.");
    if (project.objectives.empty()) issue("objectives", "Add at least one objective to play the mission.", true);
    std::map<std::string, const StorylandDyomEntity*> entities;
    size_t actorCount = 0, vehicleCount = 0;
    for (size_t index = 0; index < project.entities.size(); ++index) {
        const auto& entity = project.entities[index];
        const auto field = "entities[" + std::to_string(index) + "]";
        if (!validIdentifier(entity.id)) issue(field + ".id", "Use 1..48 ASCII letters, digits, underscores or hyphens.");
        if (!entities.emplace(entity.id, &entity).second) issue(field + ".id", "Entity IDs must be unique.");
        text(entity.name, 128, field + ".name");
        if (std::string(storylandDyomEntityKindName(entity.kind)) == "unknown") issue(field + ".kind", "Unknown entity kind.");
        if (entity.model < 0 || entity.model > 65535) issue(field + ".model", "Model IDs must be in 1..65535.");
        else if (entity.model == 0) issue(field + ".model", "Choose a native model before playing.", true);
        position(entity.position, field + ".position");
        integer(entity.health, 1, entity.kind == StorylandDyomEntityKind::Vehicle ? 10000 : 1000, field + ".health");
        integer(entity.weapon, 0, 255, field + ".weapon");
        integer(entity.ammunition, 0, 9999, field + ".ammunition");
        integer(entity.pedestrianType, 1, 21, field + ".pedestrianType");
        if (std::string(storylandDyomActorBehaviorName(entity.behavior)) == "unknown") issue(field + ".behavior", "Unknown actor behavior.");
        integer(entity.primaryColor, -1, 255, field + ".primaryColor");
        integer(entity.secondaryColor, -1, 255, field + ".secondaryColor");
        if ((entity.primaryColor < 0) != (entity.secondaryColor < 0))
            issue(field, "Set both vehicle colors, or leave both at -1 for the native colors.");
        integer(entity.pickupType, 1, 23, field + ".pickupType");
        if (entity.kind == StorylandDyomEntityKind::Pickup && entity.pickupType != 2 && entity.pickupType != 3 &&
            entity.pickupType != 4 && entity.pickupType != 5 && entity.pickupType != 8 && entity.pickupType != 11)
            issue(field + ".pickupType", "Playable VCS pickup types are 2, 3, 4, 5, 8 and 11.");
        if (entity.kind == StorylandDyomEntityKind::Actor) ++actorCount;
        if (entity.kind == StorylandDyomEntityKind::Vehicle) ++vehicleCount;
        if (entity.mustSurvive && entity.kind != StorylandDyomEntityKind::Actor && entity.kind != StorylandDyomEntityKind::Vehicle)
            issue(field + ".mustSurvive", "Only actors and vehicles can be protected mission targets.");
    }
    if (actorCount > maximumActors) issue("entities", "At most 24 authored actors are supported.");
    if (vehicleCount > maximumVehicles) issue("entities", "At most 16 authored vehicles are supported.");
    std::set<std::string> objectiveIds;
    for (size_t index = 0; index < project.objectives.size(); ++index) {
        const auto& objective = project.objectives[index];
        const auto field = "objectives[" + std::to_string(index) + "]";
        if (!validIdentifier(objective.id)) issue(field + ".id", "Use 1..48 ASCII letters, digits, underscores or hyphens.");
        if (!objectiveIds.insert(objective.id).second) issue(field + ".id", "Objective IDs must be unique.");
        text(objective.text, 2048, field + ".text");
        if (objective.text.empty()) issue(field + ".text", "Provide instructions for the player.", true);
        if (std::string(storylandDyomObjectiveKindName(objective.kind)) == "unknown") issue(field + ".kind", "Unknown objective kind.");
        position(objective.position, field + ".position");
        real(objective.radius, 0.1f, 1000, field + ".radius");
        real(objective.durationSeconds, 0, 86400, field + ".durationSeconds");
        real(objective.timeLimitSeconds, 0, 86400, field + ".timeLimitSeconds");
        if (timedObjective(objective.kind) && objective.durationSeconds <= 0)
            issue(field + ".durationSeconds", "Wait and survive objectives require a positive duration.");
        if (timedObjective(objective.kind) && objective.timeLimitSeconds > 0 && objective.timeLimitSeconds < objective.durationSeconds)
            issue(field + ".timeLimitSeconds", "The time limit cannot expire before the required duration.");
        const bool allowsTarget = targetedObjective(objective.kind) || objective.kind == StorylandDyomObjectiveKind::ReachPointInVehicle;
        if (!allowsTarget && !objective.target.empty()) issue(field + ".target", "This objective kind does not use an entity target.");
        if (targetedObjective(objective.kind) && objective.target.empty()) {
            issue(field + ".target", "Choose the target entity before playing.", true);
            continue;
        }
        if (objective.target.empty()) continue;
        const auto target = entities.find(objective.target);
        if (target == entities.end()) { issue(field + ".target", "The referenced entity does not exist.", true); continue; }
        const auto expected = objective.kind == StorylandDyomObjectiveKind::DefeatActor ? StorylandDyomEntityKind::Actor :
            objective.kind == StorylandDyomObjectiveKind::CollectPickup ? StorylandDyomEntityKind::Pickup : StorylandDyomEntityKind::Vehicle;
        if (target->second->kind != expected) issue(field + ".target", "This objective requires a " + std::string(storylandDyomEntityKindName(expected)) + " target.", true);
        if (target->second->mustSurvive && (objective.kind == StorylandDyomObjectiveKind::DefeatActor || objective.kind == StorylandDyomObjectiveKind::DestroyVehicle))
            issue(field + ".target", "A protected target cannot also be an elimination objective.", true);
    }
    return issues;
}

bool storylandParseDyomProject(const std::string& source, StorylandDyomProject& project, std::string& error) {
    error.clear();
    if (source.size() > maximumDocumentBytes) { error = "DYOM project exceeds the 4 MiB limit."; return false; }
    try {
        std::map<int, std::set<std::string>> objectKeys;
        const auto rejectDuplicateKeys = [&](int depth, Json::parse_event_t event, Json& parsed) {
            if (depth > 16) throw std::runtime_error("DYOM JSON nesting is too deep.");
            if (event == Json::parse_event_t::object_start) objectKeys[depth + 1].clear();
            if (event == Json::parse_event_t::key && !objectKeys[depth].insert(parsed.get<std::string>()).second)
                throw std::runtime_error("Duplicate JSON field: " + parsed.get<std::string>());
            if (event == Json::parse_event_t::object_end) objectKeys.erase(depth + 1);
            return true;
        };
        const Json root = Json::parse(source, rejectDuplicateKeys);
        checkKeys(root, "project", {"format", "version", "game", "title", "author", "description", "player", "hour", "minute", "weather", "reward", "timeLimitSeconds", "successText", "failureText", "entities", "objectives"});
        if (stringValue(required(root, "format", "project"), "format") != "DYOMVCS")
            throw std::runtime_error("Unrecognized DYOM project format.");
        StorylandDyomProject draft;
        const int version = integerValue(required(root, "version", "project"), "version");
        if (version != 1) throw std::runtime_error("Only DYOM project version 1 is supported.");
        draft.version = uint32_t(version);
        draft.game = stringValue(required(root, "game", "project"), "game");
        draft.title = stringValue(required(root, "title", "project"), "title");
        optionalValue(root, "author", "project", draft.author, stringValue);
        optionalValue(root, "description", "project", draft.description, stringValue);
        optionalValue(root, "hour", "project", draft.hour, integerValue);
        optionalValue(root, "minute", "project", draft.minute, integerValue);
        optionalValue(root, "weather", "project", draft.weather, integerValue);
        optionalValue(root, "reward", "project", draft.reward, integerValue);
        optionalValue(root, "timeLimitSeconds", "project", draft.timeLimitSeconds, realValue);
        optionalValue(root, "successText", "project", draft.successText, stringValue);
        optionalValue(root, "failureText", "project", draft.failureText, stringValue);
        const auto& player = required(root, "player", "project");
        checkKeys(player, "player", {"position", "health", "armour", "weapon", "ammunition"});
        draft.player.position = parsePosition(required(player, "position", "player"), "player.position");
        optionalValue(player, "health", "player", draft.player.health, integerValue);
        optionalValue(player, "armour", "player", draft.player.armour, integerValue);
        optionalValue(player, "weapon", "player", draft.player.weapon, integerValue);
        optionalValue(player, "ammunition", "player", draft.player.ammunition, integerValue);
        const auto& entities = required(root, "entities", "project");
        const auto& objectives = required(root, "objectives", "project");
        if (!entities.is_array() || entities.size() > maximumEntities) throw std::runtime_error("entities must be an array with at most 64 entries.");
        if (!objectives.is_array() || objectives.size() > maximumObjectives) throw std::runtime_error("objectives must be an array with at most 256 entries.");
        for (const auto& value : entities) {
            const auto field = "entities[" + std::to_string(draft.entities.size()) + "]";
            checkKeys(value, field, {"id", "name", "kind", "model", "position", "health", "weapon", "ammunition", "pedestrianType", "behavior", "primaryColor", "secondaryColor", "pickupType", "mustSurvive"});
            StorylandDyomEntity entity;
            entity.id = stringValue(required(value, "id", field), field + ".id");
            entity.kind = enumValue<StorylandDyomEntityKind>(required(value, "kind", field), field + ".kind", {
                {"actor", StorylandDyomEntityKind::Actor}, {"vehicle", StorylandDyomEntityKind::Vehicle},
                {"object", StorylandDyomEntityKind::Object}, {"pickup", StorylandDyomEntityKind::Pickup}});
            entity.model = integerValue(required(value, "model", field), field + ".model");
            entity.position = parsePosition(required(value, "position", field), field + ".position");
            optionalValue(value, "name", field, entity.name, stringValue);
            optionalValue(value, "health", field, entity.health, integerValue);
            optionalValue(value, "weapon", field, entity.weapon, integerValue);
            optionalValue(value, "ammunition", field, entity.ammunition, integerValue);
            optionalValue(value, "pedestrianType", field, entity.pedestrianType, integerValue);
            optionalValue(value, "primaryColor", field, entity.primaryColor, integerValue);
            optionalValue(value, "secondaryColor", field, entity.secondaryColor, integerValue);
            optionalValue(value, "pickupType", field, entity.pickupType, integerValue);
            optionalValue(value, "mustSurvive", field, entity.mustSurvive, boolValue);
            const auto behavior = value.find("behavior");
            if (behavior != value.end()) entity.behavior = enumValue<StorylandDyomActorBehavior>(*behavior, field + ".behavior", {
                {"idle", StorylandDyomActorBehavior::Idle}, {"attack_player", StorylandDyomActorBehavior::AttackPlayer},
                {"follow_player", StorylandDyomActorBehavior::FollowPlayer}});
            draft.entities.push_back(std::move(entity));
        }
        for (const auto& value : objectives) {
            const auto field = "objectives[" + std::to_string(draft.objectives.size()) + "]";
            checkKeys(value, field, {"id", "text", "kind", "position", "radius", "target", "durationSeconds", "timeLimitSeconds"});
            StorylandDyomObjective objective;
            objective.id = stringValue(required(value, "id", field), field + ".id");
            objective.text = stringValue(required(value, "text", field), field + ".text");
            objective.kind = enumValue<StorylandDyomObjectiveKind>(required(value, "kind", field), field + ".kind", {
                {"reach_point", StorylandDyomObjectiveKind::ReachPoint}, {"reach_point_on_foot", StorylandDyomObjectiveKind::ReachPointOnFoot},
                {"reach_point_in_vehicle", StorylandDyomObjectiveKind::ReachPointInVehicle}, {"enter_vehicle", StorylandDyomObjectiveKind::EnterVehicle},
                {"defeat_actor", StorylandDyomObjectiveKind::DefeatActor}, {"destroy_vehicle", StorylandDyomObjectiveKind::DestroyVehicle},
                {"collect_pickup", StorylandDyomObjectiveKind::CollectPickup}, {"wait", StorylandDyomObjectiveKind::Wait},
                {"survive", StorylandDyomObjectiveKind::Survive}});
            const auto point = value.find("position");
            if (point != value.end()) objective.position = parsePosition(*point, field + ".position");
            else if (pointObjective(objective.kind)) throw std::runtime_error(field + ".position is required for a checkpoint.");
            optionalValue(value, "radius", field, objective.radius, realValue);
            optionalValue(value, "target", field, objective.target, stringValue);
            optionalValue(value, "durationSeconds", field, objective.durationSeconds, realValue);
            optionalValue(value, "timeLimitSeconds", field, objective.timeLimitSeconds, realValue);
            draft.objectives.push_back(std::move(objective));
        }
        error = issueText(storylandValidateDyomProject(draft, false));
        if (!error.empty()) return false;
        project = std::move(draft);
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}

bool storylandSerializeDyomProject(const StorylandDyomProject& project, std::string& destination, std::string& error) {
    error = issueText(storylandValidateDyomProject(project, false));
    if (!error.empty()) return false;
    try {
        Json root = {{"format", "DYOMVCS"}, {"version", project.version}, {"game", project.game}, {"title", project.title},
            {"author", project.author}, {"description", project.description}, {"hour", project.hour}, {"minute", project.minute},
            {"weather", project.weather}, {"reward", project.reward}, {"timeLimitSeconds", project.timeLimitSeconds},
            {"successText", project.successText}, {"failureText", project.failureText},
            {"player", {{"position", positionJson(project.player.position)}, {"health", project.player.health},
                {"armour", project.player.armour}, {"weapon", project.player.weapon}, {"ammunition", project.player.ammunition}}},
            {"entities", Json::array()}, {"objectives", Json::array()}};
        for (const auto& entity : project.entities) root["entities"].push_back({
            {"id", entity.id}, {"name", entity.name}, {"kind", storylandDyomEntityKindName(entity.kind)},
            {"model", entity.model}, {"position", positionJson(entity.position)}, {"health", entity.health},
            {"weapon", entity.weapon}, {"ammunition", entity.ammunition}, {"pedestrianType", entity.pedestrianType},
            {"behavior", storylandDyomActorBehaviorName(entity.behavior)}, {"primaryColor", entity.primaryColor},
            {"secondaryColor", entity.secondaryColor}, {"pickupType", entity.pickupType}, {"mustSurvive", entity.mustSurvive}});
        for (const auto& objective : project.objectives) root["objectives"].push_back({
            {"id", objective.id}, {"text", objective.text}, {"kind", storylandDyomObjectiveKindName(objective.kind)},
            {"position", positionJson(objective.position)}, {"radius", objective.radius}, {"target", objective.target},
            {"durationSeconds", objective.durationSeconds}, {"timeLimitSeconds", objective.timeLimitSeconds}});
        std::string encoded = root.dump(2) + "\n";
        if (encoded.size() > maximumDocumentBytes) { error = "DYOM project exceeds the 4 MiB limit."; return false; }
        destination = std::move(encoded);
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}

bool storylandLoadDyomProject(const std::filesystem::path& path, StorylandDyomProject& project, std::string& error) {
    error.clear();
    if (!projectPath(path, error)) return false;
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) { error = "Cannot open DYOM project: " + path.string(); return false; }
    const auto length = stream.tellg();
    if (length < 0 || length > std::streamoff(maximumDocumentBytes)) { error = "DYOM project exceeds the 4 MiB limit or is unreadable."; return false; }
    std::string source(size_t(length), '\0');
    stream.seekg(0);
    if (!source.empty() && !stream.read(source.data(), std::streamsize(source.size()))) { error = "Cannot read the complete DYOM project."; return false; }
    return storylandParseDyomProject(source, project, error);
}

bool storylandSaveDyomProject(const std::filesystem::path& path, const StorylandDyomProject& project, std::string& error) {
    error.clear();
    if (!projectPath(path, error)) return false;
    std::string json;
    if (!storylandSerializeDyomProject(project, json, error)) return false;
    const std::vector<uint8_t> bytes(json.begin(), json.end());
    return storylandWriteFilesTransaction({{path, &bytes}}, error);
}

bool storylandCompileDyomVcs(const StorylandDyomProject& project, StorylandDyomCompiled& compiled, std::string& error) {
    error = issueText(storylandValidateDyomProject(project, true));
    if (!error.empty()) return false;
    try {
        StorylandDyomCompiled result;
        result.title = project.title;
        result.text["M_PASS"] = project.successText;
        result.text["M_FAIL"] = project.failureText;
        std::map<std::string, unsigned> entityLocals;
        std::set<int32_t> models, weapons;
        if (project.player.weapon > 0) weapons.insert(project.player.weapon);
        for (size_t index = 0; index < project.entities.size(); ++index) {
            const auto& entity = project.entities[index];
            const unsigned slot = unsigned(index + 1);
            entityLocals.emplace(entity.id, slot);
            result.entities.push_back({entity.id, entity.kind, slot});
            result.requiredModels.push_back({entity.model, entity.kind, entity.id});
            models.insert(entity.model);
            if (entity.kind == StorylandDyomEntityKind::Actor && entity.weapon > 0) weapons.insert(entity.weapon);
        }
        result.requiredWeapons.assign(weapons.begin(), weapons.end());
        NativeWriter code;
        const size_t success = code.label(), failure = code.label(), cleanup = code.label();
        code.word(0x0238); code.text("DYOM");
        code.set(StorylandDyomCompiled::objectiveLocal, -1);
        code.set(StorylandDyomCompiled::resultLocal, 0);
        code.set(blipLocal, -1); code.set(sphereLocal, -1);
        for (const auto& binding : result.entities) code.set(binding.local, -1);
        code.word(0x0166); code.word(0x0168);
        code.word(0x0109); code.integer(project.weather);
        code.word(0x005B); code.integer(project.hour); code.integer(project.minute);
        code.word(0x0044); code.local(0); code.position(project.player.position);
        code.word(0x00CF); code.local(0); code.real(project.player.position.heading);
        code.word(0x014B); code.local(0); code.integer(project.player.health);
        code.word(0x0213); code.local(0); code.integer(-1000000);
        if (project.player.armour) { code.word(0x0213); code.local(0); code.integer(project.player.armour); }
        code.word(0x02D4); code.local(0);
        if (project.player.weapon > 0) {
            code.word(0x0106); code.local(0); code.integer(project.player.weapon); code.integer(project.player.ammunition);
            code.word(0x010B); code.local(0); code.integer(project.player.weapon);
        }
        code.word(0x009B); code.local(0);
        code.word(0x0107); code.local(0); code.integer(1);
        if (!models.empty()) {
            for (const auto model : models) { code.word(0x0160); code.integer(model); }
            code.word(0x0228);
            code.set(timerB, 0);
            const size_t loading = code.label();
            code.mark(loading);
            code.wait();
            code.word(0x000F); code.local(timerB); code.integer(30000); code.jump(0x0021, failure);
            for (const auto model : models) {
                code.word(0x0161); code.integer(model);
                code.jump(0x0022, loading);
            }
        }
        for (size_t index = 0; index < project.entities.size(); ++index) {
            const auto& entity = project.entities[index];
            const unsigned slot = unsigned(index + 1);
            if (entity.kind == StorylandDyomEntityKind::Actor) {
                code.word(0x003E); code.integer(entity.pedestrianType); code.integer(entity.model); code.position(entity.position); code.local(slot);
                code.word(0x00CF); code.local(slot); code.real(entity.position.heading);
                code.word(0x014B); code.local(slot); code.integer(entity.health);
                if (entity.weapon > 0) {
                    code.word(0x0106); code.local(slot); code.integer(entity.weapon); code.integer(entity.ammunition);
                    code.word(0x010B); code.local(slot); code.integer(entity.weapon);
                }
                if (entity.behavior == StorylandDyomActorBehavior::AttackPlayer) { code.word(0x00E5); code.local(slot); code.local(0); }
                else if (entity.behavior == StorylandDyomActorBehavior::FollowPlayer) { code.word(0x00EF); code.local(slot); code.local(0); code.integer(0); }
                else { code.word(0x0042); code.local(slot); }
            } else if (entity.kind == StorylandDyomEntityKind::Vehicle) {
                code.word(0x0048); code.integer(entity.model); code.position(entity.position); code.local(slot);
                code.word(0x00D1); code.local(slot); code.real(entity.position.heading);
                code.word(0x014C); code.local(slot); code.integer(entity.health);
                if (entity.primaryColor >= 0) { code.word(0x0150); code.local(slot); code.integer(entity.primaryColor); code.integer(entity.secondaryColor); }
            } else if (entity.kind == StorylandDyomEntityKind::Object) {
                code.word(0x0092); code.integer(entity.model); code.position(entity.position); code.local(slot);
                code.word(0x00D3); code.local(slot); code.real(entity.position.heading);
            } else {
                code.word(0x013F); code.integer(entity.model); code.integer(entity.pickupType); code.position(entity.position); code.local(slot);
            }
        }
        code.set(timerA, 0);
        const auto failIfDead = [&](uint16_t opcode, unsigned slot) {
            code.word(opcode); code.local(slot); code.jump(0x0021, failure);
        };
        for (size_t index = 0; index < project.objectives.size(); ++index) {
            const auto& objective = project.objectives[index];
            const size_t loop = code.label(), done = code.label();
            const auto key = "DY" + std::to_string(1000 + index);
            result.text[key] = objective.text;
            StorylandDyomObjectiveRange range{objective.id, key, code.bytes.size(), 0};
            code.set(StorylandDyomCompiled::objectiveLocal, int32_t(index));
            print(code, key, false);
            code.set(timerB, 0);
            const unsigned target = objective.target.empty() ? 0 : entityLocals.at(objective.target);
            if (pointObjective(objective.kind)) {
                code.word(0x0248); code.position(objective.position); code.real(objective.radius); code.local(sphereLocal);
                code.word(0x00C3); code.position(objective.position); code.local(blipLocal);
            } else if (targetedObjective(objective.kind)) {
                const uint16_t blip = objective.kind == StorylandDyomObjectiveKind::DefeatActor ? 0x00C1 :
                    objective.kind == StorylandDyomObjectiveKind::CollectPickup ? 0x0263 : 0x00C0;
                code.word(blip); code.local(target); code.local(blipLocal);
            }
            code.mark(loop);
            code.wait();
            failIfDead(0x009F, 0);
            code.word(0x00A5); code.local(0); code.jump(0x0021, failure);
            for (size_t entityIndex = 0; entityIndex < project.entities.size(); ++entityIndex) {
                const auto& entity = project.entities[entityIndex];
                if (entity.mustSurvive) failIfDead(entity.kind == StorylandDyomEntityKind::Actor ? 0x009F : 0x00A0, unsigned(entityIndex + 1));
            }
            if (project.timeLimitSeconds > 0) {
                code.word(0x000F); code.local(timerA); code.integer(milliseconds(project.timeLimitSeconds)); code.jump(0x0021, failure);
            }
            if (objective.timeLimitSeconds > 0) {
                code.word(0x000F); code.local(timerB); code.integer(milliseconds(objective.timeLimitSeconds)); code.jump(0x0021, failure);
            }
            if (target && (objective.kind == StorylandDyomObjectiveKind::EnterVehicle || objective.kind == StorylandDyomObjectiveKind::ReachPointInVehicle))
                failIfDead(0x00A0, target);
            if (pointObjective(objective.kind)) {
                if (target) { code.word(0x007C); code.local(0); code.local(target); code.jump(0x0022, loop); }
                code.word(objective.kind == StorylandDyomObjectiveKind::ReachPointOnFoot ? 0x008A :
                    objective.kind == StorylandDyomObjectiveKind::ReachPointInVehicle ? 0x008B : 0x0089);
                code.local(0); code.position(objective.position);
                code.real(objective.radius); code.real(objective.radius); code.real(objective.radius); code.integer(0);
            } else if (objective.kind == StorylandDyomObjectiveKind::EnterVehicle) {
                code.word(0x007C); code.local(0); code.local(target);
            } else if (objective.kind == StorylandDyomObjectiveKind::DefeatActor) {
                code.word(0x009F); code.local(target);
            } else if (objective.kind == StorylandDyomObjectiveKind::DestroyVehicle) {
                code.word(0x00A0); code.local(target);
            } else if (objective.kind == StorylandDyomObjectiveKind::CollectPickup) {
                code.word(0x0140); code.local(target);
            } else {
                code.word(0x0015); code.local(timerB); code.integer(milliseconds(objective.durationSeconds));
            }
            code.jump(0x0021, done);
            code.jump(0x0002, loop);
            code.mark(done);
            releaseMarker(code, 0x00BB, blipLocal);
            releaseMarker(code, 0x0249, sphereLocal);
            range.end = code.bytes.size();
            result.objectives.push_back(std::move(range));
        }
        code.mark(success); result.successOffset = code.bytes.size();
        code.set(StorylandDyomCompiled::resultLocal, 1);
        code.word(0x026C);
        if (project.reward) { code.word(0x0094); code.local(0); code.integer(project.reward); }
        print(code, "M_PASS", true);
        code.jump(0x0002, cleanup);
        code.mark(failure); result.failureOffset = code.bytes.size();
        code.set(StorylandDyomCompiled::resultLocal, 2);
        code.word(0x026C);
        print(code, "M_FAIL", true);
        code.mark(cleanup); result.cleanupOffset = code.bytes.size();
        releaseMarker(code, 0x00BB, blipLocal);
        releaseMarker(code, 0x0249, sphereLocal);
        for (const auto& binding : result.entities) {
            if (binding.kind == StorylandDyomEntityKind::Actor) releaseMarker(code, 0x003F, binding.local);
            else if (binding.kind == StorylandDyomEntityKind::Vehicle) releaseMarker(code, 0x0049, binding.local);
            else if (binding.kind == StorylandDyomEntityKind::Pickup) releaseMarker(code, 0x0141, binding.local);
        }
        const size_t releaseWeather = code.label(), restoreWeather = code.label();
        code.word(0x0015); code.local(savedWeatherLocal); code.integer(0);
        code.jump(0x0022, releaseWeather);
        code.word(0x0108); code.local(savedWeatherLocal);
        code.jump(0x0002, restoreWeather);
        code.mark(releaseWeather); code.word(0x010A);
        code.mark(restoreWeather); code.word(0x0167); code.word(0x0169);
        for (const auto model : models) { code.word(0x0162); code.integer(model); }
        code.word(0x007A);
        code.word(0x0023);
        code.finish();
        result.bytecode = std::move(code.bytes);
        compiled = std::move(result);
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}

bool storylandStartDyomVcs(const StorylandDyomCompiled& compiled, StorylandMissionVm& mission,
    StorylandMissionEnvironment& environment, std::string& error) {
    error.clear();
    if (compiled.bytecode.empty() || compiled.bytecode.size() > maximumDocumentBytes ||
        compiled.successOffset >= compiled.failureOffset || compiled.failureOffset >= compiled.cleanupOffset ||
        compiled.cleanupOffset >= compiled.bytecode.size() || compiled.objectives.empty() ||
        compiled.objectives.size() > maximumObjectives || compiled.entities.size() > maximumEntities) {
        error = "DYOM requires a successfully compiled version 1 project.";
        return false;
    }
    if (!std::isfinite(environment.hour) || environment.forcedWeather < -1 || environment.forcedWeather > 7 ||
        environment.nextMissionCharacterHandle <= 0 || environment.nextMissionVehicleHandle <= 0 ||
        environment.nextObjectHandle <= 0 || environment.nextPickupHandle <= 0) {
        error = "DYOM requires an initialized native VCS play-test environment.";
        return false;
    }
    std::array<size_t, 4> counts{};
    for (const auto& binding : compiled.entities) {
        const size_t kind = size_t(binding.kind);
        if (kind >= counts.size() || binding.local < 1 || binding.local > maximumEntities) {
            error = "DYOM compiled entity bindings are invalid.";
            return false;
        }
        ++counts[kind];
    }
    if (counts[0] > maximumActors || counts[1] > maximumVehicles ||
        environment.missionCharacters.size() + counts[0] > 128 || environment.missionVehicles.size() + counts[1] > 256 ||
        environment.objects.size() + counts[2] > 4096 || environment.pickups.size() + counts[3] > 4096 ||
        int64_t(environment.nextMissionCharacterHandle) + int64_t(counts[0]) >= INT32_MAX ||
        int64_t(environment.nextMissionVehicleHandle) + int64_t(counts[1]) >= INT32_MAX ||
        int64_t(environment.nextObjectHandle) + int64_t(counts[2]) >= INT32_MAX ||
        int64_t(environment.nextPickupHandle) + int64_t(counts[3]) >= INT32_MAX) {
        error = "The native entity pools do not have enough free slots for this DYOM mission.";
        return false;
    }
    for (const auto weapon : compiled.requiredWeapons) {
        if (!environment.playerWeapons.count(weapon)) {
            error = "The original VCS weapon table does not contain weapon " + std::to_string(weapon) + ".";
            return false;
        }
    }
    mission.environment = &environment;
    mission.start(compiled.bytecode, 0, true, false);
    mission.scriptName = "DYOM";
    mission.locals[StorylandDyomCompiled::playerLocal] = storylandVcsCharacterToScript(0, environment.vcsCharacterHandleBridge);
    mission.locals[StorylandDyomCompiled::objectiveLocal] = -1;
    mission.locals[savedWeatherLocal] = environment.forcedWeather;
    mission.locals[blipLocal] = -1;
    mission.locals[sphereLocal] = -1;
    for (const auto& binding : compiled.entities) mission.locals[binding.local] = -1;
    mission.deathArrestEnabled = false;
    return true;
}

bool storylandAbortDyomVcs(const StorylandDyomCompiled& compiled, StorylandMissionVm& mission, std::string& error) {
    error.clear();
    if (!mission.environment || !mission.vcs || mission.scriptBytes() != compiled.bytecode ||
        compiled.failureOffset >= compiled.bytecode.size() || compiled.cleanupOffset >= compiled.bytecode.size()) {
        error = "This VM is not running the selected compiled DYOM mission.";
        return false;
    }
    if (mission.state == StorylandMissionState::Finished || mission.state == StorylandMissionState::Idle) return true;
    if (mission.instructions == 0) {
        mission.tick(0, false);
        if (mission.state == StorylandMissionState::Finished) return true;
    }
    mission.pc = compiled.failureOffset;
    mission.delayMilliseconds = 0;
    mission.vcsAbsoluteWait = false;
    mission.conditionCount = 0;
    mission.conditionValid = false;
    mission.state = StorylandMissionState::Running;
    mission.tick(0, false);
    if (mission.state != StorylandMissionState::Finished) { error = mission.diagnostic; return false; }
    return true;
}
