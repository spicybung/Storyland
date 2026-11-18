#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_clock.h"
#include "src/leeds/leeds_native_world.h"
#include "src/re/reVCS/gangs/storyland_empire.h"
#include "src/storyland/storyland_script_object.h"
#include "src/storyland/storyland_script_vehicle_capacity.h"
#include "src/re/reLCS2/pedestrians/storyland_lcs_wait_animation.h"
#include "src/re/reLCS2/pedestrians/storyland_lcs_wait_runtime.h"
#include "src/re/reLCS2/runtime/storyland_lcs_messages.h"
#include "src/storyland/storyland_script_camera.h"
#include "src/re/reVCS/camera/storyland_vcs_vehicle_camera.h"
#include "src/re/reVCS/world/storyland_vcs_ferris_wheel.h"
#include "src/re/reLCS2/world/storyland_lcs_garages.h"
#include "src/storyland/storyland_vehicle_transition.h"
#include "src/storyland/storyland_vehicle_entry.h"
#include "src/re/reVCS/vehicles/storyland_vcs_staged_entry.h"
#include "src/re/reLCS2/runtime/storyland_lcs_attachment.h"
#include "src/re/reVCS/runtime/storyland_vcs_attachment.h"
#include "src/re/reVCS/camera/storyland_vcs_ped_camera_zoom.h"
#include "src/re/reLCS2/vehicles/storyland_lcs_vehicle_access.h"
#include "src/storyland/storyland_vehicle_systems.h"
#include "src/storyland/storyland_mission_radar.h"
#include "src/storyland/storyland_player_restart.h"
#include "src/storyland/storyland_player_reactions.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_wait_state.h"
#include "src/storyland/storyland_weapon_animation_policy.h"
#include "src/re/reVCS/runtime/storyland_vcs_pad.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_character_handles.h"
#include "src/re/reVCS/runtime/storyland_vcs_clock_event.h"
#include "src/storyland/storyland_water_cannon_math.h"
#include "src/re/reVCS/runtime/storyland_vcs_projectile.h"
#include "src/re/reVCS/vehicles/storyland_vcs_vehicle_coordinates.h"
#include "src/re/reVCS/vehicles/storyland_vcs_vehicle_position.h"
#include "src/re/reVCS/vehicles/storyland_vcs_helicopter_door.h"
#include "src/re/reVCS/hud/storyland_vcs_hud_components.h"
#include "src/re/reVCS/pedestrians/storyland_vcs_formation.h"
#include "src/re/reVCS/runtime/storyland_vcs_seek_radius.h"
#include "src/re/reVCS/vehicles/storyland_vcs_carjack_recovery.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <cctype>
#include <limits>
#include "src/leeds/leeds_scm_definitions.inc"

namespace {
float turnLcsCharacterHeading(float heading,float dx,float dy,float rate,uint32_t elapsedMilliseconds) {
    if(dx==0&&dy==0)return heading;
    const float desired=std::atan2(-dx,dy)*57.2957795131f;
    const float seconds=float(std::min(elapsedMilliseconds,100u))/1000.0f;
    const float maximumTurn=std::max(0.0f,rate)*50.0f*seconds;
    const float turn=std::remainder(desired-heading,360.0f);
    return std::remainder(heading+std::clamp(turn,-maximumTurn,maximumTurn),360.0f);
}
float lookLcsCharacterHeading(float heading,float dx,float dy) {
    if(dx==0&&dy==0)return heading;
    const float desired=std::atan2(-dx,dy)*57.2957795131f;
    const float turn=std::remainder(desired-heading,360.0f);
    // CPed::TurnBody eases one fifth of the remaining angle per update.
    if(std::fabs(turn)>0.05f*57.2957795131f)
        heading=std::remainder(heading+turn*0.2f,360.0f);
    return heading;
}

float flushEeFloat(float value) {
    uint32_t bits=0;std::memcpy(&bits,&value,4);
    if((bits&0x7F800000u)==0) {
        bits&=0x80000000u;std::memcpy(&value,&bits,4);
    }
    return value;
}
float roundEeFloat(double value,double residual=0) {
    // VCS runs the EE FPU in round-toward-zero mode. The residual preserves
    // a tiny opposite-sign addend even when it falls below binary64 precision.
    // This helper uses the host finite range. Checked numeric script routes
    // reject EE exponent255 before conversion; existing utility callers keep
    // their finite saturation behavior.
    const double maximum=double(std::numeric_limits<float>::max());
    if(value>maximum)return std::numeric_limits<float>::max();
    if(value<-maximum)return -std::numeric_limits<float>::max();
    float result=float(value);
    const double rounded=double(result);
    const bool away=value>0?(rounded>value||(rounded==value&&residual<0)):
        value<0&&(rounded<value||(rounded==value&&residual>0));
    if(away) {
        uint32_t bits=0;std::memcpy(&bits,&result,4);
        if(bits&0x7FFFFFFFu)--bits;
        std::memcpy(&result,&bits,4);
    }
    return flushEeFloat(result);
}
bool tryRoundFiniteEeFloat(double value,float& result,double residual=0) {
    // EE exponent255 represents finite numbers. These guarded script routes
    // support the IEEE-finite subset; a raw-bit extended pipeline is required
    // beyond it. Values below2^128 still truncate to FLT_MAX and remain valid.
    constexpr double extended=0x1p128;
    if(!std::isfinite(value)||value>extended||value<-extended||
        (value==extended&&residual>=0)||(value==-extended&&residual<=0))return false;
    result=roundEeFloat(value,residual);return true;
}
bool tryTimedEeArithmetic(float destination,float rate,float step,bool subtract,float& result) {
    destination=flushEeFloat(destination);rate=flushEeFloat(rate);step=flushEeFloat(step);
    // mul.s is rounded before add.s/sub.s; a fused multiply-add is different.
    float product=0;
    if(!tryRoundFiniteEeFloat(double(rate)*double(step),product))return false;
    if(subtract)product=-product;
    const double left=destination,right=product;
    const double sum=left+right,virtualRight=sum-left;
    const double residual=(left-(sum-virtualRight))+(right-virtualRight);
    return tryRoundFiniteEeFloat(sum,result,residual);
}
struct Operand {
    int32_t bits=0;
    uint8_t type=0;
    uint16_t variable=0;
    bool reference=false;
    bool global=false;
    bool arrayScratch=false;
    bool characterOutput=false;
};
bool readRawOperand(StorylandMissionVm& vm, Operand& result) {
    if (vm.pc >= vm.scriptBytes().size()) return false;
    const uint8_t kind = vm.scriptBytes()[vm.pc++];
    result = {};
    result.type = kind;
    auto take = [&](size_t bytes, uint32_t& value) {
        if (bytes > vm.scriptBytes().size() - vm.pc) return false;
        value = 0;
        for (size_t index = 0; index < bytes; ++index)
            value |= uint32_t(vm.scriptBytes()[vm.pc++]) << (index * 8);
        return true;
    };
    uint32_t value = 0;
    if (kind == 1 || kind == 2) return true;
    if (kind >= 3 && kind <= 5) {
        const size_t bytes = kind - 2;
        if (!take(bytes, value)) return false;
        result.bits = int32_t(value << ((4 - bytes) * 8));
        return true;
    }
    if (kind == 6 || kind == 9) {
        if (!take(4, value)) return false;
        result.bits = int32_t(value);
        return true;
    }
    if (kind == 7 || kind == 8) {
        if (!take(kind == 7 ? 1 : 2, value)) return false;
        result.bits = kind == 7 ? int8_t(value) : int16_t(value);
        return true;
    }
    if (kind < 10) return false;
    const unsigned localStart = vm.vcs ? 13 : 12;
    const unsigned localArrayStart = vm.vcs ? 109 : 108;
    const unsigned globalStart = vm.vcs ? 205 : 204;
    constexpr unsigned globalArrayStart = 230;
    size_t slot = 0;
    result.global = kind >= globalStart;
    if (result.global) {
        if (!take(1, value)) return false;
        slot = ((kind - (kind >= globalArrayStart ? globalArrayStart : globalStart)) << 8) + value;
    } else if (kind >= localArrayStart) slot = kind - localArrayStart;
    else if (kind >= localStart) slot = kind - localStart;
    else {
        const unsigned timerStart=vm.vcs?11:10;
        if(kind<timerStart)return false;
        slot=104+kind-timerStart;
    }
    if (kind >= globalArrayStart || (kind >= localArrayStart && kind < globalStart)) {
        uint32_t indexSlot = 0, count = 0;
        if (!take(1, indexSlot) || !take(1, count) || indexSlot >= 96) return false;
        const int32_t index = vm.locals[indexSlot];
        if (index < 0 || uint32_t(index) >= count) {
            // VCS PS2 resolves either kind of out-of-range array to 0x50C140.
            if (!vm.vcs || !vm.environment) return false;
            result.reference = true;
            result.arrayScratch = true;
            result.bits = vm.environment->vcsArrayScratch;
            return true;
        }
        slot += size_t(index);
    }
    if (result.global ? slot >= vm.globals.size() : slot >= vm.locals.size()) return false;
    result.reference = true;
    result.variable = uint16_t(slot);
    result.bits = result.global ? vm.globals[slot] : vm.locals[slot];
    return true;
}

bool readOperand(StorylandMissionVm& vm, Operand& result) {
    const size_t operandOffset = vm.pc;
    if (!readRawOperand(vm, result)) return false;
    if (!vm.vcs || !vm.environment || !vm.environment->vcsCharacterHandleBridge ||
        vm.executionTrace.empty()) return true;
    const auto& instruction = vm.executionTrace.back();
    const uint16_t command = instruction.opcode & 0x7FFFu;
    auto masks = storylandVcsCharacterOperandMasks(command);
    if (!masks.inputs && !masks.outputs) return true;
    const int index = storylandVcsOperandIndex(vm.scriptBytes(), instruction.offset, operandOffset);
    if (index < 0) return true;
    if (command == 0x0505 && index == 2) {
        size_t kindOffset = 0;
        if (!storylandVcsOperandOffset(vm.scriptBytes(), instruction.offset, 1, kindOffset)) return false;
        const size_t continuation = vm.pc;
        vm.pc = kindOffset;
        Operand kind;
        const bool valid = readRawOperand(vm, kind);
        vm.pc = continuation;
        if (!valid) return false;
        if (kind.bits == 0) masks.inputs |= 1u << 2;
    }
    const uint32_t bit = 1u << unsigned(index);
    result.characterOutput = (masks.outputs & bit) != 0;
    if (masks.inputs & bit)
        result.bits = storylandVcsCharacterFromScript(result.bits, true);
    return true;
}

bool writeOperand(StorylandMissionVm& vm, const Operand& operand, int32_t value) {
    if (!operand.reference) return false;
    if (operand.characterOutput)
        value = storylandVcsCharacterToScript(value, vm.vcs && vm.environment &&
            vm.environment->vcsCharacterHandleBridge);
    if (operand.arrayScratch) {
        vm.environment->vcsArrayScratch = value;
        return true;
    }
    if (operand.global) {
        if (operand.variable >= vm.globals.size()) return false;
        vm.globals[operand.variable] = value;
        vm.globalStrings.erase(operand.variable);
    } else {
        if (operand.variable >= vm.locals.size()) return false;
        vm.locals[operand.variable] = value;
        vm.localStrings.erase(operand.variable);
    }
    return true;
}
bool readStringOperand(StorylandMissionVm& vm,Operand& result,std::string& text) {
    text.clear();
    if(vm.pc<vm.scriptBytes().size()&&vm.scriptBytes()[vm.pc]==0x0A) {
        result={};result.type=0x0A;
        ++vm.pc;
        while(vm.pc<vm.scriptBytes().size()&&text.size()<128) {
            const uint8_t character=vm.scriptBytes()[vm.pc++];
            if(character==0)return true;
            if(character<32||character>126)return false;
            text.push_back(char(character));
        }
        return false;
    }
    if(!readOperand(vm,result))return false;
    if(result.reference) {
        const auto& strings=result.global?vm.globalStrings:vm.localStrings;
        const auto found=strings.find(result.variable);
        if(found!=strings.end())text=found->second;
    }
    return true;
}
float operandFloat(const Operand& operand);
bool readScmText(StorylandMissionVm& vm,std::string& value) {
    value.clear();
    if(vm.pc>=vm.scriptBytes().size())return false;
    if(vm.scriptBytes()[vm.pc]==0x0A) {
        ++vm.pc;
        // SCM short strings are at most eight characters; an exactly-eight-
        // character label need not have a trailing NUL byte.
        for(size_t length=0;length<8&&vm.pc<vm.scriptBytes().size();++length) {
            const uint8_t character=vm.scriptBytes()[vm.pc++];
            if(character==0)return true;
            if(character<32||character>126)return false;
            value.push_back(char(character));
        }
        return value.size()==8;
    }
    if(vm.scriptBytes().size()-vm.pc<8)return false;
    for(size_t index=0;index<8&&vm.scriptBytes()[vm.pc+index];++index) {
        const uint8_t character=vm.scriptBytes()[vm.pc+index];
        if(character<32||character>126)return false;
        value.push_back(char(character));
    }
    vm.pc+=8;
    return true;
}
void updateScmCondition(StorylandMissionVm& vm,bool matches,bool inverted) {
    if(inverted)matches=!matches;
    if(vm.conditionCount>0) {
        vm.condition=vm.conditionOr?(vm.condition||matches):(vm.condition&&matches);
        --vm.conditionCount;
    } else vm.condition=matches;
    vm.conditionValid=true;
}
bool readVcsMissionText(StorylandMissionVm& vm,std::string& text);
int executeCutsceneCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    const uint16_t load=vm.vcs?0x01CB:0x02E9;
    const uint16_t start=vm.vcs?0x01CC:0x02EC;
    const uint16_t finished=vm.vcs?0x01CD:0x02EE;
    const uint16_t clear=vm.vcs?0x01CE:0x02EF;
    const uint16_t loaded=vm.vcs?0x03AB:0x05EE;
    const uint16_t offset=vm.vcs?0x015E:0x0249;
    const uint16_t create=vm.vcs?0xFFFF:0x02EA;
    const uint16_t animate=vm.vcs?0xFFFF:0x02EB;
    const uint16_t time=vm.vcs?0xFFFF:0x02ED;
    const uint16_t loop=vm.vcs?0xFFFF:0x04C1;
    if(command!=load&&command!=start&&command!=finished&&command!=clear&&command!=loaded&&
        command!=offset&&command!=create&&command!=animate&&command!=time&&command!=loop)return 0;
    if(!vm.environment) {error="Cutscene command requires the Play Test environment.";return -1;}
    auto& scene=vm.environment->cutscene;
    if(command==load) {
        std::string name;
        if(!(vm.vcs?readVcsMissionText(vm,name):readScmText(vm,name))||name.empty()) {error="LOAD_CUTSCENE has an invalid name.";return -1;}
        scene.name=name;scene.loaded=false;scene.playing=false;scene.finished=false;
        scene.elapsedMilliseconds=0;scene.loop=false;scene.offsetSet=false;
        scene.actors.clear();scene.nextActorHandle=1;
        vm.environment->cutsceneSkipped=false;
        ++scene.loadRequest;return 1;
    }
    if(command==loaded||command==finished) {
        const bool cutsceneFinished=scene.finished||(!vm.vcs&&scene.loadRequest==0);
        updateScmCondition(vm,command==loaded?scene.loaded:cutsceneFinished,inverted);
        return 1;
    }
    if(command==start) {
        if(!scene.loaded) {error="START_CUTSCENE requires a successfully loaded cutscene.";return -1;}
        scene.playing=true;scene.finished=false;scene.elapsedMilliseconds=0;
        ++scene.startRequest;return 1;
    }
    if(command==clear) {
        scene.playing=false;scene.loaded=false;scene.finished=false;
        if(vm.vcs)vm.environment->restorePlayerAfterCutscene();
        else {
            vm.environment->lcsExtendedWorld.cutsceneAttachments.clear();
            vm.environment->lcsExtendedWorld.cutsceneShadows=false;
            vm.environment->lcsExtendedWorld.uncompressedCutsceneAnimations.clear();
        }
        scene.actors.clear();++scene.clearRequest;return 1;
    }
    if(command==offset) {
        std::array<Operand,3> position{};
        for(auto& value:position)if(!readOperand(vm,value)) {error="CUTSCENE_OFFSET has invalid coordinates.";return -1;}
        scene.offsetX=operandFloat(position[0]);scene.offsetY=operandFloat(position[1]);scene.offsetZ=operandFloat(position[2]);
        if(!std::isfinite(scene.offsetX)||!std::isfinite(scene.offsetY)||!std::isfinite(scene.offsetZ)) {
            error="CUTSCENE_OFFSET contains non-finite coordinates.";return -1;
        }
        scene.offsetSet=true;
        return 1;
    }
    if(command==time) {
        Operand destination;
        if(!readOperand(vm,destination)||!writeOperand(vm,destination,int32_t(scene.elapsedMilliseconds))) {
            error="GET_CUTSCENE_TIME needs an output variable.";return -1;
        }
        return 1;
    }
    if(command==create) {
        Operand model,destination;
        if(!readOperand(vm,model)||!readOperand(vm,destination)||!destination.reference) {
            error="CREATE_CUTSCENE_OBJECT has invalid operands.";return -1;
        }
        if(scene.actors.size()>=128) {error="Cutscene actor limit exceeded.";return -1;}
        const int32_t handle=scene.nextActorHandle++;
        scene.actors.push_back({handle,model.bits,{}});
        writeOperand(vm,destination,handle);return 1;
    }
    if(command==animate) {
        Operand handle;std::string name;
        if(!readOperand(vm,handle)||!readScmText(vm,name)) {
            error="SET_CUTSCENE_ANIM has invalid operands.";return -1;
        }
        const auto actor=std::find_if(scene.actors.begin(),scene.actors.end(),
            [&](const auto& value){return value.handle==handle.bits;});
        if(actor==scene.actors.end()) {error="SET_CUTSCENE_ANIM refers to an unknown actor.";return -1;}
        actor->animation=name;return 1;
    }
    if(command==loop) {
        std::string name;
        if(!readScmText(vm,name)) {error="CUTSCENE_LOOP has an invalid clip name.";return -1;}
        // LCS PS2 dispatches 04C1 to an empty handler. It does not loop the
        // cutscene camera, audio, or the scene-completion timer.
        return 1;
    }
    return 0;
}
int executeMissionAudio(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(vm.vcs&&(command==0x04FB||command==0x0540)) {
        if(!vm.environment) {error="Script music position requires a live audio environment.";return -1;}
        auto& world=*vm.environment;
        if(command==0x0540) {
            Operand stream,position;
            if(!readOperand(vm,stream)||!readOperand(vm,position)||stream.bits<0||
                size_t(stream.bits)>=world.scriptMusicStartPositions.size()||position.bits<0) {
                error="Script music position requires a valid stream and nonnegative position.";return -1;
            }
            world.scriptMusicStartPositions[size_t(stream.bits)]=position.bits;
            return 1;
        }
        Operand output;
        if(!readOperand(vm,output)) {error="Script music position output is truncated.";return -1;}
        if(!world.musicStreamPositionMilliseconds&&world.audioSubsystemEnabled) {
            error="Script music position requires the live audio mixer.";return -1;
        }
        const uint64_t position=world.musicStreamPositionMilliseconds?world.musicStreamPositionMilliseconds():0;
        if(!writeOperand(vm,output,storylandVcsMusicPositionFromMilliseconds(position))) {
            error="Script music position output is not writable.";return -1;
        }
        return 1;
    }
    if(vm.vcs&&(command==0x022B||command==0x02A0||command==0x02A1||command==0x02A2||command==0x0402)) {
        if(!vm.environment) {error="Script music requires a live audio environment.";return -1;}
        auto& world=*vm.environment;
        Operand operand;
        if((command==0x022B||command==0x02A0||command==0x0402)&&!readOperand(vm,operand)) {
            error="Script music has a truncated typed operand.";return -1;
        }
        if(command==0x02A0) {world.musicDoesFade=operand.bits!=0;return 1;}
        if(command==0x0402) {
            if(!world.musicStreamPlaying) {error="Music stream query requires the live audio mixer.";return -1;}
            updateScmCondition(vm,world.musicStreamPlaying(uint8_t(operand.bits)),inverted);return 1;
        }
        if(command==0x02A2) {
            if(world.stopMissionPassedTune)world.stopMissionPassedTune();
            world.missionPassedTune=-1;world.missionTuneLoop=false;++world.missionPassedTuneSerial;
            return 1;
        }
        // 0x3667E8 truncates the selector twice: uint8(selector + 109).
        // End-of-game playback uses that same theme stream with looping enabled.
        const int32_t stream=command==0x02A1?109:uint8_t(uint8_t(operand.bits)+109);
        if(stream>=112) {error="Mission tune selector addresses outside the native 112-stream table.";return -1;}
        if(world.audioSubsystemEnabled) {
            if(!world.playMissionPassedTune) {error="Mission tune requires a live retail stream consumer.";return -1;}
            if(!world.playMissionPassedTune(stream,command==0x02A1,error))return -1;
        }
        world.missionPassedTune=stream;world.missionTuneLoop=command==0x02A1;
        ++world.missionPassedTuneSerial;
        return 1;
    }
    if(command==(vm.vcs?0x0302:0x04E8)) {
        Operand player,mood,duration;
        if(!vm.environment||!readOperand(vm,player)||!readOperand(vm,mood)||!readOperand(vm,duration)||
            player.bits!=0||mood.bits<0||mood.bits>3||duration.bits<0) {
            error="SET_PLAYER_MOOD requires player zero, mood 0..3 and a nonnegative duration.";return -1;
        }
        vm.environment->playerSpeechMood=mood.bits;
        vm.environment->playerSpeechMoodMilliseconds=uint32_t(duration.bits);return 1;
    }
    if(vm.vcs&&command==0x0410) {
        Operand destination;
        if(!vm.environment||!readOperand(vm,destination)||!destination.reference) {
            error="GET_FREE_MISSION_AUDIO_SLOT requires a writable output.";return -1;
        }
        // The native audio facade returns zero without touching the reservation
        // table when the audio subsystem itself is disabled.
        int32_t available=0;
        if(vm.environment->audioSubsystemEnabled) {
            available=-1;
            for(size_t index=0;index<5;++index) {
                auto& slot=vm.environment->audioSlots[index];
                if(!slot.reserved) {available=int32_t(index);slot.reserved=true;break;}
            }
        }
        if(!writeOperand(vm,destination,available)) {error="Mission audio output is invalid.";return -1;}
        return 1;
    }
    if(vm.vcs&&command==0x0413) {
        std::string name,bank;Operand bankOperand,index,flags;
        if(!vm.environment||!readVcsMissionText(vm,name)||!readStringOperand(vm,bankOperand,bank)||
            !readOperand(vm,index)||!readOperand(vm,flags)) {
            error="LOAD_MISSION_AUDIO_WITH_SUBTITLE requires two strings, a slot, and flags.";return -1;
        }
        if(!vm.environment->audioSubsystemEnabled)return 1;
        const uint8_t nativeIndex=uint8_t(index.bits);
        if(nativeIndex>=5)return 1;
        auto& slot=vm.environment->audioSlots[nativeIndex];
        const bool subtitlePresent=bankOperand.type==0x0A||bankOperand.bits!=0||
            (bankOperand.reference&&(bankOperand.global?vm.globalStrings:vm.localStrings).count(bankOperand.variable)!=0);
        if(name.empty()) {
            // The native wrapper records metadata, but its loader ignores an
            // empty sample name and leaves the current resource alone.
            slot.priority=false;slot.flags=flags.bits;slot.subtitleKey=bank;
            slot.vcsExtendedLoad=true;slot.vcsSubtitlePresent=subtitlePresent;slot.vcsReleasePending=false;
            return 1;
        }
        const bool reserved=slot.reserved;
        slot={};slot.reserved=reserved;slot.name=name;slot.subtitleKey=bank;slot.flags=flags.bits;
        slot.vcsExtendedLoad=true;
        slot.vcsResourceActive=!name.empty();
        slot.vcsSubtitlePresent=subtitlePresent;
        slot.serial=vm.environment->nextAudioSerial++;return 1;
    }
    if(vm.vcs&&command==0x04A1) {
        Operand index,output;
        if(!vm.environment||!readOperand(vm,index)||!readOperand(vm,output)||!output.reference||index.bits<0||index.bits>=5) {
            error="GET_MISSION_AUDIO_DURATION requires a valid slot and output.";return -1;
        }
        return writeOperand(vm,output,vm.environment->audioSubsystemEnabled?
            int32_t(vm.environment->audioSlots[size_t(index.bits)].durationMilliseconds):0)?1:-1;
    }
    if(vm.vcs&&command==0x0411) {
        Operand output;
        if(!vm.environment||!readOperand(vm,output)||!output.reference)return -1;
        int32_t count=0;
        if(vm.environment->audioSubsystemEnabled)
            for(size_t index=0;index<5;++index)if(vm.environment->audioSlots[index].reserved)++count;
        if(!writeOperand(vm,output,count))return -1;
        return 1;
    }
    if(vm.vcs&&command==0x04EB) {
        Operand index;
        if(!vm.environment||!readOperand(vm,index)||index.bits<0||index.bits>=5) {
            error="Mission audio priority slot is outside 0..4.";return -1;
        }
        vm.environment->audioSlots[size_t(index.bits)].priority=true;return 1;
    }
    const uint16_t load=vm.vcs?0x0257:0x03D4;
    const uint16_t loaded=vm.vcs?0x0258:0x03D5;
    const uint16_t play=vm.vcs?0x0259:0x03D6;
    const uint16_t finished=vm.vcs?0x025A:0x03D7;
    const uint16_t position=vm.vcs?0x025F:0x03DC;
    const uint16_t clear=vm.vcs?0x0284:0x0412;
    const bool release=vm.vcs&&command==0x0412;
    if(command!=load&&command!=loaded&&command!=play&&command!=finished&&command!=position&&command!=clear&&!release)return 0;
    if(!vm.environment) {error="Mission audio requires a runtime environment.";return -1;}
    Operand operand;
    if(!readOperand(vm,operand)) {error="Mission audio has a truncated slot operand.";return -1;}
    const int32_t slotIndex=vm.vcs?int32_t(uint8_t(operand.bits)):operand.bits;
    if(vm.vcs&&slotIndex>=5) {
        // Native script handlers narrow the slot to a byte. Audio-manager
        // queries treat an invalid slot as complete; mutations do not act.
        if(command==loaded||command==finished)updateScmCondition(vm,true,inverted);
        else if(command==load) {
            std::string ignored;
            if(!readVcsMissionText(vm,ignored)) {error="Mission audio has a truncated sound name.";return -1;}
        } else if(command==play) {
            Operand ignored;
            if(!readOperand(vm,ignored)) {error="Mission audio play has no ducking operand.";return -1;}
        } else if(command==position) {
            for(unsigned axis=0;axis<3;++axis) {
                Operand ignored;
                if(!readOperand(vm,ignored)) {error="Mission audio position is truncated.";return -1;}
            }
        }
        return 1;
    }
    if(slotIndex<0||size_t(slotIndex)>=vm.environment->audioSlots.size()) {
        error="Mission audio slot is outside the supported slot table.";return -1;
    }
    // The VCS audio facade ignores slot clear and release while its
    // subsystem is disabled; the LCS clear command remains independent.
    if(vm.vcs&&(release||command==clear)&&!vm.environment->audioSubsystemEnabled)return 1;
    auto& slot=vm.environment->audioSlots[size_t(slotIndex)];
    if(command==load) {
        std::string name;
        if(!(vm.vcs?readVcsMissionText(vm,name):readScmText(vm,name))) {error="Mission audio has a truncated sound name.";return -1;}
        if(vm.vcs&&!vm.environment->audioSubsystemEnabled)return 1;
        if(vm.vcs&&name.empty())return 1;
        const bool reserved=slot.reserved;
        const bool retain=slot.priority,extended=slot.vcsExtendedLoad,subtitlePresent=slot.vcsSubtitlePresent;
        const bool pending=slot.vcsReleasePending;
        const int32_t oldFlags=slot.flags;
        const std::string subtitle=slot.subtitleKey;
        slot={};slot.reserved=reserved;slot.name=name;slot.serial=vm.environment->nextAudioSerial++;
        if(vm.vcs) {
            slot.priority=retain;slot.vcsExtendedLoad=extended;slot.vcsSubtitlePresent=subtitlePresent;
            slot.vcsReleasePending=pending;slot.flags=oldFlags;slot.subtitleKey=subtitle;
            slot.vcsResourceActive=!name.empty();
        }
    } else if(command==loaded||command==finished) {
        updateScmCondition(vm,!vm.environment->audioSubsystemEnabled||(command==loaded?slot.loaded:slot.finished),inverted);
    } else if(release) {
        vm.environment->requestVcsMissionAudioRelease(size_t(slotIndex));
    } else if(command==clear) {
        const bool reserved=slot.reserved,retain=slot.priority,extended=slot.vcsExtendedLoad;
        const bool subtitlePresent=slot.vcsSubtitlePresent,pending=slot.vcsReleasePending;
        const bool active=slot.vcsResourceActive||!slot.name.empty();
        const int32_t oldFlags=slot.flags;const std::string subtitle=slot.subtitleKey;
        slot={};slot.reserved=reserved;slot.serial=vm.environment->nextAudioSerial++;
        if(vm.vcs) {
            slot.priority=retain;slot.vcsExtendedLoad=extended;slot.vcsSubtitlePresent=subtitlePresent;
            slot.vcsReleasePending=pending;slot.vcsResourceActive=active;
            slot.flags=oldFlags;slot.subtitleKey=subtitle;
        }
    } else if(command==play) {
        bool enabled=true;
        if(vm.vcs) {Operand value;if(!readOperand(vm,value)) {error="Mission audio play has no ducking operand.";return -1;}enabled=value.bits!=0;}
        if(vm.vcs&&slot.loaded&&slot.finished&&!slot.name.empty()) {
            // Native1C6D78 requeues a retained finished slot without changing its sample.
            slot.serial=vm.environment->nextAudioSerial++;
            slot.finished=false;slot.playbackStarted=false;
        }
        slot.play=true;slot.playFlag=enabled;
    } else {
        Operand x,y,z;
        if(!readOperand(vm,x)||!readOperand(vm,y)||!readOperand(vm,z)) {error="Mission audio position is truncated.";return -1;}
        const float px=operandFloat(x),py=operandFloat(y),pz=operandFloat(z);
        if(!std::isfinite(px)||!std::isfinite(py)||!std::isfinite(pz)) {
            error="Mission audio position must be finite.";return -1;
        }
        slot.x=px;slot.y=py;slot.z=pz;
        slot.positioned=true;
    }
    return 1;
}

