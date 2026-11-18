#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

struct StorylandVcsHudBarLayout {
    float left=0,top=0,right=0,bottom=0,fillRight=0,fillU=0;
};

struct StorylandVcsHudLayout {
    float scaleX=1,scaleY=1,top=0;
    bool alternate=false;

    float x(float value) const {return value*scaleX;}
    float y(float value) const {return top+value*scaleY;}
    float barLeft() const {return alternate?375.0f:360.0f;}
    float barScale() const {return alternate?0.68f:0.85f;}
    float moneyLeft() const {return alternate?386.0f:373.0f;}
    float wantedStep() const {return alternate?11.0f:13.5f;}

    StorylandVcsHudBarLayout bar(float level,float maximum,bool armour) const {
        const float ratio=std::isfinite(level)&&std::isfinite(maximum)&&maximum>0?
            std::clamp(std::trunc(level)/maximum,0.0f,1.0f):0.0f;
        const bool exactFull=std::isfinite(level)&&std::isfinite(maximum)&&maximum>0&&
            std::trunc(level)==maximum;
        const float fill=exactFull?1.0f:ratio>0?(ratio*40.0f+20.0f)/64.0f:0.0f;
        const float left=barLeft(),width=64.0f*barScale();
        const float nativeTop=armour?29.0f:41.0f;
        return {x(left),y(nativeTop),x(left+width),y(nativeTop+10.0f),
            x(left+width*fill),fill};
    }

    StorylandVcsHudBarLayout meter(float fraction) const {
        const float ratio=std::isfinite(fraction)?std::clamp(fraction,0.0f,1.0f):0;
        const float fill=ratio==1.0f?1.0f:ratio>0?(ratio*40.0f+20.0f)/64.0f:0;
        const float left=barLeft(),width=64.0f*barScale();
        return {x(left),y(29.0f),x(left+width),y(39.0f),x(left+width*fill),fill};
    }
};

inline StorylandVcsHudLayout storylandVcsHudLayout(float width,float height,float gameplayTop=0,
    bool alternate=false) {
    if(!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)return {};
    if(!std::isfinite(gameplayTop))gameplayTop=0;
    gameplayTop=std::clamp(gameplayTop,0.0f,height);
    return {width/480.0f,(height-gameplayTop)/272.0f,gameplayTop,alternate};
}

inline bool storylandVcsActiveWantedStarVisible(bool changeKnown,uint32_t changedAt,
    uint32_t now,uint32_t frame) {
    return !changeKnown||uint32_t(now-changedAt)>2000u||(frame&4u)!=0;
}

#include <algorithm>
#include <cmath>
#include <cstdint>

struct StorylandVcsHudMeter {
    enum class Phase : uint8_t { Visible, Hidden, FadeIn, FadeOut, Flash };

    uint8_t alpha = 0;
    Phase phase = Phase::Hidden;
    uint8_t flashFrames = 0;

    void flash() {
        phase = Phase::Flash;
        flashFrames = 40;
    }

    void advance(float nativeStep, uint32_t frame) {
        if (!std::isfinite(nativeStep) || nativeStep < 0) return;
        const float scaled = nativeStep * .02f;
        const uint8_t amount = uint8_t(uint32_t(std::min(scaled * 255.0f, 255.0f)));
        switch (phase) {
        case Phase::Visible:
            alpha = 255;
            break;
        case Phase::Hidden:
            alpha = 0;
            break;
        case Phase::FadeIn: {
            const unsigned increased = unsigned(alpha) + amount;
            alpha = uint8_t(std::min(increased, 255u));
            if (increased > 255) phase = Phase::Visible;
            break;
        }
        case Phase::FadeOut:
            if (alpha < amount) {
                alpha = 0;
                phase = Phase::Hidden;
            } else alpha = uint8_t(alpha - amount);
            break;
        case Phase::Flash:
            if ((frame & 8u) != 0) alpha = 255;
            else {
                alpha = 0;
                --flashFrames;
                if (flashFrames == 255) phase = Phase::Visible;
            }
            break;
        }
    }

    bool valid() const {
        return uint8_t(phase) <= uint8_t(Phase::Flash) &&
            (phase != Phase::Flash || flashFrames <= 40);
    }

    template<class Archive> void saveState(Archive& archive) {
        archive(alpha, phase, flashFrames);
    }
};

