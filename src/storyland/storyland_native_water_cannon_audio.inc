static void updateStorylandWaterCannonAudio() {
    static std::array<std::wstring,3> selectedBanks{};
    static std::array<size_t,3> selectedSlots{};
    const auto* sample=gPlayTestActive&&gPlayTestMissionVm.vcs?
        storylandVcsWaterCannonSample(gPlayTestNativeAudio,gDtzArchive.isPsp()):nullptr;
    const bool paused=gPlayTestRuntime.paused||gPlayTestRuntime.pauseMenu||gPlayTestDebugMenuOpen;
    for(size_t index=0;index<gPlayTestWaterCannonTickets.size();++index) {
        const auto& cannon=gPlayTestRuntime.waterCannons[index];
        const auto& position=cannon.samples[cannon.current].position;
        const float gain=cannon.ownerSlot<0||!sample||!sample->frequency||!gPlayTestRuntime.sounds?0.0f:
            storylandVcsWaterCannonGain(storylandPlayTestAudioDistance({position[0],position[1],position[2]}))*gStorylandSfxVolume;
        auto& ticket=gPlayTestWaterCannonTickets[index];
        if(ticket&&sample&&(selectedBanks[index]!=sample->bankPath||selectedSlots[index]!=sample->slot)) {
            ticket->cancelled=true;ticket.reset();
        }
        if(gain<=0) {
            if(ticket)ticket->cancelled=true;
            ticket.reset();
            continue;
        }
        if(!ticket||ticket->finished) {
            ticket=gPlayTestGameAudio.submit(sample->bankPath,sample->slot,true,gain,true,true);
            if(ticket) {
                selectedBanks[index]=sample->bankPath;selectedSlots[index]=sample->slot;
                ticket->pitch=storylandVcsWaterCannonPitch(sample->frequency);
            }
        }
        if(ticket) {ticket->gain=gain;ticket->paused=paused;}
    }
}

