static void clearStorylandPendingCharacterSpeech() {
    for(auto& state:gPlayTestSpeechStates) {
        state.queue.pending=StorylandNoCharacterSpeech;state.pendingAudioSample=-1;
        state.lcsQueue.pending=StorylandLcsNoSpeechEvent;
    }
    for(auto& entry:gPlayTestMissionSpeechStates) {
        entry.second.queue.pending=StorylandNoCharacterSpeech;entry.second.pendingAudioSample=-1;
    }
}

static void ensureStorylandCharacterSpeechStorage() {
    storylandEnsureCharacterSpeechIdentities(gPlayTestRuntime,gPlayTestMissionEnvironment);
    const size_t required=gPlayTestRuntime.pedestrians.size()+1;
    std::vector<uint64_t> identities(required);
    identities[0]=StorylandPlayerSpeechIdentity;
    for(size_t index=0;index<gPlayTestRuntime.pedestrians.size();++index) {
        const auto actor=storylandResolveCharacterSpeechActor(gPlayTestRuntime,gPlayTestMissionEnvironment,
            gNativePedestrians,{gPlayerWorldX,gPlayerWorldY,gPlayerWorldZ},int(index));
        if(actor.valid)identities[index+1]=actor.identity;
    }
    std::map<int32_t,uint64_t> missionIdentities;
    for(const auto& entry:gPlayTestMissionEnvironment.missionCharacters) {
        const auto actor=storylandResolveCharacterSpeechActor(gPlayTestRuntime,gPlayTestMissionEnvironment,
            gNativePedestrians,{gPlayerWorldX,gPlayerWorldY,gPlayerWorldZ},
            int(gPlayTestRuntime.pedestrians.size())+entry.first);
        if(actor.valid)missionIdentities.emplace(entry.first,actor.identity);
    }
    if(identities==gPlayTestSpeechIdentities&&missionIdentities==gPlayTestMissionSpeechIdentities&&
        gPlayTestSpeechStates.size()==required&&gPlayTestSpeechTickets.size()==required)return;
    const auto conversationIdentity=[&](int index) {
        return index>=0&&size_t(index)+1<gPlayTestSpeechIdentities.size()?
            gPlayTestSpeechIdentities[size_t(index)+1]:uint64_t(0);
    };
    const uint64_t conversationA=conversationIdentity(gPlayTestConversationA);
    const uint64_t conversationB=conversationIdentity(gPlayTestConversationB);
    const uint64_t conversationSpeaker=conversationIdentity(gPlayTestConversationSpeaker);
    struct StoredSpeech {
        StorylandCharacterSpeechState state;
        std::shared_ptr<StorylandGameAudioTicket> ticket;
    };
    std::map<uint64_t,StoredSpeech> retained;
    const auto preserve=[&](uint64_t identity,StorylandCharacterSpeechState state,
        std::shared_ptr<StorylandGameAudioTicket> ticket) {
        if(!identity) {if(ticket)ticket->cancelled=true;return;}
        const auto found=retained.find(identity);
        if(found==retained.end())retained.emplace(identity,StoredSpeech{std::move(state),std::move(ticket)});
        else if(ticket&&ticket!=found->second.ticket)ticket->cancelled=true;
    };
    for(size_t index=0;index<gPlayTestSpeechStates.size();++index)
        preserve(index<gPlayTestSpeechIdentities.size()?gPlayTestSpeechIdentities[index]:0,
            std::move(gPlayTestSpeechStates[index]),
            index<gPlayTestSpeechTickets.size()?std::move(gPlayTestSpeechTickets[index]):nullptr);
    for(size_t index=gPlayTestSpeechStates.size();index<gPlayTestSpeechTickets.size();++index)
        if(gPlayTestSpeechTickets[index])gPlayTestSpeechTickets[index]->cancelled=true;
    for(auto& entry:gPlayTestMissionSpeechStates) {
        const auto identity=gPlayTestMissionSpeechIdentities.find(entry.first);
        const auto ticket=gPlayTestMissionSpeechTickets.find(entry.first);
        preserve(identity==gPlayTestMissionSpeechIdentities.end()?0:identity->second,
            std::move(entry.second),ticket==gPlayTestMissionSpeechTickets.end()?nullptr:std::move(ticket->second));
    }
    for(auto& entry:gPlayTestMissionSpeechTickets)
        if(entry.second)entry.second->cancelled=true;
    gPlayTestSpeechStates.assign(required,{});
    gPlayTestSpeechTickets.assign(required,{});
    gPlayTestMissionSpeechStates.clear();gPlayTestMissionSpeechTickets.clear();
    for(size_t index=0;index<identities.size();++index) {
        const auto found=retained.find(identities[index]);
        if(found==retained.end())continue;
        gPlayTestSpeechStates[index]=std::move(found->second.state);
        gPlayTestSpeechTickets[index]=std::move(found->second.ticket);
        retained.erase(found);
    }
    for(const auto& entry:missionIdentities) {
        const auto found=retained.find(entry.second);
        if(found==retained.end())continue;
        gPlayTestMissionSpeechStates.emplace(entry.first,std::move(found->second.state));
        if(found->second.ticket)gPlayTestMissionSpeechTickets.emplace(entry.first,std::move(found->second.ticket));
        retained.erase(found);
    }
    for(auto& entry:retained)if(entry.second.ticket)entry.second.ticket->cancelled=true;
    gPlayTestSpeechIdentities=std::move(identities);
    gPlayTestMissionSpeechIdentities=std::move(missionIdentities);
    const auto conversationSlot=[&](uint64_t identity) {
        if(!identity)return -1;
        const auto found=std::find(gPlayTestSpeechIdentities.begin()+1,gPlayTestSpeechIdentities.end(),identity);
        return found==gPlayTestSpeechIdentities.end()?-1:int(found-gPlayTestSpeechIdentities.begin())-1;
    };
    gPlayTestConversationA=conversationSlot(conversationA);
    gPlayTestConversationB=conversationSlot(conversationB);
    gPlayTestConversationSpeaker=conversationSlot(conversationSpeaker);
    if(gPlayTestConversationA<0||gPlayTestConversationB<0||gPlayTestConversationSpeaker<0) {
        gPlayTestConversationA=gPlayTestConversationB=gPlayTestConversationSpeaker=-1;
        if(conversationA||conversationB)gPlayTestNextConversation=gPlayTestRuntime.elapsed+15;
    }
}

