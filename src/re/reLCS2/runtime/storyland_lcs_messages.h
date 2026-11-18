#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include <algorithm>

inline void storylandRememberLcsBrief(StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::LcsMessage& message) {
    if(!message.addToBrief||message.key.empty())return;
    const auto same=[&](const auto& previous) {
        return previous.key==message.key&&previous.missionText==message.missionText&&
            previous.numbers==message.numbers&&previous.stringKey==message.stringKey;
    };
    if(std::any_of(world.lcsPreviousBriefs.begin(),world.lcsPreviousBriefs.end(),same))return;
    world.lcsPreviousBriefs.insert(world.lcsPreviousBriefs.begin(),message);
    if(world.lcsPreviousBriefs.size()>5)world.lcsPreviousBriefs.resize(5);
}

inline void storylandSyncLcsMessageView(StorylandMissionEnvironment& world) {
    if(!world.lcsMessageQueuesUsed)return;
    const StorylandMissionEnvironment::LcsMessage* shown=nullptr;int32_t style=0;
    if(!world.lcsBriefMessages.empty())shown=&world.lcsBriefMessages.front();
    else for(size_t index=0;index<world.lcsBigMessages.size();++index)if(!world.lcsBigMessages[index].empty()) {
        shown=&world.lcsBigMessages[index].front();style=int32_t(index+1);break;
    }
    world.messageStyle=style;
    if(!shown) {world.messageKey.clear();world.messageNumbers.clear();world.messageMilliseconds=0;return;}
    world.messageKey=shown->key;world.messageNumbers=shown->numbers;
    const uint32_t elapsed=world.gameMilliseconds-shown->startedAt;
    world.messageMilliseconds=elapsed<shown->durationMilliseconds?shown->durationMilliseconds-elapsed:1;
}

inline void storylandUseLcsMessageQueues(StorylandMissionEnvironment& world) {
    if(world.lcsMessageQueuesUsed)return;
    if(!world.messageKey.empty()&&world.messageMilliseconds) {
        StorylandMissionEnvironment::LcsMessage message;
        message.key=world.messageKey;message.missionText=world.missionText;
        message.numbers=world.messageNumbers;message.durationMilliseconds=world.messageMilliseconds;
        message.startedAt=world.gameMilliseconds;
        if(world.messageStyle>0&&world.messageStyle<=8)world.lcsBigMessages[size_t(world.messageStyle-1)].push_back(std::move(message));
        else world.lcsBriefMessages.push_back(std::move(message));
    }
    world.lcsMessageQueuesUsed=true;
}

enum class StorylandLcsMessagePriority {Append,Now,Soon};
inline bool storylandQueueLcsMessage(StorylandMissionEnvironment& world,
    StorylandMissionEnvironment::LcsMessage message,int32_t style=0,
    StorylandLcsMessagePriority priority=StorylandLcsMessagePriority::Now) {
    if(style<0||style>8||message.numbers.size()>6)return false;
    storylandUseLcsMessageQueues(world);
    auto& queue=style?world.lcsBigMessages[size_t(style-1)]:world.lcsBriefMessages;
    const size_t limit=style?4:8;
    message.startedAt=world.gameMilliseconds;
    const bool visible=queue.empty()||priority==StorylandLcsMessagePriority::Now;
    if(priority==StorylandLcsMessagePriority::Append) {
        if(queue.size()>=limit)return false;
        queue.push_back(std::move(message));
    } else if(priority==StorylandLcsMessagePriority::Soon&&!queue.empty()) {
        queue.insert(queue.begin()+1,std::move(message));
        if(queue.size()>limit)queue.resize(limit);
    } else if(queue.empty())queue.push_back(std::move(message));
    else queue.front()=std::move(message);
    if(!style&&visible)storylandRememberLcsBrief(world,queue.front());
    storylandSyncLcsMessageView(world);
    return true;
}

inline void storylandClearLcsMessageKey(StorylandMissionEnvironment& world,const std::string& key,bool big) {
    storylandUseLcsMessageQueues(world);
    const auto clear=[&](auto& queue,bool brief) {
        const bool removedHead=!queue.empty()&&queue.front().key==key;
        queue.erase(std::remove_if(queue.begin(),queue.end(),[&](const auto& message){return message.key==key;}),queue.end());
        if(removedHead&&!queue.empty()) {
            queue.front().startedAt=world.gameMilliseconds;
            if(brief)storylandRememberLcsBrief(world,queue.front());
        }
    };
    if(big)for(auto& queue:world.lcsBigMessages)clear(queue,false);
    else clear(world.lcsBriefMessages,true);
    storylandSyncLcsMessageView(world);
}

inline void storylandAdvanceLcsMessages(StorylandMissionEnvironment& world) {
    if(!world.lcsMessageQueuesUsed)return;
    const auto advance=[&](auto& queue,bool brief) {
        if(queue.empty()||world.gameMilliseconds-queue.front().startedAt<=queue.front().durationMilliseconds)return;
        queue.erase(queue.begin());
        if(!queue.empty()) {
            queue.front().startedAt=world.gameMilliseconds;
            if(brief)storylandRememberLcsBrief(world,queue.front());
        }
    };
    advance(world.lcsBriefMessages,true);
    for(auto& queue:world.lcsBigMessages)advance(queue,false);
    storylandSyncLcsMessageView(world);
}

inline void storylandSkipLcsMessages(StorylandMissionEnvironment& world) {
    if(!world.lcsMessageQueuesUsed) {
        world.messageKey.clear();world.messageNumbers.clear();world.messageMilliseconds=0;world.messageStyle=0;
        return;
    }
    // MESSAGE_WAIT expires active heads; the queued messages retain their own full durations.
    if(!world.lcsBriefMessages.empty())world.lcsBriefMessages.front().startedAt=0;
    for(auto& queue:world.lcsBigMessages)if(!queue.empty())queue.front().startedAt=0;
    storylandSyncLcsMessageView(world);
}

inline void storylandClearLcsMessages(StorylandMissionEnvironment& world,
    bool clearBriefMessages, bool clearBigMessages) {
    storylandUseLcsMessageQueues(world);
    if (clearBriefMessages) {
        world.lcsBriefMessages.clear();
    }
    if (clearBigMessages) {
        for (auto& queue : world.lcsBigMessages) {
            queue.clear();
        }
    }
    storylandSyncLcsMessageView(world);
}
