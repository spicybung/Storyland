#include "src/re/reVCS/gangs/storyland_vcs_gang_turfs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace storyland {

namespace {

constexpr double GameMinutesPerDay = 1440.0;
constexpr double ProvocationDecayPerSecond = 0.025;
constexpr double ProvocationThreshold = 2.1;
constexpr double WavePreparationSeconds = 10.0;
constexpr double ExitWarningSeconds = 10.0;
constexpr float WarAreaMargin = 50.0f;
constexpr double ExitGraceSeconds = 30.0;
constexpr int GangWarWaveCount = 3;
constexpr std::array<int, GangWarWaveCount> WaveEnemyCounts = {3, 5, 7};

bool finiteNonnegative(double value)
{
    return std::isfinite(value) && value >= 0.0;
}

bool isValidOwner(int owner)
{
    return owner >= static_cast<int>(VcsTurfGang::Cholos) &&
           owner <= static_cast<int>(VcsTurfGang::Vance);
}

std::vector<std::uint64_t> copyActors(const std::unordered_set<std::uint64_t>& actors)
{
    return std::vector<std::uint64_t>(actors.begin(), actors.end());
}

} // namespace

bool VcsTurfRect::contains(const VcsTurfPoint& point) const
{
    return point.x >= minX && point.x < maxX && point.y >= minY && point.y < maxY;
}

bool VcsTurfRect::overlaps(const VcsTurfRect& other) const
{
    return minX < other.maxX && maxX > other.minX &&
           minY < other.maxY && maxY > other.minY;
}

bool VcsTurfRect::valid() const
{
    return std::isfinite(minX) && std::isfinite(minY) &&
           std::isfinite(maxX) && std::isfinite(maxY) &&
           maxX > minX && maxY > minY;
}

bool VcsGangTurfs::initializeTerritories(const std::vector<VcsTurf>& territories,
    const std::vector<VcsTurfRestrictedZone>& restrictedZones,std::string* errorMessage)
{
    const auto fail=[errorMessage](const std::string& reason) {
        if(errorMessage)*errorMessage=reason;
        return false;
    };
    if(validConfiguration_||!turfs_.empty())return fail("The turf system has already been initialized");
    if(territories.empty()||territories.size()>128)
        return fail("Gang turf data must define between one and 128 territories");
    std::array<bool,4> seenRestricted{};
    for(const auto& restriction:restrictedZones) {
        const size_t kind=static_cast<size_t>(restriction.area);
        if(kind>=seenRestricted.size()||!restriction.bounds.valid())
            return fail("A forbidden territory zone is invalid");
        seenRestricted[kind]=true;
    }
    if(!std::all_of(seenRestricted.begin(),seenRestricted.begin()+3,[](bool present){return present;}))
        return fail("Fort Baxter, Escobar Airport, and Starfish Island exclusion zones are all required");
    std::unordered_set<std::string> ids;
    for(size_t index=0;index<territories.size();++index) {
        const auto& turf=territories[index];
        if(turf.id.empty()||turf.id.size()>64||turf.displayName.empty()||turf.displayName.size()>128||
            !ids.insert(turf.id).second||!turf.bounds.valid()||
            static_cast<size_t>(turf.district)>static_cast<size_t>(VcsTurfDistrict::Viceport)||
            !isValidOwner(static_cast<int>(turf.owner))||turf.dailyIncome<0||turf.dailyIncome>1000000||
            !finiteNonnegative(turf.earnedGameMinutes)||turf.earnedGameMinutes>=GameMinutesPerDay)
            return fail("A gang turf definition is invalid or duplicated");
        for(const auto& restriction:restrictedZones)
            if(turf.bounds.overlaps(restriction.bounds))
                return fail("A gang turf definition overlaps a protected area");
        for(size_t previous=0;previous<index;++previous)
            if(turf.bounds.overlaps(territories[previous].bounds))
                return fail("Gang turf definitions overlap one another");
    }
    restrictedZones_=restrictedZones;
    turfs_=territories;
    validConfiguration_ = true;
    scheduleNextAttack();
    if (errorMessage != nullptr)
        errorMessage->clear();
    return true;
}