struct StorylandVcsHudMeters {
    StorylandVcsHudMeter oxygen;
    StorylandVcsHudMeter sprint;
    bool oxygenEnabled = true;
    bool sprintEnabled = true;
    uint32_t lastFrame = UINT32_MAX;

    void flash(int16_t object) {
        // CHud::FlashItem, SLUS_215.90 0x31F3F0.
        if (object == 10) oxygen.flash();
        else if (object == 11) sprint.flash();
    }

    void advance(bool swimming, float nativeStep, uint32_t frame) {
        if (lastFrame == frame || !std::isfinite(nativeStep) || nativeStep < 0) return;
        lastFrame = frame;
        using Phase = StorylandVcsHudMeter::Phase;
        // Native 0x31E168 gives the swimming meter priority over armour.
        if (swimming) {
            if (sprint.phase != Phase::Hidden) sprint.phase = Phase::FadeOut;
            else if (oxygen.phase != Phase::Visible && oxygen.phase != Phase::Flash)
                oxygen.phase = Phase::FadeIn;
        } else {
            if (sprint.phase != Phase::Hidden) sprint.phase = Phase::FadeOut;
            if (oxygen.phase != Phase::Hidden) oxygen.phase = Phase::FadeOut;
        }
        oxygen.advance(nativeStep, frame);
        sprint.advance(nativeStep, frame);
    }

    bool oxygenVisible() const {
        return oxygenEnabled && oxygen.phase != StorylandVcsHudMeter::Phase::Hidden;
    }

    bool sprintVisible() const {
        return sprintEnabled && sprint.phase != StorylandVcsHudMeter::Phase::Hidden;
    }

    bool valid() const {
        return oxygen.valid() && sprint.valid();
    }

    template<class Archive> void saveState(Archive& archive) {
        archive(oxygen, sprint, oxygenEnabled, sprintEnabled);
    }
};

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

struct StorylandVcsHudNumberGlyph {
    std::array<float,4> rectangle{},uv{};
    std::array<uint8_t,4> color{};
};

inline std::vector<StorylandVcsHudNumberGlyph> storylandVcsHudNumbers(
    const std::string& text,float x,float y,bool money,bool widescreen,uint8_t alpha=209) {
    const float width=widescreen?9.0f:11.0f;
    std::vector<StorylandVcsHudNumberGlyph> result;
    result.reserve(text.size());
    for(const unsigned char character:text) {
        unsigned index=money?21:10;
        if(character>='0'&&character<='9')index=character-'0'+(money?11:0);
        else if(character=='*')index=22;
        const float glyphWidth=width+(index==22?3.0f:0.0f);
        const float u=float(index%8)*.125f,v=float(index/8)*.265625f;
        const std::array<uint8_t,4> color=index==22?
            (money?std::array<uint8_t,4>{62,141,188,alpha}:
                std::array<uint8_t,4>{241,170,57,alpha}):
            std::array<uint8_t,4>{255,255,255,alpha};
        result.push_back({{x,y,x+glyphWidth,y+10},{u,v,u+.125f,v+.265625f},color});
        x+=width-2;
        if(index==10)x-=4;
        x=std::ceil(x*1.3333333730697632f)*.75f;
    }
    return result;
}

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

struct StorylandVcsRadarLayout {
    float left=28,top=204,width=64,height=48;
    float centerX() const {return left+width*.5f;}
    float centerY() const {return top+height*.5f;}
    float pointX(float normalized) const {return centerX()+normalized*width*.5f;}
    float pointY(float normalized) const {return centerY()+normalized*height*.5f;}
};

inline StorylandVcsRadarLayout storylandVcsRadarLayout(bool alternate=false) {
    return {28,204,alternate?48.0f:64.0f,48};
}

inline uint8_t storylandVcsRadarDistanceAlpha(float distance,uint8_t hudOpacity=209) {
    if(distance<=1.0f)return 255;
    if(distance>10.0f)return 128;
    const float blend=(distance-1.0f)*.1111111119389534f;
    const uint8_t combined=uint8_t(int(blend*128.0f)+int((1.0f-blend)*255.0f));
    return uint8_t(float(combined)*(float(hudOpacity)*.003921568859368563f));
}

