#include "src/storyland/storyland_game_save_policy.h"
#include "src/storyland/storyland_save.h"
#include "src/storyland/storyland_user_directories.h"

bool StorylandLinuxSession::canSaveGame(std::string* error) const {
    const auto reject = [&](const char* reason) {
        if (error) *error = reason;
        return false;
    };
    if (error) error->clear();
    if (!running || !playingGame || !program.active())
        return reject("Start a game before saving.");
    const int flag = environment.declaredMissionFlag;
    const bool missionFlag = flag >= 0 && size_t(flag) < mission.globals.size() &&
        mission.globals[size_t(flag)] != 0;
    const bool liveMission = mission.isMissionThread &&
        (mission.state == StorylandMissionState::Running || mission.state == StorylandMissionState::Waiting);
    if ((missionFlag && environment.saveGameFinished) || liveMission)
        return reject("Finish the active mission before saving.");
    if (environment.cutscene.loaded || environment.cutscene.playing)
        return reject("Saving is unavailable during a cutscene.");
    if (!environment.undressedCharacters.empty())
        return reject("Finish the character clothing transition before saving.");
    if (environment.mobilePhones.count(vcs() ? 0 : 1))
        return reject("Finish the phone call before saving.");
    if (runtime.health <= 0)
        return reject("Saving is unavailable while the player is wasted.");
    if (runtime.playerReaction.scriptedDive())
        return reject("Finish the scripted player evasive dive before saving.");
    if (runtime.wanted != 0)
        return reject("Lose the wanted level before saving.");
    if (runtime.occupiedVehicle >= 0 || runtime.entryTransition >= 0 ||
        runtime.vehicleAction != StorylandPlayTestRuntime::VehicleAction::None)
        return reject("Leave the vehicle before saving.");
    return true;
}

std::filesystem::path StorylandLinuxSession::saveGameSlotPath(
    const std::filesystem::path& documents, int slot, bool previous) const {
    const auto directory = previous ? storylandPreviousGameUserDirectory(documents, vcs()) :
        storylandGameUserDirectory(documents, vcs());
    return storylandGameSaveSlot(directory, vcs(), slot);
}

bool StorylandLinuxSession::hasGameSaveSlot(const std::filesystem::path& documents, int slot) const {
    for (bool previous : {false, true}) {
        const auto path = saveGameSlotPath(documents, slot, previous);
        std::error_code error;
        if (!path.empty() && std::filesystem::is_regular_file(path, error)) return true;
    }
    return false;
}

bool StorylandLinuxSession::saveGameSlot(const std::filesystem::path& documents,
    int slot, std::string& error) {
    error.clear();
    const auto path = saveGameSlotPath(documents, slot);
    if (path.empty()) {
        error = "Saving requires a Documents directory and a slot from one to eight.";
        return false;
    }
    if (!canSaveGame(&error)) return false;
    storyland_save::State state;
    if (!captureCheckpoint(state, error)) return false;
    storylandPrepareSavedGame(state.environment, vcs());
    std::error_code directoryError;
    std::filesystem::create_directories(path.parent_path(), directoryError);
    if (directoryError) {
        error = "Cannot create the save directory: " + directoryError.message();
        return false;
    }
    if (!storyland_save::write(path, state, mission, error)) return false;
    storylandSetPlayTestClock(runtime,state.environment.hour);
    environment.hour=runtime.clockHour;
    environment.clockResetRequested=false;
    runtime.statistics = environment.statistics = state.environment.statistics;
    environment.playerSprintEnergy = state.environment.playerSprintEnergy;
    environment.saveMenuRequested = false;
    environment.saveGameFinished = true;
    return true;
}

bool StorylandLinuxSession::loadGameSlot(const std::filesystem::path& documents,
    int slot, std::string& error) {
    error.clear();
    auto path = saveGameSlotPath(documents, slot);
    if (path.empty()) {
        error = "Loading requires a Documents directory and a slot from one to eight.";
        return false;
    }
    std::error_code fileError;
    if (!std::filesystem::is_regular_file(path, fileError))
        path = saveGameSlotPath(documents, slot, true);
    if (!loadCheckpoint(path, error)) return false;
    environment.saveMenuRequested = false;
    environment.saveGameFinished = true;
    return true;
}

bool StorylandLinuxSession::deleteGameSlot(const std::filesystem::path& documents,
    int slot, std::string& error) {
    error.clear();
    if (saveGameSlotPath(documents, slot).empty()) {
        error = "Deleting requires a save directory and a slot from one to eight.";
        return false;
    }
    bool removed = false;
    for (bool previous : {false, true}) {
        const auto path = saveGameSlotPath(documents, slot, previous);
        std::error_code fileError;
        const bool exists = std::filesystem::exists(path, fileError);
        if (!fileError && !exists) continue;
        if (!fileError && !std::filesystem::is_regular_file(path, fileError)) {
            error = "The selected save path is not a regular file.";
            return false;
        }
        if (fileError) {
            error = "Cannot inspect the saved game: " + fileError.message();
            return false;
        }
        if (std::filesystem::remove(path, fileError)) removed = true;
        if (fileError) {
            error = "Cannot delete the saved game: " + fileError.message();
            return false;
        }
    }
    if (!removed) error = "The selected save slot is already empty.";
    return removed;
}