bool VcsGangTurfs::isInsideRestrictedZone(const VcsTurfPoint& position) const
{
    for (const VcsTurfRestrictedZone& zone : restrictedZones_) {
        if (zone.bounds.contains(position))
            return true;
    }
    return false;
}

const VcsTurf* VcsGangTurfs::turfAt(const VcsTurfPoint& position) const
{
    if (!active_ || isInsideRestrictedZone(position))
        return nullptr;
    for (const VcsTurf& turf : turfs_) {
        if (turf.bounds.contains(position))
            return &turf;
    }
    return nullptr;
}

VcsTurf* VcsGangTurfs::mutableTurfById(const std::string& id)
{
    for (VcsTurf& turf : turfs_) {
        if (turf.id == id)
            return &turf;
    }
    return nullptr;
}

const VcsTurf* VcsGangTurfs::turfById(const std::string& id) const
{
    for (const VcsTurf& turf : turfs_) {
        if (turf.id == id)
            return &turf;
    }
    return nullptr;
}

const std::vector<VcsTurf>& VcsGangTurfs::turfs() const
{
    return turfs_;
}

const std::vector<VcsTurfRestrictedZone>& VcsGangTurfs::noTurfZones() const
{
    return restrictedZones_;
}

bool VcsGangTurfs::enabled() const
{
    return active_;
}

bool VcsGangTurfs::inCaptureWar() const
{
    return !warTurfId_.empty();
}

VcsTurfBattle VcsGangTurfs::battleStatus() const
{
    VcsTurfBattle battle;
    battle.turfId = warTurfId_;
    battle.defender = warDefender_;
    battle.waveNumber = wave_;
    battle.totalWaves = defending_ ? 1 : GangWarWaveCount;
    battle.enemyCount = static_cast<int>(waveActors_.size());
    battle.defending = defending_;
    battle.waitingForWave = waitingForWave_;
    battle.awaitingPlayer = awaitingPlayer_;
    battle.secondsRemaining = std::max(0.0, awaitingPlayer_ || defending_
        ? defenseDeadline_ - simulationSeconds_
        : waitingForWave_ ? nextWaveSecond_ - simulationSeconds_ : 0.0);
    return battle;
}

int VcsGangTurfs::additionalDailyIncome() const
{
    if (!active_)
        return 0;
    int total = 0;
    for (const VcsTurf& turf : turfs_) {
        if (turf.owner == VcsTurfGang::Vance)
            total += turf.dailyIncome;
    }
    return total;
}

void VcsGangTurfs::clearWar(VcsTurfHost& host)
{
    if (!waveActors_.empty()) {
        host.clearGangWarEnemies(copyActors(waveActors_));
        waveActors_.clear();
    }
    warTurfId_.clear();
    wave_ = 0;
    outsideTurfSince_ = -1.0;
    outsideWarning_ = false;
    waitingForWave_ = false;
    defending_ = false;
    awaitingPlayer_ = false;
    nextWaveSecond_ = 0.0;
    defenseDeadline_ = 0.0;
    aggressionVictims_.clear();
    provocation_=0.0;
    threatenedTurfId_.clear();
}

void VcsGangTurfs::resetCombat(VcsTurfHost& host)
{
    clearWar(host);
}