struct StorylandRadarTracePrimitive {
    std::array<std::array<float,2>,4> points{};
    std::array<uint8_t,4> color{};
};

inline std::array<StorylandRadarTracePrimitive,3> storylandVcsRadarTrace(
    float x,float y,uint8_t scale,uint8_t height,const std::array<uint8_t,4>& color,
    bool alternate=false) {
    const float vertical=float(int(float(scale)*.85f));
    const float horizontal=float(int(float(scale)*.85f*(alternate?1.0f:1.3333333730697632f)));
    std::array<StorylandRadarTracePrimitive,3> result;
    for(size_t pass=0;pass<result.size();++pass) {
        const float border=pass==0?3.0f:1.0f;
        auto& primitive=result[pass];
        primitive.color=pass==0?std::array<uint8_t,4>{0,0,0,color[3]}:
            pass==1?std::array<uint8_t,4>{255,255,255,color[3]}:color;
        if(height==0) {
            primitive.points={{{x+horizontal+border,y+vertical+(pass==0?2.0f:1.0f)},
                {x-horizontal-border,y+vertical+(pass==0?2.0f:1.0f)},
                {x,y-vertical-border},{x,y-vertical-border}}};
        } else if(height==1) {
            primitive.points={{{x,y+vertical+border},{x,y+vertical+border},
                {x+horizontal+border,y-vertical-(pass==0?2.0f:1.0f)},
                {x-horizontal-border,y-vertical-(pass==0?2.0f:1.0f)}}};
        } else {
            const float outline=pass==0?1.0f:0.0f;
            primitive.points={{{x-horizontal-outline,y-vertical-outline},
                {x+horizontal+outline,y-vertical-outline},
                {x+horizontal+outline,y+vertical+outline},
                {x-horizontal-outline,y+vertical+outline}}};
        }
    }
    return result;
}

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

struct StorylandVcsHudRectangle {
    float left=0,top=0,right=0,bottom=0;
};

struct StorylandVcsAmmoText {
    std::string text;
    StorylandVcsHudRectangle clip;
    int32_t alignment=0x12;
};

struct StorylandVcsWeaponHud {
    StorylandVcsHudRectangle icon;
    std::array<StorylandVcsAmmoText,3> ammunition;
    size_t textCount=0;
    float fontScale=.24f;
    uint8_t alpha=209;
    bool weaponVisible=false;
    int32_t nativeWeapon=-1;
};

inline StorylandVcsWeaponHud storylandVcsWeaponHud(int32_t nativeWeapon,
    int32_t weaponSlot,int32_t clipSize,int32_t total,int32_t magazine,
    bool alternate=false,bool weaponVisible=true,bool ammunitionVisible=true,
    bool nativeModeHidesAmmo=false,uint8_t alpha=209) {
    StorylandVcsWeaponHud result;
    result.nativeWeapon=nativeWeapon;
    result.weaponVisible=weaponVisible;
    result.alpha=alpha;
    result.icon=alternate?StorylandVcsHudRectangle{419.5f,16,465,60}:
        StorylandVcsHudRectangle{416,16,470,60};
    if(!weaponVisible||!ammunitionVisible||nativeModeHidesAmmo||nativeWeapon==0||
        nativeWeapon==36||nativeWeapon==39||weaponSlot<2)return result;
    const int64_t reserve=int64_t(total)-magazine;
    if(reserve>=10000)return result;
    if(clipSize>=2&&clipSize<1000) {
        const int32_t divisor=nativeWeapon==33?10:1;
        const bool dotted=nativeWeapon==34||nativeWeapon==35;
        const float offset=dotted?-4.0f:0.0f;
        result.ammunition[0]={std::to_string(std::min<int64_t>(9999,reserve/divisor)),
            {414,39,438,53},0x14};
        result.ammunition[1]={dotted?".":"-",{438+offset*.5f,39,444+offset*.5f,53},0x12};
        result.ammunition[2]={std::to_string(std::min(9999,magazine/divisor)),
            {444+offset,39,462+offset,53},0x11};
        result.textCount=3;
    } else {
        result.ammunition[0]={std::to_string(total),{411,39,462,53},0x12};
        result.textCount=1;
    }
    return result;
}

