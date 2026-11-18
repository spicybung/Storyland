#pragma once

#include "src/leeds/leeds_texture.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

inline std::pair<int,int> storylandLoadingArtworkOrder(std::string name) {
    std::transform(name.begin(),name.end(),name.begin(),
        [](unsigned char value){return char(std::tolower(value));});
    const size_t prefix=name.compare(0,6,"loadsc")==0?6u:
        name.compare(0,6,"splash")==0?6u:0u;
    if(!prefix||name.size()==prefix)return {-1,-1};
    int number=0;
    for(size_t index=prefix;index<name.size();++index) {
        if(name[index]<'0'||name[index]>'9'||number>99)return {-1,-1};
        number=number*10+name[index]-'0';
    }
    return {name.compare(0,6,"loadsc")==0?0:1,number};
}

struct StorylandLoadingScreen {
    std::filesystem::path source;
    std::string name;
    RgbaImage image;
    bool fillViewport = false;
};

std::vector<StorylandLoadingScreen> storylandReadLoadingScreens(
    const std::filesystem::path& gamePath, bool vcs = false,
    const std::filesystem::path& artworkDirectory = {});
std::vector<StorylandLoadingScreen> storylandReadVcsSplashScreens(
    const std::filesystem::path& artworkDirectory = {});

inline std::vector<size_t> storylandLoadingScreenSelection(
    const std::vector<StorylandLoadingScreen>& screens, bool vcs, unsigned chapter = 1) {
    std::vector<size_t> selection;
    if (!vcs) {
        for (size_t index = 0; index < screens.size(); ++index) selection.push_back(index);
        return selection;
    }

    // Storyland's chapter groups; the retail PS2 selector uses all six artwork pairs.
    static constexpr std::array<std::array<int, 6>, 3> chapterArtwork{{
        {{0, 4, 7, 9, 10, -1}},
        {{0, 1, 3, 7, 11, -1}},
        {{0, 2, 3, 5, 6, 8}}
    }};
    if (chapter < 1 || chapter > chapterArtwork.size()) chapter = 1;
    for (int number : chapterArtwork[chapter - 1]) {
        if (number < 0) continue;
        for (size_t index = 0; index < screens.size(); ++index) {
            const auto order = storylandLoadingArtworkOrder(screens[index].name);
            if (order.first >= 0 && order.second == number) {
                selection.push_back(index);
                break;
            }
        }
    }
    return selection;
}

class StorylandLoadingSequence {
public:
    static constexpr uint64_t screenMilliseconds=2000;
    static constexpr size_t minimumScreens=3;

    void begin(size_t count) {
        screenCount=count;
        currentScreen=0;
        shownScreens=0;
        screenStartedAt=0;
        presented=false;
        finished=false;
        running=true;
    }
    void reselect(size_t count,size_t selectedIndex,bool preserveCurrentScreen) {
        screenCount=count;
        currentScreen=selectedIndex<count?selectedIndex:0;
        shownScreens=std::min(shownScreens,count);
        if(!preserveCurrentScreen||selectedIndex>=count) {
            screenStartedAt=0;
            presented=false;
        }
    }
    void ready() {finished=true;}
    void cancel() {running=false;}
    bool active() const {return running;}
    bool workFinished() const {return finished;}
    size_t index() const {return currentScreen;}
    size_t shown() const {return shownScreens;}

    bool present(uint64_t now) {
        if(!running)return false;
        if(!screenCount) {
            if(finished)running=false;
            return running;
        }
        if(!presented) {
            presented=true;
            screenStartedAt=now;
            if(shownScreens<screenCount)++shownScreens;
            return true;
        }
        const uint64_t elapsed=now>=screenStartedAt?now-screenStartedAt:0;
        if(elapsed<screenMilliseconds)return true;
        if(finished&&shownScreens>=std::min(screenCount,minimumScreens)) {
            running=false;
            return false;
        }
        currentScreen=(currentScreen+1)%screenCount;
        shownScreens=std::min(shownScreens+1,screenCount);
        screenStartedAt=now;
        return true;
    }

private:
    size_t screenCount=0,currentScreen=0,shownScreens=0;
    uint64_t screenStartedAt=0;
    bool presented=false,finished=false,running=false;
};
