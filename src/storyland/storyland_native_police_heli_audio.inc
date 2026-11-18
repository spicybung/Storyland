static void updateStorylandPoliceHeliAudio() {
    const auto* sample=gPlayTestActive&&gPlayTestMissionVm.vcs?
        storylandVcsPoliceHeliDistantSample(gPlayTestNativeAudio,gDtzArchive.isPsp()):nullptr;
    const bool paused=gPlayTestRuntime.paused||gPlayTestRuntime.pauseMenu||gPlayTestDebugMenuOpen;
    std::set<int32_t> live;
    for(size_t slot=0;slot<gNativeVehicles.size()&&slot<gPlayTestRuntime.vehicles.size();++slot) {
        const auto& vehicle=gPlayTestRuntime.vehicles[slot];
        if(!sample||!sample->frequency||!gPlayTestRuntime.sounds||!vehicle.nativeDefinition.vcsModelLayout||
            vehicle.nativeDefinition.modelId!=261||vehicle.nativeDefinition.nativeType!=4)continue;
        const float gain=storylandVcsPoliceHeliDistantGain(vehicle.rotorSpeed,
            storylandPlayTestAudioDistance(gNativeVehicles[slot].position))*gStorylandSfxVolume;
        if(gain<=0)continue;
        const int32_t key=int32_t(slot);
        auto& voice=gPlayTestPoliceHeliVoices[key];
        if(voice.ticket&&(voice.vehicleHandle!=vehicle.scriptHandle||voice.bankPath!=sample->bankPath||
            voice.sampleSlot!=sample->slot)) {voice.ticket->cancelled=true;voice.ticket.reset();}
        if(!voice.ticket||voice.ticket->finished) {
            voice.bankPath=sample->bankPath;voice.sampleSlot=sample->slot;voice.vehicleHandle=vehicle.scriptHandle;
            voice.ticket=gPlayTestGameAudio.submit(sample->bankPath,sample->slot,true,gain,true,true);
        }
        if(voice.ticket) {voice.ticket->gain=gain;voice.ticket->paused=paused;}
        live.insert(key);
    }
    for(auto it=gPlayTestPoliceHeliVoices.begin();it!=gPlayTestPoliceHeliVoices.end();) {
        if(live.count(it->first)) {++it;continue;}
        if(it->second.ticket)it->second.ticket->cancelled=true;
        it=gPlayTestPoliceHeliVoices.erase(it);
    }
}
