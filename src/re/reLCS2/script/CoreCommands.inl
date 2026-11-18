int executeLcsOnscreenCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    if(vm.vcs)return 0;
    const bool clock=command==0x0153||command==0x03C8;
    const bool counter=command==0x0155||command==0x03C9||command==0x04FC||command==0x0664||command==0x066C;
    const bool clearClock=command==0x0154,clearCounter=command==0x0156;
    const bool freeze=command==0x039B,prefix=command==0x0662;
    if(!clock&&!counter&&!clearClock&&!clearCounter&&!freeze&&!prefix)return 0;
    if(!vm.environment) {error="Onscreen timers require a live mission environment.";return -1;}
    auto& world=*vm.environment;
    Operand variable;
    if(!readOperand(vm,variable)) {error="Onscreen command has a truncated first operand.";return -1;}
    if(freeze) {world.onscreenTimersFrozen=variable.bits!=0;return 1;}
    if(!variable.reference||!variable.global) {
        error="Onscreen timers and counters require a global script variable.";return -1;
    }
    if(clearClock) {
        if(world.onscreenClock.variable==variable.variable)world.onscreenClock={};
        return 1;
    }
    if(clearCounter) {
        for(auto& item:world.lcsOnscreenCounters)if(item.variable==variable.variable)item={};
        return 1;
    }
    Operand type;
    if(!readOperand(vm,type)) {error="Onscreen command has a truncated display type.";return -1;}
    if(prefix) {
        for(auto& item:world.lcsOnscreenCounters)if(item.variable==variable.variable) {
            item.money=type.bits!=0;
            break;
        }
        return 1;
    }
    int32_t slot=0,total=-1,totalType=0;
    if(command==0x04FC) {
        Operand position;
        if(!readOperand(vm,position)||position.bits<1||position.bits>int32_t(world.lcsOnscreenCounters.size())) {
            error="Onscreen counter position must be from one through four.";return -1;
        }
        slot=position.bits-1;
    }
    if(command==0x0664||command==0x066C) {
        total=type.bits;
        if(!readOperand(vm,type)) {error="Onscreen counter has no display type.";return -1;}
        if(command==0x066C) {
            Operand totalStyle;
            if(!readOperand(vm,totalStyle)) {error="Onscreen counter has no total display type.";return -1;}
            totalType=totalStyle.bits;
        }
    }
    std::string label,totalLabel;
    if(command==0x03C8||command==0x03C9||command==0x04FC||command==0x0664||command==0x066C) {
        if(!readScmText(vm,label)) {error="Onscreen command has an invalid text label.";return -1;}
        if(command==0x066C) {
            totalLabel=std::move(label);
            if(!readScmText(vm,label)) {error="Onscreen counter has an invalid second text label.";return -1;}
        }
    }
    if(clock) {
        if(world.onscreenClock.variable>=0)return 1;
        auto& item=world.onscreenClock;
        item.variable=variable.variable;item.countDown=type.bits!=0;item.label=std::move(label);
        if(world.counterColorPending) {
            item.color=world.counterColor;world.counterColorPending=false;
        }
        return 1;
    }
    if(world.lcsOnscreenCounters[size_t(slot)].variable>=0)return 1;
    auto& item=world.lcsOnscreenCounters[size_t(slot)];
    item.variable=variable.variable;item.type=uint16_t(type.bits);item.total=total;item.totalType=uint16_t(totalType);
    item.label=std::move(label);item.totalLabel=std::move(totalLabel);item.money=false;
    if(world.counterColorPending) {item.color=world.counterColor;world.counterColorPending=false;}
    if(world.counterBackgroundPending) {item.background=world.counterBackground;world.counterBackgroundPending=false;}
    return 1;
}

void storylandAdvanceLcsOnscreenTimers(StorylandMissionVm& vm,uint32_t elapsedMilliseconds) {
    if(vm.vcs||!vm.environment)return;
    auto& world=*vm.environment;
    auto& clock=world.onscreenClock;
    if(world.onscreenTimersFrozen||clock.variable<0||size_t(clock.variable)>=vm.globals.size())return;
    auto& value=vm.globals[size_t(clock.variable)];
    if(clock.countDown) {
        const int64_t remaining=int64_t(value)-elapsedMilliseconds;
        value=int32_t(std::max(int64_t(0),remaining));
        if(remaining<0)clock={};
    } else value=int32_t(uint32_t(value)+elapsedMilliseconds);
}

int executeLcsCoreCommand(StorylandMissionVm& vm,uint16_t command,
    uint32_t elapsedMilliseconds,std::string& error) {
    if(vm.vcs)return 0;
    if(command==0x05A6||command==0x05A7||command==0x0659) {
        const size_t inputs=command==0x05A7?6:4;
        std::array<Operand,7> values{};
        for(size_t index=0;index<=inputs;++index)if(!readOperand(vm,values[index])) {
            error="LCS vector command has truncated operands.";return -1;
        }
        if(!values[inputs].reference) {
            error="LCS vector command requires a writable output.";return -1;
        }
        std::array<float,6> coordinates{};
        for(size_t index=0;index<inputs;++index) {
            coordinates[index]=operandFloat(values[index]);
            if(!std::isfinite(coordinates[index])) {
                error="LCS vector command requires finite coordinates.";return -1;
            }
        }
        float result=0;
        if(command==0x0659) {
            constexpr float pi=3.14159265358979323846f;
            const float x=coordinates[2]-coordinates[0];
            float y=coordinates[3]-coordinates[1];
            if(y==0)y=0.0001f;
            const float direction=std::atan2(x/y,1.0f);
            const float radians=y>0?(x>0?pi-direction:-(pi+direction)):-direction;
            result=radians*180.0f/pi;
        } else {
            const size_t dimensions=inputs/2;
            result=coordinates[0]*coordinates[dimensions];
            for(size_t axis=1;axis<dimensions;++axis)
                result+=coordinates[axis]*coordinates[axis+dimensions];
        }
        if(!std::isfinite(result)) {error="LCS vector command produced a non-finite result.";return -1;}
        int32_t bits=0;
        std::memcpy(&bits,&result,sizeof(bits));
        if(!writeOperand(vm,values[inputs],bits)) {
            error="LCS vector command could not write its output.";return -1;
        }
        return 1;
    }
    const int onscreen=executeLcsOnscreenCommand(vm,command,error);
    if(onscreen)return onscreen;
    if(command==0x02AF) {
        Operand handle,enabled;
        if(!readOperand(vm,handle)||!readOperand(vm,enabled)||!vm.environment) {
            error="SET_CAR_ONLY_DAMAGED_BY_PLAYER requires a vehicle, flag and live world.";return -1;
        }
        const auto found=vm.environment->missionVehicles.find(handle.bits);
        if(found==vm.environment->missionVehicles.end()||found->second.deleted) {
            error="SET_CAR_ONLY_DAMAGED_BY_PLAYER references an unknown vehicle.";return -1;
        }
        found->second.onlyDamagedByPlayer=enabled.bits!=0;
        return 1;
    }
    if(command==0x00AE) {
        Operand handle,style;
        if(!readOperand(vm,handle)||!readOperand(vm,style)||!vm.environment) {
            error="SET_CAR_DRIVING_STYLE requires a vehicle, style and live world.";return -1;
        }
        const auto found=vm.environment->missionVehicles.find(handle.bits);
        if(found==vm.environment->missionVehicles.end()||found->second.deleted) {
            error="SET_CAR_DRIVING_STYLE references an unknown vehicle.";return -1;
        }
        if(vm.environment->vehicleIsBoat&&vm.environment->vehicleIsBoat(handle.bits)) {
            error="SET_CAR_DRIVING_STYLE has no verified boat traffic-style consumer.";return -1;
        }
        const uint8_t nativeStyle=uint8_t(style.bits);
        if(nativeStyle!=0&&nativeStyle!=1&&nativeStyle!=4) {
            error="SET_CAR_DRIVING_STYLE supports native stop, slow and ignore-lights styles; weave and plough steering are unavailable.";return -1;
        }
        found->second.drivingStyle=nativeStyle;
        return 1;
    }
    if(command==0x0248) {
        Operand handle,personality;
        if(!readOperand(vm,handle)||!readOperand(vm,personality)||!vm.environment) {
            error="SET_CHAR_PERSONALITY requires a character, native stat index and live world.";return -1;
        }
        auto& world=*vm.environment;
        if(handle.bits==1) {
            error="SET_CHAR_PERSONALITY has no verified player-personality consumer.";return -1;
        }
        const auto found=world.missionCharacters.find(handle.bits);
        if(found==world.missionCharacters.end()) {
            error="SET_CHAR_PERSONALITY references an unknown character.";return -1;
        }
        if(personality.bits<0||personality.bits>=42||!world.resolvePedStats) {
            error="SET_CHAR_PERSONALITY requires the native pedestrian-stat table and an index from zero through forty-one.";return -1;
        }
        StorylandPedStats stats;
        if(!world.resolvePedStats(found->second.model,personality.bits,stats)||!storylandValidLcsPedStats(stats)||
            stats.statIndex!=personality.bits||stats.modelId!=uint32_t(found->second.model)) {
            error="SET_CHAR_PERSONALITY could not resolve the native pedestrian-stat record.";return -1;
        }
        // SetPedStats changes the stat pointer only. It does not replace the
        // model, pedestrian type, health, task or independently selected animations.
        found->second.pedStats=std::move(stats);return 1;
    }
    if(command==0x0329) {
        if(!vm.environment) {error="SET_ZONE_GROUP requires a live mission environment.";return -1;}
        std::string zone;Operand day,group;
        if(!readScmText(vm,zone)||zone.empty()||!readOperand(vm,day)||!readOperand(vm,group)) {
            error="SET_ZONE_GROUP has a truncated label, day flag, or group.";return -1;
        }
        auto& groups=vm.environment->zonePedGroups;
        const auto key=std::make_pair(zone,int32_t(uint8_t(day.bits)!=0));
        if(groups.size()>=4096&&!groups.count(key)) {
            error="SET_ZONE_GROUP exceeded the zone group table.";return -1;
        }
        // Native SetPedGroup stores the uint16 value. Population generation
        // checks the decoded table before selecting any model from it.
        groups[key]=uint16_t(group.bits);
        return 1;
    }
    const bool timed=command>=0x0078&&command<=0x0083;
    const bool absolute=command>=0x0094&&command<=0x0097;
    const bool random=command==0x0098||command==0x0099||command==0x020D||command==0x020E;
    const bool feet=command==0x042A||command==0x0432;
    if(!timed&&!absolute&&!random&&!feet)return 0;
    const size_t count=timed||feet?2:absolute||command==0x0098||command==0x0099?1:3;
    std::array<Operand,3> values{};
    for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="LCS numeric command has a truncated operand.";return -1;
    }
    const size_t output=timed||absolute?0:count-1;
    if(!values[output].reference) {error="LCS numeric command requires a writable output.";return -1;}
    const auto storeFloat=[&](double number) {
        const float value=float(number);
        if(!std::isfinite(value)) {error="LCS numeric command produced a non-finite result.";return -1;}
        int32_t bits=0;std::memcpy(&bits,&value,sizeof(bits));
        return writeOperand(vm,values[output],bits)?1:-1;
    };
    if(timed) {
        const float step = vm.environment ? vm.environment->nativeScriptTimeStep :
            std::clamp(float(elapsedMilliseconds) * 0.05f, 0.5f, 3.0f);
        const float destination = operandFloat(values[0]);
        const float rate = operandFloat(values[1]);
        float result = 0;
        if (!std::isfinite(destination) || !std::isfinite(rate) || !std::isfinite(step) ||
            !tryTimedEeArithmetic(destination, rate, step, command >= 0x007E, result)) {
            error = "LCS timed arithmetic exceeds the supported finite EE range; destination unchanged.";
            return -1;
        }
        int32_t bits = 0;
        std::memcpy(&bits, &result, sizeof(bits));
        return writeOperand(vm, values[0], bits) ? 1 : -1;
    }
    if(absolute) {
        if(command>=0x0096)return storeFloat(std::fabs(operandFloat(values[0])));
        const int32_t source=values[0].bits;
        const uint32_t result=source<0?0u-uint32_t(source):uint32_t(source);
        return writeOperand(vm,values[0],int32_t(result))?1:-1;
    }
    if(feet) {
        if(command==0x042A)return storeFloat(double(operandFloat(values[0]))/0.3048);
        const double result=double(values[0].bits)/0.3048;
        if(result<double(INT32_MIN)||result>double(INT32_MAX)) {
            error="Metres-to-feet conversion exceeds the integer range.";return -1;
        }
        return writeOperand(vm,values[1],int32_t(result))?1:-1;
    }
    if(command==0x020D&&(!std::isfinite(operandFloat(values[0]))||
        !std::isfinite(operandFloat(values[1])))) {
        error="Random float bounds must be finite.";return -1;
    }
    vm.randomState=vm.randomState*1664525u+1013904223u;
    const uint32_t number=vm.randomState>>16;
    const double fraction=double(number)/65536.0;
    if(command==0x0099)return writeOperand(vm,values[0],int32_t(number))?1:-1;
    if(command==0x0098)return storeFloat(fraction);
    if(command==0x020D)return storeFloat(double(operandFloat(values[0]))+
        (double(operandFloat(values[1]))-operandFloat(values[0]))*fraction);
    const double result=double(values[0].bits)+
        double(int64_t(values[1].bits)-int64_t(values[0].bits))*fraction;
    return writeOperand(vm,values[2],int32_t(result))?1:-1;
}

int executeLcsCollectableCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    if(vm.vcs||(command!=0x02F1&&command!=0x02F2&&command!=0x03E6))return 0;
    if(!vm.environment) {error="LCS collectable command requires a live world.";return -1;}
    auto& world=*vm.environment;
    std::array<Operand,3> values{};
    const size_t count=command==0x02F1?3:1;
    for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="LCS collectable command has a truncated operand.";return -1;
    }
    if(command==0x02F2) {
        world.statistics["TotalPackages"]=std::max(0,values[0].bits);return 1;
    }
    if(command==0x03E6) {
        const auto found=world.statistics.find("CollectedPackages");
        const double count=found==world.statistics.end()?0:found->second;
        if(!std::isfinite(count)||!writeOperand(vm,values[0],int32_t(std::clamp(count,0.0,double(INT32_MAX))))) {
            error="GET_COLLECTABLE1S_COLLECTED requires a writable output.";return -1;
        }
        return 1;
    }
    StorylandMissionEnvironment::Pickup pickup;
    pickup.x=operandFloat(values[0]);pickup.y=operandFloat(values[1]);pickup.z=operandFloat(values[2]);
    if(!std::isfinite(pickup.x)||!std::isfinite(pickup.y)||!std::isfinite(pickup.z)) {
        error="CREATE_COLLECTABLE1 has a non-finite position.";return -1;
    }
    const int32_t model=world.modelIdByName?world.modelIdByName("package1"):-1;
    if(model<0) {error="CREATE_COLLECTABLE1 requires the native package1 model.";return -1;}
    if(pickup.z<=-100.0f) {
        float height=0;
        if(!world.groundHeightAt||!world.groundHeightAt({pickup.x,pickup.y,1000.0f},height)||!std::isfinite(height)) {
            error="CREATE_COLLECTABLE1 requires native ground at its placement.";return -1;
        }
        pickup.z=height+0.5f;
    }
    const auto reusable=std::find_if(world.pickups.begin(),world.pickups.end(),[](const auto& item) {
        return item.removed||(item.unavailable&&!item.collected&&item.respawnAt==0);
    });
    if((world.pickups.size()>=4096&&reusable==world.pickups.end())||world.nextPickupHandle==INT32_MAX) {
        error="CREATE_COLLECTABLE1 exceeded the native pickup pool.";return -1;
    }
    pickup.handle=world.nextPickupHandle++;pickup.model=model;pickup.type=6;
    pickup.area=world.area;pickup.modelNameOverride="package1";
    world.requestedModels.insert(model);
    if(reusable==world.pickups.end())world.pickups.push_back(std::move(pickup));
    else *reusable=std::move(pickup);
    return 1;
}

int executeLcsGarageCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(vm.vcs||(command!=0x0668&&command!=0x0365&&command!=0x0366&&command!=0x03B5&&command!=0x03B6))return 0;
    if(!vm.environment) {error="LCS garage command requires a live environment.";return -1;}
    auto& world=*vm.environment;Operand handle,value;
    if(!readOperand(vm,handle)||(command==0x0668&&!readOperand(vm,value))) {
        error="LCS garage command has truncated operands.";return -1;
    }
    const auto garage=std::find_if(world.garages.begin(),world.garages.end(),
        [&](const auto& item){return item.handle==handle.bits;});
    if(garage==world.garages.end()) {error="LCS garage command references an unknown garage.";return -1;}
    auto found=world.lcsGarageDoors.find(handle.bits);
    if(found==world.lcsGarageDoors.end()) {
        StorylandMissionEnvironment::LcsGarageDoorState prepared;
        if(!world.prepareLcsGarageDoors||!world.applyLcsGarageDoorTransform) {
            error="LCS garage command requires native door bindings.";return -1;
        }
        if(!world.prepareLcsGarageDoors(*garage,prepared,error))return -1;
        found=world.lcsGarageDoors.emplace(handle.bits,std::move(prepared)).first;
    }
    auto& state=found->second;
    if(command==0x03B5||command==0x03B6) {
        updateScmCondition(vm,state.state==(command==0x03B5?1:0),inverted);return 1;
    }
    if(command==0x0668) {
        garage->locked=value.bits!=0;
        if(garage->locked) {state.position=0;state.state=0;state.actionTime=world.gameMilliseconds+2000;}
        else state.state=3;
    } else if(command==0x0365) {
        if(state.state==0||state.state==2)state.state=3;
    } else if(state.state==1||state.state==3)state.state=2;
    return storylandApplyLcsGarageDoors(world,*garage,state,error)?1:-1;
}

int executeLcsStatePredicate(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(vm.vcs)return 0;
    const bool navigation=command==0x0126||command==0x0159;
    const bool information=command==0x0588;
    const bool appearance=command==0x047F||command==0x0483||command==0x04CE;
    const bool characterQuery=command==0x0159||command==0x047F;
    if(!navigation&&!information&&!appearance&&command!=0x02E3&&command!=0x0410&&
        command!=0x0411&&command!=0x044A)return 0;
    if(!vm.environment) {error="LCS state predicate requires a live environment.";return -1;}
    const auto& world=*vm.environment;
    if(command==0x0410||command==0x0411) {
        updateScmCondition(vm,command==0x0410?world.frenchGame:world.germanGame,inverted);
        return 1;
    }
    if(command==0x044A) {
        updateScmCondition(vm,std::any_of(world.vehicleCheats.begin(),world.vehicleCheats.end(),
            [](bool active){return active;}),inverted);
        return 1;
    }
    Operand handle;
    if(!readOperand(vm,handle)||(!characterQuery&&handle.bits!=0)) {
        error="LCS player predicate requires player zero.";return -1;
    }
    if(command==0x02E3||appearance) {
        bool occupied=world.inVehicle;
        int32_t vehicleHandle=world.occupiedMissionVehicle,model=world.occupiedVehicleModel;
        if(command==0x047F&&handle.bits!=1) {
            const auto character=world.missionCharacters.find(handle.bits);
            if(character==world.missionCharacters.end()) {
                error="IS_CHAR_ON_ANY_BIKE references a missing character.";return -1;
            }
            vehicleHandle=character->second.vehicle;
            occupied=vehicleHandle>=0&&!character->second.attached;model=-1;
        }
        bool matches=false;
        if(occupied) {
            if(vehicleHandle>=0) {
                const auto vehicle=world.missionVehicles.find(vehicleHandle);
                if(vehicle==world.missionVehicles.end()||vehicle->second.deleted) {
                    error="LCS vehicle predicate references a missing occupied vehicle.";return -1;
                }
                model=vehicle->second.model;
            }
            if(model<0) {error="LCS vehicle predicate requires the occupied vehicle's native model.";return -1;}
            if(command==0x02E3) {
                // Native LCS TAXI, CABBIE and BORGNINE. The leftover negative
                // Vice City KAUFMAN constant cannot identify a loaded LCS model.
                matches=model==151||model==166||model==181;
            } else {
                uint32_t flags=0;
                if(!world.vehicleHandlingFlags||!world.vehicleHandlingFlags(model,flags)) {
                    error="LCS vehicle appearance predicate requires the native handling record.";return -1;
                }
                // GetVehicleAppearance uses the exact masked handling flag,
                // not the runtime/model subclass. Multiple type bits are NONE.
                const uint32_t type=flags&0xF0000u;
                matches=command==0x04CE?(type==0x20000u||type==0x40000u):type==0x10000u;
            }
        }
        updateScmCondition(vm,matches,inverted);return 1;
    }
    std::string name;
    if(!readScmText(vm,name)||name.empty()) {
        error="LCS zone predicate requires a complete zone label.";return -1;
    }
    if(!world.zones||world.zones->vcs||
        (information?world.zones->information:world.zones->navigation).empty()) {
        error="LCS zone predicate requires the loaded native zone table.";return -1;
    }
    StorylandNativePoint position{world.x,world.y,world.z};
    int32_t vehicleHandle=world.inVehicle?world.occupiedMissionVehicle:-1;
    if(command==0x0159&&handle.bits!=1) {
        const auto character=world.missionCharacters.find(handle.bits);
        if(character==world.missionCharacters.end()) {
            error="IS_CHAR_IN_ZONE references a missing character.";return -1;
        }
        position={character->second.x,character->second.y,character->second.z};
        vehicleHandle=character->second.attached?-1:character->second.vehicle;
    }
    if(vehicleHandle>=0) {
        const auto vehicle=world.missionVehicles.find(vehicleHandle);
        if(vehicle==world.missionVehicles.end()||vehicle->second.deleted) {
            error="LCS zone predicate references a missing occupied vehicle.";return -1;
        }
        position={vehicle->second.x,vehicle->second.y,vehicle->second.z};
    }
    if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z)) {
        error="LCS zone predicate requires a finite entity position.";return -1;
    }
    // Named navigation queries check that zone's inclusive box even when a
    // more specific child also contains the entity. Information queries use
    // the native first-containing-row selection, including its root fallback.
    const auto* zone=information?world.zones->at(position,true):world.zones->named(name);
    if(!zone) {error="LCS zone predicate references an unknown navigation zone.";return -1;}
    updateScmCondition(vm,information?zone->name==name:zone->contains(position),inverted);
    return 1;
}

int executeLcsWorldCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(vm.vcs)return 0;
    const int predicate=executeLcsStatePredicate(vm,command,inverted,error);
    if(predicate)return predicate;
    if(command==0x0669) {
        // IS_FINAL_GAME describes the retail LCS target build, independently
        // of the editor's own debug configuration. Native FINAL returns true.
        updateScmCondition(vm,true,inverted);return 1;
    }
    const int garage=executeLcsGarageCommand(vm,command,inverted,error);
    if(garage)return garage;
    const bool special=command==0x0241||command==0x02F8;
    const bool specialLoaded=command==0x0242,specialRelease=command==0x029B;
    const bool phone=command==0x024F,phoneState=command==0x0253||command==0x040A;
    const bool remember=command>=0x0256&&command<=0x0259;
    if(!special&&!specialLoaded&&!specialRelease&&!phone&&!phoneState&&!remember&&
        command!=0x05F0&&command!=0x0619&&command!=0x0671&&command!=0x03CB&&command!=0x03BB&&
        command!=0x04E9&&command!=0x066A&&command!=0x04F1&&
        command!=0x042B&&command!=0x042D&&command!=0x039E&&
        !(command>=0x0635&&command<=0x0638))return 0;
    if(!vm.environment) {error="LCS world command requires a live environment.";return -1;}
    auto& world=*vm.environment;
    if(command==0x039E) {
        StorylandRoadChange change;change.mode=StorylandRoadChange::Mode::AngledAvailability;
        for(float& coordinate:change.bounds) {
            Operand value;
            if(!readOperand(vm,value)||!std::isfinite(coordinate=operandFloat(value))) {
                error="SWITCH_PED_ROADS_OFF_ANGLED requires finite baseline coordinates.";return -1;
            }
        }
        Operand width;
        if(!readOperand(vm,width)||!std::isfinite(change.width=operandFloat(width))||
            !std::isfinite(std::hypot(change.bounds[3]-change.bounds[0],change.bounds[4]-change.bounds[1]))||
            std::hypot(change.bounds[3]-change.bounds[0],change.bounds[4]-change.bounds[1])==0) {
            error="SWITCH_PED_ROADS_OFF_ANGLED requires a finite width and a nonzero XY baseline.";return -1;
        }
        if(!world.pedestrianPaths||world.pedestrianPaths->vcs) {
            error="SWITCH_PED_ROADS_OFF_ANGLED requires the native LCS path graph.";return -1;
        }
        if(change.bounds[2]>change.bounds[5])std::swap(change.bounds[2],change.bounds[5]);
        if(world.roadChanges.size()>=4096) {error="Road change history limit reached.";return -1;}
        world.roadChanges.push_back(change);return 1;
    }
    if(command==0x042B) {
        StorylandRoadChange change;change.mode=StorylandRoadChange::Mode::LevelTransitions;change.vehicle=true;
        for(float& coordinate:change.bounds) {
            Operand value;
            if(!readOperand(vm,value)||!std::isfinite(coordinate=operandFloat(value))) {
                error="MARK_ROADS_BETWEEN_LEVELS requires six finite coordinates.";return -1;
            }
        }
        if(!world.pedestrianPaths||world.pedestrianPaths->vcs) {
            error="MARK_ROADS_BETWEEN_LEVELS requires the native LCS path graph.";return -1;
        }
        for(size_t index=0;index<3;++index)if(change.bounds[index]>change.bounds[index+3])
            std::swap(change.bounds[index],change.bounds[index+3]);
        if(world.roadChanges.size()>=4096) {error="Road change history limit reached.";return -1;}
        world.roadChanges.push_back(change);return 1;
    }
    if(command==0x042D) {
        Operand handle,enabled;
        if(!readOperand(vm,handle)||!readOperand(vm,enabled)) {
            error="SET_CAR_AVOID_LEVEL_TRANSITIONS has truncated operands.";return -1;
        }
        const auto found=world.missionVehicles.find(handle.bits);
        if(found==world.missionVehicles.end()||found->second.deleted) {
            error="SET_CAR_AVOID_LEVEL_TRANSITIONS requires a live script vehicle.";return -1;
        }
        found->second.avoidLevelTransitions=enabled.bits!=0;return 1;
    }
    if(command==0x04F1) {
        std::string zone;
        std::array<Operand,12> values{};
        if(!readScmText(vm,zone)) {
            error="SET_ZONE_CIVILIAN_CAR_INFO has a truncated zone label.";return -1;
        }
        for(auto& value:values)if(!readOperand(vm,value)) {
            error="SET_ZONE_CIVILIAN_CAR_INFO has truncated car and boat densities.";return -1;
        }
        if(!world.zones||world.zones->vcs) {
            error="SET_ZONE_CIVILIAN_CAR_INFO requires the native LCS information zones.";return -1;
        }
        const bool found=std::any_of(world.zones->information.begin(),world.zones->information.end(),
            [&](const auto& entry){return entry.name==zone;});
        if(!found)return 1;
        const auto key=std::make_pair(zone,int32_t(uint8_t(values[0].bits)!=0));
        if(!world.zoneCivilianCarInformation.count(key)&&world.zoneCivilianCarInformation.size()>=4096) {
            error="SET_ZONE_CIVILIAN_CAR_INFO has a full zone table.";return -1;
        }
        std::array<int32_t,13> densities{};
        for(size_t index=0;index<11;++index) {
            const uint16_t narrowed=uint16_t(values[index+1].bits);
            densities[index]=narrowed<0x8000u?int32_t(narrowed):int32_t(narrowed)-0x10000;
        }
        world.zoneCivilianCarInformation[key]=densities;
        return 1;
    }
    if(command>=0x0635&&command<=0x0638) {
        if(command==0x0635) {
            Operand group,state;
            if(!readOperand(vm,group)||!readOperand(vm,state)||group.bits<0||group.bits>255||state.bits<0||state.bits>255) {
                error="SWAP_BUILDINGS requires a byte-sized slot and state.";return -1;
            }
            world.lcsBuildingSwapGroups[uint32_t(group.bits)]=state.bits;
            world.buildingSwapCommands.push_back({uint32_t(group.bits),uint32_t(state.bits),true});
            ++world.buildingSwapRevision;return 1;
        }
        if(!world.worldStreamingComplete) {
            error="LCS world streaming command requires a live world-geometry binding.";return -1;
        }
        const bool complete=world.worldStreamingComplete();
        if(command!=0x0638)updateScmCondition(vm,complete,inverted);
        return 1;
    }
    if(command==0x04E9||command==0x066A) {
        Operand first,second;
        if(!readOperand(vm,first)||!readOperand(vm,second)) {
            error="LCS collision command has truncated operands.";return -1;
        }
        if(command==0x066A) {
            if(first.bits<0||first.bits>3||second.bits<0||second.bits>3) {
                error="Allowed collision levels must be from zero through three.";return -1;
            }
            world.allowedCollisionLevels={first.bits,second.bits};return 1;
        }
        const std::array<float,2> position{operandFloat(first),operandFloat(second)};
        if(!std::isfinite(position[0])||!std::isfinite(position[1])||!world.requestLcsCollision) {
            error="REQUEST_COLLISION requires finite coordinates and a native collision binding.";return -1;
        }
        return world.requestLcsCollision(position,error)?1:-1;
    }
    if(command==0x03BB) {
        std::array<Operand,6> values{};
        for(auto& value:values)if(!readOperand(vm,value)) {
            error="SWAP_NEAREST_BUILDING_MODEL has truncated operands.";return -1;
        }
        std::array<float,3> position{operandFloat(values[0]),operandFloat(values[1]),operandFloat(values[2])};
        const float radius=operandFloat(values[3]);
        for(float coordinate:position)if(!std::isfinite(coordinate)) {
            error="Building swap position must be finite.";return -1;
        }
        if(!std::isfinite(radius)||radius<0||!world.swapNearestBuilding) {
            error="Building swap requires a nonnegative radius and native world binding.";return -1;
        }
        if(position[2]<=-100.0f) {
            float height=0;
            if(!world.groundHeightAt||!world.groundHeightAt({position[0],position[1],1000.0f},height)||!std::isfinite(height)) {
                error="Building swap could not resolve the ground height.";return -1;
            }
            position[2]=height;
        }
        return world.swapNearestBuilding(position,radius,values[4].bits,values[5].bits,error)?1:-1;
    }
    if(remember) {
        if(command==0x0256) {world.storedWeather=world.forcedWeather;world.weatherStored=true;}
        else if(command==0x0257&&world.weatherStored) {
            world.forcedWeather=world.storedWeather;world.weatherChanged=true;
        } else if(command==0x0258) {world.storedClockHour=world.hour;world.clockStored=true;}
        else if(command==0x0259&&world.clockStored)world.hour=world.storedClockHour;
        return 1;
    }
    if(command==0x0671) {
        std::array<float,3> direction{};
        for(float& coordinate:direction) {
            Operand value;
            if(!readOperand(vm,value)||!std::isfinite(coordinate=operandFloat(value))) {
                error="Extra-colour light direction requires three finite coordinates.";return -1;
            }
        }
        const double length=std::hypot(double(direction[0]),double(direction[1]),double(direction[2]));
        if(length>0)for(float& coordinate:direction)coordinate=float(double(coordinate)/length);
        world.extraLightDirection=direction;world.extraLightDirectionSet=length>0;
        return 1;
    }
    if(phone) {
        Operand x,y,output;
        if(!readOperand(vm,x)||!readOperand(vm,y)||!readOperand(vm,output)||!output.reference||
            !std::isfinite(operandFloat(x))||!std::isfinite(operandFloat(y))) {
            error="GRAB_PHONE requires finite coordinates and a writable handle.";return -1;
        }
        if(!world.findPayphone) {error="GRAB_PHONE requires the native payphone placements.";return -1;}
        if(world.payphones.size()>=64) {error="Script payphone pool is full.";return -1;}
        std::set<uint64_t> claimed;
        for(const auto& item:world.payphones)claimed.insert(item.second.worldKey);
        StorylandMissionEnvironment::Payphone found;
        if(!world.findPayphone({operandFloat(x),operandFloat(y)},claimed,found.worldKey,found.position)) {
            error="GRAB_PHONE found no unclaimed payphone within the native search radius.";return -1;
        }
        for(float coordinate:found.position)if(!std::isfinite(coordinate)) {
            error="Payphone placement contains a non-finite coordinate.";return -1;
        }
        int32_t handle=0;
        while(world.payphones.count(handle))++handle;
        world.payphones.emplace(handle,found);
        return writeOperand(vm,output,handle)?1:-1;
    }
    Operand value;
    if(!readOperand(vm,value)) {error="LCS world command has a truncated operand.";return -1;}
    if(command==0x03CB) {
        // LCS tests the current island level, not a model's request flag.
        // Native zone data supplies that level; an absent world is not resident.
        const bool loaded=world.zones&&!world.zones->navigation.empty();
        const int32_t detected=loaded?int32_t(world.zones->levelAt({world.x,world.y,world.z})):0;
        const int32_t level=detected?detected:world.lcsCollisionLevel;
        updateScmCondition(vm,loaded&&level==value.bits,inverted);
        return 1;
    }
    if(command==0x0619) {
        if(!value.reference||!value.global) {error="Fort Staunton flag must be a global variable.";return -1;}
        world.fortStauntonDestroyedGlobal=value.variable;world.fortStauntonDestroyed=value.bits==1;
        return 1;
    }
    if(command==0x05F0) {
        world.statistics["CollectedPackages"]=std::max(0,value.bits);
        return 1;
    }
    if(phoneState) {
        const auto found=world.payphones.find(value.bits);
        if(found==world.payphones.end()) {error="Phone command references an unknown payphone.";return -1;}
        if(command==0x040A) {
            error="TURN_PHONE_ON requires a verified native payphone render and ringing binding.";
            return -1;
        }
        found->second.enabled=false;return 1;
    }
    const bool characterSlot=command!=0x02F8;
    if(characterSlot&&(value.bits<1||value.bits>21)) {
        error="LCS special character slot must be from one through twenty-one.";return -1;
    }
    if(specialLoaded||specialRelease) {
        const auto slot=world.specialCharacterSlots.find(value.bits);
        const bool loaded=slot!=world.specialCharacterSlots.end()&&world.residentModels.count(slot->second)!=0;
        if(specialLoaded)updateScmCondition(vm,loaded,inverted);
        else if(slot!=world.specialCharacterSlots.end())world.requestedModels.erase(slot->second);
        return 1;
    }
    std::string name;
    if(!readScmText(vm,name)||name.empty()) {error="Special model request has an invalid model name.";return -1;}
    for(char& character:name)character=char(std::tolower(static_cast<unsigned char>(character)));
    int32_t model=value.bits;
    if(characterSlot) {
        std::ostringstream slotName;
        slotName<<"special"<<std::setw(2)<<std::setfill('0')<<value.bits;
        model=world.modelIdByName?world.modelIdByName(slotName.str()):-1;
        if(model<0)model=108+value.bits;
        world.specialCharacterSlots[value.bits]=model;
    }
    if(model<0) {error="Special model request requires a native model ID.";return -1;}
    const auto existing=world.specialModelNames.find(model);
    if(existing==world.specialModelNames.end()||existing->second!=name) {
        world.specialModelNames[model]=std::move(name);
        world.residentModels.erase(model);world.changedSpecialModels.insert(model);
    }
    world.requestedModels.insert(model);
    return 1;
}

int executeLcsEffectCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(vm.vcs)return 0;
    const bool corona=command==0x0254||command==0x05C9;
    const bool removeCorona=command==0x05CA;
    const bool startFire=command==0x02D4||command==0x032A||command==0x032B;
    const bool fireQuery=command==0x02D5,removeFire=command==0x02D6,clearFires=command==0x031F;
    const bool loadScene=command==0x03D0;
    const bool visibility=command==0x0368;
    if(!corona&&!removeCorona&&!startFire&&!fireQuery&&!removeFire&&!clearFires&&!loadScene&&!visibility)return 0;
    if(!vm.environment) {error="LCS world effects require a live mission environment.";return -1;}
    auto& world=*vm.environment;
    if(clearFires) {world.scriptFires.clear();return 1;}
    std::array<Operand,10> values{};
    const size_t count=visibility?6:loadScene?3:corona?(command==0x05C9?10:9):startFire?(command==0x02D4?4:2):1;
    for(size_t i=0;i<count;++i)if(!readOperand(vm,values[i])) {
        error="LCS world-effect command has truncated operands.";return -1;
    }
    if(loadScene) {
        const std::array<float,3> position{operandFloat(values[0]),operandFloat(values[1]),operandFloat(values[2])};
        for(float value:position)if(!std::isfinite(value)) {error="LOAD_SCENE requires finite coordinates.";return -1;}
        if(!world.loadSceneCollision) {error="LOAD_SCENE requires a native collision streaming binding.";return -1;}
        return world.loadSceneCollision(position,error)?1:-1;
    }
    if(removeCorona) {world.scriptCoronas.erase(values[0].bits);return 1;}
    const auto ground=[&](std::array<float,3>& position) {
        for(float coordinate:position)if(!std::isfinite(coordinate)) {
            error="LCS world effects require finite coordinates.";return false;
        }
        if(position[2]<=-100.0f) {
            float height=0;
            if(!world.groundHeightAt||!world.groundHeightAt({position[0],position[1],1000.0f},height)||!std::isfinite(height)) {
                error="LCS world-effect ground sentinel requires decoded ground collision.";return false;
            }
            position[2]=height;
        }
        return true;
    };
    if(visibility) {
        std::array<float,3> position{operandFloat(values[0]),operandFloat(values[1]),operandFloat(values[2])};
        const float radius=operandFloat(values[3]);
        if(!std::isfinite(radius)||radius<0||!world.setClosestObjectVisibility) {
            error="Closest-object visibility requires a finite nonnegative range and native world binding.";return -1;
        }
        if(!ground(position))return -1;
        return world.setClosestObjectVisibility(position,radius,values[4].bits,values[5].bits!=0,error)?1:-1;
    }
    if(corona) {
        if((command==0x05C9&&!values[9].reference)||(command==0x0254&&!values[0].reference)) {
            error="CREATE_SCRIPT_CORONA needs a writable handle; DRAW_CORONA needs a position variable identity.";return -1;
        }
        StorylandMissionEnvironment::ScriptCorona item;
        item.position={operandFloat(values[0]),operandFloat(values[1]),operandFloat(values[2])};
        item.size=operandFloat(values[3]);item.type=values[4].bits;item.flareType=values[5].bits;
        if(!std::isfinite(item.size)||item.type<0||item.type>8||item.flareType<0||item.flareType>2) {
            error="Script corona needs a finite size, native texture type 0–8 and flare type 0–2.";return -1;
        }
        for(size_t i=0;i<3;++i)item.color[i]=uint8_t(values[6+i].bits);
        if(!ground(item.position))return -1;
        if(command==0x0254) {
            const auto key=std::make_pair(values[0].global?uintptr_t(0):reinterpret_cast<uintptr_t>(&vm),values[0].variable);
            if(!world.frameScriptCoronas.count(key)&&world.frameScriptCoronas.size()>=4096) {
                error="Per-frame script corona limit exceeded.";return -1;
            }
            world.frameScriptCoronas[key]=item;
        } else {
            if(world.scriptCoronas.size()>=4096||world.nextScriptCoronaId<1||world.nextScriptCoronaId==INT32_MAX) {
                error="Script corona pool is full.";return -1;
            }
            const int32_t handle=world.nextScriptCoronaId;
            if(!writeOperand(vm,values[9],handle)) {error="Script corona output cannot be written.";return -1;}
            world.scriptCoronas.emplace(handle,item);++world.nextScriptCoronaId;
        }
        return 1;
    }
    if(fireQuery||removeFire) {
        const int32_t handle=int16_t(values[0].bits);
        if(handle<0||handle>=40) {error="Script fire handle is outside the native 40-slot pool.";return -1;}
        if(removeFire)world.scriptFires.erase(handle);
        else {
            const auto fire=world.scriptFires.find(handle);
            updateScmCondition(vm,fire==world.scriptFires.end()||!fire->second.active,inverted);
        }
        return 1;
    }
    if(!values[count-1].reference) {error="Starting a script fire requires a writable handle.";return -1;}
    StorylandMissionEnvironment::ScriptFire item;
    if(command==0x02D4) {
        item.position={operandFloat(values[0]),operandFloat(values[1]),operandFloat(values[2])};
        if(!ground(item.position))return -1;
    } else {
        item.attachedKind=command==0x032A?2:1;item.attachedHandle=values[0].bits;
        if(item.attachedKind==2) {
            const auto vehicle=world.missionVehicles.find(item.attachedHandle);
            if(vehicle==world.missionVehicles.end()||vehicle->second.deleted) {error="START_CAR_FIRE references an unknown vehicle.";return -1;}
            item.position={vehicle->second.x,vehicle->second.y,vehicle->second.z};
        } else if(item.attachedHandle==1)item.position={world.x,world.y,world.z};
        else {
            const auto character=world.missionCharacters.find(item.attachedHandle);
            if(character==world.missionCharacters.end()) {error="START_CHAR_FIRE references an unknown character.";return -1;}
            item.position={character->second.x,character->second.y,character->second.z};
        }
    }
    int32_t handle=0;
    while(handle<40&&world.scriptFires.count(handle))++handle;
    if(handle==40) {error="The native 40-slot script fire pool is full.";return -1;}
    if(!writeOperand(vm,values[count-1],handle)) {error="Script fire output cannot be written.";return -1;}
    if(item.attachedKind)for(auto& existing:world.scriptFires)
        if(existing.second.attachedKind==item.attachedKind&&existing.second.attachedHandle==item.attachedHandle)
            existing.second.active=false;
    world.scriptFires.emplace(handle,item);return 1;
}

int executeLcsCraneCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(vm.vcs||(command!=0x01F3&&command!=0x01F4&&command!=0x0300&&
        command!=0x036D&&command!=0x03A5&&command!=0x03F1))return 0;
    if(!vm.environment) {error="Crane commands require the live LCS world.";return -1;}
    auto& system=vm.environment->lcsCranes;
    if(command==0x03F1) {
        updateScmCondition(vm,(system.militaryCars&0x7F)==0x7F,inverted);
        return 1;
    }
    if(command==0x01F3||command==0x0300||command==0x036D) {
        std::array<float,10> parameters{};
        for(auto& value:parameters) {
            Operand operand;
            if(!readOperand(vm,operand)) {error="Crane activation has truncated parameters.";return -1;}
            value=operandFloat(operand);
        }
        return storylandActivateLcsCrane(system,parameters,command==0x0300,command==0x036D,error)?1:-1;
    }
    Operand x,y;
    if(!readOperand(vm,x)||!readOperand(vm,y)) {error="Crane coordinates are truncated.";return -1;}
    const float worldX=operandFloat(x),worldY=operandFloat(y);
    if(!std::isfinite(worldX)||!std::isfinite(worldY)) {error="Crane coordinates must be finite.";return -1;}
    if(command==0x01F4)return storylandDeactivateLcsCrane(system,worldX,worldY,error)?1:-1;
    Operand handle;
    if(!readOperand(vm,handle)) {error="Crane lifting query has no vehicle handle.";return -1;}
    if(!system.bindings.readVehicle) {error="Crane lifting query requires the live vehicle pool.";return -1;}
    StorylandLcsCraneVehicle vehicle;
    std::string vehicleError;
    const bool present=system.bindings.readVehicle(handle.bits,vehicle,vehicleError);
    if(!present&&!vehicleError.empty()) {error=vehicleError;return -1;}
    updateScmCondition(vm,present&&storylandLcsCraneLiftingCar(system,worldX,worldY,handle.bits),inverted);
    return 1;
}

#include "src/re/reLCS2/vehicles/storyland_lcs_boat.h"

enum class LcsScriptEntity { Player, Character, Vehicle, Object };

bool storylandLcsWeaponDamageMatches(int32_t stored,int32_t requested) {
    const int32_t weapon=static_cast<int8_t>(stored);
    if(requested==46)return weapon>=0&&weapon<=11;
    if(requested==47)return (weapon>=0&&weapon<=36)||weapon==41||weapon==42;
    return weapon==requested;
}

struct LcsScriptEntityPosition {
    std::array<float,3> coordinates{};
    float heading=0,speed=0;
    bool alive=false,inVehicle=false,attached=false,stopped=true;
};

bool lcsScriptVehicleStopped(const StorylandMissionEnvironment& world, int32_t handle,
    const StorylandMissionEnvironment::VehicleScriptState& vehicle) {
    if (world.nativeVehicleStopped) {
        bool stopped = false;
        return world.nativeVehicleStopped(handle, stopped) && stopped;
    }
    // VM-only worlds have no physical frame displacement. Production always
    // uses the live callback; unavailable physical state never takes this path.
    const float speed = std::max(std::fabs(vehicle.speed),
        std::hypot(vehicle.velocity[0], vehicle.velocity[1], vehicle.velocity[2]));
    const float distance = roundEeFloat(double(speed) * world.nativeScriptTimeStep / 50.0);
    const float limit = roundEeFloat(double(0.01f) * world.nativeScriptTimeStep);
    return std::isfinite(distance) && std::isfinite(limit) && distance <= limit;
}

bool getLcsScriptEntityPosition(const StorylandMissionEnvironment& world,LcsScriptEntity kind,
    int32_t handle,LcsScriptEntityPosition& result,bool strictStopped=false) {
    result={};
    if(kind==LcsScriptEntity::Player||(kind==LcsScriptEntity::Character&&handle==1)) {
        if(kind==LcsScriptEntity::Player&&handle!=0)return false;
        result.coordinates={world.x,world.y,world.z};result.heading=world.heading;
        result.speed=world.speed;result.alive=world.health>0;result.inVehicle=world.inVehicle;
        result.attached=world.playerAttachedVehicle>=0;
        const auto vehicle=world.missionVehicles.find(world.occupiedMissionVehicle);
        if(world.inVehicle&&vehicle!=world.missionVehicles.end()&&!vehicle->second.deleted) {
            result.coordinates={vehicle->second.x,vehicle->second.y,vehicle->second.z};
            result.speed=vehicle->second.speed;
        }
        result.stopped = std::fabs(result.speed) <= (result.inVehicle ? 0.5f : 0.01f);
        if (world.nativeCharacterStopped) {
            bool stopped = false;
            const bool available = world.nativeCharacterStopped(0, stopped);
            if (available || strictStopped) result.stopped = available && stopped;
        } else if (strictStopped) {
            if (world.inVehicle && vehicle != world.missionVehicles.end() && !vehicle->second.deleted)
                result.stopped = lcsScriptVehicleStopped(world, vehicle->first, vehicle->second);
            else result.stopped = result.speed == 0;
        }
        return true;
    }
    if(kind==LcsScriptEntity::Character) {
        const auto found=world.missionCharacters.find(handle);
        if(found==world.missionCharacters.end())return false;
        const auto& character=found->second;
        result.coordinates={character.x,character.y,character.z};result.heading=character.heading;
        result.alive=character.alive&&character.health>0;
        result.inVehicle=character.vehicle>=0&&!character.attached;result.attached=character.attached;
        result.speed=character.walkingToTarget?character.walkSpeed:0;
        if(character.vehicle>=0) {
            const auto vehicle=world.missionVehicles.find(character.vehicle);
            if(vehicle!=world.missionVehicles.end()&&!vehicle->second.deleted) {
                result.speed=vehicle->second.speed;
                if(!character.attached)result.coordinates={vehicle->second.x,vehicle->second.y,vehicle->second.z};
            }
        }
        result.stopped = result.inVehicle ? std::fabs(result.speed) <= 0.5f :
            !character.walkingToTarget || character.positionFrozen;
        if (world.nativeCharacterStopped) {
            bool stopped = false;
            const bool available = world.nativeCharacterStopped(handle, stopped);
            if (available || strictStopped) result.stopped = available && stopped;
        } else if (strictStopped) {
            if (result.inVehicle) {
                const auto vehicle = world.missionVehicles.find(character.vehicle);
                result.stopped = vehicle != world.missionVehicles.end() && !vehicle->second.deleted &&
                    lcsScriptVehicleStopped(world, vehicle->first, vehicle->second);
            } else result.stopped = character.movementState <= 1 && character.movementSpeed == 0;
        }
        return true;
    }
    if(kind==LcsScriptEntity::Vehicle) {
        const auto found=world.missionVehicles.find(handle);
        if(found==world.missionVehicles.end()||found->second.deleted)return false;
        const auto& vehicle=found->second;
        result.coordinates={vehicle.x,vehicle.y,vehicle.z};result.heading=vehicle.heading;
        result.speed=vehicle.speed;result.alive=vehicle.alive&&vehicle.health>0;
        if(world.vehicleOrientationAndSpeed&&vehicle.spawnedSlot>=0)
            result.speed=world.vehicleOrientationAndSpeed(handle)[1]*50.0f;
        result.stopped = std::fabs(result.speed) <= 0.5f;
        if (world.nativeVehicleStopped) {
            bool stopped = false;
            const bool available = world.nativeVehicleStopped(handle, stopped);
            if (available || strictStopped) result.stopped = available && stopped;
        } else if (strictStopped) result.stopped = lcsScriptVehicleStopped(world, handle, vehicle);
        return true;
    }
    const auto found=std::find_if(world.objects.begin(),world.objects.end(),
        [&](const auto& object){return object.handle==handle;});
    if(found==world.objects.end())return false;
    result.coordinates={found->x,found->y,found->z};result.heading=found->heading;result.alive=true;
    return true;
}

void detachLcsScriptCharacter(StorylandMissionEnvironment& world,int32_t handle) {
    const auto attachment=world.characterAttachments.find(handle);
    if(attachment!=world.characterAttachments.end()) {
        storylandRestoreAttachmentWeapon(world,handle,attachment->second);
        world.characterAttachments.erase(attachment);
    }
    if(handle==1) {
        const auto vehicle=world.missionVehicles.find(world.occupiedMissionVehicle);
        if(vehicle!=world.missionVehicles.end()) {
            if(vehicle->second.driver==handle)vehicle->second.driver=-1;
            if(world.inVehicle&&vehicle->second.doorLock==5)vehicle->second.doorLock=1;
        }
        world.inVehicle=false;world.occupiedMissionVehicle=-1;world.occupiedVehicleModel=-1;
        world.playerAttachedVehicle=-1;world.playerAttachment={};world.playerWarped=true;
        return;
    }
    const auto found=world.missionCharacters.find(handle);
    if(found==world.missionCharacters.end())return;
    auto& character=found->second;
    const auto vehicle=world.missionVehicles.find(character.vehicle);
    if(vehicle!=world.missionVehicles.end()&&vehicle->second.driver==handle) {
        vehicle->second.driver=-1;
        if(vehicle->second.doorLock==5)vehicle->second.doorLock=1;
    }
    character.vehicle=-1;character.passengerSeat=-1;character.attached=false;character.attachment={};
}

bool positionLcsAttachedCharacter(StorylandMissionEnvironment& world,int32_t handle) {
    const auto attached=world.characterAttachments.find(handle);
    const auto character=world.missionCharacters.find(handle);
    if(attached==world.characterAttachments.end()||character==world.missionCharacters.end())return false;
    auto& ped=character->second;
    if(ped.vehicle>=0) {
        const auto car=world.missionVehicles.find(ped.vehicle);
        if(car==world.missionVehicles.end()||car->second.deleted)return false;
    }
    std::array<float,16> frame{};
    if(!storylandLcsAttachmentParentFrame(world,ped,frame)||
        !std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);}))return false;
    const auto& offset=ped.attachment;
    ped.x=frame[0]*offset[0]+frame[4]*offset[1]+frame[8]*offset[2]+frame[12];
    ped.y=frame[1]*offset[0]+frame[5]*offset[1]+frame[9]*offset[2]+frame[13];
    ped.z=frame[2]*offset[0]+frame[6]*offset[1]+frame[10]*offset[2]+frame[14];
    const float objectAngle=std::atan2(-frame[4],frame[5])*57.2957795131f;
    const float turn=attached->second.type==1?90.0f:attached->second.type==2?180.0f:
        attached->second.type==3?-90.0f:0.0f;
    const float target=std::remainder(objectAngle+turn,360.0f);
    const float difference=std::remainder(ped.heading-target,360.0f);
    ped.heading=std::remainder(target+std::clamp(difference,-attached->second.rotationLimit,
        attached->second.rotationLimit),360.0f);
    return true;
}

void releaseLcsScriptCharacter(StorylandMissionEnvironment& world,int32_t handle) {
    const auto found=world.missionCharacters.find(handle);
    if(found==world.missionCharacters.end()||!found->second.persistent)return;
    auto& character=found->second;
    world.scriptedPedLooks.erase(handle);
    world.lcsEntityExtension.tasks.erase(handle);
    world.lcsEntityExtension.runningOverrides.erase(handle);
    character.persistent=false;character.objective=0;character.objectiveTarget=-1;
    character.objectiveCompleted=false;character.walkingToTarget=false;character.lookAtCharacter=-1;
    world.pedWaitStateMilliseconds.erase(handle);world.pedAnimationRequests.erase(handle);
    world.lcsWaitAnimations.erase(handle);
    world.characterVehicleTasks.erase(handle);
    if(character.alive&&character.health>0&&character.vehicle<0&&!character.attached) {
        character.walkSpeed=1.25f;
        world.characterWanderTasks[handle]={int32_t(uint32_t(handle)&7u),UINT32_MAX};
    }
}

void registerLcsStoredVehicle(StorylandMissionEnvironment& world,int32_t handle) {
    if(handle==world.storedMissionVehicleHandle)return;
    const auto previous=world.missionVehicles.find(world.storedMissionVehicleHandle);
    if(previous!=world.missionVehicles.end()&&world.storedMissionVehicleWasAmbient) {
        previous->second.persistent=false;
        world.missionCleanupVehicles.erase(world.storedMissionVehicleHandle);
    }
    world.storedMissionVehicleHandle=handle;world.storedMissionVehicleWasAmbient=false;
    const auto found=world.missionVehicles.find(handle);
    if(found!=world.missionVehicles.end()&&!found->second.deleted&&!found->second.persistent) {
        found->second.persistent=true;world.storedMissionVehicleWasAmbient=true;
        world.missionCleanupVehicles.insert(handle);
    }
}

void cleanUpLcsMissionEntities(StorylandMissionEnvironment& world) {
    // Native cleanup returns registered script entities to ambient ownership.
    // MAIN/startup entities and DONT_REMOVE entities are not in these sets.
    for(int32_t handle:world.missionCleanupCharacters)releaseLcsScriptCharacter(world,handle);
    world.scriptedPedLooks.clear();
    if(world.lcsEntityExtension.tasks.erase(1)) {
        world.playerWalkTask.clear();
        world.playerNavigation.clearRoute();
    }
    for(int32_t handle:world.missionCleanupVehicles) {
        const auto found=world.missionVehicles.find(handle);
        if(found!=world.missionVehicles.end())found->second.persistent=false;
    }
    for(auto& object:world.objects)
        if(world.missionCleanupObjects.count(object.handle))object.persistent=false;
    world.missionCleanupCharacters.clear();world.missionCleanupVehicles.clear();world.missionCleanupObjects.clear();
    world.storedMissionVehicleHandle=-1;world.storedMissionVehicleWasAmbient=false;
    world.carDensityMultiplier=1;world.pedDensityMultiplier=1;world.threatReactionRangeMultiplier=1;
    world.lcsEnterCarRangeMultiplier=1;
    world.wantedMultiplier=1;world.upsideDownCarChecks.clear();world.lcsStuckCarChecks={};
    world.allowedCollisionLevels={0,0};
    if(world.remoteControl.vehicle<0) {world.camera.active=false;world.camera.restoreRequested=true;}
    world.remoteControl.detonationDisabled=false;world.camera.transitioning=false;
    world.camera.transitionPending=false;world.camera.cutsceneOwned=false;world.widescreen=false;
    world.extraColor=-1;world.extraColorFade=false;
    for(auto& slot:world.audioSlots) {slot={};slot.serial=world.nextAudioSerial++;}
    world.forcedWeather=-1;world.weatherChanged=true;world.streamingEnabled=true;
    world.helpMessage.clear();world.helpMessagePersistent=false;world.helpMessageMilliseconds=0;
    world.onscreenTimersFrozen=false;world.policeIgnorePlayer=false;world.everyoneIgnoresPlayer=false;
    world.control=true;world.playerDamageProofs=0;world.playerOnlyDamagedByPlayer=false;
    for(auto& entry:world.missionVehicles)entry.second.canBeDamaged=true;
    if(world.setAllCarsDamageable)world.setAllCarsDamageable(true);
}

bool lcsPassengerSeatOccupied(const StorylandMissionEnvironment& world,int32_t character,
    int32_t vehicle,int32_t seat) {
    for(const auto& entry:world.missionCharacters)
        if(entry.first!=character&&entry.second.alive&&!entry.second.attached&&
            entry.second.vehicle==vehicle&&entry.second.passengerSeat==seat)return true;
    for(const auto& entry:world.characterVehicleTasks)if(entry.first!=character) {
        const auto& task=entry.second;
        if((!task.exiting&&task.vehicle==vehicle&&task.seat==seat)||
            (task.destinationVehicle==vehicle&&task.destinationSeat==seat))return true;
    }
    return false;
}

bool prepareLcsCharacterVehicleTask(StorylandMissionEnvironment& world,int32_t handle,
    StorylandMissionEnvironment::CharacterVehicleTask& task,std::string& error) {
    const auto character=world.missionCharacters.find(handle);
    if(!task.exiting&&task.seat>=0&&character!=world.missionCharacters.end()&&
        ((task.seat==0&&!character->second.allowFrontPassenger)||
         (task.seat>0&&world.lcsExtendedWorld.frontSeatOnlyCharacters.count(handle)))) {
        error="The requested passenger seat is disabled for this character.";return false;
    }
    if(!world.prepareCharacterVehicleTask) {error="Pedestrian vehicle tasks require native vehicle geometry and animations.";return false;}
    if(!world.prepareCharacterVehicleTask(handle,task,error))return false;
    for(float seconds:task.phaseSeconds)if(!std::isfinite(seconds)||seconds<0) {
        error="Native vehicle transition has invalid animation timing.";return false;
    }
    for(const auto* point:{&task.door,&task.seatPosition,&task.vehicleOrigin})
        for(float value:*point)if(!std::isfinite(value)) {error="Native vehicle transition geometry is non-finite.";return false;}
    for(const auto& point:task.route)for(float value:point)
        if(!std::isfinite(value)) {error="Pedestrian approach route is non-finite.";return false;}
    StorylandVehiclePhaseTiming timing;timing.seconds=task.phaseSeconds;
    const auto action=task.exiting?StorylandVehicleAction::Exit:
        task.hijacking?StorylandVehicleAction::Hijack:StorylandVehicleAction::Enter;
    if(storylandSampleVehicleTransition(action,timing,0).total<=0) {
        error="Native vehicle entry/exit animations are unavailable.";return false;
    }
    return true;
}

void advanceLcsCharacterVehicleTasks(StorylandMissionEnvironment& world,uint32_t elapsedMilliseconds) {
    using Phase=StorylandVehiclePhase;
    for(auto entry=world.characterVehicleTasks.begin();entry!=world.characterVehicleTasks.end();) {
        const int32_t handle=entry->first;
        auto& task=entry->second;
        const auto found=world.missionCharacters.find(handle);
        if(found==world.missionCharacters.end()||!found->second.alive||
            (found->second.health<=0&&!found->second.behavior.leavingVehicleToDie)) {
            entry=world.characterVehicleTasks.erase(entry);continue;
        }
        auto& character=found->second;
        if(!task.animating&&((task.exiting&&character.vehicle<0&&task.destinationVehicle<0)||
            (!task.exiting&&character.vehicle==task.vehicle&&character.passengerSeat==task.seat))) {
            character.walkingToTarget=false;character.objectiveCompleted=true;
            character.objective=0;character.objectiveTarget=-1;
            entry=world.characterVehicleTasks.erase(entry);continue;
        }
        const auto vehicle=world.missionVehicles.find(task.vehicle);
        if(vehicle==world.missionVehicles.end()||vehicle->second.deleted||!vehicle->second.alive) {
            character.walkingToTarget=false;character.objective=0;character.objectiveTarget=-1;
            entry=world.characterVehicleTasks.erase(entry);continue;
        }
        if(character.positionFrozen||world.lcsWaitAnimations.count(handle)) {
            character.walkingToTarget=false;
            if(task.animating)task.startedAt+=elapsedMilliseconds;
            ++entry;continue;
        }
        const auto& car=vehicle->second;
        const float rotation=(car.heading-task.vehicleHeading)*0.0174532925199433f;
        const float cosine=std::cos(rotation),sine=std::sin(rotation);
        const auto moveAnchor=[&](std::array<float,3>& point) {
            const float x=point[0]-task.vehicleOrigin[0],y=point[1]-task.vehicleOrigin[1];
            point={car.x+cosine*x-sine*y,car.y+sine*x+cosine*y,car.z+point[2]-task.vehicleOrigin[2]};
        };
        moveAnchor(task.door);moveAnchor(task.seatPosition);
        for(auto& point:task.route)moveAnchor(point);
        if(task.animating)moveAnchor(task.start);
        task.vehicleOrigin={car.x,car.y,car.z};task.vehicleHeading=car.heading;
        if(!task.animating) {
            if(!task.exiting&&std::hypot(character.x-car.x,character.y-car.y,character.z-car.z)>
                float(world.lcsEnterCarRangeMultiplier)*30.0f) {
                character.walkingToTarget=false;
                if(!character.persistent) {
                    character.objective=0;character.objectiveTarget=-1;
                    entry=world.characterVehicleTasks.erase(entry);continue;
                }
                ++entry;continue;
            }
            if(task.route.empty()) {
                std::string error;
                if(!prepareLcsCharacterVehicleTask(world,handle,task,error)||task.route.empty()) {
                    character.walkingToTarget=false;++entry;continue;
                }
                task.routeIndex=0;
            }
            if(!task.exiting) {
                while(task.routeIndex<task.route.size()&&std::hypot(character.x-task.route[task.routeIndex][0],
                    character.y-task.route[task.routeIndex][1])<=0.3f)++task.routeIndex;
                if(task.routeIndex<task.route.size()) {
                    character.targetX=task.route[task.routeIndex][0];character.targetY=task.route[task.routeIndex][1];
                    character.walkingToTarget=true;character.walkSpeed=3.8f;++entry;continue;
                }
                if(task.seat>=0&&lcsPassengerSeatOccupied(world,handle,task.vehicle,task.seat)) {
                    character.walkingToTarget=false;++entry;continue;
                }
            }
            const bool carried=std::any_of(world.lcsCranes.cranes.begin(),world.lcsCranes.cranes.end(),
                [&](const auto& crane) {
                    return crane.vehicle==task.vehicle&&(crane.state==StorylandLcsCrane::Raising||
                        crane.state==StorylandLcsCrane::Lifting||crane.state==StorylandLcsCrane::Aligning);
                });
            if(!task.exiting&&(carried||storylandLcsVehicleEntryBlocked(world,task.vehicle,task.seat,handle))) {
                // Native carried-car/failed-lock/protected-driver entry restores the prior
                // objective; it never reports the enter objective completed.
                character.walkingToTarget=false;character.objective=0;character.objectiveTarget=-1;
                character.objectiveCompleted=false;entry=world.characterVehicleTasks.erase(entry);continue;
            }
            task.animating=true;task.startedAt=world.gameMilliseconds;
            task.start=task.exiting?task.seatPosition:std::array<float,3>{character.x,character.y,character.z};
            character.walkingToTarget=false;
            if(task.exiting&&task.seat<0&&vehicle->second.driver==handle) {
                vehicle->second.scriptStatus=4;vehicle->second.cruiseSpeed=0;
            }
        }
        StorylandVehiclePhaseTiming timing;timing.seconds=task.phaseSeconds;
        const auto action=task.exiting?StorylandVehicleAction::Exit:
            task.hijacking?StorylandVehicleAction::Hijack:StorylandVehicleAction::Enter;
        const auto sample=storylandSampleVehicleTransition(action,timing,float(world.gameMilliseconds-task.startedAt)/1000.0f);
        const auto phase=sample.phase;
        if(!task.exiting&&task.hijacking&&!task.driverRemoved&&
            storylandLcsVehicleDriverProtected(world,task.vehicle,handle)) {
            character.walkingToTarget=false;character.objective=0;character.objectiveTarget=-1;
            character.objectiveCompleted=false;entry=world.characterVehicleTasks.erase(entry);continue;
        }
        if(task.hijacking&&!task.driverRemoved&&(phase==Phase::GetIn||phase==Phase::CloseInside||phase==Phase::Done)) {
            const int32_t driver=vehicle->second.driver;
            if(world.inVehicle&&world.occupiedMissionVehicle==task.vehicle&&world.playerPassengerSeat<0) {
                detachLcsScriptCharacter(world,1);world.x=task.door[0];world.y=task.door[1];world.z=task.door[2];
            }
            if(driver>=2&&driver!=handle) {
                detachLcsScriptCharacter(world,driver);
                auto displaced=world.missionCharacters.find(driver);
                if(displaced!=world.missionCharacters.end()) {
                    displaced->second.x=task.door[0];displaced->second.y=task.door[1];displaced->second.z=task.door[2];
                    displaced->second.walkingToTarget=false;
                    const bool flee=displaced->second.creationSource==2||car.persistent||
                        displaced->second.pedStats.temper<=displaced->second.pedStats.fear;
                    StorylandLcsEntityExtensionState::Task reaction;
                    reaction.objective=flee?41:8;
                    reaction.target=flee?task.vehicle:handle;
                    world.lcsEntityExtension.tasks[driver]=reaction;
                    displaced->second.objective=flee?0x0470:0x01CE;
                    displaced->second.objectiveTarget=reaction.target;
                    displaced->second.objectiveCompleted=false;
                }
            }
            for(auto& occupant:world.missionCharacters) {
                auto& passenger=occupant.second;
                if(occupant.first==handle||passenger.vehicle!=task.vehicle||passenger.attached||
                    passenger.stayInCarWhenJacked||passenger.behavior.frightenedWhenJacked)continue;
                StorylandLcsEntityExtensionState::Task leave;
                leave.objective=16;leave.target=task.vehicle;
                world.lcsEntityExtension.tasks[occupant.first]=leave;
                passenger.objective=0x03E7;passenger.objectiveTarget=task.vehicle;
                passenger.objectiveCompleted=false;
            }
            if(world.removeVehicleDriver)world.removeVehicleDriver(task.vehicle,task.door);
            task.driverRemoved=true;
        }
        const auto interpolate=[](const std::array<float,3>& from,const std::array<float,3>& to,float fraction) {
            return std::array<float,3>{from[0]+(to[0]-from[0])*fraction,from[1]+(to[1]-from[1])*fraction,
                from[2]+(to[2]-from[2])*fraction};
        };
        auto position=task.door;
        if(phase==Phase::GetIn)position=interpolate(task.door,task.seatPosition,sample.fraction);
        else if(phase==Phase::GetOut)position=interpolate(task.seatPosition,task.door,sample.fraction);
        else if(!task.exiting&&(phase==Phase::CloseInside||phase==Phase::Done))position=task.seatPosition;
        character.x=position[0];character.y=position[1];character.z=position[2];character.heading=car.heading;
        character.walkingToTarget=false;
        const bool bike=world.vehicleNativeType&&world.vehicleNativeType(task.vehicle)==5;
        if(world.updateVehicleTaskDoor&&!bike)
            world.updateVehicleTaskDoor(task.vehicle,task.seat,unsigned(phase),sample.elapsed);
        if(!task.exiting&&(phase==Phase::CloseInside||phase==Phase::Done)) {
            character.vehicle=task.vehicle;character.passengerSeat=task.seat;
            character.attached=false;character.attachment={};
            if(task.seat<0) {
                vehicle->second.driver=handle;vehicle->second.scriptStatus=3;
            }
        } else if(task.exiting&&(phase==Phase::CloseOutside||phase==Phase::Done))detachLcsScriptCharacter(world,handle);
        if(phase==Phase::Done) {
            if(!task.exiting&&task.seat<0)vehicle->second.engineOn=true;
            if(!task.exiting&&task.seat>=0&&character.creationSource==1&&world.inVehicle&&
                world.occupiedMissionVehicle==task.vehicle&&world.modelIdByName&&
                (car.model==world.modelIdByName("coach")||car.model==world.modelIdByName("bus")))
                world.lcsExtendedWorld.busFaresCollected+=5;
            if(task.destinationVehicle>=0) {
                const int32_t target=task.destinationVehicle,seat=task.destinationSeat;
                task={};task.vehicle=target;task.seat=seat;
                std::string error;
                prepareLcsCharacterVehicleTask(world,handle,task,error);
                ++entry;continue;
            }
            character.objectiveCompleted=true;character.objective=0;character.objectiveTarget=-1;
            if(task.exiting&&character.behavior.leavingVehicleToDie) {
                character.behavior.leavingVehicleToDie=false;
                character.health=0;character.alive=false;
            }
            entry=world.characterVehicleTasks.erase(entry);continue;
        }
        ++entry;
    }
}

int executeLcsLocationCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(command>=0x02B2&&command<=0x02BD) {
        const unsigned index=command-0x02B2;
        const bool threeDimensional=index>=6;
        const size_t count=threeDimensional?9:7;
        std::array<Operand,9> values{};
        for(size_t operand=0;operand<count;++operand)if(!readOperand(vm,values[operand])) {
            error="LCS angled-area query has truncated operands.";return -1;
        }
        if(!vm.environment) {error="LCS angled-area query requires a live world.";return -1;}
        const size_t second=threeDimensional?4:3;
        const size_t radiusIndex=threeDimensional?7:5;
        const float x1=operandFloat(values[1]),y1=operandFloat(values[2]);
        const float x2=operandFloat(values[second]),y2=operandFloat(values[second+1]);
        const float radius=operandFloat(values[radiusIndex]);
        const float z1=threeDimensional?operandFloat(values[3]):0;
        const float z2=threeDimensional?operandFloat(values[6]):0;
        const float dx=x2-x1,dy=y2-y1,length=std::hypot(dx,dy);
        if(!std::isfinite(x1)||!std::isfinite(y1)||!std::isfinite(x2)||!std::isfinite(y2)||
            !std::isfinite(z1)||!std::isfinite(z2)||!std::isfinite(radius)||!std::isfinite(length)) {
            error="LCS angled-area query requires finite geometry.";return -1;
        }
        const float axisX=length>0?dx/length:1,axisY=length>0?dy/length:0;
        const float normalX=length>0?-axisY:1,normalY=length>0?axisX:0;
        auto& world=*vm.environment;
        LcsScriptEntityPosition player;
        bool matches=getLcsScriptEntityPosition(world,LcsScriptEntity::Player,values[0].bits,player);
        const float px=player.coordinates[0]-x1,py=player.coordinates[1]-y1;
        const float along=px*axisX+py*axisY,across=px*normalX+py*normalY;
        matches=matches&&along>=0&&along<=length&&across>=0&&across<=radius;
        if(threeDimensional)matches=matches&&player.coordinates[2]>=std::min(z1,z2)&&
            player.coordinates[2]<=std::max(z1,z2);
        const unsigned movement=index%3;
        if(movement==1)matches=matches&&!player.inVehicle;
        if(movement==2)matches=matches&&player.inVehicle;
        if(index%6>=3)matches=matches&&player.stopped;
        if(values[count-1].bits) {
            StorylandMissionEnvironment::LocateRegion region;
            const float offsetX=normalX*radius,offsetY=normalY*radius;
            region.center={(x1+x2+offsetX)*.5f,(y1+y2+offsetY)*.5f,
                threeDimensional?(z1+z2)*.5f:20.0f};
            region.extent={std::max(std::fabs(dx),std::fabs(offsetX))*.5f,
                std::max(std::fabs(dy),std::fabs(offsetY))*.5f,0};
            region.threeDimensional=threeDimensional;
            if(!threeDimensional&&world.groundHeightAt) {
                float ground=0;
                if(world.groundHeightAt({region.center[0],region.center[1],1000},ground)&&std::isfinite(ground))
                    region.center[2]=ground;
            }
            region.expiresAt=world.gameMilliseconds+100;
            const auto existing=std::find_if(world.locateRegions.begin(),world.locateRegions.end(),
                [&](const auto& item) {return item.center==region.center&&item.extent==region.extent&&
                    item.threeDimensional==region.threeDimensional;});
            if(existing!=world.locateRegions.end())existing->expiresAt=region.expiresAt;
            else if(world.locateRegions.size()<32)world.locateRegions.push_back(region);
        }
        updateScmCondition(vm,matches,inverted);
        return 1;
    }
    LcsScriptEntity subject=LcsScriptEntity::Character,target=LcsScriptEntity::Character;
    unsigned dimensions=2,movement=0;
    bool area=false,entityTarget=false,stopped=false,supported=true;
    if(command==0x0056||command==0x0057) {
        subject=LcsScriptEntity::Player;dimensions=command==0x0057?3:2;area=true;
    } else if(command==0x00A3||command==0x00A4) {
        dimensions=command==0x00A4?3:2;area=true;
    } else if(command==0x00B0||command==0x00B1) {
        subject=LcsScriptEntity::Vehicle;dimensions=command==0x00B1?3:2;area=true;
    } else if(command>=0x00E8&&command<=0x00ED) {
        subject=LcsScriptEntity::Player;movement=(command-0x00E8)%3;stopped=command>=0x00EB;
    } else if(command>=0x00EE&&command<=0x00F0) {
        subject=LcsScriptEntity::Player;movement=command-0x00EE;entityTarget=true;
    } else if(command>=0x00F1&&command<=0x00F6) {
        movement=(command-0x00F1)%3;stopped=command>=0x00F4;
    } else if(command>=0x00F7&&command<=0x00F9) {
        movement=command-0x00F7;entityTarget=true;
    } else if(command>=0x00FA&&command<=0x00FF) {
        subject=LcsScriptEntity::Player;dimensions=3;movement=(command-0x00FA)%3;stopped=command>=0x00FD;
    } else if(command>=0x0100&&command<=0x0102) {
        subject=LcsScriptEntity::Player;dimensions=3;movement=command-0x0100;entityTarget=true;
    } else if(command>=0x0103&&command<=0x0108) {
        dimensions=3;movement=(command-0x0103)%3;stopped=command>=0x0106;
    } else if(command>=0x0109&&command<=0x010B) {
        dimensions=3;movement=command-0x0109;entityTarget=true;
    } else if(command>=0x019C&&command<=0x01AF) {
        const unsigned group=(command-0x019C)/5,index=(command-0x019C)%5;
        subject=group<2?LcsScriptEntity::Player:LcsScriptEntity::Character;
        dimensions=(group&1)?3:2;area=true;stopped=index>=2;
        movement=index<2?index+1:index-2;
    } else if(command==0x01B0||command==0x01B1) {
        subject=LcsScriptEntity::Vehicle;dimensions=command==0x01B1?3:2;area=true;stopped=true;
    } else if(command>=0x01B2&&command<=0x01B5) {
        subject=LcsScriptEntity::Vehicle;dimensions=command>=0x01B4?3:2;stopped=(command&1)!=0;
    } else if(command>=0x0201&&command<=0x020C) {
        const unsigned index=command-0x0201;
        subject=index<6?LcsScriptEntity::Player:LcsScriptEntity::Character;
        target=LcsScriptEntity::Vehicle;dimensions=(index%6)>=3?3:2;
        movement=index%3;entityTarget=true;
    } else if(command>=0x0476&&command<=0x047B) {
        target=LcsScriptEntity::Object;dimensions=command>=0x0479?3:2;
        movement=(command-0x0476)%3;entityTarget=true;
    } else supported=false;
    if(!supported)return 0;
    const size_t count=entityTarget?dimensions+3:dimensions*2+2;
    std::array<Operand,8> values{};
    for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="LCS location query has truncated operands.";return -1;
    }
    if(!vm.environment) {error="LCS location query requires a live world.";return -1;}
    LcsScriptEntityPosition position,destination;
    bool matches=getLcsScriptEntityPosition(*vm.environment,subject,values[0].bits,position);
    if(entityTarget)matches=getLcsScriptEntityPosition(*vm.environment,target,values[1].bits,destination)&&matches;
    for(unsigned axis=0;axis<dimensions;++axis) {
        const float first=entityTarget?destination.coordinates[axis]:operandFloat(values[axis+1]);
        const float second=operandFloat(values[entityTarget?axis+2:axis+1+dimensions]);
        if(!std::isfinite(first)||!std::isfinite(second)) {
            error="LCS location query requires finite bounds and radii.";return -1;
        }
        if(area)matches=matches&&position.coordinates[axis]>=std::min(first,second)&&
            position.coordinates[axis]<=std::max(first,second);
        else if(subject==LcsScriptEntity::Player&&!entityTarget)
            matches=matches&&std::fabs(position.coordinates[axis]-first)<second;
        else matches=matches&&std::fabs(position.coordinates[axis]-first)<=second;
    }
    if(movement==1)matches=matches&&!position.inVehicle;
    if(movement==2)matches=matches&&position.inVehicle;
    if(stopped)matches=matches&&position.stopped;
    updateScmCondition(vm,matches,inverted);
    return 1;
}
void beginLcsRemoteRemoval(StorylandMissionEnvironment& world) {
    const auto car=world.missionVehicles.find(world.remoteControl.vehicle);
    if(car==world.missionVehicles.end()||car->second.deleted)return;
    car->second.persistent=false;
    world.remoteControl.ending=true;world.remoteControl.restoreCamera=true;
    world.remoteControl.lostAt=world.gameMilliseconds;
}

void advanceLcsRemoteControl(StorylandMissionEnvironment& world,uint32_t elapsedMilliseconds) {
    auto& remote=world.remoteControl;
    if(remote.vehicle<0)return;
    const auto car=world.missionVehicles.find(remote.vehicle);
    if(car==world.missionVehicles.end()||car->second.deleted) {
        remote={};world.camera.active=false;world.camera.restoreRequested=true;return;
    }
    if(!remote.ending&&(!car->second.alive||car->second.health<=0))beginLcsRemoteRemoval(world);
    if(!remote.ending)return;
    const uint32_t elapsed=world.gameMilliseconds-remote.lostAt;
    const auto fade=[&](bool in) {
        world.fadingColor={0,0,0};world.fadeStartAlpha=world.fadeAlpha;
        world.fadeTargetAlpha=in?0.0f:1.0f;world.fadeDirection=in?1:0;
        world.fadeDuration=1000;world.fadeTotal=1000;world.fading=true;
    };
    if(remote.restoreCamera&&elapsed>=1000&&uint32_t(elapsed-elapsedMilliseconds)<1000)fade(false);
    if(elapsed>2000) {
        car->second.deleted=true;car->second.alive=false;
        world.missionCleanupVehicles.erase(remote.vehicle);
        if(remote.restoreCamera) {world.camera.active=false;world.camera.restoreRequested=true;fade(true);}
        remote={};
    }
}

int executeLcsRemoteCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(command!=0x0111&&command!=0x0246&&command!=0x040E&&command!=0x0473&&command!=0x0489&&
        command!=0x04E0&&command!=0x05E7&&command!=0x05E8)return 0;
    if(!vm.environment) {error="Remote vehicle commands require a live world.";return -1;}
    auto& world=*vm.environment;auto& remote=world.remoteControl;
    if(command==0x040E||command==0x04E0||command==0x05E8) {
        const auto car=world.missionVehicles.find(remote.vehicle);
        if(car==world.missionVehicles.end()||car->second.deleted)return 1;
        if(command==0x040E&&car->second.canBeDamaged&&!world.explodeMissionVehicle) {
            error="RC detonation requires the native vehicle explosion binding.";return -1;
        }
        beginLcsRemoteRemoval(world);
        if(command==0x040E&&car->second.canBeDamaged)world.explodeMissionVehicle(remote.vehicle);
        if(command==0x05E8) {
            car->second.scriptStatus=4;car->second.drivingToTarget=false;
            const auto occupied=world.missionVehicles.find(world.occupiedMissionVehicle);
            if(occupied!=world.missionVehicles.end())occupied->second.canBeDamaged=true;
            remote={};world.camera.active=false;world.camera.restoreRequested=true;
        }
        return 1;
    }
    Operand first;
    if(!readOperand(vm,first)) {error="Remote command has invalid operands.";return -1;}
    if(command==0x0246||command==0x0489) {
        if(first.bits!=0) {error="Remote vehicle query requires player zero.";return -1;}
        if(command==0x0246)updateScmCondition(vm,remote.vehicle>=0||remote.ending,inverted);
        else {
            Operand destination;
            if(!readOperand(vm,destination)||!destination.reference) {error="Remote vehicle output is not a variable.";return -1;}
            writeOperand(vm,destination,remote.vehicle);
        }
        return 1;
    }
    if(command==0x05E7) {
        Operand interpolate;
        if(!readOperand(vm,interpolate)) {error="Remote override has invalid interpolation flag.";return -1;}
        const auto car=world.missionVehicles.find(first.bits);
        if(car==world.missionVehicles.end()||car->second.deleted) {error="Remote override refers to an unknown vehicle.";return -1;}
        if(!world.prepareRemoteVehicle||!world.prepareRemoteVehicle(car->second.model,error)) {
            if(error.empty())error="Remote control requires native vehicle physics.";return -1;
        }
        remote={};remote.vehicle=first.bits;remote.detonationDisabled=true;remote.firstPerson=true;
        car->second.scriptStatus=12;car->second.drivingToTarget=false;
        world.missionCleanupVehicles.erase(first.bits);
        const auto occupied=world.missionVehicles.find(world.occupiedMissionVehicle);
        if(occupied!=world.missionVehicles.end())occupied->second.canBeDamaged=false;
        world.camera.active=true;world.camera.positionSet=world.camera.targetSet=false;
        world.camera.restoreRequested=interpolate.bits==0;
        return 1;
    }
    if(first.bits!=0) {error="Remote vehicle creation requires player zero.";return -1;}
    std::array<Operand,4> transform;
    for(auto& value:transform)if(!readOperand(vm,value)||!std::isfinite(operandFloat(value))) {
        error="Remote vehicle coordinates and heading must be finite.";return -1;
    }
    Operand last;
    if(!readOperand(vm,last)||(command==0x0111&&!last.reference)) {
        error="Remote vehicle model or output variable is invalid.";return -1;
    }
    const int32_t model=command==0x0111?169:last.bits; // LCS DEFAULT.IDE: RCBANDIT.
    if(model<0||!world.prepareRemoteVehicle||!world.prepareRemoteVehicle(model,error)) {
        if(error.empty())error="Remote control requires native automobile physics.";return -1;
    }
    if(world.nextMissionVehicleHandle<=0||world.nextMissionVehicleHandle==INT32_MAX) {
        error="Remote vehicle handle space is exhausted.";return -1;
    }
    const float x=operandFloat(transform[0]),y=operandFloat(transform[1]);float z=operandFloat(transform[2]);
    float ground=0;
    if(world.groundHeightAt&&world.groundHeightAt({x,y,z<=-100?1000.0f:z+2.0f},ground))z=ground;
    else if(z<=-100) {error="Remote vehicle ground height is unavailable.";return -1;}
    StorylandMissionEnvironment::VehicleScriptState car;
    car.boatAnchored=true;car.doorLock=1;
    car.model=model;car.x=x;car.y=y;car.z=z;car.heading=operandFloat(transform[3]);
    car.scriptStatus=12;car.cruiseSpeed=7.5f;
    const int32_t handle=world.nextMissionVehicleHandle++;
    world.missionVehicles.emplace(handle,car);world.requestedModels.insert(model);
    remote={};remote.vehicle=handle;
    world.camera.active=true;world.camera.positionSet=world.camera.targetSet=false;
    if(command==0x0111)writeOperand(vm,last,handle);
    return 1;
}

int executeLcsRestoredPlayerCommands(StorylandMissionVm& vm,uint16_t command,
    bool inverted,std::string& error) {
    if (vm.vcs) return 0;
    const bool minutes=command==0x00C1;
    const bool setArmour=command==0x0363;
    const bool getArmour=command==0x04E2;
    const bool model=command==0x02F7;
    const bool seat=command==0x0436||command==0x0437;
    const bool sitting=command==0x0447||command==0x0448||command==0x044D||
        command==0x044E||command==0x044F;
    const bool safety=command==0x03F4||command==0x0616;
    const bool vehicleKind=command==0x05B0||command==0x05B1||command==0x05B2||command==0x05B3;
    const bool policeVehicle=command==0x0571;
    if(!minutes&&!setArmour&&!getArmour&&!model&&!seat&&!sitting&&!safety&&
        !vehicleKind&&!policeVehicle)return 0;
    if(!vm.environment) {error="LCS opcode requires a running world.";return -1;}
    auto& world=*vm.environment;
    const size_t count=minutes||command==0x0437?3:
        setArmour||getArmour||model||command==0x0436||command==0x0447||command==0x044D?2:
        command==0x0616?0:1;
    std::array<Operand,3> args{};
    for(size_t i=0;i<count;++i)if(!readOperand(vm,args[i])) {
        error="LCS restored opcode has truncated arguments.";return -1;
    }
    const auto output=[&](size_t index,int32_t value) {
        if(writeOperand(vm,args[index],value))return true;
        error="LCS restored opcode requires a writable result.";return false;
    };
    const auto matches=[&](bool value) {
        updateScmCondition(vm,value,inverted);return 1;
    };
    if(minutes) {
        if(args[0].bits<0||args[0].bits>=24||args[1].bits<0||args[1].bits>=60||
            !std::isfinite(world.hour)) {
            error="GET_MINUTES_TO_TIME_OF_DAY requires a valid clock and time.";return -1;
        }
        const int32_t now=storylandClockMinute(world.hour);
        const int32_t target=args[0].bits*60+args[1].bits;
        return output(2,(target-now+1440)%1440)?1:-1;
    }
    if(setArmour) {
        if(args[0].bits!=0) {error="SET_PLAYER_ARMOUR requires player zero.";return -1;}
        world.armour=std::clamp(float(args[1].bits),0.0f,float(world.playerArmourLimit));
        return 1;
    }
    if(getArmour) {
        float armour=0;
        if(args[0].bits==1)armour=world.armour;
        else {
            const auto character=world.missionCharacters.find(args[0].bits);
            if(character==world.missionCharacters.end()) {
                error="GET_CHAR_ARMOUR requires an existing character.";return -1;
            }
            armour=character->second.armour;
        }
        if(!std::isfinite(armour)) {error="Character armour is not finite.";return -1;}
        return output(1,int32_t(std::clamp(std::trunc(armour),0.0f,32767.0f)))?1:-1;
    }
    if(model) {
        int32_t id=-1;
        if(args[0].bits==1) {
            if(world.modelIdByName) {
                id=world.modelIdByName("player");
                if(id<0)id=world.modelIdByName("plr");
            }
        } else {
            const auto character=world.missionCharacters.find(args[0].bits);
            if(character!=world.missionCharacters.end())id=character->second.model;
        }
        return matches(id>=0&&id==args[1].bits);
    }
    if(safety) {
        if(command==0x0616)return matches(world.playerCutsceneSafe);
        if(args[0].bits!=0) {error="MAKE_PLAYER_SAFE_FOR_CUTSCENE requires player zero.";return -1;}
        if(!world.playerCutsceneSafe) {
            world.playerCutscenePreviousControl=world.control;
            world.playerCutscenePreviousPoliceIgnore=world.policeIgnorePlayer;
            world.playerCutscenePreviousDamageProofs=world.playerDamageProofs;
            world.playerCutsceneSafe=true;
        }
        world.control=false;
        world.policeIgnorePlayer=true;
        world.playerDamageProofs=31;
        return 1;
    }
    if(seat) {
        const auto vehicle=world.missionVehicles.find(args[0].bits);
        if(vehicle==world.missionVehicles.end()||vehicle->second.deleted) {
            error="Passenger-seat query needs a valid vehicle.";return -1;
        }
        const int seatIndex=args[1].bits;
        const int capacity=world.vehiclePassengerCapacity?
            world.vehiclePassengerCapacity(args[0].bits):3;
        if(seatIndex<0||seatIndex>=capacity)return command==0x0436?matches(false):
            (output(2,-1)?1:-1);
        int32_t occupant=-1;
        if(world.inVehicle&&world.occupiedMissionVehicle==args[0].bits&&
            world.playerPassengerSeat==seatIndex)occupant=1;
        for(const auto& entry:world.missionCharacters) {
            if(entry.second.vehicle==args[0].bits&&!entry.second.attached&&
                entry.second.passengerSeat==seatIndex) {
                occupant=entry.first;break;
            }
        }
        return command==0x0436?matches(occupant<0):(output(2,occupant)?1:-1);
    }
    if(sitting||policeVehicle) {
        const bool player=command==0x0447||command==0x0448||command==0x044F||
            args[0].bits==1;
        const int32_t vehicleHandle=player?world.occupiedMissionVehicle:
            ([&](){const auto character=world.missionCharacters.find(args[0].bits);
                return character==world.missionCharacters.end()?-1:character->second.vehicle;})();
        const bool occupied=player?world.inVehicle:vehicleHandle>=0&&
            world.missionCharacters.count(args[0].bits)&&
            !world.missionCharacters.at(args[0].bits).attached;
        if(command==0x044F)return matches(args[0].bits==0&&!world.inVehicle&&
            world.playerAttachedVehicle<0);
        if(command==0x0447||command==0x044D)return matches(occupied&&vehicleHandle==args[1].bits);
        if(command==0x0448||command==0x044E)return matches(occupied);
        if(!occupied)return matches(false);
        const auto vehicle=world.missionVehicles.find(vehicleHandle);
        if(vehicle==world.missionVehicles.end()||vehicle->second.deleted)return matches(false);
        const int modelId=vehicle->second.model;
        if (!world.modelIdByName) {
            error="Police vehicle predicate needs native model names.";return -1;
        }
        static const std::array<const char*,7> policeNames={
            "police","enforcer","fbi","barracks","rhino","polmav","police2"
        };
        for(const char* name:policeNames)if(world.modelIdByName(name)==modelId)return matches(true);
        return matches(false);
    }
    const auto vehicle=world.missionVehicles.find(args[0].bits);
    if(vehicle==world.missionVehicles.end()||vehicle->second.deleted)return matches(false);
    uint32_t flags=0;
    if(!world.vehicleHandlingFlags||!world.vehicleHandlingFlags(vehicle->second.model,flags)) {
        error="Vehicle classification requires its native handling data.";return -1;
    }
    const uint32_t appearance=flags&0xF0000u;
    return matches(command==0x05B0?appearance==0:
        command==0x05B1?appearance==0x10000u:
        command==0x05B2?appearance==0x20000u:appearance==0x40000u);
}

int executeLcsEntityCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(vm.vcs)return 0;
    if(command==0x00C2||(command>=0x02CF&&command<=0x02D1)) {
        if(!vm.environment) {error="LCS visibility query requires a live world.";return -1;}
        auto& world=*vm.environment;
        if(command==0x00C2) {
            std::array<Operand,4> operands{};
            for(auto& operand:operands)if(!readOperand(vm,operand)) {
                error="LCS point visibility query has truncated operands.";return -1;
            }
            std::array<float,3> point{operandFloat(operands[0]),operandFloat(operands[1]),operandFloat(operands[2])};
            const float radius=operandFloat(operands[3]);
            if(!std::all_of(point.begin(),point.end(),[](float value){return std::isfinite(value);})||
                !std::isfinite(radius)||!world.sphereOnScreen) {
                error="LCS point visibility requires finite geometry and an active camera.";return -1;
            }
            if(point[2]<=-100) {
                if(!world.groundHeightAt) {error="LCS point visibility requires the collision world.";return -1;}
                float ground=20;
                if(!world.groundHeightAt({point[0],point[1],1000},ground)||!std::isfinite(ground))ground=20;
                point[2]=ground;
            }
            updateScmCondition(vm,world.sphereOnScreen(point,radius),inverted);return 1;
        }
        Operand operand;
        if(!readOperand(vm,operand)) {error="LCS entity visibility query has a truncated handle.";return -1;}
        int32_t visible=-1;
        if(command==0x02CF&&world.vehicleOnScreen)visible=world.vehicleOnScreen(operand.bits);
        if(command==0x02D0&&world.characterOnScreen)visible=world.characterOnScreen(operand.bits==1?0:operand.bits);
        if(command==0x02D1&&world.objectOnScreen) {
            const auto object=std::find_if(world.objects.begin(),world.objects.end(),
                [&](const auto& entry){return entry.handle==operand.bits;});
            if(object!=world.objects.end())visible=world.objectOnScreen(*object)?1:0;
        }
        if(visible<0) {error="LCS entity visibility requires a live entity and camera projection.";return -1;}
        updateScmCondition(vm,visible!=0,inverted);return 1;
    }
    if(command==0x033A) {
        Operand enabled;
        if(!vm.environment||!readOperand(vm,enabled)) {error="LCS respray policy requires a live world.";return -1;}
        vm.environment->freeResprays=enabled.bits!=0;return 1;
    }
    if(command==0x04B1) {
        Operand player;
        if(!vm.environment||!readOperand(vm,player)||player.bits!=0) {
            error="LCS plane query requires the live player.";return -1;
        }
        auto& world=*vm.environment;
        bool inPlane=false;
        if(world.inVehicle) {
            const int vehicleClass=world.activeVehicleCameraClass?world.activeVehicleCameraClass():
                world.vehicleHandlingClass?world.vehicleHandlingClass(world.occupiedMissionVehicle):-1;
            if(vehicleClass<0) {error="LCS plane query requires the occupied vehicle's native handling flags.";return -1;}
            inPlane=vehicleClass==5;
        }
        updateScmCondition(vm,inPlane,inverted);return 1;
    }
    if(command==0x0212||command==0x021B||command==0x0225||command==0x022D||command==0x0247||command==0x0299||
        command==0x033D||command==0x03F2||command==0x03F8||command==0x046B||command==0x0490||
        command==0x02A1||command==0x02FD||command==0x02FE||command==0x039C||command==0x03A1||
        command==0x03B0||command==0x03CE||command==0x0428||command==0x049B) {
        if(!vm.environment) {error="LCS vehicle query requires a live world.";return -1;}
        const size_t count=command==0x03F8?3:command==0x03CE||command==0x0212||command==0x0225||command==0x02A1?1:2;
        std::array<Operand,3> values{};
        for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
            error="LCS vehicle command has truncated operands.";return -1;
        }
        auto& world=*vm.environment;
        const int32_t handle=values[0].bits;
        const auto found=world.missionVehicles.find(handle);
        if(found==world.missionVehicles.end()||found->second.deleted) {
            error="LCS vehicle command references an unknown vehicle.";return -1;
        }
        auto& vehicle=found->second;
        if(command==0x0212||command==0x02FD||command==0x02FE) {
            std::array<float,9> basis{};
            if(!world.vehicleBasis||!world.vehicleBasis(handle,basis)) {
                error="LCS vehicle forward query requires its live basis.";return -1;
            }
            if(command==0x0212) {updateScmCondition(vm,basis[8]>=0,inverted);return 1;}
            const float length=std::hypot(basis[3],basis[4]);
            const float component=command==0x02FD?basis[3]:basis[4];
            const float result=length>0?component/length:0;
            int32_t bits=0;std::memcpy(&bits,&result,sizeof(bits));
            if(!writeOperand(vm,values[1],bits)) {error="LCS vehicle forward query needs a writable output.";return -1;}
            return 1;
        }
        if(command==0x039C) {vehicle.siren=values[1].bits!=0;return 1;}
        if(command==0x021B) {vehicle.taxiLight=values[1].bits!=0;return 1;}
        if(command==0x0299) {vehicle.canRespray=values[1].bits!=0;return 1;}
        if(command==0x033D) {vehicle.visible=values[1].bits!=0;return 1;}
        if(command==0x03F2) {vehicle.upsideDownDamageDisabled=values[1].bits!=0;return 1;}
        if(command==0x03F8) {
            if(!values[1].reference||!values[2].reference) {error="LCS vehicle colours need two writable outputs.";return -1;}
            if(!writeOperand(vm,values[1],uint8_t(vehicle.primaryColor))||
                !writeOperand(vm,values[2],uint8_t(vehicle.secondaryColor))) {
                error="LCS vehicle colour output is invalid.";return -1;
            }
            return 1;
        }
        if(command==0x046B) {vehicle.fastLane=values[1].bits!=0;vehicle.lanePolicySet=true;return 1;}
        if(command==0x0490) {vehicle.routeSeed=uint16_t(values[1].bits);vehicle.routeSeedChanged=true;return 1;}
        if(command==0x0225||command==0x022D) {
            updateScmCondition(vm,command==0x0225?vehicle.bombType!=0:vehicle.bombType==values[1].bits,inverted);
            return 1;
        }
        if(command==0x0247) {storylandInstallVehicleBomb(world,handle,vehicle,values[1].bits);return 1;}
        if(command==0x02A1) {
            const int vehicleClass=world.vehicleHandlingClass?world.vehicleHandlingClass(handle):-1;
            if(vehicleClass<0) {error="LCS boat query requires native handling flags.";return -1;}
            updateScmCondition(vm,vehicleClass==4,inverted);return 1;
        }
        if(command==0x03B0) {vehicle.strong=values[1].bits!=0;return 1;}
        if(command==0x03CE) {updateScmCondition(vm,vehicle.visiblyDamaged,inverted);return 1;}
        if(command==0x0428) {
            const float traction=operandFloat(values[1]);
            if(!std::isfinite(traction)) {error="LCS vehicle traction requires a finite multiplier.";return -1;}
            vehicle.tractionMultiplier=traction;return 1;
        }
        if(!world.vehicleNativeType) {error="LCS vehicle command requires native vehicle type metadata.";return -1;}
        const int type=world.vehicleNativeType(handle);
        if(type<0) {error="LCS vehicle command could not resolve native vehicle type.";return -1;}
        if(command==0x03A1) {
            if(type==0||type==5)vehicle.watertight=values[1].bits!=0;
            return 1;
        }
        int32_t wheel=values[1].bits;
        const size_t wheelCount=type==5?2:4;
        bool burst=false;
        if(wheel==4)burst=std::any_of(vehicle.tyreStatus.begin(),vehicle.tyreStatus.begin()+wheelCount,
            [](uint8_t status){return status==1;});
        else {
            if(type==5&&(wheel==2||wheel==3))wheel-=2;
            if(wheel<0||size_t(wheel)>=wheelCount) {error="LCS tyre index is outside its native wheel array.";return -1;}
            burst=vehicle.tyreStatus[size_t(wheel)]==1;
        }
        updateScmCondition(vm,burst,inverted);return 1;
    }
    if(command>=0x04F2&&command<=0x04F4) {
        if(!vm.environment) {error="LCS animation streaming requires a live world.";return -1;}
        std::string bank;
        if(!readScmText(vm,bank)||bank.empty()) {error="LCS animation streaming requires an eight-byte bank name.";return -1;}
        std::transform(bank.begin(),bank.end(),bank.begin(),[](unsigned char character) {
            return character>='A'&&character<='Z'?char(character+32):char(character);
        });
        auto& world=*vm.environment;
        const auto clips=storylandLcsAnimationBankClips(bank);
        const bool special=bank=="ped"||!clips.empty();
        if(command==0x04F3) {
            bool loaded=false;
            if(special) {
                if(!world.lcsAnimationDuration) {error="LCS animation-bank status is unavailable.";return -1;}
                if(bank=="ped")loaded=world.lcsAnimationDuration(bank,"idle_stance")>0;
                else loaded=std::all_of(clips.begin(),clips.end(),[&](const auto& clip) {
                    return world.lcsAnimationDuration(bank,clip)>0;
                });
            } else {
                if(!world.animationBankLoaded) {error="Animation-bank status is unavailable.";return -1;}
                loaded=world.animationBankLoaded(bank);
            }
            updateScmCondition(vm,loaded,inverted);return 1;
        }
        if(special) {
            if(!world.setLcsAnimationBankLoaded) {error="LCS animation-bank streaming is unavailable.";return -1;}
            const bool load=command==0x04F2;
            if(!world.setLcsAnimationBankLoaded(bank,load,error))return -1;
            if(load)world.lcsAnimationBanks.insert(bank);
            else {
                world.lcsAnimationBanks.erase(bank);
                for(auto it=world.lcsWaitAnimations.begin();it!=world.lcsWaitAnimations.end();) {
                    const auto* active=storylandLcsWaitAnimation(it->second.state);
                    if(active&&bank==active->bank) {
                        world.pedWaitStateMilliseconds.erase(it->first);
                        it=world.lcsWaitAnimations.erase(it);
                    } else ++it;
                }
            }
            return 1;
        }
        if(!world.animationBankAvailable||!world.animationBankAvailable(bank)) {
            error="Animation bank is absent from the native animation manager: "+bank;return -1;
        }
        if(command==0x04F2)world.requestedAnimationBanks.insert(bank);
        else {
            if(!world.removeAnimationBank) {error="Animation-bank removal is unavailable.";return -1;}
            if(world.animationRemovalDeferred&&world.animationRemovalDeferred(bank))return 1;
            world.requestedAnimationBanks.erase(bank);
            world.removeAnimationBank(bank);
        }
        return 1;
    }
    if(command==0x0429||command==0x048A||command==0x059F) {
        // SLUS214.23 queries metres, PC and Australian region respectively.
        updateScmCondition(vm,command==0x0429,inverted);
        return 1;
    }
    if(command==0x0298||command==0x029C||command==0x029D||command==0x0418||command==0x0419) {
        if(!vm.environment) {error="LCS player or statistics command requires a live world.";return -1;}
        auto& world=*vm.environment;
        if(command==0x029C) {world.modelsKilledByPlayer.clear();return 1;}
        Operand first,second;
        if(!readOperand(vm,first)||(command!=0x0298&&!readOperand(vm,second))) {
            error="LCS player or statistics command has truncated operands.";return -1;
        }
        if(command==0x0298) {
            if(!writeOperand(vm,first,world.controllerMode)) {error="Controller mode requires a writable output.";return -1;}
            return 1;
        }
        if(command==0x029D) {
            if(first.bits<0||first.bits>=240) {error="LCS killed-model index is outside its native table.";return -1;}
            const auto found=world.modelsKilledByPlayer.find(first.bits);
            const uint16_t raw=found==world.modelsKilledByPlayer.end()?0:uint16_t(found->second);
            const int32_t count=raw>=0x8000?int32_t(raw)-0x10000:int32_t(raw);
            if(!writeOperand(vm,second,count)) {error="Killed-model count requires a writable output.";return -1;}
            return 1;
        }
        if(first.bits!=0) {error="LCS recovery permission requires player zero.";return -1;}
        if(command==0x0418)world.getOutOfJailFree=second.bits!=0;
        else world.freeHealthCare=second.bits!=0;
        return 1;
    }
    if(command==0x04A8||command==0x04A9||(command>=0x04B5&&command<=0x04BC)) {
        Operand left,right;
        if(!readOperand(vm,left)||!readOperand(vm,right)) {
            error="LCS integer comparison has truncated operands.";return -1;
        }
        const bool referenceOnRight=command==0x04B7||command==0x04B8||
            command==0x04BB||command==0x04BC;
        const Operand& variable=referenceOnRight?right:left;
        const bool global=command==0x04A8||command==0x04B5||command==0x04B7||
            command==0x04B9||command==0x04BB;
        if(!variable.reference||variable.global!=global) {
            error="LCS integer comparison requires the native variable kind.";return -1;
        }
        const bool result=command==0x04A8||command==0x04A9?left.bits==right.bits:
            command<=0x04B8?left.bits>right.bits:left.bits>=right.bits;
        updateScmCondition(vm,result,inverted);
        return 1;
    }
    const int restored=executeLcsRestoredPlayerCommands(vm,command,inverted,error);
    if(restored)return restored;
    if(command==0x023A||command==0x023B||command==0x023C) {
        const size_t count=command==0x023B?2:3;
        std::array<Operand,3> values{};
        for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
            error="LCS gang configuration has truncated operands.";return -1;
        }
        if(!vm.environment) {error="LCS gang configuration requires a live population.";return -1;}
        // CGangs takes a native signed short gang index and stores full
        // 32-bit model/weapon identifiers. Convert before checking the pool.
        const int32_t rawGang=int32_t(uint32_t(values[0].bits)&0xFFFFu);
        const int32_t gangIndex=rawGang>=0x8000?rawGang-0x10000:rawGang;
        auto& gangs=vm.environment->gangs;
        if(gangIndex<0||size_t(gangIndex)>=gangs.size()) {
            error="LCS gang index is outside the native nine-gang pool.";return -1;
        }
        if(command!=0x023C&&(values[1].bits<=0||(command==0x023A&&values[2].bits<=0))) {
            error="LCS gang population requires positive native model identifiers.";return -1;
        }
        auto& gang=gangs[size_t(gangIndex)];
        if(command==0x023A)gang.pedModels={values[1].bits,values[2].bits};
        else if(command==0x023B)gang.carModel=values[1].bits;
        else gang.weapons={values[1].bits,values[2].bits};
        return 1;
    }
    if(command==0x03DF) {
        Operand handle;
        if(!vm.environment||!readOperand(vm,handle)) {
            error="SET_GARAGE_CAMERA_FOLLOWS_PLAYER requires a garage and live environment.";return -1;
        }
        auto& world=*vm.environment;
        if(std::none_of(world.garages.begin(),world.garages.end(),
            [&](const auto& garage){return garage.handle==handle.bits;})) {
            error="SET_GARAGE_CAMERA_FOLLOWS_PLAYER references an unknown garage.";return -1;
        }
        world.lcsGarageFollowPlayer.insert(handle.bits);return 1;
    }
    if(command==0x03D1||command==0x03D2||command==0x03D3) {
        const size_t count=command==0x03D1?3:1;
        std::array<Operand,3> values{};
        for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
            error="LCS stuck-car command has truncated operands.";return -1;
        }
        if(!vm.environment) {error="LCS stuck-car command requires a live world.";return -1;}
        auto& world=*vm.environment;
        if(command==0x03D1)
            return storylandAddLcsStuckCarCheck(world,values[0].bits,operandFloat(values[1]),
                uint32_t(values[2].bits),error)?1:-1;
        if(command==0x03D2)storylandRemoveLcsStuckCarCheck(world,values[0].bits);
        else updateScmCondition(vm,storylandIsLcsCarStuck(world,values[0].bits),inverted);
        return 1;
    }
    if(command==0x02D8||command==0x02D9||command==0x02E0) {
        const size_t count=command==0x02D8?4:command==0x02E0?2:1;
        std::array<Operand,4> values{};
        for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
            error="LCS boat command has truncated operands.";return -1;
        }
        if(!vm.environment) {error="Boat commands require a live native world.";return -1;}
        auto& world=*vm.environment;
        const auto found=world.missionVehicles.find(values[0].bits);
        if(found==world.missionVehicles.end()||found->second.deleted||!world.vehicleIsBoat||
            !world.vehicleIsBoat(values[0].bits)) {error="Boat command requires an existing native CBoat.";return -1;}
        auto& boat=found->second;
        if(!world.configureCarMission||!world.configureCarMission(values[0].bits,14,error)) {
            if(error.empty())error="Boat command requires native boat propulsion and handling.";return -1;
        }
        if(command==0x02E0) {
            uint8_t speed=0;
            if(!storylandLcsBoatCruiseByte(operandFloat(values[1]),speed)) {
                error="Boat cruise speed requires a finite value in (-1,256) that truncates to a native uint8.";return -1;
            }
            boat.cruiseSpeed=float(speed)*50.0f/60.0f;return 1;
        }
        std::array<float,3> destination{};
        if(command==0x02D8) {
            for(size_t axis=0;axis<3;++axis) {
                destination[axis]=operandFloat(values[axis+1]);
                if(!std::isfinite(destination[axis])) {error="Boat destination must be finite.";return -1;}
            }
            if(destination[2]<=-100&&(!world.waterHeightAt||
                !world.waterHeightAt(destination,destination[2])||!std::isfinite(destination[2]))) {
                error="Boat destination needs native water height for its low-Z sentinel.";return -1;
            }
        }
        boat.scriptStatus=3;boat.drivingToTarget=false;boat.autoPilotRoutePending=false;
        if(command==0x02D9) {
            boat.autoPilotMission=0;boat.engineOn=false;boat.cruiseSpeed=0;
        } else {
            boat.autoPilotMission=14;boat.engineOn=true;
            boat.targetX=destination[0];boat.targetY=destination[1];boat.targetZ=destination[2];
            boat.cruiseSpeed=std::max(50.0f/60.0f,boat.cruiseSpeed);
        }
        return 1;
    }
    if(const int remote=executeLcsRemoteCommand(vm,command,inverted,error))return remote;
    if(command==0x00DD) {
        if(!vm.isMissionThread)return 1;
        if(!vm.environment) {error="Mission cleanup requires a live world.";return -1;}
        cleanUpLcsMissionEntities(*vm.environment);vm.missionCleanupRequested=false;
        return 1;
    }
    if(command==0x0226) {
        Operand player,apply;
        if(!vm.environment||!readOperand(vm,player)||!readOperand(vm,apply)||player.bits!=0) {
            error="APPLY_BRAKES_TO_PLAYERS_CAR requires player zero and a brake flag.";return -1;
        }
        vm.environment->applyPlayerCarBrakes=apply.bits!=0;return 1;
    }
    if(command==0x033B) {
        Operand player,visible;
        if(!vm.environment||!readOperand(vm,player)||!readOperand(vm,visible)||player.bits!=0) {
            error="SET_PLAYER_VISIBLE requires player zero and a visibility flag.";return -1;
        }
        vm.environment->playerVisible=visible.bits!=0;return 1;
    }
    if(command==0x0486) {
        Operand multiplier;
        if(!vm.environment||!readOperand(vm,multiplier)||!std::isfinite(operandFloat(multiplier))) {
            error="SET_ENTER_CAR_RANGE_MULTIPLIER requires a finite value and a live world.";return -1;
        }
        // Native CPed stores this float assignment in an unsigned 16-bit field.
        const float value=std::trunc(std::fmod(operandFloat(multiplier),65536.0f));
        vm.environment->lcsEnterCarRangeMultiplier=uint16_t(int32_t(value));return 1;
    }
    if(command==0x03F9) {
        Operand enabled;
        if(!readOperand(vm,enabled)||!vm.environment||!vm.environment->setAllCarsDamageable) {
            error="SET_ALL_CARS_CAN_BE_DAMAGED requires the live vehicle pool.";return -1;
        }
        auto& world=*vm.environment;
        const bool canBeDamaged=enabled.bits!=0;
        for(auto& entry:world.missionVehicles) {
            auto& vehicle=entry.second;if(vehicle.deleted)continue;
            vehicle.canBeDamaged=canBeDamaged;
            if(!canBeDamaged&&std::hypot(vehicle.x-world.x,vehicle.y-world.y,vehicle.z-world.z)<4000) {
                vehicle.extinguishFireRequested=true;vehicle.health=std::max(300.0f,vehicle.health);
                vehicle.healthChangedByScript=true;
            }
        }
        world.setAllCarsDamageable(canBeDamaged);return 1;
    }
    if(command==0x032C||command==0x0543||command==0x05D7||command==0x0615) {
        const size_t count=command==0x05D7?5:6;
        std::array<Operand,6> values{};
        for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
            error="LCS vehicle area lookup has truncated operands.";return -1;
        }
        if(!values[count-1].reference||!vm.environment||!vm.environment->findScriptVehiclesInArea) {
            error="LCS vehicle area lookup requires the live vehicle pool and a writable output.";return -1;
        }
        std::array<float,4> bounds{};
        for(size_t index=0;index<bounds.size();++index) {
            bounds[index]=operandFloat(values[index]);
            if(!std::isfinite(bounds[index])) {error="LCS vehicle area bounds must be finite.";return -1;}
        }
        auto& world=*vm.environment;
        const int32_t model=command==0x05D7?-1:values[4].bits;
        uint32_t flags=command==0x05D7?0:
            StorylandMissionEnvironment::VehicleAreaRoad|StorylandMissionEnvironment::VehicleAreaAmbient;
        if(command==0x0615)flags|=StorylandMissionEnvironment::VehicleAreaAlive;
        const auto vehicles=world.findScriptVehiclesInArea(bounds,model,flags);
        int32_t result=-1;
        for(int32_t handle:vehicles) {
            const auto found=world.missionVehicles.find(handle);
            if(found==world.missionVehicles.end()||found->second.deleted)continue;
            result=handle;
            // 032C's native descending pool scan claims every matching car.
            // 0543 also scans to the end; 05D7 and 0615 stop at the first match.
            if(command==0x032C) {
                found->second.persistent=true;
                if(vm.isMissionThread)world.missionCleanupVehicles.insert(handle);
            }
            if(command==0x05D7||command==0x0615)break;
        }
        if(!writeOperand(vm,values[count-1],result)) {error="LCS vehicle handle output is invalid.";return -1;}
        return 1;
    }
    const int location=executeLcsLocationCommand(vm,command,inverted,error);
    if(location)return location;
    if(command==0x04E1) {
        if(!vm.environment) {error="Photograph query requires a live world.";return -1;}
        const bool taken=vm.environment->photographTaken;
        vm.environment->photographTaken=false;
        updateScmCondition(vm,taken,inverted);
        return 1;
    }
    if(command==0x01F0||command==0x03E3) {
        Operand value;
        if(!readOperand(vm,value)||!std::isfinite(operandFloat(value))||!vm.environment) {
            error="Population density requires a live world and finite multiplier.";return -1;
        }
        if(command==0x01F0)vm.environment->carDensityMultiplier=operandFloat(value);
        else vm.environment->pedDensityMultiplier=operandFloat(value);
        return 1;
    }
    if(command==0x03BF||command==0x0430) {
        std::array<float,6> bounds{};
        for(float& coordinate:bounds) {
            Operand value;
            if(!readOperand(vm,value)||!std::isfinite(coordinate=operandFloat(value))) {
                error="CLEAR_AREA_OF_CARS/CHARS requires six finite coordinates.";return -1;
            }
        }
        if(!vm.environment||!vm.environment->clearAmbientArea) {
            error="Area clearing requires the live population pools.";return -1;
        }
        for(size_t axis=0;axis<3;++axis)if(bounds[axis]>bounds[axis+3])std::swap(bounds[axis],bounds[axis+3]);
        vm.environment->clearAmbientArea(bounds,command==0x03BF);
        return 1;
    }
    size_t count=0;
    switch(command) {
    case 0x009A:count=6;break;
    case 0x0469:case 0x04F9:count=8;break;
    case 0x01DF:count=5;break;
    case 0x00A0:case 0x00A1:case 0x00A7:case 0x00AA:case 0x012F:
    case 0x01C0:case 0x01C1:case 0x01C3:case 0x01C4:case 0x02C7:case 0x0367:count=4;break;
    case 0x0216:case 0x023E:case 0x03A4:case 0x0435:case 0x0507:count=3;break;
    case 0x009C:case 0x00AD:case 0x00AF:case 0x00DE:case 0x00DF:case 0x00E0:case 0x00E1:case 0x00E2:case 0x00E3:
    case 0x011F:case 0x01D6:case 0x01D7:case 0x01D8:case 0x01D9:case 0x01DA:case 0x04C7:
    case 0x013C:case 0x0177:case 0x0178:case 0x0189:case 0x018A:case 0x01EE:case 0x01EF:
    case 0x020F:case 0x0213:case 0x0214:case 0x0215:case 0x0228:case 0x022B:case 0x0322:case 0x0323:case 0x03A3:
    case 0x0231:case 0x0232:case 0x0233:case 0x024A:case 0x02AE:case 0x02E8:case 0x0328:case 0x033C:case 0x03A7:case 0x03C5:case 0x03FA:
    case 0x0403:case 0x0446:case 0x044B:case 0x0471:case 0x04BF:case 0x04DC:case 0x051E:case 0x054F:count=2;break;
    case 0x009B:case 0x009F:case 0x00A2:case 0x00A6:case 0x00A8:case 0x00A9:case 0x00E4:
    case 0x011D:case 0x0121:case 0x012B:case 0x0197:case 0x019B:case 0x01C6:
    case 0x01C7:case 0x01C8:case 0x01C9:case 0x01CA:case 0x01CB:case 0x01CC:
    case 0x0234:case 0x0235:case 0x026C:case 0x02A4:case 0x02A5:case 0x03CF:case 0x03EE:case 0x0450:
    case 0x04CA:case 0x0504:case 0x0542:case 0x0553:case 0x0554:case 0x0572:case 0x057E:case 0x010D:case 0x046A:case 0x046C:case 0x046D:count=1;break;
    case 0x0405:case 0x040C:case 0x04C9:count=7;break;
    default:return 0;
    }
    const auto signature=vm.opcodeDefinitions.find(command);
    if(signature!=vm.opcodeDefinitions.end()&&signature->second.first!=int(count)) {
        error="LCS entity opcode signature disagrees with its game definition.";return -1;
    }
    std::array<Operand,8> values{};
    for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="LCS entity command has truncated operands.";return -1;
    }
    if(!vm.environment) {error="LCS entity command requires a live world.";return -1;}
    auto& world=*vm.environment;
    const auto real=[&](size_t index){return operandFloat(values[index]);};
    const auto store=[&](size_t index,int32_t value) {
        if(writeOperand(vm,values[index],value))return true;
        error="LCS entity command requires a writable output.";return false;
    };
    const auto storeReal=[&](size_t index,float value) {
        int32_t bits=0;std::memcpy(&bits,&value,sizeof(bits));return store(index,bits);
    };
    const auto finiteRange=[&](size_t first,size_t last) {
        for(size_t index=first;index<=last;++index)if(!std::isfinite(real(index))) {
            error="LCS entity coordinates and speeds must be finite.";return false;
        }
        return true;
    };
    const auto outputs=[&](size_t first,size_t last) {
        for(size_t index=first;index<=last;++index)if(!values[index].reference) {
            error="LCS entity command requires writable output variables.";return false;
        }
        return true;
    };
    int32_t handle=values[0].bits;
    if(command==0x0323||command==0x046D||command==0x0554) {
        const auto vehicle=world.missionVehicles.find(handle);
        if(vehicle==world.missionVehicles.end()||vehicle->second.deleted) {
            error="LCS vehicle damage query requires an existing vehicle.";return -1;
        }
        if(command==0x0323) {
            updateScmCondition(vm,storylandLcsWeaponDamageMatches(vehicle->second.lastWeaponDamage,values[1].bits),inverted);
        } else if(command==0x046D)vehicle->second.lastWeaponDamage=-1;
        else vehicle->second.lastDamageSource={};
        return 1;
    }
    if(command==0x01EE||command==0x01EF) {
        const auto vehicle=world.missionVehicles.find(handle);
        if(vehicle==world.missionVehicles.end()||vehicle->second.deleted) {
            error="LCS passenger query requires an existing vehicle.";return -1;
        }
        int32_t passengers=0;
        if(command==0x01EF) {
            if(!world.vehiclePassengerCapacity) {
                error="LCS passenger capacity requires native vehicle metadata.";return -1;
            }
            passengers=world.vehiclePassengerCapacity(handle);
            if(passengers<0) {error="LCS passenger capacity is unavailable.";return -1;}
        } else {
            if(world.inVehicle&&world.occupiedMissionVehicle==handle&&world.playerPassengerSeat>=0)++passengers;
            for(const auto& character:world.missionCharacters)
                if(!character.second.attached&&character.second.vehicle==handle&&character.second.passengerSeat>=0)
                    ++passengers;
        }
        return store(1,passengers)?1:-1;
    }
    if(command==0x0469||command==0x046A||command==0x04F9) {
        const auto ped=world.missionCharacters.find(handle);
        if(command==0x046A) {
            if(world.characterAttachments.count(handle))detachLcsScriptCharacter(world,handle);
            return 1;
        }
        if(handle==1) {
            error=command==0x04F9?"Player object attachment requires native player-control and camera support.":
                "Player attachment requires the native helicopter-cannon camera and control mode.";return -1;
        }
        if(ped==world.missionCharacters.end()) {error="ATTACH_CHAR_TO_CAR references an unknown character.";return -1;}
        if(ped->second.vehicle>=0&&!ped->second.attached)return 1; // Native bInVehicle guard.
        const bool objectParent=command==0x04F9;
        int32_t parentVehicle=values[1].bits;
        if(objectParent) {
            if(parentVehicle<=0) {error="ATTACH_CHAR_TO_OBJECT references an invalid object.";return -1;}
            parentVehicle=int32_t(-int64_t(parentVehicle)-1);
        } else {
            const auto car=world.missionVehicles.find(parentVehicle);
            if(car==world.missionVehicles.end()||car->second.deleted) {error="ATTACH_CHAR_TO_CAR references an unknown car.";return -1;}
        }
        if(!finiteRange(2,4)||!finiteRange(6,6)||real(6)<0) {
            if(error.empty())error="Character attachment needs finite offsets and a nonnegative rotation limit.";return -1;
        }
        std::array<float,16> frame{};
        auto frameCharacter=ped->second;frameCharacter.vehicle=parentVehicle;
        if(!storylandLcsAttachmentParentFrame(world,frameCharacter,frame)||
            !std::all_of(frame.begin(),frame.end(),[](float value){return std::isfinite(value);})) {
            error="Character attachment requires the actual native parent transform.";return -1;
        }
        auto attachment=world.characterAttachments.count(handle)?world.characterAttachments.at(handle):
            StorylandMissionEnvironment::CharacterAttachment{};
        attachment.type=uint16_t(values[5].bits);attachment.rotationLimit=real(6);
        if(!storylandEquipAttachmentWeapon(world,handle,values[7].bits,attachment,error))return -1;
        world.characterAttachments[handle]=attachment;
        auto& character=ped->second;character.vehicle=parentVehicle;character.attached=true;
        character.attachment={real(2),real(3),real(4)};character.passengerSeat=-1;
        character.walkingToTarget=false;character.animationId=-1;
        world.characterWanderTasks.erase(handle);world.characterVehicleTasks.erase(handle);
        world.pedAnimationRequests.erase(handle);world.lcsWaitAnimations.erase(handle);
        world.pedWaitStateMilliseconds.erase(handle);
        positionLcsAttachedCharacter(world,handle);return 1;
    }
    if(command==0x009A) {
        if(!finiteRange(2,4)||!outputs(5,5))return -1;
        if(handle<0||values[1].bits<0||world.missionCharacters.size()>=128) {
            error="CREATE_CHAR requires a valid character type, model and free pool slot.";return -1;
        }
        StorylandMissionEnvironment::Character character;
        character.canBeShotInVehicle=true;
        character.type=handle;character.model=values[1].bits;
        character.heedThreats = false;
        const auto threats = world.pedTypeThreats.find(character.type);
        if (threats != world.pedTypeThreats.end()) character.fearFlags = threats->second;
        if(world.resolvePedStats&&world.resolvePedStats(character.model,-1,character.pedStats))
            character.animationGroup=character.pedStats.animationGroup;
        character.x=real(2);character.y=real(3);character.z=real(4);
        if(character.z<=-100) {
            if(!world.groundHeightAt) {error="CREATE_CHAR needs ground collision for the supplied height.";return -1;}
            float height=20.0f;
            if(!world.groundHeightAt({character.x,character.y,1000},height))height=20.0f;
            if(!std::isfinite(height)) {error="CREATE_CHAR returned a non-finite ground height.";return -1;}
            character.z=height;
        }
        int32_t result=std::max(2,world.nextMissionCharacterHandle);
        while(result<INT32_MAX&&world.missionCharacters.count(result))++result;
        if(result==INT32_MAX) {error="CREATE_CHAR has exhausted character handles.";return -1;}
        world.missionCharacters.emplace(result,character);world.nextMissionCharacterHandle=result+1;
        if(vm.isMissionThread)world.missionCleanupCharacters.insert(result);
        return store(5,result)?1:-1;
    }
    if(command==0x00DF||command==0x012F||command==0x01C4||command==0x0215||command==0x02A4||
        command==0x0233||command==0x0235) {
        if(handle!=0) {error="LCS player command requires player zero.";return -1;}
        handle=1;
    }
    if(command==0x00DE||command==0x00DF||command==0x03C5) {
        if(!outputs(1,1))return -1;
        int32_t vehicle=-1;
        if(handle==1) {
            if(world.inVehicle) {
                vehicle=world.occupiedMissionVehicle;
                if(vehicle<0&&world.acquireOccupiedVehicle)vehicle=world.acquireOccupiedVehicle();
            }
        } else {
            const auto character=world.missionCharacters.find(handle);
            if(character==world.missionCharacters.end()) {error="Vehicle query references an unknown character.";return -1;}
            if(!character->second.attached)vehicle=character->second.vehicle;
        }
        const auto found=world.missionVehicles.find(vehicle);
        if(found==world.missionVehicles.end()||found->second.deleted)vehicle=-1;
        else if(vm.isMissionThread&&command!=0x03C5)registerLcsStoredVehicle(world,vehicle);
        if(handle==1&&world.inVehicle)world.occupiedMissionVehicle=vehicle;
        return store(1,vehicle)?1:-1;
    }
    if(command==0x010D||command==0x01C0||command==0x01C1||command==0x01C9||command==0x01CC||command==0x03CF||command==0x0405) {
        const auto found=std::find_if(world.objects.begin(),world.objects.end(),
            [&](const auto& object){return object.handle==handle;});
        if(command==0x03CF) {updateScmCondition(vm,found!=world.objects.end(),inverted);return 1;}
        if(command==0x010D) {
            if(found!=world.objects.end())world.objects.erase(found);
            world.invisibleScriptObjects.erase(handle);
            world.missionCleanupObjects.erase(handle);return 1;
        }
        if(command==0x01C9) {
            if(found!=world.objects.end())found->persistent=false;
            world.missionCleanupObjects.erase(handle);return 1;
        }
        if(found==world.objects.end()) {error="LCS object command references an unknown object.";return -1;}
        if(command==0x01CC) {
            found->persistent=true;world.missionCleanupObjects.erase(handle);return 1;
        }
        if(command==0x01C0) {
            if(!outputs(1,3))return -1;
            return storeReal(1,found->x)&&storeReal(2,found->y)&&storeReal(3,found->z)?1:-1;
        }
        if(command==0x01C1) {
            if(!finiteRange(1,3))return -1;
            found->x=real(1);found->y=real(2);found->z=real(3);return 1;
        }
        if(!finiteRange(1,3)||!outputs(4,6))return -1;
        const auto matrix=storylandScriptObjectMatrix(found->x,found->y,found->z,
            found->rotationX,found->rotationY,found->heading);
        std::array<float,3> position{};
        for(size_t axis=0;axis<3;++axis) {
            position[axis]=matrix[axis]*real(1)+matrix[axis+4]*real(2)+matrix[axis+8]*real(3)+matrix[axis+12];
            if(!std::isfinite(position[axis])) {error="Object offset produced non-finite coordinates.";return -1;}
        }
        return storeReal(4,position[0])&&storeReal(5,position[1])&&storeReal(6,position[2])?1:-1;
    }
    if(command==0x00A6||command==0x00A7||command==0x00A8||command==0x00A9||command==0x00AA||command==0x00AD||command==0x00AF||
        command==0x013C||command==0x018A||command==0x01C6||command==0x01C8||command==0x01CB||command==0x020F||
        command==0x02C7||command==0x02E8||command==0x0328||command==0x03A4||command==0x03A7||command==0x03FA||command==0x040C||
        command==0x0446||command==0x0471||command==0x04BF||command==0x051E) {
        const auto found=world.missionVehicles.find(handle);
        const bool exists=found!=world.missionVehicles.end()&&!found->second.deleted;
        if(command==0x013C||command==0x018A||command==0x01C6) {
            LcsScriptEntityPosition position;
            if(command==0x01C6)getLcsScriptEntityPosition(world,LcsScriptEntity::Vehicle,handle,position);
            const bool matches=exists&&(command==0x013C?found->second.model==values[1].bits:
                command==0x018A?found->second.health>float(values[1].bits):position.stopped);
            updateScmCondition(vm,matches,inverted);return 1;
        }
        if(command==0x00A6) {
            world.missionCleanupVehicles.erase(handle);
            world.vcsGarageStorableMissionVehicles.erase(handle);
            if(!exists)return 1;
            auto& vehicle=found->second;
            if(world.occupiedMissionVehicle==handle||world.playerAttachedVehicle==handle)detachLcsScriptCharacter(world,1);
            for(auto& entry:world.missionCharacters)if(entry.second.vehicle==handle)detachLcsScriptCharacter(world,entry.first);
            vehicle.deleted=true;vehicle.alive=false;vehicle.health=0;vehicle.speed=0;
            vehicle.drivingToTarget=false;vehicle.healthChangedByScript=true;vehicle.driver=-1;
            world.upsideDownCarChecks.erase(handle);return 1;
        }
        if(command==0x01C8) {
            if(exists)found->second.persistent=false;
            world.missionCleanupVehicles.erase(handle);return 1;
        }
        if(!exists) {error="LCS vehicle command references an unknown vehicle.";return -1;}
        auto& vehicle=found->second;
        if(command==0x051E) {vehicle.positionFrozen=values[1].bits!=0;return 1;}
        if(command==0x020F) {
            if(values[1].bits<0||values[1].bits>9||values[1].bits==6||values[1].bits==7||values[1].bits==8) {
                error="LOCK_CAR_DOORS special force-close/script-only states are not implemented.";return -1;
            }
            vehicle.doorLock=values[1].bits;return 1;
        }
        if(command==0x00AF) {
            const int32_t mission=uint8_t(values[1].bits);
            if(mission!=0&&mission!=1&&mission!=10&&mission!=11) {
                error="SET_CAR_MISSION requires an unsupported driving tactic; native missions 0, 1, 10 and 11 are implemented.";return -1;
            }
            if(!world.configureCarMission||!world.configureCarMission(handle,mission,error)) {
                if(error.empty())error="Car missions require native traffic and handling bindings.";return -1;
            }
            if(vehicle.autoPilotMission!=mission) {
                vehicle.autoPilotMission=mission;vehicle.autoPilotRoutePending=mission==1;
            }
            vehicle.drivingToTarget=false;return 1;
        }
        if(command==0x0328) {
            if(!world.vehicleIsBoat||!world.vehicleIsBoat(handle)) {error="ANCHOR_BOAT requires a native boat.";return -1;}
            vehicle.boatAnchored=values[1].bits!=0;return 1;
        }
        if(command==0x03FA) {
            vehicle.canBeDamaged=values[1].bits!=0;
            if(!vehicle.canBeDamaged) {
                vehicle.extinguishFireRequested=true;vehicle.health=std::max(300.0f,vehicle.health);
                vehicle.healthChangedByScript=true;
            }
            return 1;
        }
        if(command==0x00AA) {
            if(!outputs(1,3))return -1;
            return storeReal(1,vehicle.x)&&storeReal(2,vehicle.y)&&storeReal(3,vehicle.z)?1:-1;
        }
        if(command==0x0446)return store(1,vehicle.model)?1:-1;
        if(command==0x0471) {
            int32_t driver=vehicle.driver;
            if(world.inVehicle&&world.occupiedMissionVehicle==handle)driver=1;
            else if(driver!=1) {
                const auto character=world.missionCharacters.find(driver);
                if(character==world.missionCharacters.end()||!character->second.alive||
                    character->second.vehicle!=handle||character->second.passengerSeat>=0)driver=-1;
            }
            return store(1,driver)?1:-1;
        }
        if(command==0x02E8) {
            const float speed=world.vehicleOrientationAndSpeed&&vehicle.spawnedSlot>=0?
                world.vehicleOrientationAndSpeed(handle)[1]*50.0f:std::fabs(vehicle.speed);
            return storeReal(1,speed)?1:-1;
        }
        if(command==0x01CB) {
            vehicle.persistent=true;world.missionCleanupVehicles.erase(handle);return 1;
        }
        if(command==0x00A8) {
            if(!world.configureCarMission||!world.configureCarMission(handle,1,error)) {
                if(error.empty())error="CAR_WANDER_RANDOMLY requires a connected native traffic route.";return -1;
            }
            vehicle.autoPilotMission=1;vehicle.autoPilotRoutePending=true;vehicle.drivingToTarget=false;
            vehicle.cruiseSpeed=std::max(50.0f/60.0f,vehicle.cruiseSpeed);return 1;
        }
        if(command==0x00A9) {
            vehicle.drivingToTarget=false;vehicle.autoPilotMission=0;vehicle.autoPilotRoutePending=false;return 1;
        }
        if(command==0x03A7) {
            vehicle.scriptStatus=values[1].bits&31;
            if(vehicle.scriptStatus==4||vehicle.scriptStatus==5) {
                vehicle.drivingToTarget=false;vehicle.speed=0;vehicle.movedByScript=true;
            }
            return 1;
        }
        if(command==0x00AD||command==0x04BF) {
            if(!finiteRange(1,1))return -1;
            if(command==0x00AD)vehicle.cruiseSpeed=std::max(0.0f,real(1))*50.0f/60.0f;
            else {vehicle.speed=real(1)*50.0f/60.0f;vehicle.movedByScript=true;}
            return 1;
        }
        if(command==0x040C) {
            if(!finiteRange(1,3)||!outputs(4,6))return -1;
            const float angle=vehicle.heading*0.0174532925199433f,c=std::cos(angle),s=std::sin(angle);
            return storeReal(4,vehicle.x+c*real(1)-s*real(2))&&
                storeReal(5,vehicle.y+s*real(1)+c*real(2))&&storeReal(6,vehicle.z+real(3))?1:-1;
        }
        if(!finiteRange(1,command==0x03A4?2:3))return -1;
        if(command==0x03A4)vehicle.heading=std::atan2(-(real(1)-vehicle.x),real(2)-vehicle.y)*57.29577951308232f;
        else {
            vehicle.targetX=real(1);vehicle.targetY=real(2);vehicle.targetZ=real(3);vehicle.drivingToTarget=true;
            vehicle.autoPilotMission=0;vehicle.autoPilotRoutePending=false;
            vehicle.cruiseSpeed=std::max(50.0f/60.0f,vehicle.cruiseSpeed);vehicle.scriptStatus=2;
        }
        vehicle.movedByScript=true;return 1;
    }
    if(command==0x00E0||command==0x00E1||command==0x00E2||command==0x00E3||command==0x00E4) {
        const bool playerCommand=command==0x00E1||command==0x00E3;
        if(playerCommand&&handle!=0) {error="LCS player vehicle query requires player zero.";return -1;}
        if(playerCommand)handle=1;
        int32_t vehicleHandle=-1;
        if(handle==1) {if(world.inVehicle)vehicleHandle=world.occupiedMissionVehicle;}
        else {
            const auto character=world.missionCharacters.find(handle);
            if(character!=world.missionCharacters.end()&&character->second.alive&&!character->second.attached)
                vehicleHandle=character->second.vehicle;
        }
        bool matches=false;
        if(command==0x00E4)matches=handle==1?world.inVehicle:vehicleHandle>=0;
        else if(command==0x00E0||command==0x00E1)matches=vehicleHandle>=0&&vehicleHandle==values[1].bits;
        else if(handle==1)matches=world.inVehicle&&world.occupiedVehicleModel==values[1].bits;
        else {
            const auto vehicle=world.missionVehicles.find(vehicleHandle);
            matches=vehicle!=world.missionVehicles.end()&&!vehicle->second.deleted&&vehicle->second.model==values[1].bits;
        }
        updateScmCondition(vm,matches,inverted);return 1;
    }
    const bool player=handle==1;
    const auto found=world.missionCharacters.find(handle);
    const bool exists=player||found!=world.missionCharacters.end();
    const bool alive=player?world.health>0:exists&&found->second.alive&&found->second.health>0;
    if(command==0x0322||command==0x046C||command==0x0553) {
        if(!exists) {error="LCS character damage query requires an existing character.";return -1;}
        if(command==0x0322) {
            const int32_t weapon=player?world.playerLastWeaponDamage:found->second.lastWeaponDamage;
            updateScmCondition(vm,storylandLcsWeaponDamageMatches(weapon,values[1].bits),inverted);
        } else if(command==0x046C) {
            if(player)world.playerLastWeaponDamage=-1;
            else found->second.lastWeaponDamage=-1;
        } else if(player)world.playerDamageSource={};
        else found->second.lastDamageSource={};
        return 1;
    }
    if(command==0x04CA) {
        const bool photographed=exists&&world.photographedCharacters.erase(handle)!=0;
        updateScmCondition(vm,photographed,inverted);
        return 1;
    }
    if(command==0x057E) {
        if(!exists)return 1;
        if(player) {
            if(world.inVehicle)return 1;
            world.speed=0;
        } else {
            auto& character=found->second;
            if(character.vehicle>=0&&character.passengerSeat<0&&!character.attached)return 1;
            character.animationId=-1;character.animationLoop=false;character.animationBlend=1;
            character.walkingToTarget=false;character.lookAtCharacter=-1;
        }
        world.pedAnimationRequests.erase(handle);world.pedWaitStateMilliseconds.erase(handle);
        world.lcsWaitAnimations.erase(handle);
        world.characterVehicleTasks.erase(handle);
        return 1;
    }
    if(command==0x0572||command==0x00A2||command==0x011D) {
        updateScmCondition(vm,command==0x0572?exists:command==0x00A2?alive:!alive,inverted);return 1;
    }
    if(command==0x009B) {
        if(player) {error="DELETE_CHAR cannot remove the player.";return -1;}
        if(exists) {
            detachLcsScriptCharacter(world,handle);world.missionCharacters.erase(found);
            world.characterWanderTasks.erase(handle);world.mobilePhones.erase(handle);
            world.pedAnimationRequests.erase(handle);world.pedWaitStateMilliseconds.erase(handle);
            world.lcsWaitAnimations.erase(handle);
            world.characterVehicleTasks.erase(handle);
            world.characterFlags.erase(handle);world.characterSkins.erase(handle);
            for(auto look=world.scriptedPedLooks.begin();look!=world.scriptedPedLooks.end();) {
                if(look->first==handle||look->second.target==handle)look=world.scriptedPedLooks.erase(look);
                else ++look;
            }
            world.characterWeapons.erase(handle);world.characterCurrentWeapons.erase(handle);
            for(auto& other:world.missionCharacters)if(other.second.lookAtCharacter==handle)other.second.lookAtCharacter=-1;
        }
        world.missionCleanupCharacters.erase(handle);
        return 1;
    }
    if(command==0x01C7) {
        if(!player)releaseLcsScriptCharacter(world,handle);
        world.missionCleanupCharacters.erase(handle);return 1;
    }
    if(!exists) {error="LCS character command references an unknown character.";return -1;}
    auto* character=player?nullptr:&found->second;
    if(command==0x0403) {
        const auto money=uint16_t(values[1].bits);
        if(player) {world.playerPedMoney=money;world.playerPedMoneyGivenByScript=true;}
        else {character->money=money;character->moneyGivenByScript=true;}
        return 1;
    }
    if(command==0x03A3) {
        if(player)world.playerCantBeDraggedOut=values[1].bits!=0;
        else character->cantBeDraggedOut=values[1].bits!=0;
        return 1;
    }
    if(command==0x02AE) {
        if(player)world.playerOnlyDamagedByPlayer=values[1].bits!=0;
        else character->onlyDamagedByPlayer=values[1].bits!=0;
        return 1;
    }
    if(command==0x044B||command==0x054F) {
        const bool value=values[1].bits!=0;
        if(command==0x044B) {
            if(player)world.playerSuffersCriticalHits=value;
            else character->suffersCriticalHits=value;
        } else {
            if(player)world.playerCanBeShotInVehicle=value;
            else character->canBeShotInVehicle=value;
        }
        return 1;
    }
    if(command==0x04DC) {
        if(player)world.playerPositionFrozen=values[1].bits!=0;
        else character->positionFrozen=values[1].bits!=0;
        return 1;
    }
    if(command==0x00A0||command==0x0177||command==0x04C9||command==0x02A4||command==0x02A5||command==0x0450) {
        LcsScriptEntityPosition position;
        getLcsScriptEntityPosition(world,LcsScriptEntity::Character,handle,position);
        if(command==0x02A4||command==0x02A5||command==0x0450) {
            const bool matches=command==0x0450?!position.inVehicle:position.stopped;
            updateScmCondition(vm,matches,inverted);return 1;
        }
        if(command==0x0177)return storeReal(1,position.heading)?1:-1;
        if(command==0x00A0) {
            if(!outputs(1,3))return -1;
            return storeReal(1,position.coordinates[0])&&storeReal(2,position.coordinates[1])&&storeReal(3,position.coordinates[2])?1:-1;
        }
        if(!finiteRange(1,3)||!outputs(4,6))return -1;
        const float angle=position.heading*0.0174532925199433f,c=std::cos(angle),s=std::sin(angle);
        return storeReal(4,position.coordinates[0]+c*real(1)-s*real(2))&&
            storeReal(5,position.coordinates[1]+s*real(1)+c*real(2))&&storeReal(6,position.coordinates[2]+real(3))?1:-1;
    }
    if(command==0x00A1||command==0x012F||command==0x0367) {
        if(!finiteRange(1,3))return -1;
        std::array<float,3> position{real(1),real(2),real(3)};
        if(position[2]<=-100) {
            if(!world.groundHeightAt) {error="Character teleport needs ground collision for the supplied height.";return -1;}
            float height=20.0f;
            if(!world.groundHeightAt({position[0],position[1],1000},height))height=20.0f;
            if(!std::isfinite(height)) {error="Character teleport returned a non-finite ground height.";return -1;}
            position[2]=height;
        }
        if(command==0x012F||command==0x0367)detachLcsScriptCharacter(world,handle);
        if(command==0x00A1) {
            const int32_t vehicleHandle=player?(world.inVehicle?world.occupiedMissionVehicle:-1):
                (character->attached?-1:character->vehicle);
            const auto vehicle=world.missionVehicles.find(vehicleHandle);
            if(vehicle!=world.missionVehicles.end()&&!vehicle->second.deleted) {
                vehicle->second.x=position[0];vehicle->second.y=position[1];vehicle->second.z=position[2];
                vehicle->second.movedByScript=true;
            }
        }
        if(player) {world.x=position[0];world.y=position[1];world.z=position[2];}
        else {
            character->x=position[0];character->y=position[1];character->z=position[2];
            character->walkingToTarget=false;world.characterWanderTasks.erase(handle);
        }
        return 1;
    }
    if(command==0x0178) {
        if(!finiteRange(1,1))return -1;
        float heading=std::fmod(real(1),360.0f);if(heading<0)heading+=360.0f;
        if(player)world.heading=heading;else character->heading=heading;
        return 1;
    }
    if(command==0x0228||command==0x022B||command==0x0189) {
        const float health=player?world.health:character->health;
        if(command==0x0189) {updateScmCondition(vm,health>float(values[1].bits),inverted);return 1;}
        if(command==0x022B)return store(1,int32_t(std::clamp(double(health),double(INT32_MIN),double(INT32_MAX))))?1:-1;
        if(player)world.health=float(values[1].bits);
        else {
            character->health=float(values[1].bits);
            if(character->health<=0) {
                character->alive=false;character->walkingToTarget=false;character->movementSpeed=0;
                if(!character->deathRecorded) {
                    character->deathStartedAt=world.gameMilliseconds;character->deathRecorded=true;
                    character->animationGroup=0;character->animationId=13;
                    character->animationStartedAt=world.gameMilliseconds;character->animationBlend=4;
                    character->animationLoop=false;world.scriptedCharacterAnimations.erase(handle);
                }
                world.characterWanderTasks.erase(handle);
            }
        }
        return 1;
    }
    if(command==0x033C) {if(player)world.playerVisible=values[1].bits!=0;else character->visible=values[1].bits!=0;return 1;}
    if(command==0x01CA) {
        if(character)character->persistent=true;
        world.missionCleanupCharacters.erase(handle);return 1;
    }
    if(command==0x0234||command==0x0235) {
        const auto look=world.scriptedPedLooks.find(handle);
        if(look!=world.scriptedPedLooks.end())
            storylandStopScriptedPedLook(look->second,world.gameMilliseconds,player);
        return 1;
    }
    if(command==0x0231||command==0x0232||command==0x0233) {
        int32_t target=values[1].bits;
        if(command==0x0232) {
            if(target!=0) {error="CHAR_LOOK_AT_PLAYER_ALWAYS requires player zero.";return -1;}
            target=1;
        }
        if(target!=1&&!world.missionCharacters.count(target)) {
            error="Native looking references an unknown target character.";return -1;
        }
        storylandStartScriptedPedLook(world.scriptedPedLooks[handle],target,
            world.gameMilliseconds,command==0x0231);
        return 1;
    }
    if(command==0x024A) {
        if(player) {
            error="SET_ANIM_GROUP_FOR_CHAR player variant requires a player locomotion-group binding, which is not implemented.";
            return -1;
        }
        if(values[1].bits<0) {error="Animation group must be nonnegative.";return -1;}
        if(character)character->animationGroup=values[1].bits;
        const auto flags = world.characterFlags.find(handle);
        if (flags != world.characterFlags.end()) {
            flags->second &= uint16_t(~0x2000u);
            if (!flags->second) world.characterFlags.erase(flags);
        }
        world.requestedAnimationGroups.insert(values[1].bits);return 1;
    }
    if(command==0x0542) {
        world.pedWaitStateMilliseconds.erase(handle);world.pedAnimationRequests.erase(handle);
        world.lcsWaitAnimations.erase(handle);
        if(character) {character->animationId=-1;character->animationLoop=false;}
        return 1;
    }
    if(command==0x03EE) {
        const auto waiting=world.pedWaitStateMilliseconds.find(handle);
        const auto vehicleTask=world.characterVehicleTasks.find(handle);
        const bool inControl=alive&&(player?world.control&&!world.playerPositionFrozen:
            !character->attached&&!world.lcsWaitAnimations.count(handle)&&
            (vehicleTask==world.characterVehicleTasks.end()||!vehicleTask->second.animating)&&
            (waiting==world.pedWaitStateMilliseconds.end()||waiting->second<=0));
        updateScmCondition(vm,inControl,inverted);return 1;
    }
    if(command==0x0504||command==0x012B) {
        const bool matches=player?command==0x012B&&world.playerWalkTask.completed:
            character&&(command==0x0504?character->objective==0:character->objectiveCompleted);
        updateScmCondition(vm,matches,inverted);return 1;
    }
    if(command==0x01C3||command==0x01C4||command==0x0213||command==0x0214||command==0x0215) {
        float x=0,y=0;
        if(command==0x01C3||command==0x01C4) {
            if(!finiteRange(1,3))return -1;
            x=real(1);y=real(2);
        } else {
            if(command==0x0214&&values[1].bits!=0) {error="TURN_CHAR_TO_FACE_PLAYER requires player zero.";return -1;}
            LcsScriptEntityPosition target;
            if(!getLcsScriptEntityPosition(world,LcsScriptEntity::Character,command==0x0214?1:values[1].bits,target)) {
                error="Character facing target does not exist.";return -1;
            }
            x=target.coordinates[0];y=target.coordinates[1];
        }
        const float heading=std::atan2(-(x-(player?world.x:character->x)),y-(player?world.y:character->y))*57.29577951308232f;
        if(player)world.heading=heading;else character->heading=heading;
        return 1;
    }
    if(player&&command==0x0216) {
        if(!finiteRange(1,2))return -1;
        if(!world.groundHeightAt) {
            error="WALK_CHAR_TO_COORD requires the native ground-collision binding.";return -1;
        }
        float height=20.0f;
        if(!world.groundHeightAt({real(1),real(2),1000},height))height=20.0f;
        if(!std::isfinite(height)) {error="WALK_CHAR_TO_COORD returned a non-finite ground height.";return -1;}
        world.lcsEntityExtension.tasks.erase(1);
        world.playerVehicleEntryTarget=-1;world.playerVehicleEntryAsPassenger=false;
        world.playerWalkTask={true,false,real(1),real(2),height,.5f};
        world.playerNavigation.clearRoute();
        if(world.inVehicle)world.playerExitVehicleRequested=true;
        return 1;
    }
    if(player&&command==0x0121) {
        world.lcsEntityExtension.tasks.erase(1);
        world.playerWalkTask.clear();world.playerNavigation.clearRoute();
        world.playerVehicleEntryTarget=-1;world.playerVehicleEntryAsPassenger=false;
        world.playerExitVehicleRequested=false;
        return 1;
    }
    if(player&&command==0x04C7) {
        LcsScriptEntityPosition target;
        if(values[1].bits<2||!getLcsScriptEntityPosition(world,LcsScriptEntity::Character,values[1].bits,target)) {
            error="Character follow target must be another existing character.";return -1;
        }
        StorylandLcsEntityExtensionState::Task task;
        task.objective=13;task.target=values[1].bits;task.destination=target.coordinates;
        task.radius=target.inVehicle?3.0f:2.0f;
        world.lcsEntityExtension.tasks[1]=task;
        world.playerWalkTask={true,false,target.coordinates[0],target.coordinates[1],target.coordinates[2],.5f};
        world.playerNavigation.clearRoute();
        world.playerVehicleEntryTarget=-1;world.playerVehicleEntryAsPassenger=false;
        if(world.inVehicle)world.playerExitVehicleRequested=true;
        return 1;
    }
    if(!character) {error="LCS pedestrian task requires a non-player character.";return -1;}
    if(command==0x01D8||command==0x01D9||command==0x01DA||command==0x026C||command==0x0435) {
        const bool exiting=command==0x01D8||command==0x026C;
        const bool passenger=command==0x01D9||command==0x0435;
        const int32_t vehicleHandle=command==0x026C?character->vehicle:values[1].bits;
        StorylandMissionEnvironment::CharacterVehicleTask task;
        task.vehicle=vehicleHandle;task.exiting=exiting;
        const auto vehicle=world.missionVehicles.find(vehicleHandle);
        if((!exiting||character->vehicle>=0)&&(vehicle==world.missionVehicles.end()||vehicle->second.deleted)) {
            error="Pedestrian vehicle task references an unknown vehicle.";return -1;
        }
        if(passenger) {
            if(!world.vehiclePassengerCapacity) {error="Passenger tasks require native seat geometry.";return -1;}
            const int capacity=world.vehiclePassengerCapacity(vehicleHandle);
            if(capacity<1||capacity>8) {error="Vehicle has no supported passenger seats.";return -1;}
            int seat=command==0x0435?values[2].bits:-1;
            if(seat<0) {
                float nearest=std::numeric_limits<float>::infinity();
                for(int candidate=0;candidate<capacity;++candidate) {
                    if((candidate==0&&!character->allowFrontPassenger)||
                        (candidate>0&&world.lcsExtendedWorld.frontSeatOnlyCharacters.count(handle)))continue;
                    if(lcsPassengerSeatOccupied(world,handle,vehicleHandle,candidate))continue;
                    if(command==0x0435) {seat=candidate;break;}
                    StorylandMissionEnvironment::CharacterVehicleTask option;
                    option.vehicle=vehicleHandle;option.seat=candidate;
                    if(!prepareLcsCharacterVehicleTask(world,handle,option,error))return -1;
                    const float distance=std::hypot(character->x-option.door[0],character->y-option.door[1]);
                    if(distance<nearest) {nearest=distance;seat=candidate;}
                }
            }
            if(seat<0||seat>=capacity||lcsPassengerSeatOccupied(world,handle,vehicleHandle,seat)) {
                error="Passenger task has no free requested seat.";return -1;
            }
            task.seat=seat;
        }
        if(command==0x0435) {
            detachLcsScriptCharacter(world,handle);
            character->vehicle=vehicleHandle;character->passengerSeat=task.seat;
            vehicle->second.engineOn=true;
            character->x=vehicle->second.x;character->y=vehicle->second.y;character->z=vehicle->second.z;
            character->heading=vehicle->second.heading;character->walkingToTarget=false;
            character->objective=0;character->objectiveTarget=-1;
            world.characterVehicleTasks.erase(handle);world.characterWanderTasks.erase(handle);return 1;
        }
        if(exiting) {
            task.vehicle=character->vehicle;task.seat=character->passengerSeat;
        } else if(character->vehicle>=0&&
            (character->vehicle!=vehicleHandle||character->passengerSeat!=task.seat)) {
            task.destinationVehicle=vehicleHandle;task.destinationSeat=task.seat;
            task.vehicle=character->vehicle;task.seat=character->passengerSeat;task.exiting=true;
        }
        if(task.vehicle>=0&&!(character->vehicle==task.vehicle&&character->passengerSeat==task.seat&&!task.exiting)) {
            if(!prepareLcsCharacterVehicleTask(world,handle,task,error))return -1;
        }
        character->objective=command;character->objectiveTarget=vehicleHandle;character->objectiveCompleted=false;
        character->walkingToTarget=false;character->animationId=-1;character->animationLoop=false;
        world.characterWanderTasks.erase(handle);world.pedAnimationRequests.erase(handle);
        world.pedWaitStateMilliseconds.erase(handle);world.lcsWaitAnimations.erase(handle);
        world.characterVehicleTasks[handle]=std::move(task);return 1;
    }
    if(command==0x011F) {
        character->fearFlags |= uint32_t(values[1].bits);
        return 1;
    }
    if(command==0x009C) {
        world.characterVehicleTasks.erase(handle);
        auto& task=world.characterWanderTasks[handle];
        task.direction=values[1].bits>=0&&values[1].bits<8?values[1].bits:
            int32_t((uint32_t(handle)*1664525u+world.gameMilliseconds)%8u);
        task.node=UINT32_MAX;character->objective=0;character->walkingToTarget=false;
        character->objectiveCompleted=false;character->objectiveTarget=-1;
        character->animationId=-1;character->walkSpeed=1.25f;return 1;
    }
    if(command==0x009F||command==0x0121||command==0x0197||command==0x019B) {
        world.characterVehicleTasks.erase(handle);
        world.characterWanderTasks.erase(handle);character->walkingToTarget=false;character->animationId=-1;
        character->objective=command==0x0197||command==0x009F?1:command==0x019B?2:0;
        character->objectiveCompleted=false;character->objectiveTarget=-1;return 1;
    }
    if(command==0x01D6||command==0x01D7||command==0x04C7) {
        world.characterVehicleTasks.erase(handle);
        if(command==0x01D7&&values[1].bits!=0) {error="GOTO_PLAYER_ON_FOOT requires player zero.";return -1;}
        const int32_t target=command==0x01D7?1:values[1].bits;
        LcsScriptEntityPosition position;
        if(target==handle||!getLcsScriptEntityPosition(world,LcsScriptEntity::Character,target,position)) {
            error="Character movement target does not exist or is the character itself.";return -1;
        }
        world.characterWanderTasks.erase(handle);
        character->objective=command;character->objectiveTarget=target;character->objectiveCompleted=false;
        character->targetX=position.coordinates[0];character->targetY=position.coordinates[1];
        character->targetRadius=position.inVehicle?3.0f:2.0f;character->walkSpeed=command==0x04C7?1.25f:3.8f;
        character->walkingToTarget=true;character->animationId=-1;return 1;
    }
    if(command==0x0216||command==0x023E||command==0x0507||command==0x01DF) {
        if(!finiteRange(1,command==0x01DF?4:2))return -1;
        world.characterVehicleTasks.erase(handle);
        world.characterWanderTasks.erase(handle);
        character->targetX=command==0x01DF?real(1)*0.5f+real(3)*0.5f:real(1);
        character->targetY=command==0x01DF?real(2)*0.5f+real(4)*0.5f:real(2);
        character->walkSpeed=command==0x023E?3.8f:command==0x0507?6.1f:1.25f;
        character->walkingToTarget=true;character->objective=command;character->animationId=-1;
        character->objectiveCompleted=false;character->objectiveTarget=-1;
        character->targetRadius=command==0x01DF?
            std::max(std::fabs(real(3)-real(1)),std::fabs(real(4)-real(2)))*0.5f:0.5f;
        return 1;
    }
    error="LCS entity command has no implementation.";return -1;
}

