#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

// VCS PS2 CPager: 8 priority-ordered entries, 25 UTF-16 display cells.
// Native routines 0x126668, 0x126698, 0x1269A8 and 0x126F88.
struct StorylandPager {
    struct Message {
        std::u16string text;
        uint16_t speed=0,priority=0,unknown=0,length=0;
        int16_t position=-35;
        uint32_t next=0;
    };
    std::vector<Message> messages;
    uint16_t displayState=0;
    bool soundPlayed=false,soundRequested=false;
    float displayOffset=150;
    bool displayed() const {return displayState>0;}
    void clear() {messages.clear();}
    void add(std::u16string text,int32_t number,bool numbered,int32_t speed,
        int32_t priority,int32_t unknown,uint32_t now) {
        uint16_t length=uint16_t(text.size());
        size_t token=text.find(u"~1~");
        while(token!=std::u16string::npos) {
            const auto value=std::to_string(numbered?number:-1);
            const std::u16string replacement(value.begin(),value.end());
            text.replace(token,3,replacement);
            token=text.find(u"~1~",token+replacement.size());
            number=-1;
        }
        if(numbered)length=uint16_t(text.size());
        Message item{std::move(text),uint16_t(speed),uint16_t(priority),uint16_t(unknown),length,-35,now+uint16_t(speed)};
        const auto where=std::find_if(messages.begin(),messages.end(),[&](const auto& queued) {
            return queued.priority<item.priority;
        });
        if(where==messages.end()&&messages.size()==8)return;
        messages.insert(where,std::move(item));
        if(messages.size()>8)messages.pop_back();
    }
    void updateDisplay(bool textPresent) {
        // CHud 322C08 retains its entrance and exit state independently of CPager.
        if(!textPresent&&displayState==1) {displayState=2;soundPlayed=false;}
        if(!textPresent&&displayState!=2)return;
        if(displayState==0) {displayState=1;displayOffset=150;}
        if(displayState==1) {
            if(displayOffset>0)
                displayOffset=std::max(0.0f,displayOffset-std::clamp(displayOffset*.1f,2.0f,10.0f));
            if(!soundPlayed) {soundPlayed=true;soundRequested=true;}
        } else if(displayState==2) {
            displayOffset+=std::max(2.0f,displayOffset*.1f);
            if(displayOffset>150) {displayOffset=150;displayState=0;}
        }
    }
    std::u16string update(uint32_t now,bool scrollSuppressed,bool displaySuppressed=false) {
        soundRequested=false;
        if(!messages.empty()&&messages.front().position>=messages.front().length)
            messages.erase(messages.begin());
        std::u16string visible;
        if(!messages.empty()) {
            auto& item=messages.front();
            for(int cell=0;cell<25;++cell) {
                const int index=item.position+cell;
                if(index>=int(item.text.size()))break;
                visible+=index<0?u' ':item.text[size_t(index)];
            }
            // CPager 1267A4 publishes before either advancing or resetting.
            if(scrollSuppressed) {item.position=-35;item.next=now+item.speed;}
            else if(now>item.next) {++item.position;item.next=now+item.speed;}
        }
        if(!displaySuppressed)updateDisplay(!visible.empty());
        return displaySuppressed?std::u16string{}:visible;
    }
};
