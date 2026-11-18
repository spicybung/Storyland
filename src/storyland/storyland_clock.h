#pragma once

#include <cmath>
#include <cstdint>

struct StorylandClockParts {
    unsigned hour=0,minute=0,second=0;
};

inline StorylandClockParts storylandClockParts(float hour) {
    if(!std::isfinite(hour))return {};
    double normalized=std::fmod(double(hour),24.0);
    if(normalized<0)normalized+=24.0;
    const unsigned seconds=unsigned(std::lround(normalized*3600.0))%86400u;
    return {seconds/3600u,(seconds/60u)%60u,seconds%60u};
}

inline int storylandClockMinute(float hour) {
    const auto clock=storylandClockParts(hour);
    return int(clock.hour*60u+clock.minute);
}

inline float storylandVcsSetClockHour(int32_t hour,int32_t minute) {
    const unsigned normalizedHour=uint8_t(hour)%24u;
    const unsigned normalizedMinute=uint8_t(minute)%60u;
    return float(normalizedHour*60u+normalizedMinute)/60.0f;
}

inline uint32_t storylandVcsMinutesUntil(float hour,int32_t targetHour,int32_t targetMinute) {
    const uint32_t current=uint32_t(storylandClockMinute(hour));
    uint32_t target=uint32_t(uint8_t(targetHour))*60u+uint8_t(targetMinute);
    if(target<current)target+=1440u;
    return target-current;
}

struct StorylandClockProgress {
    double seconds=0;
    double rounding=0;
    float publishedHour=0;
    bool initialized=false;

    void reset(float hour) {
        const auto clock=storylandClockParts(hour);
        seconds=double(clock.hour*3600u+clock.minute*60u+clock.second);
        rounding=0;
        publishedHour=hour;
        initialized=true;
    }

    double advance(float& hour,double elapsedGameSeconds) {
        if(!std::isfinite(hour)||!std::isfinite(elapsedGameSeconds)||elapsedGameSeconds<=0)return 0;
        if(!initialized||hour!=publishedHour)reset(hour);
        const double increment=elapsedGameSeconds-rounding;
        const double total=seconds+increment;
        if(!std::isfinite(total))return 0;
        rounding=(total-seconds)-increment;
        const double days=std::floor((total+0.0000001)/86400.0);
        seconds=total-days*86400.0;
        if(seconds<0)seconds=0;
        // The public hour contains native whole seconds; the fractional tick stays here.
        const auto wholeSeconds=uint32_t(std::floor(seconds+0.0000001));
        hour=float(wholeSeconds%86400u)/3600.0f;
        publishedHour=hour;
        return days;
    }
};