int executeRetailWorldCommand(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    if(vm.vcs&&command==0x00B8) {
        Operand scale;
        if(!vm.environment||!readOperand(vm,scale)||!std::isfinite(operandFloat(scale))||operandFloat(scale)<0) {
            error="SET_TIME_SCALE requires a finite nonnegative scale.";return -1;
        }
        vm.environment->nativeTimeScale=operandFloat(scale);return 1;
    }
    if(vm.vcs&&command==0x005C) {
        Operand hour,minute,output;
        if(!vm.environment||!readOperand(vm,hour)||!readOperand(vm,minute)||
            !readOperand(vm,output)||!output.reference) {
            error="GET_MINUTES_TO_TIME_OF_DAY requires two clock bytes and an output.";return -1;
        }
        // Native2287A8 narrows both inputs to bytes and adds one day only
        // when the target lies before the current clock, without modulo24.
        if(!std::isfinite(vm.environment->hour)||vm.environment->hour<0||vm.environment->hour>=24) {
            error="Clock minute query requires a valid current time.";return -1;
        }
        const uint32_t minutes=storylandVcsMinutesUntil(vm.environment->hour,hour.bits,minute.bits);
        if(!writeOperand(vm,output,int32_t(minutes))) {
            error="Clock minute output is invalid.";return -1;
        }
        return 1;
    }
    if(vm.vcs&&command==0x032A) {
        Operand output;
        if(!vm.environment||!readOperand(vm,output)||!output.reference) {
            error="GET_RADIO_CHANNEL requires a writable station output.";return -1;
        }
        const auto& world=*vm.environment;
        // Native3D5FE8 filters the live music manager result, retaining the
        // nine public stations and special channel111 only. An uninitialized
        // audio manager returns zero (188F98), rather than the off sentinel.
        const int32_t channel=!world.audioSubsystemEnabled?0:
            world.currentRadioChannel?world.currentRadioChannel():world.radioChannel;
        if(!writeOperand(vm,output,uint32_t(channel)<9||channel==111?channel:-1)) {
            error="GET_RADIO_CHANNEL output is invalid.";return -1;
        }
        return 1;
    }
    const bool addRoofCheck=command==(vm.vcs?0x00DD:0x0195);
    const bool removeRoofCheck=command==(vm.vcs?0x00DE:0x0196);
    const bool stuckOnRoof=command==(vm.vcs?0x00DC:0x0194);
    const bool upsideDown=command==(vm.vcs?0x012D:0x01F9);
    if(addRoofCheck||removeRoofCheck||stuckOnRoof||upsideDown) {
        Operand handle;
        if(!vm.environment||!readOperand(vm,handle)) {error="Upside-down vehicle command requires a handle.";return -1;}
        auto& world=*vm.environment;
        if(removeRoofCheck) {world.upsideDownCarChecks.erase(handle.bits);return 1;}
        if(stuckOnRoof) {
            const auto found=world.upsideDownCarChecks.find(handle.bits);
            updateScmCondition(vm,found!=world.upsideDownCarChecks.end()&&found->second>1000,inverted);return 1;
        }
        const auto found=world.missionVehicles.find(handle.bits);
        if(found==world.missionVehicles.end()||found->second.deleted) {
            error="Upside-down vehicle command references an unknown car.";return -1;
        }
        if(addRoofCheck) {
            if(world.upsideDownCarChecks.size()<6||world.upsideDownCarChecks.count(handle.bits))
                world.upsideDownCarChecks[handle.bits]=0;
        } else {
            if(!world.vehicleOrientationAndSpeed) {error="Vehicle orientation query requires live physics.";return -1;}
            updateScmCondition(vm,world.vehicleOrientationAndSpeed(handle.bits)[0]<=-0.97f,inverted);
        }
        return 1;
    }
    if(command==(vm.vcs?0x021C:0x036F)||(!vm.vcs&&command==0x036E)) {
        Operand characterHandle,vehicleHandle;
        if(!vm.environment||!readOperand(vm,characterHandle)||!readOperand(vm,vehicleHandle)) {
            error="WARP_CHAR_INTO_CAR requires character and vehicle handles.";return -1;
        }
        if(!vm.vcs&&command==0x036E) {
            if(characterHandle.bits!=0) {error="WARP_PLAYER_INTO_CAR requires player zero.";return -1;}
            characterHandle.bits=1;
        }
        auto& world=*vm.environment;
        const auto found=world.missionVehicles.find(vehicleHandle.bits);
        if(found==world.missionVehicles.end()||found->second.deleted) {
            error="WARP_CHAR_INTO_CAR references an unknown vehicle.";return -1;
        }
        auto& vehicle=found->second;
        if(characterHandle.bits==(vm.vcs?0:1)) {
            world.x=vehicle.x;world.y=vehicle.y;world.z=vehicle.z;world.heading=vehicle.heading;
            world.inVehicle=true;world.occupiedMissionVehicle=vehicleHandle.bits;
            world.occupiedVehicleModel=vehicle.model;world.playerWarped=true;
            world.playerPassengerSeat=-1;world.playerVehicleTransition=false;world.playerExitVehicleRequested=false;
            world.playerAttachedVehicle=-1;world.playerAttachment={};
            // VCS 417008 ->212FD0,213154..213190 selects STATUS_PLAYER from
            // the actual driver. It preserves the existing autopilot fields.
            vehicle.scriptStatus=0;vehicle.engineOn=true;
            if(vm.vcs) {
                world.playerWalkTask.clear();
                world.playerVehicleEntryTarget=-1;world.playerVehicleEntryAsPassenger=false;
            }
        } else {
            const auto character=world.missionCharacters.find(characterHandle.bits);
            if(character==world.missionCharacters.end()) {error="WARP_CHAR_INTO_CAR references an unknown character.";return -1;}
            auto& ped=character->second;
            ped.vehicle=vehicleHandle.bits;ped.passengerSeat=-1;ped.attached=false;ped.attachment={};
            ped.x=vehicle.x;ped.y=vehicle.y;ped.z=vehicle.z;ped.heading=vehicle.heading;
            ped.walkingToTarget=false;world.characterWanderTasks.erase(characterHandle.bits);
        }
        vehicle.driver=characterHandle.bits;
        vehicle.engineOn=true;
        return 1;
    }
    if(command==(vm.vcs?0x009D:0x0117)) {
        updateScmCondition(vm,vm.deathArrestExecuted,inverted);return 1;
    }
    if(!vm.vcs&&command==0x0621) {
        updateScmCondition(vm,false,inverted);return 1;
    }
    if(!vm.vcs&&(command==0x0618||command==0x061F)) {
        if(command==0x061F) {
            Operand level;
            if(!readOperand(vm,level)) {error="BUILD_WORLD_GEOMETRY requires a level.";return -1;}
        }
        // Retail is neither an E3 build nor the developer world-geometry builder.
        updateScmCondition(vm,false,inverted);return 1;
    }
    if(command==(vm.vcs?0x007B:0x00DE)||command==(vm.vcs?0x024B:0x03C5)) {
        Operand character,output;
        if(!vm.environment||!readOperand(vm,character)||!readOperand(vm,output)||!output.reference) {
            error="STORE_CAR_CHAR_IS_IN requires a character and writable handle.";return -1;
        }
        auto& world=*vm.environment;
        int32_t vehicle=-1;
        if(character.bits==(vm.vcs?0:1)) {
            if(world.inVehicle) {
                vehicle=world.occupiedMissionVehicle;
                if(vehicle<0&&world.acquireOccupiedVehicle)vehicle=world.acquireOccupiedVehicle();
                world.occupiedMissionVehicle=vehicle;
            }
        } else {
            const auto found=world.missionCharacters.find(character.bits);
            if(found==world.missionCharacters.end()) {error="Vehicle query references an unknown character.";return -1;}
            vehicle=found->second.vehicle;
        }
        const auto found=world.missionVehicles.find(vehicle);
        if(found==world.missionVehicles.end()||found->second.deleted)vehicle=-1;
        else if(vm.isMissionThread&&command==(vm.vcs?0x007B:0x00DE))found->second.persistent=true;
        if(!writeOperand(vm,output,vehicle)) {error="Vehicle query output is invalid.";return -1;}
        return 1;
    }
    if(command==(vm.vcs?0x01B3:0x02C4)) {
        Operand vehicle;
        if(!vm.environment||!readOperand(vm,vehicle)) {error="IS_CAR_IN_WATER requires a vehicle handle.";return -1;}
        const auto& world=*vm.environment;
        const auto found=world.missionVehicles.find(vehicle.bits);
        const bool inWater=found!=world.missionVehicles.end()&&!found->second.deleted&&
            world.vehicleInWater&&world.vehicleInWater(vehicle.bits);
        updateScmCondition(vm,inWater,inverted);return 1;
    }
    const bool closestCarNode=command==(vm.vcs?0x01B5:0x02C6);
    const bool closestCarHeading=command==(vm.vcs?0x025B:0x03D8);
    const bool closestPedNode=vm.vcs&&command==0x01B4;
    const bool closestWaterNode=vm.vcs&&command==0x04C4;
    const bool nthCarNode=vm.vcs&&command==0x02F5;
    const bool nthPedNode=vm.vcs&&command==0x02F6;
    if(closestCarNode||closestCarHeading||closestPedNode||closestWaterNode||nthCarNode||nthPedNode) {
        const bool nth=nthCarNode||nthPedNode,vehicle=!closestPedNode&&!nthPedNode;
        std::array<Operand,7> values{};
        const size_t outputStart=nth?4:3,count=outputStart+(closestCarHeading?4:3);
        for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
            error="Closest path node has truncated operands.";return -1;
        }
        if(!vm.environment||!vm.environment->pedestrianPaths) {
            error="Closest path node requires the native path graph.";return -1;
        }
        for(size_t index=outputStart;index<count;++index)if(!values[index].reference) {
            error="Closest path node requires writable coordinate outputs.";return -1;
        }
        auto& world=*vm.environment;
        const auto& paths=*world.pedestrianPaths;const auto& nodes=paths.nodes;
        StorylandNativePoint target{operandFloat(values[0]),operandFloat(values[1]),operandFloat(values[2])};
        if(!std::isfinite(target.x)||!std::isfinite(target.y)||!std::isfinite(target.z)) {
            error="Closest path node requires finite coordinates.";return -1;
        }
        if(vm.vcs?(target.z==-100||target.z<=-250):target.z<=-100) {
            if(!world.groundHeightAt) {error="Closest path node needs native ground collision for this height.";return -1;}
            // Native FindGroundZ returns20 when its collision trace misses.
            if (closestWaterNode) {
                target.z = 20;
            }
            float height=0;
            if(world.groundHeightAt({target.x,target.y,1000},height)&&std::isfinite(height))target.z=height;
        }
        const auto roadFlags=storylandNativeRoadNodeFlags(paths,world.roadChanges);
        const StorylandNativePathNode* selected=nullptr;
        uint32_t selectedIndex=UINT32_MAX;
        if(vm.vcs) {
            if(paths.vcsSearchGrid.size()!=2500) {
                error="VCS closest path node requires its retained native spatial grid.";return -1;
            }
            const size_t kind=vehicle?0:1;
            auto& marked=world.pathSearchMarkedNodes[kind];
            if(nth)marked.clear();
            // Native268750 truncates cell coordinates after three separate EE
            // float operations. The query rectangle includes complete80-unit
            // cells, not only positions inside the nominal radius.
            const auto cell=[](float coordinate,float offset,float bias) {
                const float sum=roundEeFloat(double(coordinate)+offset);
                const float scaled=roundEeFloat(double(sum)*0.012500000186264515);
                return int(std::clamp(roundEeFloat(double(scaled)+bias),0.0f,49.0f));
            };
            const float limit=nth?999999.875f:800.0f;
            for(unsigned pass=0;pass<2&&!selected;++pass) {
                const float radius=pass?limit:100.0f;
                float best=roundEeFloat(double(radius)*radius);
                const int minX=cell(target.x,-radius,30),maxX=cell(target.x,radius,30);
                const int minY=cell(target.y,-radius,25),maxY=cell(target.y,radius,25);
                bool exact=false;
                for(int y=minY;y<=maxY&&!exact;++y)for(int x=minX;x<=maxX&&!exact;++x)
                    for(uint16_t index:paths.vcsSearchGrid[size_t(x)*50+size_t(y)][kind]) {
                        if(index>=nodes.size())continue;
                        const auto& node=nodes[index];
                        // The water query calls the same retail search with
                        // waterMode=1; road and pedestrian queries pass zero.
                        const bool water=(node.flags&0x100)!=0;
                        if(water!=closestWaterNode||((vehicle||nth)&&(node.flags&0x40))||
                            (!nth&&marked.count(index)))continue;
                        // Every SCM caller passes includeDisabled=1: neither
                        // node0x20 nor SWITCH_ROADS_OFF excludes these queries.
                        const float dx=std::fabs(roundEeFloat(double(node.position.x)-target.x));
                        const float dy=std::fabs(roundEeFloat(double(node.position.y)-target.y));
                        const float dz=std::fabs(roundEeFloat(double(node.position.z)-target.z));
                        const float distance=roundEeFloat(double(roundEeFloat(double(dx)+dy))+
                            roundEeFloat(double(dz)*3.0));
                        if(distance<best) {
                            best=distance;selected=&node;selectedIndex=index;
                            if(distance<0.010000000707805157f) {exact=true;break;}
                        }
                    }
            }
            // Retail268CF0 marks earlier choices but passes includeMarked=1
            // to268880, bypassing the0x200 exclusion at268AB0. Consequently
            // positive nth values repeat the nearest result. Retain its flag
            // mutation because later non-nth queries do exclude that node.
            if(nth&&selected&&int32_t(uint32_t(values[3].bits)-1u)>0)marked.insert(selectedIndex);
            if(!selected&&!vehicle) {
                error="Closest pedestrian node has no native candidate; retail would address an invalid node.";return -1;
            }
        } else {
            float best=closestCarHeading?999999.9f:800.f;
            for(const auto& node:nodes) {
                if(!node.vehicle||node.water||(roadFlags[size_t(&node-nodes.data())]&0x60u))continue;
                const float distance=std::fabs(node.position.x-target.x)+std::fabs(node.position.y-target.y)+
                    3*std::fabs(node.position.z-target.z);
                if(distance<best) {best=distance;selected=&node;}
            }
        }
        StorylandNativePoint point{};float heading=0;
        if(selected) {
            point=selected->position;
            if(vehicle&&!selected->links.empty()&&selected->links.front().target<nodes.size()) {
                const auto& next=nodes[selected->links.front().target].position;
                float x=next.x-point.x,y=next.y-point.y;
                const float length=std::hypot(x,y);
                if(length>0) {x/=length;y/=length;}
                else if(vm.vcs) {x=1;y=0;}
                if(length>0||vm.vcs) {
                    heading=std::atan2(-x,y)*(vm.vcs?57.2957763671875f:57.29577951308232f);
                    if(!vm.vcs&&heading<0)heading+=360;
                    if(selected->width) {
                        if(x<0){x=-x;y=-y;}
                        const float offset=2.5f+selected->width/16.0f;
                        point.x-=y*offset;point.y+=x*offset;
                    }
                }
            }
        }
        const std::array<float,4> output{point.x,point.y,point.z,heading};
        for(size_t index=outputStart;index<count;++index) {
            int32_t bits=0;std::memcpy(&bits,&output[index-outputStart],sizeof(bits));
            if(!writeOperand(vm,values[index],bits)) {error="Closest path node output is invalid.";return -1;}
        }
        return 1;
    }
    const bool numberedNow=command==(vm.vcs?0x011F:0x01EA);
    const bool twoNumbersNow=command==(vm.vcs?0x01D9:0x0302);
    const bool numberedBig=vm.vcs&&command==0x011D;
    if(numberedNow||twoNumbersNow||numberedBig) {
        std::string label;
        if(!vm.environment||!(vm.vcs?readVcsMissionText(vm,label):readScmText(vm,label))) {
            error="Numbered print requires a valid message label.";return -1;
        }
        const size_t numbers=twoNumbersNow?2:1;
        std::array<Operand,4> values{};
        for(size_t index=0;index<numbers+2;++index)if(!readOperand(vm,values[index])) {
            error="Numbered print has truncated parameters.";return -1;
        }
        if(numberedBig&&(values[numbers+1].bits<1||values[numbers+1].bits>8)) {
            error="Big message style must be from one through eight.";return -1;
        }
        auto& world=*vm.environment;
        world.messageKey=std::move(label);
        world.messageNumbers.clear();
        for(size_t index=0;index<numbers;++index)world.messageNumbers.push_back(values[index].bits);
        world.messageMilliseconds=uint32_t(std::clamp(values[numbers].bits,0,600000));
        world.messageStyle=numberedBig?values[numbers+1].bits:0;
        if(numberedBig)vm.observeMissionMessage(world.messageKey,world.messageStyle);
        return 1;
    }
    if(!vm.vcs&&command==0x011C) {
        Operand player;
        if(!vm.environment||!readOperand(vm,player)||player.bits!=0) {
            error="IS_PLAYER_DEAD requires the live player index.";return -1;
        }
        updateScmCondition(vm,vm.environment->health<=0,inverted);return 1;
    }
    if(command==(vm.vcs?0x02D7:0x0499)) {
        std::array<Operand,5> values{};
        for(auto& value:values)if(!readOperand(vm,value)) {
            error="GET_POSITION_OF_ANALOGUE_STICKS has truncated operands.";return -1;
        }
        if(!vm.environment||values[0].bits<0||values[0].bits>1) {
            error="GET_POSITION_OF_ANALOGUE_STICKS requires pad index zero or one.";return -1;
        }
        for(size_t index=1;index<values.size();++index)if(!values[index].reference) {
            error="GET_POSITION_OF_ANALOGUE_STICKS requires four writable outputs.";return -1;
        }
        for(size_t index=0;index<4;++index) {
            const int32_t position=values[0].bits==0?vm.environment->analogueSticks[index]:0;
            if(!writeOperand(vm,values[index+1],position)) {
                error="GET_POSITION_OF_ANALOGUE_STICKS could not write an output.";return -1;
            }
        }
        return 1;
    }
    if(vm.vcs&&(command==0x020C||command==0x04F1)) {
        std::array<Operand,8> values{};
        if(!vm.environment) {error="SLIDE_OBJECT requires a live world.";return -1;}
        for(auto& value:values)if(!readOperand(vm,value)) {
            error="SLIDE_OBJECT has truncated operands.";return -1;
        }
        auto& world=*vm.environment;
        const auto found=std::find_if(world.objects.begin(),world.objects.end(),
            [&](const auto& object){return object.handle==values[0].bits;});
        if(found==world.objects.end()) {error="SLIDE_OBJECT references an unknown object.";return -1;}
        std::array<float,3> target{},step{},position{found->x,found->y,found->z};
        for(size_t axis=0;axis<3;++axis) {
            target[axis]=operandFloat(values[axis+1]);
            step[axis]=operandFloat(values[axis+4]);
            if(!std::isfinite(target[axis])||!std::isfinite(step[axis])||(command==0x020C&&step[axis]<0)) {
                error="SLIDE_OBJECT requires finite targets and nonnegative axis steps.";return -1;
            }
        }
        if(command==0x04F1) {
            // Retail30EAF0 treats the supplied step as a point, including
            // translation, then takes absolute world components. Preserve
            // that observed behavior; this command does not scale by time.
            const auto matrix=storylandScriptObjectMatrix(found->x,found->y,found->z,
                found->rotationX,found->rotationY,found->heading);
            std::array<float,3> transformed{};
            for(size_t axis=0;axis<3;++axis) {
                transformed[axis]=std::fabs(matrix[axis]*step[0]+matrix[4+axis]*step[1]+matrix[8+axis]*step[2]+matrix[12+axis]);
                if(!std::isfinite(transformed[axis])) {error="Local SLIDE_OBJECT transform overflowed.";return -1;}
            }
            step=transformed;
        }
        if(position==target) {updateScmCondition(vm,true,inverted);return 1;}
        for(size_t axis=0;axis<3;++axis) {
            const float difference=target[axis]-position[axis];
            if(std::fabs(difference)<=step[axis])position[axis]=target[axis];
            else position[axis]+=difference<0?-step[axis]:step[axis];
        }
        if(values[7].bits) {
            if(!world.objectSlideCollision) {
                error="SLIDE_OBJECT collision checking requires the live entity collision query.";return -1;
            }
            const int collision=world.objectSlideCollision(found->handle,position);
            if(collision<0) {error="SLIDE_OBJECT cannot resolve the object's native collision bounds.";return -1;}
            if(collision>0) {updateScmCondition(vm,true,inverted);return 1;}
        }
        found->x=position[0];found->y=position[1];found->z=position[2];
        updateScmCondition(vm,position==target,inverted);
        return 1;
    }
    if(vm.vcs&&(command==0x012F||command==0x021B)) {
        Operand label;
        std::string text;
        std::array<Operand,8> values{};
        if(!vm.environment||!readStringOperand(vm,label,text)) {
            error="START_KILL_FRENZY requires a live world and text operand.";return -1;
        }
        for(auto& value:values)if(!readOperand(vm,value)) {
            error="START_KILL_FRENZY has truncated operands.";return -1;
        }
        const std::array<int32_t,4> models{{values[3].bits,values[4].bits,values[5].bits,values[6].bits}};
        if(!vm.environment->startKillFrenzy) {
            error="START_KILL_FRENZY requires the live weapon runtime.";return -1;
        }
        if(!vm.environment->startKillFrenzy(text,values[0].bits,values[1].bits,values[2].bits,
            models,values[7].bits!=0,command==0x021B,error))return -1;
        return 1;
    }
    if(vm.vcs&&command==0x0130) {
        Operand output;
        if(!vm.environment||!readOperand(vm,output)||
            !writeOperand(vm,output,vm.environment->killFrenzyStatus)) {
            error="READ_KILL_FRENZY_STATUS requires a live world and writable output.";
            return -1;
        }
        return 1;
    }
    if(command==(vm.vcs?0x00D9:0x0191)) {
        std::array<Operand,4> values{};
        if(!vm.environment) {error="ADD_ONE_OFF_SOUND requires a live world.";return -1;}
        for(auto& value:values)if(!readOperand(vm,value)) {
            error="ADD_ONE_OFF_SOUND has truncated operands.";return -1;
        }
        StorylandMissionEnvironment::ScriptSound sound;
        for(size_t axis=0;axis<3;++axis) {
            sound.position[axis]=operandFloat(values[axis]);
            if(!std::isfinite(sound.position[axis])) {
                error="Script sound position must be finite.";return -1;
            }
        }
        sound.id=uint16_t(values[3].bits);
        if(vm.environment->audioSubsystemEnabled) {
            if(vm.environment->scriptSounds.size()>=4096) {
                error="Script sound queue is full.";return -1;
            }
            vm.environment->scriptSounds.push_back(sound);
        }
        return 1;
    }
    if(vm.vcs&&command==0x0040) {
        Operand handle,direction;
        if(!vm.environment||!readOperand(vm,handle)||!readOperand(vm,direction)) {
            error="CHAR_WANDER_DIR requires a character and direction.";return -1;
        }
        if(handle.bits==0)return 1;
        auto& world=*vm.environment;
        const auto found=world.missionCharacters.find(handle.bits);
        if(found==world.missionCharacters.end()) {
            error="CHAR_WANDER_DIR references an unknown character.";return -1;
        }
        auto& character=found->second;
        if(!character.alive||character.health<=0)return 1;
        auto& task=world.characterWanderTasks[handle.bits];
        task.direction=direction.bits>=0&&direction.bits<8?direction.bits:
            int32_t((uint32_t(handle.bits)*1664525u+world.gameMilliseconds)%7u);
        task.node=UINT32_MAX;
        character.objective=0;
        character.walkingToTarget=false;
        character.animationId=-1;
        character.walkSpeed=1.25f;
        return 1;
    }
    if(vm.vcs&&command==0x0554) {
        std::array<Operand,4> counts{};
        if(!vm.environment) {error="Empire ownership totals require a live world.";return -1;}
        for(auto& count:counts)if(!readOperand(vm,count)) {
            error="Empire ownership totals have truncated operands.";return -1;
        }
        // Native handler 0x1862E0 copies the low byte of each argument.
        for(size_t gang=0;gang<counts.size();++gang)
            vm.environment->empireSitesOwned[gang]=uint8_t(counts[gang].bits);
        return 1;
    }
    if(!vm.vcs&&command==0x0417) {
        error="LCS PS2 0x0417 is an unused SWITCH_VEHICLE_WEAPONS slot (native dispatcher returns -1). "
            "The positional music command 0x0417 belongs to VCS.";
        return -1;
    }
    if(vm.vcs&&(command==0x0417||command==0x0418)) {
        std::array<Operand,5> values{};
        const size_t count=command==0x0417?5:3;
        if(!vm.environment) {error="Script music requires a live world.";return -1;}
        for(size_t index=0;index<count;++index)if(!readOperand(vm,values[index])) {
            error="Script music command has truncated operands.";return -1;
        }
        StorylandScriptMusicEmitter emitter;
        for(size_t axis=0;axis<3;++axis) {
            emitter.position[axis]=operandFloat(values[axis]);
            if(!std::isfinite(emitter.position[axis])) {error="Script music position must be finite.";return -1;}
        }
        auto& emitters=vm.environment->musicEmitters;
        if(command==0x0417) {
            emitter.radius=values[3].bits;emitter.stream=values[4].bits;
            emitters.push_back(emitter);
        } else {
            const auto found=std::find_if(emitters.begin(),emitters.end(),[&](const auto& existing) {
                return existing.position==emitter.position;
            });
            if(found!=emitters.end())emitters.erase(found);
        }
        return 1;
    }

    if(vm.vcs&&(command==0x0460||command==0x0463||command==0x0464||command==0x0467||
        command==0x046A||command==0x046B)) {
        Operand index,value;
        if(!vm.environment||!readOperand(vm,index)||!readOperand(vm,value)||
            index.bits<0||size_t(index.bits)>=vm.environment->empireSites.size()) {
            error="Empire visual command requires site 0..29 and a value or output.";return -1;
        }
        auto& site=vm.environment->empireSites[size_t(index.bits)];
        if(command==0x0467||command==0x046A||command==0x046B) {
            const int32_t result=command==0x0467?site.type:
                command==0x046A?int32_t(site.damaged):site.state;
            if(!writeOperand(vm,value,result)) {
                error="Empire property output is not writable.";
                return -1;
            }
        } else if(command==0x0460)site.type=value.bits;
        else if(command==0x0463)site.damaged=value.bits!=0;
        else site.state=value.bits;
        return 1;
    }
    if(vm.vcs&&(command==0x045F||command==0x0461||command==0x0462||
        command==0x0466||command==0x0468||command==0x0469)) {
        Operand site,value;
        if(!vm.environment||!readOperand(vm,site)||!readOperand(vm,value)||
            site.bits<0||size_t(site.bits)>=vm.environment->empireSites.size()) {
            error="Empire property command requires site 0..29 and a value or output.";return -1;
        }
        auto& state=vm.environment->empireSites[size_t(site.bits)];
        int32_t& property=command==0x045F||command==0x0466?state.owner:
            command==0x0461||command==0x0468?state.scale:state.condition;
        if(command<=0x0462)property=value.bits;
        else if(!writeOperand(vm,value,property)) {error="Empire property output is not writable.";return -1;}
        return 1;
    }
    if(vm.vcs&&command==0x04F6) {
        Operand enabled;
        if(!vm.environment||!readOperand(vm,enabled)) {
            error="ENABLE_EMPIRE_STATS requires a live world and a boolean operand.";return -1;
        }
        // Native 0x17EE20 normalizes any nonzero input to true.
        vm.environment->empireStatsEnabled=enabled.bits!=0;
        return 1;
    }
    if(vm.vcs&&(command==0x0465||command==0x046C)) {
        Operand site;
        std::array<Operand,3> outputs{};
        const size_t count=command==0x0465?3:1;
        if(!readOperand(vm,site)||site.bits<0||size_t(site.bits)>=StorylandVcsEmpireSites.size()) {
            error="Empire transform query requires a retail site index 0..29.";return -1;
        }
        for(size_t i=0;i<count;++i)if(!readOperand(vm,outputs[i])||!outputs[i].reference) {
            error="Empire transform query requires writable outputs.";return -1;
        }
        const auto& transform=StorylandVcsEmpireSites[size_t(site.bits)];
        for(size_t i=0;i<count;++i) {
            const float value=command==0x0465?transform[i]:transform[3]*57.2957763671875f;
            int32_t bits;std::memcpy(&bits,&value,sizeof(bits));
            if(!writeOperand(vm,outputs[i],bits)) {error="Empire transform output is invalid.";return -1;}
        }
        return 1;
    }
    if(!vm.vcs) {
        struct OpcodePair {uint16_t lcs,shared;};
        static constexpr OpcodePair lcsWorldOpcodes[]={
            {0x00A5,0x0048},
            {0x00AB,0x004E},
            {0x00AC,0x004F},
            {0x00E2,0x007D},
            {0x010C,0x0092},
            {0x011E,0x00A0},
            {0x0157,0x00AF},
            {0x0161,0x00B7},
            {0x0169,0x00BB},
            {0x016E,0x00C8},
            {0x0170,0x00CA},
            {0x01EC,0x0121},
            {0x01ED,0x0122},
            {0x020E,0x0139},
            {0x022F,0x0151},
            {0x0230,0x0152},
            {0x024C,0x0160},
            {0x024D,0x0161},
            {0x024E,0x0162},
            {0x02A8,0x01AE},
            {0x033C,0x0200},
            {0x0377,0x0220},
            {0x0378,0x0221},
            {0x0390,0x0228},
            {0x039A,0x022C},
            {0x03CC,0x024F},
            {0x03EB,0x0268},
            {0x0422,0x028D},
            {0x0475,0x02C0},
            {0x0477,0x02C2},
            {0x0487,0x02CB},
            {0x04DC,0x02F8},
            {0x057E,0x0352},
            {0x05AB,0x0377},
            {0x065C,0x03F1},
            {0x0663,0x03F8},
        };
        const auto found=std::find_if(std::begin(lcsWorldOpcodes),std::end(lcsWorldOpcodes),
            [&](const auto& pair) {return pair.lcs==command;});
        if(found==std::end(lcsWorldOpcodes))return 0;
        command=found->shared;
    }
    const bool supported=command==0x00AF||command==0x00B7||command==0x0139||
        command==0x0160||command==0x0161||
        command==0x0162||command==0x0221||command==0x024F||command==0x02C0||
        command==0x02DB||command==0x02F8||command==0x04DA||command==0x04EF||
        command==0x04F2||command==0x00C8||command==0x01AE||command==0x0200||
        command==0x0228||command==0x02CB||command==0x0352||command==0x0443||
        command==0x007D||command==0x0474||command==0x00CA||command==0x0564||
        command==0x042E||command==0x03C1||command==0x03F8||command==0x028D||
        command==0x047B||command==0x0268||command==0x0489||(vm.vcs&&command==0x049D)||
        command==0x004F||command==0x00A0||command==0x0280||command==0x0483||
        command==0x04C5||command==0x00BB||command==0x0152||command==0x0151||command==0x0121||command==0x0122||
        command==0x022C||command==0x004E||command==0x0092||
        command==0x0377||command==0x0220||command==0x03F1||command==0x02C2||command==0x0048;
    if(!supported)return 0;
    if(!vm.environment) {error="Retail world command requires a live environment.";return -1;}
    auto& world=*vm.environment;
    if(command==0x0121||command==0x0122||command==0x0151||command==0x0152) {
        StorylandRoadChange change;
        change.vehicle=command==0x0121||command==0x0122;
        change.enabled=command==0x0121||command==0x0151;
        for(float& coordinate:change.bounds) {
            Operand value;
            if(!readOperand(vm,value)||!std::isfinite(coordinate=operandFloat(value))) {
                error="Road switch has an invalid coordinate.";return -1;
            }
        }
        if(world.roadChanges.size()>=4096) {error="Road change history limit reached.";return -1;}
        world.roadChanges.push_back(change);
        if(command==0x0152)world.disabledPedRoads.push_back({change.bounds});
        return 1;
    }
    if(command==0x049D) {
        Operand raised;
        if(!readOperand(vm,raised)) {error="Cash HUD placement requires a flag.";return -1;}
        // SLUS-21590 0x406350, consumed by the cash renderer at 0x31EC3C.
        world.vcsCashHudRaised=uint8_t(raised.bits)!=0;
        return 1;
    }
    if(command==0x0489) {world.empireHudRequested=true;return 1;}
    if(command==0x00BB) {
        Operand handle;
        if(!readOperand(vm,handle)) {error="REMOVE_BLIP has an invalid handle.";return -1;}
        world.markers.erase(std::remove_if(world.markers.begin(),world.markers.end(),
            [&](const auto& item){return item.handle==handle.bits;}),world.markers.end());
        return 1;
    }
    if(command==0x004F||command==0x00A0||command==0x0280||command==0x03F1||command==0x04C5) {
        Operand value;
        if(!readOperand(vm,value)) {error="Retail vehicle/world opcode has a truncated argument.";return -1;}
        if(command==0x004F||command==0x00A0) {
            const auto found=world.missionVehicles.find(value.bits);
            const bool alive=found!=world.missionVehicles.end()&&found->second.alive&&
                (!vm.vcs||!found->second.deleted);
            updateScmCondition(vm,command==0x004F?alive:!alive,inverted);
        } else if(command==0x0280) world.totalRampages=std::max(0,value.bits);
        else if(command==0x03F1) world.emergencyServicesEnabled=value.bits!=0;
        else {
            world.vcsSwimmingBoundary.setEnabled(value.bits!=0);
        }
        return 1;
    }
    if(command==0x0483) {
        Operand model,destination;
        if(!readOperand(vm,model)||!readOperand(vm,destination)||!destination.reference) {
            error="GET_BUILDING_HANDLE_FROM_HASH needs a model hash and destination.";return -1;
        }
        if(!world.buildingSwapHandle) {
            error="Building handle queries require the native building-swap registry.";return -1;
        }
        int32_t handle=-1;
        if(!world.buildingSwapHandle(uint32_t(model.bits),handle,error))return -1;
        if(!writeOperand(vm,destination,handle)) {
            error="GET_BUILDING_HANDLE_FROM_HASH output is invalid.";return -1;
        }
        return 1;
    }
    if(command==0x02C2) {
        std::array<Operand,5> values{};
        for(auto& value:values)if(!readOperand(vm,value)) {
            error="LOCATE_CHAR_ON_FOOT_OBJECT_2D has invalid parameters.";return -1;
        }
        const auto object=std::find_if(world.objects.begin(),world.objects.end(),
            [&](const auto& item){return item.handle==values[1].bits;});
        const float radiusX=operandFloat(values[2]),radiusY=operandFloat(values[3]);
        const bool within=values[0].bits==(vm.vcs?0:1)&&!world.inVehicle&&object!=world.objects.end()&&
            std::isfinite(radiusX)&&std::isfinite(radiusY)&&radiusX>=0&&radiusY>=0&&
            std::fabs(world.x-object->x)<=radiusX&&std::fabs(world.y-object->y)<=radiusY;
        updateScmCondition(vm,within,inverted);
        return 1;
    }
    if(command==0x022C||command==0x004E||command==0x0048||command==0x0092||
        command==0x0377||command==0x0220) {
        const int count=command==0x022C||command==0x0092||command==0x0048?5:
            command==0x004E?4:command==0x0220?3:2;
        std::array<Operand,6> values{};
        for(int i=0;i<count;++i)if(!readOperand(vm,values[size_t(i)])) {
            error="Retail entity/world command has truncated typed arguments.";return -1;
        }
        const auto real=[&](size_t index){return operandFloat(values[index]);};
        if(command==0x022C) {
            StorylandMissionEnvironment::ClearAreaEvent item;
            item.x=real(0);item.y=real(1);item.z=real(2);item.radius=real(3);
            item.particles=values[4].bits!=0;
            if(!std::isfinite(item.x)||!std::isfinite(item.y)||!std::isfinite(item.z)||
                !std::isfinite(item.radius)||item.radius<0||item.radius>50000) {
                error="CLEAR_AREA has invalid coordinates or radius.";return -1;
            }
            if(world.clearAreaEvents.size()>=1024)world.clearAreaEvents.erase(world.clearAreaEvents.begin());
            world.clearAreaEvents.push_back(item);return 1;
        }
        if(command==0x004E) {
            auto found=world.missionVehicles.find(values[0].bits);
            if(found!=world.missionVehicles.end()) {
                const float x=real(1),y=real(2);
                float z=real(3);
                auto& vehicle=found->second;
                if(vm.vcs) {
                    if(!storylandSetVcsVehicleCoordinates(world,found->first,vehicle,x,y,z,false,error))return -1;
                } else {
                    vehicle.x=x;vehicle.y=y;vehicle.z=z;
                    vehicle.movedByScript=true;
                    vehicle.teleportedByScript=true;
                }
            }
            return 1;
        }
        if(command==0x0048) {
            if(!values[4].reference||values[4].variable>=(values[4].global?vm.globals.size():vm.locals.size())||
                (vm.vcs?!storylandScriptVehicleCapacityAvailable(world.missionVehicles,256):
                    world.missionVehicles.size()>=256)||
                world.nextMissionVehicleHandle<1||world.nextMissionVehicleHandle==INT32_MAX||
                world.missionVehicles.count(world.nextMissionVehicleHandle)) {
                error="CREATE_CAR needs a writable output and a free script vehicle slot.";return -1;
            }
            StorylandMissionEnvironment::VehicleScriptState vehicle;
            if(!vm.vcs) {vehicle.boatAnchored=true;vehicle.doorLock=1;}
            vehicle.model=values[0].bits;
            vehicle.x=real(1);vehicle.y=real(2);vehicle.z=real(3);
            if(vehicle.model<0||!std::isfinite(vehicle.x)||!std::isfinite(vehicle.y)||!std::isfinite(vehicle.z)) {
                error="CREATE_CAR has invalid model or coordinates.";return -1;
            }
            if(vm.vcs&&!storylandNormalizeVcsVehicleGroundHeight(world,vehicle.x,vehicle.y,vehicle.z,error))return -1;
            if(!vm.vcs) {
                vehicle.scriptStatus=4;vehicle.autoPilotMission=0;vehicle.cruiseSpeed=7.5f;
                if(vehicle.z<=-100.0f&&(!world.groundHeightAt||
                    !world.groundHeightAt({vehicle.x,vehicle.y,1000},vehicle.z))) {
                    error="CREATE_CAR requires native ground height for the negative-Z sentinel.";return -1;
                }
            }
            if(!vm.vcs&&world.lcsExtendedWorld.vehicleComponentsPending&&
                world.lcsExtendedWorld.selectVehicleComponents&&
                world.lcsExtendedWorld.selectVehicleComponents(vehicle.model,
                    world.lcsExtendedWorld.nextVehicleComponents,vehicle.components)) {
                world.lcsExtendedWorld.explicitVehicleComponents.insert(world.nextMissionVehicleHandle);
                world.lcsExtendedWorld.vehicleComponentsPending=false;
                world.lcsExtendedWorld.nextVehicleComponents={-2,-2};
            }
            const int32_t handle=world.nextMissionVehicleHandle++;
            world.missionVehicles.emplace(handle,vehicle);
            if(world.vehicleNativeType) {
                const int32_t type=world.vehicleNativeType(handle);
                world.missionVehicles.at(handle).bikeStandingOverride=
                    vm.vcs?(type==6||type==8):type==5;
            }
            if(vm.vcs&&world.constructMissionVehicle&&!world.constructMissionVehicle(handle,error)) {
                world.missionVehicles.erase(handle);
                if(error.empty())error="CREATE_CAR could not construct its native physical vehicle.";
                return -1;
            }
            if(!vm.vcs&&world.vehicleIsBoat&&world.vehicleIsBoat(handle))world.missionVehicles.at(handle).cruiseSpeed=20.0f*50.0f/60.0f;
            if(vm.isMissionThread)world.missionCleanupVehicles.insert(handle);
            if(!writeOperand(vm,values[4],handle)) {error="CREATE_CAR output cannot be written.";return -1;}
            return 1;
        }
        if(command==0x0092) {
            if(!values[4].reference||values[4].variable>=(values[4].global?vm.globals.size():vm.locals.size())||world.objects.size()>=4096||
                world.nextObjectHandle<=0||world.nextObjectHandle==INT32_MAX) {error="CREATE_OBJECT needs a valid handle output and free slot.";return -1;}
            StorylandMissionEnvironment::Object object;
            object.handle=world.nextObjectHandle;object.model=values[0].bits;
            object.x=real(1);object.y=real(2);object.z=real(3);
            if(!storylandPrepareScriptObject(world,object,true,error))return -1;
            world.objects.push_back(object);
            ++world.nextObjectHandle;
            if(vm.isMissionThread)world.missionCleanupObjects.insert(object.handle);
            if(!writeOperand(vm,values[4],object.handle)) {error="CREATE_OBJECT cannot write its handle.";return -1;}
            return 1;
        }
        if(command==0x0377) {
            const int32_t state=vm.vcs?storylandVcsPadState(world,values[0].bits):0;
            if(!writeOperand(vm,values[1],state)) {error="GET_PAD_STATE requires an output variable.";return -1;}
            return 1;
        }
        world.pedWaitStateMilliseconds[values[0].bits]=std::clamp(values[2].bits,0,600000);
        world.pedAnimationRequests[values[0].bits]={values[1].bits,
            world.pedWaitStateMilliseconds[values[0].bits],world.nextPedAnimationSerial++};
        return 1;
    }
    if(command==0x04F2) {
        updateScmCondition(vm,vm.isMissionThread,inverted);
        return 1;
    }
    if(command==0x03C1) {
        // VCS PS2 handler 0x3916C0 returns false without inspecting world state.
        updateScmCondition(vm,false,inverted);
        return 1;
    }
    if(command==0x0268) {
        world.helpMessage.clear();world.helpMessagePersistent=false;world.helpMessageMilliseconds=0;
        return 1;
    }
    if(command==0x03F8) {
        Operand source;
        std::string outfit;
        if(vm.pc<vm.scriptBytes().size()&&(
            (vm.scriptBytes()[vm.pc]>=0x0C&&vm.scriptBytes()[vm.pc]<0x20)||
            vm.scriptBytes()[vm.pc]>=0xCC)) {
            if(!readStringOperand(vm,source,outfit)) {
                error="STORE_PLAYER_OUTFIT has an invalid typed outfit name.";return -1;
            }
        } else if(!readScmText(vm,outfit)) {
            error="STORE_PLAYER_OUTFIT has an invalid retail outfit name.";return -1;
        }
        world.storedPlayerOutfit=std::move(outfit);
        return 1;
    }
    if(command==0x0474) {
        updateScmCondition(vm,vm.vcs?world.pager.displayed():world.messageMilliseconds>0,inverted);
        return 1;
    }
    if(command==0x00CA) {
        updateScmCondition(vm,world.fading,inverted);
        return 1;
    }
    if(command==0x0221) {
        world.cameraFrontPlayerRequested=false;
        world.cameraBehindPlayerRequested=true;
        return 1;
    }
    if(command==0x0228) {
        world.loadAllModelsRequested=true;
        return 1;
    }
    if(command==0x00AF||command==0x00B7) {
        Operand zoneOperand;
        std::string zone;
        if(!(vm.vcs?readStringOperand(vm,zoneOperand,zone):readScmText(vm,zone))) {
            error="SET_ZONE_POPULATION_INFO has an invalid zone operand.";return -1;
        }
        if(zone.empty()&&zoneOperand.reference)
            zone=std::string(zoneOperand.global?"global:":"local:")+std::to_string(zoneOperand.variable);
        if(zone.empty()) {error="SET_ZONE_POPULATION_INFO requires a zone name or reference.";return -1;}
        std::array<Operand,12> values{};
        for(auto& value:values)if(!readOperand(vm,value)) {
            error="SET_ZONE_POPULATION_INFO has truncated density and population arguments.";return -1;
        }
        auto& table=command==0x00AF?world.zoneCarInformation:world.zonePedInformation;
        if((vm.vcs&&(values[0].bits<0||values[0].bits>1))||table.size()>=4096) {
            error="SET_ZONE_POPULATION_INFO has an invalid day/night setting or a full zone table.";return -1;
        }
        StorylandMissionEnvironment::ZonePedInformation entry;
        entry.dayNight=vm.vcs?values[0].bits:int32_t(uint8_t(values[0].bits)!=0);
        for(size_t index=0;index<entry.values.size();++index)
            entry.values[index]=values[index+1].bits;
        if(!vm.vcs) {
            auto& density=command==0x00AF?world.zoneCarDensities:world.zonePedDensities;
            density.erase({zone,entry.dayNight});
        }
        table[{std::move(zone),entry.dayNight}]=entry;
        return 1;
    }
    const int count=command==0x00C8||command==0x0139?3:
        command==0x007D||command==0x0200||command==0x047B||command==0x02C0||
        command==0x02DB||command==0x02F8?2:1;
    std::array<Operand,3> values{};
    for(int index=0;index<count;++index)if(!readOperand(vm,values[size_t(index)])) {
        error="VCS world command has truncated typed operands.";return -1;
    }
    if(command==0x02DB) {
        updateScmCondition(vm,values[0].bits==values[1].bits,inverted);
        return 1;
    }
    if(command==0x047B) {
        if(!values[1].reference) {
            error="VCS empire command 7418 requires an output variable.";return -1;
        }
        if(!world.empireBuildingHandle) {
            error="Empire building query requires the live animated-building pool.";return -1;
        }
        world.lastEmpireQuery=values[0].bits;
        const int32_t value=world.empireBuildingHandle(values[0].bits);
        if(!writeOperand(vm,values[1],value)) {
            error="VCS empire command 7418 cannot write its output.";return -1;
        }
        return 1;
    }
    if(command==0x0139) {
        if(!values[2].reference) {error="GENERATE_RANDOM_INT_IN_RANGE requires an output variable.";return -1;}
        const int32_t width=int32_t(uint32_t(values[1].bits)-uint32_t(values[0].bits));
        const int64_t product=int64_t(world.nextNativeRandom())*width;
        int64_t scaled=product/2147483648ll;
        if(product<0&&product%2147483648ll)--scaled; // Native arithmetic right shift31.
        const int32_t result=int32_t(uint32_t(values[0].bits)+uint32_t(scaled));
        if(!writeOperand(vm,values[2],result)) {
            error="Random integer output variable cannot be written.";return -1;
        }
        return 1;
    }
    if(command==0x00C8) {
        for(size_t index=0;index<world.fadingColor.size();++index)
            world.fadingColor[index]=uint8_t(std::clamp(values[index].bits,0,255));
        return 1;
    }
    if(command==0x007D) {
        updateScmCondition(vm,values[0].bits==(vm.vcs?0:1)&&
            world.occupiedVehicleModel==values[1].bits,inverted);
        return 1;
    }
    if(command==0x042E) {
        // Native21B328 compares the island/streaming owner at4CD118.
        updateScmCondition(vm,world.currentWorldLevel==uint32_t(values[0].bits),inverted);
        return 1;
    }
    if(command==0x0200) {
        if(values[0].bits!=(vm.vcs?0:1)) {error="SET_CHAR_VISIBLE requires a player character.";return -1;}
        world.playerVisible=values[1].bits!=0;
        return 1;
    }
    if(command==0x02C0||command==0x02F8) {
        if(values[0].bits!=(vm.vcs?0:1)) {
            error="Character command references a character not yet in the Play Test entity pool.";
            return -1;
        }
        if(command==0x02C0) {
            if(!writeOperand(vm,values[1],world.currentWeapon)) {
                error="GET_CURRENT_CHAR_WEAPON requires a writable output variable.";return -1;
            }
        } else world.playerPositionFrozen=values[1].bits!=0;
        return 1;
    }
    if(command==0x0160||command==0x0161||command==0x0162) {
        int32_t model = values[0].bits;
        if(model<0) {
            if(!world.resolveObjectModel || (model=world.resolveObjectModel(model))<0) {
                error="Model streaming could not resolve SCM object reference "+std::to_string(values[0].bits)+".";
                return -1;
            }
        }
        if(command==0x0160) {
            world.requestedModels.insert(model);
            if (vm.vcs && world.specialModelNames.count(model)) {
                world.specialModelOwners[model] |= vm.isMissionThread ? 2 : 128;
            }
            return 1;
        }
        if(command==0x0161) {
            updateScmCondition(vm,world.residentModels.count(model)!=0,inverted);
            return 1;
        }
        if (vm.vcs && world.specialModelNames.count(model)) {
            const auto owner = world.specialModelOwners.find(model);
            if (owner != world.specialModelOwners.end()) {
                owner->second &= uint8_t(~(vm.isMissionThread ? 2 : 128));
                if (owner->second) {
                    return 1;
                }
                world.specialModelOwners.erase(owner);
            }
        }
        world.requestedModels.erase(model);
        return 1;
    }
    if(command==0x024F) {
        float multiplier=operandFloat(values[0]);
        if(!std::isfinite(multiplier)||multiplier>100||multiplier<0||
            (multiplier!=0&&multiplier<0.00001f))multiplier=float(values[0].bits);
        if(!std::isfinite(multiplier)||multiplier<0||multiplier>100) {
            error="SET_WANTED_MULTIPLIER is outside the valid range.";return -1;
        }
        world.wantedMultiplier=multiplier;
        return 1;
    }
    if(command==0x028D) {
        float nearClip=operandFloat(values[0]);
        if(!std::isfinite(nearClip)||nearClip<0.001f||nearClip>50.0f)
            nearClip=float(values[0].bits);
        if(!std::isfinite(nearClip)||nearClip<0.001f||nearClip>50.0f) {
            error="SET_NEAR_CLIP has an invalid clipping distance.";return -1;
        }
        world.cameraNearClip=nearClip;
        return 1;
    }
    if(command==0x01AE) {world.widescreen=values[0].bits!=0;return 1;}
    if(command==0x0564) {world.allowPauseInWidescreen=values[0].bits!=0;return 1;}
    if(command==0x0352) {
        const int32_t handle=values[0].bits;
        if(handle==(vm.vcs?0:1)) {
            if(!vm.vcs||world.health>0)world.speed=0;
            return 1;
        }
        if(!vm.vcs) {error="STOP_CHAR references a character outside the player pool.";return -1;}
        // 41B0E0 accepts any pool ped, ignores null/dead peds, and
        // sets the current ped/movement states to idle/none. It does not
        // change the persistent objective at CPed+280 or CPed+8A4.
        const auto found=world.missionCharacters.find(handle);
        if(found==world.missionCharacters.end()||!found->second.alive||
            found->second.health<=0)return 1;
        auto& character=found->second;
        character.movementSpeed=0;
        character.movementState=0;
        character.animationId=-1;
        character.animationLoop=false;
        world.pedAnimationRequests.erase(handle);
        world.scriptedCharacterAnimations.erase(handle);
        return 1;
    }
    if(command==0x02CB) {
        const float value=operandFloat(values[0]);
        world.threatReactionRangeMultiplier=std::isfinite(value)&&value>=0&&value<=100?
            value:std::clamp(float(values[0].bits),0.0f,100.0f);
        return 1;
    }
    if(command==0x0443) {
        if(values[0].bits>=0)world.requestedAnimationGroups.insert(values[0].bits);
        return 1;
    }
    if(command==0x04DA) {world.totalBalloons=std::max(0,values[0].bits);return 1;}
    if(command==0x04EF) {
        // Retail VCS Sanny identifies this as unknown_command_8850.
        // Retain the exact operand; no verified effect is currently known.
        world.unknownCommand8850=values[0].bits;
        return 1;
    }
    return 0;
}
int executeRetailInitialization(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    const uint16_t garage=vm.vcs?0x0145:0x021E;
    const uint16_t object=vm.vcs?0x01AA:0x02A0;
    const uint16_t given=vm.vcs?0x01EA:0x031C;
    const uint16_t fade=vm.vcs?0x00C9:0x016F;
    const uint16_t text=vm.vcs?0x033A:0x0551;
    const uint16_t splash=vm.vcs?0x02A9:0x0452;
    const uint16_t total=vm.vcs?0x0296:0x0431;
    const uint16_t randomCar=vm.vcs?0x03A3:0x05D7;
    const uint16_t bridge=vm.vcs?0x03C6:0xFFFF;
    const uint16_t garageCars=vm.vcs?0x0353:0x057F;
    const uint16_t objectHeading=vm.vcs?0x00D3:0x017C;
    const uint16_t objectOffset=vm.vcs?0x0279:0xFFFF;
    const uint16_t progress=vm.vcs?0x01E0:0x0312;
    const uint16_t garageSwing=vm.vcs?0x0247:0x03C0;
    const uint16_t garageLock=vm.vcs?0x03FC:0xFFFF;
    const uint16_t objectPersistent=vm.vcs?0x0117:0x01CC;
    const uint16_t clockEvent=vm.vcs?0x0403:0x0670;
    const uint16_t deathArrest=vm.vcs?0x009C:0x0116;
    const uint16_t arrested=vm.vcs?0x00A5:0x0135;
    const uint16_t missionFlag=vm.vcs?0x00D6:0x0185;
    const uint16_t undress=vm.vcs?0x020F:0x0357;
    const bool dress=!vm.vcs&&command==0x0358;
    const uint16_t rubbish=vm.vcs?0x023F:0x03B2;
    const uint16_t worldProcessing=vm.vcs?0x0245:0x03BC;
    const uint16_t streaming=vm.vcs?0x0241:0x03B4;
    const uint16_t ignore=vm.vcs?0x024A:0x03C4;
    const uint16_t clearThreats=vm.vcs?0x04C3:0xFFFF;
    const uint16_t cameraSpecial=vm.vcs?0x03B7:0xFFFF;
    const uint16_t useText=vm.vcs?0x0270:0xFFFF;
    const uint16_t policeRestart=vm.vcs?0x00CC:0x0172;
    const uint16_t hospitalRestart=vm.vcs?0x00CB:0x0171;
    const bool overrideRestart=!vm.vcs&&command==0x0173;
    const bool cancelRestart=!vm.vcs&&command==0x01FB;
    const uint16_t taxiRestart=vm.vcs?0x0363:0x0592;
    const uint16_t addThreat=vm.vcs?0x0271:0xFFFF;
    const uint16_t policeIgnore=vm.vcs?0x012E:0x01FC;
    if(command!=garage&&command!=object&&command!=given&&command!=fade&&
        command!=text&&command!=splash&&command!=total&&command!=randomCar&&command!=bridge&&
        command!=garageCars&&command!=objectHeading&&command!=objectOffset&&command!=progress&&
        command!=garageSwing&&command!=garageLock&&command!=objectPersistent&&command!=clockEvent&&
        command!=deathArrest&&command!=arrested&&command!=missionFlag&&command!=undress&&!dress&&
        command!=rubbish&&command!=worldProcessing&&command!=streaming&&
        command!=ignore&&command!=clearThreats&&command!=cameraSpecial&&command!=useText&&
        command!=policeRestart&&command!=hospitalRestart&&command!=taxiRestart&&
        command!=addThreat&&command!=policeIgnore&&!overrideRestart&&!cancelRestart)return 0;
    if(!vm.environment) {error="Retail initialization requires a Play Test environment.";return -1;}
    auto& environment=*vm.environment;
    if(cancelRestart) {environment.restartOverrideEnabled=false;return 1;}
    if(command==text) {
        if(!(vm.vcs?readVcsMissionText(vm,environment.missionText):readScmText(vm,environment.missionText))) {error="LOAD_MISSION_TEXT has an invalid bank name.";return -1;}
        return 1;
    }
    if(command==undress||dress) {
        Operand handle,skinOperand;std::string skin;
        if(!readOperand(vm,handle)||(!dress&&
            !(vm.vcs?readStringOperand(vm,skinOperand,skin):readScmText(vm,skin)))) {
            error="Character clothing requires a character and the native skin operands.";return -1;
        }
        const bool player=handle.bits==(vm.vcs?0:1);
        const auto character=environment.missionCharacters.find(handle.bits);
        if(!player&&character==environment.missionCharacters.end()) {
            error="Character clothing references an unknown native pedestrian.";return -1;
        }
        if(vm.vcs) {
            for(char& value:skin)if(value>='A'&&value<='Z')value=char(value-'A'+'a');
            if(!environment.replaceCharacterSkin||
                !environment.replaceCharacterSkin(handle.bits,skin,true,error)) {
                if(error.empty())error="Character clothing requires a live native model replacement binding.";
                return -1;
            }
            if(player) {
                environment.playerOutfit=skin;
                environment.playerWaitState=0;
                environment.playerWaitDeadline=0;
                environment.playerWaitVersionB=false;
            }
            else {
                character->second.objective=0;
                character->second.objectiveTarget=-1;
                character->second.walkingToTarget=false;
                character->second.animationId=-1;
                character->second.waitState=0;
                character->second.waitDeadline=0;
                character->second.waitVersionB=false;
            }
            environment.characterSkins[handle.bits]=skin;
            environment.pedWaitStateMilliseconds.erase(handle.bits);
            environment.pedAnimationRequests.erase(handle.bits);
            environment.scriptedCharacterAnimations.erase(handle.bits);
            return 1;
        }
        const int32_t model=player?0:character->second.model;
        if(dress) {
            const auto previous=environment.characterSkins.find(handle.bits);
            if(previous!=environment.characterSkins.end())skin=previous->second;
            else {
                const auto alias=environment.specialModelNames.find(model);
                if(alias!=environment.specialModelNames.end())skin=alias->second;
            }
        } else for(char& value:skin)if(value>='A'&&value<='Z')value=char(value-'A'+'a');
        if(!environment.replaceCharacterSkin||
            !environment.replaceCharacterSkin(handle.bits,skin,dress,error)) {
            if(error.empty())error="Character clothing requires a live native model replacement binding.";
            return -1;
        }
        if(dress) {
            environment.undressedCharacters.erase(handle.bits);
            if(player&&!skin.empty())environment.playerOutfit=skin;
            environment.pedAnimationRequests.erase(handle.bits);
            environment.scriptedCharacterAnimations.erase(handle.bits);
        } else {
            environment.undressedCharacters.insert(handle.bits);
            environment.characterSkins[handle.bits]=skin;
            environment.specialModelNames[model]=skin;
            environment.residentModels.erase(model);
            environment.changedSpecialModels.insert(model);
            environment.requestedModels.insert(model);
        }
        return 1;
    }
    if(command==splash) {
        if(!(vm.vcs?readVcsMissionText(vm,environment.splashScreen):readScmText(vm,environment.splashScreen))) {error="LOAD_SPLASH_SCREEN has an invalid texture name.";return -1;}
        return 1;
    }
    if(command==given) {++environment.missionsGiven;return 1;}
    const int count=command==garage?10:command==objectOffset?7:command==object||command==randomCar?5:
        command==policeRestart||command==hospitalRestart||command==taxiRestart||overrideRestart?4:
        command==clockEvent?3:command==fade||command==garageCars||command==objectHeading||
        command==ignore||command==addThreat||command==policeIgnore||command==garageLock?2:1;
    const auto definition=vm.opcodeDefinitions.find(command);
    if(definition!=vm.opcodeDefinitions.end()&&definition->second.first!=count) {
        error="Incorrect retail initialization opcode signature.";return -1;
    }
    std::array<Operand,10> values{};
    for(int index=0;index<count;++index)if(!readOperand(vm,values[size_t(index)])) {
        error="Invalid typed retail initialization operand.";return -1;
    }
    const auto real=[&](size_t index){return operandFloat(values[index]);};
    if(command==garage) {
        StorylandMissionEnvironment::Garage item;
        for(size_t i=0;i<8;++i) {
            item.bounds[i]=real(i);
            if(!std::isfinite(item.bounds[i])) {error="Garage position is invalid.";return -1;}
        }
        if(!values[9].reference || environment.garages.size()>=2048) {
            error="Garage needs an output variable and a free pool slot.";return -1;
        }
        for(auto& restored:environment.garages) {
            if(!environment.importedGarageHandles.count(restored.handle))continue;
            bool same=true;for(size_t i=0;i<8;++i)if(std::fabs(restored.bounds[i]-item.bounds[i])>.05f)same=false;
            if(same) {
                environment.importedGarageHandles.erase(restored.handle);
                writeOperand(vm,values[9],restored.handle);return 1;
            }
        }
        item.type=values[8].bits;item.handle=environment.nextGarageHandle++;
        item.maximumCars=4;
        environment.garages.push_back(item);
        writeOperand(vm,values[9],item.handle);return 1;
    }
    if(command==object) {
        StorylandMissionEnvironment::Object item;
        item.model=values[0].bits;item.x=real(1);item.y=real(2);item.z=real(3);
        if(!std::isfinite(item.x)||!std::isfinite(item.y)||!std::isfinite(item.z)||
            !values[4].reference||values[4].variable>=(values[4].global?vm.globals.size():vm.locals.size())||
            environment.objects.size()>=4096||environment.nextObjectHandle<=0||environment.nextObjectHandle==INT32_MAX) {
            error="CREATE_OBJECT requires finite coordinates and a free object handle.";return -1;
        }
        item.handle=environment.nextObjectHandle;
        if(vm.vcs&&!storylandPrepareScriptObject(environment,item,false,error))return -1;
        environment.objects.push_back(item);
        ++environment.nextObjectHandle;
        if(vm.isMissionThread)environment.missionCleanupObjects.insert(item.handle);
        writeOperand(vm,values[4],item.handle);return 1;
    }
    if(command==fade) {
        environment.fadeDuration=float(std::clamp(values[0].bits,0,120000));
        environment.fadeDirection=values[1].bits;
        environment.fadeTotal=environment.fadeDuration;
        environment.fadeStartAlpha=environment.fadeAlpha;
        environment.fadeTargetAlpha=environment.fadeDirection?0.0f:1.0f;
        environment.fading=environment.fadeDuration>0;
        if(!environment.fading)environment.fadeAlpha=environment.fadeTargetAlpha;
        return 1;
    }
    if(command==total) {
        if(vm.vcs)environment.totalMissions=std::max(0,values[0].bits);
        else {
            // The German retail mission set removes two missions. Preserve
            // the native signed assignment and defined 32-bit subtraction.
            environment.totalMissions=int32_t(uint32_t(values[0].bits)-(environment.germanGame?2u:0u));
            environment.statistics["TotalNumberMissions"]=environment.totalMissions;
        }
        return 1;
    }
    if(command==progress) {environment.progressTotal=std::max(0,values[0].bits);return 1;}
    if(command==deathArrest) {environment.deathArrestState=values[0].bits;vm.deathArrestEnabled=values[0].bits==1;return 1;}
    if(command==missionFlag) {
        environment.declaredMissionFlag=values[0].reference&&values[0].global?
            int32_t(values[0].variable):-1;
        return 1;
    }
    if(command==rubbish) {environment.rubbishEnabled=values[0].bits!=0;return 1;}
    if(command==worldProcessing) {environment.worldProcessingEnabled=values[0].bits!=0;return 1;}
    if(command==streaming) {environment.streamingEnabled=values[0].bits!=0;return 1;}
    if(command==ignore) {environment.everyoneIgnoresPlayer=values[1].bits!=0;return 1;}
    if(command==clearThreats) {environment.clearedPedThreats.insert(values[0].bits);environment.pedTypeThreats[values[0].bits]=0;return 1;}
    if(command==cameraSpecial) {
        if(!environment.activeCameraMode) {error="Ped camera zoom requires the active native camera mode.";return -1;}
        if(environment.activeCameraMode()==4)environment.cameraSpecialMode=values[0].bits;
        return 1;
    }
    if(command==useText) {environment.textCommandsEnabled=values[0].bits!=0;return 1;}
    if(command==policeIgnore) {environment.policeIgnorePlayer=values[1].bits!=0;return 1;}
    if(command==addThreat) {environment.pedTypeThreats[values[0].bits]|=uint32_t(values[1].bits);return 1;}
    if(command==policeRestart||command==hospitalRestart||command==taxiRestart||overrideRestart) {
        StorylandMissionEnvironment::PoliceRestart point;
        point.x=real(0);point.y=real(1);point.z=real(2);point.heading=real(3);
        if(!std::isfinite(point.x)||!std::isfinite(point.y)||
            !std::isfinite(point.z)||!std::isfinite(point.heading)) {
            error="Invalid taxi/hospital/police restart coordinates.";return -1;
        }
        if(command!=taxiRestart&&!storylandNormalizeRestartHeight(environment.groundHeightAt,
            point.x,point.y,point.z,vm.vcs,error))return -1;
        if(overrideRestart) {
            environment.restartOverride=point;environment.restartOverrideEnabled=true;return 1;
        }
        auto& restarts=command==policeRestart?environment.policeRestarts:
            command==hospitalRestart?environment.hospitalRestarts:environment.taxiRestarts;
        const size_t capacity=command!=taxiRestart?8:vm.vcs?16:128;
        if(restarts.size()>=capacity) {error="Taxi/hospital/police restart pool is full.";return -1;}
        if(!vm.vcs&&command==taxiRestart)environment.taxiRestarts.clear();
        restarts.push_back(point);return 1;
    }
    if(command==arrested) {updateScmCondition(vm,environment.playerArrested,inverted);return 1;}
    if(command==clockEvent) {
        storylandSetStoriesClockEvent(environment,values[0].bits,values[1].bits,values[2].bits);
        return 1;
    }
    if(command==bridge) {environment.bridgeGeometry=values[0].bits;updateScmCondition(vm,true,inverted);return 1;}
    if(command==garageCars) {
        auto found=std::find_if(environment.garages.begin(),environment.garages.end(),
            [&](const auto& item){return item.handle==values[0].bits;});
        if(found==environment.garages.end()) {error="SET_GARAGE_MAX_CARS has no matching garage.";return -1;}
        found->maximumCars=std::clamp(values[1].bits,0,100);return 1;
    }
    if(command==garageSwing) {
        auto found=std::find_if(environment.garages.begin(),environment.garages.end(),
            [&](const auto& item){return item.handle==values[0].bits;});
        if(found==environment.garages.end()) {error="SET_GARAGE_DOOR refers to an unknown garage.";return -1;}
        if(!found->swingDoor&&!vm.vcs) {
            const auto motion=environment.lcsGarageDoors.find(found->handle);
            if(motion!=environment.lcsGarageDoors.end()) {
                motion->second.height=std::max(.01f,motion->second.height*.5f-.1f);
                motion->second.position=std::min(motion->second.position,motion->second.height);
                found->swingDoor=true;
                return storylandApplyLcsGarageDoors(environment,*found,motion->second,error)?1:-1;
            }
        }
        found->swingDoor=true;return 1;
    }
    if(command==garageLock) {
        auto found=std::find_if(environment.garages.begin(),environment.garages.end(),
            [&](const auto& item){return item.handle==values[0].bits;});
        if(found==environment.garages.end()) {
            error="LOCK_GARAGE refers to an unknown garage; retail startup context may be missing.";
            return -1;
        }
        found->locked=values[1].bits!=0;
        return 1;
    }
    if(command==objectPersistent) {
        auto found=std::find_if(environment.objects.begin(),environment.objects.end(),
            [&](const auto& item){return item.handle==values[0].bits;});
        if(vm.vcs) {
            environment.missionCleanupObjects.erase(values[0].bits);
            if(found!=environment.objects.end())found->persistent=true;
            return 1;
        }
        if(found==environment.objects.end()) {error="DONT_REMOVE_OBJECT refers to an unknown object.";return -1;}
        found->persistent=true;return 1;
    }
    if(command==objectOffset) {
        const auto found=std::find_if(environment.objects.begin(),environment.objects.end(),
            [&](const auto& item){return item.handle==values[0].bits;});
        if(found==environment.objects.end()||!std::isfinite(found->heading)||
            !std::isfinite(real(1))||!std::isfinite(real(2))||!std::isfinite(real(3))||
            !values[4].reference||!values[5].reference||!values[6].reference) {
            error="GET_OFFSET_FROM_OBJECT_IN_WORLD_COORDS requires an object, finite offset and three outputs.";
            return -1;
        }
        // Retail VCS 0x30FC80 transforms the offset by the object's basis,
        // then adds its position, including all three authored rotation axes.
        const auto matrix=storylandScriptObjectMatrix(found->x,found->y,found->z,
            found->rotationX,found->rotationY,found->heading);
        const float coordinates[3]={matrix[0]*real(1)+matrix[4]*real(2)+matrix[8]*real(3)+matrix[12],
            matrix[1]*real(1)+matrix[5]*real(2)+matrix[9]*real(3)+matrix[13],
            matrix[2]*real(1)+matrix[6]*real(2)+matrix[10]*real(3)+matrix[14]};
        for(float coordinate:coordinates)if(!std::isfinite(coordinate)) {
            error="GET_OFFSET_FROM_OBJECT_IN_WORLD_COORDS produced an invalid position.";return -1;
        }
        for(size_t axis=0;axis<3;++axis) {
            int32_t bits=0;std::memcpy(&bits,&coordinates[axis],sizeof(bits));
            writeOperand(vm,values[4+axis],bits);
        }
        return 1;
    }
    if(command==objectHeading) {
        auto found=std::find_if(environment.objects.begin(),environment.objects.end(),
            [&](const auto& item){return item.handle==values[0].bits;});
        if(found==environment.objects.end()||!std::isfinite(real(1))) {
            error="SET_OBJECT_HEADING has no matching object or a valid angle.";return -1;
        }
        found->heading=real(1);return 1;
    }
    if(command==randomCar) {
        if(!values[4].reference) {error="GET_RANDOM_CAR requires an output variable.";return -1;}
        const float x1=real(0),y1=real(1),x2=real(2),y2=real(3);
        if(!std::isfinite(x1)||!std::isfinite(y1)||!std::isfinite(x2)||!std::isfinite(y2)) {
            error="GET_RANDOM_CAR has invalid area bounds.";return -1;
        }
        if(!vm.vcs) {
            int32_t handle=-1;
            for(const auto& car:environment.carGenerators)
                if(car.generations>0 && car.x>=std::min(x1,x2)&&car.x<=std::max(x1,x2)&&
                    car.y>=std::min(y1,y2)&&car.y<=std::max(y1,y2)) {handle=car.handle;break;}
            writeOperand(vm,values[4],handle);
            updateScmCondition(vm,handle>=0,inverted);return 1;
        }
        if(!environment.findScriptVehiclesInArea) {
            error="GET_RANDOM_CAR requires the live vehicle pool.";return -1;
        }
        const std::array<float,4> bounds={std::min(x1,x2),std::min(y1,y2),
            std::max(x1,x2),std::max(y1,y2)};
        const auto vehicles=environment.findScriptVehiclesInArea(bounds,-1,0);
        const int32_t handle=vehicles.empty()?-1:vehicles.front();
        if(!writeOperand(vm,values[4],handle)) {
            error="GET_RANDOM_CAR requires a writable output.";return -1;
        }
        return 1;
    }
    return 0;
}
float operandFloat(const Operand& operand) {
    float result=0;
    std::memcpy(&result,&operand.bits,sizeof(result));
    return result;
}enum class MissionAction {
    GroundHeight,SetMobile,DisableLevelSplash,ActivateSave,SaveFinished,CoordinateInLevel,CreatePlayer,GetPosition,SetPosition,GetCharPosition,SetCharPosition,GetPlayerChar,
    AddMoney,GetMoney,MoneyGreater,SetWanted,RaiseWanted,WantedGreater,ClearWanted,GetWanted,
    SetMaxWanted,GetTime,SetTime,GetHeading,SetHeading,GetCharHeading,SetCharHeading,
    SetHealth,GetHealth,GetPlayerHealth,HealthGreater,AddArmour,SetControl,ControlOn,
    CharControl,CharOnFoot,CharAlive,PlayerPlaying,CharInCar,Print,ClearPrints,CreatePickup,CreatePickupWithAmmo,CreateMoneyPickup,PickupCollected,RemovePickup,
    CreateCarGenerator,SwitchCarGenerator,GetGameTimer,ClearPlayerWeapons,PlayerInCar
};
struct MissionCommand { uint16_t lcs,vcs; MissionAction action; unsigned arguments; };
const MissionCommand missionCommands[]={
    {0x02D3,0x01BB,MissionAction::GroundHeight,4},
    {0x0530,0x032F,MissionAction::SetMobile,2},
    {0,0x0530,MissionAction::DisableLevelSplash,1},
    {0x02E6,0x01C8,MissionAction::CreateMoneyPickup,5},
    {0x03EE,0x026A,MissionAction::CharControl,1},
    {0x0450,0x02A8,MissionAction::CharOnFoot,1},
    {0x03DD,0x0260,MissionAction::ActivateSave,0},
    {0x03DE,0x0261,MissionAction::SaveFinished,0},
    {0,0x04E7,MissionAction::CoordinateInLevel,3},
    {0x01C2,0x010E,MissionAction::GetGameTimer,1},
    {0x03BD,0,MissionAction::ClearPlayerWeapons,1},
    {0x00E5,0,MissionAction::PlayerInCar,1},
    {0x0150,0x00A9,MissionAction::CreateCarGenerator,13},
    {0x0151,0x00AA,MissionAction::SwitchCarGenerator,2},
    {0x0053,0x0028,MissionAction::CreatePlayer,5},
    {0x0054,0,MissionAction::GetPosition,4},{0x0055,0,MissionAction::SetPosition,4},
    {0x00A0,0x0043,MissionAction::GetCharPosition,4},{0x00A1,0x0044,MissionAction::SetCharPosition,4},
    {0x01FA,0,MissionAction::GetPlayerChar,2},
    {0x010E,0x0094,MissionAction::AddMoney,2},{0x0110,0x0096,MissionAction::GetMoney,2},
    {0x010F,0x0095,MissionAction::MoneyGreater,2},
    {0x0112,0x0098,MissionAction::SetWanted,2},{0x0113,0x0099,MissionAction::RaiseWanted,2},
    {0x0114,0x009A,MissionAction::WantedGreater,2},{0x0115,0x009B,MissionAction::ClearWanted,1},
    {0x01C5,0x0110,MissionAction::GetWanted,2},{0x01F5,0x0128,MissionAction::SetMaxWanted,1},
    {0x00BF,0x005A,MissionAction::GetTime,2},{0x00C0,0x005B,MissionAction::SetTime,2},
    {0x0175,0,MissionAction::GetHeading,2},{0x0176,0,MissionAction::SetHeading,2},
    {0x0177,0x00CE,MissionAction::GetCharHeading,2},{0x0178,0x00CF,MissionAction::SetCharHeading,2},
    {0x0228,0x014B,MissionAction::SetHealth,2},{0x022B,0x014D,MissionAction::GetHealth,2},
    {0x022A,0,MissionAction::GetPlayerHealth,2},{0x0188,0,MissionAction::HealthGreater,2},
    {0x0364,0x0213,MissionAction::AddArmour,2},
    {0x01B9,0x0107,MissionAction::SetControl,2},{0x03F3,0x026E,MissionAction::ControlOn,1},
    {0x00A2,0x0045,MissionAction::CharAlive,1},{0x025B,0,MissionAction::PlayerPlaying,1},
    {0x00E4,0x007E,MissionAction::CharInCar,1},
    {0x0218,0x013F,MissionAction::CreatePickup,6},
    {0x0330,0x01F9,MissionAction::CreatePickupWithAmmo,7},
    {0x0219,0x0140,MissionAction::PickupCollected,1},
    {0x021A,0x0141,MissionAction::RemovePickup,1},
    {0x00BA,0x0055,MissionAction::Print,2},{0x00BB,0x0056,MissionAction::Print,2},
    {0x00BC,0x0057,MissionAction::Print,2},{0x00BD,0x0058,MissionAction::Print,2},
    {0x00BE,0x0059,MissionAction::ClearPrints,0}
};
int executeMarkerCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    const auto definition=vm.opcodeDefinitions.find(command);
    if(definition==vm.opcodeDefinitions.end())return 0;
    std::istringstream words(definition->second.second);
    std::string name;
    words>>name;
    const bool add=name=="add_sphere"||name=="add_sphere_0172";
    const bool remove=name=="remove_sphere"||name=="remove_sphere_0172";
    const bool checkpoint=name=="draw_checkpoint";
    const bool checkpointArrow=name=="create_checkpoint_arrow_deprecated";
    const bool drawSphere=name=="draw_sphere";
    const bool area=name=="set_area_visible";
    if(!add&&!remove&&!checkpoint&&!checkpointArrow&&!drawSphere&&!area)return 0;
    if(!vm.environment) {error="Marker command requires a live environment.";return -1;}
    const int expected=add?5:remove||area?1:checkpoint?7:checkpointArrow?10:4;
    if(definition->second.first!=expected) {error="Marker command signature mismatch.";return -1;}
    std::array<Operand,10> values{};
    for(int index=0;index<expected;++index)if(!readOperand(vm,values[size_t(index)])) {
        error="Invalid marker command operand.";return -1;
    }
    auto& environment=*vm.environment;
    if(area) {
        if(values[0].bits<0||values[0].bits>255) {error="Area ID exceeds its supported range.";return -1;}
        environment.area=values[0].bits;
        return 1;
    }
    if(remove) {
        environment.markers.erase(std::remove_if(environment.markers.begin(),environment.markers.end(),
            [&](const auto& marker){return marker.persistent&&marker.handle==values[0].bits;}),environment.markers.end());
        return 1;
    }
    const auto position=std::array<float,3>{operandFloat(values[0]),operandFloat(values[1]),operandFloat(values[2])};
    const float radius=checkpointArrow?3.2f:operandFloat(values[3]);
    if(!std::all_of(position.begin(),position.end(),[](float value){return std::isfinite(value);})||
        !std::isfinite(radius)||radius<=0||radius>1000) {
        error="Marker has invalid coordinates or radius.";return -1;
    }
    if(checkpoint||checkpointArrow) {
        StorylandMissionEnvironment::ScriptCheckpoint item;
        item.position=position;
        item.area=environment.area;
        item.radius=radius;
        item.type=checkpointArrow?3:vm.vcs?6:5;
        item.nativeDrawCheckpoint=checkpoint;
        if(item.nativeDrawCheckpoint) {
            // VCS 0x405D6C / LCS 0x2F3118: one cylinder, radius*0.7, full alpha.
            item.radius*=0.7f;
            const bool groundSentinel=vm.vcs?
                item.position[2]==-100.0f||item.position[2]<=-250.0f:item.position[2]<=-100.0f;
            if(groundSentinel) {
                if(!environment.groundHeightAt) {
                    error="Checkpoint ground sentinel requires the native collision world.";
                    return -1;
                }
                float ground=20;
                if(!environment.groundHeightAt({item.position[0],item.position[1],1000},ground))ground=20;
                if(!std::isfinite(ground)) {error="Checkpoint ground query returned a non-finite height.";return -1;}
                item.position[2]=ground;
            }
        }
        if(checkpointArrow) {
            item.direction={operandFloat(values[3]),operandFloat(values[4]),operandFloat(values[5])};
            if(!std::all_of(item.direction.begin(),item.direction.end(),[](float value){return std::isfinite(value);} )||
                !std::isfinite(operandFloat(values[9]))) {
                error="Directional checkpoint contains invalid coordinates.";return -1;
            }
            item.position[2]+=operandFloat(values[9])+7.0f;
            item.color={uint8_t(std::clamp(values[6].bits,0,255)),
                uint8_t(std::clamp(values[7].bits,0,255)),
                uint8_t(std::clamp(values[8].bits,0,255)),200};
        } else {
            item.direction={position[0],position[1]+1.0f,position[2]};
            item.color={uint8_t(values[4].bits),uint8_t(values[5].bits),uint8_t(values[6].bits),255};
        }
        const auto key=std::make_pair(reinterpret_cast<uintptr_t>(&vm),
            int32_t(std::min(vm.pc,size_t(INT32_MAX))));
        if(!environment.frameScriptCheckpoints.count(key)&&environment.frameScriptCheckpoints.size()>=32) {
            error="Checkpoint marker pool is full.";return -1;
        }
        environment.frameScriptCheckpoints[key]=item;
        return 1;
    }
    StorylandMissionEnvironment::Marker marker;
    marker.x=position[0];marker.y=position[1];marker.z=position[2];
    marker.radius=radius;marker.area=environment.area;
    marker.createdAt=environment.gameMilliseconds;
    if(add||drawSphere) {
        marker.type=vm.vcs?(name=="add_sphere_0172"?7u:6u):5u;
        marker.red=vm.vcs?(marker.type==7u?30u:237u):255u;
        marker.green=vm.vcs?(marker.type==7u?255u:130u):255u;
        marker.blue=vm.vcs?(marker.type==7u?255u:180u):128u;
        const bool groundSentinel=vm.vcs?(marker.z==-100.0f||marker.z<=-250.0f):marker.z<=-100.0f;
        if(groundSentinel) {
            if(!environment.groundHeightAt) {
                error="Sphere ground sentinel requires the native collision world.";return -1;
            }
            float height=20;
            if(!environment.groundHeightAt({marker.x,marker.y,1000},height))height=20;
            if(!std::isfinite(height)) {error="Sphere ground query returned a non-finite height.";return -1;}
            marker.z=height;
        }
    }
    if(vm.vcs&&drawSphere) {
        StorylandMissionEnvironment::ScriptCheckpoint item;
        item.position={marker.x,marker.y,marker.z};
        item.direction={marker.x,marker.y+1.0f,marker.z};
        item.radius=marker.radius;
        item.type=6u;
        item.area=marker.area;
        item.color={marker.red,marker.green,marker.blue,228u};
        const auto key=std::make_pair(reinterpret_cast<uintptr_t>(&vm),int32_t(std::min(vm.pc,size_t(INT32_MAX))));
        if(environment.frameScriptCheckpoints.count(key)||environment.frameScriptCheckpoints.size()<32)
            environment.frameScriptCheckpoints[key]=item;
        return 1;
    }
    if(add&&!values[4].reference) {error="Marker handle output requires a variable.";return -1;}
    if(add) {
        size_t slot=0;
        for(;slot<environment.markerGenerations.size();++slot) {
            const auto occupied=std::find_if(environment.markers.begin(),environment.markers.end(),
                [&](const auto& item) {
                    return item.persistent&&(uint32_t(item.handle)&0xffffu)==slot;
                });
            if(occupied==environment.markers.end())break;
        }
        if(slot==environment.markerGenerations.size()) {
            error="The native persistent marker pool has no free slot.";return -1;
        }
        auto& generation=environment.markerGenerations[slot];
        generation=generation>=0xfffeu?1u:uint16_t(generation+1u);
        marker.handle=int32_t((uint32_t(generation)<<16)|uint32_t(slot));
    } else {
        if(environment.markers.size()>=4096||environment.nextMarkerHandle==INT32_MAX) {
            error="Mission marker pool is full.";return -1;
        }
        marker.handle=environment.nextMarkerHandle++;
    }
    marker.persistent=add;marker.milliseconds=200;
    environment.markers.push_back(marker);
    if(add&&!writeOperand(vm,values[4],marker.handle)) {
        environment.markers.pop_back();
        error="Marker handle output cannot be stored.";return -1;
    }
    return 1;
}

