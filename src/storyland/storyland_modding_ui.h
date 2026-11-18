#pragma once

#include "src/audio/storyland_mission_voice_profiles.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

struct StorylandDyomProject;

enum class StorylandModdingFileKind { Scm, ScmPatch, Dyom, DyomVcs = Dyom, Disassembly };

struct StorylandModdingServices {
    std::filesystem::path scmPath;
    std::filesystem::path gameDirectory;
    std::filesystem::path userDirectory;
    bool gameLoaded = false;
    bool vcs = false;
    bool playerAvailable = false;
    std::array<float, 4> playerPlacement{};
    bool canStartDyom = false;
    std::string playRestriction;
    bool dyomActive = false;
    bool dyomFinished = false;
    bool dyomFailed = false;
    size_t dyomPc = 0;
    int dyomObjective = -1;
    size_t dyomObjectiveCount = 0;
    std::string dyomObjectiveText;
    std::string dyomStatus;
    std::function<bool(const StorylandDyomProject&, std::string&)> playDyom;
    std::function<void()> stopDyom;
    StorylandMissionVoiceBackend previewMissionVoice;
    std::string missionVoiceBackendName;
    std::string missionVoicePlaybackStatus;
    bool missionVoiceBusy = false;
    bool missionVoiceFailed = false;
    std::function<void()> stopMissionVoice;
    std::function<std::string(int32_t)> modelName;
    std::function<std::filesystem::path(StorylandModdingFileKind, bool)> chooseFile;
};

class StorylandModdingUi {
public:
    StorylandModdingUi();
    ~StorylandModdingUi();
    StorylandModdingUi(const StorylandModdingUi&) = delete;
    StorylandModdingUi& operator=(const StorylandModdingUi&) = delete;

    void showScm(const std::filesystem::path& path = {});
    void showDyom(const std::filesystem::path& path = {});
    bool requestOpen(const std::filesystem::path& path);
    void draw(const StorylandModdingServices& services);
    bool visible() const;
    bool hasUnsavedChanges() const;

private:
    struct Workspace;
    std::unique_ptr<Workspace> workspace;
};
