#pragma once

#include "src/re/reVCS/gangs/storyland_vcs_gang_turfs.h"

#include <cstdint>
#include <iosfwd>
#include <string>

namespace storyland {

struct VcsTurfHudColor {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    std::uint8_t alpha = 0;
};

struct VcsTurfPedDeath {
    std::uint64_t nativeActorId = 0;
    VcsTurfGang nativeGang = VcsTurfGang::Cholos;
    VcsTurfPoint deathPosition;
    bool victorCausedDeath = false;
};

// Every operation refers to an actual in-game object, region, or wallet.
// Return false when a resource or operation is unavailable; do not create placeholder actors.
class VcsTurfEngineAccess {
public:
    virtual ~VcsTurfEngineAccess() = default;

    virtual bool readMainlandDistrict(VcsTurfDistrict district, VcsTurfRect& bounds) const = 0;
    virtual bool readRestrictedArea(VcsTurfRestrictedArea area, VcsTurfRect& bounds) const = 0;
    virtual bool readGangTurfDefinitions(std::vector<VcsTurf>& territories) const = 0;
    virtual bool readLiveFrame(VcsTurfFrame& frame) const = 0;

    virtual bool findLoadedPedestrianSpawn(
        VcsTurfGang gang,
        const VcsTurfRect& turf,
        int index,
        VcsTurfPoint& point) = 0;

    virtual bool createNativeGangPed(
        VcsTurfGang gang,
        const VcsTurfPoint& point,
        std::uint64_t& nativeActorId) = 0;

    virtual bool taskNativeGangPedToAttackVictor(std::uint64_t nativeActorId) = 0;
    virtual void removeNativeGangWarPed(std::uint64_t nativeActorId) = 0;

    // Must add the amount to Victor's existing wallet without altering Empire Building revenue.
    virtual bool addVictorWalletMoney(int amount) = 0;
    virtual void showGameHudMessage(const std::string& text) = 0;
    virtual void refreshNativeGangTurfMap() = 0;

    // Translate world-space turf bounds into the game's real radar/map projection.
    virtual void drawGangTurfOnMap(
        const VcsTurfRect& area,
        const VcsTurfHudColor& color,
        bool highlighted) = 0;

    virtual void drawGangTurfHud(
        const VcsTurf* turfUnderVictor,
        const VcsTurfBattle* battle,
        int extraDailyIncome) = 0;
};

class VcsGangTurfsLive final : public VcsTurfHost {
public:
    explicit VcsGangTurfsLive(VcsTurfEngineAccess& game);

    bool initialize(std::string* errorMessage = nullptr);
    void tick();
    void nativePedDied(const VcsTurfPedDeath& death);
    void drawMapAndHud();
    void resetCombat();

    bool save(std::ostream& stream) const;
    bool load(std::istream& stream);

    const VcsGangTurfs& gameplay() const;
    int pendingExtraMoney() const;
    bool initialized() const;

    bool spawnGangWarEnemies(
        const VcsTurfBattle& battle,
        const VcsTurfRect& area,
        std::vector<std::uint64_t>& spawnedActorIds) override;
    void clearGangWarEnemies(const std::vector<std::uint64_t>& actorIds) override;
    void announceGangTurfEvent(const std::string& message) override;
    void payAdditionalGangTurfIncome(int amount) override;
    void gangTurfOwnershipChanged(const VcsTurf& turf, VcsTurfGang previousOwner) override;

private:
    void flushPendingMoney();
    static VcsTurfHudColor ownerColor(VcsTurfGang gang);

    VcsTurfEngineAccess& game_;
    VcsGangTurfs turfs_;
    bool initialized_ = false;
    int pendingExtraMoney_ = 0;
};

} // namespace storyland