bool VcsGangTurfs::startWave(VcsTurfHost& host)
{
    const VcsTurf* turf = turfById(warTurfId_);
    if (turf == nullptr || wave_ < 1 || wave_ > GangWarWaveCount)
        return false;

    VcsTurfBattle requested;
    requested.turfId = turf->id;
    requested.defender = warDefender_;
    requested.waveNumber = wave_;
    requested.totalWaves = defending_ ? 1 : GangWarWaveCount;
    requested.defending = defending_;
    requested.enemyCount = defending_ ? WaveEnemyCounts.back()
        : WaveEnemyCounts[static_cast<std::size_t>(wave_ - 1)];

    std::vector<std::uint64_t> spawnedActors;
    if (!host.spawnGangWarEnemies(requested, turf->bounds, spawnedActors)) {
        host.clearGangWarEnemies(spawnedActors);
        return false;
    }

    std::unordered_set<std::uint64_t> uniqueActors(spawnedActors.begin(), spawnedActors.end());
    if (uniqueActors.size() != static_cast<std::size_t>(requested.enemyCount) ||
        uniqueActors.find(0) != uniqueActors.end()) {
        host.clearGangWarEnemies(spawnedActors);
        return false;
    }

    waveActors_ = std::move(uniqueActors);
    waitingForWave_ = false;
    host.announceGangTurfEvent(
        defending_ ? "Defend " + turf->displayName + " from the attacking gang"
            : "Gang war: " + turf->displayName + " - wave " + std::to_string(wave_) + "/3");
    return true;
}

void VcsGangTurfs::finishCapture(VcsTurfHost& host)
{
    VcsTurf* turf = mutableTurfById(warTurfId_);
    if (turf == nullptr) {
        clearWar(host);
        return;
    }
    const VcsTurfGang previousOwner = turf->owner;
    turf->owner = VcsTurfGang::Vance;
    turf->earnedGameMinutes = 0.0;
    const std::string message = "Turf captured: " + turf->displayName +
        " (+$" + std::to_string(turf->dailyIncome) + "/day on top of empires)";
    clearWar(host);
    scheduleNextAttack();
    host.gangTurfOwnershipChanged(*turf, previousOwner);
    host.announceGangTurfEvent(message);
}

void VcsGangTurfs::updateIncome(double gameMinutes, VcsTurfHost& host)
{
    if (!finiteNonnegative(gameMinutes))
        return;

    if (!haveGameClock_) {
        haveGameClock_ = true;
        lastGameMinutes_ = gameMinutes;
        return;
    }

    const double delta = gameMinutes - lastGameMinutes_;
    lastGameMinutes_ = gameMinutes;
    if (delta <= 0.0 || !std::isfinite(delta))
        return;

    // Handle fast travel/time advances, but prevent a corrupt clock from paying unbounded cash.
    const double creditedDelta = std::min(delta, 3.0 * GameMinutesPerDay);
    int totalMoney = 0;
    for (VcsTurf& turf : turfs_) {
        if (turf.owner != VcsTurfGang::Vance)
            continue;
        turf.earnedGameMinutes += creditedDelta;
        while (turf.earnedGameMinutes >= GameMinutesPerDay) {
            turf.earnedGameMinutes -= GameMinutesPerDay;
            totalMoney += turf.dailyIncome;
        }
    }
    if (totalMoney > 0) {
        host.payAdditionalGangTurfIncome(totalMoney);
        host.announceGangTurfEvent("Gang turf income: +$" + std::to_string(totalMoney));
    }
}

void VcsGangTurfs::scheduleNextAttack()
{
    randomState_ ^= randomState_ << 13;
    randomState_ ^= randomState_ >> 17;
    randomState_ ^= randomState_ << 5;
    attackSecondsRemaining_ = 648.0 + double(randomState_) / 4294967296.0 * 972.0;
}

