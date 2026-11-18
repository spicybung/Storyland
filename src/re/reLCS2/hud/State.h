#pragma once

#include <algorithm>
#include <cstdint>

inline int32_t storylandLcsVisibleMoneyStep(int32_t visible,int32_t money) {
    const int64_t difference=int64_t(money)-visible;
    const int64_t magnitude=difference<0?-difference:difference;
    if(!magnitude)return visible;
    const int32_t step=magnitude>100000?12345:magnitude>10000?1234:
        magnitude>1000?123:magnitude>50?42:1;
    return int32_t(int64_t(visible)+(difference<0?-step:step));
}

struct StorylandLcsHudState {
    int32_t visibleMoney=0;
    uint32_t armourLostAt=0;
    bool moneyInitialized=false;

    void resetMoney(int32_t money) {visibleMoney=money;moneyInitialized=true;}
    void advanceMoney(int32_t money) {
        if(!moneyInitialized)resetMoney(money);
        else visibleMoney=storylandLcsVisibleMoneyStep(visibleMoney,money);
    }
};

struct StorylandLcsHudFlashState {
    int healthDraws=50,armourDraws=50;
    uint32_t lastFrame=0;
    bool frameKnown=false;

    void advance(int16_t& flashing,float health,float armour,bool healthDrawn,
        bool armourDrawn,uint32_t frame) {
        if(frameKnown&&lastFrame==frame)return;
        frameKnown=true;lastFrame=frame;
        int* remaining=nullptr;
        if(flashing==4&&health>10.0f&&healthDrawn)remaining=&healthDraws;
        else if(flashing==3&&armour>10.0f&&armourDrawn)remaining=&armourDraws;
        if(remaining&&--*remaining==-1) {flashing=-1;*remaining=50;}
    }
};

#include <cstdint>

struct StorylandLcsRadarBlinkState {
    uint32_t changedAt=0;
    bool visible=false;

    bool update(uint32_t pauseMilliseconds) {
        if(uint32_t(pauseMilliseconds-changedAt)>(visible?500u:200u)) {
            changedAt=pauseMilliseconds;
            visible=!visible;
        }
        return visible;
    }
};