int executeDefinitionCommand(StorylandMissionVm& vm, uint16_t command, bool inverted, std::string& error) {
    const auto definition = vm.opcodeDefinitions.find(command);
    if (definition == vm.opcodeDefinitions.end()) return 0;
    std::istringstream words(definition->second.second);
    std::string name;
    words >> name;
    const bool carQuery=vm.vcs&&(name.find("locate_car_")==0||name.find("locate_stopped_car_")==0||
        name.find("is_car_in_area_")==0||name.find("is_car_stopped_in_area_")==0);
    const bool locate = name.find("locate_char_") == 0 || name.find("locate_stopped_char_") == 0 ||
        (carQuery&&name.find("locate_")==0);
    const bool area = name.find("is_char_in_area_") == 0 || name.find("is_char_stopped_in_area_") == 0 ||
        (carQuery&&name.find("is_car_")==0);
    const bool distance = name.find("get_distance_between_coords_") == 0;
    const bool math = name == "sqrt" || name == "sin" || name == "cos" ||
        name == "abs_any_int" || name == "abs_any_float";
    const bool dead = name == "is_char_dead";
    // Character-to-character queries require an entity pool, not the player's coordinates twice.
    if (locate && name.find("_char_") != name.rfind("_char_")) return 0;
    if (!locate && !area && !distance && !math && !dead) return 0;
    const int count = definition->second.first;
    if (count < 1 || count > 8) { error = "Invalid Sanny command arity."; return -1; }
    std::array<Operand,8> values{};
    for (int index = 0; index < count; ++index) {
        if (!readOperand(vm,values[size_t(index)])) {
            error = "Invalid SCM operand for " + name; return -1;
        }
    }
    const auto real = [&](size_t index) { return operandFloat(values[index]); };
    const auto storeReal = [&](size_t index, float value) {
        if (!std::isfinite(value)) {error = "Non-finite result for " + name; return false;}
        int32_t bits;
        std::memcpy(&bits,&value,sizeof(bits));
        if (writeOperand(vm,values[index],bits)) return true;
        error = "SCM output requires a variable for " + name; return false;
    };
    if (math) {
        if (name == "abs_any_int") {
            if (values[0].bits == INT32_MIN) {error = "Integer absolute value overflows."; return -1;}
            if (!writeOperand(vm,values[0],std::abs(values[0].bits))) {
                error = "ABS requires a variable."; return -1;
            }
            return 1;
        }
        const float input = real(0);
        const float result = name == "sqrt" ? std::sqrt(input) : name == "sin" ?
            std::sin(input * 0.0174532925199433f) : name == "cos" ?
            std::cos(input * 0.0174532925199433f) : std::fabs(input);
        return storeReal(name == "abs_any_float" ? 0 : 1,result) ? 1 : -1;
    }
    if (distance) {
        const bool three = name.find("_3d") != std::string::npos;
        const size_t dimensions = three ? 3 : 2;
        if (count != int(dimensions*2+1)) {error = "Distance command arity mismatch."; return -1;}
        double squared = 0;
        for (size_t axis = 0; axis < dimensions; ++axis) {
            const double difference = double(real(axis))-real(axis+dimensions);
            squared += difference*difference;
        }
        return storeReal(dimensions*2,float(std::sqrt(squared))) ? 1 : -1;
    }
    if (!vm.environment || (!carQuery&&values[0].bits != (vm.vcs ? 0 : 1))) {
        error = "Character query requires a known live player character."; return -1;
    }
    const auto& environment = *vm.environment;
    bool matches = environment.health <= 0;
    std::array<float,3> queryPosition{environment.x,environment.y,environment.z};
    float querySpeed=environment.speed;
    if(carQuery) {
        const auto vehicle=environment.missionVehicles.find(values[0].bits);
        if(!storylandVcsVehiclePosition(environment,values[0].bits,queryPosition,error))return -1;
        querySpeed=vehicle->second.speed;
    }
    if (!dead) {
        const bool three = name.find("_3d") != std::string::npos;
        const size_t dimensions = three ? 3 : 2;
        if (count != int(dimensions*2+2)) {error = "Area query arity mismatch."; return -1;}
        const auto& position=queryPosition;
        matches = true;
        for (size_t axis = 0; axis < dimensions; ++axis) {
            const float first = real(axis+1), second = real(axis+1+dimensions);
            if (!std::isfinite(first) || !std::isfinite(second)) {error = "Invalid area coordinate."; return -1;}
            if (locate) matches = matches && std::fabs(position[axis]-first) <= std::fabs(second);
            else matches = matches && position[axis] >= std::min(first,second) && position[axis] <= std::max(first,second);
        }
        if (name.find("on_foot") != std::string::npos) matches = matches && !environment.inVehicle;
        if (name.find("in_car") != std::string::npos) matches = matches && environment.inVehicle;
        if (name.find("stopped") != std::string::npos) matches = matches && std::fabs(querySpeed) < 0.01f;
    }
    if (inverted) matches = !matches;
    if (vm.conditionCount) {
        vm.condition = vm.conditionOr ? vm.condition || matches : vm.condition && matches;
        --vm.conditionCount;
    } else vm.condition = matches;
    vm.conditionValid = true;
    return 1;
}