bool VcsGangTurfs::requestDefensiveWar(const std::string& turfId, VcsTurfGang attacker,
    VcsTurfHost& host)
{
    const auto* turf = turfById(turfId);
    if (!active_ || inCaptureWar() || !turf || turf->owner != VcsTurfGang::Vance ||
        (attacker != VcsTurfGang::Cholos && attacker != VcsTurfGang::Haitians) ||
        !lastFrame_.playerAlive ||
        lastFrame_.scriptedMissionActive || lastFrame_.paused || !lastFrame_.playerInExterior)
        return false;
    warTurfId_ = turfId;
    warDefender_ = attacker;
    wave_ = 1;
    defending_ = awaitingPlayer_ = true;
    const double centerX = (turf->bounds.minX + turf->bounds.maxX) * .5;
    const double centerY = (turf->bounds.minY + turf->bounds.maxY) * .5;
    defenseDeadline_ = simulationSeconds_ + 240.0 +
        std::hypot(lastFrame_.playerPosition.x-centerX,lastFrame_.playerPosition.y-centerY)*.2;
    aggressionVictims_.clear();
    threatenedTurfId_.clear();
    host.announceGangTurfEvent("Your territory is under attack: " + turf->displayName);
    return true;
}

bool VcsGangTurfs::startScheduledAttack(VcsTurfHost& host)
{
    if (turfs_.empty()) return false;
    const size_t start = size_t(randomState_) % turfs_.size();
    for (size_t i=0;i<turfs_.size();++i) {
        const auto& turf=turfs_[(start+i)%turfs_.size()];
        if (turf.owner!=VcsTurfGang::Vance) continue;
        for (const auto& rival:turfs_) {
            if (rival.owner==VcsTurfGang::Vance) continue;
            const float dx=std::max({0.0f,turf.bounds.minX-rival.bounds.maxX,
                rival.bounds.minX-turf.bounds.maxX});
            const float dy=std::max({0.0f,turf.bounds.minY-rival.bounds.maxY,
                rival.bounds.minY-turf.bounds.maxY});
            if (std::hypot(dx,dy)<=10.0f &&
                requestDefensiveWar(turf.id,rival.owner,host)) return true;
        }
    }
    return false;
}

void VcsGangTurfs::finishDefense(bool victory,VcsTurfHost& host)
{
    auto* turf=mutableTurfById(warTurfId_);
    if (!turf) {clearWar(host);return;}
    const auto previous=turf->owner;
    const auto name=turf->displayName;
    if (!victory) {turf->owner=warDefender_;turf->earnedGameMinutes=0.0;}
    clearWar(host);
    scheduleNextAttack();
    if (!victory) host.gangTurfOwnershipChanged(*turf,previous);
    host.announceGangTurfEvent((victory?"Territory defended: ":"Territory lost: ")+name);
}

