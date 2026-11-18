#pragma once
#include "src/leeds/leeds_native_world.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <vector>
#include <functional>

enum class StorylandCrime { Gunshot, Assault, Killing, VehicleTheft, Shooting, Burning, Runover, Explosion, NastyAssault };
struct StorylandCrimeEvent {
    StorylandCrime type;
    StorylandNativePoint position;
    bool policeVictim=false;
    uint32_t source=0;
    bool policeDoesntCare=false;
    uint8_t lcsType() const {
        switch(type) {
        case StorylandCrime::Gunshot:return 1;
        case StorylandCrime::Assault:return policeVictim?3:2;
        case StorylandCrime::Shooting:return policeVictim?5:4;
        case StorylandCrime::VehicleTheft:return 6;
        case StorylandCrime::Runover:return policeVictim?11:10;
        case StorylandCrime::Burning:return policeVictim?14:13;
        case StorylandCrime::Explosion:return 17;
        case StorylandCrime::NastyAssault:return policeVictim?19:18;
        default:return 0;
        }
    }
    uint32_t lcsLifetime() const {
        switch(type) {
        case StorylandCrime::Assault:case StorylandCrime::NastyAssault:return 2000;
        case StorylandCrime::VehicleTheft:return 1500;
        case StorylandCrime::Shooting:case StorylandCrime::Burning:return 10000;
        default:return 1000;
        }
    }
};
struct StorylandRegisteredCrimeEvent {
    uint8_t type=0,state=0; // Native event lifecycle: normal, clear, can-delete.
    uint32_t source=0,updatedAt=0,lifetime=0;
};
struct StorylandQueuedCrime {
    uint8_t type=0;
    uint32_t source=0,startedAt=0;
    StorylandNativePoint position;
    bool reported=false,policeDoesntCare=false;
};
struct StorylandCrimeState {
    std::vector<StorylandCrimeEvent> events;
    std::array<float,9> nextReport{};
    std::array<StorylandQueuedCrime,16> queuedCrimes{};
    std::array<StorylandRegisteredCrimeEvent,64> registeredEvents{};
    uint32_t milliseconds=0,lastUpdateAt=0;
    // Both Stories HUDs use this transient two-second wanted-level timer.
    uint32_t vcsWantedChangedAt=0;
    bool vcsWantedChangeKnown=false;
    double millisecondRemainder=0;
    float heat=0,unseenSeconds=0;
    int publishedWanted=0;
    bool policeVisible=false,searching=false;
    bool policeNearby=false,policeAtCrime=false;
    bool vcs=true;
    std::function<bool(const StorylandNativePoint&,float)> policePresence;
    void report(StorylandCrime type,StorylandNativePoint position,float now,bool policeVictim=false,
        uint32_t source=0,bool policeDoesntCare=false) {
        const size_t index=size_t(type);
        if(index>=nextReport.size()||!std::isfinite(now)||events.size()>=64)return;
        events.push_back({type,position,policeVictim,source,policeDoesntCare});
    }
    float threshold(int stars) const {
        // VCS PS2 SLUS_215.90: wanted-level comparisons at 0x003AA938.
        constexpr float vcsThresholds[]={0,40,180,550,1200,2400,4800};
        constexpr float lcsThresholds[]={0,50,180,550,1200,2400,4800};
        const int level=std::clamp(stars,0,6);
        return vcs?vcsThresholds[level]:lcsThresholds[level];
    }
    void setWantedLevel(int wanted) {
        // VCS SetWantedLevel switch at 0x003AABD0..0x003AAC18.
        constexpr float vcsLevels[]={0,60,200,570,1220,2420,4820};
        constexpr float lcsLevels[]={0,70,200,570,1220,2420,4820};
        const int level=std::clamp(wanted,0,6);
        heat=vcs?vcsLevels[level]:lcsLevels[level];
        unseenSeconds=0;
        if(level!=publishedWanted) {
            vcsWantedChangedAt=milliseconds;
            vcsWantedChangeKnown=true;
        }
        publishedWanted=level;
        queuedCrimes={};
    }
    void synchronize(int wanted) {
        if(wanted!=publishedWanted)setWantedLevel(wanted);
    }
    void addLcsChaos(uint8_t type,float multiplier,bool policeDoesntCare=false) {
        constexpr float weights[]={0,0,5,45,30,80,15,10,5,5,18,80,400,20,80,20,500,25,35,100};
        if(type>=std::size(weights)||!std::isfinite(multiplier)||multiplier<=0)return;
        const float sensitivity=multiplier*(policeDoesntCare?0.333f:1.0f);
        const float weight=vcs&&type==9?10.0f:weights[type];
        heat=std::min(10000.0f,std::trunc(heat+weight*sensitivity));
        unseenSeconds=0;
    }
    void witnessed(const StorylandCrimeEvent& crime,float multiplier) {
        if(!std::isfinite(multiplier)||multiplier<=0)return;
        addLcsChaos(crime.lcsType(),multiplier,crime.policeDoesntCare);
        if(crime.policeVictim&&crime.type==StorylandCrime::Assault)setLcsWantedNoDrop(1,6);
        if(crime.policeVictim&&(crime.type==StorylandCrime::Shooting||crime.type==StorylandCrime::NastyAssault))
            setLcsWantedNoDrop(2,6);
    }
    int lcsWantedLevel(int maximum) {
        constexpr float lcsLimits[]={0,115,365,875,1800,3600,7200};
        constexpr float vcsLimits[]={0,110,365,875,1800,3600,7200};
        const int cap=std::clamp(maximum,0,6);
        heat=std::clamp(heat,0.0f,vcs?vcsLimits[cap]:lcsLimits[cap]);
        int level=0;
        for(int candidate=1;candidate<=cap;++candidate)
            if(heat>=threshold(candidate))level=candidate;
        if(level!=publishedWanted) {
            vcsWantedChangedAt=milliseconds;
            vcsWantedChangeKnown=true;
        }
        publishedWanted=level;
        return level;
    }
    void setLcsWantedNoDrop(int level,int maximum) {
        const int target=std::clamp(level,0,std::clamp(maximum,0,6));
        if(target>publishedWanted)synchronize(target);
    }
    bool queueLcsCrime(const StorylandCrimeEvent& event,bool reported) {
        const uint8_t type=event.lcsType();
        if(type==0)return true;
        for(auto& crime:queuedCrimes) {
            if(crime.type!=type||crime.source!=event.source)continue;
            if(crime.reported)return true;
            crime.reported=reported;
            return false;
        }
        for(auto& crime:queuedCrimes) {
            if(crime.type!=0)continue;
            crime={type,event.source,milliseconds,event.position,reported,event.policeDoesntCare};
            break;
        }
        return false;
    }
    void updateLcsRegisteredEvents() {
        for(auto& event:registeredEvents) {
            if(event.type==0)continue;
            if(uint32_t(milliseconds-event.updatedAt)>event.lifetime||event.state==2)event={};
            else if(event.state==1)event.state=2;
        }
    }
    bool registerLcsEvent(const StorylandCrimeEvent& event) {
        const uint8_t type=event.lcsType();
        if(type==0)return false;
        for(auto& current:registeredEvents) {
            if(current.type!=type||current.source!=event.source)continue;
            // CEventList refreshes an existing entity event and returns before CWanted reporting.
            current.updatedAt=milliseconds;current.lifetime=event.lcsLifetime();
            return false;
        }
        for(auto& current:registeredEvents) {
            if(current.type!=0)continue;
            current={type,uint8_t(type==1?(vcs?2:1):0),event.source,milliseconds,event.lcsLifetime()};
            break;
        }
        return true; // Native crime reporting still runs when the event pool is full.
    }
    void processLcsEvents(float multiplier,int maximum) {
        for(const auto& event:events) {
            if(!registerLcsEvent(event))continue;
            // Preserve the original immediate-report branch: it leaves the queue
            // entry unreported until UpdateCrimesQ, unless SetWantedLevel clears it.
            const bool previouslyReported=queueLcsCrime(event,false);
            const bool present=policePresence?policePresence(event.position,14.0f):policeAtCrime;
            if(present&&!previouslyReported) {
                addLcsChaos(event.lcsType(),multiplier,event.policeDoesntCare);
                lcsWantedLevel(maximum);
            }
            if(present||event.lcsType()==3)setLcsWantedNoDrop(1,maximum);
            if(event.lcsType()==5||event.lcsType()==19)setLcsWantedNoDrop(2,maximum);
        }
        events.clear();
    }
    void processLcsQueue(float multiplier,int maximum) {
        for(auto& crime:queuedCrimes) {
            if(crime.type==0)continue;
            const uint32_t age=milliseconds-crime.startedAt;
            if(age>500&&!crime.reported) {
                addLcsChaos(crime.type,multiplier,crime.policeDoesntCare);
                crime.reported=true;
                lcsWantedLevel(maximum);
            }
            if(age>10000)crime={};
        }
    }
    int update(float seconds,int maximum,bool ignored,float multiplier=1.0f) {
        if(!std::isfinite(seconds)||seconds<=0)return publishedWanted;
        // CWanted::Update and its sixteen-entry crime queue share these
        // rules in both supplied PS2 executables (VCS 0x3AA760/0x3AB388).
        const double elapsed=double(seconds)*1000.0+millisecondRemainder;
        const double whole=std::floor(elapsed);
        millisecondRemainder=elapsed-whole;
        milliseconds+=uint32_t(std::fmod(whole,4294967296.0));
        lcsWantedLevel(maximum);
        updateLcsRegisteredEvents();
        processLcsEvents(multiplier,maximum);
        if(uint32_t(milliseconds-lastUpdateAt)>1000) {
            if(publishedWanted>1)lastUpdateAt=milliseconds;
            else if(!policeNearby) {
                lastUpdateAt=milliseconds;
                heat=std::max(0.0f,heat-1.0f);
                lcsWantedLevel(maximum);
            }
            processLcsQueue(multiplier,maximum);
        }
        // Ignore flags suppress pursuit, not crime registration or chaos.
        searching=publishedWanted>0&&(!policeVisible||ignored);
        return publishedWanted;
    }
};