int executeMissionGameplay(StorylandMissionVm& vm,uint16_t command,bool inverted,std::string& error) {
    const MissionCommand* definition=nullptr;
    for(const auto& candidate:missionCommands)
        if(command!=0 && command==(vm.vcs?candidate.vcs:candidate.lcs)) {definition=&candidate;break;}
    if(!definition) return 0;
    const auto metadata=vm.opcodeDefinitions.find(command);
    const unsigned expected=definition->arguments+(definition->action==MissionAction::Print?1:0);
    if(metadata!=vm.opcodeDefinitions.end() && metadata->second.first!=int(expected)) {
        error="SCM handler arity disagrees with the game-specific Sanny Builder definition.";
        return -1;
    }
    if(!vm.environment) {error="Gameplay command requires a live player context.";return -1;}
    auto& environment=*vm.environment;
    const auto action=definition->action;
    std::string label;
    if(action==MissionAction::Print) {
        if(!(vm.vcs?readVcsMissionText(vm,label):readScmText(vm,label))) {
            error="Truncated mission text label.";return -1;
        }
    }
    std::array<Operand,13> arguments{};
    for(unsigned index=0;index<definition->arguments;++index) {
        if(!readOperand(vm,arguments[index])) {error="Invalid gameplay command operand.";return -1;}
    }
    const auto integer=[&](size_t index) {return arguments[index].bits;};
    const auto real=[&](size_t index) {return operandFloat(arguments[index]);};
    const auto store=[&](size_t index,int32_t value) {
        if(writeOperand(vm,arguments[index],value)) return true;
        error="Gameplay command output is not a valid variable.";return false;
    };
    const auto storeReal=[&](size_t index,float value) {
        int32_t bits=0;std::memcpy(&bits,&value,4);return store(index,bits);
    };
    const auto compare=[&](bool value) {
        if(inverted) value=!value;
        if(vm.conditionCount) {
            vm.condition=vm.conditionOr?(vm.condition||value):(vm.condition&&value);
            --vm.conditionCount;
        } else vm.condition=value;
        vm.conditionValid=true;
    };
    const bool character=action==MissionAction::CharControl||action==MissionAction::CharOnFoot||action==MissionAction::GetCharPosition||action==MissionAction::SetCharPosition||
        action==MissionAction::GetCharHeading||action==MissionAction::SetCharHeading||action==MissionAction::SetHealth||
        action==MissionAction::GetHealth||action==MissionAction::AddArmour||action==MissionAction::CharAlive||action==MissionAction::CharInCar;
    const bool noHandle=action==MissionAction::GroundHeight||action==MissionAction::SetMobile||action==MissionAction::DisableLevelSplash||action==MissionAction::CreateMoneyPickup||action==MissionAction::ActivateSave||action==MissionAction::SaveFinished||action==MissionAction::CoordinateInLevel||action==MissionAction::GetTime||action==MissionAction::SetTime||action==MissionAction::SetMaxWanted||
        action==MissionAction::Print||action==MissionAction::ClearPrints||action==MissionAction::CreatePickup||
        action==MissionAction::CreatePickupWithAmmo||action==MissionAction::PickupCollected||action==MissionAction::RemovePickup||
        action==MissionAction::CreateCarGenerator||action==MissionAction::SwitchCarGenerator||
        action==MissionAction::GetGameTimer;
    const bool npcControl=vm.vcs&&action==MissionAction::CharControl&&integer(0)!=0;
    if(!noHandle&&!npcControl&&integer(0)!=(character?(vm.vcs?0:1):0)) {
        error="This command targets an entity outside the live player context.";return -1;
    }
    switch(action) {
    case MissionAction::GroundHeight: {
        const std::array<float,3> position{real(0),real(1),real(2)};
        for(float coordinate:position)if(!std::isfinite(coordinate)) {
            error="GET_GROUND_Z_FOR_3D_COORD requires finite coordinates.";return -1;
        }
        if(!environment.groundHeightAt) {error="Ground-height query requires the native collision world.";return -1;}
        float height=0;
        if(!environment.groundHeightAt(position,height))height=0;
        return storeReal(3,height)?1:-1;
    }
    case MissionAction::DisableLevelSplash:
        environment.suppressLevelSplash=integer(0)!=0;return 1;
    case MissionAction::SetMobile: {
        const int32_t handle=integer(0);
        if(handle!=(vm.vcs?0:1)&&!environment.missionCharacters.count(handle)) {
            error="SET_CHAR_ANSWERING_MOBILE refers to an unknown character.";return -1;
        }
        const bool answering=integer(1)!=0;
        auto found=environment.mobilePhones.find(handle);
        if (answering && handle == (vm.vcs ? 0 : 1) && environment.playerSavedWeapon == 48) {
            environment.playerSavedWeapon = environment.currentWeapon;
        }
        if(!answering&&found==environment.mobilePhones.end())return 1;
        if(found==environment.mobilePhones.end()||found->second.answering!=answering)
            environment.mobilePhones[handle]={answering,environment.gameMilliseconds};
        return 1;
    }
    case MissionAction::CharControl:
        if(npcControl) {
            if(!environment.missionCharacters.count(integer(0))) {
                error="IS_CHAR_IN_CONTROL references a missing character.";return -1;
            }
            if(!environment.characterInControl) {
                error="IS_CHAR_IN_CONTROL requires the live character state binding.";return -1;
            }
            compare(environment.characterInControl(integer(0)));
        } else compare(environment.control&&environment.health>0&&!environment.playerPositionFrozen);
        return 1;
    case MissionAction::CharOnFoot:compare(!environment.inVehicle&&!environment.activeVcsCharacterAttachment(0));return 1;
    case MissionAction::ActivateSave:
        environment.saveMenuRequested=true;environment.saveGameFinished=false;return 1;
    case MissionAction::SaveFinished:compare(environment.saveGameFinished);return 1;
    case MissionAction::CoordinateInLevel: {
        const auto* target=environment.zones?environment.zones->at({real(0),real(1),real(2)},false):nullptr;
        const auto* current=environment.zones?environment.zones->at({environment.x,environment.y,environment.z},false):nullptr;
        compare(target&&current&&target->level==current->level);return 1;
    }
    case MissionAction::GetGameTimer:return store(0,int32_t(environment.gameMilliseconds))?1:-1;
    case MissionAction::ClearPlayerWeapons:
        environment.clearPlayerWeaponsRequested=true;environment.currentWeapon=0;return 1;
    case MissionAction::PlayerInCar:compare(environment.inVehicle);return 1;
    case MissionAction::SwitchCarGenerator: {
        auto found=std::find_if(environment.carGenerators.begin(),environment.carGenerators.end(),
            [&](const auto& item){return item.handle==integer(0);});
        if(found==environment.carGenerators.end()) {
            error="SWITCH_CAR_GENERATOR refers to an unknown generator.";return -1;
        }
        // Both native engines store finite counts in a 16-bit field;
        // 0xffff represents a permanently enabled generator.
        const int32_t count=integer(1);
        const uint16_t nativeCount=count>=101?uint16_t(0xffff):uint16_t(count);
        found->generations=nativeCount==0xffff?-1:int32_t(nativeCount);
        return 1;
    }
    case MissionAction::CreateCarGenerator: {
        StorylandMissionEnvironment::CarGenerator generator;
        generator.x=real(0);generator.y=real(1);generator.z=real(2);
        generator.heading=real(3);generator.model=integer(4);
        generator.color1=integer(5);generator.color2=integer(6);
        generator.forceSpawn=integer(7);generator.alarm=integer(8);
        generator.doorLock=integer(9);
        generator.minimumDelay=integer(10);generator.maximumDelay=integer(11);
        if(!std::isfinite(generator.x)||!std::isfinite(generator.y)||
            !std::isfinite(generator.z)||!std::isfinite(generator.heading)||
            !arguments[12].reference) {
            error="CREATE_CAR_GENERATOR has invalid coordinates, model or output.";return -1;
        }
        if(environment.carGenerators.size()>=8192||environment.nextCarGeneratorHandle==INT32_MAX) {
            error="CREATE_CAR_GENERATOR exceeded the retail generator pool.";return -1;
        }
        generator.handle=environment.nextCarGeneratorHandle++;
        environment.carGenerators.push_back(generator);
        return store(12,generator.handle)?1:-1;
    }
    case MissionAction::PickupCollected:
    case MissionAction::RemovePickup: {
        auto pickup = std::find_if(environment.pickups.begin(),environment.pickups.end(),
            [&](const StorylandMissionEnvironment::Pickup& item) {return item.handle==integer(0);});
        if (action==MissionAction::PickupCollected) {
            auto& events=environment.collectedPickupHandles;
            const auto event=std::find(events.begin(),events.end(),integer(0));
            const bool collected=event!=events.end()||(pickup!=environment.pickups.end()&&pickup->collected);
            if(event!=events.end())events.erase(event);
            // Native 0x1DE078 consumes a collection event independently of
            // whether the pickup object was subsequently removed/reused.
            compare(collected);
            if(pickup!=environment.pickups.end())pickup->collected=false;
        }
        else if (pickup!=environment.pickups.end()) pickup->removed=true;
        return 1;
    }
    case MissionAction::CreatePickup:
    case MissionAction::CreateMoneyPickup:
    case MissionAction::CreatePickupWithAmmo: {
        const bool withAmmo=action==MissionAction::CreatePickupWithAmmo;
        const bool money=action==MissionAction::CreateMoneyPickup;
        const size_t coordinate=money?0:withAmmo?3:2, output=money?4:withAmmo?6:5;
        const float x=real(coordinate),y=real(coordinate+1);
        float z=real(coordinate+2);
        if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)) {
            error="CREATE_PICKUP contains a non-finite position.";return -1;
        }
        if((vm.vcs&&money&&(z==-100.0f||z<=-250.0f))||(!vm.vcs&&z<=-100.0f)) {
            float ground=0;
            if(!environment.groundHeightAt||!environment.groundHeightAt({x,y,z},ground)||!std::isfinite(ground)) {
                error="CREATE_PICKUP requires native ground height for its ground sentinel.";return -1;
            }
            z=roundEeFloat(double(ground)+0.5);
        }
        const auto reusable=std::find_if(environment.pickups.begin(),environment.pickups.end(),[](const auto& pickup) {
            return pickup.removed||(pickup.unavailable&&!pickup.collected&&pickup.respawnAt==0);
        });
        if((environment.pickups.size()>=4096&&reusable==environment.pickups.end()) || environment.nextPickupHandle==INT32_MAX) {
            error="CREATE_PICKUP exceeded the mission pickup limit.";return -1;
        }
        if(!arguments[output].reference) {
            error="CREATE_PICKUP requires a pickup handle output variable.";return -1;
        }
        const int32_t handle=environment.nextPickupHandle++;
        StorylandMissionEnvironment::Pickup created{handle,money?0:integer(0),money?8:integer(1),x,y,z,false,
            money?integer(3):withAmmo?std::max(0,integer(2)):0};
        const size_t slot=reusable==environment.pickups.end()?environment.pickups.size():size_t(reusable-environment.pickups.begin());
        if(slot==environment.pickups.size())environment.pickups.push_back(std::move(created));
        else {
            environment.lcsPickupDetails.erase(environment.pickups[slot].handle);
            environment.pickups[slot]=std::move(created);
        }
        auto& pickup=environment.pickups[slot];pickup.area=environment.area;
        if(money) {
            pickup.modelNameOverride="money";
            if(environment.modelIdByName) {
                const int32_t model=environment.modelIdByName("money");if(model>=0)pickup.model=model;
            }
        }
        if(pickup.type==4||pickup.type==5)
            pickup.expiresAt=environment.gameMilliseconds+(pickup.type==4?20000u:120000u);
        if(pickup.type==8)pickup.expiresAt=environment.gameMilliseconds+22000u;
        return store(output,handle)?1:-1;
    }
    case MissionAction::CreatePlayer:
        if(!store(4,0)) return -1;
        if(!std::isfinite(real(1))||!std::isfinite(real(2))||!std::isfinite(real(3))) break;
        environment.x=real(1);environment.y=real(2);environment.z=real(3);return 1;
    case MissionAction::GetPosition:case MissionAction::GetCharPosition:
        return storeReal(1,environment.x)&&storeReal(2,environment.y)&&storeReal(3,environment.z)?1:-1;
    case MissionAction::SetPosition:case MissionAction::SetCharPosition: {
        if(!std::isfinite(real(1))||!std::isfinite(real(2))||!std::isfinite(real(3))) break;
        float height=real(3);
        if(!vm.vcs&&action==MissionAction::SetPosition&&height<=-100.0f) {
            if(!environment.groundHeightAt) {
                error="SET_PLAYER_COORDINATES requires the native ground-collision binding.";return -1;
            }
            if(!environment.groundHeightAt({real(1),real(2),1000},height))height=20.0f;
            if(!std::isfinite(height)) {error="SET_PLAYER_COORDINATES returned a non-finite ground height.";return -1;}
        }
        environment.x=real(1);environment.y=real(2);environment.z=height;return 1;
    }
    case MissionAction::GetPlayerChar:return store(1,vm.vcs?0:1)?1:-1;
    case MissionAction::AddMoney:
        environment.money=int32_t(std::clamp(int64_t(environment.money)+integer(1),int64_t(0),int64_t(INT32_MAX)));return 1;
    case MissionAction::GetMoney:return store(1,environment.money)?1:-1;
    case MissionAction::MoneyGreater:compare(environment.money>integer(1));return 1;
    case MissionAction::SetWanted:
        environment.wanted=std::clamp(integer(1),0,environment.maxWanted);
        if(!vm.vcs)environment.requestWantedReset();
        return 1;
    case MissionAction::RaiseWanted: {
        const int wanted=std::max(environment.wanted,std::clamp(integer(1),0,environment.maxWanted));
        const bool raised=wanted>environment.wanted;
        environment.wanted=wanted;
        if(!vm.vcs&&raised)environment.requestWantedReset();
        return 1;
    }
    case MissionAction::ClearWanted:
        environment.wanted=0;if(!vm.vcs)environment.requestWantedReset();return 1;
    case MissionAction::WantedGreater:compare(environment.wanted>integer(1));return 1;
    case MissionAction::GetWanted:return store(1,environment.wanted)?1:-1;
    case MissionAction::SetMaxWanted:
        environment.maxWanted=std::clamp(integer(0),0,6);environment.wanted=std::min(environment.wanted,environment.maxWanted);return 1;
    case MissionAction::GetTime: {
        const auto clock=storylandClockParts(environment.hour);
        return store(0,int32_t(clock.hour))&&store(1,int32_t(clock.minute))?1:-1;
    }
    case MissionAction::SetTime:
        if(vm.vcs)environment.hour=storylandVcsSetClockHour(integer(0),integer(1));
        else {
            const int64_t minutes=int64_t(integer(0))*60+integer(1);
            environment.hour=float((minutes%1440+1440)%1440)/60.0f;
        }
        environment.clockResetRequested=true;
        return 1;
    case MissionAction::GetHeading:case MissionAction::GetCharHeading:return storeReal(1,environment.heading)?1:-1;
    case MissionAction::SetHeading:case MissionAction::SetCharHeading:
        if(!std::isfinite(real(1))) break;
        environment.heading=std::remainder(real(1),360.0f);return 1;
    case MissionAction::SetHealth:environment.health=float(std::clamp(integer(1),0,1000));return 1;
    case MissionAction::GetHealth:case MissionAction::GetPlayerHealth:return store(1,int32_t(environment.health))?1:-1;
    case MissionAction::HealthGreater:compare(environment.health>integer(1));return 1;
    case MissionAction::AddArmour:
        environment.armour = std::clamp(environment.armour + float(integer(1)), 0.0f,
            vm.vcs ? float(environment.playerArmourLimit) : 100.0f);
        return 1;
    case MissionAction::SetControl:
        environment.control=integer(1)!=0;
        environment.cameraControlEnabled=environment.control;
        return 1;
    case MissionAction::ControlOn:compare(environment.control);return 1;
    case MissionAction::CharAlive:case MissionAction::PlayerPlaying:compare(environment.health>0);return 1;
    case MissionAction::CharInCar:compare(environment.inVehicle);return 1;
    case MissionAction::Print: {
        const bool big=command==(vm.vcs?0x0055:0x00BA);
        if(big&&(integer(1)<1||integer(1)>8)) {
            error="Big message style must be from one through eight.";return -1;
        }
        environment.messageNumbers.clear();
        environment.messageKey=label;
        environment.messageMilliseconds=uint32_t(std::clamp(integer(0),0,600000));
        environment.messageStyle=big?integer(1):0;
        if(big)vm.observeMissionMessage(label,environment.messageStyle);
        return 1;
    }
    case MissionAction::ClearPrints:
        environment.messageKey.clear();environment.messageMilliseconds=0;
        environment.messageNumbers.clear();environment.messageStyle=0;return 1;
    }
    error="Gameplay command contains a non-finite coordinate or angle.";return -1;
}

