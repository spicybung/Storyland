#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

struct StorylandScriptPlayerNavigation {
    bool seekNodes=false;
    bool localAvoidanceSolver=false;
    bool alternateObstructionHandling=false;
    uint32_t seekNode=UINT32_MAX;
    uint32_t seekWaitUntil=0;
    uint32_t seekRouteCursor=0;
    std::vector<uint32_t> seekRoute;
    std::array<float,2> seekDestination{};

    void clearRoute() {
        seekNode=UINT32_MAX;
        seekWaitUntil=0;
        seekRouteCursor=0;
        seekRoute.clear();
        seekDestination={};
    }
    template<class Archive> void saveState(Archive& archive) {
        archive(seekNodes,localAvoidanceSolver,alternateObstructionHandling,
            seekNode,seekWaitUntil,seekRouteCursor,seekRoute,seekDestination);
    }
};

inline bool storylandValidScriptPlayerNavigation(const StorylandScriptPlayerNavigation& navigation) {
    return navigation.seekRoute.size()<=65536&&navigation.seekRouteCursor<=navigation.seekRoute.size()&&
        std::isfinite(navigation.seekDestination[0])&&std::isfinite(navigation.seekDestination[1]);
}

enum class StorylandScriptPlayerMoveMode : uint8_t { Walk=2, Run=4, Sprint=5 };

struct StorylandScriptPlayerWalkTask {
    bool active=false,completed=false;
    float targetX=0,targetY=0,targetZ=0;
    float radius=.5f;
    StorylandScriptPlayerMoveMode moveMode=StorylandScriptPlayerMoveMode::Walk;

    void clear() { *this={}; }
    template<class Archive> void saveState(Archive& archive) {
        archive(active,completed,targetX,targetY,targetZ,radius);
    }
};

inline bool storylandValidScriptPlayerWalkTask(const StorylandScriptPlayerWalkTask& task) {
    const bool validMode=task.moveMode==StorylandScriptPlayerMoveMode::Walk||
        task.moveMode==StorylandScriptPlayerMoveMode::Run||task.moveMode==StorylandScriptPlayerMoveMode::Sprint;
    return validMode&&!(task.active&&task.completed)&&
        (task.moveMode==StorylandScriptPlayerMoveMode::Walk||task.active||task.completed)&&
        std::isfinite(task.targetX)&&std::isfinite(task.targetY)&&
        std::isfinite(task.targetZ)&&std::isfinite(task.radius)&&task.radius==.5f;
}

inline bool storylandScriptPlayerWalkArrived(const StorylandScriptPlayerWalkTask& task,float x,float y) {
    return std::hypot(task.targetX-x,task.targetY-y)<=task.radius;
}
