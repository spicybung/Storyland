#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

struct StorylandLcsHudBar {
    float left=0,top=0,right=0,bottom=0,fillRight=0,fillU=0;
};

struct StorylandLcsHudUpgrade {
    bool visible=false;
    float x=0,y=0,scaleX=0,scaleY=0;
};

struct StorylandLcsHudLayout {
    float scaleX=1,scaleY=1,top=0;
    bool widescreen=false;

    float x(float value) const {return value*scaleX;}
    float y(float value) const {return top+value*scaleY;}
    float numbersLeft() const {return widescreen?386.0f:373.0f;}
    float wantedStep() const {return widescreen?11.0f:13.5f;}

    StorylandLcsHudBar bar(float level,float maximum,bool armour) const {
        const float value=std::isfinite(level)?std::trunc(level):0.0f;
        const float ratio=std::isfinite(maximum)&&maximum>0?
            std::clamp(value,0.0f,maximum)/maximum:0.0f;
        float fillPixels=(ratio*40.0f+16.0f)+4.0f;
        float fillU=fillPixels/64.0f;
        if(value==0||!std::isfinite(maximum)||maximum<=0)fillPixels=fillU=0;
        else if(value==maximum) {fillPixels=64.0f;fillU=1.0f;}
        const float left=widescreen?375.0f:360.0f;
        const float horizontalScale=widescreen?0.68f:0.85f;
        const float nativeTop=armour?28.0f:40.0f;
        return {x(left),y(nativeTop),x(left+64.0f*horizontalScale),y(nativeTop+10.0f),
            x(left+fillPixels*horizontalScale),fillU};
    }

    StorylandLcsHudUpgrade upgrade(float maximum,bool armour) const {
        if(!std::isfinite(maximum)||maximum<=100.0f)return {};
        const bool large=maximum>125.0f;
        const float horizontal=large?(widescreen?.3375f:.45f):(widescreen?.2625f:.35f);
        return {true,widescreen?387.0f:375.0f,armour?24.0f:36.0f,horizontal,large?.75f:.6f};
    }
};

inline StorylandLcsHudLayout storylandLcsHudLayout(float width,float height,
    float gameplayTop=0,bool widescreen=false) {
    if(!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)return {};
    if(!std::isfinite(gameplayTop))gameplayTop=0;
    gameplayTop=std::clamp(gameplayTop,0.0f,height);
    return {width/480.0f,(height-gameplayTop)/272.0f,gameplayTop,widescreen};
}

struct StorylandLcsHudNumberGlyph {
    std::array<float,4> rectangle{},uv{};
    std::array<uint8_t,4> color{};
};

inline std::vector<StorylandLcsHudNumberGlyph> storylandLcsHudNumbers(
    const std::string& text,float x,float y,bool money,bool widescreen,uint8_t alpha=209) {
    const float width=widescreen?9.0f:11.0f;
    std::array<uint8_t,4> color{255,255,255,alpha};
    std::vector<StorylandLcsHudNumberGlyph> result;
    result.reserve(text.size());
    for(unsigned char character:text) {
        unsigned index=money?21:10;
        if(character>='0'&&character<='9')index=character-'0'+(money?11:0);
        else if(character==0x16||character=='*') {
            index=22;
            color=money?std::array<uint8_t,4>{62,141,188,alpha}:
                std::array<uint8_t,4>{192,155,54,alpha};
        }
        const float u=float(index%8)*.125f,v=float(index/8)*.265625f;
        result.push_back({{x,y,x+width+(index==22?3.0f:0.0f),y+10.0f},
            {u,v,u+.125f,v+.265625f},color});
        x+=width-2.0f;
        if(index==10)x-=4.0f;
        x=std::ceil(x*1.3333333730697632f)*.75f;
    }
    return result;
}

inline bool storylandLcsHealthVisible(float health,int16_t flashing,uint32_t frame) {
    if(flashing==4&&(frame&8u)==0)return false;
    return (std::isfinite(health)&&std::trunc(health)>=10.0f)||(frame&8u)!=0;
}

inline bool storylandLcsArmourVisible(float armour,int16_t flashing,uint32_t frame,
    uint32_t lastLoss,uint32_t now) {
    if(!std::isfinite(armour)||armour<=1.0f||(flashing==3&&(frame&8u)==0))return false;
    return lastLoss==0||uint32_t(lastLoss+2000u)<now||(frame&4u)!=0;
}

inline bool storylandLcsWantedStarVisible(int star,int wanted,int suspended,
    bool changeKnown,uint32_t changedAt,uint32_t now,uint32_t frame) {
    if(star<0||star>=6)return false;
    if(star<wanted)return uint32_t((changeKnown?changedAt:0)+2000u)<now||(frame&4u)!=0;
    return star<suspended&&(frame&4u)!=0;
}

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

struct StorylandLcsRadarLayout {
    float left=24,top=208,width=64,height=48;

