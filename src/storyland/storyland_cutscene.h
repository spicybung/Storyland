#pragma once
#include "src/leeds/leeds_mocap_archive.h"
#include "src/storyland/storyland_play_test.h"
#include "src/storyland/storyland_mobile_animation.h"
#include <algorithm>
#include <cmath>

struct StorylandCutsceneCamera {
    StorylandNativePoint eye,target;
    float fieldOfView=70,roll=0;
};

inline float storylandCutsceneActorOriginLift(StorylandDtzGameVariant variant,bool psp) {
    return variant==StorylandDtzGameVariant::Vcs||
        (variant==StorylandDtzGameVariant::Lcs&&!psp)?1.0f:0.0f;
}

struct StorylandMissionEnvironment;

class StorylandCutscene {
public:
    struct Subtitle {
        uint32_t start=0,end=0;
        std::string key,text,audioStream;
    };
    struct Actor {
        StorylandPlayTestAsset asset;
        StorylandAnimFile animation;
        std::string clipName;
        bool rootMotion=false;
        bool staticBindPose=false;
        int32_t scriptHandle=-1;
        std::vector<StorylandModelPoint> posed;
        std::vector<StorylandCutsceneFramePose> frames;
        std::array<float,16> entityMatrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    };
    bool load(const StorylandMocapArchive& mocap,const StorylandDtzArchive& archive,
        const std::string& name,std::string& error);
    void clear();
    bool sample(float seconds,std::string& error);
    bool synchronizeScriptActors(const StorylandDtzArchive& archive,
        const StorylandMissionEnvironment& world,std::string& error);
    bool applyAttachments(const StorylandMissionEnvironment& world,std::string& error);
    bool actorAttachmentMatrix(int32_t handle,int32_t bone,const std::string& component,
        std::array<float,16>& matrix) const;
    StorylandCutsceneCamera cameraAt(float seconds) const;
    const Subtitle* subtitleAt(uint32_t milliseconds) const;
    std::string audioStreamName() const;
    bool ready() const {return loaded;}
    float durationSeconds() const {return duration;}
    const StorylandNativePoint& origin() const {return offset;}
    const std::vector<Actor>& actors() const {return actorRows;}
    const std::vector<Subtitle>& subtitles() const {return subtitleRows;}
private:
    struct SplineKey {
        float time=0;
        std::array<float,3> incoming{},value{},outgoing{};
    };
    bool loaded=false,nativeLcs=false,nativeLcsPs2=false;
    float duration=0,actorVerticalOffset=0;
    StorylandNativePoint offset;
    std::vector<Actor> actorRows;
    std::vector<Subtitle> subtitleRows;
    std::array<std::vector<SplineKey>,4> splines;
    bool loadDefinition(const std::vector<uint8_t>& bytes,std::vector<std::string>& models,
        std::vector<std::string>& clips,std::vector<bool>& rootMotion,bool lcs,std::string& error);
    bool loadCamera(const std::vector<uint8_t>& bytes,std::string& error);
    std::array<float,3> sampleSpline(size_t spline,float seconds) const;
};

struct StorylandCutsceneSkipTransition {
    bool active=false;
    uint32_t startedAt=0,durationMilliseconds=1000;
    float initialAlpha=0;
    void begin(uint32_t now,float alpha,uint32_t duration=1000) {
        active=true;startedAt=now;durationMilliseconds=duration;
        initialAlpha=std::isfinite(alpha)?std::clamp(alpha,0.0f,1.0f):0;
    }
    float alphaAt(uint32_t now) const {
        const float amount=durationMilliseconds?std::clamp(float(uint32_t(now-startedAt))/
            float(durationMilliseconds),0.0f,1.0f):1;
        return initialAlpha+(1-initialAlpha)*amount;
    }
};

struct StorylandMissionEnvironment;
class StorylandCutscenePlayer {
public:
    bool update(uint32_t elapsedMilliseconds,const StorylandMocapArchive& mocap,
        const StorylandDtzArchive& archive,StorylandMissionEnvironment& world,std::string& error,
        bool sampleActors=true);
    void clear();
    void skip(StorylandMissionEnvironment& world,uint32_t fadeMilliseconds=1000);
    void discard(StorylandMissionEnvironment& world);
    bool skipping() const {return skipTransition.active;}
    bool advanceSkip(StorylandMissionEnvironment& world);
    bool actorAttachmentMatrix(const StorylandDtzArchive& archive,StorylandMissionEnvironment& world,
        int32_t handle,int32_t bone,const std::string& component,std::array<float,16>& matrix);
    const StorylandCutscene& scene() const {return loadedScene;}
private:
    StorylandCutscene loadedScene;
    uint32_t loadSeen=0,startSeen=0,clearSeen=0;
    bool completionFadeStarted=false;
    StorylandCutsceneSkipTransition skipTransition;
    StorylandCutsceneCamera skipCamera;
    std::array<float,3> skipCameraUp{};
    bool skipCameraValid=false;
};