void storylandAdvanceLcsDeadPedMoney(StorylandMissionEnvironment& world,uint32_t& randomState) {
    const auto random=[&]() {randomState=randomState*1664525u+1013904223u;return randomState>>16;};
    for(auto& entry:world.missionCharacters) {
        auto& ped=entry.second;
        if(ped.alive&&ped.health>0) {ped.deathDropsCreated=false;continue;}
        if(ped.deathDropsCreated)continue;
        // CPed::CreateDeadPedMoney: emergency-service models, occupied cars,
        // and mission peds without script-assigned purses never drop money.
        if((ped.model>=1&&ped.model<=6)||!ped.moneyGivenByScript||
            (ped.vehicle>=0&&!ped.attached)||ped.money<10) {ped.deathDropsCreated=true;continue;}
        if(!world.groundHeightAt)continue; // Wait for the actual collision binding.
        ped.deathDropsCreated=true;
        const int count=std::min(int(ped.money)/20+1,7),share=int(ped.money)/count;
        std::array<float,3> position{ped.x,ped.y,ped.z};
        for(int index=0;index<count;++index) {
            position[0]+=1.5f*std::sin(float(random()%256u)*3.14159265358979323846f/128);
            position[1]+=1.5f*std::cos(float(random()%256u)*3.14159265358979323846f/128);
            float ground=0;const bool found=world.groundHeightAt(position,ground);
            position[2]=ground+.5f;
            if(!found||!std::isfinite(ground))continue;
            const int32_t amount=share+int32_t(random()&3u);
            if(world.pickups.size()>=4096||world.nextPickupHandle==INT32_MAX)continue;
            StorylandMissionEnvironment::Pickup pickup;
            pickup.handle=world.nextPickupHandle++;pickup.type=8;pickup.modelNameOverride="money";
            pickup.x=position[0];pickup.y=position[1];pickup.z=position[2];pickup.ammunition=amount;
            pickup.area=world.area;pickup.expiresAt=world.gameMilliseconds+30000u;
            world.pickups.push_back(std::move(pickup));
        }
        ped.money=0;
    }
}

void storylandAdvanceLcsMissionTasks(StorylandMissionEnvironment& world,uint32_t elapsedMilliseconds,uint32_t& randomState) {
    if(world.coopGuest)return;
    for(auto look=world.scriptedPedLooks.begin();look!=world.scriptedPedLooks.end();) {
        const auto owner=world.missionCharacters.find(look->first);
        const bool alive=look->first==1?world.health>0:
            owner!=world.missionCharacters.end()&&owner->second.alive&&owner->second.health>0;
        const bool targetExists=look->second.target==1||world.missionCharacters.count(look->second.target);
        if(!alive||!targetExists) {look=world.scriptedPedLooks.erase(look);continue;}
        auto& task=look->second;
        if(task.active&&task.deadline&&task.deadline<world.gameMilliseconds)
            storylandStopScriptedPedLook(task,world.gameMilliseconds,look->first==1);
        if(task.restoring&&task.pose.restored)task.restoring=false;
        if(!task.active&&!task.restoring&&task.deadline<world.gameMilliseconds)
            look=world.scriptedPedLooks.erase(look);
        else ++look;
    }
    for(auto& entry:world.missionCharacters) {
        auto& character=entry.second;
        if(character.alive&&(character.health>0||character.behavior.leavingVehicleToDie))continue;
        character.alive=false;character.walkingToTarget=false;character.movementSpeed=0;character.movementState=0;
        if(!character.deathRecorded) {
            character.deathStartedAt=world.gameMilliseconds;character.deathRecorded=true;
        }
        world.characterWanderTasks.erase(entry.first);
        world.lcsWaitAnimations.erase(entry.first);world.pedWaitStateMilliseconds.erase(entry.first);
        world.pedAnimationRequests.erase(entry.first);world.mobilePhones.erase(entry.first);
    }
    storylandAdvanceLcsDeadPedMoney(world,randomState);
    advanceLcsCharacterVehicleTasks(world,elapsedMilliseconds);
    for(auto attachment=world.characterAttachments.begin();attachment!=world.characterAttachments.end();) {
        const int32_t handle=attachment++->first;
        if(!positionLcsAttachedCharacter(world,handle))detachLcsScriptCharacter(world,handle);
    }
    for(auto waiting=world.pedWaitStateMilliseconds.begin();waiting!=world.pedWaitStateMilliseconds.end();) {
        if(waiting->second>0)waiting->second=int32_t(std::max(int64_t(0),int64_t(waiting->second)-elapsedMilliseconds));
        if(waiting->second==0)waiting=world.pedWaitStateMilliseconds.erase(waiting);
        else ++waiting;
    }
    for(auto& entry:world.missionCharacters) {
        auto& character=entry.second;
        if(!character.alive||character.health<=0)continue;
        character.movementState=1;
        if(character.positionFrozen||character.stayInSamePlace||world.lcsWaitAnimations.count(entry.first))continue;
        const bool following=character.objective==0x01D6||character.objective==0x01D7||character.objective==0x04C7;
        const bool coordinates=character.objective==0x0216||character.objective==0x023E||
            character.objective==0x0507||character.objective==0x01DF;
        float distance=std::hypot(character.targetX-character.x,character.targetY-character.y);
        if(following) {
            LcsScriptEntityPosition target;
            if(!getLcsScriptEntityPosition(world,LcsScriptEntity::Character,character.objectiveTarget,target)) {
                character.walkingToTarget=false;character.objective=0;character.objectiveTarget=-1;
                character.lookAtCharacter=-1;continue;
            }
            character.targetX=target.coordinates[0];character.targetY=target.coordinates[1];
            character.targetRadius=target.inVehicle?3.0f:2.0f;
            distance=std::hypot(character.targetX-character.x,character.targetY-character.y,
                target.coordinates[2]-character.z);
            if(distance<=character.targetRadius) {
                character.walkingToTarget=false;character.objectiveCompleted=true;
                character.lookAtCharacter=character.objectiveTarget;
                character.heading=lookLcsCharacterHeading(character.heading,character.targetX-character.x,
                    character.targetY-character.y);
            } else {
                character.walkingToTarget=true;
                character.movementState=character.objective==0x04C7||distance<=3.0f?2:3;
            }
        } else if(coordinates) {
            if(character.objectiveCompleted||distance<=std::max(0.05f,character.targetRadius)) {
                character.walkingToTarget=false;character.objectiveCompleted=true;
            } else {
                character.walkingToTarget=true;
                character.movementState=character.objective==0x0507?4:character.objective==0x023E?3:2;
            }
        } else if(character.walkingToTarget||world.characterWanderTasks.count(entry.first)) {
            character.movementState=world.characterVehicleTasks.count(entry.first)?3:2;
        }
        if(character.movementState>=2) {
            const int32_t movement=character.movementState-2;
            if(world.animationMovementSpeed) {
                const float speed=world.animationMovementSpeed(character.animationGroup,movement);
                if(std::isfinite(speed)&&speed>0)character.walkSpeed=speed;
            } else if(following) {
                character.walkSpeed=movement==0?1.25f:3.8f;
            }
        }
        if((following||coordinates)&&character.walkingToTarget&&character.vehicle<0&&!character.attached) {
            const float horizontal=std::hypot(character.targetX-character.x,character.targetY-character.y);
            if(horizontal<=0.0001f)character.walkingToTarget=false;
            else if(world.pedestrianLineClear) {
                const float seconds=float(std::min(elapsedMilliseconds,100u))/1000.0f;
                const float fraction=std::min(1.0f,character.walkSpeed*seconds/horizontal);
                const std::array<float,3> next{character.x+(character.targetX-character.x)*fraction,
                    character.y+(character.targetY-character.y)*fraction,character.z+1.0f};
                character.walkingToTarget=world.pedestrianLineClear({character.x,character.y,character.z+1.0f},next);
            }
            if(!character.walkingToTarget)character.movementState=1;
        }
    }
}

int executeLcsWeaponCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    const bool give=command==0x01B6||command==0x01B7;
    const bool add=command==0x0119;
    const bool set=command==0x017F||command==0x0180;
    const bool select=command==0x01BD||command==0x01BE;
    const bool has=command==0x0495||command==0x0496;
    const bool current=command==0x02DC||command==0x02DD;
    const bool getCurrent=command==0x0474||command==0x0475;
    const bool getAmmo=command==0x041E;
    const bool clear=command==0x0494;
    const bool getSlot=command==0x04BD;
    if(!give&&!add&&!set&&!select&&!has&&!current&&!getCurrent&&!getAmmo&&!clear&&!getSlot)return 0;
    if(!vm.environment) {error="LCS weapon command requires the live weapon inventory.";return -1;}

    const size_t count=getSlot?5:give||add||set||getAmmo?3:clear?1:2;
    std::array<Operand,5> values{};
    for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="LCS weapon command has a truncated operand.";return -1;
    }
    const bool playerCommand=command==0x01B6||command==0x01BD||command==0x017F||command==0x0495||
        command==0x02DC||command==0x0474||command==0x041E;
    auto& world=*vm.environment;
    if(playerCommand&&values[0].bits!=0) {error="LCS weapon command requires player zero.";return -1;}
    const bool player=playerCommand||values[0].bits==1;
    if(!player&&!world.missionCharacters.count(values[0].bits)) {
        error="LCS weapon command references an unknown character.";return -1;
    }
    auto& inventory=player?world.playerWeapons:world.characterWeapons[values[0].bits];
    int32_t& currentWeapon=player?world.currentWeapon:world.characterCurrentWeapons[values[0].bits];
    if(getCurrent) {
        if(!writeOperand(vm,values[1],currentWeapon)) {
            error="GET_CURRENT_WEAPON requires a writable output.";return -1;
        }
        return 1;
    }
    if(clear) {
        for(auto& entry:inventory) {
            entry.second.owned=entry.first==0;
            entry.second.ammunition=0;
        }
        auto& fists=inventory[0];fists.slot=0;fists.owned=true;
        currentWeapon=0;
        if(player) {world.weaponInventoryChanged=true;world.weaponSelectionRequested=true;}
        else {
            auto& character=world.missionCharacters.at(values[0].bits);
            character.weaponsChanged=true;character.weaponSelectionChanged=true;
        }
        return 1;
    }
    if(getSlot) {
        if(values[1].bits<1||values[1].bits>10||!values[2].reference||
            !values[3].reference||!values[4].reference) {
            error="GET_CHAR_WEAPON_IN_SLOT requires a one-based slot and three writable outputs.";
            return -1;
        }
        const int32_t slot=values[1].bits-1;
        int32_t type=0,ammunition=0,model=-1;
        for(const auto& entry:inventory)if(entry.second.owned&&entry.second.slot==slot) {
            type=entry.first;ammunition=entry.second.ammunition;model=entry.second.model;
            break;
        }
        if(!writeOperand(vm,values[2],type)||!writeOperand(vm,values[3],ammunition)||
            !writeOperand(vm,values[4],model)) {
            error="GET_CHAR_WEAPON_IN_SLOT could not write its outputs.";return -1;
        }
        return 1;
    }
    const int32_t type=values[1].bits;
    if(type<0||type>=37) {error="LCS weapon type is outside the native inventory ID range.";return -1;}
    const auto found=inventory.find(type);
    const bool owned=type==0||(found!=inventory.end()&&found->second.owned);
    if(has||current) {
        updateScmCondition(vm,current?currentWeapon==type:owned,inverted);
        return 1;
    }
    if(getAmmo) {
        const int32_t ammunition=owned&&found!=inventory.end()?found->second.ammunition:0;
        if(!writeOperand(vm,values[2],ammunition)) {
            error="GET_AMMO_IN_PLAYER_WEAPON requires a writable output.";return -1;
        }
        return 1;
    }
    if(select) {
        if(owned) {
            currentWeapon=type;
            if(player)world.weaponSelectionRequested=true;
            else world.missionCharacters.at(values[0].bits).weaponSelectionChanged=true;
        }
        return 1;
    }
    if(values[2].bits<0) {error="LCS weapon ammunition cannot be negative.";return -1;}
    int32_t targetType=type;
    if(!give&&!owned) {
        const auto definition=world.playerWeapons.find(type);
        if(definition==world.playerWeapons.end()||definition->second.slot<0)return 1;
        const auto sameSlot=std::find_if(inventory.begin(),inventory.end(),[&](const auto& entry) {
            return entry.second.owned&&entry.second.slot==definition->second.slot;
        });
        if(sameSlot==inventory.end())return 1;
        targetType=sameSlot->first;
    }
    auto& weapon=inventory[targetType];
    if(!player) {
        const auto definition=world.playerWeapons.find(targetType);
        if(definition!=world.playerWeapons.end()) {
            weapon.slot=definition->second.slot;
            weapon.model=definition->second.model;
        }
    }
    int64_t ammunition=set?values[2].bits:int64_t(owned?weapon.ammunition:0)+values[2].bits;
    if(!give&&!owned)ammunition=set?values[2].bits:int64_t(weapon.ammunition)+values[2].bits;
    if(give) {
        if(weapon.slot>=0)for(auto& entry:inventory)
            if(entry.first!=type&&entry.first!=0&&entry.second.slot==weapon.slot) {
                if(!owned&&entry.second.owned&&storylandLcsWeaponSlotMergesAmmo(weapon.slot))
                    ammunition+=entry.second.ammunition;
                entry.second.owned=false;entry.second.ammunition=0;
            }
        weapon.owned=true;
        currentWeapon=type;
        if(player)world.weaponSelectionRequested=true;
        else world.missionCharacters.at(values[0].bits).weaponSelectionChanged=true;
    }
    // GiveWeapon initializes a newly replaced slot directly; repeat grants and SetAmmo
    // use the native 99,999 cap. Use wide arithmetic before storing the VM's signed count.
    weapon.ammunition=int32_t(std::min<int64_t>(give&&!owned?INT32_MAX:99999,ammunition));
    if(player)world.weaponInventoryChanged=true;
    else world.missionCharacters.at(values[0].bits).weaponsChanged=true;
    return 1;
}

int executeLcsRadarCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    const bool legacy=command==0x0166||command==0x0167;
    const bool pickup=command==0x03E1;
    const bool display=command==0x0586;
    if(!legacy&&!pickup&&!display)return 0;
    if(!vm.environment) {error="LCS radar command requires the live world.";return -1;}
    std::array<Operand,4> values{};
    const size_t count=legacy?4:pickup?2:1;
    for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="LCS radar command has a truncated operand.";return -1;
    }
    auto& world=*vm.environment;
    if(display) {world.radarVisible=values[0].bits!=0;return 1;}
    if(!values[count-1].reference) {error="Adding a radar blip requires a writable handle.";return -1;}
    StorylandMissionEnvironment::RadarBlip blip;
    using Entity=StorylandMissionEnvironment::BlipEntity;
    blip.entity=pickup?Entity::Pickup:command==0x0166?Entity::Vehicle:Entity::Character;
    blip.entityHandle=blip.entity==Entity::Character&&values[0].bits==1?0:values[0].bits;
    if(legacy) {
        if(values[2].bits<0||values[2].bits>3) {error="LCS radar display mode must be from zero through three.";return -1;}
        const bool exists=command==0x0166?
            world.missionVehicles.count(values[0].bits)&&!world.missionVehicles.at(values[0].bits).deleted:
            values[0].bits==1||world.missionCharacters.count(values[0].bits);
        if(!exists) {error="LCS radar blip references an unknown entity.";return -1;}
        blip.color=uint32_t(values[1].bits);
        blip.display=values[2].bits;
    } else {
        const auto found=std::find_if(world.pickups.begin(),world.pickups.end(),
            [&](const auto& item){return item.handle==values[0].bits&&!item.removed;});
        if(found==world.pickups.end()) {error="LCS pickup blip references an unknown pickup.";return -1;}
        blip.color=6;
    }
    blip.handle=storylandAllocateMissionBlipHandle(world,false);
    if(!writeOperand(vm,values[count-1],blip.handle)) {error="LCS radar output cannot be written.";return -1;}
    if(blip.handle!=-1)world.radarBlips.push_back(blip);
    return 1;
}

int executeLcsWaitAnimationCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    const bool load=command==0x061C,unload=command==0x061D,wait=command==0x0377||command==0x0666;
    if(!load&&!unload&&!wait)return 0;
    if(!vm.environment) {error="LCS wait-state animation requires the live animation system.";return -1;}
    std::array<Operand,3> values{};
    const size_t count=wait?3:1;
    for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="LCS wait-state animation has a truncated operand.";return -1;
    }
    auto& world=*vm.environment;
    const int32_t state=values[wait?1:0].bits;
    const auto* definition=storylandLcsWaitAnimation(state);
    if(load||unload) {
        if(!definition||state<36) {
            if(unload)return 1; // Native unload ignores wait states outside the non-standard banks.
            if(!world.setLcsAnimationBankLoaded) {error="The base pedestrian animation bank is unavailable.";return -1;}
            if(!world.setLcsAnimationBankLoaded("ped",true,error))return -1;
            world.lcsAnimationBanks.insert("ped");return 1;
        }
        if(!world.setLcsAnimationBankLoaded) {error="Non-standard pedestrian animations require live bank streaming.";return -1;}
        if(!world.setLcsAnimationBankLoaded(definition->bank,load,error))return -1;
        if(load)world.lcsAnimationBanks.insert(definition->bank);
        else {
            world.lcsAnimationBanks.erase(definition->bank);
            for(auto item=world.lcsWaitAnimations.begin();item!=world.lcsWaitAnimations.end();) {
                const auto* active=storylandLcsWaitAnimation(item->second.state);
                if(active&&std::string(active->bank)==definition->bank) {
                    world.pedWaitStateMilliseconds.erase(item->first);item=world.lcsWaitAnimations.erase(item);
                } else ++item;
            }
        }
        return 1;
    }
    const int32_t handle=values[0].bits;
    const auto character=world.missionCharacters.find(handle);
    if(handle!=1&&character==world.missionCharacters.end()) {
        if(command==0x0666)return 1; // Native repeat variant explicitly checks the pool lookup.
        error="SET_CHAR_WAIT_STATE references an unknown character.";return -1;
    }
    if(state==0) {
        world.lcsWaitAnimations.erase(handle);world.pedWaitStateMilliseconds.erase(handle);world.pedAnimationRequests.erase(handle);
        if(character!=world.missionCharacters.end())character->second.animationId=-1;
        return 1;
    }
    if(!definition) {error="SET_CHAR_WAIT_STATE has no verified animation binding for state "+std::to_string(state)+".";return -1;}
    if(state==35&&values[2].bits<0) {error="The native simple hands-up wait state requires an explicit nonnegative duration.";return -1;}
    if(!world.setLcsAnimationBankLoaded||!world.lcsAnimationDuration) {
        error="SET_CHAR_WAIT_STATE requires live animation-bank streaming and clip timing.";return -1;
    }
    if(!world.setLcsAnimationBankLoaded(definition->bank,true,error))return -1;
    const float duration=world.lcsAnimationDuration(definition->bank,definition->clip);
    if(!std::isfinite(duration)||duration<=0) {error="The requested wait-state clip has no decoded duration.";return -1;}
    if(state==21||state==24) {
        for(const char* clip:{"seat_idle","seat_up"})if(!storylandLcsWaitClipMilliseconds(world.lcsAnimationDuration("ped",clip))) {
            error="Seated wait requires the complete decoded idle and stand-up animation sequence.";return -1;
        }
    }
    const auto previous=world.lcsWaitAnimations.find(handle);
    // BlendAnimation retains the live association's repeat flag when the same
    // standard/blended state is requested again; AddAnimation creates a new one.
    const bool repeat=command==0x0666||(state<=45&&previous!=world.lcsWaitAnimations.end()&&
        previous->second.state==state&&previous->second.repeat);
    const int32_t milliseconds=state==21||state==23?(repeat?100000:storylandLcsWaitClipMilliseconds(duration)):
        state==24?(values[2].bits>=0?values[2].bits:storylandLcsDefaultSeatedWait(vm.randomState)):
        state==25?(values[2].bits>=0?values[2].bits:100000):
        state==3?(values[2].bits>=0?values[2].bits:storylandLcsDefaultRoadLookWait(vm.randomState)):
        state<36?(values[2].bits>=0?values[2].bits:3000):
        definition->repeat||repeat?-1:storylandLcsWaitClipMilliseconds(duration);
    world.lcsAnimationBanks.insert(definition->bank);
    world.lcsWaitAnimations[handle]={state,world.gameMilliseconds,milliseconds,world.nextPedAnimationSerial++,repeat};
    world.pedWaitStateMilliseconds[handle]=milliseconds;
    world.pedAnimationRequests.erase(handle);
    if(character!=world.missionCharacters.end())character->second.animationId=-1;
    return 1;
}

int executeLcsCameraCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    if(command==0x0378||command==0x03CD) {
        if(!vm.environment) {error="LCS camera placement requires the live player.";return -1;}
        vm.environment->camera.lcsFrontRequested=command==0x03CD;
        vm.environment->cameraBehindPlayerRequested=command==0x0378;
        return 1;
    }
    if(command==0x015C||command==0x015D||command==0x015E) {
        if(!vm.environment) {error="LCS camera tracking requires the live entity pools.";return -1;}
        Operand handle,mode,switchType;
        if(!readOperand(vm,handle)||!readOperand(vm,mode)||!readOperand(vm,switchType)) {
            error="LCS camera tracking has truncated entity, mode, or switch operands.";return -1;
        }
        if(mode.bits!=15) {error="LCS entity camera mode "+std::to_string(mode.bits)+" has no verified live renderer; fixed mode 15 is supported.";return -1;}
        if(switchType.bits!=1&&switchType.bits!=2) {error="LCS camera switch must interpolate (1) or jump (2).";return -1;}
        auto& world=*vm.environment;
        const int32_t kind=command==0x015C?1:command==0x015D?3:2;
        StorylandNativePoint target;
        if(!storylandResolveLcsCameraTarget(world,kind,handle.bits,target)) {
            error="LCS camera tracking references an unavailable entity or non-finite position.";return -1;
        }
        auto& camera=world.camera;
        camera.lcsTrackedKind=kind;camera.lcsTrackedHandle=kind==1?0:handle.bits;
        camera.targetX=target.x;camera.targetY=target.y;camera.targetZ=target.z;
        camera.active=true;camera.positionSet=true;camera.targetSet=true;camera.restoreRequested=false;
        camera.cutsceneOwned=false;camera.fieldOfView=70;camera.roll=0;camera.mode=switchType.bits;
        camera.transitioning=camera.interpolationScripted&&camera.interpolationMilliseconds>0&&switchType.bits==1;
        camera.transitionPending=camera.transitioning;
        return 1;
    }
    if(command==0x05AC||command==0x05AD) {
        if(!vm.environment) {error="LCS navigation arrow requires the live world.";return -1;}
        if(command==0x05AD) {vm.environment->lcsNavigationArrowVisible=false;return 1;}
        std::array<float,3> target{};
        for(auto& coordinate:target) {
            Operand value;
            if(!readOperand(vm,value)) {error="SET_NAVIGATION_ARROW has truncated coordinates.";return -1;}
            coordinate=operandFloat(value);
            if(!std::isfinite(coordinate)) {error="SET_NAVIGATION_ARROW requires finite coordinates.";return -1;}
        }
        vm.environment->lcsNavigationArrowTarget=target;
        vm.environment->lcsNavigationArrowVisible=true;
        return 1;
    }
    const bool interpolation=command==0x0465,position=command==0x0164,target=command==0x0165;
    const bool getPedZoom=command==0x060C,setPedZoom=command==0x060D;
    const bool freezePedZoom=command==0x0613;
    const bool restore=command==0x015F||command==0x02F0;
    if(!interpolation&&!position&&!target&&!restore&&!getPedZoom&&!setPedZoom&&!freezePedZoom)return 0;
    if(!vm.environment) {error="LCS camera command requires the live camera.";return -1;}
    auto& world=*vm.environment;auto& camera=world.camera;
    if(freezePedZoom) {
        Operand enabled;
        if(!readOperand(vm,enabled)) {error="FREEZE_PED_ZOOM_SWITCH has a truncated flag.";return -1;}
        camera.pedZoomSwitchFrozen=enabled.bits!=0;return 1;
    }
    if(getPedZoom||setPedZoom) {
        Operand value;
        if(!readOperand(vm,value)) {error="LCS pedestrian camera zoom has a truncated operand.";return -1;}
        const bool followPed=!camera.active&&!world.inVehicle&&!world.cutscene.playing;
        if(getPedZoom) {
            if(!writeOperand(vm,value,followPed?camera.pedZoomIndicator:-1)) {
                error="GET_CAMERA_PED_ZOOM_INDICATOR requires a writable output.";return -1;
            }
        } else if(followPed)camera.pedZoomIndicator=value.bits;
        return 1;
    }
    if(restore) {
        camera.active=false;camera.restoreRequested=true;camera.cutsceneOwned=false;
        camera.lcsTrackedKind=0;camera.lcsTrackedHandle=-1;
        camera.interpolationScripted=false;camera.transitioning=false;camera.transitionPending=false;
        camera.fieldOfView=70;camera.roll=0;return 1;
    }
    const size_t count=interpolation?2:position?6:4;
    std::array<Operand,6> values{};
    for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="LCS camera command has a truncated operand.";return -1;
    }
    const auto real=[&](size_t index){return operandFloat(values[index]);};
    for(size_t index=0;index<(interpolation?1:position?6:3);++index)if(!std::isfinite(real(index))) {
        error="LCS camera coordinates and interpolation percentage must be finite.";return -1;
    }
    if(interpolation) {
        camera.interpolationWeight=std::clamp(real(0)*0.01f,0.0f,1.0f);
        camera.interpolationMilliseconds=std::clamp(values[1].bits,0,600000);
        camera.interpolationScripted=true;return 1;
    }
    float targetZ=target?real(2):0;
    if(target&&targetZ<=-100) {
        if(!world.groundHeightAt) {error="LCS camera target requires ground collision for the supplied height.";return -1;}
        if(!world.groundHeightAt({real(0),real(1),1000},targetZ)||!std::isfinite(targetZ)) {
            error="LCS camera target has no ground collision at the requested coordinates.";return -1;
        }
    }
    camera.cutsceneOwned=false;camera.fieldOfView=70;camera.roll=0;
    if(position) {
        camera.x=real(0);camera.y=real(1);camera.z=real(2);
        camera.upX=real(3);camera.upY=real(4);camera.upZ=real(5);camera.positionSet=true;
    } else {
        camera.lcsTrackedKind=0;camera.lcsTrackedHandle=-1;
        camera.targetX=real(0);camera.targetY=real(1);camera.targetZ=targetZ;
        camera.mode=values[3].bits;camera.targetSet=true;camera.active=true;camera.restoreRequested=false;
        camera.transitioning=camera.interpolationScripted&&camera.interpolationMilliseconds>0&&camera.mode==1;
        camera.transitionPending=camera.transitioning;
    }
    return 1;
}

void storylandBeginLcsScriptTextFrame(StorylandMissionEnvironment& world) {
    if(!world.lcsTextCommandMode)return;
    world.lcsScriptTexts.clear();world.lcsNextScriptText={};
    if(world.lcsTextCommandMode==1)world.lcsTextCommandMode=0;
}

int executeLcsScriptTextCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    const bool display=command==0x0343||command==0x045F||command==0x0460;
    const bool style=command==0x0344||command==0x0345||command==0x0346||command==0x0347||
        command==0x0348||command==0x034A||command==0x034D||command==0x03E9;
    if(!display&&!style&&command!=0x03F5)return 0;
    if(!vm.environment) {error="LCS script text requires the live HUD.";return -1;}
    auto& world=*vm.environment;
    if(command==0x03F5) {
        Operand enabled;
        if(!readOperand(vm,enabled)) {error="USE_TEXT_COMMANDS has a truncated flag.";return -1;}
        world.textCommandsEnabled=enabled.bits!=0;
        world.lcsTextCommandMode=enabled.bits?2:1;
        return 1;
    }
    if(world.lcsScriptTexts.size()>=48) {error="LCS script text exceeds the native 48-line frame buffer.";return -1;}
    if(display) {
        Operand x,y;std::string key;
        if(!readOperand(vm,x)||!readOperand(vm,y)||!readScmText(vm,key)) {
            error="DISPLAY_TEXT has truncated coordinates or a GXT label.";return -1;
        }
        const float atX=operandFloat(x),atY=operandFloat(y);
        if(!std::isfinite(atX)||!std::isfinite(atY)) {error="DISPLAY_TEXT coordinates must be finite.";return -1;}
        std::vector<int32_t> numbers;
        const unsigned count=command==0x045F?1:command==0x0460?2:0;
        for(unsigned index=0;index<count;++index) {
            Operand number;
            if(!readOperand(vm,number)) {error="DISPLAY_TEXT has a truncated number.";return -1;}
            numbers.push_back(number.bits);
        }
        auto line=world.lcsNextScriptText;
        line.x=atX;line.y=atY;line.key=std::move(key);line.missionText=world.missionText;
        line.numbers=std::move(numbers);world.lcsScriptTexts.push_back(std::move(line));
        // Each native slot has its own default style; style does not carry to the next line.
        world.lcsNextScriptText={};return 1;
    }
    std::array<Operand,4> values{};
    const unsigned count=command==0x0344?2:command==0x0345?4:1;
    for(unsigned index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="LCS script text style has a truncated operand.";return -1;
    }
    auto& line=world.lcsNextScriptText;
    if(command==0x0344) {
        const float x=operandFloat(values[0]),y=operandFloat(values[1]);
        if(!std::isfinite(x)||!std::isfinite(y)) {error="Text scale must be finite.";return -1;}
        line.scaleX=x;line.scaleY=y;
    } else if(command==0x0345) {
        for(size_t index=0;index<4;++index)line.color[index]=uint8_t(values[index].bits);
    } else if(command==0x0348) {
        const float wrap=operandFloat(values[0]);
        if(!std::isfinite(wrap)) {error="Text wrap position must be finite.";return -1;}
        line.wrapX=wrap;
    } else {
        const bool enabled=values[0].bits!=0;
        if(command==0x0346)line.justify=enabled;
        else if(command==0x0347)line.centered=enabled;
        else if(command==0x034A)line.background=enabled;
        else if(command==0x034D)line.proportional=enabled;
        else line.rightJustify=enabled;
    }
    return 1;
}

int executeLcsTextCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    const int scriptText=executeLcsScriptTextCommand(vm,command,error);
    if(scriptText)return scriptText;
    if(command==0x0628||command==0x0629) {
        std::array<Operand,4> values{};
        if(!vm.environment) {error="LCS counter colours require the live HUD.";return -1;}
        for(auto& value:values)if(!readOperand(vm,value)) {
            error="LCS counter colour has a truncated component.";return -1;
        }
        auto& color=command==0x0628?vm.environment->counterColor:vm.environment->counterBackground;
        for(size_t index=0;index<color.size();++index)color[index]=uint8_t(values[index].bits);
        if(command==0x0628)vm.environment->counterColorPending=true;
        else vm.environment->counterBackgroundPending=true;
        return 1;
    }
    const bool help=command==0x03EA||command==0x0517||command==0x062D||command==0x062E||
        command==0x0673||command==0x0674||command==0x0675||command==0x0676;
    int numberCount=-1;
    const bool big=command==0x00BA||command==0x01E8||command==0x021C||command==0x021D||
        command==0x0372||command==0x0373;
    switch(command) {
    case 0x00BA:case 0x00BB:case 0x00BC:case 0x00BD:case 0x021C:case 0x0389:
    case 0x064C:case 0x0655:case 0x0656:case 0x0657:numberCount=0;break;
    case 0x01E8:case 0x01E9:case 0x01EA:case 0x01EB:case 0x021D:case 0x0658:numberCount=1;break;
    case 0x0301:case 0x0302:case 0x0303:case 0x0372:case 0x065F:numberCount=2;break;
    case 0x0304:case 0x0305:case 0x0373:numberCount=3;break;
    case 0x0307:numberCount=4;break;
    case 0x030D:numberCount=6;break;
    default:break;
    }
    const bool clearSmall=command==0x03F0;
    const bool clearBig=command==0x061A,clearAll=command==0x00BE;
    const bool clearKey=command==0x03DA||command==0x03DB;
    if(!help&&numberCount<0&&!clearSmall&&!clearBig&&!clearAll&&!clearKey)return 0;
    if(!vm.environment) {error="LCS text command requires the live HUD.";return -1;}
    auto& world=*vm.environment;
    if(clearSmall||clearBig||clearAll) {
        storylandClearLcsMessages(world,clearSmall||clearAll,clearBig||clearAll);
        return 1;
    }
    std::string label;
    if(!readScmText(vm,label)) {error="LCS text command has an invalid GXT label.";return -1;}
    if(clearKey) {storylandClearLcsMessageKey(world,label,command==0x03DB);return 1;}
    if(help) {
        const bool newHelp=world.helpMessage!=label||world.helpMessageMilliseconds==0;
        world.helpMessage=std::move(label);
        if(newHelp&&world.audioSubsystemEnabled&&world.frontendAudioEnabled&&world.scriptSounds.size()<4096)
            world.scriptSounds.push_back({197,{world.x,world.y,world.z},true});
        world.helpMessagePersistent=command==0x0517||command==0x062E||command==0x0674||command==0x0676;
        world.helpMessageMilliseconds=7000;
        return 1;
    }
    std::string stringKey;
    if(command==0x0389&&!readScmText(vm,stringKey)) {error="String print has an invalid inserted GXT label.";return -1;}
    std::array<Operand,8> values{};
    for(int index=0;index<numberCount+2;++index)if(!readOperand(vm,values[size_t(index)])) {
        error="LCS numbered print has a truncated operand.";return -1;
    }
    if(big&&(values[size_t(numberCount+1)].bits<1||values[size_t(numberCount+1)].bits>8)) {
        error="LCS big message style must be from one through eight.";return -1;
    }
    StorylandMissionEnvironment::LcsMessage message;
    message.key=std::move(label);message.missionText=world.missionText;message.stringKey=std::move(stringKey);
    for(int index=0;index<numberCount;++index)message.numbers.push_back(values[size_t(index)].bits);
    message.durationMilliseconds=uint32_t(values[size_t(numberCount)].bits);
    message.flags=big?0:uint16_t(values[size_t(numberCount+1)].bits);
    message.overrideFade=command==0x064C||command==0x0656;
    message.addToBrief=command!=0x0657&&command!=0x0658&&command!=0x065F;
    const bool append=command==0x00BB||command==0x01E9||command==0x021C||command==0x021D||
        command==0x0301||command==0x0304||command==0x0307||command==0x030D;
    const bool soon=command==0x00BD||command==0x01EB||command==0x0303;
    const std::string outcomeKey=message.key;
    const int32_t style=big?values[size_t(numberCount+1)].bits:0;
    const bool queued=storylandQueueLcsMessage(world,std::move(message),style,
        append?StorylandLcsMessagePriority::Append:soon?StorylandLcsMessagePriority::Soon:StorylandLcsMessagePriority::Now);
    if(queued&&big)vm.observeMissionMessage(outcomeKey,style);
    return 1;
}

int executeLcsRadioCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    if(command!=0x0370&&command!=0x0423&&command!=0x0523)return 0;
    if(!vm.environment) {error="LCS radio command requires the live audio environment.";return -1;}
    std::array<Operand,2> values{};
    const size_t count=command==0x0423?2:1;
    for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
        error="LCS radio command has a truncated operand.";return -1;
    }
    auto& world=*vm.environment;
    if(command==0x0370) {
        if(values[0].bits<0||values[0].bits>4) {error="LCS music mode must be from zero through four.";return -1;}
        if(world.stopMissionPassedTune)world.stopMissionPassedTune();
        world.scriptAudioMode=values[0].bits;
        world.radioEnabled=values[0].bits==1;
        world.radioChanged=true;
    } else if(command==0x0423) {
        world.radioChannel=values[0].bits;
        world.radioPosition=values[1].bits;
        world.radioChannelChanged=true;
    } else if(!writeOperand(vm,values[0],world.radioChannel)) {
        error="GET_RADIO_CHANNEL requires a writable output.";return -1;
    }
    return 1;
}

enum class LcsStatisticUpdate { Add, Increment, Maximum, MaximumFloat, Minimum };

int executeLcsStatisticCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(command==0x01FF) {
        Operand output;
        if(!vm.environment||!readOperand(vm,output)||
            !writeOperand(vm,output,vm.environment->killFrenzyStatus)) {
            error="READ_KILL_FRENZY_STATUS requires a live world and writable output.";return -1;
        }
        return 1;
    }
    if(command==0x040D) {
        Operand total;
        if(!vm.environment||!readOperand(vm,total)) {
            error="SET_TOTAL_NUMBER_OF_KILL_FRENZIES requires a live world and integer total.";return -1;
        }
        // CStats stores the supplied signed value without clamping it. Keep
        // the shared progression field and native statistics view consistent.
        vm.environment->totalRampages=total.bits;
        vm.environment->statistics["TotalNumberKillFrenzies"]=total.bits;
        return 1;
    }
    if(command==0x064F||command==0x0650) {
        if(!vm.environment) {error="Developer mode command requires the live world.";return -1;}
        if(command==0x064F)updateScmCondition(vm,vm.environment->developerMode,inverted);
        else {
            Operand enabled;
            if(!readOperand(vm,enabled)) {error="SET_DEVELOPER_FLAG requires an integer flag.";return -1;}
            vm.environment->developerMode=enabled.bits!=0;
        }
        return 1;
    }
    if(command==0x0311||command==0x0318||command==0x031A||command==0x059A||
        command==0x0646||command==0x0647) {
        if(!vm.environment) {error="LCS progression command requires the live statistics.";return -1;}
        auto& world=*vm.environment;
        if(command==0x0318) {++world.uniqueJumpsFound;return 1;}
        if(command==0x031A) {++world.taxiPassengersDroppedOff;return 1;}
        if(command==0x059A) {
            vm.registerMissionPassed({},true);
            return 1;
        }
        Operand value;
        if(!readOperand(vm,value)) {error="LCS progression command has a truncated integer.";return -1;}
        if(command==0x0311)world.progressMade=int32_t(uint32_t(world.progressMade)+uint32_t(value.bits));
        else if(command==0x0646)world.exportedCars+=uint32_t(value.bits);
        else world.totalExportedCars=value.bits;
        return 1;
    }
    struct StatisticCommand {
        uint16_t opcode;
        const char* key;
        LcsStatisticUpdate update;
        int32_t indices;
    };
    static constexpr StatisticCommand commands[]={
        {0x0313,"MaximumJumpDistance",LcsStatisticUpdate::MaximumFloat,0},
        {0x0314,"MaximumJumpHeight",LcsStatisticUpdate::MaximumFloat,0},
        {0x0315,"MaximumJumpFlips",LcsStatisticUpdate::Maximum,0},
        {0x0316,"MaximumJumpSpins",LcsStatisticUpdate::Maximum,0},
        {0x0317,"BestStuntJump",LcsStatisticUpdate::Maximum,0},
        {0x031B,"MoneyMadeWithTaxi",LcsStatisticUpdate::Add,0},
        {0x0406,"LivesSavedWithAmbulance",LcsStatisticUpdate::Increment,0},
        {0x0407,"CriminalsCaught",LcsStatisticUpdate::Increment,0},
        {0x0408,"HighestLevelAmbulanceMission",LcsStatisticUpdate::Maximum,0},
        {0x0409,"FiresExtinguished",LcsStatisticUpdate::Increment,0},
        {0x0433,"FastestTimes",LcsStatisticUpdate::Minimum,23},
        {0x0434,"HighestScores",LcsStatisticUpdate::Maximum,5},
        {0x04D4,"FashionBudget",LcsStatisticUpdate::Add,0},
        {0x052D,"WeaponBudget",LcsStatisticUpdate::Add,0},
        {0x052E,"PropertyBudget",LcsStatisticUpdate::Add,0},
        {0x0538,"Assassinations",LcsStatisticUpdate::Add,0},
        {0x0587,"BestPositions",LcsStatisticUpdate::Minimum,1},
        {0x059E,"HighestLevelFireMission",LcsStatisticUpdate::Maximum,0},
        {0x05E9,"CarsSold",LcsStatisticUpdate::Increment,0},
        {0x05EA,"MoneyMadeWithCarSales",LcsStatisticUpdate::Add,0},
        {0x05F1,"BikesSold",LcsStatisticUpdate::Increment,0},
        {0x05F2,"MoneyMadeWithBikeSales",LcsStatisticUpdate::Add,0},
        {0x05F3,"PackagesSmuggled",LcsStatisticUpdate::Increment,0},
        {0x05F4,"SmugglersWasted",LcsStatisticUpdate::Increment,0},
        {0x05F8,"MoneyMadeInCoach",LcsStatisticUpdate::Add,0},
        {0x05F9,"MoneyMadeCollectingTrash",LcsStatisticUpdate::Add,0},
        {0x05FA,"HitmenKilled",LcsStatisticUpdate::Increment,0},
        {0x05FB,"GuardianAngelMissionsPassed",LcsStatisticUpdate::Increment,0},
        {0x0603,"NumberOutfitChanges",LcsStatisticUpdate::Increment,0},
        {0x063A,"LongestFacePlantDist",LcsStatisticUpdate::MaximumFloat,0},
        {0x063B,"MaxSecondsOnCarnageLeft",LcsStatisticUpdate::Maximum,0},
        {0x063C,"MaxKillsOnRcTriad",LcsStatisticUpdate::Maximum,0},
        {0x063D,"HighestLevelSlashTv",LcsStatisticUpdate::Maximum,0},
        {0x063E,"MoneyMadeWithSlashTv",LcsStatisticUpdate::Add,0},
        {0x063F,"TotalKillsOnSlashTV",LcsStatisticUpdate::Add,0},
        {0x0640,"NoodlesDelivered",LcsStatisticUpdate::Add,0},
        {0x0641,"TopScrapyardChallengeScore",LcsStatisticUpdate::Maximum,0},
        {0x0642,"Top9mmMayhemScore",LcsStatisticUpdate::Maximum,0},
        {0x0643,"TopScooterShooterScore",LcsStatisticUpdate::Maximum,0},
        {0x0644,"MoneyMadeFromTourist",LcsStatisticUpdate::Add,0},
        {0x0645,"TouristsTakenToSpots",LcsStatisticUpdate::Add,0},
        {0x064A,"DirtBikeMostAir",LcsStatisticUpdate::Maximum,0},
        {0x0651,"TopWichitaWipeoutScore",LcsStatisticUpdate::Maximum,0}
    };
    const auto definition=std::find_if(std::begin(commands),std::end(commands),
        [&](const auto& entry){return entry.opcode==command;});
    if(definition==std::end(commands))return 0;
    if(!vm.environment) {error="LCS statistic command requires the live statistics.";return -1;}
    std::string key=definition->key;
    if(definition->indices) {
        Operand index;
        if(!readOperand(vm,index)||index.bits<0||index.bits>=definition->indices) {
            error="LCS statistic index is outside the native table.";return -1;
        }
        key+="["+std::to_string(index.bits)+"]";
    }
    double value=1;
    if(definition->update!=LcsStatisticUpdate::Increment) {
        Operand source;
        if(!readOperand(vm,source)) {error="LCS statistic command has a truncated value.";return -1;}
        value=definition->update==LcsStatisticUpdate::MaximumFloat?operandFloat(source):double(source.bits);
        if(!std::isfinite(value)) {error="LCS statistic value must be finite.";return -1;}
    }
    auto& statistics=vm.environment->statistics;
    const auto found=statistics.find(key);
    const double previous=found==statistics.end()?0:found->second;
    if(!std::isfinite(previous)) {error="Stored LCS statistic is non-finite.";return -1;}
    switch(definition->update) {
    case LcsStatisticUpdate::Increment:case LcsStatisticUpdate::Add:
        value=std::clamp(previous+value,double(INT32_MIN),double(INT32_MAX));break;
    case LcsStatisticUpdate::Maximum:case LcsStatisticUpdate::MaximumFloat:
        value=std::max(previous,value);break;
    case LcsStatisticUpdate::Minimum:
        if(found!=statistics.end()&&(command==0x0587||previous!=0))value=std::min(previous,value);
        break;
    }
    statistics[key]=value;
    return 1;
}

int executeLcsMissionCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(vm.vcs)return 0;
    if(command==0x03EC) {
        if(!vm.environment) {error="FLASH_HUD_OBJECT requires the live HUD.";return -1;}
        Operand value;
        if(!readOperand(vm,value)) {error="FLASH_HUD_OBJECT has a truncated item.";return -1;}
        vm.environment->lcsHudFlashItem=int16_t(value.bits);
        return 1;
    }
    const int weapon=executeLcsWeaponCommand(vm,command,inverted,error);
    if(weapon)return weapon;
    const int radar=executeLcsRadarCommand(vm,command,error);
    if(radar)return radar;
    const int camera=executeLcsCameraCommand(vm,command,error);
    if(camera)return camera;
    const int animation=executeLcsWaitAnimationCommand(vm,command,error);
    if(animation)return animation;
    const int text=executeLcsTextCommand(vm,command,error);
    if(text)return text;
    const int radio=executeLcsRadioCommand(vm,command,error);
    if(radio)return radio;
    return executeLcsStatisticCommand(vm,command,inverted,error);
}

// LCS PS2 script handlers at 0x229220, 0x1886cc, 0x1886ec,
// 0x188714, 0x188b6c, 0x188b94 and the direct-return cases.
int executeLcsNativeExtraCommands(StorylandMissionVm& vm, uint16_t command,
    std::string& error) {
    if (vm.vcs) return 0;

    switch (command) {
    case 0x03AE: // Save newline to debug file: native handler immediately returns.
    case 0x054A: // Remove everything for huge cutscene: native handler immediately returns.
    case 0x05C4:
    case 0x05D1:
    case 0x05D2:
        return 1;
    case 0x03AC: // Save integer to debug file.
    case 0x03AD: // Save float to debug file.
    case 0x0536: // Add stores knocked off.
    case 0x0539: // Add pizza deliveries.
    case 0x053B: // Add ice cream sold.
    case 0x0548: // Add Bloodring kills.
    case 0x0549: // Record longest Bloodring time.
        break;
    default:
        return 0;
    }

    Operand value;
    if (!readOperand(vm, value)) {
        error = "LCS native command has a truncated operand.";
        return -1;
    }
    // These native handlers read their argument but do not write anything.
    if (command == 0x03AC || command == 0x03AD || command == 0x0536) return 1;

    if (!vm.environment) {
        error = "LCS statistic command requires the live statistics.";
        return -1;
    }
    const char* key = command == 0x0539 ? "PizzasDelivered" :
        command == 0x053B ? "IceCreamSold" :
        command == 0x0548 ? "BloodRingKills" : "BloodRingTime";
    auto& statistics = vm.environment->statistics;
    const auto found = statistics.find(key);
    const double previous = found == statistics.end() ? 0.0 : found->second;

    if (command == 0x0539 || command == 0x053B) {
        if (!std::isfinite(previous) || std::fabs(previous) > std::numeric_limits<float>::max()) {
            error = "Stored LCS floating-point statistic is invalid.";
            return -1;
        }
        // The EE converts the integer to single precision before adding it.
        const double left = flushEeFloat(static_cast<float>(previous));
        const double right = roundEeFloat(static_cast<double>(value.bits));
        const double sum = left + right;
        const double virtualRight = sum - left;
        const double residual = (left - (sum - virtualRight)) + (right - virtualRight);
        statistics[key] = roundEeFloat(sum, residual);
        return 1;
    }

    if (!std::isfinite(previous) || previous < double(INT32_MIN) ||
        previous > double(INT32_MAX) || std::trunc(previous) != previous) {
        error = "Stored LCS integer statistic is invalid.";
        return -1;
    }
    const int32_t integer = static_cast<int32_t>(previous);
    if (command == 0x0549) {
        statistics[key] = std::max(integer, value.bits);
    } else {
        const uint32_t sum = uint32_t(integer) + uint32_t(value.bits);
        int32_t signedSum;
        std::memcpy(&signedSum, &sum, sizeof(signedSum));
        statistics[key] = signedSum;
    }
    return 1;
}

int executeLcsPresentationCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (vm.vcs) return 0;
    switch (command) {
    case 0x0003: case 0x00E6: case 0x0127: case 0x032F: case 0x0402:
    case 0x0457: case 0x0459: case 0x0468: case 0x04CC: case 0x05D5:
        break;
    default:
        return 0;
    }
    if (!vm.environment) {
        error = "LCS camera and input commands require the live world.";
        return -1;
    }
    auto& world = *vm.environment;
    if (command == 0x0457) {
        world.cameraControlEnabled = true;
        return 1;
    }
    if (command == 0x0459 || command == 0x0468) {
        std::array<Operand, 3> outputs;
        for (auto& output : outputs) {
            if (!readOperand(vm, output) || !output.reference) {
                error = "LCS camera coordinates require three writable outputs.";
                return -1;
            }
        }
        std::array<float, 3> eye{}, target{};
        if (!world.activeCameraVectors || !world.activeCameraVectors(eye, target)) {
            error = "LCS camera coordinates require the active camera.";
            return -1;
        }
        for (size_t axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(eye[axis]) || !std::isfinite(target[axis])) {
                error = "LCS camera coordinates are not finite.";
                return -1;
            }
        }
        if (command == 0x0468) {
            const float dx = target[0] - eye[0];
            const float dy = target[1] - eye[1];
            const float dz = target[2] - eye[2];
            const float length = std::hypot(dx, dy, dz);
            if (!(length > 0) || !std::isfinite(length)) {
                error = "LCS camera has no finite forward direction.";
                return -1;
            }
            // The native debug target is the source plus its unit front vector.
            eye = {eye[0] + dx / length, eye[1] + dy / length, eye[2] + dz / length};
        }
        for (size_t axis = 0; axis < 3; ++axis) {
            int32_t bits = 0;
            std::memcpy(&bits, &eye[axis], sizeof(bits));
            if (!writeOperand(vm, outputs[axis], bits)) return -1;
        }
        return 1;
    }
    Operand first;
    if (!readOperand(vm, first)) {
        error = "LCS camera or input command has a missing operand.";
        return -1;
    }
    if (command == 0x0003) {
        auto& shake = world.cameraShake;
        const float requested = float(first.bits) * 0.001f;
        const float elapsed = float(uint32_t(world.gameMilliseconds - shake.startedAt));
        const float previous = std::clamp(shake.strength - elapsed * 0.001f, 0.0f, 2.0f);
        if (previous < requested) {
            shake.strength = requested;
            shake.startedAt = world.gameMilliseconds;
            shake.sampled = false;
        }
        return 1;
    }
    if (command == 0x00E6) {
        Operand button;
        if (!readOperand(vm, button)) {
            error = "IS_BUTTON_PRESSED requires a pad and button.";
            return -1;
        }
        const uint16_t index = uint16_t(button.bits);
        const bool pressed = uint16_t(first.bits) == 0 && index < world.padState.size() &&
            world.padState[index] != 0;
        updateScmCondition(vm, pressed, inverted);
        return 1;
    }
    if (command == 0x0127) {
        if (first.bits != 0) {
            error = "Horn input requires player zero.";
            return -1;
        }
        size_t button = 18;
        if (world.controllerMode == 1) button = 4;
        else if (world.controllerMode == 3) button = 6;
        const bool supportedMode = world.controllerMode == 0 || world.controllerMode == 1 ||
            world.controllerMode == 3 || world.controllerMode == 4;
        updateScmCondition(vm, supportedMode && world.control && world.inVehicle &&
            !world.playerVehicleTransition && world.padState[button] != 0, inverted);
        return 1;
    }
    if (command == 0x0402) {
        Operand delay;
        if (!readOperand(vm, delay) || first.bits != 0 || delay.bits < 0 || delay.bits >= 10) {
            error = "Drunk input delay requires player zero and a pad-history slot from zero to nine.";
            return -1;
        }
        world.drunkInputDelay = uint8_t(delay.bits);
        return 1;
    }
    if (command == 0x04CC) {
        world.securityCamera = first.bits != 0;
        return 1;
    }
    if (command == 0x05D5) {
        world.wantsHud = first.bits != 0;
        return 1;
    }
    if (!world.activeCameraMode) {
        error = "Camera zoom requires the active camera controller.";
        return -1;
    }
    const int32_t mode = world.activeCameraMode();
    if (mode != 4 && mode != 18) return 1;
    const int16_t level = int16_t(first.bits);
    auto& zoom = world.cameraZoom;
    if (mode == 4) {
        static constexpr float offsets[] = {0, 2.5f, 3.5f};
        if (level >= 0 && level < 3) zoom.pedTarget = offsets[level];
        zoom.ped = true;
        return 1;
    }
    const int32_t kind = world.activeVehicleCameraClass ? world.activeVehicleCameraClass() : -1;
    if (kind < 1 || kind > 5) {
        error = "Vehicle camera zoom requires its native handling class.";
        return -1;
    }
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
    if (level >= 0 && level < 3) zoom.vehicleTarget = offsets[level][index];
    zoom.vehicle = true;
    return 1;
}

int executeLcsCharacterCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (vm.vcs) return 0;

    size_t count = 0;
    bool playerCommand = false;
    switch (command) {
    case 0x011B: // IS_PLAYER_STILL_ALIVE
    case 0x02E4: // IS_PLAYER_SHOOTING
    case 0x0505: // IS_PLAYER_WEARING, followed by an eight-byte label.
        count = 1; playerCommand = true; break;
    case 0x0227: // SET_PLAYER_HEALTH
    case 0x0335: // SET_PLAYER_NEVER_GETS_TIRED
    case 0x0336: // SET_PLAYER_FAST_RELOAD
    case 0x045C: // IS_PLAYER_TARGETTING_CHAR
    case 0x054B: // IS_PLAYER_TOUCHING_VEHICLE
    case 0x0562: // MAKE_PLAYER_FIRE_PROOF
    case 0x0563: // INCREASE_PLAYER_MAX_HEALTH
    case 0x0564: // INCREASE_PLAYER_MAX_ARMOUR
    case 0x066D: // GET_PLAYER_SAVED_WEAPON
        count = 2; playerCommand = true; break;
    case 0x054D: // CHECK_FOR_PED_MODEL_AROUND_PLAYER
        count = 6; playerCommand = true; break;
    case 0x02E5: // IS_CHAR_SHOOTING
    case 0x03A8: // IS_CHAR_MALE
    case 0x04B2: // IS_CHAR_IN_WATER
    case 0x0526: // IS_CHAR_DROWNING_IN_WATER
    case 0x05D0: // IS_PLAYER_USING_FIRST_PERSON_CAMERA; native ignores its operand.
        count = 1; break;
    case 0x0296: // SET_CHAR_HEED_THREATS
    case 0x0337: // SET_CHAR_BLEEDING
    case 0x0355: // SET_CHAR_STAY_IN_SAME_PLACE
    case 0x0488: // SET_CHAR_CEASE_ATTACK_TIMER
    case 0x048E: // SHUT_CHAR_UP
    case 0x0541: // SET_CHAR_IN_PLAYERS_GROUP_CAN_FIGHT
    case 0x0630: // SET_CHAR_MAX_HEALTH
    case 0x0631: // Native CPed::m_nShootTimer override.
    case 0x0632: // Native CPed::m_nAttackTimer override.
        count = 2; break;
    case 0x04FA: // SET_CHAR_AS_PLAYER_FRIEND; native ignores its player operand.
    case 0x0519: // SET_CHAR_CAN_BE_DAMAGED_BY_MEMBERS_OF_GANG
        count = 3; break;
    case 0x04AA: // GET_DEAD_CHAR_PICKUP_COORDS
        count = 4; break;
    default:
        return 0;
    }

    std::array<Operand, 6> values{};
    for (size_t index = 0; index < count; ++index) {
        if (!readOperand(vm, values[index])) {
            error = "LCS character command has a truncated or invalid operand.";
            return -1;
        }
    }
    std::string skin;
    if (command == 0x0505 && !readScmText(vm, skin)) {
        error = "IS_PLAYER_WEARING requires an eight-byte model label.";
        return -1;
    }
    if (!vm.environment) {
        error = "LCS character command requires the live world.";
        return -1;
    }
    auto& world = *vm.environment;
    const auto condition = [&](bool result) {
        updateScmCondition(vm, result, inverted);
        return 1;
    };
    const auto writable = [&](size_t index) {
        const auto& value = values[index];
        return value.reference && !value.arrayScratch &&
            (value.global ? value.variable < vm.globals.size() : value.variable < vm.locals.size());
    };
    const auto store = [&](size_t index, int32_t result) {
        if (writeOperand(vm, values[index], result)) return 1;
        error = "LCS character query requires a writable output.";
        return -1;
    };

    if (command == 0x05D0) {
        if (!world.activeCameraMode) {
            error = "First-person camera query requires the active camera controller.";
            return -1;
        }
        // SLUS-21423 0x293198 does not include VCS camera mode 47.
        const int32_t mode = world.activeCameraMode();
        return condition(mode == 7 || mode == 8 || mode == 34 || mode == 45 || mode == 46);
    }

    if (playerCommand) {
        if (values[0].bits != 0) {
            error = "LCS player command requires player index zero.";
            return -1;
        }
        switch (command) {
        case 0x011B:
            if (!world.nativePlayerState) {
                error = "IS_PLAYER_STILL_ALIVE requires the live player lifecycle.";
                return -1;
            }
            return condition(world.nativePlayerState() != 1);
        case 0x0227:
            // Retail 0x25C3D8 clamps above the byte limit, then truncates to a byte.
            world.health = float(uint8_t(std::min(int32_t(world.playerHealthLimit), values[1].bits)));
            return 1;
        case 0x02E4:
            if (!world.characterShooting) {
                error = "IS_PLAYER_SHOOTING requires the live shooting state.";
                return -1;
            }
            return condition(world.characterShooting(0));
        case 0x0335:
            world.playerNeverTired = values[1].bits != 0;
            return 1;
        case 0x0336:
            world.playerFastReload = values[1].bits != 0;
            return 1;
        case 0x045C: {
            const int32_t target = values[1].bits;
            const bool exists = target == 1 || world.missionCharacters.count(target) != 0;
            return condition(exists && world.playerTarget.kind == StorylandMissionEnvironment::TargetKind::Character &&
                world.playerTarget.handle == (target == 1 ? 0 : target));
        }
        case 0x0505: {
            if (!world.currentPlayerModel || !world.modelIdByName) {
                error = "IS_PLAYER_WEARING requires the loaded player model and native model metadata.";
                return -1;
            }
            for (char& character : skin)
                if (character >= 'A' && character <= 'Z') character = char(character + ('a' - 'A'));
            const int32_t requested = world.modelIdByName(skin);
            return condition(requested >= 0 && requested == world.currentPlayerModel());
        }
        case 0x054B: {
            const auto vehicle = world.missionVehicles.find(values[1].bits);
            if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) return condition(false);
            if (!world.characterTouchingVehicle) {
                error = "Player contact query requires the live physical contacts.";
                return -1;
            }
            return condition(world.characterTouchingVehicle(0, values[1].bits));
        }
        case 0x054D: {
            if (!world.currentPlayerModel || !world.allAmbientCharactersInArea) {
                error = "Nearby-model query requires the loaded player model and complete pedestrian pool.";
                return -1;
            }
            std::array<float, 3> center{world.x, world.y, world.z};
            const auto vehicle = world.missionVehicles.find(world.occupiedMissionVehicle);
            if (world.inVehicle && vehicle != world.missionVehicles.end())
                center = {vehicle->second.x, vehicle->second.y, vehicle->second.z};
            std::array<float, 6> bounds{};
            for (size_t axis = 0; axis < 3; ++axis) {
                const float radius = std::fabs(operandFloat(values[axis + 1]));
                if (!std::isfinite(radius) || !std::isfinite(center[axis])) {
                    error = "Nearby-model query requires finite player coordinates and extents.";
                    return -1;
                }
                bounds[axis] = center[axis] - radius;
                bounds[axis + 3] = center[axis] + radius;
                if (!std::isfinite(bounds[axis]) || !std::isfinite(bounds[axis + 3])) {
                    error = "Nearby-model query bounds overflow the native coordinate range.";
                    return -1;
                }
            }
            const auto matches = [&](int32_t model, float x, float y, float z) {
                const int32_t nativeModel = int16_t(uint16_t(model));
                return (nativeModel == values[4].bits || nativeModel == values[5].bits) &&
                    x >= bounds[0] && x <= bounds[3] && y >= bounds[1] && y <= bounds[4] &&
                    z >= bounds[2] && z <= bounds[5];
            };
            if (matches(world.currentPlayerModel(), world.x, world.y, world.z)) return condition(true);
            for (const auto& entry : world.missionCharacters) {
                const auto& character = entry.second;
                if (matches(character.model, character.x, character.y, character.z)) return condition(true);
            }
            for (const auto& character : world.allAmbientCharactersInArea(bounds))
                if (matches(character.model, character.position[0], character.position[1], character.position[2]))
                    return condition(true);
            return condition(false);
        }
        case 0x0562:
            world.playerFireproofReward = values[1].bits != 0;
            return 1;
        case 0x0563:
            world.playerHealthLimit = uint8_t(world.playerHealthLimit + uint8_t(values[1].bits));
            world.health = float(world.playerHealthLimit);
            world.lcsHudFlashItem = 4;
            return 1;
        case 0x0564:
            world.playerArmourLimit = uint8_t(world.playerArmourLimit + uint8_t(values[1].bits));
            world.armour = float(world.playerArmourLimit);
            world.lcsHudFlashItem = 3;
            return 1;
        case 0x066D:
            if (!writable(1)) {
                error = "GET_PLAYER_SAVED_WEAPON requires a writable output.";
                return -1;
            }
            // The shared inventory uses VCS's 48 sentinel; LCS CPed stores 45.
            return store(1, world.playerSavedWeapon == 48 ? 45 : world.playerSavedWeapon);
        default:
            return 0;
        }
    }

    const int32_t handle = values[0].bits;
    const bool player = handle == 1;
    const auto found = world.missionCharacters.find(handle);
    auto* character = !player && found != world.missionCharacters.end() ? &found->second : nullptr;
    if (!player && !character) {
        if (command == 0x04B2 || command == 0x0526) return condition(false);
        if (command == 0x048E) return 1;
        error = "LCS character command references an unknown character.";
        return -1;
    }
    const int32_t runtimeHandle = player ? 0 : handle;
    if (command == 0x02E5 || command == 0x04B2 || command == 0x0526) {
        const auto& query = command == 0x02E5 ? world.characterShooting :
            command == 0x04B2 ? world.characterInWater : world.characterDrowning;
        if (!query) {
            error = "LCS character state query requires the live runtime.";
            return -1;
        }
        return condition(query(runtimeHandle));
    }
    if (command == 0x03A8) return condition(player || (character->type != 5 && character->type != 20));
    if (command == 0x048E) {
        (player ? world.playerSpeechEnabled : character->speechEnabled) = values[1].bits == 0;
        return 1;
    }
    if (command == 0x04AA) {
        for (size_t index = 1; index < 4; ++index) {
            if (!writable(index)) {
                error = "Dead-character pickup coordinates require three writable outputs.";
                return -1;
            }
        }
        std::array<float, 3> position{};
        if (!world.deadCharacterPickupPosition || !world.deadCharacterPickupPosition(runtimeHandle, position) ||
            !std::all_of(position.begin(), position.end(), [](float value) { return std::isfinite(value); })) {
            error = "Dead-character pickup coordinates require a valid live placement result.";
            return -1;
        }
        for (size_t axis = 0; axis < 3; ++axis) {
            int32_t bits;
            std::memcpy(&bits, &position[axis], sizeof(bits));
            if (store(axis + 1, bits) < 0) return -1;
        }
        return 1;
    }
    if (command == 0x0337) {
        (player ? world.playerBleeding : character->bleeding) = values[1].bits != 0;
        return 1;
    }
    if (command == 0x0519) {
        auto& mask = player ? world.playerGangDamageMask : character->gangDamageMask;
        // 0x187C44 narrows the MIPS shift result to eight bits before updating the halfword.
        const uint16_t bit = uint8_t(uint32_t(1) << (uint32_t(values[1].bits) & 31));
        mask = values[2].bits != 0 ? uint16_t(mask | bit) : uint16_t(mask & ~bit);
        return 1;
    }
    if (command == 0x0630) {
        const float health = roundEeFloat(double(values[1].bits));
        if (player) {
            world.health = health;
            world.playerMaximumHealth = health;
        } else {
            character->health = health;
            character->maximumHealth = health;
        }
        return 1;
    }
    if (player) {
        error = "This LCS character AI command requires a character controlled by the mission AI.";
        return -1;
    }
    switch (command) {
    case 0x0296:
        character->heedThreats = values[1].bits != 0;
        return 1;
    case 0x0355:
        character->stayInSamePlace = values[1].bits != 0;
        return 1;
    case 0x0488:
        character->ceaseAttackDelay = uint32_t(values[1].bits);
        return 1;
    case 0x04FA:
        character->playerFriend = values[2].bits != 0;
        return 1;
    case 0x0541:
        character->canFightInGroup = values[1].bits != 0;
        return 1;
    case 0x0631:
        character->attackTimers.shootOverride = uint32_t(values[1].bits);
        return 1;
    case 0x0632:
        character->attackTimers.attackOverride = uint32_t(values[1].bits);
        return 1;
    default:
        return 0;
    }
}

bool lcsGarageContainsVehicle(const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::Garage& garage, int32_t handle,
    bool& contains, std::string& error) {
    const auto vehicle = world.missionVehicles.find(handle);
    if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
        error = "GARAGE_CONTAIN_CAR references an unknown vehicle.";
        return false;
    }
    std::array<float, 16> matrix{};
    if (!world.objectWorld || !world.vehicleWorldMatrix ||
        !world.vehicleWorldMatrix(handle, matrix) ||
        !std::all_of(matrix.begin(), matrix.end(), [](float value) { return std::isfinite(value); })) {
        error = "GARAGE_CONTAIN_CAR requires the native vehicle transform and collision model.";
        return false;
    }
    const auto model = world.objectWorld->collisionModels.find(uint32_t(vehicle->second.model));
    if (model == world.objectWorld->collisionModels.end()) {
        error = "GARAGE_CONTAIN_CAR has no decoded vehicle collision model.";
        return false;
    }
    const auto& bounds = garage.bounds;
    const float ax = bounds[3] - bounds[0], ay = bounds[4] - bounds[1];
    const float bx = bounds[5] - bounds[0], by = bounds[6] - bounds[1];
    const float lengthA = std::hypot(ax, ay), lengthB = std::hypot(bx, by);
    if (!std::all_of(bounds.begin(), bounds.end(), [](float value) { return std::isfinite(value); }) ||
        !std::isfinite(lengthA) || !std::isfinite(lengthB) || lengthA <= 0 || lengthB <= 0) {
        error = "GARAGE_CONTAIN_CAR requires finite, nondegenerate garage bounds.";
        return false;
    }
    const float oppositeX = bounds[3] + bounds[5] - bounds[0];
    const float oppositeY = bounds[4] + bounds[6] - bounds[1];
    const float minimumX = std::min({bounds[0], bounds[3], bounds[5], oppositeX});
    const float maximumX = std::max({bounds[0], bounds[3], bounds[5], oppositeX});
    const float minimumY = std::min({bounds[1], bounds[4], bounds[6], oppositeY});
    const float maximumY = std::max({bounds[1], bounds[4], bounds[6], oppositeY});
    const float minimumZ = std::min(bounds[2], bounds[7]);
    const float maximumZ = std::max(bounds[2], bounds[7]);
    contains = false;
    if (matrix[12] < minimumX || matrix[12] > maximumX ||
        matrix[13] < minimumY || matrix[13] > maximumY ||
        matrix[14] < minimumZ || matrix[14] > maximumZ) return true;

    // SLUS_214.23 0x198828 checks each native COL sphere after the origin test.
    for (const auto& sphere : model->second.spheres) {
        const float x = matrix[12] + matrix[0] * sphere.center.x +
            matrix[4] * sphere.center.y + matrix[8] * sphere.center.z;
        const float y = matrix[13] + matrix[1] * sphere.center.x +
            matrix[5] * sphere.center.y + matrix[9] * sphere.center.z;
        const float z = matrix[14] + matrix[2] * sphere.center.x +
            matrix[6] * sphere.center.y + matrix[10] * sphere.center.z;
        const float radius = sphere.radius;
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
            !std::isfinite(radius) || radius < 0) {
            error = "GARAGE_CONTAIN_CAR encountered an invalid native collision sphere.";
            return false;
        }
        if (garage.type == 12) {
            if (x + radius < minimumX || x - radius > maximumX ||
                y + radius < minimumY || y - radius > maximumY) return true;
        } else {
            const float alongA = ((x - bounds[0]) * ax + (y - bounds[1]) * ay) / lengthA;
            const float alongB = ((x - bounds[0]) * bx + (y - bounds[1]) * by) / lengthB;
            if (z < minimumZ + radius || z > maximumZ - radius ||
                alongA < radius || alongA > lengthA - radius ||
                alongB < radius || alongB > lengthB - radius) return true;
        }
    }
    contains = true;
    return true;
}

