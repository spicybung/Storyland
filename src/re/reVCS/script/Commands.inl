bool readVcsMissionText(StorylandMissionVm& vm,std::string& text) {
    Operand source;
    return readStringOperand(vm,source,text);
}

int executeVcsRestoredWorldCommand(StorylandMissionVm& vm,uint16_t command,
    bool inverted,std::string& error) {
    if(!vm.vcs)return 0;
    if(command==0x053F) {
        Operand character;
        if(!readOperand(vm,character)) {
            error="Native character audio command has a truncated operand.";
            return -1;
        }
        // PS2 retail367288 calls2FADF0, whose callee1C20E0 immediately
        // returns. It neither looks up a ped nor requests an audio sample.
        return 1;
    }
    int count=-1;
    switch(command) {
    case 0x0282:case 0x0283:case 0x02CE:case 0x036F:case 0x02FD:case 0x054F:count=0;break;
    case 0x01A5:case 0x026B:case 0x02F0:case 0x03C9:case 0x03CD:case 0x0488:count=1;break;
    case 0x0287:case 0x0303:case 0x0330:count=2;break;
    case 0x04D7:count=3;break;
    case 0x04D5:count=5;break;
    case 0x022F:count=7;break;
    case 0x04ED:count=8;break;
    case 0x052D:count=9;break;
    case 0x00B1:case 0x00B2:case 0x0428:count=0;break;
    case 0x04C7:case 0x04C8:case 0x04C9:count=0;break;
    case 0x04CA:count=2;break;
    default:return 0;
    }
    if(!vm.environment) {error="World command requires a live mission environment.";return -1;}
    auto& world=*vm.environment;
    if(command==0x00B1||command==0x00B2||command==0x0428) {
        Operand zoneOperand,period;
        std::string name;
        if(!readStringOperand(vm,zoneOperand,name)||name.empty()||!readOperand(vm,period)) {
            error="Zone population command requires a zone name and time selector.";return -1;
        }
        const int time=uint8_t(period.bits)!=0?1:0;
        const StorylandNativeZone* native=nullptr;
        if(world.zones)for(const auto& zone:world.zones->information)
            if(zone.name==name) {native=&zone;break;}
        if(!native||!native->populationLoaded) {
            error="Zone population command requires matching native information-zone data.";return -1;
        }
        auto& table=command==0x00B1?world.zoneCarInformation:world.zonePedInformation;
        const auto key=std::make_pair(name,time);
        auto found=table.find(key);
        StorylandMissionEnvironment::ZonePedInformation population;
        population.dayNight=time;
        if(found!=table.end())population=found->second;
        else if(command==0x00B1) {
            const auto& record=native->populationRecords[size_t(time)];
            population.values[0]=record.carDensity;
            population.values[10]=record.policeCarThreshold;
            int32_t previous=record.policeCarThreshold;
            for(size_t gang=0;gang<record.gangCarThresholds.size();++gang) {
                const int32_t threshold=record.gangCarThresholds[gang];
                population.values[gang+1]=threshold-previous;previous=threshold;
            }
        }
        else {
            const auto& record=native->populationRecords[size_t(time)];
            population.values[0]=record.pedestrianDensity;
            population.values[10]=record.policePedestrianThreshold;
            int32_t previous=record.policePedestrianThreshold;
            for(size_t gang=0;gang<record.gangPedestrianThresholds.size();++gang) {
                const int32_t threshold=record.gangPedestrianThresholds[gang];
                population.values[gang+1]=threshold-previous;previous=threshold;
            }
        }
        if(command==0x0428) {
            std::array<Operand,11> outputs{};
            for(auto& output:outputs)if(!readOperand(vm,output)||!output.reference) {
                error="GET_ZONE_PED_INFO requires eleven writable outputs.";return -1;
            }
            for(size_t index=0;index<outputs.size();++index)
                writeOperand(vm,outputs[index],population.values[index]);
        } else {
            Operand density;
            if(!readOperand(vm,density)) {error="Zone density operand is truncated.";return -1;}
            if(found==table.end()&&table.size()>=4096) {error="Zone population table is full.";return -1;}
            population.values[0]=uint16_t(density.bits);
            table[key]=population;
        }
        return 1;
    }
    std::array<Operand,9> values{};
    for(int index=0;index<count;++index)if(!readOperand(vm,values[size_t(index)])) {
        error="World command has a truncated typed operand.";return -1;
    }
    const auto real=[&](size_t index){return operandFloat(values[index]);};
    const auto point=[&](size_t index){return std::array<float,3>{real(index),real(index+1),real(index+2)};};
    const auto finite=[&](size_t begin,size_t end) {
        for(size_t index=begin;index<end;++index)if(!std::isfinite(real(index))) {
            error="World coordinates and sizes must be finite.";return false;
        }
        return true;
    };
    if(command==0x04D5) {
        if(!finite(0,3))return -1;
        world.savePosition=point(0);
        world.savePositionExterior=values[3].bits!=0;
        world.saveLevel=values[4].bits;
        world.savePositionValid=true;
        return 1;
    }
    if(command==0x04C7) {StorylandVcsFerrisWheel::restart(world.ferrisWheel);return 1;}
    if(command==0x04C8) {world.ferrisWheel.stopRequested=true;return 1;}
    if(command==0x04C9) {
        updateScmCondition(vm,world.ferrisWheel.state==3,inverted);return 1;
    }
    if(command==0x04CA) {
        if(values[0].bits<0||size_t(values[0].bits)>=world.ferrisWheel.cabins.size()) {
            error="Ferris wheel cabin index must be between zero and eleven.";return -1;
        }
        if(!writeOperand(vm,values[1],StorylandVcsFerrisWheel::cabinHandle(world,size_t(values[0].bits)))) {
            error="Ferris wheel cabin query requires a writable object handle.";return -1;
        }
        return 1;
    }
    if(command==0x0282||command==0x0283||command==0x02CE||command==0x036F) {
        // SLUS215.90 is a US console executable. Region does not follow GXT language.
        const bool result=command==0x0282?world.frenchGame:command==0x0283?world.germanGame:false;
        updateScmCondition(vm,result,inverted);return 1;
    }
    if(command==0x01A5) {
        if(!writeOperand(vm,values[0],world.controllerMode)) {
            error="GET_CONTROLLER_MODE requires a writable output.";return -1;
        }
        return 1;
    }
    if(command==0x03C9||command==0x03CD) {
        // The PS2 handlers at 0x1E7730 and 0x1E78C0 both store zero.
        if(!writeOperand(vm,values[0],0)) {
            error="Native platform query requires a writable output.";return -1;
        }
        return 1;
    }
    if(command==0x02FD||command==0x054F) {
        const bool taken=world.photographTaken;
        world.photographTaken=false;
        if(command==0x02FD)updateScmCondition(vm,taken,inverted);
        return 1;
    }
    if(command==0x026B) {world.generateCarsAroundCamera=values[0].bits!=0;return 1;}
    if(command==0x02F0) {world.securityCamera=values[0].bits!=0;return 1;}
    if(command==0x0287||command==0x0330) {
        if(values[0].bits!=0) {error="This runtime has one local player (handle zero).";return -1;}
        if(command==0x0287)world.freeHealthCare=values[1].bits!=0;
        else world.drunkCameraIntensity=uint8_t(values[1].bits);
        return 1;
    }
    if(command==0x0488) {
        StorylandVcsIntoxication::set(world,values[0].bits);
        return 1;
    }
    if(command==0x022F) {
        if(!finite(0,7))return -1;
        if(world.roadChanges.size()>=4096) {error="Script road change capacity is full.";return -1;}
        StorylandRoadChange change;
        for(size_t index=0;index<6;++index)change.bounds[index]=real(index);
        change.angled=true;change.width=real(6);
        world.roadChanges.push_back(change);return 1;
    }
    if(command==0x0303) {
        if(!finite(0,2))return -1;
        if(!world.requestCollision) {error="REQUEST_COLLISION requires the native collision streamer.";return -1;}
        return world.requestCollision({real(0),real(1),0},error)?1:-1;
    }
    if(command==0x04D7) {
        StorylandMissionEnvironment::CameraFocus target;
        if(values[2].bits) {
            if(values[1].bits>0) {
                const auto vehicle=world.missionVehicles.find(values[1].bits);
                if(vehicle==world.missionVehicles.end()||vehicle->second.deleted) {
                    error="Camera look-at vehicle is unavailable.";return -1;
                }
                target.kind=3;target.handle=values[1].bits;
            } else if(values[0].bits>=0) {
                const auto character=world.missionCharacters.find(values[0].bits);
                if(values[0].bits!=0&&(character==world.missionCharacters.end()||!character->second.alive)) {
                    error="Camera look-at character is unavailable.";return -1;
                }
                target.kind=values[0].bits==0?1:2;target.handle=values[0].bits;
            } else {error="Camera look-at requires a live character or vehicle.";return -1;}
        }
        world.cameraLookAt=target;
        if(!values[2].bits) {world.camera.active=false;world.camera.restoreRequested=true;world.cameraFocus={};}
        return 1;
    }
    if(!finite(0,3)||!finite(7,8)||(command==0x052D&&!finite(3,6)))return -1;
    StorylandMissionEnvironment::ScriptShadow shadow;
    shadow.position=point(0);shadow.radius=real(7)*2;
    shadow.intensity=int16_t(values[6].bits);
    shadow.pedestrian=command==0x052D;
    if(shadow.radius<0||shadow.radius>10000) {error="Static shadow radius is outside the render domain.";return -1;}
    if(command==0x04ED)shadow.color={uint8_t(values[3].bits),uint8_t(values[4].bits),uint8_t(values[5].bits)};
    else {
        if(!world.shadowCollision) {error="Pedestrian shadow requires native building collision.";return -1;}
        auto end=point(3);
        std::array<float,3> direction{};
        for(size_t axis=0;axis<3;++axis)direction[axis]=end[axis]-shadow.position[axis];
        const float length=std::hypot(direction[0],direction[1],direction[2]);
        if(!std::isfinite(length)) {error="Static shadow direction is non-finite.";return -1;}
        if(length>0)for(auto& axis:direction)axis/=length;else direction={1,0,0};
        for(size_t axis=0;axis<3;++axis)end[axis]=shadow.position[axis]+direction[axis]*130;
        std::array<float,3> hit{};
        if(!world.shadowCollision(shadow.position,end,hit))return 1;
        shadow.position=hit;
    }
    const auto key=std::make_pair(reinterpret_cast<uintptr_t>(&vm),int32_t(vm.pc));
    if(!world.frameScriptShadows.count(key)&&world.frameScriptShadows.size()>=64) {
        error="Frame static shadow capacity is full.";return -1;
    }
    world.frameScriptShadows[key]=shadow;return 1;
}

int executeVcsPresentationEffectsCommand(StorylandMissionVm& vm,uint16_t command,
    bool inverted,std::string& error) {
    if(!vm.vcs)return 0;
    int count=-1;
    switch(command) {
    case 0x029D: case 0x029E: case 0x0379: count=0;break;
    case 0x029C: case 0x049F: count=1;break;
    case 0x0277: case 0x0286: case 0x054C: count=2;break;
    case 0x0253: case 0x0378: count=3;break;
    case 0x00CD: case 0x016A: count=4;break;
    case 0x0223: case 0x0240: count=6;break;
    case 0x029F: case 0x04A0: count=8;break;
    case 0x03E2: case 0x04BF: case 0x0476: count=10;break;
    case 0x0231: count=12;break;
    default:return 0;
    }
    if(!vm.environment) {error="VCS presentation effect requires a live environment.";return -1;}
    auto& world=*vm.environment;
    std::array<Operand,12> values{};
    for(int index=0;index<count;++index)if(!readOperand(vm,values[size_t(index)])) {
        error="VCS presentation effect has truncated operands.";return -1;
    }
    const auto real=[&](size_t index){return operandFloat(values[index]);};
    const auto point=[&](size_t index){return std::array<float,3>{real(index),real(index+1),real(index+2)};};
    const auto finite=[&](size_t first,size_t last) {
        for(size_t index=first;index<=last;++index)if(!std::isfinite(real(index))) {
            error="Presentation coordinates and sizes must be finite.";return false;
        }
        return true;
    };
    const auto ground=[&](std::array<float,3>& position) {
        if(position[2]!=-100.0f&&position[2]>-250.0f)return true;
        float height=0;
        if(!world.groundHeightAt||!world.groundHeightAt(position,height)||!std::isfinite(height)) {
            error="Ground-sentinel position requires a native collision surface.";return false;
        }
        position[2]=height;return true;
    };
    if(command==0x029D) {world.credits.active=false;world.credits.lines.clear();return 1;}
    if(command==0x029E) {updateScmCondition(vm,!world.credits.active,inverted);return 1;}
    if(command==0x029C) {
        std::vector<StorylandMissionEnvironment::CreditsLine> lines;
        if(values[0].bits<0||values[0].bits>98||!world.prepareCredits||
            !world.prepareCredits(values[0].bits,lines,error)||lines.empty()||lines.size()>1024) {
            if(error.empty())error="START_CREDITS requires its native CRED table and font metrics.";
            return -1;
        }
        world.credits={true,values[0].bits,48,std::move(lines)};return 1;
    }
    if(command==0x0379) {world.pointerArrowActive=false;return 1;}
    if(command==0x0378) {
        if(!finite(0,2))return -1;
        auto position=point(0);if(!ground(position))return -1;
        world.pointerArrowPosition=position;world.pointerArrowActive=true;return 1;
    }
    if(command==0x00CD||command==0x016A) {
        if(!finite(0,3))return -1;
        auto position=point(0);if(!ground(position))return -1;
        world.nextRestartOverride={position[0],position[1],position[2],real(3)};
        world.nextRestartOverrideActive=true;
        if(command==0x016A)world.criticalMissionRestartRequested=true;
        return 1;
    }
    if(command==0x0277||command==0x0286) {
        if(values[0].bits!=0) {error="This runtime has one local player (handle zero).";return -1;}
        if(command==0x0286) {world.getOutOfJailFree=values[1].bits!=0;return 1;}
        if(values[1].bits<0||values[1].bits>=10) {
            error="Drunk input delay is outside the native ten-sample pad history.";return -1;
        }
        world.drunkInputDelay=uint8_t(values[1].bits);return 1;
    }
    if(command==0x0253) {
        if(!finite(0,2))return -1;
        if(!world.loadScene) {error="LOAD_SCENE requires the native scene streaming consumer.";return -1;}
        return world.loadScene(point(0),error)?1:-1;
    }
    if(command==0x054C) {
        const int32_t index=values[0].bits;
        if(index<0||index>3) {error="Completion milestone index is outside the native range0..3.";return -1;}
        if(index<3)world.statistics["GameCompletionMilestone"+std::to_string(index)]=values[1].bits!=0?1:0;
        else if(values[1].bits) {
            world.progressMade=world.progressTotal;
            const auto total=world.statistics.find("TotalProgressInGame");
            world.statistics["ProgressMade"]=total==world.statistics.end()?world.progressTotal:total->second;
        }
        return 1;
    }
    if(command==0x0223||command==0x0240) {
        if(!finite(0,5))return -1;
        std::array<float,6> bounds{};
        for(size_t axis=0;axis<3;++axis) {
            bounds[axis]=std::min(real(axis),real(axis+3));
            bounds[axis+3]=std::max(real(axis),real(axis+3));
        }
        if(command==0x0223) {
            if(!world.sniperBulletInArea) {error="Sniper area test requires the live bullet trace consumer.";return -1;}
            updateScmCondition(vm,world.sniperBulletInArea(bounds),inverted);return 1;
        }
        // Native removes CParticleObject emitters, leaving emitted particles alive.
        auto& emitters=world.scriptParticleEmitters;
        emitters.erase(std::remove_if(emitters.begin(),emitters.end(),[&](const auto& emitter) {
            for(size_t axis=0;axis<3;++axis)if(emitter.particle.position[axis]<bounds[axis]||
                emitter.particle.position[axis]>bounds[axis+3])return false;
            return true;
        }),emitters.end());
        return 1;
    }
    if(command==0x03E2||command==0x04BF) {
        if(!finite(0,5)||!finite(9,9))return -1;
        if(!values[0].reference) {error="Checkpoint requires a coordinate variable identity.";return -1;}
        StorylandMissionEnvironment::ScriptCheckpoint checkpoint;
        checkpoint.position=point(0);checkpoint.direction=point(3);checkpoint.area=world.area;
        checkpoint.type=command==0x03E2?3:8;
        checkpoint.radius=command==0x03E2?3.2f:real(9);
        checkpoint.color={uint8_t(values[6].bits),uint8_t(values[7].bits),uint8_t(values[8].bits),
            uint8_t(command==0x03E2?200:100)};
        if(command==0x03E2)checkpoint.position[2]+=real(9)+7.0f;
        if(checkpoint.radius<0||checkpoint.radius>10000||!std::isfinite(checkpoint.position[2])) {
            error="Checkpoint radius/height is outside the finite render domain.";return -1;
        }
        const auto key=std::make_pair(values[0].global?uintptr_t(0):reinterpret_cast<uintptr_t>(&vm),int32_t(values[0].variable));
        if(!world.frameScriptCheckpoints.count(key)&&world.frameScriptCheckpoints.size()>=32) {
            error="Host checkpoint marker capacity is full.";return -1;
        }
        world.frameScriptCheckpoints[key]=checkpoint;return 1;
    }
    if(command==0x049F) {
        if(!world.switchDollarParticles) {error="Dollar particles require the live particle manager.";return -1;}
        world.dollarParticleEnabled=values[0].bits!=0;
        world.switchDollarParticles(world.dollarParticleEnabled);return 1;
    }
    if(command==0x0231||command==0x029F||command==0x04A0) {
        if(!finite(1,7))return -1;
        StorylandMissionEnvironment::ScriptParticle particle;
        particle.type=command==0x0231?int32_t(uint16_t(values[0].bits)):values[0].bits;
        particle.position=point(1);particle.velocity=point(4);particle.size=real(7);
        if(command==0x0231) {
            if(!world.scriptEmitterAvailable||!world.scriptEmitterAvailable(uint16_t(particle.type))) {
                error="Particle-object subtype has no native emitter implementation.";return -1;
            }
            if(!ground(particle.position))return -1;
            particle.size=std::max(0.0f,particle.size);
            if(!particle.emitterParametersSupported()) {
                error="Particle emitter exceeds the downstream radius/velocity limits after native subtype scaling.";return -1;
            }
            if(particle.type==17)particle.color={uint8_t(values[8].bits),uint8_t(values[9].bits),uint8_t(values[10].bits),255};
            if(world.scriptParticleEmitters.size()>=256) {error="Script particle emitter capacity reached.";return -1;}
            const uint32_t duration=particle.type==6?5000u:uint32_t(values[11].bits);
            const uint32_t deadline=duration?world.gameMilliseconds+duration:0;
            world.scriptParticleEmitters.push_back({particle,deadline,deadline!=0});
            return 1;
        }
        if(command==0x04A0) {
            if(!world.dollarParticleEnabled||!world.createDollarParticles) {
                error="Dollar emission requires an enabled dollar particle manager.";return -1;
            }
            return world.createDollarParticles(uint8_t(values[0].bits),particle,error)?1:-1;
        }
        if(!world.createScriptParticles) {error="Single particle requires the live native descriptor consumer.";return -1;}
        return world.createScriptParticles({particle},error)?1:-1;
    }
    // Retail1EC068 emits three particles at each point and then advances by a
    // unit vector. Count is trunc(max(density,1)*4); the last SCM operand is unused.
    if(!finite(1,8))return -1;
    if(real(8)<0||real(8)>10000) {
        error="Particle burst radius is outside the finite host render domain.";return -1;
    }
    const float density=std::max(1.0f,real(7));
    if(density>170.0f||!world.createScriptParticles) {
        error="Particle burst exceeds the bounded live particle budget.";return -1;
    }
    auto position=point(1);auto direction=point(4);
    for(size_t axis=0;axis<3;++axis)direction[axis]-=position[axis];
    const float length=std::hypot(direction[0],direction[1],direction[2]);
    if(!std::isfinite(length)) {error="Particle burst direction is non-finite.";return -1;}
    if(length>0)for(auto& axis:direction)axis/=length;else direction={1,0,0};
    const uint32_t savedRandom=world.nativeRandomState;
    const auto random=[&](float low,float high) {
        return low+(high-low)*(float(world.nextNativeRandom())/2147483648.0f);
    };
    std::vector<StorylandMissionEnvironment::ScriptParticle> particles;
    for(int index=0;index<int(density*4.0f);++index) {
        for(unsigned variant=0;variant<3;++variant) {
            StorylandMissionEnvironment::ScriptParticle particle;
            particle.type=values[0].bits;particle.position=position;particle.size=real(8);
            particle.color={240,240,240,255};
            const float spread=variant==0?.02f:.04f;
            particle.velocity={random(-spread,spread),random(-spread,spread),
                variant==0?random(-.02f,0):random(0,.07f)};
            particles.push_back(particle);
        }
        for(size_t axis=0;axis<3;++axis)position[axis]+=direction[axis];
    }
    if(!world.createScriptParticles(particles,error)) {world.nativeRandomState=savedRandom;return -1;}
    return 1;
}

int executeVcsWorldUtilityCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (!vm.vcs) {
        return 0;
    }
    const int presentation=executeVcsPresentationEffectsCommand(vm,command,inverted,error);
    if(presentation)return presentation;
    int count = -1;
    switch (command) {
    case 0x0166: case 0x0167:
        count = 0;
        break;
    case 0x0351: case 0x049B: case 0x04D9: case 0x03F2: case 0x04F3:
    case 0x0477: case 0x04D8: case 0x0556:
        count = 1;
        break;
    case 0x0484: case 0x0493: case 0x01AD: case 0x04CE: case 0x049C:
        count = 2;
        break;
    case 0x02AD: case 0x02B5:
        count = 3;
        break;
    case 0x0494:
        count = 4;
        break;
    case 0x0421:
        count = 5;
        break;
    case 0x0227: case 0x047C:
        count = 6;
        break;
    case 0x0555:
        count = 12;
        break;
    case 0x04CD:
        count = 0;
        break;
    default:
        return 0;
    }
    if (!vm.environment) {
        error = "VCS world utility requires a live environment.";
        return -1;
    }
    auto& world = *vm.environment;
    if (command == 0x04CD) {
        Operand label;
        Operand enabled;
        std::string name;
        if (!readStringOperand(vm, label, name) || name.size() > 8 ||
            !readOperand(vm, enabled)) {
            error = "SET_AREA_NAME requires a typed GXT key and an enable flag.";
            return -1;
        }
        world.areaNameForced = enabled.bits != 0;
        world.areaNameKey = world.areaNameForced ? name : std::string{};
        using Phase = StorylandMissionEnvironment::VcsAreaCaption::Phase;
        world.vcsAreaCaption.phase = world.areaNameForced ? Phase::Hold : Phase::FadeOut;
        if (!world.areaNameForced) {
            world.vcsAreaCaption.previousKey = storylandMissionAreaNameKey(world);
        }
        return 1;
    }
    if (command == 0x0166) {
        // CWeather::StoreWeatherState at 0x3E5A80 saves both weather IDs,
        // interpolation and rain. The forced-weather selector is independent.
        world.storedWeatherState = world.weatherState;
        world.weatherStored = true;
        return 1;
    }
    if (command == 0x0167) {
        // Restore has no "was stored" guard and does not clear the saved flag.
        world.weatherState = world.storedWeatherState;
        world.weatherRestoreRequested = true;
        return 1;
    }
    std::array<Operand, 12> values{};
    for (int index = 0; index < count; ++index) {
        if (!readOperand(vm, values[size_t(index)])) {
            error = "VCS world utility has truncated typed operands.";
            return -1;
        }
    }
    if (command == 0x04CE) {
        if (storylandSetVcsHudComponentVisible(world,values[0].bits,values[1].bits != 0)) return 1;
        error = "VCS HUD component has no verified presentation consumer.";
        return -1;
    }
    if (command == 0x0556) {
        // Native11D540 writes489EC4; no unsupported rendering effect is assigned.
        world.vcsScript0556Value = values[0].bits != 0 ? 0.1f : 1.0f;
        return 1;
    }
    if (command == 0x049C) {
        // Native4062E8 consumes both operands and writes only the first to HUD+2C28.
        world.scriptBinocularOverlay = values[0].bits != 0;
        return 1;
    }
    const auto write = [&](size_t index, int32_t value) {
        if (!writeOperand(vm, values[index], value)) {
            error = "VCS world query requires a writable output.";
            return false;
        }
        return true;
    };
    const auto writeReal = [&](size_t index, float value) {
        int32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return write(index, bits);
    };
    if (command == 0x0484) {
        // The SCM compiler already hashes this operand. Retail 0x21B7A0
        // performs CollectParameters(1), then StoreParameters(1), unchanged.
        return write(1, values[0].bits) ? 1 : -1;
    }
    if (command == 0x049B) {
        const auto found = world.statistics.find("DaysPassed");
        const double days = found == world.statistics.end() ? 0 : found->second;
        if (!std::isfinite(days) || days < 0 || days > UINT32_MAX || std::floor(days) != days) {
            error = "Days passed is outside its native unsigned word.";
            return -1;
        }
        const uint32_t bits = uint32_t(days);
        int32_t result = 0;
        std::memcpy(&result, &bits, sizeof(result));
        return write(0, result) ? 1 : -1;
    }
    if (command == 0x0351) {
        auto& best = world.statistics["HighestLevelVigilanteMission"];
        best = std::max(best, double(values[0].bits));
        return 1;
    }
    if (command == 0x04D9) {
        // Despite the old INI's "get" name, 0x185398 loads an unsigned
        // halfword from the input and stores it in the balloons-burst stat.
        world.statistics["BalloonsBurst"] = uint16_t(values[0].bits);
        return 1;
    }
    if (command == 0x03F2 || command == 0x04F3) {
        const uint8_t costume = uint8_t(values[0].bits);
        if (costume < 16) {
            auto& statistic = world.statistics["CostumeUnlockMask"];
            if (!std::isfinite(statistic) || statistic < 0 || statistic > UINT16_MAX ||
                std::floor(statistic) != statistic) {
                error = "Costume statistics mask is not an unsigned halfword.";
                return -1;
            }
            uint16_t mask = uint16_t(statistic);
            const uint16_t bit = uint16_t(1u << costume);
            mask = command == 0x03F2 ? uint16_t(mask | bit) : uint16_t(mask & ~bit);
            statistic = mask;
        }
        return 1;
    }
    if (command == 0x0477) {
        if (!values[0].reference) {
            error = "Mission script name requires a writable string variable.";
            return -1;
        }
        const std::string name = world.activeMissionScriptName ? world.activeMissionScriptName() :
            vm.isMissionThread ? vm.scriptName : std::string{};
        if (!write(0, 0)) {
            return -1;
        }
        if (!name.empty()) {
            auto& strings = values[0].global ? vm.globalStrings : vm.localStrings;
            strings[values[0].variable] = name.substr(0, 8);
        }
        return 1;
    }
    if (command == 0x01AD) {
        // Native wakes at unsigned(now + delay), and only sets the skip bit
        // when requested; a later regular WAIT explicitly clears that bit.
        const uint32_t now = uint32_t(world.gameMilliseconds);
        const uint32_t deadline = now + uint32_t(values[0].bits);
        vm.delayMilliseconds = deadline > now ? deadline - now : 0;
        vm.vcsWakeTime = deadline;
        vm.vcsAbsoluteWait = true;
        if (values[1].bits != 0) {
            vm.messageWaitSkippable = true;
        }
        vm.state = StorylandMissionState::Waiting;
        return 1;
    }
    if (command == 0x02AD || command == 0x02B5) {
        for (size_t index = 0; index < 3; ++index) {
            if (!values[index].reference) {
                error = "Debug camera vector requires three writable outputs.";
                return -1;
            }
        }
        std::array<float, 3> eye{};
        std::array<float, 3> target{};
        if (!world.activeCameraVectors || !world.activeCameraVectors(eye, target)) {
            error = "Debug camera query requires the active camera lens.";
            return -1;
        }
        std::array<float, 3> result = eye;
        if (command == 0x02B5) {
            // Native returns source + unit front, not the distant look-at
            // coordinate used by the host camera API.
            const float dx = target[0] - eye[0];
            const float dy = target[1] - eye[1];
            const float dz = target[2] - eye[2];
            const float length = std::hypot(dx, dy, dz);
            if (!(length > 0) || !std::isfinite(length)) {
                error = "Debug camera has no finite forward vector.";
                return -1;
            }
            result = {
                roundEeFloat(double(eye[0]) + dx / length),
                roundEeFloat(double(eye[1]) + dy / length),
                roundEeFloat(double(eye[2]) + dz / length)
            };
        }
        for (size_t index = 0; index < 3; ++index) {
            if (!std::isfinite(result[index])) {
                error = "Debug camera vector is non-finite.";
                return -1;
            }
        }
        return writeReal(0, result[0]) && writeReal(1, result[1]) && writeReal(2, result[2]) ? 1 : -1;
    }
    if (command == 0x0493 || command == 0x0494) {
        const size_t outputs = command == 0x0493 ? 1 : 3;
        for (size_t index = 1; index <= outputs; ++index) {
            if (!values[index].reference) {
                error = "Building bounds query requires writable outputs.";
                return -1;
            }
        }
        if (!world.buildingSwapBounds) {
            error = "Building bounds query requires the native placement table.";
            return -1;
        }
        std::array<float, 3> position{};
        float radius = 0;
        world.buildingSwapBounds(uint32_t(values[0].bits), position, radius);
        if (!std::isfinite(radius) || !std::all_of(position.begin(), position.end(),
            [](float value) { return std::isfinite(value); })) {
            error = "Building bounds query produced non-finite collision data.";
            return -1;
        }
        if (command == 0x0493) {
            return writeReal(1, radius) ? 1 : -1;
        }
        return writeReal(1, position[0]) && writeReal(2, position[1]) && writeReal(3, position[2]) ? 1 : -1;
    }
    if (command == 0x04D8) {
        storylandPrepareObjectPoolIndices(world);
        const auto found = std::find_if(world.objects.begin(), world.objects.end(),
            [&](const auto& object) { return object.handle == values[0].bits; });
        // 0x311764 returns true for a stale pool reference. For a live normal
        // object it compares the signed health byte to zero, not health > 0.
        const bool inactive = found == world.objects.end() || found->nativePoolIndex <= 0 ||
            found->nativePoolIndex >= 475 || found->damage.health == 0;
        updateScmCondition(vm, inactive, inverted);
        return 1;
    }
    if (command == 0x0421 || command == 0x047C) {
        const size_t first = command == 0x0421 ? 0 : 1;
        const size_t output = command == 0x0421 ? 4 : 5;
        if (!values[output].reference) {
            error = "Object pool search requires a writable output.";
            return -1;
        }
        for (size_t index = first; index < output; ++index) {
            if (!std::isfinite(operandFloat(values[index]))) {
                error = "Object pool search coordinates must be finite.";
                return -1;
            }
        }
        int32_t result = -1;
        storylandPrepareObjectPoolIndices(world);
        if (command == 0x0421) {
            const float x0 = std::min(operandFloat(values[0]), operandFloat(values[2]));
            const float x1 = std::max(operandFloat(values[0]), operandFloat(values[2]));
            const float y0 = std::min(operandFloat(values[1]), operandFloat(values[3]));
            const float y1 = std::max(operandFloat(values[1]), operandFloat(values[3]));
            int32_t selectedIndex = -1;
            for (const auto& object : world.objects) {
                if (object.x >= x0 && object.x <= x1 && object.y >= y0 && object.y <= y1 &&
                    object.nativePoolIndex > selectedIndex) {
                    result = object.handle;
                    selectedIndex = object.nativePoolIndex;
                }
            }
        } else {
            const float radius = operandFloat(values[4]);
            const float squaredRadius = roundEeFloat(double(radius) * radius);
            for (size_t index = 0; index < world.objects.size(); ++index) {
                const auto& object = world.objects[index];
                if (object.model != values[0].bits) {
                    continue;
                }
                const float dx = roundEeFloat(double(operandFloat(values[1])) - object.x);
                const float dy = roundEeFloat(double(operandFloat(values[2])) - object.y);
                const float dz = roundEeFloat(double(operandFloat(values[3])) - object.z);
                const float xy = roundEeFloat(double(roundEeFloat(double(dx) * dx)) +
                    roundEeFloat(double(dy) * dy));
                const float distance = roundEeFloat(double(xy) + roundEeFloat(double(dz) * dz));
                if (distance < squaredRadius) {
                    if (result < 0 || object.nativePoolIndex < result) {
                        result = object.nativePoolIndex;
                    }
                }
            }
        }
        return write(output, result) ? 1 : -1;
    }
    if (command == 0x0555) {
        std::array<float, 3> start{};
        std::array<float, 3> end{};
        for (size_t axis = 0; axis < 3; ++axis) {
            start[axis] = operandFloat(values[axis]);
            end[axis] = operandFloat(values[axis + 3]);
            if (!std::isfinite(start[axis]) || !std::isfinite(end[axis])) {
                error = "Line-of-sight endpoints must be finite.";
                return -1;
            }
        }
        // Native ordering is see-through, buildings, cars, peds, objects,
        // dummies. The old INI shifts every flag by one.
        if (values[6].bits != 0) {
            error = "See-through material filtering is not available for script collision queries.";
            return -1;
        }
        uint32_t mask = 0;
        for (size_t index = 0; index < 5; ++index) {
            if (values[index + 7].bits != 0) {
                mask |= 1u << index;
            }
        }
        if (!world.scriptLineOfSightHit) {
            error = "Line-of-sight query requires the native collision runtime.";
            return -1;
        }
        updateScmCondition(vm, world.scriptLineOfSightHit(start, end, mask), inverted);
        return 1;
    }
    // 0x0227 checks mission-created entities only. Native 0x196620 expands
    // each entity's origin by its model bounding radius, including the Z axis.
    std::array<float, 6> bounds{};
    for (size_t axis = 0; axis < 3; ++axis) {
        const float center = operandFloat(values[axis]);
        const float radius = operandFloat(values[axis + 3]);
        bounds[axis] = std::min(center - radius, center + radius);
        bounds[axis + 3] = std::max(center - radius, center + radius);
        if (!std::isfinite(bounds[axis]) || !std::isfinite(bounds[axis + 3])) {
            error = "Mission entity obstruction bounds must be finite.";
            return -1;
        }
    }
    if (!world.nativeModelRadius) {
        error = "Mission entity obstruction requires native model bounds.";
        return -1;
    }
    const auto overlaps = [&](int32_t model, float x, float y, float z, bool& valid) {
        float radius = 0;
        valid = world.nativeModelRadius(model, radius) && std::isfinite(radius) && radius >= 0;
        if (!valid) {
            return false;
        }
        return x + radius >= bounds[0] && x - radius <= bounds[3] &&
            y + radius >= bounds[1] && y - radius <= bounds[4] &&
            z + radius >= bounds[2] && z - radius <= bounds[5];
    };
    bool occupied = false;
    bool valid = true;
    for (const auto& entry : world.missionVehicles) {
        const auto& vehicle = entry.second;
        if (!vehicle.deleted && vehicle.persistent) {
            occupied |= overlaps(vehicle.model, vehicle.x, vehicle.y, vehicle.z, valid);
            if (!valid) {
                error = "Mission vehicle has no native bounding sphere.";
                return -1;
            }
        }
    }
    for (const auto& entry : world.missionCharacters) {
        const auto& character = entry.second;
        if (character.creationSource == 2) {
            occupied |= overlaps(character.model, character.x, character.y, character.z, valid);
            if (!valid) {
                error = "Mission character has no native bounding sphere.";
                return -1;
            }
        }
    }
    for (const auto& object : world.objects) {
        if (object.creationSource == 2) {
            occupied |= overlaps(object.model, object.x, object.y, object.z, valid);
            if (!valid) {
                error = "Mission object has no native bounding sphere.";
                return -1;
            }
        }
    }
    updateScmCondition(vm, occupied, inverted);
    return 1;
}

int executeVcsWeaponCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(!vm.vcs) {
        if(command==0x01BD)command=0x010B;
        else return 0;
    }
    const bool give=command==0x0106,addAmmo=command==0x009E,setAmmo=command==0x00D5;
    const bool equip=command==0x010B,clear=command==0x02D4,has=command==0x02D5;
    const bool ammunition=command==0x028B,slot=command==0x02E7;
    const bool current=command==0x01C2,removeSlot=command==0x042A;
    if(!give&&!addAmmo&&!setAmmo&&!equip&&!clear&&!has&&!ammunition&&!slot&&!current&&!removeSlot)return 0;
    if(!vm.environment) {error="Weapon command requires a live character inventory.";return -1;}
    const size_t count=clear?1:slot?5:give||addAmmo||setAmmo||ammunition?3:2;
    std::array<Operand,5> values{};
    for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="Weapon command has a truncated typed operand.";return -1;
    }
    auto& world=*vm.environment;
    const auto character=world.missionCharacters.find(values[0].bits);
    if(values[0].bits!=0&&character==world.missionCharacters.end()) {
        error="Weapon command references an unknown character.";return -1;
    }
    if((slot&&(!values[2].reference||!values[3].reference||!values[4].reference))||
        (ammunition&&!values[2].reference)) {
        error="Weapon query requires writable outputs.";return -1;
    }
    const int32_t type=values[1].bits;
    if(!clear&&(type<0||type>(slot||removeSlot?31:255))) {
        error="Weapon or slot is outside the native range.";return -1;
    }
    const auto apply=[&](auto& inventory,int32_t& selected,bool& changed,bool& selectionChanged)->int {
        if(clear) {
            for(auto& entry:inventory) {entry.second.ammunition=0;entry.second.owned=entry.first==0;}
            selected=0;changed=true;selectionChanged=true;return 1;
        }
        if(current) {updateScmCondition(vm,selected==type,inverted);return 1;}
        if(slot) {
            int32_t nativeId=0,ammo=0,model=-1;
            // 419610 indexes CPed+55C; weapon types start at578, one slot later.
            const int32_t nativeSlot=type-1;
            for(const auto& entry:inventory)if(entry.second.owned&&entry.second.slot==nativeSlot) {
                nativeId=entry.first;ammo=entry.second.ammunition;model=entry.second.model;break;
            }
            if(!writeOperand(vm,values[2],nativeId)||!writeOperand(vm,values[3],ammo)||!writeOperand(vm,values[4],model)) {
                error="Weapon slot outputs could not be written.";return -1;
            }
            return 1;
        }
        if(removeSlot) {
            for(auto& entry:inventory)if(entry.second.slot==type) {
                entry.second.owned=false;entry.second.ammunition=0;
                if(selected==entry.first) {selected=0;selectionChanged=true;}
            }
            changed=true;return 1;
        }
        const auto found=inventory.find(type);
        const bool owned=found!=inventory.end()&&found->second.owned;
        if(has) {updateScmCondition(vm,owned,inverted);return 1;}
        if(ammunition) {
            if(!writeOperand(vm,values[2],owned?found->second.ammunition:0)) {
                error="Ammo query output could not be written.";return -1;
            }
            return 1;
        }
        if(equip) {if(owned) {selected=type;selectionChanged=true;}return 1;}
        if(!give&&!owned)return 1;
        const auto metadata=world.playerWeapons.find(type);
        if(give&&metadata==world.playerWeapons.end()) {
            error="Weapon is absent from the loaded native weapon definitions.";return -1;
        }
        auto& weapon=inventory[type];
        if(give) {
            weapon.slot=metadata->second.slot;weapon.model=metadata->second.model;
            for(auto& other:inventory)if(other.first!=type&&other.first!=0&&other.second.slot==weapon.slot) {
                other.second.owned=false;other.second.ammunition=0;
            }
            weapon.owned=true;selected=type;selectionChanged=true;
        }
        // Native 41430C passes the signed word through GiveWeapon. In
        // particular, original Cleaning House grants melee weapon6 with -1.
        // CPed's ADDU wraps at32 bits; CWeapon never clamps a negative total.
        const uint32_t sum=uint32_t(weapon.ammunition)+uint32_t(values[2].bits);
        int32_t signedSum=0;
        std::memcpy(&signedSum,&sum,sizeof(signedSum));
        weapon.ammunition=std::min(setAmmo?values[2].bits:signedSum,9999);
        changed=true;return 1;
    };
    if(values[0].bits==0)return apply(world.playerWeapons,world.currentWeapon,
        world.weaponInventoryChanged,world.weaponSelectionRequested);
    auto& ped=character->second;
    const int result=apply(ped.weapons,ped.currentWeapon,ped.weaponsChanged,ped.weaponSelectionChanged);
    if(result>0&&give&&ped.vehicle<0&&!ped.attached) {
        // SLUS_215.90 0x4143EC..0x414460 resets the standing NPC's movement
        // associations after a grant and selects the native two-hand group.
        ped.animationGroup=storylandNativeWeaponUsesTwoHands(true,ped.currentWeapon)?33:0;
        ped.animationId=-1;
        ped.animationLoop=false;
        world.pedAnimationRequests.erase(values[0].bits);
        world.scriptedCharacterAnimations.erase(values[0].bits);
    }
    return result;
}

int executeVcsRadarCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    if (vm.vcs && (command == 0x044D || command == 0x04BB || command == 0x0509 ||
        command == 0x0548 || command == 0x0549)) {
        if (!vm.environment) {
            error = "Radar command requires a live world.";
            return -1;
        }
        auto& world = *vm.environment;
        if (command == 0x04BB) {
            std::set<int32_t> handles;
            for (const auto& blip : world.radarBlips) {
                handles.insert(blip.handle);
            }
            world.markers.erase(std::remove_if(world.markers.begin(), world.markers.end(),
                [&](const auto& marker) { return handles.count(marker.handle) != 0; }), world.markers.end());
            world.radarBlips.clear();
            return 1;
        }
        const unsigned count = command == 0x044D ? 4 : command == 0x0549 ? 1 : 3;
        std::array<Operand, 4> values{};
        for (unsigned index = 0; index < count; ++index) {
            if (!readOperand(vm, values[index])) {
                error = "Radar command has truncated typed operands.";
                return -1;
            }
        }
        const unsigned start = command == 0x044D ? 1 : 0;
        for (unsigned index = start; index < count; ++index) {
            if (!std::isfinite(operandFloat(values[index]))) {
                error = "Radar coordinates and radii must be finite.";
                return -1;
            }
        }
        if (command == 0x044D) {
            const int32_t layer = values[0].bits;
            if (layer < 0 || layer >= 3) {
                error = "Toggleable radar layer must be 0, 1, or 2.";
                return -1;
            }
            auto& points = world.toggleableRadarBlips[size_t(layer)];
            if (points.size() >= 100) {
                error = "Native toggleable radar layer has reached 100 entries.";
                return -1;
            }
            const float x = operandFloat(values[1]);
            const float y = operandFloat(values[2]);
            // Native 0x1180A0 stores XY only. FLT_MAX/FLT_MAX is its empty slot.
            if (x != std::numeric_limits<float>::max() || y != std::numeric_limits<float>::max()) {
                points.push_back({x, y});
            }
        } else if (command == 0x0509 || command == 0x0548) {
            auto& point = command == 0x0509 ? world.radarLocalOrigin : world.radarMapAnchor;
            for (unsigned index = 0; index < 3; ++index) {
                point[index] = operandFloat(values[index]);
            }
        } else {
            const float radius = operandFloat(values[0]);
            const float squared = radius * radius;
            for (auto& blip : world.radarBlips) {
                StorylandNativePoint point;
                if (!storylandMissionBlipPosition(world, blip, point)) {
                    continue;
                }
                const float dx = point.x - world.x;
                const float dy = point.y - world.y;
                const float dz = point.z - world.z;
                if (radius == 0 || dx * dx + dy * dy + dz * dz >= squared) {
                    blip.exterior = true;
                }
            }
        }
        return 1;
    }
    const uint16_t sourceCommand=command;
    if(!vm.vcs) {
        switch(command) {
        case 0x0169:command=0x00BB;break;
        case 0x016A:command=0x00BC;break;
        case 0x016B:command=0x00BD;break;
        case 0x016C:command=0x00BE;break;
        case 0x016D:command=0x00BF;break;
        case 0x018B:command=0x00C0;break;
        case 0x018C:command=0x00C1;break;
        case 0x018D:command=0x00C2;break;
        case 0x018E:case 0x018F:command=0x00C3;break;
        case 0x0190:command=0x00C4;break;
        case 0x02AC:command=0x00C5;break;
        case 0x02AD:command=0x00C6;break;
        case 0x04D3:command=0x00C7;break;
        case 0x0575:command=0x034C;break;
        default:return 0;
        }
    }
    const bool empire=command==0x049E||command==0x04CB;
    const bool remove=command==0x00BB;
    const bool change=command==0x00BC||command==0x00BD||command==0x00BF||command==0x00C4;
    const bool linked=command==0x00C0||command==0x00C1||command==0x00C2||command==0x0263;
    const bool plain=command==0x00C3,old=command==0x00BE;
    const bool sprite=command==0x00C5||command==0x00C6||command==0x00C7||command==0x034C;
    const bool settings=command==0x035A||command==0x0492||command==0x04F5;
    if(!empire&&!remove&&!change&&!linked&&!plain&&!old&&!sprite&&!settings)return 0;
    if(!vm.environment) {error="Radar command requires a live environment.";return -1;}
    const size_t count=remove||settings?1:change||linked||empire?2:plain?4:old?6:5;
    std::array<Operand,6> values{};
    for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="Radar command has a truncated typed operand.";return -1;
    }
    auto& world=*vm.environment;
    if(settings) {
        const int32_t value=values[0].bits;
        if(command==0x035A)world.radarVisible=value!=0;
        else if(command==0x0492) {
            if(value<0||value>3) {error="Radar display mode is outside 0..3.";return -1;}
            world.radarDisplayMode=value;
        } else {
            const float range=operandFloat(values[0]);
            if(!std::isfinite(range)||range<0||range>10000) {error="Radar zoom must be a finite nonnegative range.";return -1;}
            world.radarRange=range;
        }
        return 1;
    }
    if(remove) {
        world.radarBlips.erase(std::remove_if(world.radarBlips.begin(),world.radarBlips.end(),
            [&](const auto& blip){return blip.handle==values[0].bits;}),world.radarBlips.end());
        world.markers.erase(std::remove_if(world.markers.begin(),world.markers.end(),
            [&](const auto& marker){return marker.handle==values[0].bits;}),world.markers.end());
        return 1;
    }
    if(change) {
        const auto found=std::find_if(world.radarBlips.begin(),world.radarBlips.end(),
            [&](const auto& blip){return blip.handle==values[0].bits;});
        if(found==world.radarBlips.end())return 1;
        const int32_t value=values[1].bits;
        if(command==0x00BC)found->color=uint32_t(value);
        else if(command==0x00BD)found->bright=vm.vcs?value!=0:value==1;
        else if(command==0x00BF)found->scale=vm.vcs?uint8_t(value):uint16_t(value);
        else {
            if(value<0||value>3) {error="Blip display mode is outside 0..3.";return -1;}
            found->display=value;
        }
        return 1;
    }
    const size_t output=count-1;
    if(!values[output].reference) {error="Adding a radar blip requires a writable handle output.";return -1;}
    StorylandMissionEnvironment::RadarBlip blip;
    if(empire) {
        const int32_t siteIndex=values[0].bits;
        if(siteIndex<0||size_t(siteIndex)>=world.empireSites.size()) {
            error="Empire radar blip requires a site index from 0 through 29.";
            return -1;
        }
        const auto& position=StorylandVcsEmpireSites[size_t(siteIndex)];
        const auto& site=world.empireSites[size_t(siteIndex)];
        blip.x=position[0];
        blip.y=position[1];
        blip.z=position[2];
        blip.sprite=site.state==5?31:site.damaged?30:29;
        blip.shortRange=command==0x049E;
    } else if(linked) {
        blip.entityHandle=(!vm.vcs&&command==0x00C1&&values[0].bits==1)?0:values[0].bits;
        using Entity=StorylandMissionEnvironment::BlipEntity;
        blip.entity=command==0x00C0?Entity::Vehicle:command==0x00C1?Entity::Character:
            command==0x00C2?Entity::Object:Entity::Pickup;
        blip.color=command==0x00C0?0:command==0x00C1?1:6;
    } else {
        blip.x=operandFloat(values[0]);blip.y=operandFloat(values[1]);blip.z=operandFloat(values[2]);
        if(!std::isfinite(blip.x)||!std::isfinite(blip.y)||!std::isfinite(blip.z)) {
            error="Radar blip coordinates must be finite.";return -1;
        }
        if(!vm.vcs&&blip.z<=-100.0f) {
            if(!world.groundHeightAt) {error="Radar coordinates require native ground collision for this height.";return -1;}
            float height=0;
            if(!world.groundHeightAt({blip.x,blip.y,1000},height))height=0;
            if(!std::isfinite(height)) {error="Radar ground height is non-finite.";return -1;}
            blip.z=height;
        }
        if(!vm.vcs&&(sourceCommand==0x018E||sourceCommand==0x02AC||sourceCommand==0x0575))
            blip.entity=StorylandMissionEnvironment::BlipEntity::ContactPoint;
        if(!vm.vcs&&sourceCommand==0x018F)blip.color=4;
        if(sprite) {
            if(values[3].bits<0||values[3].bits>255) {error="Radar sprite ID is outside its native range.";return -1;}
            blip.sprite=values[3].bits;
        }
        if(old) {
            blip.color=uint32_t(values[3].bits);blip.display=values[4].bits;
            if(blip.display<0||blip.display>3) {error="Blip display mode is outside 0..3.";return -1;}
        }
        blip.shortRange=command==0x00C7||command==0x034C;
        if(command==0x00C5||command==0x034C)blip.color=2;
        else if(command==0x00C6)blip.color=vm.vcs?4:5;
    }
    blip.handle=storylandAllocateMissionBlipHandle(world,vm.vcs);
    if(!writeOperand(vm,values[output],blip.handle)) {error="Radar blip handle could not be written.";return -1;}
    if(blip.handle!=-1)world.radarBlips.push_back(blip);
    return 1;
}

int executeVcsCameraCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if (vm.vcs && command == 0x01F8) {
        Operand value;
        if (!vm.environment || !readOperand(vm, value)) {
            error = "Camera zoom requires a live follow camera and a signed halfword level.";
            return -1;
        }
        auto& world = *vm.environment;
        // Native 0x287C00 dispatches only active follow-ped4 and follow-car18.
        if (!world.activeCameraMode) {
            error = "Camera zoom requires the actual active camera controller.";
            return -1;
        }
        const int32_t mode = world.activeCameraMode();
        if (mode != 4 && mode != 18) {
            return 1;
        }
        const int16_t level = int16_t(value.bits);
        auto& zoom = world.cameraZoom;
        if (mode == 4) {
            static constexpr float offsets[] = {0, 2.5f, 3.5f};
            if (level >= 0 && level < 3) {
                zoom.pedTarget = offsets[level];
            }
            zoom.ped = true;
        } else {
            const int32_t kind = world.activeVehicleCameraClass ? world.activeVehicleCameraClass() : -1;
            if (kind < 1 || kind > 5) {
                error = "Vehicle camera zoom requires its actual native handling class.";
                return -1;
            }
            // Native 0x377560 remaps handling classes1/2/3/4/5 to0/1/2/4/3.
            static constexpr unsigned classes[] = {0, 1, 2, 4, 3};
            static constexpr float offsets[3][5] = {
                {-1.0f, -.2f, -3.2f, .05f, -2.41f},
                {2.0f, 2.2f, 1.65f, 2.9f, 6.49f},
                {6.0f, 6.0f, 15.9f, 15.9f, 15.0f}
            };
            const unsigned index = classes[kind - 1];
            if (!zoom.vehicle) {
                zoom.vehicleCurrent = offsets[std::clamp(world.cameraVehicleZoom, 1, 3) - 1][index];
            }
            if (level >= 0 && level < 3) {
                zoom.vehicleTarget = offsets[level][index];
            }
            zoom.vehicle = true;
        }
        return 1;
    }
    if(vm.vcs&&(command==0x0250||command==0x02AB||command==0x039C||command==0x040C||
        command==0x0269||command==0x03A1||command==0x04E1)) {
        if(!vm.environment) {error="Camera/HUD command requires a live world.";return -1;}
        auto& world=*vm.environment;
        if(command==0x0250) {
            world.cameraBehindPlayerRequested=false;world.cameraFrontPlayerRequested=true;
        } else if(command==0x02AB)world.cameraControlEnabled=true;
        else if(command==0x03A1||command==0x0269) {
            Operand value;
            if(!readOperand(vm,value)) {error="HUD command has a missing typed operand.";return -1;}
            if(command==0x03A1)world.wantsHud=value.bits!=0;
            else {
                const int16_t object=int16_t(value.bits);
                world.flashingHudObject=object;
                world.vcsHudMeters.flash(object);
            }
        } else if(command==0x04E1)updateScmCondition(vm,world.wantsHud,inverted);
        else if(command==0x039C) {
            Operand player;
            if(!readOperand(vm,player)||player.bits!=0||!world.activeCameraMode) {
                error="First-person camera query requires player zero and the active controller.";return -1;
            }
            const int32_t mode=world.activeCameraMode();
            updateScmCondition(vm,mode==7||mode==34||mode==8||mode==45||mode==47||mode==46,inverted);
        } else {
            std::array<Operand,5> values;
            for(auto& value:values)if(!readOperand(vm,value)) {error="GET_SCREEN_COORDINATES requires XYZ and two outputs.";return -1;}
            const std::array<float,3> point{operandFloat(values[0]),operandFloat(values[1]),operandFloat(values[2])};
            if(!values[3].reference||!values[4].reference||!std::isfinite(point[0])||
                !std::isfinite(point[1])||!std::isfinite(point[2])||!world.screenCoordinates) {
                error="GET_SCREEN_COORDINATES requires a finite point, writable outputs, and the active lens.";return -1;
            }
            std::array<float,2> screen{};
            if(!world.screenCoordinates(point,screen)) {error="Active camera cannot project this point.";return -1;}
            for(float coordinate:screen)if(!std::isfinite(coordinate)||double(coordinate)<INT32_MIN||double(coordinate)>INT32_MAX) {
                error="Projected coordinate is outside the native signed integer range.";return -1;
            }
            if(!writeOperand(vm,values[3],int32_t(screen[0]))||!writeOperand(vm,values[4],int32_t(screen[1])))return -1;
        }
        return 1;
    }
    if(vm.vcs&&(command==0x007F||command==0x005D||command==0x03C0||command==0x0470||command==0x0471||
        command==0x0524||command==0x0525)) {
        if(!vm.environment) {error="Camera/input query requires a live world.";return -1;}
        auto& world=*vm.environment;
        if(command==0x03C0) {
            // Retail 0x2D54A8 tests CPad[0]+0x96 bit0x20, the same
            // script-disable flag set/cleared by SET_PLAYER_CONTROL.
            updateScmCondition(vm,!world.control,inverted);
        } else if(command==0x007F) {
            Operand pad,button;
            if(!readOperand(vm,pad)||!readOperand(vm,button)) {error="IS_BUTTON_PRESSED requires a pad and button.";return -1;}
            const uint16_t index=uint16_t(button.bits);
            updateScmCondition(vm,uint16_t(pad.bits)==0&&index<world.padState.size()&&world.padState[index]!=0,inverted);
        } else if(command==0x005D) {
            std::array<Operand,4> values;
            for(auto& value:values)if(!readOperand(vm,value)) {error="IS_POINT_ON_SCREEN requires a sphere.";return -1;}
            std::array<float,3> point{operandFloat(values[0]),operandFloat(values[1]),operandFloat(values[2])};
            const float radius=operandFloat(values[3]);
            if(!std::isfinite(point[0])||!std::isfinite(point[1])||!std::isfinite(point[2])||
                !std::isfinite(radius)||radius<0||!world.sphereOnScreen) {error="Screen visibility needs a finite sphere and the active camera.";return -1;}
            if(point[2]==-100||point[2]<=-250) {
                if(!world.groundHeightAt) {error="Screen visibility needs ground collision for the authored height sentinel.";return -1;}
                float height=0;world.groundHeightAt({point[0],point[1],1000},height);point[2]=height;
            }
            updateScmCondition(vm,world.sphereOnScreen(point,radius),inverted);
        } else if(command==0x0524||command==0x0525) {
            Operand value;
            if(!readOperand(vm,value)||(command==0x0524&&!value.reference)) {error="Vehicle camera zoom operand is invalid.";return -1;}
            if(command==0x0524) {if(!writeOperand(vm,value,world.cameraVehicleZoom))return -1;}
            else {
                if(value.bits<0||value.bits>3) {error="Vehicle camera zoom is outside the native 0..3 range.";return -1;}
                world.cameraVehicleZoom=value.bits;
            }
        } else {
            std::array<Operand,3> outputs;
            for(auto& output:outputs)if(!readOperand(vm,output)||!output.reference) {error="Active camera query requires three writable outputs.";return -1;}
            std::array<float,3> eye{},target{};
            if(!world.activeCameraVectors||!world.activeCameraVectors(eye,target)) {error="Active camera vectors are unavailable.";return -1;}
            const auto& source=command==0x0470?eye:target;
            for(size_t index=0;index<3;++index) {
                int32_t bits;std::memcpy(&bits,&source[index],4);
                if(!writeOperand(vm,outputs[index],bits))return -1;
            }
        }
        return 1;
    }
    if(vm.vcs&&(command==0x00B3||command==0x00B4||command==0x00B5)) {
        Operand handle,mode,style;
        if(!vm.environment||!readOperand(vm,handle)||!readOperand(vm,mode)||!readOperand(vm,style)) {
            error="Entity camera requires a target, mode, and switch style.";return -1;
        }
        auto& world=*vm.environment;
        if(command==0x00B4) {
            const auto found=world.missionVehicles.find(handle.bits);
            if(found==world.missionVehicles.end()||found->second.deleted)return 1;
        } else if(command==0x00B5&&handle.bits!=0) {
            const auto found=world.missionCharacters.find(handle.bits);
            if(found==world.missionCharacters.end())return 1;
        }
        const int32_t controller=int16_t(mode.bits),switchStyle=int16_t(style.bits);
        const bool vehicleController=command==0x00B4&&(controller==3||controller==14);
        if(controller!=15&&!vehicleController) {
            error="Entity camera has no verified controller for this target and mode.";return -1;
        }
        if(switchStyle!=1&&switchStyle!=2) {
            error="Entity camera requires native transition style 1 or instant style 2.";return -1;
        }
        if(vehicleController&&!StorylandVcsVehicleCamera::validate(world,handle.bits,controller,error))return -1;
        const int32_t previousMode=world.activeCameraMode?world.activeCameraMode():
            world.camera.active?(world.cameraFocus.kind?world.cameraFocus.mode:15):4;
        if(controller!=14&&(!world.camera.positionSet||controller==3)) {
            std::array<float,3> eye{},target{};
            if(!world.activeCameraVectors||!world.activeCameraVectors(eye,target)) {error="Entity camera needs an authored or active lens position.";return -1;}
            world.camera.x=eye[0];world.camera.y=eye[1];world.camera.z=eye[2];world.camera.positionSet=true;
        }
        world.cameraFocus.kind=command==0x00B3?1:command==0x00B5?2:3;
        world.cameraFocus.handle=handle.bits;world.cameraFocus.mode=controller;
        world.cameraFocus.switchStyle=switchStyle;
        world.camera.active=true;world.camera.restoreRequested=false;world.camera.cutsceneOwned=false;
        world.camera.mode=switchStyle;world.camera.fieldOfView=70;world.camera.roll=0;
        world.camera.vehicleBetaSpeed=0;world.camera.vehicleAligning=false;world.camera.vehicleUpdated=false;
        if(switchStyle==1&&!world.camera.interpolationScripted) {
            world.camera.interpolationMilliseconds=1350;
            world.camera.interpolationWeight=previousMode==15?.05f:.25f;
        }
        world.camera.transitioning=switchStyle==1&&world.camera.interpolationMilliseconds>0;
        world.camera.transitionPending=world.camera.transitioning;
        return 1;
    }
    if(vm.vcs&&command==0x0003) {
        Operand magnitude;
        if(!vm.environment||!readOperand(vm,magnitude)) {error="SHAKE_CAM requires an integer strength.";return -1;}
        auto& world=*vm.environment;
        auto& shake=world.cameraShake;
        // SLUS 0x287730 converts the integer with 0.001f. The native setter
        // (0x376988) and renderer intentionally use different decay rates.
        const float requested=float(magnitude.bits)*0.001f;
        const float elapsed=float(uint32_t(world.gameMilliseconds-shake.startedAt));
        const float previous=std::clamp(shake.strength-elapsed*0.001f,0.0f,2.0f);
        if(previous<requested) {
            shake.strength=requested;shake.startedAt=world.gameMilliseconds;shake.sampled=false;
        }
        return 1;
    }
    if(vm.vcs&&command==0x04D6) {
        Operand player,enabled;
        if(!vm.environment||!readOperand(vm,player)||!readOperand(vm,enabled)||player.bits!=0) {
            error="SET_PLAYER_FORCE_MANUAL_AIM requires the live player zero.";return -1;
        }
        auto& world=*vm.environment;
        if(world.inVehicle||world.activeVcsCharacterAttachment(0)) {
            error="Forced manual aim requires an unattached player on foot.";return -1;
        }
        if(!enabled.bits) {
            const bool resetRequested=world.manualAim.resetRequested;
            world.manualAim={};world.manualAim.resetRequested=resetRequested;return 1;
        }
        const int32_t weapon=world.currentWeapon;
        const int32_t mode=weapon==30||weapon==31?7:weapon==32?8:weapon==38?46:weapon==39?47:11;
        auto& aim=world.manualAim;
        if(aim.forced&&aim.mode==mode)return 1;
        const bool resetRequested=aim.resetRequested;
        aim={};aim.resetRequested=resetRequested;aim.forced=true;aim.mode=mode;
        return 1;
    }
    if(!vm.vcs) {
        switch(command) {
        case 0x015F:command=0x00B6;break;
        case 0x02F0:command=0x01CF;break;
        case 0x0164:command=0x00B9;break;
        case 0x0165:command=0x00BA;break;
        default:return 0;
        }
    }
    const bool position=command==0x00B9,target=command==0x00BA;
    const bool restore=command==0x00B6||command==0x01CF;
    if(!position&&!target&&!restore)return 0;
    if(!vm.environment) {error="Camera command requires a live environment.";return -1;}
    auto& camera=vm.environment->camera;
    if(restore) {
        camera.cutsceneOwned=false;camera.fieldOfView=70;camera.roll=0;
        camera.active=false;camera.restoreRequested=true;vm.environment->cameraFocus={};
        vm.environment->cameraLookAt={};
        camera.interpolationScripted=false;camera.transitioning=false;camera.transitionPending=false;
        vm.environment->cameraZoom.ped=false;
        vm.environment->cameraZoom.vehicle=false;
        return 1;
    }
    std::array<Operand,6> values{};
    const size_t count=position?6:4;
    for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="Camera command has a truncated typed operand.";return -1;
    }
    const auto real=[&](size_t index){return operandFloat(values[index]);};
    for(size_t index=0;index<(position?6:3);++index)if(!std::isfinite(real(index))) {
        error="Scripted camera vectors must be finite.";return -1;
    }
    if(target&&(values[3].bits<0||values[3].bits>2)) {error="Camera switch style is outside the native 0..2 range.";return -1;}
    camera.cutsceneOwned=false;camera.fieldOfView=70;camera.roll=0;
    if(position) {
        camera.x=real(0);camera.y=real(1);camera.z=real(2);
        camera.upX=real(3);camera.upY=real(4);camera.upZ=real(5);
        camera.positionSet=true;
    } else {
        vm.environment->cameraFocus={};
        camera.targetX=real(0);camera.targetY=real(1);camera.targetZ=real(2);
        camera.mode=values[3].bits;camera.targetSet=true;camera.active=true;
        camera.restoreRequested=false;
        camera.transitioning=camera.interpolationScripted&&camera.interpolationMilliseconds>0&&camera.mode==1;
        camera.transitionPending=camera.transitioning;
    }
    return 1;
}

int executeVcsEmpireStatisticsCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    if(!vm.vcs)return 0;
    const bool attackOrDefense=command>=0x050A&&command<=0x050D;
    const bool money=command>=0x0550&&command<=0x0553;
    const bool businessCount=command==0x04E5;
    const bool completedJob=command==0x0546;
    if(!attackOrDefense&&!money&&!businessCount&&!completedJob)return 0;
    if(!vm.environment) {
        error="Empire statistics require a live environment.";
        return -1;
    }
    std::array<Operand,2> values{};
    const size_t count=attackOrDefense?0:businessCount?2:1;
    for(size_t index=0;index<count;++index) {
        if(!readOperand(vm,values[index])) {
            error="Empire statistics command has a truncated operand.";
            return -1;
        }
    }
    auto& statistics=vm.environment->statistics;
    std::string key;
    uint32_t amount=1;
    if(businessCount||completedJob) {
        const int32_t business=values[0].bits;
        if(business<1||business>6) {
            error="Empire business type must be from 1 through 6.";
            return -1;
        }
        key=businessCount?"EmpireBusinessesOwnedType":"EmpireJobsCompletedType";
        key+=std::to_string(business);
        if(businessCount) {
            statistics[key]=uint8_t(values[1].bits);
            return 1;
        }
    } else if(attackOrDefense) {
        static constexpr const char* keys[]={
            "EmpireAttacksSuccessful","EmpireAttacksUnsuccessful",
            "EmpireDefensesSuccessful","EmpireDefensesUnsuccessful"
        };
        key=keys[command-0x050A];
    } else {
        static constexpr const char* keys[]={
            "EmpireMoneySpentOnDevelopment","EmpireMoneySpentOnRepair",
            "EmpireLargestBuildingEarning","EmpireMoneyEarnedFromBuildings"
        };
        key=keys[command-0x0550];
        amount=uint32_t(values[0].bits);
    }
    // Native counters are 32-bit words; the record-earning comparison is unsigned.
    const auto found=statistics.find(key);
    const double stored=found==statistics.end()?0.0:found->second;
    if(!std::isfinite(stored)||stored<0.0||stored>double(UINT32_MAX)) {
        error="Empire statistic is outside its native unsigned 32-bit range.";
        return -1;
    }
    const uint32_t previous=uint32_t(stored);
    statistics[key]=command==0x0552?std::max(previous,amount):uint32_t(previous+amount);
    return 1;
}

int executeVcsStreamingCommand(StorylandMissionVm& vm, uint16_t command, bool inverted, std::string& error) {
    const bool animation = command >= 0x030B && command <= 0x030D;
    const bool special = command == 0x0159 || command == 0x015A || command == 0x01A7 || command == 0x01D1;
    if (!vm.vcs || (!animation && !special && command != 0x02CF && command != 0x03DC)) {
        return 0;
    }
    if (!vm.environment) {
        error = "Streaming command requires a live asset manager.";
        return -1;
    }
    auto& world = *vm.environment;
    if (command == 0x03DC) {
        bool idle = true;
        for (int32_t model : world.requestedModels) {
            if (!world.residentModels.count(model)) {
                idle = false;
            }
        }
        for (int32_t group : world.requestedAnimationGroups) {
            if (!world.animationGroupLoaded) {
                error = "Animation streaming status is unavailable.";
                return -1;
            }
            if (!world.animationGroupLoaded(group)) {
                idle = false;
            }
        }
        for (const auto& bank : world.requestedAnimationBanks) {
            if (!world.animationBankLoaded) {
                error = "Named animation streaming status is unavailable.";
                return -1;
            }
            if (!world.animationBankLoaded(bank)) {
                idle = false;
            }
        }
        updateScmCondition(vm, idle, inverted);
        return 1;
    }
    if (animation) {
        std::string name;
        if (!readVcsMissionText(vm, name)) {
            error = "Named animation streaming requires a typed bank name.";
            return -1;
        }
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char character) {
            return character >= 'A' && character <= 'Z' ? char(character + 32) : char(character);
        });
        if (command == 0x030C) {
            if (!world.animationBankLoaded) {
                error = "Named animation status has no live decoder binding.";
                return -1;
            }
            updateScmCondition(vm, world.animationBankLoaded(name), inverted);
            return 1;
        }
        if (!world.animationBankAvailable || !world.animationBankAvailable(name)) {
            error = "Animation bank is absent from the native animation manager: " + name;
            return -1;
        }
        if (command == 0x030B) {
            world.requestedAnimationBanks.insert(name);
        } else {
            if (!world.removeAnimationBank) {
                error = "Animation removal has no live decoder binding.";
                return -1;
            }
            // Native 0x3C8BC8 retains a loaded/requested bank while a queued
            // model needs it. The early return records no deferred intent.
            if (world.animationRemovalDeferred && world.animationRemovalDeferred(name)) {
                return 1;
            }
            world.requestedAnimationBanks.erase(name);
            world.removeAnimationBank(name);
        }
        return 1;
    }
    Operand operand;
    if (!readOperand(vm, operand)) {
        error = "Model streaming requires an integer model or slot.";
        return -1;
    }
    const int64_t expanded = command == 0x01D1 || command == 0x02CF ? operand.bits : int64_t(operand.bits) + 14;
    if (expanded < 0 || expanded > INT32_MAX) {
        error = "Special model slot is outside the native model table.";
        return -1;
    }
    const int32_t model = int32_t(expanded);
    if (command == 0x02CF) {
        if (!world.nativeModelAvailable) {
            error = "Model availability has no native IDE table binding.";
            return -1;
        }
        updateScmCondition(vm, world.nativeModelAvailable(model), inverted);
        return 1;
    }
    if (command == 0x015A) {
        updateScmCondition(vm, world.residentModels.count(model) != 0, inverted);
        return 1;
    }
    const uint8_t owner = vm.isMissionThread ? 2 : 128;
    if (command == 0x01A7) {
        const auto found = world.specialModelOwners.find(model);
        if (found != world.specialModelOwners.end()) {
            found->second &= uint8_t(~owner);
            if (!found->second) {
                world.specialModelOwners.erase(found);
                world.requestedModels.erase(model);
            }
        }
        return 1;
    }
    std::string name;
    if (!readVcsMissionText(vm, name)) {
        error = "Special model streaming requires a typed asset name.";
        return -1;
    }
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char character) {
        return character >= 'A' && character <= 'Z' ? char(character + 32) : char(character);
    });
    if (!world.validateSpecialModel || !world.validateSpecialModel(model, name, error)) {
        if (error.empty()) {
            error = "Special model has no native asset binding.";
        }
        return -1;
    }
    const auto previous = world.specialModelNames.find(model);
    if (previous == world.specialModelNames.end() || previous->second != name) {
        world.residentModels.erase(model);
    }
    world.specialModelNames[model] = name;
    world.specialModelOwners[model] |= owner;
    world.requestedModels.insert(model);
    return 1;
}

int executeVcsContinuousSoundCommand(StorylandMissionVm& vm, uint16_t command, std::string& error) {
    if (!vm.vcs || (command != 0x00DA && command != 0x00DB)) {
        return 0;
    }
    if (!vm.environment) {
        error = "Continuous sound requires a live audio entity pool.";
        return -1;
    }
    auto& world = *vm.environment;
    std::array<Operand, 5> values{};
    const unsigned count = command == 0x00DA ? 5 : 1;
    for (unsigned index = 0; index < count; ++index) {
        if (!readOperand(vm, values[index])) {
            error = "Continuous sound has truncated typed operands.";
            return -1;
        }
    }
    if (command == 0x00DB) {
        world.continuousSounds.erase(values[0].bits);
        return 1;
    }
    if (!values[4].reference) {
        error = "Continuous sound needs a writable handle output.";
        return -1;
    }
    StorylandMissionEnvironment::ContinuousSound sound;
    sound.id = uint16_t(values[3].bits);
    for (unsigned index = 0; index < 3; ++index) {
        sound.position[index] = operandFloat(values[index]);
        if (!std::isfinite(sound.position[index])) {
            error = "Continuous sound position must be finite.";
            return -1;
        }
    }
    if (!world.continuousSoundAvailable || !world.continuousSoundAvailable(sound.id)) {
        error = "Continuous sound has no verified native sample binding: " + std::to_string(sound.id);
        return -1;
    }
    if (world.continuousSounds.size() >= 250 || world.nextContinuousSoundHandle == INT32_MAX) {
        error = "Continuous audio entity pool is full.";
        return -1;
    }
    const int32_t handle = world.nextContinuousSoundHandle;
    if (!writeOperand(vm, values[4], handle)) {
        error = "Continuous sound handle output could not be written.";
        return -1;
    }
    ++world.nextContinuousSoundHandle;
    world.continuousSounds.emplace(handle, sound);
    return 1;
}

int executeVcsFrameTextCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    const bool display=command==0x0203||command==0x02B1||command==0x02B2||command==0x04F7;
    const bool style=(command>=0x0204&&command<=0x020A)||command==0x0266||command==0x054A;
    if(!vm.vcs||(!display&&!style))return 0;
    if(!vm.environment) {error="Frame text requires a live environment.";return -1;}
    auto& world=*vm.environment;
    if(display) {
        Operand x,y;std::string key;
        if(!readOperand(vm,x)||!readOperand(vm,y)||!readVcsMissionText(vm,key)) {
            error="Frame text requires coordinates and a GXT label.";return -1;
        }
        std::vector<int32_t> numbers;
        const unsigned count=command==0x02B1?1:command==0x02B2?2:0;
        for(unsigned index=0;index<count;++index) {
            Operand number;
            if(!readOperand(vm,number)) {error="Numbered frame text has truncated operands.";return -1;}
            numbers.push_back(number.bits);
        }
        const float px=operandFloat(x),py=operandFloat(y);
        if(!std::isfinite(px)||!std::isfinite(py)||double(px)<INT32_MIN||double(px)>INT32_MAX||
            double(py)<INT32_MIN||double(py)>INT32_MAX) {
            error="Frame text coordinates are outside the native signed integer range.";return -1;
        }
        // Native DISPLAY_TEXT consumes a full call even when its twenty slots
        // are occupied. It does not overwrite the last committed row.
        if(world.scriptFrameText.size()>=20)return 1;
        auto row=world.scriptFrameTextStyle;
        row.x=int32_t(px);row.y=int32_t(py);
        row.text=storylandMissionFormattedText(world,key,numbers);
        if(row.text.size()>63)row.text.resize(63);
        if(command==0x04F7)row.flags|=0x20;
        else row.flags&=uint8_t(~0x20u);
        world.scriptFrameText.push_back(std::move(row));
        world.scriptFrameTextStyle={};
        return 1;
    }
    const unsigned count=command==0x0204?2:command==0x0205?4:1;
    std::array<Operand,4> values{};
    for(unsigned index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="Frame text style has truncated operands.";return -1;
    }
    if(world.scriptFrameText.size()>=20) {
        error="Frame text style would address beyond the native twenty-row buffer.";return -1;
    }
    auto next=world.scriptFrameTextStyle;
    if(command==0x054A) {
        const uint8_t font=uint8_t(values[0].bits);
        if(font>=4) {error="Frame text font must address one of the four native styles.";return -1;}
        next.font=font;
    } else if(command==0x0204) {
        const float x=operandFloat(values[0]),y=operandFloat(values[1]);
        if(!std::isfinite(x)||!std::isfinite(y)||y<0) {
            error="Frame text scale must be finite with nonnegative vertical scale.";return -1;
        }
        next.scaleX=x;next.scaleY=y;
    } else if(command==0x0205) {
        for(unsigned channel=0;channel<4;++channel)next.color[channel]=uint8_t(values[channel].bits);
    } else if(command==0x0208) {
        const float wrap=operandFloat(values[0]);
        if(!std::isfinite(wrap)) {error="Frame text wrap must be finite.";return -1;}
        next.wrap=std::min(480.0f,wrap);
    } else {
        const uint8_t mask=command==0x0206?1:command==0x0207?2:command==0x0209?4:
            command==0x020A?16:64;
        if(values[0].bits)next.flags|=mask;
        else next.flags&=uint8_t(~mask);
    }
    world.scriptFrameTextStyle=std::move(next);
    return 1;
}

int executeVcsPanelCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    if(!vm.vcs||!((command>=0x044E&&command<=0x045D)||command==0x048B||command==0x048C))return 0;
    if(!vm.environment) {error="Script panels require a live environment.";return -1;}
    auto& world=*vm.environment;
    if(command==0x044F) {
        for(auto& panel:world.scriptPanels) {panel.visible=false;panel.items.clear();}
        return 1;
    }
    const bool create=command==0x0454||command==0x0455||command==0x048B;
    std::string key;
    if(create&&!readVcsMissionText(vm,key)) {error="Panel item requires a text or sprite name.";return -1;}
    unsigned count=0;
    switch(command) {
    case 0x044E:case 0x0458:case 0x0459:case 0x045A:count=2;break;
    case 0x0450:case 0x0451:case 0x045B:case 0x048C:count=3;break;
    case 0x0452:case 0x0453:case 0x0457:count=5;break;
    case 0x0454:count=4;break;
    case 0x0455:count=11;break;
    case 0x0456:case 0x045C:count=1;break;
    case 0x045D:case 0x048B:count=6;break;
    }
    std::array<Operand,11> values{};
    for(unsigned index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="Script panel command has truncated typed operands.";return -1;
    }
    const bool panelCommand=command<=0x0456;
    const int32_t handle=values[0].bits;
    if(panelCommand||create) {
        if(handle<0||size_t(handle)>=world.scriptPanels.size()) {
            error="Script panel index is outside the native ten-panel table.";return -1;
        }
        auto& panel=world.scriptPanels[size_t(handle)];
        if(create) {
            if(!values[count-1].reference) {error="Panel creation requires a writable item handle.";return -1;}
            if(world.nextScriptPanelItem<0||world.nextScriptPanelItem==INT32_MAX||panel.items.size()>=4096) {
                error="Script panel item pool is full.";return -1;
            }
            StorylandMissionEnvironment::ScriptPanelItem item;
            item.handle=world.nextScriptPanelItem;
            item.key=std::move(key);item.x=values[1].bits;item.y=values[2].bits;
            if(command==0x0455) {
                const unsigned numbers=unsigned(std::clamp(int(int8_t(values[3].bits)),0,6));
                for(unsigned index=0;index<numbers;++index)item.numbers.push_back(values[4+index].bits);
            } else if(command==0x048B) {
                item.sprite=true;item.width=int16_t(values[3].bits);item.height=int16_t(values[4].bits);
            }
            if(!writeOperand(vm,values[count-1],item.handle)) {error="Panel item output is invalid.";return -1;}
            panel.items.push_back(std::move(item));
            ++world.nextScriptPanelItem;
            return 1;
        }
        switch(command) {
        case 0x044E:panel.visible=values[1].bits!=0;break;
        case 0x0450:panel.x=values[1].bits;panel.y=values[2].bits;break;
        case 0x0451:panel.width=values[1].bits;panel.height=values[2].bits;break;
        case 0x0452:case 0x0453: {
            auto& color=command==0x0452?panel.borderColor:panel.backgroundColor;
            for(unsigned channel=0;channel<4;++channel)color[channel]=uint8_t(values[channel+1].bits);
            break;
        }
        case 0x0456:panel.items.clear();break;
        }
        return 1;
    }
    for(auto& panel:world.scriptPanels) {
        auto found=std::find_if(panel.items.begin(),panel.items.end(),[&](const auto& item){return item.handle==handle;});
        if(found==panel.items.end())continue;
        auto& item=*found;
        switch(command) {
        case 0x0457:
            for(unsigned channel=0;channel<4;++channel)item.color[channel]=uint8_t(values[channel+1].bits);
            break;
        case 0x0458: {
            const float scale=operandFloat(values[1]);
            if(!std::isfinite(scale)||scale<0) {error="Panel item scale must be finite and nonnegative.";return -1;}
            item.scale=scale;break;
        }
        case 0x0459:
            if(values[1].bits<0||values[1].bits>=4) {error="Panel text font is outside the native four-font table.";return -1;}
            item.font=values[1].bits;break;
        case 0x045A:item.alignment=values[1].bits;break;
        case 0x045B:item.x=values[1].bits;item.y=values[2].bits;break;
        case 0x045C:panel.items.erase(found);break;
        case 0x045D:
            item.highlight=int8_t(values[1].bits);
            for(unsigned channel=0;channel<4;++channel)item.highlightColor[channel]=uint8_t(values[channel+2].bits);
            break;
        case 0x048C:item.width=int16_t(values[1].bits);item.height=int16_t(values[2].bits);break;
        }
        return 1;
    }
    error="Script panel command references a missing item handle.";return -1;
}

int executeVcsPrintCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    if(!vm.vcs)return 0;
    unsigned numbers=0;
    bool big=false,queued=false,soon=false,immediate=false,substitution=false;
    bool addToBrief=true;
    switch(command) {
    case 0x0055:big=true;break;
    case 0x0056:queued=true;break;
    case 0x0057:immediate=true;break;
    case 0x0058:soon=true;break;
    case 0x011D:numbers=1;big=true;break;
    case 0x011E:numbers=1;queued=true;break;
    case 0x011F:numbers=1;immediate=true;break;
    case 0x0120:numbers=1;soon=true;break;
    case 0x0143:big=true;queued=true;break;
    case 0x0144:numbers=1;big=true;queued=true;break;
    case 0x01D8:numbers=2;queued=true;break;
    case 0x01D9:numbers=2;immediate=true;break;
    case 0x01DA:numbers=2;soon=true;break;
    case 0x01DB:numbers=3;queued=true;break;
    case 0x01DC:numbers=3;immediate=true;break;
    case 0x01DD:numbers=4;queued=true;break;
    case 0x01DE:numbers=6;queued=true;break;
    case 0x021E:numbers=2;big=true;break;
    case 0x021F:numbers=3;big=true;break;
    case 0x0226:substitution=true;immediate=true;break;
    case 0x03EA:case 0x03EB:immediate=true;break;
    case 0x03EC:immediate=true;addToBrief=false;break;
    case 0x03ED:numbers=1;immediate=true;addToBrief=false;break;
    case 0x03F4:numbers=2;immediate=true;addToBrief=false;break;
    case 0x0059:case 0x025D:case 0x025E:case 0x026C:case 0x03C2:case 0x040F:break;
    default:return 0;
    }
    if(!vm.environment) {error="Mission messages require a live environment.";return -1;}
    auto& world=*vm.environment;
    const auto rememberBrief=[&](const StorylandMissionEnvironment::Message& message) {
        if(!message.addToBrief||(message.style&&message.style!=8))return;
        auto& brief=world.briefMessages;
        brief.erase(std::remove_if(brief.begin(),brief.end(),[&](const auto& previous) {
            return previous.key==message.key&&previous.stringKey==message.stringKey;
        }),brief.end());
        brief.push_front(message);if(brief.size()>20)brief.pop_back();
    };
    const auto mirrorSubtitle=[&]() {
        world.messageStyle=0;
        if(world.subtitleQueue.empty()) {
            world.messageKey.clear();world.messageNumbers.clear();world.messageStringKey.clear();world.messageMilliseconds=0;
        } else {
            const auto& first=world.subtitleQueue.front();
            world.messageKey=first.key;world.messageNumbers=first.numbers;
            world.messageStringKey=first.stringKey;world.messageMilliseconds=first.remaining;
        }
    };
    if(command==0x0059||command==0x026C||command==0x03C2) {
        if(command!=0x03C2) {world.subtitleQueue.clear();mirrorSubtitle();}
        if(command!=0x026C)for(auto& queue:world.bigMessageQueues)queue.clear();
        return 1;
    }
    std::string key;
    if(!readVcsMissionText(vm,key)) {error="Mission message has a truncated GXT label.";return -1;}
    if(command==0x040F) {
        Operand output;
        if(!readOperand(vm,output)||!output.reference) {error="Text length requires a writable output.";return -1;}
        const auto text=storylandMissionFormattedText(world,key);
        return writeOperand(vm,output,int32_t(std::min(text.size(),size_t(INT32_MAX))))?1:-1;
    }
    if(command==0x025D||command==0x025E) {
        const auto clear=[&](auto& queue) {
            const bool removesFront=!queue.empty()&&queue.front().key==key;
            queue.erase(std::remove_if(queue.begin(),queue.end(),[&](const auto& message){return message.key==key;}),queue.end());
            if(removesFront&&!queue.empty()) {queue.front().remaining=queue.front().duration;rememberBrief(queue.front());}
        };
        if(command==0x025D) {clear(world.subtitleQueue);mirrorSubtitle();}
        else for(auto& queue:world.bigMessageQueues)clear(queue);
        return 1;
    }
    StorylandMissionEnvironment::Message message;
    message.key=std::move(key);message.addToBrief=addToBrief;
    if(substitution&&!readVcsMissionText(vm,message.stringKey)) {error="Substituted message requires a second GXT label.";return -1;}
    for(unsigned index=0;index<numbers;++index) {
        Operand number;
        if(!readOperand(vm,number)) {error="Numbered mission message is truncated.";return -1;}
        message.numbers.push_back(number.bits);
    }
    Operand duration,style;
    if(!readOperand(vm,duration)||!readOperand(vm,style)) {error="Mission message needs duration and style/flags.";return -1;}
    message.duration=uint32_t(duration.bits);
    if(big) {
        const uint16_t nativeStyle=uint16_t(style.bits);
        if(nativeStyle<1||nativeStyle>8) {error="Big-message style is outside the native eight-style table.";return -1;}
        message.style=nativeStyle;
        if(nativeStyle==1||nativeStyle==2||nativeStyle==7)message.duration=4900;
    } else message.flags=uint16_t(style.bits);
    message.remaining=message.duration;
    if((command==0x03EA||command==0x03EB)&&!world.missionAudioSubtitleEnabled) {
        rememberBrief(message);return 1;
    }
    auto& queue=big?world.bigMessageQueues[size_t(message.style-1)]:world.subtitleQueue;
    const size_t limit=big?4:8;
    const bool startsNow=queue.empty()||immediate||(!queued&&!soon);
    if(startsNow) {
        if(queue.empty())queue.push_back(message);
        else queue.front()=message;
    } else if(soon) {
        queue.insert(queue.begin()+1,message);
        if(queue.size()>limit)queue.pop_back();
    } else if(queue.size()<limit)queue.push_back(message);
    if(startsNow)rememberBrief(message);
    if(big)vm.observeMissionMessage(message.key,message.style);
    else mirrorSubtitle();
    return 1;
}

int executeVcsTextAndStatsCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(const int result=executeVcsStreamingCommand(vm,command,inverted,error))return result;
    if(const int result=executeVcsContinuousSoundCommand(vm,command,error))return result;
    if(const int result=executeVcsFrameTextCommand(vm,command,error))return result;
    if(const int result=executeVcsPanelCommand(vm,command,error))return result;
    if(const int result=executeVcsPrintCommand(vm,command,error))return result;
    if(!vm.vcs) {
        if(command==0x03DA)command=0x025D;
        else if(command==0x0591)command=0x0362;
        else if(command==0x01BA)command=0x0108;
        else if(command==0x01BB)command=0x0109;
        else if(command==0x01BC)command=0x010A;
        else return 0;
    }
    if(command==0x025D) {
        std::string label;
        if(!vm.environment||!(vm.vcs?readVcsMissionText(vm,label):readScmText(vm,label))) {error="CLEAR_THIS_PRINT requires a text label.";return -1;}
        auto& world=*vm.environment;
        if(world.messageKey==label) {world.messageKey.clear();world.messageMilliseconds=0;}
        return 1;
    }
    const bool format=command==0x040D;
    const bool help=command==0x0267||command==0x0323||command==0x03D3||command==0x03D4||
        command==0x0406||command==0x0407||command==0x0408||command==0x0409;
    const bool progress=command==0x01DF||command==0x0362;
    const bool passed=command==0x01EB,oddjob=command==0x036A,developer=command==0x03E5;
    const bool radio=command==0x021D;
    const bool weather=command==0x0108||command==0x0109||command==0x010A;
    if(!format&&!help&&!progress&&!passed&&!oddjob&&!developer&&!radio&&!weather)return 0;
    if(!vm.environment) {error="Text/stats command requires a live environment.";return -1;}
    auto& world=*vm.environment;
    if(developer) {updateScmCondition(vm,world.developerMode,inverted);return 1;}
    if(oddjob) {vm.registerMissionPassed({},true);return 1;}
    if(help||passed) {
        std::string label;
        if(!readVcsMissionText(vm,label)) {error="Mission text key is truncated or invalid.";return -1;}
        if(passed) {
            vm.registerMissionPassed(label);
        }
        else {
            const bool newHelp=world.helpMessage!=label||world.helpMessageMilliseconds==0;
            world.helpMessage=std::move(label);
            if(newHelp&&world.audioSubsystemEnabled&&world.frontendAudioEnabled&&world.scriptSounds.size()<4096)
                world.scriptSounds.push_back({197,{world.x,world.y,world.z},true});
            world.helpMessageNumbers.clear();
            world.helpMessagePersistent=command==0x0323||command==0x03D4||command==0x0407||command==0x0409;
            world.helpMessageMilliseconds=7000;
        }
        return 1;
    }
    if(weather&&command==0x010A) {world.forcedWeather=-1;return 1;}
    const size_t count=format?4:1;
    std::array<Operand,4> values{};
    for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="Text/stats command has an invalid typed operand.";return -1;
    }
    if(format) {
        const int32_t width=values[2].bits,start=values[3].bits,number=values[1].bits;
        if(!values[0].reference||width<=0||width>16||start<0||start>127||start+width>128) {
            error="Integer-to-label insertion has an invalid destination, width or byte offset.";return -1;
        }
        std::string digits=std::to_string(number<0?-int64_t(number):int64_t(number));
        const size_t sign=number<0?1u:0u;
        if(digits.size()+sign>size_t(width)) {
            error="Integer does not fit the requested text-label field.";return -1;
        }
        digits=std::string(size_t(width)-digits.size()-sign,'0')+digits;
        if(number<0)digits.insert(digits.begin(),'-');
        auto& strings=values[0].global?vm.globalStrings:vm.localStrings;
        auto& destination=strings[values[0].variable];
        if(destination.size()<size_t(start+width))destination.resize(size_t(start+width),' ');
        destination.replace(size_t(start),size_t(width),digits);
        return 1;
    }
    if(radio) {world.radioEnabled=values[0].bits!=0;world.radioChanged=true;return 1;}
    if(weather) {
        if(values[0].bits<0||values[0].bits>7) {error="Weather index exceeds the loaded native timecycle.";return -1;}
        world.forcedWeather = values[0].bits;
        world.weatherChanged = true;
        world.weatherRestoreRequested = false;
        world.weatherState.previous = int16_t(values[0].bits);
        world.weatherState.current = int16_t(values[0].bits);
        world.weatherState.interpolation = 0;
        world.weatherState.rain = values[0].bits == 2 || values[0].bits == 5 ? 1.0f : 0.0f;
        return 1;
    }
    if(command==0x01DF) {
        world.progressMade=int32_t(std::clamp(int64_t(world.progressMade)+values[0].bits,int64_t(0),int64_t(INT32_MAX)));
        return 1;
    }
    const float percentage=world.progressTotal>0?
        std::clamp(100.0f*float(world.progressMade)/float(world.progressTotal),0.0f,100.0f):0.0f;
    int32_t bits=0;std::memcpy(&bits,&percentage,sizeof(bits));
    if(!values[0].reference||!writeOperand(vm,values[0],bits)) {
        error="GET_PROGRESS_PERCENTAGE requires a writable float output.";return -1;
    }
    return 1;
}

int executeVcsBitwiseCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(vm.vcs&&(command==0x003C||command==0x003D)) {
        Operand output;
        if(!vm.environment||!readOperand(vm,output)||!output.reference) {
            error="Native random generator requires a shared world and writable output.";return -1;
        }
        uint32_t random=vm.environment->nextNativeRandom();
        // Native100010 discards three draws before using the fourth low16.
        if(command==0x003C)for(unsigned draw=1;draw<4;++draw)random=vm.environment->nextNativeRandom();
        int32_t result=int32_t(random&0xFFFFu);
        if(command==0x003C) {
            const float value=float(result)*(1.0f/65536.0f);std::memcpy(&result,&value,4);
        }
        if(!writeOperand(vm,output,result)) {error="Native random output is invalid.";return -1;}
        return 1;
    }
    if(vm.vcs&&(command==0x0138||command==0x0372||command==0x0373)) {
        // Native100138/100628/1006C8 accept2/4/6 float inputs and one output.
        const size_t inputs=command==0x0138?2:command==0x0372?4:6;
        std::array<Operand,7> values{};
        for(size_t index=0;index<=inputs;++index)if(!readOperand(vm,values[index])) {
            error="Float command has a truncated typed operand.";return -1;
        }
        if(!values[inputs].reference) {error="Float command requires a writable output.";return -1;}
        for(size_t index=0;index<inputs;++index)if(!std::isfinite(operandFloat(values[index]))) {
            error="Float command requires finite inputs.";return -1;
        }
        float result=0;
        uint32_t nextRandom=0;
        const auto extended=[](std::string& message) {
            message="Float command requires unsupported extended EE exponent255 behavior; destination and random state unchanged.";
            return -1;
        };
        if(command==0x0138) {
            if(!vm.environment) {error="Native random range requires its shared world.";return -1;}
            nextRandom=vm.environment->previewNativeRandom();
            const float fraction=roundEeFloat(double(roundEeFloat(nextRandom))*(1.0/2147483648.0));
            const float low=flushEeFloat(operandFloat(values[0])),high=flushEeFloat(operandFloat(values[1]));
            float width=0;
            if(!tryTimedEeArithmetic(high,low,1,true,width)||
                !tryTimedEeArithmetic(low,width,fraction,false,result))return extended(error);
        } else {
            const size_t dimensions=inputs/2;
            for(size_t index=0;index<dimensions;++index) {
                float product=0;
                if(!tryRoundFiniteEeFloat(double(flushEeFloat(operandFloat(values[index])))*
                    flushEeFloat(operandFloat(values[index+dimensions])),product))return extended(error);
                if(index) {
                    if(!tryTimedEeArithmetic(result,product,1,false,result))return extended(error);
                } else result=product;
            }
        }
        int32_t bits;std::memcpy(&bits,&result,sizeof(bits));
        if(!std::isfinite(result)||!writeOperand(vm,values[inputs],bits)) {
            error="Float command result is invalid.";return -1;
        }
        if(command==0x0138)vm.environment->nativeRandomState=nextRandom;
        return 1;
    }
    if(!vm.vcs||(command!=0x048D&&command!=0x048E&&command!=0x048F))return 0;
    std::array<Operand,3> values{};
    const size_t count=command==0x048F?2:3;
    for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="Bitwise command has a truncated typed operand.";return -1;
    }
    const uint32_t first=uint32_t(values[0].bits),second=uint32_t(values[1].bits);
    if(command==0x048F) {updateScmCondition(vm,(first&second)!=0,inverted);return 1;}
    const int32_t result=int32_t(command==0x048D?first|second:second&~first);
    if(!values[2].reference||!writeOperand(vm,values[2],result)) {
        error="Bitwise command requires a writable integer output.";return -1;
    }
    return 1;
}

int executeVcsRecordStatisticsCommand(StorylandMissionVm &vm, uint16_t command, std::string &error) {
    // SLUS_215.90 statistic handlers 184AC8..1865D0 and their 28A298..28B01C
    // callees update the same records displayed by the Statistics panel. The
    // existing STAT map persists these values; absent entries use native Init
    // defaults from 289C20, including INT_MAX for scalar time records.
    enum class Operation {
        MaximumSigned,
        MinimumSigned,
        FirstOrMinimumSigned,
        Increment,
        AddSigned,
        MaximumFloat,
        MaximumTenths,
        AddSignedToFloat,
        IncrementUniqueJumps,
        DrivingRange
    };
    struct Route {
        uint16_t opcode;
        const char *key;
        Operation operation;
        uint8_t courses = 0;
    };
    static constexpr Route routes[] = {
        {0x01E1, "MaximumJumpDistance", Operation::MaximumFloat},
        {0x01E2, "MaximumJumpHeight", Operation::MaximumFloat},
        {0x01E3, "MaximumJumpFlips", Operation::MaximumSigned},
        {0x01E4, "MaximumJumpSpins", Operation::MaximumSigned},
        {0x01E5, "BestStuntJump", Operation::MaximumSigned},
        {0x01E6, "NumberOfUniqueJumpsFound", Operation::IncrementUniqueJumps},
        {0x01E9, "MoneyMadeWithTaxi", Operation::AddSigned},
        {0x027A, "LivesSavedWithAmbulance", Operation::Increment},
        {0x027B, "CriminalsCaught", Operation::Increment},
        {0x027C, "HighestLevelAmbulanceMission", Operation::MaximumSigned},
        {0x027D, "FiresExtinguished", Operation::Increment},
        {0x032E, "WeaponBudget", Operation::AddSignedToFloat},
        {0x036E, "HighestLevelFireMission", Operation::MaximumSigned},
        {0x03B0, "TimesOutfitChanged", Operation::Increment},
        {0x03DF, "LongestFacePlantDistance", Operation::MaximumFloat},
        {0x04EA, "ShopliftersApprehended", Operation::Increment},
        {0x04F4, "DrivingRangeBestScore", Operation::DrivingRange},
        {0x050E, "CrashMissionMostDamage", Operation::MaximumSigned},
        {0x050F, "MashingUpTheMallFastestTime", Operation::MinimumSigned},
        {0x0510, "HarborHoverRaceFastestTime", Operation::MinimumSigned},
        {0x0511, "HaitiHoverRaceFastestTime", Operation::MinimumSigned},
        {0x0512, "CrimewaveValueOfGoodsSaved", Operation::MaximumSigned},
        {0x0513, "CrimewaveHighestLevel", Operation::MaximumSigned},
        {0x0514, "AirAmbulanceHighestLevel", Operation::MaximumSigned},
        {0x0515, "ViceSightsHighestLevel", Operation::MaximumSigned},
        {0x0516, "ViceSightsViewed", Operation::MaximumSigned},
        {0x0517, "FireCopterHighestLevel", Operation::MaximumSigned},
        {0x0518, "BeachPatrolHighestLevel", Operation::MaximumSigned},
        {0x0519, "CrimsOnWingsFastestTime", Operation::MinimumSigned},
        {0x051A, "PCJPlayTownFastestTime", Operation::MinimumSigned},
        {0x051B, "PCJPlayPointFastestTime", Operation::MinimumSigned},
        {0x051C, "WatersportsFastestTime", Operation::FirstOrMinimumSigned, 8},
        {0x051D, "WatersportsBestLap", Operation::FirstOrMinimumSigned, 8},
        {0x051E, "DTrialFastestTime", Operation::FirstOrMinimumSigned, 12},
        {0x051F, "DTrialBestLap", Operation::FirstOrMinimumSigned, 12},
        {0x0520, "TurismoFastestTime", Operation::FirstOrMinimumSigned, 9},
        {0x0521, "TurismoBestLap", Operation::FirstOrMinimumSigned, 9},
        {0x0522, "TurismoBestPosition", Operation::MinimumSigned, 9},
        {0x052B, "MVRaceFastestTime", Operation::MinimumSigned},
        {0x052C, "SkywolfFastestTime", Operation::MinimumSigned},
        {0x0535, "DTrialFurthestHoop", Operation::MaximumTenths},
        {0x0536, "BTrialFurthestHoop", Operation::MaximumTenths},
        {0x0537, "BTrialFastestTime", Operation::FirstOrMinimumSigned, 8},
        {0x0538, "BTrialBestLap", Operation::FirstOrMinimumSigned, 8},
        {0x0539, "QTTrialFurthestHoop", Operation::MaximumTenths},
        {0x053A, "QTTrialFastestTime", Operation::FirstOrMinimumSigned, 4},
        {0x053B, "QTTrialBestLap", Operation::FirstOrMinimumSigned, 4},
        {0x053C, "ShootingRangeFastestTime", Operation::MinimumSigned},
        {0x053D, "ShootingRangeBestAccuracy", Operation::MaximumSigned},
        {0x053E, "ShootingRangeBestScore", Operation::MaximumSigned},
        {0x055B, "RushMostDamage", Operation::MaximumSigned},
        {0x055C, "AirRace2FastestTime", Operation::MinimumSigned},
        {0x055D, "DockPCJPlayFastestTime", Operation::MinimumSigned},
        {0x055E, "ParkPCJPlayFastestTime", Operation::MinimumSigned},
        {0x055F, "MVRace2FastestTime", Operation::MinimumSigned},
        {0x0560, "GTrialBestLap", Operation::FirstOrMinimumSigned},
        {0x0561, "GTrialFurthestHoop", Operation::MaximumTenths},
        {0x0562, "GTrialFastestTime", Operation::FirstOrMinimumSigned},
    };
    const Route *route = nullptr;
    for (const auto &candidate : routes)
        if (candidate.opcode == command) {
            route = &candidate;
            break;
        }
    if (!route)
        return 0;
    if (!vm.environment) {
        error = "Record statistics require a live environment.";
        return -1;
    }
    const Operation operation = route->operation;
    size_t count = 1;
    if (operation == Operation::Increment || operation == Operation::IncrementUniqueJumps)
        count = 0;
    else if (route->courses || operation == Operation::DrivingRange)
        count = 2;
    std::array<Operand, 2> values{};
    for (size_t index = 0; index < count; ++index)
        if (!readOperand(vm, values[index])) {
            error = "Record statistic has a truncated typed operand.";
            return -1;
        }
    std::string key = route->key;
    if (route->courses) {
        const int32_t course = values[0].bits;
        // Native callers supply indices into fixed arrays. Reject an address
        // outside that array rather than reproduce an adjacent-memory write.
        if (course < 0 || course >= route->courses) {
            error = "Statistic course index is outside its native array.";
            return -1;
        }
        key += "[" + std::to_string(course) + "]";
    }
    auto &world = *vm.environment;
    auto &statistics = world.statistics;
    const auto readSigned = [&](const std::string &name, int32_t initial, int32_t &value) {
        const auto found = statistics.find(name);
        const double stored = found == statistics.end() ? double(initial) : found->second;
        if (!std::isfinite(stored) || stored < INT32_MIN || stored > INT32_MAX ||
            std::trunc(stored) != stored)
            return false;
        value = int32_t(stored);
        return true;
    };
    const auto readFloat = [&](const std::string &name, float &value) {
        const auto found = statistics.find(name);
        const double stored = found == statistics.end() ? 0.0 : found->second;
        if (!std::isfinite(stored) || std::abs(stored) > double(std::numeric_limits<float>::max()))
            return false;
        value = float(stored);
        return double(value) == stored;
    };
    const auto maximumFloat = [](float left, float right) {
        // MAX.S selects by signed bit ordering: subnormal bit patterns remain
        // intact, and +0 wins over -0. It does not perform arithmetic flushing.
        if (left == 0.0f && right == 0.0f)
            return std::signbit(left) && std::signbit(right) ? -0.0f : 0.0f;
        return std::max(left, right);
    };
    const auto wrapAdd = [](int32_t left, int32_t right) {
        const uint32_t bits = uint32_t(left) + uint32_t(right);
        int32_t result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    };
    if (operation == Operation::IncrementUniqueJumps) {
        ++world.uniqueJumpsFound;
        int32_t signedValue;
        std::memcpy(&signedValue, &world.uniqueJumpsFound, sizeof(signedValue));
        statistics[key] = signedValue;
        return 1;
    }
    if (operation == Operation::MaximumFloat || operation == Operation::MaximumTenths ||
        operation == Operation::AddSignedToFloat || operation == Operation::DrivingRange) {
        const std::string floatKey = operation == Operation::DrivingRange ? "DrivingRangeFurthestDrive" : key;
        float previous = 0, result = 0;
        if (!readFloat(floatKey, previous)) {
            error = "Stored statistic is not a finite native float.";
            return -1;
        }
        if (operation == Operation::AddSignedToFloat) {
            const float increment = roundEeFloat(double(values[0].bits));
            if (!tryTimedEeArithmetic(previous, increment, 1.0f, false, result)) {
                error = "Weapon budget addition requires the unsupported extended EE float range.";
                return -1;
            }
        } else {
            float candidate = operandFloat(values[operation == Operation::DrivingRange ? 1 : 0]);
            if (!std::isfinite(candidate)) {
                error = "Record statistic requires the unsupported extended EE float range.";
                return -1;
            }
            if (operation == Operation::MaximumTenths) {
                float scaled = 0;
                if (!tryRoundFiniteEeFloat(double(flushEeFloat(candidate)) * 10.0, scaled)) {
                    error = "Hoop statistic multiplication requires the unsupported extended EE float range.";
                    return -1;
                }
                // The native pipeline is MUL.S10, CVT.W.S, CVT.S.W, MUL.S0.1.
                // CVT.W.S saturates signed overflow; each float step rounds
                // toward zero before the next step, rather than using floor.
                const int32_t tenths = double(scaled) >= 2147483648.0    ? INT32_MAX
                                       : double(scaled) <= -2147483648.0 ? INT32_MIN
                                                                         : int32_t(scaled);
                const float converted = roundEeFloat(double(tenths));
                if (!tryRoundFiniteEeFloat(double(converted) * double(0.1f), candidate)) {
                    error = "Hoop statistic conversion is outside the supported EE float range.";
                    return -1;
                }
            }
            result = maximumFloat(candidate, previous);
        }
        int32_t score = 0;
        if (operation == Operation::DrivingRange && !readSigned(key, 0, score)) {
            error = "Stored driving-range score is not a native signed word.";
            return -1;
        }
        // Both driving-range records commit only after all operands and stored
        // values pass validation; a rejected distance cannot change the score.
        if (operation == Operation::DrivingRange)
            statistics[key] = std::max(score, values[0].bits);
        statistics[floatKey] = double(result);
        return 1;
    }
    int32_t previous = 0;
    if (!readSigned(key, operation == Operation::MinimumSigned ? INT32_MAX : 0, previous)) {
        error = "Stored statistic is not a native signed word.";
        return -1;
    }
    const int32_t value = operation == Operation::Increment ? 1 : values[route->courses ? 1 : 0].bits;
    int32_t result = 0;
    switch (operation) {
    case Operation::MaximumSigned:
        result = std::max(previous, value);
        break;
    case Operation::MinimumSigned:
        result = std::min(previous, value);
        break;
    case Operation::FirstOrMinimumSigned:
        result = previous == 0 ? value : std::min(previous, value);
        break;
    case Operation::Increment:
    case Operation::AddSigned:
        result = wrapAdd(previous, value);
        break;
    default:
        error = "Unknown native statistic operation.";
        return -1;
    }
    statistics[key] = result;
    return 1;
}

int executeVcsIntroCommand(StorylandMissionVm &vm, uint16_t command, bool inverted, std::string &error) {
    if (vm.vcs) {
        const int statistic = executeVcsRecordStatisticsCommand(vm, command, error);
        if (statistic)
            return statistic;
        if (command == 0x0291) {
            // The US retail handler 391338 returns true without reading a
            // language preference. It still participates in SCM conditions.
            updateScmCondition(vm, true, inverted);
            return 1;
        }
        if (command >= 0x02E3 && command <= 0x02E6) {
            Operand left, right;
            if (!readOperand(vm, left) || !readOperand(vm, right)) {
                error = "Signed comparison has a truncated typed operand.";
                return -1;
            }
            // Both spellings share the same native handler and retain the
            // encoded operand order: E3/E4 use >, E5/E6 use >=.
            updateScmCondition(vm, command <= 0x02E4 ? left.bits > right.bits : left.bits >= right.bits,
                               inverted);
            return 1;
        }
        if (command == 0x0292 || command == 0x0297) {
            Operand input, output;
            if (!readOperand(vm, input) || !readOperand(vm, output) || !output.reference) {
                error = "Metres-to-feet conversion requires an input and writable output.";
                return -1;
            }
            const float source = command == 0x0297 ? roundEeFloat(double(input.bits)) : operandFloat(input);
            float converted = 0;
            // 100378/1003F0 multiply by raw 0x40555555 (10/3 rounded to
            // float), rather than the modern physical metres-to-feet factor.
            if (!std::isfinite(source) ||
                !tryRoundFiniteEeFloat(double(flushEeFloat(source)) * double(3.3333332538604736328125f),
                                       converted)) {
                error = "Metres-to-feet conversion requires the unsupported extended EE float range.";
                return -1;
            }
            int32_t result = 0;
            if (command == 0x0297)
                result = double(converted) >= 2147483648.0    ? INT32_MAX
                         : double(converted) <= -2147483648.0 ? INT32_MIN
                                                              : int32_t(converted);
            else
                std::memcpy(&result, &converted, sizeof(result));
            if (!writeOperand(vm, output, result)) {
                error = "Metres-to-feet output is not writable.";
                return -1;
            }
            return 1;
        }
    }
    if(!vm.vcs) {
        switch(command) {
        case 0x0573:command=0x034B;break;
        case 0x0319:command=0x01E7;break;
        case 0x05EF:command=0x03AC;break;
        case 0x0635:command=0x03DA;break;
        default:return 0;
        }
    }
    if(command==0x034A||command==0x034B) {
        Operand handle;
        if(!vm.environment||!readOperand(vm,handle)) {error="Entity existence query requires a handle.";return -1;}
        if(command==0x034A) {
            updateScmCondition(vm,handle.bits==0||vm.environment->missionCharacters.count(handle.bits)!=0,inverted);
            return 1;
        }
        const auto& vehicles=vm.environment->missionVehicles;
        const auto found=vehicles.find(handle.bits);
        updateScmCondition(vm,found!=vehicles.end()&&!found->second.deleted,inverted);
        return 1;
    }
    if(command==0x01A8) {
        if(!vm.environment) {error="Model kill reset requires a live environment.";return -1;}
        vm.environment->modelsKilledByPlayer.clear();return 1;
    }
    if(command==0x01A9) {
        Operand model,output;
        if(!vm.environment||!readOperand(vm,model)||!readOperand(vm,output)||model.bits<0) {
            error="Model kill count requires a model and writable output.";return -1;
        }
        const auto found=vm.environment->modelsKilledByPlayer.find(model.bits);
        const uint32_t count=found==vm.environment->modelsKilledByPlayer.end()?0:found->second;
        if(!writeOperand(vm,output,int32_t(std::min(count,uint32_t(INT32_MAX))))) {
            error="Model kill count has an invalid output.";return -1;
        }
        return 1;
    }
    if (command == 0x01E8) {
        if (!vm.environment) {
            error = "Taxi passenger completion requires mission statistics.";
            return -1;
        }
        ++vm.environment->taxiPassengersDroppedOff;
        vm.environment->statistics["PassengersDroppedOffWithTaxi"] =
            int32_t(vm.environment->taxiPassengersDroppedOff);
        return 1;
    }
    if(command==0x01F6) {
        std::array<Operand,6> values{};
        if(!vm.environment) {error="Vehicle area query requires the live vehicle pool.";return -1;}
        for(auto& value:values)if(!readOperand(vm,value)) {error="Vehicle area query has a truncated operand.";return -1;}
        if(!values[5].reference) {error="Vehicle area query requires a writable handle output.";return -1;}
        std::array<float,4> bounds{};
        for(size_t index=0;index<4;++index) {
            bounds[index]=operandFloat(values[index]);
            if(!std::isfinite(bounds[index])) {error="Vehicle area bounds must be finite.";return -1;}
        }
        auto& world=*vm.environment;
        const int32_t handle=world.acquireAmbientVehicle?world.acquireAmbientVehicle(bounds,values[4].bits):-1;
        if(!writeOperand(vm,values[5],handle)) {error="Vehicle area query requires a writable handle output.";return -1;}
        return 1;
    }
    if(command==0x02A4) {
        if(!vm.environment) {error="Vehicle cheat query requires a live environment.";return -1;}
        const auto& cheats=vm.environment->vehicleCheats;
        updateScmCondition(vm,std::any_of(cheats.begin(),cheats.end(),[](bool active){return active;}),inverted);
        return 1;
    }
    if (command == 0x01E7 || command == 0x03E0 || command == 0x03AC) {
        Operand value;
        if (!vm.environment || !readOperand(vm, value)) {
            error = "Mission statistic requires one integer operand.";
            return -1;
        }
        if (command == 0x01E7) {
            vm.environment->totalUniqueJumps = value.bits;
            vm.environment->statistics["TotalNumberOfUniqueJumps"] = value.bits;
        } else if (command == 0x03AC) {
            vm.environment->uniqueJumpsFound = uint32_t(value.bits);
            vm.environment->statistics["NumberOfUniqueJumpsFound"] = value.bits;
        } else
            vm.environment->exportedCars += uint32_t(value.bits);
        return 1;
    }
    if(command==0x010C) {
        Operand handle;std::array<Operand,3> outputs{};
        if(!vm.environment||!readOperand(vm,handle)) {error="Object coordinates require a live object handle.";return -1;}
        for(auto& output:outputs)if(!readOperand(vm,output)) {error="Object coordinates have a truncated output.";return -1;}
        for(const auto& output:outputs)if(!output.reference) {error="Object coordinates require writable outputs.";return -1;}
        const auto& objects=vm.environment->objects;
        const auto object=std::find_if(objects.begin(),objects.end(),[&](const auto& value){return value.handle==handle.bits;});
        if(object==objects.end()) {error="Object coordinates reference an unknown object.";return -1;}
        const std::array<float,3> coordinates{object->x,object->y,object->z};
        for(size_t axis=0;axis<3;++axis) {
            int32_t bits;std::memcpy(&bits,&coordinates[axis],sizeof(bits));
            if(!writeOperand(vm,outputs[axis],bits)) {error="Object coordinates require writable outputs.";return -1;}
        }
        return 1;
    }
    if(command==0x03B6) {
        Operand output;
        if(!vm.environment||!readOperand(vm,output)||!output.reference) {
            error="Camera mode query requires a writable output.";return -1;
        }
        if(!vm.environment->activeCameraMode) {
            error="Ped camera zoom query requires the active native camera mode.";return -1;
        }
        const int32_t indicator=vm.environment->activeCameraMode()==4?
            vm.environment->cameraSpecialMode:-1;
        if(!writeOperand(vm,output,indicator)) {error="Ped camera zoom output could not be written.";return -1;}
        return 1;
    }
    if(command==0x0318) {
        Operand player;std::string outfit;
        if(!vm.environment||!readOperand(vm,player)||player.bits!=0||!readVcsMissionText(vm,outfit)) {
            error="IS_PLAYER_WEARING needs the live player and an outfit name.";return -1;
        }
        const auto normalize=[](std::string value) {
            for(char& character:value)character=char(std::toupper(static_cast<unsigned char>(character)));
            return value;
        };
        updateScmCondition(vm,normalize(outfit)==normalize(vm.environment->playerOutfit),inverted);return 1;
    }
    if(command==0x0482||command==0x03DA) {
        Operand hash,state;
        if(!vm.environment||!readOperand(vm,hash)||!readOperand(vm,state)||state.bits<0||state.bits>255||
            (command==0x03DA&&(hash.bits<0||hash.bits>255))) {
            error="Building swap requires a valid hash or group and native byte state.";return -1;
        }
        if(command==0x0482)vm.environment->buildingSwapStates[uint32_t(hash.bits)]=state.bits;
        vm.environment->buildingSwapCommands.push_back({uint32_t(hash.bits),uint32_t(state.bits),command==0x03DA});
        ++vm.environment->buildingSwapRevision;return 1;
    }
    if(command==0x0252||command==0x0093) {
        Operand handle;
        if(!vm.environment||!readOperand(vm,handle)) {error="Object command needs a live object pool and handle.";return -1;}
        auto& objects=vm.environment->objects;
        const auto found=std::find_if(objects.begin(),objects.end(),[&](const auto& value){return value.handle==handle.bits;});
        if(command==0x0252)updateScmCondition(vm,found!=objects.end(),inverted);
        else if(found!=objects.end())objects.erase(found);
        return 1;
    }
    if(command==0x01D6) {
        Operand handle,type;
        if(!vm.environment||!readOperand(vm,handle)||!readOperand(vm,type)) {
            error="CHANGE_GARAGE_TYPE needs a garage handle and type.";return -1;
        }
        const auto garage=std::find_if(vm.environment->garages.begin(),vm.environment->garages.end(),
            [&](const auto& value){return value.handle==handle.bits;});
        if(garage==vm.environment->garages.end()) {error="CHANGE_GARAGE_TYPE references an unknown garage.";return -1;}
        garage->type=uint8_t(type.bits);garage->state=0;garage->targetVehicle=-1;return 1;
    }
    if(command==0x0312) {
        if(!vm.environment) {error="Set-piece registration requires a live environment.";return -1;}
        Operand type;
        StorylandMissionEnvironment::SetPiece entry;
        if(!readOperand(vm,type)) {error="Set-piece registration has no type.";return -1;}
        entry.type=type.bits;
        for(float& coordinate:entry.coordinates) {
            Operand value;
            if(!readOperand(vm,value)||!std::isfinite(coordinate=operandFloat(value))) {
                error="Set-piece registration has an invalid coordinate.";return -1;
            }
        }
        if(vm.environment->setPieces.size()>=256) {error="Set-piece registration pool is full.";return -1;}
        vm.environment->setPieces.push_back(entry);return 1;
    }
    if(command==0x030A||command==0x01F3) {
        if(!vm.environment) {error="Zone population requires a live environment.";return -1;}
        std::string zone;
        if(!readVcsMissionText(vm,zone)||zone.empty()) {error="Zone population has no valid zone name.";return -1;}
        const size_t count=command==0x030A?14:2;
        std::array<Operand,14> values{};
        for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
            error="Zone population has a truncated typed operand.";return -1;
        }
        if(values[0].bits<0||values[0].bits>1) {error="Zone day/night index must be zero or one.";return -1;}
        const auto key=std::make_pair(zone,values[0].bits);
        if(command==0x01F3)vm.environment->zonePedGroups[key]=values[1].bits;
        else {
            std::array<int32_t,13> entry{};
            for(size_t index=0;index<entry.size();++index)entry[index]=uint16_t(values[index+1].bits);
            vm.environment->zoneCivilianCarInformation[key]=entry;
        }
        return 1;
    }
    const bool gang=command==0x0155||command==0x0156||command==0x0157||
        command==0x0367||command==0x0486||command==0x04FC||command==0x04FF||command==0x0547;
    const bool flags=command==0x0500||command==0x0501;
    const bool counter=command==0x03CF||command==0x03D0;
    const bool roads=command==0x0293;
    const bool density=command==0x0125||command==0x0264;
    if(!gang&&!flags&&!counter&&!roads&&!density&&command!=0x0272&&command!=0x0401&&
        command!=0x0534&&command!=0x04BD&&command!=0x03BD&&command!=0x03E1&&command!=0x04C6&&command!=0x04E2&&command!=0x03FD)return 0;
    if(!vm.environment) {error="Intro command requires the live mission environment.";return -1;}
    auto& world=*vm.environment;
    if(command==0x03FD) {updateScmCondition(vm,true,inverted);return 1;}
    const size_t count=roads?6:command==0x0486?5:counter?4:command==0x0155||command==0x0157||command==0x04FF?3:
        flags||gang||command==0x0272?2:1;
    std::array<Operand,6> values{};
    for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="Intro command has a truncated typed operand.";return -1;
    }
    const int32_t first=values[0].bits,second=values[1].bits;
    if(density) {
        const float value=operandFloat(values[0]);
        if(!std::isfinite(value)||value<0||value>10) {error="Population density multiplier is invalid.";return -1;}
        if(command==0x0125)world.carDensityMultiplier=value;
        else world.pedDensityMultiplier=value;
        return 1;
    }
    if(gang) {
        if(first<0||size_t(first)>=world.gangs.size()) {error="Gang index is outside the native pool.";return -1;}
        auto& target=world.gangs[size_t(first)];
        if(command==0x0486) {
            target.empireColor={uint8_t(second),uint8_t(values[2].bits),uint8_t(values[3].bits),255};
            target.empireColorIndex=uint8_t(values[4].bits);
        }
        else if(command==0x0155)target.pedModels={second,values[2].bits};
        else if(command==0x0156)target.carModel=second;
        else if(command==0x0157)target.weapons={second,values[2].bits};
        else if(command==0x0367)target.attackWithPolice=second!=0;
        else if(command==0x04FC)target.accuracy=std::min(int32_t(uint8_t(second)),100);
        else if(command==0x04FF) {target.health=uint16_t(second);target.armour=uint16_t(values[2].bits);}
        else {
            const float value=operandFloat(values[1]);
            if(!std::isfinite(value)||value<0||value>2.55f) {error="Gang attack multiplier is outside its native byte range.";return -1;}
            target.attackMultiplier=float(uint8_t(value*100.0f))/100.0f;
        }
        return 1;
    }
    if(flags) {
        if(second<0||second>15) {error="Character flag bit exceeds the native 16-bit field.";return -1;}
        const uint16_t mask=uint16_t(1u<<second);
        if(command==0x0500)world.characterFlags[first]|=mask;
        else world.characterFlags[first]&=uint16_t(~mask);
        return 1;
    }
    if(counter) {
        auto& color=command==0x03CF?world.counterColor:world.counterBackground;
        for(size_t index=0;index<4;++index)color[index]=uint8_t(values[index].bits);
        if(command==0x03CF)world.counterColorPending=true;
        else world.counterBackgroundPending=true;
        return 1;
    }
    if(roads) {
        std::array<float,6> bounds{};
        for(size_t index=0;index<6;++index) {
            bounds[index]=operandFloat(values[index]);
            if(!std::isfinite(bounds[index])) {error="Interlevel road box has a non-finite coordinate.";return -1;}
        }
        for(size_t axis=0;axis<3;++axis)if(bounds[axis]>bounds[axis+3])std::swap(bounds[axis],bounds[axis+3]);
        if(world.interlevelRoads.size()>=4096) {error="Interlevel road box pool is full.";return -1;}
        world.interlevelRoads.push_back(bounds);return 1;
    }
    if(command==0x04BD) {
        world.radarPositionFrozen=first!=0;
        world.radarPosition=first?std::array<float,3>{world.x,world.y,world.z}:std::array<float,3>{};
    }
    else if(command==0x03BD)world.cameraScriptModeEnabled=first!=0;
    else if(command==0x0272)world.pedTypeThreats[first]&=~uint32_t(second);
    else if(command==0x0401)world.pauseMenuDisabled=first!=0;
    else if(command==0x0534)world.planesAllowedToMove=first!=0;
    else if(command==0x04C6)world.vcsRandomBoatsEnabled=first!=0;
    else if(command==0x03E1)world.totalExportedCars=first;
    else {
        if(first<0||first>2) {error="Script audio mode is outside its native range.";return -1;}
        world.scriptAudioMode=first;
    }
    return 1;
}

bool vcsCharacterPosition(const StorylandMissionEnvironment& world,int32_t handle,
    float& x,float& y,float& z,float& heading) {
    if(handle==0) {x=world.x;y=world.y;z=world.z;heading=world.heading;return world.health>0;}
    const auto found=world.missionCharacters.find(handle);
    if(found==world.missionCharacters.end()||!found->second.alive)return false;
    const auto& character=found->second;
    x=character.x;y=character.y;z=character.z;heading=character.heading;
    return true;
}

void storylandCancelVcsVehicleEntry(StorylandMissionEnvironment& world, int32_t handle,
    StorylandMissionEnvironment::Character& character);

int32_t storylandVcsSignedConversion(float value) {
    if(!std::isfinite(value)||value>=2147483648.0f||value<float(INT32_MIN))
        return std::signbit(value)?INT32_MIN:INT32_MAX;
    return int32_t(std::trunc(value));
}
float storylandVcsIntegerFloat(int32_t value) {
    const float rounded=float(value);
    return std::fabs(double(rounded))>std::fabs(double(value))?std::nextafter(rounded,0.0f):rounded;
}

int32_t storylandVcsPassenger(const StorylandMissionEnvironment& world,int32_t vehicle,int32_t seat) {
    if(world.inVehicle&&world.occupiedMissionVehicle==vehicle&&world.playerPassengerSeat==seat)return 0;
    for(const auto& entry:world.missionCharacters)
        if(entry.second.vehicle==vehicle&&!entry.second.attached&&entry.second.passengerSeat==seat)
            return entry.first;
    if(world.ambientVehiclePassengerPresent&&world.ambientVehiclePassengerPresent(vehicle,seat))return INT32_MAX;
    return -1;
}

int32_t storylandVcsPassengerHandle(const StorylandMissionEnvironment& world,int32_t vehicle,int32_t seat) {
    const int32_t present=storylandVcsPassenger(world,vehicle,seat);
    if(present!=INT32_MAX)return present;
    if(!world.acquireAmbientVehiclePassenger)return INT32_MAX;
    const int32_t handle=world.acquireAmbientVehiclePassenger(vehicle,seat);
    return handle>=0?handle:INT32_MAX;
}

// This host objective represents native follow-objective12, selected by the
// persistent leader policy. Native0119 itself only installs the leader.
constexpr int StorylandVcsLeaderFollowObjective = 0x0119;

void storylandClearVcsLeader(StorylandMissionEnvironment& world, int32_t handle,
    StorylandMissionEnvironment::Character& character) {
    if (character.leader < 0) return;
    character.leader = -1;
    if (character.vehicle >= 0 || character.attached || !character.alive || character.health <= 0 ||
        (world.characterReactionOwned && world.characterReactionOwned(handle))) return;
    character.leaderUpdateAfter = 0;
    character.leaderVehicleAfter = 0;
    character.objective = 0;
    character.objectiveTarget = -1;
    character.walkingToTarget = false;
    character.followingScriptPath = false;
    character.seekRoute.clear();
    character.seekRouteCursor = 0;
    character.seekNode = UINT32_MAX;
    world.characterTargets.erase(handle);
    world.characterWanderTasks.erase(handle);
    if (character.creationSource != 2) {
        auto& task = world.characterWanderTasks[handle];
        task.direction = int32_t(world.nextNativeRandom() % 8);
    }
}

void storylandClearNearbyVcsFollowers(StorylandMissionEnvironment& world, int32_t handle,
    const StorylandMissionEnvironment::Character& leader) {
    struct Neighbour {
        float distanceSquared;
        int32_t handle;
    };
    std::vector<Neighbour> neighbours;
    unsigned deadCount = 0;
    const auto add = [&](int32_t candidate, float x, float y, float z, bool dead) {
        const float dx = x - leader.x;
        const float dy = y - leader.y;
        const float dz = z - leader.z;
        const float planarDistanceSquared = dx * dx + dy * dy;
        if (planarDistanceSquared >= 900.0f || !std::isfinite(planarDistanceSquared) ||
            !std::isfinite(dz) || (dead && deadCount >= 4)) return;
        if (dead) ++deadCount;
        neighbours.push_back({planarDistanceSquared + dz * dz, candidate});
    };
    const auto exposed = [&](int32_t vehicle) {
        return vehicle < 0 || (world.vehicleNativeType && world.vehicleNativeType(vehicle) == 6);
    };
    if (!world.inVehicle || (world.occupiedMissionVehicle >= 0 &&
        world.vehicleNativeType && world.vehicleNativeType(world.occupiedMissionVehicle) == 6))
        add(0, world.x, world.y, world.z, world.health <= 0);
    for (const auto& entry : world.missionCharacters) {
        const auto& character = entry.second;
        if (entry.first == handle || character.removeFromWorld ||
            (!character.attached && !exposed(character.vehicle))) continue;
        add(entry.first, character.x, character.y, character.z,
            !character.alive || character.health <= 0);
    }
    if (world.allAmbientCharactersInArea) {
        const float extent = std::numeric_limits<float>::max();
        const std::array<float,6> bounds{
            leader.x - 30.0f, leader.y - 30.0f, -extent,
            leader.x + 30.0f, leader.y + 30.0f, extent};
        for (const auto& character : world.allAmbientCharactersInArea(bounds))
            if (character.onFoot)
                add(-1, character.position[0], character.position[1], character.position[2],
                    character.health <= 0);
    }
    std::stable_sort(neighbours.begin(), neighbours.end(), [](const Neighbour& first, const Neighbour& second) {
        return first.distanceSquared < second.distanceSquared;
    });
    const size_t count = std::min(size_t(10), neighbours.size());
    for (size_t index = 0; index < count; ++index) {
        const auto found = world.missionCharacters.find(neighbours[index].handle);
        if (found != world.missionCharacters.end() && found->second.leader == handle)
            storylandClearVcsLeader(world, found->first, found->second);
    }
}

void storylandClearDamageSource(StorylandMissionEnvironment& world,
    StorylandMissionEnvironment::DamageSourceKind kind,int32_t handle) {
    const auto clear=[&](auto& source) {if(source.kind==kind&&source.handle==handle)source={};};
    clear(world.playerDamageSource);
    for(auto& entry:world.missionCharacters)clear(entry.second.lastDamageSource);
    for(auto& entry:world.missionVehicles)clear(entry.second.lastDamageSource);
    for(auto& explosion:world.scriptExplosions)clear(explosion.source);
    for(auto& fire:world.scriptFires)clear(fire.second.source);
}

int executeVcsRestoredEntityQuery(StorylandMissionVm& vm,uint16_t command,
    bool inverted,std::string& error) {
    if(!vm.vcs)return 0;
    int count=0;
    switch(command) {
    case 0x004D:count=4;break;
    case 0x0080:case 0x0081:case 0x0082:count=6;break;
    case 0x0089:case 0x008A:case 0x008B:count=8;break;
    case 0x0086:case 0x0087:case 0x0088:count=5;break;
    case 0x008F:case 0x0090:case 0x0091:count=6;break;
    case 0x0273:case 0x0298:case 0x029A:count=3;break;
    case 0x00A6:case 0x0201:case 0x022E:case 0x0275:case 0x0299:
    case 0x02A3:case 0x02A6:case 0x02BD:case 0x0326:case 0x0327:case 0x0328:case 0x0329:count=2;break;
    case 0x013D:case 0x01B7:case 0x01B8:case 0x0274:case 0x02A7:
    case 0x02B3:case 0x02C8:case 0x02DE:case 0x02DF:case 0x02E0:
    case 0x02E1:case 0x02F1:case 0x02D8:case 0x03AF:count=1;break;
    default:return 0;
    }
    if(!vm.environment) {error="Entity query requires a live environment.";return -1;}
    std::array<Operand,8> values{};
    for(int index=0;index<count;++index)if(!readOperand(vm,values[size_t(index)])) {
        error="Entity query has a truncated typed operand.";return -1;
    }
    auto& world=*vm.environment;
    const auto condition=[&](bool value){updateScmCondition(vm,value,inverted);return 1;};
    const auto store=[&](size_t index,int32_t value) {
        if(writeOperand(vm,values[index],value))return true;
        error="Entity query requires a writable output.";return false;
    };
    if(command>=0x0326&&command<=0x0329) {
        const bool victimCharacter=command<=0x0327,attackerCharacter=command==0x0326||command==0x0328;
        StorylandMissionEnvironment::DamageSource source;
        if(victimCharacter) {
            if(values[0].bits==0)source=world.playerDamageSource;
            else {
                const auto found=world.missionCharacters.find(values[0].bits);
                if(found==world.missionCharacters.end())return condition(false);
                source=found->second.lastDamageSource;
            }
        } else {
            const auto found=world.missionVehicles.find(values[0].bits);
            if(found==world.missionVehicles.end()||found->second.deleted)return condition(false);
            source=found->second.lastDamageSource;
        }
        using Kind=StorylandMissionEnvironment::DamageSourceKind;
        if(!attackerCharacter)return condition(source.kind==Kind::Vehicle&&source.handle==values[1].bits);
        bool matches=source.kind==Kind::Character&&source.handle==values[1].bits;
        int32_t car=-1;
        if(values[1].bits==0) {if(world.inVehicle)car=world.occupiedMissionVehicle;}
        else {
            const auto attacker=world.missionCharacters.find(values[1].bits);
            if(attacker==world.missionCharacters.end())return condition(false);
            if(!attacker->second.attached)car=attacker->second.vehicle;
        }
        return condition(matches||(car>=0&&source.kind==Kind::Vehicle&&source.handle==car));
    }
    const auto position=[&](int32_t handle,std::array<float,3>& result,bool& inVehicle)->bool {
        int32_t vehicle=-1;
        if(handle==0) {
            result={world.x,world.y,world.z};inVehicle=world.inVehicle;
            if(inVehicle)vehicle=world.occupiedMissionVehicle;
        } else {
            const auto found=world.missionCharacters.find(handle);
            if(found==world.missionCharacters.end())return false;
            const auto& ped=found->second;result={ped.x,ped.y,ped.z};
            inVehicle=ped.vehicle>=0&&!ped.attached;
            if(inVehicle)vehicle=ped.vehicle;
        }
        const auto car=world.missionVehicles.find(vehicle);
        if(car!=world.missionVehicles.end()&&!car->second.deleted)
            result={car->second.x,car->second.y,car->second.z};
        return true;
    };
    if((command>=0x0080&&command<=0x0082)||(command>=0x0089&&command<=0x008B)) {
        const bool threeDimensional=command>=0x0089;
        const size_t axes=threeDimensional?3:2;
        std::array<float,3> point{},center{},extent{};
        bool inVehicle=false;
        if(!position(values[0].bits,point,inVehicle)) {
            error="Coordinate locate references an unknown character.";return -1;
        }
        for(size_t axis=0;axis<axes;++axis) {
            center[axis]=operandFloat(values[axis+1]);
            extent[axis]=operandFloat(values[axis+1+axes]);
            if(!std::isfinite(center[axis])||!std::isfinite(extent[axis])) {
                error="Coordinate locate requires finite positions and extents.";return -1;
            }
        }
        // Native256E60 uses the occupied vehicle's position and inclusive bounds.
        if(threeDimensional&&(center[2]==-100.0f||center[2]<=-250.0f)) {
            if(!world.groundHeightAt) {
                error="Coordinate locate ground sentinel requires the native collision world.";return -1;
            }
            float height=20;
            if(!world.groundHeightAt({center[0],center[1],1000},height))height=20;
            if(!std::isfinite(height)) {
                error="Coordinate locate ground query returned a non-finite height.";return -1;
            }
            center[2]=height;
        }
        bool inside=true;
        for(size_t axis=0;axis<axes;++axis)
            inside=inside&&point[axis]>=center[axis]-extent[axis]&&
                point[axis]<=center[axis]+extent[axis];
        const int variant=command-(threeDimensional?0x0089:0x0080);
        if(variant==1)inside=inside&&!inVehicle;
        if(variant==2)inside=inside&&inVehicle;
        if(values[axes*2+1].bits&&extent[0]>=0&&extent[1]>=0) {
            StorylandMissionEnvironment::LocateRegion region;
            region.center=center;region.extent=extent;region.threeDimensional=threeDimensional;
            region.expiresAt=world.gameMilliseconds+100;
            if(!threeDimensional&&world.groundHeightAt) {
                float height=0;
                if(world.groundHeightAt({center[0],center[1],1000},height))region.center[2]=height;
            }
            if(world.locateRegions.size()<128)world.locateRegions.push_back(region);
        }
        return condition(inside);
    }
    if((command>=0x0086&&command<=0x0088)||(command>=0x008F&&command<=0x0091)) {
        const bool threeDimensional=command>=0x008F;
        std::array<float,3> first{},second{},extent{};bool firstInVehicle=false,secondInVehicle=false;
        if(!position(values[0].bits,first,firstInVehicle)||!position(values[1].bits,second,secondInVehicle)) {
            error="Character-relative locate references an unknown character.";return -1;
        }
        const size_t axes=threeDimensional?3:2;
        bool inside=true;
        for(size_t axis=0;axis<axes;++axis) {
            extent[axis]=operandFloat(values[axis+2]);
            if(!std::isfinite(extent[axis])) {error="Locate extents must be finite.";return -1;}
            inside=inside&&first[axis]>=second[axis]-extent[axis]&&first[axis]<=second[axis]+extent[axis];
        }
        const int variant=command-(threeDimensional?0x008F:0x0086);
        if(variant==1)inside=inside&&!firstInVehicle;
        if(variant==2)inside=inside&&firstInVehicle;
        if(values[axes+2].bits&&extent[0]>=0&&extent[1]>=0) {
            StorylandMissionEnvironment::LocateRegion region;
            region.center=second;region.extent=extent;region.threeDimensional=threeDimensional;
            region.expiresAt=world.gameMilliseconds+100;
            if(!threeDimensional&&world.groundHeightAt) {
                float height=0;
                if(world.groundHeightAt({second[0],second[1],second[2]+1000},height))region.center[2]=height;
            }
            if(world.locateRegions.size()<128)world.locateRegions.push_back(region);
        }
        return condition(inside);
    }
    if(command==0x0273) {
        const auto vehicle=world.missionVehicles.find(values[0].bits);
        if(vehicle==world.missionVehicles.end()||vehicle->second.deleted||
            !values[1].reference||!values[2].reference) {
            error="GET_CAR_COLOURS requires a live vehicle and two writable outputs.";return -1;
        }
        // Deliberate host correction: SLUS3D4990 never copies the colours to
        // ScriptParams and returns its stale scratch values. Expose the live
        // palette indices here rather than manufacturing that native bug.
        writeOperand(vm,values[1],vehicle->second.primaryColor);
        writeOperand(vm,values[2],vehicle->second.secondaryColor);
        return 1;
    }
    const bool characterCommand=command==0x01B8||command==0x0298||command==0x02A6||command==0x02A7||
        command==0x02C8||(command>=0x02DE&&command<=0x02E1)||command==0x02F1;
    const int32_t handle=values[0].bits;
    if(characterCommand) {
        const auto found=world.missionCharacters.find(handle);
        if(handle!=0&&found==world.missionCharacters.end()) {error="Entity query references an unknown character.";return -1;}
        const bool player=handle==0;
        auto* ped=player?nullptr:&found->second;
        const bool seated=player?world.inVehicle&&!world.playerVehicleTransition:
            ped->vehicle>=0&&!ped->attached&&!ped->passengerSideCarjack&&
                !world.vcsCharacterExitingVehicle(handle);
        const int32_t vehicle=player?world.occupiedMissionVehicle:ped->vehicle;
        if(command==0x01B8) {
            const int result=world.characterOnScreen?world.characterOnScreen(handle):-1;
            if(result<0) {error="Character on-screen query requires the native collision sphere and active camera.";return -1;}
            return condition(result!=0);
        }
        if(command==0x02A6||command==0x02A7)return condition(seated&&(command==0x02A7||vehicle==values[1].bits));
        if(command==0x02E1) {
            if(!world.characterInWater) {error="Character water query requires live physical water contact.";return -1;}
            return condition(world.characterInWater(handle));
        }
        if(command==0x0298) {
            const auto target=world.missionVehicles.find(values[1].bits);
            if(target==world.missionVehicles.end()||target->second.deleted||!world.vehiclePassengerCapacity) {
                error="Passenger warp requires a live vehicle with native seat metadata.";return -1;
            }
            const int capacity=world.vehiclePassengerCapacity(values[1].bits);
            int seat=values[2].bits;
            if(seat<0) {
                for(seat=0;seat<capacity;++seat)if(storylandVcsPassenger(world,values[1].bits,seat)<0)break;
            }
            if(seat>=capacity||storylandVcsPassenger(world,values[1].bits,seat)>=0) {
                error="Passenger warp requires an unoccupied native passenger seat.";return -1;
            }
            auto old=world.missionVehicles.find(vehicle);
            if(old!=world.missionVehicles.end()&&old->second.driver==handle)old->second.driver=-1;
            auto& car=target->second;
            if(player) {
                world.x=car.x;world.y=car.y;world.z=car.z;world.heading=car.heading;
                world.inVehicle=true;world.occupiedMissionVehicle=values[1].bits;world.occupiedVehicleModel=car.model;
                world.playerPassengerSeat=seat;world.playerWarped=true;world.playerAttachedVehicle=-1;
                world.playerVehicleTransition=false;world.playerExitVehicleRequested=false;
                world.playerWalkTask.clear();
                world.playerVehicleEntryTarget=-1;world.playerVehicleEntryAsPassenger=false;
            } else {
                ped->vehicle=values[1].bits;ped->passengerSeat=seat;ped->attached=false;
                ped->x=car.x;ped->y=car.y;ped->z=car.z;ped->heading=car.heading;
                ped->walkingToTarget=false;ped->objective=0;world.characterWanderTasks.erase(handle);
            }
            car.engineOn=true;
            return 1;
        }
        if(command!=0x02C8&&!player) {error="This native vehicle-class query takes the player character.";return -1;}
        if(!seated)return condition(false);
        if(!world.vehicleHandlingClass) {error="Vehicle-class query requires native handling model flags.";return -1;}
        const int type=world.vehicleHandlingClass(vehicle);
        if(type<0) {error="Vehicle-class query cannot resolve native handling metadata.";return -1;}
        return condition(command==0x02C8?type==2:command==0x02DE?type==4:
            command==0x02DF?type==3:command==0x02E0?type==5:type==3||type==5);
    }
    const auto extinguish=[&](int32_t carHandle,auto& vehicle) {
        vehicle.health=std::max(vehicle.health,301.0f);vehicle.healthChangedByScript=true;
        vehicle.extinguishRequested=true;
        for(auto& fire:world.scriptFires)
            if(fire.second.attachedKind==2&&fire.second.attachedHandle==carHandle)fire.second.active=false;
    };
    if(command==0x0274) {
        const bool enabled=handle!=0;
        for(auto& entry:world.missionVehicles)if(!entry.second.deleted) {
            entry.second.canBeDamaged=enabled;
            if(!enabled)extinguish(entry.first,entry.second);
        }
        if(world.setAllVehicleDamageEnabled)world.setAllVehicleDamageEnabled(enabled);
        return 1;
    }
    const auto found=world.missionVehicles.find(handle);
    if(found==world.missionVehicles.end()||found->second.deleted) {error="Entity query references an unknown vehicle.";return -1;}
    auto& vehicle=found->second;
    if(command==0x00A6)return condition(vehicle.model==values[1].bits);
    if(command==0x02A3)return store(1,vehicle.model)?1:-1;
    if(command==0x004D) {
        for(size_t index=1;index<4;++index)if(!values[index].reference) {error="Vehicle coordinates require three writable outputs.";return -1;}
        std::array<float,3> point{};
        if(!storylandVcsVehiclePosition(world,handle,point,error))return -1;
        for(size_t index=0;index<3;++index) {int32_t bits;std::memcpy(&bits,&point[index],4);if(!store(index+1,bits))return -1;}
        return 1;
    }
    if(command==0x013D) {
        std::array<float,9> basis{};
        if(!world.vehicleBasis||!world.vehicleBasis(handle,basis)) {error="Upright query requires the live native vehicle basis.";return -1;}
        return condition(basis[8]>=0);
    }
    if(command==0x01B7) {
        const int result=world.vehicleOnScreen?world.vehicleOnScreen(handle):-1;
        if(result<0) {error="Vehicle on-screen query requires the native collision sphere and active camera.";return -1;}
        return condition(result!=0);
    }
    if(command==0x02D8) {
        bool burning=vehicle.health<250||(world.vehicleOnFire&&world.vehicleOnFire(handle));
        for(const auto& fire:world.scriptFires)
            burning=burning||(fire.second.active&&fire.second.attachedKind==2&&fire.second.attachedHandle==handle);
        return condition(burning);
    }
    if(command==0x0299||command==0x029A) {
        const int seat=values[1].bits;
        if(seat<0||seat>=8) {error="Seat query lies outside the vehicle's native passenger array.";return -1;}
        // 4180C8 indexes the passenger array directly. Only the free-seat
        // predicate at 3D4E50 compares against the model's maximum passengers.
        if(command==0x0299) {
            if(!world.vehiclePassengerCapacity) {error="Seat predicate requires native passenger capacity.";return -1;}
            if(seat>=world.vehiclePassengerCapacity(handle))return condition(false);
        }
        const int32_t passenger=command==0x029A?storylandVcsPassengerHandle(world,handle,seat):
            storylandVcsPassenger(world,handle,seat);
        if(command==0x029A&&passenger==INT32_MAX) {
            error="Occupied passenger seat could not be exposed to the full script character pool.";return -1;
        }
        return command==0x0299?condition(passenger<0):store(2,passenger)?1:-1;
    }
    if(command==0x02BD) {
        if(!values[1].reference) {error="Driver query requires a writable output.";return -1;}
        if(vehicle.driver<0&&world.acquireVehicleDriver) {
            const int32_t driver=world.acquireVehicleDriver(handle);
            if(driver<-1) {error="Driver query cannot resolve the actual native seat occupant.";return -1;}
            vehicle.driver=driver;
        }
        return store(1,vehicle.driver)?1:-1;
    }
    if(command==0x02B3) {
        if(world.inVehicle&&world.occupiedMissionVehicle==handle) {
            world.playerExitVehicleRequested=true;world.playerVehicleTransition=true;
        }
        for(auto& entry:world.missionCharacters)if(entry.second.vehicle==handle&&!entry.second.attached) {
            entry.second.objective=0x00F1;entry.second.objectiveTarget=handle;
        }
        return 1;
    }
    if(command==0x0201)vehicle.visible=values[1].bits!=0;
    else if(command==0x022E)vehicle.siren=values[1].bits!=0;
    else if(command==0x0275) {vehicle.canBeDamaged=values[1].bits!=0;if(!vehicle.canBeDamaged)extinguish(handle,vehicle);}
    else if(command==0x03AF) {
        vehicle.health=0;vehicle.alive=false;vehicle.scriptStatus=5;vehicle.wrecked=true;
        vehicle.healthChangedByScript=true;vehicle.extinguishRequested=true;vehicle.siren=false;
        for(auto& fire:world.scriptFires)
            if(fire.second.attachedKind==2&&fire.second.attachedHandle==handle)fire.second.active=false;
    }
    return 1;
}

int executeVcsCharacterBatchCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (!vm.vcs) {
        return 0;
    }
    int count = 0;
    switch (command) {
    case 0x00A3:
    case 0x00E0:
    case 0x023E:
    case 0x030E:
    case 0x0317:
        count = 1;
        break;
    case 0x00EB:
    case 0x00EC:
    case 0x00F2:
    case 0x01F0:
    case 0x01FB:
    case 0x01FC:
    case 0x020E:
    case 0x0218:
    case 0x0232:
    case 0x02C9:
    case 0x02CC:
    case 0x02F8:
    case 0x02F9:
    case 0x0532:
    case 0x032D:
    case 0x0331:
    case 0x0339:
    case 0x0347:
    case 0x034F:
    case 0x0400:
    case 0x0434:
    case 0x0503:
    case 0x0506:
    case 0x054D:
    case 0x04E4:
    case 0x0531:
        count = 2;
        break;
    case 0x011B:
    case 0x0310:
    case 0x0324:
        count = 3;
        break;
    case 0x00E1:
    case 0x011C:
    case 0x02DC:
        count = 4;
        break;
    case 0x00E2:
        count = 5;
        break;
    default:
        return 0;
    }
    if (!vm.environment) {
        error = "Character command requires a live environment.";
        return -1;
    }
    std::array<Operand,5> values{};
    for (int index = 0; index < count; ++index) {
        if (!readOperand(vm, values[size_t(index)])) {
            error = "Character command has a truncated typed operand.";
            return -1;
        }
    }
    auto& world = *vm.environment;
    const auto condition = [&](bool result) {
        updateScmCondition(vm, result, inverted);
        return 1;
    };
    const auto real = [&](size_t index) {
        return operandFloat(values[index]);
    };
    if (command == 0x011C || command == 0x023E) {
        const int16_t route = int16_t(uint16_t(values[0].bits));
        if (command == 0x023E) {
            auto& points = world.characterRoutePoints;
            const auto first = std::find_if(points.begin(), points.end(),
                [route](const auto& point) { return point.route == route; });
            // Retail 0x3AB7D0 repeatedly reads the first matching row while
            // advancing its index to 200, so the final loop frees the suffix.
            points.erase(first, points.end());
            return 1;
        }
        if (route == -1 || world.characterRoutePoints.size() >= 200) {
            error = "The native route table has no free slot or uses its reserved -1 route ID.";
            return -1;
        }
        for (size_t index = 1; index < 4; ++index) {
            if (!std::isfinite(real(index))) {
                error = "Route coordinates must be finite.";
                return -1;
            }
        }
        // Retail has 200 ordered, 32-byte rows with signed 16-bit route IDs.
        world.characterRoutePoints.push_back({route, {real(1), real(2), real(3)}});
        return 1;
    }
    const int32_t handle = values[0].bits;
    const bool player = handle == 0;
    const auto found = world.missionCharacters.find(handle);
    if (!player && found == world.missionCharacters.end()) {
        error = "Character command references an unknown character.";
        return -1;
    }
    auto* character = player ? nullptr : &found->second;
    if(command==0x02F9) {
        if(player)world.playerDrownsInWater=values[1].bits!=0;
        else character->drownsInWater=values[1].bits!=0;
        return 1;
    }
    if(command==0x0532) {
        if(!player) {error="Infinite swimming requires the active player handle.";return -1;}
        world.playerInfiniteSwim=values[1].bits!=0;
        return 1;
    }
    if (command == 0x0324) {
        // EE sllv masks the shift to five bits, then the script truncates
        // the resulting bit to uint8 before modifying the uint16 mask.
        const uint16_t bit = uint16_t(uint8_t(1u << (uint32_t(values[1].bits) & 31u)));
        auto& mask = player ? world.playerGangDamageMask : character->gangDamageMask;
        if (values[2].bits) {
            mask |= bit;
        } else {
            mask &= uint16_t(~bit);
        }
        return 1;
    }
    if (command == 0x0434) {
        // Native32C2D8 stores CPed+1D8 bit6; no retail consumer is identified.
        if (values[1].bits != 0) world.vcsPedFlag1D8Bit6.insert(handle);
        else world.vcsPedFlag1D8Bit6.erase(handle);
        return 1;
    }
    if (command == 0x032D) {
        if (player) world.playerStayInCarWhenJacked = values[1].bits != 0;
        else character->stayInCarWhenJacked = values[1].bits != 0;
        return 1;
    }
    if (command == 0x0339) {
        // Native 41A698 changes CPed+1D0 bit34 for either player or NPC.
        if (player) world.playerCanBeShotInVehicle = values[1].bits != 0;
        else character->canBeShotInVehicle = values[1].bits != 0;
        return 1;
    }
    if (command == 0x04E4 || command == 0x0531) {
        // 417EA0: CPed+1C8 bit33 uses nonzero.41E228: +1D8 bit35 uses lowbit.
        const bool enabled = command == 0x0531 ? storylandVcsEjectionFlag(values[1].bits) : values[1].bits != 0;
        if (command == 0x0531) {
            if (player) world.playerDontReenterAfterEjection = enabled;
            else character->dontReenterAfterEjection = enabled;
        } else {
            if (player) world.playerAvoidUnsupportedForwardStep = enabled;
            else character->avoidUnsupportedForwardStep = enabled;
        }
        return 1;
    }
    if (command == 0x054D) {
        // Retail41E458 changes CPed+1D8 bit40. The right-front carjack
        // branch20BDBC moves an enabled driver into the passenger door.
        if (player) world.playerCanBeDraggedFromPassengerSide = values[1].bits != 0;
        else character->canBeDraggedFromPassengerSide = values[1].bits != 0;
        return 1;
    }
    if (command == 0x02F8) {
        if (player) world.playerPositionFrozen = values[1].bits != 0;
        else character->positionFrozen = values[1].bits != 0;
        return 1;
    }
    if (command == 0x0503 || command == 0x0506) {
        const bool enabled = values[1].bits != 0;
        if (command == 0x0503) {
            if (player) world.playerDisableVerticalMovement = enabled;
            else character->disableVerticalMovement = enabled;
        } else {
            if (player) world.playerTestShotInVehicle = enabled;
            else character->testShotInVehicle = enabled;
        }
        return 1;
    }
    if (command == 0x01FB || command == 0x01FC || command == 0x0400) {
        if (!player) {
            error = "PlayerInfo command requires the active player handle.";
            return -1;
        }
        if (command == 0x0400) {
            if (!writeOperand(vm, values[1], world.playerSavedWeapon)) {
                error = "Saved-weapon query requires a writable output.";
                return -1;
            }
        } else if (command == 0x01FB) {
            world.playerNeverTired = values[1].bits != 0;
        } else {
            world.playerFastReload = values[1].bits != 0;
        }
        return 1;
    }
    if (command == 0x02DC) {
        for (size_t index = 1; index < 4; ++index) {
            if (!values[index].reference || values[index].variable >=
                (values[index].global ? vm.globals.size() : vm.locals.size())) {
                error = "Pickup-coordinate query requires three writable outputs.";
                return -1;
            }
        }
        std::array<float,3> position{};
        if (!world.deadCharacterPickupPosition ||
            !world.deadCharacterPickupPosition(handle, position)) {
            error = "Pickup-coordinate query requires the native collision and pickup world.";
            return -1;
        }
        for (size_t index = 0; index < 3; ++index) {
            int32_t bits = 0;
            std::memcpy(&bits, &position[index], sizeof bits);
            if (!writeOperand(vm, values[index + 1], bits)) {
                error = "Pickup-coordinate output could not be written.";
                return -1;
            }
        }
        return 1;
    }
    if (!character && command != 0x0218) {
        error = "This character AI command requires a non-player actor.";
        return -1;
    }
    if (command == 0x00A3) {
        return condition(character->objectiveCompleted);
    }
    if (command == 0x0317) {
        return condition(character->objective == 0);
    }
    if (command == 0x01F0) {
        const int32_t target = values[1].bits == 0 || world.missionCharacters.count(values[1].bits) ?
            values[1].bits : -1;
        return condition(character->leader == target && !character->leaderOutOfRange);
    }
    if (command == 0x0218 || command == 0x02C9) {
        if (!world.scriptLineOfSightHit) {
            error = "Character sight requires the native collision world.";
            return -1;
        }
        if (command == 0x02C9 && character->vehicle >= 0) {
            error = "Dead-character sight from inside a vehicle requires native ignored-vehicle collision filtering.";
            return -1;
        }
        const std::array<float,3> observer = player ?
            std::array<float,3>{world.x, world.y, world.z} :
            std::array<float,3>{character->x, character->y, character->z};
        const float observerHeading = player ? world.heading : character->heading;
        const auto visible = [&](float x, float y, float z, uint32_t mask) {
            const float dx = x - observer[0];
            const float dy = y - observer[1];
            const float heading = observerHeading * 0.0174532925199433f;
            // Native CanSeeEntity rejects the rear hemisphere and distances
            // of 40 units or more before tracing between points at z + 1.
            if (dx * -std::sin(heading) + dy * std::cos(heading) < 0 ||
                dx * dx + dy * dy >= 1600.0f) {
                return false;
            }
            return !world.scriptLineOfSightHit(
                {observer[0], observer[1], observer[2] + 1.0f},
                {x, y, z + 1.0f}, mask);
        };
        if (command == 0x0218) {
            if (values[1].bits == 0) {
                return condition(visible(world.x, world.y, world.z, 9));
            }
            const auto target = world.missionCharacters.find(values[1].bits);
            if (target == world.missionCharacters.end()) {
                error = "Character sight references an unknown target.";
                return -1;
            }
            return condition(visible(target->second.x, target->second.y, target->second.z, 9));
        }
        std::vector<std::pair<float,int32_t>> neighbours;
        for (const auto& candidate : world.missionCharacters) {
            if (candidate.first == handle) {
                continue;
            }
            const auto& ped = candidate.second;
            const float distance = std::hypot(ped.x - character->x,
                ped.y - character->y, ped.z - character->z);
            neighbours.emplace_back(distance, candidate.first);
        }
        std::stable_sort(neighbours.begin(), neighbours.end());
        const size_t limit = std::min(size_t(32), neighbours.size());
        for (size_t index = 0; index < limit; ++index) {
            const auto& dead = world.missionCharacters.at(neighbours[index].second);
            if (dead.health <= 0 && dead.type == values[1].bits &&
                visible(dead.x, dead.y, dead.z, 3)) {
                return condition(true);
            }
        }
        return condition(false);
    }
    switch (command) {
    case 0x020E:
        character->stayInSamePlace = values[1].bits != 0;
        return 1;
    case 0x0232:
        character->cannotBeDraggedOut = values[1].bits != 0;
        return 1;
    case 0x0347:
        character->neverTargeted = values[1].bits != 0;
        return 1;
    case 0x034F:
        character->stayInSamePlace = values[1].bits != 0;
        // Native 41B008 sets stop-and-shoot flags and resets MoveState through 2190C0.
        character->stopShooting = false;
        character->movementState = 0;
        character->movementSpeed = 0;
        character->walkingToTarget = false;
        return 1;
    case 0x02CC:
        character->ceaseAttackDelay = uint32_t(values[1].bits);
        return 1;
    case 0x0331:
        character->canFightInGroup = values[1].bits != 0;
        return 1;
    case 0x0310:
        if (values[1].bits != 0) {
            error = "Character friendship requires the active player handle.";
            return -1;
        }
        character->playerFriend = values[2].bits != 0;
        if (character->playerFriend) {
            character->fearFlags &= ~1u;
            character->provokedByPlayer = false;
        }
        return 1;
    case 0x030E: {
        const auto vehicle = world.missionVehicles.find(character->vehicle);
        if (character->attached || character->passengerSeat != 0 ||
            vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
            return 1;
        }
        if (vehicle->second.driver >= 0) {
            const auto driver = world.missionCharacters.find(vehicle->second.driver);
            if (driver == world.missionCharacters.end() ||
                !world.vcsCharacterExitingVehicle(driver->first)) {
                return 1;
            }
            const auto activeExit = world.characterVehicleTasks.find(driver->first);
            if (activeExit != world.characterVehicleTasks.end() && activeExit->second.exiting) return 1;
            // The native animation only promotes the front passenger after
            // the driver starts leaving. Finish that exit through the normal
            // exit-position callback, preserving both seat occupants.
            std::array<float,3> exit{};
            if (!world.vehicleExitPosition ||
                !world.vehicleExitPosition(character->vehicle, -1, exit)) {
                return 1;
            }
            driver->second.x = exit[0];
            driver->second.y = exit[1];
            driver->second.z = exit[2];
            driver->second.vehicle = -1;
            driver->second.passengerSeat = -1;
            driver->second.objective = 0;
            driver->second.objectiveCompleted = true;
        }
        vehicle->second.driver = handle;
        character->passengerSeat = -1;
        character->objective = 0;
        character->walkingToTarget = false;
        return 1;
    }
    default:
        break;
    }
    // Native SetObjective 0x3BB6F8 permits objective 0x16 while attached.
    // Marked Men uses it for Phil's attack from the Bobcat's truck bed.
    const bool attachedVehicleAttack = character->attached && command == 0x00EC;
    const bool seatedVehicleFlee = command == 0x00F2 && character->vehicle >= 0 && !character->attached;
    const bool queuedVehicleAttack = command == 0x00EC && !character->attached &&
        world.vcsCharacterExitingVehicle(handle);
    if ((character->vehicle >= 0 || character->attached) &&
        !attachedVehicleAttack && !seatedVehicleFlee && !queuedVehicleAttack) {
        error = "This objective requires an on-foot actor; queued vehicle exits are not available.";
        return -1;
    }
    if (seatedVehicleFlee && (!world.vehicleNativeType ||
        world.vehicleNativeType(character->vehicle) != 0 || !world.vehicleCanExit ||
        !world.prepareCharacterVehicleTask)) {
        error = "Seated flee requires the native automobile exit, collision and animation consumers.";
        return -1;
    }
    if (queuedVehicleAttack) {
        const auto exit = world.characterVehicleTasks.find(handle);
        const int32_t vehicle = exit != world.characterVehicleTasks.end() && exit->second.exiting ?
            exit->second.vehicle : character->vehicle;
        if (!world.vehicleNativeType || world.vehicleNativeType(vehicle) != 0 ||
            !world.vehicleCanExit || !world.prepareCharacterVehicleTask) {
            error = "Queued vehicle attack requires the native automobile exit and animation consumers.";
            return -1;
        }
    }
    if (command == 0x00E1 || command == 0x00E2) {
        for (int index = 1; index < count; ++index) {
            if (!std::isfinite(real(size_t(index)))) {
                error = "Guard coordinates must be finite.";
                return -1;
            }
        }
        const float x = command == 0x00E1 ? real(1) : float((double(real(1)) + real(3)) * 0.5);
        const float y = command == 0x00E1 ? real(2) : float((double(real(2)) + real(4)) * 0.5);
        const float radius = command == 0x00E1 ? 0.5f : float(std::max(
            std::fabs(double(real(3)) - real(1)), std::fabs(double(real(4)) - real(2))) * 0.5);
        float height = command == 0x00E1 ? real(3) : world.z - 0.94f;
        if (height == -100.0f || height <= -250.0f) {
            if (!world.groundHeightAt) {
                error = "Guard ground sentinel requires the native collision world.";
                return -1;
            }
            if (!world.groundHeightAt({x, y, 1000}, height)) {
                height = 20.0f;
            }
        }
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(radius) || !std::isfinite(height)) {
            error = "Guard coordinates overflow the native coordinate range.";
            return -1;
        }
        character->targetX = x;
        character->targetY = y;
        character->objectiveRadius = radius;
        character->objectiveHeight = height;
        character->walkSpeed = 1.25f;
    } else if (command == 0x011B) {
        const int16_t route = int16_t(uint16_t(values[1].bits));
        const int16_t mode = int16_t(uint16_t(values[2].bits));
        const auto first = std::find_if(world.characterRoutePoints.begin(), world.characterRoutePoints.end(),
            [route](const auto& point) { return point.route == route; });
        if (first == world.characterRoutePoints.end() || mode < 1 || mode > 3) {
            error = "Follow-route requires an existing route and native mode 1, 2 or 3.";
            return -1;
        }
        auto end = first;
        while (end != world.characterRoutePoints.end() && end->route == route) {
            ++end;
        }
        if (std::distance(first, end) < 2) {
            error = "Native follow-route requires at least two contiguous route points.";
            return -1;
        }
        character->routeId = route;
        character->routeMode = mode;
        character->routeCursor = 1;
        character->routeDirection = 1;
        character->objectiveHeight = (first + 1)->position[2];
        character->walkSpeed = 1.25f;
    } else if (command == 0x00EB) {
        const auto object = std::find_if(world.objects.begin(), world.objects.end(),
            [&](const auto& item) { return item.handle == values[1].bits; });
        if (object == world.objects.end()) {
            error = "Destroy-object objective references an unknown object.";
            return -1;
        }
        character->objectiveTarget = values[1].bits;
    } else if (command == 0x00EC || command == 0x00F2) {
        const auto vehicle = world.missionVehicles.find(values[1].bits);
        if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
            error = "Vehicle objective references an unknown vehicle.";
            return -1;
        }
        if ((command == 0x00F2 || queuedVehicleAttack) && character->objective == command) return 1;
        character->objectiveTarget = values[1].bits;
    } else if (command == 0x00E0) {
        const auto threat = world.characterTargets.find(handle);
        character->objectiveTarget = threat != world.characterTargets.end() &&
            threat->second.kind == StorylandMissionEnvironment::TargetKind::Character ?
            threat->second.handle : 0;
    }
    if (queuedVehicleAttack) {
        const auto exit = world.characterVehicleTasks.find(handle);
        if (exit == world.characterVehicleTasks.end() || !exit->second.exiting) {
            storylandCancelVcsVehicleEntry(world, handle, *character);
            StorylandMissionEnvironment::CharacterVehicleTask task;
            task.vehicle = character->vehicle;
            task.seat = character->passengerSeat;
            task.exiting = true;
            world.characterVehicleTasks.emplace(handle, std::move(task));
        }
        // Native3BB8CC retains exit16 and stores attack22. The existing
        // SC53 task holds that exit; objective/target retain its continuation.
        character->threatVisible = false;
        world.characterTargets.erase(handle);
    }
    character->objective = command;
    character->objectiveCompleted = false;
    character->walkingToTarget = false;
    character->seekRoute.clear();
    character->seekRouteCursor = 0;
    character->attackStartedAt = 0;
    character->fleeVehicle = -1;
    character->fleeVehicleDeadline = 0;
    world.characterWanderTasks.erase(handle);
    return 1;
}

int executeVcsCharacterRecoveryCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (!vm.vcs) return 0;
    int count = -1;
    switch (command) {
    case 0x052F: count = 0; break;
    case 0x00F0: case 0x01C6: case 0x01F1: case 0x020D: case 0x0265:
    case 0x02EC: case 0x032B: case 0x034D: case 0x0436: case 0x054E: count = 1; break;
    case 0x02ED: case 0x0336: case 0x0342: case 0x0449: case 0x044A:
    case 0x04AE: case 0x04AF: case 0x04B0: case 0x04B1: case 0x04B2: case 0x04B3: count = 2; break;
    case 0x0343: count = 3; break;
    case 0x0222: count = 4; break;
    case 0x03A2: case 0x0419: count = 5; break;
    case 0x01C1: case 0x0337: count = 6; break;
    case 0x01C4: count = 8; break;
    case 0x0425: count = 9; break;
    default: return 0;
    }
    if (!vm.environment) { error = "Character recovery command requires a live world."; return -1; }
    std::array<Operand,9> values{};
    for (int index = 0; index < count; ++index) {
        if (!readOperand(vm, values[size_t(index)])) {
            error = "Character recovery command has a truncated typed operand."; return -1;
        }
    }
    auto& world = *vm.environment;
    const auto condition = [&](bool value) { updateScmCondition(vm, value, inverted); return 1; };
    const auto writable = [&](size_t index) {
        const auto& value = values[index];
        return value.reference && value.variable < (value.global ? vm.globals.size() : vm.locals.size());
    };
    const auto store = [&](size_t index, int32_t value) {
        if (!writable(index) || !writeOperand(vm, values[index], value)) {
            error = "Character query requires a writable typed output."; return -1;
        }
        return 1;
    };
    const auto real = [&](size_t index) { return operandFloat(values[index]); };
    const auto finite = [&](size_t first, size_t end) {
        for (size_t index = first; index < end; ++index) if (!std::isfinite(real(index))) return false;
        return true;
    };
    const int32_t handle = values[0].bits;
    const auto found = world.missionCharacters.find(handle);
    auto* character = found == world.missionCharacters.end() ? nullptr : &found->second;
    if (command == 0x052F) {
        if (!world.returnPlayerWeapons || !world.returnPlayerWeapons()) {
            error = "Confiscated-weapon restoration requires the live inventory."; return -1;
        }
        return 1;
    }
    if (command == 0x054E) {
        if (!writable(0) || !world.countReturnableWeapons) {
            error = "Confiscated-weapon count requires a writable output and the live inventory."; return -1;
        }
        return store(0, world.countReturnableWeapons());
    }
    if (command == 0x0265) { world.forcedRandomPedType = handle; return 1; }
    if (command == 0x02EC || command == 0x02ED || command == 0x01C6) {
        if (handle != 0) { error = "Player-info command requires the local player handle."; return -1; }
        if (command == 0x02EC) { world.playerHavoc = 0; return 1; }
        if (command == 0x02ED) {
            int32_t bits = 0; std::memcpy(&bits, &world.playerHavoc, sizeof(bits));
            return store(1, bits);
        }
        int32_t model = world.occupiedVehicleModel;
        const auto car = world.missionVehicles.find(world.occupiedMissionVehicle);
        if (car != world.missionVehicles.end()) model = car->second.model;
        const int32_t nativeModel = int16_t(uint16_t(model));
        return condition(world.inVehicle &&
            (nativeModel == 226 || nativeModel == 183 || nativeModel == -959 || nativeModel == -937));
    }
    if (command == 0x020D) {
        if (!character || character->type == 0) return 1;
        world.missionCleanupCharacters.erase(handle);
        if (character->creationSource != 2) return 1;
        if (character->vehicle >= 0 && !character->attached) {
            storylandDeleteMissionCharacter(world, handle);
        } else {
            character->persistent = false; character->creationSource = 1;
            character->objectiveCompleted = false; character->removeFromWorld = true;
        }
        return 1;
    }
    if (command == 0x032B || command == 0x0436 || command == 0x034D) {
        if (handle != 0 && !character) return condition(false);
        if (command == 0x034D) return condition((handle == 0 ? world.playerWaitState : character->waitState) == 12);
        const auto& query = command == 0x032B ? world.characterDrowning : world.characterFighting;
        if (!query) { error = "Native character state query requires the live runtime."; return -1; }
        return condition(query(handle));
    }
    if (command == 0x01C1) {
        if (!finite(1, 5) || !world.characterShooting) {
            error = "Shooting-area query requires finite bounds and the live shooting state."; return -1;
        }
        float x = 0, y = 0, z = 0, heading = 0;
        if (!vcsCharacterPosition(world, handle, x, y, z, heading)) return condition(false);
        const float minX = std::min(real(1), real(3)), maxX = std::max(real(1), real(3));
        const float minY = std::min(real(2), real(4)), maxY = std::max(real(2), real(4));
        if (values[5].bits && world.locateRegions.size() < 128) {
            StorylandMissionEnvironment::LocateRegion region;
            region.center = {(minX + maxX) * .5f, (minY + maxY) * .5f, z};
            region.extent = {(maxX - minX) * .5f, (maxY - minY) * .5f, 0};
            region.expiresAt = world.gameMilliseconds + 100;
            world.locateRegions.push_back(region);
        }
        return condition(world.characterShooting(handle) && x >= minX && x <= maxX && y >= minY && y <= maxY);
    }
    if (command == 0x0336) {
        if ((handle != 0 && !character) || !world.missionVehicles.count(values[1].bits)) return condition(false);
        if (!world.characterTouchingVehicle) { error = "Touching-car query requires live physical contacts."; return -1; }
        return condition(world.characterTouchingVehicle(handle, values[1].bits));
    }
    if (command == 0x0337) {
        if (!finite(1, 4) || !world.ambientCharactersInArea) {
            error = "Nearby-model query requires finite extents and live pedestrian enumeration."; return -1;
        }
        float x = 0, y = 0, z = 0, heading = 0;
        if (!vcsCharacterPosition(world, handle, x, y, z, heading)) return condition(false);
        const std::array<float,6> bounds{x - std::fabs(real(1)), y - std::fabs(real(2)), z - std::fabs(real(3)),
            x + std::fabs(real(1)), y + std::fabs(real(2)), z + std::fabs(real(3))};
        const auto matches = [&](int32_t model, float px, float py, float pz) {
            return (model == values[4].bits || model == values[5].bits) && px >= bounds[0] && px <= bounds[3] &&
                py >= bounds[1] && py <= bounds[4] && pz >= bounds[2] && pz <= bounds[5];
        };
        if (world.modelIdByName && matches(world.modelIdByName(world.playerOutfit), world.x, world.y, world.z))
            return condition(true);
        for (const auto& entry : world.missionCharacters)
            if (!entry.second.removeFromWorld && matches(entry.second.model, entry.second.x, entry.second.y, entry.second.z))
                return condition(true);
        for (const auto& ped : world.ambientCharactersInArea(bounds))
            if (matches(ped.model, ped.position[0], ped.position[1], ped.position[2])) return condition(true);
        return condition(false);
    }
    if (command == 0x01C4 || command == 0x03A2 || command == 0x0419 || command == 0x0425) {
        const size_t output = size_t(count - 1), coordinates = command == 0x0425 ? 6 : 4;
        if (!writable(output) || !finite(0, coordinates) ||
            !world.ambientCharactersInArea || !world.acquireAmbientCharacters) {
            error = "Random-character acquisition requires finite bounds, a writable output, and the live population."; return -1;
        }
        float playerZ = world.z;
        const auto playerCar = world.missionVehicles.find(world.occupiedMissionVehicle);
        if (world.inVehicle && playerCar != world.missionVehicles.end()) playerZ = playerCar->second.z;
        std::array<float,6> bounds{};
        if (command == 0x0425) {
            for (size_t axis = 0; axis < 3; ++axis) {
                bounds[axis] = std::min(real(axis), real(axis + 3));
                bounds[axis + 3] = std::max(real(axis), real(axis + 3));
            }
        } else {
            bounds = {std::min(real(0), real(2)), std::min(real(1), real(3)), playerZ - 5,
                std::max(real(0), real(2)), std::max(real(1), real(3)), playerZ + 5};
        }
        const auto suitable = [&](int32_t type, int32_t model, bool onFoot) {
            if (type == 0 || (command == 0x0419 && !onFoot)) return false;
            if (command == 0x0425) return model == values[6].bits || model == values[7].bits;
            if (command != 0x01C4) return true;
            return ((type == 4 || type == 5) && values[4].bits) ||
                (type >= 7 && type <= 15 && values[5].bits) ||
                ((type == 18 || type == 20) && values[6].bits);
        };
        int32_t result = -1;
        for (auto entry = world.missionCharacters.rbegin(); entry != world.missionCharacters.rend(); ++entry) {
            auto& ped = entry->second;
            const bool checked = command == 0x01C4 || command == 0x0425;
            if (ped.removeFromWorld || (command != 0x0425 && storylandVcsWaitBlocksMovement(ped.waitState)) ||
                (checked && (ped.creationSource != 1 || !ped.alive || ped.health <= 0)) ||
                (command == 0x01C4 && entry->first == world.lastRandomCharacter) ||
                !suitable(ped.type, ped.model, ped.vehicle < 0 || ped.attached) ||
                ped.x < bounds[0] || ped.x > bounds[3] || ped.y < bounds[1] || ped.y > bounds[4] ||
                ped.z < bounds[2] || ped.z > bounds[5]) continue;
            result = entry->first; ped.creationSource = 2; ped.persistent = true; break;
        }
        if (result < 0) {
            for (const auto& ped : world.ambientCharactersInArea(bounds)) {
                if (!suitable(ped.type, ped.model, ped.onFoot)) continue;
                std::vector<int32_t> acquired;
                if (!world.acquireAmbientCharacters({ped.slot}, true, acquired) || acquired.size() != 1) {
                    error = "The selected ambient character could not be acquired atomically."; return -1;
                }
                result = acquired.front(); break;
            }
        }
        if (result >= 0) {
            world.lastRandomCharacter = result;
            if (vm.isMissionThread) world.missionCleanupCharacters.insert(result);
        }
        return store(output, result);
    }
    if (command == 0x0222 || command == 0x0342 || command == 0x0343) {
        const size_t output = size_t(count - 1);
        if (!writable(output) || !world.randomCharacterModel || world.missionCharacters.size() >= 128 ||
            world.nextMissionCharacterHandle < 1 || world.nextMissionCharacterHandle == INT32_MAX ||
            world.missionCharacters.count(world.nextMissionCharacterHandle)) {
            error = "Random-character creation requires native population metadata and a writable free pool slot."; return -1;
        }
        StorylandMissionEnvironment::Character ped;
        int32_t vehicle = -1;
        if (command == 0x0222) {
            if (!finite(0, 3)) { error = "Random-character coordinates must be finite."; return -1; }
            ped.x = real(0); ped.y = real(1); ped.z = real(2);
            if (ped.z == -100.0f || ped.z <= -250.0f) {
                float height = 0;
                if (!world.groundHeightAt || !world.groundHeightAt({ped.x, ped.y, 1000}, height) || !std::isfinite(height)) {
                    error = "Random-character placement requires native ground collision."; return -1;
                }
                ped.z = height;
            }
        } else {
            vehicle = handle;
            const auto car = world.missionVehicles.find(vehicle);
            if (car == world.missionVehicles.end() || !car->second.alive || car->second.deleted) {
                error = "Random vehicle occupant requires a live vehicle."; return -1;
            }
            if (command == 0x0342) {
                if (car->second.driver >= 0 || (world.inVehicle && world.occupiedMissionVehicle == vehicle &&
                    world.playerPassengerSeat < 0)) return store(output, -1);
            } else {
                if (!world.vehiclePassengerCapacity) { error = "Random passenger requires native seat metadata."; return -1; }
                const int capacity = world.vehiclePassengerCapacity(vehicle);
                int seat = values[1].bits < 0 ? 0 : int(uint8_t(values[1].bits));
                if (values[1].bits < 0) while (seat < capacity && storylandVcsPassenger(world, vehicle, seat) >= 0) ++seat;
                if (seat >= capacity || storylandVcsPassenger(world, vehicle, seat) >= 0) return store(output, -1);
                ped.passengerSeat = seat;
            }
            ped.x = car->second.x; ped.y = car->second.y; ped.z = car->second.z;
            ped.heading = car->second.heading; ped.vehicle = vehicle;
        }
        if (!world.randomCharacterModel(vehicle, ped.model, ped.type) || ped.model < 0 || ped.type < 0) return store(output, -1);
        if (world.initializeCharacterAppearance &&
            !world.initializeCharacterAppearance(ped, error)) return -1;
        ped.playerFriend = false;
        ped.nativePedRandomSeed = uint8_t(world.nextNativeRandom());
        ped.nativePedRandomSeedInitialized = true;
        const uint16_t draw = uint16_t(world.nextNativeRandom()) % 25;
        ped.walletMoney = draw == 23 ? 400 : draw; ped.walletInitialized = true;
        ped.speechProfile = world.nativePedSpeechProfile ? world.nativePedSpeechProfile(ped.model) : -1;
        const auto threats = world.pedTypeThreats.find(ped.type);
        if (threats != world.pedTypeThreats.end()) ped.fearFlags = threats->second;
        const int32_t result = world.nextMissionCharacterHandle++;
        world.missionCharacters.emplace(result, ped);
        if (command == 0x0342) world.missionVehicles.at(vehicle).driver = result;
        if (vm.isMissionThread) world.missionCleanupCharacters.insert(result);
        return store(output, result);
    }
    if (!character) { error = "Character behavior requires a live non-player actor."; return -1; }
    if (command == 0x00F0) {
        if (!character->alive || character->health <= 0 || character->attached) {
            error = "Steal-any-car objective requires a living, unattached actor."; return -1;
        }
        character->objective = 0x00F0; character->objectiveTarget = -1;
        character->followingScriptPath = false;
        character->objectiveCompleted = false; character->walkingToTarget = false;
        character->attackStartedAt = 0; world.characterWanderTasks.erase(handle);
        world.characterTargets.erase(handle);
        return 1;
    }
    if (command == 0x01F1) {
        if (!world.explodeCharacterHead || !world.explodeCharacterHead(handle)) {
            error = "Explode-head requires the native character damage consumer."; return -1;
        }
        return 1;
    }
    if (command == 0x0449 || command == 0x04B2 || command == 0x04B3) {
        if (!writable(1) || !world.characterColorSetCount) {
            error = "Character color count requires native model metadata and a writable output."; return -1;
        }
        const int32_t result = world.characterColorSetCount(handle, command == 0x04B3);
        if (result < 0) { error = "The character's native color table is not available."; return -1; }
        return store(1, result);
    }
    if (command == 0x04B0 || command == 0x04B1)
        return store(1, command == 0x04B0 ? character->colorSet : character->scriptColorSet);
    std::array<uint32_t,6> colors{};
    if (!world.characterColorSet || !world.characterColorSet(handle, command == 0x04AF, values[1].bits, colors)) {
        error = "The requested native character color set is unavailable or outside its table."; return -1;
    }
    character->clothingColors = colors; character->clothingColorsSet = true;
    if (command == 0x04AE) { character->colorSet = int8_t(uint8_t(values[1].bits)); character->scriptColorSet = -1; }
    if (command == 0x04AF) { character->scriptColorSet = int8_t(uint8_t(values[1].bits)); character->colorSet = -1; }
    return 1;
}

bool storylandIceCreamPosition(const StorylandMissionEnvironment& world, int32_t vehicle,
    int slot, std::array<float,3>& position) {
    if (slot < 0 || slot > 3 || !world.vehicleWorldMatrix) return false;
    std::array<float,16> matrix{};
    if (!world.vehicleWorldMatrix(vehicle, matrix)) return false;
    const float x = slot & 1 ? 2.0f : -2.0f;
    const float y = slot & 2 ? -0.5f : 1.0f;
    for (size_t axis = 0; axis < 3; ++axis)
        position[axis] = matrix[12 + axis] + x * matrix[axis] + y * matrix[4 + axis];
    return std::all_of(position.begin(), position.end(), [](float value) { return std::isfinite(value); });
}

bool storylandCharacterOutsideArea(const StorylandMissionEnvironment::Character& character) {
    const auto& area = character.behavior.area;
    return character.behavior.areaEnabled && (character.x < area[0] || character.x > area[2] ||
        character.y < area[1] || character.y > area[3]);
}

int executeVcsCharacterExtendedCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (!vm.vcs) return 0;
    int count = 0;
    switch (command) {
    case 0x0219: case 0x026F: case 0x02EF: case 0x0320: case 0x035C: case 0x035D:
    case 0x0361: case 0x03C4: case 0x03FA: case 0x0437: case 0x0438: case 0x0439:
    case 0x0446: case 0x044C: case 0x04F8: count = 1; break;
    case 0x028C: case 0x02B8: case 0x0368: case 0x03A5: case 0x03AE: case 0x03BC:
    case 0x041B: case 0x041C: case 0x0485: case 0x04A3: case 0x04A4: case 0x04CF:
    case 0x04D0: case 0x04F0: case 0x0526: case 0x0558: case 0x0563: count = 2; break;
    case 0x0276: case 0x0359: case 0x0487: case 0x0523: case 0x0528: case 0x0529: case 0x052A:
    case 0x054B: count = 3; break;
    case 0x0322: case 0x046D: count = 5; break;
    case 0x0041: case 0x0433: count = 6; break;
    case 0x0365: count = 8; break;
    case 0x04A2: count = 9; break;
    default: return 0;
    }
    if (!vm.environment) { error = "Character command requires the live world."; return -1; }
    std::array<Operand,9> values{};
    for (int index = 0; index < count; ++index) {
        if (!readOperand(vm, values[size_t(index)])) {
            error = "Character command has a truncated typed operand."; return -1;
        }
    }
    auto& world = *vm.environment;
    const auto real = [&](size_t index) { return operandFloat(values[index]); };
    const auto finite = [&](size_t first, size_t last) {
        for (size_t index = first; index < last; ++index)
            if (!std::isfinite(real(index))) return false;
        return true;
    };
    const auto writable = [&](size_t index) {
        const auto& value = values[index];
        return value.reference && value.variable < (value.global ? vm.globals.size() : vm.locals.size());
    };
    const auto store = [&](size_t index, int32_t value) {
        if (!writable(index) || !writeOperand(vm, values[index], value)) {
            error = "Character query requires a writable output."; return -1;
        }
        return 1;
    };
    const auto storeReal = [&](size_t index, float value) {
        int32_t bits = 0; std::memcpy(&bits, &value, sizeof(bits)); return store(index, bits);
    };
    const auto condition = [&](bool value) { updateScmCondition(vm, value, inverted); return 1; };
    if (command == 0x03C4) {
        world.requestedAnimationGroups.insert(0);
        world.loadAllModelsRequested = true;
        return 1;
    }
    if (command == 0x03FA) {
        if (values[0].bits == 0) return 1;
        if (values[0].bits < 0 || !world.setPlayerClipAmmunition ||
            !world.setPlayerClipAmmunition(values[0].bits)) {
            error = "The active player weapon cannot accept that clip ammunition."; return -1;
        }
        return 1;
    }
    if (command == 0x0365) {
        if (!writable(7) || !finite(0, 4)) {
            error = "Customer acquisition requires finite bounds and a writable output."; return -1;
        }
        const std::array<float,6> bounds{std::min(real(0), real(2)), std::min(real(1), real(3)), world.z - 5,
            std::max(real(0), real(2)), std::max(real(1), real(3)), world.z + 5};
        const auto suitable = [&](int type) {
            return ((type == 4 || type == 5) && values[4].bits) ||
                (type >= 7 && type <= 15 && values[5].bits) ||
                ((type == 18 || type == 20) && values[6].bits);
        };
        int32_t result = -1;
        for (auto entry = world.missionCharacters.rbegin(); entry != world.missionCharacters.rend(); ++entry) {
            const auto& ped = entry->second;
            if (entry->first == world.lastRandomCharacter || ped.creationSource != 1 || !ped.alive ||
                ped.health <= 0 || ped.vehicle >= 0 || ped.attached || storylandVcsWaitBlocksMovement(ped.waitState) || ped.removeFromWorld ||
                ped.behavior.attemptedAttractor || ped.behavior.iceCreamVehicle >= 0 || !suitable(ped.type) ||
                ped.x < bounds[0] || ped.x > bounds[3] || ped.y < bounds[1] || ped.y > bounds[4] ||
                ped.z < bounds[2] || ped.z > bounds[5]) continue;
            result = entry->first; break;
        }
        if (result < 0 && world.ambientCharactersInArea && world.acquireAmbientCharacters) {
            for (const auto& ped : world.ambientCharactersInArea(bounds)) {
                if (!ped.onFoot || ped.health <= 0 || !suitable(ped.type)) continue;
                std::vector<int32_t> acquired;
                if (!world.acquireAmbientCharacters({ped.slot}, true, acquired) || acquired.size() != 1) {
                    error = "The selected customer could not be acquired."; return -1;
                }
                result = acquired.front(); break;
            }
        }
        if (result >= 0) {
            auto& ped = world.missionCharacters.at(result);
            ped.creationSource = 2; ped.persistent = true;
            world.lastRandomCharacter = result;
            if (vm.isMissionThread) world.missionCleanupCharacters.insert(result);
        }
        return store(7, result);
    }
    const int32_t handle = values[0].bits;
    const bool player = handle == 0;
    const auto found = world.missionCharacters.find(handle);
    auto* character = found == world.missionCharacters.end() ? nullptr : &found->second;
    const bool exists = player || character;
    if(command==0x04F8) {
        // 32C518: active paired-action bit and a paired entity of ped type.
        // A null native pedestrian lookup returns false.
        return condition(exists&&world.characterPairedAction&&world.characterPairedAction(handle));
    }
    if (command == 0x02EF) return condition(exists && world.photographedCharacters.erase(handle) != 0);
    if (command == 0x03BC) {
        StorylandMissionEnvironment::Target target;
        if (player) target = world.playerTarget;
        else {
            const auto selected = world.characterTargets.find(handle);
            if (selected != world.characterTargets.end()) target = selected->second;
        }
        return store(1, target.kind == StorylandMissionEnvironment::TargetKind::Character &&
            (target.handle == 0 || world.missionCharacters.count(target.handle)) ? target.handle : -1);
    }
    if (command == 0x0320) return condition(character && character->behavior.leavingVehicleToDie);
    if (command == 0x035D) return condition(character && character->behavior.attemptedAttractor);
    if (command == 0x0361) return condition(character && character->behavior.boughtIceCream);
    if (command == 0x0437) return condition(character && storylandCharacterOutsideArea(*character));
    if (command == 0x0438) return condition(character && character->behavior.areaEnabled);
    if (command == 0x0439) return condition(character && character->behavior.areaBoundaryBlocked);
    if (command == 0x0446) {
        const int state = player ? world.playerWaitState : character ? character->waitState : 0;
        return condition(state == 17 || state == 18);
    }
    if (command == 0x044C) return condition(character && character->alive && character->vehicle < 0 &&
        !character->attached && character->walkingToTarget && character->walkSpeed > 0 &&
        character->walkSpeed <= 2 && !storylandVcsWaitBlocksMovement(character->waitState) &&
        !character->behavior.diveDeadline);
    if (!exists) { error = "Character command references an unknown character."; return -1; }
    if (command == 0x0558) {
        // Native 41E568 stores the low byte of the typed operand's bits.
        // In RENC7 the literal 1.0 has bits 0x3F800000 and stores zero.
        const uint8_t value=uint8_t(uint32_t(values[1].bits)&0xFFu);
        if(player)world.playerVcsSeekNodeRadius=value;
        else character->vcsSeekNodeRadius=value;
        return 1;
    }
    if (command == 0x0487) {
        auto& marks = player ? world.playerVcsFootstepMarks : character->vcsFootstepMarks;
        storylandSetVcsFootstepMarks(marks, values[1].bits, values[2].bits);
        return 1;
    }
    if (command == 0x054B || command == 0x0563) {
        auto& flags = player ? world.playerVcsScriptFlags : character->vcsScriptFlags;
        if (command == 0x054B) {
            // 41E3B8 looks up the vehicle without dereferencing it. Values
            // outside the native two-side/default range leave CPed unchanged.
            if (values[2].bits >= 0 && values[2].bits <= 2)
                flags.evasiveDirection = uint8_t(values[2].bits);
        } else flags.acceptsPairedActions = values[1].bits != 0;
        return 1;
    }
    if (command == 0x026F || command == 0x04F0) {
        if (!player) { error = "Player command requires the active player handle."; return -1; }
        if (command == 0x026F) {
            if (!world.playerCutsceneSafe) {
                world.playerCutscenePreviousControl = world.control;
                world.playerCutscenePreviousPoliceIgnore = world.policeIgnorePlayer;
                world.playerCutscenePreviousDamageProofs = world.playerDamageProofs;
            }
            world.playerCutsceneSafe = true; world.control = false;
            world.playerDamageProofs = 31; world.policeIgnorePlayer = true;
            world.playerTarget = {}; world.manualAim.forced = false;
        } else world.playerDamageProofs = values[1].bits ? 0 : 31;
        return 1;
    }
    if (command == 0x02B8 || command == 0x04CF || command == 0x04D0) {
        if (!finite(1, 2)) { error = "Attached camera limit must be finite."; return -1; }
        if (!player || !world.activeVcsCharacterAttachment(0)) return 1;
        const float angle = real(1) * .017453292519943295f;
        if (command != 0x04D0) world.playerAttachedPitchLimits[0] = angle;
        if (command != 0x04CF) world.playerAttachedPitchLimits[1] = angle;
        return 1;
    }
    if (command == 0x028C) {
        if (values[1].bits == 15) {
            if (!world.characterSpeech) { error = "Native character speech is unavailable."; return -1; }
            world.characterSpeech(handle, 0x92);
        }
        return 1;
    }
    if (command == 0x046D) {
        for (size_t index = 1; index < 5; ++index) if (!writable(index)) {
            error = "Projected character bounds require four writable outputs."; return -1;
        }
        std::array<float,3> center{}; float radius = 0;
        if (!world.characterBoundingSphere || !world.screenCoordinates ||
            !world.characterBoundingSphere(handle, center, radius) || !std::isfinite(radius) || radius < 0) {
            error = "The character's native collision sphere and camera projection are unavailable."; return -1;
        }
        std::array<std::array<float,2>,4> projected{};
        const std::array<std::array<float,3>,4> points{{
            {center[0] - radius, center[1], center[2]}, {center[0] + radius, center[1], center[2]},
            {center[0], center[1], center[2] + radius}, {center[0], center[1], center[2] - radius}}};
        for (size_t index = 0; index < 4; ++index)
            if (!world.screenCoordinates(points[index], projected[index])) projected[index] = {0, 0};
        storeReal(1, projected[0][0]); storeReal(2, projected[1][0]);
        storeReal(3, projected[2][1]); return storeReal(4, projected[3][1]);
    }
    if (command == 0x0322 || command == 0x04A2) {
        if (!finite(1, size_t(count))) { error = "Character geometry query requires finite coordinates."; return -1; }
        float x = 0, y = 0, z = 0, heading = 0;
        if (!vcsCharacterPosition(world, handle, x, y, z, heading)) return condition(false);
        if (command == 0x04A2) {
            const float ax = real(4) - real(1), ay = real(5) - real(2);
            const float bx = real(6) - real(1), by = real(7) - real(2);
            const float a = std::hypot(ax, ay), b = std::hypot(bx, by);
            if (a == 0 || b == 0) return condition(false);
            const float u = ((x - real(1)) * ax + (y - real(2)) * ay) / a;
            const float v = ((x - real(1)) * bx + (y - real(2)) * by) / b;
            return condition(z >= real(3) - .2f && z <= real(3) + real(8) + .2f &&
                u >= -.2f && u <= a + .2f && v >= -.2f && v <= b + .2f);
        }
        if (!world.pedestrianLineClear || !world.groundHeightAt) {
            error = "Wander-path query requires the native collision world."; return -1;
        }
        // Native CREATE_CHAR 0x412338 positions the ped's matrix origin one
        // unit above the script coordinate. Storyland stores the on-foot base.
        const bool onFoot = player ? !world.inVehicle && !world.activeVcsCharacterAttachment(0) :
            character && character->vehicle < 0 && !character->attached &&
                !world.activeVcsCharacterAttachment(handle);
        const float originZ = onFoot ? z + 1.0f : z;
        const float dx = real(1) - x, dy = real(2) - y, dz = real(3) - originZ;
        const float length = std::hypot(dx, dy, dz);
        if (std::fabs(dz) > real(4) || !world.pedestrianLineClear(
            {x, y, std::min(originZ, real(3))}, {real(1), real(2), std::min(originZ, real(3))})) return condition(false);
        const int samples = int(std::min(4.0f, std::ceil(length)));
        if (samples == 0) return condition(true);
        // Native 0x1997C8 also rejects exposed water along the first sampled
        // metres; a bridge's solid surface above the water remains walkable.
        if (world.objectWorld) for (int index = 1; index < samples; ++index) {
            const float fraction = float(index) / std::max(length, .0001f);
            const float sampleX = x + dx * fraction, sampleY = y + dy * fraction;
            float water = 0, surface = 0;
            if (storylandNativeWaterHeight(*world.objectWorld, sampleX, sampleY, water) &&
                (!world.groundHeightAt({sampleX, sampleY, std::max(originZ, real(3))}, surface) || surface < water))
                return condition(false);
        }
        float ground = 0;
        if (!world.groundHeightAt({x, y, originZ}, ground) || ground < originZ - 5) return condition(false);
        float previous = ground + .5f;
        for (int index = 1; index < samples; ++index) {
            const float fraction = float(index) / std::max(length, .0001f);
            float height = 0;
            if (!world.groundHeightAt({x + dx * fraction, y + dy * fraction, previous}, height) ||
                std::fabs(height - previous) > 1) return condition(false);
            previous = height + .5f;
        }
        return condition(true);
    }
    if (command == 0x0485 || command == 0x0523 || command == 0x0526 || command == 0x0528 ||
        command == 0x0529 || command == 0x052A) {
        auto association = world.scriptedCharacterAnimations.find(handle);
        const auto playerAnimation = world.pedAnimationRequests.find(handle);
        const int animation = association != world.scriptedCharacterAnimations.end() ? association->second.animation :
            character ? character->animationId : command == 0x0528 && playerAnimation != world.pedAnimationRequests.end() ?
                playerAnimation->second.animationId : -1;
        if (command == 0x0485) return condition(animation >= 0 && animation == values[1].bits);
        if (command == 0x0529) {
            if (!writable(2)) { error = "Animation time requires a writable output."; return -1; }
            float time = 0;
            if (animation == values[1].bits && animation >= 0)
                time = world.characterAnimationTime(handle, character ? character->animationStartedAt : world.gameMilliseconds);
            return storeReal(2, time);
        }
        if (command == 0x0523) {
            if (values[1].bits < 0 || values[1].bits >= 200 || values[2].bits < 0) {
                error = "The script animation group or association ID is invalid."; return -1;
            }
            const float duration = world.animationDuration ? world.animationDuration(values[1].bits, values[2].bits) : 0;
            if (!(duration > 0) || !std::isfinite(duration)) {
                error = "The requested native animation is unavailable."; return -1;
            }
            StorylandMissionEnvironment::ScriptAnimation state;
            state.group = values[1].bits; state.animation = values[2].bits; state.clock = world.gameMilliseconds;
            state.scriptManaged = true;
            state.loop = world.animationLoops && world.animationLoops(state.group, state.animation);
            world.scriptedCharacterAnimations[handle] = state;
            if (character) {
                character->animationGroup = state.group; character->animationId = state.animation;
                character->animationStartedAt = world.gameMilliseconds; character->animationLoop = state.loop;
                character->animationBlend = 1;
            } else world.pedAnimationRequests[0] = {state.animation, 0, world.nextPedAnimationSerial++, state.group};
            return 1;
        }
        if (command == 0x0526) {
            if (!finite(1, 2)) { error = "Animation blend rate must be finite."; return -1; }
            if (association != world.scriptedCharacterAnimations.end() && association->second.scriptManaged)
                association->second.blendRate = real(1);
            return 1;
        }
        if (command == 0x0528) {
            if (!finite(2, 3)) { error = "Animation blend rate must be finite."; return -1; }
            // Retail 0x184260 looks up the existing association by ID, writes
            // its blend delta and enables removal once it fades out. Audimon
            // uses a negative rate here when a character's voice clip ends.
            if (animation < 0 || animation != values[1].bits) return 1;
            if (association == world.scriptedCharacterAnimations.end()) {
                StorylandMissionEnvironment::ScriptAnimation state;
                state.group = character ? character->animationGroup : playerAnimation->second.animationGroup;
                state.animation = animation;
                state.time = world.characterAnimationTime(handle,
                    character ? character->animationStartedAt : world.gameMilliseconds);
                state.clock = world.gameMilliseconds;
                state.loop = character ? character->animationLoop :
                    world.animationLoops && world.animationLoops(state.group, animation);
                association = world.scriptedCharacterAnimations.emplace(handle, state).first;
            }
            association->second.blendRate = real(2);
            return 1;
        }
        if (!finite(2, 3)) { error = "Animation time must be finite."; return -1; }
        if (real(2) < 0 || animation < 0 || animation != values[1].bits) return 1;
        const int group = association != world.scriptedCharacterAnimations.end() ? association->second.group : character->animationGroup;
        const float duration = world.animationDuration ? world.animationDuration(group, animation) : 0;
        if (!(duration > 0) || !std::isfinite(duration)) { error = "Animation duration is unavailable."; return -1; }
        const bool loop = association != world.scriptedCharacterAnimations.end() ? association->second.loop : character->animationLoop;
        const float time = loop ? std::fmod(real(2), duration) : std::min(real(2), duration);
        if (association == world.scriptedCharacterAnimations.end()) {
            StorylandMissionEnvironment::ScriptAnimation state;
            state.group = group; state.animation = animation; state.loop = loop;
            association = world.scriptedCharacterAnimations.emplace(handle, state).first;
        }
        association->second.time = time; association->second.clock = world.gameMilliseconds;
        return 1;
    }
    if (command == 0x03AE && player) {
        const auto car = world.missionVehicles.find(values[1].bits);
        if (car == world.missionVehicles.end() || car->second.deleted) {
            error = "Evasive dive requires an existing vehicle."; return -1;
        }
        if (!world.playerEvasiveDive) {
            error = "The native player evasive-dive binding is unavailable."; return -1;
        }
        return world.playerEvasiveDive(values[1].bits, error) ? 1 : -1;
    }
    if (!character) { error = "This behavior requires a non-player character."; return -1; }
    auto& behavior = character->behavior;
    if (command == 0x041B) { behavior.targetPriority = values[1].bits; return 1; }
    if (command == 0x041C) return store(1, behavior.targetPriority);
    if (command == 0x0368) { behavior.frightenedWhenJacked = values[1].bits != 0; return 1; }
    if (command == 0x03A5) { behavior.dropsWeapons = values[1].bits != 0; return 1; }
    if (command == 0x035C) {
        if (behavior.iceCreamVehicle >= 0) {
            behavior.iceCreamVehicle = -1; behavior.iceCreamSlot = -1; behavior.iceCreamStartedAt = 0;
            character->objective = 0; character->walkingToTarget = false;
        }
        return 1;
    }
    if (command == 0x0433) {
        if (!finite(1, 5)) { error = "Character area bounds must be finite."; return -1; }
        behavior.area = {std::min(real(1), real(3)), std::min(real(2), real(4)),
            std::max(real(1), real(3)), std::max(real(2), real(4))};
        behavior.areaEnabled = std::any_of(behavior.area.begin(), behavior.area.end(), [](float value) { return value != 0; });
        behavior.adjustTargetsToArea = values[5].bits != 0; behavior.areaBoundaryBlocked = false;
        return 1;
    }
    if (command == 0x04A3 || command == 0x04A4) {
        const int32_t target = values[1].bits;
        const auto car = world.missionVehicles.find(target);
        if ((command == 0x04A3 && target != 0 && !world.missionCharacters.count(target)) ||
            (command == 0x04A4 && (car == world.missionVehicles.end() || car->second.deleted))) {
            error = "Forced threat refers to an unknown target."; return -1;
        }
        behavior.forcedThreat = target; behavior.forcedThreatIsVehicle = command == 0x04A4;
        return 1;
    }
    if (command == 0x0276) {
        const auto other = world.missionCharacters.find(values[1].bits);
        if (other == world.missionCharacters.end() || other->first == handle || values[2].bits < 0) {
            error = "Conversation requires two characters and a nonnegative duration."; return -1;
        }
        const auto converse = [&](StorylandMissionEnvironment::Character& ped, int32_t partner) {
            ped.behavior.conversationPartner = partner;
            ped.behavior.conversationDeadline = world.gameMilliseconds + uint32_t(values[2].bits);
            ped.behavior.nextConversationSpeech = world.gameMilliseconds + 3000;
            ped.lookAtCharacter = partner; ped.walkingToTarget = false;
            ped.animationGroup = 0; ped.animationId = 14; ped.animationStartedAt = world.gameMilliseconds;
            ped.animationLoop = true;
        };
        converse(*character, other->first); converse(other->second, handle); return 1;
    }
    if (command == 0x0041) {
        if (!finite(1, 5) || real(4) < 0) { error = "Follow-path coordinates and arrival radius must be finite."; return -1; }
        float height = real(3);
        if (height == -100 || height <= -250) {
            if (!world.groundHeightAt) { error = "Follow-path ground sentinel requires native collision."; return -1; }
            if (!world.groundHeightAt({real(1), real(2), 1000}, height)) height = 20;
        }
        if (!character->alive || character->vehicle >= 0 || character->attached) return 1;
        character->objective = 0; character->followingScriptPath = true; character->objectiveCompleted = false;
        character->targetX = real(1); character->targetY = real(2); character->objectiveHeight = height;
        character->objectiveRadius = real(4); character->walkSpeed = values[5].bits == 1 ? 3.8f : 1.25f;
        character->walkingToTarget = true; character->seekNodes = true; character->seekRoute.clear();
        world.characterWanderTasks.erase(handle); return 1;
    }
    if (command == 0x0219) {
        character->objective = command; character->objectiveTarget = -1; character->objectiveCompleted = false;
        character->walkingToTarget = false; world.characterWanderTasks.erase(handle); return 1;
    }
    if (command == 0x03AE) {
        const auto car = world.missionVehicles.find(values[1].bits);
        if (car == world.missionVehicles.end() || car->second.deleted) { error = "Evasive dive requires an existing vehicle."; return -1; }
        if (!character->alive || character->vehicle >= 0 || character->attached) return 1;
        uint8_t flags = behavior.statFlags;
        if (character->pedStat < 0 && world.nativePedStat) {
            std::array<float,4> parameters{};std::array<uint8_t,5> personality{};
            if (world.nativePedStat(character->model, -1, parameters, personality)) flags = personality[4];
        }
        const int animation = flags & 0x10 ? 156 : 146;
        const float duration = world.animationDuration ? world.animationDuration(0, animation) : 0;
        if (!(duration > 0) || !std::isfinite(duration) || duration >= 60) { error = "The native evasive animation is unavailable."; return -1; }
        std::array<float,3> velocity = car->second.velocity;
        if (world.vehicleWorldVelocity && !world.vehicleWorldVelocity(values[1].bits, velocity)) {
            error = "The evasive target vehicle has no live velocity."; return -1;
        }
        if (!std::isfinite(velocity[0]) || !std::isfinite(velocity[1])) {
            error = "The evasive target velocity is non-finite."; return -1;
        }
        const uint8_t direction = character->vcsScriptFlags.evasiveDirection;
        const uint32_t random = direction == 0 ? world.nextNativeRandom() : 0;
        const float heading = storylandVcsForcedEvasionHeading(velocity[0], velocity[1], direction, random);
        const float angle = heading * .017453292519943295f;
        behavior.diveVelocity = animation == 156 ? std::array<float,2>{} :
            std::array<float,2>{std::cos(angle) * 3, std::sin(angle) * 3};
        behavior.diveDeadline = world.gameMilliseconds + uint32_t(duration * 1000);
        character->heading = heading - 90;
        character->animationGroup = 0; character->animationId = animation;
        character->animationStartedAt = world.gameMilliseconds; character->animationLoop = false;
        character->walkingToTarget = false; return 1;
    }
    if (command == 0x0359) {
        if (!writable(2)) { error = "Attractor assignment requires a writable result."; return -1; }
        const auto car = world.missionVehicles.find(values[1].bits);
        if (car == world.missionVehicles.end() || car->second.deleted) { error = "Attractor assignment requires an existing vehicle."; return -1; }
        if (character->objective || behavior.attemptedAttractor || behavior.iceCreamVehicle >= 0 ||
            !character->alive || character->vehicle >= 0 || character->attached) return store(2, 0);
        std::array<float,16> matrix{};
        if (!world.vehicleWorldMatrix || !world.vehicleWorldMatrix(values[1].bits, matrix)) {
            error = "Vehicle service positions require its native transform."; return -1;
        }
        const float dx = character->x - matrix[12], dy = character->y - matrix[13], dz = character->z - matrix[14];
        const int slot = (dx * matrix[0] + dy * matrix[1] + dz * matrix[2] > 0 ? 1 : 0) |
            (dx * matrix[4] + dy * matrix[5] + dz * matrix[6] > 0 ? 0 : 2);
        std::array<float,3> position{};
        if (!storylandIceCreamPosition(world, values[1].bits, slot, position)) return store(2, 0);
        if (std::hypot(character->x - position[0], character->y - position[1], character->z - position[2]) >= 20)
            return store(2, 0);
        for (const auto& entry : world.missionCharacters)
            if (entry.second.behavior.iceCreamVehicle == values[1].bits && entry.second.behavior.iceCreamSlot == slot)
                return store(2, 0);
        if (world.pedestrianLineClear && !world.pedestrianLineClear(
            {character->x, character->y, character->z + 1}, {position[0], position[1], position[2] + 1})) return store(2, 0);
        behavior.iceCreamVehicle = values[1].bits; behavior.iceCreamSlot = slot; behavior.iceCreamStartedAt = 0;
        behavior.iceCreamDeadline = world.gameMilliseconds + 30000;
        character->objective = command; character->targetX = position[0]; character->targetY = position[1];
        character->walkingToTarget = true; character->walkSpeed = 1.25f;
        return store(2, 1);
    }
    return 0;
}

int executeVcsRestoredCharacterCommand(StorylandMissionVm& vm,uint16_t command,
    bool inverted,std::string& error) {
    if(!vm.vcs)return 0;
    const int extendedResult = executeVcsCharacterExtendedCommand(vm, command, inverted, error);
    if (extendedResult != 0) return extendedResult;
    const int recoveryResult = executeVcsCharacterRecoveryCommand(vm, command, inverted, error);
    if (recoveryResult != 0) return recoveryResult;
    const int batchResult = executeVcsCharacterBatchCommand(vm, command, inverted, error);
    if (batchResult != 0) {
        return batchResult;
    }
    int count=0;
    switch(command) {
    case 0x0220:case 0x0309:case 0x03FB:count=3;break;
    case 0x04B9:case 0x0448:case 0x0423:count=4;break;
    case 0x0285:case 0x0278:case 0x03DE:case 0x0507:case 0x0393:
    case 0x00A1:case 0x01A4:case 0x01AF:case 0x01FD:case 0x02A5:
    case 0x0344:case 0x0348:case 0x035F:case 0x042D:case 0x042C:
    case 0x01D0:case 0x01EE:case 0x015D:case 0x0213:case 0x02FE:case 0x02C0:case 0x007D:
    case 0x00E4:case 0x00E5:case 0x00E6:case 0x00E7:case 0x00F3:case 0x00F4:
    case 0x00E8:case 0x00E9:case 0x00EA:case 0x0119:case 0x02BE:
    case 0x03D5:case 0x03D6:case 0x03D7:case 0x0429:case 0x04E6:count=2;break;
    case 0x00F5:case 0x0158:case 0x00EF:count=3;break;
    case 0x00ED:count=5;break;
    case 0x0332:case 0x03A6:case 0x04EE:case 0x0115:case 0x034A:
    case 0x02A8:case 0x007E:case 0x01AC:case 0x01C7:case 0x033B:case 0x02BA:
    case 0x0338:case 0x011A:case 0x04D3:case 0x00F1:case 0x04DF:count=1;break;
    default:return 0;
    }
    if(!vm.environment) {error="Character command requires a live environment.";return -1;}
    std::array<Operand,5> values{};
    for(int index=0;index<count;++index)if(!readOperand(vm,values[size_t(index)])) {
        error="Character command has a truncated typed operand.";return -1;
    }
    auto& world=*vm.environment;
    const int32_t handle=values[0].bits;
    const bool player=handle==0;
    const auto found=world.missionCharacters.find(handle);
    const bool exists=player||found!=world.missionCharacters.end();
    if(command==0x0429) {
        if(!values[1].reference) {error="Current character target requires a writable output.";return -1;}
        StorylandMissionEnvironment::Target target;
        if(exists) {
            if(player)target=world.playerTarget;
            else {
                const auto selected=world.characterTargets.find(handle);
                if(selected!=world.characterTargets.end())target=selected->second;
            }
        }
        const int32_t result=target.kind==StorylandMissionEnvironment::TargetKind::Character&&
            (target.handle==0||world.missionCharacters.count(target.handle))?target.handle:-1;
        return writeOperand(vm,values[1],result)?1:-1;
    }
    if(command==0x034A) {updateScmCondition(vm,exists,inverted);return 1;}
    if(command==0x04E6) {
        // SLUS-21590 0x41DCE8 accepts a null pool lookup.
        if(exists) {
            if(values[1].bits&1)world.vcsPedFlag14.insert(handle);
            else world.vcsPedFlag14.erase(handle);
        }
        return 1;
    }
    if(!exists) {error="Character command references an unknown character.";return -1;}
    auto* character=player?nullptr:&found->second;
    const bool alive=player?world.health>0:character->alive&&character->health>0;
    const bool onFoot=player?!world.inVehicle&&!world.activeVcsCharacterAttachment(0):
        character->vehicle<0&&!character->attached;
    const auto condition=[&](bool value){updateScmCondition(vm,value,inverted);return 1;};
    const auto store=[&](size_t index,int32_t value) {
        if(writeOperand(vm,values[index],value))return true;
        error="Character query requires a writable output.";return false;
    };
    const auto real=[&](size_t index){return operandFloat(values[index]);};
    if(command==0x03D5) {
        const float health=storylandVcsIntegerFloat(values[1].bits);
        if(player) {world.health=health;world.playerMaximumHealth=health;}
        else {character->health=health;character->maximumHealth=health;}
        return 1;
    }
    if(command==0x03D6||command==0x03D7) {
        if(player) {
            error="Native player attack-timer integration is not implemented; the player was not changed.";
            return -1;
        }
        auto& timers=character->attackTimers;
        (command==0x03D6?timers.shootOverride:timers.attackOverride)=uint32_t(values[1].bits);
        return 1;
    }
    if(command==0x02A8||command==0x007E)return condition(alive&&(command==0x02A8?onFoot:
        (player?world.inVehicle:character->vehicle>=0&&!character->attached)));
    if(command==0x007D) {
        const int32_t car=player?world.occupiedMissionVehicle:character->vehicle;
        const auto foundCar=world.missionVehicles.find(car);
        return condition(alive&&!onFoot&&foundCar!=world.missionVehicles.end()&&foundCar->second.model==values[1].bits);
    }
    if(command==0x01AC)return condition(alive&&(player?std::fabs(world.speed)<.01f:!character->walkingToTarget));
    if(command==0x01D0) {
        if(player&&!world.modelIdByName) {error="Player-model query requires native model metadata.";return -1;}
        const int32_t model=player?world.modelIdByName(world.playerOutfit):character->model;
        return condition(model==values[1].bits);
    }
    if(command==0x042D)return condition((player?0:character->type)==values[1].bits);
    if(command==0x042C)return condition((player?world.armour:character->armour)>float(values[1].bits));
    if(command==0x01C7) {
        const bool shooting=world.characterShooting?world.characterShooting(handle):
            character&&character->hasShot&&world.gameMilliseconds-character->shotAt<=100;
        return condition(shooting);
    }
    if(command==0x04DF) {
        const int weapon=player?world.currentWeapon:character->currentWeapon;
        return condition(player&&world.manualAim.mode==7&&(weapon==30||weapon==31));
    }
    if(command==0x0448||command==0x0423) {
        if(command==0x0423) {
            for(size_t index=1;index<4;++index)if(!std::isfinite(real(index))) {
                error="Character coordinates must be finite.";return -1;
            }
            if(player) {world.x=real(1);world.y=real(2);world.z=real(3);}
            else {character->x=real(1);character->y=real(2);character->z=real(3);}
            return 1;
        }
        for(size_t index=1;index<4;++index)if(!values[index].reference) {
            error="Dead-character coordinates require three writable outputs.";return -1;
        }
        const std::array<float,3> position=player?std::array<float,3>{world.x,world.y,world.z}:
            std::array<float,3>{character->x,character->y,character->z};
        for(size_t index=0;index<3;++index) {
            int32_t bits;std::memcpy(&bits,&position[index],4);
            if(!store(index+1,bits))return -1;
        }
        return 1;
    }
    if(command==0x0309||command==0x03A6) {
        auto& crouch=player?world.playerCrouch:character->crouch;
        if(command==0x03A6)return condition(crouch.active);
        const bool enable=values[1].bits!=0;
        if(!enable) {
            storylandClearCrouch(crouch,world.gameMilliseconds);
            crouch.requested=false;
            return 1;
        }
        // SetDuck(-1) retains an existing, non-fading duck-down association.
        // The script-owned request bit is set even when that association wins.
        if(values[2].bits==-1&&crouch.poseAnimation==153&&crouch.poseFade.present&&crouch.poseFade.rate>=0) {
            crouch.requested=true;
            return 1;
        }
        const bool extending=crouch.active&&values[2].bits!=-1;
        if(!extending) {
            // Native SetDuck blends association 155 without cancelling the
            // existing phone, script or weapon task, including attached peds.
            if(alive) {
                const float duration=world.animationDuration?world.animationDuration(0,155):0;
                if(!std::isfinite(duration)||duration<=0||duration>=60) {
                    error="Crouch requires the native group 0 association 155.";return -1;
                }
            }
        }
        crouch.requested=true;
        const uint32_t deadline=values[2].bits==-1?UINT32_MAX:
            world.gameMilliseconds+uint32_t(values[2].bits);
        if(extending) {
            if(deadline>crouch.deadline)crouch.deadline=deadline;
        } else {
            crouch.active=true;crouch.deadline=deadline;crouch.poseAnimation=155;
            crouch.startedAt=world.gameMilliseconds;
            crouch.poseFade={0,4.0f,world.gameMilliseconds,true};
        }
        return 1;
    }
    if(command==0x0393) {
        if(player)world.playerAllowFrontPassenger=values[1].bits!=0;
        else character->allowFrontPassenger=values[1].bits!=0;
        return 1;
    }
    if(command==0x04EE) {
        if(player)world.playerRenderAlpha=0;else character->renderAlpha=0;
        return 1;
    }
    if(command==0x0507) {
        if(player)world.playerSpeechProfile=values[1].bits;
        else character->speechProfile=values[1].bits;
        return 1;
    }
    if(command==0x02C0)return store(1,player?world.currentWeapon:character->currentWeapon)?1:-1;
    if(command==0x02FE)return store(1,int32_t(player?world.armour:character->armour))?1:-1;
    if(command==0x01EE)return condition(storylandObjectWeaponMatches(
        player?world.playerLastWeaponDamage:character->lastWeaponDamage,values[1].bits));
    if(command==0x02BA) {if(player)world.playerLastWeaponDamage=-1;else character->lastWeaponDamage=-1;return 1;}
    if(command==0x033B) {if(player)world.playerDamageSource={};else character->lastDamageSource={};return 1;}
    if (command == 0x0213 && player) {
        world.armour = std::clamp(world.armour + float(values[1].bits), 0.0f, float(world.playerArmourLimit));
        return 1;
    }
    if(command==0x0220||command==0x03FB||command==0x0332) {
        const int32_t state=command==0x0332?0:values[1].bits;
        if(!storylandVcsWaitStateValid(state)) {
            error="SET_CHAR_WAIT_STATE has an invalid native state ID.";return -1;
        }
        const auto& crouch=player?world.playerCrouch:character->crouch;
        if(state&&(!alive||!onFoot||crouch.active)&&storylandVcsWaitBlocksMovement(state)) {
            error="This stationary wait animation requires a live, on-foot actor.";return -1;
        }
        // Preserve interaction/movement states even when the actor has an
        // objective. Their original task, not an invented idle animation,
        // owns the movement for the requested interval.
        const int32_t animation=storylandVcsWaitAssociation(state);
        const float nativeDuration=animation>=0&&world.animationDuration?
            world.animationDuration(0,animation):0.0f;
        const bool nativeAnimation=animation>=0&&std::isfinite(nativeDuration)&&nativeDuration>0.0f;
        auto& waitState=player?world.playerWaitState:character->waitState;
        auto& waitDeadline=player?world.playerWaitDeadline:character->waitDeadline;
        auto& versionB=player?world.playerWaitVersionB:character->waitVersionB;
        waitState=state;versionB=command==0x03FB;
        if(!state) {
            waitDeadline=0;
            world.pedWaitStateMilliseconds.erase(handle);
            world.pedAnimationRequests.erase(handle);
            if(character)character->animationId=-1;
        } else {
            const uint32_t duration=state==12||state==13?5000u:
                values[2].bits>=0?uint32_t(values[2].bits):
                state==24?25000u+uint32_t((uint64_t(world.nextNativeRandom())*5000u)>>31):3000u;
            waitDeadline=world.gameMilliseconds+duration;
            world.pedWaitStateMilliseconds[handle]=int32_t(duration);
            if(nativeAnimation) {
                if(player)world.pedAnimationRequests[handle]={animation,
                    int32_t(duration),world.nextPedAnimationSerial++,0};
                else {
                    character->animationGroup=0;character->animationId=animation;
                    character->animationStartedAt=world.gameMilliseconds;
                    character->animationBlend=state==24?128:4;character->animationLoop=versionB;
                }
            } else if(player)world.pedAnimationRequests.erase(handle);
            else if(character->animationId>=0 && character->waitState==state &&
                    !world.scriptedCharacterAnimations.count(handle)) {
                // A previous owned idle track is no longer driving the new
                // interaction; do not leave the character frozen in it.
                character->animationId=-1;
            }
        }
        return 1;
    }
    if((command==0x00F1||command==0x00E8)&&player) {
        world.playerVehicleEntryTarget=-1;world.playerVehicleEntryAsPassenger=false;
        if(world.inVehicle&&(command==0x00F1||world.occupiedMissionVehicle==values[1].bits)) {
            world.playerExitVehicleRequested=true;world.playerVehicleTransition=true;
        }
        return 1;
    }
    if((command==0x00E9||command==0x00EA)&&player) {
        const auto vehicle=world.missionVehicles.find(values[1].bits);
        if(vehicle==world.missionVehicles.end()||vehicle->second.deleted) {
            error="Player vehicle-entry objective references an unknown vehicle.";return -1;
        }
        // 10BEB8/10BF48 set objective 17/18 on any ped, including the player.
        // The runtime approaches and enters through the normal entry phases.
        world.playerWalkTask.clear();
        world.playerExitVehicleRequested=false;
        world.playerVehicleEntryTarget=values[1].bits;
        world.playerVehicleEntryAsPassenger=command==0x00E9;
        return 1;
    }
    if(command==0x0285||command==0x04B9) {
        if(player) {
            auto& navigation=world.playerNavigation;
            navigation.seekNodes=values[1].bits!=0;
            if(command==0x04B9) {
                navigation.localAvoidanceSolver=values[2].bits!=0;
                navigation.alternateObstructionHandling=values[3].bits!=0;
            }
            navigation.clearRoute();
            return 1;
        }
        if(command==0x04B9) {
            // 417D78 combines 0285, the 04B7 bit and the 04B6 bit.
            character->behavior.localAvoidanceSolver=values[2].bits!=0;
            character->behavior.alternateObstructionHandling=values[3].bits!=0;
        }
        character->seekNodes=values[1].bits!=0;
        character->seekNode=UINT32_MAX;character->seekRoute.clear();
        character->seekRouteCursor=0;character->seekWaitUntil=0;
        return 1;
    }
    if((command==0x0158||command==0x00F5)&&player) {
        if(!std::isfinite(real(1))||!std::isfinite(real(2))) {
            error="Coordinate running requires finite coordinates.";return -1;
        }
        if(!alive)return 1;
        float originOffset=0;
        if(!world.characterOriginOffset||!world.characterOriginOffset(0,originOffset)||
            !std::isfinite(originOffset)||!std::isfinite(world.z)) {
            error="Coordinate running requires the original player model origin.";return -1;
        }
        const float targetZ=(world.z+originOffset)-1.04f+.1f;
        if(!std::isfinite(targetZ)) {error="Coordinate running has an invalid player origin.";return -1;}
        // 10C7A0/10C650 assign objective25/49; 205540 selects sprint separately.
        const auto mode=command==0x00F5?StorylandScriptPlayerMoveMode::Sprint:StorylandScriptPlayerMoveMode::Run;
        world.playerVehicleEntryTarget=-1;world.playerVehicleEntryAsPassenger=false;
        world.playerWalkTask={true,false,real(1),real(2),targetZ,.5f,mode};
        world.playerScriptRunning=true;
        world.playerNavigation.clearRoute();
        if(world.inVehicle)world.playerExitVehicleRequested=true;
        return 1;
    }
    // Native SetObjective(29), 3BB6F8..3BB720, rejects attached actors
    // before touching leader state. This also applies to the mounted player.
    if(command==0x0119&&world.activeVcsCharacterAttachment(handle))return 1;
    if(!character) {
        error="This character behavior requires a non-player actor.";return -1;
    }
    if (command == 0x02A5) { character->criticalHits = values[1].bits != 0; return 1; }
    if (command == 0x035F) {
        const bool enabled = values[1].bits != 0;
        if (!world.entityCollisionStreaming || !world.entityCollisionStreaming(1, handle, enabled)) {
            error = "Character collision residency could not be updated."; return -1;
        }
        character->loadCollision = enabled;
        return 1;
    }
    if (command == 0x015D) {
        std::array<float,4> parameters{};
        std::array<uint8_t,5> personality{};
        if (values[1].bits < 0 || !world.nativePedStat ||
            !world.nativePedStat(character->model, values[1].bits, parameters, personality) ||
            !std::all_of(parameters.begin(), parameters.end(), [](float value) { return std::isfinite(value) && value >= 0; })) {
            error = "The requested native pedestrian statistic record is unavailable."; return -1;
        }
        character->pedStat = values[1].bits;
        auto& behavior = character->behavior;
        behavior.statFleeDistance = parameters[0]; behavior.statHeadingRate = parameters[1];
        behavior.statAttackStrength = parameters[2]; behavior.statDefendWeakness = parameters[3];
        behavior.statFear = personality[0]; behavior.statTemper = personality[1];
        behavior.statLawfulness = personality[2]; behavior.statView = personality[3];
        behavior.statFlags = personality[4];
        return 1;
    }
    switch(command) {
    case 0x0278:character->walletMoney=uint16_t(values[1].bits);character->walletInitialized=true;character->walletForced=true;character->moneyDropped=false;return 1;
    case 0x03DE:character->keepObjectiveOnRelease=(values[1].bits&1)!=0;return 1;
    case 0x0115:character->persistent=true;return 1;
    case 0x00A1:
        character->fearFlags |= uint32_t(values[1].bits);
        character->searchThreats = character->fearFlags != 0;
        return 1;
    case 0x01A4:character->heedThreats=values[1].bits!=0;return 1;
    case 0x01AF:character->onlyDamagedByPlayer=values[1].bits!=0;return 1;
    case 0x01FD:character->bleeding=values[1].bits!=0;return 1;
    case 0x0344:character->ignoreThreatsBehindObjects=values[1].bits!=0;return 1;
    case 0x0348:character->crouchWhenThreatened=values[1].bits!=0;return 1;
    case 0x0213:character->armour=std::clamp(character->armour+float(values[1].bits),0.0f,100.0f);return 1;
    case 0x0338:character->walkingToTarget=false;character->seekRoute.clear();return 1;
    case 0x011A:storylandClearVcsLeader(world,handle,*character);return 1;
    case 0x04D3:storylandClearNearbyVcsFollowers(world,handle,*character);return 1;
    case 0x02BE: {
        int32_t count=0;
        for(const auto& entry:world.missionCharacters)if(entry.second.alive&&entry.second.leader==handle)++count;
        return store(1,count)?1:-1;
    }
    default:break;
    }
    if(command==0x00F1||command==0x00E8) {
        if(command==0x00E8&&character->vehicle!=values[1].bits)return 1;
        const auto exit=world.characterVehicleTasks.find(handle);
        if(exit!=world.characterVehicleTasks.end()&&exit->second.exiting&&
            world.vcsCharacterExitingVehicle(handle))return 1;
        if(character->vehicle<0)return 1;
        character->objective=command;character->objectiveTarget=character->vehicle;
        character->leaderVehicleAfter=0;
        character->objectiveCompleted = false;
        return 1;
    }
    if(command==0x00E9||command==0x00EA) {
        const auto vehicle=world.missionVehicles.find(values[1].bits);
        if(vehicle==world.missionVehicles.end()||vehicle->second.deleted) {
            error="Vehicle-entry objective references an unknown vehicle.";return -1;
        }
        if(command==0x00EA&&vehicle->second.driver>=0) {
            error="Driver-entry objective cannot replace an occupied seat.";return -1;
        }
        const int capacity=world.vehiclePassengerCapacity?world.vehiclePassengerCapacity(values[1].bits):0;
        int seat=character->allowFrontPassenger?0:1;
        if(command==0x00E9) {
            const int permittedCapacity=character->behavior.onlyFrontPassenger?std::min(capacity,1):capacity;
            while(seat<permittedCapacity&&storylandVcsPassenger(world,values[1].bits,seat)>=0)++seat;
            // Native SetObjective queues entry; an occupied permitted seat is
            // retried by the objective rather than terminating its SCM thread.
            if(seat>=permittedCapacity)seat=-1;
        }
        character->objective=int(command);character->objectiveTarget=values[1].bits;
        character->objectiveCompleted = false;
        character->passengerSeat=command==0x00EA?-1:seat;
        return 1;
    }
    if(command==0x00F5||command==0x0158||command==0x00ED) {
        if (!character->alive || character->health <= 0) return 1;
        for(size_t index=1;index<size_t(count);++index)if(!std::isfinite(real(index))) {
            error="Movement objective coordinates must be finite.";return -1;
        }
        character->targetX=command==0x00ED?(real(1)+real(3))*.5f:real(1);
        character->targetY=command==0x00ED?(real(2)+real(4))*.5f:real(2);
        character->objectiveHeight = character->z;
        if (world.groundHeightAt) {
            float height = 0;
            character->objectiveHeight = world.groundHeightAt(
                {character->targetX, character->targetY, 1000}, height) ? height : 20.0f;
        }
        character->walkSpeed=command==0x00F5?5.5f:3.8f;
        character->walkingToTarget=true;character->objective=int(command);
        character->objectiveCompleted = false;
        character->objectiveRadius = command == 0x00ED ?
            std::max(std::fabs(real(3) - real(1)), std::fabs(real(4) - real(2))) * 0.5f : 0.5f;
    } else {
        const int32_t target=values[1].bits;
        if(target==handle||(target!=0&&world.missionCharacters.find(target)==world.missionCharacters.end())) {
            error="Character objective references an unknown or identical target.";return -1;
        }
        if(command==0x0119) {
            character->leader=target;
            character->leaderUpdateAfter=0;
            return 1;
        }
        character->objective=int(command);character->objectiveTarget=target;
        character->objectiveCompleted = false;
        if(command==0x00EF) character->formation=values[2].bits;
    }
    world.characterWanderTasks.erase(handle);
    character->seekRoute.clear();character->seekRouteCursor=0;
    return 1;
}

int executeVcsVehicleAutopilotCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (!vm.vcs) return 0;
    size_t count = 0;
    switch (command) {
    case 0x02DA: case 0x02F4: case 0x046F: case 0x0541:
        count = 5;
        break;
    case 0x024E: case 0x0366:
        count = 4;
        break;
    case 0x04BE:
        count = 3;
        break;
    case 0x0051: case 0x0052: case 0x01FA: case 0x0294: case 0x02B9:
    case 0x02D2: case 0x02EB: case 0x02F2: case 0x02FF: case 0x0300:
    case 0x03A4: case 0x0447:
        count = 2;
        break;
    case 0x004B: case 0x00A7: case 0x02CA: case 0x02F3: case 0x0345:
    case 0x0397: case 0x0398: case 0x03BB: case 0x03D8: case 0x0424: case 0x04D1:
        count = 1;
        break;
    default:
        return 0;
    }
    if (!vm.environment) {
        error = "Vehicle autopilot command requires a live environment.";
        return -1;
    }
    std::array<Operand,5> values{};
    for (size_t index = 0; index < count; ++index) {
        if (!readOperand(vm, values[index])) {
            error = "Vehicle autopilot command has a truncated typed operand.";
            return -1;
        }
    }
    auto& world = *vm.environment;
    const auto real = [&](size_t index) {return operandFloat(values[index]);};
    const auto condition = [&](bool value) {
        updateScmCondition(vm, value, inverted);
        return 1;
    };
    const auto fail = [&](const char* message) {
        error = message;
        return -1;
    };
    const auto storeReal = [&](size_t index, float value) {
        int32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return writeOperand(vm, values[index], bits);
    };
    if (command == 0x02CA) {
        if (!std::isfinite(real(0))) return fail("Vehicle entry search multiplier must be finite.");
        // 3D51F8 stores a signed halfword; 253FA4/254014 initialize it to one.
        world.enterCarRangeMultiplier = int16_t(uint16_t(storylandVcsSignedConversion(real(0))));
        return 1;
    }
    if (command == 0x024E || command == 0x0366) {
        std::array<float,4> bounds{};
        for (size_t index = 0; index < bounds.size(); ++index) {
            bounds[index] = real(index);
            if (!std::isfinite(bounds[index])) return fail("Vehicle area coordinates must be finite.");
        }
        if (command == 0x024E) {
            if (!world.createRandomParkedVehicle) return fail("Car park creation requires loaded native vehicle models.");
            world.createRandomParkedVehicle({bounds[0], bounds[1], bounds[2]}, bounds[3]);
            return 1;
        }
        if (!world.unlockVehicleDoorsInArea) return fail("Area unlock requires the live vehicle pool.");
        if (bounds[0] > bounds[2]) std::swap(bounds[0], bounds[2]);
        if (bounds[1] > bounds[3]) std::swap(bounds[1], bounds[3]);
        world.unlockVehicleDoorsInArea(bounds);
        for (auto& item : world.missionVehicles) {
            auto& vehicle = item.second;
            if (!vehicle.deleted && vehicle.x >= bounds[0] && vehicle.x <= bounds[2] &&
                vehicle.y >= bounds[1] && vehicle.y <= bounds[3]) vehicle.doorLock = 1;
        }
        return 1;
    }
    const int32_t handle = values[0].bits;
    const auto found = world.missionVehicles.find(handle);
    if (found == world.missionVehicles.end() || found->second.deleted) {
        return fail("Vehicle autopilot command references an unknown vehicle.");
    }
    auto& vehicle = found->second;
    if (command == 0x0424) return condition(vehicle.siren);
    if (command == 0x0398) {
        bool entering = world.vehicleEntryInProgress && world.vehicleEntryInProgress(handle);
        for (const auto& item : world.missionCharacters) {
            const auto& character = item.second;
            if (character.alive && character.vehicle < 0 && character.objectiveTarget == handle &&
                (character.objective == 0x00EA || character.objective == 0x00F0) &&
                std::hypot(character.x - vehicle.x, character.y - vehicle.y, character.z - vehicle.z) <= 3) {
                entering = true;
            }
        }
        return condition(entering);
    }
    if (command == 0x0397) {
        if (!world.vehicleNativeType || !world.vehicleBoatBuoyant) return fail("Boat buoyancy query requires native vehicle physics.");
        const int32_t type = world.vehicleNativeType(handle);
        if (type != 1 && type != 2) return fail("Boat buoyancy query requires a native boat.");
        return condition(world.vehicleBoatBuoyant(handle));
    }
    if (command == 0x03A4) {
        if (!values[1].reference || !world.consumeVehicleCollision) return fail("Collision query requires a writable output and the live contact pool.");
        return writeOperand(vm, values[1], world.consumeVehicleCollision(handle)) ? 1 : -1;
    }
    if (command == 0x0541) {
        for (size_t index = 2; index < 5; ++index) {
            if (!values[index].reference) return fail("Path coordinate query requires writable outputs.");
        }
        std::array<float,3> point{};
        if (!world.vehiclePathCoordinate ||
            !world.vehiclePathCoordinate(handle, values[1].bits, point)) {
            return fail("Path coordinate query requires an initialized native road route and cache index0..3.");
        }
        for (size_t axis = 0; axis < point.size(); ++axis) storeReal(axis + 2, point[axis]);
        return 1;
    }
    if (command == 0x046F) {
        for (size_t index = 1; index < 5; ++index) {
            if (!values[index].reference) return fail("Vehicle screen margins require writable outputs.");
        }
        std::array<float,3> center{};
        float radius = 0;
        if (!world.vehicleBoundingSphere || !world.screenCoordinates ||
            !world.vehicleBoundingSphere(handle, center, radius)) {
            return fail("Vehicle screen margins require the native collision sphere and active camera lens.");
        }
        std::array<float,4> margins{};
        for (size_t edge = 0; edge < margins.size(); ++edge) {
            auto point = center;
            point[edge < 2 ? 0 : 2] += (edge % 2 ? radius : -radius);
            std::array<float,2> screen{};
            if (world.screenCoordinates(point, screen) && screen[0] > 6.4f && screen[0] < 633.6f &&
                screen[1] > 4.48f && screen[1] < 443.52f) {
                margins[edge] = edge == 0 ? screen[0] : edge == 1 ? 640 - screen[0] :
                    edge == 2 ? 448 - screen[1] : screen[1];
            }
            storeReal(edge + 1, margins[edge]);
        }
        return 1;
    }
    if (command == 0x004B || command == 0x0052 || command == 0x01FA) {
        const int32_t requestedMission = command == 0x004B ? 1 : command == 0x01FA ? 15 : values[1].bits;
        const int32_t mission = int8_t(uint8_t(requestedMission));
        if (mission != 0 && mission != 1 && mission != 3 && mission != 5 && mission != 6 &&
            mission != 10 && mission != 11 &&
            mission != 15 && mission != 16 && mission != 18 && mission != 19 && mission != 20 && mission != 22) {
            return fail("Vehicle mission supports native stop, cruise, player pursuit, targeted blocking/ramming, helicopter and plane routes.");
        }
        if ((mission == 20 || mission == 22) && (!world.vehicleHandlingClass ||
            world.vehicleHandlingClass(handle) != (mission == 20 ? 3 : 5) ||
            !world.vehicleFlightAvailable || !world.vehicleFlightAvailable(handle))) {
            return fail("Aircraft mission requires the matching native flying handling profile.");
        }
        if (command == 0x01FA) {
            const auto target = world.missionVehicles.find(values[1].bits);
            if (target == world.missionVehicles.end() || target->second.deleted || target->first == handle) {
                return fail("Vehicle blocking requires a distinct live target vehicle.");
            }
            vehicle.aiTargetVehicle = target->first;
            vehicle.cruiseSpeed = std::max(vehicle.cruiseSpeed, 5.0f);
        }
        if (command != 0x0052 || vehicle.aiMission != requestedMission) {
            vehicle.aiMissionStartedAt = world.gameMilliseconds;
            vehicle.aiMission = int8_t(mission);
            if (command == 0x0052 && (mission == 16 || mission == 18 || mission == 19))
                vehicle.autoPilotRoutePending = true;
        }
        vehicle.aiActive = true;
        vehicle.drivingToTarget = false;
        if (command == 0x004B) vehicle.cruiseSpeed = std::max(vehicle.cruiseSpeed, 5.0f / 6.0f);
        return 1;
    }
    if (command == 0x0051) {
        const int8_t style = int8_t(uint8_t(values[1].bits));
        if (style < 0 || style > 4) return fail("Native driving style requires a supported road policy0..4.");
        vehicle.drivingStyle = style;
        return 1;
    }
    if (command == 0x00A7 || command == 0x04D1) {
        vehicle.alarmMilliseconds = command == 0x00A7 ? UINT16_MAX : 15000;
        vehicle.alarmChangedByScript = true;
        return 1;
    }
    if (command == 0x0294) vehicle.avoidLevelTransitions = values[1].bits != 0;
    else if (command == 0x02B9) {
        vehicle.fastLane = values[1].bits != 0;
        vehicle.lanePolicySet = true;
    } else if (command == 0x02D2) {
        vehicle.routeSeed = uint16_t(values[1].bits);
        vehicle.routeSeedChanged = true;
    } else if (command == 0x02EB) vehicle.convoy = values[1].bits != 0;
    else if (command == 0x0300) vehicle.straightLineDistance = uint8_t(values[1].bits);
    else if (command == 0x04BE) {
        if (!std::isfinite(real(1)) || !std::isfinite(real(2)) || real(1) <= 0 || real(2) <= 0 ||
            real(1) > 1000 || real(2) > 1000) return fail("Physical mass multipliers must be finite and within(0,1000].");
        vehicle.massMultiplier = real(1);
        vehicle.turnMassMultiplier = real(2);
        vehicle.heavy = false;
    } else if (command == 0x03BB) {
        if (!world.vehicleNativeType || world.vehicleNativeType(handle) != 0) return fail("Rear door processing requires a native automobile.");
        for (size_t door : {size_t(4), size_t(5)}) {
            if (vehicle.doorDamage[door] != 3) vehicle.doorOpenRatios[door] = 1;
        }
        vehicle.doorsChangedByScript = true;
    } else if (command == 0x0447) {
        if (!world.vehicleNativeType || world.vehicleNativeType(handle) != 0 || !std::isfinite(real(1))) {
            return fail("Visible damage requires a native automobile and a finite damage argument.");
        }
        // 3D7E30 draws once for each native component5..17. At fixed impulse128
        // all six native part coefficients exceed the150 threshold. The amount
        // argument reaches retail3F48B0, whose body returns without any effect.
        for (size_t component = 0; component < 13; ++component) {
            const float random = float(world.nextNativeRandom()) / 2147483647.0f;
            if (random > .6f) continue;
            if (component < vehicle.doorDamage.size()) {
                auto& status = vehicle.doorDamage[component];
                if (status < 3) ++status;
            } else {
                const size_t panel = component - vehicle.doorDamage.size();
                auto& status = vehicle.panelDamage[panel];
                if (status < 3) ++status;
                if (panel < vehicle.lightDamage.size()) vehicle.lightDamage[panel] = 1;
            }
            vehicle.visiblyDamaged = true;
        }
        vehicle.doorsChangedByScript = true;
    } else {
        if (!world.vehicleHandlingClass) return fail("Aircraft control requires native handling metadata.");
        const int32_t handlingClass = world.vehicleHandlingClass(handle);
        const bool plane = command == 0x02F4;
        if (handlingClass != (plane ? 5 : 3)) return fail("Aircraft control references an incompatible native aircraft.");
        if (!world.vehicleFlightAvailable || !world.vehicleFlightAvailable(handle)) {
            return fail("Aircraft control requires the model's native flying handling profile.");
        }
        if (command == 0x02DA || command == 0x02F4) {
            for (size_t index = 1; index <= 3; ++index) {
                if (!std::isfinite(real(index))) return fail("Aircraft destination must be finite.");
            }
            vehicle.targetX = real(1);
            vehicle.targetY = real(2);
            vehicle.targetZ = real(3);
            vehicle.cruiseSpeed = float(uint8_t(values[4].bits)) * (5.0f / 6.0f);
            vehicle.aiMission = plane ? 22 : 20;
            vehicle.aiActive = true;
            vehicle.scriptStatus = 3;
            vehicle.drivingToTarget = false;
            vehicle.aiMissionStartedAt = world.gameMilliseconds;
        } else if (command == 0x02F2) {
            if (!std::isfinite(real(1))) return fail("Helicopter orientation must be finite.");
            vehicle.heliOrientation = float(std::fmod((double(real(1)) - 90) * .01745329251994329577,
                6.283185307179586));
            if (vehicle.heliOrientation < 0) vehicle.heliOrientation += 6.283185307179586f;
        } else if (command == 0x02F3) vehicle.heliOrientation = -1;
        else if (command == 0x02FF) vehicle.heliStabiliser = values[1].bits != 0;
        else if (command == 0x0345) vehicle.heliCrashing = true;
        else if (command == 0x03D8) {
            vehicle.rotorSpeed = .22f;
            vehicle.rotorChangedByScript = true;
        }
    }
    return 1;
}

int executeVcsVehicleRecoveryCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (!vm.vcs) {
        return 0;
    }
    size_t count = 0;
    switch (command) {
    case 0x0333: case 0x03BF:
        count = 6;
        break;
    case 0x01BF: case 0x03B3: case 0x03B9:
        count = 4;
        break;
    case 0x014A: case 0x01C3: case 0x01F2: case 0x0230: case 0x023D:
    case 0x026D: case 0x0290: case 0x02D9: case 0x0316: case 0x0334:
    case 0x0495: case 0x0497: case 0x0498: case 0x0499: case 0x049A:
    case 0x0533:
        count = 2;
        break;
    case 0x013B: case 0x01C0: case 0x0251: case 0x0301: case 0x031C:
    case 0x031F: case 0x03D1: case 0x04A5: case 0x04DD:
        count = 1;
        break;
    default:
        return 0;
    }
    if (!vm.environment) {
        error = "Vehicle command requires a live environment.";
        return -1;
    }
    std::array<Operand,6> values{};
    for (size_t index = 0; index < count; ++index) {
        if (!readOperand(vm, values[index])) {
            error = "Vehicle command has a truncated typed operand.";
            return -1;
        }
    }
    auto& world = *vm.environment;
    const auto store = [&](size_t index, int32_t value) {
        if (writeOperand(vm, values[index], value)) {
            return 1;
        }
        error = "Vehicle query requires a writable output.";
        return -1;
    };
    if (command == 0x014A) {
        // 0x2D324C ignores the nominal player parameter and changes CPlayerInfo.
        world.playerCarBrakes = values[1].bits != 0;
        return 1;
    }
    if (command == 0x0333 || command == 0x03BF) {
        if (!values[5].reference || !world.findAmbientVehicleNoSave) {
            error = "Ambient vehicle query requires the live vehicle pool and a writable output.";
            return -1;
        }
        std::array<float,4> bounds{};
        for (size_t index = 0; index < bounds.size(); ++index) {
            bounds[index] = operandFloat(values[index]);
            if (!std::isfinite(bounds[index])) {
                error = "Ambient vehicle query requires finite bounds.";
                return -1;
            }
        }
        // Native 0x3A61B0 normalizes bounds and includes the edges. Scanning
        // starts at the highest pool slot without adding a cleanup owner.
        if (bounds[0] > bounds[2]) std::swap(bounds[0], bounds[2]);
        if (bounds[1] > bounds[3]) std::swap(bounds[1], bounds[3]);
        return store(5, world.findAmbientVehicleNoSave(bounds, values[4].bits, command == 0x03BF));
    }
    const int32_t handle = values[0].bits;
    const auto found = world.missionVehicles.find(handle);
    if (found == world.missionVehicles.end() || found->second.deleted) {
        error = "Vehicle command references an unknown vehicle.";
        return -1;
    }
    auto& vehicle = found->second;
    if (command == 0x0251) {
        updateScmCondition(vm, vehicle.visiblyDamaged, inverted);
        return 1;
    }
    if (command == 0x0497 || command == 0x0498) {
        return store(1, command == 0x0497 ? vehicle.colorSet : vehicle.scriptColorSet);
    }
    if (command == 0x0499 || command == 0x049A) {
        if (!world.vehicleColorSetCount) {
            error = "Vehicle colour count requires native model metadata.";
            return -1;
        }
        const int32_t number = world.vehicleColorSetCount(handle, command == 0x049A);
        if (number < 0) {
            error = "Vehicle colour count cannot resolve native model metadata.";
            return -1;
        }
        return store(1, number);
    }
    if (command == 0x0495) {
        std::array<int32_t,2> colors{};
        if (!world.vehicleColorSet || !world.vehicleColorSet(handle, false, values[1].bits, colors)) {
            error = "Vehicle colour set is outside the native model's palette.";
            return -1;
        }
        vehicle.primaryColor = colors[0];
        vehicle.secondaryColor = colors[1];
        vehicle.colorSet = int8_t(uint8_t(values[1].bits));
        vehicle.scriptColorSet = -1;
        return 1;
    }
    if (command == 0x04DD) {
        vehicle.clumpVisible = false;
        return 1;
    }
    if (command == 0x023D) {
        vehicle.strong = values[1].bits != 0;
        return 1;
    }
    if (command == 0x0334) {
        vehicle.canBurstTyres = values[1].bits != 0;
        return 1;
    }
    if (command == 0x0290) {
        const float multiplier = operandFloat(values[1]);
        if (!std::isfinite(multiplier)) {
            error = "Vehicle traction requires a finite multiplier.";
            return -1;
        }
        vehicle.tractionMultiplier = multiplier;
        return 1;
    }
    if (command == 0x03B3 || command == 0x03B9) {
        std::array<float,3> vector{};
        for (size_t index = 0; index < vector.size(); ++index) {
            vector[index] = operandFloat(values[index + 1]);
            if (!std::isfinite(vector[index])) {
                error = "Vehicle rotation requires three finite values.";
                return -1;
            }
        }
        if (command == 0x03B3) {
            // Native stores radians per 50-Hz step (input * 0.02). Runtime
            // integrates radians per second, so the original values are kept.
            vehicle.angularVelocity = vector;
            vehicle.angularVelocityChangedByScript = true;
        } else {
            vehicle.rotation = vector;
            vehicle.heading = vector[2];
            vehicle.headingChangedByScript = true;
            vehicle.rotationChangedByScript = true;
        }
        return 1;
    }
    if (!world.vehicleNativeType) {
        error = "Vehicle command requires native subclass metadata.";
        return -1;
    }
    const int type = world.vehicleNativeType(handle);
    if (type < 0) {
        error = "Vehicle command cannot resolve its native subclass.";
        return -1;
    }
    if (command == 0x013B) {
        if (vehicle.wrecked || vehicle.health <= 0) {
            return 1;
        }
        const int explosionType = type == 1 || type == 2 ? 5 : type == 4 || type == 5 ? 6 : 3;
        if (!storylandAddScriptExplosion(world, explosionType,
            {vehicle.x, vehicle.y, vehicle.z}, error,
            {StorylandMissionEnvironment::DamageSourceKind::Vehicle, handle})) {
            return -1;
        }
        vehicle.health = 0;
        vehicle.alive = false;
        vehicle.wrecked = true;
        vehicle.scriptStatus = 5;
        vehicle.healthChangedByScript = true;
        vehicle.visiblyDamaged = true;
        vehicle.siren = false;
        vehicle.drivingToTarget = false;
        for (auto& entry : world.missionCharacters) {
            auto& character = entry.second;
            if (character.vehicle == handle && !character.attached) {
                character.health = 0;
                character.alive = false;
            }
        }
        if (world.inVehicle && world.occupiedMissionVehicle == handle) {
            world.health = 0;
        }
        return 1;
    }
    if (command == 0x01BF || command == 0x01C0 || command == 0x01C3 || command == 0x01F2) {
        if (type != 1 && type != 2) {
            error = "Boat command requires a native boat or jetski.";
            return -1;
        }
        if (command == 0x01F2) {
            vehicle.boatAnchored = values[1].bits != 0;
        } else if (command == 0x01C3) {
            const float speed = operandFloat(values[1]);
            if (!std::isfinite(speed)) {
                error = "Boat cruise speed must be finite.";
                return -1;
            }
            const int8_t nativeSpeed = int8_t(uint8_t(storylandVcsSignedConversion(speed)));
            vehicle.cruiseSpeed = float(nativeSpeed) * (5.0f / 6.0f);
        } else if (command == 0x01C0) {
            vehicle.boatAutopilot = false;
            vehicle.drivingToTarget = false;
            vehicle.cruiseSpeed = 0;
            vehicle.scriptStatus = 3;
        } else {
            std::array<float,3> destination{};
            for (size_t index = 0; index < destination.size(); ++index) {
                destination[index] = operandFloat(values[index + 1]);
                if (!std::isfinite(destination[index])) {
                    error = "Boat destination requires finite coordinates.";
                    return -1;
                }
            }
            if (destination[2] == -100.0f || destination[2] <= -250.0f) {
                if (!world.vehicleWaterHeight ||
                    !world.vehicleWaterHeight(destination[0], destination[1], destination[2])) {
                    error = "Boat destination requires the native water surface.";
                    return -1;
                }
            }
            vehicle.targetX = destination[0];
            vehicle.targetY = destination[1];
            vehicle.targetZ = destination[2];
            vehicle.cruiseSpeed = std::max(5.0f / 6.0f, vehicle.cruiseSpeed);
            vehicle.drivingToTarget = true;
            vehicle.boatAutopilot = true;
            vehicle.scriptStatus = 3;
        }
        return 1;
    }
    if (command == 0x0230) {
        if (type == 0 || type == 6) {
            vehicle.watertight = values[1].bits != 0;
        }
        return 1;
    }
    if (command == 0x026D) {
        // The Quad constructor at 0x401B58 calls CAutomobile at 0x1337F0.
        if (type != 0 && type != 9) {
            error = "Upside-down damage control requires a native automobile.";
            return -1;
        }
        vehicle.upsideDownDamageDisabled = values[1].bits != 0;
        return 1;
    }
    if (command == 0x02D9 || command == 0x0316) {
        if (type != 0 && type != 6 && type != 8 && type != 9) {
            error = "Tyre command requires a native wheeled vehicle.";
            return -1;
        }
        int32_t wheel = command == 0x0316 ? uint8_t(values[1].bits) : values[1].bits;
        const size_t wheelCount = type == 6 ? 2 : 4;
        if (command == 0x02D9 && wheel == 4) {
            const bool burst = std::any_of(vehicle.tyreStatus.begin(),
                vehicle.tyreStatus.begin() + wheelCount, [](uint8_t status) { return status == 1; });
            updateScmCondition(vm, burst, inverted);
            return 1;
        }
        if (type == 6 && (wheel == 2 || wheel == 3)) {
            wheel -= 2;
        }
        if (command == 0x0316 && (type == 0 || type == 9) && wheel >= 13 && wheel <= 16) {
            static constexpr std::array<int32_t,4> componentWheels{0,2,1,3};
            wheel = componentWheels[size_t(wheel - 13)];
        }
        if (wheel < 0 || size_t(wheel) >= wheelCount) {
            error = "Tyre index is outside the native wheel array.";
            return -1;
        }
        if (command == 0x02D9) {
            updateScmCondition(vm, vehicle.tyreStatus[size_t(wheel)] == 1, inverted);
        } else if (vehicle.canBurstTyres && vehicle.model != 246) {
            vehicle.tyreStatus[size_t(wheel)] = 1;
        }
        return 1;
    }
    if (type != 0 && !(command == 0x031C && type == 4) &&
        !(command == 0x0533 && (type == 4 || type == 5))) {
        error = "Door command requires a compatible native automobile body.";
        return -1;
    }
    if (command == 0x0301) {
        if (vehicle.doorDamage[1] < 2) {
            vehicle.doorOpenRatios[1] = 1;
            vehicle.doorsChangedByScript = true;
        }
    } else if (command == 0x031F) {
        if (vehicle.doorDamage[1] < 2) {
            vehicle.doorDamage[1] = 2;
        }
        vehicle.bootAngularVelocity = -2;
        vehicle.doorsChangedByScript = true;
    } else if (command == 0x031C) {
        for (size_t door = 1; door < vehicle.doorDamage.size(); ++door) {
            vehicle.doorOpenRatios[door] = 0;
            if (door >= 2 && vehicle.doorDamage[door] == 2) {
                vehicle.doorDamage[door] = 0;
            } else if (door >= 2 && vehicle.doorDamage[door] == 4) {
                vehicle.doorDamage[door] = 1;
            }
        }
        vehicle.bootAngularVelocity = 0;
        vehicle.doorsChangedByScript = true;
    } else if (command == 0x03D1) {
        vehicle.doorDamage[1] = 3;
        vehicle.visiblyDamaged = true;
        vehicle.doorsChangedByScript = true;
    } else if (command == 0x04A5) {
        // Retail removes an already damaged bonnet; an intact bonnet remains.
        if (vehicle.doorDamage[0] != 0) {
            vehicle.doorDamage[0] = 3;
            vehicle.visiblyDamaged = true;
            vehicle.doorsChangedByScript = true;
        }
    } else if (command == 0x0533) {
        // Native translation table 0x4BE2E0 maps RF, RR, LF, LR.
        static constexpr std::array<size_t,4> doors{3,5,2,4};
        const int32_t requested = values[1].bits;
        if (requested < 0 || size_t(requested) >= doors.size()) {
            error = "Door removal requires native index 0..3.";
            return -1;
        }
        vehicle.doorDamage[doors[size_t(requested)]] = 3;
        vehicle.visiblyDamaged = true;
        vehicle.doorsChangedByScript = true;
    }
    return 1;
}

int executeVcsVehicleExtensionCommand(StorylandMissionVm& vm,uint16_t command,
    bool inverted,std::string& error) {
    if(!vm.vcs)return 0;
    size_t count=0;
    switch(command) {
    case 0x0281:case 0x02FC:case 0x03A8:count=0;break;
    case 0x0147:case 0x0148:case 0x0149:case 0x015B:case 0x01F7:case 0x01FF:
    case 0x0214:case 0x0215:case 0x0242:case 0x0243:case 0x0262:case 0x02D1:
    case 0x02F7:case 0x0335:case 0x034E:case 0x0394:case 0x04A7:case 0x04BA:count=1;break;
    case 0x0146:case 0x014F:case 0x015C:case 0x025C:case 0x028F:case 0x02CD:
    case 0x02D6:case 0x0321:case 0x03A7:case 0x04A6:case 0x04A8:case 0x04A9:case 0x04AA:
    case 0x04AB:case 0x04AD:case 0x04B8:case 0x04D2:count=2;break;
    case 0x04AC:count=4;break;
    case 0x02BF:count=6;break;
    default:return 0;
    }
    if(!vm.environment) {error="Vehicle systems command requires a live environment.";return -1;}
    std::array<Operand,6> values{};
    for(size_t i=0;i<count;++i)if(!readOperand(vm,values[i])) {
        error="Vehicle systems command has a truncated typed operand.";return -1;
    }
    auto& world=*vm.environment;
    const auto condition=[&](bool value){updateScmCondition(vm,value,inverted);return 1;};
    const auto output=[&](size_t index) {
        if(values[index].reference&&values[index].variable<
            (values[index].global?vm.globals.size():vm.locals.size()))return true;
        error="Vehicle systems query requires a writable output.";return false;
    };
    const auto store=[&](size_t index,int32_t value) {return writeOperand(vm,values[index],value)?1:-1;};
    if(command==0x0148) {world.freeBombs=values[0].bits!=0;return 1;}
    if(command==0x01FF) {world.freeResprays=values[0].bits!=0;return 1;}
    if(command==0x034E) {world.taxiBoost=values[0].bits!=0;return 1;}
    if(command==0x02D1) {world.remoteDetonationEnabled=values[0].bits!=0;return 1;}
    if(command==0x02F7) {world.remoteContactDetonation=values[0].bits!=0;return 1;}
    if(command==0x0394) {
        const float height=operandFloat(values[0]);
        if(!std::isfinite(height)) {error="RC helicopter height limit must be finite.";return -1;}
        world.rcHeliHeightLimit=height;return 1;
    }
    if(command==0x015B||command==0x02CD) {
        if(values[0].bits!=0) {error="Remote control requires the live player.";return -1;}
        const auto remote=world.missionVehicles.find(world.remoteVehicle);
        const bool active=remote!=world.missionVehicles.end()&&!remote->second.deleted&&!remote->second.wrecked;
        if(command==0x015B)return condition(active);
        if(!output(1))return -1;
        return store(1,active?world.remoteVehicle:-1);
    }
    if(command==0x02BF) {
        if(values[0].bits!=0) {
            if(error.empty())error="Remote control requires the live player.";return -1;
        }
        std::array<float,3> position{operandFloat(values[1]),operandFloat(values[2]),operandFloat(values[3])};
        const float heading=operandFloat(values[4]);
        if(!std::isfinite(heading)||!std::all_of(position.begin(),position.end(),[](float v){return std::isfinite(v);})) {
            error="Remote vehicle coordinates and heading must be finite.";return -1;
        }
        if(position[2]==-100||position[2]<=-250) {
            if(!world.groundHeightAt||!world.groundHeightAt(position,position[2])) {
                error="Remote vehicle ground placement needs native ground collision.";return -1;
            }
        }
        int32_t model=int16_t(uint16_t(values[5].bits));
        if(model<0&&world.resolveObjectModel)model=world.resolveObjectModel(model);
        if(model<0||!world.nativeModelAvailable||!world.nativeModelAvailable(model)) {
            error="Remote vehicle model is not available in the native archive.";return -1;
        }
        if(!storylandScriptVehicleCapacityAvailable(world.missionVehicles,256)||world.nextMissionVehicleHandle<1||
            world.nextMissionVehicleHandle==INT32_MAX||world.missionVehicles.count(world.nextMissionVehicleHandle)) {
            error="Remote control needs a free vehicle pool slot.";return -1;
        }
        StorylandMissionEnvironment::VehicleScriptState car;
        car.model=model;car.x=position[0];car.y=position[1];car.z=position[2];car.heading=heading;
        car.headingChangedByScript=true;car.driver=-1;car.aiActive=false;
        const int32_t handle=world.nextMissionVehicleHandle++;
        const auto previous=world.missionVehicles.find(world.remoteVehicle);
        if(previous!=world.missionVehicles.end())previous->second.persistent=false;
        world.missionVehicles.emplace(handle,car);world.remoteVehicle=handle;
        if(vm.isMissionThread)world.missionCleanupVehicles.insert(handle);
        return 1;
    }
    if(command==0x0281||command==0x02FC||command==0x03A8) {
        if(command==0x0281)return storylandExplodeScriptVehicle(world,world.remoteVehicle,error)?1:-1;
        const auto remote=world.missionVehicles.find(world.remoteVehicle);
        if(remote!=world.missionVehicles.end()) {
            remote->second.persistent=false;
            if(command==0x02FC) {
                remote->second.deleted=true;remote->second.alive=false;
                world.vcsVehicleFlag28.erase(remote->first);
                world.vcsGarageStorableMissionVehicles.erase(remote->first);
                world.photographedVehicles.erase(remote->first);
                world.releaseCharacterVehicleReferences(remote->first);
                remote->second.visible=false;remote->second.collisionEnabled=false;
            }
        }
        world.remoteVehicle=-1;
        if(command==0x03A8) {
            // Original2D5438 calls CCamera::Restore after releasing the remote pointer.
            world.camera.active=false;world.camera.restoreRequested=true;
            world.camera.cutsceneOwned=false;world.camera.fieldOfView=70;world.camera.roll=0;
            world.camera.interpolationScripted=false;world.camera.transitioning=false;
            world.camera.transitionPending=false;world.camera.vehicleUpdated=false;
            world.cameraFocus={};world.cameraLookAt={};
            world.cameraZoom.ped=false;world.cameraZoom.vehicle=false;
        }
        return 1;
    }
    if(command==0x0146||command==0x0147||command==0x01F7||command==0x0214||command==0x0215||
        command==0x0242||command==0x0243||command==0x025C||command==0x0262||command==0x028F) {
        const int32_t handle=(command==0x0214||command==0x0215||command==0x0242||command==0x0243)?
            values[0].bits:int16_t(uint16_t(values[0].bits));
        const auto found=std::find_if(world.garages.begin(),world.garages.end(),[&](const auto& g){return g.handle==handle;});
        if(found==world.garages.end()) {error="Garage command references an unknown garage.";return -1;}
        auto& garage=*found;
        if(command==0x0147)return condition(garage.state==5);
        if(command==0x0242)return condition(garage.state==1||garage.state==4);
        if(command==0x0243)return condition(garage.state==0);
        if(command==0x01F7) {const bool done=garage.resprayDone;garage.resprayDone=false;return condition(done);}
        if(command==0x025C) {
            const int group=storylandImportGarageIndex(garage.type);
            const uint32_t bit=(uint32_t(values[1].bits)-1)&31;
            return condition(group>=0&&(world.importGarageMasks[size_t(group)]&(uint32_t(1)<<bit))!=0);
        }
        if(command==0x028F) {
            if(!world.vehicleWorldCorners) {error="Garage containment needs native vehicle collision bounds.";return -1;}
            return condition(storylandGarageContainsVehicle(world,garage,values[1].bits));
        }
        if(command==0x0146) {
            const int32_t car=values[1].bits;
            if(car>=0&&(world.missionVehicles.find(car)==world.missionVehicles.end()||world.missionVehicles.at(car).deleted)) {
                error="Garage target references an unknown vehicle.";return -1;
            }
            garage.targetVehicle=car<0?-1:car;
            if(car>=0&&garage.state==5)garage.state=0;
        } else if(command==0x0214) {
            if(garage.state==0||garage.state==2||garage.state==5)garage.state=3;
        } else if(command==0x0215) {
            if(garage.state==1||garage.state==3)garage.state=2;
        } else garage.cameraFollowsPlayer=true;
        return 1;
    }
    if(command==0x0321||command==0x04AA||command==0x04B8||command==0x04BA) {
        const auto found=std::find_if(world.objects.begin(),world.objects.end(),
            [&](const auto& o){return o.handle==values[0].bits;});
        if(found==world.objects.end()) {error="Object contact command references an unknown object.";return -1;}
        auto& object=*found;
        if(command==0x04BA) {object.collided=false;object.collidedVehicles.clear();return 1;}
        if(command==0x04AA) {object.ropeLiftable=values[1].bits!=0;return 1;}
        const auto car=world.missionVehicles.find(values[1].bits);
        if(car==world.missionVehicles.end()||car->second.deleted) {error="Object contact references an unknown vehicle.";return -1;}
        if(command==0x0321) {object.ignoredVehicle=car->first;return 1;}
        return condition(std::find(object.collidedVehicles.begin(),object.collidedVehicles.end(),car->first)!=object.collidedVehicles.end());
    }
    const int32_t handle=values[0].bits;
    const auto found=world.missionVehicles.find(handle);
    if(found==world.missionVehicles.end()||found->second.deleted) {error="Vehicle command references an unknown vehicle.";return -1;}
    auto& vehicle=found->second;
    if(command==0x03A7) {
        if(!world.vehicleHandlingClass) {
            error="Remote takeover requires the original native handling class.";return -1;
        }
        const int32_t handling=world.vehicleHandlingClass(handle);
        if(handling<1||handling>5) {
            error="Remote takeover could not resolve its native handling class.";return -1;
        }
        if(handling==3||handling==5) {
            error="Remote aircraft requires the original mode18 camera controller.";return -1;
        }
        if(!StorylandVcsVehicleCamera::validate(world,handle,16,error))return -1;
        const int32_t previousMode=world.activeCameraMode?world.activeCameraMode():
            world.camera.active?(world.cameraFocus.kind?world.cameraFocus.mode:15):4;
        const bool reset=!world.camera.active||world.cameraFocus.kind!=3||
            world.cameraFocus.mode!=16||world.cameraFocus.handle!=handle;
        const int32_t switchStyle=values[1].bits!=0?1:2;
        vehicle.scriptStatus=12;vehicle.aiActive=false;vehicle.drivingToTarget=false;
        world.remoteVehicle=handle;
        world.remoteDetonationEnabled=false;
        world.cameraFocus.kind=3;world.cameraFocus.handle=handle;
        world.cameraFocus.mode=16;world.cameraFocus.switchStyle=switchStyle;
        world.camera.active=true;world.camera.restoreRequested=false;world.camera.cutsceneOwned=false;
        world.camera.mode=switchStyle;world.camera.fieldOfView=60;world.camera.roll=0;
        world.camera.vehicleUpdated=false;world.camera.vehicleBetaSpeed=0;world.camera.vehicleAligning=false;
        if(reset)world.vcsRemoteCameraUpsideDownOffset=0;
        if(switchStyle==1&&!world.camera.interpolationScripted) {
            world.camera.interpolationMilliseconds=1350;
            world.camera.interpolationWeight=previousMode==15?.05f:.25f;
        }
        world.camera.transitioning=switchStyle==1&&world.camera.interpolationMilliseconds>0;
        world.camera.transitionPending=world.camera.transitioning;
        if(vm.isMissionThread)world.missionCleanupVehicles.insert(handle);
        return 1;
    }
    if(command==0x0149)return condition(vehicle.bombType!=0);
    if(command==0x014F)return condition(vehicle.bombType==values[1].bits);
    if(command==0x015C) {
        storylandInstallVehicleBomb(world,handle,vehicle,values[1].bits);
        return 1;
    }
    if(command==0x02D6) {vehicle.contactExplosion=values[1].bits!=0;return 1;}
    if(command==0x04AB) {vehicle.ropeLiftable=values[1].bits!=0;return 1;}
    if(command==0x04D2) {
        vehicle.bikeStandingOverride=(uint8_t(values[1].bits)&1)!=0;
        vehicle.bikeStandingChangedByScript=true;
        return 1;
    }
    if(command==0x0335) {
        // Retail 0x3D62A0: hunter rounds are rate limited to more than150ms.
        if(uint32_t(world.gameMilliseconds-vehicle.lastHunterShot)<=150)return 1;
        if(!world.fireHunterGun||!world.fireHunterGun(handle,error)) {
            if(error.empty())error="Hunter cannon requires its live weapon consumer.";return -1;
        }
        vehicle.lastHunterShot=world.gameMilliseconds;return 1;
    }
    if(command==0x04A6)return storylandCreateVehicleRope(world,handle,values[1].bits,error)?1:-1;
    if(command==0x04A7) {storylandRemoveVehicleRope(world,handle);return 1;}
    if(command==0x04AC) {
        std::array<float,3> offset{operandFloat(values[1]),operandFloat(values[2]),operandFloat(values[3])};
        if(!std::all_of(offset.begin(),offset.end(),[](float v){return std::isfinite(v);})) {
            error="Rope anchor offset must be finite.";return -1;
        }
        vehicle.ropeOffset=offset;return 1;
    }
    if(!output(1))return -1;
    const auto rope=world.vehicleRopes.find(handle);
    const int32_t result=rope==world.vehicleRopes.end()?-1:command==0x04A8?rope->second.attachedObject:
        command==0x04A9?rope->second.attachedVehicle:rope->second.hookObject;
    return store(1,result);
}

int executeVcsRestoredVehicleCommand(StorylandMissionVm& vm,uint16_t command,
    bool inverted,std::string& error) {
    if(!vm.vcs)return 0;
    int count=0;
    switch(command) {
    case 0x0315:count=7;break;
    case 0x03B4:case 0x0422:case 0x03B8:count=4;break;
    case 0x012C:case 0x04E8:case 0x04E9:case 0x0111:case 0x0116:
    case 0x01AB:case 0x037D:case 0x034B:case 0x033C:case 0x02BB:case 0x0370:count=1;break;
    case 0x0325:case 0x0350:case 0x035E:case 0x013A:case 0x0142:
    case 0x01A6:case 0x01B0:case 0x01CA:case 0x01D4:case 0x01D5:
    case 0x0123:case 0x0124:case 0x00D8:case 0x01EF:case 0x0369:
    case 0x042F:case 0x0430:count=2;break;
    default:return 0;
    }
    if(!vm.environment) {error="Vehicle command requires a live environment.";return -1;}
    std::array<Operand,7> values{};
    for(int index=0;index<count;++index)if(!readOperand(vm,values[size_t(index)])) {
        error="Vehicle command has a truncated typed operand.";return -1;
    }
    auto& world=*vm.environment;
    const int32_t handle=values[0].bits;
    const auto store=[&](size_t index,int32_t value) {
        if(writeOperand(vm,values[index],value))return true;
        error="Vehicle query requires writable outputs.";return false;
    };
    const auto storeFloat=[&](size_t index,float value) {
        int32_t bits;std::memcpy(&bits,&value,4);return store(index,bits);
    };
    const auto condition=[&](bool value){updateScmCondition(vm,value,inverted);return 1;};
    if(command==0x0315) {
        if(handle!=0) {error="Wheelie statistics require the live player character.";return -1;}
        for(size_t index=1;index<7;++index)if(!values[index].reference) {
            error="Wheelie statistics require six writable outputs.";return -1;
        }
        for(size_t index=0;index<3;++index)
            if(!store(index*2+1,world.playerVehicleStunt.milliseconds[index])||
                !storeFloat(index*2+2,world.playerVehicleStunt.distances[index]))return -1;
        world.playerVehicleStunt.milliseconds={};world.playerVehicleStunt.distances={};
        return 1;
    }
    const auto found=world.missionVehicles.find(handle);
    if(command==0x034B)return condition(found!=world.missionVehicles.end()&&!found->second.deleted);
    if(found==world.missionVehicles.end()||found->second.deleted) {
        error="Vehicle command references an unknown vehicle.";return -1;
    }
    auto& vehicle=found->second;
    if(command==0x035E) {
        const bool enabled=values[1].bits!=0;
        if(!world.entityCollisionStreaming||!world.entityCollisionStreaming(2,handle,enabled)) {
            error="Vehicle collision residency could not be updated.";return -1;
        }
        vehicle.loadCollision=enabled;return 1;
    }
    if(command==0x0142) {vehicle.taxiLight=values[1].bits!=0;return 1;}
    if(command==0x01A6) {vehicle.canRespray=values[1].bits!=0;return 1;}
    if(command==0x0370) {
        if(!world.vehicleNativeType) {error="Bomb removal requires native vehicle metadata.";return -1;}
        if(world.vehicleNativeType(handle)==0&&vehicle.bombType) {
            storylandClearVehicleBomb(vehicle);
        }
        return 1;
    }
    if(command==0x012C||command==0x04E8||command==0x04E9) {
        const auto contacts=world.vehicleContacts?world.vehicleContacts(handle):StorylandMissionEnvironment::VehicleContacts{};
        if(!contacts.valid) {error="Vehicle contact query requires a live native physical vehicle.";return -1;}
        if(command==0x012C)return condition(!contacts.bodyContact&&
            std::none_of(contacts.wheels.begin(),contacts.wheels.end(),[](bool contact){return contact;}));
        const bool playerDriver=world.inVehicle&&world.occupiedMissionVehicle==handle&&
            world.playerPassengerSeat<0&&vehicle.driver<=0;
        return condition(contacts.twoWheels&&vehicle.scriptStatus!=5&&playerDriver&&
            world.playerVehicleStunt.vehicle==handle&&
            world.playerVehicleStunt.activeMilliseconds[command==0x04E8?1:2]>50.0f);
    }
    if(command==0x0111)return condition(std::fabs(vehicle.speed)<.01f);
    if(command==0x00D8)return condition(vehicle.health>float(values[1].bits));
    if(command==0x0430)return store(1,storylandVcsSignedConversion(vehicle.maximumHealth))?1:-1;
    if(command==0x01EF)return condition(storylandObjectWeaponMatches(vehicle.lastWeaponDamage,values[1].bits));
    if(command==0x01AB||command==0x037D) {
        if(!world.vehicleNativeType) {error="Vehicle type query requires native model metadata.";return -1;}
        const int type=world.vehicleNativeType(handle);
        return condition(command==0x01AB?(type==1||type==2):(type==6||type==8));
    }
    if(command==0x01CA) {
        std::array<float,3> velocity=vehicle.velocity;
        const bool liveVelocity=bool(world.vehicleWorldVelocity);
        if(liveVelocity&&!world.vehicleWorldVelocity(handle,velocity)) {
            error="Vehicle speed requires its live physical velocity.";return -1;
        }
        const float magnitude=std::hypot(velocity[0],velocity[1],velocity[2]);
        if(magnitude==0)return storeFloat(1,liveVelocity?0.0f:vehicle.speed)?1:-1;
        std::array<float,9> basis{};
        if(!world.vehicleBasis||!world.vehicleBasis(handle,basis)) {
            error="Vehicle speed requires its live physical basis.";return -1;
        }
        // Native3D357C..3D35E8 signs the full speed by its forward projection.
        const float forward=velocity[0]*basis[3]+velocity[1]*basis[4]+velocity[2]*basis[5];
        return storeFloat(1,forward<0?-magnitude:magnitude)?1:-1;
    }
    if(command==0x01D4||command==0x01D5) {
        std::array<float,9> basis{};
        if(!world.vehicleBasis||!world.vehicleBasis(handle,basis)) {
            error="Vehicle heading query requires its live native basis.";return -1;
        }
        const float length=std::hypot(basis[3],basis[4]);
        const float component=command==0x01D4?basis[3]:basis[4];
        return storeFloat(1,length>0?component/length:0.0f)?1:-1;
    }
    if(command==0x0123||command==0x0124) {
        int32_t passengers=0;
        if(command==0x0124) {
            if(!world.vehiclePassengerCapacity) {error="Passenger capacity requires native vehicle metadata.";return -1;}
            passengers=world.vehiclePassengerCapacity(handle);
        } else {
            if(world.inVehicle&&world.occupiedMissionVehicle==handle&&world.playerPassengerSeat>=0)++passengers;
            for(const auto& entry:world.missionCharacters)
                if(entry.second.alive&&!entry.second.attached&&entry.second.vehicle==handle&&entry.second.passengerSeat>=0)++passengers;
        }
        return store(1,passengers)?1:-1;
    }
    if(command==0x03B8) {
        std::array<float,9> basis{};
        if(!world.vehicleBasis||!world.vehicleBasis(handle,basis)) {
            error="Vehicle rotation requires its live native basis.";return -1;
        }
        for(size_t index=1;index<4;++index)if(!values[index].reference) {
            error="Vehicle rotation requires three writable outputs.";return -1;
        }
        const auto degrees=[](float radians) {
            float value=radians*57.2957795131f;
            if(value<0)value+=360;else if(value>360)value-=360;
            return value;
        };
        // 0x3D758C..0x3D7778: roll, pitch, heading from the physical matrix.
        return storeFloat(1,degrees(std::atan2(-basis[2],basis[8])))&&
            storeFloat(2,degrees(std::asin(std::clamp(basis[5],-1.0f,1.0f))))&&
            storeFloat(3,degrees(std::atan2(-basis[3],basis[4])))?1:-1;
    }
    if(command==0x03B4||command==0x0422) {
        std::array<float,3> vector{};
        for(size_t index=0;index<3;++index) {
            vector[index]=operandFloat(values[index+1]);
            if(!std::isfinite(vector[index])) {error="Vehicle vector must be finite.";return -1;}
        }
        if(command==0x0422) {
            if(!storylandSetVcsVehicleCoordinates(world,handle,vehicle,
                vector[0],vector[1],vector[2],true,error))return -1;
        } else {
            // Native 3D74C0 stores input * 0.02 for its 50-Hz physical step.
            vehicle.velocity=vector;vehicle.velocityChangedByScript=true;
            const float angle=vehicle.heading*.0174532925199433f;
            vehicle.speed=-std::sin(angle)*vector[0]+std::cos(angle)*vector[1];
        }
        return 1;
    }
    switch(command) {
    case 0x042F:
        vehicle.health=vehicle.maximumHealth=storylandVcsIntegerFloat(values[1].bits);
        vehicle.healthChangedByScript=true;
        break;
    case 0x0325:vehicle.positionFrozen=values[1].bits!=0;break;
    case 0x0350:vehicle.positionFrozen=values[1].bits!=0;vehicle.loadCollision=values[1].bits==0;break;
    case 0x013A:vehicle.doorLock=values[1].bits;break;
    case 0x01B0:vehicle.onlyDamagedByPlayer=values[1].bits!=0;break;
    case 0x0116:
        // Native 0x3D1FE4 removes cleanup membership without releasing ownership.
        world.missionCleanupVehicles.erase(handle);
        break;
    case 0x033C:vehicle.lastDamageSource={};break;
    case 0x02BB:vehicle.lastWeaponDamage=-1;break;
    case 0x0369:vehicle.fadeOut=values[1].bits!=0;break;
    default:break;
    }
    return 1;
}

int executeVcsEntitySupplementCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (!vm.vcs) {
        return 0;
    }
    size_t count = 0;
    switch (command) {
    case 0x0254:
        count = 3;
        break;
    case 0x02D0:
    case 0x0340:
    case 0x0341:
        count = 2;
        break;
    case 0x0237:
    case 0x0255:
    case 0x0256:
    case 0x0349:
    case 0x036C:
    case 0x037C:
    case 0x037E:
    case 0x037F:
        count = 1;
        break;
    default:
        return 0;
    }
    if (!vm.environment) {
        error = "Entity command requires a live environment.";
        return -1;
    }
    std::array<Operand,3> values{};
    for (size_t index = 0; index < count; ++index) {
        if (!readOperand(vm, values[index])) {
            error = "Entity command has a truncated typed operand.";
            return -1;
        }
    }
    auto& world = *vm.environment;
    const int32_t handle = values[0].bits;
    const auto condition = [&](bool result) {
        updateScmCondition(vm, result, inverted);
        return 1;
    };
    if (command == 0x0255) {
        // Native removal clears every matching entry, including duplicate checks.
        for (auto& check : world.stuckCarChecks) {
            if (check.vehicle == handle) {
                check = {};
            }
        }
        return 1;
    }
    if (command == 0x0256) {
        for (const auto& check : world.stuckCarChecks) {
            if (check.vehicle == handle) {
                return condition(check.stuck);
            }
        }
        return condition(false);
    }
    if (command == 0x0254 || command == 0x037C || command == 0x037E || command == 0x037F) {
        const auto found = world.missionVehicles.find(handle);
        if (found == world.missionVehicles.end() || found->second.deleted) {
            if (command == 0x0254) {
                return 1; // Native registration ignores a null vehicle.
            }
            error = "Vehicle query references an unknown vehicle.";
            return -1;
        }
        const auto& vehicle = found->second;
        if (command == 0x0254) {
            const float distance = operandFloat(values[1]);
            if (!std::isfinite(distance)) {
                error = "The stuck-car distance requires a finite native float.";
                return -1;
            }
            const auto free = std::find_if(world.stuckCarChecks.begin(), world.stuckCarChecks.end(),
                [](const auto& check) { return check.vehicle < 0; });
            if (free == world.stuckCarChecks.end()) {
                error = "The native 16-entry stuck-car pool is full; registration was not changed.";
                return -1;
            }
            free->vehicle = handle;
            free->position = {vehicle.x, vehicle.y, vehicle.z};
            free->sampledAt = world.gameMilliseconds;
            free->distance = distance;
            free->interval = uint32_t(values[2].bits);
            free->stuck = false;
            return 1;
        }
        if (command == 0x037C) {
            // Retail 0x3D6AB0 tests the signed model halfword as an unsigned
            // integer range. It does not inspect the vehicle's handling class.
            const uint32_t halfword = uint16_t(vehicle.model);
            const int32_t signedModel = halfword < 0x8000 ? int32_t(halfword) : int32_t(halfword) - 0x10000;
            const uint32_t model = uint32_t(signedModel);
            return condition(model >= 0xAAu && model <= 0xFFFFFC4Cu);
        }
        if (!world.vehicleNativeType) {
            error = "Vehicle type query requires native model metadata.";
            return -1;
        }
        const int32_t type = world.vehicleNativeType(handle);
        if (type < 0) {
            error = "Vehicle type query cannot resolve native model metadata.";
            return -1;
        }
        return condition(type == (command == 0x037E ? 5 : 4));
    }
    const bool player = handle == 0;
    const auto found = world.missionCharacters.find(handle);
    if (!player && found == world.missionCharacters.end()) {
        if (command == 0x02D0) {
            return 1; // The native speech helper accepts null.
        }
        error = "Character command references an unknown character.";
        return -1;
    }
    auto* character = player ? nullptr : &found->second;
    if (command == 0x0237) {
        const int32_t type = player ? 0 : character->type;
        return condition(type != 5 && type != 20);
    }
    if (command == 0x036C) {
        const auto& crouch = player ? world.playerCrouch : character->crouch;
        const bool association = crouch.poseAnimation == 153 && crouch.poseFade.present &&
            (crouch.poseFade.rate >= 0 || crouch.poseFade.at(world.gameMilliseconds) > 0);
        return condition(association);
    }
    if (command == 0x0349) {
        const bool seated = player ? world.inVehicle && !world.playerVehicleTransition :
            character->vehicle >= 0 && !character->attached &&
            !world.vcsCharacterExitingVehicle(handle);
        const int32_t carHandle = player ? world.occupiedMissionVehicle : character->vehicle;
        const auto car = world.missionVehicles.find(carHandle);
        if (!seated || car == world.missionVehicles.end() || car->second.deleted) {
            return condition(false);
        }
        // Native law-enforcement table 0x4C2750, with 0349's explicit model219 exclusion.
        constexpr std::array<int32_t,8> models{191, 193, 204, 207, 218, 234, 246, 268};
        return condition(std::find(models.begin(), models.end(), car->second.model) != models.end());
    }
    if (command == 0x02D0) {
        if (player) {
            world.playerSpeechEnabled = values[1].bits == 0;
        } else {
            character->speechEnabled = values[1].bits == 0;
        }
        return 1;
    }
    if (!player) {
        error = "PlayerInfo health and armour limits require the live player.";
        return -1;
    }
    const uint8_t increment = uint8_t(values[1].bits);
    if (command == 0x0340) {
        world.playerHealthLimit = uint8_t(unsigned(world.playerHealthLimit) + increment);
        world.health = float(world.playerHealthLimit);
        world.flashingHudObject = 4;
    } else {
        world.playerArmourLimit = uint8_t(unsigned(world.playerArmourLimit) + increment);
        world.armour = float(world.playerArmourLimit);
        world.flashingHudObject = 3;
    }
    return 1;
}

void storylandClearVcsCollectivePlayerObjective(StorylandMissionEnvironment& world) {
    world.playerVehicleEntryTarget = -1;
    world.playerVehicleEntryAsPassenger = false;
    world.playerWalkTask.clear();
    world.playerNavigation.clearRoute();
    world.playerExitVehicleRequested = false;
}

int executeVcsCollectiveBatchCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (!vm.vcs) {
        return 0;
    }
    int count = -1;
    switch (command) {
    case 0x04FD: count = 0; break;
    case 0x016E: case 0x017E: case 0x017F: case 0x0188:
    case 0x018F: case 0x043B: count = 1; break;
    case 0x0179: case 0x017A: case 0x0180: case 0x0182: count = 2; break;
    case 0x017D: case 0x0184: case 0x043C: case 0x043D: count = 3; break;
    case 0x016F: case 0x018C: count = 4; break;
    case 0x0170: count = 5; break;
    case 0x0187: count = 6; break;
    default: return 0;
    }
    if (!vm.environment) {
        error = "Collective command requires a live environment.";
        return -1;
    }
    std::array<Operand,6> values{};
    for (int index = 0; index < count; ++index) {
        if (!readOperand(vm, values[size_t(index)])) {
            error = "Collective command has a truncated typed operand.";
            return -1;
        }
    }
    auto& world = *vm.environment;
    if (command == 0x04FD) {
        // Native 11C958 consumes this request at its next update, once only.
        world.forceCollectiveUpdate = true;
        return 1;
    }
    const int32_t groupIndex = values[0].bits;
    if (groupIndex < 0 || size_t(groupIndex) >= world.characterCollectives.size()) {
        error = "Collective index must address one of the four native script groups.";
        return -1;
    }
    auto& group = world.characterCollectives[size_t(groupIndex)];
    auto& policy = world.collectivePolicies[size_t(groupIndex)];
    const auto condition = [&](bool result) {
        updateScmCondition(vm, result, inverted);
        return 1;
    };
    if (command == 0x043B || command == 0x018F) {
        for (int32_t handle : group) {
            if (handle == 0) {
                if (command == 0x043B) {
                    if (world.playerWalkTask.active || world.playerVehicleEntryTarget >= 0 ||
                        world.playerExitVehicleRequested) return condition(false);
                } else if (!world.inVehicle || world.activeVcsCharacterAttachment(0) ||
                    world.playerExitVehicleRequested) return condition(false);
                continue;
            }
            const auto found = world.missionCharacters.find(handle);
            if (found == world.missionCharacters.end()) {
                continue;
            }
            const auto& character = found->second;
            if (command == 0x043B) {
                if (character.objective != 0) {
                    return condition(false);
                }
            } else {
                // 219D50 first requires the actual vehicle pointer. 219CC0
                // includes seated/dead occupants, but excludes leaving tasks.
                const auto vehicle = world.missionVehicles.find(character.vehicle);
                if (character.attached || vehicle == world.missionVehicles.end() ||
                    vehicle->second.deleted || world.vcsCharacterExitingVehicle(handle)) {
                    return condition(false);
                }
                const bool seated = character.passengerSeat < 0 ? vehicle->second.driver == handle :
                    storylandVcsPassenger(world, character.vehicle, character.passengerSeat) == handle;
                if (!seated) {
                    return condition(false);
                }
            }
        }
        return condition(true); // Retail empty-group predicates start true.
    }
    if (command == 0x043C || command == 0x043D) {
        const int32_t targetGroup = int8_t(uint8_t(values[1].bits));
        if (targetGroup < 0 || size_t(targetGroup) >= world.characterCollectives.size()) {
            error = "Collective policy target must address a public native group after its signed-byte cast.";
            return -1;
        }
        policy.mode = command == 0x043C ? 1 : 2;
        policy.targetGroup = targetGroup;
        // The native handler uses CVT.S.W, not a float reinterpretation.
        policy.range = storylandVcsIntegerFloat(values[2].bits);
        policy.manualObjective = false;
        return 1;
    }
    if (command == 0x0188) {
        for (int32_t handle : group) {
            const auto found = world.missionCharacters.find(handle);
            if (found != world.missionCharacters.end() && found->second.followingScriptPath) {
                auto& character = found->second;
                character.followingScriptPath = false;
                character.walkingToTarget = false;
                character.seekRoute.clear();
                character.seekRouteCursor = 0;
                character.seekNode = UINT32_MAX;
            }
        }
        return 1;
    }
    if (command == 0x018C) {
        const float x = operandFloat(values[1]);
        const float y = operandFloat(values[2]);
        const float radius = operandFloat(values[3]);
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(radius) || radius < 0 ||
            !std::isfinite(x - radius) || !std::isfinite(x + radius) ||
            !std::isfinite(y - radius) || !std::isfinite(y + radius)) {
            error = "Collective recruitment requires a finite point and nonnegative finite radius.";
            return -1;
        }
        if (!world.groundHeightAt || !world.ambientCharactersInArea || !world.acquireAmbientCharacters) {
            error = "Collective recruitment requires actual ambient characters and world collision.";
            return -1;
        }
        float ground = 20;
        if (world.groundHeightAt({x, y, 1000}, ground) && !std::isfinite(ground)) {
            error = "Collective recruitment ground height must be finite.";
            return -1;
        }
        const std::array<float,6> bounds{x - radius, y - radius, -1.0e30f,
            x + radius, y + radius, 1.0e30f};
        const auto ambient = world.ambientCharactersInArea(bounds);
        size_t used = 0;
        for (const auto& existing : world.characterCollectives) {
            used += existing.size();
        }
        const auto within = [&](float px, float py) {
            const float dx = px - x, dy = py - y;
            // 194C10 takes the 2D branch and compares squared float distance.
            return dx * dx + dy * dy < radius * radius;
        };
        // Already promoted/released peds retain ambient ownership. The host's
        // stable handle order replaces native sector-list ordering explicitly.
        std::vector<int32_t> existing;
        bool complete = true;
        for (auto entry = world.missionCharacters.rbegin(); entry != world.missionCharacters.rend(); ++entry) {
            const auto& character = entry->second;
            if (character.creationSource == 2 || character.type == 6 ||
                !within(character.x, character.y)) {
                continue;
            }
            if (std::find(group.begin(), group.end(), entry->first) != group.end()) {
                continue;
            }
            if (used == 32) {
                complete = false;
                break;
            }
            existing.push_back(entry->first);
            ++used;
        }
        std::vector<int32_t> slots;
        if (complete) {
            std::set<int32_t> seen;
            for (const auto& character : ambient) {
                if (character.slot < 0 || character.type == 6 || !seen.insert(character.slot).second ||
                    !within(character.position[0], character.position[1])) {
                    continue;
                }
                if (used == 32) {
                    complete = false;
                    break;
                }
                slots.push_back(character.slot);
                ++used;
            }
        }
        std::vector<int32_t> acquired;
        if (!slots.empty() && (!world.acquireAmbientCharacters(slots, false, acquired) ||
            acquired.size() != slots.size())) {
            error = "Collective recruitment could not acquire the selected real ambient characters.";
            return -1;
        }
        // Capacity failure retains the successful prefix, as native 25A930
        // returns immediately at its first unsuccessful 11B3F0 allocation.
        for (int32_t handle : existing) {
            group.insert(group.begin(), handle);
        }
        for (int32_t handle : acquired) {
            group.insert(group.begin(), handle);
        }
        return condition(complete);
    }

    const bool characterTarget = command == 0x017D || command == 0x0182;
    if (characterTarget) {
        const int32_t target = values[1].bits;
        if ((target != 0 && !world.missionCharacters.count(target)) ||
            std::find(group.begin(), group.end(), target) != group.end()) {
            error = "Collective task requires an existing character outside its own membership.";
            return -1;
        }
        if (command == 0x017D && values[2].bits != 0) {
            error = "Collective formation currently supports the native default follow formation only.";
            return -1;
        }
    }
    if (command == 0x0179) {
        const auto object = std::find_if(world.objects.begin(), world.objects.end(),
            [&](const auto& item) { return item.handle == values[1].bits; });
        if (object == world.objects.end()) {
            error = "Collective destruction references an unknown object.";
            return -1;
        }
    }
    if (command == 0x017A || command == 0x0180) {
        const auto vehicle = world.missionVehicles.find(values[1].bits);
        if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
            error = "Collective vehicle task references an unknown vehicle.";
            return -1;
        }
    }
    int16_t route = -1;
    int16_t routeMode = 1;
    if (command == 0x0184) {
        route = int16_t(uint16_t(values[1].bits));
        routeMode = int16_t(uint16_t(values[2].bits));
        const auto first = std::find_if(world.characterRoutePoints.begin(), world.characterRoutePoints.end(),
            [route](const auto& point) { return point.route == route; });
        if (route == -1 || routeMode < 1 || routeMode > 3 || first == world.characterRoutePoints.end() ||
            first + 1 == world.characterRoutePoints.end() || (first + 1)->route != route) {
            error = "Collective route requires two contiguous native route points and mode1..3.";
            return -1;
        }
    }
    const bool destination = command == 0x016F || command == 0x0170 || command == 0x0187;
    float x = 0, y = 0, z = 20, radius = 1;
    if (destination) {
        const int last = command == 0x0187 ? 4 : count - 1;
        for (int index = 1; index <= last; ++index) {
            if (!std::isfinite(operandFloat(values[size_t(index)]))) {
                error = "Collective coordinates and radius must be finite.";
                return -1;
            }
        }
        x = operandFloat(values[1]);
        y = operandFloat(values[2]);
        if (command == 0x0170) {
            const float minimumX = std::min(x, operandFloat(values[3]));
            const float minimumY = std::min(y, operandFloat(values[4]));
            x = (minimumX + std::max(x, operandFloat(values[3]))) * .5f;
            y = (minimumY + std::max(y, operandFloat(values[4]))) * .5f;
            radius = std::max(x - minimumX, y - minimumY);
        } else {
            z = operandFloat(values[3]);
            if (command == 0x0187) {
                radius = operandFloat(values[4]);
            }
        }
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(radius) || radius < 0) {
            error = "Collective destination is outside the supported native coordinate range.";
            return -1;
        }
        if (command == 0x0170 || z == -100 || z <= -250) {
            if (!world.groundHeightAt) {
                error = "Collective ground-relative task requires world collision.";
                return -1;
            }
            float height = 20;
            z = world.groundHeightAt({x, y, 1000}, height) ? height : 20;
            if (!std::isfinite(z)) {
                error = "Collective ground collision returned a non-finite height.";
                return -1;
            }
        }
    }
    if (command == 0x017E) {
        for (int32_t handle : group) {
            const auto found = world.missionCharacters.find(handle);
            if (found != world.missionCharacters.end() && found->second.alive && found->second.health > 0 &&
                (found->second.vehicle >= 0 || found->second.attached)) {
                error = "Collective vehicle theft requires on-foot members.";
                return -1;
            }
        }
    }
    if (command != 0x0187) {
        policy.manualObjective = true;
    }
    for (int32_t handle : group) {
        const auto found = world.missionCharacters.find(handle);
        if (found == world.missionCharacters.end()) {
            continue;
        }
        auto& character = found->second;
        if (!character.alive || character.health <= 0) {
            if (command != 0x0187) {
                character.objectiveCompleted = command != 0x0170;
            }
            continue;
        }
        if (command == 0x0187 && (character.vehicle >= 0 || character.attached)) {
            continue;
        }
        character.followingScriptPath = command == 0x0187;
        character.objectiveCompleted = false;
        character.walkingToTarget = false;
        character.seekRoute.clear();
        character.seekRouteCursor = 0;
        character.seekNode = UINT32_MAX;
        world.characterWanderTasks.erase(handle);
        character.objectiveTarget = values[1].bits;
        if (command == 0x016E) {
            character.objective = 0x00E0;
            character.objectiveTarget = 0;
        } else if (destination) {
            character.objective = command == 0x016F ? 0x00E1 : command == 0x0170 ? 0x00E2 : 0;
            character.objectiveTarget = -1;
            character.targetX = x;
            character.targetY = y;
            character.objectiveHeight = z;
            character.objectiveRadius = radius;
            character.walkSpeed = command == 0x0187 && values[5].bits == 1 ? 3.8f : 1.25f;
            character.walkingToTarget = std::hypot(character.x - x, character.y - y) > radius;
        } else if (command == 0x0179 || command == 0x017A || command == 0x0180) {
            character.objective = command == 0x0179 ? 0x00EB : command == 0x017A ? 0x00EC : 0x00F2;
        } else if (command == 0x017D) {
            character.objective = 0x00EF;
            character.leader = values[1].bits;
            character.formation = 0;
        } else if (command == 0x017E) {
            character.objective = 0x00F0;
            character.objectiveTarget = -1;
        } else if (command == 0x017F) {
            character.objective = 0x00F1;
            character.objectiveTarget = character.vehicle;
        } else if (command == 0x0182) {
            character.objective = 0x00F4;
        } else if (command == 0x0184) {
            character.objective = 0x011B;
            character.objectiveTarget = -1;
            character.routeId = route;
            character.routeMode = routeMode;
            character.routeCursor = 1;
            character.routeDirection = 1;
            character.walkSpeed = 1.25f;
        }
    }
    return 1;
}

int executeVcsCollectiveCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    const int extended = executeVcsCollectiveBatchCommand(vm, command, inverted, error);
    if (extended) {
        return extended;
    }
    if (!vm.vcs) {
        return 0;
    }
    int count = 0;
    switch (command) {
    case 0x016C:
    case 0x016D:
    case 0x0171:
    case 0x018B:
    case 0x018E:
        count = 1;
        break;
    case 0x016B:
    case 0x0172:
    case 0x0173:
    case 0x0174:
    case 0x0175:
    case 0x0176:
    case 0x0177:
    case 0x0178:
    case 0x0181:
    case 0x0189:
    case 0x018A:
    case 0x019D:
    case 0x01A3:
        count = 2;
        break;
    case 0x017C:
    case 0x0183:
    case 0x0185:
        count = 3;
        break;
    case 0x017B:
    case 0x0196:
    case 0x0197:
    case 0x0198:
    case 0x0199:
    case 0x019A:
    case 0x019B:
        count = 5;
        break;
    case 0x019C:
    case 0x019E:
    case 0x019F:
        count = 6;
        break;
    default:
        return 0;
    }
    if (!vm.environment) {
        error = "Collective command requires a live environment.";
        return -1;
    }
    std::array<Operand, 6> values{};
    for (int index = 0; index < count; ++index) {
        if (!readOperand(vm, values[size_t(index)])) {
            error = "Collective command has a truncated typed operand.";
            return -1;
        }
    }
    auto& world = *vm.environment;
    if (command == 0x018B) {
        // Native 11B5A8 removes this character from all four public groups.
        for (auto& group : world.characterCollectives) {
            group.erase(std::remove(group.begin(), group.end(), values[0].bits), group.end());
        }
        return 1;
    }
    const int32_t groupIndex = values[0].bits;
    if (groupIndex < 0 || size_t(groupIndex) >= world.characterCollectives.size()) {
        error = "Collective index must address one of the four native script groups.";
        return -1;
    }
    auto& group = world.characterCollectives[size_t(groupIndex)];
    if ((command >= 0x0196 && command <= 0x019C) || command == 0x019E || command == 0x019F) {
        const bool area = command >= 0x019C;
        const bool characterTarget = command <= 0x0198;
        const int variant = area ? (command == 0x019C ? 0 : command == 0x019E ? 1 : 2) :
            characterTarget ? command - 0x0196 : command - 0x0199;
        const float x1 = area ? operandFloat(values[1]) : 0;
        const float y1 = area ? operandFloat(values[2]) : 0;
        const float x2 = area ? operandFloat(values[3]) : 0;
        const float y2 = area ? operandFloat(values[4]) : 0;
        const float extentX = area ? (std::max(x1, x2) - std::min(x1, x2)) * 0.5f : operandFloat(values[2]);
        const float extentY = area ? (std::max(y1, y2) - std::min(y1, y2)) * 0.5f : operandFloat(values[3]);
        const auto position = [&](int32_t handle, std::array<float, 3>& point,
            bool& seated) -> bool {
            int32_t vehicleHandle = -1;
            if (handle == 0) {
                point = {world.x, world.y, world.z};
                seated = world.inVehicle && !world.activeVcsCharacterAttachment(0) &&
                    !world.playerExitVehicleRequested;
                if (seated) vehicleHandle = world.occupiedMissionVehicle;
            } else {
                const auto found = world.missionCharacters.find(handle);
                if (found == world.missionCharacters.end() || found->second.removeFromWorld)
                    return false;
                const auto& character = found->second;
                point = {character.x, character.y, character.z};
                seated = character.vehicle >= 0 && !character.attached &&
                    !world.vcsCharacterExitingVehicle(handle);
                if (seated) vehicleHandle = character.vehicle;
            }
            if (seated) {
                const auto vehicle = world.missionVehicles.find(vehicleHandle);
                if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) return false;
                point = {vehicle->second.x, vehicle->second.y, vehicle->second.z};
            }
            return true;
        };
        std::array<float, 3> center{};
        if (area) {
            center = {(x1 + x2) * 0.5f, (y1 + y2) * 0.5f, 0};
        } else if (characterTarget) {
            bool seated = false;
            if (!position(values[1].bits, center, seated)) {
                error = "Collective proximity query requires a live target character.";
                return -1;
            }
        } else {
            const auto vehicle = world.missionVehicles.find(values[1].bits);
            if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
                error = "Collective proximity query requires a live target vehicle.";
                return -1;
            }
            center = {vehicle->second.x, vehicle->second.y, vehicle->second.z};
        }
        if (values[area ? 5 : 4].bits && world.locateRegions.size() < 128) {
            StorylandMissionEnvironment::LocateRegion region;
            region.center = center;
            region.extent = {extentX, extentY, 0};
            region.expiresAt = world.gameMilliseconds + 100;
            if (world.groundHeightAt) {
                float height = 0;
                if (world.groundHeightAt({center[0], center[1], 1000}, height))
                    region.center[2] = height;
            }
            world.locateRegions.push_back(region);
        }
        bool result = true;
        // The original vehicle-relative loop advances its collective index twice.
        const size_t stride = characterTarget ? 1 : 2;
        for (size_t index = 0; index < group.size(); index += stride) {
            std::array<float, 3> point{};
            bool seated = false;
            if (!position(group[index], point, seated)) continue;
            const bool inside = area ? point[0] >= std::min(x1, x2) && point[0] <= std::max(x1, x2) &&
                point[1] >= std::min(y1, y2) && point[1] <= std::max(y1, y2) :
                point[0] >= center[0] - extentX && point[0] <= center[0] + extentX &&
                point[1] >= center[1] - extentY && point[1] <= center[1] + extentY;
            if (!inside || (variant == 1 && seated) || (variant == 2 && !seated)) {
                result = false;
                break;
            }
        }
        updateScmCondition(vm, result, inverted);
        return 1;
    }
    if (command == 0x019D) {
        const int32_t handle = values[1].bits;
        const bool exists = handle == 0 || world.missionCharacters.count(handle) != 0;
        const bool member = exists && std::find(group.begin(), group.end(), handle) != group.end();
        updateScmCondition(vm, member, inverted);
        return 1;
    }
    if (command == 0x01A3) {
        if (!writeOperand(vm, values[1], int32_t(group.size()))) {
            error = "Collective size requires a writable output.";
            return -1;
        }
        return 1;
    }
    if (command == 0x018E) {
        // Releasing native membership nodes does not alter the actors' tasks.
        group.clear();
        return 1;
    }
    if (command == 0x018A) {
        group.erase(std::remove(group.begin(), group.end(), values[1].bits), group.end());
        return 1;
    }
    if (command == 0x0189) {
        const int32_t handle = values[1].bits;
        if (handle < 0 || (handle != 0 && !world.missionCharacters.count(handle))) {
            error = "Collective membership requires an existing VCS player or character handle.";
            return -1;
        }
        bool added = std::find(group.begin(), group.end(), handle) != group.end();
        if (!added) {
            size_t nodes = 0;
            for (const auto& existing : world.characterCollectives) {
                nodes += existing.size();
            }
            if (nodes < 32) {
                group.insert(group.begin(), handle);
                added = true;
            }
        }
        // 11B3F0 returns true for an existing member even when the pool is full.
        updateScmCondition(vm, added, inverted);
        return 1;
    }

    const bool characterTarget = command == 0x0172 || command == 0x0173 ||
        command == 0x0174 || command == 0x0175 || command == 0x0181;
    const bool vehicleTarget = command >= 0x0176 && command <= 0x0178;
    if (characterTarget) {
        const int32_t target = values[1].bits;
        if (target != 0 && world.missionCharacters.find(target) == world.missionCharacters.end()) {
            error = "Collective objective references an unknown character target.";
            return -1;
        }
        if (std::find(group.begin(), group.end(), target) != group.end()) {
            error = "Collective objectives targeting a member of the same group are not supported.";
            return -1;
        }
    }
    if (vehicleTarget) {
        const auto vehicle = world.missionVehicles.find(values[1].bits);
        if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
            error = "Collective objective references an unknown vehicle.";
            return -1;
        }
        if (command == 0x0177 && !world.vehiclePassengerCapacity) {
            error = "Collective passenger entry requires native seat metadata.";
            return -1;
        }
    }

    const bool destination = command == 0x017B || command == 0x017C ||
        command == 0x0183 || command == 0x0185;
    float targetX = 0;
    float targetY = 0;
    float targetZ = 20;
    float radius = .5f;
    if (destination) {
        for (int index = 1; index < count; ++index) {
            if (!std::isfinite(operandFloat(values[size_t(index)]))) {
                error = "Collective destination coordinates must be finite.";
                return -1;
            }
        }
        targetX = operandFloat(values[1]);
        targetY = operandFloat(values[2]);
        if (command == 0x017B) {
            const float otherX = operandFloat(values[3]);
            const float otherY = operandFloat(values[4]);
            const float minimumX = std::min(targetX, otherX);
            const float minimumY = std::min(targetY, otherY);
            targetX = (minimumX + std::max(targetX, otherX)) * .5f;
            targetY = (minimumY + std::max(targetY, otherY)) * .5f;
            // Retail 3DE3F0 chooses the larger half-extent, not a rectangle test.
            radius = std::max(targetX - minimumX, targetY - minimumY);
        }
        if (!std::isfinite(targetX) || !std::isfinite(targetY) || !std::isfinite(radius)) {
            error = "Collective destination exceeds the finite native coordinate range.";
            return -1;
        }
        if (!world.groundHeightAt) {
            error = "Collective destination requires the native ground collision callback.";
            return -1;
        }
        float ground = 20;
        if (world.groundHeightAt({targetX, targetY, 1000}, ground)) {
            if (!std::isfinite(ground)) {
                error = "Collective ground height must be finite.";
                return -1;
            }
            targetZ = ground;
        }
    }

    int32_t wanderDirection = values[1].bits;
    if (command == 0x016B && (wanderDirection < 0 || wanderDirection >= 8)) {
        // 3DDC8C/443BC8 select one direction before iterating the whole group.
        wanderDirection = int32_t((uint64_t(world.nextNativeRandom()) * 7) >> 31);
    }
    auto& policy = world.collectivePolicies[size_t(groupIndex)];
    policy.manualObjective = command != 0x016C;
    if (command == 0x016C) {
        policy.mode = 0;
    }
    for (const int32_t handle : group) {
        if (handle == 0) {
            // 11BE20 clears every ped's objective; assignment helpers reject
            // player type0 through20C310 without installing an AI objective.
            if (command == 0x016C) storylandClearVcsCollectivePlayerObjective(world);
            continue;
        }
        const auto found = world.missionCharacters.find(handle);
        if (found == world.missionCharacters.end()) {
            continue;
        }
        auto& character = found->second;
        if (command == 0x016C) {
            character.followingScriptPath = false;
            character.objective = 0;
            character.objectiveCompleted = false;
            character.objectiveTarget = -1;
            character.walkingToTarget = false;
            character.seekRoute.clear();
            character.seekRouteCursor = 0;
            world.characterWanderTasks.erase(handle);
            continue;
        }
        if (!character.alive || character.health <= 0) {
            if (command != 0x016B) {
                // The radius helper 11BAE0 clears bit40 even for a dead ped;
                // the other objective helpers mark that skipped ped complete.
                character.objectiveCompleted = command != 0x017B;
            }
            continue;
        }
        character.objectiveCompleted = false;
        character.followingScriptPath = false;
        if (command == 0x016B) {
            auto& task = world.characterWanderTasks[handle];
            task.direction = wanderDirection;
            task.node = UINT32_MAX;
            character.objective = 0;
            character.objectiveTarget = -1;
            character.walkingToTarget = false;
            character.animationId = -1;
            character.walkSpeed = 1.25f;
            continue;
        }
        world.characterWanderTasks.erase(handle);
        character.seekRoute.clear();
        character.seekRouteCursor = 0;
        if (command == 0x016D || command == 0x0171) {
            character.objective = command == 0x016D ? 1 : 2;
            character.objectiveTarget = -1;
            character.walkingToTarget = false;
            continue;
        }
        if (characterTarget) {
            character.objective = command == 0x0172 ? 0x00E4 :
                command == 0x0173 ? 0x00E5 : command == 0x0174 ? 0x00E6 :
                command == 0x0175 ? 0x00E7 : 0x00F3;
            character.objectiveTarget = values[1].bits;
            continue;
        }
        if (vehicleTarget) {
            if (command == 0x0176 && character.vehicle != values[1].bits) {
                continue;
            }
            character.objective = command == 0x0176 ? 0x00E8 :
                command == 0x0177 ? 0x00E9 : 0x00EA;
            character.objectiveTarget = values[1].bits;
            if (command == 0x0177) {
                character.passengerSeat = -1;
            }
            continue;
        }
        if (destination) {
            character.targetX = targetX;
            character.targetY = targetY;
            character.objectiveHeight = targetZ;
            character.objectiveRadius = radius;
            character.objective = command == 0x017B ? 0x00ED :
                command == 0x017C ? 0x00EE : command == 0x0183 ? 0x00F5 : 0x0158;
            character.walkSpeed = command == 0x017C ? 1.25f : command == 0x0183 ? 5.5f : 3.8f;
            character.walkingToTarget = std::hypot(character.x - targetX, character.y - targetY) > radius;
        }
    }
    return 1;
}

void storylandCancelVcsVehicleEntry(StorylandMissionEnvironment& world, int32_t handle,
    StorylandMissionEnvironment::Character& character);

int executeVcsEntityCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    const int collective = executeVcsCollectiveCommand(vm, command, inverted, error);
    if (collective) {
        return collective;
    }
    const uint16_t sourceCommand=command;
    const int supplement = executeVcsEntitySupplementCommand(vm, command, inverted, error);
    if (supplement) {
        return supplement;
    }
    const int restoredQuery=executeVcsRestoredEntityQuery(vm,command,inverted,error);
    if(restoredQuery)return restoredQuery;
    const int restoredCharacter=executeVcsRestoredCharacterCommand(vm,command,inverted,error);
    if(restoredCharacter)return restoredCharacter;
    const int recoveredVehicle = executeVcsVehicleRecoveryCommand(vm, command, inverted, error);
    if (recoveredVehicle) return recoveredVehicle;
    const int autopilotVehicle = executeVcsVehicleAutopilotCommand(vm, command, inverted, error);
    if (autopilotVehicle) return autopilotVehicle;
    const int restoredVehicle=executeVcsRestoredVehicleCommand(vm,command,inverted,error);
    if(restoredVehicle)return restoredVehicle;
    if(!vm.vcs) {
        switch(command) {
        case 0x0229:command=0x014C;break;
        case 0x022C:command=0x014E;break;
        case 0x022E:command=0x0150;break;
        case 0x012E:command=0x00A4;break;
        case 0x01CD:command=0x0118;break;
        case 0x01F1:command=0x0126;break;
        case 0x01F2:command=0x0127;break;
        case 0x02E7:command=0x01C9;break;
        case 0x02B0:command=0x01B1;break;
        case 0x0458:command=0x03F6;break;
        case 0x04FE:command=0x0313;break;
        case 0x04FF:command=0x0314;break;
        case 0x0179:command=0x00D0;break;
        case 0x017A:command=0x00D1;break;
        case 0x017B:command=0x00D2;break;
        case 0x02B1:command=0x01B2;break;
        default:return 0;
        }
    }
    if(command==0x0126||command==0x0127||command==0x01C9||command==0x01B1||command==0x03F6) {
        const size_t count=command==0x0127?1:command==0x01B1?6:command==0x03F6?4:2;
        std::array<Operand,6> values{};
        for(size_t i=0;i<count;++i)if(!readOperand(vm,values[i])) {error="Truncated entity attributes.";return -1;}
        if(!vm.environment) {error="Entity attributes require a live environment.";return -1;}
        auto& world=*vm.environment;
        if(command==0x0126) {
            auto found=world.missionVehicles.find(values[0].bits);
            if(found==world.missionVehicles.end()||found->second.deleted) {error="SET_CAR_HEAVY references an unknown vehicle.";return -1;}
            found->second.heavy=values[1].bits!=0;return 1;
        }
        if(command==0x03F6) {
            auto found=std::find_if(world.objects.begin(),world.objects.end(),[&](const auto& object){return object.handle==values[0].bits;});
            if(found==world.objects.end()) {error="SET_OBJECT_ROTATION references an unknown object.";return -1;}
            for(size_t i=1;i<4;++i)if(!std::isfinite(operandFloat(values[i]))) {error="Object rotation must be finite.";return -1;}
            found->rotationX=operandFloat(values[1]);found->rotationY=operandFloat(values[2]);found->heading=operandFloat(values[3]);return 1;
        }
        uint8_t proofs=0;
        if(command==0x01B1)for(size_t i=1;i<6;++i)if(values[i].bits)proofs|=uint8_t(1u<<(i-1));
        if(command==0x01B1&&values[0].bits==(vm.vcs?0:1)) {world.playerDamageProofs=proofs;return 1;}
        auto found=world.missionCharacters.find(values[0].bits);
        if(found==world.missionCharacters.end()) {error="Character attributes reference an unknown character.";return -1;}
        auto& character=found->second;
        if(command==0x0127) {
            character.fearFlags=0;character.provokedByPlayer=false;
            world.characterTargets.erase(values[0].bits);
        }
        else if(command==0x01B1)character.damageProofs=proofs;
        else character.accuracy=uint8_t(std::clamp(int64_t(vm.vcs?uint8_t(values[1].bits):values[1].bits)*5/4,int64_t(0),int64_t(100)));
        return 1;
    }
    if(command==0x01B2) {
        std::array<Operand,6> values{};
        for(auto& value:values)if(!readOperand(vm,value)) {
            error="SET_CAR_PROOFS requires a vehicle and five flags.";return -1;
        }
        if(!vm.environment) {error="SET_CAR_PROOFS requires a live world.";return -1;}
        const auto found=vm.environment->missionVehicles.find(values[0].bits);
        if(found==vm.environment->missionVehicles.end()||found->second.deleted) {
            error="SET_CAR_PROOFS references an unknown vehicle.";return -1;
        }
        uint8_t proofs=0;
        for(size_t index=0;index<5;++index)if(values[index+1].bits)proofs|=uint8_t(1u<<index);
        found->second.damageProofs=proofs;
        return 1;
    }
    if(command==0x0246||command==0x0295) {
        std::array<float,6> bounds{};
        for(auto& coordinate:bounds) {
            Operand value;
            if(!readOperand(vm,value)||!std::isfinite(coordinate=operandFloat(value))) {
                error="Area clearing requires six finite coordinates.";return -1;
            }
        }
        for(size_t axis=0;axis<3;++axis)if(bounds[axis]>bounds[axis+3])std::swap(bounds[axis],bounds[axis+3]);
        if(!vm.environment||!vm.environment->clearAmbientArea) {
            error="Area clearing requires the live population pools.";return -1;
        }
        vm.environment->clearAmbientArea(bounds,command==0x0246);
        return 1;
    }
    if(command==0x041A) {
        std::string name;Operand output;
        if(!vm.environment||!readVcsMissionText(vm,name)||name.empty()||
            !readOperand(vm,output)||!output.reference) {
            error="REQUEST_MODEL_NAME requires a model name and writable output.";return -1;
        }
        auto& world=*vm.environment;
        const int32_t model=world.modelIdByName?world.modelIdByName(name):-1;
        if(model<0) {error="REQUEST_MODEL_NAME cannot resolve the native model: "+name;return -1;}
        if(!writeOperand(vm,output,model)) {error="REQUEST_MODEL_NAME output is invalid.";return -1;}
        world.requestedModels.insert(model);
        // Native 0x12BD80 also streams model 366 when requesting model 362.
        if(model==362)world.requestedModels.insert(366);
        return 1;
    }
    if(command==0x0202) {
        if(!vm.environment) {error="Area occupancy requires the live world.";return -1;}
        std::array<Operand,11> values{};
        for(auto& value:values)if(!readOperand(vm,value)) {error="Area occupancy has a truncated operand.";return -1;}
        std::array<float,6> bounds{};
        for(size_t index=0;index<6;++index) {
            bounds[index]=operandFloat(values[index]);
            if(!std::isfinite(bounds[index])) {error="Area occupancy bounds are non-finite.";return -1;}
        }
        for(size_t axis=0;axis<3;++axis)if(bounds[axis]>bounds[axis+3])std::swap(bounds[axis],bounds[axis+3]);
        uint32_t mask=0;
        for(size_t index=0;index<5;++index)if(values[index+6].bits)mask|=1u<<index;
        auto& world=*vm.environment;
        const auto inside=[&](float x,float y,float z) {
            return x>=bounds[0]&&x<=bounds[3]&&y>=bounds[1]&&y<=bounds[4]&&z>=bounds[2]&&z<=bounds[5];
        };
        bool occupied=world.areaOccupied&&world.areaOccupied(bounds,mask);
        if(mask&2u)for(const auto& item:world.missionVehicles)
            if(item.second.alive&&inside(item.second.x,item.second.y,item.second.z))occupied=true;
        if(mask&4u) {
            if(world.health>0&&inside(world.x,world.y,world.z))occupied=true;
            for(const auto& item:world.missionCharacters)
                if(item.second.alive&&inside(item.second.x,item.second.y,item.second.z))occupied=true;
        }
        if(mask&8u)for(const auto& object:world.objects)if(inside(object.x,object.y,object.z))occupied=true;
        updateScmCondition(vm,occupied,inverted);return 1;
    }
    if(command==0x0544&&vm.vcs) {
        Operand flag;
        if(!readOperand(vm,flag)) {
            error="Sniper building-impact policy has a truncated operand.";return -1;
        }
        if(!vm.environment) {
            error="Sniper building-impact policy requires the live world.";return -1;
        }
        vm.environment->vcsStopSniperBulletsOnBuildings=uint8_t(flag.bits);
        return 1;
    }
    if(command==0x04FA&&vm.vcs) {
        if(!vm.environment) {error="Vehicle command requires the live vehicle pool.";return -1;}
        Operand vehicle,flag;
        if(!readOperand(vm,vehicle)||!readOperand(vm,flag)) {
            error="Vehicle path flag has a truncated operand.";return -1;
        }
        auto& world=*vm.environment;
        const auto found=world.missionVehicles.find(vehicle.bits);
        if(found==world.missionVehicles.end()||found->second.deleted) {
            error="Vehicle path flag references an unknown vehicle.";return -1;
        }
        if(flag.bits&1)world.vcsVehicleFlag28.insert(vehicle.bits);
        else world.vcsVehicleFlag28.erase(vehicle.bits);
        return 1;
    }
    int count=0;
    switch(command) {
    case 0x003E:count=6;break;
    case 0x0118:count=5;break;
    case 0x0043:case 0x0044:case 0x010F:case 0x04B4:count=4;break;
    case 0x00A4:case 0x0216:count=4;break;
    case 0x02B6:case 0x030F:count=8;break;
    case 0x02EE:case 0x027F:count=7;break;
    case 0x031B:case 0x0404:case 0x02C7:case 0x00EE:count=3;break;
    case 0x004A:case 0x01B6:count=4;break;
    case 0x0233:case 0x0150:case 0x0414:count=3;break;
    case 0x00CE:case 0x00CF:case 0x013E:case 0x015F:case 0x042B:case 0x0200:
    case 0x04B5:case 0x00D0:case 0x00D1:case 0x0496:case 0x0050:case 0x02E9:case 0x028E:case 0x052E:case 0x02B4:
    case 0x014C:case 0x014E:case 0x00D2:
    case 0x0236:case 0x007C:case 0x014B:case 0x014D:case 0x00D7:case 0x0153:case 0x0313:case 0x01EC:count=2;break;
    case 0x0445:case 0x003F:case 0x0113:case 0x0154:case 0x0049:case 0x0444:case 0x0045:case 0x009F:case 0x00DF:case 0x00E3:case 0x00A2:case 0x0042:
    case 0x0112:case 0x02B7:case 0x04DE:case 0x0314:case 0x004C:case 0x02EA:count=1;break;
    default:return 0;
    }
    const auto signature=vm.opcodeDefinitions.find(sourceCommand);
    if(signature!=vm.opcodeDefinitions.end()&&signature->second.first!=count) {
        error="VCS entity opcode signature disagrees with its retail definition.";return -1;
    }
    if(!vm.environment) {error="VCS entity command requires the live entity pools.";return -1;}
    auto& world=*vm.environment;
    std::array<Operand,8> values{};
    for(int index=0;index<count;++index)if(!readOperand(vm,values[size_t(index)])) {
        error="VCS entity command has a truncated typed operand.";return -1;
    }
    const auto real=[&](size_t index){return operandFloat(values[index]);};
    const auto store=[&](size_t index,int32_t value) {
        if(writeOperand(vm,values[index],value))return true;
        error="VCS entity command requires a writable output.";return false;
    };
    const auto storeReal=[&](size_t index,float value) {
        int32_t bits;std::memcpy(&bits,&value,4);return store(index,bits);
    };
    const int32_t handle=values[0].bits;
    if(command==0x014C||command==0x014E||command==0x0150) {
        const auto found=world.missionVehicles.find(handle);
        if(found==world.missionVehicles.end()||found->second.deleted) {
            error="Vehicle property command references an unknown vehicle.";return -1;
        }
        auto& vehicle=found->second;
        if(command==0x014E)return store(1,int32_t(std::clamp(double(vehicle.health),double(INT32_MIN),double(INT32_MAX))))?1:-1;
        if(command==0x014C) {
            // Native 0x3D27A8 converts the integer operand to vehicle health.
            vehicle.health=float(values[1].bits);
            vehicle.alive=vehicle.health>0;
            vehicle.healthChangedByScript=true;
        } else {
            if(values[1].bits<0||values[1].bits>255||values[2].bits<0||values[2].bits>255) {
                error="CHANGE_CAR_COLOUR requires native palette indices 0..255.";return -1;
            }
            vehicle.primaryColor=values[1].bits;vehicle.secondaryColor=values[2].bits;
        }
        return 1;
    }
    if(command==0x00D2) {
        const auto object=std::find_if(world.objects.begin(),world.objects.end(),
            [&](const auto& item){return item.handle==handle;});
        if(object==world.objects.end()) {error="GET_OBJECT_HEADING references an unknown object.";return -1;}
        return storeReal(1,object->heading)?1:-1;
    }
    if(command==0x0445) {
        updateScmCondition(vm,world.animationGroupLoaded&&world.animationGroupLoaded(handle),inverted);
        return 1;
    }
    if(command==0x0236) {
        const auto car=world.missionVehicles.find(handle);
        if(car==world.missionVehicles.end()) {error="SET_CAR_STATUS references an unknown vehicle.";return -1;}
        // VCS PS2 handler 0x3D3F20 writes the entity's five-bit status field.
        car->second.scriptStatus=values[1].bits&31;
        if(car->second.scriptStatus==4||car->second.scriptStatus==5) {
            car->second.drivingToTarget=false;car->second.speed=0;
        }
        return 1;
    }
    if(command==0x007C) {
        bool inside=false;
        if(handle==0)inside=world.inVehicle&&world.occupiedMissionVehicle==values[1].bits;
        else {
            const auto character=world.missionCharacters.find(handle);
            inside=character!=world.missionCharacters.end()&&character->second.alive&&character->second.vehicle==values[1].bits;
        }
        updateScmCondition(vm,inside,inverted);return 1;
    }
    if(command==0x0113||command==0x00D0||command==0x00D1||command==0x0496) {
        const auto vehicle=world.missionVehicles.find(handle);
        if(vehicle==world.missionVehicles.end()||vehicle->second.deleted) {
            // Native 3D1F18 delegates release to the null-safe 25B178 helper.
            if(command==0x0113) {world.missionCleanupVehicles.erase(handle);return 1;}
            error="Vehicle command references an unknown car.";return -1;
        }
        if(command==0x0113) {vehicle->second.persistent=false;world.missionCleanupVehicles.erase(handle);}
        else if(command==0x00D0) {
            float heading=std::fmod(vehicle->second.heading,360.0f);
            if(heading<0)heading+=360.0f;
            return storeReal(1,heading)?1:-1;
        }
        else if(command==0x0496) {
            std::array<int32_t,2> colors{};
            if (!world.vehicleColorSet || !world.vehicleColorSet(handle, true, values[1].bits, colors)) {
                error = "Script colour set is outside the native model's palette.";
                return -1;
            }
            vehicle->second.primaryColor = colors[0];
            vehicle->second.secondaryColor = colors[1];
            vehicle->second.scriptColorSet = int8_t(uint8_t(values[1].bits));
            vehicle->second.colorSet = -1;
        }
        else {
            if(!std::isfinite(real(1))) {error="Vehicle heading is non-finite.";return -1;}
            vehicle->second.heading=real(1);vehicle->second.headingChangedByScript=true;
        }
        return 1;
    }
    if(command==0x0049) {
        if(vm.vcs) {
            const auto vehicle=world.missionVehicles.find(handle);
            if(vehicle==world.missionVehicles.end()||vehicle->second.deleted) {
                // 3D08C4 returns on a null vehicle after mission-cleanup lookup.
                if(vm.isMissionThread)world.missionCleanupVehicles.erase(handle);
                return 1;
            }
        }
        world.vcsVehicleFlag28.erase(handle);
        world.vcsGarageStorableMissionVehicles.erase(handle);
        world.photographedVehicles.erase(handle);
        std::vector<int32_t> occupants;
        for(const auto& entry:world.missionCharacters) {
            if(entry.first>0&&entry.second.vehicle==handle&&!entry.second.attached)
                occupants.push_back(entry.first);
        }
        for(int32_t occupant:occupants)storylandDeleteMissionCharacter(world,occupant);
        world.releaseCharacterVehicleReferences(handle);
        storylandClearDamageSource(world,StorylandMissionEnvironment::DamageSourceKind::Vehicle,handle);
        world.missionCleanupVehicles.erase(handle);
        const auto vehicle=world.missionVehicles.find(handle);
        if(vehicle!=world.missionVehicles.end()) {
            vehicle->second.alive=false;vehicle->second.deleted=true;vehicle->second.driver=-1;
        }
        world.upsideDownCarChecks.erase(handle);
        for(auto& entry:world.missionCharacters) {
            auto& character=entry.second;
            if(character.vehicle==handle) {
                character.vehicle=-1;character.attached=false;character.passengerSeat=-1;character.attachment={};
            }
            const int objective=character.objective;
            if(character.objectiveTarget==handle&&(objective==0x00E8||objective==0x00E9||
                objective==0x00EA||objective==0x00F1)) {
                character.objective=0;character.objectiveTarget=-1;character.walkingToTarget=false;
                character.seekRoute.clear();character.seekRouteCursor=0;
            }
        }
        for(auto& object:world.objects)if(object.attachedVehicle==handle) {
            object.attachedVehicle=-1;object.attachmentOffset={};
        }
        for(auto& fire:world.scriptFires)if(fire.second.attachedKind==2&&fire.second.attachedHandle==handle) {
            fire.second.active=false;fire.second.attachedKind=0;fire.second.attachedHandle=-1;
        }
        if(world.playerAttachedVehicle==handle) {world.playerAttachedVehicle=-1;world.playerAttachment={};}
        if(world.occupiedMissionVehicle==handle) {
            world.inVehicle=false;world.occupiedMissionVehicle=-1;world.occupiedVehicleModel=-1;
            world.playerPassengerSeat=-1;world.playerWarped=true;
            world.manualAim.seatedActive=false;world.manualAim.initialized=false;
            world.playerVehicleTransition=false;world.playerExitVehicleRequested=false;
            world.playerWalkTask.clear();
            world.playerVehicleEntryTarget=-1;world.playerVehicleEntryAsPassenger=false;
        }
        if(world.remoteVehicle==handle)world.remoteVehicle=-1;
        if(world.cameraFocus.kind==3&&world.cameraFocus.handle==handle) {
            if(world.cameraFocus.mode==16) {
                world.camera.active=false;world.camera.restoreRequested=true;
                world.camera.transitioning=false;world.camera.transitionPending=false;
                world.camera.vehicleUpdated=false;
            }
            world.cameraFocus={};
        }
        if(world.cameraLookAt.kind==3&&world.cameraLookAt.handle==handle)world.cameraLookAt={};
        world.radarBlips.erase(std::remove_if(world.radarBlips.begin(),world.radarBlips.end(),
            [&](const auto& blip){return blip.entity==StorylandMissionEnvironment::BlipEntity::Vehicle&&blip.entityHandle==handle;}),world.radarBlips.end());
        return 1;
    }
    if(command==0x0444) {world.requestedAnimationGroups.erase(handle);return 1;}
    if(command==0x04DE) {world.timeFrozen=handle!=0;return 1;}
    if(command==0x0313) {
        if(handle<0||handle>=24) {error="Extra colour index must be in the native 24-row table.";return -1;}
        world.extraColor=handle;world.extraColorFade=values[1].bits!=0;return 1;
    }
    if(command==0x0314) {
        world.extraColor=-1;world.extraColorFade=handle!=0;
        world.extraLightDirection={};world.extraLightDirectionSet=false;
        return 1;
    }
    if(command==0x02EA) {world.area=handle;return 1;}
    if(command==0x0404) {
        for(size_t index=0;index<3;++index) {
            if(!std::isfinite(real(index))) {error="Extra light direction is non-finite.";return -1;}
            world.extraLightDirection[index]=real(index);
        }
        world.extraLightDirectionSet=true;return 1;
    }
    if(command==0x031B) {world.vehicleModelComponents[handle]={values[1].bits,values[2].bits};return 1;}
    // SLUS_215.90 reads a channel and writes one position; its backend returns zero.
    if(command==0x052E)return store(1,0)?1:-1;
    if(command==0x028E) {
        world.radioChannel=handle;world.radioPosition=values[1].bits;world.radioChannelChanged=true;return 1;
    }
    if(command==0x02B4) {
        if(!std::isfinite(real(0))) {error="Camera interpolation percentage is not finite.";return -1;}
        world.camera.interpolationWeight=std::clamp(real(0)*0.01f,0.0f,1.0f);
        world.camera.interpolationMilliseconds=std::clamp(values[1].bits,0,600000);
        world.camera.interpolationScripted=true;
        return 1;
    }
    if(command==0x003E||command==0x00A4||command==0x0118) {
        StorylandMissionEnvironment::Character character;
        // CREATE_CHAR keeps the constructor's CPed+1D0 bit34 (0x41229C..0x4122E4).
        character.canBeShotInVehicle=!vm.vcs||command==0x003E;
        size_t output=5;
        if(command==0x003E) {
            character.type=handle;character.model=values[1].bits;
            character.x=real(2);character.y=real(3);character.z=real(4);
        } else {
            const auto vehicle=world.missionVehicles.find(handle);
            if(vehicle==world.missionVehicles.end()||vehicle->second.deleted) {error="CREATE_CHAR_INSIDE_CAR references an unknown vehicle.";return -1;}
            character.type=values[1].bits;character.model=values[2].bits;character.vehicle=handle;
            character.x=vehicle->second.x;character.y=vehicle->second.y;character.z=vehicle->second.z;
            character.heading=vehicle->second.heading;output=3;
            if(command==0x0118) {
                output=4;
                if(!world.vehiclePassengerCapacity) {error="Passenger creation requires native vehicle seating.";return -1;}
                const int capacity=world.vehiclePassengerCapacity(handle);
                if(capacity<1||capacity>8) {error="Vehicle has no supported passenger seating.";return -1;}
                const auto occupied=[&](int seat) {
                    for(const auto& entry:world.missionCharacters)
                        if(entry.second.alive&&entry.second.vehicle==handle&&entry.second.passengerSeat==seat)return true;
                    return false;
                };
                int seat=values[3].bits;
                if(seat<0) {seat=0;while(seat<capacity&&occupied(seat))++seat;}
                if(seat>=capacity||occupied(seat)) {error="Requested passenger seat is occupied or outside the vehicle.";return -1;}
                character.passengerSeat=seat;
            }
        }
        if(character.model<0||!std::isfinite(character.x)||!std::isfinite(character.y)||!std::isfinite(character.z)||
            !values[output].reference||world.missionCharacters.size()>=128||world.nextMissionCharacterHandle==INT32_MAX) {
            error="Character creation requires finite coordinates, a model and a free writable pool slot.";return -1;
        }
        // CREATE_CHAR 4122EC..412334 resolves the native ground sentinels.
        // Its +1 center offset is already represented by the host feet origin.
        if(vm.vcs&&command==0x003E&&!storylandNormalizeRestartHeight(world.groundHeightAt,
            character.x,character.y,character.z,true,error))return -1;
        if(!vm.vcs&&world.nextMissionCharacterHandle==1)world.nextMissionCharacterHandle=2;
        if(!vm.vcs&&world.resolvePedStats&&world.resolvePedStats(character.model,-1,character.pedStats))
            character.animationGroup=character.pedStats.animationGroup;
        if(vm.vcs) {
            if(world.initializeCharacterAppearance&&
                !world.initializeCharacterAppearance(character,error))return -1;
            character.nativePedRandomSeed=uint8_t(world.nextNativeRandom());
            character.nativePedRandomSeedInitialized=true;
        }
        const uint16_t walletDraw=uint16_t(world.nextNativeRandom())%25;
        character.walletMoney=walletDraw==23?400:walletDraw;
        character.walletInitialized=true;
        character.speechProfile=world.nativePedSpeechProfile?world.nativePedSpeechProfile(character.model):-1;
        const auto threats=world.pedTypeThreats.find(character.type);
        character.fearFlags=threats==world.pedTypeThreats.end()?0:threats->second;
        const int32_t result=world.nextMissionCharacterHandle++;
        world.missionCharacters.emplace(result,character);
        if(vm.isMissionThread)world.missionCleanupCharacters.insert(result);
        if(command==0x00A4) {
            world.missionVehicles[handle].driver=result;
            if(!vm.vcs)world.missionVehicles[handle].scriptStatus=3;
        }
        return store(output,result)?1:-1;
    }
    if(command==0x004A||command==0x01B6||command==0x004C||command==0x0050||command==0x02E9||command==0x0233||command==0x02C7) {
        const auto found=world.missionVehicles.find(handle);
        if(found==world.missionVehicles.end()) {error="Vehicle task references an unknown live vehicle.";return -1;}
        auto& vehicle=found->second;
        if(command==0x02C7) {
            vehicle.temporaryAction=values[1].bits;
            vehicle.temporaryActionUntil=world.gameMilliseconds+uint32_t(std::clamp(values[2].bits,0,600000));
            return 1;
        }
        if(command==0x004C) {
            vehicle.drivingToTarget=false;vehicle.aiActive=false;vehicle.aiMission=0;
            vehicle.speed=0;return 1;
        }
        if(command==0x0050||command==0x02E9) {
            const float speed=real(1);
            if(!std::isfinite(speed)) {error="Vehicle task speed is invalid.";return -1;}
            if(command==0x0050) {
                if(!world.vehicleMaximumAiSpeed) {error="Cruise speed requires native handling metadata.";return -1;}
                const float cap=world.vehicleMaximumAiSpeed(handle);
                if(!std::isfinite(cap)||cap<0) {error="Vehicle has no valid native AI speed limit.";return -1;}
                const int8_t requested=int8_t(uint8_t(storylandVcsSignedConversion(speed)));
                const int8_t capped=int8_t(uint8_t(storylandVcsSignedConversion(std::min(float(requested),cap*60.0f))));
                vehicle.cruiseSpeed=float(capped)*(5.0f/6.0f);
            } else {
                std::array<float,9> basis{};
                if(!world.vehicleBasis||!world.vehicleBasis(handle,basis)) {
                    error="Forward speed requires the vehicle's live native basis.";return -1;
                }
                vehicle.speed=speed*(5.0f/6.0f);
                vehicle.velocity={basis[3]*vehicle.speed,basis[4]*vehicle.speed,basis[5]*vehicle.speed};
                vehicle.velocityChangedByScript=true;
            }
            return 1;
        }
        for(size_t index=1;index<(command==0x0233?3u:4u);++index)if(!std::isfinite(real(index))) {
            error="Vehicle task has a non-finite coordinate.";return -1;
        }
        if(command==0x0233) {
            vehicle.heading=std::atan2(-(real(1)-vehicle.x),real(2)-vehicle.y)*57.2957795131f;
            vehicle.headingChangedByScript=true;
        }
        else {
            const bool accurate=vm.vcs&&command==0x01B6;
            float targetZ=real(3);
            if(accurate&&!storylandNormalizeRestartHeight(world.groundHeightAt,
                real(1),real(2),targetZ,true,error))return -1;
            if(command==0x004A||accurate) {
                const int8_t roadMission=accurate?12:8;
                const float dx=real(1)-vehicle.targetX;
                const float dy=real(2)-vehicle.targetY;
                const float dz=targetZ-vehicle.targetZ;
                if((vehicle.aiMission==roadMission||vehicle.aiMission==roadMission+1)&&
                    dx*dx+dy*dy+dz*dz<0.0100000007f)return 1;
                vehicle.autoPilotRoutePending=true;
                vehicle.scriptStatus=3;
                vehicle.cruiseSpeed=std::max(vehicle.cruiseSpeed,5.0f/6.0f);
                vehicle.aiMissionStartedAt=world.gameMilliseconds;
                if(vm.vcs)vehicle.aiMission=roadMission;
            }
            vehicle.targetX=real(1);vehicle.targetY=real(2);vehicle.targetZ=targetZ;
            vehicle.aiActive=command==0x004A||accurate;
            vehicle.drivingToTarget=true;
        }
        vehicle.movedByScript=true;return 1;
    }
    if(command==0x027F) {
        const auto vehicle=world.missionVehicles.find(handle);
        if(vehicle==world.missionVehicles.end()) {error="Vehicle offset references an unknown car.";return -1;}
        for(size_t index=1;index<4;++index)if(!std::isfinite(real(index))) {error="Vehicle offset is non-finite.";return -1;}
        std::array<float,16> matrix{};
        if(!world.vehicleWorldMatrix||!world.vehicleWorldMatrix(handle,matrix)) {
            error="Vehicle offset requires its live native body matrix.";return -1;
        }
        // The runtime road-placement position is below the native vehicle
        // origin. Use the actual body matrix, including ride height, pitch and
        // roll, for authored offsets such as Victor's intro dismount.
        for(size_t axis=0;axis<3;++axis)
            if(!storeReal(4+axis,matrix[12+axis]+matrix[axis]*real(1)+
                matrix[4+axis]*real(2)+matrix[8+axis]*real(3)))return -1;
        return 1;
    }
    const auto found=world.missionCharacters.find(handle);
    const bool player=handle==0;
    const bool alive=player?world.health>0:found!=world.missionCharacters.end()&&found->second.alive&&found->second.health>0;
    if(command==0x0045||command==0x009F) {updateScmCondition(vm,command==0x0045?alive:!alive,inverted);return 1;}
    if(command==0x003F) {
        // Native 0x412580/0x412590 ignores missing peds and player peds.
        if(player)return 1;
        storylandDeleteMissionCharacter(world,handle);
        return 1;
    }
    // Native 0x414750 returns immediately when GetPed cannot resolve the
    // release reference. Original mission cleanup also releases deleted peds.
    if(command==0x0112&&!player&&found==world.missionCharacters.end())return 1;
    if(command==0x02B7) {
        // Native 0x418894/0x4188A0 reject missing peds and empty attachments.
        if(!world.activeVcsCharacterAttachment(handle))return 1;
        storylandDetachVcsCharacter(world,handle);
        return 1;
    }
    if(!player&&found==world.missionCharacters.end()) {error="Character command references a missing live character.";return -1;}
    auto* character=player?nullptr:&found->second;
    if(command==0x0414||command==0x04B4) {
        const int32_t group=values[1].bits,animation=values[2].bits;
        const float blend=command==0x04B4?real(3):1000.0f;
        if(!std::isfinite(blend)||blend<0) {error="Character animation has an invalid blend rate.";return -1;}
        const bool loop=world.animationLoops&&world.animationLoops(group,animation);
        if(group<0||animation<0) {error="Animation command requires a valid group and animation.";return -1;}
        world.requestedAnimationGroups.insert(group);
        const float duration=world.animationDuration?world.animationDuration(group,animation):0;
        const bool available=alive&&std::isfinite(duration)&&duration>0&&
            world.animationGroupLoaded&&world.animationGroupLoaded(group);
        if(available) {
            if(character) {
                if(character->animationGroup!=group||character->animationId!=animation) {
                    character->animationGroup=group;character->animationId=animation;
                    character->animationStartedAt=world.gameMilliseconds;
                }
                character->animationBlend=blend;character->animationLoop=loop;
            } else {
                const auto request=world.pedAnimationRequests.find(0);
                if(request==world.pedAnimationRequests.end()||request->second.animationGroup!=group||
                    request->second.animationId!=animation)
                    world.pedAnimationRequests[0]={animation,loop?-1:int32_t(std::min(double(INT32_MAX),double(duration)*1000)),world.nextPedAnimationSerial++,group};
            }
        }
        updateScmCondition(vm,available,inverted);return 1;
    }
    if(command==0x0112) {
        if(character) {character->persistent=false;character->creationSource=1;}
        world.missionCleanupCharacters.erase(handle);return 1;
    }
    if(command==0x00EE) {
        if(!std::isfinite(real(1))||!std::isfinite(real(2))) {
            error="WALK_TO needs finite coordinates.";return -1;
        }
        if(player) {
            // 10C240 sets objective24 for either player or mission ped.
            // The native target Z is the player's foot height, not a ground query.
            world.playerVehicleEntryTarget=-1;world.playerVehicleEntryAsPassenger=false;
            world.playerWalkTask={true,false,real(1),real(2),world.z,.5f};
            world.playerScriptRunning=false;
            world.playerNavigation.clearRoute();
            if(world.inVehicle)world.playerExitVehicleRequested=true;
            return 1;
        }
        world.characterWanderTasks.erase(handle);
        character->targetX=real(1);character->targetY=real(2);character->walkingToTarget=true;
        character->objective = 0x00EE;
        character->objectiveRadius = 0.5f;
        character->objectiveCompleted = false;
        character->objectiveHeight = character->z;
        if (world.groundHeightAt) {
            float height = 0;
            character->objectiveHeight = world.groundHeightAt(
                {character->targetX, character->targetY, 1000}, height) ? height : 20.0f;
        }
        character->vehicle=-1;character->attached=false;return 1;
    }
    if(command==0x0043) {
        float x,y,z,heading;
        if(vm.vcs) {
            // 412614 resolves the pool entry; 412620 selects the occupied car.
            // 412640..412658 copies the entity origin, including after death.
            int32_t occupied=player?(world.inVehicle?world.occupiedMissionVehicle:-1):
                (character->attached?-1:character->vehicle);
            if(player&&world.inVehicle&&occupied<0&&world.acquireOccupiedVehicle) {
                occupied=world.acquireOccupiedVehicle();
                world.occupiedMissionVehicle=occupied;
            }
            const auto vehicle=world.missionVehicles.find(occupied);
            std::array<float,16> frame{};
            if(vehicle!=world.missionVehicles.end()&&!vehicle->second.deleted) {
                if(!world.vehicleAttachmentFrame||!world.vehicleAttachmentFrame(occupied,frame)) {
                    error="Character coordinates require the occupied vehicle's native entity frame.";return -1;
                }
            } else if(!world.characterWorldMatrix||!world.characterWorldMatrix(handle,frame)) {
                error="Character coordinates require the character's native entity frame.";return -1;
            }
            x=frame[12];y=frame[13];z=frame[14];
            if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)) {
                error="Character coordinates have a non-finite native entity origin.";return -1;
            }
        } else if(!vcsCharacterPosition(world,handle,x,y,z,heading)) {
            error="GET_CHAR_COORDINATES references a dead character.";return -1;
        }
        return storeReal(1,x)&&storeReal(2,y)&&storeReal(3,z)?1:-1;
    }
    if(command==0x0044||command==0x0216) {
        for(size_t index=1;index<4;++index)if(!std::isfinite(real(index))) {error="Character coordinates are non-finite.";return -1;}
        float z=real(3);
        // 412AE0/416A80 resolve the same sentinel heights through 196778.
        // Host character positions are feet, while 4126B0 adds the native
        // model-base offset to its center-origin entity position.
        if(vm.vcs&&!storylandNormalizeRestartHeight(world.groundHeightAt,
            real(1),real(2),z,true,error))return -1;
        if(command==0x0044) {
            const int32_t vehicleHandle=player?(world.inVehicle?world.occupiedMissionVehicle:-1):
                (character->attached?-1:character->vehicle);
            const auto vehicle=world.missionVehicles.find(vehicleHandle);
            if(vehicle!=world.missionVehicles.end()&&!vehicle->second.deleted) {
                vehicle->second.x=real(1);vehicle->second.y=real(2);vehicle->second.z=z;
                vehicle->second.movedByScript=true;
            }
        }
        if(command==0x0216) {
            const int32_t vehicleHandle=player?world.occupiedMissionVehicle:character->vehicle;
            const auto vehicle=world.missionVehicles.find(vehicleHandle);
            if(vehicle!=world.missionVehicles.end()&&vehicle->second.driver==handle) {
                // 416B64 removes the actual driver before clearing the ped's vehicle.
                auto& car=vehicle->second;
                car.driver=-1;car.scriptStatus=4;car.cruiseSpeed=0;car.speed=0;
                car.velocity={0,0,-.0005f};car.velocityChangedByScript=true;
                car.angularVelocity={};car.angularVelocityChangedByScript=true;
                car.aiActive=false;car.drivingToTarget=false;
            }
            world.characterWanderTasks.erase(handle);
            if(player) {
                world.occupiedMissionVehicle=-1;world.occupiedVehicleModel=-1;
                world.playerPassengerSeat=-1;world.playerVehicleTransition=false;
                world.manualAim.seatedActive=false;world.manualAim.initialized=false;
                world.playerExitVehicleRequested=false;
                world.playerAttachment={};
            } else {
                storylandCancelVcsVehicleEntry(world,handle,*character);
                character->passengerSeat=-1;character->attachment={};
                character->objective=0;character->objectiveTarget=-1;
                character->objectiveCompleted=false;character->walkingToTarget=false;
                character->seekRoute.clear();character->seekRouteCursor=0;
            }
        }
        if(player) {
            world.x=real(1);world.y=real(2);world.z=z;
            world.playerWarped=true;
            world.playerWalkTask.clear();
            world.playerVehicleEntryTarget=-1;world.playerVehicleEntryAsPassenger=false;
            if(command==0x0216) {world.playerAttachedVehicle=-1;world.inVehicle=false;}
        } else {
            character->x=real(1);character->y=real(2);character->z=z;
            if(command==0x0216) {character->vehicle=-1;character->attached=false;}
        }
        return 1;
    }
    if(command==0x014B) {
        if(player)world.health=float(values[1].bits);
        else {
            character->health=float(values[1].bits);
            if(character->health<=0) {
                character->alive=false;character->walkingToTarget=false;character->waitState=0;
                if(!character->deathRecorded) {
                    character->animationGroup=0;character->animationId=vm.vcs?16:13;
                    character->animationStartedAt=world.gameMilliseconds;character->animationBlend=4;
                    character->animationLoop=false;character->deathStartedAt=world.gameMilliseconds;
                    character->deathRecorded=true;world.scriptedCharacterAnimations.erase(handle);
                }
            }
        }
        return 1;
    }
    if(command==0x014D)return store(1,int32_t(player?world.health:character->health))?1:-1;
    if(command==0x00D7) {
        updateScmCondition(vm,(player?world.health:character->health)>float(values[1].bits),inverted);return 1;
    }
    if(command==0x00CE)return storeReal(1,player?world.heading:character->heading)?1:-1;
    if(command==0x00CF) {
        if(!std::isfinite(real(1))) {error="Character heading is non-finite.";return -1;}
        const int32_t vehicleHandle=player?(world.inVehicle?world.occupiedMissionVehicle:-1):
            (character->attached?-1:character->vehicle);
        const auto vehicle=world.missionVehicles.find(vehicleHandle);
        if(vehicle!=world.missionVehicles.end()&&!vehicle->second.deleted)return 1;
        if(player)world.heading=real(1);else character->heading=real(1);
        return 1;
    }
    if(command==0x0200) {if(player)world.playerVisible=values[1].bits!=0;else character->visible=values[1].bits!=0;return 1;}
    if(command==0x015F) {if(character)character->animationGroup=values[1].bits;return 1;}
    if(command==0x042B)return store(1,character?character->animationGroup:world.currentPedStat)?1:-1;
    if(command==0x04B5) {
        if(player)world.playerCollisionEnabled=values[1].bits!=0;
        else character->collisionEnabled=values[1].bits!=0;
        return 1;
    }
    if(command==0x01EC) {
        const bool running=values[1].bits!=0;
        if(player) {
            // 4162A8 changes CPed+1C8 bit14 independently of the current objective.
            world.playerScriptRunning=running;
        } else character->walkSpeed=running?3.8f:1.25f;
        return 1;
    }
    if(command==0x0154) {if(character)character->lookAtCharacter=-1;return 1;}
    if(command==0x0153) {if(character)character->lookAtCharacter=values[1].bits;return 1;}
    if(command==0x00DF||command==0x00E3||command==0x00A2||command==0x0042) {
        world.characterWanderTasks.erase(handle);
        if(character) {
            character->objective=command==0x00DF?1:command==0x00E3?2:0;
            character->objectiveCompleted = false;
            character->walkingToTarget = false;
            if (vm.vcs && command == 0x00A2) {
                character->objectiveTarget = -1;
                character->attackStartedAt = 0;
            }
        } else {
            world.playerVehicleEntryTarget=-1;world.playerVehicleEntryAsPassenger=false;
            world.playerWalkTask.clear();world.playerExitVehicleRequested=false;
        }
        return 1;
    }
    if(command==0x02B6||command==0x030F) {
        if((player&&world.inVehicle)||(!player&&character->vehicle>=0&&!character->attached))return 1;
        const bool objectParent=command==0x030F;
        std::array<float,16> parentFrame{};
        if(objectParent) {
            const auto object=std::find_if(world.objects.begin(),world.objects.end(),
                [&](const auto& candidate) {return candidate.handle==values[1].bits;});
            if(object==world.objects.end()) {error="ATTACH_PED_TO_OBJECT references an unknown object.";return -1;}
            parentFrame=storylandScriptObjectMatrix(object->x,object->y,object->z,
                object->rotationX,object->rotationY,object->heading);
        } else {
            const auto vehicle=world.missionVehicles.find(values[1].bits);
            if(vehicle==world.missionVehicles.end()||vehicle->second.deleted) {error="ATTACH_PED_TO_CAR references an unknown vehicle.";return -1;}
            if(!world.vehicleAttachmentFrame||!world.vehicleAttachmentFrame(values[1].bits,parentFrame)) {
                error="Character attachment requires its native vehicle origin.";return -1;
            }
        }
        std::array<float,3> offset{real(2),real(3),real(4)};
        for(float coordinate:offset)if(!std::isfinite(coordinate)) {error="Character attachment offset is non-finite.";return -1;}
        if(!std::isfinite(real(6))||real(6)<0) {error="Character attachment rotation limit is invalid.";return -1;}
        float originOffset=0;
        if(!world.characterOriginOffset||!world.characterOriginOffset(handle,originOffset)||
            !std::isfinite(originOffset)||!std::all_of(parentFrame.begin(),parentFrame.end(),
                [](float value) {return std::isfinite(value);})) {
            error="Character attachment requires finite native parent and character origins.";return -1;
        }
        auto attachment=world.vcsCharacterAttachments.count(handle)?
            world.vcsCharacterAttachments.at(handle):StorylandVcsCharacterAttachment{};
        attachment.type=uint16_t(values[5].bits);
        attachment.rotationLimit=real(6)*0.0174532942473888397216796875f;
        if(!storylandEquipVcsAttachmentWeapon(world,handle,values[7].bits,attachment,error))return -1;
        world.vcsCharacterAttachments[handle]=attachment;
        if(objectParent)world.vcsCharacterObjectParents[handle]=values[1].bits;
        else world.vcsCharacterObjectParents.erase(handle);
        if(player) {
            world.playerAttachedVehicle=objectParent?-1:values[1].bits;
            world.playerAttachment=offset;world.playerCollisionEnabled=false;
        }
        else {
            character->vehicle=objectParent?-1:values[1].bits;
            character->attachment=offset;character->attached=true;
            character->passengerSeat=-1;character->walkingToTarget=false;
            world.characterWanderTasks.erase(handle);world.characterVehicleTasks.erase(handle);
        }
        if(!storylandPositionVcsAttachedCharacter(world,handle)) {
            error="Character attachment could not resolve its native transform.";return -1;
        }
        return 1;
    }
    if(vm.vcs&&command==0x02EE) {
        const std::array<float,3> offset{real(1),real(2),real(3)};
        for(float value:offset)if(!std::isfinite(value)) {
            error="Character offset is non-finite.";return -1;
        }
        std::array<float,16> matrix{};
        if(!world.characterWorldMatrix||!world.characterWorldMatrix(handle,matrix)) {
            error="Character offset requires its actual native entity matrix.";return -1;
        }
        std::array<float,3> position{};
        for(size_t axis=0;axis<3;++axis) {
            // 4196E8..419798 rotates the offset before adding the entity origin.
            position[axis]=(matrix[axis]*offset[0]+matrix[4+axis]*offset[1]+
                matrix[8+axis]*offset[2])+matrix[12+axis];
            if(!std::isfinite(position[axis])) {
                error="Character offset has a non-finite native transform.";return -1;
            }
        }
        return storeReal(4,position[0])&&storeReal(5,position[1])&&storeReal(6,position[2])?1:-1;
    }
    float x,y,z,heading;
    if(!vcsCharacterPosition(world,handle,x,y,z,heading)) {error="Character transform references a dead character.";return -1;}
    if(command==0x02EE) {
        for(size_t index=1;index<4;++index)if(!std::isfinite(real(index))) {error="Character offset is non-finite.";return -1;}
        const float angle=heading*0.0174532925199433f,c=std::cos(angle),s=std::sin(angle);
        return storeReal(4,x+c*real(1)-s*real(2))&&storeReal(5,y+s*real(1)+c*real(2))&&storeReal(6,z+real(3))?1:-1;
    }
    float targetX=real(1),targetY=real(2);
    if(command==0x013E) {
        float targetZ,targetHeading;
        if(!vcsCharacterPosition(world,values[1].bits,targetX,targetY,targetZ,targetHeading)) {
            error="TURN_CHAR_TO_FACE_CHAR references a missing target.";return -1;
        }
    }
    if(!std::isfinite(targetX)||!std::isfinite(targetY)) {error="Character facing target is non-finite.";return -1;}
    heading=std::atan2(-(targetX-x),targetY-y)*57.2957795131f;
    if(player)world.heading=heading;else character->heading=heading;
    return 1;
}

void storylandChooseScriptWanderTarget(StorylandMissionEnvironment& world,
    StorylandMissionEnvironment::Character& character,StorylandMissionEnvironment::WanderTask& task) {
    if(!world.pedestrianPaths||character.vehicle>=0||character.attached)return;
    const auto& nodes=world.pedestrianPaths->nodes;
    const auto roadFlags=storylandNativeRoadNodeFlags(*world.pedestrianPaths,world.roadChanges);
    const auto usable=[&](size_t index) {
        if(index>=nodes.size()||nodes[index].vehicle||nodes[index].water)return false;
        return (roadFlags[index]&0x20u)==0;
    };
    if(!usable(task.node)||std::hypot(character.x-nodes[task.node].position.x,
        character.y-nodes[task.node].position.y)>std::sqrt(7.0f)) {
        float best=1000000.0f;
        task.node=UINT32_MAX;
        for(size_t index=0;index<nodes.size();++index) {
            if(!usable(index))continue;
            const auto& position=nodes[index].position;
            const float distance=std::hypot(character.x-position.x,character.y-position.y,character.z-position.z);
            if(distance<best) {best=distance;task.node=uint32_t(index);}
        }
    }
    if(!usable(task.node))return;
    const auto& node=nodes[task.node];
    const float angle=float(task.direction)*0.7853981633974483f;
    const float directionX=std::sin(angle),directionY=std::cos(angle);
    float best=-2.0f;
    uint32_t selected=UINT32_MAX;
    for(const auto& link:node.links) {
        if(!usable(link.target))continue;
        const auto& target=nodes[link.target].position;
        const float dx=target.x-node.position.x,dy=target.y-node.position.y;
        const float length=std::hypot(dx,dy);
        if(length<0.01f)continue;
        if(world.pedestrianLineClear&&!world.pedestrianLineClear(
            {character.x,character.y,character.z+1.0f},{target.x,target.y,target.z+1.0f}))continue;
        const float dot=(dx*directionX+dy*directionY)/length;
        if(dot>=best) {best=dot;selected=link.target;}
    }
    if(selected==UINT32_MAX)return;
    const auto& target=nodes[selected].position;
    const float dx=target.x-node.position.x,dy=target.y-node.position.y;
    if(dx<0) {
        if(2*std::fabs(dy)<-dx)task.direction=6;
        else if(-2*dx<dy)task.direction=0;
        else if(2*dx>dy)task.direction=4;
        else task.direction=dy>0?7:5;
    } else {
        if(2*std::fabs(dy)<dx)task.direction=2;
        else if(2*dx<dy)task.direction=0;
        else if(-2*dx>dy)task.direction=4;
        else task.direction=dy>0?1:3;
    }
    task.node=selected;
    character.targetX=target.x;
    character.targetY=target.y;
    character.walkingToTarget=true;
}

bool storylandSeekLocalPedAvoidance(StorylandMissionEnvironment& world,
    StorylandMissionEnvironment::Character& character,float& x,float& y) {
    if(!character.behavior.localAvoidanceSolver||!world.pedestrianLineClear)return true;
    const auto clear=[&](float fromX,float fromY,float toX,float toY) {
        return world.pedestrianLineClear({fromX,fromY,character.z+1.0f},{toX,toY,character.z+1.0f});
    };
    if(clear(character.x,character.y,x,y))return true;
    const float dx=x-character.x,dy=y-character.y,distance=std::hypot(dx,dy);
    if(distance<=0.05f)return false;
    const float heading=std::atan2(dy,dx),radius=std::min(2.0f,distance);
    float best=std::numeric_limits<float>::max(),targetX=x,targetY=y;
    // Native 17D8A8 substitutes a local collision-avoidance destination. The
    // host collision backend supplies clear segments rather than its navmesh.
    for(float turn:{0.785398163f,-0.785398163f,1.570796327f,-1.570796327f,
        2.356194490f,-2.356194490f,3.141592654f}) {
        const float nextX=character.x+std::cos(heading+turn)*radius;
        const float nextY=character.y+std::sin(heading+turn)*radius;
        if(!clear(character.x,character.y,nextX,nextY)||!clear(nextX,nextY,x,y))continue;
        const float score=radius+std::hypot(x-nextX,y-nextY);
        if(score<best) {best=score;targetX=nextX;targetY=nextY;}
    }
    if(best==std::numeric_limits<float>::max())return false;
    x=targetX;y=targetY;
    return true;
}

void storylandWaitForScriptPedObstruction(StorylandMissionEnvironment& world,
    int32_t handle,StorylandMissionEnvironment::Character& character) {
    character.seekRoute.clear();
    character.seekWaitUntil=world.gameMilliseconds+500;
    if(!character.behavior.alternateObstructionHandling)return;
    // Native 204970 enables the failed-obstruction branch's WAITSTATE_TRAFFIC_LIGHTS.
    character.waitState=1;
    character.waitDeadline=character.seekWaitUntil;
    world.pedWaitStateMilliseconds[handle]=500;
}

bool storylandScriptPedNodeUsable(const StorylandMissionEnvironment& world,uint32_t index) {
    if(!world.pedestrianPaths||index>=world.pedestrianPaths->nodes.size())return false;
    const auto& node=world.pedestrianPaths->nodes[index];
    if(node.vehicle||node.water)return false;
    bool enabled=!node.disabled;
    for(const auto& change:world.roadChanges)if(!change.vehicle&&
        node.position.x>=change.bounds[0]&&node.position.x<=change.bounds[3]&&
        node.position.y>=change.bounds[1]&&node.position.y<=change.bounds[4]&&
        node.position.z>=change.bounds[2]&&node.position.z<=change.bounds[5])enabled=change.enabled;
    return enabled;
}

bool storylandSeekScriptPedNode(StorylandMissionEnvironment& world,int32_t handle,
    StorylandMissionEnvironment::Character& character,float& x,float& y) {
    if(!character.seekNodes && !character.followingScriptPath &&
        !(character.creationSource==2&&character.behavior.seekNodesForMission))return true;
    // Original 204AA4..204B04 only searches the node network when the
    // squared planar seek distance is at least the square of this signed byte.
    const float dx=x-character.x,dy=y-character.y;
    if(storylandVcsWithinSeekRadius(character.vcsSeekNodeRadius,dx,dy))return true;
    if(!world.pedestrianPaths||!world.pedestrianLineClear)return false;
    if(character.seekWaitUntil&&int32_t(world.gameMilliseconds-character.seekWaitUntil)<0)return false;
    const auto clear=[&](float fromX,float fromY,float fromZ,float toX,float toY,float toZ) {
        return world.pedestrianLineClear({fromX,fromY,fromZ+1.0f},{toX,toY,toZ+1.0f});
    };
    const bool coordinateObjective = character.followingScriptPath ||
        character.objective == 0x00E1 || character.objective == 0x00E2 ||
        character.objective == 0x011B || character.objective == 0x00ED || character.objective == 0x00EE ||
        character.objective == 0x00F5 || character.objective == 0x0158;
    const float destinationZ = coordinateObjective ? character.objectiveHeight : character.z;
    if(clear(character.x,character.y,character.z,x,y,destinationZ)) {
        character.seekRoute.clear();character.seekRouteCursor=0;character.seekNode=UINT32_MAX;
        return true;
    }
    const auto& nodes=world.pedestrianPaths->nodes;
    if(character.seekRoute.empty()||std::hypot(character.seekDestination[0]-x,
        character.seekDestination[1]-y)>1.0f) {
        uint32_t start=UINT32_MAX,finish=UINT32_MAX;
        float startDistance=std::numeric_limits<float>::max(),finishDistance=startDistance;
        for(size_t index=0;index<nodes.size();++index) {
            if(!storylandScriptPedNodeUsable(world,uint32_t(index)))continue;
            const auto& point=nodes[index].position;
            const float from=std::hypot(character.x-point.x,character.y-point.y,character.z-point.z);
            const float to=std::hypot(x-point.x,y-point.y,destinationZ-point.z);
            if(from<startDistance&&clear(character.x,character.y,character.z,point.x,point.y,point.z)) {
                start=uint32_t(index);startDistance=from;
            }
            if(to<finishDistance&&clear(point.x,point.y,point.z,x,y,destinationZ)) {
                finish=uint32_t(index);finishDistance=to;
            }
        }
        character.seekRoute.clear();character.seekRouteCursor=0;
        character.seekDestination={x,y};
        if(start==UINT32_MAX||finish==UINT32_MAX) {
            character.seekWaitUntil=world.gameMilliseconds+500;return false;
        }
        std::vector<float> distances(nodes.size(),std::numeric_limits<float>::max());
        std::vector<uint32_t> previous(nodes.size(),UINT32_MAX);
        std::set<std::pair<float,uint32_t>> pending;
        pending.emplace(0.0f,start);distances[start]=0.0f;
        while(!pending.empty()) {
            const uint32_t node=pending.begin()->second;
            pending.erase(pending.begin());
            if(node==finish)break;
            for(const auto& link:nodes[node].links) {
                if(!storylandScriptPedNodeUsable(world,link.target))continue;
                const auto& a=nodes[node].position;const auto& b=nodes[link.target].position;
                if(!clear(a.x,a.y,a.z,b.x,b.y,b.z))continue;
                const float next=distances[node]+std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);
                if(next>=distances[link.target])continue;
                pending.erase({distances[link.target],link.target});
                distances[link.target]=next;previous[link.target]=node;
                pending.emplace(next,link.target);
            }
        }
        if(finish!=start&&previous[finish]==UINT32_MAX) {
            character.seekWaitUntil=world.gameMilliseconds+500;return false;
        }
        for(uint32_t node=finish;node!=UINT32_MAX;node=previous[node]) {
            character.seekRoute.push_back(node);
            if(node==start)break;
        }
        std::reverse(character.seekRoute.begin(),character.seekRoute.end());
    }
    while(character.seekRouteCursor<character.seekRoute.size()) {
        const uint32_t index=character.seekRoute[character.seekRouteCursor];
        if(!storylandScriptPedNodeUsable(world,index)) {
            character.seekRoute.clear();character.seekWaitUntil=world.gameMilliseconds+500;return false;
        }
        const auto& node=nodes[index];
        if(std::hypot(character.x-node.position.x,character.y-node.position.y)<.35f) {
            ++character.seekRouteCursor;continue;
        }
        character.seekNode=index;x=node.position.x;y=node.position.y;
        return true;
    }
    character.seekRoute.clear();character.seekRouteCursor=0;character.seekNode=UINT32_MAX;
    return clear(character.x,character.y,character.z,x,y,destinationZ);
}

void storylandAdvanceVcsLeaderPolicy(StorylandMissionEnvironment& world, int32_t handle,
    StorylandMissionEnvironment::Character& character) {
    if (character.leader < 0) return;
    float x = 0, y = 0, z = 0, heading = 0;
    if (!vcsCharacterPosition(world, character.leader, x, y, z, heading)) {
        storylandClearVcsLeader(world, handle, character);
        return;
    }
    const auto leader = world.missionCharacters.find(character.leader);
    const bool playerLeader = character.leader == 0;
    const int32_t leaderVehicle = playerLeader ?
        (world.inVehicle ? world.occupiedMissionVehicle : -1) : leader->second.vehicle;
    const auto vehicle = world.missionVehicles.find(leaderVehicle);
    const bool leaderInVehicle = vehicle != world.missionVehicles.end() &&
        !vehicle->second.deleted && vehicle->second.alive;
    if (leaderInVehicle) {
        x = vehicle->second.x;
        y = vehicle->second.y;
        z = vehicle->second.z;
    }
    const float dx = x - character.x, dy = y - character.y, dz = z - character.z;
    const float distanceSquared = (dx * dx + dy * dy) + dz * dz;
    if (character.leaderUpdateAfter >= world.gameMilliseconds && distanceSquared <= 225.0f) return;

    const auto clearObjective = [&]() {
        character.objective = 0;
        character.objectiveTarget = -1;
        character.leaderUpdateAfter = 0;
        character.walkingToTarget = false;
        character.followingScriptPath = false;
        character.seekRoute.clear();
        character.seekRouteCursor = 0;
        character.seekNode = UINT32_MAX;
        world.characterTargets.erase(handle);
        world.characterWanderTasks.erase(handle);
    };
    const auto follow = [&]() {
        if (character.objective != StorylandVcsLeaderFollowObjective ||
            character.objectiveTarget != character.leader) {
            character.seekRoute.clear();
            character.seekRouteCursor = 0;
            character.seekNode = UINT32_MAX;
        }
        character.objective = StorylandVcsLeaderFollowObjective;
        character.objectiveTarget = character.leader;
        character.objectiveCompleted = false;
        character.leaderUpdateAfter = 0;
        world.characterWanderTasks.erase(handle);
    };
    const bool controlled = character.vehicle < 0 && !character.attached &&
        (!world.characterReactionOwned || !world.characterReactionOwned(handle));
    const bool alreadyOutOfRange = character.leaderOutOfRange;
    character.leaderOutOfRange = distanceSquared > 900.0f;
    if (character.leaderOutOfRange && alreadyOutOfRange) {
        if (!controlled) return;
        if (character.creationSource == 1) follow();
        else clearObjective();
        return;
    }

    if (!controlled) {
        if (character.vehicle < 0 || character.attached) return;
        const bool leaderExiting = playerLeader ? world.playerExitVehicleRequested :
            world.vcsCharacterExitingVehicle(character.leader);
        if (leaderInVehicle && !leaderExiting) return;
        const bool leaderEnteringSameVehicle = playerLeader ?
            world.playerVehicleTransition && world.playerVehicleEntryTarget == character.vehicle :
            (leader->second.objective == 0x00E9 || leader->second.objective == 0x00EA ||
                leader->second.objective == 0x00F0) &&
                leader->second.objectiveTarget == character.vehicle;
        if (leaderEnteringSameVehicle || world.vcsCharacterExitingVehicle(handle)) return;
        const uint32_t delay = 250u + uint32_t((uint64_t(world.nextNativeRandom()) * 1250u) >> 31);
        character.leaderVehicleAfter = world.gameMilliseconds + delay;
        character.objective = 0x00F1;
        character.objectiveTarget = character.vehicle;
        character.objectiveCompleted = false;
        character.walkingToTarget = false;
        return;
    }

    if (leaderInVehicle) {
        if (character.objective == 0x00E9 || character.objective == 0x00EA) return;
        const int capacity = world.vehiclePassengerCapacity ? world.vehiclePassengerCapacity(leaderVehicle) : 0;
        int occupied = 0;
        for (int seat = 0; seat < capacity; ++seat)
            if (storylandVcsPassenger(world, leaderVehicle, seat) >= 0) ++occupied;
        if (occupied >= capacity) return;
        character.objective = 0x00E9;
        character.objectiveTarget = leaderVehicle;
        character.objectiveCompleted = false;
        character.leaderVehicleAfter = 0;
        character.walkSpeed = 3.8f;
        world.characterWanderTasks.erase(handle);
        return;
    }

    const bool attacking = (world.characterShooting && world.characterShooting(handle)) ||
        (world.characterFighting && world.characterFighting(handle));
    const bool movingPlayerLeader = playerLeader && world.playerCrouch.movementState >= 2;
    if (attacking && !(movingPlayerLeader && distanceSquared > 225.0f)) return;
    follow();
}

void storylandCancelVcsVehicleEntry(StorylandMissionEnvironment& world, int32_t handle,
    StorylandMissionEnvironment::Character& character) {
    const auto active = world.characterVehicleTasks.find(handle);
    if (active == world.characterVehicleTasks.end()) return;
    const auto& task = active->second;
    if (task.animating && character.vehicle < 0) {
        character.x = task.door[0];
        character.y = task.door[1];
        character.z = task.door[2];
    } else if (task.animating && character.vehicle == task.vehicle) {
        character.x = task.seatPosition[0];
        character.y = task.seatPosition[1];
        character.z = task.seatPosition[2];
    }
    const int nativeType = world.vehicleNativeType ? world.vehicleNativeType(task.vehicle) : -1;
    const bool saddle = nativeType == 6 || nativeType == 9;
    if (world.updateVehicleTaskDoor && task.animating && !saddle)
        world.updateVehicleTaskDoor(task.vehicle, task.seat, unsigned(StorylandVehiclePhase::Done), 0);
    world.characterVehicleTasks.erase(active);
}

void storylandAdvanceVcsVehicleEntry(StorylandMissionEnvironment& world, int32_t handle,
    StorylandMissionEnvironment::Character& character) {
    using Phase = StorylandVehiclePhase;
    const auto vehicle = world.missionVehicles.find(character.objectiveTarget);
    if (vehicle == world.missionVehicles.end() || vehicle->second.deleted || !vehicle->second.alive) {
        storylandCancelVcsVehicleEntry(world, handle, character);
        character.walkingToTarget = false;
        character.objective = 0;
        character.objectiveCompleted = false;
        return;
    }
    auto& car = vehicle->second;
    const int nativeType = world.vehicleNativeType ? world.vehicleNativeType(vehicle->first) : -1;
    const bool quad = nativeType == 9;
    const bool bike = nativeType == 6;
    const bool saddle = quad || bike;
    const bool nativeAutomobile = nativeType == 0 && character.objective == 0x00E9;
    const bool nativeAutomobileSeek = nativeType == 0;
    const auto reserved = [&](int seat) {
        if (!world.inVehicle && world.playerVehicleTransition &&
            world.playerVehicleEntryTarget == vehicle->first && world.playerPassengerSeat == seat) return true;
        for (const auto& entry : world.characterVehicleTasks)
            if (entry.first != handle && !entry.second.exiting &&
                entry.second.vehicle == vehicle->first && entry.second.seat == seat) return true;
        return false;
    };
    const auto occupied = [&](int seat) {
        const int32_t occupant = seat < 0 ? car.driver : storylandVcsPassenger(world, vehicle->first, seat);
        return (occupant >= 0 && occupant != handle) || reserved(seat);
    };
    const auto protectedDriver = [&]() {
        if (world.inVehicle && world.occupiedMissionVehicle == vehicle->first &&
            world.playerPassengerSeat < 0)
            return world.playerCantBeDraggedOut || !world.control;
        const auto driver = world.missionCharacters.find(car.driver);
        return driver != world.missionCharacters.end() && driver->first != handle &&
            (driver->second.cannotBeDraggedOut || driver->second.cantBeDraggedOut);
    };
    const auto prepare = [&](StorylandMissionEnvironment::CharacterVehicleTask& task) {
        std::string error;
        if (!world.prepareCharacterVehicleTask(handle, task, error)) return false;
        for (float seconds : task.phaseSeconds)
            if (!std::isfinite(seconds) || seconds < 0) return false;
        for (const auto* point : {&task.door, &task.seatPosition, &task.vehicleOrigin})
            for (float coordinate : *point) if (!std::isfinite(coordinate)) return false;
        for (const auto& point : task.route)
            for (float coordinate : point) if (!std::isfinite(coordinate)) return false;
        StorylandVehiclePhaseTiming timing;
        timing.seconds = task.phaseSeconds;
        const auto action = task.hijacking ? StorylandVehicleAction::Hijack : StorylandVehicleAction::Enter;
        return (!task.hijacking || task.phaseSeconds[unsigned(Phase::PullOut)] > 0) &&
            storylandSampleVehicleTransition(action, timing, 0).total > 0;
    };
    const auto preparePassenger = [&](StorylandMissionEnvironment::CharacterVehicleTask& task) {
        std::array<StorylandMissionEnvironment::CharacterVehicleTask,3> candidates{};
        std::array<std::array<float,3>,3> doors{};
        uint8_t available = 0;
        const int capacity = world.vehiclePassengerCapacity ? world.vehiclePassengerCapacity(vehicle->first) : 0;
        const int permitted = character.behavior.onlyFrontPassenger ? std::min(capacity,1) : std::min(capacity,3);
        for (int seat = character.allowFrontPassenger ? 0 : 1; seat < permitted; ++seat) {
            if (occupied(seat)) continue;
            auto& candidate = candidates[size_t(seat)];
            candidate.vehicle = vehicle->first;
            candidate.seat = seat;
            if (!prepare(candidate)) continue;
            doors[size_t(seat)] = candidate.door;
            available |= uint8_t(1u << seat);
        }
        // Original305978 selects the nearest room-valid anchor, then Seek approaches it.
        const int seat = storylandVcsNearestPassengerSeat(character.x,character.y,doors,available);
        if (seat < 0) return false;
        task = std::move(candidates[size_t(seat)]);
        return true;
    };

    character.walkingToTarget = false;
    auto active = world.characterVehicleTasks.find(handle);
    bool approachPrepared = false;
    if (active == world.characterVehicleTasks.end()) {
        StorylandMissionEnvironment::CharacterVehicleTask task;
        task.vehicle = vehicle->first;
        int seat = -1;
        if (character.objective != 0x00E9) {
            if (reserved(seat) || protectedDriver()) return;
            if (character.objective == 0x00F0 && car.driver < 0 && world.acquireVehicleDriver) {
                const int32_t driver = world.acquireVehicleDriver(vehicle->first);
                if (driver >= 0) car.driver = driver;
            }
        } else if (nativeAutomobile) {
            if (!preparePassenger(task)) return;
            approachPrepared = true;
        } else {
            const int capacity = world.vehiclePassengerCapacity ? world.vehiclePassengerCapacity(vehicle->first) : 0;
            const int permitted = character.behavior.onlyFrontPassenger ? std::min(capacity, 1) : capacity;
            seat = character.allowFrontPassenger ? 0 : 1;
            while (seat < permitted && occupied(seat)) ++seat;
            if (seat >= permitted) return;
        }
        if (!approachPrepared) {
            task.seat = seat;
            if (!prepare(task)) return;
        }
        active = world.characterVehicleTasks.emplace(handle, std::move(task)).first;
        approachPrepared = true;
    }
    auto& task = active->second;
    if (occupied(task.seat) && !(task.seat < 0 && task.hijacking &&
        !task.driverRemoved && !reserved(task.seat) && !protectedDriver())) {
        storylandCancelVcsVehicleEntry(world, handle, character);
        return;
    }
    if (task.hijacking && !task.driverRemoved && protectedDriver()) {
        storylandCancelVcsVehicleEntry(world, handle, character);
        return;
    }
    const float rotation = (car.heading - task.vehicleHeading) * 0.0174532925199433f;
    const float cosine = std::cos(rotation), sine = std::sin(rotation);
    const auto moveAnchor = [&](std::array<float,3>& point) {
        const float x = point[0] - task.vehicleOrigin[0], y = point[1] - task.vehicleOrigin[1];
        point = {car.x + cosine * x - sine * y, car.y + sine * x + cosine * y,
            car.z + point[2] - task.vehicleOrigin[2]};
    };
    moveAnchor(task.door);
    moveAnchor(task.seatPosition);
    for (auto& point : task.route) moveAnchor(point);
    task.vehicleOrigin = {car.x, car.y, car.z};
    task.vehicleHeading = car.heading;
    if (character.positionFrozen) {
        if (task.animating) task.startedAt += world.nativeFrameMilliseconds;
        return;
    }
    if (!task.animating) {
        const bool stagedApproach = storylandVcsKeepStagedEntryRoute(nativeAutomobile,
            task.rightHandSide, car.heading, task.door, task.route, task.routeIndex,
            character.movementSpeed, world.gameMilliseconds, task.startedAt);
        if (!approachPrepared && !stagedApproach && !(nativeAutomobile && character.objective == 0x00E9 ?
            preparePassenger(task) : prepare(task))) return;
        if (task.route.empty()) return;
        while (task.routeIndex < task.route.size()) {
            const bool finalPoint = task.routeIndex + 1 == task.route.size();
            const auto& target = (bike || nativeAutomobileSeek) && finalPoint ?
                task.door : task.route[task.routeIndex];
            if (nativeAutomobileSeek && finalPoint) {
                if (!storylandVcsHelicopterSeekReached(character.x,character.y,target[0],target[1])) break;
            } else if (std::hypot(character.x - target[0], character.y - target[1]) > 0.3f) break;
            ++task.routeIndex;
        }
        if (task.routeIndex < task.route.size()) {
            const auto& target = nativeAutomobileSeek && task.routeIndex + 1 == task.route.size() ?
                task.door : task.route[task.routeIndex];
            character.targetX = target[0];
            character.targetY = target[1];
            character.objectiveHeight = task.route[task.routeIndex][2];
            character.walkSpeed = 3.8f;
            character.walkingToTarget = !character.stayInSamePlace;
            return;
        }
        if (std::fabs(car.speed) >= 0.5f ||
            (!quad && !storylandRetailVehicleLocksOpen(true, car.doorLock, false))) return;
        task.animating = true;
        task.startedAt = world.gameMilliseconds;
        task.start = {character.x, character.y, character.z};
    }
    StorylandVehiclePhaseTiming timing;
    timing.seconds = task.phaseSeconds;
    const auto sample = storylandSampleVehicleTransition(task.hijacking ?
        StorylandVehicleAction::Hijack : StorylandVehicleAction::Enter, timing,
        float(world.gameMilliseconds - task.startedAt) * 0.001f);
    if (task.hijacking && !task.driverRemoved &&
        (sample.phase == Phase::GetIn || sample.phase == Phase::CloseInside || sample.phase == Phase::Done)) {
        const int32_t driverHandle = car.driver;
        if (world.inVehicle && world.occupiedMissionVehicle == task.vehicle && world.playerPassengerSeat < 0) {
            world.inVehicle = false;
            world.occupiedMissionVehicle = -1;
            world.playerPassengerSeat = -1;
            world.playerVehicleTransition = false;
            world.playerExitVehicleRequested = false;
            world.x = task.door[0]; world.y = task.door[1]; world.z = task.door[2];
            world.playerWarped = true;
        }
        const auto driver = world.missionCharacters.find(driverHandle);
        if (driver != world.missionCharacters.end() && driverHandle != handle) {
            auto& displaced = driver->second;
            displaced.vehicle = -1; displaced.passengerSeat = -1;
            displaced.attached = false; displaced.attachment = {};
            displaced.x = task.door[0]; displaced.y = task.door[1]; displaced.z = task.door[2];
            displaced.walkingToTarget = false;
            displaced.objective = displaced.behavior.frightenedWhenJacked ? 0x00F2 : 0x00E4;
            displaced.objectiveTarget = displaced.behavior.frightenedWhenJacked ? task.vehicle : handle;
            displaced.objectiveCompleted = false;
            world.characterVehicleTasks.erase(driverHandle);
            if(nativeAutomobileSeek)
                storylandBeginVcsCarjackedGetUp(world,driverHandle,task.vehicle,task.low,task.rightHandSide,
                    std::max(0.0f,float(world.gameMilliseconds-task.startedAt)*.001f-
                        task.phaseSeconds[0]-task.phaseSeconds[1]-task.phaseSeconds[2]));
        }
        if (world.removeVehicleDriver) world.removeVehicleDriver(task.vehicle, task.door);
        car.driver = -1;
        task.driverRemoved = true;
        for (auto& occupant : world.missionCharacters) {
            auto& passenger = occupant.second;
            if (occupant.first == handle || passenger.vehicle != task.vehicle || passenger.attached ||
                passenger.stayInCarWhenJacked) continue;
            passenger.objective = 0x00F1;
            passenger.objectiveTarget = task.vehicle;
            passenger.objectiveCompleted = false;
        }
        if (world.inVehicle && world.occupiedMissionVehicle == task.vehicle &&
            world.playerPassengerSeat >= 0 && !world.playerStayInCarWhenJacked) {
            world.playerExitVehicleRequested = true;
            world.playerVehicleTransition = true;
        }
    }
    auto position = task.door;
    if (sample.phase == Phase::GetIn)
        for (size_t axis = 0; axis < position.size(); ++axis)
            position[axis] += (task.seatPosition[axis] - position[axis]) * sample.fraction;
    else if (sample.phase == Phase::CloseInside || sample.phase == Phase::Done)
        position = task.seatPosition;
    character.x = position[0];
    character.y = position[1];
    character.z = position[2];
    character.heading = car.heading;
    if (world.updateVehicleTaskDoor && !saddle)
        world.updateVehicleTaskDoor(task.vehicle, task.seat, unsigned(sample.phase), sample.elapsed);
    if (sample.phase == Phase::CloseInside || sample.phase == Phase::Done) {
        character.vehicle = task.vehicle;
        character.passengerSeat = task.seat;
        character.attached = false;
        character.attachment = {};
        if (task.seat < 0) {
            if (nativeType == 4 && car.driver != handle) {
                // Native1FF2E4: an animated non-player driver takes status3.
                if (car.scriptStatus == 2) {
                    car.speed = 0;
                    car.velocity = {};
                    car.angularVelocity = {};
                    car.velocityChangedByScript = true;
                    car.angularVelocityChangedByScript = true;
                }
                car.scriptStatus = 3;
            }
            car.driver = handle;
        }
    }
    if (sample.phase == Phase::Done) {
        if (task.seat < 0) car.engineOn = true;
        character.objective = 0;
        character.objectiveTarget = -1;
        character.objectiveCompleted = true;
        character.attackStartedAt = 0;
        world.characterTargets.erase(handle);
        world.characterVehicleTasks.erase(active);
    }
}

bool storylandAdvanceVcsVehicleExit(StorylandMissionEnvironment& world, int32_t handle,
    StorylandMissionEnvironment::Character& character) {
    using Phase = StorylandVehiclePhase;
    character.walkingToTarget = false;
    auto active = world.characterVehicleTasks.find(handle);
    const int32_t vehicleHandle = active == world.characterVehicleTasks.end() ?
        character.vehicle : active->second.vehicle;
    const auto vehicle = world.missionVehicles.find(vehicleHandle);
    if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
        storylandCancelVcsVehicleEntry(world, handle, character);
        character.vehicle = -1;
        character.passengerSeat = -1;
        return true;
    }
    auto& car = vehicle->second;
    const int nativeType = world.vehicleNativeType ? world.vehicleNativeType(vehicleHandle) : -1;
    const bool saddle = nativeType == 6 || nativeType == 9;
    if (active == world.characterVehicleTasks.end()) {
        StorylandMissionEnvironment::CharacterVehicleTask task;
        task.vehicle = vehicleHandle;
        task.seat = character.passengerSeat;
        task.exiting = true;
        active = world.characterVehicleTasks.emplace(handle, std::move(task)).first;
    }
    auto& task = active->second;
    if (character.positionFrozen) {
        if (task.animating) task.startedAt += world.nativeFrameMilliseconds;
        return false;
    }
    if (!task.animating) {
        if (!world.vehicleCanExit || !world.vehicleCanExit(vehicleHandle)) {
            // SetExitCar302978 asks a non-player driver to stop; it does not
            // discard the body's velocity or eject its occupant.
            if (car.driver > 0 && car.aiMission != 28) {
                car.aiMission = 0;
                car.cruiseSpeed = 0;
                car.drivingToTarget = false;
            }
            return false;
        }
        std::string error;
        if (!world.prepareCharacterVehicleTask ||
            !world.prepareCharacterVehicleTask(handle, task, error) || task.route.empty()) return false;
        for (float seconds : task.phaseSeconds)
            if (!std::isfinite(seconds) || seconds < 0) return false;
        for (const auto* point : {&task.door, &task.seatPosition, &task.vehicleOrigin})
            for (float coordinate : *point) if (!std::isfinite(coordinate)) return false;
        if (task.phaseSeconds[unsigned(Phase::GetOut)] <= 0 &&
            std::any_of(task.phaseSeconds.begin(), task.phaseSeconds.end(),
                [](float seconds) { return seconds != 0; })) return false;
        task.animating = true;
        task.startedAt = world.gameMilliseconds;
        task.start = task.seatPosition;
        if (car.driver == handle) { car.scriptStatus = 4; car.cruiseSpeed = 0; }
    } else {
        const float rotation = (car.heading - task.vehicleHeading) * 0.0174532925199433f;
        const float cosine = std::cos(rotation), sine = std::sin(rotation);
        const auto moveAnchor = [&](std::array<float,3>& point) {
            const float x = point[0] - task.vehicleOrigin[0], y = point[1] - task.vehicleOrigin[1];
            point = {car.x + cosine * x - sine * y, car.y + sine * x + cosine * y,
                car.z + point[2] - task.vehicleOrigin[2]};
        };
        moveAnchor(task.door);
        moveAnchor(task.seatPosition);
        moveAnchor(task.start);
        task.vehicleOrigin = {car.x, car.y, car.z};
        task.vehicleHeading = car.heading;
    }
    StorylandVehiclePhaseTiming timing;
    timing.seconds = task.phaseSeconds;
    const auto sample = storylandSampleVehicleTransition(StorylandVehicleAction::Exit, timing,
        float(world.gameMilliseconds - task.startedAt) * 0.001f);
    auto position = task.door;
    if (sample.phase == Phase::GetOut)
        for (size_t axis = 0; axis < position.size(); ++axis)
            position[axis] = task.seatPosition[axis] +
                (task.door[axis] - task.seatPosition[axis]) * sample.fraction;
    character.x = position[0];
    character.y = position[1];
    character.z = position[2];
    character.heading = car.heading;
    if (world.updateVehicleTaskDoor && !saddle)
        world.updateVehicleTaskDoor(vehicleHandle, task.seat, unsigned(sample.phase), sample.elapsed);
    if (sample.phase == Phase::CloseOutside || sample.phase == Phase::Done) {
        if (car.driver == handle) {
            if((nativeType==6||nativeType==8)&&
                std::fabs(car.velocity[0])<5.0f&&std::fabs(car.velocity[1])<5.0f) {
                car.bikeStandingOverride=true;
                car.bikeStandingChangedByScript=true;
            }
            car.driver = -1;
        }
        character.vehicle = -1;
        character.passengerSeat = -1;
        character.attached = false;
        character.attachment = {};
    }
    if (sample.phase != Phase::Done) return false;
    world.characterVehicleTasks.erase(active);
    return true;
}

void storylandAdvanceScriptCharacterObjective(StorylandMissionEnvironment& world, int32_t handle,
    StorylandMissionEnvironment::Character& character) {
    const int objective = character.objective;
    const auto vehicleTask = world.characterVehicleTasks.find(handle);
    if (vehicleTask != world.characterVehicleTasks.end()) {
        const auto& task = vehicleTask->second;
        const bool exiting = task.exiting && (objective == 0x00F2 || objective == 0x00EC ||
            objective == 0x00E8 || objective == 0x00F1) &&
            (task.vehicle == character.vehicle || (task.animating && character.vehicle < 0));
        const bool entering = !task.exiting && (objective == 0x00E9 || objective == 0x00EA || objective == 0x00F0) &&
            (task.seat < 0) == (objective != 0x00E9) && task.vehicle == character.objectiveTarget;
        if (!entering && !exiting) storylandCancelVcsVehicleEntry(world, handle, character);
    }
    if (objective != 0x00F2) {
        character.fleeVehicle = -1;
        character.fleeVehicleDeadline = 0;
    }
    if (objective != 0) {
        character.followingScriptPath = false;
    }
    if (character.followingScriptPath) {
        const bool arrived = std::hypot(character.targetX - character.x,
            character.targetY - character.y) <= character.objectiveRadius;
        character.walkingToTarget = !arrived && !character.stayInSamePlace;
        if (arrived) {
            character.followingScriptPath = false;
            character.seekRoute.clear();
            character.seekRouteCursor = 0;
            character.seekNode = UINT32_MAX;
        }
        return;
    }
    const auto complete = [&]() {
        character.walkingToTarget = false;
        character.objective = 0;
        character.objectiveCompleted = true;
        character.attackStartedAt = 0;
        character.fleeVehicle = -1;
        character.fleeVehicleDeadline = 0;
        world.characterTargets.erase(handle);
    };
    const bool queuedVehicleAttack = objective == 0x00EC && world.vcsCharacterExitingVehicle(handle);
    if (queuedVehicleAttack && character.leaderVehicleAfter &&
        character.leaderVehicleAfter >= world.gameMilliseconds) return;
    if ((objective == 0x00F2 || queuedVehicleAttack) &&
        (character.vehicle >= 0 || world.characterVehicleTasks.count(handle))) {
        if (!storylandAdvanceVcsVehicleExit(world, handle, character)) return;
        character.leaderVehicleAfter = 0;
    }
    if (objective == 0x0359) return;
    if (objective == 1 || objective == 2 || objective == 0x00DF || objective == 0x00E3) {
        character.walkingToTarget = false;
        return;
    }
    if (objective == 0x00E8 || objective == 0x00F1) {
        if (character.leaderVehicleAfter && character.leaderVehicleAfter >= world.gameMilliseconds) return;
        const auto active = world.characterVehicleTasks.find(handle);
        const int32_t vehicleHandle = active == world.characterVehicleTasks.end() ?
            character.vehicle : active->second.vehicle;
        const int nativeType = world.vehicleNativeType ? world.vehicleNativeType(vehicleHandle) : -1;
        if (world.prepareCharacterVehicleTask && (active != world.characterVehicleTasks.end() ||
            nativeType == 0 || nativeType == 6 || nativeType == 9)) {
            if (!storylandAdvanceVcsVehicleExit(world, handle, character)) return;
            character.leaderVehicleAfter = 0;
            complete();
            if (character.behavior.leavingVehicleToDie) {
                character.behavior.leavingVehicleToDie = false;
                character.health = 0; character.alive = false;
            }
            return;
        }
        const auto vehicle = world.missionVehicles.find(character.vehicle);
        if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
            character.vehicle = -1;
            character.passengerSeat = -1;
            character.leaderVehicleAfter = 0;
            complete();
            return;
        }
        std::array<float,3> exit{};
        if (!world.vehicleExitPosition ||
            !world.vehicleExitPosition(character.vehicle, character.passengerSeat, exit)) {
            return;
        }
        if (vehicle->second.driver == handle) {
            vehicle->second.driver = -1;
        }
        character.x = exit[0];
        character.y = exit[1];
        character.z = exit[2];
        character.vehicle = -1;
        character.passengerSeat = -1;
        character.leaderVehicleAfter = 0;
        complete();
        if (character.behavior.leavingVehicleToDie) {
            character.behavior.leavingVehicleToDie = false;
            character.health = 0; character.alive = false;
        }
        return;
    }
    if (objective == StorylandVcsLeaderFollowObjective) {
        float x = 0, y = 0, z = 0, heading = 0;
        if (character.leader < 0 || character.objectiveTarget != character.leader ||
            !vcsCharacterPosition(world, character.leader, x, y, z, heading)) {
            complete();
            return;
        }
        const auto leader = world.missionCharacters.find(character.leader);
        const bool playerLeader = character.leader == 0;
        const int32_t vehicleHandle = playerLeader ?
            (world.inVehicle ? world.occupiedMissionVehicle : -1) : leader->second.vehicle;
        const auto vehicle = world.missionVehicles.find(vehicleHandle);
        const bool leaderInVehicle = vehicle != world.missionVehicles.end() &&
            !vehicle->second.deleted && vehicle->second.alive;
        if (leaderInVehicle) {
            x = vehicle->second.x;
            y = vehicle->second.y;
            z = vehicle->second.z;
        }
        const float distance = std::hypot(x - character.x, y - character.y, z - character.z);
        character.targetX = x;
        character.targetY = y;
        character.objectiveHeight = z;
        character.targetRadius = leaderInVehicle ? 3.0f : 2.0f;
        character.walkingToTarget = distance > character.targetRadius && !character.stayInSamePlace;
        if (!character.walkingToTarget) return;
        const int leaderMovement = playerLeader ? world.playerCrouch.movementState :
            !leader->second.walkingToTarget ? 1 : leader->second.walkSpeed > 4.0f ? 5 :
                leader->second.walkSpeed > 2.0f ? 4 : 2;
        int movement = 4;
        if (distance < 5.0f) {
            if (leaderMovement < 2) movement = distance > 3.0f ? 4 : 2;
            else if (playerLeader) movement = distance < 3.0f || leaderMovement == 2 ? 2 : 4;
            else movement = leaderMovement;
        } else if (leaderMovement == 5) movement = 5;
        character.walkSpeed = movement == 5 ? 5.5f : movement == 4 ? 3.8f : 1.25f;
        return;
    }
    if (objective == 0x00F0 && character.vehicle >= 0 && !world.characterVehicleTasks.count(handle)) {
        complete();
        return;
    }
    if (objective == 0x00F0 && character.objectiveTarget < 0) {
        const float radius = 50.0f * float(world.enterCarRangeMultiplier);
        float nearest = radius;
        int32_t selected = -1;
        for (const auto& entry : world.missionVehicles) {
            const auto& car = entry.second;
            if (!car.alive || car.deleted || car.persistent || std::fabs(car.speed) > 0.1f ||
                (car.doorLock != 0 && car.doorLock != 1)) continue;
            const float distance = std::hypot(car.x - character.x, car.y - character.y, car.z - character.z);
            if (distance < nearest) { nearest = distance; selected = entry.first; }
        }
        if (world.findVehicleToSteal && radius > 0) {
            const int32_t ambient = world.findVehicleToSteal({character.x, character.y, character.z}, nearest);
            if (ambient >= 0) selected = ambient;
        }
        if (selected < 0) {
            character.objective = 0;
            character.walkingToTarget = false;
            return;
        }
        character.objectiveTarget = selected;
        character.walkSpeed = 3.8f;
    }
    if (objective == 0x00E9 || objective == 0x00EA || objective == 0x00F0) {
        // Native objectives17/18 select a door before seeking the car;
        // reaching the collision-body center is not vehicle entry.
        const int nativeType = world.vehicleNativeType ? world.vehicleNativeType(character.objectiveTarget) : -1;
        if (world.prepareCharacterVehicleTask &&
            (world.characterVehicleTasks.count(handle) || nativeType == 0 || nativeType == 4 || nativeType == 6 ||
                (objective != 0x00E9 && nativeType == 9))) {
            storylandAdvanceVcsVehicleEntry(world, handle, character);
            return;
        }
        const auto vehicle = world.missionVehicles.find(character.objectiveTarget);
        if (vehicle == world.missionVehicles.end() || vehicle->second.deleted || !vehicle->second.alive) {
            character.objective = 0;
            character.walkingToTarget = false;
            return;
        }
        auto& car = vehicle->second;
        if (objective == 0x00F0 && std::hypot(car.x - character.x, car.y - character.y,
            car.z - character.z) > 50.0f * float(world.enterCarRangeMultiplier)) {
            character.objective = 0;
            character.walkingToTarget = false;
            return;
        }
        character.targetX = car.x;
        character.targetY = car.y;
        character.walkingToTarget = !character.stayInSamePlace;
        if (std::hypot(character.x - car.x, character.y - car.y) < 1.3f &&
            std::fabs(car.speed) < 0.5f) {
            if (objective == 0x00F0 && car.driver < 0 && world.acquireVehicleDriver) {
                const int32_t driver = world.acquireVehicleDriver(character.objectiveTarget);
                if (driver >= 0) car.driver = driver;
            }
            if (objective == 0x00F0 && car.driver >= 0 && car.driver != handle) {
                if (car.driver == 0) {
                    if (world.playerCantBeDraggedOut || world.playerStayInCarWhenJacked) return;
                    world.playerExitVehicleRequested = true;
                    world.playerVehicleTransition = true;
                    return;
                }
                const auto driver = world.missionCharacters.find(car.driver);
                if (driver != world.missionCharacters.end()) {
                    if (driver->second.cannotBeDraggedOut || driver->second.stayInCarWhenJacked) return;
                    std::array<float,3> exit{};
                    if (!world.vehicleExitPosition ||
                        !world.vehicleExitPosition(character.objectiveTarget, -1, exit)) return;
                    auto& displaced = driver->second;
                    displaced.vehicle = -1; displaced.passengerSeat = -1;
                    displaced.x = exit[0]; displaced.y = exit[1]; displaced.z = exit[2];
                    displaced.objective = displaced.behavior.frightenedWhenJacked ? 0x00F2 : 0x00E4;
                    displaced.objectiveTarget = displaced.behavior.frightenedWhenJacked ? character.objectiveTarget : handle;
                    displaced.objectiveCompleted = false;
                }
                car.driver = -1;
                if (world.inVehicle && world.occupiedMissionVehicle == vehicle->first &&
                    world.playerPassengerSeat >= 0 && !world.playerStayInCarWhenJacked) {
                    world.playerExitVehicleRequested = true;
                    world.playerVehicleTransition = true;
                }
            }
            if (objective == 0x00EA || objective == 0x00F0) {
                if (car.driver >= 0 && car.driver != handle) {
                    return;
                }
                car.driver = handle;
                character.passengerSeat = -1;
            } else {
                const int capacity = world.vehiclePassengerCapacity ?
                    world.vehiclePassengerCapacity(character.objectiveTarget) : 0;
                int seat = character.allowFrontPassenger ? 0 : 1;
                const int permittedCapacity = character.behavior.onlyFrontPassenger ? std::min(capacity,1) : capacity;
                while (seat < permittedCapacity && storylandVcsPassenger(world, character.objectiveTarget, seat) >= 0) {
                    ++seat;
                }
                if (seat >= permittedCapacity) {
                    return;
                }
                character.passengerSeat = seat;
            }
            character.vehicle = character.objectiveTarget;
            character.attached = false;
            complete();
        }
        return;
    }
    if (objective == 0x00ED || objective == 0x00EE || objective == 0x00F5 || objective == 0x0158) {
        if (std::hypot(character.targetX - character.x, character.targetY - character.y) <=
            std::max(0.0f, character.objectiveRadius)) {
            complete();
        } else {
            character.walkingToTarget = !character.stayInSamePlace;
        }
        return;
    }
    if (objective == 0x00E1 || objective == 0x00E2) {
        character.walkingToTarget = !character.stayInSamePlace &&
            std::hypot(character.targetX - character.x, character.targetY - character.y) >
            std::max(0.0f, character.objectiveRadius);
        return;
    }
    if (objective == 0x011B) {
        const auto first = std::find_if(world.characterRoutePoints.begin(), world.characterRoutePoints.end(),
            [&](const auto& point) { return point.route == character.routeId; });
        if (first == world.characterRoutePoints.end()) {
            character.objective = 0;
            character.walkingToTarget = false;
            return;
        }
        auto end = first;
        while (end != world.characterRoutePoints.end() && end->route == character.routeId) {
            ++end;
        }
        const int16_t count = int16_t(std::distance(first, end));
        if (count < 2 || character.routeCursor < 0 || character.routeCursor >= count) {
            character.objective = 0;
            character.walkingToTarget = false;
            return;
        }
        const auto& target = (first + character.routeCursor)->position;
        character.targetX = target[0];
        character.targetY = target[1];
        character.objectiveHeight = target[2];
        if (std::hypot(target[0] - character.x, target[1] - character.y) < 0.5f) {
            int next = character.routeCursor + character.routeDirection;
            if (next < 0 || next >= count) {
                if (character.routeMode == 1) {
                    complete();
                    return;
                }
                if (character.routeMode == 2) {
                    character.routeDirection = int16_t(-character.routeDirection);
                    next = character.routeCursor + character.routeDirection;
                } else {
                    next = 0;
                }
            }
            character.routeCursor = int16_t(next);
            const auto& destination = (first + character.routeCursor)->position;
            character.targetX = destination[0];
            character.targetY = destination[1];
            character.objectiveHeight = destination[2];
        }
        character.walkingToTarget = !character.stayInSamePlace;
        return;
    }
    float x = 0;
    float y = 0;
    float z = 0;
    float heading = 0;
    const bool objectTarget = objective == 0x00EB;
    const bool vehicleTarget = objective == 0x00EC || objective == 0x00F2;
    if (objectTarget) {
        const auto object = std::find_if(world.objects.begin(), world.objects.end(),
            [&](const auto& item) { return item.handle == character.objectiveTarget; });
        if (object == world.objects.end() || object->damage.renderDamaged || !object->damage.visible) {
            complete();
            return;
        }
        x = object->x;
        y = object->y;
        z = object->z;
    } else if (vehicleTarget) {
        const int32_t target = objective == 0x00F2 && character.fleeVehicle >= 0 ?
            character.fleeVehicle : character.objectiveTarget;
        const auto vehicle = world.missionVehicles.find(target);
        if (vehicle == world.missionVehicles.end() || vehicle->second.deleted ||
            (objective != 0x00F2 && (!vehicle->second.alive || vehicle->second.health <= 0))) {
            complete();
            return;
        }
        x = vehicle->second.x;
        y = vehicle->second.y;
        z = vehicle->second.z;
    } else {
        if (objective != 0x00E0 && objective != 0x00E4 && objective != 0x00E5 &&
            objective != 0x00E6 && objective != 0x00E7 && objective != 0x00F3 &&
            objective != 0x00F4 && objective != 0x00EF) {
            return;
        }
        if (world.coopTargetPosition && character.objectiveTarget == 0 &&
            (objective == 0x00E4 || objective == 0x00E5)) {
            std::array<float,3> target{};
            if (world.coopTargetPosition(handle, target) < 0) { character.walkingToTarget = false; return; }
            x = target[0]; y = target[1]; z = target[2];
        } else if (!vcsCharacterPosition(world, character.objectiveTarget, x, y, z, heading)) {
            if (objective == 0x00E0 || objective == 0x00E4 || objective == 0x00E5) {
                complete();
            } else {
                character.walkingToTarget = false;
                character.objective = 0;
            }
            return;
        }
    }
    if (objective == 0x00EF) {
        // 40F810 samples the native RNG once and transforms the selected
        // formation offset by the target's frame. Pedestrians use the host
        // heading basis; arbitrary tilted native frames are not represented.
        const auto point = storylandVcsFormationTarget(character.formation,
            {x,y,z}, heading, world.nextNativeRandom());
        x = point[0]; y = point[1]; z = point[2];
        character.objectiveHeight = z;
    }
    float dx = x - character.x;
    float dy = y - character.y;
    const float distance = std::hypot(dx, dy);
    if (objective == 0x00E0 || objective == 0x00E6 || objective == 0x00F2) {
        if (objective == 0x00F2) {
            if (character.fleeVehicle < 0) {
                character.fleeVehicle = character.objectiveTarget;
                character.fleeVehicleDeadline = world.gameMilliseconds + 6000u;
            } else if (character.fleeVehicleDeadline != 0 &&
                character.fleeVehicleDeadline < world.gameMilliseconds) {
                // Native205E64 extends a nearby threat by5000ms; it ends a
                // completed6000ms flee only once the actual car is30m away.
                if (dx * dx + dy * dy < 900.0f)
                    character.fleeVehicleDeadline = world.gameMilliseconds + 5000u;
                else {
                    complete();
                    return;
                }
            }
        }
        if (objective == 0x00E0 && distance >= (character.pedStat >= 0 ? character.behavior.statFleeDistance : 60.0f)) {
            complete();
            return;
        }
        if (distance < 0.001f) {
            dx = 1;
            dy = 0;
        }
        const float scale = 10.0f / std::max(0.001f, std::hypot(dx, dy));
        character.targetX = character.x - dx * scale;
        character.targetY = character.y - dy * scale;
        character.walkSpeed = 3.8f;
        character.walkingToTarget = (objective == 0x00F2 || distance < 60.0f) && !character.stayInSamePlace;
        return;
    }
    if (objective == 0x00F4) {
        character.heading = std::atan2(-dx, dy) * 57.2957795131f;
        character.walkingToTarget = false;
        return;
    }
    const float stopping = objective == 0x00EF ? 2.5f :
        objective == 0x00E4 || objective == 0x00E5 || objectTarget || vehicleTarget ? 8.0f : 1.0f;
    character.walkingToTarget = distance > stopping && !character.stayInSamePlace;
    character.targetX = x;
    character.targetY = y;
    character.walkSpeed = objective == 0x00F3 || objective == 0x00EF ? 1.25f : 3.8f;
    if ((objective == 0x00E7 || objective == 0x00F3) && distance <= stopping) {
        complete();
    }
}

bool storylandAdvanceCollectivePolicies(StorylandMissionEnvironment& world, std::string& error) {
    if ((world.nativeFrameCounter & 7) && !world.forceCollectiveUpdate) {
        return true;
    }
    world.forceCollectiveUpdate = false;
    const size_t groupIndex = (world.nativeFrameCounter >> 4) & 3;
    const auto& policy = world.collectivePolicies[groupIndex];
    if (!policy.mode || policy.manualObjective || policy.targetGroup < 0 ||
        size_t(policy.targetGroup) >= world.characterCollectives.size()) {
        return true;
    }
    const auto liveMembers = [&](const std::vector<int32_t>& group) {
        std::vector<int32_t> result;
        for (int32_t handle : group) {
            if (handle == 0) {
                if (world.health > 0) result.push_back(handle);
                continue;
            }
            const auto found = world.missionCharacters.find(handle);
            if (found != world.missionCharacters.end() && found->second.alive && found->second.health > 0) {
                result.push_back(handle);
            }
        }
        return result;
    };
    const auto sources = liveMembers(world.characterCollectives[groupIndex]);
    const auto targets = liveMembers(world.characterCollectives[size_t(policy.targetGroup)]);
    std::map<int32_t,std::array<float,3>> positions;
    if (!sources.empty() && !targets.empty()) {
        for (const auto* members : {&sources, &targets}) {
            for (int32_t handle : *members) {
                if (positions.count(handle)) continue;
                std::array<float,16> frame{};
                if (!world.characterWorldMatrix || !world.characterWorldMatrix(handle, frame) ||
                    !std::isfinite(frame[12]) || !std::isfinite(frame[13]) || !std::isfinite(frame[14])) {
                    error = "Collective targeting requires each live member's native entity frame.";
                    return false;
                }
                positions[handle] = {frame[12], frame[13], frame[14]};
            }
        }
    }
    std::vector<std::vector<size_t>> candidates(targets.size());
    const float rangeSquared = policy.range * policy.range;
    const auto distanceSquared = [&](int32_t source, int32_t target) {
        const auto& a = positions.at(source);
        const auto& b = positions.at(target);
        const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
        return (dx * dx + dy * dy) + dz * dz;
    };
    // Native 11C360 retains at most four nearby sources per target. It
    // assigns scarce targets first, then balances the chosen source counts.
    for (size_t source = 0; source < sources.size(); ++source) {
        for (size_t target = 0; target < targets.size(); ++target) {
            if (sources[source] != targets[target] && candidates[target].size() < 4 &&
                distanceSquared(sources[source], targets[target]) < rangeSquared) {
                candidates[target].push_back(source);
            }
        }
    }
    std::vector<unsigned> assignedCount(sources.size(), 0);
    std::vector<bool> assigned(sources.size(), false);
    if (size_t(policy.targetGroup) == groupIndex) {
        for (size_t index = 0; index < assignedCount.size(); ++index) {
            assignedCount[index] = unsigned(candidates[index].size());
        }
    }
    const auto assign = [&](size_t index, int32_t target) {
        const int32_t handle = sources[index];
        assigned[index] = true;
        // Native11C238 counts the assignment, then20C310 excludes player
        // type0 from AI tasks. He remains a candidate and a real target.
        if (handle == 0) return;
        auto& character = world.missionCharacters.at(handle);
        const int objective = policy.mode == 1 ? 0x00E4 : 0x00E6;
        if (character.objective != objective || character.objectiveTarget != target) {
            character.objective = objective;
            character.objectiveTarget = target;
            character.objectiveCompleted = false;
            character.followingScriptPath = false;
            character.walkingToTarget = false;
            character.seekRoute.clear();
            character.seekRouteCursor = 0;
            world.characterWanderTasks.erase(handle);
        }
    };
    for (size_t count = 1; count <= 4; ++count) {
        // Stable host membership order replaces the retail free-list node
        // order; target scarcity and four-candidate limits remain native.
        for (size_t reverse = targets.size(); reverse > 0; --reverse) {
            const size_t target = reverse - 1;
            if (candidates[target].size() != count) {
                continue;
            }
            size_t selected = candidates[target].front();
            for (size_t candidate : candidates[target]) {
                if (assignedCount[candidate] < assignedCount[selected]) {
                    selected = candidate;
                }
            }
            ++assignedCount[selected];
            assign(selected, targets[target]);
        }
    }
    for (size_t source = 0; source < sources.size(); ++source) {
        if (assigned[source]) {
            continue;
        }
        int32_t nearest = -1;
        float distance = 9.0e21f; // Retail initial squared-distance bound.
        for (int32_t target : targets) {
            if (sources[source] != target) {
                const float candidate = distanceSquared(sources[source], target);
                if (candidate < distance) {
                    distance = candidate;
                    nearest = target;
                }
            }
        }
        if (nearest >= 0) {
            // Native fallback deliberately has no range cutoff.
            assign(source, nearest);
        } else {
            if (sources[source] == 0) {
                storylandClearVcsCollectivePlayerObjective(world);
                continue;
            }
            auto& character = world.missionCharacters.at(sources[source]);
            character.objective = 0;
            character.objectiveTarget = -1;
            character.walkingToTarget = false;
            character.followingScriptPath = false;
            character.seekRoute.clear();
            world.characterWanderTasks.erase(sources[source]);
            world.characterTargets.erase(sources[source]);
        }
    }
    return true;
}

void storylandAdvanceCharacterAnimations(StorylandMissionEnvironment& world) {
    for (auto entry = world.scriptedCharacterAnimations.begin(); entry != world.scriptedCharacterAnimations.end();) {
        const auto character = world.missionCharacters.find(entry->first);
        auto& animation = entry->second;
        if (entry->first != 0 && (character == world.missionCharacters.end() ||
            character->second.animationId != animation.animation || !character->second.alive)) {
            entry = world.scriptedCharacterAnimations.erase(entry); continue;
        }
        const float seconds = float(world.gameMilliseconds - animation.clock) * .001f;
        if (!animation.paused) animation.time += seconds;
        animation.weight = std::clamp(animation.weight + seconds * animation.blendRate, 0.0f, 1.0f);
        animation.clock = world.gameMilliseconds;
        const float duration = world.animationDuration ? world.animationDuration(animation.group, animation.animation) : 0;
        if (std::isfinite(duration) && duration > 0)
            animation.time = animation.loop ? std::fmod(animation.time, duration) : std::min(animation.time, duration);
        if (animation.weight == 0 && animation.blendRate < 0) {
            if (entry->first == 0) world.pedAnimationRequests.erase(0);
            else character->second.animationId = -1;
            entry = world.scriptedCharacterAnimations.erase(entry); continue;
        }
        if (character != world.missionCharacters.end()) character->second.animationBlend = animation.weight;
        ++entry;
    }
}

bool storylandAdvanceCharacterBehavior(StorylandMissionEnvironment& world, int32_t handle,
    StorylandMissionEnvironment::Character& character, float seconds) {
    auto& behavior = character.behavior;
    if (behavior.forcedThreat >= 0) {
        const auto target = world.missionCharacters.find(behavior.forcedThreat);
        const auto car = world.missionVehicles.find(behavior.forcedThreat);
        if ((behavior.forcedThreatIsVehicle && (car == world.missionVehicles.end() || car->second.deleted)) ||
            (!behavior.forcedThreatIsVehicle && behavior.forcedThreat != 0 &&
                (target == world.missionCharacters.end() || !target->second.alive))) behavior.forcedThreat = -1;
    }
    if (behavior.conversationPartner >= 0) {
        const auto partner = world.missionCharacters.find(behavior.conversationPartner);
        if (partner == world.missionCharacters.end() || !partner->second.alive ||
            partner->second.behavior.conversationPartner != handle ||
            int32_t(world.gameMilliseconds - behavior.conversationDeadline) >= 0) {
            behavior.conversationPartner = -1; character.lookAtCharacter = -1;
            if (character.animationId == 14) character.animationId = -1;
        } else {
            character.heading = std::atan2(-(partner->second.x - character.x), partner->second.y - character.y) * 57.2957795131f;
            character.walkingToTarget = false;
            if (world.characterSpeech && int32_t(world.gameMilliseconds - behavior.nextConversationSpeech) >= 0) {
                world.characterSpeech(handle, 0x9C); behavior.nextConversationSpeech = world.gameMilliseconds + 3000;
            }
            return true;
        }
    }
    if (behavior.diveDeadline) {
        if (int32_t(world.gameMilliseconds - behavior.diveDeadline) >= 0) {
            behavior.diveDeadline = 0;
            if (character.animationId == 146 || character.animationId == 156) character.animationId = -1;
        } else {
            const std::array<float,3> next{character.x + behavior.diveVelocity[0] * seconds,
                character.y + behavior.diveVelocity[1] * seconds, character.z};
            if (!world.pedestrianLineClear || world.pedestrianLineClear(
                {character.x, character.y, character.z + .4f}, {next[0], next[1], next[2] + .4f})) {
                character.x = next[0]; character.y = next[1];
            }
            return true;
        }
    }
    if (character.vehicle >= 0 && !character.attached && !behavior.leavingVehicleToDie && character.health <= 0) {
        const auto car = world.missionVehicles.find(character.vehicle);
        if (car != world.missionVehicles.end() && !car->second.deleted) {
            behavior.leavingVehicleToDie = true;
            behavior.dyingExitStartedAt = world.gameMilliseconds;
            character.objective = 0x00F1; character.objectiveTarget = character.vehicle;
        }
    }
    if (behavior.leavingVehicleToDie) {
        character.objective = 0x00F1; character.objectiveTarget = character.vehicle;
        const int animation = character.passengerSeat >= 0 && character.passengerSeat % 2 == 0 ? 122 : 82;
        const float duration = world.animationDuration ? world.animationDuration(0, animation) : 0;
        if (std::isfinite(duration) && duration > 0 && duration < 60 &&
            float(world.gameMilliseconds - behavior.dyingExitStartedAt) * .001f < duration) return true;
        return false;
    }
    if (character.objective == 0x0219) {
        if (character.vehicle >= 0 || character.attached) {
            character.objective = 0; character.objectiveCompleted = true; return false;
        }
        int32_t selected = -1; float nearest = 50;
        const int taxi = world.modelIdByName ? world.modelIdByName("taxi") : -1;
        const int cabbie = world.modelIdByName ? world.modelIdByName("cabbie") : -1;
        for (const auto& entry : world.missionVehicles) {
            const auto& car = entry.second;
            if (!car.alive || car.deleted || (car.model != taxi && car.model != cabbie) ||
                (car.doorLock != 0 && car.doorLock != 1) || !world.vehiclePassengerCapacity) continue;
            const int capacity = world.vehiclePassengerCapacity(entry.first);
            bool vacant = false;
            for (int seat = 0; seat < capacity; ++seat) if (storylandVcsPassenger(world, entry.first, seat) < 0) vacant = true;
            const float distance = std::hypot(car.x - character.x, car.y - character.y, car.z - character.z);
            if (vacant && distance < nearest) { selected = entry.first; nearest = distance; }
        }
        if (selected < 0 && world.findAmbientVehicleNoSave && world.vehiclePassengerCapacity) {
            const std::array<float,4> bounds{character.x - 50, character.y - 50,
                character.x + 50, character.y + 50};
            for (int model : {taxi, cabbie}) {
                if (model < 0) continue;
                const int32_t handle = world.findAmbientVehicleNoSave(bounds, model, true);
                const auto found = world.missionVehicles.find(handle);
                if (found == world.missionVehicles.end() || found->second.deleted) continue;
                const auto& car = found->second;
                if (car.doorLock != 0 && car.doorLock != 1) continue;
                const int capacity = world.vehiclePassengerCapacity(handle);
                bool vacant = false;
                for (int seat = 0; seat < capacity; ++seat)
                    if (storylandVcsPassenger(world, handle, seat) < 0) vacant = true;
                const float distance = std::hypot(car.x - character.x, car.y - character.y, car.z - character.z);
                if (vacant && distance < nearest) { selected = handle; nearest = distance; }
            }
        }
        if (selected < 0) { character.walkingToTarget = false; return true; }
        auto& taxiCar = world.missionVehicles.at(selected);
        taxiCar.drivingToTarget = true; taxiCar.targetX = character.x; taxiCar.targetY = character.y;
        taxiCar.targetZ = character.z; taxiCar.cruiseSpeed = 3;
        if (nearest < 4) {
            taxiCar.drivingToTarget = false; taxiCar.speed = 0;
            character.objective = 0x00E9; character.objectiveTarget = selected; character.walkSpeed = 1.25f;
        } else return true;
    }
    if (behavior.iceCreamVehicle >= 0) {
        const auto car = world.missionVehicles.find(behavior.iceCreamVehicle);
        std::array<float,3> position{};
        const bool expired = int32_t(world.gameMilliseconds - behavior.iceCreamDeadline) >= 0;
        if (car == world.missionVehicles.end() || car->second.deleted || !car->second.alive || expired ||
            character.objective != 0x0359 || !storylandIceCreamPosition(world, behavior.iceCreamVehicle, behavior.iceCreamSlot, position)) {
            behavior.attemptedAttractor = true; behavior.iceCreamVehicle = -1; behavior.iceCreamSlot = -1;
            behavior.iceCreamStartedAt = 0;
            if (character.objective == 0x0359) { character.objective = 0; character.walkingToTarget = false; }
            return false;
        }
        character.targetX = position[0]; character.targetY = position[1];
        const float distance = std::hypot(character.x - position[0], character.y - position[1]);
        character.walkingToTarget = distance > .2f;
        if (character.walkingToTarget) { behavior.iceCreamStartedAt = 0; return false; }
        if (behavior.iceCreamStartedAt == 0) {
            behavior.iceCreamStartedAt = world.gameMilliseconds ? world.gameMilliseconds : 1;
            character.heading = car->second.heading + (behavior.iceCreamSlot & 1 ? 90 : -90);
        }
        if (int32_t(world.gameMilliseconds - behavior.iceCreamStartedAt) >= 3000) {
            behavior.attemptedAttractor = true; behavior.boughtIceCream = true;
            behavior.iceCreamVehicle = -1; behavior.iceCreamSlot = -1; behavior.iceCreamStartedAt = 0;
            character.objective = 0; character.objectiveCompleted = true;
        }
        return true;
    }
    return false;
}

void storylandAdvanceLcsMissionEntities(StorylandMissionEnvironment& world,uint32_t elapsedMilliseconds) {
    const float seconds=float(std::min(elapsedMilliseconds,100u))/1000.0f;
    for(auto task=world.characterWanderTasks.begin();task!=world.characterWanderTasks.end();) {
        if(world.missionCharacters.find(task->first)==world.missionCharacters.end())
            task=world.characterWanderTasks.erase(task);
        else ++task;
    }
    for(auto& item:world.missionVehicles) {
        auto& vehicle=item.second;
        if(!vehicle.alive)continue;
        const bool carried=std::any_of(world.lcsCranes.cranes.begin(),world.lcsCranes.cranes.end(),
            [&](const auto& crane) {
                return crane.vehicle==item.first&&(crane.state==StorylandLcsCrane::Lifting||
                    crane.state==StorylandLcsCrane::Raising||crane.state==StorylandLcsCrane::Aligning);
            });
        if(vehicle.positionFrozen||carried) {vehicle.speed=0;continue;}
        const int nativeType=world.vehicleNativeType?world.vehicleNativeType(item.first):-1;
        const bool roadRoute=world.pedestrianPaths && vehicle.spawnedSlot>=0 &&
            (nativeType==0||nativeType==5||nativeType==6||nativeType==8);
        if(vehicle.drivingToTarget&&!roadRoute) {
            const float dx=vehicle.targetX-vehicle.x,dy=vehicle.targetY-vehicle.y,dz=vehicle.targetZ-vehicle.z;
            const float distance=std::sqrt(dx*dx+dy*dy+dz*dz);
            if(distance>.05f) {
                const float wanted=std::max(0.0f,vehicle.cruiseSpeed);
                vehicle.speed+=std::clamp(wanted-vehicle.speed,-4.0f*seconds,2.5f*seconds);
                const float fraction=std::min(1.0f,std::max(0.0f,vehicle.speed)*seconds/distance);
                vehicle.x+=dx*fraction;vehicle.y+=dy*fraction;vehicle.z+=dz*fraction;
                vehicle.heading=std::atan2(-dx,dy)*57.2957795131f;
                vehicle.movedByScript=true;
            } else {vehicle.drivingToTarget=false;vehicle.speed=0;}
        }
    }
    const auto attach=[&](int32_t vehicleHandle,const std::array<float,3>& offset,
        float& x,float& y,float& z,float& heading) {
        const auto vehicle=world.missionVehicles.find(vehicleHandle);
        if(vehicle==world.missionVehicles.end()||!vehicle->second.alive)return;
        const auto& target=vehicle->second;
        const float angle=target.heading*0.0174532925199433f,c=std::cos(angle),s=std::sin(angle);
        x=target.x+c*offset[0]-s*offset[1];y=target.y+s*offset[0]+c*offset[1];z=target.z+offset[2];heading=target.heading;
    };
    if(world.playerAttachedVehicle>=0&&!world.characterAttachments.count(1))
        attach(world.playerAttachedVehicle,world.playerAttachment,world.x,world.y,world.z,world.heading);
    for(auto& item:world.missionCharacters) {
        auto& character=item.second;
        if(character.networkControlled)continue;
        character.movementSpeed=0;
        if(world.characterReactionOwned&&world.characterReactionOwned(item.first))continue;
        const auto wander=world.characterWanderTasks.find(item.first);
        if(wander!=world.characterWanderTasks.end()&&character.alive&&character.health>0&&!character.positionFrozen&&
            !character.stayInSamePlace&&
            !world.lcsWaitAnimations.count(item.first)&&
            !character.walkingToTarget)
            storylandChooseScriptWanderTarget(world,character,wander->second);
        if(character.walkingToTarget&&character.alive&&character.health>0&&!character.positionFrozen&&
            !character.stayInSamePlace&&!world.lcsWaitAnimations.count(item.first)&&character.vehicle<0&&!character.attached) {
            const float dx=character.targetX-character.x,dy=character.targetY-character.y;
            const float distance=std::hypot(dx,dy);
            if(distance>.05f) {
                character.heading=turnLcsCharacterHeading(character.heading,dx,dy,
                    character.pedStats.headingChangeRate,elapsedMilliseconds);
                const float heading=character.heading*0.0174532925199433f;
                const float step=std::min(std::max(0.0f,character.walkSpeed)*seconds,distance);
                const float nextX=character.x-std::sin(heading)*step;
                const float nextY=character.y+std::cos(heading)*step;
                if(wander!=world.characterWanderTasks.end()&&world.pedestrianLineClear&&
                    !world.pedestrianLineClear({character.x,character.y,character.z+1.0f},
                        {nextX,nextY,character.z+1.0f})) {
                    character.walkingToTarget=false;
                    wander->second.direction=(wander->second.direction+1)%8;
                    continue;
                }
                const float previousX=character.x,previousY=character.y;
                if(world.moveMissionCharacter) {
                    std::array<float,3> position{character.x,character.y,character.z};
                    if(world.moveMissionCharacter(item.first,{nextX,nextY,character.z},position)&&
                        std::all_of(position.begin(),position.end(),[](float value){return std::isfinite(value);})) {
                        character.x=position[0];character.y=position[1];character.z=position[2];
                    }
                } else {character.x=nextX;character.y=nextY;}
                if(wander!=world.characterWanderTasks.end()&&world.groundHeightAt) {
                    float ground=0;
                    if(world.groundHeightAt({character.x,character.y,character.z+1.5f},ground)&&
                        std::fabs(ground-character.z)<2.0f)character.z=ground;
                }
                const float travelled=std::hypot(character.x-previousX,character.y-previousY);
                character.movementSpeed=seconds>0?travelled/seconds:0;
                character.walkedDistance+=double(travelled);
            } else character.walkingToTarget=false;
        }
        const auto vehicleTask=world.characterVehicleTasks.find(item.first);
        if(character.vehicle>=0&&!world.characterAttachments.count(item.first)&&
            (vehicleTask==world.characterVehicleTasks.end()||!vehicleTask->second.animating))
            attach(character.vehicle,character.attachment,character.x,character.y,character.z,character.heading);
    }
}

void storylandAdvanceVcsMissionEntities(StorylandMissionEnvironment& world,uint32_t elapsedMilliseconds,bool vcs=true) {
    world.locateRegions.erase(std::remove_if(world.locateRegions.begin(),world.locateRegions.end(),
        [&](const auto& region){return int32_t(world.gameMilliseconds-region.expiresAt)>=0;}),world.locateRegions.end());
    if(!vcs) {storylandAdvanceLcsMissionEntities(world,elapsedMilliseconds);return;}
    storylandAdvanceCharacterAnimations(world);
    std::vector<int32_t> removedCharacters;
    for (const auto& entry : world.missionCharacters)
        if (!entry.second.networkControlled && entry.second.removeFromWorld) removedCharacters.push_back(entry.first);
    for (int32_t handle : removedCharacters) storylandDeleteMissionCharacter(world, handle);
    const float seconds=float(std::min(elapsedMilliseconds,100u))/1000.0f;
    for (auto& check : world.stuckCarChecks) {
        if (check.vehicle < 0 || uint32_t(check.sampledAt + check.interval) >= world.gameMilliseconds) {
            continue;
        }
        const auto vehicle = world.missionVehicles.find(check.vehicle);
        if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
            check = {};
            continue;
        }
        const std::array<float,3> position{vehicle->second.x, vehicle->second.y, vehicle->second.z};
        const float moved = std::hypot(position[0] - check.position[0], position[1] - check.position[1],
            position[2] - check.position[2]);
        check.stuck = moved < check.distance;
        check.position = position;
        check.sampledAt = world.gameMilliseconds;
    }
    for(auto task=world.characterWanderTasks.begin();task!=world.characterWanderTasks.end();) {
        if(world.missionCharacters.find(task->first)==world.missionCharacters.end())
            task=world.characterWanderTasks.erase(task);
        else ++task;
    }
    for(auto& item:world.missionVehicles) {
        auto& vehicle=item.second;
        if(vehicle.fadeOut)vehicle.renderAlpha=uint8_t(std::max(0,int(vehicle.renderAlpha)-16));
        if(!vehicle.alive||vehicle.positionFrozen||vehicle.boatAnchored)continue;
        const int nativeType=world.vehicleNativeType?world.vehicleNativeType(item.first):-1;
        const bool roadVehicle=vehicle.spawnedSlot>=0&&
            (nativeType==0||nativeType==5||nativeType==6||nativeType==8||nativeType==9);
        if(vehicle.drivingToTarget&&!(roadVehicle&&world.pedestrianPaths)) {
            const float dx=vehicle.targetX-vehicle.x,dy=vehicle.targetY-vehicle.y,dz=vehicle.targetZ-vehicle.z;
            const float distance=roadVehicle?std::hypot(dx,dy):std::sqrt(dx*dx+dy*dy+dz*dz);
            if(distance>.05f) {
                const float wanted=std::max(0.0f,vehicle.cruiseSpeed);
                vehicle.speed+=std::clamp(wanted-vehicle.speed,-4.0f*seconds,2.5f*seconds);
                const float fraction=std::min(1.0f,std::max(0.0f,vehicle.speed)*seconds/distance);
                vehicle.x+=dx*fraction;vehicle.y+=dy*fraction;
                if(!roadVehicle)vehicle.z+=dz*fraction;
                vehicle.heading=std::atan2(-dx,dy)*57.2957795131f;
                vehicle.movedByScript=true;
            } else {vehicle.drivingToTarget=false;vehicle.speed=0;}
        }
    }
    const auto attach=[&](int32_t vehicleHandle,const std::array<float,3>& offset,
        float& x,float& y,float& z,float& heading) {
        const auto vehicle=world.missionVehicles.find(vehicleHandle);
        if(vehicle==world.missionVehicles.end()||!vehicle->second.alive)return;
        const auto& target=vehicle->second;
        const float angle=target.heading*0.0174532925199433f,c=std::cos(angle),s=std::sin(angle);
        x=target.x+c*offset[0]-s*offset[1];y=target.y+s*offset[0]+c*offset[1];z=target.z+offset[2];heading=target.heading;
    };
    if(world.activeVcsCharacterAttachment(0)) {
        if(world.vcsCharacterAttachments.count(0))storylandPositionVcsAttachedCharacter(world,0);
        else attach(world.playerAttachedVehicle,world.playerAttachment,world.x,world.y,world.z,world.heading);
    }
    const int playerWeaponAnimation=world.characterCrouchWeaponAnimation?
        world.characterCrouchWeaponAnimation(0):155;
    storylandAdvanceCrouch(world.playerCrouch,world.gameMilliseconds,world.health>0,playerWeaponAnimation);
    if(world.playerRenderAlpha<255)world.playerRenderAlpha=uint8_t(std::min(255,int(world.playerRenderAlpha)+16));
    const auto finishWait=[&](int32_t handle,int32_t& state,uint32_t& deadline,
        bool& versionB,StorylandMissionEnvironment::Character* character) {
        if(!state||int32_t(world.gameMilliseconds-deadline)<=0)return;
        if(state==24&&world.animationDuration) {
            const float duration=world.animationDuration(0,163);
            if(std::isfinite(duration)&&duration>0) {
                const uint32_t milliseconds=uint32_t(std::clamp(std::ceil(double(duration)*1000.0),1.0,100000.0));
                state=23;
                deadline=world.gameMilliseconds+milliseconds;
                versionB=false;
                world.pedWaitStateMilliseconds[handle]=int32_t(milliseconds);
                if(character) {
                    character->animationGroup=0;character->animationId=163;
                    character->animationStartedAt=world.gameMilliseconds;
                    character->animationBlend=4;character->animationLoop=false;
                } else {
                    world.pedAnimationRequests[handle]={163,int32_t(milliseconds),
                        world.nextPedAnimationSerial++,0};
                }
                return;
            }
        }
        state=0;deadline=0;versionB=false;
        if(character)character->animationId=-1;
        world.pedWaitStateMilliseconds.erase(handle);
        if(!character)world.pedAnimationRequests.erase(handle);
    };
    finishWait(0,world.playerWaitState,world.playerWaitDeadline,world.playerWaitVersionB,nullptr);
    for(auto& item:world.missionCharacters) {
        auto& character=item.second;
        if(character.networkControlled)continue;
        character.movementSpeed=0;
        if(character.renderAlpha<255&&!character.fadeOut)
            character.renderAlpha=uint8_t(std::min(255,int(character.renderAlpha)+16));
        if(character.fadeOut)character.renderAlpha=uint8_t(std::max(0,int(character.renderAlpha)-16));
        const int weaponAnimation=world.characterCrouchWeaponAnimation?
            world.characterCrouchWeaponAnimation(item.first):155;
        storylandAdvanceCrouch(character.crouch,world.gameMilliseconds,
            character.alive&&character.health>0,weaponAnimation);
        finishWait(item.first,character.waitState,character.waitDeadline,character.waitVersionB,&character);
        if (!character.alive || character.health <= 0) {
            const auto activeTask = world.characterVehicleTasks.find(item.first);
            const bool dyingExit = character.alive && character.health <= 0 &&
                character.behavior.leavingVehicleToDie && activeTask != world.characterVehicleTasks.end() &&
                activeTask->second.exiting;
            if (!dyingExit) {
                // Entry cancellation normally snaps the pedestrian to the door
                // or seat. A dead body must keep its last physical position.
                const std::array<float,3> position{character.x,character.y,character.z};
                storylandCancelVcsVehicleEntry(world, item.first, character);
                character.x=position[0];character.y=position[1];character.z=position[2];
            }
            character.fleeVehicle = -1;
            character.fleeVehicleDeadline = 0;
            character.walkingToTarget = false;
            // A destroyed vehicle can detach an already fatal occupant before
            // the normal exit-completion callback clears this flag. Preserve
            // a real in-progress exit, but never leave a detached corpse in
            // the living exit state indefinitely.
            const auto task = world.characterVehicleTasks.find(item.first);
            const bool exiting = task != world.characterVehicleTasks.end() && task->second.exiting;
            if (character.health <= 0 && character.vehicle < 0 && !exiting) {
                character.behavior.leavingVehicleToDie = false;
                character.alive = false;
            }
        }
        if(!character.alive||(character.health<=0&&character.vehicle<0&&!character.behavior.leavingVehicleToDie))continue;
        if(world.vcsPedFalls.count(item.first))continue;
        if(world.characterReactionOwned&&world.characterReactionOwned(item.first))continue;
        if(storylandAdvanceCharacterBehavior(world,item.first,character,seconds))continue;
        if(storylandVcsWaitBlocksMovement(character.waitState))continue;
        storylandAdvanceVcsLeaderPolicy(world,item.first,character);
        storylandAdvanceScriptCharacterObjective(world,item.first,character);
        const auto wander=world.characterWanderTasks.find(item.first);
        if(wander!=world.characterWanderTasks.end()&&!character.walkingToTarget&&!character.stayInSamePlace)
            storylandChooseScriptWanderTarget(world,character,wander->second);
        const int movement=character.walkingToTarget?(character.walkSpeed>4.0f?5:character.walkSpeed>2.0f?4:2):1;
        storylandObserveCrouchMovement(character.crouch,world.gameMilliseconds,movement,weaponAnimation);
        if(character.walkingToTarget&&!character.stayInSamePlace&&!character.positionFrozen&&
            character.vehicle<0&&!character.attached&&
            (character.collisionEnabled||character.disableVerticalMovement)) {
            float targetX=character.targetX,targetY=character.targetY;
            if(character.collisionEnabled&&
                (!storylandSeekScriptPedNode(world,item.first,character,targetX,targetY)||
                 !storylandSeekLocalPedAvoidance(world,character,targetX,targetY))) {
                storylandWaitForScriptPedObstruction(world,item.first,character);
                continue;
            }
            const float dx=targetX-character.x,dy=targetY-character.y;
            const float distance=std::hypot(dx,dy);
            if(distance>.05f) {
                float speed = std::max(0.0f,character.walkSpeed);
                if (character.objective == StorylandVcsLeaderFollowObjective && world.animationMovementSpeed) {
                    const int authoredGroup = character.pedStats.valid ?
                        character.pedStats.animationGroup : character.animationGroup;
                    const int group = storylandNativePedWeaponLocomotionGroup(true,character.currentWeapon,
                        world.vcsFollowerLocomotionGroup(character,authoredGroup));
                    const int motion = character.walkSpeed > 4.0f ? 2 : character.walkSpeed > 2.0f ? 1 : 0;
                    const float nativeSpeed = world.animationMovementSpeed(group,motion);
                    if (std::isfinite(nativeSpeed) && nativeSpeed > 0) speed = nativeSpeed;
                }
                const float fraction=std::min(1.0f,speed*seconds/distance);
                float nextX=character.x+dx*fraction,nextY=character.y+dy*fraction;
                character.behavior.areaBoundaryBlocked = false;
                if (character.behavior.areaEnabled && !storylandCharacterOutsideArea(character)) {
                    const auto& area = character.behavior.area;
                    character.behavior.areaBoundaryBlocked = nextX <= area[0] || nextX >= area[2] ||
                        nextY <= area[1] || nextY >= area[3];
                    nextX = std::clamp(nextX, area[0], area[2]);
                    nextY = std::clamp(nextY, area[1], area[3]);
                    if (character.behavior.adjustTargetsToArea && character.behavior.areaBoundaryBlocked) {
                        character.targetX = std::clamp(character.targetX, area[0], area[2]);
                        character.targetY = std::clamp(character.targetY, area[1], area[3]);
                    }
                }
                if(character.collisionEnabled&&world.pedestrianLineClear&&!world.pedestrianLineClear(
                    {character.x,character.y,character.z+1.0f},{nextX,nextY,character.z+1.0f})) {
                    if(wander!=world.characterWanderTasks.end()) {
                        character.walkingToTarget=false;
                        wander->second.direction=(wander->second.direction+1)%8;
                    } else {
                        storylandWaitForScriptPedObstruction(world,item.first,character);
                    }
                    continue;
                }
                // Measure only committed on-foot travel. Objective warps and
                // vehicle attachment updates must not advance walking clips.
                const float previousX=character.x,previousY=character.y;
                if(character.collisionEnabled&&world.moveMissionCharacter) {
                    std::array<float,3> resolved{};
                    if(!world.moveMissionCharacter(item.first,{nextX,nextY,character.z},resolved)||
                        !std::all_of(resolved.begin(),resolved.end(),[](float value){return std::isfinite(value);}))continue;
                    nextX=resolved[0];nextY=resolved[1];character.z=resolved[2];
                }
                character.x=nextX;character.y=nextY;
                if(character.collisionEnabled&&world.groundHeightAt) {
                    float ground=0;
                    if(world.groundHeightAt({character.x,character.y,character.z+1.5f},ground)&&
                        std::fabs(ground-character.z)<2.0f)character.z=ground;
                }
                const float travelled=std::hypot(character.x-previousX,character.y-previousY);
                character.movementSpeed=seconds>0?travelled/seconds:0;
                character.walkedDistance+=double(travelled);
                const float targetHeading=std::atan2(-dx,dy)*57.2957795131f;
                if(character.pedStat>=0) {
                    const float turn=std::remainder(targetHeading-character.heading,360.0f);
                    const float limit=character.behavior.statHeadingRate*seconds*57.2957795131f;
                    character.heading+=std::clamp(turn,-limit,limit);
                } else character.heading=targetHeading;
            } else if(std::hypot(character.targetX-character.x,character.targetY-character.y)<.1f) {
                character.walkingToTarget=false;
            }
        }
        const auto vehicleTask = world.characterVehicleTasks.find(item.first);
        const bool exiting = vehicleTask != world.characterVehicleTasks.end() &&
            vehicleTask->second.exiting && vehicleTask->second.animating;
        if(character.vehicle>=0&&!exiting&&!world.vcsCharacterAttachments.count(item.first)) {
            attach(character.vehicle,character.attachment,character.x,character.y,character.z,character.heading);
        }
    }
    // Native193F34 tests the registered parent independently of the actor's
    // health or wait/AI state, including dead58 actors still bound to a parent.
    for(const auto& attachment:world.vcsCharacterAttachments) {
        if(attachment.first==0||!world.activeVcsCharacterAttachment(attachment.first))continue;
        const auto character=world.missionCharacters.find(attachment.first);
        if(character!=world.missionCharacters.end()&&!character->second.networkControlled)
            storylandPositionVcsAttachedCharacter(world,attachment.first);
    }
}

// Native object, target, road and world-effect commands. Keeping the dispatch
// separate prevents character and object handles with equal integers aliasing.
int executeVcsObjectCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(!vm.vcs)return 0;
    int count=-1;
    switch(command) {
    case 0x0093:case 0x0114:case 0x01B9:case 0x0212:case 0x021A:case 0x0252:
    case 0x02FB:case 0x0306:case 0x03B2:case 0x03BE:case 0x03E8:case 0x04C1:case 0x01BD:case 0x01BE:case 0x0396:case 0x0420:
    case 0x04D4:
    case 0x0527:
        count = 1;
        break;
    case 0x00D4:case 0x0225:case 0x022A:case 0x028A:case 0x02AE:case 0x02AF:
    case 0x02FA:case 0x033D:case 0x0346:case 0x03B1:case 0x0431:case 0x0432:
    case 0x0490:case 0x0491:case 0x01F4:case 0x01F5:count=2;break;
    case 0x0475:count=3;break;
    case 0x010C:case 0x010D:case 0x020B:case 0x0224:case 0x0229:case 0x02AC:
    case 0x03A9:case 0x03AA:case 0x03F5:case 0x01BC:case 0x013C:case 0x02D3:count=4;break;
    case 0x0211:case 0x03E7:case 0x04EC:case 0x039B:count=5;break;
    case 0x0217:case 0x0304:case 0x0307:case 0x03B5:case 0x03C3:case 0x04C0:count=6;break;
    case 0x0210:case 0x0279:count=7;break;
    case 0x0305:case 0x0308:count=8;break;
    case 0x0165:count=9;break;
    case 0x0395:count=10;break;
    case 0x02E8:count=12;break;
    case 0x01ED:count=0;break;
    default:return 0;
    }
    if(!vm.environment) {error="VCS object command requires a live world.";return -1;}
    auto& world=*vm.environment;
    std::array<Operand,12> values{};
    for(int i=0;i<count;++i)if(!readOperand(vm,values[size_t(i)])) {error="Truncated object command.";return -1;}
    // Retail0x3340B0 consumes one operand and returns, without a pool lookup.
    if(command==0x0420)return 1;
    const auto real=[&](size_t index){return operandFloat(values[index]);};
    const auto writable=[&](size_t index) {
        const auto& operand=values[index];
        return operand.reference&&operand.variable<(operand.global?vm.globals.size():vm.locals.size());
    };
    const auto outputs=[&](size_t first,size_t last) {
        for(size_t i=first;i<=last;++i)if(!writable(i)) {error="Object query needs writable outputs.";return false;}
        return true;
    };
    const auto put=[&](size_t index,int32_t value){return writeOperand(vm,values[index],value)?1:-1;};
    const auto putReal=[&](size_t index,float value){int32_t bits;std::memcpy(&bits,&value,4);return put(index,bits);};
    const auto finite=[&](size_t first,size_t last) {
        for(size_t i=first;i<=last;++i)if(!std::isfinite(real(i))) {error="Object command requires finite values.";return false;}
        return true;
    };
    if (command == 0x039B) {
        if (!finite(0, 3) || !outputs(4, 4)) {
            return -1;
        }
        int32_t weapon = -1;
        if (!world.pickupWeaponInSphere ||
            !world.pickupWeaponInSphere({real(0), real(1), real(2)}, real(3), weapon)) {
            error = "Pickup weapon query requires a resident native weapon-model pickup.";
            return -1;
        }
        return put(4, weapon);
    }
    const auto ground=[&](std::array<float,3>& point,float offset=0.0f) {
        for(float value:point)if(!std::isfinite(value)) {error="World-effect position is non-finite.";return false;}
        // Retail VCS reserves exactly -100 and values <= -250; -101 is a real Z.
        if(point[2]==-100.0f||point[2]<=-250.0f) {
            float height=0;
            if(!world.groundHeightAt||!world.groundHeightAt({point[0],point[1],1000},height)||!std::isfinite(height)) {
                error="Ground sentinel requires the native collision world.";return false;
            }
            point[2]=height+offset;
        }
        return true;
    };
    const auto condition=[&](bool value){updateScmCondition(vm,value,inverted);return 1;};
    if (command == 0x04D4) {
        // Retail 0x2D5B50 takes one boolean, without a player-handle operand.
        world.playerFireproofReward = values[0].bits != 0;
        return 1;
    }
    if (command == 0x0527) {
        const int32_t character = values[0].bits;
        if (character != 0 && !world.missionCharacters.count(character)) {
            error = "IS_CHAR_ON_FIRE references an unknown character.";
            return -1;
        }
        // CPed+0x6B0 holds an attached CFire pointer. A nearby ground flame
        // does not satisfy this query; extinguishing clears the attachment.
        for (const auto& entry : world.scriptFires) {
            const auto& fire = entry.second;
            if (fire.active && fire.attachedKind == 1 && fire.attachedHandle == character) {
                return condition(true);
            }
        }
        return condition(false);
    }
    if(command==0x02D3) {
        if(!finite(0,3))return -1;
        // CPickups0x2D7FB8 checks nonzero native pickup type, including slots
        // waiting to respawn. The strict3D distance does not use a ground sentinel.
        for(const auto& pickup:world.pickups)
            if(pickup.type!=0&&!pickup.removed&&(!pickup.unavailable||pickup.respawnAt!=0)&&
                std::hypot(pickup.x-real(0),pickup.y-real(1),pickup.z-real(2))<real(3))return condition(true);
        return condition(false);
    }
    if(command==0x02AE||command==0x02AF) {
        const auto target=values[0].bits==0?world.playerTarget:
            world.characterTargets.count(values[0].bits)?world.characterTargets.at(values[0].bits):StorylandMissionEnvironment::Target{};
        const auto kind=command==0x02AE?StorylandMissionEnvironment::TargetKind::Character:StorylandMissionEnvironment::TargetKind::Object;
        bool exists=false;
        if(kind==StorylandMissionEnvironment::TargetKind::Character)
            exists=values[1].bits==0||world.missionCharacters.count(values[1].bits)!=0;
        else exists=std::any_of(world.objects.begin(),world.objects.end(),[&](const auto& o){return o.handle==values[1].bits;});
        return condition(exists&&target.kind==kind&&target.handle==values[1].bits);
    }
    if(command==0x02E8) {
        if(!finite(0,4)||!outputs(5,11)||real(3)<0||real(4)<0)return -1;
        if(!world.pedestrianPaths) {error="GET_CLOSEST_STRAIGHT_ROAD requires decoded native paths.";return -1;}
        const auto& paths=*world.pedestrianPaths;
        double best=double(real(4))*real(4);std::array<float,7> result{};
        for(size_t index=0;index<paths.nodes.size();++index) {
            const auto& first=paths.nodes[index];if(!first.vehicle||first.water||first.disabled)continue;
            for(const auto& link:first.links) {
                if(link.target>=paths.nodes.size()||link.target<=index)continue;
                const auto& second=paths.nodes[link.target];if(!second.vehicle||second.water||second.disabled)continue;
                const double dx=second.position.x-first.position.x,dy=second.position.y-first.position.y;
                const double length=std::hypot(dx,dy);if(length<real(3)||length<=0)continue;
                const double t=std::clamp(((real(0)-first.position.x)*dx+(real(1)-first.position.y)*dy)/(length*length),0.0,1.0);
                const double x=first.position.x+t*dx,y=first.position.y+t*dy,z=first.position.z+t*(second.position.z-first.position.z);
                const double distance=(x-real(0))*(x-real(0))+(y-real(1))*(y-real(1))+(z-real(2))*(z-real(2));
                if(distance>=best)continue;
                bool disabled=false;
                for(const auto& change:world.roadChanges)if(change.vehicle&&x>=change.bounds[0]&&x<=change.bounds[3]&&
                    y>=change.bounds[1]&&y<=change.bounds[4]&&z>=change.bounds[2]&&z<=change.bounds[5])disabled=!change.enabled;
                if(disabled)continue;
                best=distance;result={first.position.x,first.position.y,first.position.z,second.position.x,second.position.y,second.position.z,link.dividerWidth};
            }
        }
        for(size_t i=0;i<7;++i)putReal(i+5,result[i]);return 1;
    }
    if(command==0x013C) {
        if(!finite(0,2))return -1;
        return storylandAddScriptExplosion(world,values[3].bits,{real(0),real(1),real(2)},error)?1:-1;
    }
    if(command==0x0210) {
        if(!finite(1,6))return -1;
        for(const auto& explosion:world.scriptExplosions) {
            if(!explosion.active||explosion.type!=values[0].bits||
                uint32_t(world.gameMilliseconds-explosion.startedAt)>=explosion.lifetime)continue;
            bool inside=true;
            for(size_t axis=0;axis<3;++axis)inside=inside&&
                explosion.position[axis]>=std::min(real(axis+1),real(axis+4))&&
                explosion.position[axis]<=std::max(real(axis+1),real(axis+4));
            if(inside)return condition(true);
        }
        return condition(false);
    }
    if(command==0x01ED) {
        for(auto fire=world.scriptFires.begin();fire!=world.scriptFires.end();)
            if(fire->second.scriptOwned)fire=world.scriptFires.erase(fire);else ++fire;
        return 1;
    }
    if(command==0x01BD||command==0x01BE) {
        const int32_t handle=int16_t(values[0].bits);
        if(handle<0||handle>=40) {error="Script fire handle is outside the native pool.";return -1;}
        const auto fire=world.scriptFires.find(handle);
        if(command==0x01BE) {world.scriptFires.erase(handle);return 1;}
        return condition(fire==world.scriptFires.end()||!fire->second.active);
    }
    if(command==0x01BC||command==0x01F4||command==0x01F5) {
        if(!outputs(size_t(count-1),size_t(count-1)))return -1;
        StorylandMissionEnvironment::ScriptFire fire;
        if(command==0x01BC) {fire.position={real(0),real(1),real(2)};if(!ground(fire.position))return -1;}
        else {
            fire.attachedKind=command==0x01F4?2:1;fire.attachedHandle=values[0].bits;
            if(fire.attachedKind==2) {
                const auto car=world.missionVehicles.find(fire.attachedHandle);
                if(car==world.missionVehicles.end()||car->second.deleted) {error="START_CAR_FIRE references an unknown vehicle.";return -1;}
                fire.position={car->second.x,car->second.y,car->second.z};
            } else if(fire.attachedHandle==0)fire.position={world.x,world.y,world.z};
            else {
                const auto ped=world.missionCharacters.find(fire.attachedHandle);
                if(ped==world.missionCharacters.end()) {error="START_CHAR_FIRE references an unknown character.";return -1;}
                fire.position={ped->second.x,ped->second.y,ped->second.z};
            }
        }
        // START_CAR/CHAR_FIRE returns -1 when the entity is already burning.
        if(fire.attachedKind)for(const auto& entry:world.scriptFires)
            if(entry.second.active&&entry.second.attachedKind==fire.attachedKind&&entry.second.attachedHandle==fire.attachedHandle)
                return put(size_t(count-1),-1);
        // Script fires retain their pool reservation after extinguishing until
        // REMOVE_SCRIPT_FIRE clears the separate native script-owned flag.
        int32_t handle=0;while(handle<40&&world.scriptFires.count(handle))++handle;
        if(handle==40) {error="The native script fire pool is full.";return -1;}
        if(fire.attachedKind==2&&world.vehicleNativeType&&world.vehicleNativeType(fire.attachedHandle)==0) {
            auto& car=world.missionVehicles.at(fire.attachedHandle);
            if(car.health>=225) {car.health=215;car.healthChangedByScript=true;}
        }
        world.scriptFires[handle]=fire;return put(size_t(count-1),handle);
    }
    if(command==0x04EC) {
        if(!finite(0,3)||real(3)<0)return -1;
        std::array<float,3> point{real(0),real(1),real(2)};if(!ground(point))return -1;
        if(values[4].bits) {
            storylandSuppressFireWithWater(world,point,real(3));
            return 1;
        }
        for(auto& entry:world.scriptFires) {
            auto& fire=entry.second;const auto& p=fire.position;
            if(!fire.active||std::hypot(p[0]-point[0],p[1]-point[1],p[2]-point[2])>=real(3))continue;
            fire.active=false;
            fire.attachedKind=0;
            fire.attachedHandle=-1;
        }
        return 1;
    }
    if(command==0x0396) {world.scriptCoronas.erase(values[0].bits);return 1;}
    if(command==0x0165||command==0x0395) {
        if(!finite(0,3))return -1;
        if(command==0x0165&&!values[0].reference) {error="DRAW_CORONA requires a position variable identity.";return -1;}
        if(command==0x0395&&(!outputs(9,9)||world.nextScriptCoronaId<0||world.nextScriptCoronaId==INT32_MAX))return -1;
        StorylandMissionEnvironment::ScriptCorona corona;
        corona.position={real(0),real(1),real(2)};corona.size=real(3);corona.type=values[4].bits;corona.flareType=values[5].bits;
        if(corona.type<0||corona.type>8||corona.flareType<0||corona.flareType>2) {error="Unsupported native corona texture or flare type.";return -1;}
        if(!ground(corona.position,command==0x0165?.1f:0.0f))return -1;
        for(size_t i=0;i<3;++i)corona.color[i]=uint8_t(values[i+6].bits);
        if(command==0x0165) {
            const auto key=std::make_pair(values[0].global?uintptr_t(0):reinterpret_cast<uintptr_t>(&vm),int32_t(values[0].variable));
            if(!world.frameScriptCoronas.count(key)&&world.frameScriptCoronas.size()>=4096) {error="Frame corona limit reached.";return -1;}
            world.frameScriptCoronas[key]=corona;
        } else {
            if(world.scriptCoronas.size()>=4096) {error="Permanent corona limit reached.";return -1;}
            const int32_t handle=world.nextScriptCoronaId++;world.scriptCoronas.emplace(handle,corona);put(9,handle);
        }
        return 1;
    }
    if(command==0x0217) {
        if(!finite(0,3)||real(3)<0||!world.setClosestObjectVisibility) {error="Closest-object visibility requires the native world binding.";return -1;}
        std::array<float,3> position{real(0),real(1),real(2)};if(!ground(position))return -1;
        return world.setClosestObjectVisibility(position,real(3),values[4].bits,values[5].bits!=0,error)?1:-1;
    }
    if(command==0x03C3) {
        if(!finite(0,5))return -1;
        std::array<float,6> box{};for(size_t i=0;i<6;++i)box[i]=real(i);
        for(size_t axis=0;axis<3;++axis)if(box[axis]>box[axis+3])std::swap(box[axis],box[axis+3]);
        std::vector<int32_t> remove;
        for(const auto& object:world.objects)if(object.x>=box[0]&&object.x<=box[3]&&object.y>=box[1]&&object.y<=box[4]&&object.z>=box[2]&&object.z<=box[5]&&!object.persistent)remove.push_back(object.handle);
        for(int32_t handle:remove)storylandDeleteScriptObject(world,handle);return 1;
    }
    const int32_t handle=command==0x00D4?values[1].bits:values[0].bits;
    if(command==0x0093) {
        // Retail 0x30D090 explicitly tolerates a missing pool entry.
        if(handle<=0x1DD00)storylandDeleteScriptObject(world,handle);
        return 1;
    }
    const auto found=std::find_if(world.objects.begin(),world.objects.end(),[&](const auto& object){return object.handle==handle;});
    if (command == 0x0114) {
        // Native 0x25B1F8 accepts an absent pool entry and converts only mission objects.
        if (found != world.objects.end() && found->creationSource == 2) {
            found->persistent = false;
            found->creationSource = 3;
        }
        if (vm.isMissionThread) world.missionCleanupObjects.erase(handle);
        return 1;
    }
    if(command==0x0252)return condition(found!=world.objects.end());
    if((command==0x03B2||command==0x03E8||command==0x04C1)&&found==world.objects.end())return 1;
    if(command==0x03B1&&found==world.objects.end())return condition(false);
    if(found==world.objects.end()) {error="Object command references an unknown object.";return -1;}
    auto& object=*found;
    if(command==0x03BE) {object.damage.renderDamaged=true;return 1;}
    if(command==0x028A) {
        object.drawLast=values[1].bits!=0;return 1;
    }
    if(command==0x02FA) {
        object.recordsCollisions=values[1].bits!=0;return 1;
    }
    if(command==0x0279) {
        if(!outputs(4,6)||!finite(1,3))return -1;
        const auto matrix=storylandScriptObjectMatrix(object.x,object.y,object.z,object.rotationX,object.rotationY,object.heading);
        std::array<float,3> position{object.x,object.y,object.z};
        for(size_t row=0;row<3;++row)for(size_t k=0;k<3;++k)position[row]+=matrix[k*4+row]*real(k+1);
        for(float value:position)if(!std::isfinite(value)) {error="Object-relative coordinate overflows.";return -1;}
        for(size_t row=0;row<3;++row)putReal(4+row,position[row]);return 1;
    }
    if(command==0x010C||command==0x03F5) {
        if(!outputs(1,3))return -1;
        putReal(1,command==0x010C?object.x:object.rotationX);putReal(2,command==0x010C?object.y:object.rotationY);
        putReal(3,command==0x010C?object.z:object.heading);return 1;
    }
    if(command==0x0491) {if(!outputs(1,1))return -1;return put(1,int32_t(object.damage.health));}
    if(command==0x0431||command==0x0432) {
        if(!outputs(1,1))return -1;
        const auto basis=storylandScriptObjectMatrix(0,0,0,object.rotationX,object.rotationY,object.heading);
        const float denominator=basis[4]*basis[4]+basis[5]*basis[5];
        if(denominator<=0||!std::isfinite(denominator)) {error="Object heading vector is degenerate.";return -1;}
        return putReal(1,basis[command==0x0431?4:5]/denominator);
    }
    if(command==0x0212) {
        if(std::find(world.targettableObjects.begin(),world.targettableObjects.end(),handle)!=world.targettableObjects.end())return 1;
        for(auto& slot:world.targettableObjects)
            if(std::none_of(world.objects.begin(),world.objects.end(),[&](const auto& o){return o.handle==slot;})) {slot=handle;break;}
        return 1;
    }
    if(command==0x021A)return condition(!object.damage.visible||object.damage.renderDamaged);
    if(command==0x03B1)return condition(storylandObjectWeaponMatches(object.damage.lastWeapon,values[1].bits));
    if(command==0x03B2) {object.damage.lastWeapon=-1;return 1;}
    if(command==0x01B9) {
        if(!world.objectOnScreen) {error="IS_OBJECT_ON_SCREEN requires the active camera projection.";return -1;}
        return condition(object.damage.visible&&world.objectOnScreen(object));
    }
    if(command==0x00D4) {
        if(!world.objectTouchingCharacter) {error="Object contact requires decoded collision bounds.";return -1;}
        return condition(world.objectTouchingCharacter(object,values[0].bits));
    }
    if(command==0x0306)return condition(world.objectInWater?world.objectInWater(object):object.inWater);
    if(command==0x02FB)return condition(object.collided);
    if(command==0x0304||command==0x0305||command==0x0307||command==0x0308) {
        const bool three=command==0x0305||command==0x0308,area=command==0x0307||command==0x0308;
        const size_t axes=three?3:2;if(!finite(1,axes*2))return -1;
        const std::array<float,3> point{object.x,object.y,object.z};bool inside=true;double distance=0;
        for(size_t i=0;i<axes;++i) {
            if(area)inside=inside&&point[i]>=std::min(real(i+1),real(i+1+axes))&&point[i]<=std::max(real(i+1),real(i+1+axes));
            else {const double radius=std::fabs(real(i+1+axes)),d=std::fabs(point[i]-real(i+1));inside=inside&&d<=radius;distance+=radius>0?d*d/(radius*radius):(d==0?0:INFINITY);}
        }
        if(!area&&values[axes*2+1].bits)inside=distance<=1;
        return condition(inside);
    }
    if(command==0x010D) {
        std::array<float,3> position{real(1),real(2),real(3)};if(!ground(position))return -1;
        object.x=position[0];object.y=position[1];object.z=position[2];return 1;
    }
    if(command==0x0211||command==0x03E7) {
        if(!finite(2,4))return -1;
        const auto car=world.missionVehicles.find(values[1].bits);
        if(car==world.missionVehicles.end()||car->second.deleted) {error="Object placement references an unknown vehicle.";return -1;}
        std::array<float,16> matrix;
        if(!world.vehicleWorldMatrix||!world.vehicleWorldMatrix(values[1].bits,matrix)) {
            error="Object placement requires the native vehicle matrix.";return -1;
        }
        const std::array<float,3> offset{real(2),real(3),real(4)};
        // 0211 copies the complete car basis; 03E7 preserves the object's
        // authored basis as its local attachment matrix (CObject0x132198).
        const std::array<float,3> rotation=command==0x03E7?
            std::array<float,3>{object.rotationX,object.rotationY,object.heading}:std::array<float,3>{};
        auto candidate=object;
        if(!storylandApplyScriptObjectAttachment(candidate,matrix,offset,rotation)) {
            error="Object placement produced an invalid transform.";return -1;
        }
        if(command==0x03E7) {
            candidate.attachedVehicle=values[1].bits;candidate.attachedCharacter=-1;candidate.attachedBone=-1;
            candidate.attachmentOffset=offset;candidate.attachmentRotation=rotation;candidate.dynamic=true;
        } else {
            std::array<float,3> velocity;
            if(!world.vehicleWorldVelocity||!world.vehicleWorldVelocity(values[1].bits,velocity)||
                !std::all_of(velocity.begin(),velocity.end(),[](float v){return std::isfinite(v);})) {
                error="Relative object placement requires the live vehicle velocity.";return -1;
            }
            // CPhysical::PlacePhysicalRelativeToOtherPhysical0x355F10 leads
            // by0.9 native time steps and copies linear velocity. Host units/s.
            const float lead=.9f*world.nativeScriptTimeStep/50.0f;
            candidate.x+=velocity[0]*lead;candidate.y+=velocity[1]*lead;candidate.z+=velocity[2]*lead;
            if(!std::isfinite(candidate.x)||!std::isfinite(candidate.y)||!std::isfinite(candidate.z)) {
                error="Relative object placement overflowed.";return -1;
            }
            candidate.velocity=velocity;
        }
        object=std::move(candidate);return 1;
    }
    if(command==0x04C0) {
        if(!finite(3,5))return -1;
        const int32_t character=values[1].bits,bone=int8_t(uint8_t(values[2].bits));
        if(character!=0&&!world.missionCharacters.count(character)) {
            error="ATTACH_OBJECT_TO_CHAR references an unknown character.";return -1;
        }
        std::array<float,16> matrix;
        if(!world.characterBoneMatrix||!world.characterBoneMatrix(character,bone,matrix)) {
            error="ATTACH_OBJECT_TO_CHAR requires the decoded HAnim bone matrix.";return -1;
        }
        const std::array<float,3> offset{real(3),real(4),real(5)},rotation{object.rotationX,object.rotationY,object.heading};
        auto candidate=object;
        if(!storylandApplyScriptObjectAttachment(candidate,matrix,offset,rotation)) {
            error="ATTACH_OBJECT_TO_CHAR produced an invalid transform.";return -1;
        }
        candidate.attachedCharacter=character;candidate.attachedVehicle=-1;candidate.attachedBone=bone;
        candidate.attachmentOffset=offset;candidate.attachmentRotation=rotation;candidate.dynamic=true;
        object=std::move(candidate);return 1;
    }
    if(command==0x03E8||command==0x04C1) {object.attachedVehicle=-1;object.attachedCharacter=-1;return 1;}
    if(command==0x020B) {
        if(!finite(1,2))return -1;
        const float previous=object.heading,target=real(1),speed=std::fabs(real(2));
        const float delta=std::remainder(target-previous,360.0f);
        const float next=previous+std::clamp(delta,-speed,speed);
        if(values[3].bits&&!world.objectSlideCollision) {error="ROTATE_OBJECT collision needs the native bounds.";return -1;}
        object.heading=next;
        if(values[3].bits&&world.objectSlideCollision(handle,{object.x,object.y,object.z})!=0) {object.heading=previous;return condition(false);}
        return condition(std::fabs(delta)<=speed);
    }
    if(command==0x0224||command==0x0229||command==0x03A9||command==0x02AC) {
        if(!finite(1,3))return -1;
        if(command==0x02AC) {object.rotationX=real(1);object.rotationY=real(2);object.heading=real(3);}
        else if(command==0x03A9)object.rotationVelocity={real(1),real(2),real(3)};
        else {
            for(size_t i=0;i<3;++i)object.velocity[i]=command==0x0229?object.velocity[i]+real(i+1):real(i+1);
            object.dynamic=true;
        }
        return 1;
    }
    // Validate every argument before hydrating or changing persistent state.
    if(command==0x03AA&&(!finite(1,3)||real(1)<=0||real(2)<=0||real(3)<0||real(3)>1))return -1;
    if(command==0x0475&&values[2].bits&&!finite(1,1))return -1;
    storylandHydrateScriptObject(world,object);
    switch(command) {
    case 0x0225:object.collisionEnabled=values[1].bits!=0;break;
    case 0x022A:object.dynamic=values[1].bits!=0;break;
    case 0x033D:object.positionFrozen=values[1].bits!=0;object.damage.disableMoveForce=object.positionFrozen;break;
    case 0x0346:object.area=uint8_t(values[1].bits);break;
    case 0x0490:object.damage.health=int8_t(uint8_t(values[1].bits));object.collisionEnabled=true;object.damage.proofs&=uint8_t(~4u);break;
    case 0x03AA:
        object.damage.mass=real(1);object.damage.turnMass=real(2);object.damage.airResistance=real(3);
        object.damage.disableMoveForce=real(1)>=99999.0f;object.damage.disableTurnForce=object.damage.disableMoveForce;
        object.gravity=!object.damage.disableMoveForce;break;
    case 0x03B5:
        object.damage.proofs=0;for(size_t i=0;i<5;++i)if(values[i+1].bits)object.damage.proofs|=uint8_t(1u<<i);break;
    case 0x0475:
        object.collisionEnabled=values[2].bits!=0;
        if(object.collisionEnabled)object.damage.multiplier=real(1);else object.damage.damageEffect=0;
        break;
    default:return 0;
    }
    return 1;
}

int executeVcsAdditionalOpcodeCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(!vm.vcs)return 0;
    // VCS 0x3917A0 publishes true, subject to script inversion and
    // AND/OR aggregation, without consuming an operand.
    if(command==0x04DC) {
        updateScmCondition(vm,true,inverted);
        return 1;
    }
    // The VCS PS2 handlers at 0x185198 and 0x1853F8 collect one
    // parameter. 0x387180 collects three, copies them to its stack and
    // returns. 0x183880 also collects one operand and calls a native return.
    // None changes game state.
    if(command==0x03AD||command==0x03C5||command==0x04DB||command==0x04FE) {
        const int count=command==0x04FE?3:1;
        for(int index=0;index<count;++index) {
            Operand value;
            if(!readOperand(vm,value)) {
                error="Native VCS command has a truncated operand.";
                return -1;
            }
        }
        return 1;
    }
    static constexpr uint16_t routes[]={
        0x003A,0x003B,0x0047,0x0080,0x0081,0x0082,0x0083,0x0085,0x008A,0x008D,0x008E,0x0097,0x00A8,
        0x00F6,0x00F7,0x00F8,0x00F9,0x00FA,0x00FB,0x00FC,0x00FD,0x00FE,0x00FF,0x0100,0x0101,0x0102,0x0103,0x0104,0x0105,
        0x0131,0x0132,0x0133,0x0134,0x0136,0x0137,0x0163,0x0186,0x01D2,0x01D3,0x01D7,0x0234,0x0235,0x023C,0x0244,
        0x0248,0x027E,0x029B,0x02BC,0x02C1,0x02C3,0x02C5,0x02DD,0x02E5,0x031A,0x031E,0x032C,0x033E,0x033F,
        0x0354,0x0355,0x0356,0x0358,0x0360,0x036B,0x036D,0x0374,0x0375,0x0376,0x03C7,0x03CA,0x03D2,0x03D9,0x03DB,
        0x03E6,0x03FE,0x043E,0x043F,0x0440,0x0441,0x0478,0x0479,0x047A,0x04BC,0x04E3,0x050B,0x050C,
        0x02AA,0x0371,0x039D,0x039E,0x039F,0x03BA,0x03C8,0x03CB,0x03CE,0x041E,0x041F,0x044B,0x0504
    };
    static_assert(std::size(routes)==99,"VCS additional opcode batch must contain exactly 99 routes");
    if(std::find(std::begin(routes),std::end(routes),command)==std::end(routes))return 0;
    if(!vm.environment){error="VCS opcode requires the live world.";return -1;}
    auto& world=*vm.environment;
    auto read=[&](Operand& value)->bool{return readOperand(vm,value);};
    auto real=[&](const Operand& value)->float{return operandFloat(value);};
    auto finite=[&](float value)->bool{return std::isfinite(value);};
    auto character=[&](int32_t handle)->StorylandMissionEnvironment::Character* {
        if(handle==0)return nullptr;
        auto found=world.missionCharacters.find(handle);
        return found==world.missionCharacters.end()?nullptr:&found->second;
    };
    auto vehicle=[&](int32_t handle)->StorylandMissionEnvironment::VehicleScriptState* {
        auto found=world.missionVehicles.find(handle);
        return found==world.missionVehicles.end()||found->second.deleted?nullptr:&found->second;
    };
    auto charPosition=[&](int32_t handle,std::array<float,3>& out,bool& inCar,float& speed)->bool {
        if(handle==0){out={world.x,world.y,world.z};inCar=world.inVehicle;speed=0;return true;}
        const auto* ped=character(handle);if(!ped)return false;
        out={ped->x,ped->y,ped->z};inCar=ped->vehicle>=0;speed=ped->movementSpeed;return true;
    };
    auto carPosition=[&](int32_t handle,std::array<float,3>& out,float& speed)->bool {
        const auto* car=vehicle(handle);if(!car)return false;out={car->x,car->y,car->z};speed=car->speed;return true;
    };
    auto axisInside=[&](float value,float center,float radius,bool sphere)->bool {
        const float r=std::fabs(radius);return sphere?std::fabs(value-center)<=r:(value>=center-r&&value<=center+r);
    };
    auto pointRadius=[&](const std::array<float,3>& p,const std::array<float,3>& c,const std::array<float,3>& r,bool useZ,bool sphere)->bool {
        if(sphere){
            const double nx=r[0]>0?(p[0]-c[0])/std::fabs(r[0]):0;
            const double ny=r[1]>0?(p[1]-c[1])/std::fabs(r[1]):0;
            const double nz=useZ&&r[2]>0?(p[2]-c[2])/std::fabs(r[2]):0;
            return nx*nx+ny*ny+(useZ?nz*nz:0)<=1.0;
        }
        return axisInside(p[0],c[0],r[0],false)&&axisInside(p[1],c[1],r[1],false)&&(!useZ||axisInside(p[2],c[2],r[2],false));
    };
    auto pointBox=[&](const std::array<float,3>& p,const std::array<float,3>& a,const std::array<float,3>& b,bool useZ,bool sphere)->bool {
        std::array<float,3> c{(a[0]+b[0])*.5f,(a[1]+b[1])*.5f,(a[2]+b[2])*.5f};
        std::array<float,3> r{std::fabs(b[0]-a[0])*.5f,std::fabs(b[1]-a[1])*.5f,std::fabs(b[2]-a[2])*.5f};
        return pointRadius(p,c,r,useZ,sphere);
    };
    auto update=[&](bool value){updateScmCondition(vm,value,inverted);};

    if(command==0x003A||command==0x003B){
        Operand value;if(!read(value)||!value.reference){error="ABS requires a writable variable.";return -1;}
        if(command==0x003A){const int64_t n=value.bits;return writeOperand(vm,value,int32_t(uint32_t(n<0?-n:n)))?1:-1;}
        float f=real(value);if(!finite(f)){error="ABS float operand is not finite.";return -1;}f=std::fabs(f);int32_t bits=0;std::memcpy(&bits,&f,4);return writeOperand(vm,value,bits)?1:-1;
    }
    if(command==0x0131||command==0x01D2||command==0x01D3){
        Operand input,output;if(!read(input)||!read(output)||!output.reference){error="Math opcode requires an input and writable output.";return -1;}
        const float value=real(input);if(!finite(value)){error="Math opcode input is not finite.";return -1;}
        float result=0;
        if(command==0x0131){if(value<0){error="SQRT input is negative.";return -1;}result=std::sqrt(value);} else {
            const float radians=value*0.01745329251994329577f;result=command==0x01D2?std::sin(radians):std::cos(radians);
        }
        int32_t bits=0;std::memcpy(&bits,&result,4);return writeOperand(vm,output,bits)?1:-1;
    }
    if(command==0x02E5){Operand a,b;if(!read(a)||!read(b)){error="Comparison is truncated.";return -1;}update(real(a)>=real(b));return 1;}

    const bool locateChar=command==0x0047||(command>=0x0080&&command<=0x008E)||(command>=0x00F6&&command<=0x00FF)||
        command==0x0132||command==0x0133||command==0x0134||command==0x0136||command==0x0137||command==0x02C1||command==0x02C3||command==0x02C5;
    if(locateChar){
        Operand h;if(!read(h)){error="Character location query has no handle.";return -1;}
        std::array<float,3> p{};bool inCar=false;float movement=0;if(!charPosition(h.bits,p,inCar,movement)){update(false);return 1;}
        bool requireFoot=false,requireCar=false,stopped=false,useZ=false,sphere=false,matched=false;
        switch(command){
        case 0x0081:case 0x008A:case 0x008D:case 0x00F6:case 0x00F9:case 0x00FB:case 0x00FE:case 0x0133:case 0x0136:case 0x02C5:requireFoot=true;break;
        case 0x0082:case 0x0085:case 0x008E:case 0x00F7:case 0x00FA:case 0x00FC:case 0x00FF:case 0x0134:case 0x0137:case 0x02C3:requireCar=true;break;
        default:break;}
        switch(command){case 0x0083:case 0x0085:case 0x008D:case 0x008E:case 0x00F8:case 0x00F9:case 0x00FA:case 0x00FD:case 0x00FE:case 0x00FF:stopped=true;break;default:break;}
        useZ=command==0x0047||command==0x008A||command==0x008D||command==0x008E||command==0x00FB||command==0x00FC||command==0x00FD||command==0x00FE||command==0x00FF||command==0x0136||command==0x0137||command==0x02C5;
        const bool targetVehicle=command==0x0132||command==0x0133||command==0x0134||command==0x0136||command==0x0137;
        const bool targetObject=command==0x02C1||command==0x02C3||command==0x02C5;
        if(targetVehicle||targetObject){
            Operand target;if(!read(target)){error="Entity-relative location query has no target.";return -1;}
            std::array<float,3> c{};
            if(targetVehicle){float unused=0;if(!carPosition(target.bits,c,unused)){update(false);return 1;}}
            else {const auto found=std::find_if(world.objects.begin(),world.objects.end(),[&](const auto& item){return item.handle==target.bits;});if(found==world.objects.end()){update(false);return 1;}c={found->x,found->y,found->z};}
            std::array<float,3> r{};Operand rx,ry,rz,sp;
            if(!read(rx)||!read(ry)||(useZ&&!read(rz))||!read(sp)){error="Entity-relative location query is truncated.";return -1;}
            r={real(rx),real(ry),useZ?real(rz):1};sphere=sp.bits!=0;matched=pointRadius(p,c,r,useZ,sphere);
        } else if(command>=0x00F6&&command<=0x00FF||command==0x0047){
            std::array<float,3>a{},b{};Operand v;
            for(int i=0;i<(useZ?3:2);++i){if(!read(v)){error="Area query is truncated.";return -1;}a[i]=real(v);}for(int i=0;i<(useZ?3:2);++i){if(!read(v)){error="Area query is truncated.";return -1;}b[i]=real(v);}if(!read(v)){error="Area query sphere flag is missing.";return -1;}sphere=v.bits!=0;matched=pointBox(p,a,b,useZ,sphere);
        } else {
            std::array<float,3> c{},r{};Operand v;
            for(int i=0;i<(useZ?3:2);++i){if(!read(v)){error="Locate query center is truncated.";return -1;}c[i]=real(v);}for(int i=0;i<(useZ?3:2);++i){if(!read(v)){error="Locate query radius is truncated.";return -1;}r[i]=real(v);}if(!read(v)){error="Locate query sphere flag is missing.";return -1;}sphere=v.bits!=0;matched=pointRadius(p,c,r,useZ,sphere);
        }
        if(requireFoot&&inCar)matched=false;if(requireCar&&!inCar)matched=false;if(stopped&&std::fabs(movement)>.05f)matched=false;update(matched);return 1;
    }
    if(command>=0x0100&&command<=0x0105){
        Operand h;if(!read(h)){error="Vehicle location query has no handle.";return -1;}std::array<float,3> p{};float speed=0;if(!carPosition(h.bits,p,speed)){update(false);return 1;}
        const bool stopped=command==0x0100||command==0x0101||command==0x0103||command==0x0105;const bool useZ=command==0x0101||command==0x0104||command==0x0105;const bool box=command==0x0100||command==0x0101;
        std::array<float,3>a{},b{},c{},r{};Operand v;bool sphere=false,matched=false;
        if(box){for(int i=0;i<(useZ?3:2);++i){if(!read(v))return -1;a[i]=real(v);}for(int i=0;i<(useZ?3:2);++i){if(!read(v))return -1;b[i]=real(v);}if(!read(v))return -1;sphere=v.bits!=0;matched=pointBox(p,a,b,useZ,sphere);}else{for(int i=0;i<(useZ?3:2);++i){if(!read(v))return -1;c[i]=real(v);}for(int i=0;i<(useZ?3:2);++i){if(!read(v))return -1;r[i]=real(v);}if(!read(v))return -1;sphere=v.bits!=0;matched=pointRadius(p,c,r,useZ,sphere);}if(stopped&&std::fabs(speed)>.05f)matched=false;update(matched);return 1;
    }
    if(command==0x0097){
        Operand model,x,y,z,heading,out;if(!read(model)||!read(x)||!read(y)||!read(z)||!read(heading)||!read(out)||!out.reference){error="Remote vehicle creation is truncated.";return -1;}
        const float px=real(x),py=real(y),pz=real(z),ph=real(heading);if(!finite(px)||!finite(py)||!finite(pz)||!finite(ph)){error="Remote vehicle coordinates are invalid.";return -1;}
        int32_t handle=world.nextMissionVehicleHandle++;auto& car=world.missionVehicles[handle];car.model=model.bits;car.x=px;car.y=py;car.z=pz;car.heading=ph;car.persistent=true;car.driver=0;world.remoteVehicle=handle;if(world.prepareRemoteVehicle){std::string reason;if(!world.prepareRemoteVehicle(handle,reason)){world.missionVehicles.erase(handle);error=reason;return -1;}}return writeOperand(vm,out,handle)?1:-1;
    }
    if(command==0x00A8){Operand h;if(!read(h))return -1;const auto* car=vehicle(h.bits);update(car&&(car->wrecked||car->health<=0));return 1;}
    if(command==0x0163){
        Operand x,y,out;if(!read(x)||!read(y)||!read(out)||!out.reference){error="GRAB_PHONE is truncated.";return -1;}if(!world.findPayphone){error="GRAB_PHONE requires native payphone placements.";return -1;}
        std::set<uint64_t> claimed;for(const auto& item:world.payphones)claimed.insert(item.second.worldKey);StorylandMissionEnvironment::Payphone phone;if(!world.findPayphone({real(x),real(y)},claimed,phone.worldKey,phone.position)){error="No unclaimed payphone was found.";return -1;}int32_t handle=0;while(world.payphones.count(handle))++handle;world.payphones.emplace(handle,phone);return writeOperand(vm,out,handle)?1:-1;
    }
    if(command==0x027E){Operand h;if(!read(h))return -1;auto found=world.payphones.find(h.bits);if(found==world.payphones.end()){error="TURN_PHONE_ON references an unknown payphone.";return -1;}found->second.enabled=true;return 1;}
    if(command==0x0186){Operand value;if(!read(value))return -1;world.statistics["CollisionObjectTakeTaxi"]=value.bits;return 1;}
    if(command==0x01D7||command==0x0234){
        std::array<Operand,10> v{};const int count=command==0x01D7?10:3;for(int i=0;i<count;++i)if(!read(v[i])){error="Crane command is truncated.";return -1;}if(command==0x01D7){world.statistics["VcsCrusherCraneActive"]=1;world.statistics["VcsCrusherCraneX"]=real(v[0]);world.statistics["VcsCrusherCraneY"]=real(v[1]);return 1;}update(world.statistics["VcsCrusherCraneActive"]!=0);return 1;
    }
    if(command==0x023C) {
        Operand x,y,z;
        if(!read(x)||!read(y)||!read(z))return -1;
        if(world.objectWorld)
            storylandVcsPoliceRadioReport(world.vcsPoliceRadio,world.objectWorld->zones,
                {real(x),real(y),real(z)},world.audioSubsystemEnabled,world.scriptAudioMode);
        return 1;
    }
    if(command==0x0244){Operand x,y,z,r,from,to;if(!read(x)||!read(y)||!read(z)||!read(r)||!read(from)||!read(to))return -1;world.buildingSwapCommands.push_back({uint32_t(from.bits),uint32_t(to.bits),true});++world.buildingSwapRevision;return 1;}
    if(command==0x029B) {
        Operand handle,flag;
        if(!read(handle)||!read(flag))return -1;
        auto* ped=character(handle.bits);
        if(!ped) {error="Character flag references an unknown character.";return -1;}
        ped->isChrisCriminal=flag.bits!=0;
        return 1;
    }
    if(command==0x02BC){
        std::array<Operand,10> v{};for(auto& item:v)if(!read(item))return -1;if(!v[9].reference)return -1;int32_t result=-1;for(const auto& entry:world.missionCharacters){const auto& ped=entry.second;if(!ped.alive)continue;if(ped.x<std::min(real(v[0]),real(v[2]))||ped.x>std::max(real(v[0]),real(v[2]))||ped.y<std::min(real(v[1]),real(v[3]))||ped.y>std::max(real(v[1]),real(v[3])))continue;if(ped.type==6||ped.pedStats.pedType==6){result=entry.first;break;}}return writeOperand(vm,v[9],result)?1:-1;
    }
    if(command==0x02DD){std::array<Operand,6> v{};for(auto& item:v)if(!read(item))return -1;if(!v[5].reference)return -1;int32_t id=world.nextScriptCoronaId++;world.statistics["ProtectionPickup:"+std::to_string(id)]=v[3].bits;return writeOperand(vm,v[5],id)?1:-1;}
    if(command==0x031A){Operand x,y,z;if(!read(x)||!read(y)||!read(z))return -1;world.statistics["SwatRopeX"]=real(x);world.statistics["SwatRopeY"]=real(y);world.statistics["SwatRopeZ"]=real(z);return 1;}
    if(command==0x031E){std::array<Operand,7> v{};for(auto& item:v)if(!read(item))return -1;if(!v[6].reference)return -1;const float dx=real(v[3])-real(v[0]),dy=real(v[4])-real(v[1]),dz=real(v[5])-real(v[2]);float d=std::sqrt(dx*dx+dy*dy+dz*dz);int32_t bits=0;std::memcpy(&bits,&d,4);return writeOperand(vm,v[6],bits)?1:-1;}
    if(command==0x032C){Operand x,y,z;if(!read(x)||!read(y)||!read(z))return -1;update(false);return 1;}
    if(command==0x033E){std::array<Operand,8> v{};for(auto& item:v)if(!read(item))return -1;world.statistics["TaxiShortcutActive"]=1;for(int i=0;i<8;++i)world.statistics["TaxiShortcut"+std::to_string(i)]=real(v[i]);return 1;}
    if(command==0x033F){
        if(world.clearRestartTaxi)world.clearRestartTaxi();
        world.restartMissionTaxiDestinationSet=false;
        world.statistics["TaxiShortcutActive"]=0;
        return 1;
    }
    if(command==0x0354){update(world.statistics["WantedStarsFlashing"]!=0);return 1;}
    if(command==0x0355||command==0x0356){Operand v;if(!read(v))return -1;world.statistics[command==0x0355?"AllowHurricanes":"BridgeStatusMp3"]=v.bits;return 1;}
    if(command==0x0358){Operand player,out;if(!read(player)||!read(out)||!out.reference)return -1;return writeOperand(vm,out,int32_t(world.statistics["CoachPassengersDroppedOff"]))?1:-1;}
    if(command==0x0360){
        std::array<Operand,6> v{};for(auto& item:v)if(!read(item))return -1;
        StorylandMissionEnvironment::ScriptParticle p;p.type=15;
        p.position={real(v[0]),real(v[1]),real(v[2])};
        p.velocity={(real(v[3])-real(v[0]))/50.0f,(real(v[4])-real(v[1]))/50.0f,(real(v[5])-real(v[2]))/50.0f};
        std::string reason;if(world.createScriptParticles&&!world.createScriptParticles({p},reason)){error=reason;return -1;}return 1;
    }
    if(command==0x036D){
        Operand heli,radius,flag;if(!read(heli)||!read(radius)||!read(flag))return -1;
        const auto* car=vehicle(heli.bits);if(!car)return 1;
        StorylandMissionEnvironment::ScriptParticle p;p.type=53;
        p.position={car->x,car->y,car->z-.35f};p.size=std::max(.1f,std::fabs(real(radius)));
        p.velocity={0,0,-.02f};
        std::string reason;if(world.createScriptParticles&&!world.createScriptParticles({p},reason)){error=reason;return -1;}return 1;
    }
    if(command==0x036B){Operand player;if(!read(player))return -1;update(world.statistics["TaxiShortcutActive"]!=0&&world.inVehicle);return 1;}
    if(command==0x0374||command==0x0375||command==0x0376){const int count=command==0x0374?3:command==0x0375?4:5;std::vector<Operand> v(count);for(auto& item:v)if(!read(item))return -1;world.statistics["DebugDisplayX"]=v[0].bits;world.statistics["DebugDisplayY"]=v[1].bits;for(int i=2;i<count;++i)world.statistics["DebugDisplayValue"+std::to_string(i-2)]=real(v[i]);return 1;}
    if(command==0x03C7){std::array<Operand,4> v{};for(auto& item:v)if(!read(item))return -1;world.buildingSwapStates[uint32_t(v[0].bits)]=v[3].bits;world.buildingSwapCommands.push_back({uint32_t(v[0].bits),uint32_t(v[3].bits),true});++world.buildingSwapRevision;return 1;}
    if(command==0x03CA){world.statistics["MultiplayerScriptDone"]=1;return 1;}
    if(command==0x03D2)return executeMarkerCommand(vm,command,error);
    if(command==0x03D9){Operand v;if(!read(v))return -1;world.statistics["CrusherRewardMultiplier"]=real(v);return 1;}
    if(command==0x03DB){++world.buildingSwapRevision;return 1;}
    if(command==0x03E6){Operand v;if(!read(v))return -1;world.statistics["DeveloperFlag"]=v.bits;return 1;}
    if(command==0x03FE){Operand enabled,level;if(!read(enabled)||!read(level))return -1;if(level.bits<0||level.bits>=int(world.allowedCollisionLevels.size()))return -1;world.allowedCollisionLevels[size_t(level.bits)]=enabled.bits;return 1;}
    if(command==0x043E||command==0x04BC||command==0x04E3){
        const bool targeted=command!=0x043E;
        std::vector<Operand> values(targeted?11:8);
        for(auto& value:values)if(!read(value))return -1;
        if(!values.back().reference) {error="Script projectile requires a writable handle.";return -1;}
        const int sourceIndex=targeted?8:5;
        StorylandMissionEnvironment::DamageSource source;
        if(values[sourceIndex].bits!=-1)source={command==0x04BC?
            StorylandMissionEnvironment::DamageSourceKind::Vehicle:
            StorylandMissionEnvironment::DamageSourceKind::Character,values[sourceIndex].bits};
        const std::array<float,3> target=targeted?
            std::array<float,3>{real(values[4]),real(values[5]),real(values[6])}:
            std::array<float,3>{};
        int32_t handle=-1;
        if(!storylandCreateVcsScriptProjectile(world,values[0].bits,
            {real(values[1]),real(values[2]),real(values[3])},target,
            real(values[targeted?7:4]),source,values[targeted?9:6].bits,targeted,handle,error))return -1;
        return writeOperand(vm,values.back(),handle)?1:-1;
    }
    if(command==0x043F) {
        Operand handle,explode;if(!read(handle)||!read(explode))return -1;
        return storylandRemoveVcsScriptProjectile(world,handle.bits,explode.bits!=0,error)?1:-1;
    }
    if(command==0x0440) {
        Operand handle,type;if(!read(handle)||!read(type))return -1;
        update(handle.bits>=0&&size_t(handle.bits)<world.scriptProjectiles.size()&&
            world.scriptProjectiles[size_t(handle.bits)].weaponType==type.bits);
        return 1;
    }
    if(command==0x0441) {
        Operand handle;if(!read(handle))return -1;
        update(handle.bits<0||size_t(handle.bits)>=world.scriptProjectiles.size()||
            !world.scriptProjectiles[size_t(handle.bits)].active);
        return 1;
    }
    if(command==0x0478){std::array<Operand,13> v{};for(auto& item:v)if(!read(item))return -1;StorylandMissionEnvironment::ScriptCorona c;c.position={real(v[0]),real(v[1]),real(v[2])};c.color={uint8_t(v[6].bits),uint8_t(v[7].bits),uint8_t(v[8].bits)};c.size=std::max(.01f,std::fabs(real(v[10])));world.frameScriptCoronas[{reinterpret_cast<uintptr_t>(&vm),0x478}]=c;return 1;}
    if(command==0x050B||command==0x050C){world.statistics[command==0x050B?"EmpireAttackUnsuccessful":"EmpireSiteDefendedSuccessfully"]+=1;return 1;}

    // These retail entries are inert/NOP in the supplied VCS command table or have no game-side implementation.
    if(command==0x02AA||command==0x0371||command==0x039D||command==0x039E||command==0x03BA||command==0x03C8||command==0x03CB||command==0x03CE||command==0x044B)return 1;
    if(command==0x039F||command==0x041E||command==0x041F){Operand ignored;if(!read(ignored))return -1;return 1;}
    if(command==0x0504){for(int i=0;i<4;++i){Operand ignored;if(!read(ignored))return -1;}return 1;}
    error="Additional VCS opcode route fell through its implementation.";return -1;
}

int executeVcsAimPadCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    if(!vm.vcs||(command!=0x03E9&&command!=0x0481))return 0;
    if(!vm.environment) {
        error="VCS aim-pad parameters require the live player environment.";
        return -1;
    }
    if(command==0x0481) {
        Operand player,enabled;
        if(!readOperand(vm,player)||!readOperand(vm,enabled)) {
            error="VCS manual-aim reset has truncated operands.";
            return -1;
        }
        if(player.bits!=0) {
            error="VCS manual-aim reset requires the actual player character.";
            return -1;
        }
        vm.environment->manualAim.resetRequested=enabled.bits!=0;
        return 1;
    }
    std::array<float,2> parameters{};
    for(float& parameter:parameters) {
        Operand value;
        if(!readOperand(vm,value)) {
            error="VCS aim-pad parameters are truncated.";
            return -1;
        }
        parameter=operandFloat(value);
        if(!std::isfinite(parameter)) {
            error="VCS aim-pad parameters must be finite.";
            return -1;
        }
    }
    vm.environment->weaponAimPadParameters=parameters;
    return 1;
}

int executeVcsMountedGunCommand(StorylandMissionVm& vm, uint16_t command,
    bool, std::string& error) {
    if (!vm.vcs || command != 0x0505) return 0;
    if (!vm.environment) { error = "Mounted-gun command requires the live world."; return -1; }
    std::array<Operand, 4> values{};
    for (auto& value : values) {
        if (!readOperand(vm, value)) {
            error = "Mounted-gun command has a truncated typed operand.";
            return -1;
        }
    }
    auto& world = *vm.environment;
    const int32_t handle = values[0].bits;
    const auto found = world.missionCharacters.find(handle);
    if (found == world.missionCharacters.end()) {
        error = "Mounted-gun command requires an existing mission character.";
        return -1;
    }
    auto& character = found->second;
    const auto install = [&](int32_t animation, bool initial) {
        StorylandMissionEnvironment::ScriptAnimation association;
        association.group = storylandVcsMountedGunGroup;
        association.animation = animation;
        association.clock = world.gameMilliseconds;
        association.weight = initial ? 1.0f : 0.0f;
        association.blendRate = initial ? 0.0f : 1.0f;
        association.loop = world.animationLoops && world.animationLoops(association.group, animation);
        world.scriptedCharacterAnimations[handle] = association;
        character.animationGroup = association.group;
        character.animationId = animation;
        character.animationStartedAt = world.gameMilliseconds;
        character.animationBlend = association.weight;
        character.animationLoop = association.loop;
    };
    if (!character.mountedGunInitialized) {
        if (!world.ensureAnimationGroup || !world.ensureAnimationGroup(storylandVcsMountedGunGroup, error)) {
            if (error.empty()) error = "The original mounted-gun animation bank is unavailable.";
            return -1;
        }
        const float aimDuration = world.animationDuration ?
            world.animationDuration(storylandVcsMountedGunGroup, storylandVcsMountedGunAimAnimation) : 0;
        const float holdDuration = world.animationDuration ?
            world.animationDuration(storylandVcsMountedGunGroup, storylandVcsMountedGunIdleAnimation) : 0;
        if (!std::isfinite(aimDuration) || aimDuration <= 0 ||
            !std::isfinite(holdDuration) || holdDuration <= 0) {
            error = "The original mounted-gun associations have no decoded timing.";
            return -1;
        }
        world.requestedAnimationGroups.insert(storylandVcsMountedGunGroup);
        world.pedAnimationRequests.erase(handle);
        install(storylandVcsMountedGunIdleAnimation, true);
        character.mountedGunInitialized = true;
    }
    StorylandVcsMountedGunRequest request;
    request.character = handle;
    request.vehicle = values[1].bits != 0;
    request.target = values[2].bits;
    if (values[3].bits == 0) {
        if (character.animationId == storylandVcsMountedGunAimAnimation)
            install(storylandVcsMountedGunIdleAnimation, false);
        world.mountedGunRequests.push_back(request);
        return 1;
    }

    std::array<float, 3> target{};
    std::array<float, 16> targetFrame{};
    bool targetPresent = false;
    if (request.vehicle) {
        const auto vehicle = world.missionVehicles.find(request.target);
        if (vehicle != world.missionVehicles.end() && !vehicle->second.deleted) {
            if (!world.vehicleAttachmentFrame || !world.vehicleAttachmentFrame(request.target, targetFrame)) {
                error = "Mounted-gun target requires the vehicle's actual entity matrix.";
                return -1;
            }
            targetPresent = true;
        }
    } else if (request.target == 0 || world.missionCharacters.count(request.target)) {
        if (!world.characterWorldMatrix || !world.characterWorldMatrix(request.target, targetFrame)) {
            error = "Mounted-gun target requires the character's actual entity matrix.";
            return -1;
        }
        targetPresent = true;
    }
    // Native pool resolution returns null for a missing target; initialization
    // has already happened, and that call neither changes aim nor fires.
    if (!targetPresent) {
        world.mountedGunRequests.push_back(request);
        return 1;
    }
    target = {targetFrame[12], targetFrame[13], targetFrame[14]};
    std::array<float, 16> frame{};
    if (!world.characterWorldMatrix || !world.characterWorldMatrix(handle, frame)) {
        error = "Mounted-gun aiming requires the character's actual entity matrix.";
        return -1;
    }
    auto association = world.scriptedCharacterAnimations.find(handle);
    if (association == world.scriptedCharacterAnimations.end() ||
        association->second.group != storylandVcsMountedGunGroup ||
        association->second.animation != storylandVcsMountedGunAimAnimation) {
        install(storylandVcsMountedGunAimAnimation, false);
        association = world.scriptedCharacterAnimations.find(handle);
    }
    const float duration = world.animationDuration ?
        world.animationDuration(storylandVcsMountedGunGroup, storylandVcsMountedGunAimAnimation) : 0;
    StorylandVcsMountedGunAim aim;
    if (!storylandVcsEvaluateMountedGunAim(frame, target, association->second.time,
        duration, association->second.loop, aim)) {
        error = "Mounted-gun aiming has invalid native transform or animation data.";
        return -1;
    }
    association->second.time = aim.animationTime;
    association->second.clock = world.gameMilliseconds;
    association->second.paused = true;
    if (aim.inFront) {
        request.fire = storylandVcsMountedGunBurst(character.attackTimers,
            world.gameMilliseconds, character.crouch.requested, [&]() { return world.nextNativeRandom(); });
        // 2168E8 temporarily sets the weapon target, then clears it on return.
        world.characterTargets.erase(handle);
    }
    world.mountedGunRequests.push_back(request);
    return 1;
}
