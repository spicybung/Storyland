#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

struct StorylandLcsExtendedWorld {
    struct RampageWeapon {
        bool active = false;
        int32_t physicalWeapon = -1, interruptedWeapon = 0, selectedWeapon = -1;
        int32_t ammunition = 0;
        bool valid() const {
            return physicalWeapon >= -1 && physicalWeapon <= 47 &&
                interruptedWeapon >= 0 && interruptedWeapon < 37 &&
                selectedWeapon >= -1 && selectedWeapon < 37 &&
                ammunition >= 0 && ammunition <= 99999 &&
                (active || (physicalWeapon == -1 && interruptedWeapon == 0 &&
                    selectedWeapon == -1 && ammunition == 0));
        }
        template<class Archive> void saveState(Archive& archive) {
            archive(active, physicalWeapon, interruptedWeapon, selectedWeapon, ammunition);
        }
    } rampageWeapon;
    struct Roadblock {
        std::array<float, 3> start{}, end{};
        bool used = false;
        std::vector<int32_t> vehicles, characters;
        template<class Archive> void saveState(Archive& archive) {
            archive(start, end, used, vehicles, characters);
        }
    };
    struct Rope {
        std::array<std::array<float, 3>, 32> points{}, velocities{};
        int32_t character = -1;
        uint32_t createdAt = 0;
        float progress = 0;
        bool active = false, landed = false;
        template<class Archive> void saveState(Archive& archive) {
            archive(points, velocities, character, createdAt, progress, active, landed);
        }
    };
    struct CutsceneAttachment {
        int32_t object = -1, parent = -1, bone = -1;
        std::string component;
        bool vehicle = false;
        template<class Archive> void saveState(Archive& archive) {
            archive(object, parent, bone, component, vehicle);
        }
    };
    struct TaxiShortcut {
        std::array<float, 3> origin{}, destination{};
        float originHeading = 0, destinationHeading = 0;
        int32_t vehicle = -1, driver = -1;
        uint32_t startedAt = 0;
        uint8_t state = 0;
        bool previousControl = true;
        int32_t blip = -1;
        template<class Archive> void saveState(Archive& archive) {
            archive(origin, destination, originHeading, destinationHeading, vehicle, driver, startedAt, state, previousControl, blip);
        }
    } taxiShortcut;
    std::vector<Roadblock> roadblocks;
    std::vector<Rope> swatRopes;
    std::map<int32_t, CutsceneAttachment> cutsceneAttachments;
    std::array<int8_t, 2> nextVehicleComponents{-2, -2};
    bool vehicleComponentsPending = false;
    std::set<int32_t> explicitVehicleComponents;
    std::function<bool(int32_t, const std::array<int8_t, 2>&,
        std::array<int32_t, 2>&)> selectVehicleComponents;
    std::set<int32_t> bigWheelVehicles;
    std::set<int32_t> fadingVehicles;
    std::set<int32_t> brokenGlassObjects;
    std::set<int32_t> crackedGlassObjects;
    std::set<uint64_t> deletedNativeObjects;
    std::set<int32_t> frontSeatOnlyCharacters;
    std::array<float, 3> policeRadioPosition{};
    uint64_t policeRadioSerial = 0;
    uint8_t musicTrack = 0;
    uint64_t musicSerial = 0;
    bool musicPlaying = false, musicLooped = false;
    std::vector<std::string> uncompressedCutsceneAnimations;
    bool liftCamera = false, cutsceneShadows = true, hurricanesAllowed = true;
    bool playerInStadium = false, autoAim = true;
    uint8_t riotIntensity = 0;
    int32_t minSuspendedWanted = 0;
    float minSuspendedChaos = 0;
    uint32_t wantedSuspendedAt = 0;
    std::function<void()> suspendWanted;
    uint8_t scriptPause = 0, scriptInputPause = 0;
    uint32_t busFaresCollected = 0;
    int32_t lastCrushedVehicle = -1;
    uint8_t crusherRewardMultiplier = 1;
    std::map<int32_t, int32_t> crusherTargets;
    std::map<size_t, uint32_t> setPieceLastTriggered;
    std::function<bool(int32_t, int32_t, const std::string&, std::array<float, 16>&)> objectAttachmentMatrix;
    std::function<bool(int32_t, int32_t, bool&)> collisionWaiting;
    std::function<bool(int32_t)> playerTouchesObject;
    std::function<bool(const std::array<float, 6>&, std::string&)> clearNativeObjects;
    std::function<bool(uint8_t, bool, bool, std::string&)> playMusic;
    std::function<bool(const std::array<float, 3>&, std::string&)> policeRadioMessage;
    std::function<bool()> wantedStarsFlashing;
    std::function<bool(int32_t, std::array<float, 3>&, std::vector<std::array<float, 4>>&,
        uint32_t&, int32_t&, std::string&)> crusherVehicleGeometry;
    std::function<size_t()> crusherVehiclePoolCapacity;
    std::function<void()> crusherPlayerDamage;
    std::function<bool(const std::array<float, 3>&, bool&)> nearestGlassShattered;
    bool suppressExplosionSound = false;
    bool valid() const {
        if (brokenGlassObjects.size() > 4096 || crackedGlassObjects.size() > 4096 ||
            deletedNativeObjects.size() > 200000) return false;
        const auto finite = [](const auto& point) {
            for (const auto value : point) if (!std::isfinite(value)) return false;
            return true;
        };
        if (!rampageWeapon.valid() || minSuspendedWanted < 0 || minSuspendedWanted > 6 ||
            !std::isfinite(minSuspendedChaos) || minSuspendedChaos < 0 || minSuspendedChaos > 10000 ||
            roadblocks.size() > 16 || swatRopes.size() > 8 || cutsceneAttachments.size() > 256 ||
            uncompressedCutsceneAnimations.size() > 7 ||
            bigWheelVehicles.size() > 4096 || fadingVehicles.size() > 4096 || explicitVehicleComponents.size() > 4096 ||
            frontSeatOnlyCharacters.size() > 128 ||
            crusherTargets.size() > 4096 || setPieceLastTriggered.size() > 256 ||
            !finite(policeRadioPosition) || taxiShortcut.state > 6 ||
            !finite(taxiShortcut.origin) || !finite(taxiShortcut.destination) ||
            !std::isfinite(taxiShortcut.originHeading) || !std::isfinite(taxiShortcut.destinationHeading)) return false;
        for (const auto& block : roadblocks) {
            if (!finite(block.start) || !finite(block.end) || block.vehicles.size() > 16 ||
                block.characters.size() > 32) return false;
        }
        for (const auto& rope : swatRopes) {
            if (!std::isfinite(rope.progress) || rope.progress < 0) return false;
            for (const auto& point : rope.points) if (!finite(point)) return false;
            for (const auto& velocity : rope.velocities) if (!finite(velocity)) return false;
        }
        for (const auto& attachment : cutsceneAttachments) {
            if (attachment.first < 0 || attachment.second.object != attachment.first ||
                attachment.second.parent < 0 || attachment.second.component.size() > 8) return false;
        }
        for (const auto& name : uncompressedCutsceneAnimations) if (name.empty() || name.size() > 8) return false;
        for (int32_t handle : frontSeatOnlyCharacters) if (handle < 1) return false;
        for (const auto& target : crusherTargets) if (target.first < 0 || target.second < 0) return false;
        return true;
    }
    template<class Archive> void saveState(Archive& archive) {
        archive(rampageWeapon, taxiShortcut, roadblocks, swatRopes, cutsceneAttachments,
            nextVehicleComponents, vehicleComponentsPending, explicitVehicleComponents, bigWheelVehicles, fadingVehicles,
            brokenGlassObjects, crackedGlassObjects, deletedNativeObjects,
            frontSeatOnlyCharacters, policeRadioPosition, policeRadioSerial, musicTrack, musicSerial,
            musicPlaying, musicLooped, uncompressedCutsceneAnimations, liftCamera, cutsceneShadows, hurricanesAllowed,
            playerInStadium, autoAim, riotIntensity, minSuspendedWanted, minSuspendedChaos,
            wantedSuspendedAt, scriptPause, scriptInputPause,
            busFaresCollected, lastCrushedVehicle, crusherRewardMultiplier, crusherTargets,
            setPieceLastTriggered);
    }
};