static bool playStorylandNativeSpeech(int speaker,uint16_t event,uint64_t expectedIdentity=0) {
    if((gPlayTestMissionVm.vcs&&gPlayTestSpeechBank.empty())||gPlayTestRuntime.paused||
        gPlayTestRuntime.pauseMenu||gPlayTestDebugMenuOpen||gPlayTestCutsceneActive||
        gPlayTestMissionEnvironment.cutscene.playing)return false;
    ensureStorylandCharacterSpeechStorage();
    const auto actor=storylandResolveCharacterSpeechActor(gPlayTestRuntime,gPlayTestMissionEnvironment,
        gNativePedestrians,{gPlayerWorldX,gPlayerWorldY,gPlayerWorldZ},speaker,expectedIdentity);
    if(!actor.valid)return false;
    auto& state=actor.mission?gPlayTestMissionSpeechStates[actor.handle]:
        gPlayTestSpeechStates[actor.player?0:size_t(actor.speaker)+1];
    if(!gPlayTestMissionVm.vcs) {
        if(!actor.player)return false;
        return storylandQueueLcsPlayerSpeech(state.lcsQueue,event,
            gPlayTestMissionEnvironment.gameMilliseconds,actor.position.z,gPlayTestCamera.eye.z,
            gPlayTestCharacterSpeechCamera.averageDisplacement,
            []{return gPlayTestCharacterSpeechGlobal.nextRandom();});
    }
    return storylandQueueVcsCharacterSpeech(state.queue,event,
        gPlayTestMissionEnvironment.gameMilliseconds,actor.position.z,gPlayTestCamera.eye.z,
        gPlayTestCharacterSpeechCamera.averageDisplacement,
        []{return gPlayTestCharacterSpeechGlobal.nextRandom();});
}

