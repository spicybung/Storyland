static StorylandNativePoint storylandPlayTestAudioListener() {
    if(gPlayTestMissionVm.vcs) {
        if(gPlayTestCamera.initialized)return gPlayTestCamera.eye;
        const auto& camera=gPlayTestMissionEnvironment.camera;
        if(camera.active&&camera.positionSet)return {camera.x,camera.y,camera.z};
    }
    return {gPlayerWorldX,gPlayerWorldY,gPlayerWorldZ};
}

static float storylandPlayTestAudioDistance(const StorylandNativePoint& point) {
    const auto listener=storylandPlayTestAudioListener();
    const float x=point.x-listener.x,y=point.y-listener.y,z=point.z-listener.z;
    return gPlayTestMissionVm.vcs?storylandVcsAudioDistance(x,y,z):std::hypot(x,y,z);
}

static void updateStorylandPlayTestScriptMusic() {
    auto& voice=gPlayTestScriptMusicVoice;
    const auto selection=!gPlayTestActive?StorylandScriptMusicSelection{}:gPlayTestMissionVm.vcs?
        storylandSelectScriptMusic(gPlayTestMissionEnvironment.musicEmitters,
            {gPlayerWorldX,gPlayerWorldY,gPlayerWorldZ}):
        storylandLcsRiotMusic(gPlayTestMissionEnvironment.lcsExtendedWorld.riotIntensity,
            {gPlayTestCamera.eye.x,gPlayTestCamera.eye.y,gPlayTestCamera.eye.z});
    if(selection.stream!=gPlayTestScriptMusicStream) {
        voice.load.reset();voice.player->close();voice.path.clear();voice.attempted=false;
        gPlayTestScriptMusicStream=selection.stream;
    }
    if(selection.stream<0)return;
    const bool audible=gPlayTestRuntime.sounds&&!gPlayTestRuntime.paused&&
        !gPlayTestRuntime.pauseMenu&&!gPlayTestDebugMenuOpen&&!gPlayTestCutsceneActive&&
        !gPlayTestMissionEnvironment.cutscene.playing&&!gPlayTestMissionPassedTune;
    voice.gain=audible?selection.gain*storylandPlayTestMusicFadeGain():0;
    if(!voice.attempted) {
        voice.attempted=true;
        const char* stem=gPlayTestMissionVm.vcs?storylandVcsMusicStreamStem(selection.stream):
            storylandLcsMusicStreamStem(selection.stream);
        if(stem)for(const auto& asset:gPlayTestRetailAssets.files) {
            auto name=std::filesystem::path(asset.path).stem().string();
            std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
            if(name!=stem)continue;
            if(voice.path.empty()||std::filesystem::path(asset.path).extension()==L".VB"||
                std::filesystem::path(asset.path).extension()==L".vb")voice.path=asset.path;
        }
        if(voice.path.empty()) {
            gPlayTestSoundStatus="Script music stream unavailable: "+
                (stem?std::string(stem):std::to_string(selection.stream));
        } else {
            auto job=std::make_shared<StorylandRadioLoad>();voice.load=job;
            const auto path=voice.path;
            const int32_t position=gPlayTestMissionVm.vcs&&selection.stream>=0&&
                size_t(selection.stream)<gPlayTestMissionEnvironment.scriptMusicStartPositions.size()?
                gPlayTestMissionEnvironment.scriptMusicStartPositions[size_t(selection.stream)]:0;
            const double startSeconds=storylandRadioScriptPositionSeconds(position,gPlayTestMissionVm.vcs);
            std::thread([job,path,startSeconds]() {
                const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
                std::unique_ptr<StorylandMediaFile> player;std::string error;bool loaded=false;
                try {
                    player=std::make_unique<StorylandMediaFile>();
                    loaded=player->loadRadioStream(path,error,startSeconds)&&!player->clips().empty();
                } catch(const std::exception& exception) {error=exception.what();}
                catch(...) {error="Script music decoder failed.";}
                {
                    std::lock_guard<std::mutex> lock(job->mutex);
                    if(loaded)job->player=std::move(player);
                    job->error=error;job->ready=true;
                }
                if(SUCCEEDED(com))CoUninitialize();
            }).detach();
        }
    }
    if(voice.load) {
        auto job=voice.load;std::lock_guard<std::mutex> lock(job->mutex);
        if(job->ready) {
            if(job->player) {
                voice.player=std::move(job->player);voice.player->setPlaybackGain(voice.gain);
                std::string error;if(!voice.player->play(error))gPlayTestSoundStatus="Script music: "+error;
            } else gPlayTestSoundStatus="Script music: "+job->error;
            voice.load.reset();
        }
    }
    voice.player->setPlaybackGain(voice.gain);
    voice.player->setPlaybackPaused(gPlayTestRuntime.paused||gPlayTestRuntime.pauseMenu||gPlayTestDebugMenuOpen);
    const auto error=voice.player->radioPlaybackError();
    if(!error.empty())gPlayTestSoundStatus="Script music: "+error;
}