int executeVcsOnscreenCommand(StorylandMissionVm& vm,uint16_t command,std::string& error) {
    if(!vm.vcs)return 0;
    switch(command) {
    case 0x00AB:case 0x00AC:case 0x00AD:case 0x00AE:case 0x024C:case 0x024D:
    case 0x0311:case 0x022D:case 0x03F7:case 0x03F9:case 0x03FF:case 0x0168:case 0x0169:break;
    default:return 0;
    }
    if(!vm.environment) {error="Onscreen clock/counter command requires the live world.";return -1;}
    auto& world=*vm.environment;
    if(command==0x0168) {
        if(!std::isfinite(world.hour)) {error="Cannot store a non-finite world clock.";return -1;}
        world.storedClockHour=world.hour;world.clockStored=true;return 1;
    }
    if(command==0x0169) {world.hour=world.storedClockHour;world.clockResetRequested=true;return 1;}
    Operand first;
    if(!readOperand(vm,first)) {error="Onscreen clock/counter operand is truncated.";return -1;}
    if(command==0x022D) {world.onscreenTimerFrozen=first.bits!=0;return 1;}
    if(!first.reference||!first.global||first.variable>=vm.globals.size()) {
        error="Onscreen clocks and counters require a shared SCM global variable.";return -1;
    }
    const int32_t slot=int32_t(first.variable);
    if(command==0x00AC) {
        if(world.onscreenTimer.globalSlot==slot)world.onscreenTimer={};
        return 1;
    }
    if(command==0x00AE) {
        for(auto& counter:world.onscreenCounters)if(counter.globalSlot==slot)counter={};
        return 1;
    }
    Operand second;
    if(!readOperand(vm,second)) {error="Onscreen clock/counter type is missing.";return -1;}
    if(command==0x03F7) {
        for(auto& counter:world.onscreenCounters)if(counter.globalSlot==slot) {
            counter.money=second.bits!=0;break;
        }
        return 1;
    }
    const auto label=[&](std::string& result) {
        Operand value;
        if(!readStringOperand(vm,value,result)) {error="Onscreen label has an invalid typed string.";return false;}
        // Native AddClock/AddCounter copy at most ten bytes from the SCM key.
        if(result.size()>10)result.resize(10);
        return true;
    };
    const auto color=[&](std::array<uint8_t,4>& destination,bool background) {
        bool& pending=background?world.counterBackgroundPending:world.counterColorPending;
        if(!pending)return;
        const auto& source=background?world.counterBackground:world.counterColor;
        // Retail copies RGB only; the entry retains its initialized alpha.
        std::copy_n(source.begin(),3,destination.begin());pending=false;
    };
    if(command==0x00AB||command==0x024C) {
        StorylandMissionEnvironment::OnscreenTimer timer;
        timer.globalSlot=slot;timer.countDown=second.bits!=0;
        if(command==0x024C&&!label(timer.label))return -1;
        // AddClock(0x3967A8) leaves an occupied clock untouched. An offset of
        // zero is the native empty sentinel and cannot display a global zero.
        if(world.onscreenTimer.globalSlot<0&&slot>0) {
            color(timer.color,false);world.onscreenTimer=std::move(timer);
        }
        return 1;
    }
    StorylandMissionEnvironment::OnscreenCounter counter;
    counter.globalSlot=slot;counter.type=uint16_t(second.bits);
    size_t row=0;
    if(command==0x0311) {
        Operand position;
        if(!readOperand(vm,position)) {error="Onscreen counter row is missing.";return -1;}
        const uint16_t nativeRow=uint16_t(uint16_t(position.bits)-1);
        if(nativeRow>=world.onscreenCounters.size()) {error="Onscreen counter row is outside the native three-row table.";return -1;}
        row=nativeRow;
    } else if(command==0x03F9||command==0x03FF) {
        Operand type;
        if(!readOperand(vm,type)) {error="Onscreen maximum counter type is missing.";return -1;}
        counter.maximum=second.bits;counter.type=uint16_t(type.bits);
        if(command==0x03FF) {
            Operand secondaryType;
            if(!readOperand(vm,secondaryType)||!label(counter.secondaryLabel))return -1;
            counter.secondaryType=uint16_t(secondaryType.bits);
        }
    }
    if(command!=0x00AD&&!label(counter.label))return -1;
    // AddCounter(0x3968E8) uses the requested row, without searching for space.
    if(world.onscreenCounters[row].globalSlot<0&&slot>0) {
        color(counter.color,false);color(counter.background,true);
        world.onscreenCounters[row]=std::move(counter);
    }
    return 1;
}

void advanceVcsOnscreenTimer(StorylandMissionVm& vm) {
    if(!vm.vcs||!vm.environment)return;
    auto& world=*vm.environment;auto& timer=world.onscreenTimer;
    if(world.onscreenTimerFrozen||timer.globalSlot<0||size_t(timer.globalSlot)>=vm.globals.size())return;
    auto& value=vm.globals[size_t(timer.globalSlot)];
    const int32_t previousSeconds=value/1000;
    // COnscreenTimerEntry::Process (0x396340) samples the shared native step,
    // multiplies by20 in EE single precision, then converts toward zero.
    const int32_t delta=int32_t(roundEeFloat(double(world.nativeScriptTimeStep)*20.0));
    value=int32_t(timer.countDown?uint32_t(value)-uint32_t(delta):uint32_t(value)+uint32_t(delta));
    vm.globalStrings.erase(size_t(timer.globalSlot));
    if(!timer.countDown)return;
    if(value<0) {value=0;timer={};return;}
    if(previousSeconds<12) {
        if(timer.warningFrames)--timer.warningFrames;
        if(value/1000!=previousSeconds) {
            timer.warningFrames=8;
            if(world.audioSubsystemEnabled&&world.scriptSounds.size()<256)
                world.scriptSounds.push_back({168,{world.x,world.y,world.z},true});
        }
    } else timer.warningFrames=0;
}

#include "src/storyland/storyland_spatial_commands.inl"
#include "src/re/reVCS/script/Commands.inl"
#include "src/re/reLCS2/script/CoreCommands.inl"
#include "src/re/reLCS2/script/ExtendedCommands.inl"
#include "src/re/reLCS2/script/WorldCommands.inl"
#include "src/re/reLCS2/script/RetailCommands.inl"
#include "src/storyland/storyland_extended_mission_commands.inl"
#include "src/storyland/storyland_script_model_commands.inl"
#include "src/re/reVCS/script/ExtendedCommands.inl"

int executeVcsZoneCommand(StorylandMissionVm& vm, uint16_t command, bool inverted,
    std::string& error) {
    if (!vm.vcs || (command != 0x00B0 && command != 0x035B && command != 0x041D &&
        command != 0x0426 && command != 0x0427)) return 0;
    if (!vm.environment || !vm.environment->zones) {
        error = "Zone commands require the loaded DTZ zone tables.";
        return -1;
    }
    const auto& world = *vm.environment;
    const auto& zones = *world.zones;
    const bool informationZone = command == 0x035B || command == 0x0427;
    if ((informationZone ? zones.information : zones.navigation).empty()) {
        error = zones.status.empty() ? "The required DTZ zone table is unavailable." : zones.status;
        return -1;
    }
    StorylandNativePoint position;
    if (command == 0x00B0 || command == 0x035B) {
        Operand handle;
        if (!readOperand(vm, handle) || (command == 0x035B && handle.bits != 0)) {
            error = "Zone query has an invalid character or player handle.";
            return -1;
        }
        float heading = 0;
        if (!vcsCharacterPosition(world, handle.bits, position.x, position.y, position.z, heading)) {
            error = "Zone query references a missing character.";
            return -1;
        }
        int32_t vehicleHandle = -1;
        if (handle.bits == 0) {
            if (world.inVehicle) vehicleHandle = world.occupiedMissionVehicle;
        } else {
            vehicleHandle = world.missionCharacters.at(handle.bits).vehicle;
        }
        const auto vehicle = world.missionVehicles.find(vehicleHandle);
        if (vehicle != world.missionVehicles.end())
            position = {vehicle->second.x, vehicle->second.y, vehicle->second.z};
    } else {
        std::array<Operand, 3> coordinates;
        for (auto& coordinate : coordinates) {
            if (!readOperand(vm, coordinate)) {
                error = "Zone query has a truncated position.";
                return -1;
            }
        }
        position = {operandFloat(coordinates[0]), operandFloat(coordinates[1]), operandFloat(coordinates[2])};
    }
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
        error = "Zone query has a non-finite position.";
        return -1;
    }
    if (command == 0x0426 || command == 0x0427) {
        Operand output;
        if (!readOperand(vm, output) || !output.reference) {
            error = "Zone name query requires an output variable.";
            return -1;
        }
        // The retail handlers use navigation for 0x0426 and information for
        // 0x0427, despite their reversed names in the opcode descriptions.
        const auto* zone = zones.at(position, informationZone);
        if (!zone) {
            error = "Zone name query could not resolve a native zone.";
            return -1;
        }
        writeOperand(vm, output, 0);
        auto& strings = output.global ? vm.globalStrings : vm.localStrings;
        strings[output.variable] = zone->name;
        return 1;
    }
    Operand label;
    std::string name;
    if (!readStringOperand(vm, label, name) || name.empty() || name.size() > 8) {
        error = "Zone query requires a zone name of one to eight characters.";
        return -1;
    }
    const auto* zone = informationZone ? zones.at(position, true) : zones.named(name);
    const bool inside = zone && (informationZone ? zone->name == name : zone->contains(position));
    updateScmCondition(vm, inside, inverted);
    return 1;
}



}

bool storylandChooseLcsRestart(StorylandMissionEnvironment& world,
    const std::array<float,3>& position,bool arrested,
    StorylandMissionEnvironment::PoliceRestart& result) {
    for(float coordinate:position)if(!std::isfinite(coordinate))return false;
    if(world.restartOverrideEnabled) {
        result=world.restartOverride;
        world.restartOverrideEnabled=false;
        return true;
    }
    const StorylandNativePoint origin{position[0],position[1],position[2]};
    const auto& restarts=arrested?world.policeRestarts:world.hospitalRestarts;
    const bool hasLevels=world.zones&&!world.zones->vcs&&!world.zones->map.empty();
    const uint32_t level=hasLevels?world.zones->levelAt(origin):0;
    float nearest=4000.0f*4000.0f;
    const StorylandMissionEnvironment::PoliceRestart* selected=nullptr;
    for(unsigned pass=0;pass<2&&!selected;++pass) {
        if(pass==0&&!hasLevels)continue;
        for(const auto& point:restarts) {
            if(!std::isfinite(point.x)||!std::isfinite(point.y)||
                !std::isfinite(point.z)||!std::isfinite(point.heading))continue;
            if(pass==0&&world.zones->levelAt({point.x,point.y,point.z})!=level)continue;
            const float x=point.x-origin.x,y=point.y-origin.y,z=point.z-origin.z;
            const float distance=x*x+y*y+z*z;
            if(nearest>=distance) {nearest=distance;selected=&point;}
        }
    }
    if(selected) {result=*selected;return true;}
    // FindNodeClosestToCoors(PATH_PED) includes disabled and transition nodes;
    // its default filter excludes water and uses the native weighted L1 metric.
    if(!world.pedestrianPaths||world.pedestrianPaths->vcs)return false;
    const auto& nodes=world.pedestrianPaths->nodes;
    // The native search initializes its selected index to zero. With the
    // restart's large distance limit, a search with no nearer eligible ped
    // node retains node zero, even if that entry belongs to a vehicle path.
    const StorylandNativePathNode* node=nodes.empty()?nullptr:&nodes.front();
    nearest=10000.0f;
    for(const auto& candidate:nodes) {
        if(candidate.vehicle||candidate.water)continue;
        const auto& point=candidate.position;
        const float distance=std::fabs(point.x-origin.x)+std::fabs(point.y-origin.y)+
            3.0f*std::fabs(point.z-origin.z);
        if(distance<nearest) {nearest=distance;node=&candidate;}
    }
    if(!node||!std::isfinite(node->position.x)||!std::isfinite(node->position.y)||
        !std::isfinite(node->position.z))return false;
    result={node->position.x,node->position.y,node->position.z,0};
    return true;
}

