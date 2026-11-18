#pragma once
#include "src/storyland/storyland_mission_program.h"
#include "src/storyland/storyland_scm.h"
#include <cmath>

inline bool storylandCaptureIntroRestartPoint(StorylandMissionEnvironment& world,
    bool martinezMeeting) {
    if (!world.introMissionProtected || world.introMissionRestartQueued ||
        world.introRestartMartinezCaptured || world.health <= 0 || world.playerArrested ||
        !std::isfinite(world.x) || !std::isfinite(world.y) ||
        !std::isfinite(world.z) || !std::isfinite(world.heading) ||
        std::hypot(world.x,world.y) < 1.0f) return false;
    if (martinezMeeting || !world.introRestartPositionValid) {
        world.introRestartPosition = {world.x,world.y,world.z};
        world.introRestartHeading = world.heading;
        world.introRestartPositionValid = true;
        world.introRestartMartinezCaptured = martinezMeeting;
        return true;
    }
    return false;
}

inline int32_t storylandFindIntroMissionSlot(const StorylandScmFile& script, bool vcs) {
    if (!vcs) return -1;
    for (size_t index = 0; index < script.missions().size(); ++index) {
        const auto& mission = script.missions()[index];
        if (mission.textTable == "JER_A1") return int32_t(index);
    }
    return -1;
}

inline void storylandConfigureIntroMission(StorylandMissionEnvironment& world,
    const StorylandScmFile& script, bool vcs, bool playGame) {
    world.introMissionSlot = storylandFindIntroMissionSlot(script, vcs);
    world.introMissionProtected = playGame && world.introMissionSlot >= 0;
    world.introMissionRestartQueued = false;
    world.introRestartPosition = {};
    world.introRestartHeading = 0;
    world.introRestartPositionValid = false;
    world.introRestartMartinezCaptured = false;
    storylandCaptureIntroRestartPoint(world,false);
}

inline bool storylandIntroPagerCheckpointEligible(const StorylandMissionEnvironment& world,
    int activeMissionSlot,bool vcs,bool alreadyCaptured) {
    return vcs&&!alreadyCaptured&&world.introMissionProtected&&
        !world.introMissionRestartQueued&&world.health>0&&
        activeMissionSlot==world.introMissionSlot&&world.introMissionSlot>=0&&
        world.cutscene.name=="JERA1"&&world.cutscene.playing&&
        world.cutscene.elapsedMilliseconds<=1000u&&
        std::isfinite(world.x)&&std::isfinite(world.y)&&std::isfinite(world.z);
}

inline bool storylandHoldIntroMissionForRestart(StorylandMissionEnvironment& world,
    const StorylandMissionProgram& program, const StorylandMissionVm& mission) {
    if (world.introMissionRestartQueued) return true;
    if (!world.introMissionProtected) return false;
    if (mission.missionSlot == world.introMissionSlot && world.cutscene.name == "JERA1" &&
        world.cutscene.playing && world.cutscene.elapsedMilliseconds <= 1000)
        storylandCaptureIntroRestartPoint(world,true);
    else if (!world.introRestartPositionValid)
        storylandCaptureIntroRestartPoint(world,false);
    if (program.activeMission() > world.introMissionSlot ||
        (mission.missionSlot == world.introMissionSlot &&
            mission.missionOutcome == StorylandMissionOutcome::Passed)) {
        world.introMissionProtected = false;
        return false;
    }
    if (world.health <= 0 || world.playerArrested || world.criticalMissionRestartRequested)
        world.introMissionRestartQueued = true;
    return world.introMissionRestartQueued;
}

inline void storylandAdvanceIntroRestartClock(StorylandMissionEnvironment& world,
    bool vcs, uint32_t elapsedMilliseconds) {
    StorylandMissionVm clock;
    clock.environment = &world;
    clock.vcs = vcs;
    clock.isMissionThread = false;
    clock.tick(elapsedMilliseconds);
}