static void updateStorylandPlayTestAmbience(float seconds) {
    if (!gPlayTestActive) {
        for (auto& voice : gPlayTestAmbience) {
            voice.load.reset();
            voice.player->close();
            voice.attempted = false;
            voice.gain = 0;
        }
        return;
    }
    gPlayTestAmbientProbe -= seconds;
    if (gPlayTestAmbientProbe <= 0) {
        gPlayTestAmbientProbe = 0.5f;
        gPlayTestWaterMix = 0;
        const auto& world = gDtzArchive.nativeWorld();
        const StorylandNativePoint listener{gPlayerWorldX, gPlayerWorldY, gPlayerWorldZ + 1};
        const StorylandNativePoint ceiling{listener.x, listener.y, listener.z + 10};
        gPlayTestExteriorMix = storylandTraceNativeSegment(world, listener, ceiling) < 0.999f ? 0.15f : 1.0f;
        for (float radius : {0.0f, 16.0f, 48.0f, 96.0f}) {
            for (int direction = 0; direction < 4; ++direction) {
                const float x = gPlayerWorldX + (direction == 0 ? radius : direction == 1 ? -radius : 0);
                const float y = gPlayerWorldY + (direction == 2 ? radius : direction == 3 ? -radius : 0);
                float height = 0;
                if (!storylandNativeWaterHeight(world, x, y, height)) continue;
                const StorylandNativePoint top{x, y, std::max(gPlayerWorldZ + 3, height + 3)};
                const StorylandNativePoint bottom{x, y, height + 0.15f};
                if (storylandTraceNativeSegment(world, top, bottom) < 0.999f) continue;
                gPlayTestWaterMix = std::max(gPlayTestWaterMix, 1.0f - radius / 120.0f);
            }
        }
    }
    const bool audible = gPlayTestRuntime.sounds && !gPlayTestRuntime.paused &&
        !gPlayTestMissionEnvironment.cutscene.playing && gPlayTestRuntime.visibleArea == 0;
    const float exterior = audible ? gPlayTestExteriorMix * (gPlayTestRuntime.occupiedVehicle >= 0 ? 0.45f : 1.0f) : 0;
    for (size_t index = 0; index < gPlayTestAmbience.size(); ++index) {
        auto& voice = gPlayTestAmbience[index];
        const float target = exterior * (index == 0 ? 0.32f * (1 - gPlayTestWaterMix) : 0.55f * gPlayTestWaterMix);
        voice.gain += (target - voice.gain) * std::clamp(seconds * 2.0f, 0.0f, 1.0f);
        if (!voice.attempted && !voice.path.empty()) {
            voice.attempted = true;
            auto job = std::make_shared<StorylandRadioLoad>();
            voice.load = job;
            const auto path = voice.path;
            std::thread([job, path]() {
                const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                std::string error;
                std::unique_ptr<StorylandMediaFile> player;
                bool loaded=false;
                try {
                    player=std::make_unique<StorylandMediaFile>();
                    loaded=player->loadRadioStream(path,error)&&!player->clips().empty();
                } catch(const std::exception& exception) {
                    error="Ambience decoder failed: "+std::string(exception.what());
                } catch(...) {
                    error="Ambience decoder reported an unexpected failure.";
                }
                {
                    std::lock_guard<std::mutex> lock(job->mutex);
                    if (loaded) job->player = std::move(player);
                    job->error = error;
                    job->ready = true;
                }
                if (SUCCEEDED(com)) CoUninitialize();
            }).detach();
        }
        if (voice.load) {
            auto job = voice.load;
            std::lock_guard<std::mutex> lock(job->mutex);
            if (job->ready) {
                if (job->player) {
                    voice.player = std::move(job->player);
                    voice.player->setPlaybackGain(voice.gain);
                    std::string error;
                    if (!voice.player->play(error)) gPlayTestSoundStatus = "Ambience: " + error;
                } else gPlayTestSoundStatus = "Ambience: " + job->error;
                voice.load.reset();
            }
        }
        voice.player->setPlaybackGain(voice.gain);
        const auto error = voice.player->radioPlaybackError();
        if (!error.empty()) gPlayTestSoundStatus = "Ambience: " + error;
    }
}