void VcsGangTurfs::update(const VcsTurfFrame& frame, VcsTurfHost& host)
{
    double elapsed=0.0;
    if (std::isfinite(frame.realTimeSeconds)) {
        if (haveUpdateClock_ && frame.realTimeSeconds>=lastUpdateSecond_)
            elapsed=frame.realTimeSeconds-lastUpdateSecond_;
        lastUpdateSecond_=frame.realTimeSeconds;
        haveUpdateClock_=true;
    }
    lastFrame_ = frame;
    const bool shouldBeActive = validConfiguration_ && frame.gameIsVcs && frame.extendedAndSaFeatures;
    if (!shouldBeActive) {
        if (active_) {clearWar(host);scheduleNextAttack();}
        active_ = false;
        aggressionVictims_.clear();
        threatenedTurfId_.clear();provocation_=0.0;
        if (finiteNonnegative(frame.elapsedGameMinutes)) {
            lastGameMinutes_ = frame.elapsedGameMinutes;
            haveGameClock_ = true;
        }
        return;
    }
    active_ = true;
    if (frame.paused) {
        if (finiteNonnegative(frame.elapsedGameMinutes)) {
            lastGameMinutes_=frame.elapsedGameMinutes;haveGameClock_=true;
        }
        return;
    }
    simulationSeconds_+=elapsed;
    updateIncome(frame.elapsedGameMinutes, host);
    if (!frame.playerAlive || frame.scriptedMissionActive || !frame.playerInExterior) {
        if (inCaptureWar()) {
            clearWar(host);
            host.announceGangTurfEvent("Gang war cancelled");
        }
        aggressionVictims_.clear();threatenedTurfId_.clear();provocation_=0.0;
        return;
    }
    if (!inCaptureWar()) {
        const VcsTurf* turf = turfAt(frame.playerPosition);
        provocation_=std::max(0.0,provocation_-elapsed*ProvocationDecayPerSecond);
        if (turf == nullptr || turf->id != threatenedTurfId_ || provocation_<=0.0) {
            aggressionVictims_.clear();threatenedTurfId_.clear();provocation_=0.0;
        }
        attackSecondsRemaining_-=elapsed;
        if (attackSecondsRemaining_<=0.0) {
            if (!startScheduledAttack(host)) scheduleNextAttack();
        }
        return;
    }
    const VcsTurf* warTurf=turfById(warTurfId_);
    if (!warTurf) {clearWar(host);return;}
    if (defending_) {
        if (simulationSeconds_>=defenseDeadline_) {finishDefense(false,host);return;}
        if (awaitingPlayer_) {
            const double centerX=(warTurf->bounds.minX+warTurf->bounds.maxX)*.5;
            const double centerY=(warTurf->bounds.minY+warTurf->bounds.maxY)*.5;
            if (warTurf->bounds.contains(frame.playerPosition) ||
                std::hypot(frame.playerPosition.x-centerX,frame.playerPosition.y-centerY)<70.0) {
                if (startWave(host)) {
                    awaitingPlayer_=false;
                    defenseDeadline_+=30.0;
                }
            }
        }
        return;
    }
    const auto& bounds=warTurf->bounds;
    const bool inside=frame.playerPosition.x>=bounds.minX-WarAreaMargin &&
        frame.playerPosition.x<=bounds.maxX+WarAreaMargin &&
        frame.playerPosition.y>=bounds.minY-WarAreaMargin &&
        frame.playerPosition.y<=bounds.maxY+WarAreaMargin &&
        !isInsideRestrictedZone(frame.playerPosition);
    if (!inside) {
        if (outsideTurfSince_<0.0) outsideTurfSince_=simulationSeconds_;
        const double outside=simulationSeconds_-outsideTurfSince_;
        if (!outsideWarning_ && outside>=ExitWarningSeconds) {
            outsideWarning_=true;host.announceGangTurfEvent("Return to the territory or lose the gang war");
        }
        if (outside>=ExitGraceSeconds) {
            clearWar(host);host.announceGangTurfEvent("Gang war lost: you left the territory");return;
        }
    } else {outsideTurfSince_=-1.0;outsideWarning_=false;}
    if (waitingForWave_ && simulationSeconds_>=nextWaveSecond_)startWave(host);
}

void VcsGangTurfs::gangMemberKilled(const VcsTurfKill& kill, VcsTurfHost& host)
{
    if (!active_ || kill.actorId == 0 || lastFrame_.paused || !lastFrame_.playerInExterior)
        return;

    // A registered wave enemy counts as defeated even if a car, police officer,
    // fire, or another gang member landed the final hit. Otherwise wars can stall.
    if (inCaptureWar()) {
        if (kill.gang != warDefender_ || waveActors_.erase(kill.actorId) == 0)
            return;
        if (!waveActors_.empty())
            return;
        if (defending_) {finishDefense(true,host);return;}
        if (wave_ == GangWarWaveCount) {
            finishCapture(host);
            return;
        }
        ++wave_;
        waitingForWave_=true;
        nextWaveSecond_=simulationSeconds_+WavePreparationSeconds;
        host.announceGangTurfEvent("Wave cleared - prepare for the next attack");
        return;
    }

    if (!kill.killedByVictor || !lastFrame_.playerAlive ||
        !lastFrame_.playerOnFoot || lastFrame_.scriptedMissionActive)
        return;

    const VcsTurf* turf = turfAt(kill.position);
    const VcsTurf* playerTurf = turfAt(lastFrame_.playerPosition);
    if (turf == nullptr || playerTurf == nullptr || turf->id != playerTurf->id ||
        turf->owner == VcsTurfGang::Vance || turf->owner != kill.gang)
        return;

    if (threatenedTurfId_ != turf->id) {
        threatenedTurfId_ = turf->id;
        aggressionVictims_.clear();provocation_=0.0;
    }
    lastAggressionSecond_ = simulationSeconds_;
    if (!aggressionVictims_.insert(kill.actorId).second)
        return;

    provocation_+=1.0;
    if (provocation_ <= ProvocationThreshold) {
        host.announceGangTurfEvent(
            "Turf takeover: " + std::to_string(aggressionVictims_.size()) + "/3 rival gang members defeated");
        return;
    }

    warTurfId_ = turf->id;
    warDefender_ = turf->owner;
    wave_ = 1;
    aggressionVictims_.clear();
    threatenedTurfId_.clear();
    waitingForWave_=true;
    nextWaveSecond_=simulationSeconds_+WavePreparationSeconds;
    host.announceGangTurfEvent("Gang war provoked - prepare for the first wave");
}