bool StorylandMissionEnvironment::activeVcsCharacterAttachment(int32_t handle) const {
    const auto object = vcsCharacterObjectParents.find(handle);
    if (object != vcsCharacterObjectParents.end()) return object->second > 0;
    if (handle == 0) return playerAttachedVehicle >= 0;
    const auto character = missionCharacters.find(handle);
    return character != missionCharacters.end() && character->second.attached &&
        character->second.vehicle >= 0;
}

void StorylandMissionEnvironment::releaseVcsObjectAttachments(int32_t handle) {
    for (auto& entry : vcsCharacterObjectParents) {
        if (entry.second != handle) continue;
        // CEntity::ResolveReferences clears CPed+858 without explicit detach.
        entry.second = -1;
        const auto character = missionCharacters.find(entry.first);
        if (character != missionCharacters.end()) character->second.attached = false;
    }
}

void StorylandMissionEnvironment::releaseVcsVehicleAttachments(int32_t handle) {
    std::vector<int32_t> attached;
    for(const auto& entry:vcsCharacterAttachments) {
        if(vcsCharacterObjectParents.count(entry.first))continue;
        const auto character=missionCharacters.find(entry.first);
        if((entry.first==0&&playerAttachedVehicle==handle)||
            (character!=missionCharacters.end()&&character->second.vehicle==handle))
            attached.push_back(entry.first);
    }
    for(int32_t character:attached)storylandDetachVcsCharacter(*this,character);
    mountedGunRequests.erase(std::remove_if(mountedGunRequests.begin(),mountedGunRequests.end(),
        [handle](const auto& request) {return request.vehicle&&request.target==handle;}),mountedGunRequests.end());
}

// Script deletion and world clearing must release the same typed weak references.
void storylandDeleteMissionCharacter(StorylandMissionEnvironment& world,int32_t handle) {
    world.vcsPedFlag28.erase(handle);
    world.vcsPedFlag1D8Bit6.erase(handle);
    world.vcsPedFalls.erase(handle);
    if(handle<=0)return; // Handle zero belongs to the player.
    for(auto look=world.scriptedPedLooks.begin();look!=world.scriptedPedLooks.end();) {
        if(look->first==handle||look->second.target==handle)look=world.scriptedPedLooks.erase(look);
        else ++look;
    }
    world.vcsPedFlag14.erase(handle);
    for (auto& collective : world.characterCollectives) {
        collective.erase(std::remove(collective.begin(), collective.end(), handle), collective.end());
    }
    storylandClearDamageSource(world,StorylandMissionEnvironment::DamageSourceKind::Character,handle);
    world.missionCleanupCharacters.erase(handle);
    world.characterFlags.erase(handle);
    world.characterWanderTasks.erase(handle);world.characterTargets.erase(handle);
    world.characterVehicleTasks.erase(handle);world.characterAttachments.erase(handle);
    world.vcsCharacterAttachments.erase(handle);
    world.vcsCharacterObjectParents.erase(handle);
    world.mountedGunRequests.erase(std::remove_if(world.mountedGunRequests.begin(),
        world.mountedGunRequests.end(),[handle](const auto& request) {
            return request.character==handle||(!request.vehicle&&request.target==handle);
        }),world.mountedGunRequests.end());
    world.lcsWaitAnimations.erase(handle);
    world.pedWaitStateMilliseconds.erase(handle);world.pedAnimationRequests.erase(handle);world.mobilePhones.erase(handle);
    const auto isTarget=[&](const auto& target) {
        return target.kind==StorylandMissionEnvironment::TargetKind::Character&&target.handle==handle;
    };
    if(isTarget(world.playerTarget))world.playerTarget={};
    for(auto& target:world.characterTargets)if(isTarget(target.second))target.second={};
    for(auto& car:world.missionVehicles)if(car.second.driver==handle)car.second.driver=-1;
    for(auto& object:world.objects)if(object.attachedCharacter==handle) {
        object.attachedCharacter=-1;object.attachedBone=-1;object.attachmentOffset={};object.attachmentRotation={};
    }
    for(auto& entry:world.missionCharacters) {
        if(entry.second.leader==handle) {
            entry.second.leader=-1;
            entry.second.leaderUpdateAfter=0;
            entry.second.leaderVehicleAfter=0;
        }
        if(entry.second.lookAtCharacter==handle)entry.second.lookAtCharacter=-1;
        const int objective=entry.second.objective;
        const bool targetsCharacter=objective==0x00E4||objective==0x00E5||objective==0x00E6||
            objective==0x00E7||objective==0x00F3||objective==0x00F4||objective==0x00EF||objective==0x02BE||
            objective==StorylandVcsLeaderFollowObjective;
        if(targetsCharacter&&entry.second.objectiveTarget==handle) {
            entry.second.objectiveTarget=-1;entry.second.objective=0;
            entry.second.walkingToTarget=false;entry.second.seekRoute.clear();entry.second.seekRouteCursor=0;
        }
    }
    for(auto& fire:world.scriptFires)if(fire.second.attachedKind==1&&fire.second.attachedHandle==handle) {
        fire.second.active=false;fire.second.attachedKind=0;fire.second.attachedHandle=-1;
    }
    if(world.cameraFocus.kind==2&&world.cameraFocus.handle==handle)world.cameraFocus={};
    if(world.cameraLookAt.kind==2&&world.cameraLookAt.handle==handle)world.cameraLookAt={};
    world.photographedCharacters.erase(handle);
    world.radarBlips.erase(std::remove_if(world.radarBlips.begin(),world.radarBlips.end(),
        [&](const auto& blip){return blip.entity==StorylandMissionEnvironment::BlipEntity::Character&&blip.entityHandle==handle;}),world.radarBlips.end());
    world.scriptedCharacterAnimations.erase(handle);
    for (auto& entry : world.missionCharacters) {
        auto& behavior = entry.second.behavior;
        if (!behavior.forcedThreatIsVehicle && behavior.forcedThreat == handle) behavior.forcedThreat = -1;
        if (behavior.conversationPartner == handle) {
            behavior.conversationPartner = -1;
            if (entry.second.animationId == 14) entry.second.animationId = -1;
        }
    }
    world.missionCharacters.erase(handle);
}

namespace {
bool retireVcsReleasedDeadResidents(StorylandMissionEnvironment& world, int32_t vehicleHandle) {
    const auto vehicle = world.missionVehicles.find(vehicleHandle);
    if (vehicle == world.missionVehicles.end() || vehicle->second.persistent ||
        vehicle->second.deleted || world.coopGuest || world.missionCleanupVehicles.count(vehicleHandle) ||
        vehicleHandle == world.occupiedMissionVehicle || vehicleHandle == world.playerAttachedVehicle ||
        vehicleHandle == world.playerVehicleEntryTarget || vehicleHandle == world.remoteVehicle ||
        (world.vehicleEntryInProgress && world.vehicleEntryInProgress(vehicleHandle)) ||
        vehicle->second.driver == 0 || vehicle->second.driver < -1)
        return false;
    for (const auto& entry : world.characterVehicleTasks) {
        if (entry.second.vehicle == vehicleHandle || entry.second.destinationVehicle == vehicleHandle)
            return false;
    }

    std::vector<int32_t> residents;
    std::vector<int32_t> attachments;
    std::array<bool, 8> passengerSeats{};
    int passengerCapacity = -1;
    bool driverFound = vehicle->second.driver < 0;
    for (const auto& entry : world.missionCharacters) {
        const auto& character = entry.second;
        if (character.vehicle != vehicleHandle) continue;
        if (entry.first <= 0 || character.networkControlled || character.persistent ||
            character.creationSource != 1 || character.alive || !std::isfinite(character.health) ||
            character.health > 0 || world.missionCleanupCharacters.count(entry.first) ||
            world.characterVehicleTasks.count(entry.first))
            return false;
        if (character.attached) {
            if (vehicle->second.driver == entry.first || character.passengerSeat >= 0 ||
                !world.vcsCharacterAttachments.count(entry.first))
                return false;
            attachments.push_back(entry.first);
        } else if (vehicle->second.driver == entry.first) {
            if (character.passengerSeat >= 0) return false;
            driverFound = true;
            residents.push_back(entry.first);
        } else {
            if (passengerCapacity < 0) {
                if (!world.vehiclePassengerCapacity) return false;
                passengerCapacity = world.vehiclePassengerCapacity(vehicleHandle);
            }
            if (passengerCapacity < 1 || passengerCapacity > int(passengerSeats.size())) return false;
            if (character.passengerSeat < 0 || character.passengerSeat >= int(passengerSeats.size()) ||
                character.passengerSeat >= passengerCapacity ||
                passengerSeats[size_t(character.passengerSeat)])
                return false;
            passengerSeats[size_t(character.passengerSeat)] = true;
            residents.push_back(entry.first);
        }
    }
    if (!driverFound || (residents.empty() && attachments.empty())) return false;

    // Native 18FA50 -> 3E6660 removes actual residents before the parent.
    // This subset retains every live, mission-owned or network-controlled actor.
    for (int32_t handle : residents) storylandDeleteMissionCharacter(world, handle);
    for (int32_t handle : attachments) {
        auto& character = world.missionCharacters.at(handle);
        // Parent ResolveReferences clears registered +858 without restoring
        // weapons. The separate dead actor keeps its state until normal culling.
        character.vehicle = -1;
        character.attached = false;
        world.vcsCharacterAttachments.erase(handle);
        world.characterAttachments.erase(handle);
    }
    return true;
}
}

void storylandCleanupVcsMissionActors(StorylandMissionEnvironment& world) {
    world.restorePlayerAfterCutscene();
    if(world.cutscene.loaded||world.cutscene.playing||!world.cutscene.actors.empty()) {
        ++world.cutscene.clearRequest;
        world.cutscene.loaded=false;world.cutscene.playing=false;world.cutscene.finished=false;
        world.cutscene.offsetSet=false;world.cutscene.actors.clear();
        world.cutscene.subtitleKey.clear();world.cutscene.subtitleText.clear();
        world.camera.cutsceneOwned=false;world.camera.restoreRequested=true;
    }
    for(int32_t handle:world.missionCleanupCharacters) {
        const auto found=world.missionCharacters.find(handle);
        if(found==world.missionCharacters.end())continue;
        auto& character=found->second;
        character.persistent=false;character.creationSource=1;
        if(!character.keepObjectiveOnRelease) {
            character.objective=0;character.objectiveTarget=-1;character.walkingToTarget=false;
            character.waitState=0;character.waitDeadline=0;character.animationId=-1;
            world.characterWanderTasks.erase(handle);world.characterTargets.erase(handle);
        }
    }
    for(int32_t handle:world.missionCleanupVehicles) {
        const auto found=world.missionVehicles.find(handle);
        if(found!=world.missionVehicles.end())found->second.persistent=false;
    }
    world.missionCleanupCharacters.clear();world.missionCleanupVehicles.clear();
}

std::u16string storylandMissionFormattedText(const StorylandMissionEnvironment& environment,
    const std::string& key,const std::vector<int32_t>& numbers,const std::string& stringKey) {
    std::u16string text=environment.pagerText?environment.pagerText(key):
        std::u16string(key.begin(),key.end());
    size_t cursor=0;
    for(int32_t number:numbers) {
        const size_t token=text.find(u"~1~",cursor);
        if(token==std::u16string::npos)break;
        const std::string digits=std::to_string(number);
        const std::u16string replacement(digits.begin(),digits.end());
        text.replace(token,3,replacement);
        cursor=token+replacement.size();
    }
    if(!stringKey.empty()) {
        const auto replacement=environment.pagerText?environment.pagerText(stringKey):
            std::u16string(stringKey.begin(),stringKey.end());
        size_t token=text.find(u"~a~");
        if(token!=std::u16string::npos) {
            do {
                text.replace(token,3,replacement);
                token=text.find(u"~a~",token+replacement.size());
            } while(token!=std::u16string::npos);
        } else {
            token=text.find(u"~1~");
            if(token!=std::u16string::npos)text.replace(token,3,replacement);
        }
    }
    return text;
}

std::string storylandMissionAreaNameKey(const StorylandMissionEnvironment& environment) {
    if (environment.areaNameForced) {
        return environment.areaNameKey;
    }
    if (!environment.zones) {
        return {};
    }
    const StorylandNativePoint position{environment.x, environment.y, environment.z};
    if (!environment.zones->vcs) {
        const auto* zone = environment.zones->at(position, true);
        return zone ? zone->name : std::string{};
    }
    const auto& navigation = environment.zones->navigation;
    if (navigation.empty()) {
        return {};
    }
    // 0x23AEF4/0x23AF0C prefer navigation type one, then type zero.
    // The original VCS sibling chain traverses these records in reverse order.
    for (uint32_t type : {1u, 0u}) {
        for (auto zone = navigation.rbegin(); zone != navigation.rend(); ++zone) {
            if (zone->type == type && zone->contains(position)) {
                return zone->name;
            }
        }
    }
    return navigation.front().name;
}

void storylandAdvanceVcsAreaCaption(StorylandMissionEnvironment& environment) {
    auto& caption = environment.vcsAreaCaption;
    caption.alpha = 0;
    if (!caption.visible || !environment.wantsHud || environment.widescreen ||
        environment.cutscene.playing) {
        return;
    }
    const std::string currentKey = storylandMissionAreaNameKey(environment);
    if (currentKey.empty()) {
        return;
    }
    using Phase = StorylandMissionEnvironment::VcsAreaCaption::Phase;
    if (currentKey != caption.previousKey) {
        if (caption.phase == Phase::Hidden) {
            caption.phase = Phase::FadeIn;
            caption.displayMilliseconds = 0;
            caption.fadeMilliseconds = 0;
        } else {
            if (caption.phase != Phase::Hold || !environment.areaNameForced) {
                caption.phase = Phase::Replace;
            }
            caption.displayMilliseconds = 5;
        }
        caption.displayedKey = currentKey;
        caption.previousKey = currentKey;
    }
    // CHud::DrawAreaName, 0x320338: integer timers advance once per native
    // frame, using the same EE timestep conversion as the onscreen clock.
    const int32_t delta = int32_t(roundEeFloat(double(environment.nativeScriptTimeStep) * 20.0));
    switch (caption.phase) {
    case Phase::Hidden:
        caption.displayedKey.clear();
        return;
    case Phase::Hold:
        caption.fadeMilliseconds = 1000;
        if (caption.displayMilliseconds > 10000) {
            caption.phase = Phase::FadeOut;
        }
        caption.alpha = 255;
        break;
    case Phase::FadeIn:
        caption.fadeMilliseconds += delta;
        if (caption.fadeMilliseconds > 1000) {
            caption.fadeMilliseconds = 1000;
            caption.phase = Phase::Hold;
        }
        break;
    case Phase::FadeOut:
    case Phase::Replace:
        caption.fadeMilliseconds -= delta;
        if (caption.fadeMilliseconds < 0) {
            caption.fadeMilliseconds = 0;
            if (caption.phase == Phase::Replace) {
                caption.phase = Phase::FadeIn;
                caption.displayedKey = caption.previousKey;
            } else {
                caption.phase = Phase::Hidden;
            }
        }
        break;
    }
    if (caption.alpha == 0) {
        const float opacity = roundEeFloat(double(caption.fadeMilliseconds) * 0.2549999952316284);
        caption.alpha = uint8_t(std::clamp(int32_t(opacity), 0, 255));
    }
    if (!environment.helpMessage.empty() || !environment.bigMessageQueues[1].empty() ||
        !environment.bigMessageQueues[6].empty() || !environment.bigMessageQueues[2].empty()) {
        caption.phase = Phase::FadeOut;
        caption.alpha = 0;
        return;
    }
    caption.displayMilliseconds += uint32_t(delta);
}

bool storylandMissionPagerScrollSuppressed(const StorylandMissionEnvironment& environment,
    bool displaySuppressed) {
    // CPager 126800/126828 reads native big-message slots 0 and 2.
    const auto displayedBigMessage=[&](size_t slot) {
        const auto& queue=environment.bigMessageQueues[slot];
        if(queue.empty()||!queue.front().remaining)return false;
        const auto& message=queue.front();
        const auto text=storylandMissionFormattedText(environment,message.key,message.numbers,message.stringKey);
        return !text.empty()&&text.front()!=u'\0';
    };
    return displaySuppressed||displayedBigMessage(0)||displayedBigMessage(2);
}

std::u16string storylandUpdateMissionPager(StorylandMissionEnvironment& environment,
    bool displaySuppressed) {
    auto text=environment.pager.update(environment.gameMilliseconds,
        storylandMissionPagerScrollSuppressed(environment,displaySuppressed),displaySuppressed);
    // CHud 322D40 emits event 101 once during each native entrance.
    if(environment.pager.soundRequested&&environment.audioSubsystemEnabled&&
        environment.frontendAudioEnabled&&environment.scriptSounds.size()<4096)
        environment.scriptSounds.push_back({101,{environment.x,environment.y,environment.z},true});
    return text;
}

void storylandAdvanceMissionPresentation(StorylandMissionEnvironment& environment,uint32_t elapsedMilliseconds,
    bool vcs) {
    if(vcs) {
        environment.scriptFrameText.clear();
        environment.scriptFrameTextStyle={};
        environment.frameScriptCheckpoints.clear();
        environment.frameScriptShadows.clear();
    }
    if(environment.credits.active&&elapsedMilliseconds) {
        auto& credits=environment.credits;
        const uint32_t end=credits.lines.empty()?0:uint32_t(credits.lines.back().offset)+(vcs?704u:uint32_t(credits.lines.back().height+14)+458u);
        credits.scroll+=environment.creditsFastForward?20:1;
        if(environment.creditsSkipPressed||credits.scroll>end) {
            credits.active=false;credits.lines.clear();
        }
    }
    environment.cameraShake.sampled=false;
    auto& zoom = environment.cameraZoom;
    const float zoomStep = 0.12f * std::max(0.0f, environment.nativeScriptTimeStep);
    const auto moveZoom = [&](float current, float target) {
        return current < target ? std::min(target, current + zoomStep) : std::max(target, current - zoomStep);
    };
    if (environment.inVehicle && zoom.vehicle) {
        zoom.vehicleCurrent = moveZoom(zoom.vehicleCurrent, zoom.vehicleTarget);
    }
    if (!environment.inVehicle) {
        if(vcs&&environment.activeCameraMode&&environment.activeCameraMode()==4) {
            StorylandVcsPedCameraZoom::Input input;
            if(environment.control&&environment.cameraControlEnabled&&!environment.cutscene.playing&&
                !environment.activeVcsCharacterAttachment(0))
                input=StorylandVcsPedCameraZoom::pressed(environment.controllerMode,
                    environment.padState,environment.previousPadState);
            environment.cameraSpecialMode=StorylandVcsPedCameraZoom::cycle(environment.cameraSpecialMode,
                input.nearer,input.farther,environment.cameraScriptModeEnabled);
            const float target=zoom.ped?zoom.pedTarget:
                StorylandVcsPedCameraZoom::target(environment.cameraSpecialMode);
            zoom.pedCurrent=StorylandVcsPedCameraZoom::advance(zoom.pedCurrent,target,
                environment.nativeScriptTimeStep);
        } else if(!vcs&&zoom.ped) {
            zoom.pedCurrent=moveZoom(zoom.pedCurrent,zoom.pedTarget);
        }
    }
    if(!vcs)return;
    const auto advance=[&](auto& queue) {
        if(queue.empty())return;
        if(queue.front().remaining>elapsedMilliseconds) {
            queue.front().remaining-=elapsedMilliseconds;
            return;
        }
        queue.pop_front();
        if(queue.empty())return;
        queue.front().remaining=queue.front().duration;
        if(queue.front().addToBrief&&(queue.front().style==0||queue.front().style==8)) {
            const auto& message=queue.front();
            auto& brief=environment.briefMessages;
            brief.erase(std::remove_if(brief.begin(),brief.end(),[&](const auto& previous) {
                return previous.key==message.key&&previous.stringKey==message.stringKey;
            }),brief.end());
            brief.push_front(message);
            if(brief.size()>20)brief.pop_back();
        }
    };
    const bool hadSubtitle=!environment.subtitleQueue.empty();
    advance(environment.subtitleQueue);
    for(auto& queue:environment.bigMessageQueues)advance(queue);
    if(!environment.subtitleQueue.empty()) {
        const auto& message=environment.subtitleQueue.front();
        environment.messageKey=message.key;
        environment.messageNumbers=message.numbers;
        environment.messageStringKey=message.stringKey;
        environment.messageMilliseconds=message.remaining;
        environment.messageStyle=0;
    } else if(hadSubtitle) {
        environment.messageKey.clear();
        environment.messageNumbers.clear();
        environment.messageStringKey.clear();
        environment.messageMilliseconds=0;
    }
    storylandAdvanceVcsAreaCaption(environment);
    const std::string vehicleName = environment.inVehicle && environment.nativeVehicleNameKey ?
        environment.nativeVehicleNameKey(environment.occupiedVehicleModel) : std::string{};
    uint8_t areaPhase = uint8_t(environment.vcsAreaCaption.phase);
    storylandAdvanceVcsVehicleCaption(environment.vcsVehicleCaption,vehicleName,
        storylandVcsHudComponentVisible(environment,7) && environment.wantsHud &&
            !environment.widescreen && !environment.cutscene.playing,
        !environment.helpMessage.empty(),environment.nativeScriptTimeStep,areaPhase);
    environment.vcsAreaCaption.phase = StorylandMissionEnvironment::VcsAreaCaption::Phase(areaPhase);
}

void storylandResetMissionPresentation(StorylandMissionEnvironment& world) {
    world.missionResult={};
    world.subtitleQueue.clear();world.bigMessageQueues={};world.briefMessages.clear();
    world.messageKey.clear();world.messageNumbers.clear();world.messageStringKey.clear();
    world.messageMilliseconds=0;world.messageStyle=0;
    world.helpMessage.clear();world.helpMessageNumbers.clear();world.helpMessageMilliseconds=0;
    world.helpMessagePersistent=false;
    world.scriptPanels={};world.nextScriptPanelItem=0;
    world.scriptFrameText.clear();world.scriptFrameTextStyle={};
    world.cameraFocus={};world.cameraShake={};world.manualAim={};
    world.cameraLookAt={};world.frameScriptShadows.clear();world.photographFlashUntil=0;
    world.securityCamera=false;world.drunkCameraIntensity=0;world.drunkVisualIntensity=0;
    world.drunkVisualPeriod=0;world.drunkVisualAmplitude=0;
    world.drunkSteeringDrift=0;
    world.cameraZoom={};
    world.cameraControlEnabled=true;world.cameraFrontPlayerRequested=false;
    world.cameraBehindPlayerRequested=false;
    world.wantsHud=true;world.flashingHudObject=-1;
    world.areaNameKey.clear();
    world.areaNameForced = false;
    world.vcsAreaCaption = {};
    world.vcsAmmoHudVisible = true;
    world.vcsHudHiddenComponents = 0;
    world.vcsScript0556Value = 1.0f;
    world.vcsRemoteCameraUpsideDownOffset = 0.0f;
    world.vcsVehicleCaption = {};
    world.hudFlashFrames={50,50};world.hudFlashLastFrame={UINT32_MAX,UINT32_MAX};
    world.vcsHudMeters={};
    world.onscreenTimer={};world.onscreenCounters={};world.onscreenTimerFrozen=false;
    world.counterColorPending=false;world.counterBackgroundPending=false;
    world.missionPassedTune=-1;world.missionTuneLoop=false;++world.missionPassedTuneSerial;
    if(world.stopMissionPassedTune)world.stopMissionPassedTune();
}

bool StorylandMissionVm::loadOpcodeDefinitions(const std::wstring& path) {
    opcodeDefinitions.clear();
    std::ifstream file{std::filesystem::path(path)};
    if(!file)return false;
    std::string line;
    while(std::getline(file,line)) {
        if(line.size()<7||line[4]!='='||!std::isxdigit(static_cast<unsigned char>(line[0])))continue;
        uint32_t code=0;
        try {code=uint32_t(std::stoul(line.substr(0,4),nullptr,16));}
        catch(const std::exception&) {continue;}
        if(code>0x7FFFu)continue;
        const size_t comma=line.find(',',5);
        if(comma==std::string::npos)continue;
        int parameters=0;
        try {parameters=std::stoi(line.substr(5,comma-5));}
        catch(const std::exception&) {continue;}
        opcodeDefinitions[uint16_t(code)]={parameters,line.substr(comma+1)};
    }
    return !opcodeDefinitions.empty();
}

void StorylandMissionVm::cleanupLcsMission() {
    if(vcs||!environment)return;
    cleanUpLcsMissionEntities(*environment);
    missionCleanupRequested=false;
}

void StorylandMissionVm::recordMissionOutcome(StorylandMissionOutcome outcome,
    StorylandMissionFailure failure,const std::string& key,const std::string& reason) {
    const bool changed=missionOutcome!=outcome;
    missionOutcome=outcome;
    missionFailure=failure;
    if(!key.empty())missionResultKey=key;
    if(!environment)return;

    auto& result=environment->missionResult;
    if(changed||missionResultSequence==0) {
        if(result.sequence<UINT64_MAX)++result.sequence;
        missionResultSequence=result.sequence;
        result.gameMilliseconds=environment->gameMilliseconds;
    } else if(result.sequence!=missionResultSequence) {
        return;
    }
    result.outcome=missionOutcome;
    result.failure=missionFailure;
    result.missionSlot=missionSlot;
    result.scriptName=scriptName;
    result.key=missionResultKey;
    result.reason=reason;
}

void StorylandMissionVm::registerMissionPassed(const std::string& key,bool oddjob) {
    if(!environment)return;
    auto& world=*environment;
    if(vcs)world.restartMissionTaxiDestinationSet=false;
    if(oddjob) {
        world.oddjobsPassed=int32_t(uint32_t(world.oddjobsPassed)+1u);
        if(!vcs)world.statistics["OddJobMissionsPassed"]=world.oddjobsPassed;
    } else {
        world.passedMissions.insert(key);
        world.lastMissionPassedName=key;
    }
    world.statistics["MissionsPassed"]+=1;
    world.statistics["LastMissionPassedTime"]=world.gameMilliseconds;
    if(!vcs) {
        world.statistics["TotalLegitimateKills"]+=world.statistics["KillsSinceLastCheckpoint"];
        world.statistics["KillsSinceLastCheckpoint"]=0;
    }
    recordMissionOutcome(StorylandMissionOutcome::Passed,StorylandMissionFailure::None,
        oddjob?scriptName:key,"Mission passed.");
}

void StorylandMissionVm::recordMissionFailure(StorylandMissionFailure failure,
    const std::string& reason) {
    recordMissionOutcome(StorylandMissionOutcome::Failed,failure,scriptName,reason);
}

void StorylandMissionVm::observeMissionMessage(const std::string& key,int32_t style) {
    if(!isMissionThread||style<=0)return;
    if(key=="M_PASS"||key=="M_PASSN") {
        recordMissionOutcome(StorylandMissionOutcome::Passed,StorylandMissionFailure::None,
            missionResultKey.empty()?scriptName:missionResultKey,"Mission passed.");
    } else if(key=="M_FAIL") {
        if(missionOutcome==StorylandMissionOutcome::Failed)return;
        recordMissionFailure(StorylandMissionFailure::Script,"Mission failed.");
    }
}

bool StorylandMissionVm::handleDeathArrest(StorylandMissionFailure failure) {
    if(!environment||!isMissionThread||!deathArrestEnabled||
        (state!=StorylandMissionState::Running&&state!=StorylandMissionState::Waiting)||
        (failure!=StorylandMissionFailure::Death&&failure!=StorylandMissionFailure::Arrest))return false;
    const int32_t flag=environment->declaredMissionFlag;
    if(flag<0||size_t(flag)>=globals.size()||globals[size_t(flag)]!=1)return false;
    if(returnStack.empty()||(!functionFrames.empty()&&functionFrames.front().returnDepth==0)) {
        state=StorylandMissionState::Unsupported;
        vcsAbsoluteWait=false;
        diagnostic="Death/arrest requires a verified outer mission GOSUB return.";
        return false;
    }
    const size_t destination=returnStack.front();
    if(destination>=scriptBytes().size()) {
        state=StorylandMissionState::Fault;
        vcsAbsoluteWait=false;
        diagnostic="Death/arrest mission return is outside MAIN.SCM.";
        return false;
    }
    if(!functionFrames.empty()) {
        const int32_t timerA=locals[104];
        const int32_t timerB=locals[105];
        locals=functionFrames.front().previousLocals;
        localStrings=functionFrames.front().previousStrings;
        locals[104]=timerA;
        locals[105]=timerB;
    }
    returnStack.clear();
    functionFrames.clear();
    pc=destination;
    delayMilliseconds=0;
    vcsAbsoluteWait=false;
    state=StorylandMissionState::Running;
    globals[size_t(flag)]=0;
    deathArrestExecuted=true;
    if(!vcs)storylandClearLcsMessages(*environment,true,false);
    else {
        environment->subtitleQueue.clear();
        if(environment->messageStyle==0) {
            environment->messageKey.clear();
            environment->messageNumbers.clear();
            environment->messageMilliseconds=0;
            environment->messageStringKey.clear();
        }
    }
    const std::string reason=failure==StorylandMissionFailure::Arrest?
        "Mission failed: Busted.":"Mission failed: Wasted.";
    recordMissionFailure(failure,reason);
    diagnostic=reason+" Running the mission's failure cleanup.";
    return true;
}