    float centerX() const {return left+width*.5f;}
    float centerY() const {return top+height*.5f;}
    float pointX(float normalized) const {return left+(normalized+1.0f)*.5f*width;}
    float pointY(float normalized) const {return top+(1.0f-normalized)*.5f*height;}
};

inline StorylandLcsRadarLayout storylandLcsRadarLayout(bool widescreen=false) {
    return {24,208,widescreen?48.0f:64.0f,48};
}

inline float storylandLcsRadarRange(bool inVehicle,float speed) {
    if(!inVehicle||!std::isfinite(speed))return 96.0f;
    const float nativeSpeed=std::fabs(speed)*.02f;
    if(nativeSpeed<.3f)return 96.0f;
    if(nativeSpeed>=.9f)return 280.0f;
    return ((nativeSpeed-.3f)*184.0f)*1.6666667461395264f+96.0f;
}

inline uint8_t storylandLcsRadarDistanceAlpha(float distance,uint8_t hudOpacity=209) {
    if(distance<=1.0f)return 255;
    if(distance>10.0f)return 128;
    const float blend=(distance-1.0f)*.1111111119389534f;
    const uint8_t combined=uint8_t(int(blend*128.0f)+int((1.0f-blend)*255.0f));
    return uint8_t(float(combined)*(float(hudOpacity)*.003921568859368563f));
}

inline float storylandLcsRadarHorizontal(float extent,bool widescreen) {
    return widescreen?extent:extent*1.1111111640930176f;
}

inline std::array<float,2> storylandLcsRadarSpriteHalfSize(bool widescreen=false) {
    return {float(int(storylandLcsRadarHorizontal(6.8f,widescreen))),6.0f};
}

inline std::array<std::array<float,2>,4> storylandLcsRotatingRadarSprite(
    float x,float y,float angle,bool widescreen=false) {
    const auto size=storylandLcsRadarSpriteHalfSize(widescreen);
    const float corrected=angle-.7853981852531433f;
    std::array<std::array<float,2>,4> corners;
    for(size_t index=0;index<corners.size();++index) {
        const float corner=float(index)*1.5707963705062866f+corrected;
        corners[index]={x+std::sin(corner)*size[0],y+std::cos(corner)*size[1]};
    }
    return corners;
}

struct StorylandLcsRadarTracePrimitive {
    std::array<std::array<float,2>,4> points{};
    std::array<uint8_t,4> color{};
};

inline std::array<StorylandLcsRadarTracePrimitive,2> storylandLcsRadarTrace(
    float x,float y,int16_t scale,uint8_t height,const std::array<uint8_t,4>& color,
    bool widescreen=false) {
    const float extent=std::max(2.0f,float(scale)*.85f);
    std::array<StorylandLcsRadarTracePrimitive,2> result;
    result[0].color={0,0,0,255};
    result[1].color={color[0],color[1],color[2],255};
    if(height<=1) {
        const float edge=extent+1.0f;
        const float outer=storylandLcsRadarHorizontal(edge+3.0f,widescreen);
        const float inner=storylandLcsRadarHorizontal(edge+1.0f,widescreen);
        const float top=y-edge,bottom=y+edge;
        if(height==0) {
            result[0].points={{{x+outer,bottom+2.0f},{x-outer,bottom+2.0f},
                {x,top-3.0f},{x,top-3.0f}}};
            result[1].points={{{x+inner,bottom+1.0f},{x-inner,bottom+1.0f},
                {x,top-1.0f},{x,top-1.0f}}};
        } else {
            result[0].points={{{x,bottom+3.0f},{x,bottom+3.0f},
                {x+outer,top-2.0f},{x-outer,top-2.0f}}};
            result[1].points={{{x,bottom+1.0f},{x,bottom+1.0f},
                {x+inner,top-1.0f},{x-inner,top-1.0f}}};
        }
    } else {
        const float horizontal=storylandLcsRadarHorizontal(extent,widescreen);
        const float left=x-horizontal,right=x+horizontal,top=y-extent,bottom=y+extent;
        result[0].points={{{left-1.0f,top-1.0f},{right+1.0f,top-1.0f},
            {right+1.0f,bottom+1.0f},{left-1.0f,bottom+1.0f}}};
        result[1].points={{{left,top},{right,top},{right,bottom},{left,bottom}}};
    }
    return result;
}

inline uint8_t storylandLcsPausePlayerSprite(float nativeHeading) {
    if(!std::isfinite(nativeHeading))nativeHeading=0;
    float angle=nativeHeading-1.1780972480773926f;
    constexpr float pi=3.1415927410125732f,twoPi=6.2831854820251465f;
    if(angle< -pi||angle>=pi) {
        angle=std::fmod(std::clamp(angle,-25.0f,25.0f),twoPi);
        if(angle< -pi)angle+=twoPi;
        if(angle>=pi)angle-=twoPi;
    }
    if(angle<0)angle+=twoPi;
    return uint8_t(int(7.0f-angle*1.2732394933700562f));
}