static void serviceStorylandCharacterSpeech() {
    static StorylandVcsSpeechAudio nativeSpeechAudio;
    static StorylandLcsSpeechAudio nativeLcsSpeechAudio;
    if((gPlayTestMissionVm.vcs&&gPlayTestSpeechBank.empty())||gPlayTestRuntime.paused||
        gPlayTestRuntime.pauseMenu||gPlayTestDebugMenuOpen||gPlayTestCutsceneActive||
        gPlayTestMissionEnvironment.cutscene.playing)return;
    ensureStorylandCharacterSpeechStorage();
    bool servicedNativeComment=false;
    const auto service=[&](int speaker) {
        const auto actor=storylandResolveCharacterSpeechActor(gPlayTestRuntime,gPlayTestMissionEnvironment,
            gNativePedestrians,{gPlayerWorldX,gPlayerWorldY,gPlayerWorldZ},speaker);
        if(!actor.valid)return;
        if(!gPlayTestMissionVm.vcs&&!actor.player)return;
        const bool burning=storylandCharacterHasActiveFire(gPlayTestRuntime,gPlayTestMissionEnvironment,
            gDtzArchive.nativeWorld(),actor);
        if(actor.mission&&!burning&&!gPlayTestMissionSpeechStates.count(actor.handle))return;
        auto& state=actor.mission?gPlayTestMissionSpeechStates[actor.handle]:
            gPlayTestSpeechStates[actor.player?0:size_t(speaker)+1];
        auto& ticket=actor.mission?gPlayTestMissionSpeechTickets[actor.handle]:
            gPlayTestSpeechTickets[actor.player?0:size_t(speaker)+1];
        if(state.pendingAudioSample>=0) {
            if(!ticket||ticket->failed||ticket->cancelled)state.pendingAudioSample=-1;
            else if(ticket->loaded&&!gPlayTestMissionEnvironment.vcsAudioServiceSuppressed) {
                if(servicedNativeComment)return;
                servicedNativeComment=true;
                float pitch=1;
                const bool accepted=gPlayTestMissionVm.vcs?
                    nativeSpeechAudio.admit(state.pendingAudioSample,state.pendingAudioEvent,
                        state.pendingAudioFrequency,true,false,gPlayTestVcsAudioRandom,pitch):
                    nativeLcsSpeechAudio.admit(state.pendingAudioSample,state.pendingAudioEvent,
                        state.pendingAudioFrequency,true,false,gPlayTestLcsAudioRandom,pitch);
                state.pendingAudioSample=-1;
                if(accepted) {ticket->pitch=pitch;ticket->requested=true;}
                else ticket->cancelled=true;
            }
            return;
        }
        StorylandCharacterSpeechService context;
        context.player=actor.player;context.burning=burning;
        context.suppressedAnimation=actor.suppressedAnimation;
        const int admitted=gPlayTestMissionVm.vcs?
            storylandServiceVcsCharacterSpeech(state.queue,gPlayTestCharacterSpeechGlobal,
                gPlayTestMissionEnvironment.gameMilliseconds,context,
                []{return gPlayTestCharacterSpeechGlobal.nextRandom();}):
            storylandServiceLcsPlayerSpeech(state.lcsQueue,gPlayTestCharacterSpeechGlobal,
                gPlayTestMissionEnvironment.gameMilliseconds,context,
                []{return gPlayTestCharacterSpeechGlobal.nextRandom();});
        if(admitted<0)return;
        if(!actor.enabled)return;
        const auto* range=gPlayTestMissionVm.vcs?storylandCharacterSpeechRange(actor,uint16_t(admitted)):
            storylandLcsPlayerSpeechRange(uint16_t(admitted));
        if(!range||!range->count)return;
        if(gPlayTestMissionVm.vcs&&actor.player&&!state.playerGate(uint16_t(admitted)))return;
        const float gain=actor.player?1.0f:std::clamp(1-std::hypot(actor.position.x-gPlayerWorldX,
            actor.position.y-gPlayerWorldY,actor.position.z-gPlayerWorldZ)/50.0f,0.0f,1.0f);
        if(gain<.02f)return;
        const int sample=state.choose(*range,gPlayTestCharacterSpeechGlobal.nextRandom());
        if(sample<0)return;
        const auto* nativeSample=gPlayTestMissionVm.vcs?
            gPlayTestNativeAudio.vcsSample(2,size_t(sample),gDtzArchive.isPsp()):
            gPlayTestNativeAudio.lcsSample(uint32_t(sample));
        const bool blocked=gPlayTestMissionVm.vcs?nativeSpeechAudio.blocked(sample,uint16_t(admitted)):
            nativeLcsSpeechAudio.blocked(sample,uint16_t(admitted));
        if(!nativeSample||!nativeSample->frequency||blocked)return;
        if(ticket)ticket->cancelled=true;
        ticket=gPlayTestGameAudio.submit(nativeSample->bankPath,nativeSample->slot,true,
            gain*gStorylandSfxVolume,false,false,actor.player);
        state.pendingAudioSample=sample;state.pendingAudioEvent=uint16_t(admitted);
        state.pendingAudioFrequency=nativeSample->frequency;
    };
    service(-1);
    for(size_t index=0;index<gPlayTestRuntime.pedestrians.size();++index)service(int(index));
    for(const auto& entry:gPlayTestMissionEnvironment.missionCharacters)
        service(int(gPlayTestRuntime.pedestrians.size())+entry.first);
}

