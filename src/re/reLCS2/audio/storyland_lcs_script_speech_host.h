#pragma once
#include "src/re/reLCS2/audio/storyland_lcs_script_speech.h"
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include <cmath>
#include <map>

struct StorylandLcsScriptSpeechHost {
    std::map<int32_t,StorylandLcsScriptSpeechSpeaker> speakers;
    StorylandLcsScriptSpeechGlobal shared;
    StorylandNativePoint previousCamera{};
    uint32_t cameraFrame=UINT32_MAX;
    unsigned cameraSamples=0;
    float cameraTravel=0,cameraAverageSpeed=0;

    void clear() { *this={}; }

    void updateCamera(uint32_t frame,const StorylandNativePoint& camera) {
        if(frame==cameraFrame)return;
        if(cameraFrame!=UINT32_MAX)
            cameraTravel+=std::hypot(camera.x-previousCamera.x,
                camera.y-previousCamera.y,camera.z-previousCamera.z);
        previousCamera=camera;cameraFrame=frame;
        if(++cameraSamples==4) {
            cameraAverageSpeed=cameraTravel/4.0f;
            cameraTravel=0;cameraSamples=0;
        }
    }

    static bool speakerState(const StorylandMissionEnvironment& world,
        const StorylandPlayTestRuntime& runtime,
        const std::vector<StorylandNativeTrafficActor>& pedestrians,
        int32_t key,float& height,bool& alive) {
        if(key==0) {height=world.z;alive=world.health>0;return true;}
        if(key<0) {
            const size_t slot=size_t(-int64_t(key)-2);
            if(slot>=runtime.pedestrians.size()||slot>=pedestrians.size())return false;
            height=pedestrians[slot].position.z;alive=runtime.pedestrians[slot].health>0;return true;
        }
        const auto found=world.missionCharacters.find(key);
        if(found==world.missionCharacters.end())return false;
        height=found->second.z;alive=found->second.alive&&found->second.health>0;return true;
    }

    bool queue(StorylandMissionEnvironment& world,const StorylandPlayTestRuntime& runtime,
        const std::vector<StorylandNativeTrafficActor>& pedestrians,
        int32_t speaker,uint16_t event,const StorylandNativePoint& camera) {
        if(event!=StorylandLcsScriptSpeechEvent)return false;
        const int32_t key=speaker<0?0:size_t(speaker)<runtime.pedestrians.size()?
            -speaker-2:speaker-int32_t(runtime.pedestrians.size());
        float height=0;bool alive=false;
        if(!speakerState(world,runtime,pedestrians,key,height,alive))return false;
        updateCamera(world.nativeFrameCounter,camera);
        return storylandQueueLcsScriptSpeech(speakers[key],event,world.gameMilliseconds,
            height,camera.z,cameraAverageSpeed,[&world] {return world.nextNativeRandom();});
    }

    void service(StorylandMissionEnvironment& world,const StorylandPlayTestRuntime& runtime,
        const std::vector<StorylandNativeTrafficActor>& pedestrians,const StorylandNativePoint& camera) {
        updateCamera(world.nativeFrameCounter,camera);
        for(auto entry=speakers.begin();entry!=speakers.end();) {
            float height=0;bool alive=false;
            if(!speakerState(world,runtime,pedestrians,entry->first,height,alive)) {
                entry=speakers.erase(entry);continue;
            }
            if(alive)storylandServiceLcsScriptSpeech(entry->second,shared,
                world.gameMilliseconds,false,[&world] {return world.nextNativeRandom();});
            ++entry;
        }
    }
};