int executeLcsVehicleObjectCommand(StorylandMissionVm& vm, uint16_t command,
    bool inverted, std::string& error) {
    if (vm.vcs) return 0;
    size_t count = 0;
    switch (command) {
    case 0x014C: case 0x01F8: case 0x0210: case 0x0362: case 0x036B:
    case 0x048F: case 0x049A: case 0x05CB: case 0x05CC: case 0x062A: case 0x0653:
        count = 1; break;
    case 0x058A: count = 0; break;
    case 0x0397: case 0x03C6: case 0x041D: case 0x0427: case 0x045D:
    case 0x0513: case 0x0555: case 0x05CE: case 0x05D8: case 0x0677:
        count = 2; break;
    case 0x0352: case 0x0391: case 0x03CA: case 0x0596:
    case 0x05EC: case 0x05ED: case 0x0660:
        count = 4; break;
    case 0x0361: case 0x0652: count = 5; break;
    case 0x035B: count = 7; break;
    case 0x0353: count = 8; break;
    case 0x033E: count = 11; break;
    default: return 0;
    }
    std::array<Operand, 11> values{};
    for (size_t index = 0; index < count; ++index) {
        if (!readOperand(vm, values[index])) {
            error = "LCS vehicle/object command has truncated operands.";
            return -1;
        }
    }
    if (!vm.environment) {
        error = "LCS vehicle/object command requires a live world.";
        return -1;
    }
    auto& world = *vm.environment;
    const auto real = [&](size_t index) { return operandFloat(values[index]); };
    const auto finite = [&](size_t first, size_t last) {
        for (size_t index = first; index <= last; ++index) {
            if (!std::isfinite(real(index))) {
                error = "LCS vehicle/object command requires finite coordinates and physical values.";
                return false;
            }
        }
        return true;
    };
    const auto output = [&](size_t index) {
        if (values[index].reference) return true;
        error = "LCS vehicle/object command requires a writable output variable.";
        return false;
    };
    const auto store = [&](size_t index, int32_t value) {
        if (writeOperand(vm, values[index], value)) return true;
        error = "LCS vehicle/object command could not write its output variable.";
        return false;
    };
    const auto storeReal = [&](size_t index, float value) {
        int32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return store(index, bits);
    };
    const auto condition = [&](bool value) {
        updateScmCondition(vm, value, inverted);
        return 1;
    };

    if (command == 0x05CB || command == 0x05CC) {
        const int32_t handle = values[0].bits;
        const auto vehicle = world.missionVehicles.find(handle);
        if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
            error = "LCS vehicle condition references an unknown vehicle.";
            return -1;
        }
        if (command == 0x05CB) {
            if (!world.vehicleBoatBuoyant) {
                error = "LCS boat buoyancy condition requires live vehicle physics.";
                return -1;
            }
            return condition(world.vehicleBoatBuoyant(handle));
        }
        if (!world.vehicleEntryInProgress) {
            error = "LCS vehicle entry condition requires the live vehicle transition.";
            return -1;
        }
        bool entering = world.vehicleEntryInProgress(handle);
        for (const auto& item : world.characterVehicleTasks) {
            const auto character = world.missionCharacters.find(item.first);
            const auto& task = item.second;
            if (character != world.missionCharacters.end() && character->second.alive &&
                task.vehicle == handle && !task.exiting && task.animating) {
                entering = true;
                break;
            }
        }
        return condition(entering);
    }

    if (command == 0x05CE) {
        const int32_t vehicleHandle = values[0].bits;
        const int32_t characterHandle = values[1].bits;
        const auto vehicle = world.missionVehicles.find(vehicleHandle);
        const auto character = world.missionCharacters.find(characterHandle);
        if (vehicle == world.missionVehicles.end() || vehicle->second.deleted ||
            (characterHandle != 1 && character == world.missionCharacters.end())) {
            error = "LCS vehicle rocking references an unknown vehicle or character.";
            return -1;
        }
        if (!world.vcsCharacterVehicleTurnForce) {
            error = "LCS vehicle rocking requires live vehicle physics.";
            return -1;
        }
        if (!world.vcsCharacterVehicleTurnForce(vehicleHandle, characterHandle, world.nextNativeRandom())) {
            error = "LCS vehicle rocking could not resolve the physical state.";
            return -1;
        }
        if (world.audioSubsystemEnabled && world.scriptSounds.size() < 4096) {
            StorylandMissionEnvironment::ScriptSound event;
            event.id = 0x17;
            event.position = {vehicle->second.x, vehicle->second.y, vehicle->second.z};
            std::array<float, 16> matrix{};
            if (world.vehicleWorldMatrix && world.vehicleWorldMatrix(vehicleHandle, matrix))
                event.position = {matrix[12], matrix[13], matrix[14]};
            event.vcsVehicleAudioEvent = StorylandMissionEnvironment::ScriptSound::LcsVehicleRocking;
            world.scriptSounds.push_back(event);
        }
        return 1;
    }

    if (command == 0x0677) {
        const int32_t handle = values[0].bits;
        const auto vehicle = world.missionVehicles.find(handle);
        if (vehicle == world.missionVehicles.end() || vehicle->second.deleted) {
            error = "LCS garage vehicle permission references an unknown vehicle.";
            return -1;
        }
        if (values[1].bits != 0) world.vcsGarageStorableMissionVehicles.insert(handle);
        else world.vcsGarageStorableMissionVehicles.erase(handle);
        return 1;
    }

    if (command == 0x048F) {
        world.remoteDetonationEnabled = values[0].bits != 0;
        return 1;
    }
    if (command == 0x058A) {
        const bool supported = world.controllerMode == 0 || world.controllerMode == 1 ||
            world.controllerMode == 3 || world.controllerMode == 4;
        const size_t button = world.controllerMode == 4 ? 7 : 17;
        return condition(supported && world.control && world.padState[button] != 0);
    }
    if (command == 0x033E) {
        if (!finite(0, 5)) return -1;
        std::array<float, 6> bounds{real(0), real(1), real(2), real(3), real(4), real(5)};
        for (size_t axis = 0; axis < 3; ++axis)
            if (bounds[axis] > bounds[axis + 3]) std::swap(bounds[axis], bounds[axis + 3]);
        uint32_t mask = 0;
        for (size_t index = 6; index < 11; ++index)
            if (values[index].bits) mask |= 1u << (index - 6);
        if (!mask) return condition(false);
        if (!world.areaOccupied) {
            error = "IS_AREA_OCCUPIED requires the native world and population collision query.";
            return -1;
        }
        return condition(world.areaOccupied(bounds, mask));
    }
    if (command == 0x035B) {
        if (!finite(1, 6)) return -1;
        std::array<float, 6> bounds{real(1), real(2), real(3), real(4), real(5), real(6)};
        for (size_t axis = 0; axis < 3; ++axis)
            if (bounds[axis] > bounds[axis + 3]) std::swap(bounds[axis], bounds[axis + 3]);
        const bool found = std::any_of(world.scriptExplosions.begin(), world.scriptExplosions.end(),
            [&](const auto& explosion) {
                if (!explosion.active || explosion.type != values[0].bits ||
                    uint32_t(world.gameMilliseconds - explosion.startedAt) >= explosion.lifetime) return false;
                for (size_t axis = 0; axis < 3; ++axis)
                    if (explosion.position[axis] < bounds[axis] || explosion.position[axis] > bounds[axis + 3]) return false;
                return true;
            });
        return condition(found);
    }
    if (command == 0x03CA) {
        if (!finite(0, 3)) return -1;
        if (!world.createRandomParkedVehicle) {
            error = "CREATE_RANDOM_CAR_FOR_CAR_PARK requires the live population vehicle pool.";
            return -1;
        }
        world.createRandomParkedVehicle({real(0), real(1), real(2)}, real(3));
        return 1;
    }
    if (command == 0x0596) {
        if (!finite(0, 3)) return -1;
        if (!world.unlockVehicleDoorsInArea) {
            error = "UNLOCK_ALL_CAR_DOORS_IN_AREA requires the live vehicle pool.";
            return -1;
        }
        std::array<float, 4> bounds{real(0), real(1), real(2), real(3)};
        if (bounds[0] > bounds[2]) std::swap(bounds[0], bounds[2]);
        if (bounds[1] > bounds[3]) std::swap(bounds[1], bounds[3]);
        world.unlockVehicleDoorsInArea(bounds);
        for (auto& entry : world.missionVehicles) {
            auto& vehicle = entry.second;
            if (!vehicle.deleted && vehicle.x >= bounds[0] && vehicle.x <= bounds[2] &&
                vehicle.y >= bounds[1] && vehicle.y <= bounds[3]) vehicle.doorLock = 1;
        }
        return 1;
    }
    if (command == 0x03C6) {
        if (values[0].bits != 0 || !output(1)) {
            if (error.empty()) error = "STORE_CAR_PLAYER_IS_IN_NO_SAVE requires player zero.";
            return -1;
        }
        if (!world.inVehicle) {
            error = "STORE_CAR_PLAYER_IS_IN_NO_SAVE requires a player currently in a vehicle.";
            return -1;
        }
        int32_t handle = world.occupiedMissionVehicle;
        if (handle < 0 && world.acquireOccupiedVehicle) handle = world.acquireOccupiedVehicle();
        const auto found = world.missionVehicles.find(handle);
        if (found == world.missionVehicles.end() || found->second.deleted) {
            error = "STORE_CAR_PLAYER_IS_IN_NO_SAVE could not resolve the occupied vehicle.";
            return -1;
        }
        world.occupiedMissionVehicle = handle;
        return store(1, handle) ? 1 : -1;
    }
    if (command == 0x0427) {
        const int32_t handle = int16_t(uint16_t(values[0].bits));
        const auto garage = std::find_if(world.garages.begin(), world.garages.end(),
            [&](const auto& item) { return item.handle == handle; });
        if (garage == world.garages.end()) {
            error = "GARAGE_CONTAIN_CAR references an unknown garage.";
            return -1;
        }
        bool contains = false;
        if (!lcsGarageContainsVehicle(world, *garage, values[1].bits, contains, error)) return -1;
        return condition(contains);
    }
    if (command == 0x014C || command == 0x01F8 || command == 0x0210 ||
        command == 0x049A || command == 0x05D8 || command == 0x062A) {
        const int32_t handle = values[0].bits;
        const auto found = world.missionVehicles.find(handle);
        if (found == world.missionVehicles.end() || found->second.deleted) {
            error = "LCS vehicle command references an unknown vehicle.";
            return -1;
        }
        auto& vehicle = found->second;
        if (command == 0x014C) {
            vehicle.alarmMilliseconds = UINT16_MAX;
            vehicle.alarmChangedByScript = true;
            return 1;
        }
        if (command == 0x01F8) {
            if (!world.vehicleContacts) {
                error = "IS_CAR_IN_AIR_PROPER requires live physical contact data.";
                return -1;
            }
            const auto contacts = world.vehicleContacts(handle);
            if (!contacts.valid) {
                error = "IS_CAR_IN_AIR_PROPER could not resolve the vehicle's physical contacts.";
                return -1;
            }
            return condition(!contacts.bodyContact &&
                std::none_of(contacts.wheels.begin(), contacts.wheels.end(), [](bool contact) { return contact; }));
        }
        if (command == 0x0210) return storylandExplodeScriptVehicle(world, handle, error) ? 1 : -1;
        if (command == 0x049A) {
            const bool attachedFire = std::any_of(world.scriptFires.begin(), world.scriptFires.end(),
                [&](const auto& entry) {
                    const auto& fire = entry.second;
                    return fire.active && fire.attachedKind == 2 && fire.attachedHandle == handle;
                });
            return condition(vehicle.health < 250 || attachedFire ||
                (world.vehicleOnFire && world.vehicleOnFire(handle)));
        }
        if (command == 0x05D8) {
            if (!output(1)) return -1;
            if (!world.consumeVehicleCollision) {
                error = "STORE_CAR_COLLIDED_WITH_NO_SAVE requires live vehicle collision records.";
                return -1;
            }
            return store(1, world.consumeVehicleCollision(handle)) ? 1 : -1;
        }
        vehicle.doorDamage[1] = 3;
        vehicle.doorsChangedByScript = true;
        vehicle.visiblyDamaged = true;
        return 1;
    }

    const int32_t handle = values[command == 0x045D ? 1 : 0].bits;
    if (command == 0x045D && values[0].bits != 0) {
        error = "IS_PLAYER_TARGETTING_OBJECT requires player zero.";
        return -1;
    }
    const auto found = std::find_if(world.objects.begin(), world.objects.end(),
        [&](const auto& item) { return item.handle == handle; });
    if (found == world.objects.end()) {
        if (command == 0x0653) return 1;
        error = "LCS object command references an unknown object.";
        return -1;
    }
    auto& object = *found;
    if (command == 0x045D)
        return condition(world.playerTarget.kind == StorylandMissionEnvironment::TargetKind::Object &&
            world.playerTarget.handle == handle);
    if (command == 0x036B) return condition(!object.damage.visible || object.damage.renderDamaged);
    if (command == 0x0362) {
        if (std::find(world.targettableObjects.begin(), world.targettableObjects.end(), handle) !=
            world.targettableObjects.end()) return 1;
        const auto slot = std::find_if(world.targettableObjects.begin(), world.targettableObjects.end(),
            [&](int32_t candidate) {
                return std::none_of(world.objects.begin(), world.objects.end(),
                    [&](const auto& item) { return item.handle == candidate; });
            });
        if (slot != world.targettableObjects.end()) *slot = handle;
        return 1;
    }
    if (command == 0x0397) {
        object.dynamic = values[1].bits != 0;
        return 1;
    }
    if (command == 0x041D) {
        object.drawLast = values[1].bits != 0;
        return 1;
    }
    if (command == 0x0513) {
        const auto car = world.missionVehicles.find(values[1].bits);
        if (car == world.missionVehicles.end() || car->second.deleted) {
            error = "SORT_OUT_OBJECT_COLLISION_WITH_CAR references an unknown vehicle.";
            return -1;
        }
        object.ignoredVehicle = values[1].bits;
        return 1;
    }
    if (command == 0x0555) {
        storylandHydrateScriptObject(world, object);
        object.positionFrozen = values[1].bits != 0;
        object.damage.disableMoveForce = object.positionFrozen;
        return 1;
    }
    if (command == 0x0653) {
        if (object.attachedVehicle >= 0) {
            object.attachedVehicle = -1;
            object.ignoredVehicle = -1;
        }
        return 1;
    }
    if (command == 0x0391 || command == 0x05EC) {
        if (!finite(1, 3)) return -1;
        std::array<float, 3> velocity{real(1), real(2), real(3)};
        if (command == 0x0391) {
            for (size_t axis = 0; axis < 3; ++axis) velocity[axis] += object.velocity[axis];
            if (!std::all_of(velocity.begin(), velocity.end(), [](float value) { return std::isfinite(value); })) {
                error = "ADD_TO_OBJECT_VELOCITY overflowed the physical velocity.";
                return -1;
            }
            object.velocity = velocity;
        } else object.rotationVelocity = velocity;
        return 1;
    }
    if (command == 0x05ED) {
        if (!finite(1, 3)) return -1;
        if (real(1) <= 0 || real(2) <= 0 || real(3) < 0 || real(3) > 1) {
            error = "SET_OBJECT_PHYSICAL_PROPERTIES requires positive masses and air resistance in 0..1.";
            return -1;
        }
        storylandHydrateScriptObject(world, object);
        object.damage.mass = real(1);
        object.damage.turnMass = real(2);
        object.damage.airResistance = real(3);
        object.damage.disableMoveForce = real(1) >= 99998.0f;
        object.damage.disableTurnForce = object.damage.disableMoveForce;
        object.damage.propertiesLoaded = true;
        object.gravity = !object.damage.disableMoveForce;
        return 1;
    }
    if (command == 0x0361 || command == 0x0652) {
        if (!finite(2, 4)) return -1;
        const int32_t carHandle = values[1].bits;
        const auto car = world.missionVehicles.find(carHandle);
        if (car == world.missionVehicles.end() || car->second.deleted) {
            error = "Object placement references an unknown vehicle.";
            return -1;
        }
        std::array<float, 16> matrix{};
        std::array<float, 3> velocity{};
        if (!world.vehicleWorldMatrix || !world.vehicleWorldMatrix(carHandle, matrix) ||
            !world.vehicleWorldVelocity || !world.vehicleWorldVelocity(carHandle, velocity) ||
            !std::all_of(velocity.begin(), velocity.end(), [](float value) { return std::isfinite(value); })) {
            error = "Object placement requires the live vehicle transform and velocity.";
            return -1;
        }
        const std::array<float, 3> offset{real(2), real(3), real(4)};
        auto candidate = object;
        if (!storylandApplyScriptObjectAttachment(candidate, matrix, offset, {0, 0, 0})) {
            error = "Object placement produced an invalid transform.";
            return -1;
        }
        candidate.velocity = velocity;
        if (command == 0x0361) {
            if (!std::isfinite(world.nativeScriptTimeStep)) {
                error = "Relative object placement requires a finite native time step.";
                return -1;
            }
            const float lead = .9f * world.nativeScriptTimeStep / 50.0f;
            candidate.x += velocity[0] * lead;
            candidate.y += velocity[1] * lead;
            candidate.z += velocity[2] * lead;
            if (!std::isfinite(candidate.x) || !std::isfinite(candidate.y) || !std::isfinite(candidate.z)) {
                error = "Relative object placement overflowed the world coordinates.";
                return -1;
            }
        } else {
            if (!std::all_of(car->second.angularVelocity.begin(), car->second.angularVelocity.end(),
                [](float value) { return std::isfinite(value); })) {
                error = "Object attachment requires finite vehicle angular velocity.";
                return -1;
            }
            candidate.attachedVehicle = carHandle;
            candidate.ignoredVehicle = carHandle;
            candidate.attachedCharacter = -1;
            candidate.attachedBone = -1;
            candidate.attachmentOffset = offset;
            candidate.attachmentRotation = {};
            candidate.rotationVelocity = car->second.angularVelocity;
            candidate.dynamic = true;
        }
        object = std::move(candidate);
        return 1;
    }
    if (command == 0x0660) {
        if (!output(1) || !output(2) || !output(3)) return -1;
        const auto matrix = storylandScriptObjectMatrix(object.x, object.y, object.z,
            object.rotationX, object.rotationY, object.heading);
        if (!std::all_of(matrix.begin(), matrix.end(), [](float value) { return std::isfinite(value); })) {
            error = "GET_OBJECT_ROTATION requires a finite object transform.";
            return -1;
        }
        constexpr float degrees = 57.2957763671875f;
        std::array<float, 3> angles{
            std::atan2(-matrix[2], matrix[10]) * degrees,
            std::asin(std::clamp(matrix[6], -1.0f, 1.0f)) * degrees,
            std::atan2(-matrix[4], matrix[5]) * degrees};
        for (float& angle : angles) {
            if (angle < 0) angle += 360;
            else if (angle > 360) angle -= 360;
        }
        return storeReal(1, angles[0]) && storeReal(2, angles[1]) && storeReal(3, angles[2]) ? 1 : -1;
    }
    if (command == 0x0352) {
        if (!finite(1, 2)) return -1;
        const auto matrix = storylandScriptObjectMatrix(object.x, object.y, object.z,
            object.rotationX, object.rotationY, object.heading);
        float current = std::atan2(-matrix[4], matrix[5]) * 57.2957763671875f;
        if (current < 0) current += 360;
        if (!std::isfinite(current)) {
            error = "ROTATE_OBJECT requires a finite object transform.";
            return -1;
        }
        const float target = real(1), step = real(2);
        if (current == target) return condition(true);
        float positive = target - current, negative = current - target;
        if (positive < 0) positive += 360;
        if (negative < 0) negative += 360;
        const float next = positive < negative ? (positive < step ? target : current + step) :
            (negative < step ? target : current - step);
        if (!std::isfinite(next)) {
            error = "ROTATE_OBJECT overflowed its heading.";
            return -1;
        }
        if (values[3].bits && !world.objectSlideCollision) {
            error = "ROTATE_OBJECT collision checks require native collision geometry.";
            return -1;
        }
        const float previousX = object.rotationX, previousY = object.rotationY, previousHeading = object.heading;
        object.rotationX = 0;
        object.rotationY = 0;
        object.heading = next;
        if (values[3].bits) {
            const int collision = world.objectSlideCollision(handle, {object.x, object.y, object.z});
            if (collision != 0) {
                object.rotationX = previousX;
                object.rotationY = previousY;
                object.heading = previousHeading;
                if (collision < 0) {
                    error = "ROTATE_OBJECT could not resolve the object's collision geometry.";
                    return -1;
                }
                return condition(true);
            }
        }
        return condition(next == target);
    }
    if (command == 0x0353) {
        if (!finite(1, 6)) return -1;
        const std::array<float, 3> target{real(1), real(2), real(3)};
        std::array<float, 3> next{object.x, object.y, object.z};
        if (next == target) return condition(true);
        for (size_t axis = 0; axis < 3; ++axis) {
            const float distance = next[axis] - target[axis], step = real(axis + 4);
            if (distance >= 0) next[axis] = distance > step ? next[axis] - step : target[axis];
            else next[axis] = -distance > step ? next[axis] + step : target[axis];
        }
        if (!std::all_of(next.begin(), next.end(), [](float value) { return std::isfinite(value); })) {
            error = "SLIDE_OBJECT overflowed its coordinates.";
            return -1;
        }
        if (values[7].bits) {
            if (!world.objectSlideCollision) {
                error = "SLIDE_OBJECT collision checks require native collision geometry.";
                return -1;
            }
            const int collision = world.objectSlideCollision(handle, next);
            if (collision < 0) {
                error = "SLIDE_OBJECT could not resolve the object's collision geometry.";
                return -1;
            }
            if (collision > 0) return condition(true);
        }
        object.x = next[0];
        object.y = next[1];
        object.z = next[2];
        return condition(next == target);
    }
    error = "LCS vehicle/object command has no execution path.";
    return -1;
}

enum class StorylandLcsUtilityStatisticUpdate { Maximum, MinimumTime, MinimumPosition, Increment };

int executeLcsUtilityStatisticCommand(StorylandMissionVm& vm, uint16_t command,
    std::string& error) {
    struct Definition {
        uint16_t opcode;
        const char* key;
        StorylandLcsUtilityStatisticUpdate update;
        int32_t count;
    };
    static constexpr Definition definitions[] = {
        {0x057D, "HighestLevelVigilanteMission", StorylandLcsUtilityStatisticUpdate::Maximum, 0},
        {0x05F5, "FastestSmugglingTime", StorylandLcsUtilityStatisticUpdate::MinimumTime, 0},
        {0x05FC, "HighestGuardianAngelJusticeDished", StorylandLcsUtilityStatisticUpdate::Increment, 0},
        {0x05FD, "BestBanditLapTimes", StorylandLcsUtilityStatisticUpdate::MinimumTime, 3},
        {0x05FE, "BestBanditPositions", StorylandLcsUtilityStatisticUpdate::MinimumPosition, 3},
        {0x0601, "FastestHeliRaceTime", StorylandLcsUtilityStatisticUpdate::MinimumTime, 0},
        {0x0602, "BestHeliRacePosition", StorylandLcsUtilityStatisticUpdate::MinimumPosition, 0},
        {0x0604, "FastestStreetRaceTimes", StorylandLcsUtilityStatisticUpdate::MinimumTime, 6},
        {0x0605, "FastestStreetRaceLapTimes", StorylandLcsUtilityStatisticUpdate::MinimumTime, 6},
        {0x0606, "BestStreetRacePositions", StorylandLcsUtilityStatisticUpdate::MinimumPosition, 6},
        {0x0648, "FastestDirtBikeLapTimes", StorylandLcsUtilityStatisticUpdate::MinimumTime, 10},
        {0x0649, "FastestDirtBikeTimes", StorylandLcsUtilityStatisticUpdate::MinimumTime, 10},
        {0x0667, "BestTimeGoGoFaggio", StorylandLcsUtilityStatisticUpdate::MinimumTime, 0}
    };
    const auto definition = std::find_if(std::begin(definitions), std::end(definitions),
        [&](const auto& item) { return item.opcode == command; });
    if (definition == std::end(definitions) && command != 0x066B) return 0;
    if (!vm.environment) {
        error = "LCS statistic command requires the live statistics.";
        return -1;
    }
    auto& statistics = vm.environment->statistics;
    const auto previousInteger = [&](const std::string& key, int32_t initial, int32_t& value) {
        const auto found = statistics.find(key);
        const double previous = found == statistics.end() ? double(initial) : found->second;
        if (!std::isfinite(previous) || previous < double(INT32_MIN) ||
            previous > double(INT32_MAX) || std::trunc(previous) != previous) {
            error = "Stored LCS integer statistic is invalid.";
            return false;
        }
        value = int32_t(previous);
        return true;
    };
    if (command == 0x066B) {
        static constexpr const char* keys[] = {
            "GuardianAngelHighestLevelInd", "GuardianAngelHighestLevelCom", "GuardianAngelHighestLevelSub"
        };
        std::array<Operand, 3> levels{};
        std::array<int32_t, 3> previous{};
        for (size_t index = 0; index < levels.size(); ++index) {
            if (!readOperand(vm, levels[index])) {
                error = "REGISTER_GUARDIAN_ANGEL_LEVELS requires three integer levels.";
                return -1;
            }
            if (!previousInteger(keys[index], 0, previous[index])) return -1;
        }
        for (size_t index = 0; index < levels.size(); ++index)
            statistics[keys[index]] = std::max(previous[index], levels[index].bits);
        return 1;
    }

    std::string key = definition->key;
    if (definition->count) {
        Operand index;
        if (!readOperand(vm, index) || index.bits < 0 || index.bits >= definition->count) {
            error = "LCS race statistic index is outside the native table.";
            return -1;
        }
        key += "[" + std::to_string(index.bits) + "]";
    }
    Operand source;
    if (!readOperand(vm, source)) {
        error = "LCS statistic command has a truncated integer value.";
        return -1;
    }
    int32_t previous = 0;
    const int32_t initial = definition->update == StorylandLcsUtilityStatisticUpdate::MinimumPosition ? INT32_MAX : 0;
    if (!previousInteger(key, initial, previous)) return -1;
    int32_t result = source.bits;
    switch (definition->update) {
    case StorylandLcsUtilityStatisticUpdate::Maximum:
        result = std::max(previous, result);
        break;
    case StorylandLcsUtilityStatisticUpdate::MinimumTime:
        if (previous != 0) result = std::min(previous, result);
        break;
    case StorylandLcsUtilityStatisticUpdate::MinimumPosition:
        result = std::min(previous, result);
        break;
    case StorylandLcsUtilityStatisticUpdate::Increment: {
        // Retail 0x231B88 ignores the collected argument and increments once.
        const uint32_t incremented = uint32_t(previous) + 1u;
        std::memcpy(&result, &incremented, sizeof(result));
        break;
    }
    }
    statistics[key] = result;
    return 1;
}

int executeLcsUtilityCommand(StorylandMissionVm& vm, uint16_t command, bool inverted,
    std::string& error) {
    if (vm.vcs) return 0;
    const int statistic = executeLcsUtilityStatisticCommand(vm, command, error);
    if (statistic) return statistic;

    int count = -1;
    switch (command) {
    case 0x015A: case 0x015B: case 0x0617: count = 2; break;
    case 0x0383: case 0x038F: count = 6; break;
    case 0x0439: case 0x0441: case 0x048D: case 0x066F: count = 1; break;
    case 0x043A: case 0x043B: case 0x056F: count = 0; break;
    case 0x0491: count = 4; break;
    case 0x05CF: count = 5; break;
    case 0x064D: case 0x064E: count = 2; break;
    default: return 0;
    }
    if (!vm.environment) {
        error = "LCS utility command requires the live mission environment.";
        return -1;
    }
    auto& world = *vm.environment;
    std::string label;
    if ((command == 0x015A || command == 0x015B || command == 0x0617) && !readScmText(vm, label)) {
        error = "LCS utility command has a truncated eight-byte text label.";
        return -1;
    }
    std::array<Operand, 6> values{};
    for (int index = 0; index < count; ++index) {
        if (!readOperand(vm, values[size_t(index)])) {
            error = "LCS utility command has a truncated operand.";
            return -1;
        }
    }
    const auto condition = [&](bool value) {
        updateScmCondition(vm, value, inverted);
        return 1;
    };
    const auto finiteValues = [&](size_t first, size_t last) {
        for (size_t index = first; index <= last; ++index) {
            if (!std::isfinite(operandFloat(values[index]))) {
                error = "LCS spatial command requires finite coordinates and extents.";
                return false;
            }
        }
        return true;
    };
    const auto writable = [&](size_t index) {
        const auto& output = values[index];
        if (!output.reference || output.variable >= (output.global ? vm.globals.size() : vm.locals.size())) {
            error = "LCS utility query requires a writable output.";
            return false;
        }
        return true;
    };

    if (command == 0x015A || command == 0x015B) {
        if (!world.zones || world.zones->vcs) {
            error = "LCS zone density requires the native information zones.";
            return -1;
        }
        const auto found = std::find_if(world.zones->information.begin(), world.zones->information.end(),
            [&](const auto& zone) { return zone.name == label; });
        if (found == world.zones->information.end()) return 1;
        const int32_t day = uint8_t(values[0].bits) != 0 ? 1 : 0;
        const auto key = std::make_pair(label, day);
        auto& densities = command == 0x015A ? world.zoneCarDensities : world.zonePedDensities;
        if (!densities.count(key) && densities.size() >= 4096) {
            error = "LCS zone density table is full.";
            return -1;
        }
        densities[key] = uint16_t(values[1].bits);
        return 1;
    }
    if (command == 0x0383 || command == 0x038F) {
        if (!finiteValues(0, 5)) return -1;
        std::array<float, 6> bounds{};
        for (size_t axis = 0; axis < 3; ++axis) {
            const float first = operandFloat(values[axis]);
            const float second = operandFloat(values[axis + 3]);
            const float lower = command == 0x038F ? roundEeFloat(double(first) - second) : first;
            const float upper = command == 0x038F ? roundEeFloat(double(first) + second) : second;
            bounds[axis] = std::min(lower, upper);
            bounds[axis + 3] = std::max(lower, upper);
        }
        if (command == 0x0383) {
            if (!world.sniperBulletInArea) {
                error = "IS_SNIPER_BULLET_IN_AREA requires the live sniper trace.";
                return -1;
            }
            return condition(world.sniperBulletInArea(bounds));
        }
        if (!world.missionEntityInArea) {
            error = "IS_POINT_OBSCURED_BY_A_MISSION_ENTITY requires native entity collision bounds.";
            return -1;
        }
        return condition(world.missionEntityInArea(bounds));
    }
    if (command == 0x0439) {
        const int32_t group = values[0].bits;
        if (group < 0 || group > 98 || !world.prepareCredits) {
            error = "START_CREDITS requires a native credit group and the live text renderer.";
            return -1;
        }
        std::vector<StorylandMissionEnvironment::CreditsLine> lines;
        if (!world.prepareCredits(group, lines, error)) return -1;
        if (lines.empty() || lines.size() > 1000) {
            error = "START_CREDITS requires one through one thousand native credit lines.";
            return -1;
        }
        world.credits.active = true;
        world.credits.group = group;
        world.credits.scroll = 48;
        world.credits.lines = std::move(lines);
        return 1;
    }
    if (command == 0x043A) {
        world.credits.active = false;
        return 1;
    }
    if (command == 0x043B) return condition(!world.credits.active);
    if (command == 0x0441) {
        world.musicDoesFade = values[0].bits != 0;
        return 1;
    }
    if (command == 0x048D) {
        if (!world.nativeModelAvailable) {
            error = "IS_MODEL_AVAILABLE requires the native model directory.";
            return -1;
        }
        const int32_t model = values[0].bits;
        return condition(model >= 0 && world.nativeModelAvailable(model));
    }
    if (command == 0x0491) {
        if (!finiteValues(0, 3)) return -1;
        if (!world.anyPickupInSphere) {
            error = "IS_ANY_PICKUP_AT_COORDS requires the live pickup pools.";
            return -1;
        }
        return condition(world.anyPickupInSphere({operandFloat(values[0]), operandFloat(values[1]),
            operandFloat(values[2])}, operandFloat(values[3])));
    }
    if (command == 0x056F) return condition(world.cutsceneSkipped);
    if (command == 0x05CF) {
        if (!finiteValues(0, 3) || !writable(4)) return -1;
        int32_t weapon = -1;
        if (!world.pickupWeaponInSphere || !world.pickupWeaponInSphere(
                {operandFloat(values[0]), operandFloat(values[1]), operandFloat(values[2])},
                operandFloat(values[3]), weapon)) {
            error = "GET_PICKUP_WEAPON_ID requires the live native pickup models.";
            return -1;
        }
        return writeOperand(vm, values[4], weapon) ? 1 : -1;
    }
    if (command == 0x0617) {
        storylandUseLcsMessageQueues(world);
        if (!world.lcsBriefMessages.empty()) return 1;
        StorylandMissionEnvironment::LcsMessage message;
        message.key = std::move(label);
        message.missionText = world.missionText;
        message.durationMilliseconds = uint32_t(values[0].bits);
        message.flags = uint16_t(values[1].bits);
        storylandQueueLcsMessage(world, std::move(message));
        return 1;
    }
    if (command == 0x064D || command == 0x064E) {
        if (command == 0x064D && !writable(1)) return -1;
        const auto pickup = std::find_if(world.pickups.begin(), world.pickups.end(),
            [&](const auto& item) { return item.handle == values[0].bits; });
        if (command == 0x064E) {
            if (pickup != world.pickups.end()) pickup->revenue = roundEeFloat(double(values[1].bits));
            return 1;
        }
        const float revenue = pickup == world.pickups.end() ? 0.0f : pickup->revenue;
        if (!std::isfinite(revenue)) {
            error = "GET_PICKUP_VALUE encountered non-finite native revenue.";
            return -1;
        }
        const int32_t result = int32_t(std::clamp(std::trunc(double(revenue)), double(INT32_MIN), double(INT32_MAX)));
        return writeOperand(vm, values[1], result) ? 1 : -1;
    }
    if (!world.musicStreamPlaying) {
        error = "IS_AUDIO_CHANNEL_PLAYING requires the live audio mixer.";
        return -1;
    }
    return condition(world.musicStreamPlaying(uint8_t(values[0].bits)));
}
