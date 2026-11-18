#pragma once
#include <algorithm>
#include <cmath>

struct StorylandHudClusterLayout {
    float scale=1;
    float right=0,top=0,statsRight=0;
    float barLeft=0,barWidth=144,barHeight=17;
    float clockTop=0,healthTop=0,armourTop=0,moneyTop=0,wantedTop=0;
    float weaponLeft=0,weaponTop=0,weaponSize=64;
    float ammunitionCenter=0,ammunitionTop=0;
    float numberHeight=18,ammunitionHeight=16,wantedSpacing=25;
};

inline StorylandHudClusterLayout storylandHudClusterLayout(float width,float height,
    bool hasArmour,float gameplayTop=0) {
    StorylandHudClusterLayout layout;
    if(!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)return layout;
    if(!std::isfinite(gameplayTop))gameplayTop=0;
    layout.scale=std::clamp(std::min(height/720.0f,width/640.0f),.5f,2.0f);
    const float scale=layout.scale;
    layout.right=width-28*scale;
    layout.top=std::max(34*scale,gameplayTop+10*scale);
    layout.statsRight=layout.right-76*scale;
    layout.barWidth=144*scale;
    layout.barHeight=17*scale;
    layout.barLeft=layout.statsRight-layout.barWidth;
    layout.clockTop=layout.top;
    layout.healthTop=layout.top+24*scale;
    layout.armourTop=layout.healthTop+23*scale;
    layout.moneyTop=(hasArmour?layout.armourTop:layout.healthTop)+23*scale;
    layout.wantedTop=layout.moneyTop+24*scale;
    layout.weaponLeft=layout.right-64*scale;
    layout.weaponTop=layout.top;
    layout.weaponSize=64*scale;
    layout.ammunitionCenter=layout.weaponLeft+layout.weaponSize*.5f;
    layout.ammunitionTop=layout.weaponTop+layout.weaponSize+4*scale;
    layout.numberHeight=18*scale;
    layout.ammunitionHeight=16*scale;
    layout.wantedSpacing=25*scale;
    return layout;
}