inline unsigned storylandLcsRadarBlipPass(bool coordinate,int sprite) {
    if(coordinate&&sprite==25)return 4;
    if((coordinate&&sprite==30)||(!coordinate&&sprite==25))return 0;
    if(sprite==18||sprite==19||sprite==27||sprite==16)return 1;
    return coordinate?2:3;
}

#include "src/leeds/leeds_retail_font.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

enum class StorylandLcsHudTextAlignment {Left,Center,Right};

struct StorylandLcsAmmoText {
    std::string text;
    float x=0,y=42;
    StorylandLcsHudTextAlignment alignment=StorylandLcsHudTextAlignment::Left;
};

struct StorylandLcsWeaponHud {
    std::array<float,4> icon{};
    std::array<StorylandLcsAmmoText,3> ammunition{};
    size_t textCount=0;
    float scaleX=.2f,scaleY=.44f;
    uint8_t alpha=209;
    int32_t nativeWeapon=0;
};

inline StorylandLcsWeaponHud storylandLcsWeaponHud(int32_t nativeWeapon,
    int32_t weaponSlot,int32_t clipSize,int32_t total,int32_t magazine,
    bool widescreen=false,bool ammunitionVisible=true,bool frenzy=false,uint8_t alpha=209) {
    StorylandLcsWeaponHud result;
    result.nativeWeapon=nativeWeapon;
    result.alpha=alpha;
    result.scaleX=widescreen?.18f:.2f;
    result.icon=widescreen?std::array<float,4>{420.0f,16,468.600006103515625f,60}:
        std::array<float,4>{416,16,470,60};
    const int64_t reserve=int64_t(total)-magazine;
    if(!ammunitionVisible||frenzy||nativeWeapon==0||nativeWeapon==34||
        weaponSlot<2||reserve>=9999)return result;
    if(clipSize>=2&&clipSize<1000) {
        const int divisor=nativeWeapon==31?10:1;
        const int displayedMagazine=nativeWeapon==31?std::min(9999,magazine/divisor):magazine;
        result.ammunition[0]={std::to_string(std::min<int64_t>(9999,reserve/divisor)),
            widescreen?438.0f:435.0f,42,StorylandLcsHudTextAlignment::Right};
        result.ammunition[1]={"-",widescreen?439.0f:436.0f,42,StorylandLcsHudTextAlignment::Left};
        result.ammunition[2]={std::to_string(displayedMagazine),widescreen?441.0f:439.0f,42,
            StorylandLcsHudTextAlignment::Left};
        result.textCount=3;
    } else {
        result.ammunition[0]={std::to_string(total),435,42,StorylandLcsHudTextAlignment::Center};
        result.textCount=1;
    }
    return result;
}

inline std::string storylandLcsWeaponIconKey(int32_t nativeWeapon) {
    if(nativeWeapon==0)return "fist";
    if(nativeWeapon==13)return "gren_rem";
    return nativeWeapon>0?"weapon:native:"+std::to_string(nativeWeapon):std::string{};
}

struct StorylandLcsHudFontGlyph {
    std::array<float,4> rectangle{},uv{};
};

inline std::vector<StorylandLcsHudFontGlyph> storylandLcsHudFontText(
    const StorylandRetailFont& font,const std::string& text,float x,float y,
    float scaleX,float scaleY,StorylandLcsHudTextAlignment alignment,
    uint32_t atlasWidth,uint32_t atlasHeight) {
    std::vector<StorylandLcsHudFontGlyph> result;
    if(!atlasWidth||!atlasHeight||!std::isfinite(scaleX)||!std::isfinite(scaleY)||
        scaleX<=0||scaleY<=0)return result;
    const auto advance=[&](const StorylandRetailFontGlyph& glyph) {
        return std::floor((float(glyph.advance)*scaleX)*1.3333333730697632f)*.75f;
    };
    float width=0;
    for(unsigned char character:text) {
        const auto* glyph=storylandRetailTextGlyph(font,character);
        if(glyph)width+=advance(*glyph);
    }
    if(alignment==StorylandLcsHudTextAlignment::Right)x-=width;
    else if(alignment==StorylandLcsHudTextAlignment::Center)x-=width*.5f;
    result.reserve(text.size());
    for(unsigned char character:text) {
        const auto* glyph=storylandRetailTextGlyph(font,character);
        if(!glyph)continue;
        if(character!=' ')result.push_back({{x,y,x+30.72f*scaleX,y+20.0f*scaleY},
            {float(glyph->u)/atlasWidth,float(glyph->v)/atlasHeight,
             float(glyph->u+glyph->atlasWidth)/atlasWidth,
             float(glyph->v+glyph->atlasHeight)/atlasHeight}});
        x+=advance(*glyph);
    }
    return result;
}