void StorylandMissionVm::start(std::vector<uint8_t> mission, uint32_t missionFileOffset,bool vcsGame,
    bool resetMissionMarkers) {
    deathArrestEnabled=true;deathArrestExecuted=false;
    stop();
    if(environment&&resetMissionMarkers) {environment->markers.clear();environment->nextMarkerHandle=1;}
    script = std::move(mission);
    missionBase=0;
    wholeScm=false;
    fileOffset = missionFileOffset;
    vcs=vcsGame;
    if(!vcs&&environment&&environment->nextScriptCoronaId==0&&environment->scriptCoronas.empty())
        environment->nextScriptCoronaId=1;
    isMissionThread=true;
    isInternalMission=true;
    opcodeDefinitions.clear();
    provisionalOpcodes.clear();
    const auto addDefinition = [&](const StorylandScmDefinition& definition) {
        opcodeDefinitions.emplace(definition.opcode,std::make_pair(definition.parameters,definition.text));
    };
    if (vcs) for (const auto& definition : vcsDefinitions) addDefinition(definition);
    else for (const auto& definition : lcsDefinitions) addDefinition(definition);
    // Direct globals 0xCD..0xE5 address 0..6399; array tags 0xE6..0xFF
    // also address the final 6400..6655 block (native decoder 0x254FF8).
    globals.assign(6656,0);
    globalStrings.clear();localStrings.clear();
    state = scriptBytes().empty() ? StorylandMissionState::Fault : StorylandMissionState::Running;
    if (scriptBytes().empty()) diagnostic = "Selected mission has no binary script.";
}

void StorylandMissionVm::startRetail(const std::vector<uint8_t>& fullScm,uint32_t missionOffset,bool vcsGame) {
    startRetailShared(std::make_shared<const std::vector<uint8_t>>(fullScm),missionOffset,vcsGame);
}

void StorylandMissionVm::startRetailShared(std::shared_ptr<const std::vector<uint8_t>> fullScm,
    uint32_t missionOffset,bool vcsGame,bool resetMissionMarkers) {
    start({},missionOffset,vcsGame,resetMissionMarkers);
    sharedRetailScript=std::move(fullScm);
    wholeScm=true;missionBase=missionOffset;pc=missionOffset;
    if(!sharedRetailScript||missionOffset>=scriptBytes().size()) {
        state=StorylandMissionState::Fault;
        diagnostic="Retail SCM entry offset is outside MAIN.SCM.";
    } else {state=StorylandMissionState::Running;diagnostic.clear();}
}

void StorylandMissionVm::stop() {
    state = StorylandMissionState::Idle;
    script.clear();sharedRetailScript.reset();
    pc = 0;
    fileOffset = 0;
    delayMilliseconds = 0;
    vcsWakeTime=0;
    vcsAbsoluteWait=false;
    messageWaitSkippable=false;
    instructions = 0;
    executionTrace.clear();
    pendingLaunches.clear();missionCleanupRequested=false;
    missionOutcome=StorylandMissionOutcome::None;
    missionFailure=StorylandMissionFailure::None;
    missionResultKey.clear();missionSlot=-1;missionResultSequence=0;
    isInternalMission=false;
    globals.clear();locals.fill(0);returnStack.clear();globalStrings.clear();localStrings.clear();
    functionFrames.clear();missionBase=0;wholeScm=false;
    condition=false;conditionValid=false;conditionCount=0;conditionOr=false;
    diagnostic.clear();
    provisionalOpcodes.clear();
    scriptName.clear();
}

