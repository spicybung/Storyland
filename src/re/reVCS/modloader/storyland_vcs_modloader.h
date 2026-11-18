#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

enum class StorylandModloaderTarget { Unknown, VcsPs2, VcsPsp, LcsPs2, LcsPsp };

struct StorylandModTurfDefinition {
    std::string id;
    std::string name;
    std::string districtZone;
    std::string owner;
    std::array<float, 4> bounds{};
    int dailyIncome = 0;
};

struct StorylandModGangTurfConfig {
    bool enabled = false;
    int populationKind = 10;
    int pedestrianType = 32;
    int health = 100;
    int armour = 0;
    std::array<std::string, 2> haitianModels{};
    std::array<int, 2> weaponSlots{3, 5};
    std::vector<StorylandModTurfDefinition> territories;
    std::vector<std::string> excludedZones;
};

struct StorylandModDtzPatch {
    uint32_t offset = 0;
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> expected;
    std::string source;
};

struct StorylandVcsMod {
    std::filesystem::path directory;
    std::string name;
    int priority = 0;
    bool enabled = true;
    std::vector<std::pair<std::string, std::filesystem::path>> replacements;
    std::vector<StorylandModDtzPatch> patches;
    bool hasGangTurfs = false;
    StorylandModGangTurfConfig gangTurfs;
};

class StorylandVcsModloader {
public:
    bool mount(const std::filesystem::path& gameRoot, StorylandModloaderTarget target,
        std::string& error);
    std::filesystem::path resolveGameFile(const std::string& retailName,
        const std::filesystem::path& fallback = {}) const;
    std::filesystem::path resolveRetailPath(const std::filesystem::path& retailPath) const;
    std::vector<std::pair<std::string, std::filesystem::path>> files() const;
    bool applyDtzPatches(std::vector<uint8_t>& unpackedBytes, std::string& error) const;
    const StorylandModGangTurfConfig& gangTurfConfig() const { return gangTurfs; }
    const std::vector<StorylandVcsMod>& mods() const { return mounted; }
    const std::filesystem::path& gameRoot() const { return root; }
    bool active() const;

private:
    std::filesystem::path root;
    std::vector<StorylandVcsMod> mounted;
    StorylandModGangTurfConfig gangTurfs;
};
