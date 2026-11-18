#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <unordered_set>
#include <vector>

namespace storyland {

enum class VcsTurfGang : std::uint8_t {
    Cholos = 0,
    Haitians = 1,
    Vance = 2
};

enum class VcsTurfDistrict : std::uint8_t {
    LittleHaiti,
    LittleHavana,
    Downtown,
    Viceport
};

enum class VcsTurfRestrictedArea : std::uint8_t {
    FortBaxter,
    EscobarAirport,
    StarfishIsland,
    Additional
};

struct VcsTurfPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct VcsTurfRect {
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    bool contains(const VcsTurfPoint& point) const;
    bool overlaps(const VcsTurfRect& other) const;
    bool valid() const;
};

struct VcsTurfDistrictZone {
    VcsTurfDistrict district = VcsTurfDistrict::LittleHaiti;
    VcsTurfRect bounds;
};

struct VcsTurfRestrictedZone {
    VcsTurfRestrictedArea area = VcsTurfRestrictedArea::FortBaxter;
    VcsTurfRect bounds;
};

struct VcsTurf {
    std::string id;
    std::string displayName;
    VcsTurfDistrict district = VcsTurfDistrict::LittleHaiti;
    VcsTurfRect bounds;
    VcsTurfGang owner = VcsTurfGang::Cholos;
    int dailyIncome = 0;
    double earnedGameMinutes = 0.0;
};

struct VcsTurfFrame {
    VcsTurfPoint playerPosition;
    bool playerAlive = true;
    bool playerOnFoot = true;
    bool scriptedMissionActive = false;
    bool gameIsVcs = false;
    bool extendedAndSaFeatures = false;
    double realTimeSeconds = 0.0;
    double elapsedGameMinutes = 0.0;
    bool paused = false;
    bool playerInExterior = true;
};

struct VcsTurfKill {
    std::uint64_t actorId = 0;
    VcsTurfGang gang = VcsTurfGang::Cholos;
    VcsTurfPoint position;
    bool killedByVictor = false;
};

struct VcsTurfBattle {
    std::string turfId;
    VcsTurfGang defender = VcsTurfGang::Cholos;
    int waveNumber = 0;
    int totalWaves = 3;
    int enemyCount = 0;
    bool defending = false;
    bool waitingForWave = false;
    bool awaitingPlayer = false;
    double secondsRemaining = 0.0;
};

class VcsTurfHost {
public:
    virtual ~VcsTurfHost() = default;

    virtual bool spawnGangWarEnemies(
        const VcsTurfBattle& battle,
        const VcsTurfRect& area,
        std::vector<std::uint64_t>& spawnedActorIds) = 0;

    virtual void clearGangWarEnemies(const std::vector<std::uint64_t>& actorIds) = 0;
    virtual void announceGangTurfEvent(const std::string& message) = 0;
    virtual void payAdditionalGangTurfIncome(int amount) = 0;
    virtual void gangTurfOwnershipChanged(const VcsTurf& turf, VcsTurfGang previousOwner) = 0;
};

class VcsGangTurfs {
public:
    bool initializeTerritories(
        const std::vector<VcsTurf>& territories,
        const std::vector<VcsTurfRestrictedZone>& restrictedZones,
        std::string* errorMessage = nullptr);

    void update(const VcsTurfFrame& frame, VcsTurfHost& host);
    void gangMemberKilled(const VcsTurfKill& kill, VcsTurfHost& host);
    void resetCombat(VcsTurfHost& host);
    bool requestDefensiveWar(const std::string& turfId, VcsTurfGang attacker, VcsTurfHost& host);

    bool readSave(std::istream& stream);
    bool writeSave(std::ostream& stream) const;

    const VcsTurf* turfAt(const VcsTurfPoint& position) const;
    const VcsTurf* turfById(const std::string& id) const;
    const std::vector<VcsTurf>& turfs() const;
    const std::vector<VcsTurfRestrictedZone>& noTurfZones() const;
    bool enabled() const;
    bool inCaptureWar() const;
    VcsTurfBattle battleStatus() const;
    int additionalDailyIncome() const;

private:
    bool startWave(VcsTurfHost& host);
    void finishCapture(VcsTurfHost& host);
    void finishDefense(bool victory, VcsTurfHost& host);
    void scheduleNextAttack();
    bool startScheduledAttack(VcsTurfHost& host);
    void clearWar(VcsTurfHost& host);
    void updateIncome(double gameMinutes, VcsTurfHost& host);
    bool isInsideRestrictedZone(const VcsTurfPoint& position) const;
    VcsTurf* mutableTurfById(const std::string& id);

    std::vector<VcsTurf> turfs_;
    std::vector<VcsTurfRestrictedZone> restrictedZones_;
    bool validConfiguration_ = false;
    bool active_ = false;
    bool haveGameClock_ = false;
    double lastGameMinutes_ = 0.0;
    VcsTurfFrame lastFrame_;
    std::string threatenedTurfId_;
    std::unordered_set<std::uint64_t> aggressionVictims_;
    double lastAggressionSecond_ = 0.0;
    double provocation_ = 0.0;
    std::string warTurfId_;
    VcsTurfGang warDefender_ = VcsTurfGang::Cholos;
    int wave_ = 0;
    double outsideTurfSince_ = -1.0;
    std::unordered_set<std::uint64_t> waveActors_;
    bool waitingForWave_ = false;
    bool defending_ = false;
    bool awaitingPlayer_ = false;
    bool outsideWarning_ = false;
    double nextWaveSecond_ = 0.0;
    double defenseDeadline_ = 0.0;
    double simulationSeconds_ = 0.0;
    double lastUpdateSecond_ = 0.0;
    bool haveUpdateClock_ = false;
    double attackSecondsRemaining_ = 0.0;
    std::uint32_t randomState_ = 0x7153A9C5u;
};

} // namespace storyland