static void updateStorylandSpeechConversations() {
    if(!gPlayTestMissionVm.vcs||!gPlayTestSpeechBank.empty())ensureStorylandCharacterSpeechStorage();
    const float now=gPlayTestRuntime.elapsed;
    const bool paused=gPlayTestRuntime.paused||gPlayTestRuntime.pauseMenu||gPlayTestDebugMenuOpen;
    for(auto& ticket:gPlayTestSpeechTickets)if(ticket) {
        ticket->paused=paused;
        if(ticket->failed)gPlayTestSoundStatus="Pedestrian/player speech: "+ticket->diagnostic;
    }
    for(auto& entry:gPlayTestMissionSpeechTickets)if(entry.second) {
        entry.second->paused=paused;
        if(entry.second->failed)gPlayTestSoundStatus="Mission pedestrian speech: "+entry.second->diagnostic;
    }
    if(!gPlayTestSpeechTickets.empty()&&gPlayTestSpeechTickets[0])
        gPlayTestSpeechTickets[0]->gain=gStorylandSfxVolume;
    const auto calm=[&](int index) {
        if(index<0||size_t(index)>=gNativePedestrians.size()||size_t(index)>=gPlayTestRuntime.pedestrians.size())return false;
        const auto& ped=gPlayTestRuntime.pedestrians[size_t(index)];
        return ped.health>0&&!ped.hostile&&!ped.water.active&&
            (ped.state==StorylandPedState::Walk||ped.state==StorylandPedState::Idle);
    };
    const auto separation=[](const auto& a,const auto& b) {return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);};
    const auto finish=[&]() {
        gPlayTestConversationA=gPlayTestConversationB=gPlayTestConversationSpeaker=-1;
        gPlayTestNextConversation=now+15;
    };
    for(size_t index=1;index<gPlayTestSpeechTickets.size();++index) {
        if(!gPlayTestSpeechTickets[index]||gPlayTestSpeechTickets[index]->finished||index-1>=gNativePedestrians.size())continue;
        const auto& position=gNativePedestrians[index-1].position;
        gPlayTestSpeechTickets[index]->gain=std::clamp(1-separation(position,StorylandNativePoint{gPlayerWorldX,gPlayerWorldY,gPlayerWorldZ})/50.0f,0.0f,1.0f)*gStorylandSfxVolume;
    }
    for(auto it=gPlayTestMissionSpeechTickets.begin();it!=gPlayTestMissionSpeechTickets.end();) {
        const auto actor=gPlayTestMissionEnvironment.missionCharacters.find(it->first);
        if(actor==gPlayTestMissionEnvironment.missionCharacters.end()) {
            if(it->second)it->second->cancelled=true;
            gPlayTestMissionSpeechStates.erase(it->first);
            it=gPlayTestMissionSpeechTickets.erase(it);
            continue;
        }
        if(it->second&&!it->second->finished) {
            const auto& character=actor->second;
            it->second->gain=std::clamp(1-std::hypot(character.x-gPlayerWorldX,
                character.y-gPlayerWorldY,character.z-gPlayerWorldZ)/50.0f,0.0f,1.0f)*gStorylandSfxVolume;
        }
        ++it;
    }
    if(!gPlayTestMissionVm.vcs||paused||gPlayTestCutsceneActive||
        gPlayTestMissionEnvironment.cutscene.playing||gPlayTestSpeechBank.empty())return;
    if(gPlayTestConversationA>=0) {
        const int a=gPlayTestConversationA,b=gPlayTestConversationB;
        if(!calm(a)||!calm(b)||now>gPlayTestConversationDeadline||
            separation(gNativePedestrians[size_t(a)].position,gNativePedestrians[size_t(b)].position)>5) {finish();return;}
        for(int index:{a,b}) {
            auto& ped=gPlayTestRuntime.pedestrians[size_t(index)];ped.state=StorylandPedState::Idle;ped.timer=.4f;
            auto& actor=gNativePedestrians[size_t(index)];
            const auto& other=gNativePedestrians[size_t(index==a?b:a)].position;
            actor.yaw=std::atan2(other.y-actor.position.y,other.x-actor.position.x)*57.2957795131f;
        }
        const auto& ticket=gPlayTestSpeechTickets[size_t(gPlayTestConversationSpeaker)+1];
        if(ticket&&!ticket->finished)return;
        if(gPlayTestConversationTurn>=3) {finish();return;}
        const int reply=gPlayTestConversationSpeaker==a?b:a;
        if(playStorylandNativeSpeech(reply,uint16_t(StorylandSpeechEvent::Chat))) {
            gPlayTestConversationSpeaker=reply;++gPlayTestConversationTurn;
        }
        return;
    }
    if(now<gPlayTestNextConversation)return;
    gPlayTestNextConversation=now+2;
    for(size_t a=0;a<gNativePedestrians.size();++a)for(size_t b=a+1;b<gNativePedestrians.size();++b) {
        if(!calm(int(a))||!calm(int(b)))continue;
        const auto& first=gNativePedestrians[a].position;
        const auto& second=gNativePedestrians[b].position;
        const float gap=separation(first,second);
        if(gap<1||gap>3.5f||separation(first,StorylandNativePoint{gPlayerWorldX,gPlayerWorldY,gPlayerWorldZ})>40)continue;
        const auto* reply=storylandVcsSpeechRange(uint16_t(gPlayTestRuntime.pedestrians[b].stats.modelId),uint16_t(StorylandSpeechEvent::Chat),unsigned(b)%5);
        if(!reply||!reply->count)continue;
        auto origin=first,target=second;origin.z+=1.3f;target.z+=1.3f;
        if(storylandTraceNativeSegment(gDtzArchive.nativeWorld(),origin,target)<.995f)continue;
        if(!playStorylandNativeSpeech(int(a),uint16_t(StorylandSpeechEvent::Chat)))continue;
        gPlayTestConversationA=int(a);gPlayTestConversationB=int(b);gPlayTestConversationSpeaker=int(a);
        gPlayTestConversationTurn=1;gPlayTestConversationDeadline=now+15;return;
    }
}