void StorylandMissionVm::tick(uint32_t elapsedMilliseconds,bool advanceEnvironment) {
    if(environment&&advanceEnvironment) {
        if(vcs) {
            ++environment->nativeFrameCounter;
            double rawTicks=double(elapsedMilliseconds)*147456.0;
            if(environment->nativeScriptTimeStepSource) {
                const float source=environment->nativeScriptTimeStepSource();
                if(std::isfinite(source))rawTicks=std::round(double(source)*2949120.0);
            }
            // Reconstruct the hardware counter interval before applying the
            // retail multiply order. Deriving integer time from step*20 loses
            // a millisecond at intervals such as17ms and33ms.
            const float ticks=roundEeFloat(std::clamp(rawTicks,0.0,double(UINT32_MAX)));
            float timeScale=environment->nativeTimeScale;
            if(environment->nativeTimeScaleSource) {
                const float source=environment->nativeTimeScaleSource();
                if(std::isfinite(source)&&source>=0)timeScale=source;
            }
            const float scaledTicks=roundEeFloat(double(ticks)*timeScale);
            uint32_t stepBits=0x34B60B61u,millisecondBits=0x36E38E39u;
            float stepFactor=0,millisecondFactor=0;
            std::memcpy(&stepFactor,&stepBits,4);std::memcpy(&millisecondFactor,&millisecondBits,4);
            const float step=roundEeFloat(double(scaledTicks)*stepFactor);
            environment->nativeAnimationTimeStep=std::max(step,0.5f);
            environment->nativeScriptTimeStep=std::min(environment->nativeAnimationTimeStep,3.3333334922790527f);
            // CTimer2F4960 scales both clocks before its independent limits:
            // integer gameplay time advances at most60ms, while physics uses
            // the separately clamped EE step. Bootstrap ticks never resample.
            environment->nativeFrameMilliseconds=uint32_t(std::clamp(
                roundEeFloat(double(scaledTicks)*millisecondFactor),0.0f,60.0f));
            elapsedMilliseconds=environment->nativeFrameMilliseconds;
        }
        if(!vcs) {
            ++environment->nativeFrameCounter;
            double rawTicks=double(elapsedMilliseconds)*147456.0;
            if(environment->nativeScriptTimeStepSource) {
                const float source=environment->nativeScriptTimeStepSource();
                if(std::isfinite(source))rawTicks=std::round(double(source)*2949120.0);
            }
            float timeScale=environment->nativeTimeScale;
            if(environment->nativeTimeScaleSource) {
                const float source=environment->nativeTimeScaleSource();
                if(std::isfinite(source)&&source>=0)timeScale=source;
            }
            const bool scriptPaused=environment->lcsExtendedWorld.scriptPause!=0;
            const float ticks=scriptPaused?0.0f:
                roundEeFloat(std::clamp(rawTicks,0.0,double(UINT32_MAX)));
            const float scaledTicks=roundEeFloat(double(ticks)*timeScale);
            const uint32_t stepBits=0x34B60B61u,millisecondBits=0x36E38E39u;
            float stepFactor=0,millisecondFactor=0;
            std::memcpy(&stepFactor,&stepBits,4);
            std::memcpy(&millisecondFactor,&millisecondBits,4);
            // LCS CTimer2B4D70..2B4F68 uses the same EE multiply order as
            // VCS, but caps the simulation step at3.0. Script pause bypasses
            // the0.5 lower limit and leaves both simulation clocks stopped.
            const float step=roundEeFloat(double(scaledTicks)*stepFactor);
            environment->nativeScriptTimeStep=std::clamp(step,scriptPaused?0.0f:0.5f,3.0f);
            environment->nativeFrameMilliseconds=uint32_t(std::clamp(
                roundEeFloat(double(scaledTicks)*millisecondFactor),0.0f,60.0f));
            elapsedMilliseconds=environment->nativeFrameMilliseconds;
        }
        advanceVcsOnscreenTimer(*this);
        environment->gameMilliseconds+=elapsedMilliseconds;
        if(vcs) {
            auto& world=*environment;
            const auto outsidePopulation=[&](float x,float y,float z) {
                return std::hypot(x-world.x,y-world.y,z-world.z)>150.0f&&
                    (!world.sphereOnScreen||!world.sphereOnScreen({x,y,z},4.0f));
            };
            std::vector<int32_t> releasedCharacters;
            for(const auto& entry:world.missionCharacters) {
                const auto& character=entry.second;
                if(!character.networkControlled&&!character.persistent&&!character.attached&&character.vehicle<0&&
                    outsidePopulation(character.x,character.y,character.z))releasedCharacters.push_back(entry.first);
            }
            for(int32_t handle:releasedCharacters)storylandDeleteMissionCharacter(world,handle);
            for(auto& entry:world.missionVehicles) {
                auto& vehicle=entry.second;
                if(!vehicle.persistent&&!vehicle.deleted&&entry.first!=world.occupiedMissionVehicle&&
                    entry.first!=world.playerAttachedVehicle&&outsidePopulation(vehicle.x,vehicle.y,vehicle.z)&&
                    (std::none_of(world.missionCharacters.begin(),world.missionCharacters.end(),
                        [&](const auto& character){return character.second.vehicle==entry.first;})||
                     retireVcsReleasedDeadResidents(world,entry.first)))
                    vehicle.deleted=true;
                if(vehicle.deleted) {
                    world.vcsVehicleFlag28.erase(entry.first);
                    world.photographedVehicles.erase(entry.first);
                }
            }
        }
        if(environment->failCurrentMission>0)--environment->failCurrentMission;
        if(!vcs) {
            environment->frameScriptCheckpoints.clear();
            environment->frameScriptCoronas.clear();
            storylandBeginLcsScriptTextFrame(*environment);
            if(!storylandAdvanceLcsWaitAnimations(*environment,randomState,diagnostic)) {
                state=StorylandMissionState::Unsupported;return;
            }
            storylandAdvanceLcsOnscreenTimers(*this,elapsedMilliseconds);
            storylandAdvanceLcsGarageDoors(*this,elapsedMilliseconds);
            if(state==StorylandMissionState::Unsupported)return;
            const std::array<float,3> player{environment->x,environment->y,environment->z};
            const auto camera=environment->craneCameraPosition?environment->craneCameraPosition():
                environment->camera.active?std::array<float,3>{environment->camera.x,
                    environment->camera.y,environment->camera.z}:player;
            if(!storylandAdvanceLcsCranes(environment->lcsCranes,elapsedMilliseconds,
                environment->gameMilliseconds,camera,player,diagnostic)) {
                state=StorylandMissionState::Unsupported;return;
            }
            storylandAdvanceLcsMissionTasks(*environment,elapsedMilliseconds,randomState);
            storylandAdvanceLcsEntityExtension(*environment,elapsedMilliseconds);
            const int32_t slot=environment->fortStauntonDestroyedGlobal;
            environment->fortStauntonDestroyed=slot>=0&&size_t(slot)<globals.size()&&globals[size_t(slot)]==1;
        }
        if(!vcs)storylandAdvanceLcsStuckCarChecks(*environment);
        storylandAdvanceScriptObjects(*environment,elapsedMilliseconds,vcs);
        if(!vcs)for(const auto& attachment:environment->characterAttachments) {
            const auto character=environment->missionCharacters.find(attachment.first);
            if(character!=environment->missionCharacters.end()&&character->second.vehicle < -1)
                positionLcsAttachedCharacter(*environment,attachment.first);
        }
        if(!vcs&&!storylandAdvanceLcsExtendedWorld(*environment,diagnostic)) {
            state=StorylandMissionState::Unsupported;return;
        }
        if(vcs) {
            if(!StorylandVcsFerrisWheel::advance(*environment,diagnostic)) {
                state=StorylandMissionState::Unsupported;return;
            }
        }
        for(auto check=environment->upsideDownCarChecks.begin();check!=environment->upsideDownCarChecks.end();) {
            const auto vehicle=environment->missionVehicles.find(check->first);
            if(vehicle==environment->missionVehicles.end()||vehicle->second.deleted) {
                check=environment->upsideDownCarChecks.erase(check);continue;
            }
            const auto motion=environment->vehicleOrientationAndSpeed?
                environment->vehicleOrientationAndSpeed(check->first):std::array<float,3>{1,0,0};
            if(motion[0]<=-0.97f&&motion[1]<0.01f&&motion[2]<0.02f)
                check->second=uint32_t(std::min(uint64_t(UINT32_MAX),uint64_t(check->second)+elapsedMilliseconds));
            else check->second=0;
            ++check;
        }
        if(vcs&&!storylandAdvanceCollectivePolicies(*environment,diagnostic)) {
            state=StorylandMissionState::Fault;return;
        }
        storylandAdvanceVcsMissionEntities(*environment,elapsedMilliseconds,vcs);
        if(!environment->helpMessagePersistent&&!environment->helpMessage.empty()) {
            environment->helpMessageMilliseconds=environment->helpMessageMilliseconds>elapsedMilliseconds?
                environment->helpMessageMilliseconds-elapsedMilliseconds:0;
            if(!environment->helpMessageMilliseconds)environment->helpMessage.clear();
        }
        if(environment->fading) {
            environment->fadeDuration=std::max(0.0f,
                environment->fadeDuration-float(elapsedMilliseconds));
            const float fraction=environment->fadeTotal>0?1.0f-environment->fadeDuration/environment->fadeTotal:1.0f;
            environment->fadeAlpha=environment->fadeStartAlpha+
                (environment->fadeTargetAlpha-environment->fadeStartAlpha)*fraction;
            if(environment->fadeDuration<=0)environment->fading=false;
        }
        if(!vcs)advanceLcsRemoteControl(*environment,elapsedMilliseconds);
        for(auto& marker:environment->markers)if(!marker.persistent)
            marker.milliseconds=marker.milliseconds>elapsedMilliseconds?marker.milliseconds-elapsedMilliseconds:0;
        environment->markers.erase(std::remove_if(environment->markers.begin(),environment->markers.end(),
            [](const auto& marker){return !marker.persistent&&marker.milliseconds==0;}),environment->markers.end());
        environment->playerSpeechMoodMilliseconds=environment->playerSpeechMoodMilliseconds>elapsedMilliseconds?
            environment->playerSpeechMoodMilliseconds-elapsedMilliseconds:0;
        if(!vcs&&environment->lcsMessageQueuesUsed)storylandAdvanceLcsMessages(*environment);
        else {
            environment->messageMilliseconds=environment->messageMilliseconds>elapsedMilliseconds?
                environment->messageMilliseconds-elapsedMilliseconds:0;
            if(!environment->messageMilliseconds) environment->messageKey.clear();
        }
        storylandAdvanceMissionPresentation(*environment,elapsedMilliseconds,vcs);
    }
    if(!vcs&&environment&&environment->failCurrentMission==1&&isMissionThread&&isInternalMission&&
        (state==StorylandMissionState::Running||state==StorylandMissionState::Waiting)&&
        returnStack.size()+functionFrames.size()==1) {
        if(returnStack.empty()) {
            state=StorylandMissionState::Unsupported;
            diagnostic="FAIL_CURRENT_MISSION requires a verified outer GOSUB return.";
            return;
        }
        const size_t destination=returnStack.back();
        if(destination>=scriptBytes().size()) {
            state=StorylandMissionState::Fault;
            diagnostic="Failed mission return is outside MAIN.SCM.";
            return;
        }
        returnStack.pop_back();
        pc=destination;
        recordMissionFailure(StorylandMissionFailure::Script,"Mission failed.");
    }
    if(environment&&isMissionThread&&deathArrestEnabled&&
        (state==StorylandMissionState::Running||state==StorylandMissionState::Waiting)) {
        if(environment->playerArrested)handleDeathArrest(StorylandMissionFailure::Arrest);
        else if(environment->health<=0)handleDeathArrest(StorylandMissionFailure::Death);
    }
    if (state == StorylandMissionState::Running || state == StorylandMissionState::Waiting) {
        for (size_t slot : {size_t(104),size_t(105)})
            locals[slot]=int32_t(uint32_t(locals[slot])+elapsedMilliseconds);
    }
    if (state == StorylandMissionState::Waiting) {
        const bool stillWaiting = vcsAbsoluteWait && environment ?
            environment->gameMilliseconds < vcsWakeTime : elapsedMilliseconds < delayMilliseconds;
        if (stillWaiting) {
            if(!vcs&&messageWaitSkippable&&environment&&environment->confirmJustPressed) {
                delayMilliseconds=0;
                storylandSkipLcsMessages(*environment);
                return; // Native wakes the thread on the following update.
            }
            if (vcs && messageWaitSkippable && environment && environment->messageSkipPressed) {
                delayMilliseconds = 0;
                vcsAbsoluteWait = false;
                environment->messageMilliseconds = 0;
                environment->messageKey.clear();
                if (!environment->subtitleQueue.empty()) {
                    environment->subtitleQueue.front().remaining = 0;
                }
                for (auto& queue : environment->bigMessageQueues) {
                    if (!queue.empty()) {
                        queue.front().remaining = 0;
                    }
                }
                return;
            }
            if(vcsAbsoluteWait && environment)
                delayMilliseconds=vcsWakeTime-environment->gameMilliseconds;
            else
                delayMilliseconds-=elapsedMilliseconds;
            return;
        }
        delayMilliseconds = 0;
        vcsAbsoluteWait = false;
        state = StorylandMissionState::Running;
    }
    if (state != StorylandMissionState::Running) return;
    // Original VCS 255FA8 runs until a command yields. The large bound diagnoses
    // a runaway thread instead of silently adding a physical frame boundary.
    const unsigned commandLimit = vcs ? 1048576u : 256u;
    for (unsigned budget = 0; budget < commandLimit && state == StorylandMissionState::Running; ++budget) {
        if (pc > scriptBytes().size() || scriptBytes().size() - pc < 2) {
            state = StorylandMissionState::Fault;
            diagnostic = "Mission terminated without an end-thread instruction.";
            return;
        }
        const size_t instructionOffset = pc;
        const uint16_t opcode = uint16_t(scriptBytes()[pc]) | (uint16_t(scriptBytes()[pc + 1]) << 8);
        pc += 2;
        ++instructions;
        if (executionTrace.size() >= 512) executionTrace.pop_front();
        executionTrace.push_back({instructionOffset,opcode});
        const uint16_t command=opcode&0x7FFFu;
        if (vcs && wholeScm && environment && command == 0x0028)
            environment->vcsCharacterHandleBridge = true;
        const bool inverted=(opcode&0x8000u)!=0;
        const uint16_t terminate=vcs?0x0023:0x004E;
        const uint16_t jumpTrue=vcs?0x0021:0x004C;
        const uint16_t jumpFalse=vcs?0x0022:0x004D;
        const uint16_t call=vcs?0x0025:0x0050;
        const uint16_t ret=vcs?0x0026:0x0051;
        const uint16_t ifCode=vcs?0x0078:0x00DB;
        const uint16_t missionFinished=vcs?0x007A:0x00DD;
        auto fail=[&](const std::string& reason) {
            state=StorylandMissionState::Unsupported;
            std::ostringstream message;
            message << reason << " Opcode 0x" << std::hex << std::uppercase
                << opcode << " at mission offset 0x" << instructionOffset
                << ", operand cursor 0x" << pc << '.';
            diagnostic=message.str();
        };
        auto jumpTo=[&](int32_t target)->bool {
            // Negative mission labels are offsets from the start of the
            // mission's own script. Positive main-script addresses must be
            // translated from their absolute file position.
            const int64_t destination=target<0?int64_t(missionBase)-int64_t(target):
                int64_t(target)+8-(wholeScm?0:int64_t(fileOffset));
            if(destination<0 || destination>=int64_t(scriptBytes().size()))return false;
            pc=size_t(destination);return true;
        };
        if(!vcs&&command==0x066E) {
            Operand disabled;
            if(!environment||!readOperand(*this,disabled)) {fail("Invalid pause-menu access command.");return;}
            environment->pauseMenuDisabled=disabled.bits!=0;continue;
        }
        if(!vcs&&command==0x031D) {
            if(!environment) {fail("Mission statistics require the live world.");return;}
            std::string key;
            if(!readScmText(*this,key)||key.empty()) {fail("Invalid completed mission label.");return;}
            registerMissionPassed(key);
            continue;
        }
        if(!vcs&&command==0x0461) {
            if(!environment) {fail("FAIL_CURRENT_MISSION requires a live script environment.");return;}
            environment->failCurrentMission=2;
            continue;
        }
        if(!vcs&&command==0x0399) {
            Operand tune;
            if(!readOperand(*this,tune)||!environment) {
                fail("PLAY_MISSION_PASSED_TUNE requires a track and live audio environment.");return;
            }
            if(tune.bits!=1) {
                fail("PLAY_MISSION_PASSED_TUNE only has a verified completion stream for selector one.");return;
            }
            std::string audioError;
            if(!environment->playLcsMissionPassedTune||!environment->playLcsMissionPassedTune(tune.bits,audioError)) {
                fail(audioError.empty()?"The native MISCOM2 mission-completion stream is unavailable.":audioError);return;
            }
            continue;
        }
        if(!vcs&&(command==0x05FF||command==0x0600)) {
            Operand value;
            if(!environment||!readOperand(*this,value)) {fail("Invalid train-race statistic.");return;}
            auto& best=environment->statistics[command==0x05FF?"MostTimeLeftTrainRace":"HighestTrainCashEarned"];
            best=std::max(best,double(value.bits));continue;
        }
        if(vcs&&(command==0x0472||command==0x0473||command==0x04E0)) {
            if(!environment) {fail("Pager requires the live world.");return;}
            if(command==0x04E0) {environment->pager.clear();continue;}
            Operand source;std::string key;
            if(!readStringOperand(*this,source,key)) {fail("Invalid pager text operand.");return;}
            std::array<Operand,4> values{};
            const bool numbered=command==0x0473;
            for(size_t i=0;i<(numbered?4u:3u);++i)
                if(!readOperand(*this,values[i])) {fail("Truncated pager arguments.");return;}
            const auto text=environment->pagerText?environment->pagerText(key):std::u16string(key.begin(),key.end());
            const size_t first=numbered?1:0;
            environment->pager.add(text,values[0].bits,numbered,values[first].bits,
                values[first+1].bits,values[first+2].bits,environment->gameMilliseconds);
            continue;
        }
        std::string gameplayError;
        const int lcsRetailCollective = executeLcsRetailCollectiveQuery(*this, command, inverted, gameplayError);
        if (lcsRetailCollective < 0) { fail(gameplayError); return; }
        if (lcsRetailCollective > 0) continue;
        const int lcsRetailCharacter = executeLcsRetailCharacterCommand(*this, command, gameplayError);
        if (lcsRetailCharacter < 0) { fail(gameplayError); return; }
        if (lcsRetailCharacter > 0) continue;
        const int lcsRetailWorld = executeLcsRetailWorldQuery(*this, command, inverted, gameplayError);
        if (lcsRetailWorld < 0) { fail(gameplayError); return; }
        if (lcsRetailWorld > 0) continue;
        const int lcsRestoredCommand=executeLcsRestoredCommand(*this,command,inverted,gameplayError);
        if(lcsRestoredCommand<0) {fail(gameplayError);return;}
        if(lcsRestoredCommand>0)continue;
        const int lcsCollectiveCharacterQuery=executeLcsCollectiveCharacterQuery(*this,command,inverted,gameplayError);
        if(lcsCollectiveCharacterQuery<0) {fail(gameplayError);return;}
        if(lcsCollectiveCharacterQuery>0)continue;
        const int lcsEntityExtensionCommand=executeLcsEntityExtensionCommand(*this,command,inverted,gameplayError);
        if(lcsEntityExtensionCommand<0) {fail(gameplayError);return;}
        if(lcsEntityExtensionCommand>0)continue;
        const int lcsBonusCommand=executeLcsBonusPickupCommand(*this,command,gameplayError);
        if(lcsBonusCommand<0) {fail(gameplayError);return;}
        if(lcsBonusCommand>0)continue;
        const int lcsExtendedWorldSpawnCommand=executeLcsExtendedWorldSpawnCommand(*this,command,gameplayError);
        if(lcsExtendedWorldSpawnCommand<0) {fail(gameplayError);return;}
        if(lcsExtendedWorldSpawnCommand>0)continue;
        const int lcsExtendedWorldCommand=executeLcsExtendedPresentationCommand(*this,command,inverted,gameplayError);
        if(lcsExtendedWorldCommand<0) {fail(gameplayError);return;}
        if(lcsExtendedWorldCommand>0) continue;
        const int lcsExtendedVehicleCommand=executeLcsExtendedVehicleCommand(*this,command,inverted,gameplayError);
        if(lcsExtendedVehicleCommand<0) {fail(gameplayError);return;}
        if(lcsExtendedVehicleCommand>0) continue;
        const int lcsExtendedPathQuery=executeLcsExtendedPathQuery(*this,command,gameplayError);
        if(lcsExtendedPathQuery<0) {fail(gameplayError);return;}
        if(lcsExtendedPathQuery>0) continue;
        const int lcsExtendedCharacterQuery=executeLcsExtendedCharacterQuery(*this,command,inverted,gameplayError);
        if(lcsExtendedCharacterQuery<0) {fail(gameplayError);return;}
        if(lcsExtendedCharacterQuery>0) continue;
        const int lcsExtendedSceneCommand=executeLcsExtendedSceneCommand(*this,command,inverted,gameplayError);
        if(lcsExtendedSceneCommand<0) {fail(gameplayError);return;}
        if(lcsExtendedSceneCommand>0) continue;
        const int lcsPresentationCommand=executeLcsPresentationCommand(*this,command,inverted,gameplayError);
        if(lcsPresentationCommand<0) {fail(gameplayError);return;}
        if(lcsPresentationCommand>0)continue;
        const int lcsCharacterCommand=executeLcsCharacterCommand(*this,command,inverted,gameplayError);
        if(lcsCharacterCommand<0) {fail(gameplayError);return;}
        if(lcsCharacterCommand>0)continue;
        const int lcsVehicleObjectCommand=executeLcsVehicleObjectCommand(*this,command,inverted,gameplayError);
        if(lcsVehicleObjectCommand<0) {fail(gameplayError);return;}
        if(lcsVehicleObjectCommand>0)continue;
        const int lcsUtilityCommand=executeLcsUtilityCommand(*this,command,inverted,gameplayError);
        if(lcsUtilityCommand<0) {fail(gameplayError);return;}
        if(lcsUtilityCommand>0)continue;
        const int spatialCommand = executeSpatialCommand(*this, command, inverted, gameplayError);
        if (spatialCommand < 0) { fail(gameplayError); return; }
        if (spatialCommand > 0) continue;
        const int lcsCoreCommand=executeLcsCoreCommand(*this,command,elapsedMilliseconds,gameplayError);
        if(lcsCoreCommand<0) {fail(gameplayError);return;}
        if(lcsCoreCommand>0)continue;
        const int lcsCollectableCommand=executeLcsCollectableCommand(*this,command,gameplayError);
        if(lcsCollectableCommand<0) {fail(gameplayError);return;}
        if(lcsCollectableCommand>0)continue;
        const int lcsWorldCommand=executeLcsWorldCommand(*this,command,inverted,gameplayError);
        if(lcsWorldCommand<0) {fail(gameplayError);return;}
        if(lcsWorldCommand>0)continue;
        const int lcsEffectCommand=executeLcsEffectCommand(*this,command,inverted,gameplayError);
        if(lcsEffectCommand<0) {fail(gameplayError);return;}
        if(lcsEffectCommand>0)continue;
        const int lcsCraneCommand=executeLcsCraneCommand(*this,command,inverted,gameplayError);
        if(lcsCraneCommand<0) {fail(gameplayError);return;}
        if(lcsCraneCommand>0)continue;
        const int lcsEntityCommand=executeLcsEntityCommand(*this,command,inverted,gameplayError);
        if(lcsEntityCommand<0) {fail(gameplayError);return;}
        if(lcsEntityCommand>0)continue;
        const int lcsMissionCommand=executeLcsMissionCommand(*this,command,inverted,gameplayError);
        if(lcsMissionCommand<0) {fail(gameplayError);return;}
        if(lcsMissionCommand>0)continue;
        const int lcsNativeExtraCommand=executeLcsNativeExtraCommands(*this,command,gameplayError);
        if(lcsNativeExtraCommand<0) {fail(gameplayError);return;}
        if(lcsNativeExtraCommand>0)continue;
        if(vcs&&(command==0x0235||command==0x0248||command==0x0249||command==0x0479||command==0x047A)) {
            const int sphereCommand=executeMarkerCommand(*this,command,gameplayError);
            if(sphereCommand<0) {fail(gameplayError);return;}
            if(sphereCommand>0)continue;
        }
        const int extendedMissionCommand=executeExtendedMissionCommand(*this,command,inverted,gameplayError);
        if(extendedMissionCommand<0) {fail(gameplayError);return;}
        if(extendedMissionCommand>0)continue;
        const int aimPadCommand=executeVcsAimPadCommand(*this,command,gameplayError);
        if(aimPadCommand<0) {fail(gameplayError);return;}
        if(aimPadCommand>0)continue;
        const int additionalVcsCommand=executeVcsAdditionalOpcodeCommand(*this,command,inverted,gameplayError);
        if(additionalVcsCommand<0) {fail(gameplayError);return;}
        if(additionalVcsCommand>0)continue;
        const int nativeCompatibilityCommand=executeVcsNativeCompatibilityCommand(*this,command,inverted,gameplayError);
        if(nativeCompatibilityCommand<0) {fail(gameplayError);return;}
        if(nativeCompatibilityCommand>0)continue;
        const int restoredQueryCommand=executeVcsRestoredQueryCommand(*this,command,inverted,gameplayError);
        if(restoredQueryCommand<0) {fail(gameplayError);return;}
        if(restoredQueryCommand>0)continue;
        const int garageOpcode=executeVcsGarageOpcode(*this,command,inverted,gameplayError);
        if(garageOpcode<0) {fail(gameplayError);return;}
        if(garageOpcode>0)continue;
        const int cameraFovOpcode=executeVcsCameraFovOpcode(*this,command,inverted,gameplayError);
        if(cameraFovOpcode<0) {fail(gameplayError);return;}
        if(cameraFovOpcode>0)continue;
        const int meleeImpactOpcode=executeVcsMeleeImpactOpcode(*this,command,inverted,gameplayError);
        if(meleeImpactOpcode<0) {fail(gameplayError);return;}
        if(meleeImpactOpcode>0)continue;
        const int vehicleTurnForceOpcode=executeVcs039aOpcode(*this,command,gameplayError);
        if(vehicleTurnForceOpcode<0) {fail(gameplayError);return;}
        if(vehicleTurnForceOpcode>0)continue;
        const int vehicleSeaMineOpcode=executeVcsVehicleSeaMineOpcode(*this,command,inverted,gameplayError);
        if(vehicleSeaMineOpcode<0) {fail(gameplayError);return;}
        if(vehicleSeaMineOpcode>0)continue;
        const int followerAnimationOpcode=executeVcsFollowerAnimationOpcode(*this,command,inverted,gameplayError);
        if(followerAnimationOpcode<0) {fail(gameplayError);return;}
        if(followerAnimationOpcode>0)continue;
        const int scriptModelCommand=executeVcsScriptModelEffectCommand(*this,command,gameplayError);
        if(scriptModelCommand<0) {fail(gameplayError);return;}
        if(scriptModelCommand>0)continue;
        const int restoredWorldCommand=executeVcsRestoredWorldCommand(*this,command,inverted,gameplayError);
        if(restoredWorldCommand<0) {fail(gameplayError);return;}
        if(restoredWorldCommand>0)continue;
        const int worldUtilityCommand = executeVcsWorldUtilityCommand(*this, command, inverted, gameplayError);
        if (worldUtilityCommand < 0) {
            fail(gameplayError);
            return;
        }
        if (worldUtilityCommand > 0) {
            continue;
        }
        const int zoneCommand=executeVcsZoneCommand(*this,command,inverted,gameplayError);
        if(zoneCommand<0) {fail(gameplayError);return;}
        if(zoneCommand>0)continue;
        const int objectCommand=executeVcsObjectCommand(*this,command,inverted,gameplayError);
        if(objectCommand<0) {fail(gameplayError);return;}
        if(objectCommand>0)continue;
        const int vehicleExtensionCommand=executeVcsVehicleExtensionCommand(*this,command,inverted,gameplayError);
        if(vehicleExtensionCommand<0) {fail(gameplayError);return;}
        if(vehicleExtensionCommand>0)continue;
        const int mountedGunCommand=executeVcsMountedGunCommand(*this,command,inverted,gameplayError);
        if(mountedGunCommand<0) {fail(gameplayError);return;}
        if(mountedGunCommand>0)continue;
        const int entityCommand=executeVcsEntityCommand(*this,command,inverted,gameplayError);
        if(entityCommand<0) {fail(gameplayError);return;}
        if(entityCommand>0)continue;
        const int introCommand=executeVcsIntroCommand(*this,command,inverted,gameplayError);
        if(introCommand<0) {fail(gameplayError);return;}
        if(introCommand>0)continue;
        const int bitwiseCommand=executeVcsBitwiseCommand(*this,command,inverted,gameplayError);
        if(bitwiseCommand<0) {fail(gameplayError);return;}
        if(bitwiseCommand>0)continue;
        const int empireStatisticsCommand=executeVcsEmpireStatisticsCommand(*this,command,gameplayError);
        if(empireStatisticsCommand<0) {fail(gameplayError);return;}
        if(empireStatisticsCommand>0)continue;
        const int textAndStatsCommand=executeVcsTextAndStatsCommand(*this,command,inverted,gameplayError);
        if(textAndStatsCommand<0) {fail(gameplayError);return;}
        if(textAndStatsCommand>0)continue;
        const int weaponCommand=executeVcsWeaponCommand(*this,command,inverted,gameplayError);
        if(weaponCommand<0) {fail(gameplayError);return;}
        if(weaponCommand>0)continue;
        const int radarCommand=executeVcsRadarCommand(*this,command,gameplayError);
        if(radarCommand<0) {fail(gameplayError);return;}
        if(radarCommand>0)continue;
        const int cameraCommand=executeVcsCameraCommand(*this,command,inverted,gameplayError);
        if(cameraCommand<0) {fail(gameplayError);return;}
        if(cameraCommand>0)continue;
        const int audioCommand=executeMissionAudio(*this,command,inverted,gameplayError);
        if(audioCommand<0) {fail(gameplayError);return;}
        if(audioCommand>0)continue;
        const int worldCommand=executeRetailWorldCommand(*this,command,inverted,gameplayError);
        if(worldCommand<0) {fail(gameplayError);return;}
        if(worldCommand>0)continue;
        const int initialization=executeRetailInitialization(*this,command,inverted,gameplayError);
        if(initialization<0) {fail(gameplayError);return;}
        if(initialization>0)continue;
        const int cutscene=executeCutsceneCommand(*this,command,inverted,gameplayError);
        if(cutscene<0) {fail(gameplayError);return;}
        if(cutscene>0)continue;
        const int gameplay=executeMissionGameplay(*this,command,inverted,gameplayError);
        if(gameplay<0) {fail(gameplayError);return;}
        if(gameplay>0) continue;
        const int markerCommand=executeMarkerCommand(*this,command,gameplayError);
        if(markerCommand<0) {fail(gameplayError);return;}
        if(markerCommand>0)continue;
        const int onscreenCommand=executeVcsOnscreenCommand(*this,command,gameplayError);
        if(onscreenCommand<0) {fail(gameplayError);return;}
        if(onscreenCommand>0)continue;
        const int definedCommand=executeDefinitionCommand(*this,command,inverted,gameplayError);
        if(definedCommand<0) {fail(gameplayError);return;}
        if(definedCommand>0) continue;
        if(command==(vcs?0x0024:0x004F)||command==(vcs?0x0079:0x00DC)||command==(vcs?0x0289:0x041C)) {
            Operand target;
            if(!wholeScm||!readOperand(*this,target)) {fail("Script launch needs the complete SCM and a valid target.");return;}
            ScriptLaunch launch;
            launch.target=target.bits;launch.internalMission=command==(vcs?0x0289:0x041C);
            launch.missionThread=command!=(vcs?0x0024:0x004F);
            if(command==(vcs?0x0024:0x004F)) {
                while(pc<scriptBytes().size()&&scriptBytes()[pc]!=0) {
                    if(launch.arguments.size()>=96) {fail("Script launch exceeds the native local argument capacity.");return;}
                    Operand argument;std::string value;
                    if(!(vcs?readStringOperand(*this,argument,value):readOperand(*this,argument))) {fail("Script launch has an invalid argument.");return;}
                    const size_t index=launch.arguments.size();
                    launch.arguments.push_back(argument.bits);
                    if(vcs&&(argument.type==0x0A||!value.empty()))launch.strings[index]=std::move(value);
                }
                if(pc>=scriptBytes().size()) {fail("Script launch has no argument terminator.");return;}
                ++pc;
            }
            pendingLaunches.push_back(std::move(launch));
            state=StorylandMissionState::Waiting;delayMilliseconds=0;vcsAbsoluteWait=false;
            return;
        }
        if(command==(vcs?0x01BA:0x02D2)) {
            Operand first,second;
            if(!wholeScm||!readOperand(*this,first)||!readOperand(*this,second)||returnStack.size()>=16) {
                fail("GOSUB_FILE needs two labels and a free return-stack entry.");return;
            }
            returnStack.push_back(pc);
            if(!jumpTo(first.bits)) {fail("GOSUB_FILE target is outside MAIN.SCM.");return;}
            continue;
        }
        if(command==(vcs?0x037Au:0x05AEu)||command==(vcs?0x037Bu:0x05AFu)) {
            if(!wholeScm) {fail("CALL_FUNC requires the complete MAIN.SCM to resolve retail function labels.");return;}
            if(scriptBytes().size()-pc<3) {fail("CALL_FUNC has a truncated argument header.");return;}
            const uint8_t inputCount=scriptBytes()[pc++];
            const uint8_t outputCount=scriptBytes()[pc++];
            const uint8_t localBase=scriptBytes()[pc++];
            if(inputCount>32||outputCount>32||size_t(localBase)+inputCount+outputCount>locals.size()) {
                fail("CALL_FUNC has an invalid local variable range.");return;
            }
            Operand destination;
            if(!readOperand(*this,destination)) {fail("CALL_FUNC has no valid function address.");return;}
            std::array<int32_t,128> arguments{};
            std::map<size_t,std::string> argumentStrings;
            for(unsigned index=0;index<inputCount;++index) {
                if(vcs&&pc<scriptBytes().size()&&scriptBytes()[pc]==0x0Au) {
                    std::string value;
                    if(!(vcs?readVcsMissionText(*this,value):readScmText(*this,value))) {fail("CALL_FUNC has an invalid string input.");return;}
                    argumentStrings[index]=std::move(value);
                } else {
                    Operand value;
                    if(!readOperand(*this,value)) {fail("CALL_FUNC has an invalid input.");return;}
                    arguments[index]=value.bits;
                    if(value.reference) {
                        const auto& source=value.global?globalStrings:localStrings;
                        const auto found=source.find(value.variable);
                        if(found!=source.end())argumentStrings[index]=found->second;
                    }
                }
            }
            FunctionFrame frame;
            frame.returnPc=pc;
            frame.previousLocals=locals;
            frame.previousStrings=localStrings;
            frame.inputCount=inputCount;
            frame.outputCount=outputCount;
            frame.localBase=localBase;
            frame.returnDepth=returnStack.size();
            frame.invertReturn=(command==(vcs?0x037Au:0x05AFu))!=inverted;
            for(unsigned index=0;index<outputCount;++index) {
                Operand result;
                if(!readOperand(*this,result)||!result.reference) {fail("CALL_FUNC output is not a variable.");return;}
                frame.outputs.push_back({result.variable,result.global});
            }
            frame.returnPc=pc;
            if(functionFrames.size()>=16 || !jumpTo(destination.bits)) {
                fail("CALL_FUNC cannot reach a function within MAIN.SCM.");return;
            }
            functionFrames.push_back(std::move(frame));
            const int32_t timer0=locals[104],timer1=locals[105];
            locals.fill(0);locals[104]=timer0;locals[105]=timer1;
            localStrings=std::move(argumentStrings);
            // Callee locals start at zero. localBase reserves caller stack
            // space; this VM preserves that space in previousLocals instead.
            for(unsigned index=0;index<inputCount;++index)locals[index]=arguments[index];
        } else if(!vcs&&(command==0x04B3||command==0x04B4)) {
            Operand destination,source;
            if(!readOperand(*this,destination)||!readOperand(*this,source)||
                !writeOperand(*this,destination,source.bits)) {
                fail("LCS assignment requires a writable destination and typed source.");return;
            }
        } else if(vcs&&(command==0x0006u||command==0x0037u||command==0x02E2u)) {
            Operand destination,source;
            std::string stringValue;
            if(!readOperand(*this,destination)||!destination.reference||
                !readStringOperand(*this,source,stringValue)) {
                fail("String/any assignment has invalid typed operands.");return;
            }
            const bool sourceString=source.type==0x0Au||
                (source.reference&&(source.global?globalStrings:localStrings).count(source.variable)>0);
            if(!writeOperand(*this,destination,source.bits)) {fail("String assignment destination is invalid.");return;}
            auto& strings=destination.global?globalStrings:localStrings;
            if(sourceString)strings[destination.variable]=std::move(stringValue);
            else strings.erase(destination.variable);
        } else if(vcs&&(command==0x001Fu||command==0x0020u)) {
            Operand first,second;
            std::string left,right;
            if(!readStringOperand(*this,first,left)||!readStringOperand(*this,second,right)) {
                fail("String comparison has invalid operands.");return;
            }
            updateScmCondition(*this,left==right,inverted);
        } else if(command==0x0000) {
            continue;
        } else if(vcs&&(command==0x0239||command==0x023A||command==0x023B)) {
            // Retail debug-output handlers consume their numeric argument,
            // then return without file I/O (0x333D18/0x333D48/0x333D78).
            if(command!=0x023B) {
                Operand ignored;
                if(!readOperand(*this,ignored)) {fail("Debug command has a truncated operand.");return;}
            }
        } else if(vcs&&command==0x03EE) {
            std::array<Operand,5> values{};
            for(auto& value:values)if(!readOperand(*this,value)) {fail("Heading query has a truncated operand.");return;}
            for(size_t index=0;index<4;++index)if(!std::isfinite(operandFloat(values[index]))) {
                fail("Heading query requires finite coordinates.");return;
            }
            if(!values[4].reference) {fail("Heading query requires a writable destination.");return;}
            // 0x3690B0 computes atan2(x2-x1,y1-y2), in degrees; these
            // operands are two points, despite the old vector-angle label.
            const float dx=operandFloat(values[2])-operandFloat(values[0]);
            const float dy=operandFloat(values[1])-operandFloat(values[3]);
            if(!std::isfinite(dx)||!std::isfinite(dy)) {fail("Heading query coordinate difference overflowed.");return;}
            const float heading=std::atan2(dx,dy)*57.2957763671875f;
            int32_t bits=0;std::memcpy(&bits,&heading,4);writeOperand(*this,values[4],bits);
        } else if(command==(vcs?0x0238u:0x03A9u)) {
            // VCS reads a full typed string, then copies/lowercases eight bytes
            // into CRunningScript::m_szName (retail handler 0x3B2168).
            if(vcs) {
                Operand name;
                if(!readStringOperand(*this,name,scriptName)) {
                    fail("SCRIPT_NAME has an invalid typed string operand.");return;
                }
                if(scriptName.size()>8)scriptName.resize(8);
                for(char& character:scriptName)if(character>='A'&&character<='Z')character+=char('a'-'A');
            } else if(!readScmText(*this,scriptName)) {
                fail("SCRIPT_NAME has an invalid text operand.");return;
            }
        } else if(command==(vcs?0x02B0u:0x045Eu)) {
            Operand argument;std::string name;
            if(!(vcs?readStringOperand(*this,argument,name):readScmText(*this,name))) {
                fail("TERMINATE_ALL_SCRIPTS_WITH_THIS_NAME has an invalid name.");return;
            }
            for(char& character:name)if(character>='A'&&character<='Z')character+=char('a'-'A');
            if(environment) {
                if(environment->scriptTerminationRequests.size()>=96) {
                    fail("Script termination request pool is full.");return;
                }
                environment->scriptTerminationRequests.push_back(name);
            }
            std::string ownName=scriptName;
            for(char& character:ownName)if(character>='A'&&character<='Z')character+=char('a'-'A');
            if(name==ownName) {
                pendingLaunches.clear();delayMilliseconds=0;
                state=StorylandMissionState::Finished;
                if(isMissionThread)missionCleanupRequested=true;
                diagnostic="SCM thread terminated by script name.";
            }
        } else if(command==missionFinished) {
            if(isMissionThread) {
                missionCleanupRequested=true;
                if(vcs&&environment) {
                    auto& world=*environment;
                    storylandCleanupVcsMissionActors(world);
                    const auto objects=world.missionCleanupObjects;
                    for(int32_t handle:objects)storylandDeleteScriptObject(world,handle);
                    world.missionCleanupObjects.clear();
                    world.carDensityMultiplier=1;world.pedDensityMultiplier=1;
                    world.control=true;world.cameraControlEnabled=true;
                    world.cameraFrontPlayerRequested=false;
                    if(world.flashingHudObject!=3&&world.flashingHudObject!=4)world.flashingHudObject=-1;
                    world.camera.active=false;world.camera.restoreRequested=true;
                    world.camera.cutsceneOwned=false;world.cameraFocus={};world.cameraLookAt={};world.manualAim={};
                    world.helpMessage.clear();world.helpMessageNumbers.clear();world.helpMessageMilliseconds=0;
                    for(size_t index=0;index<5;++index) {
                        auto& slot=world.audioSlots[index];
                        slot={};slot.serial=world.nextAudioSerial++;
                    }
                }
            }
        } else if(command==terminate) {
            state=StorylandMissionState::Finished;
            if(missionOutcome==StorylandMissionOutcome::Passed)
                diagnostic="Mission passed; SCM thread finished.";
            else if(missionOutcome==StorylandMissionOutcome::Failed)
                diagnostic="Mission failed; SCM thread finished.";
            else diagnostic="SCM thread finished without a recorded mission outcome.";
        } else if(command==0x0001||(!vcs&&command==0x02A6)) {
            Operand wait,skippable;
            if(!readOperand(*this,wait)||(!vcs&&(wait.bits<0||wait.bits>600000))) {
                fail("WAIT has an invalid time argument.");return;
            }
            if(command==0x02A6) {
                if(!readOperand(*this,skippable)) {fail("MESSAGE_WAIT has no skip flag.");return;}
                if(skippable.bits)messageWaitSkippable=true;
            } else messageWaitSkippable=false;
            if(vcs&&environment) {
                const uint32_t now=environment->gameMilliseconds;
                vcsWakeTime=now+uint32_t(wait.bits);
                vcsAbsoluteWait=true;
                delayMilliseconds=now<vcsWakeTime?vcsWakeTime-now:0;
            } else {
                vcsAbsoluteWait=false;
                delayMilliseconds=uint32_t(wait.bits);
            }
            state=StorylandMissionState::Waiting;
        } else if(command==0x0002 || command==jumpTrue || command==jumpFalse || command==call) {
            Operand target;
            if(!readOperand(*this,target)) {
                fail("Branch has an invalid SCM parameter.");return;
            }
            if(command==jumpTrue || command==jumpFalse) {
                if(!conditionValid) {fail("Conditional branch without an evaluated condition.");return;}
                if((command==jumpTrue)!=condition)continue;
            }
            if(command==call) {
                if(returnStack.size()>=16) {fail("Mission subroutine stack overflow.");return;}
                returnStack.push_back(pc);
            }
            if(!jumpTo(target.bits)) {
                fail("Branch targets code outside this mission; main-script execution context is required.");return;
            }
        } else if(command==ret||command==(vcs?0x005E:0x00C5)||command==(vcs?0x005F:0x00C6)||
            (vcs&&(command==0x0391||command==0x0392))||
            (!vcs&&(command==0x05C5||command==0x05C6))) {
            const bool conditionalReturn=vcs?(command==0x0391||command==0x0392):
                (command==0x05C5||command==0x05C6);
            if(conditionalReturn&&!condition)continue;
            // Both retail conditional-true names evaluate FALSE before returning.
            if(conditionalReturn&&command==(vcs?0x0392:0x05C6))
                updateScmCondition(*this,false,inverted);
            else if(!conditionalReturn&&command!=ret)
                updateScmCondition(*this,command==(vcs?0x005E:0x00C5),inverted);
            if(!returnStack.empty() && (functionFrames.empty() ||
                returnStack.size()>functionFrames.back().returnDepth)) {
                pc=returnStack.back();returnStack.pop_back();
            }
            else if(!functionFrames.empty()) {
                FunctionFrame frame=std::move(functionFrames.back());functionFrames.pop_back();
                if(frame.invertReturn)condition=!condition;
                std::vector<int32_t> results;
                std::map<size_t,std::string> resultStrings;
                for(size_t index=0;index<frame.outputs.size();++index)
                {
                    const size_t slot=frame.inputCount+index;
                    results.push_back(locals[slot]);
                    const auto found=localStrings.find(slot);
                    if(found!=localStrings.end())resultStrings[index]=found->second;
                }
                const int32_t timer0=locals[104],timer1=locals[105];
                locals=frame.previousLocals;locals[104]=timer0;locals[105]=timer1;
                localStrings=std::move(frame.previousStrings);
                pc=frame.returnPc;
                for(size_t index=0;index<frame.outputs.size();++index) {
                    const auto& output=frame.outputs[index];
                    if(output.second) {
                        if(output.first<globals.size())globals[output.first]=results[index];
                    } else if(output.first<locals.size())locals[output.first]=results[index];
                    auto& strings=output.second?globalStrings:localStrings;
                    const auto found=resultStrings.find(index);
                    if(found!=resultStrings.end())strings[output.first]=found->second;
                    else strings.erase(output.first);
                }
            } else {fail("RETURN without a matching GOSUB or CALL_FUNC.");return;}
        } else if(command==(vcs?0x0027:0x0052)) {
            for(int parameter=0;parameter<6;++parameter) {
                Operand ignored;
                if(!readOperand(*this,ignored)) {fail("LINE/NOP contains an unsupported argument.");return;}
            }
        } else if(command==ifCode) {
            Operand count;
            if(!readOperand(*this,count)||!((count.bits>=0&&count.bits<=8)||(count.bits>=21&&count.bits<=28))) {
                fail("IF has an unsupported condition count.");return;
            }
            conditionOr=count.bits>=21;
            condition=!conditionOr;
            conditionValid=true;
            conditionCount=conditionOr?uint32_t(count.bits-20):uint32_t(count.bits);
        } else if((vcs&&command>=0x000F&&command<=0x001E) ||
                  (!vcs&&((command>=0x0018&&command<=0x003C)||(command>=0x0042&&command<=0x0046)))) {
            Operand first,second;
            if(!readOperand(*this,first)||!readOperand(*this,second)) {
                fail("Conditional comparison has unsupported parameters.");return;
            }
            const unsigned kind=vcs?(command<=0x0014?0:command<=0x001A?1:2):
                (command<=0x0027?0:command<=0x0037?1:2);
            const bool floating=vcs?
                ((command>=0x0012&&command<=0x0014)||(command>=0x0018&&command<=0x001A)||command>=0x001D):
                ((command>=0x0020&&command<=0x0027)||(command>=0x0030&&command<=0x0037)||command>=0x0042);
            bool matches=false;
            if(floating) {
                const float a=operandFloat(first);
                const float b=operandFloat(second);
                matches=kind==0?a>b:(kind==1?a>=b:a==b);
            } else {
                matches=kind==0?first.bits>second.bits:
                    (kind==1?first.bits>=second.bits:first.bits==second.bits);
            }
            if(inverted)matches=!matches;
            if(conditionCount>0) {
                condition=conditionOr?(condition||matches):(condition&&matches);
                --conditionCount;
            } else condition=matches;
            conditionValid=true;
        } else if(vcs&&command>=0x0031&&command<=0x0034) {
            Operand destination,rate;
            if(!readOperand(*this,destination)||!readOperand(*this,rate)||!destination.reference) {
                fail("Timed arithmetic requires a variable destination and a numeric rate.");return;
            }
            const float left=operandFloat(destination),right=operandFloat(rate);
            const float step=environment?environment->nativeScriptTimeStep:1.0f;
            if(!std::isfinite(left)||!std::isfinite(right)||!std::isfinite(step)) {
                fail("Timed arithmetic has a non-finite operand.");return;
            }
            float calculated=0;
            if(!tryTimedEeArithmetic(left,right,step,command>=0x0033,calculated)) {
                fail("Timed arithmetic requires unsupported extended EE exponent255 behavior; destination unchanged.");return;
            }
            int32_t result=0;std::memcpy(&result,&calculated,4);
            if(!writeOperand(*this,destination,result)) {fail("Invalid timed arithmetic destination.");return;}
        } else if((vcs&&command>=0x0038&&command<=0x0039)||(!vcs&&command>=0x008C&&command<=0x0093)) {
            Operand destination,value;
            if(!readOperand(*this,destination)||!readOperand(*this,value)||!destination.reference) {
                fail("Conversion requires a variable destination and a numeric source.");return;
            }
            int32_t result=0;
            if((command&1)==0) {
                const double number=operandFloat(value);
                if(!std::isfinite(number)||number<double(INT32_MIN)||number>double(INT32_MAX)) {
                    fail("Float-to-integer conversion exceeds the integer range.");return;
                }
                result=int32_t(number);
            } else {
                const float number=float(value.bits);std::memcpy(&result,&number,4);
            }
            writeOperand(*this,destination,result);
        } else if((vcs&&((command>=0x0004&&command<=0x000E)||((command>=0x0029&&command<=0x0030)||(command>=0x0035&&command<=0x0036)))) ||
                 (!vcs&&((command>=0x0004&&command<=0x0017)||(command>=0x0058&&command<=0x0077)||
                    (command>=0x0084&&command<=0x008B)))) {
            Operand destination,value;
            if(!readOperand(*this,destination)||!readOperand(*this,value)||!destination.reference) {
                fail("Variable command has unsupported parameter types.");return;
            }
            const bool extended=command>=0x0029;
            const bool assignment=vcs?(command<=0x0005||command>=0x0035):(command<=0x0007||command>=0x0084);
            const bool floating=extended?(vcs?(command%2==0):
                (command<0x0078?command%2==1:command>=0x0086&&command<=0x0089)):vcs?(command==0x0005||command==0x0008||command==0x000A||
                command==0x000C||command==0x000E):
                (command==0x0005||command==0x0007||command==0x0009||command==0x000B||
                 command==0x000D||command==0x000F||command==0x0011||command==0x0013||
                 command==0x0015||command==0x0017);
            int32_t result=0;
            if(floating) {
                const float left=operandFloat(destination),right=operandFloat(value);
                float calculated=assignment?right:left;
                if(!assignment) {
                    const unsigned kind=extended?(vcs?unsigned(command-0x0029)/2:unsigned(command-0x0058)/8):
                        (vcs?unsigned(command-0x0007)/2:unsigned(command-0x0008)/4);
                    if(kind==3&&right==0) {fail("Division by zero.");return;}
                    if(kind==0)calculated=left+right;
                    else if(kind==1)calculated=left-right;
                    else if(kind==2)calculated=left*right;
                    else if(right!=0)calculated=left/right;
                }
                if(!std::isfinite(calculated)) {fail("Arithmetic produced an invalid floating-point value.");return;}
                std::memcpy(&result,&calculated,4);
            } else {
                const int64_t left=destination.bits,right=value.bits;
                int64_t calculated=assignment?right:left;
                if(!assignment) {
                    const unsigned kind=extended?(vcs?unsigned(command-0x0029)/2:unsigned(command-0x0058)/8):
                        (vcs?unsigned(command-0x0007)/2:unsigned(command-0x0008)/4);
                    if(kind==3&&right==0) {fail("Division by zero.");return;}
                    if(kind==0)calculated=left+right;
                    else if(kind==1)calculated=left-right;
                    else if(kind==2)calculated=left*right;
                    else if(right!=0)calculated=left/right;
                }
                result=int32_t(uint32_t(calculated));
            }
            if(!writeOperand(*this,destination,result)) {fail("Invalid script variable destination.");return;}
        } else {
            state = StorylandMissionState::Unsupported;
            std::ostringstream message;
            message << "Opcode 0x" << std::hex << std::uppercase << std::setw(4)
                << std::setfill('0') << opcode << " at mission offset 0x" << instructionOffset
                << (command>(vcs?0x0564u:0x0678u)?" exceeds the retail opcode range; the mission's entry offset may be incorrect.":
                    " has no verified gameplay handler for this engine.");
            const auto definition=opcodeDefinitions.find(command);
            if(definition!=opcodeDefinitions.end())
                message << " Sanny Builder: " << definition->second.second
                    << " (" << std::dec << definition->second.first << " parameters).";
            diagnostic = message.str();
        }
    }
    if (vcs && state == StorylandMissionState::Running) {
        state = StorylandMissionState::Fault;
        diagnostic = "VCS script exceeded " + std::to_string(commandLimit) +
            " commands without yielding.";
    }
}

void StorylandMissionObjective::start(int id, float playerX, float playerY, float playerZ,
    float checkpointX, float checkpointY, float checkpointZ) {
    missionId = id;
    startX = playerX; startY = playerY; startZ = playerZ;
    destinationX = checkpointX; destinationY = checkpointY; destinationZ = checkpointZ;
    state = StorylandMissionObjectiveState::GoToCheckpoint;
}

bool StorylandMissionObjective::update(float x, float y, float z) {
    if (state != StorylandMissionObjectiveState::GoToCheckpoint &&
        state != StorylandMissionObjectiveState::ReturnToStart) return false;
    const float targetX = state == StorylandMissionObjectiveState::GoToCheckpoint ? destinationX : startX;
    const float targetY = state == StorylandMissionObjectiveState::GoToCheckpoint ? destinationY : startY;
    const float targetZ = state == StorylandMissionObjectiveState::GoToCheckpoint ? destinationZ : startZ;
    const float dx = x - targetX, dy = y - targetY, dz = z - targetZ;
    if (dx * dx + dy * dy + dz * dz > radius * radius) return false;
    if (state == StorylandMissionObjectiveState::GoToCheckpoint) {
        state = StorylandMissionObjectiveState::ReturnToStart;
        return false;
    }
    state = StorylandMissionObjectiveState::Complete;
    return true;
}

void StorylandMissionObjective::stop() {
    *this = {};
}