bool VcsGangTurfs::writeSave(std::ostream& stream) const
{
    if (!validConfiguration_)
        return false;
    stream << "STORYLAND_VCS_GANG_TURFS 2\n";
    stream << std::setprecision(17) << lastGameMinutes_ << ' ' << haveGameClock_ << ' '
           << (inCaptureWar()?std::max(648.0,attackSecondsRemaining_):
               std::clamp(attackSecondsRemaining_,0.0,1620.0)) << ' ' << randomState_ << '\n';
    stream << turfs_.size() << '\n';
    for (const VcsTurf& turf : turfs_) {
        stream << std::quoted(turf.id) << ' '
               << static_cast<int>(turf.owner) << ' '
               << turf.earnedGameMinutes << '\n';
    }
    return stream.good();
}

bool VcsGangTurfs::readSave(std::istream& stream)
{
    if (!validConfiguration_)
        return false;

    std::string magic;
    int version = 0;
    if (!(stream >> magic >> version) || magic != "STORYLAND_VCS_GANG_TURFS" || (version != 1 && version != 2))
        return false;

    double savedLastGameMinutes = 0.0;
    int savedHaveClock = 0;
    if (!(stream >> savedLastGameMinutes >> savedHaveClock) ||
        !finiteNonnegative(savedLastGameMinutes) ||
        (savedHaveClock != 0 && savedHaveClock != 1))
        return false;

    double savedAttackRemaining=attackSecondsRemaining_;
    std::uint64_t savedRandom=randomState_;
    if (version>=2 && (!(stream >> savedAttackRemaining >> savedRandom) ||
        !finiteNonnegative(savedAttackRemaining) || savedAttackRemaining>1620.0 ||
        savedRandom==0 || savedRandom>UINT32_MAX)) return false;
    std::size_t count = 0;
    if (!(stream >> count) || count != turfs_.size())
        return false;

    std::unordered_map<std::string, std::pair<VcsTurfGang, double>> saved;
    for (std::size_t index = 0; index < count; ++index) {
        std::string id;
        int owner = 0;
        double minutes = 0.0;
        if (!(stream >> std::quoted(id) >> owner >> minutes) ||
            !isValidOwner(owner) || !finiteNonnegative(minutes) || minutes >= GameMinutesPerDay ||
            turfById(id) == nullptr ||
            !saved.emplace(id, std::make_pair(static_cast<VcsTurfGang>(owner), minutes)).second)
            return false;
    }

    if (saved.size() != turfs_.size())
        return false;

    for (VcsTurf& turf : turfs_) {
        const auto it = saved.find(turf.id);
        turf.owner = it->second.first;
        turf.earnedGameMinutes = it->second.second;
    }
    lastGameMinutes_ = savedLastGameMinutes;
    haveGameClock_ = savedHaveClock != 0;
    attackSecondsRemaining_=savedAttackRemaining;
    randomState_=uint32_t(savedRandom);
    haveUpdateClock_=false;
    return true;
}

} // namespace storyland