inline std::string storylandVcsWeaponIconKey(int32_t nativeWeapon) {
    if(nativeWeapon==0)return "fist";
    if(nativeWeapon==13)return "gren_rem";
    return nativeWeapon>0?"weapon:native:"+std::to_string(nativeWeapon):std::string{};
}

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct StorylandVcsVehicleCaption {
    enum class Phase : uint8_t { Hidden, Hold, FadeIn, FadeOut, Replace };
    std::string previousKey, displayedKey;
    uint32_t displayMilliseconds = 0;
    int32_t fadeMilliseconds = 0;
    Phase phase = Phase::Hidden;
    bool drawing = false;
};

inline std::string storylandVcsVehicleNameKey(const std::vector<uint8_t>& bytes,int32_t model) {
    const auto word = [&](size_t offset) -> uint32_t {
        if (offset > bytes.size() || bytes.size() - offset < 4) return 0;
        return uint32_t(bytes[offset]) | uint32_t(bytes[offset+1]) << 8 |
            uint32_t(bytes[offset+2]) << 16 | uint32_t(bytes[offset+3]) << 24;
    };
    if (model < 0 || uint32_t(model) >= word(0x38)) return {};
    const uint32_t table = word(0x3C);
    if (!table || uint64_t(table) + uint64_t(model) * 4 + 4 > bytes.size()) return {};
    const uint32_t info = word(size_t(table) + size_t(model) * 4);
    if (!info || uint64_t(info) + 0x160 > bytes.size() || bytes[size_t(info)+0x10] != 6) return {};
    // Native23B180 passes CVehicleModelInfo+158 to the original GXT lookup.
    std::string key;
    for (size_t index = 0; index < 8 && bytes[size_t(info)+0x158+index]; ++index)
        key.push_back(char(bytes[size_t(info)+0x158+index]));
    return key;
}

inline void storylandAdvanceVcsVehicleCaption(StorylandVcsVehicleCaption& caption,
    const std::string& currentKey,bool visible,bool helpText,float nativeTimeStep,uint8_t& areaPhase) {
    caption.drawing = false;
    if (!visible || !std::isfinite(nativeTimeStep) || nativeTimeStep < 0) return;
    using Phase = StorylandVcsVehicleCaption::Phase;
    if (currentKey.empty()) {
        caption.previousKey.clear();
        caption.phase = Phase::Hidden;
        caption.displayMilliseconds = 0;
        caption.fadeMilliseconds = 0;
        return;
    }
    if (currentKey != caption.previousKey) {
        if (caption.phase == Phase::Hidden) {
            caption.displayedKey = currentKey;
            caption.phase = Phase::FadeIn;
            caption.displayMilliseconds = 0;
            caption.fadeMilliseconds = 0;
            if (areaPhase == 1 || areaPhase == 2) areaPhase = 3;
        } else {
            caption.phase = Phase::Replace;
            caption.displayMilliseconds = 0;
        }
        caption.previousKey = currentKey;
    }
    if (caption.phase == Phase::Hidden) return;
    const int32_t delta = int32_t(nativeTimeStep * 20.0f);
    switch (caption.phase) {
    case Phase::FadeIn:
        caption.fadeMilliseconds += delta;
        if (caption.fadeMilliseconds > 1000) {
            caption.fadeMilliseconds = 1000;
            caption.phase = Phase::Hold;
        }
        break;
    case Phase::FadeOut:
    case Phase::Replace:
        caption.fadeMilliseconds -= delta;
        if (caption.fadeMilliseconds < 0) {
            caption.fadeMilliseconds = 0;
            if (caption.phase == Phase::Replace) {
                caption.phase = Phase::FadeIn;
                caption.displayedKey = caption.previousKey;
                caption.displayMilliseconds = 0;
            } else caption.phase = Phase::Hidden;
        }
        break;
    case Phase::Hold: {
        int32_t signedMilliseconds = 0;
        std::memcpy(&signedMilliseconds,&caption.displayMilliseconds,sizeof(signedMilliseconds));
        if (float(signedMilliseconds) > 10000.0f) {
            caption.phase = Phase::FadeOut;
            caption.fadeMilliseconds = 1000;
        }
        break;
    }
    case Phase::Hidden:
        return;
    }
    if (helpText) return;
    caption.displayMilliseconds += uint32_t(delta);
    // Native320970 supplies opaque white even while the phase timer changes.
    caption.drawing = true;
}
