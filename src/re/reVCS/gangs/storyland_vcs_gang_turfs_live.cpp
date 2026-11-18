#include "src/re/reVCS/gangs/storyland_vcs_gang_turfs_live.h"

#include <algorithm>
#include <array>
#include <limits>
#include <sstream>
#include <utility>

namespace storyland {

VcsGangTurfsLive::VcsGangTurfsLive(VcsTurfEngineAccess& game)
    : game_(game)
{
}

bool VcsGangTurfsLive::initialize(std::string* errorMessage)
{
    if (initialized_) {
        if (errorMessage != nullptr)
            *errorMessage = "Gang turf runtime has already been initialized";
        return false;
    }

    const std::array<VcsTurfDistrict, 4> districts = {
        VcsTurfDistrict::LittleHaiti,
        VcsTurfDistrict::LittleHavana,
        VcsTurfDistrict::Downtown,
        VcsTurfDistrict::Viceport
    };
    const std::array<VcsTurfRestrictedArea, 3> restrictions = {
        VcsTurfRestrictedArea::FortBaxter,
        VcsTurfRestrictedArea::EscobarAirport,
        VcsTurfRestrictedArea::StarfishIsland
    };

    std::vector<VcsTurfDistrictZone> loadedDistricts;
    std::vector<VcsTurfRestrictedZone> loadedRestrictions;
    for (const VcsTurfDistrict district : districts) {
        VcsTurfRect bounds;
        if (!game_.readMainlandDistrict(district, bounds) || !bounds.valid()) {
            if (errorMessage != nullptr)
                *errorMessage = "A mandatory mainland district could not be resolved from the world zone loader";
            return false;
        }
        loadedDistricts.push_back({district, bounds});
    }
    for (const VcsTurfRestrictedArea restriction : restrictions) {
        VcsTurfRect bounds;
        if (!game_.readRestrictedArea(restriction, bounds) || !bounds.valid()) {
            if (errorMessage != nullptr)
                *errorMessage = "A mandatory excluded area could not be resolved from the world zone loader";
            return false;
        }
        loadedRestrictions.push_back({restriction, bounds});
    }

    std::vector<VcsTurf> territories;
    if(!game_.readGangTurfDefinitions(territories)) {
        if(errorMessage)*errorMessage="Gang turf definitions are unavailable from the data loader";
        return false;
    }
    for(const auto& turf:territories) {
        const auto district=std::find_if(loadedDistricts.begin(),loadedDistricts.end(),
            [&](const auto& zone){return zone.district==turf.district;});
        if(district==loadedDistricts.end()||turf.bounds.minX<district->bounds.minX||
            turf.bounds.minY<district->bounds.minY||turf.bounds.maxX>district->bounds.maxX||
            turf.bounds.maxY>district->bounds.maxY) {
            if(errorMessage)*errorMessage="A gang turf definition is outside its mainland district";
            return false;
        }
    }
    initialized_ = turfs_.initializeTerritories(territories, loadedRestrictions, errorMessage);
    return initialized_;
}

void VcsGangTurfsLive::tick()
{
    if (!initialized_)
        return;

    VcsTurfFrame frame;
    if (!game_.readLiveFrame(frame)) {
        // Unknown runtime state must not cause a gang war or award income.
        frame.gameIsVcs = false;
        frame.extendedAndSaFeatures = false;
    }
    turfs_.update(frame, *this);
    if (turfs_.enabled())
        flushPendingMoney();
}

void VcsGangTurfsLive::nativePedDied(const VcsTurfPedDeath& death)
{
    if (!initialized_ || !turfs_.enabled() || death.nativeActorId == 0)
        return;

    VcsTurfKill kill;
    kill.actorId = death.nativeActorId;
    kill.gang = death.nativeGang;
    kill.position = death.deathPosition;
    kill.killedByVictor = death.victorCausedDeath;
    turfs_.gangMemberKilled(kill, *this);
}

VcsTurfHudColor VcsGangTurfsLive::ownerColor(VcsTurfGang gang)
{
    switch (gang) {
    case VcsTurfGang::Cholos: return {235, 128, 30, 104};
    case VcsTurfGang::Haitians: return {150, 66, 192, 104};
    case VcsTurfGang::Vance: return {56, 178, 76, 104};
    }
    return {0, 0, 0, 0};
}

void VcsGangTurfsLive::drawMapAndHud()
{
    if (!initialized_ || !turfs_.enabled())
        return;

    VcsTurfFrame frame;
    if (!game_.readLiveFrame(frame) || !frame.gameIsVcs || !frame.extendedAndSaFeatures)
        return;

    const VcsTurfBattle activeBattle = turfs_.battleStatus();
    for (const VcsTurf& turf : turfs_.turfs()) {
        game_.drawGangTurfOnMap(
            turf.bounds,
            ownerColor(turf.owner),
            !activeBattle.turfId.empty() && turf.id == activeBattle.turfId);
    }

    const VcsTurf* current = turfs_.turfAt(frame.playerPosition);
    const VcsTurfBattle* battle = turfs_.inCaptureWar() ? &activeBattle : nullptr;
    game_.drawGangTurfHud(current, battle, turfs_.additionalDailyIncome());
}

void VcsGangTurfsLive::resetCombat()
{
    if (initialized_)
        turfs_.resetCombat(*this);
}

bool VcsGangTurfsLive::save(std::ostream& stream) const
{
    if (!initialized_)
        return false;

    stream << "STORYLAND_VCS_GANG_TURFS_LIVE 1\n";
    stream << pendingExtraMoney_ << '\n';
    return turfs_.writeSave(stream);
}

bool VcsGangTurfsLive::load(std::istream& stream)
{
    if (!initialized_)
        return false;

    std::string magic;
    int version = 0;
    int pending = 0;
    if (!(stream >> magic >> version >> pending) ||
        magic != "STORYLAND_VCS_GANG_TURFS_LIVE" || version != 1 || pending < 0)
        return false;

    // The core loader validates its entire record before changing turf ownership.
    if (!turfs_.readSave(stream))
        return false;

    turfs_.resetCombat(*this);
    pendingExtraMoney_ = pending;
    game_.refreshNativeGangTurfMap();
    return true;
}

bool VcsGangTurfsLive::spawnGangWarEnemies(
    const VcsTurfBattle& battle,
    const VcsTurfRect& area,
    std::vector<std::uint64_t>& spawnedActorIds)
{
    if (!initialized_ || !turfs_.enabled() ||
        battle.defender == VcsTurfGang::Vance ||
        battle.enemyCount <= 0 || battle.enemyCount > 32 || !area.valid())
        return false;

    spawnedActorIds.clear();
    for (int index = 0; index < battle.enemyCount; ++index) {
        VcsTurfPoint point;
        if (!game_.findLoadedPedestrianSpawn(battle.defender, area, index, point) ||
            !area.contains(point)) {
            clearGangWarEnemies(spawnedActorIds);
            spawnedActorIds.clear();
            return false;
        }

        bool prohibited = false;
        for (const VcsTurfRestrictedZone& zone : turfs_.noTurfZones()) {
            if (zone.bounds.contains(point)) {
                prohibited = true;
                break;
            }
        }
        if (prohibited) {
            clearGangWarEnemies(spawnedActorIds);
            spawnedActorIds.clear();
            return false;
        }

        std::uint64_t nativeActorId = 0;
        if (!game_.createNativeGangPed(battle.defender, point, nativeActorId) ||
            nativeActorId == 0) {
            clearGangWarEnemies(spawnedActorIds);
            spawnedActorIds.clear();
            return false;
        }
        spawnedActorIds.push_back(nativeActorId);
        if (!game_.taskNativeGangPedToAttackVictor(nativeActorId)) {
            clearGangWarEnemies(spawnedActorIds);
            spawnedActorIds.clear();
            return false;
        }
        for (std::size_t previous = 0; previous + 1 < spawnedActorIds.size(); ++previous) {
            if (spawnedActorIds[previous] == nativeActorId) {
                clearGangWarEnemies(spawnedActorIds);
                spawnedActorIds.clear();
                return false;
            }
        }
    }
    return true;
}

void VcsGangTurfsLive::clearGangWarEnemies(const std::vector<std::uint64_t>& actorIds)
{
    for (const std::uint64_t actorId : actorIds) {
        if (actorId != 0)
            game_.removeNativeGangWarPed(actorId);
    }
}

void VcsGangTurfsLive::announceGangTurfEvent(const std::string& message)
{
    game_.showGameHudMessage(message);
}

void VcsGangTurfsLive::payAdditionalGangTurfIncome(int amount)
{
    if (amount <= 0)
        return;

    const int maxInt = std::numeric_limits<int>::max();
    if (pendingExtraMoney_ > maxInt - amount)
        pendingExtraMoney_ = maxInt;
    else
        pendingExtraMoney_ += amount;
    flushPendingMoney();
}

void VcsGangTurfsLive::flushPendingMoney()
{
    if (pendingExtraMoney_ > 0 && game_.addVictorWalletMoney(pendingExtraMoney_))
        pendingExtraMoney_ = 0;
}

void VcsGangTurfsLive::gangTurfOwnershipChanged(const VcsTurf&, VcsTurfGang)
{
    game_.refreshNativeGangTurfMap();
}

const VcsGangTurfs& VcsGangTurfsLive::gameplay() const
{
    return turfs_;
}

int VcsGangTurfsLive::pendingExtraMoney() const
{
    return pendingExtraMoney_;
}

bool VcsGangTurfsLive::initialized() const
{
    return initialized_;
}

} // namespace storyland