static void updateStorylandPlayTestEngines(float seconds) {
    if (!gPlayTestActive || !gPlayTestRuntime.sounds || gPlayTestRuntime.health<=0) {
        for (auto& voice : gPlayTestEngines) {
            for (auto& player : voice.players) {if(player)player->cancelled=true;player.reset();}
            voice.vehicle = -1;
            voice.scriptHandle = -1;
            voice.bobo={};
        }
        return;
    }
    const bool paused=gPlayTestRuntime.paused||gPlayTestRuntime.pauseMenu||gPlayTestDebugMenuOpen||
        gPlayTestMissionEnvironment.cutscene.playing;
    for(auto& voice:gPlayTestEngines)for(auto& player:voice.players)if(player)player->paused=paused;
    if(paused)return;
    std::vector<std::pair<float, int>> candidates;
    const int remoteVehicle=gPlayTestMissionVm.vcs?storylandVcsRemoteVehicleSlot(gPlayTestRuntime):-1;
    for (size_t index = 0; index < gNativeVehicles.size() && index < gPlayTestRuntime.vehicles.size(); ++index) {
        const auto& vehicle = gPlayTestRuntime.vehicles[index];
        if (vehicle.model < gPlayTestRuntime.modelDefinitions.size() &&
            gPlayTestRuntime.modelDefinitions[vehicle.model].bicycle()) continue;
        if (vehicle.health <= 0 || (!vehicle.engineOn && !vehicle.siren)) continue;
        if (gPlayTestMissionVm.vcs) {
            if (!vehicle.hasDriver && !vehicle.missionDriver && !vehicle.siren &&
                int(index) != gPlayTestRuntime.occupiedVehicle && int(index)!=remoteVehicle) continue;
        } else if (!vehicle.siren&&(!vehicle.engineOn || (!vehicle.hasDriver && !vehicle.missionDriver &&
            vehicle.autoPilotMission != 14 && int(index) != gPlayTestRuntime.occupiedVehicle))) continue;
        const auto& point = gNativeVehicles[index].position;
        const float distance = storylandPlayTestAudioDistance(point);
        const bool retained=std::any_of(gPlayTestEngines.begin(),gPlayTestEngines.end(),
            [&](const auto& voice){return voice.vehicle==int(index)&&voice.model==vehicle.model;});
        // Keep a playing voice across small distance changes and the audible
        // boundary. A noticeably closer car can still take its place.
        const float range=vehicle.siren?(gPlayTestMissionVm.vcs?75.0f:110.0f):
            std::max(StorylandRetailSkidRange,StorylandRetailEngineRange);
        if (int(index)==gPlayTestRuntime.occupiedVehicle||distance<range+(retained?5.0f:0.0f))
            candidates.emplace_back(int(index)==gPlayTestRuntime.occupiedVehicle?-1000.0f:
                distance-(retained?8.0f:0.0f),int(index));
    }
    std::sort(candidates.begin(), candidates.end());
    if (candidates.size() > gPlayTestEngines.size()) candidates.resize(gPlayTestEngines.size());
    for (auto& voice : gPlayTestEngines) {
        const auto found = std::find_if(candidates.begin(), candidates.end(), [&](const auto& item) { return item.second == voice.vehicle; });
        if (found == candidates.end()) {
            for (auto& player : voice.players) {if(player)player->cancelled=true;player.reset();}
            voice.vehicle = -1;
            voice.scriptHandle = -1;
            voice.bobo={};
        }
    }
    for (const auto& candidate : candidates) {
        auto found = std::find_if(gPlayTestEngines.begin(), gPlayTestEngines.end(), [&](const auto& voice) { return voice.vehicle == candidate.second; });
        if (found == gPlayTestEngines.end()) {
            found = std::find_if(gPlayTestEngines.begin(), gPlayTestEngines.end(), [](const auto& voice) { return voice.vehicle < 0; });
        }
        if (found == gPlayTestEngines.end()) continue;
        auto& voice = *found;
        const auto& vehicle = gPlayTestRuntime.vehicles[size_t(candidate.second)];
        if ((voice.model != vehicle.model || voice.playerVehicle!=(candidate.second==gPlayTestRuntime.occupiedVehicle) ||
            voice.scriptHandle!=vehicle.scriptHandle) && voice.vehicle >= 0) {
            for (auto& player : voice.players) {if(player)player->cancelled=true;player.reset();}
            voice.vehicle = -1;
        }
        if (voice.vehicle < 0) {
            voice.vehicle = candidate.second;
            voice.scriptHandle = vehicle.scriptHandle;
            voice.model = vehicle.model;
            voice.playerVehicle=candidate.second==gPlayTestRuntime.occupiedVehicle;
            voice.accelerationGear=UINT32_MAX;
            voice.gain=voice.rev=0;
            voice.retryAt=0;
            voice.bobo={};
        }
        const auto* nativeAudio=vehicle.model<gPlayTestRuntime.modelDefinitions.size()?
            &gPlayTestRuntime.modelDefinitions[vehicle.model]:nullptr;
        const bool vcsAudio=nativeAudio&&nativeAudio->vcsModelLayout&&nativeAudio->hasNativeAudio;
        const bool bobo=vcsAudio&&nativeAudio->modelId==208&&!voice.playerVehicle;
        // A full decode queue or a temporary device failure must not leave a
        // retained vehicle permanently silent. Keep healthy voices playing.
        if(!bobo&&gPlayTestRuntime.elapsed>=voice.retryAt&&
            (!voice.players[0]||voice.players[0]->finished||
             !voice.players[1]||voice.players[1]->finished)) {
            voice.retryAt=gPlayTestRuntime.elapsed+1.0f;
            const bool motorcycle = vehicle.model < gPlayTestRuntime.modelDefinitions.size() &&
                gPlayTestRuntime.modelDefinitions[vehicle.model].motorcycle();
            const bool hasDefinition=vehicle.model<gPlayTestRuntime.modelDefinitions.size();
            const auto pair=hasDefinition?
                gPlayTestNativeAudio.enginePair(gPlayTestRuntime.modelDefinitions[vehicle.model],
                    gNativeVehicles[size_t(candidate.second)].water,voice.playerVehicle):
                gPlayTestNativeAudio.enginePair(gNativeVehicles[size_t(candidate.second)].water,motorcycle);
            for (size_t layer = 0; layer < 2; ++layer) {
                const auto* sample = pair[layer];
                if (!sample) continue;
                if(voice.players[layer]&&!voice.players[layer]->finished)continue;
                if(voice.players[layer]&&voice.players[layer]->failed)
                    gPlayTestSoundStatus="Vehicle audio "+sample->label+": "+voice.players[layer]->diagnostic;
                voice.players[layer]=gPlayTestGameAudio.submit(sample->bankPath,sample->slot,true,0,true,true);
            }
        }
        const float targetRev=vehicle.nativeHandling?std::clamp(vehicle.transmission.rpm+
            std::fabs(vehicle.throttle)*0.12f,0.0f,1.0f):
            std::clamp(std::fabs(vehicle.speed)/std::max(1.0f,vehicle.handlingTopSpeed)+
                std::fabs(vehicle.throttle)*0.35f,0.0f,1.0f);
        const auto& position=gNativeVehicles[size_t(candidate.second)].position;
        const float distance=candidate.second==gPlayTestRuntime.occupiedVehicle?0.0f:
            storylandPlayTestAudioDistance(position);
        const float targetGain=.32f*storylandRetailSoundVolume(127,StorylandRetailEngineRange,distance)/127.0f;
        const float effectGain=.32f*std::clamp(1-distance/75.0f,0.0f,1.0f);
        const float step=std::isfinite(seconds)?std::clamp(seconds,0.0f,.1f):0.0f;
        voice.rev+=(targetRev-voice.rev)*(1-std::exp(-step*12));
        voice.gain+=(targetGain-voice.gain)*(1-std::exp(-step*10));
        const float rev=voice.rev,gain=voice.gain;
        const bool idleAvailable=voice.players[0]&&!voice.players[0]->failed;
        const bool revAvailable=voice.players[1]&&!voice.players[1]->failed;
        if(bobo) {
            if(voice.players[0])voice.players[0]->cancelled=true;
            voice.players[0].reset();
            const bool wheelsOnGround=vehicle.contactsValid?
                std::any_of(vehicle.wheelContacts.begin(),vehicle.wheelContacts.end(),[](bool value){return value;}):
                vehicle.grounded;
            const auto sound=storylandVcsBoboMotorSound(voice.bobo,vehicle.speed/50.0f,
                nativeAudio->maximumTransmissionVelocity,vehicle.throttle,wheelsOnGround,
                uint32_t(candidate.second),distance);
            auto& motor=voice.players[1];
            if(!sound.volume) {
                if(motor)motor->cancelled=true;
                motor.reset();
            } else if(const auto* sample=gPlayTestNativeAudio.vcsSample(0,74)) {
                if(motor&&(motor->finished||motor->failed))motor.reset();
                if(!motor)motor=gPlayTestGameAudio.submit(sample->bankPath,sample->slot,true,0,true,true);
                if(motor) {
                    motor->gain=float(sound.volume)/127.0f*gStorylandSfxVolume;
                    if(sample->frequency)motor->pitch=float(sound.frequency)/float(sample->frequency);
                }
            }
        } else {
            if(idleAvailable) {
                voice.players[0]->gain=gain*(revAvailable?std::sqrt(1-rev):1.0f);
                voice.players[0]->pitch=.9f+rev*.3f;
            }
            if(revAvailable) {
                voice.players[1]->gain=gain*(idleAvailable?std::sqrt(rev):1.0f);
                voice.players[1]->pitch=.75f+rev*.8f;
            }
        }
        auto& siren=voice.players[5];
        if(!vehicle.siren||!nativeAudio) {
            if(siren)siren->cancelled=true;
            siren.reset();
        } else {
            if(!siren||siren->finished) {
                if(const auto* sample=gPlayTestNativeAudio.vehicleSirenSample(*nativeAudio,gDtzArchive.isPsp())) {
                    siren=gPlayTestGameAudio.submit(sample->bankPath,sample->slot,true,0,true,true);
                    const auto frequency=vcsAudio?nativeAudio->sirenFrequency:
                        storylandLcsVehicleSirenAudio(nativeAudio->modelId).frequency;
                    if(siren&&sample->frequency&&frequency)siren->pitch=float(frequency)/float(sample->frequency);
                }
            }
            if(siren)siren->gain=vcsAudio?std::min(1.0f,effectGain*2.0f):
                storylandRetailSoundVolume(80,110.0f,distance)/127.0f;
        }
        const auto* audioProfile=!gPlayTestRuntime.crime.vcs&&vehicle.model<gPlayTestRuntime.modelDefinitions.size()?
            storylandLcsVehicleAudio(gPlayTestRuntime.modelDefinitions[vehicle.model].modelId):nullptr;
        const bool playerVehicle=candidate.second==gPlayTestRuntime.occupiedVehicle;
        auto& acceleration=voice.players[4];
        const bool accelerating=vcsAudio&&playerVehicle&&nativeAudio->audioBank>=4&&
            nativeAudio->audioBank<=27&&storylandVcsBankSampleCount(nativeAudio->audioBank)>0&&!gNativeVehicles[size_t(candidate.second)].water&&
            vehicle.throttle>0.15f&&vehicle.speed>0.4f&&!vehicle.handbrake&&vehicle.grounded;
        if(!accelerating) {
            if(acceleration)acceleration->cancelled=true;
            acceleration.reset();voice.accelerationGear=UINT32_MAX;
        } else if(voice.accelerationGear!=vehicle.transmission.gear) {
            if(acceleration)acceleration->cancelled=true;
            acceleration.reset();voice.accelerationGear=vehicle.transmission.gear;
            // 0x1BA0F4 selects the first sample of the model's dedicated bank.
            if(const auto* sample=gPlayTestNativeAudio.vcsSample(nativeAudio->audioBank,0))
                acceleration=gPlayTestGameAudio.submit(sample->bankPath,sample->slot,true,0,true,false);
        }
        if(acceleration&&!acceleration->failed&&!acceleration->finished) {
            acceleration->gain=gain;acceleration->pitch=0.9f+rev*0.3f;
            if(acceleration->started&&revAvailable)voice.players[1]->gain=0;
        }
        const bool horn=playerVehicle&&gPlayTestRuntime.playerControl&&gPlayTestHornHeld;
        const bool water=gNativeVehicles[size_t(candidate.second)].water;
        const float slip=water||!vehicle.skidRoadContact?0.0f:vehicle.tyreSlip;
        for(size_t layer=2;layer<4;++layer) {
            const float level=layer==2?(horn?std::min(1.0f,effectGain*2.0f):0.0f):
                storylandRetailSkidVolume(slip,distance)/127.0f;
            auto& ticket=voice.players[layer];
            if(level<=0) {
                if(ticket)ticket->cancelled=true;
                ticket.reset();continue;
            }
            if(ticket&&(ticket->failed||ticket->finished))ticket.reset();
            const auto* sample=vcsAudio?(layer==2?
                gPlayTestNativeAudio.vcsVehicleSample(nativeAudio->hornSample,gDtzArchive.isPsp()):
                gPlayTestNativeAudio.vcsSample(0,260)):layer==2&&audioProfile?
                gPlayTestNativeAudio.namedVehicleSample(audioProfile->horn):gPlayTestNativeAudio.vehicleNoise(layer==2);
            if(!ticket) {
                if(sample) {
                    ticket=gPlayTestGameAudio.submit(sample->bankPath,sample->slot,true,0,true,true);
                    if(ticket&&layer==2&&sample->frequency) {
                        if(vcsAudio)ticket->pitch=float(nativeAudio->hornFrequency)/float(sample->frequency);
                        else if(audioProfile)ticket->pitch=float(audioProfile->hornFrequency)/float(sample->frequency);
                    }
                }
                else if(!vcsAudio) {
                    const auto named=gPlayTestNamedSounds.find(layer==2?"car_horn":"tyre_skid");
                    if(named!=gPlayTestNamedSounds.end())
                        ticket=gPlayTestGameAudio.submit(named->second,0,false,0,true,true);
                }
            }
            if(ticket) {
                ticket->gain=level;
                if(layer==3&&sample&&sample->frequency)ticket->pitch=storylandRetailSkidFrequency(slip)/sample->frequency;
            }
        }
    }
}
static bool queueStorylandLcsPoliceReport(const std::array<float,3>& position,std::string& error) {
    const auto& world=gPlayTestMissionEnvironment;
    return gPlayTestPoliceRadioQueue.enqueue(gDtzArchive.nativeWorld().zones,
        {position[0],position[1],position[2]},world.audioSubsystemEnabled,world.scriptAudioMode,
        [](uint32_t sample) {return gPlayTestNativeAudio.lcsPoliceSample(sample)!=nullptr;},error);
}

