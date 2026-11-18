#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

struct StorylandScreenDropletCamera {
    std::array<float,3> position{};
    std::array<float,3> right{1.0f,0.0f,0.0f};
    std::array<float,3> up{0.0f,0.0f,1.0f};
    std::array<float,3> forward{0.0f,1.0f,0.0f};
    std::array<float,3> splashPosition{};
    float rain=0.0f;
    float sprayStrength=0.0f;
    bool splashPositionValid=false;
    bool rainBlocked=false;
    bool firstPerson=false;
    bool enabled=true;
    bool movingEnabled=true;
};

struct StorylandScreenDroplet {
    float x=0.0f, y=0.0f;
    float size=4.0f, magnification=1.0f;
    float age=0.0f, lifetime=2.0f;
    std::array<std::uint8_t,4> color{255,255,255,255};
    int movingSlot=-1;
    bool active=false;
    bool blood=false;
};

struct StorylandScreenDropletVertex {
    float x=0.0f, y=0.0f;
    float maskU=0.0f, maskV=0.0f;
    float sceneU=0.0f, sceneV=0.0f;
};

std::array<StorylandScreenDropletVertex,4> storylandScreenDropletQuad(
    const StorylandScreenDroplet& drop,int width,int height);
std::array<std::uint8_t,64*64*4> storylandScreenDropletMask();

class StorylandScreenDroplets {
public:
    static constexpr std::size_t MaximumDrops=2000;
    static constexpr std::size_t MaximumMovingDrops=700;

    void reset();
    void setViewport(int width,int height);
    void addWater(float strength);
    void addBlood(float strength);
    void registerSplash(const std::array<float,3>& position,bool validPosition=true);
    void advance(float seconds,const StorylandScreenDropletCamera& camera);
    void advance(float seconds) {advance(seconds,StorylandScreenDropletCamera{});}
    const std::array<StorylandScreenDroplet,MaximumDrops>& droplets() const {return drops_;}
    std::size_t count() const {return activeCount_;}
    std::size_t movingCount() const {return movingCount_;}

private:
    struct MovingDrop {int index=-1;float distance=0.0f;};
    float randomUnit();
    int spawn(float x,float y,float size,float lifetime,bool blood);
    void fillMoving(float amount,bool blood);
    void attachMoving(int index);
    void deactivate(std::size_t index);
    void trace(MovingDrop& moving);
    std::array<StorylandScreenDroplet,MaximumDrops> drops_{};
    std::array<MovingDrop,MaximumMovingDrops> moving_{};
    std::array<float,3> previousCamera_{};
    std::array<float,3> screenMove_{};
    float cameraDistance_=0.0f;
    float forwardMotionAtThirty_=0.0f;
    double rainBudget_=0.0,sprayBudget_=0.0,splashClock_=0.0;
    std::array<float,3> splashPosition_{};
    int splashTick_=-1;
    int width_=640,height_=480;
    std::size_t activeCount_=0,movingCount_=0;
    bool haveCamera_=false;
    std::uint32_t randomState_=0xD021B25Bu;
};

class StorylandScreenDropletPlayback {
public:
    void reset();
    void advance(bool enabled,float gameSeconds,std::uint32_t waterSerial,
        std::uint32_t bloodSerial,float waterStrength,float bloodStrength,
        const StorylandScreenDropletCamera& camera,int width,int height);
    const StorylandScreenDroplets& simulation() const {return simulation_;}
private:
    StorylandScreenDroplets simulation_;
    bool haveFrame_=false,haveSerials_=false;
    float lastGameSeconds_=0.0f;
    std::uint32_t waterSerial_=0,bloodSerial_=0;
};
