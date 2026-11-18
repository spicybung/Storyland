int executeVcsNativeCompatibilityCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(!vm.vcs)return 0;
    if(command==0x0288||command==0x0390||command==0x03DD||command==0x03E3||
        command==0x03E4||command==0x03F0||command==0x04CC)return 1;
    const int count=command==0x03A0?1:command==0x0415?10:command==0x0416?6:-1;
    if(count>=0) {
        // These PS2 handlers call CollectParameters with these exact counts
        // and return without a side effect: 333E70, 333FF0 and 334020.
        Operand value;
        for(int index=0;index<count;++index)if(!readOperand(vm,value)) {
            error="Native compatibility command has a truncated typed operand.";return -1;
        }
        return 1;
    }
    if(command==0x055A) {
        if(!vm.environment||!vm.environment->streamingBacklogged) {
            error="Native streaming backlog query requires the live resource queue binding.";return -1;
        }
        bool backlogged=false;
        if(!vm.environment->streamingBacklogged(backlogged,error))return -1;
        updateScmCondition(vm,backlogged,inverted);return 1;
    }
    if(command!=0x0559)return 0;
    Operand handle;
    if(!readOperand(vm,handle)) {error="Deferred object render command has no object handle.";return -1;}
    if(!vm.environment) {error="Deferred object rendering requires a live world.";return -1;}
    auto& objects=vm.environment->objects;
    const auto found=std::find_if(objects.begin(),objects.end(),[&](const auto& object){return object.handle==handle.bits;});
    if(found==objects.end()) {error="Deferred object render command references an unknown object.";return -1;}
    // Native 3119C0 sets CObject+1E0 bit28. It is independent of the
    // CEntity draw-last bit written by 028A.
    found->deferredRender=true;return 1;
}

int executeVcsScriptModelEffectCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    if(!vm.vcs||command<0x047D||command>0x0480)return 0;
    if(!vm.environment) {error="Script model effects require a live world.";return -1;}
    auto& world=*vm.environment;
    const bool filtered=command==0x047F||command==0x0480;
    const bool transform=command==0x047E||command==0x0480;
    const size_t inputs=command==0x047D?2:command==0x0480?4:3;
    std::array<Operand,4> arguments{},outputs{};
    for(size_t index=0;index<inputs;++index)if(!readOperand(vm,arguments[index])) {
        error="Script model effect query has a truncated input.";return -1;
    }
    std::string label;
    if(filtered) {
        Operand text;
        if(!readStringOperand(vm,text,label)) {
            error="Filtered script model effect query requires a native label string.";return -1;
        }
    }
    const size_t outputCount=transform?4:1;
    for(size_t index=0;index<outputCount;++index) {
        auto& output=outputs[index];
        if(!readOperand(vm,output)||!output.reference||output.variable>=
            (output.global?vm.globals.size():vm.locals.size())) {
            error="Script model effect query requires writable output variables.";return -1;
        }
    }
    if(!world.scriptEntityModelMatrix||!world.modelScriptEffects) {
        error="Script model effect queries require the native entity and effect registry bindings.";return -1;
    }
    int32_t model=-1;std::array<float,16> matrix{};
    std::vector<StorylandModelScriptEffect> effects;
    const bool exists=world.scriptEntityModelMatrix(arguments[0].bits,model,matrix);
    if(exists) {
        if(!std::all_of(matrix.begin(),matrix.end(),[](float value){return std::isfinite(value);})) {
            error="Native script model entity matrix is non-finite.";return -1;
        }
        if(!world.modelScriptEffects(model,effects)) {
            error="The native model script-effect registry could not be decoded.";return -1;
        }
    }
    const int32_t type=arguments[1].bits;
    const int32_t subtype=filtered?arguments[transform?3:2].bits:-1;
    const int32_t character=filtered?(label.empty()?0:uint8_t(label.front())):-1;
    int32_t matches=0;
    std::array<float,4> position{};
    for(const auto& effect:effects) {
        if(!storylandScriptEffectMatches(effect,type,subtype,character))continue;
        if(transform&&matches==arguments[2].bits)position=storylandScriptEffectWorldTransform(effect,matrix);
        ++matches;
    }
    for(size_t index=0;index<outputCount;++index) {
        int32_t value=matches;
        if(transform)std::memcpy(&value,&position[index],4);
        if(!writeOperand(vm,outputs[index],value)) {
            error="Script model effect output became invalid.";return -1;
        }
    }
    return 1;
}