static void updateStorylandLcsPoliceRadio() {
    auto& world=gPlayTestMissionEnvironment;
    if(!gPlayTestActive||gPlayTestMissionVm.vcs||!world.audioSubsystemEnabled) {
        if(gPlayTestPoliceRadioTicket)gPlayTestPoliceRadioTicket->cancelled=true;
        gPlayTestPoliceRadioTicket.reset();gPlayTestPoliceRadioQueue.clear();return;
    }
    const bool paused=gPlayTestRuntime.paused||gPlayTestRuntime.pauseMenu||gPlayTestDebugMenuOpen||
        world.scriptAudioMode==2;
    const float gain=gPlayTestRuntime.sounds?gStorylandSfxVolume*(100.0f/127.0f):0.0f;
    if(gPlayTestPoliceRadioTicket) {
        auto& ticket=*gPlayTestPoliceRadioTicket;
        ticket.gain=gain;ticket.paused=paused;
        if(!ticket.finished&&!ticket.failed)return;
        if(ticket.failed)gPlayTestSoundStatus="Police radio: "+ticket.diagnostic;
        gPlayTestPoliceRadioTicket.reset();
    }
    if(paused)return;
    uint32_t sampleId=0;
    if(!gPlayTestPoliceRadioQueue.take(world.nativeFrameCounter,sampleId))return;
    const auto* sample=gPlayTestNativeAudio.lcsPoliceSample(sampleId);
    if(!sample) {
        gPlayTestSoundStatus="The queued LCS police-radio sample is no longer available.";
        gPlayTestPoliceRadioQueue.clear();return;
    }
    auto ticket=gPlayTestGameAudio.submit(sample->bankPath,sample->slot,true,gain,false);
    const uint32_t frequency=sampleId==uint32_t(StorylandLcsPoliceRadioSample::MessageNoise)?
        10025u+world.nextNativeRandom()%2000u:22050u;
    ticket->pitch=sample->frequency?float(frequency)/float(sample->frequency):1.0f;
    ticket->requested=true;
    gPlayTestPoliceRadioTicket=std::move(ticket);
}

